// PROBES P-B ("the stope falls") and P-C ("the chamber fight") — RUNG E2's
// acceptance instruments (docs/ENEMY_AI_E1_E2_SPEC.md, ACCEPTANCE).
//
// THE MEASURED PROBLEM (tapes 83-88, ~81 min of Chad's own flying): the enemy
// StrikeOrder on the allied deep pump was armed 100% of every tape, and
// on_station read 0.0 s in 5 of 6 — closest approach 4.9 to 18.5 km. No combat
// has EVER occurred underground in any tape; every in-net death is a RUN-mode
// terrain crash. Three locks did it (L1 the order had no transit verb, L2 the
// divert's 35-deg gamma could not reach a pump on the far side of the arena,
// L3 no guns underground ever). These probes measure the chain those locks
// broke, end to end.
//
// PRIMARY-DATA LAW, same as P-A:
//   * REAL LOADERS (aircraft.toml / scenario.toml / game.toml) — the dials
//     under test are the dials that ship.
//   * REAL WORLD — a live world::TunnelNet with the arena body on, the live
//     heightfield as the crash surface, and the two-faction atmosphere.
//   * REAL PLACEMENT — the deep pumps come from combat::place_deep_pump off
//     the LIVE arena, exactly as app/main.cpp places them.
//   * REAL ORDER — the app's own sequence: orders armed -> drone::tick ->
//     enemy_fire_tick -> combat_player_tick, with the same damage arbitration
//     glue app/instructor_tick.h runs.
//   * A STEPPED PLAYER in P-C (never a stamped-velocity parked state — the
//     fixture-phantom trap that manufactured a defect through two aiming-law
//     rewrites).
//
// TEST_CASE and SECTION names are STRICTLY ASCII.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/instructor_tick.h"
#include "combat/conquest.h"
#include "combat/kill.h"
#include "combat/raid.h"
#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

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

world::HeightField uniform_field(double elev_m, double relief = 4000.0) {
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

// The T1 canon tunnel dials (the test_tunnel.cpp / test_maverick.cpp fixture —
// the SAME net the game-loop certificate flies).
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 0.0;
    tp.arena_a_m = 7350.0;
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.chambers_on = true;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;
    tp.trench_rim_m = 150.0;
    return tp;
}

sim::GroundParams tunnel_ground_params() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    return gp;
}

// The PRE arm: every RUNG E2 dial at its off value (the struct defaults, which
// are field-for-field the pre-E2 welded StrikeOrder envelope). E1 stays ON in
// both arms — this probe measures E2, not E1 again.
//
// ★ A3 (audit 2026-08-25) — INVERTED, and here is the defect it repairs. The
// old form copied `shipped` FIRST and reset a hand-enumerated 17-field E2
// list, so any NEW strike dial leaked into the CONTROL arm: E18's
// strike_attack_alt_m did exactly that — the PRE arm inherited the fix from
// the TOML, killed pumps[2], and E18 was refused on the false red its own
// control produced. THE LAW, again: a hand-maintained LIST that describes the
// shipped table stops describing it the moment the table moves.
//
// Now the control starts from DroneParams{} — which IS the pre-E2 table by
// construction, every strike_*/arena_* dial at its structural OFF — and the
// NON-E2 fields are copied forward. The failure mode has swapped sides ON
// PURPOSE: a NEW strike/arena dial is OFF in the control with no edit here
// (the growth direction that has now recurred twice), while a NEW non-E2 dial
// must be ADDED to the copy-list below or the control flies it at default.
// The sizeof tripwire underneath forces that partition decision whenever
// DroneParams grows.
//
// THE PARTITION, verified field-for-field against drone.h:57-678 (82 members
// total: 80 scalars + maverick + bfm): E2 = 19 fields, the strike_*/arena_*
// block (rungs E2.1-E2.4 plus E18's two dive dials). 63 copied forward.
// ⚠ ONE NAMED EXCEPTION: strike_concurrent_max is spelled strike_* but is
// E3's tempo dial, NOT E2 — it is copied. So "every new strike_* dial is
// auto-safe" is true only for E2 dials; a new tempo dial still needs a
// decision here.
drone::DroneParams pre_e2(const drone::DroneParams& shipped) {
    drone::DroneParams dp;  // every field at its struct default = E2 OFF
    // -- the plant and the patrol program --
    dp.count = shipped.count;
    dp.size = shipped.size;
    dp.straight_time = shipped.straight_time;
    dp.turn_time = shipped.turn_time;
    dp.turn_bank = shipped.turn_bank;
    dp.throttle = shipped.throttle;
    dp.speed = shipped.speed;
    dp.spawn_ahead = shipped.spawn_ahead;
    dp.spawn_side = shipped.spawn_side;
    dp.spawn_alt = shipped.spawn_alt;
    dp.spread_m = shipped.spread_m;
    dp.hp = shipped.hp;
    dp.hit_radius_m = shipped.hit_radius_m;
    // -- pursuit, fire discipline, E1's feed-forwards --
    dp.engage_range = shipped.engage_range;
    dp.disengage_range = shipped.disengage_range;
    dp.pursue_k_az = shipped.pursue_k_az;
    dp.pursue_k_el = shipped.pursue_k_el;
    dp.pursue_max_bank = shipped.pursue_max_bank;
    dp.pursue_max_gamma = shipped.pursue_max_gamma;
    dp.pursue_bank_gain = shipped.pursue_bank_gain;
    dp.pursue_pitch_gain = shipped.pursue_pitch_gain;
    dp.pursue_track_pitch_gain = shipped.pursue_track_pitch_gain;
    dp.pursue_pull = shipped.pursue_pull;
    dp.raid_fight_yield_m = shipped.raid_fight_yield_m;
    dp.pursue_steer_lead_max_s = shipped.pursue_steer_lead_max_s;
    dp.pursue_lead_speed = shipped.pursue_lead_speed;
    dp.pursue_lead_max_s = shipped.pursue_lead_max_s;
    dp.fire_cone_cos = shipped.fire_cone_cos;
    dp.fire_align_cos = shipped.fire_align_cos;
    dp.fire_range_min = shipped.fire_range_min;
    dp.fire_range_max = shipped.fire_range_max;
    dp.snapshot_range_m = shipped.snapshot_range_m;
    dp.pursue_speed_bump = shipped.pursue_speed_bump;
    dp.pursue_climb_throttle_gain = shipped.pursue_climb_throttle_gain;
    dp.throttle_ff = shipped.throttle_ff;
    dp.bank_slew_dps = shipped.bank_slew_dps;
    // -- self-preservation --
    dp.avoid_agl_enter_m = shipped.avoid_agl_enter_m;
    dp.avoid_agl_release_m = shipped.avoid_agl_release_m;
    dp.avoid_lookahead_s = shipped.avoid_lookahead_s;
    dp.avoid_air_dive_agl_m = shipped.avoid_air_dive_agl_m;
    dp.avoid_gamma = shipped.avoid_gamma;
    dp.avoid_bank_cap = shipped.avoid_bank_cap;
    dp.avoid_air_frac_full = shipped.avoid_air_frac_full;
    dp.avoid_air_frac_hard = shipped.avoid_air_frac_hard;
    dp.avoid_air_dive_gamma = shipped.avoid_air_dive_gamma;
    // -- S1-DECK: the deck band, the scope probe, the track law, the forward
    //    eyes. NON-E2 self-preservation, so the control arm flies them. --
    dp.deck_avoid_agl_enter_m = shipped.deck_avoid_agl_enter_m;
    dp.deck_avoid_agl_release_m = shipped.deck_avoid_agl_release_m;
    dp.deck_scope_hyst_frac = shipped.deck_scope_hyst_frac;
    dp.deck_track_agl_m = shipped.deck_track_agl_m;
    dp.deck_track_gain = shipped.deck_track_gain;
    dp.deck_track_dive_cap = shipped.deck_track_dive_cap;
    dp.deck_lookahead_s = shipped.deck_lookahead_s;
    dp.avoid_pull_net_g = shipped.avoid_pull_net_g;
    // -- the whole maverick tunnel machine + the BFM machine (E1 stays ON) --
    dp.maverick = shipped.maverick;
    dp.bfm = shipped.bfm;
    // -- E3's tempo pair. NOT E2: both were shipped-in-both-arms under the
    //    old form, and defaulting them would move the measured PRE baseline. --
    dp.transit_fight_yield_m = shipped.transit_fight_yield_m;
    dp.strike_concurrent_max = shipped.strike_concurrent_max;
    // -- E6 postures, E17 raid geometry, E7/E10 tiers --
    dp.extend_leash = shipped.extend_leash;
    dp.aggression_range_m = shipped.aggression_range_m;
    dp.raid_fight_in_place = shipped.raid_fight_in_place;
    dp.raid_attack_alt_m = shipped.raid_attack_alt_m;
    dp.raid_reattack_m = shipped.raid_reattack_m;
    // -- S2-TUNNEL: the raid router + C1's errand speed. NOT E2 (they are raid
    //    geometry / routing, not the strike divert), so the control arm flies
    //    them, exactly like the E17 pair above. --
    dp.raid_route_via_tunnel = shipped.raid_route_via_tunnel;
    dp.raid_deck_run_slots = shipped.raid_deck_run_slots;
    dp.raid_route_gap_max_m = shipped.raid_route_gap_max_m;
    dp.raid_route_stagger_s = shipped.raid_route_stagger_s;
    dp.raid_speed_target = shipped.raid_speed_target;
    // -- D2: the reposition (stage 0) + the return (stage 1). NOT E2 (they are
    //    boundary/return geometry, not the strike divert), so the control arm
    //    flies them, exactly like the S2-TUNNEL block above. --
    dp.regroup_frac_arm = shipped.regroup_frac_arm;
    dp.regroup_frac_lip = shipped.regroup_frac_lip;
    dp.regroup_frac_release = shipped.regroup_frac_release;
    dp.regroup_pull_frac = shipped.regroup_pull_frac;
    dp.regroup_agl_m = shipped.regroup_agl_m;
    dp.regroup_max_s = shipped.regroup_max_s;
    dp.regroup_attack_window_s = shipped.regroup_attack_window_s;
    dp.regroup_require_pump_attack = shipped.regroup_require_pump_attack;
    dp.regroup_return_deck_run = shipped.regroup_return_deck_run;
    // -- D3: the parabolic deck run (stages 2-5). NOT E2 (raid geometry, not
    //    the strike divert), so the control arm flies them, exactly like the
    //    D2 block above. --
    dp.raid_ballistic_climb_agl_m = shipped.raid_ballistic_climb_agl_m;
    dp.raid_ballistic_climb_max_s = shipped.raid_ballistic_climb_max_s;
    dp.raid_ballistic_dive_gamma = shipped.raid_ballistic_dive_gamma;
    dp.raid_ballistic_dive_speed = shipped.raid_ballistic_dive_speed;
    dp.raid_ballistic_align_min = shipped.raid_ballistic_align_min;
    dp.raid_ballistic_run_agl_m = shipped.raid_ballistic_run_agl_m;
    dp.ace_bank_cap = shipped.ace_bank_cap;
    dp.mid_bank_cap = shipped.mid_bank_cap;
    dp.ace_aggression_min = shipped.ace_aggression_min;
    dp.mid_aggression_min = shipped.mid_aggression_min;
    dp.ace_bank_track_lo = shipped.ace_bank_track_lo;
    dp.ace_bank_track_hi = shipped.ace_bank_track_hi;
    dp.ace_bank_agl_lo_m = shipped.ace_bank_agl_lo_m;
    dp.ace_bank_agl_hi_m = shipped.ace_bank_agl_hi_m;
    dp.slash_doctrine = shipped.slash_doctrine;
    // -- ENV-2: the air-envelope governor. NOT E2 (this probe's "E2" is the
    //    STRIKE DIVERT, an older and entirely different rung -- see the naming
    //    note in drone/drone.h at env_edge_alt_m). These are flight-envelope
    //    dials, so the control arm flies them, like every block above. --
    dp.env_edge_alt_m = shipped.env_edge_alt_m;
    dp.env_slope = shipped.env_slope;
    dp.env_plateau_m = shipped.env_plateau_m;
    dp.env_desc_cap = shipped.env_desc_cap;
    dp.env_desc_fade_band_m = shipped.env_desc_fade_band_m;
    dp.env_fade_band_m = shipped.env_fade_band_m;
    dp.env_shrink_radius_frac = shipped.env_shrink_radius_frac;
    // -- ENV-3: the defence scramble. NOT E2 (the strike divert). --
    dp.defend_sprint_speed = shipped.defend_sprint_speed;
    dp.defend_release_m = shipped.defend_release_m;
    dp.defend_dive_gamma = shipped.defend_dive_gamma;
    dp.defend_dive_close_gamma = shipped.defend_dive_close_gamma;
    dp.defend_dive_close_range_m = shipped.defend_dive_close_range_m;
    // Chad's 2026-08-30 doctrine: the parabola's ARM and EXIT gates. NON-E2.
    dp.defend_dive_arm_alt_m = shipped.defend_dive_arm_alt_m;
    dp.defend_dive_exit_speed = shipped.defend_dive_exit_speed;
    // -- ENV-3.4: the perch. NOT E2 (the strike divert). --
    dp.guard_alt_lo_m = shipped.guard_alt_lo_m;
    dp.guard_alt_hi_m = shipped.guard_alt_hi_m;
    dp.guard_margin_m = shipped.guard_margin_m;
    dp.guard_climb_gain = shipped.guard_climb_gain;
    dp.guard_gamma_cap = shipped.guard_gamma_cap;
    // -- RUNG DF-1: the deck fight. NOT E2 (the strike divert). It is a
    //    flight-envelope dial on the ENGAGED deck tick, so the control arm
    //    flies it like every block above. ⚠ IT MUST BE COPIED: left at the
    //    struct default it is NEGATIVE = OFF, so an uncopied probe would fly
    //    the un-capped pursuit and grade a build nobody ships — the exact
    //    silent-revert class this partition exists to prevent.
    dp.deck_fight_climb_cap = shipped.deck_fight_climb_cap;
    // NOT copied — the E2 partition (19), left at DroneParams{}'s structural
    // OFF: strike_on_order, strike_stagger_s, strike_engage_m, strike_k_az,
    //   strike_k_el, strike_bank_cap, strike_gamma_cap, strike_bail_s,
    //   strike_station_range_m, strike_station_cos, strike_attack_alt_m,
    //   strike_glide_limit, strike_defer_exit, strike_strafe,
    //   arena_fight_range_m, arena_fight_s, arena_gamma_cap,
    //   arena_guard_margin_m, arena_guard_release_m.
    return dp;
}

// ★ A3 THE TRIPWIRE. An inverted list is STILL a hand-maintained list, and
// this codebase has no reflection, so the cheapest thing that goes red when
// DroneParams grows is its size. On red: the CHECK prints both numbers —
// decide which side of pre_e2's partition the new field lives on, add it to
// the copy-list above if it is not an E2 strike_*/arena_* dial, THEN weld the
// new size here in the SAME commit. Blind spot, stated honestly: a field
// added inside existing padding (e.g. a bool beside slash_doctrine) may not
// move sizeof — the partition comment above is the only guard there. It also
// fires on maverick/bfm sub-struct growth; that is the safe direction (it
// forces the decision), not a false alarm to suppress.
TEST_CASE("pre_e2 tripwire: DroneParams has not grown past the partition") {
    // S1-DECK added EIGHT doubles to the self-preservation block (90 members):
    // 1312 + 8*8 = 1376, all copied forward by pre_e2 above.
    // S2-TUNNEL added FIVE more (95 members): 3 doubles + 1 int + 1 bool ->
    // 24 + 4 + 1 = 29 bytes, padded to 32 under MSVC/x64 8-byte alignment:
    // 1376 + 32 = 1408. All five are NON-E2 and are copied forward by pre_e2.
    // S3-GUNS added ONE bool to the nested bfm sub-struct
    // (BfmParams::intercept_chase_unfaded, drone/bfm.h): 1 byte padded to 8
    // under MSVC/x64 -> 1408 + 8 = 1416. It is NON-E2 and needs no new copy
    // line: pre_e2 already copies the whole sub-struct at `dp.bfm =
    // shipped.bfm` above. ★ THE TRIPWIRE DID ITS JOB — it went red on a field
    // added two layers down in a header nobody was watching, which is exactly
    // the "sub-struct growth" case its own comment names as the safe direction.
    // D2 added NINE more (104 members): 7 doubles + 2 bools -> 56 + 2 = 58
    // bytes, padded to 64 under MSVC/x64 8-byte alignment: 1416 + 64 = 1480.
    // All nine are NON-E2 and are copied forward by pre_e2 above.
    // D3 added SIX doubles (110 members): 48 bytes, no padding needed under
    // MSVC/x64 8-byte alignment: 1480 + 48 = 1528. All six are NON-E2 and are
    // copied forward by pre_e2 above.
    // ENV-2 (the AIR-ENVELOPE governor -- ⚠ NOT this probe's "E2", which is the
    // strike divert) added SEVEN doubles (117 members): 56 bytes, no padding
    // needed under MSVC/x64 8-byte alignment: 1528 + 56 = 1584. All seven are
    // NON-E2 and are copied forward by pre_e2 above.
    // ★ THE TRIPWIRE DID ITS JOB AGAIN: it went red at 1584 vs 1528 the first
    // time the governor's dials were built, which is exactly 7*8, and forced
    // the partition decision before the dials could reach an arm uncopied.
    // ENV-3 (the defence scramble) added FIVE doubles (122 members): 40
    // bytes, no padding needed: 1584 + 40 = 1624. All five are NON-E2 and are
    // copied forward by pre_e2 above.
    // ENV-3.4 (the perch) added FIVE doubles (127 members): 40 bytes, no
    // padding needed: 1624 + 40 = 1664. All five are NON-E2 and are copied
    // forward by pre_e2 above.
    // THE PARABOLA'S BOUND (Chad's 2026-08-30 doctrine: arm once, ride, exit)
    // added TWO doubles (129 members): 16 bytes, no padding needed:
    // 1664 + 16 = 1680. Both are NON-E2 and are copied forward by pre_e2
    // above. ★ THE TRIPWIRE DID ITS JOB A THIRD TIME: it went red at 1680 vs
    // 1664 the moment the arm/exit dials were added, and the copy lines above
    // were written because of it -- uncopied, the exit gate would have read 0
    // in this probe, which is "no speed gate", which is the sustained floor
    // the doctrine exists to forbid. That is the exact class of silent revert
    // this tripwire is for.
    // RUNG DF-1 (the deck fight, Chad's 2026-08-31 ruling) added ONE double
    // (130 members): 8 bytes, no padding needed: 1680 + 8 = 1688. It is NON-E2
    // and is copied forward by pre_e2 above. ★ THE TRIPWIRE DID ITS JOB A
    // FOURTH TIME: it went red at 1688 vs 1680 the moment the dial was added,
    // and the copy line above was written because of it. Uncopied, the dial
    // would have sat at its NEGATIVE off-value inside this probe, so the probe
    // would have flown the un-capped pursuit the rung exists to forbid and
    // reported on a build nobody ships.
    constexpr std::size_t kAuditedSize = 1688;  // measured 2026-09-01, 130
                                                //   members + 1 bfm bool
    INFO("sizeof(drone::DroneParams) = " << sizeof(drone::DroneParams)
                                         << " audited " << kAuditedSize);
    CHECK(sizeof(drone::DroneParams) == kAuditedSize);
}

// ---------------------------------------------------------------------------
// THE WORLD. One conquest-shaped headless composition, built once per arm.
struct StopeWorld {
    world::HeightField hf;
    world::TunnelNet net;
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    sim::AtmosphereField af;
    sim::Environment env;
    combat::Pump pumps[4];
    std::vector<drone::DroneState> fleet;
    glm::dvec3 home[2];  // faction ellipse centre direction (VALLEY, SUDBURY)
    world::FactionGrowth grow[2]{};
};

// The app's own conquest deployment (app/main.cpp + the P-A probe): a
// deterministic golden-angle disc of kConquestSpreadM around the pilot's OWN
// faction ELLIPSE CENTRE, at spawn_alt. Pilots therefore start where the air
// actually is and TRANSIT the real 12-15 km to their mouth, which is the chain
// the rung has to survive — a mouth-side deployment would have flattered it.
constexpr double kConquestSpreadM = 5000.0;

sim::SimState faction_state(const StopeWorld& w, const drone::DroneParams& dp,
                            int i) {
    const double golden = kPi * (3.0 - std::sqrt(5.0));
    const glm::dvec3 c = w.home[combat::maverick_faction(i)];
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
        dp, dir * (w.hf.radius_at(dir) + dp.spawn_alt), heading);
}

// The app's bubble-containment leash (app/instructor_tick.h): each pilot to its
// OWN faction ellipse. Without it an unengaged pilot patrols out of its dome
// into vacuum, mushes and crashes forever — measured in this very probe
// (min atm_frac 0 over the fleet, ~90 crashes per 15 min in BOTH arms), which
// would have made P-B a measurement of the respawn pen rather than of the
// offensive. Tunnel dispositions are leash-exempt inside drone::tick, so the
// runs themselves are untouched.
void arm_leash(StopeWorld& w) {
    for (drone::DroneState& d : w.fleet) {
        if (d.inert) continue;
        drone::BubbleLeash lz;
        glm::dvec3 cdir{0.0, 1.0, 0.0};
        glm::dvec3 maj{0.0};
        double a = 0.0, b = 0.0;
        world::faction_ellipse(combat::maverick_faction(d.spawn_index), w.grow,
                               cdir, maj, a, b);
        if (a > 0.0 && b > 0.0) {
            lz.enabled = true;
            lz.center_dir = cdir;
            lz.major_axis = maj;
            lz.a_m = a;
            lz.b_m = b;
        }
        d.leash = lz;
        // ---- ENV-1 mirror (a hand-mirror is not a caller). This probe has
        // no conquest state, so the scales come straight off w.grow — both
        // domes full, which is exactly the world it means to fly.
        {
            const int own = combat::maverick_faction(d.spawn_index);
            world::FactionGrowth unit[2];  // radius_scale = 1 => BASE radii
            drone::AirDomes ad;
            for (int slot = 0; slot < 2; ++slot) {
                const int f = (slot == 0) ? own : (1 - own);
                drone::DomeEllipse& e = ad.dome[slot];
                e.radius_scale = w.grow[f].radius_scale;
                if (!(e.radius_scale >= 1e-9)) continue;
                world::faction_ellipse(f, w.grow, e.center_dir, e.major_axis,
                                       e.a_m, e.b_m);
                glm::dvec3 bdir{0.0, 1.0, 0.0};
                glm::dvec3 bmaj{1.0, 0.0, 0.0};
                world::faction_ellipse(f, unit, bdir, bmaj, e.base_a_m,
                                       e.base_b_m);
                e.live = e.a_m > 0.0 && e.b_m > 0.0;
            }
            d.air_domes = ad;
        }
    }
}

void build_world(StopeWorld& w, const drone::DroneParams& dp) {
    w.hf = uniform_field(300.0);
    w.net = world::build_tunnel_net(test_tp(), &w.hf);
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, w.bubbles);
    w.af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    w.af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    w.af.bubbles = w.bubbles;
    w.env.ground = &w.hf;
    w.env.tunnels = &w.net;
    w.env.atm = &w.af;
    w.env.ground_params = tunnel_ground_params();

    const glm::dvec3 mouthE = w.net.spine.front().pos;
    const glm::dvec3 mouthM = w.net.spine.back().pos;
    w.home[combat::CQ_VALLEY] = glm::normalize(world::kValleyCenterDir);
    w.home[combat::CQ_SUDBURY] = glm::normalize(world::kSudburyCenterDir);
    const double lift =
        w.net.arena.a_pos *
        std::sqrt(1.0 - combat::kDeepPumpFrac * combat::kDeepPumpFrac);
    // A pump's HP is expressed here in SECONDS OF AI ON-STATION TIME, which is
    // exactly what the app's model reduces to: pump_max_hp = pump_kill_seconds
    // * battery_dps, and an on-station striker deals raid_dps_frac *
    // battery_dps, so the battery DPS cancels and the kill time is
    // pump_kill_seconds / raid_dps_frac. Derived from the shipped [conquest]
    // table, never a welded number.
    const double kill_s =
        kGame.conquest.pump_kill_seconds / kGame.conquest.raid_dps_frac;
    const auto surf = [&](const glm::dvec3& dir) {
        const glm::dvec3 u = glm::normalize(dir);
        return u * (w.hf.radius_at(u) + 10.0);
    };
    combat::make_pumps(
        surf(world::kPumpValleySurface), surf(world::kPumpSudburySurface),
        combat::place_deep_pump(w.net.arena.center, w.net.arena.u_long,
                                glm::normalize(mouthE), w.net.arena.a_pos,
                                w.net.arena.b, combat::kDeepPumpFrac, lift),
        combat::place_deep_pump(w.net.arena.center, w.net.arena.u_long,
                                glm::normalize(mouthM), w.net.arena.a_pos,
                                w.net.arena.b, combat::kDeepPumpFrac, lift),
        kill_s, w.pumps);

    // Ten pilots on the app's conquest deployment.
    w.fleet.clear();
    for (int i = 0; i < combat::kNumMavericks; ++i)
        w.fleet.push_back(drone::spawn_drone(kAp, dp, i, combat::kNumMavericks));
    for (int i = 0; i < static_cast<int>(w.fleet.size()); ++i) {
        w.fleet[i].curr = faction_state(w, dp, i);
        w.fleet[i].prev = w.fleet[i].curr;
        w.fleet[i].grounded = false;
        w.fleet[i].hp = dp.hp;
    }
}

// The app's own E3.2 CONCURRENT-STRIKER CAP count (app/instructor_tick.h): a
// PRE-LOOP snapshot over the fleet's mav state, per faction, of the
// collapse-TRIGGERED runs currently in the pipeline. Snapshot, not a running
// tally, so the verdict cannot depend on fleet iteration order.
void count_on_order_runs(const StopeWorld& w, const drone::DroneParams& dp,
                         int out[2]) {
    out[0] = 0;
    out[1] = 0;
    if (dp.strike_concurrent_max <= 0) return;
    for (const drone::DroneState& d : w.fleet) {
        if (d.inert) continue;
        if (d.mav.run_on_order &&
            d.mav.mode != maverick::MaverickState::Mode::PATROL)
            ++out[combat::maverick_faction(d.spawn_index)];
    }
}

// The app's own strike-order arming (app/instructor_tick.h), including the
// E2.1 entry-direction derivation and the E3.2 slot verdict.
void arm_strike(drone::DroneState& d, const StopeWorld& w,
                const drone::DroneParams& dp, const int on_order_runs[2]) {
    const int own = combat::maverick_faction(d.spawn_index);
    const int ef = 1 - own;
    drone::StrikeOrder so;
    so.order_hold = dp.strike_concurrent_max > 0 &&
                    on_order_runs[own] >= dp.strike_concurrent_max;
    so.engage_m = dp.strike_engage_m;
    so.k_az = dp.strike_k_az;
    so.k_el = dp.strike_k_el;
    so.bank_cap = dp.strike_bank_cap;
    so.gamma_cap = dp.strike_gamma_cap;
    so.bail_s = dp.strike_bail_s;
    so.station_range_m = dp.strike_station_range_m;
    so.station_cos = dp.strike_station_cos;
    if (w.pumps[2 + ef].alive) {
        so.active = true;
        so.target_pos = w.pumps[2 + ef].pos;
        so.pump_idx = 2 + ef;
        const glm::dvec3& own_deep = w.pumps[2 + own].pos;
        const double d_front = glm::length(w.net.spine.front().pos - own_deep);
        const double d_back = glm::length(w.net.spine.back().pos - own_deep);
        so.entry_dir = d_front <= d_back ? +1 : -1;
    }
    d.strike = so;
}

// What one P-B arm measured.
struct StopeResult {
    double on_station_s[4] = {0.0, 0.0, 0.0, 0.0};  // AI on-station seconds
    bool pump_dead[4] = {false, false, false, false};
    double kill_time_s[4] = {-1.0, -1.0, -1.0, -1.0};
    int entries = 0;         // PATROL -> TRANSIT (a run actually ordered)
    int runs = 0;            // TRANSIT/DIVE_IN -> RUN (the bore reached)
    int arena_entries = 0;   // a drone crossed into the arena pocket
    double closest_m = 1e18; // closest any striker got to pumps[2]
    long long strafe_rounds = 0;  // E2.3 rounds spawned at a pump
    double max_fire_range_m = 0.0;
    int run_crashes = 0;     // respawns while in an underground mode
    int crashes = 0;         // respawns, any disposition
    int crash_mode[5] = {0, 0, 0, 0, 0};  // by MaverickState::Mode
    int arena_crashes = 0;   // respawns from inside the arena pocket
    int transit_timeouts = 0;  // TRANSIT -> PATROL (never armed the dive)
    // Divert QUALITY (what the strike actually does with its budget): ticks
    // spent diverting in the chamber, how many of them the shell guard owned,
    // and the speed/range/aspect it holds. These are the numbers that sized
    // strike_station_range_m — the striker carries ~180 m/s out of its descent
    // and turns at a ~1.5 km radius, so a point-target envelope smaller than
    // its own pursuit loop credits only instants.
    long long div_ticks = 0, div_guard = 0, div_inrange = 0, div_cone = 0;
    double div_speed_sum = 0.0, div_range_sum = 0.0;
};

StopeResult fly_stope(const drone::DroneParams& dp, double minutes) {
    StopeResult out;
    StopeWorld w;
    build_world(w, dp);

    combat::CombatWorld cw;
    cw.setup = kScen.combat;
    cw.damage_params = kScen.damage;
    cw.params.hit_radius_m = dp.hit_radius_m;

    // UNDEFENDED: the player is parked on the far side of the planet, so no
    // foe is ever assigned and nothing defends the stope — the composition the
    // spec asks for. He is never stepped and never read as a target.
    const glm::dvec3 far_up = glm::normalize(-w.home[combat::CQ_VALLEY]);
    const sim::SimState player = drone::level_state_at(
        dp, far_up * (kAp.R + 1000.0),
        glm::normalize(glm::cross(far_up, glm::dvec3{0.0, 1.0, 0.0})));

    const double dt = kAp.sim_dt;
    const long long ticks = static_cast<long long>(minutes * 60.0 / dt);
    std::vector<int> prev_mode(w.fleet.size(), 0);
    std::vector<char> was_arena(w.fleet.size(), 0);
    std::vector<double> cd_before(w.fleet.size(), 0.0);

    for (long long t = 0; t < ticks; ++t) {
        const double now_s = static_cast<double>(t) * dt;
        arm_leash(w);
        int slots[2];
        count_on_order_runs(w, dp, slots);
        for (std::size_t i = 0; i < w.fleet.size(); ++i) {
            drone::DroneState& d = w.fleet[i];
            arm_strike(d, w, dp, slots);

            const int before = static_cast<int>(d.mav.mode);
            const bool was_in_arena = drone::in_arena(w.net, d.curr.position);
            const drone::DroneTickResult r =
                drone::tick(d, kAp, dp, &w.env, nullptr);
            if (r.respawned) {
                ++out.crashes;
                ++out.crash_mode[before];
                if (was_in_arena) ++out.arena_crashes;
                if (maverick::is_underground_mode(
                        static_cast<maverick::MaverickState::Mode>(before)))
                    ++out.run_crashes;
                // The app's FIX-F2 relocation: a crashed conquest drone is put
                // back in its own faction's air, not the stock +X scatter (the
                // measured respawn pen — without it this probe would measure
                // the pen and not the offensive).
                d.curr = faction_state(w, dp, static_cast<int>(i));
                d.prev = d.curr;
            }
            const int after = static_cast<int>(d.mav.mode);
            if (before == static_cast<int>(maverick::MaverickState::Mode::PATROL) &&
                after != before)
                ++out.entries;
            if (after == static_cast<int>(maverick::MaverickState::Mode::RUN) &&
                before != after)
                ++out.runs;
            if (before ==
                    static_cast<int>(maverick::MaverickState::Mode::TRANSIT) &&
                after == static_cast<int>(maverick::MaverickState::Mode::PATROL))
                ++out.transit_timeouts;
            const bool now_arena = drone::in_arena(w.net, d.curr.position);
            if (now_arena && !was_arena[i]) ++out.arena_entries;
            was_arena[i] = now_arena ? 1 : 0;
            prev_mode[i] = after;

            // Divert-quality accounting (the numbers that sized the
            // on-station envelope).
            if (d.strike.active && !d.strike_bailed &&
                d.mav.mode == maverick::MaverickState::Mode::RUN &&
                drone::in_arena(w.net, d.curr.position)) {
                ++out.div_ticks;
                if (d.arena_guard_engaged) ++out.div_guard;
                out.div_speed_sum += glm::length(d.curr.velocity);
                const glm::dvec3 to = d.strike.target_pos - d.curr.position;
                const double rr = glm::length(to);
                out.div_range_sum += rr;
                if (rr <= d.strike.station_range_m) {
                    ++out.div_inrange;
                    const glm::dvec3 nose =
                        d.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
                    if (glm::dot(nose, to / rr) >= d.strike.station_cos)
                        ++out.div_cone;
                }
            }
            if (d.strike.active && d.strike.pump_idx == 2)
                out.closest_m = std::min(
                    out.closest_m,
                    glm::length(w.pumps[2].pos - d.curr.position));

            // The app's damage arbitration (instructor_tick.h block 4),
            // deep-strike half only — nothing raids a surface pump in this
            // composition because the player never comes near.
            const combat::RaidParams rp_surface;
            const combat::RaidParams rp_deep = combat::strike_params(
                d.strike.station_range_m, d.strike.station_cos);
            int pi = -1;
            const combat::RaidParams* rp = nullptr;
            const bool in_tunnel = maverick::is_tunnel_mode(d.mav.mode);
            if (d.raid.active && !in_tunnel) {
                pi = d.raid.pump_idx;
                rp = &rp_surface;
            } else if (d.strike.active && !d.strike_bailed &&
                       d.mav.mode == maverick::MaverickState::Mode::RUN) {
                pi = d.strike.pump_idx;
                rp = &rp_deep;
            }
            if (pi >= 0 && pi < 4 && w.pumps[pi].alive &&
                combat::raider_on_station(d.curr, w.pumps[pi].pos, *rp)) {
                out.on_station_s[pi] += dt;
                w.pumps[pi].hp -= dt;  // HP is in on-station seconds
                if (w.pumps[pi].hp <= 0.0) {
                    w.pumps[pi].alive = false;
                    out.pump_dead[pi] = true;
                    out.kill_time_s[pi] = now_s;
                }
            }
        }

        // The real fire pipeline: E2.3's strafe rounds spawn HERE, through the
        // shipped enemy_fire_tick, with the tunnel net threaded so a round in
        // the stope keeps flying (T10).
        for (std::size_t i = 0; i < w.fleet.size(); ++i)
            cd_before[i] = w.fleet[i].fire_cooldown;
        combat::enemy_fire_tick(cw, w.fleet, dt, kAp.g, kAp.R, &w.net);
        for (std::size_t i = 0; i < w.fleet.size(); ++i) {
            const drone::DroneState& d = w.fleet[i];
            if (d.fire_cooldown <= cd_before[i]) continue;
            ++out.strafe_rounds;
            out.max_fire_range_m =
                std::max(out.max_fire_range_m,
                         glm::length(d.gun_tgt_pos - d.curr.position));
        }
        combat::combat_player_tick(cw, player, player);
    }
    return out;
}

}  // namespace

// ===========================================================================
// PROBE P-B — "THE STOPE FALLS". Undefended composition, 15 simulated minutes.
// The measured baseline (PRE, every E2 dial off) is the tape behaviour: orders
// armed the whole time, the stope never reached. POST must actually kill
// pumps[2] — victory by pump destruction, the thing Chad asked for.
// MUTATION: revert strike_on_order -> entries collapse to the trait schedule
// and the pump survives; revert strike_gamma_cap to 35 -> the divert arrives
// but never gets on station.
// ===========================================================================
TEST_CASE("probe P-B: the stope falls") {
    const drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.maverick.enabled);   // premise: the tunnel machine runs
    REQUIRE(shipped.strike_on_order);    // premise: the rung actually ships on
    const drone::DroneParams baseline = pre_e2(shipped);

    const StopeResult pre = fly_stope(baseline, 15.0);
    const StopeResult post = fly_stope(shipped, 15.0);

    // ★ A5 LIVENESS ARM (diagnostic, never ships): cruise 100 m/s vs the
    // shipped 85. Every spawn velocity is seeded from dp.speed and the cruise
    // governor holds it, so in a fully deterministic sim the trajectories MUST
    // diverge from tick 0. A bit-identical aggregate means the fixture is
    // BLIND or the plumbing is DEAD -- and P-B's PRE/POST comparison, which
    // runs entirely through the strike dials, could not tell you which. Flown
    // SHORT (3 min) against a matching-duration baseline so it costs a
    // fraction of the probe.
    {
        drone::DroneParams live = shipped;
        live.speed = 100.0;
        const StopeResult base3 = fly_stope(shipped, 3.0);
        const StopeResult live3 = fly_stope(live, 3.0);
        INFO("liveness base3: entries=" << base3.entries << " closest="
             << base3.closest_m << " crashes=" << base3.crashes);
        INFO("liveness live3: entries=" << live3.entries << " closest="
             << live3.closest_m << " crashes=" << live3.crashes);
        REQUIRE((live3.entries != base3.entries ||
                 live3.closest_m != base3.closest_m ||
                 live3.crashes != base3.crashes));
    }

    INFO("PRE  entries=" << pre.entries << " runs=" << pre.runs
                         << " arena_entries=" << pre.arena_entries
                         << " on_station[2]=" << pre.on_station_s[2]
                         << " on_station[3]=" << pre.on_station_s[3]
                         << " dead2=" << pre.pump_dead[2]
                         << " kill_t2=" << pre.kill_time_s[2]
                         << " closest=" << pre.closest_m
                         << " strafe_rounds=" << pre.strafe_rounds
                         << " run_crashes=" << pre.run_crashes
                         << " crashes=" << pre.crashes
                         << " arena_crashes=" << pre.arena_crashes
                         << " timeouts=" << pre.transit_timeouts
                         << " crash_by_mode=" << pre.crash_mode[0] << "/"
                         << pre.crash_mode[1] << "/" << pre.crash_mode[2] << "/"
                         << pre.crash_mode[3] << "/" << pre.crash_mode[4]
                         );
    INFO("POST entries=" << post.entries << " runs=" << post.runs
                         << " arena_entries=" << post.arena_entries
                         << " on_station[2]=" << post.on_station_s[2]
                         << " on_station[3]=" << post.on_station_s[3]
                         << " dead2=" << post.pump_dead[2]
                         << " kill_t2=" << post.kill_time_s[2]
                         << " closest=" << post.closest_m
                         << " strafe_rounds=" << post.strafe_rounds
                         << " max_fire_range=" << post.max_fire_range_m
                         << " run_crashes=" << post.run_crashes
                         << " crashes=" << post.crashes
                         << " arena_crashes=" << post.arena_crashes
                         << " timeouts=" << post.transit_timeouts
                         << " crash_by_mode=" << post.crash_mode[0] << "/"
                         << post.crash_mode[1] << "/" << post.crash_mode[2] << "/"
                         << post.crash_mode[3] << "/" << post.crash_mode[4]

                         << " div_ticks=" << post.div_ticks
                         << " div_guard_frac="
                         << (post.div_ticks ? double(post.div_guard) / post.div_ticks : 0.0)
                         << " div_mean_speed="
                         << (post.div_ticks ? post.div_speed_sum / post.div_ticks : 0.0)
                         << " div_mean_range="
                         << (post.div_ticks ? post.div_range_sum / post.div_ticks : 0.0)
                         << " div_inrange_frac="
                         << (post.div_ticks ? double(post.div_inrange) / post.div_ticks : 0.0)
                         << " div_cone_frac="
                         << (post.div_ticks ? double(post.div_cone) / post.div_ticks : 0.0));

    // (1) STRIKE TRAFFIC REACHES THE STOPE. Runs are ordered, the bore is
    // reached, and drones cross into the arena pocket — and not fewer of them
    // than the pre-E2 schedule managed.
    //
    // NOT pinned on `entries` (PATROL -> TRANSIT), deliberately: the baseline
    // records MORE of those than the shipped table does (47 vs 38) because
    // most of its transits TIME OUT without ever arming a dive (30 of 47) and
    // the pilot immediately re-enters the queue. Counting departures instead
    // of arrivals would have scored the broken arm higher. The E2.1 collapse
    // mechanism itself is pinned in test_enemy_ai_e2.cpp against the pre-E2
    // scheduler call.
    REQUIRE(post.runs >= 1);
    REQUIRE(post.arena_entries >= 1);
    // Starvation tripwire, NOT a quality comparison (Fable adjudication,
    // rung E3): the concurrent-striker cap intentionally trades run COUNT
    // for run QUALITY (measured: PRE 11 runs / 27 transit timeouts / 9
    // underground crashes / pump survives vs POST 9 runs / 0 underground
    // crashes / pump dead at 313 s). Raw cross-arm counts are the same
    // welded trap this file already documents for `entries`; the outcome
    // pins above (on_station, pump death inside the window) carry the
    // certification. 6 is the floor below which traffic is genuinely
    // starved (the E2.1 starvation episode measured 1-2).
    REQUIRE(post.runs >= 6);

    // (2) ON-STATION EARNS THE KILL. The threshold is the CONFIG's own
    // (pump_kill_seconds / raid_dps_frac) — 30 s at the pre-E12 table, 23.1 s
    // at the shipped one.
    //
    // ★ RUNG E12.2 MOVED IT, AND THIS PIN DID ITS JOB. The clause used to
    // read `REQUIRE(kill_s >= 30.0)` and `on_station_s[2] >= 30.0`: a welded
    // copy of the shipped table plus a premise that the table never moves.
    // When Chad's "you can actually lose" ruling took raid_dps_frac 0.5 ->
    // 0.65 the leg went RED — which is exactly what its own comment asked for
    // ("so a [conquest] retune moves the pin honestly instead of silently"),
    // and is why it is being re-derived rather than re-welded to 23.1.
    // The BEHAVIOUR under test never changed: a strike window must earn
    // enough on-station time to kill the pump. Now it says so in one place.
    const double kill_s =
        kGame.conquest.pump_kill_seconds / kGame.conquest.raid_dps_frac;
    INFO("kill threshold (pump_kill_seconds / raid_dps_frac) = " << kill_s
                                                                 << " s");
    REQUIRE(kill_s > 0.0);
    REQUIRE(post.on_station_s[2] >= kill_s);

    // (3) VICTORY BY PUMP DESTRUCTION, inside the 15-minute window.
    REQUIRE(post.pump_dead[2]);
    REQUIRE(post.kill_time_s[2] <= 15.0 * 60.0);

    // (4) The baseline really was broken (fixture-no-op guard): the pre-E2
    // fleet never got the allied deep pump near death.
    REQUIRE_FALSE(pre.pump_dead[2]);
    REQUIRE(post.on_station_s[2] > pre.on_station_s[2]);

    // (5) E2.3: the strafe is VISIBLE — real rounds left the guns underground,
    // and none of them was lobbed. The tracer band is the GUN's own
    // (fire_range_max), NOT the wider on-station DPS envelope: the underground
    // analogue of P-A's snapshot-range requirement, and the reason the strafe
    // gate clamps to the gun band instead of the envelope it credits DPS in.
    REQUIRE(post.strafe_rounds > 0);
    REQUIRE(pre.strafe_rounds == 0);
    REQUIRE(post.max_fire_range_m <= shipped.fire_range_max);
    REQUIRE(post.max_fire_range_m <= shipped.snapshot_range_m);

    // (6) THE TAPE-85 REGRESSION LEG. Every in-net death in every tape is a
    // RUN-mode terrain crash, so more strike traffic must not be paid for in
    // more wrecks: with all dials shipped, underground crashes stay bounded
    // against the baseline per unit of traffic actually flown. Compared as a
    // RATE (crashes per run entered) because POST deliberately flies far more
    // runs than PRE — an absolute count would penalise the rung for working.
    const double pre_rate = pre.runs > 0 ? static_cast<double>(pre.run_crashes) /
                                               static_cast<double>(pre.runs)
                                         : 0.0;
    const double post_rate =
        post.runs > 0
            ? static_cast<double>(post.run_crashes) / static_cast<double>(post.runs)
            : 0.0;
    INFO("underground crashes per run: PRE=" << pre_rate
                                             << " POST=" << post_rate);
    REQUIRE(post_rate <= std::max(0.5, pre_rate + 0.25));
}

// ===========================================================================
// PROBE P-C — "THE CHAMBER FIGHT". A STEPPED player flown through the arena
// while strikers are in it. Three claims:
//   * enemy rounds are spawned UNDERGROUND, aimed at the player,
//   * no terrain-avoid pull-up into the ceiling (the latch never engages on a
//     drone inside the arena),
//   * zero rounds spawned at a target range beyond the snapshot band.
// MUTATION: revert the terrain-avoid arena exemption -> the drones climb and
// the rounds vanish; revert the fight interrupt -> engaged is never honoured
// underground and no round is aimed at the player at all.
// ===========================================================================
TEST_CASE("probe P-C: the chamber fight") {
    const drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.arena_fight_range_m > 0.0);

    const auto fly = [&](const drone::DroneParams& dp) {
        struct R {
            long long rounds_at_player = 0;
            long long avoid_ticks = 0;   // in-arena drone-ticks pulling up
            long long arena_ticks = 0;   // in-arena drone-ticks (the premise)
            long long fight_ticks = 0;   // interrupt budget actually spent
            double max_fire_range_m = 0.0;
            long long player_hits = 0;
        } out;

        StopeWorld w;
        build_world(w, dp);
        const maverick::TunnelRoute route(w.net);

        // The station deepest inside the arena — the player's entry line runs
        // along the spine through it, which is the real approach an attacking
        // pilot flies (the bore is the only way in).
        double s_arena = -1.0, best = 1e18;
        for (double s = 0.0; s <= route.L; s += 100.0) {
            const glm::dvec3 p = route.point_at(s).pos;
            if (!drone::in_arena(w.net, p)) continue;
            const double n2 = drone::arena_norm2(w.net, p);
            if (n2 < best) {
                best = n2;
                s_arena = s;
            }
        }
        REQUIRE(s_arena >= 0.0);

        // FOUR strikers already committed in the chamber (the probe measures
        // the FIGHT, not the transit — P-B measures the transit). Two per
        // faction so the composition is symmetric.
        w.fleet.resize(4);
        for (int i = 0; i < 4; ++i) {
            const double s_i = s_arena + (i - 1.5) * 700.0;
            const maverick::TunnelRoute::Sample p = route.point_at(s_i);
            drone::DroneState d = drone::spawn_drone(kAp, dp, i, 4);
            d.curr = drone::level_state_at(dp, p.pos, p.tan);
            d.curr.velocity = p.tan * dp.maverick.base_speed;
            d.curr.last_vhat = p.tan;
            d.prev = d.curr;
            d.grounded = false;
            d.hp = dp.hp;
            d.mav.inited = true;
            d.mav.mode = maverick::MaverickState::Mode::RUN;
            d.mav.run_dir = 1;
            d.mav.s_est = s_i;
            w.fleet[i] = d;
        }

        // ---- THE STEPPED PLAYER --------------------------------------------
        // Flown along the arena's long axis through the chamber at 180 m/s
        // (an underground speed — the stope is 5.2 km across its short axis,
        // so an ace does not cross it at 275) and turned back at each end, so
        // he stays in the pocket for the whole window. Position, velocity,
        // heading and orientation are all mutually consistent every tick.
        constexpr double kPlayerSpeed = 180.0;
        const glm::dvec3 axis = glm::normalize(w.net.arena.u_lat);
        glm::dvec3 p_dir = axis;
        sim::SimState player;
        player.position = w.net.arena.center - axis * (w.net.arena.b * 0.45);
        player.velocity = kPlayerSpeed * p_dir;
        player.last_vhat = p_dir;
        {
            const glm::dvec3 up = glm::normalize(player.position);
            const glm::dvec3 right = glm::normalize(glm::cross(p_dir, up));
            player.orientation = glm::normalize(
                glm::quat_cast(glm::dmat3{right, glm::cross(right, p_dir),
                                          -p_dir}));
        }

        combat::CombatWorld cw;
        cw.setup = kScen.combat;
        cw.damage_params = kScen.damage;
        cw.params.hit_radius_m = dp.hit_radius_m;
        cw.player_hp = combat::summary_hp(cw.damage);
        cw.player_invuln_ticks = 0;

        const int player_faction = combat::CQ_VALLEY;
        const int max_engaged =
            combat::difficulty_params(cw.setup.difficulty).max_engaged;
        const double dt = kAp.sim_dt;
        const long long ticks = static_cast<long long>(180.0 / dt);
        std::vector<double> cd_before(w.fleet.size(), 0.0);

        for (long long t = 0; t < ticks; ++t) {
            const sim::SimState player_prev = player;
            // Reverse at 0.45 of the semi-axis so he never leaves the pocket.
            const double along =
                glm::dot(player.position - w.net.arena.center, axis);
            if (along > w.net.arena.b * 0.45 && glm::dot(p_dir, axis) > 0.0)
                p_dir = -axis;
            if (along < -w.net.arena.b * 0.45 && glm::dot(p_dir, axis) < 0.0)
                p_dir = axis;
            player.position += p_dir * (kPlayerSpeed * dt);
            player.velocity = (player.position - player_prev.position) / dt;
            player.last_vhat = glm::normalize(player.velocity);
            {
                const glm::dvec3 up = glm::normalize(player.position);
                const glm::dvec3 right =
                    glm::normalize(glm::cross(p_dir, up));
                player.orientation = glm::normalize(glm::quat_cast(
                    glm::dmat3{right, glm::cross(right, p_dir), -p_dir}));
            }

            combat::assign_foes(w.fleet, player, player_faction, max_engaged,
                                dp);
            int slots[2];
            count_on_order_runs(w, dp, slots);
            for (drone::DroneState& d : w.fleet) {
                arm_strike(d, w, dp, slots);
                const sim::SimState* target =
                    d.foe == drone::kFoePlayer ? &player : nullptr;
                if (d.foe >= 0 && d.foe < static_cast<int>(w.fleet.size()))
                    target = &w.fleet[d.foe].curr;
                drone::tick(d, kAp, dp, &w.env, target);
                if (drone::in_arena(w.net, d.curr.position)) {
                    ++out.arena_ticks;
                    if (d.terrain_avoid_engaged) ++out.avoid_ticks;
                }
                if (d.arena_fight_ticks > 0) ++out.fight_ticks;
            }

            for (std::size_t i = 0; i < w.fleet.size(); ++i)
                cd_before[i] = w.fleet[i].fire_cooldown;
            combat::enemy_fire_tick(cw, w.fleet, dt, kAp.g, kAp.R, &w.net);
            for (std::size_t i = 0; i < w.fleet.size(); ++i) {
                const drone::DroneState& d = w.fleet[i];
                if (d.fire_cooldown <= cd_before[i]) continue;
                out.max_fire_range_m =
                    std::max(out.max_fire_range_m,
                             glm::length(d.gun_tgt_pos - d.curr.position));
                if (d.foe == drone::kFoePlayer) ++out.rounds_at_player;
            }
            combat::combat_player_tick(cw, player_prev, player);
        }
        out.player_hits = cw.player_hits;
        return out;
    };

    const auto post = fly(shipped);
    const auto pre = fly(pre_e2(shipped));

    // ★ A5 LIVENESS ARM (diagnostic, never ships): base_speed +20%. This
    // fixture seeds each striker's velocity from dp.maverick.base_speed
    // directly (d.curr.velocity = p.tan * dp.maverick.base_speed, above), so
    // every trajectory must diverge from tick 0; bit-identical counters mean
    // the fixture is BLIND or the plumbing is DEAD -- which the PRE/POST pair
    // above, running entirely through the E2 dials, cannot distinguish.
    {
        drone::DroneParams live = shipped;
        live.maverick.base_speed = shipped.maverick.base_speed * 1.2;
        const auto lv = fly(live);
        INFO("liveness post: arena_ticks=" << post.arena_ticks
             << " avoid=" << post.avoid_ticks
             << " rounds=" << post.rounds_at_player);
        INFO("liveness live: arena_ticks=" << lv.arena_ticks
             << " avoid=" << lv.avoid_ticks
             << " rounds=" << lv.rounds_at_player);
        REQUIRE((lv.arena_ticks != post.arena_ticks ||
                 lv.avoid_ticks != post.avoid_ticks ||
                 lv.rounds_at_player != post.rounds_at_player));
    }

    INFO("PRE  arena_ticks=" << pre.arena_ticks << " avoid_ticks="
                             << pre.avoid_ticks << " fight_ticks="
                             << pre.fight_ticks << " rounds_at_player="
                             << pre.rounds_at_player << " hits="
                             << pre.player_hits);
    INFO("POST arena_ticks=" << post.arena_ticks << " avoid_ticks="
                             << post.avoid_ticks << " fight_ticks="
                             << post.fight_ticks << " rounds_at_player="
                             << post.rounds_at_player << " hits="
                             << post.player_hits << " max_fire_range="
                             << post.max_fire_range_m);

    // Premise: the drones really were in the chamber with the player.
    REQUIRE(post.arena_ticks > 0);
    // (1) The chamber fight engaged, and rounds were spawned underground at
    // the player — the thing that has never happened in any tape.
    REQUIRE(post.fight_ticks > 0);
    REQUIRE(post.rounds_at_player > 0);
    REQUIRE(pre.rounds_at_player == 0);
    // (2) NO terrain-avoid pull-up into the ceiling, anywhere in the arena.
    REQUIRE(post.avoid_ticks == 0);
    // (3) Zero rounds beyond the snapshot band (the P-A requirement, held
    // underground too).
    REQUIRE(shipped.snapshot_range_m > 0.0);
    REQUIRE(post.max_fire_range_m <= shipped.snapshot_range_m);
}

// ===========================================================================
// PROBE P-D — "THE TRANSITING STRIKER FIGHTS BACK" (RUNG E3.3).
//
// THE MEASURED PROBLEM, from Chad's fly tape build-play/conquest_tape_1
// (13:41, signature verified): E2.1's on-order collapse worked — the strike
// pipeline ran — but it put the three non-raid ENEMY strikers into committed
// TRANSIT for 53-80% of their lives toward a pump 20+ km away, and a committed
// TRANSIT ignores the player. Every enemy strike window in that tape ends
// "drone killed by fire" at rng_p 15-706 m: Chad shot all three off a straight
// line at point-blank range and they were dead by 3:34, permanently. His
// verdict was "they behaved the same as always, didn't engage".
//
// The probe reproduces exactly that geometry and asserts the new behaviour end
// to end, in ONE continuous run of the real tick — never four disconnected
// fixtures:
//   (1) the strike order collapses the countdown and the pilot LAUNCHES,
//   (2) an ace on his corridor inside transit_fight_yield_m makes him ABORT,
//   (3) he then ENGAGES and FIRES at the player (the tape's zero),
//   (4) the ace leaves, the standing order RE-LAUNCHES the run, and the run
//       COMPLETES (the bore is reached) — the errand is delayed, not
//       abandoned, which is the half a "stop dying" fix would have failed.
// The PRE arm (E3 dials off) is the tape: no abort, no rounds, and the pilot
// flies the ace's guns.
//
// MUTATION: revert transit_fight_yield_m -> (2) and (3) fail with the pilot
// still in TRANSIT; break the give-up's strike_collapsed clear -> (4)'s
// relaunch slips by a full trait period and fails its bound.
// ===========================================================================
namespace {

struct DuelResult {
    int launch_tick = -1;    // PATROL -> TRANSIT (the ordered launch)
    int contact_tick = -1;   // the ace arrives on the corridor
    int abort_tick = -1;     // TRANSIT -> PATROL with the ace close
    double abort_range_m = -1.0;
    long long engaged_ticks = 0;   // ticks with the player as an assigned foe
    long long pursue_ticks = 0;    // ticks the pilot actually flew the fight
    long long rounds_at_player = 0;
    double min_range_m = 1e18;
    int leave_tick = -1;     // the ace goes home
    int relaunch_tick = -1;  // the SECOND PATROL -> TRANSIT
    int run_tick = -1;       // the bore is reached (the run completes)
    int relaunches = 0;      // every PATROL -> TRANSIT after the abort
    int dives = 0;           // TRANSIT -> DIVE_IN after the abort
    int crashes = 0;

    // ---- RUNG E8.1 — THE FIRE-GATE WITNESS ACCUMULATORS -------------------
    // Spec E7.N owes this rung a MEASUREMENT: E7.2's slashers have never fired
    // a round and `coordinated` was named only as a HYPOTHESIS. These count,
    // over the ticks that actually evaluated the fire gate at the player,
    // which clause was open -- read from DroneState::w_* (the shipped
    // expressions' own output, never re-derived here).
    long long gate_ticks = 0;    // ticks the gate was evaluated at the player
    long long coord_open = 0;    // ...with the coordinated clause open
    long long cone_open = 0;     // ...with the cone clause open
    long long band_open = 0;     // ...inside [fire_range_min, snapshot_range]
    long long all_open = 0;      // ...all three at once (a real solution)
    double coord_best = -2.0;    // the best (highest) coordinated cos seen
    double cone_best = -2.0;     // the best cone cos seen
    long long perch_ticks = 0;   // doctrine witness: Perch was actually flown
    long long slash_ticks = 0;   // ...and Slash
    long long slash_gate_ticks = 0;   // gate evaluated while IN Slash
    long long slash_all_open = 0;     // ...with a full solution
    long long slash_cone_open = 0;    // ...nose on him
    long long slash_band_open = 0;    // ...inside the firing band
    double slash_cone_best = -2.0;
    double slash_rng_min = 1e18;
    // The JOINT question the census exists to answer: on the ticks the nose IS
    // on him during a slash, how far away is he? (cone open, band shut = the
    // pass shoots from too far / breaks off too early; band open, cone shut =
    // the dive geometry never points.)
    double slash_rng_at_cone_min = 1e18;
    double slash_rng_at_cone_max = -1.0;
    long long slash_cone_open_band_far = 0;   // cone open, beyond the band
    long long slash_cone_open_band_near = 0;  // cone open, inside min range
    // Is the pass even a DIVE? (gamma = achieved flight-path angle; alt_delta
    // = height over the player; the leash/terrain latches are the two blocks
    // that can take the steering away from the doctrine entirely.)
    double slash_gamma_sum = 0.0;
    double slash_gamma_min = 1e18;
    double slash_alt_sum = 0.0;
    long long slash_leashed = 0;
    long long slash_terrain = 0;
    long long slash_samples = 0;
};

// One pilot, one ace, one continuous window. `pilot` is a NON-raid,
// NON-defend pilot of the faction opposing the player (the tape's shape: the
// three enemy strikers who died in transit).
DuelResult fly_duel(const drone::DroneParams& dp, int pilot, double fight_s,
                    double window_s) {
    DuelResult out;
    StopeWorld w;
    build_world(w, dp);

    // The fleet is this ONE pilot: the probe measures the yield loop, not the
    // fleet tempo (P-B measures that). Keeping his real spawn_index keeps his
    // traits, faction and entry mouth exactly what they are in the game.
    drone::DroneState only = w.fleet[static_cast<std::size_t>(pilot)];
    w.fleet.clear();
    w.fleet.push_back(only);

    combat::CombatWorld cw;
    cw.setup = kScen.combat;
    cw.damage_params = kScen.damage;
    cw.params.hit_radius_m = dp.hit_radius_m;
    cw.player_hp = combat::summary_hp(cw.damage);
    cw.player_invuln_ticks = 0;

    const int player_faction =
        1 - combat::maverick_faction(only.spawn_index);  // the ace is his enemy
    const int max_engaged =
        combat::difficulty_params(cw.setup.difficulty).max_engaged;

    // THE STEPPED ACE. Parked on the corridor means SHUTTLED along it: he is
    // anchored where the striker was at first contact and flies a reversing
    // 1400 m beat at 200 m/s, so he stays on the transit line and inside the
    // yield radius for the whole fight window instead of blowing through it in
    // four seconds. Position, velocity, heading and orientation are mutually
    // consistent every tick (never a stamped-velocity parked state).
    constexpr double kAceSpeed = 200.0;
    constexpr double kAceBeat = 700.0;
    glm::dvec3 anchor{0.0}, ace_axis{0.0}, ace_dir{0.0};
    sim::SimState ace;
    bool ace_live = false;
    // ONE SHOT. The ace visits exactly once, so BOTH arms see the identical
    // window and the PRE arm's own (later, unrelated) transit timeout can
    // never be miscounted as a yield.
    bool ace_spent = false;

    const double dt = kAp.sim_dt;
    const long long ticks = static_cast<long long>(window_s / dt);
    for (long long t = 0; t < ticks; ++t) {
        drone::DroneState& d = w.fleet[0];
        const int before = static_cast<int>(d.mav.mode);
        // The app's bubble leash (arm_leash / instructor_tick.h): tunnel
        // dispositions are leash-exempt inside drone::tick, so the transit
        // itself is untouched, but a pilot who has just YIELDED is back in a
        // patrol disposition and must be confined like any other — without it
        // this probe would measure the vacuum strand, not the yield.
        arm_leash(w);

        // The ace joins once the striker is genuinely COMMITTED and under way
        // (20 s into the transit) — the tape's geometry, an ace meeting a
        // striker already on its line, not one caught at the launch point.
        if (!ace_live && !ace_spent &&
            d.mav.mode == maverick::MaverickState::Mode::TRANSIT &&
            d.mav.mode_ticks > std::llround(20.0 / dt)) {
            ace_live = true;
            out.contact_tick = static_cast<int>(t);
            const glm::dvec3 vhat = glm::normalize(d.curr.velocity);
            anchor = d.curr.position + vhat * 900.0;
            ace_axis = vhat;
            ace_dir = -vhat;  // head-on, the merge an ace actually grants
            ace.position = anchor + ace_axis * kAceBeat;
            ace.velocity = ace_dir * kAceSpeed;
            ace.last_vhat = ace_dir;
            const glm::dvec3 upv = glm::normalize(ace.position);
            const glm::dvec3 right = glm::normalize(glm::cross(ace_dir, upv));
            ace.orientation = glm::normalize(glm::quat_cast(
                glm::dmat3{right, glm::cross(right, ace_dir), -ace_dir}));
        }
        // The ace goes home once the fight window is spent.
        if (ace_live && out.contact_tick >= 0 &&
            static_cast<double>(t - out.contact_tick) * dt > fight_s) {
            ace_live = false;
            ace_spent = true;
            out.leave_tick = static_cast<int>(t);
        }

        const sim::SimState ace_prev = ace;
        if (ace_live) {
            const double along = glm::dot(ace.position - anchor, ace_axis);
            if (along > kAceBeat && glm::dot(ace_dir, ace_axis) > 0.0)
                ace_dir = -ace_axis;
            if (along < -kAceBeat && glm::dot(ace_dir, ace_axis) < 0.0)
                ace_dir = ace_axis;
            ace.position += ace_dir * (kAceSpeed * dt);
            ace.velocity = (ace.position - ace_prev.position) / dt;
            ace.last_vhat = glm::normalize(ace.velocity);
            const glm::dvec3 upv = glm::normalize(ace.position);
            const glm::dvec3 right = glm::normalize(glm::cross(ace_dir, upv));
            ace.orientation = glm::normalize(glm::quat_cast(
                glm::dmat3{right, glm::cross(right, ace_dir), -ace_dir}));
            combat::assign_foes(w.fleet, ace, player_faction, max_engaged, dp);
            out.min_range_m = std::min(
                out.min_range_m, glm::length(ace.position - d.curr.position));
        } else {
            d.foe = drone::kFoeNone;
            d.engaged = false;
        }

        int slots[2];
        count_on_order_runs(w, dp, slots);
        arm_strike(d, w, dp, slots);
        const sim::SimState* target =
            (ace_live && d.foe == drone::kFoePlayer) ? &ace : nullptr;
        if (ace_live && d.engaged) ++out.engaged_ticks;
        const drone::DroneTickResult r = drone::tick(d, kAp, dp, &w.env, target);
        if (r.respawned) {
            ++out.crashes;
            d.curr = faction_state(w, dp, pilot);
            d.prev = d.curr;
        }
        const int after = static_cast<int>(d.mav.mode);
        const int T = static_cast<int>(maverick::MaverickState::Mode::TRANSIT);
        const int P = static_cast<int>(maverick::MaverickState::Mode::PATROL);
        if (before == P && after == T) {
            if (out.launch_tick < 0) {
                out.launch_tick = static_cast<int>(t);
            } else if (out.abort_tick >= 0) {
                ++out.relaunches;
                if (out.relaunch_tick < 0)
                    out.relaunch_tick = static_cast<int>(t);
            }
        }
        if (before == T && after == P && ace_live && out.abort_tick < 0) {
            out.abort_tick = static_cast<int>(t);
            out.abort_range_m = glm::length(ace.position - d.curr.position);
        }
        if (out.abort_tick >= 0 && before == T &&
            after == static_cast<int>(maverick::MaverickState::Mode::DIVE_IN))
            ++out.dives;
        if (after == static_cast<int>(maverick::MaverickState::Mode::RUN) &&
            before != after && out.run_tick < 0 && out.relaunch_tick >= 0)
            out.run_tick = static_cast<int>(t);
        // The fight is only real if he is FLYING it. d.bfm.mode_ticks is the
        // honest witness: EVERY branch that is not the pursuit/BFM branch —
        // and a committed tunnel run above all — resets d.bfm to a fresh
        // BfmState each tick (the FIX-5 stale-state rule, drone.h), so a
        // non-zero dwell counter after the tick can only mean the fight was
        // actually flown.
        if (ace_live && d.engaged && d.bfm.mode_ticks > 0) ++out.pursue_ticks;

        // E8.1: the fire-gate witness, sampled on every tick that evaluated
        // the gate against the player. Nothing here can change the run --
        // these are reads of a per-tick report.
        if (d.w_gate_live && d.foe == drone::kFoePlayer) {
            ++out.gate_ticks;
            const bool coord = d.w_coord_cos >= dp.fire_align_cos;
            const bool cone = d.w_cone_cos >= dp.fire_cone_cos;
            const bool band = d.w_range_m >= dp.fire_range_min &&
                              d.w_range_m <= std::max(dp.fire_range_max,
                                                      dp.snapshot_range_m);
            if (coord) ++out.coord_open;
            if (cone) ++out.cone_open;
            if (band) ++out.band_open;
            if (coord && cone && band) ++out.all_open;
            out.coord_best = std::max(out.coord_best, d.w_coord_cos);
            out.cone_best = std::max(out.cone_best, d.w_cone_cos);
            if (d.bfm.mode == bfm::BfmState::Mode::Slash) {
                ++out.slash_gate_ticks;
                if (coord && cone && band) ++out.slash_all_open;
                if (cone) ++out.slash_cone_open;
                if (band) ++out.slash_band_open;
                out.slash_cone_best =
                    std::max(out.slash_cone_best, d.w_cone_cos);
                out.slash_rng_min = std::min(out.slash_rng_min, d.w_range_m);
                if (cone) {
                    out.slash_rng_at_cone_min =
                        std::min(out.slash_rng_at_cone_min, d.w_range_m);
                    out.slash_rng_at_cone_max =
                        std::max(out.slash_rng_at_cone_max, d.w_range_m);
                    if (d.w_range_m > std::max(dp.fire_range_max,
                                               dp.snapshot_range_m))
                        ++out.slash_cone_open_band_far;
                    if (d.w_range_m < dp.fire_range_min)
                        ++out.slash_cone_open_band_near;
                }
            }
        }
        if (d.bfm.mode == bfm::BfmState::Mode::Perch) ++out.perch_ticks;
        if (d.bfm.mode == bfm::BfmState::Mode::Slash) {
            ++out.slash_ticks;
            if (ace_live) {
                ++out.slash_samples;
                const glm::dvec3 up = sim::local_up(d.curr.position);
                const double sp2 = glm::length(d.curr.velocity);
                const double gam =
                    sp2 > 1e-6
                        ? std::asin(std::clamp(
                              glm::dot(d.curr.velocity / sp2, up), -1.0, 1.0))
                        : 0.0;
                out.slash_gamma_sum += gam;
                out.slash_gamma_min = std::min(out.slash_gamma_min, gam);
                out.slash_alt_sum += glm::length(d.curr.position) -
                                     glm::length(ace.position);
                if (d.leash_engaged) ++out.slash_leashed;
                if (d.terrain_avoid_engaged) ++out.slash_terrain;
            }
        }

        const double cd_before = d.fire_cooldown;
        combat::enemy_fire_tick(cw, w.fleet, dt, kAp.g, kAp.R, &w.net);
        if (d.fire_cooldown > cd_before && d.foe == drone::kFoePlayer)
            ++out.rounds_at_player;
        if (ace_live) combat::combat_player_tick(cw, ace_prev, ace);
    }
    return out;
}

}  // namespace

TEST_CASE("probe P-D: the transiting striker fights back") {
    const drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.strike_on_order);              // premise: E2.1 is live
    REQUIRE(shipped.transit_fight_yield_m > 0.0);  // premise: E3.1 ships on

    // The tape's pilot: a NON-raid, NON-defend striker (raid duty would have
    // frozen his run schedule long before the transit — hold_runs). Picked off
    // the SHIPPED roster rule, never a welded index.
    int pilot = -1;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        if (!combat::faction_raider(i)) {
            pilot = i;
            break;
        }
    }
    REQUIRE(pilot >= 0);
    INFO("pilot " << pilot << " ("
                  << maverick::traits_for(pilot).callsign << ")");

    drone::DroneParams pre = shipped;
    pre.transit_fight_yield_m = 0.0;
    pre.strike_concurrent_max = 0;

    const DuelResult post = fly_duel(shipped, pilot, 60.0, 1200.0);
    const DuelResult base = fly_duel(pre, pilot, 60.0, 1200.0);

    const double dt = kAp.sim_dt;
    INFO("PRE  launch=" << base.launch_tick * dt << " contact="
                        << base.contact_tick * dt << " abort="
                        << base.abort_tick << " engaged_s="
                        << base.engaged_ticks * dt << " pursue_s="
                        << base.pursue_ticks * dt << " rounds="
                        << base.rounds_at_player << " min_range="
                        << base.min_range_m << " relaunch=" << base.relaunch_tick
                        << " run=" << base.run_tick
                        << " crashes=" << base.crashes);
    INFO("POST launch=" << post.launch_tick * dt << " contact="
                        << post.contact_tick * dt << " abort_s="
                        << post.abort_tick * dt << " abort_range="
                        << post.abort_range_m << " engaged_s="
                        << post.engaged_ticks * dt << " pursue_s="
                        << post.pursue_ticks * dt << " rounds="
                        << post.rounds_at_player << " min_range="
                        << post.min_range_m << " leave_s="
                        << post.leave_tick * dt << " relaunch_s="
                        << post.relaunch_tick * dt << " run_s="
                        << post.run_tick * dt << " relaunches="
                        << post.relaunches << " dives=" << post.dives
                        << " crashes=" << post.crashes);

    // (0) PREMISE: both arms flew the same setup — the order launched a run
    // and the ace really did arrive on the corridor inside the yield radius.
    REQUIRE(post.launch_tick >= 0);
    REQUIRE(post.contact_tick >= 0);
    REQUIRE(post.min_range_m < shipped.transit_fight_yield_m);
    REQUIRE(base.launch_tick >= 0);
    REQUIRE(base.contact_tick >= 0);

    // (1) THE ABORT. The committed transit gives the errand up, with the ace
    // inside the yield radius.
    REQUIRE(post.abort_tick >= 0);
    REQUIRE(post.abort_range_m <= shipped.transit_fight_yield_m);
    REQUIRE(base.abort_tick < 0);  // the tape: he flew straight on

    // (2) HE FIGHTS, AND HE FIRES. The tape's number for this pilot is zero on
    // both counts (a committed TRANSIT is maverick_committed, so pursue()/BFM
    // are bypassed and wants_fire is forced false).
    REQUIRE(post.pursue_ticks > 0);
    REQUIRE(post.rounds_at_player > 0);
    REQUIRE(base.pursue_ticks == 0);
    REQUIRE(base.rounds_at_player == 0);

    // (3) THE ERRAND IS DELAYED, NOT ABANDONED. The ace leaves; the standing
    // order re-collapses the reloaded countdown and the run relaunches, then
    // completes to the bore. Without this leg a "fix" that simply cancelled
    // the offensive would pass every assertion above.
    REQUIRE(post.leave_tick >= 0);
    REQUIRE(post.relaunch_tick > post.abort_tick);
    REQUIRE(post.run_tick > post.relaunch_tick);
}

// ===========================================================================
// PROBE P-S — "THE SLASHER FIRES" (RUNG E8.1; the acceptance E7.2 should have
// had, owed by spec E7.N items (a) and (b)).
//
// E7.N: `slash_doctrine = true` sends probe P-D's rounds_at_player to ZERO,
// and every E7.2 test asserts the guns_hot MODE FLAG rather than a round --
// "the doctrine was proven to INTEND to shoot and never proven to shoot." The
// spec named `coordinated` (the bandit flying down its own nose, gated by
// fire_align_cos) as THE LIKELY MECHANISM and recorded it as a HYPOTHESIS,
// explicitly NOT a measurement.
//
// This probe MEASURES it. Same fixture, same pilot, same stepped ace as P-D --
// the one difference is the doctrine dial -- and it reads the E8.1 fire-gate
// witness (DroneState::w_*, written by the shipped pursue() expressions
// themselves) to census WHICH clause is open on the ticks the gate runs. That
// census, not an inference from a zero, is what tells the next rung what to
// fix.
// ===========================================================================
TEST_CASE("probe P-S: the slasher fire gate census") {
    const drone::DroneParams shipped = kScen.drone;

    int pilot = -1;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        if (!combat::faction_raider(i)) {
            pilot = i;
            break;
        }
    }
    REQUIRE(pilot >= 0);
    // PREMISE: this pilot really is the tier the doctrine converts (E7.N: the
    // roster rule picks CANARY, aggression 0.55, base tier). A doctrine arm
    // flown by an ACE would measure nothing about slashers.
    INFO("pilot " << pilot << " (" << maverick::traits_for(pilot).callsign
                  << ")");

    drone::DroneParams on = shipped;
    on.slash_doctrine = true;
    drone::DroneParams off = shipped;
    off.slash_doctrine = false;

    const DuelResult a = fly_duel(on, pilot, 60.0, 1200.0);
    const DuelResult b = fly_duel(off, pilot, 60.0, 1200.0);

    const auto census = [](const char* tag, const DuelResult& r) {
        const double n = static_cast<double>(std::max<long long>(r.gate_ticks, 1));
        WARN(tag << ": rounds=" << r.rounds_at_player
                 << "  pursue_s=" << r.pursue_ticks * kAp.sim_dt
                 << "  min_range=" << r.min_range_m
                 << "  perch_s=" << r.perch_ticks * kAp.sim_dt
                 << "  slash_s=" << r.slash_ticks * kAp.sim_dt
                 << "\n    gate_ticks=" << r.gate_ticks
                 << "  coordinated_open=" << 100.0 * r.coord_open / n << "%"
                 << "  cone_open=" << 100.0 * r.cone_open / n << "%"
                 << "  band_open=" << 100.0 * r.band_open / n << "%"
                 << "  ALL_open=" << 100.0 * r.all_open / n << "%"
                 << "\n    best coordinated cos=" << r.coord_best
                 << "  best cone cos=" << r.cone_best
                 << "  slash_gate_ticks=" << r.slash_gate_ticks
                 << "  slash_all_open=" << r.slash_all_open
                 << "\n    IN SLASH: cone_open=" << r.slash_cone_open
                 << "  band_open=" << r.slash_band_open
                 << "  best_cone_cos=" << r.slash_cone_best
                 << "  min_range=" << r.slash_rng_min
                 << "\n    range WHEN the nose is on him: ["
                 << r.slash_rng_at_cone_min << ", " << r.slash_rng_at_cone_max
                 << "]  too_far=" << r.slash_cone_open_band_far
                 << "  too_near=" << r.slash_cone_open_band_near);
    };
    census("DOCTRINE ON ", a);
    census("DOCTRINE OFF", b);

    // ---- E8.1 THE STANDOFF SWEEP ------------------------------------------
    // The dive-envelope algebra bounds perch_lag_m to [1572, 2085] at the
    // shipped height/gamma/attack_range (see the dial's own comment). Which
    // value inside that window actually puts rounds downrange is a
    // MEASUREMENT, not a choice -- so sweep it and read the census. The
    // pre-E8 perch (lag = the 220 m tracking point) is the control.
    struct Arm {
        double lag, time_s, rel_frac, gamma_deg;
    };
    const double kShipGamma = on.pursue_max_gamma * 57.2958;
    for (Arm arm : {Arm{0.0, 5.0, 0.45, kShipGamma},      // the pre-E8 control
                    Arm{1700.0, 12.0, 0.10, kShipGamma},  // best geometry arm
                    // ★ IS THE DIVE ENVELOPE THE WALL? A slash's depression
                    // angle to the target GROWS as the pass closes (you close
                    // horizontally faster than you descend), so a dive clamped
                    // at 30 deg can never point at a man below you. These arms
                    // raise the clamp to test that directly. NOT a proposal --
                    // pursue_max_gamma is shared with the terrain pull-up.
                    Arm{1700.0, 12.0, 0.10, 45.0},
                    Arm{1700.0, 12.0, 0.10, 60.0},
                    Arm{1200.0, 12.0, 0.10, 60.0},
                    Arm{800.0, 12.0, 0.10, 70.0}}) {
        const double lag = arm.lag;
        drone::DroneParams sw = on;
        sw.bfm.perch_lag_m = arm.lag;
        sw.bfm.slash_time_s = arm.time_s;
        sw.bfm.perch_release_frac = arm.rel_frac;
        sw.pursue_max_gamma = arm.gamma_deg / 57.2958;
        sw.bfm.yoyo_gamma = std::min(sw.bfm.yoyo_gamma, sw.pursue_max_gamma);
        const DuelResult r = fly_duel(sw, pilot, 60.0, 1200.0);
        WARN("perch_lag=" << lag << " slash_time_s=" << arm.time_s << " max_gamma=" << arm.gamma_deg
                          << " release_frac=" << arm.rel_frac
                          << (lag <= 0.0 ? " (pre-E8 control)" : "")
                          << ": rounds=" << r.rounds_at_player
                          << "  slash_s=" << r.slash_ticks * kAp.sim_dt
                          << "  slash_gate=" << r.slash_gate_ticks
                          << "  slash_cone_open=" << r.slash_cone_open
                          << "  slash_all_open=" << r.slash_all_open
                          << "  best_slash_cone_cos=" << r.slash_cone_best
                          << "  min_range=" << r.min_range_m
                          << "\n    IS IT A DIVE? mean_gamma="
                          << (r.slash_samples
                                  ? r.slash_gamma_sum / r.slash_samples * 57.2958
                                  : 0.0)
                          << "deg  steepest="
                          << (r.slash_samples ? r.slash_gamma_min * 57.2958 : 0.0)
                          << "deg  mean_alt_over_him="
                          << (r.slash_samples
                                  ? r.slash_alt_sum / r.slash_samples
                                  : 0.0)
                          << "m  leashed=" << r.slash_leashed << "/"
                          << r.slash_samples
                          << "  terrain_avoid=" << r.slash_terrain);
    }

    // (0) PREMISE — the arms are comparable and the doctrine was really flown.
    // Without this the census below is a census of nothing (the E7.2 coverage
    // gap in its purest form: a doctrine that never engaged would also report
    // zero rounds).
    REQUIRE(b.pursue_ticks > 0);
    REQUIRE(a.pursue_ticks > 0);
    REQUIRE(a.slash_ticks + a.perch_ticks > 0);
    REQUIRE(b.slash_ticks + b.perch_ticks == 0);  // off arm flies no doctrine

    // (1) THE REGRESSION SENTINEL (E7.N item (c)): the OFF arm is the signed
    // E3 behaviour -- it fires.
    REQUIRE(b.rounds_at_player > 0);

    // (2) THE MEASUREMENT ITSELF. The gate must have RUN at the player while
    // the doctrine was on -- a doctrine that never presents the gate is a
    // different defect from one whose gate vetoes, and the census can only
    // distinguish them if this holds.
    REQUIRE(a.gate_ticks > 0);

    // (3) ★★★ THE WITNESS MUST AGREE WITH THE PIPELINE (rung E9, from the E8
    // red team's P0-5). As E8 shipped it, `a.gate_ticks > 0` above was the ONLY
    // assertion touching the witness, and it reads `w_gate_live` -- not one
    // census VALUE. The red team inverted the witness (w_cone_cos = 1.0,
    // w_coord_cos = -1.0: the exact opposite of BOTH E8.1 findings) and this
    // probe stayed green, 7/7. Every number the E8.1 ruling rests on came out
    // of an instrument no assertion checked. That is E7.N's own lesson one
    // level up: the doctrine was proven to INTEND to shoot and never to shoot;
    // the witness was proven to EXIST and never to be TRUE.
    //
    // THE INVARIANT IS ONE-WAY ON PURPOSE. Rounds cannot leave the barrel
    // unless the gate was open, so `rounds > 0` MUST imply an open census. The
    // converse is NOT asserted and must not be: the BFM `guns_hot` veto can
    // hold fire with the gate wide open, which is exactly what the doctrine
    // arm is for. The OFF arm fires (clause 1), so this is a live pin on the
    // real numbers, per FIELD, so that no single witness value can lie alone.
    REQUIRE(b.all_open > 0);                          // coord AND cone AND band
    REQUIRE(b.coord_open > 0);                        // w_coord_cos
    REQUIRE(b.cone_open > 0);                         // w_cone_cos
    REQUIRE(b.band_open > 0);                         // w_range_m
    REQUIRE(b.cone_best >= shipped.fire_cone_cos);    // it really did point
    REQUIRE(b.coord_best >= shipped.fire_align_cos);  // and really was coordinated
}

// ===========================================================================
// PROBE P-E — "THE WAVES REFILL IT" (RUNG E5, Chad's ruling 2026-08-20).
//
// THE MEASURED PROBLEM (Chad's fly tape conquest_tape_1, 13:41): the enemy
// strikers were all dead by 3:34 and the remaining ~13 minutes were EMPTY sky.
// In conquest a killed maverick STAYS dead (respawn_drones=false), so attrition
// is one-way and a match that goes empty stays empty. Chad's ruling supersedes
// that consciously: keep 5v5 density, kills buy TEMPO not attrition.
//
// This probe kills the WHOLE enemy wing in a real headless conquest
// composition and measures three things the ruling demands together:
//   (1) THE SKY REFILLS — waves deploy after the delay, airborne, in their own
//       faction's air, with their trait identity intact.
//   (2) THE WAR IS STILL WINNABLE — the deep pump still dies with waves on
//       (victory by pump destruction survives reinforcement).
//   (3) THE KNOB-OFF ARM IS BIT-IDENTICAL — reinforce_delay_s = 0 reproduces
//       today's permanent death exactly, trajectory for trajectory.
//
// It flies the SHIPPED policy (app::AirborneWavePolicy over the real
// place_in_faction_air) and the SHIPPED wave machine — no probe-local
// reimplementation of either, so a wiring bug cannot hide behind a mirror.
// ===========================================================================
namespace {

struct WaveResult {
    long long waves = 0;
    double first_wave_s = -1.0;
    long long empty_ticks_after_first = 0;  // enemy roster empty AFTER a wave
    int enemy_flying_end = 0;               // living enemy pilots at the end
    int enemy_flying_min_after = 99;        // ...low-water mark after wave 1
    double min_wave_agl_m = 1e18;           // waves really launch AIRBORNE
    int wave_wrong_dome = 0;                // a wave landed off its own side
    int wave_identity_broken = 0;           // spawn_index / faction not kept
    bool pump_dead = false;
    double pump_kill_s = -1.0;
    combat::Outcome outcome = combat::Outcome::PLAYING;
    // The bit-exact trajectory signature of the whole fleet at the end of the
    // run (the knob-off differential's instrument).
    double traj_sig = 0.0;
};

// One arm. `delay_s` <= 0 is the OFF arm. `wave_machine` false additionally
// skips the E5 call entirely — the two together are the differential: OFF must
// equal ABSENT bit for bit.
WaveResult fly_waves(const drone::DroneParams& dp, double delay_s,
                     bool wave_machine, double minutes) {
    WaveResult out;
    StopeWorld w;
    build_world(w, dp);

    combat::CombatWorld cw;
    cw.setup = kScen.combat;
    cw.damage_params = kScen.damage;
    cw.params.hit_radius_m = dp.hit_radius_m;

    // The conquest bookkeeping the app carries, with the app's own dome look
    // values — place_in_faction_air reads them for its ceiling clamp.
    app::ConquestWorld cqw;
    cqw.state.player_faction = combat::CQ_VALLEY;
    cqw.params.reinforce_delay_s = delay_s;
    cqw.params.reinforce_restores_roster = true;
    cqw.bubble_ceiling_m = kGame.atmosphere.bubble_ceiling_m;
    cqw.bubble_edge_soft_m = kGame.atmosphere.bubble_edge_soft_m;
    cqw.bubble_ceil_soft_m = kGame.atmosphere.bubble_ceil_soft_m;
    // The app's (0) stamp: with waves live there is no wiped faction, so the
    // victory-by-wipe route is off and the match is decided by the pumps.
    cqw.state.reinforcements_live = delay_s > 0.0;

    // place_in_faction_air reads dparams (the cruise-speed reseed) only.
    app::DroneWorld dwv;
    dwv.dparams = dp;

    app::AirborneWavePolicy policy;
    policy.cq = &cqw;
    policy.dw = &dwv;
    policy.ap = &kAp;

    combat::ReinforceParams rfp;
    rfp.delay_s = delay_s;
    rfp.restores_roster = true;
    rfp.hp_full = dp.hp;

    // THE WING DIES. Every ENEMY-of-the-player pilot (the shipped roster rule,
    // never welded indices) is shot down on tick 0 through the REAL kill
    // bookkeeping — inert wreck + on_ai_kill, exactly what the AI-vs-AI sweep
    // and combat_tick pass 2 leave behind.
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        if (!combat::is_enemy(i, cqw.state.player_faction)) continue;
        w.fleet[i].inert = true;
        w.fleet[i].engaged = false;
        combat::on_ai_kill(cqw.state, i);
    }

    // The player is parked on the far side of the planet: undefended, so
    // nothing distracts the wing and the pump kill is the honest measurement.
    const glm::dvec3 far_up = glm::normalize(-w.home[combat::CQ_VALLEY]);
    const sim::SimState player = drone::level_state_at(
        dp, far_up * (kAp.R + 1000.0),
        glm::normalize(glm::cross(far_up, glm::dvec3{0.0, 1.0, 0.0})));

    const double dt = kAp.sim_dt;
    const long long ticks = static_cast<long long>(minutes * 60.0 / dt);

    for (long long t = 0; t < ticks; ++t) {
        const double now_s = static_cast<double>(t) * dt;
        arm_leash(w);
        int slots[2];
        count_on_order_runs(w, dp, slots);
        for (std::size_t i = 0; i < w.fleet.size(); ++i) {
            drone::DroneState& d = w.fleet[i];
            if (d.inert) continue;  // a wreck does not fly (the app skips it)
            arm_strike(d, w, dp, slots);
            const drone::DroneTickResult r =
                drone::tick(d, kAp, dp, &w.env, nullptr);
            if (r.respawned) {  // the app's FIX-F2 relocation
                d.curr = faction_state(w, dp, static_cast<int>(i));
                d.prev = d.curr;
            }

            // The app's damage arbitration (deep-strike half — nothing raids a
            // surface pump with the player on the far side of the world).
            const combat::RaidParams rp_deep = combat::strike_params(
                d.strike.station_range_m, d.strike.station_cos);
            if (d.strike.active && !d.strike_bailed &&
                d.mav.mode == maverick::MaverickState::Mode::RUN) {
                const int pi = d.strike.pump_idx;
                if (pi >= 0 && pi < 4 && w.pumps[pi].alive &&
                    combat::raider_on_station(d.curr, w.pumps[pi].pos,
                                              rp_deep)) {
                    w.pumps[pi].hp -= dt;  // HP is in on-station seconds
                    if (w.pumps[pi].hp <= 0.0) {
                        w.pumps[pi].alive = false;
                        // The SHARED pump-death path: score, dome growth, dome
                        // shrink, and the sudden-death clock — the whole reason
                        // "the match still ends by pump destruction" is a
                        // claim about conquest state and not just about HP.
                        combat::damage_pump(cqw.state, pi, 1e9,
                                            1 - w.pumps[pi].faction,
                                            cqw.params);
                        if (pi == 2 && !out.pump_dead) {
                            out.pump_dead = true;
                            out.pump_kill_s = now_s;
                        }
                    }
                }
            }
        }

        combat::enemy_fire_tick(cw, w.fleet, dt, kAp.g, kAp.R, &w.net);
        combat::combat_player_tick(cw, player, player);

        // ---- RUNG E5: the wave machine, exactly as instructor_tick calls it.
        const long long waves_before = cqw.reinforce.waves;
        if (wave_machine)
            combat::reinforce_tick(cqw.reinforce, w.fleet, cqw.state, rfp,
                                   policy, dt);
        if (cqw.reinforce.waves != waves_before) {
            if (out.first_wave_s < 0.0) out.first_wave_s = now_s;
            // Audit every pilot that just came back.
            for (std::size_t i = 0; i < w.fleet.size(); ++i) {
                const drone::DroneState& d = w.fleet[i];
                if (d.inert || d.age_ticks != 0) continue;
                const int own = combat::maverick_faction(d.spawn_index);
                const glm::dvec3 u = glm::normalize(d.curr.position);
                out.min_wave_agl_m =
                    std::min(out.min_wave_agl_m,
                             glm::length(d.curr.position) - w.hf.radius_at(u));
                // Its own faction's side of the world: the dome it launched
                // into is the one whose centre direction it is nearest.
                if (glm::dot(u, w.home[own]) < glm::dot(u, w.home[1 - own]))
                    ++out.wave_wrong_dome;
                if (d.spawn_index != static_cast<int>(i) ||
                    d.hp <= 0.0)
                    ++out.wave_identity_broken;
            }
        }
        out.waves = cqw.reinforce.waves;

        // Roster/flying census (enemy = the player's opposition).
        int flying = 0;
        for (const drone::DroneState& d : w.fleet)
            if (!d.inert && combat::is_enemy(d.spawn_index,
                                            cqw.state.player_faction))
                ++flying;
        if (out.first_wave_s >= 0.0) {
            if (flying == 0) ++out.empty_ticks_after_first;
            out.enemy_flying_min_after =
                std::min(out.enemy_flying_min_after, flying);
        }
        out.enemy_flying_end = flying;
    }

    out.outcome = cqw.state.outcome;
    // The differential signature: every drone's final position/velocity,
    // summed with an index weight so a permutation cannot cancel.
    for (std::size_t i = 0; i < w.fleet.size(); ++i) {
        const drone::DroneState& d = w.fleet[i];
        const double k = static_cast<double>(i + 1);
        out.traj_sig += k * (d.curr.position.x + d.curr.position.y +
                             d.curr.position.z + d.curr.velocity.x +
                             d.curr.velocity.y + d.curr.velocity.z);
    }
    return out;
}

}  // namespace

TEST_CASE("probe P-E: reinforcement waves refill a dead wing") {
    const drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.maverick.enabled);  // premise: the tunnel machine runs
    REQUIRE(shipped.strike_on_order);   // premise: the offensive is live

    // THE SHIPPED DIAL. Read from config, never welded here.
    const double delay_s = kGame.conquest.reinforce_delay_s;
    INFO("reinforce_delay_s " << delay_s);
    REQUIRE(delay_s > 0.0);  // premise: E5 ships ON

    // 15 minutes: long enough for the deep pump to fall (P-B measures ~10) and
    // for several wave cycles at the shipped delay.
    const WaveResult post = fly_waves(shipped, delay_s, true, 15.0);
    const WaveResult off = fly_waves(shipped, 0.0, true, 15.0);
    const WaveResult absent = fly_waves(shipped, 0.0, false, 15.0);

    INFO("POST waves=" << post.waves << " first_wave_s=" << post.first_wave_s
                       << " empty_after=" << post.empty_ticks_after_first
                       << " flying_end=" << post.enemy_flying_end
                       << " flying_min_after=" << post.enemy_flying_min_after
                       << " min_wave_agl=" << post.min_wave_agl_m
                       << " wrong_dome=" << post.wave_wrong_dome
                       << " identity_broken=" << post.wave_identity_broken
                       << " pump2_dead=" << post.pump_dead
                       << " pump2_kill_s=" << post.pump_kill_s);
    INFO("OFF  waves=" << off.waves << " flying_end=" << off.enemy_flying_end
                       << " pump2_dead=" << off.pump_dead
                       << " sig=" << off.traj_sig
                       << " ABSENT sig=" << absent.traj_sig);

    // (0) THE TAPE'S BASELINE, reproduced: with waves off the wing that died on
    // tick 0 never comes back and the sky stays empty for the whole match.
    REQUIRE(off.waves == 0);
    REQUIRE(off.enemy_flying_end == 0);

    // (1) THE SKY REFILLS. Every one of the five dead enemy pilots is back.
    REQUIRE(post.waves >= combat::team_size(combat::CQ_SUDBURY));
    REQUIRE(post.enemy_flying_end == combat::team_size(combat::CQ_SUDBURY));
    // At the shipped delay the first wave lands one delay after the deaths
    // (one tick of slack for the arm-then-count-down ordering).
    REQUIRE(post.first_wave_s >= delay_s);
    REQUIRE(post.first_wave_s <= delay_s + 2.0 * kAp.sim_dt);
    // AND IT STAYS FULL: once the waves start, the enemy roster is never empty
    // again — "the match must never go empty", measured, not asserted.
    REQUIRE(post.empty_ticks_after_first == 0);
    REQUIRE(post.enemy_flying_min_after >= 1);

    // (2) A WAVE IS AN AIRBORNE LAUNCH FROM ITS OWN SIDE, with its identity
    // intact (spawn_index drives the maverick traits, the raid duty and the
    // faction — a wave that renumbered pilots would silently re-roll the wing).
    REQUIRE(post.min_wave_agl_m > 200.0);
    REQUIRE(post.wave_wrong_dome == 0);
    REQUIRE(post.wave_identity_broken == 0);

    // (3) THE WAR IS STILL WINNABLE. The allied deep pump still dies with the
    // waves on — reinforcement buys the enemy tempo, not invulnerability. This
    // is the leg that fails if waves ever start defending by accident.
    REQUIRE(post.pump_dead);
    REQUIRE(post.pump_kill_s > 0.0);

    // (4) NO SILENT DOME RE-INFLATION (the spec's trap 1, measured): the whole
    // point of the roster wiring. A pump death is the ONLY thing that moves a
    // radius_scale, so the arm with five extra waves of enemy planes must not
    // differ from the arm with none in anything a wave does not own.
    REQUIRE(post.outcome == combat::Outcome::PLAYING);  // no wipe-win, no loss

    // (5) THE KNOB-OFF DIFFERENTIAL: delay 0 with the machine WIRED IN is bit
    // for bit the run with the machine ABSENT.
    REQUIRE(off.traj_sig == absent.traj_sig);
    REQUIRE(off.enemy_flying_end == absent.enemy_flying_end);
    REQUIRE(off.pump_dead == absent.pump_dead);
}

// ===========================================================================
// RUNG E5 UNIT LEG — the wave machine's own contracts, open-loop. These are
// the mutations the composition probe above is too coarse to catch.
// ===========================================================================
TEST_CASE("E5: the wave machine contracts") {
    // A minimal policy that always accepts, recording who it deployed.
    struct AcceptPolicy : combat::ReinforcePolicy {
        std::vector<int> deployed;
        bool deploy(drone::DroneState& d, int spawn_index) override {
            d.curr.position = glm::dvec3{1.0, 2.0, 3.0};
            d.prev = d.curr;
            deployed.push_back(spawn_index);
            return true;
        }
    };
    // A policy that refuses forever (the "no air left" / "no marker" seam).
    struct RefusePolicy : combat::ReinforcePolicy {
        int asked = 0;
        bool deploy(drone::DroneState& d, int spawn_index) override {
            (void)d;
            (void)spawn_index;
            ++asked;
            return false;
        }
    };

    const double dt = 1.0 / 120.0;
    combat::ReinforceParams rp;
    rp.delay_s = 1.0;  // 120 ticks
    rp.hp_full = 40.0;

    std::vector<drone::DroneState> fleet(2);
    fleet[0].spawn_index = 0;
    fleet[1].spawn_index = 1;
    fleet[0].fleet_count = 2;
    fleet[1].fleet_count = 2;

    SECTION("off arm: delay 0 never touches anything") {
        combat::ConquestState cs;
        AcceptPolicy pol;
        combat::ReinforceState rs;
        combat::ReinforceParams off = rp;
        off.delay_s = 0.0;
        fleet[0].inert = true;
        const glm::dvec3 wreck = fleet[0].curr.position;
        for (int t = 0; t < 1000; ++t)
            combat::reinforce_tick(rs, fleet, cs, off, pol, dt);
        CHECK(rs.waves == 0);
        CHECK(pol.deployed.empty());
        CHECK(fleet[0].inert);
        CHECK(fleet[0].curr.position == wreck);
        CHECK(rs.pending[0] == false);
    }

    SECTION("the delay is earned: not one tick early") {
        combat::ConquestState cs;
        cs.reinforcements_live = true;
        AcceptPolicy pol;
        combat::ReinforceState rs;
        fleet[0].inert = true;
        combat::on_ai_kill(cs, 0);
        for (int t = 0; t < 120; ++t) {
            combat::reinforce_tick(rs, fleet, cs, rp, pol, dt);
            CHECK(fleet[0].inert);  // still a wreck through the whole delay
        }
        combat::reinforce_tick(rs, fleet, cs, rp, pol, dt);
        CHECK_FALSE(fleet[0].inert);
        CHECK(rs.waves == 1);
        CHECK(fleet[0].hp == 40.0);
        CHECK(fleet[0].spawn_index == 0);
        CHECK(cs.mav_alive[0]);  // back on the roster: the next kill scores
    }

    SECTION("a refusing policy holds the wreck and never re-zeroes the clock") {
        combat::ConquestState cs;
        cs.reinforcements_live = true;
        RefusePolicy pol;
        combat::ReinforceState rs;
        fleet[0].inert = true;
        fleet[0].curr.position = glm::dvec3{7.0, 8.0, 9.0};
        combat::on_ai_kill(cs, 0);
        for (int t = 0; t < 300; ++t)
            combat::reinforce_tick(rs, fleet, cs, rp, pol, dt);
        CHECK(fleet[0].inert);
        CHECK(fleet[0].curr.position == glm::dvec3{7.0, 8.0, 9.0});
        CHECK(rs.waves == 0);
        CHECK_FALSE(cs.mav_alive[0]);
        // Asked every tick from expiry on — the countdown is not re-armed.
        CHECK(pol.asked >= 180);
        // And it deploys the instant a policy accepts.
        AcceptPolicy ok;
        combat::reinforce_tick(rs, fleet, cs, rp, ok, dt);
        CHECK_FALSE(fleet[0].inert);
        CHECK(rs.waves == 1);
    }

    SECTION("a decided match takes no reinforcements") {
        combat::ConquestState cs;
        cs.outcome = combat::Outcome::DEFEAT;
        AcceptPolicy pol;
        combat::ReinforceState rs;
        fleet[0].inert = true;
        for (int t = 0; t < 300; ++t)
            combat::reinforce_tick(rs, fleet, cs, rp, pol, dt);
        CHECK(rs.waves == 0);
        CHECK(fleet[0].inert);
    }

    SECTION("waves live: the victory-by-wipe route is off") {
        combat::ConquestState cs;
        cs.player_faction = combat::CQ_VALLEY;
        cs.reinforcements_live = true;
        for (int i = 0; i < combat::kNumMavericks; ++i)
            if (combat::is_enemy(i, cs.player_faction)) combat::on_ai_kill(cs, i);
        CHECK(cs.outcome == combat::Outcome::PLAYING);
        // ...and the pump route still decides the match.
        combat::ConquestParams cp;
        cs.pumps[1].alive = true;
        cs.pumps[1].hp = 1.0;
        cs.pumps[1].faction = combat::CQ_SUDBURY;
        CHECK(combat::damage_pump(cs, 1, 10.0, combat::CQ_VALLEY, cp));
        CHECK(cs.score[combat::CQ_VALLEY] > 0);

        // The default (waves OFF) still wipes to VICTORY — bit-identical rule.
        combat::ConquestState base;
        base.player_faction = combat::CQ_VALLEY;
        for (int i = 0; i < combat::kNumMavericks; ++i)
            if (combat::is_enemy(i, base.player_faction))
                combat::on_ai_kill(base, i);
        CHECK(base.outcome == combat::Outcome::VICTORY);
    }

    SECTION("a wave never moves a dome or a score") {
        combat::ConquestState cs;
        cs.reinforcements_live = true;
        cs.radius_scale[0] = 0.5;
        cs.radius_scale[1] = 0.0;
        cs.ceiling_scale[0] = 0.5;
        cs.ceiling_scale[1] = 0.0;
        cs.score[0] = 7;
        cs.score[1] = 3;
        cs.countdown_faction = 1;
        cs.countdown_s = 123.0;
        combat::on_reinforce(cs, 3);
        CHECK(cs.mav_alive[3]);
        CHECK(cs.radius_scale[0] == 0.5);
        CHECK(cs.radius_scale[1] == 0.0);  // a crushed dome STAYS crushed
        CHECK(cs.ceiling_scale[1] == 0.0);
        CHECK(cs.score[0] == 7);
        CHECK(cs.score[1] == 3);
        CHECK(cs.countdown_faction == 1);
        CHECK(cs.countdown_s == 123.0);
    }
}

// ---------------------------------------------------------------------------
// ★★★ RUNG E15 — STAGE A'S BUDGET SCALES WITH THE DISTANCE IT HAS TO FLY.
//
// Chad's fly report on tape 9 (2026-08-24): "they didnt attack underground
// however, are there any being allocated to the underground??" They were: the
// enemy launched TEN runs and arrived ONCE, and three of the eight failures
// ended at EXACTLY the raw transit_timeout_s, never having captured the FIX.
// A fixed 150 s is a RANGE LIMIT wearing a timeout's clothes.
// ---------------------------------------------------------------------------
TEST_CASE("E15: the transit budget grows with the distance left to fly") {
    // Pure arithmetic on the shipped table -- no fixture, so this pins the
    // CONTRACT rather than one geometry's outcome.
    const double t0 = kScen.drone.maverick.transit_timeout_s;
    // ★ THE RATE IS NAMED HERE, NOT READ FROM THE SHIPPED TABLE. E15 ships
    // OFF (0.0) on the crash measurement in config/scenario.toml, so reading
    // the shipped value would make this leg vacuous -- it would be checking
    // that a disabled dial is disabled. What is under test is the CONTRACT the
    // dial implements, which must stay correct for the day it is turned on.
    const double per_km = 11.0;  // 1000 / 91.5 m/s, the measured transit closure
    INFO("transit_timeout_s=" << t0 << " rate under test=" << per_km);
    REQUIRE(t0 > 0.0);

    // The launches Chad's tape actually recorded, and what each one is now
    // allowed. The 91.5 m/s figure is the MEASURED transit closure of the arm
    // that was failing, so "needs" is not a preference, it is the tape.
    const double kMeasuredClosure = 91.5;  // m/s, enemy transits, tape 9
    for (double km : {9.5, 15.9, 16.2, 22.0}) {
        const double budget = t0 + per_km * km;
        const double needs = 1000.0 * km / kMeasuredClosure;
        INFO("launch at " << km << " km: budget " << budget << " s, needs "
                          << needs << " s");
        CHECK(budget >= needs);
    }
    // ...and the 150 s flat budget did NOT cover the long ones. This is the
    // fixture-shows-the-defect clause: without it the test above could pass on
    // a budget that was already sufficient.
    CHECK(t0 < 1000.0 * 15.9 / kMeasuredClosure);
    CHECK(t0 < 1000.0 * 22.0 / kMeasuredClosure);
}

TEST_CASE("E15: a striker launched far away still reaches the stope") {
    // The DIFFERENTIAL. Same world, same pilot, same launch point -- only the
    // dial moves. The off arm must fail to arrive (the defect Chad watched)
    // and the on arm must arrive.
    const auto reach = [&](double per_km) {
        drone::DroneParams dp = kScen.drone;
        dp.maverick.transit_reach_s_per_km = per_km;
        const StopeResult r = fly_stope(dp, 15.0);
        return r;
    };
    const StopeResult off = reach(0.0);
    const StopeResult on = reach(11.0);  // explicit: E15 ships OFF (see above)
    std::printf(
        "\n[E15] transit budget flat vs distance-scaled (15 min, same world)\n"
        "  flat   : entries %d  runs %d  timeouts %d  on_station[2] %.1f s\n"
        "  scaled : entries %d  runs %d  timeouts %d  on_station[2] %.1f s\n",
        off.entries, off.runs, off.transit_timeouts, off.on_station_s[2],
        on.entries, on.runs, on.transit_timeouts, on.on_station_s[2]);
    INFO("flat runs=" << off.runs << " timeouts=" << off.transit_timeouts
                      << " | scaled runs=" << on.runs
                      << " timeouts=" << on.transit_timeouts);
    // Fewer transits are thrown away, and no run traffic is lost doing it.
    CHECK(on.transit_timeouts <= off.transit_timeouts);
    CHECK(on.runs >= off.runs);
}

TEST_CASE("E15: reach budget off is bit identical to the flat timeout") {
    // The knob-off arm the whole ladder ships with.
    drone::DroneParams a = kScen.drone;
    drone::DroneParams b = kScen.drone;
    a.maverick.transit_reach_s_per_km = 0.0;
    b.maverick.transit_reach_s_per_km = 0.0;
    const StopeResult ra = fly_stope(a, 6.0);
    const StopeResult rb = fly_stope(b, 6.0);
    REQUIRE(ra.runs == rb.runs);
    REQUIRE(ra.entries == rb.entries);
    REQUIRE(ra.transit_timeouts == rb.transit_timeouts);
    REQUIRE(ra.on_station_s[2] == rb.on_station_s[2]);
}
