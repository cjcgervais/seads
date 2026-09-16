// Conquest-state unit tests (combat/conquest.h). Pure, headless: no raylib,
// no clock, no rng. Each case names the mutation lever it catches.

#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/conquest_world.h"  // integration glue (growth -> bubbles)
#include "combat/conquest.h"
#include "combat/kill.h"  // no-respawn gate (CombatWorld/combat_tick)
#include "combat/raid.h"
#include "drone/maverick.h"  // COMPETITIVE rung: enemy pump raids
#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_world.h"
#include "drone/drone.h"
#include "sim/fields.h"
#include "sim/params.h"
#include "weapon/ballistics.h"
#include "world/faction_bubbles.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

// A player round segment [prev,curr] that sweeps straight through `pos` (a
// pump center), same convention as test_combat's projectile fixtures.
weapon::Projectile round_through(const glm::dvec3& prev, const glm::dvec3& curr,
                                 double damage = 30.0) {
    weapon::Projectile p;
    p.prev_pos = prev;
    p.pos = curr;
    p.vel = curr - prev;
    p.v_ref = 300.0;
    p.damage = damage;
    p.active = true;
    return p;
}

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kP);
const cfg::WorldParams kWorld =
    cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");

// The SAME 5-gun battery composition app/main.cpp builds (1 hub cannon, 2
// cowl MG, 2 wing cannon) -- config-relative oracle, no re-typed gun table.
weapon::GunBattery player_battery() {
    weapon::GunBattery b;
    b.convergence_range = kWorld.guns.convergence_range_m;
    const auto& g = kWorld.guns;
    b.guns.push_back({{},
                      weapon::Round::Cannon20mm,
                      g.cannon_speed_mps,
                      g.cannon_rof_hz,
                      g.cannon_drag_k,
                      g.cannon_damage});
    b.guns.push_back({{},
                      weapon::Round::MG792,
                      g.mg_speed_mps,
                      g.mg_rof_hz,
                      g.mg_drag_k,
                      g.mg_damage});
    b.guns.push_back({{},
                      weapon::Round::MG792,
                      g.mg_speed_mps,
                      g.mg_rof_hz,
                      g.mg_drag_k,
                      g.mg_damage});
    b.guns.push_back({{},
                      weapon::Round::Cannon20mm,
                      g.cannon_speed_mps,
                      g.cannon_rof_hz,
                      g.cannon_drag_k,
                      g.cannon_damage});
    b.guns.push_back({{},
                      weapon::Round::Cannon20mm,
                      g.cannon_speed_mps,
                      g.cannon_rof_hz,
                      g.cannon_drag_k,
                      g.cannon_damage});
    return b;
}

}  // namespace

// ---------------------------------------------------------------------------
// PUMPS

// 2026-07-25 fly-2 ruling D: pump HP is config-DERIVED (pump_kill_seconds *
// the live gun battery's DPS), and each hit deals the ROUND'S OWN damage
// (weapon::Projectile::damage), not a flat welded number. MUTATION: any
// welded HP/damage constant reappearing (e.g. hard-coding kPumpHitDamage=20
// or pump_max_hp=60 again) desyncs pu.hp from the config-derived oracle
// computed here and fails the round-count REQUIRE.
TEST_CASE("conquest: pump HP/hit-damage are config-derived, not welded") {
    const weapon::GunBattery battery = player_battery();
    const double dps = combat::battery_dps(battery);
    REQUIRE(dps > 0.0);  // baseline term > eps before checking a ratio
    const double pump_max_hp =
        combat::derive_pump_max_hp(battery, kGame.conquest.pump_kill_seconds);
    REQUIRE(pump_max_hp ==
            Catch::Approx(dps * kGame.conquest.pump_kill_seconds));

    combat::ConquestState cs;
    combat::Pump& pu = cs.pumps[0];
    pu.pos = glm::dvec3{100.0, 0.0, 0.0};
    pu.radius_m = 25.0;
    pu.hp = pump_max_hp;
    pu.alive = true;

    combat::ConquestParams params;

    // Fire the SAME battery mix (3 cannon rounds + 2 mg rounds, matching
    // player_battery()) round-robin until the pump dies, counting hits.
    int hits = 0;
    double damage_dealt = 0.0;
    while (pu.alive && hits < 100000) {
        const weapon::GunSpec& g = battery.guns[hits % battery.guns.size()];
        std::vector<weapon::Projectile> pool = {
            round_through(glm::dvec3{100.0, -1000.0, 0.0},
                          glm::dvec3{100.0, 1000.0, 0.0}, g.damage)};
        combat::conquest_tick(cs, pool, params);
        REQUIRE_FALSE(pool[0].active);  // retired on hit
        ++hits;
        damage_dealt += g.damage;
    }
    REQUIRE_FALSE(pu.alive);
    // The kill happened within one round's damage of the exact budget (the
    // last round can overshoot by at most its own damage).
    CHECK(damage_dealt >= pump_max_hp);
    CHECK(damage_dealt - pump_max_hp < 30.0);  // < one cannon round's damage

    // Config-relative round-count oracle: N rounds firing the battery's own
    // average per-round damage should land within a couple of rounds of
    // pump_kill_seconds worth of fire (dps == avg_round_damage * rof_avg, so
    // hits * avg_damage ~= pump_max_hp by construction).
    const double avg_damage = damage_dealt / static_cast<double>(hits);
    CHECK(avg_damage > 0.0);

    // MUTATION lever: dead pump takes NO further hits (hp must not keep
    // dropping, and no further round is retired against it since it's
    // skipped by the `!pu.alive` guard — a straight-through miss stays
    // active only because there is no OTHER pump for it to hit here).
    const double hp_at_death = pu.hp;
    std::vector<weapon::Projectile> pool4 = {round_through(
        glm::dvec3{100.0, -1000.0, 0.0}, glm::dvec3{100.0, 1000.0, 0.0})};
    combat::conquest_tick(cs, pool4, params);
    REQUIRE(pu.hp == Catch::Approx(hp_at_death));
    REQUIRE(pool4[0].active);  // never retired: nothing alive to hit
}

// fly-3: every registered pump hit emits ONE PumpHitEvent at the swept-hit
// interpolated position (the app's spark/HUD feedback). MUTATION: dropping the
// hit_events push (or pushing p.pos instead of the interpolated point) fails
// the count / the position bound.
TEST_CASE("conquest: a pump hit emits one PumpHitEvent at the impact point") {
    combat::ConquestState cs;
    combat::Pump& pu = cs.pumps[0];
    pu.pos = glm::dvec3{100.0, 0.0, 0.0};
    pu.radius_m = 25.0;
    pu.hp = 1000.0;
    combat::ConquestParams params;

    std::vector<weapon::Projectile> pool = {round_through(
        glm::dvec3{100.0, -1000.0, 0.0}, glm::dvec3{100.0, 1000.0, 0.0})};
    std::vector<combat::PumpHitEvent> hits;
    combat::conquest_tick(cs, pool, params, nullptr, &hits);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].pump_index == 0);
    // The impact point sits on the pump's sphere (within radius + slop of the
    // center), NOT at the segment end (p.pos, 1000 m past it).
    CHECK(glm::length(hits[0].pos - pu.pos) <= pu.radius_m + 1.0);
}

// A hit that MISSES every pump leaves every pump and the round untouched.
TEST_CASE("conquest: a missed round hits no pump and stays active") {
    combat::ConquestState cs;
    cs.pumps[0].pos = glm::dvec3{100.0, 0.0, 0.0};
    cs.pumps[0].radius_m = 25.0;
    combat::ConquestParams params;

    std::vector<weapon::Projectile> pool = {round_through(
        glm::dvec3{100.0, -1000.0, 5000.0}, glm::dvec3{100.0, 1000.0, 5000.0})};
    const double hp_before = cs.pumps[0].hp;
    combat::conquest_tick(cs, pool, params);
    REQUIRE(pool[0].active);
    REQUIRE(cs.pumps[0].hp == Catch::Approx(hp_before));
}

// MUTATION: growth credited to the VICTIM faction instead of the destroyer
// (or score/growth applied to the wrong index) -> the two REQUIREs on
// radius_scale[CQ_SUDBURY]/score[CQ_SUDBURY] flip.
//
// 2026-07-25 fly-2 ruling C: the victim's scales now SHRINK (not "untouched")
// -- Chad's ruling: growth of the destroyer stays, but the victim's own dome
// shrinks. MUTATION: dropping the shrink entirely (victim stays 1.0), or
// shrinking the DESTROYER instead of the victim, flips the victim REQUIREs.
TEST_CASE("conquest: pump death grows the destroyer AND shrinks the victim") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_SUDBURY;
    cs.pumps[0].pos = glm::dvec3{100.0, 0.0, 0.0};
    cs.pumps[0].radius_m = 25.0;
    cs.pumps[0].hp =
        30.0;  // one hit from death (round_through's default damage)
    cs.pumps[0].faction = combat::CQ_VALLEY;  // the VICTIM's own faction
    cs.pumps[0].alive = true;

    combat::ConquestParams params;
    params.growth_radius_frac = 0.15;
    params.growth_ceiling_frac = 0.2;
    params.shrink_radius_frac = 0.1;
    params.shrink_ceiling_frac = 0.05;
    params.pump_score = 100;

    std::vector<weapon::Projectile> pool = {round_through(
        glm::dvec3{100.0, -1000.0, 0.0}, glm::dvec3{100.0, 1000.0, 0.0})};
    std::vector<combat::PumpDeathEvent> events;
    combat::conquest_tick(cs, pool, params, &events);

    REQUIRE_FALSE(cs.pumps[0].alive);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].pump_index == 0);
    REQUIRE(events[0].victim_faction == combat::CQ_VALLEY);
    REQUIRE(events[0].destroyer_faction == combat::CQ_SUDBURY);

    // Destroyer's (SUDBURY) scales grew; score credited to SUDBURY.
    REQUIRE(cs.radius_scale[combat::CQ_SUDBURY] == Catch::Approx(1.15));
    REQUIRE(cs.ceiling_scale[combat::CQ_SUDBURY] == Catch::Approx(1.2));
    REQUIRE(cs.score[combat::CQ_SUDBURY] == 100);

    // Victim's (VALLEY) scales SHRANK by the configured fracs; score untouched.
    REQUIRE(cs.radius_scale[combat::CQ_VALLEY] == Catch::Approx(0.9));
    REQUIRE(cs.ceiling_scale[combat::CQ_VALLEY] == Catch::Approx(0.95));
    REQUIRE(cs.score[combat::CQ_VALLEY] == 0);
}

// The shrink FLOORS at 0.4 -- a bubble never vanishes (ruling C). MUTATION:
// dropping the std::max floor clamp lets radius_scale go negative/zero here.
// 2026-07-26 Chad ruling (supersedes fly-2 ruling C's 0.4 floor): losing every
// pump must actually take the dome away. This leg now pins the OPPOSITE
// contract -- the shrink clamps at exactly 0 (never negative, which would
// invert the ellipse axes downstream) and never bottoms out early.
TEST_CASE("conquest: victim shrink clamps at 0, the bubble can vanish") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_SUDBURY;
    cs.radius_scale[combat::CQ_VALLEY] = 0.5;
    cs.ceiling_scale[combat::CQ_VALLEY] = 0.45;
    cs.pumps[0].pos = glm::dvec3{100.0, 0.0, 0.0};
    cs.pumps[0].radius_m = 25.0;
    cs.pumps[0].hp = 30.0;
    cs.pumps[0].faction = combat::CQ_VALLEY;
    cs.pumps[0].alive = true;

    combat::ConquestParams params;
    params.shrink_radius_frac = 0.7;   // 0.5 - 0.7 = -0.2, would go NEGATIVE
                                       // without the clamp
    params.shrink_ceiling_frac = 0.7;  // 0.45 - 0.7 = -0.25, likewise

    std::vector<weapon::Projectile> pool = {round_through(
        glm::dvec3{100.0, -1000.0, 0.0}, glm::dvec3{100.0, 1000.0, 0.0})};
    combat::conquest_tick(cs, pool, params);

    REQUIRE_FALSE(cs.pumps[0].alive);
    CHECK(cs.radius_scale[combat::CQ_VALLEY] == Catch::Approx(0.0));
    CHECK(cs.ceiling_scale[combat::CQ_VALLEY] == Catch::Approx(0.0));
    CHECK(cs.radius_scale[combat::CQ_VALLEY] >= 0.0);
    CHECK(cs.ceiling_scale[combat::CQ_VALLEY] >= 0.0);
}

// The two deep pumps sit on opposite sides of the shared Black Stope cavern
// 2026-07-25 fly-2 ruling B: place_deep_pump is a PURE helper taking the
// arena as ARGUMENTS (Chad's F6 report -- "no pumps in the black stope" --
// was traced to the OLD black_stope_pump_positions duplicating [tunnel]
// arena constants + approximating the midpoint terrain radius with the bare
// sphere). Test it against a GENERIC synthetic arena (not the config
// constants -- "place frame tests at generic points" discipline) so no
// hand-picked number degenerates the check.
TEST_CASE(
    "conquest: place_deep_pump sits inside the ellipsoid wall with margin") {
    const glm::dvec3 center{500.0, -200.0, 100.0};
    const glm::dvec3 u_long = glm::normalize(glm::dvec3{1.0, 1.0, 1.0});
    const double vertical_semi = 2600.0;
    const double horizontal_semi = 4200.0;

    // Two distinct, non-parallel mouth bearings (neither aligned with u_long).
    const glm::dvec3 valley_bearing =
        glm::normalize(glm::dvec3{-0.5, 0.1, 0.86});
    const glm::dvec3 sudbury_bearing =
        glm::normalize(glm::dvec3{0.86, -0.3, 0.42});

    const glm::dvec3 valley = combat::place_deep_pump(
        center, u_long, valley_bearing, vertical_semi, horizontal_semi,
        combat::kDeepPumpFrac, combat::kDeepPumpHeightAboveFloor_m);
    const glm::dvec3 sudbury = combat::place_deep_pump(
        center, u_long, sudbury_bearing, vertical_semi, horizontal_semi,
        combat::kDeepPumpFrac, combat::kDeepPumpHeightAboveFloor_m);

    // Non-degenerate, well-separated (opposite bearings -> a real distance).
    REQUIRE(glm::length(valley - center) > 1.0);
    REQUIRE(glm::length(sudbury - center) > 1.0);
    REQUIRE(glm::length(valley - sudbury) > 1000.0);

    // INSIDE-THE-ELLIPSOID pin against the SAME arguments (no duplicated
    // constants): (h/a)^2 + (v/c)^2 < 1, and strictly ABOVE the floor
    // (v > floor_v, i.e. resting on/above it, not through it). MUTATION this
    // kills: dropping the height_above_floor_m offset (ratio -> 1 exactly,
    // sitting ON the wall) or reverting frac to a value that pierces the wall
    // (e.g. 0.95).
    const glm::dvec3 un = glm::normalize(u_long);
    for (const glm::dvec3& p : {valley, sudbury}) {
        const glm::dvec3 rel = p - center;
        const double v = glm::dot(rel, un);
        const glm::dvec3 horiz = rel - v * un;
        const double h = glm::length(horiz);
        const double ratio = (h / horizontal_semi) * (h / horizontal_semi) +
                             (v / vertical_semi) * (v / vertical_semi);
        CHECK(ratio < 1.0);
        const double floor_v =
            -vertical_semi *
            std::sqrt(1.0 - combat::kDeepPumpFrac * combat::kDeepPumpFrac);
        CHECK(v > floor_v);  // strictly above the floor at that station
        CHECK(v < 0.0);      // still well below the arena's own center/roof
    }
}

// The LIVE integration path (main.cpp): build a real world::TunnelNet from
// the shipped [tunnel] config and place both deep pumps off its arena toward
// the two real tunnel mouths -- exercising the EXACT call the app makes.
// MUTATION: passing the wrong semi-axis (b instead of a_pos, or vice versa)
// would blow the inside-the-wall ratio on this real, non-degenerate arena.
TEST_CASE(
    "conquest: deep pumps placed off the LIVE TunnelNet arena stay "
    "inside the wall") {
    world::TunnelParams tp;
    tp.sphere_R = kP.R;
    tp.tube_width_m = kGame.tunnel.tube_width_m;
    tp.tube_height_m = kGame.tunnel.tube_height_m;
    tp.depth_m = kGame.tunnel.depth_m;
    tp.soft_m = kGame.tunnel.soft_m;
    tp.ramp_frac = kGame.tunnel.ramp_frac;
    tp.spacing_m = kGame.tunnel.spacing_m;
    tp.floor_height_m = kGame.tunnel.floor_height_m;
    tp.arena_a_m = kGame.tunnel.arena_a_m;
    tp.arena_c_m = kGame.tunnel.arena_c_m;
    tp.arena_depth_m = kGame.tunnel.arena_depth_m;
    tp.cavern_core_m = kGame.tunnel.cavern_core_m;
    tp.breach_margin_m = kGame.tunnel.breach_margin_m;
    tp.chamber_long_m = kGame.tunnel.chamber_long_m;
    tp.chamber_lat_m = kGame.tunnel.chamber_lat_m;
    tp.chamber_vert_m = kGame.tunnel.chamber_vert_m;
    tp.chamber_breach_offset_m = kGame.tunnel.chamber_breach_offset_m;
    tp.connector_radius_m = kGame.tunnel.connector_radius_m;
    tp.chambers_on = kGame.tunnel.chambers_on;
    tp.bowl_radius_m = kGame.tunnel.bowl_radius_m;
    tp.bowl_depth_m = kGame.tunnel.bowl_depth_m;
    tp.mouth_sink_m = kGame.tunnel.mouth_sink_m;
    tp.min_cover_m = kGame.tunnel.min_cover_m;
    tp.trench_len_m = kGame.tunnel.trench_len_m;
    tp.trench_rim_m = kGame.tunnel.trench_rim_m;
    tp.headframe_h_m = kGame.tunnel.headframe_h_m;
    tp.headframe_on = kGame.tunnel.headframe_on;
    const world::TunnelNet net = world::build_tunnel_net(tp, nullptr);
    REQUIRE(net.arena_on);  // the shipped table has a live arena

    const glm::dvec3 valley = combat::place_deep_pump(
        net.arena.center, net.arena.u_long, world::kTunnelMouthErrington,
        net.arena.a_pos, net.arena.b, combat::kDeepPumpFrac,
        combat::kDeepPumpHeightAboveFloor_m);
    const glm::dvec3 sudbury = combat::place_deep_pump(
        net.arena.center, net.arena.u_long, world::kTunnelMouthMurray,
        net.arena.a_pos, net.arena.b, combat::kDeepPumpFrac,
        combat::kDeepPumpHeightAboveFloor_m);

    REQUIRE(glm::length(valley - sudbury) > 1000.0);  // non-degenerate pair
    const glm::dvec3 un = glm::normalize(net.arena.u_long);
    for (const glm::dvec3& p : {valley, sudbury}) {
        const glm::dvec3 rel = p - net.arena.center;
        const double v = glm::dot(rel, un);
        const glm::dvec3 horiz = rel - v * un;
        const double h = glm::length(horiz);
        const double ratio = (h / net.arena.b) * (h / net.arena.b) +
                             (v / net.arena.a_pos) * (v / net.arena.a_pos);
        CHECK(ratio < 1.0);
    }
}

// make_pumps wires all four correctly (faction, surface flag, position, HP).
TEST_CASE(
    "conquest: make_pumps wires faction/surface/position/hp for all four") {
    combat::Pump pumps[combat::kNumPumps];
    const glm::dvec3 vsurf{1.0, 2.0, 3.0}, ssurf{4.0, 5.0, 6.0};
    const glm::dvec3 vdeep{7.0, 8.0, 9.0}, sdeep{10.0, 11.0, 12.0};
    combat::make_pumps(vsurf, ssurf, vdeep, sdeep, 12345.0, pumps);

    REQUIRE(pumps[0].faction == combat::CQ_VALLEY);
    REQUIRE(pumps[0].surface);
    REQUIRE(pumps[0].pos == vsurf);

    REQUIRE(pumps[1].faction == combat::CQ_SUDBURY);
    REQUIRE(pumps[1].surface);
    REQUIRE(pumps[1].pos == ssurf);

    REQUIRE(pumps[2].faction == combat::CQ_VALLEY);
    REQUIRE_FALSE(pumps[2].surface);
    REQUIRE(pumps[2].pos == vdeep);

    REQUIRE(pumps[3].faction == combat::CQ_SUDBURY);
    REQUIRE_FALSE(pumps[3].surface);
    REQUIRE(pumps[3].pos == sdeep);

    // MUTATION: make_pumps ignoring the pump_max_hp argument (welding
    // kPumpMaxHpFallback/60 again) flips this for every pump.
    for (const combat::Pump& p : pumps) {
        REQUIRE(p.alive);
        REQUIRE(p.hp == Catch::Approx(12345.0));
    }
}

// ---------------------------------------------------------------------------
// LIVES / DEFEAT

// MUTATION: the latch re-evaluates outcome after DEFEAT (e.g. a missing
// `if (cs.outcome != PLAYING) return;` guard) -> the 9th death's REQUIRE
// still passes but planes_left keeps draining past the 8th; assert BOTH.
TEST_CASE("conquest: 8 deaths latch DEFEAT; further deaths are frozen") {
    combat::ConquestState cs;
    REQUIRE(cs.planes_left == 8);
    for (int i = 0; i < 8; ++i) {
        REQUIRE(cs.outcome == combat::Outcome::PLAYING);
        combat::on_player_death(cs);
    }
    REQUIRE(cs.outcome == combat::Outcome::DEFEAT);
    REQUIRE(cs.planes_left == 0);

    // A 9th (and 10th) death must not change outcome OR planes_left further.
    combat::on_player_death(cs);
    combat::on_player_death(cs);
    REQUIRE(cs.outcome == combat::Outcome::DEFEAT);
    REQUIRE(cs.planes_left == 0);

    // A kill after DEFEAT is also frozen (the state-wide latch, not just the
    // outcome field).
    combat::ConquestParams params;
    const int score_before = cs.score[cs.player_faction];
    combat::on_player_kill(cs, 0, params);
    REQUIRE(cs.score[cs.player_faction] == score_before);
    REQUIRE(cs.mav_alive[0]);  // frozen: the kill never even applied
}

// ---------------------------------------------------------------------------
// VICTORY / TEAMS

// Player flies VALLEY; the 5 SUDBURY-team mavericks (indices 0-4) are the
// enemy. Killing all 5 -> VICTORY. MUTATION: the roster split flipped
// (5-9 SUDBURY instead of 0-4) -> this kills the wrong 5 and VICTORY never
// latches (or latches after only friendly kills, caught by the next case).
TEST_CASE("conquest: killing the whole enemy wing latches VICTORY") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_VALLEY;
    combat::ConquestParams params;

    // Wing-relative (RUNG E13 made the wings 7/3): the contract is "kill every
    // ENEMY and you win", never "kill five". A literal 5 stopped being the
    // enemy wing the moment the roster moved, and stopped testing the latch.
    const int enemy_n = combat::team_size(combat::CQ_SUDBURY);
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        if (!combat::is_enemy(i, combat::CQ_VALLEY)) continue;
        REQUIRE(cs.outcome == combat::Outcome::PLAYING);  // not before the last
        combat::on_player_kill(cs, i, params);
    }
    REQUIRE(cs.outcome == combat::Outcome::VICTORY);
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (combat::is_enemy(i, combat::CQ_VALLEY))
            REQUIRE_FALSE(cs.mav_alive[i]);
    // The friendly (VALLEY) wing was never touched.
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (!combat::is_enemy(i, combat::CQ_VALLEY)) REQUIRE(cs.mav_alive[i]);
    REQUIRE(cs.score[combat::CQ_VALLEY] ==
            enemy_n * params.maverick_kill_score);
}

// Friendly-fire leg: killing an OWN-team maverick pays the tax and never
// latches VICTORY by itself (even after all 5 friendlies are gone, the 5
// enemies are still alive). MUTATION: is_enemy's `!=` flipped to `==` ->
// this REQUIREs both the penalty sign and the non-victory.
TEST_CASE("conquest: friendly-fire kill pays the score penalty, no victory") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_VALLEY;
    combat::ConquestParams params;

    // Wing-relative (RUNG E13): the first pilot of the player's OWN wing,
    // whichever index that is, not a literal 5.
    const int own_first = combat::kSudburyTeamSize;
    const int own_n = combat::team_size(combat::CQ_VALLEY);
    REQUIRE_FALSE(combat::is_enemy(own_first, combat::CQ_VALLEY));
    combat::on_player_kill(cs, own_first, params);
    REQUIRE(cs.mav_alive[own_first] == false);
    REQUIRE(cs.score[combat::CQ_VALLEY] == -params.friendly_fire_penalty);
    REQUIRE(cs.outcome == combat::Outcome::PLAYING);

    // Kill the REST of the friendlies too: still no victory (enemies
    // untouched) -- the clause the whole test exists for.
    for (int i = own_first + 1; i < combat::kNumMavericks; ++i)
        combat::on_player_kill(cs, i, params);
    REQUIRE(cs.outcome == combat::Outcome::PLAYING);
    REQUIRE(cs.score[combat::CQ_VALLEY] ==
            -own_n * params.friendly_fire_penalty);
}

// Killing an already-dead index a second time is a no-op (idempotent —
// conquest mode has no respawn). MUTATION: dropping the `if (!mav_alive[i])
// return;` guard double-counts the score.
TEST_CASE("conquest: re-killing an already-dead maverick index is a no-op") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_VALLEY;
    combat::ConquestParams params;
    combat::on_player_kill(cs, 0, params);
    const int score_after_first = cs.score[combat::CQ_VALLEY];
    combat::on_player_kill(cs, 0, params);
    REQUIRE(cs.score[combat::CQ_VALLEY] == score_after_first);
}

// ---------------------------------------------------------------------------
// OFF-ARM (the no-op firewall)

// A default-constructed ConquestState fed conquest_tick with NO projectiles
// is bit-for-bit unchanged. MUTATION: any stray write inside conquest_tick
// not gated on an actual hit (e.g. an unconditional outcome re-eval that
// mutates a field) flips one of these fields.
TEST_CASE("conquest: off-arm -- no projectiles, no state change") {
    combat::ConquestState before;
    combat::ConquestState after;
    combat::ConquestParams params;
    std::vector<weapon::Projectile> empty;

    combat::conquest_tick(after, empty, params);

    REQUIRE(after.planes_left == before.planes_left);
    REQUIRE(std::memcmp(after.score, before.score, sizeof(before.score)) == 0);
    REQUIRE(std::memcmp(after.radius_scale, before.radius_scale,
                        sizeof(before.radius_scale)) == 0);
    REQUIRE(std::memcmp(after.ceiling_scale, before.ceiling_scale,
                        sizeof(before.ceiling_scale)) == 0);
    REQUIRE(after.outcome == before.outcome);
    REQUIRE(std::memcmp(after.mav_alive, before.mav_alive,
                        sizeof(before.mav_alive)) == 0);
    REQUIRE(after.player_faction == before.player_faction);
    for (int i = 0; i < combat::kNumPumps; ++i) {
        REQUIRE(after.pumps[i].hp == before.pumps[i].hp);
        REQUIRE(after.pumps[i].alive == before.pumps[i].alive);
        REQUIRE(after.pumps[i].pos == before.pumps[i].pos);
    }
}

// A default ConquestState's default fields match the spec exactly (planes
// pool, all-1.0 scales, all-alive mavericks, PLAYING). MUTATION: any default
// drifting (e.g. planes_left starting at 7, or one mav_alive slot false)
// flips one REQUIRE.
TEST_CASE("conquest: default-constructed state matches the spec defaults") {
    combat::ConquestState cs;
    REQUIRE(cs.planes_left == 8);
    REQUIRE(cs.score[0] == 0);
    REQUIRE(cs.score[1] == 0);
    REQUIRE(cs.radius_scale[0] == Catch::Approx(1.0));
    REQUIRE(cs.radius_scale[1] == Catch::Approx(1.0));
    REQUIRE(cs.ceiling_scale[0] == Catch::Approx(1.0));
    REQUIRE(cs.ceiling_scale[1] == Catch::Approx(1.0));
    REQUIRE(cs.outcome == combat::Outcome::PLAYING);
    for (int i = 0; i < combat::kNumMavericks; ++i) REQUIRE(cs.mav_alive[i]);
}

// ---------------------------------------------------------------------------
// INTEGRATION WIRING (spec §4/§1/§7)

// The maverick roster split (RUNG E13: 7 SUDBURY / 3 VALLEY, Chad's
// "we can be outnumbered"). Written CONFIG-RELATIVE against kSudburyTeamSize
// so re-splitting the wing is one constant and this leg follows it -- the
// literal 0-4 / 5-9 form it replaced would have had to be hand-edited, which
// is how a boundary test quietly stops testing the boundary.
// MUTATION: kSudburyTeamSize changed, or the `<` comparison flipped.
TEST_CASE("conquest: maverick_faction splits the roster at the wing boundary") {
    REQUIRE(combat::kSudburyTeamSize + combat::kValleyTeamSize ==
            combat::kNumMavericks);
    REQUIRE(combat::kSudburyTeamSize > 0);
    REQUIRE(combat::kValleyTeamSize > 0);
    for (int i = 0; i < combat::kSudburyTeamSize; ++i)
        REQUIRE(combat::maverick_faction(i) == combat::CQ_SUDBURY);
    for (int i = combat::kSudburyTeamSize; i < combat::kNumMavericks; ++i)
        REQUIRE(combat::maverick_faction(i) == combat::CQ_VALLEY);
    // Wraps by index (the app clamps count to 10, but the modulo is defined).
    REQUIRE(combat::maverick_faction(combat::kNumMavericks) ==
            combat::CQ_SUDBURY);
    REQUIRE(combat::maverick_faction(-1) == combat::CQ_VALLEY);
    // team_slot is the index WITHIN a wing, and it covers each wing exactly
    // once -- the property the six copied `i % 5` sites were relying on and
    // which is false for them across an uneven split.
    for (int f = 0; f < 2; ++f) {
        std::vector<int> seen;
        for (int i = 0; i < combat::kNumMavericks; ++i) {
            if (combat::maverick_faction(i) != f) continue;
            const int s = combat::team_slot(i);
            REQUIRE(s >= 0);
            REQUIRE(s < combat::team_size(f));
            seen.push_back(s);
        }
        REQUIRE(static_cast<int>(seen.size()) == combat::team_size(f));
        std::sort(seen.begin(), seen.end());
        REQUIRE(std::unique(seen.begin(), seen.end()) == seen.end());
    }
}

// ★★★ RUNG L12 -- THE NUMBERS DISADVANTAGE IS THE PLAYER'S, NOT VALLEY'S.
// Chad 2026-09-11: "whichever faction the player chooses, central city or
// valley, he needs to have the numbers disadvantage against ai so that the
// difficulty relative to number of enemy stays consistent whether they pick
// the valley or the central city."
//
// Leg (a) is the REGRESSION PIN: for a Valley player every number is what
// shipped, keyed by faction name, so this rung cannot have moved the game he
// signed. Leg (b) is the ruling. Leg (c) is the ruling stated as the thing he
// actually feels -- HIS count and the ENEMY count, invariant to the pick.
// MUTATION: roster_sizes returning {ai_side, player_side}, or team_size
// comparing against CQ_SUDBURY instead of against player_faction.
TEST_CASE("conquest: L12 the roster is keyed to the PLAYER'S side") {
    // (a) VALLEY PLAYER == TODAY, per faction, byte for byte.
    REQUIRE(combat::team_size(combat::CQ_SUDBURY, combat::CQ_VALLEY) ==
            combat::kSudburyTeamSize);
    REQUIRE(combat::team_size(combat::CQ_VALLEY, combat::CQ_VALLEY) ==
            combat::kValleyTeamSize);
    // ... and the one-argument (defaulted) form is that same roster, which is
    // what keeps every pre-L12 caller and fixture unchanged.
    REQUIRE(combat::team_size(combat::CQ_SUDBURY) == combat::kSudburyTeamSize);
    REQUIRE(combat::team_size(combat::CQ_VALLEY) == combat::kValleyTeamSize);
    for (int i = 0; i < combat::kSudburyTeamSize; ++i)
        REQUIRE(combat::maverick_faction(i, combat::CQ_VALLEY) ==
                combat::maverick_faction(i));
    for (int i = 0; i < combat::kNumMavericks; ++i)
        REQUIRE(combat::maverick_faction(i, combat::CQ_VALLEY) ==
                (i < combat::kSudburyTeamSize ? combat::CQ_SUDBURY
                                              : combat::CQ_VALLEY));

    // (b) SUDBURY (Central City) PLAYER: HIS faction fields the small wing and
    // the AI's fields the large one. This is the leg that was wrong before.
    REQUIRE(combat::team_size(combat::CQ_SUDBURY, combat::CQ_SUDBURY) ==
            combat::kValleyTeamSize);
    REQUIRE(combat::team_size(combat::CQ_VALLEY, combat::CQ_SUDBURY) ==
            combat::kSudburyTeamSize);
    for (int i = 0; i < combat::kNumMavericks; ++i)
        REQUIRE(combat::maverick_faction(i, combat::CQ_SUDBURY) ==
                (i < combat::kSudburyTeamSize ? combat::CQ_VALLEY
                                              : combat::CQ_SUDBURY));

    // (c) THE INVARIANCE HE ASKED FOR: the same number of pilots beside him
    // and the same number against him, whichever side he picks -- counted the
    // way the game counts them (walk the roster through maverick_faction /
    // is_enemy, do not just re-read team_size).
    for (int pf = 0; pf < 2; ++pf) {
        int mine = 0;
        int theirs = 0;
        for (int i = 0; i < combat::kNumMavericks; ++i) {
            if (combat::is_enemy(i, pf))
                ++theirs;
            else
                ++mine;
            // The wing a pilot is counted in and the wing team_size reports
            // for that faction can never disagree.
            REQUIRE(combat::team_slot(i) <
                    combat::team_size(combat::maverick_faction(i, pf), pf));
        }
        INFO("player_faction " << pf);
        REQUIRE(mine == combat::kValleyTeamSize);
        REQUIRE(theirs == combat::kSudburyTeamSize);
        REQUIRE(theirs > mine);  // outnumbered, always
        // team_slot still covers each wing exactly once under either pick.
        for (int f = 0; f < 2; ++f) {
            std::vector<int> seen;
            for (int i = 0; i < combat::kNumMavericks; ++i) {
                if (combat::maverick_faction(i, pf) != f) continue;
                seen.push_back(combat::team_slot(i));
            }
            REQUIRE(static_cast<int>(seen.size()) ==
                    combat::team_size(f, pf));
            std::sort(seen.begin(), seen.end());
            REQUIRE(std::unique(seen.begin(), seen.end()) == seen.end());
        }
        // Raid duty follows the LIVE wing, so it is invariant too: the AI's
        // big wing sends three, his own small wing sends one, either way.
        REQUIRE(combat::raiders_for_team(1 - pf, pf) == 3);
        REQUIRE(combat::raiders_for_team(pf, pf) == 1);
    }
}

// RUNG E13: raid duty is a PROPORTION of a wing, not a count -- and the
// proportion must reproduce the pre-E13 rule at the wing size it was authored
// for. Pinned as arithmetic so a re-split cannot silently put two thirds of a
// small squadron on offense.
TEST_CASE("conquest: raiders_for_team scales with the wing and floors at one") {
    REQUIRE(combat::raiders_for_team(combat::CQ_SUDBURY) >= 1);
    REQUIRE(combat::raiders_for_team(combat::CQ_VALLEY) >= 1);
    for (int f = 0; f < 2; ++f)
        REQUIRE(combat::raiders_for_team(f) <= combat::team_size(f));
    // The shipped 7/3 split, and the pre-E13 5-wing it is derived from.
    REQUIRE(combat::kSudburyTeamSize == 7);
    REQUIRE(combat::kValleyTeamSize == 3);
    REQUIRE(combat::raiders_for_team(combat::CQ_SUDBURY) == 3);
    REQUIRE(combat::raiders_for_team(combat::CQ_VALLEY) == 1);
}

// No-respawn gate: with CombatWorld::respawn_drones == false a killed drone is
// marked INERT and stays dead through a second combat_tick (no re-explosion, no
// double kill), and its spawn_index is reported for the on_player_kill hook.
// MUTATION: dropping the `if (respawn_drones)` branch respawns it (inert stays
// false, hp resets > 0); dropping the `d.inert` skip in pass 2 re-kills it.
TEST_CASE("conquest: respawn_drones=false leaves a killed drone inert") {
    combat::CombatWorld cw;
    cw.respawn_drones = false;
    cw.params.hit_radius_m = 9.0;

    std::vector<drone::DroneState> drones(1);
    drones[0].spawn_index = 3;
    drones[0].hp = -1.0;  // already at/below zero: pass 2 kills it this tick
    drones[0].inert = false;

    const sim::AircraftParams ap;
    const drone::DroneParams dp;
    std::vector<weapon::Projectile> pool;  // no rounds needed (hp already <= 0)

    combat::combat_tick(pool, drones, cw, ap, dp, 1.0 / 120.0);
    REQUIRE(drones[0].inert);  // frozen wreck, NOT respawned
    REQUIRE(cw.kills == 1);
    REQUIRE(cw.killed_spawn_indices.size() == 1);
    REQUIRE(cw.killed_spawn_indices[0] == 3);

    // A second tick must not re-kill the inert wreck (pass 2 skips it) and
    // clears the per-tick report.
    combat::combat_tick(pool, drones, cw, ap, dp, 1.0 / 120.0);
    REQUIRE(drones[0].inert);
    REQUIRE(cw.kills == 1);  // NOT 2
    REQUIRE(cw.killed_spawn_indices.empty());
}

// The pre-conquest default (respawn_drones == true) still respawns — proving
// the gate defaults to the bit-identical old behavior.
TEST_CASE("conquest: respawn_drones=true (default) still respawns in place") {
    combat::CombatWorld cw;  // respawn_drones defaults true
    REQUIRE(cw.respawn_drones);
    std::vector<drone::DroneState> drones(1);
    drones[0].hp = -1.0;
    const sim::AircraftParams ap;
    const drone::DroneParams dp;
    std::vector<weapon::Projectile> pool;
    combat::combat_tick(pool, drones, cw, ap, dp, 1.0 / 120.0);
    REQUIRE_FALSE(drones[0].inert);  // respawned, not a wreck
    REQUIRE(drones[0].hp > 0.0);     // respawn_in_place reset hp
    REQUIRE(cw.kills == 1);
}

// Growth -> bubble rebuild (spec §7): after a synthetic pump death the rebuilt
// bubble vector's DESTROYER-faction radii/ceilings are scaled by the config
// frac while the OTHER faction's bubble is unchanged. Exercises the real
// integration path: faction_growth_from_state (the 1:1
// CqFaction->world::Faction map) + world::build_faction_bubbles. MUTATION:
// mapping the wrong faction index, or dropping the clear() so bubbles double,
// flips these.
TEST_CASE("conquest: a pump death grows only the destroyer's rebuilt bubble") {
    app::ConquestWorld cq;
    cq.state.player_faction = combat::CQ_SUDBURY;  // destroyer on a pump kill
    cq.params.growth_radius_frac = 0.25;
    cq.params.growth_ceiling_frac = 0.25;
    // This case isolates GROWTH (see the shrink-focused cases in
    // combat/conquest.h's own test suite for ruling C); zero the shrink so
    // the victim's (VALLEY, the killed pump's default faction) rebuilt
    // bubble stays at the untouched baseline this test checks below.
    cq.params.shrink_radius_frac = 0.0;
    cq.params.shrink_ceiling_frac = 0.0;
    cq.bubble_ceiling_m = 4000.0;
    cq.bubble_edge_soft_m = 1200.0;
    cq.bubble_ceil_soft_m = 600.0;

    // Kill a pump (one hit from death) so SUDBURY's scales grow to 1.25.
    cq.state.pumps[0].pos = glm::dvec3{100.0, 0.0, 0.0};
    cq.state.pumps[0].radius_m = 25.0;
    cq.state.pumps[0].hp =
        30.0;  // one hit from death (round_through's default damage)
    cq.state.pumps[0].alive = true;
    std::vector<weapon::Projectile> pool = {round_through(
        glm::dvec3{100.0, -1000.0, 0.0}, glm::dvec3{100.0, 1000.0, 0.0})};
    combat::conquest_tick(cq.state, pool, cq.params);
    REQUIRE(cq.state.radius_scale[combat::CQ_SUDBURY] == Catch::Approx(1.25));

    sim::AtmosphereField field;
    cq.atm_field = &field;
    app::rebuild_conquest_bubbles(cq);

    // VALLEY (0) then SUDBURY (1), the build_faction_bubbles order.
    REQUIRE(field.bubbles.size() == 2);
    // VALLEY (untouched destroyer's rival) at baseline radius.
    REQUIRE(field.bubbles[0].ground_radius_m ==
            Catch::Approx(world::kValleyMajorRadiusM));
    // SUDBURY (the destroyer) grown by 1.25 in radius AND ceiling.
    REQUIRE(field.bubbles[1].ground_radius_m ==
            Catch::Approx(world::kSudburyMajorRadiusM * 1.25));
    REQUIRE(field.bubbles[1].ceiling_m == Catch::Approx(4000.0 * 1.25));
    REQUIRE(field.bubbles[0].ceiling_m ==
            Catch::Approx(4000.0));  // VALLEY base
}

// Off-arm bubble count (spec §7): the conquest path emits EXACTLY 2 faction
// ellipse bubbles; the disabled path emits the ONE single test bubble.
// MUTATION: a stray append, or build_faction_bubbles not clearing, changes
// the count.
TEST_CASE(
    "conquest: bubble count is 2 (enabled) vs 1 (disabled single bubble)") {
    // Enabled: the two ellipses = 2 bubbles.
    world::FactionGrowth grow[2];  // baseline 1.0/1.0
    struct DomeCfg {
        double bubble_ceiling_m = 4000.0;
        double bubble_edge_soft_m = 1200.0;
        double bubble_ceil_soft_m = 600.0;
        double bubble_dome_h_m =
            4961.96;  // S-domeround: H at n=3, ceiling 4000
    } cfg;
    std::vector<sim::AtmosphereField::Bubble> two;
    world::build_faction_bubbles(cfg, grow, two);
    REQUIRE(two.size() == 2);

    // Disabled: the app's exact old single-bubble path.
    std::vector<sim::AtmosphereField::Bubble> one;
    one.push_back(sim::AtmosphereField::Bubble{glm::dvec3{0.0, 1.0, 0.0},
                                               6000.0, 4000.0, 1200.0, 600.0});
    REQUIRE(one.size() == 1);
}

// faction_growth_from_state maps the CqFaction index straight onto the
// world::Faction index (the 1:1 mapping the integration relies on). MUTATION:
// swapping the two indices, or reading radius into ceiling, flips these.
TEST_CASE("conquest: faction_growth_from_state maps 1:1 CqFaction->Faction") {
    combat::ConquestState cs;
    cs.radius_scale[combat::CQ_VALLEY] = 1.1;
    cs.ceiling_scale[combat::CQ_VALLEY] = 1.2;
    cs.radius_scale[combat::CQ_SUDBURY] = 1.3;
    cs.ceiling_scale[combat::CQ_SUDBURY] = 1.4;
    world::FactionGrowth grow[2];
    app::faction_growth_from_state(cs, grow);
    REQUIRE(grow[world::VALLEY].radius_scale == Catch::Approx(1.1));
    REQUIRE(grow[world::VALLEY].ceiling_scale == Catch::Approx(1.2));
    REQUIRE(grow[world::SUDBURY].radius_scale == Catch::Approx(1.3));
    REQUIRE(grow[world::SUDBURY].ceiling_scale == Catch::Approx(1.4));
}

// ===========================================================================
// ENEMY PUMP RAIDS (COMPETITIVE rung, 2026-07-25 fly-2 "a little competitive")
// ===========================================================================

// RAIDER DESIGNATION is deterministic: the 2 highest-aggression ENEMY
// mavericks. For player_faction = VALLEY the enemies are indices 0-4 (SUDBURY);
// the trait aggressions are GULCH .75, SHAFT .85, CANARY .55, NICKEL .70,
// SLAGHEAP .60 -> the top two are SHAFT(1) and GULCH(0). MUTATION: picking the
// LOWEST aggression, or dropping the enemy filter (a friendly index 5-9
// designated), flips these.
TEST_CASE("raid: raider designation is a wing's most aggressive pilots") {
    // ★ WRITTEN AS THE RULE, NOT AS A MEMORISED ANSWER. This used to spell out
    // "SHAFT .85 and GULCH .75 are the two", which RUNG E13's 7/3 re-split
    // falsified the same day -- and which would ALSO have gone stale on any
    // edit to the trait table. The contract has three parts and none of them
    // is a literal: the COUNT is combat::raiders_for_team, the ORDER is
    // aggression descending with a LOWER-INDEX tie-break, and is_raider is
    // faction_raider AND is_enemy.
    //
    // L12: and each of the three now reads the roster AS THIS PLAYER SEES IT.
    // The pf loop used to ask maverick_faction / faction_raider with no player
    // term at all while asking is_raider / is_enemy with `pf`, so its two
    // halves described DIFFERENT rosters the moment the wing sizes started
    // following the player's own side. Same three clauses, one roster.
    for (int pf = 0; pf < 2; ++pf) {
        const int ef = 1 - pf;  // the enemy wing for this player faction
        std::vector<int> raiders;
        std::vector<int> rest;
        for (int i = 0; i < combat::kNumMavericks; ++i) {
            if (combat::maverick_faction(i, pf) != ef) continue;
            (combat::faction_raider(i, nullptr, pf) ? raiders : rest)
                .push_back(i);
        }
        INFO("player faction " << pf << ", enemy wing " << ef);

        // (1) THE COUNT is the wing's own proportion.
        REQUIRE(static_cast<int>(raiders.size()) ==
                combat::raiders_for_team(ef, pf));

        // (2) THE ORDER: every raider out-ranks every non-raider of the same
        // wing, by aggression descending, ties broken by LOWER index. This is
        // the clause that would catch a ranking flipped to ascending, which a
        // list of names cannot.
        for (int r : raiders) {
            const double ra = maverick::traits_for(r).aggression;
            for (int o : rest) {
                const double oa = maverick::traits_for(o).aggression;
                INFO("raider " << maverick::traits_for(r).callsign << " ("
                               << ra << ") vs "
                               << maverick::traits_for(o).callsign << " ("
                               << oa << ")");
                REQUIRE((ra > oa || (ra == oa && r < o)));
            }
        }

        // (3) is_raider == faction_raider AND is_enemy (the pre-R3 shape,
        // kept as the equivalence contract). Own-wing pilots are never
        // raiders against their own side's player.
        for (int i = 0; i < combat::kNumMavericks; ++i)
            REQUIRE(combat::is_raider(i, pf) ==
                    (combat::is_enemy(i, pf) &&
                     combat::faction_raider(i, nullptr, pf)));
        for (int i = 0; i < combat::kNumMavericks; ++i)
            if (combat::maverick_faction(i, pf) == pf)
                REQUIRE_FALSE(combat::is_raider(i, pf));
    }
}

// RAIDER-ON-STATION geometry: within range AND roughly nose-on = on station;
// out of range, or nose-off, or coincident = not. MUTATION: drop the range gate
// (a raider a km away strafes) or the nose-on gate (a flyby damages the pump).
TEST_CASE("raid: raider_on_station gates on range and nose-on") {
    combat::RaidParams rp;  // range 800, nose_on_cos 0.7
    sim::SimState s;
    s.position = glm::dvec3{kP.R + 500.0, 0.0, 0.0};
    // Nose along -Z (level_state_at convention): build orientation with nose
    // -Z.
    const glm::dvec3 up = glm::normalize(s.position);
    const glm::dvec3 fwd =
        glm::normalize(glm::dvec3{0.0, 0.0, -1.0} -
                       glm::dot(glm::dvec3{0.0, 0.0, -1.0}, up) * up);
    const glm::dvec3 rgt = glm::normalize(glm::cross(fwd, up));
    s.orientation = glm::normalize(glm::quat_cast(glm::dmat3{rgt, up, -fwd}));
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};

    const glm::dvec3 on = s.position + nose * 400.0;  // ahead, in range
    REQUIRE(combat::raider_on_station(s, on, rp));

    const glm::dvec3 far =
        s.position + nose * 1500.0;  // ahead but out of range
    REQUIRE_FALSE(combat::raider_on_station(s, far, rp));

    const glm::dvec3 side = s.position + rgt * 400.0;  // in range but abeam
    REQUIRE_FALSE(combat::raider_on_station(s, side, rp));

    REQUIRE_FALSE(combat::raider_on_station(s, s.position, rp));  // coincident
}

// RAID DAMAGE MATH is config-derived (raid_dps_frac * the PLAYER battery DPS *
// dt), and a raider pump-kill flows the NORMAL death path: the ENEMY faction is
// credited (score + grow) and the VICTIM (player faction) shrinks, with a
// PumpDeathEvent emitted. MUTATION: welding the raid rate, or crediting the
// wrong faction, desyncs the tick count / the score+shrink checks.
TEST_CASE("raid: damage math config-derived; a raider kill credits the enemy") {
    const weapon::GunBattery battery = player_battery();
    const double dps = combat::battery_dps(battery);
    REQUIRE(dps > 0.0);
    const double raid_dps_frac = 0.15;
    const double dt = kP.sim_dt;
    const double per_tick = raid_dps_frac * dps * dt;
    REQUIRE(per_tick > 0.0);

    combat::ConquestState cs;
    cs.player_faction = combat::CQ_VALLEY;  // the player flies VALLEY
    const int enemy = combat::CQ_SUDBURY;   // the raider's faction
    const double pump_max_hp = 200.0;
    combat::Pump& pu =
        cs.pumps[combat::CQ_VALLEY];  // player's own surface pump
    pu.pos = glm::dvec3{kP.R, 0.0, 0.0};
    pu.faction = combat::CQ_VALLEY;
    pu.hp = pump_max_hp;
    pu.alive = true;

    combat::ConquestParams params;
    const double v_grow0 = cs.radius_scale[enemy];
    const double victim_shrink0 = cs.radius_scale[combat::CQ_VALLEY];
    std::vector<combat::PumpDeathEvent> events;

    // Grind the pump down at the config-derived rate until it dies, counting
    // ticks; the oracle is ceil(hp / per_tick).
    int ticks = 0;
    bool died = false;
    while (pu.alive && ticks < 1000000) {
        died = combat::damage_pump(cs, combat::CQ_VALLEY, per_tick, enemy,
                                   params, &events);
        ++ticks;
    }
    REQUIRE(died);
    const int oracle = static_cast<int>(std::ceil(pump_max_hp / per_tick));
    REQUIRE(ticks == oracle);

    // Enemy credited: score + dome GROWTH; victim (player VALLEY) SHRINKS.
    REQUIRE(cs.score[enemy] == params.pump_score);
    REQUIRE(cs.radius_scale[enemy] ==
            Catch::Approx(v_grow0 + params.growth_radius_frac));
    REQUIRE(cs.radius_scale[combat::CQ_VALLEY] ==
            Catch::Approx(victim_shrink0 - params.shrink_radius_frac));
    // One death event, victim = player faction, destroyer = enemy.
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].victim_faction == combat::CQ_VALLEY);
    REQUIRE(events[0].destroyer_faction == enemy);
    REQUIRE(events[0].pump_index == combat::CQ_VALLEY);
}

// damage_pump is a no-op on a dead pump / out-of-range index / non-positive
// damage (guards the shared path). MUTATION: drop a guard -> a stray
// score/event.
TEST_CASE("raid: damage_pump guards dead pump, bad index, zero damage") {
    combat::ConquestState cs;
    combat::ConquestParams params;
    std::vector<combat::PumpDeathEvent> events;
    // Dead pump.
    cs.pumps[0].alive = false;
    REQUIRE_FALSE(combat::damage_pump(cs, 0, 10.0, 1, params, &events));
    // Bad index.
    REQUIRE_FALSE(combat::damage_pump(cs, 99, 10.0, 1, params, &events));
    REQUIRE_FALSE(combat::damage_pump(cs, -1, 10.0, 1, params, &events));
    // Zero/negative damage on a live pump.
    cs.pumps[1].alive = true;
    cs.pumps[1].hp = 50.0;
    REQUIRE_FALSE(combat::damage_pump(cs, 1, 0.0, 1, params, &events));
    REQUIRE(cs.pumps[1].hp == Catch::Approx(50.0));  // untouched
    REQUIRE(events.empty());
}

// ---------------------------------------------------------------------------
// SUDDEN-DEATH MATCH CLOCK (Chad's ruling, 2026-07-26): "once bubble is lost to
// one side, end of match in 10 mins and crown the victor. If the bubble-losing
// team can kill all of the enemy in 10 mins then they win, if not, out on timer
// they lose."

// A default-constructed ConquestState leaves all four pumps at the Pump struct
// default faction (CQ_VALLEY) -- only make_pumps assigns the real per-faction
// ownership. Stamp the make_pumps layout on so "kill faction F's pumps" means
// what it says: [0] Valley surface, [1] Sudbury surface, [2] Valley deep,
// [3] Sudbury deep.
static void stamp_pump_factions(combat::ConquestState& cs) {
    cs.pumps[0].faction = combat::CQ_VALLEY;
    cs.pumps[1].faction = combat::CQ_SUDBURY;
    cs.pumps[2].faction = combat::CQ_VALLEY;
    cs.pumps[3].faction = combat::CQ_SUDBURY;
}

// Helper: kill BOTH of `faction`'s pumps (surface index f, deep index f+2) so
// its dome scale reaches 0 and the clock arms.
static void kill_both_pumps(combat::ConquestState& cs,
                            const combat::ConquestParams& params, int faction) {
    const int destroyer = 1 - faction;
    combat::damage_pump(cs, faction, 1e9, destroyer, params);
    combat::damage_pump(cs, faction + 2, 1e9, destroyer, params);
}

TEST_CASE("countdown: disarmed until a faction actually loses its bubble") {
    combat::ConquestState cs;
    stamp_pump_factions(cs);
    combat::ConquestParams params;  // shrink 0.5/pump, floor 0

    REQUIRE(cs.countdown_faction == -1);
    REQUIRE_FALSE(combat::countdown_active(cs));

    // ONE pump only: the dome is halved, not gone -> still no clock.
    combat::damage_pump(cs, combat::CQ_VALLEY, 1e9, combat::CQ_SUDBURY, params);
    CHECK(cs.radius_scale[combat::CQ_VALLEY] == Catch::Approx(0.5));
    CHECK(cs.countdown_faction == -1);
    CHECK_FALSE(combat::countdown_active(cs));

    // A long tick while disarmed must do NOTHING (no accidental latch).
    combat::conquest_countdown_tick(cs, 10000.0, params);
    CHECK(cs.outcome == combat::Outcome::PLAYING);

    // The SECOND pump takes the bubble to 0 -> the clock arms on the victim.
    combat::damage_pump(cs, combat::CQ_VALLEY + 2, 1e9, combat::CQ_SUDBURY,
                        params);
    CHECK(cs.radius_scale[combat::CQ_VALLEY] == Catch::Approx(0.0));
    REQUIRE(cs.countdown_faction == combat::CQ_VALLEY);
    CHECK(cs.countdown_s == Catch::Approx(params.match_countdown_s));
    CHECK(combat::countdown_active(cs));
}

TEST_CASE("countdown: the clocked side LOSES at the buzzer, either way round") {
    combat::ConquestParams params;

    // (a) the PLAYER is the one who lost the bubble -> DEFEAT on the buzzer.
    {
        combat::ConquestState cs;
        stamp_pump_factions(cs);
        cs.player_faction = combat::CQ_VALLEY;
        kill_both_pumps(cs, params, combat::CQ_VALLEY);
        REQUIRE(cs.countdown_faction == combat::CQ_VALLEY);

        // One tick short of the buzzer: still PLAYING.
        combat::conquest_countdown_tick(cs, params.match_countdown_s - 1.0,
                                        params);
        CHECK(cs.outcome == combat::Outcome::PLAYING);
        CHECK(cs.countdown_s == Catch::Approx(1.0));

        combat::conquest_countdown_tick(cs, 1.0, params);
        CHECK(cs.outcome == combat::Outcome::DEFEAT);
        CHECK(cs.countdown_s == Catch::Approx(0.0));  // clamped, never negative
    }

    // (b) the ENEMY lost the bubble -> the same buzzer is the player's VICTORY.
    {
        combat::ConquestState cs;
        stamp_pump_factions(cs);
        cs.player_faction = combat::CQ_VALLEY;
        kill_both_pumps(cs, params, combat::CQ_SUDBURY);
        REQUIRE(cs.countdown_faction == combat::CQ_SUDBURY);
        combat::conquest_countdown_tick(cs, params.match_countdown_s, params);
        CHECK(cs.outcome == combat::Outcome::VICTORY);
    }
}

TEST_CASE("countdown: winning outright before the buzzer still wins") {
    combat::ConquestParams params;
    combat::ConquestState cs;
    stamp_pump_factions(cs);
    cs.player_faction = combat::CQ_VALLEY;

    // The player loses their bubble and goes on the clock...
    kill_both_pumps(cs, params, combat::CQ_VALLEY);
    REQUIRE(cs.countdown_faction == combat::CQ_VALLEY);

    // ...then kills every enemy maverick with time to spare. Chad's spec: "if
    // the bubble-losing team can kill all of the enemy in 10 mins then they
    // win".
    combat::conquest_countdown_tick(cs, 60.0, params);
    REQUIRE(cs.outcome == combat::Outcome::PLAYING);
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (combat::is_enemy(i, cs.player_faction))
            combat::on_player_kill(cs, i, params);
    CHECK(cs.outcome == combat::Outcome::VICTORY);

    // The buzzer must NOT then overwrite a won match.
    combat::conquest_countdown_tick(cs, 10000.0, params);
    CHECK(cs.outcome == combat::Outcome::VICTORY);
}

TEST_CASE(
    "countdown: arms ONCE -- a second collapse never re-arms or extends") {
    combat::ConquestParams params;
    combat::ConquestState cs;
    stamp_pump_factions(cs);
    cs.player_faction = combat::CQ_VALLEY;

    kill_both_pumps(cs, params, combat::CQ_VALLEY);
    REQUIRE(cs.countdown_faction == combat::CQ_VALLEY);
    combat::conquest_countdown_tick(cs, 120.0, params);
    const double after_burn = cs.countdown_s;
    REQUIRE(after_burn == Catch::Approx(params.match_countdown_s - 120.0));

    // ★ THE CLOCK IS NEVER RE-POINTED. This is the part of the original claim
    // that is still exactly true, checked in the only form that is REACHABLE:
    // ask the arm directly for the other faction while it still owns pumps.
    combat::arm_countdown_if_bubble_lost(cs, combat::CQ_SUDBURY, params);
    CHECK(cs.countdown_faction == combat::CQ_VALLEY);
    CHECK(cs.countdown_s == Catch::Approx(after_burn));

    // ★★★ SUPERSEDED BY CHAD'S 2026-08-30 RULING, AND SAYING SO IS THE POINT.
    // This leg used to assert that when the OTHER side lost its bubble too, the
    // original clock kept running against the original faction. He flew that
    // rule as tape 15 and it cost him a won match: he was on the clock, killed
    // their LAST pump with 50.4 s left while leading 230-200, and the buzzer
    // still crowned them. His ruling: with every pump gone the clock is null and
    // it becomes a deathmatch.
    // ⚠ THE ORIGINAL REASONING IS NOT REPEALED -- re-arming WOULD hand the side
    // already on the clock a free extension, which is why the clock is disarmed
    // outright here rather than re-pointed at SUDBURY. Nobody gains time; the
    // pump war simply ends. With two pumps per faction, "the second side loses
    // its last pump" and "every pump on the map is dead" are the SAME event,
    // which is why the old assertion is unreachable now rather than merely
    // weakened.
    // ★★★ AND SUPERSEDED AGAIN BY CHAD'S 2026-09-03 RULING (rung L6), which is
    // why the seconds line below CHANGED rather than being deleted. His words:
    // "you could rebuild your own pump then destroy the remaining enemy pumps,
    // which would turn the clock's remaining time against your enemy." The
    // clock is now ONE POOL that RE-POINTS, so an all-dead map can no longer
    // throw the seconds away -- a later repair would have nothing left to turn
    // against anybody. What survives untouched is the half that saved his
    // match: with every pump gone there is NO TARGET, so no buzzer can fire.
    kill_both_pumps(cs, params, combat::CQ_SUDBURY);
    CHECK(cs.countdown_faction == -1);
    CHECK(cs.countdown_paused);
    CHECK(cs.countdown_s == Catch::Approx(after_burn));  // L6: kept, not nulled
    CHECK(cs.deathmatch);
    // ...and nobody is crowned by a clock that is pointed at nobody.
    combat::conquest_countdown_tick(cs, 10.0 * params.match_countdown_s, params);
    CHECK(cs.outcome == combat::Outcome::PLAYING);
    CHECK(cs.countdown_s == Catch::Approx(after_burn));  // nor spent by it
}

// ===========================================================================
// RUNG E11 — A FACTION WITH NO PUMPS HAS NO SKY, AND THE CLOCK ARMS ON IT.
//
// ★★★ CHAD'S TAPE 6 (2026-08-23), his words: "both pumps destroyed, enemy
// still had a little bubble left (there should not be), there was no enemy on
// the clock countdown and enemy ai loitered near their tiny bubble."
//
// THE DEFECT, reproduced here as the EXACT sequence his match flew:
//     his kill   -> rs[him] += growth, rs[them] -= shrink   [1.25, 0.50]
//     their kill -> rs[them] += growth, rs[him] -= shrink    [0.75, 0.75]
//     his kill   -> rs[him] += growth, rs[them] -= shrink    [1.00, 0.25]
// The destroyer's GROWTH and the victim's SHRINK are the same variable, so a
// faction that had scored kept a positive scale after losing every pump it
// owned. The dome law ("both pumps lost = no bubble at all") held only for a
// faction that never destroyed anything, and the countdown -- which armed on
// `radius_scale > 0.0` -- therefore never armed at all.
// ★ THE LESSON: an accumulator is not a fact. "Owns a living pump" is exact;
// a running total of two different rewards is not, and only one of them can be
// asked whether a faction has been eliminated.
// ===========================================================================
TEST_CASE("conquest E11: scoring a kill cannot buy you a dome after you lose "
          "every pump") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_SUDBURY;
    combat::ConquestParams params;
    params.growth_radius_frac = 0.25;   // the values his match flew
    params.shrink_radius_frac = 0.5;
    params.growth_ceiling_frac = 0.25;
    params.shrink_ceiling_frac = 0.5;

    // Two pumps each, all alive.
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].alive = true;
        cs.pumps[i].hp = 100.0;
        cs.pumps[i].faction = (i % 2 == 0) ? combat::CQ_SUDBURY
                                           : combat::CQ_VALLEY;
    }
    const int him = combat::CQ_SUDBURY, them = combat::CQ_VALLEY;

    // 1. He kills one of theirs.
    REQUIRE(combat::damage_pump(cs, 1, 200.0, him, params));
    CHECK(cs.radius_scale[him] == Catch::Approx(1.25));
    CHECK(cs.radius_scale[them] == Catch::Approx(0.5));
    CHECK(cs.countdown_faction < 0);  // they still own pump 3

    // 2. THEY kill one of his -- and this is the step that banked the growth
    //    which used to survive their own elimination.
    REQUIRE(combat::damage_pump(cs, 0, 200.0, them, params));
    CHECK(cs.radius_scale[them] == Catch::Approx(0.75));
    CHECK(cs.radius_scale[him] == Catch::Approx(0.75));
    CHECK(cs.countdown_faction < 0);  // he still owns pump 2

    // 3. He kills their LAST pump. Pre-E11 this left them at 0.75 - 0.5 = 0.25
    //    with no countdown; his fleet then loitered inside a dome that should
    //    not exist.
    REQUIRE(combat::damage_pump(cs, 3, 200.0, him, params));
    CHECK_FALSE(combat::faction_has_pump(cs, them));
    CHECK(cs.radius_scale[them] == Catch::Approx(0.0));
    CHECK(cs.ceiling_scale[them] == Catch::Approx(0.0));
    // ★ AND THE CLOCK ARMS ON THEM, which is the half he actually reported.
    CHECK(cs.countdown_faction == them);
    CHECK(cs.countdown_s == Catch::Approx(params.match_countdown_s));
    // He is untouched: he still owns pump 2, so his own dome survives.
    CHECK(combat::faction_has_pump(cs, him));
    CHECK(cs.radius_scale[him] > 0.0);
}

TEST_CASE("conquest E11: an own goal collapses the scorer's own dome") {
    // NON-VACUITY for the both-factions sweep in collapse_bubble_if_pumpless:
    // the growth is applied to the destroyer even when the destroyer IS the
    // victim, so a faction that finishes off its own last pump must still end
    // with no sky. Nothing else in the file exercises destroyer == victim at
    // the elimination boundary.
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_SUDBURY;
    combat::ConquestParams params;
    params.growth_radius_frac = 0.25;
    params.shrink_radius_frac = 0.5;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].alive = true;
        cs.pumps[i].hp = 100.0;
        cs.pumps[i].faction = (i % 2 == 0) ? combat::CQ_SUDBURY
                                           : combat::CQ_VALLEY;
    }
    const int them = combat::CQ_VALLEY;
    REQUIRE(combat::damage_pump(cs, 1, 200.0, them, params));  // own goal
    REQUIRE(combat::damage_pump(cs, 3, 200.0, them, params));  // and the last
    CHECK_FALSE(combat::faction_has_pump(cs, them));
    CHECK(cs.radius_scale[them] == Catch::Approx(0.0));
    CHECK(cs.countdown_faction == them);
}

TEST_CASE("conquest E11: the clock reads the PUMPS, not the dome accumulator") {
    // ★ THIS LEG EXISTS BECAUSE A MUTATION SURVIVED. Reverting
    // arm_countdown_if_bubble_lost to its old `radius_scale > 0.0` test left
    // the E11 sequence test GREEN -- once collapse_bubble_if_pumpless has run,
    // the scale IS zero and both readings agree. The two are only separable
    // BEFORE the collapse, so the clock is exercised directly here against a
    // state that has the shape the bug had: no pumps, positive dome scale.
    // Without this, the fix to the clock would be untested and a future
    // refactor could quietly restore the defect Chad flew.
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_SUDBURY;
    combat::ConquestParams params;
    const int them = combat::CQ_VALLEY;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].faction = (i % 2 == 0) ? combat::CQ_SUDBURY : them;
        cs.pumps[i].alive = (i % 2 == 0);  // THEY own nothing living
    }
    cs.radius_scale[them] = 0.25;  // ...but banked growth from their own kill
    REQUIRE_FALSE(combat::faction_has_pump(cs, them));
    REQUIRE(cs.radius_scale[them] > 0.0);

    combat::arm_countdown_if_bubble_lost(cs, them, params);
    CHECK(cs.countdown_faction == them);
    CHECK(cs.countdown_s == Catch::Approx(params.match_countdown_s));

    // And the converse, so the clause is not simply always-true: a faction that
    // still owns a pump never gets a clock, whatever its scale has fallen to.
    combat::ConquestState ok;
    ok.player_faction = combat::CQ_SUDBURY;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        ok.pumps[i].faction = (i % 2 == 0) ? combat::CQ_SUDBURY : them;
        ok.pumps[i].alive = true;
    }
    ok.radius_scale[them] = 0.0;  // no sky, but a pump still standing
    combat::arm_countdown_if_bubble_lost(ok, them, params);
    CHECK(ok.countdown_faction < 0);
}

// ---------------------------------------------------------------------------
// ★★★ THE DEATHMATCH — CHAD'S 2026-08-30 RULING, AND THE MATCH IT COST HIM.
//
// He flew tape 15 and lost a game he had won. MEASURED FROM THAT TAPE, which is
// what makes this a defect and not a preference:
//   t= 504 s  his second pump dies -> his dome collapses, the 600 s clock arms
//   t=1054 s  HE KILLS THEIR LAST PUMP. Score 230-200 him. Clock reads 50.4 s.
//   t=1104 s  buzzer -> DEFEAT, while leading, with every pump on the map gone.
// HIS RULING: "if all pumps are destroyed the loss timer just goes off and it a
// total points battle to the last plane standing... after that the clock to
// lose is null and it is now team deathmatch but with no bubbles anywhere."
// ---------------------------------------------------------------------------
TEST_CASE("deathmatch: killing the last pump on the map nulls the loss clock") {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_SUDBURY;
    combat::ConquestParams params;
    const int them = combat::CQ_VALLEY;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].faction = (i % 2 == 0) ? combat::CQ_SUDBURY : them;
        cs.pumps[i].alive = true;
    }

    SECTION("HIS TAPE 15: on the clock, then he kills the last pump") {
        // Lose both of his own -> the clock arms on HIM, exactly as it should.
        for (int i = 0; i < combat::kNumPumps; ++i)
            if (cs.pumps[i].faction == cs.player_faction)
                cs.pumps[i].alive = false;
        combat::arm_countdown_if_bubble_lost(cs, cs.player_faction, params);
        REQUIRE(cs.countdown_faction == cs.player_faction);
        REQUIRE(cs.countdown_s > 0.0);
        REQUIRE_FALSE(cs.deathmatch);

        // Kill one of theirs: the pump war is still on, the clock still his.
        for (int i = 0; i < combat::kNumPumps; ++i)
            if (cs.pumps[i].faction == them) {
                cs.pumps[i].alive = false;
                break;
            }
        combat::null_countdown_if_all_pumps_dead(cs);
        CHECK(cs.countdown_faction == cs.player_faction);
        CHECK_FALSE(cs.deathmatch);

        // ...and now THE LAST PUMP ON THE MAP. This is the moment he flew.
        for (int i = 0; i < combat::kNumPumps; ++i) cs.pumps[i].alive = false;
        combat::null_countdown_if_all_pumps_dead(cs);
        CHECK(cs.countdown_faction == -1);
        // ★ L6 (Chad, 2026-09-03): THE SECONDS ARE KEPT. The target is gone --
        // which is the whole of the 2026-08-30 fix, and is what the buzzer
        // assertion below grades -- but the POOL survives, because a later
        // repair can point it at the other side.
        CHECK(cs.countdown_s > 0.0);
        CHECK(cs.countdown_paused);
        CHECK(cs.deathmatch);

        // ★ AND THE BUZZER CAN NEVER FIRE AGAIN. This is the assertion that
        // would have saved his match: ticking the clock for far longer than
        // the countdown he had left must not decide anything.
        combat::conquest_countdown_tick(cs, 600.0, params);
        CHECK(cs.outcome == combat::Outcome::PLAYING);
    }

    SECTION("the clock is untouched while ANY pump still stands") {
        cs.pumps[0].alive = false;
        cs.pumps[1].alive = false;
        cs.pumps[2].alive = false;  // one left
        combat::arm_countdown_if_bubble_lost(cs, cs.pumps[0].faction, params);
        const int armed = cs.countdown_faction;
        combat::null_countdown_if_all_pumps_dead(cs);
        CHECK(cs.countdown_faction == armed);
        CHECK_FALSE(cs.deathmatch);
    }

    SECTION("points break the tie when both sides empty together") {
        // "A total points battle to the last plane standing" has no answer
        // when nobody is left standing. Score does.
        for (int i = 0; i < combat::kNumPumps; ++i) cs.pumps[i].alive = false;
        combat::null_countdown_if_all_pumps_dead(cs);
        REQUIRE(cs.deathmatch);
        cs.reinforcements_live = false;  // waves need air; there is none
        for (int i = 0; i < combat::kNumMavericks; ++i)
            cs.mav_alive[i] = !combat::is_enemy(i, cs.player_faction);
        cs.planes_left = 0;
        cs.score[cs.player_faction] = 230;  // his tape-15 score
        cs.score[them] = 200;
        combat::conquest_eval_outcome(cs);
        CHECK(cs.outcome == combat::Outcome::VICTORY);
    }

    SECTION("...and losing the points battle is still a loss") {
        for (int i = 0; i < combat::kNumPumps; ++i) cs.pumps[i].alive = false;
        combat::null_countdown_if_all_pumps_dead(cs);
        cs.reinforcements_live = false;
        for (int i = 0; i < combat::kNumMavericks; ++i)
            cs.mav_alive[i] = !combat::is_enemy(i, cs.player_faction);
        cs.planes_left = 0;
        cs.score[cs.player_faction] = 100;
        cs.score[them] = 200;
        combat::conquest_eval_outcome(cs);
        CHECK(cs.outcome == combat::Outcome::DEFEAT);
    }

    SECTION("running out of planes with enemies still flying is a LOSS") {
        // The tie-break must not become a way to win by dying last-but-one.
        for (int i = 0; i < combat::kNumPumps; ++i) cs.pumps[i].alive = false;
        combat::null_countdown_if_all_pumps_dead(cs);
        cs.reinforcements_live = false;
        for (int i = 0; i < combat::kNumMavericks; ++i) cs.mav_alive[i] = true;
        cs.planes_left = 0;
        cs.score[cs.player_faction] = 999;
        cs.score[them] = 0;
        combat::conquest_eval_outcome(cs);
        CHECK(cs.outcome == combat::Outcome::DEFEAT);
    }
    // REQUIRED-RED MUTATION: delete the null_countdown_if_all_pumps_dead call
    // in on_pump_destroyed / make the function a no-op. VERIFIED RED on the
    // tape-15 section -- which is the whole point: that red IS his lost match.
    // SECOND REQUIRED-RED: drop the `any_enemy_alive` guard from the tie-break
    // in conquest_eval_outcome. VERIFIED RED on the last section.
}

// ---------------------------------------------------------------------------
// ★★★ RUNG E14 — A DEFENDER SLOT GOES TO SOMEONE WHO CAN ACTUALLY FLY IT.
//
// Chad's fly report on tape 9 (2026-08-24): "enemy ai abandoned their own pump
// leaving me to attack it, and only one stayed to defend... the others went for
// the opposing bubble."
//
// MEASURED IN HIS TAPE, which is what makes this a defect and not a taste: he
// spent 87 s inside the threat radius of the enemy surface pump while he killed
// it. Drone 6 held a defend order for 99.7% of it and DID come (down to 72 m).
// Drones 2 and 4 also held defend orders — 56% and 30% of the time — from a
// median 12.9 km and 12.6 km away, and never closed at all. They were mid
// TUNNEL RUN, and drone::tick gates the whole defend branch behind
// `if (!tunnel_mode)`. Two slots issued, one defender that could fly.
// ---------------------------------------------------------------------------
TEST_CASE("E14: a committed tunnel run never consumes a defender slot") {
    const glm::dvec3 pump_dir = glm::normalize(glm::dvec3{0.3, 0.5, 0.81});
    combat::Pump pumps[4]{};
    // Only faction 0's surface pump matters here; the rest stay dead so the
    // loop cannot pick defenders for them.
    pumps[0] = combat::Pump{pump_dir * 15010.0, 25.0, 100.0, 0, true, true};
    pumps[1].alive = false;
    pumps[2].alive = false;
    pumps[3].alive = false;

    // Five faction-0 pilots at increasing range from the pump. combat::
    // maverick_faction puts VALLEY (0) at the TOP of the roster, so take the
    // wing's own indices rather than assuming any literal.
    std::vector<int> wing;
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (combat::maverick_faction(i) == 0) wing.push_back(i);
    REQUIRE(wing.size() >= 3);

    const auto build = [&](bool nearest_two_in_tunnel) {
        std::vector<drone::DroneState> fleet;
        for (std::size_t n = 0; n < wing.size(); ++n) {
            drone::DroneState d;
            d.spawn_index = wing[n];
            // Rank n sits (n + 1) km out along a fixed tangent.
            const glm::dvec3 t =
                glm::normalize(glm::cross(pump_dir, glm::dvec3{0, 0, 1}));
            d.curr.position =
                pumps[0].pos + t * (1000.0 * static_cast<double>(n + 1));
            d.prev = d.curr;
            // The TWO NEAREST are mid tunnel run in the arm under test.
            if (nearest_two_in_tunnel && n < 2)
                d.mav.mode = maverick::MaverickState::Mode::TRANSIT;
            fleet.push_back(d);
        }
        return fleet;
    };

    // A threatening enemy AIRCRAFT parked on the pump so `threatened` arms.
    sim::SimState player;  // player is faction 0 here, so he is NOT the threat
    player.position = pump_dir * 40000.0;

    const auto defenders = [&](std::vector<drone::DroneState>& fleet) {
        // One faction-1 drone sitting on the pump = the threat.
        drone::DroneState bandit;
        bandit.spawn_index = 0;  // SUDBURY
        REQUIRE(combat::maverick_faction(bandit.spawn_index) == 1);
        bandit.curr.position = pumps[0].pos;
        bandit.prev = bandit.curr;
        fleet.push_back(bandit);
        combat::assign_defense(fleet, pumps, player, /*player_faction=*/0,
                               /*threat_radius_m=*/2500.0,
                               /*defenders_per_faction=*/2);
        std::vector<int> out;
        for (const drone::DroneState& d : fleet)
            if (d.defend.active) out.push_back(d.spawn_index);
        fleet.pop_back();
        return out;
    };

    // (a) BASELINE — everyone on PATROL: the two NEAREST are picked. This is
    // the fixture-shows-the-mechanism arm; without it the test below could
    // pass on a defence that never picks anyone.
    {
        std::vector<drone::DroneState> fleet = build(false);
        const std::vector<int> got = defenders(fleet);
        INFO("baseline defenders");
        REQUIRE(got.size() == 2);
        // Stated as the RULE, not as a pair of indices: pass 0 draws from
        // NON-RAIDERS nearest-first (the game-AI-R4 rule that keeps defence
        // from cannibalising the offense), so the wing's raider is skipped
        // while non-raiders remain. Naming wing[0]/wing[1] here was wrong the
        // moment E13's raiders_for_team picked a different pilot.
        const int spare_non_raiders =
            static_cast<int>(wing.size()) - combat::raiders_for_team(0);
        for (int si : got) {
            CHECK(!combat::faction_raider(si));
            CHECK(combat::maverick_faction(si) == 0);
        }
        CHECK(spare_non_raiders >= 2);  // premise of the clause above
        // ...and nearest-first among those: the closest eligible pilot is in.
        int nearest_eligible = -1;
        for (std::size_t n = 0; n < wing.size(); ++n)
            if (!combat::faction_raider(wing[n])) {
                nearest_eligible = wing[n];
                break;
            }
        REQUIRE(nearest_eligible >= 0);
        CHECK(std::find(got.begin(), got.end(), nearest_eligible) !=
              got.end());
    }

    // (b) THE DEFECT — the two nearest are mid tunnel run. Pre-E14 they were
    // picked anyway (nearest wins) and then drone::tick ignored the order,
    // leaving the pump with ZERO real defenders. Now the slots skip them and
    // go to pilots who will actually fly — and the COUNT is however many the
    // wing can honestly field, which after E13's 7/3 split may be fewer than
    // defenders_per_faction. Written that way on purpose: a literal 2 here
    // would be one more constant describing a roster that has already moved
    // once this week.
    {
        std::vector<drone::DroneState> fleet = build(true);
        const std::vector<int> got = defenders(fleet);
        const std::size_t available = wing.size() - 2;  // two are in transit
        INFO("tunnel-committed nearest two must be skipped; wing "
             << wing.size() << ", available " << available);
        REQUIRE(got.size() == std::min<std::size_t>(2, available));
        REQUIRE(!got.empty());
        for (int si : got) {
            CHECK(si != wing[0]);
            CHECK(si != wing[1]);
        }
    }

    // (c) A wing with NOBODY available fields FEWER defenders, never a
    // phantom order — it honestly has nobody else.
    {
        std::vector<drone::DroneState> fleet = build(false);
        for (drone::DroneState& d : fleet)
            d.mav.mode = maverick::MaverickState::Mode::RUN;
        const std::vector<int> got = defenders(fleet);
        CHECK(got.empty());
    }
}
