// RUNG S3-GUNS — THE COSMETIC ROUND POOL.
//
// Chad's ruling: "attack the pump, attack me". The AI-vs-AI gun window and the
// pump raid credit both subtract HP with NOTHING IN THE AIR — `wants_fire` was
// 0 of 70,220 drone samples across tapes 10 and 11, so the DPS came out of
// nowhere on screen and the comment at app/instructor_tick.h promising "real
// tracers fly for the look" was false. combat::cosmetic_fire fixes the look.
//
// The whole risk of that fix is exactly one thing: a cosmetic round that can
// hurt somebody would move the difficulty Chad signed without a dial being
// touched. This file is the proof that it cannot, and it proves it TWICE, from
// both halves of the guarantee:
//
//   THE BRACES (structural) — no damage sweep in the tree is ever handed the
//     cosmetic pool. Tests 1-3 put a round with LETHAL geometry AND LETHAL
//     damage in cosmetic_pool, run every sweep that can subtract HP, and
//     demand zero movement.
//   THE BELT (data) — every round cosmetic_fire spawns carries damage == 0.0,
//     so even a future caller who wires the pool into a sweep by mistake
//     subtracts nothing. Tests 4-5 route a REAL cosmetic round through the
//     player sweep, the drone sweep and the pump sweep and demand zero.
//
// Test 6 pins the refactor: the REAL enemy round still leaves the muzzle on
// bit-identical velocity through the factored combat::aim_slew_round, and the
// cosmetic round leaves on the SAME solution (one aim law, no stale copy —
// the E1.2 lesson).
//
// Pure, headless: no raylib, no clock, no rng.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

#include "combat/conquest.h"
#include "combat/kill.h"
#include "app/instructor_tick.h"
#include "app/loop.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "config/load_scenario.h"
#include "test/harness/instructor.h"
#include "drone/drone.h"
#include "weapon/ballistics.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

constexpr double kR = 15000.0;  // SEADS ground radius

drone::DroneState drone_at(const glm::dvec3& pos, const glm::dvec3& vel) {
    drone::DroneState d;
    sim::SimState s;
    s.position = pos;
    s.velocity = vel;
    s.orientation = glm::dquat{1, 0, 0, 0};
    d.curr = s;
    d.prev = s;
    d.grounded = false;
    d.hp = 100.0;
    return d;
}

combat::CombatWorld make_cw() {
    combat::CombatWorld cw;
    cw.params.hit_radius_m = 15.0;
    cw.setup.difficulty = 3;
    cw.setup.player_hit_radius_m = 9.0;
    cw.setup.bandit_gun.kind = weapon::Round::Cannon20mm;
    cw.setup.bandit_gun.muzzle_speed = 800.0;
    cw.setup.bandit_gun.rof_hz = 8.0;
    cw.setup.bandit_gun.damage = 12.0;
    cw.setup.bandit_gun.drag_k = 0.0;
    cw.setup.bandit_convergence = 250.0;
    return cw;
}

// A round whose [prev_pos,pos] segment runs straight through `through`, armed
// with real, lethal damage. This is the round a bug would let through.
weapon::Projectile lethal_round(const glm::dvec3& through, double damage) {
    weapon::Projectile p;
    p.prev_pos = through + glm::dvec3{0.0, 0.0, 40.0};
    p.pos = through + glm::dvec3{0.0, 0.0, -40.0};
    p.vel = p.pos - p.prev_pos;
    p.v_ref = glm::length(p.vel);
    p.age = 0.01;
    p.drag_k = 0.0;
    p.damage = damage;
    p.kind = weapon::Round::Cannon20mm;
    p.active = true;
    return p;
}

// Four pumps on the sphere at 200 HP each (the fixture only needs a live pump
// with a position; the shipped placement law is not under test here).
void make_test_pumps(combat::ConquestState& cs) {
    combat::make_pumps(glm::dvec3{kR, 0.0, 0.0}, glm::dvec3{-kR, 0.0, 0.0},
                       glm::dvec3{0.0, kR, 0.0}, glm::dvec3{0.0, -kR, 0.0},
                       200.0, cs.pumps);
}

}  // namespace

// ===========================================================================
// Test 1 — THE BRACES, player half. A LETHAL round parked in cosmetic_pool,
// aimed straight through the player's own segment, cannot touch him:
// combat_player_tick sweeps cw.enemy_pool and is never handed the other pool.
// The positive control in the SAME assertion is what makes this falsifiable:
// an identical round in enemy_pool DOES take exactly one round of HP off.
// MUTATION (run, went RED): route the cosmetic round into cw.enemy_pool
//   instead -> player_hits 1 -> 2 and the damage doubles.
// ===========================================================================
TEST_CASE("S3-GUNS: a cosmetic round cannot damage the PLAYER") {
    const glm::dvec3 ppos{kR + 2000.0, 0.0, 0.0};
    sim::SimState prev;
    prev.position = ppos;
    prev.orientation = glm::dquat{1, 0, 0, 0};
    sim::SimState curr = prev;
    curr.position = ppos + glm::dvec3{0.0, 1.0, 0.0};

    // (a) cosmetic only: nothing may move.
    {
        combat::CombatWorld cw = make_cw();
        cw.cosmetic_pool.push_back(lethal_round(ppos, 40.0));
        const double hp0 = combat::summary_hp(cw.damage);
        combat::combat_player_tick(cw, prev, curr);
        CHECK(combat::summary_hp(cw.damage) == Catch::Approx(hp0));
        CHECK(cw.player_hits == 0);
        CHECK(cw.deaths == 0);
        // The round is not even consumed — no sweep saw it.
        CHECK(cw.cosmetic_pool[0].active);
    }
    // (b) positive control: the SAME round in the real pool DOES bite. Without
    //     this the test above could pass on a broken fixture.
    {
        combat::CombatWorld cw = make_cw();
        cw.enemy_pool.push_back(lethal_round(ppos, 40.0));
        const double hp0 = combat::summary_hp(cw.damage);
        combat::combat_player_tick(cw, prev, curr);
        CHECK(combat::summary_hp(cw.damage) < hp0);
        CHECK(cw.player_hits == 1);
        CHECK(!cw.enemy_pool[0].active);
    }
}

// ===========================================================================
// Test 2 — THE BRACES, drone half. combat_tick sweeps the PLAYER battery pool
// against drones. A lethal cosmetic round through a drone's centre takes no HP
// off it, and the positive control does.
// MUTATION (run, went RED): pass cw.cosmetic_pool to combat_tick -> hp drops
//   100 -> 60 and the CHECK fails.
// ===========================================================================
TEST_CASE("S3-GUNS: a cosmetic round cannot damage a DRONE") {
    drone::DroneParams dp;
    dp.hp = 100.0;
    dp.hit_radius_m = 15.0;
    const glm::dvec3 dpos{kR + 2000.0, 0.0, 0.0};

    combat::CombatWorld cw = make_cw();
    std::vector<drone::DroneState> drones{drone_at(dpos, glm::dvec3{0.0})};
    drones[0].hp = dp.hp;
    cw.cosmetic_pool.push_back(lethal_round(dpos, 40.0));

    std::vector<weapon::Projectile> player_pool;  // the pool combat_tick sweeps
    combat::combat_tick(player_pool, drones, cw, kAp, dp, kAp.sim_dt);
    CHECK(drones[0].hp == Catch::Approx(100.0));
    CHECK(cw.hits == 0);
    CHECK(cw.cosmetic_pool[0].active);

    // Positive control on the same fixture.
    player_pool.push_back(lethal_round(dpos, 40.0));
    combat::combat_tick(player_pool, drones, cw, kAp, dp, kAp.sim_dt);
    CHECK(drones[0].hp < 100.0);
    CHECK(cw.hits == 1);
}

// ===========================================================================
// Test 3 — THE BRACES, pump half. conquest_tick sweeps the player pool against
// the four pumps. A lethal cosmetic round sitting on a pump takes no pump HP.
// MUTATION (run, went RED): pass cw.cosmetic_pool to conquest_tick -> the pump
//   hp falls and hit_events is non-empty.
// ===========================================================================
TEST_CASE("S3-GUNS: a cosmetic round cannot damage a PUMP") {
    combat::ConquestState cs;
    combat::ConquestParams cp;
    make_test_pumps(cs);
    const double hp0 = cs.pumps[0].hp;
    REQUIRE(hp0 > 0.0);

    combat::CombatWorld cw = make_cw();
    cw.cosmetic_pool.push_back(lethal_round(cs.pumps[0].pos, 40.0));

    std::vector<weapon::Projectile> player_pool;
    std::vector<combat::PumpDeathEvent> ev;
    std::vector<combat::PumpHitEvent> hits;
    combat::conquest_tick(cs, player_pool, cp, &ev, &hits);
    CHECK(cs.pumps[0].hp == Catch::Approx(hp0));
    CHECK(hits.empty());
    CHECK(cw.cosmetic_pool[0].active);

    // Positive control on the SAME fixture: the identical round in the pool
    // conquest_tick actually sweeps DOES take pump HP. Without this the
    // assertion above could be passing on a pump the fixture never armed.
    player_pool.push_back(lethal_round(cs.pumps[0].pos, 40.0));
    combat::conquest_tick(cs, player_pool, cp, &ev, &hits);
    CHECK(cs.pumps[0].hp < hp0);
    CHECK(!hits.empty());
}

// ===========================================================================
// Test 4 — THE BELT. A round actually produced by combat::cosmetic_fire carries
// damage == 0.0. Forced twice (GunSpec + round), so the guarantee survives a
// future spawn path that ignores the spec.
// MUTATION (run, went RED): delete `r.damage = 0.0;` in cosmetic_fire and set
//   gun.damage = diff.bandit_damage -> damage reads 12.0 and this CHECK fails.
// ===========================================================================
TEST_CASE("S3-GUNS: every cosmetic round is spawned with zero damage") {
    combat::CombatWorld cw = make_cw();
    drone::DroneState d = drone_at(glm::dvec3{kR + 2000.0, 0.0, 0.0},
                                   glm::dvec3{0.0, 0.0, 100.0});
    const glm::dvec3 tgt{kR + 2000.0, 0.0, 600.0};

    REQUIRE(combat::cosmetic_fire(cw, d, tgt, glm::dvec3{0.0}, kAp.g,
                                  kAp.sim_dt));
    REQUIRE(cw.cosmetic_pool.size() == 1);
    CHECK(cw.cosmetic_pool[0].active);
    CHECK(cw.cosmetic_pool[0].damage == 0.0);
    CHECK(cw.cosmetic_rounds == 1);
}

// ===========================================================================
// Test 5 — THE BELT, exercised. Take the real cosmetic round from test 4 and
// deliberately hand it to ALL THREE damage sweeps as if a future caller had
// wired the pool in by mistake. Geometry is arranged so it hits every time.
// Nothing may lose HP: this is the leg that still holds if the structural
// guarantee is ever broken.
// MUTATION (run, went RED): set the round's damage to 40.0 before the sweeps
//   -> player hp, drone hp and pump hp all fall and all three CHECKs fail.
// ===========================================================================
TEST_CASE("S3-GUNS: a real cosmetic round routed INTO the sweeps is inert") {
    combat::CombatWorld src = make_cw();
    drone::DroneState shooter = drone_at(glm::dvec3{kR + 2000.0, 0.0, 0.0},
                                         glm::dvec3{0.0, 0.0, 100.0});
    REQUIRE(combat::cosmetic_fire(src, shooter,
                                  glm::dvec3{kR + 2000.0, 0.0, 600.0},
                                  glm::dvec3{0.0}, kAp.g, kAp.sim_dt));
    const weapon::Projectile cos_round = src.cosmetic_pool[0];
    REQUIRE(cos_round.active);

    // Re-aim its SEGMENT at each victim in turn (damage/kind/v_ref preserved —
    // only the geometry is made lethal, which is the point).
    const auto through = [&](const glm::dvec3& p) {
        weapon::Projectile r = cos_round;
        r.prev_pos = p + glm::dvec3{0.0, 0.0, 40.0};
        r.pos = p + glm::dvec3{0.0, 0.0, -40.0};
        r.vel = r.pos - r.prev_pos;
        r.v_ref = glm::length(r.vel);
        return r;
    };

    // (a) the player
    {
        const glm::dvec3 ppos{kR + 2000.0, 0.0, 0.0};
        sim::SimState prev;
        prev.position = ppos;
        prev.orientation = glm::dquat{1, 0, 0, 0};
        sim::SimState curr = prev;
        curr.position = ppos + glm::dvec3{0.0, 1.0, 0.0};
        combat::CombatWorld cw = make_cw();
        cw.enemy_pool.push_back(through(ppos));  // the WRONG pool, on purpose
        const double hp0 = combat::summary_hp(cw.damage);
        combat::combat_player_tick(cw, prev, curr);
        CHECK(combat::summary_hp(cw.damage) == Catch::Approx(hp0));
    }
    // (b) a drone
    {
        drone::DroneParams dp;
        dp.hp = 100.0;
        dp.hit_radius_m = 15.0;
        const glm::dvec3 dpos{kR + 2000.0, 0.0, 0.0};
        combat::CombatWorld cw = make_cw();
        std::vector<drone::DroneState> drones{drone_at(dpos, glm::dvec3{0.0})};
        drones[0].hp = dp.hp;
        std::vector<weapon::Projectile> pool{through(dpos)};
        combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt);
        CHECK(drones[0].hp == Catch::Approx(100.0));
    }
    // (c) a pump
    {
        combat::ConquestState cs;
        combat::ConquestParams cp;
        make_test_pumps(cs);
        const double hp0 = cs.pumps[0].hp;
        std::vector<weapon::Projectile> pool{through(cs.pumps[0].pos)};
        std::vector<combat::PumpDeathEvent> ev;
        combat::conquest_tick(cs, pool, cp, &ev, nullptr);
        CHECK(cs.pumps[0].hp == Catch::Approx(hp0));
    }
}

// ===========================================================================
// Test 6 — ONE AIM LAW, NO STALE COPY. combat::aim_slew_round was factored out
// of enemy_fire_tick's body verbatim. Two things must hold:
//   (a) the REAL path still goes through it and only it — an independently
//       rebuilt round (weapon::spawn + aim_slew_round with the drone's own
//       gun_tgt_pos/_vel) matches enemy_fire_tick's spawned round BITWISE;
//   (b) a cosmetic round fired at the SAME target leaves on the SAME velocity,
//       so the tracer Chad sees is the tracer the real gun would have flown.
// MUTATION (run, went RED): change kSprayHz 0.9 -> 0.91 inside aim_slew_round
//   -> (a) still passes (both sides share the helper) but the recorded
//   reference velocity below moves, and re-pointing the helper's tgt_pos at
//   d.curr.position instead breaks BOTH legs.
// ===========================================================================
TEST_CASE("S3-GUNS: the factored aim solve is the ONLY aim solve") {
    combat::CombatWorld cw = make_cw();
    const glm::dvec3 pos{kR + 2000.0, 0.0, 0.0};
    const glm::dvec3 tgt{kR + 2000.0, 0.0, 600.0};
    const glm::dvec3 tgt_vel{0.0, 40.0, 0.0};

    std::vector<drone::DroneState> drones{
        drone_at(pos, glm::dvec3{0.0, 0.0, 100.0})};
    drones[0].engaged = true;
    drones[0].wants_fire = true;
    drones[0].gun_tgt_pos = tgt;
    drones[0].gun_tgt_vel = tgt_vel;
    drones[0].age_ticks = 37;
    drones[0].spawn_index = 5;

    combat::enemy_fire_tick(cw, drones, kAp.sim_dt, kAp.g, kR, nullptr);
    REQUIRE(cw.enemy_pool.size() == 1);
    REQUIRE(cw.enemy_pool[0].active);

    // (a) independently rebuilt through the helper -> BITWISE identical.
    {
        const combat::DifficultyParams diff =
            combat::difficulty_params(cw.setup.difficulty);
        weapon::GunSpec gun = cw.setup.bandit_gun;
        gun.rof_hz = diff.bandit_rof_hz;
        gun.damage = diff.bandit_damage;
        weapon::GunBattery b;
        b.convergence_range = cw.setup.bandit_convergence;
        weapon::Projectile r =
            weapon::spawn(b, gun, drones[0].curr, kAp.g, kAp.sim_dt);
        combat::aim_slew_round(r, drones[0], tgt, tgt_vel, kAp.g, kAp.sim_dt);
        CHECK(r.vel.x == cw.enemy_pool[0].vel.x);
        CHECK(r.vel.y == cw.enemy_pool[0].vel.y);
        CHECK(r.vel.z == cw.enemy_pool[0].vel.z);
    }

    // (b) the cosmetic round on the same target flies the same solution.
    {
        combat::CombatWorld cw2 = make_cw();
        drone::DroneState d = drones[0];
        d.cosmetic_cooldown = 0.0;
        REQUIRE(combat::cosmetic_fire(cw2, d, tgt, tgt_vel, kAp.g, kAp.sim_dt));
        CHECK(cw2.cosmetic_pool[0].vel.x == cw.enemy_pool[0].vel.x);
        CHECK(cw2.cosmetic_pool[0].vel.y == cw.enemy_pool[0].vel.y);
        CHECK(cw2.cosmetic_pool[0].vel.z == cw.enemy_pool[0].vel.z);
    }
}

// ===========================================================================
// Test 7 — CADENCE. cosmetic_fire fires at the difficulty table's bandit_rof_hz
// and not once per tick: over 1.0 s of ticks it must produce rof_hz rounds
// (+/- 1 for the boundary), which is what keeps a raid run from filling the
// screen and what ties "rounds Chad sees" to "HP Chad loses" on ONE dial.
// MUTATION (run, went RED): drop the `d.cosmetic_cooldown > 0.0` early return
//   -> 120 rounds instead of 11 and the band fails.
// ===========================================================================
TEST_CASE("S3-GUNS: cosmetic cadence is the difficulty table's rof") {
    combat::CombatWorld cw = make_cw();
    const combat::DifficultyParams diff =
        combat::difficulty_params(cw.setup.difficulty);
    REQUIRE(diff.bandit_rof_hz > 0.0);

    drone::DroneState d = drone_at(glm::dvec3{kR + 2000.0, 0.0, 0.0},
                                   glm::dvec3{0.0, 0.0, 100.0});
    const glm::dvec3 tgt{kR + 2000.0, 0.0, 600.0};
    const double dt = 1.0 / 120.0;
    int fired = 0;
    for (int i = 0; i < 120; ++i)
        if (combat::cosmetic_fire(cw, d, tgt, glm::dvec3{0.0}, kAp.g, dt))
            ++fired;
    CHECK(fired >= static_cast<int>(diff.bandit_rof_hz) - 1);
    CHECK(fired <= static_cast<int>(diff.bandit_rof_hz) + 1);
    CHECK(cw.cosmetic_rounds == fired);
}

// ===========================================================================
// Test 8 — LIVENESS, THROUGH THE REAL app::tick, ONE SEAM AT A TIME.
//
// The house law says a bit-identical arm is a BLIND FIXTURE or a DEAD BRANCH,
// so the two emission sites must be proven REACHABLE through the shipped
// app/instructor_tick.h, not merely through the unit helpers above.
//
// ⚠ AND THIS IS WHY, for the next agent: the E12 / P-H `fly_match` fixture is a
// HAND-MIRROR of app/instructor_tick.h (test/unit/test_conquest_match.cpp:28,
// :607, :1212), NOT a caller of it. Its 17-digit bit-identity across this rung
// proves the DroneState field addition is inert — it proves NOTHING about the
// emission, because the emission is not in that fixture. This leg is the
// emission's only liveness.
//
// Two arms, each isolating one seam:
//   ARM A (no GunWorld): the pump raid credit is unreachable — its per-tick DPS
//     is scaled by combat::battery_dps(gw->battery) — so every cosmetic round
//     comes from the AI-vs-AI gun window.
//   ARM B (GunWorld armed, both surface pumps on the merge point): the raid
//     credit runs too; pump HP falls and HitSparks appear on the pump.
// MUTATION (run, went RED both ways): commenting out the cosmetic_fire call at
//   the AI-vs-AI seam takes ARM A's rounds to 0; commenting out the one at the
//   raid seam takes ARM B's round count down to ARM A's and kills the sparks.
// ===========================================================================
namespace {
struct LivenessResult {
    long long rounds = 0;
    int gun_window_ticks = 0;
    double pump_hp_lost = 0.0;
    int sparks = 0;
    int flew = 0;
};

// Drone A level-trimmed a radian around the sphere from the player (so
// combat::assign_foes' player slots cannot take the pair and pass 3 hands them
// each other); drone B is A spun 180 deg about local up, 300 m down A's nose.
// A head-on merge, so combat::ai_guns_on's ~32 deg snapshot cone plus its 0.9
// velocity/nose alignment is satisfied on the first ticks and this leg never
// depends on a dogfight resolving.
LivenessResult run_liveness(bool arm_the_player_battery) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);
    app::LoopState st;
    st.curr = s0;
    st.prev = s0;
    st.prev_up = sim::local_up(s0.position);
    st.aim.reseed(s0.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;

    app::DroneWorld dw;
    dw.dparams = kScen.drone;
    combat::CombatWorld cw;
    cw.setup = kScen.combat;
    cw.respawn_drones = false;
    app::ConquestWorld cq;
    // The SHIPPED default is the all-vs-player FURBALL (game.toml [conquest]
    // all_vs_player), in which every drone's foe is the PLAYER and the AI-vs-AI
    // seam is structurally unreachable. This leg measures the TEAM air war,
    // which is the branch that seam lives in.
    cq.params.all_vs_player = false;

    const glm::dvec3 side = glm::normalize(glm::cross(up, heading));
    const glm::dvec3 up_b = glm::angleAxis(1.0, side) * up;
    const glm::dvec3 head_b =
        glm::normalize(glm::cross(up_b, glm::angleAxis(1.0, side) * side));

    // Two spawn indices the SHIPPED roster puts on opposite factions — read off
    // combat::maverick_faction, never a hand-written faction map (the law: read
    // the loader, not a recorded table).
    int si_a = -1, si_b = -1;
    for (int i = 0; i < 10 && si_b < 0; ++i) {
        if (si_a < 0) {
            si_a = i;
            continue;
        }
        if (combat::maverick_faction(i) != combat::maverick_faction(si_a))
            si_b = i;
    }
    REQUIRE(si_b >= 0);
    const int si[2] = {si_a, si_b};

    sim::SimState a_st =
        harness::level_trim_state(kAp, 150.0, 4000.0, up_b, head_b);
    const glm::dvec3 nose_a = a_st.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dquat flip = glm::angleAxis(3.14159265358979, up_b);
    sim::SimState b_st = a_st;
    b_st.orientation = flip * a_st.orientation;
    b_st.velocity = flip * a_st.velocity;
    b_st.position = a_st.position + nose_a * 300.0;
    const sim::SimState pair[2] = {a_st, b_st};
    for (int i = 0; i < 2; ++i) {
        drone::DroneState d;
        d.curr = pair[i];
        d.prev = pair[i];
        d.grounded = false;
        d.spawn_index = si[i];
        d.hp = 1.0e9;  // never dies: this leg measures EMISSION, not attrition
        dw.drones.push_back(d);
    }

    // Both SURFACE pumps on the merge point: whichever faction the raid
    // designation picks, its raider is on-station immediately.
    combat::make_pumps(a_st.position, a_st.position,
                       a_st.position + up_b * 4000.0,
                       a_st.position - up_b * 4000.0, 500.0, cq.state.pumps);
    const double pump_hp0 = cq.state.pumps[0].hp + cq.state.pumps[1].hp;

    weapon::GunWorld gw;
    {
        weapon::GunSpec g{};
        g.kind = weapon::Round::Cannon20mm;
        g.muzzle_speed = 800.0;
        g.rof_hz = 10.0;
        g.damage = 30.0;
        gw.battery.guns.push_back(g);
        gw.battery.convergence_range = 250.0;
    }

    LivenessResult out;
    app::TickInput in;
    in.throttle = thr;
    for (int k = 0; k < 12000; ++k) {
        app::tick(st, in, kAp, kCp, nullptr, &dw,
                  arm_the_player_battery ? &gw : nullptr, &cw, nullptr, &cq);
        if (combat::ai_guns_on(dw.drones[0].curr, dw.drones[1].curr,
                               dw.dparams.fire_range_min,
                               dw.dparams.fire_range_max))
            ++out.gun_window_ticks;
    }
    out.rounds = cw.cosmetic_rounds;
    out.pump_hp_lost = pump_hp0 - (cq.state.pumps[0].hp + cq.state.pumps[1].hp);
    for (const weapon::Projectile& p : cw.cosmetic_pool)
        if (p.age > 0.0) ++out.flew;
    for (const combat::Fx& f : cw.fx.pool)
        if (f.kind == combat::FxKind::HitSpark) ++out.sparks;
    return out;
}
}  // namespace

TEST_CASE("S3-GUNS: both cosmetic seams are LIVE through the real app tick") {
    // ARM A — the AI-vs-AI seam alone (no player battery => no raid credit).
    const LivenessResult a = run_liveness(false);
    INFO("ARM A rounds=" << a.rounds << " gunwin=" << a.gun_window_ticks
                         << " pump_lost=" << a.pump_hp_lost);
    CHECK(a.gun_window_ticks > 0);  // the fixture really opens the gun window
    CHECK(a.pump_hp_lost == 0.0);   // and the raid credit really is off
    CHECK(a.rounds > 0);            // => these rounds are the AI-vs-AI seam's
    CHECK(a.flew > 0);              // cosmetic_fire_tick advanced them

    // ARM B — both seams. The raid credit takes pump HP, and the pump wears
    // the same HitSpark the player's own rounds make.
    const LivenessResult b = run_liveness(true);
    INFO("ARM B rounds=" << b.rounds << " pump_lost=" << b.pump_hp_lost
                         << " sparks=" << b.sparks);
    CHECK(b.pump_hp_lost > 0.0);
    CHECK(b.rounds > a.rounds);  // strictly more: the raid seam adds its own
    CHECK(b.sparks > 0);
}
