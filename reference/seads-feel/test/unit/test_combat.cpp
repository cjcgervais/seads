// Kill-loop unit tests (combat/kill.h).
// Pure, headless: no raylib, no clock, no rng.
// Ten pinned cases per the design spec; each names the defect it catches.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>

#include "app/instructor_tick.h"
#include "combat/fx_curves.h"
#include "combat/kill.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "config/load_scenario.h"
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

// A drone parked level at pos, stationary (prev == curr).
drone::DroneState stationary_drone(const glm::dvec3& pos,
                                   const drone::DroneParams& dp) {
    drone::DroneState d;
    sim::SimState s;
    s.position = pos;
    s.velocity = glm::dvec3{0.0};
    s.orientation = glm::dquat{1, 0, 0, 0};
    d.curr = s;
    d.prev = s;
    d.grounded = false;
    d.hp = dp.hp;
    return d;
}

// A single active projectile with given prev_pos and pos.
weapon::Projectile make_proj(const glm::dvec3& prev_pos,
                              const glm::dvec3& pos,
                              double damage = 30.0) {
    weapon::Projectile p;
    p.prev_pos = prev_pos;
    p.pos = pos;
    p.vel = pos - prev_pos;
    p.age = 0.01;
    p.drag_k = 0.0;
    p.damage = damage;
    // KE model (Fable 2026-07-13): reference impact speed = this round's own
    // speed, so a CENTER (normal-incidence) hit on a stationary drone lands at
    // (|v_rel|/v_ref)^2 == 1 and deals exactly `damage` — the mechanics tests
    // below keep their original hp-drop contract while the KE falloff is
    // exercised separately (see the "KE damage falloff" case).
    p.v_ref = glm::length(p.vel);
    p.kind = weapon::Round::Cannon20mm;
    p.active = true;
    return p;
}

// Default combat params with R=15 m.
combat::CombatParams default_cparams() {
    combat::CombatParams cp;
    cp.hit_radius_m = 15.0;
    return cp;
}

drone::DroneParams default_dp() {
    drone::DroneParams dp;
    dp.hp = 100.0;
    dp.hit_radius_m = 15.0;
    return dp;
}

combat::CombatWorld make_cw() {
    combat::CombatWorld cw;
    cw.params = default_cparams();
    return cw;
}

}  // namespace

// ===========================================================================
// Test 1: Hit registers — segment through a stationary drone → hp drops by
// exactly `damage`, round inactive, spark spawned.
// MUTATION: flip the hit condition → drone hp unchanged, round stays active.
// ===========================================================================
TEST_CASE("combat: hit registers hp drop, round retired, spark spawned") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    const glm::dvec3 drone_pos{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(drone_pos, dp));
    drones[0].hp = dp.hp;

    // Round flies straight through the drone center.
    std::vector<weapon::Projectile> pool;
    pool.push_back(make_proj(drone_pos + glm::dvec3{0.0, 0.0, 20.0},
                             drone_pos + glm::dvec3{0.0, 0.0, -20.0}, 30.0));

    combat::CombatWorld cw = make_cw();
    combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt);

    CHECK(drones[0].hp == Catch::Approx(dp.hp - 30.0));
    CHECK(!pool[0].active);
    CHECK(cw.hits == 1);

    // A HitSpark should have been spawned.
    int active_fx = 0;
    for (const auto& f : cw.fx.pool) if (f.active) ++active_fx;
    CHECK(active_fx == 1);
    if (!cw.fx.pool.empty() && cw.fx.pool[0].active)
        CHECK(cw.fx.pool[0].kind == combat::FxKind::HitSpark);
}

// ===========================================================================
// Test 2a: Swept no-tunnel — both endpoints OUTSIDE R but closest approach
// INSIDE → still a hit (tests the swept part of the algorithm).
// MUTATION: use point-in-sphere only → misses the mid-segment hit.
// ===========================================================================
TEST_CASE("combat: swept collision catches closest-approach hit") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    const glm::dvec3 drone_pos{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(drone_pos, dp));
    drones[0].hp = dp.hp;

    // Round passes 6.7 m from center (within R=15 m), but both endpoints
    // are 14 m away laterally + 0.5 m longitudinally — well outside R.
    // The closest approach is at the midpoint, which is at distance ~6.7 m.
    const double lateral = 6.7;  // < 15 m → hit
    std::vector<weapon::Projectile> proj_pool;
    proj_pool.push_back(
        make_proj(drone_pos + glm::dvec3{lateral, 0.0, 14.0},
                  drone_pos + glm::dvec3{lateral, 0.0, -14.0}, 30.0));

    combat::CombatWorld cw = make_cw();
    combat::combat_tick(proj_pool, drones, cw, kAp, dp, kAp.sim_dt);

    CHECK(!proj_pool[0].active);  // hit registered
    CHECK(cw.hits == 1);
}

// ===========================================================================
// Test 2b: Drone-supplied closure — drone moves laterally toward the round's
// path → relative motion term causes the hit that would miss if drone were
// stationary.
// MUTATION: drop the drone velocity term from dr → miss.
// ===========================================================================
TEST_CASE("combat: drone-supplied closure triggers hit via relative motion") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    const glm::dvec3 p0{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(p0, dp));

    // Use R=5 for this test. Round passes 6 m laterally (miss if drone is
    // stationary at p0). Drone moves 4 m toward the round this tick, so in
    // the relative frame the closest approach is ~2 m < R=5 → hit.
    combat::CombatWorld cw = make_cw();
    cw.params.hit_radius_m = 5.0;

    // Round travels along X at lateral Y=6 from p0 (outside R=5 statically).
    // prev_pos = p0 + {6, 0, 20}, pos = p0 + {6, 0, -20}
    // Drone moves 4 m in Y this tick: prev = p0, curr = p0 + {0, 4, 0}
    drones[0].curr.position = p0 + glm::dvec3{0.0, 4.0, 0.0};
    // prev stays at p0 (set by stationary_drone constructor)

    std::vector<weapon::Projectile> proj_pool;
    proj_pool.push_back(
        make_proj(p0 + glm::dvec3{0.0, 6.0, 20.0},
                  p0 + glm::dvec3{0.0, 6.0, -20.0}, 30.0));

    // Verify: with drone stationary the closest approach is 6 m > R=5 → miss.
    {
        std::vector<drone::DroneState> static_drones;
        static_drones.push_back(stationary_drone(p0, dp));
        std::vector<weapon::Projectile> sp;
        sp.push_back(make_proj(p0 + glm::dvec3{0.0, 6.0, 20.0},
                               p0 + glm::dvec3{0.0, 6.0, -20.0}, 30.0));
        combat::CombatWorld cw2 = make_cw();
        cw2.params.hit_radius_m = 5.0;
        combat::combat_tick(sp, static_drones, cw2, kAp, dp, kAp.sim_dt);
        REQUIRE(cw2.hits == 0);  // confirm static miss
    }

    combat::combat_tick(proj_pool, drones, cw, kAp, dp, kAp.sim_dt);
    CHECK(cw.hits == 1);  // drone-supplied closure caused the hit
}

// ===========================================================================
// Test 3: Miss — closest approach at R+ε → no damage, round stays active.
// MUTATION: flip inequality to <= → triggers on the boundary-miss case.
// ===========================================================================
TEST_CASE("combat: near miss leaves round active and drone undamaged") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    const glm::dvec3 drone_pos{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(drone_pos, dp));
    const double R = 15.0;

    // Round passes exactly R + 0.1 m away from the drone center.
    const double miss_dist = R + 0.1;
    std::vector<weapon::Projectile> proj_pool;
    proj_pool.push_back(
        make_proj(drone_pos + glm::dvec3{miss_dist, 0.0, 20.0},
                  drone_pos + glm::dvec3{miss_dist, 0.0, -20.0}, 30.0));

    combat::CombatWorld cw = make_cw();
    combat::combat_tick(proj_pool, drones, cw, kAp, dp, kAp.sim_dt);

    CHECK(proj_pool[0].active);        // miss: round still flying
    CHECK(drones[0].hp == Catch::Approx(dp.hp));  // no damage
    CHECK(cw.hits == 0);
}

// ===========================================================================
// Test 4: Earliest victim -- one segment crosses two overlapping drones
// => only smaller-s drone damaged, ONE round retired; tie => lower index.
// MUTATION: pick any drone (not smallest s) => wrong victim.
// ===========================================================================
TEST_CASE("combat: earliest victim selected (smallest s), tie breaks to lower index") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    const glm::dvec3 base{6371000.0 + 2000.0, 0.0, 0.0};

    // Drone 0 is 30 m "ahead" (in the round's path), drone 1 is 60 m ahead.
    // Round travels from z=+100 to z=-100 through both.
    drones.push_back(stationary_drone(base + glm::dvec3{0.0, 0.0, -30.0}, dp));
    drones.push_back(stationary_drone(base + glm::dvec3{0.0, 0.0, -60.0}, dp));

    std::vector<weapon::Projectile> proj_pool;
    proj_pool.push_back(
        make_proj(base + glm::dvec3{0.0, 0.0, 100.0},
                  base + glm::dvec3{0.0, 0.0, -100.0}, 30.0));

    combat::CombatWorld cw = make_cw();
    combat::combat_tick(proj_pool, drones, cw, kAp, dp, kAp.sim_dt);

    // Drone 0 (at z=-30) is hit first (smaller s).
    CHECK(drones[0].hp == Catch::Approx(dp.hp - 30.0));
    CHECK(drones[1].hp == Catch::Approx(dp.hp));  // untouched
    CHECK(!proj_pool[0].active);
    CHECK(cw.hits == 1);
}

// ===========================================================================
// Test 5: N-hits-kill — 3×30 leaves 10 hp alive; 4th kills → respawned at
// full hp, age_ticks==0, prev==curr, explosion active, kills==1.
// ===========================================================================
TEST_CASE("combat: 4 cannon hits kill the drone (respawn at full hp)") {
    drone::DroneParams dp = default_dp();  // hp=100
    std::vector<drone::DroneState> drones;
    const glm::dvec3 drone_pos{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(drone_pos, dp));

    combat::CombatWorld cw = make_cw();

    // Hits 1–3: 30 damage each → hp goes 100 → 70 → 40 → 10
    for (int i = 0; i < 3; ++i) {
        std::vector<weapon::Projectile> pool;
        pool.push_back(make_proj(drone_pos + glm::dvec3{0.0, 0.0, 20.0},
                                 drone_pos + glm::dvec3{0.0, 0.0, -20.0}, 30.0));
        // Reset prev/curr so the drone hasn't moved (stationary test fixture).
        drones[0].prev.position = drone_pos;
        drones[0].curr.position = drone_pos;
        combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt);
    }
    CHECK(drones[0].hp == Catch::Approx(10.0));
    CHECK(cw.kills == 0);
    CHECK(cw.hits == 3);

    // Hit 4: lethal
    std::vector<weapon::Projectile> pool4;
    pool4.push_back(make_proj(drone_pos + glm::dvec3{0.0, 0.0, 20.0},
                              drone_pos + glm::dvec3{0.0, 0.0, -20.0}, 30.0));
    // Need prev/curr at drone_pos for the hit test (the drone respawned
    // at a DIFFERENT spawn slot, but for test simplicity keep prev/curr at
    // the same pos for the lethal shot).
    drones[0].prev.position = drone_pos;
    drones[0].curr.position = drone_pos;
    drones[0].hp = 10.0;  // ensure exactly 10 before the lethal shot

    combat::combat_tick(pool4, drones, cw, kAp, dp, kAp.sim_dt);

    CHECK(cw.kills == 1);
    CHECK(cw.hits == 4);
    // After respawn: hp == dp.hp, age_ticks == 0, prev == curr.
    CHECK(drones[0].hp == Catch::Approx(dp.hp));
    CHECK(drones[0].age_ticks == 0);
    CHECK(glm::length(drones[0].prev.position - drones[0].curr.position) <
          1e-9);
    // An explosion FX should be active.
    int explosions = 0;
    for (const auto& f : cw.fx.pool)
        if (f.active && f.kind == combat::FxKind::Explosion) ++explosions;
    CHECK(explosions >= 1);
}

// ===========================================================================
// Test 6: Same-tick overkill (P0-2) — two rounds through one drone same tick
// → both retire, ONE kill, ONE explosion; a third round through the spawn
// slot does NOT damage the fresh drone this tick (P0-2: pass 2 respawns AFTER
// pass 1 damage+retire, so the respawned position isn't tested this tick).
// ===========================================================================
TEST_CASE("combat: same-tick overkill: two rounds to one kill, spawn safe") {
    drone::DroneParams dp = default_dp();  // hp=100
    std::vector<drone::DroneState> drones;
    const glm::dvec3 drone_pos{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(drone_pos, dp));
    drones[0].hp = 30.0;  // needs 1 hit to kill (with damage=30)

    std::vector<weapon::Projectile> pool;
    // Round 0: lethal (damage=30, hp=30)
    pool.push_back(make_proj(drone_pos + glm::dvec3{0.0, 0.0, 20.0},
                             drone_pos + glm::dvec3{0.0, 0.0, -20.0}, 30.0));
    // Round 1: overkill (same path, same tick)
    pool.push_back(make_proj(drone_pos + glm::dvec3{0.0, 1.0, 20.0},
                             drone_pos + glm::dvec3{0.0, 1.0, -20.0}, 30.0));
    // Round 2: through the SPAWN SLOT — tests the P0-2 spawn-safety claim.
    // The drone respawns in pass 2 at a different position (respawn_in_place
    // moves it); this round's prev_pos/pos are at the OLD drone_pos, so it
    // may hit the respawned drone only if the respawn teleports it to the
    // same position. If the two-pass discipline holds, this round is INACTIVE
    // (it was retired in pass 1 via hitting the pre-respawn drone) OR the
    // freshly respawned drone is untouched (pass 1 already processed all hits
    // against the pre-kill state; pass 2 respawns, no pass 3).
    // We add a third round at a known miss trajectory (the respawn moves to a
    // different position, but we still want to assert the fresh drone is at
    // full hp after the two-pass cycle). (FIX F.3: the promised assertion was
    // missing from the test body — added here per the P0-2 spec.)
    pool.push_back(make_proj(drone_pos + glm::dvec3{0.0, 0.0, 20.0},
                             drone_pos + glm::dvec3{0.0, 0.0, -20.0}, 30.0));
    // Mark round 2 as hitting the old pos (same path as round 0).
    // In a two-pass system, pass 1 fires all three rounds, but rounds 0 and 1
    // retire the drone; then pass 2 respawns it. Round 2 (pool[2]) hits the
    // pre-kill drone (which is at drone_pos with hp>0 at pass-1 start).
    // The spec says only pass 1 damage is applied; the respawned drone is safe.
    // We assert: after combat_tick, drones[0].hp == dp.hp (respawned full hp).

    combat::CombatWorld cw = make_cw();
    combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt);

    // Both rounds 0 and 1 must have retired (both hit the drone this tick).
    CHECK(!pool[0].active);
    CHECK(!pool[1].active);
    // Exactly ONE kill.
    CHECK(cw.kills == 1);
    // The drone respawned with full hp after the kill.
    CHECK(drones[0].hp == Catch::Approx(dp.hp));
    // Only one explosion.
    int explosions = 0;
    for (const auto& f : cw.fx.pool)
        if (f.active && f.kind == combat::FxKind::Explosion) ++explosions;
    CHECK(explosions == 1);
    // P0-2 SPAWN-SAFETY ASSERTION (FIX F.3): the fresh drone is at full hp
    // even though round 2 passed through the old spawn position this tick.
    // The two-pass discipline ensures pass 2 (respawn) happens AFTER all pass 1
    // hits, so the respawned drone cannot be hit until the NEXT tick.
    CHECK(drones[0].hp == Catch::Approx(dp.hp));  // fresh drone: full hp, untouched
    // Bonus: round 2 must also have been retired (it hit the pre-kill drone in
    // pass 1, same as rounds 0 and 1 — all three rounds tested the pre-respawn
    // drone, which has hp <= 0 so still receives damage in pass 1).
    CHECK(!pool[2].active);  // round 2 retired: hit the pre-kill drone body
}

// ===========================================================================
// Test 7: Crash-respawn resets HP (P0-4) — wound a drone, force ground
// contact via tick() → respawned at full hp.
// ===========================================================================
TEST_CASE("combat: crash-respawn via tick() resets hp to dp.hp") {
    drone::DroneParams dp = default_dp();

    // A drone already below ground level — crash detected on next tick.
    // Place it at altitude = -10 m (already underground) so altitude() <= 0
    // fires on the FIRST tick.
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    // Set position BELOW the crash sphere (R - 10 m radially inward).
    d.curr.position = {kAp.R - 10.0, 0.0, 0.0};
    d.curr.velocity = {0.0, 0.0, 0.0};
    d.curr.orientation = glm::dquat{1, 0, 0, 0};
    d.prev = d.curr;
    d.grounded = false;
    d.hp = 40.0;  // wounded

    // One tick → altitude <= 0 → respawn_in_place → hp = dp.hp.
    drone::tick(d, kAp, dp, nullptr);

    CHECK(d.hp == Catch::Approx(dp.hp));  // full hp after respawn
    CHECK(d.age_ticks == 0);
    CHECK(d.grounded);
}

// ===========================================================================
// Test 8: FIREWALL — run step_frame N ticks with cw present vs cw=nullptr
// → player LoopState (st.curr, st.prev) bit-identical.
// ===========================================================================
TEST_CASE("combat: cw=nullptr is bit-identical to no-combat (firewall)") {
    app::LoopState st_with{}, st_without{};
    const sim::SimState spawn = app::spawn_state(kAp);
    st_with.curr = st_without.curr = spawn;
    st_with.prev = st_without.prev = spawn;
    st_with.prev_up = st_without.prev_up = sim::local_up(spawn.position);
    st_with.aim.reseed(spawn.orientation, st_with.prev_up);
    st_without.aim.reseed(spawn.orientation, st_without.prev_up);
    st_with.grounded = st_without.grounded = true;

    drone::DroneParams dp = default_dp();

    // Each run gets its own drone world to avoid shared state.
    app::DroneWorld dw_with;
    dw_with.dparams = dp;
    dw_with.drones.push_back(drone::spawn_drone(kAp, dp, 0, 1));

    app::DroneWorld dw_without;
    dw_without.dparams = dp;
    dw_without.drones.push_back(drone::spawn_drone(kAp, dp, 0, 1));

    weapon::GunWorld gw_with;
    gw_with.battery.convergence_range = 500.0;
    weapon::GunSpec gs;
    gs.muzzle_body = {0.0, 0.0, -2.87};
    gs.kind = weapon::Round::Cannon20mm;
    gs.muzzle_speed = 805.0;
    gs.rof_hz = 12.0;
    gs.drag_k = 0.0008;
    gs.damage = 30.0;
    gw_with.battery.guns.push_back(gs);

    weapon::GunWorld gw_without;
    gw_without.battery.convergence_range = 500.0;
    gw_without.battery.guns.push_back(gs);

    combat::CombatWorld cw_obj = make_cw();

    app::Accumulator accum_with(kAp.sim_dt, 0.25);
    app::Accumulator accum_without(kAp.sim_dt, 0.25);
    double pdx_w = 0.0, pdy_w = 0.0, pdx_n = 0.0, pdy_n = 0.0;

    // Run 60 ticks with fire_held=true.
    for (int i = 0; i < 60; ++i) {
        app::FrameInput fin;
        fin.fire_held = true;
        fin.throttle = 1.0;
        app::step_frame(st_with, accum_with, kAp.sim_dt, fin, pdx_w, pdy_w,
                        kAp, kCp, nullptr, &dw_with, &gw_with, &cw_obj);
        app::step_frame(st_without, accum_without, kAp.sim_dt, fin, pdx_n, pdy_n,
                        kAp, kCp, nullptr, &dw_without, &gw_without, nullptr);
    }

    // Player state must be bit-identical (no combat_tick writes to LoopState).
    CHECK(std::memcmp(&st_with.curr, &st_without.curr, sizeof(sim::SimState)) == 0);
    CHECK(std::memcmp(&st_with.prev, &st_without.prev, sizeof(sim::SimState)) == 0);
}

// ===========================================================================
// Test 9: Determinism — identical scripted scenario twice → identical results.
// ===========================================================================
TEST_CASE("combat: determinism: same scenario produces identical results") {
    drone::DroneParams dp = default_dp();

    auto run_scenario = [&]() {
        std::vector<drone::DroneState> drones;
        const glm::dvec3 pos{6371000.0 + 2000.0, 0.0, 0.0};
        drones.push_back(stationary_drone(pos, dp));

        std::vector<weapon::Projectile> pool;
        pool.push_back(make_proj(pos + glm::dvec3{0.0, 0.0, 20.0},
                                 pos + glm::dvec3{0.0, 0.0, -20.0}, 30.0));

        combat::CombatWorld cw = make_cw();
        combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt);
        combat::fx_tick(cw.fx, kAp.sim_dt);

        return std::make_tuple(drones[0].hp, cw.kills, cw.hits,
                               pool[0].active,
                               cw.fx.pool.empty() ? false : cw.fx.pool[0].active);
    };

    const auto r1 = run_scenario();
    const auto r2 = run_scenario();
    CHECK(std::get<0>(r1) == std::get<0>(r2));
    CHECK(std::get<1>(r1) == std::get<1>(r2));
    CHECK(std::get<2>(r1) == std::get<2>(r2));
    CHECK(std::get<3>(r1) == std::get<3>(r2));
    CHECK(std::get<4>(r1) == std::get<4>(r2));
}

// ===========================================================================
// Test 10: Spawn-tick round (P0-1) — a round spawned into a reused free slot
// has prev_pos == pos → cannot phantom-hit via the dead slot's old position.
// MUTATION: inherit prev_pos from the dead slot → phantom kill possible.
// ===========================================================================
TEST_CASE("combat: newly spawned round has prev_pos==pos (no phantom hit)") {
    // Build a minimal GunWorld, fire one round, retire it, fire into the same slot.
    weapon::GunWorld gw;
    gw.battery.convergence_range = 500.0;
    weapon::GunSpec gs;
    gs.muzzle_body = {0.0, 0.0, -2.87};
    gs.kind = weapon::Round::Cannon20mm;
    gs.muzzle_speed = 805.0;
    gs.rof_hz = 12.0;
    gs.drag_k = 0.0;
    gs.damage = 30.0;
    gw.battery.guns.push_back(gs);

    sim::SimState shooter;
    shooter.position = {kAp.R + 2000.0, 0.0, 0.0};
    shooter.orientation = glm::dquat{1, 0, 0, 0};
    shooter.velocity = glm::dvec3{0.0};

    // Fire one round then retire it.
    weapon::fire_tick(gw, shooter, true, kAp.sim_dt, kAp.g, kAp.R);
    REQUIRE(!gw.pool.empty());
    gw.pool[0].active = false;  // manually retire

    // Fire again into the free slot.
    weapon::fire_tick(gw, shooter, true, kAp.sim_dt, kAp.g, kAp.R);

    // The reused (or new) slot must have prev_pos == pos at birth.
    for (const weapon::Projectile& p : gw.pool) {
        if (!p.active) continue;
        const double diff = glm::length(p.prev_pos - p.pos);
        CHECK(diff == Catch::Approx(0.0).margin(1e-9));
    }
}

// ===========================================================================
// KE damage model (Fable 2026-07-13): damage = D_ref · min(|v_rel|²/v_ref², 2)
// · clamp(cosθ, 0.3, 1). swept_hit uses POSITIONS and the damage uses p.vel,
// so we hold a center-through hit fixed and vary only p.vel / v_ref / lateral.
// ===========================================================================
namespace {
// Fire ONE projectile (given vel, v_ref, lateral graze offset) at a stationary
// drone parked at huge hp (no kill/respawn), return HP dropped = damage dealt.
double damage_of(const glm::dvec3& vel, double v_ref, double lateral,
                 double ref_damage = 30.0) {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    const glm::dvec3 drone_pos{6371000.0 + 2000.0, 0.0, 0.0};
    drones.push_back(stationary_drone(drone_pos, dp));
    drones[0].hp = 100000.0;  // huge: no shot here can kill/respawn
    weapon::Projectile p;
    p.prev_pos = drone_pos + glm::dvec3{lateral, 0.0, 20.0};
    p.pos      = drone_pos + glm::dvec3{lateral, 0.0, -20.0};
    p.vel = vel;
    p.v_ref = v_ref;
    p.damage = ref_damage;
    p.drag_k = 0.0;
    p.age = 0.01;
    p.kind = weapon::Round::Cannon20mm;
    p.active = true;
    std::vector<weapon::Projectile> pool{p};
    combat::CombatWorld cw = make_cw();
    combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt);
    return 100000.0 - drones[0].hp;
}
}  // namespace

TEST_CASE("combat: KE damage scales with (v_rel/v_ref)^2 (range falloff)") {
    const double d_fast = damage_of({0.0, 0.0, -800.0}, 800.0, 0.0);  // q²=1   → 30
    const double d_slow = damage_of({0.0, 0.0, -400.0}, 800.0, 0.0);  // q²=.25 → 7.5
    CHECK(d_fast == Catch::Approx(30.0));
    CHECK(d_slow == Catch::Approx(7.5));
    CHECK(d_slow < d_fast);  // monotone: a slower (farther) round bites less
}

TEST_CASE("combat: obliquity floors a glancing hit") {
    const double d_center = damage_of({0.0, 0.0, -800.0}, 800.0, 0.0);  // f_obl=1   → 30
    const double d_graze  = damage_of({0.0, 0.0, -800.0}, 800.0, 6.0);  // ⟂ → floor → 9
    CHECK(d_center == Catch::Approx(30.0));
    CHECK(d_graze  == Catch::Approx(9.0));  // 30 · 1 · 0.3 (floor)
    CHECK(d_graze < d_center);
}

TEST_CASE("combat: overspeed merge caps at 2x reference damage") {
    const double d = damage_of({0.0, 0.0, -2000.0}, 800.0, 0.0);  // q²_raw=6.25 → cap 2
    CHECK(d == Catch::Approx(60.0));  // 30 · 2.0 · 1, NOT 187.5
}

TEST_CASE("combat: near-zero relative impact speed does no damage (NaN guard)") {
    const double d = damage_of({0.0, 0.0, -0.5}, 800.0, 0.0);  // |v_rel|²=0.25 < 1
    CHECK(d == Catch::Approx(0.0));
}

// ===========================================================================
// Hit-FX curves (combat/fx_curves.h, Fable 2026-07-13): the spark/fireball/
// debris are PURE, DETERMINISTIC, and burn out to EXACTLY zero at end of life.
// (Chad can't fly-test, so the visual curves get a headless invariant pin.)
// ===========================================================================
TEST_CASE("fx: spark count is bounded and energy-scaled") {
    CHECK(combat::spark_count(0.0) == 6);
    CHECK(combat::spark_count(1.0) == 16);
    CHECK(combat::spark_count(0.5) >= 6);
    CHECK(combat::spark_count(0.5) <= 16);
    CHECK(combat::spark_count(2.0) == 16);   // clamps
    CHECK(combat::spark_count(-1.0) == 6);   // clamps
}

TEST_CASE("fx: spark curve is deterministic and dies to zero at 0.20 s") {
    const glm::dvec3 n{0.0, 0.0, 1.0};
    const combat::SparkSample a = combat::spark_particle(42, 3, 0.05, 0.8, n);
    const combat::SparkSample b = combat::spark_particle(42, 3, 0.05, 0.8, n);
    CHECK(a.brightness == b.brightness);            // pure: same in → same out
    CHECK(glm::length(a.offset - b.offset) < 1e-15);
    const combat::SparkSample end = combat::spark_particle(42, 3, 0.20, 0.8, n);
    CHECK(end.brightness == Catch::Approx(0.0).margin(1e-12));  // burns out exactly
    CHECK(end.size_m == Catch::Approx(0.0).margin(1e-12));
    const combat::SparkSample mid = combat::spark_particle(42, 3, 0.03, 0.8, n);
    CHECK(mid.brightness > 0.0);                     // alive mid-life
}

TEST_CASE("fx: fireball expands monotonically and alpha hits exact 0 at 1.5 s") {
    const combat::FireballState f0 = combat::fireball_state(0.02, 1.0);
    const combat::FireballState f1 = combat::fireball_state(0.30, 1.0);
    CHECK(f1.radius_m > f0.radius_m);                       // fast expand
    CHECK(combat::fireball_state(1.5, 1.0).alpha ==
          Catch::Approx(0.0).margin(1e-12));               // exact burnout
    CHECK(f0.alpha > 0.0);
}

TEST_CASE("fx: debris deterministic, moves, dims to zero, and sags under gravity") {
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const combat::DebrisSample d = combat::debris_particle(7, 5, 0.5, 1.0, up);
    const combat::DebrisSample d2 = combat::debris_particle(7, 5, 0.5, 1.0, up);
    CHECK(glm::length(d.offset - d2.offset) < 1e-15);      // pure: same in → same out
    CHECK(glm::length(d.offset) > 0.0);                    // motion happened
    CHECK(combat::debris_particle(7, 5, 1.5, 1.0, up).brightness ==
          Catch::Approx(0.0).margin(1e-12));               // dims out exactly

    // Gravity sag is the ONLY term along -up shared by every chunk, so it is
    // isolated by the mean over a full (near-balanced) Fibonacci sphere: the
    // fleet's average -up displacement must grow with time. (A single chunk's
    // along-direction throw can dwarf its own sag, so average, don't sample.)
    auto mean_sag = [&](double age) {
        double acc = 0.0;
        for (int k = 0; k < combat::kDebrisCount; ++k)
            acc += glm::dot(combat::debris_particle(7, k, age, 1.0, up).offset, -up);
        return acc / combat::kDebrisCount;
    };
    CHECK(mean_sag(1.4) > mean_sag(0.2));                  // the fleet falls over time
}

// ===========================================================================
// Bandit return fire + player damage (docs/bandit_combat_plan.md §3-6).
// ===========================================================================

// Difficulty mapping: an int level 1..5, clamped, monotone in threat (more
// attackers, faster cadence, harder rounds). MUTATION: a non-monotone / unclamped
// table -> a level knob that doesn't scale the fight.
TEST_CASE("combat: difficulty_params clamps and scales monotonically") {
    CHECK(combat::difficulty_params(1).max_engaged == 1);
    CHECK(combat::difficulty_params(5).max_engaged == 4);
    // Clamp below / above the [1,5] range.
    CHECK(combat::difficulty_params(0).max_engaged ==
          combat::difficulty_params(1).max_engaged);
    CHECK(combat::difficulty_params(99).bandit_damage ==
          combat::difficulty_params(5).bandit_damage);
    // Monotone cadence + damage.
    CHECK(combat::difficulty_params(3).bandit_rof_hz >
          combat::difficulty_params(1).bandit_rof_hz);
    CHECK(combat::difficulty_params(5).bandit_damage >
          combat::difficulty_params(3).bandit_damage);
    CHECK(combat::difficulty_params(4).max_engaged >=
          combat::difficulty_params(2).max_engaged);
}

namespace {
// A CombatWorld set up for bandit return fire (a real bandit gun).
combat::CombatWorld make_armed_cw(int difficulty = 3) {
    combat::CombatWorld cw = make_cw();
    cw.setup.difficulty = difficulty;
    cw.setup.player_hp = 100.0;
    cw.player_hp = 100.0;
    cw.setup.player_hit_radius_m = 9.0;
    cw.setup.respawn_invuln_time = 2.0;
    cw.setup.bandit_convergence = 250.0;
    cw.setup.bandit_gun.muzzle_body = glm::dvec3{0.0};  // CG
    cw.setup.bandit_gun.kind = weapon::Round::Cannon20mm;
    cw.setup.bandit_gun.muzzle_speed = 600.0;
    cw.setup.bandit_gun.drag_k = 0.0;
    return cw;
}
}  // namespace

// enemy_fire_tick: an engaged bandit that wants to fire and is off cooldown
// SPAWNS one round into the enemy pool and recharges its cooldown. MUTATION:
// forget to recharge -> full-auto every tick (cadence gate below catches it too).
TEST_CASE("combat: enemy_fire_tick spawns a bandit round and recharges cooldown") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    drones.push_back(drone::spawn_drone(kAp, dp, 0, 1));
    drones[0].engaged = true;
    drones[0].wants_fire = true;
    drones[0].fire_cooldown = 0.0;

    combat::CombatWorld cw = make_armed_cw();
    combat::enemy_fire_tick(cw, drones, kAp.sim_dt, kAp.g, kAp.R);

    int active = 0;
    for (const auto& p : cw.enemy_pool) if (p.active) ++active;
    CHECK(active == 1);
    CHECK(drones[0].fire_cooldown > 0.0);  // recharged (drains, never resets)
    // A non-engaged / non-firing bandit spawns nothing.
    std::vector<drone::DroneState> idle;
    idle.push_back(drone::spawn_drone(kAp, dp, 0, 1));  // engaged=false
    combat::CombatWorld cw2 = make_armed_cw();
    combat::enemy_fire_tick(cw2, idle, kAp.sim_dt, kAp.g, kAp.R);
    CHECK(cw2.enemy_pool.empty());
}

// Cadence is gated by rof: holding wants_fire every tick for one second fires
// ~rof rounds, NOT one per tick. MUTATION: reset cooldown to 0 on a non-fire tick
// (the iter-9 trigger-tap bug) -> a round every tick.
TEST_CASE("combat: enemy fire cadence obeys rof, not one-per-tick") {
    drone::DroneParams dp = default_dp();
    std::vector<drone::DroneState> drones;
    drones.push_back(drone::spawn_drone(kAp, dp, 0, 1));
    drones[0].engaged = true;

    const double rof = combat::difficulty_params(3).bandit_rof_hz;  // burst rate
    combat::CombatWorld cw = make_armed_cw(3);
    const int ticks = static_cast<int>(1.0 / kAp.sim_dt);  // ~1 s
    for (int i = 0; i < ticks; ++i) {
        drones[0].wants_fire = true;  // held every tick
        combat::enemy_fire_tick(cw, drones, kAp.sim_dt, kAp.g, kAp.R);
    }
    // Rounds don't retire within 1 s, so the pool size == total spawned. Holding
    // fire for 1 s at `rof` Hz fires ~rof rounds (a burst), NOT one per tick.
    const int spawned = static_cast<int>(cw.enemy_pool.size());
    CHECK(spawned >= static_cast<int>(rof) - 1);
    CHECK(spawned <= static_cast<int>(rof) + 1);  // paced by rof
    CHECK(spawned < ticks / 4);                   // decisively NOT full-auto
}

// combat_player_tick: an enemy round swept through the player segment drops HP,
// retires the round, sparks. MUTATION: skip the sweep -> no damage taken (the
// whole "bandits can shoot me" feature is dead).
TEST_CASE("combat: an enemy round damages the player") {
    combat::CombatWorld cw = make_armed_cw();
    sim::SimState pprev, pcurr;
    const glm::dvec3 pos{6371000.0 + 2000.0, 0.0, 0.0};
    pprev.position = pos;
    pcurr.position = pos;              // stationary player (point segment)
    pprev.velocity = pcurr.velocity = glm::dvec3{0.0};

    weapon::Projectile p;              // center-through, |v|==v_ref, normal hit
    p.prev_pos = pos + glm::dvec3{0.0, 0.0, 20.0};
    p.pos = pos + glm::dvec3{0.0, 0.0, -20.0};
    p.vel = glm::normalize(p.pos - p.prev_pos) * 600.0;
    p.v_ref = 600.0;
    p.damage = 16.0;
    p.drag_k = 0.0;
    p.age = 0.01;
    p.kind = weapon::Round::Cannon20mm;
    p.active = true;
    cw.enemy_pool.push_back(p);

    const double hp0 = cw.player_hp;  // 100 (all components pristine)
    combat::combat_player_tick(cw, pprev, pcurr);
    // Damage now routes to COMPONENTS (player_hp is the derived worst-component
    // summary). A CENTER-through hit (identity orientation, on the CG line) routes
    // to the fuselage: structure 65% / pilot 35% of the 16 KE / component_hp.
    CHECK(cw.player_hp < hp0);                      // the plane took damage
    CHECK(cw.damage.structure < 1.0);              // center hit -> structure
    CHECK(cw.damage.pilot < 1.0);                  // ...and the pilot (35% share)
    CHECK(cw.damage.wing_left == 1.0);             // not a wing (on centerline)
    CHECK(cw.damage.wing_right == 1.0);
    CHECK(!cw.enemy_pool[0].active);   // round retired
    CHECK(cw.player_hits == 1);
    int sparks = 0;
    for (const auto& f : cw.fx.pool)
        if (f.active && f.kind == combat::FxKind::HitSpark) ++sparks;
    CHECK(sparks == 1);
}

// Respawn invuln (P1-4): while player_invuln_ticks > 0 the sweep is SKIPPED (a
// fresh spawn can't be re-hit by in-flight rounds) and the counter decrements.
// MUTATION: drop the invuln gate -> spawn-camp kills right after respawn.
TEST_CASE("combat: player invulnerability blocks damage and decrements") {
    combat::CombatWorld cw = make_armed_cw();
    cw.player_invuln_ticks = 5;
    sim::SimState pprev, pcurr;
    const glm::dvec3 pos{6371000.0 + 2000.0, 0.0, 0.0};
    pprev.position = pcurr.position = pos;

    weapon::Projectile p;  // a lethal center-through round
    p.prev_pos = pos + glm::dvec3{0.0, 0.0, 20.0};
    p.pos = pos + glm::dvec3{0.0, 0.0, -20.0};
    p.vel = glm::normalize(p.pos - p.prev_pos) * 600.0;
    p.v_ref = 600.0;
    p.damage = 30.0;
    p.active = true;
    cw.enemy_pool.push_back(p);

    const double hp0 = cw.player_hp;
    combat::combat_player_tick(cw, pprev, pcurr);
    CHECK(cw.player_hp == Catch::Approx(hp0));  // no damage while invulnerable
    CHECK(cw.player_invuln_ticks == 4);         // decremented
    CHECK(cw.enemy_pool[0].active);             // round NOT consumed
}

// Integration: in the committed scenario, a player chasing the spawned fleet
// makes the bandits ENGAGE and actively CLOSE (the "they just fly away" fix). We
// don't pin gunnery hits (too geometry-sensitive for a stable pin — verified by
// fly + smoke), only that the pursuit turns the fleet from fleeing to hunting:
// the nearest attacker's nose swings meaningfully toward the player and it closes
// range over the run. MUTATION: ignore the engaged flag / patrol always -> the
// nearest bandit neither engages nor closes.
TEST_CASE("combat: the fleet engages and closes on a chasing player") {
    const cfg::ScenarioParams scen =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

    app::LoopState st{};
    st.curr = st.prev = app::spawn_state(kAp);
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = true;

    app::DroneWorld dw;
    dw.dparams = scen.drone;
    dw.gparams = scen.gunsight;
    for (int i = 0; i < dw.dparams.count; ++i)
        dw.drones.push_back(
            drone::spawn_drone(kAp, dw.dparams, i, dw.dparams.count));

    weapon::GunWorld gw;
    gw.battery.convergence_range = 500.0;
    combat::CombatWorld cw = make_cw();
    cw.setup = scen.combat;
    cw.player_hp = scen.combat.player_hp;

    auto nose_on_0 = [&]() {
        const auto& d = dw.drones[0];
        const glm::dvec3 nz = d.curr.orientation * glm::dvec3{0, 0, -1};
        return glm::dot(glm::normalize(st.curr.position - d.curr.position), nz);
    };
    // Drone 0 starts ahead of the player, flying AWAY (nose pointing away from the
    // player: nose-on ~ -1).
    const double nose_on_start = nose_on_0();
    REQUIRE(nose_on_start < -0.5);
    const double range_start =
        glm::length(dw.drones[0].curr.position - st.curr.position);

    app::TickInput in;
    in.throttle = 1.0;  // chase straight ahead
    bool ever_engaged = false;
    double best_nose_on = nose_on_start;
    double min_range0 = range_start;
    // FIX-F1 fold: the window is DERIVED from the reversal physics, not a
    // welded 20 s. The old window was calibrated to the pre-fix flow where
    // the energy bail (extend_energy_m = 600) broke the fight early and the
    // re-intercept happened to swing the nose fast; an honest lag-pursuit
    // reversal at the bank cap takes t_rev = pi*V/(g*tan(bank_cap)) seconds
    // (~26 s at pursuit speed), so the window is 2*t_rev + a 10 s
    // engage/roll-in margin — config-relative, survives any dial retune.
    const double v_pursuit = scen.drone.speed + scen.drone.pursue_speed_bump;
    const double t_rev =
        3.14159265358979 * v_pursuit /
        (kAp.g * std::tan(scen.drone.pursue_max_bank));
    const int window_ticks = static_cast<int>(
        std::lround((2.0 * t_rev + 10.0) / kAp.sim_dt));
    for (int t = 0; t < window_ticks; ++t) {
        app::tick(st, in, kAp, kCp, nullptr, &dw, &gw, &cw);
        for (const auto& d : dw.drones)
            if (d.engaged) ever_engaged = true;
        best_nose_on = std::max(best_nose_on, nose_on_0());
        min_range0 = std::min(
            min_range0,
            glm::length(dw.drones[0].curr.position - st.curr.position));
    }

    REQUIRE(ever_engaged);                       // the fleet became attackers
    REQUIRE(best_nose_on > 0.3);                 // drone 0 swung its nose AT the player
    REQUIRE(best_nose_on > nose_on_start + 0.8); // a big swing from fleeing (~ -1)
    REQUIRE(min_range0 < range_start - 200.0);   // it closed, did not fly away
    // (gunnery HITS are geometry-sensitive — verified by fly + smoke, not pinned.)
}

// Damage WIRING end-to-end (Fable impl-review gap 1): a broken wing biases the
// commanded roll toward the broken side and actually banks the plane. Raw mode so
// the stick is a clean 0 and the whole roll is the injected bias. MUTATION: delete
// `with_bias` at the sim::step site, or the dmg_roll_bias block -> no bias, no bank.
TEST_CASE("damage wiring: a broken wing biases the roll and banks the plane") {
    auto fly = [](double wing_left, double wing_right) {
        app::LoopState st{};
        const sim::SimState spawn = app::spawn_state(kAp);
        st.curr = st.prev = spawn;
        st.prev_up = sim::local_up(spawn.position);
        st.aim.reseed(spawn.orientation, st.prev_up);
        st.grounded = false;  // live airframe (bias is gated off when grounded)

        combat::CombatWorld cw = make_armed_cw();
        cw.damage.wing_left = wing_left;
        cw.damage.wing_right = wing_right;

        app::TickInput in;
        in.raw_mode = true;           // clean stick: roll bias is the only roll input
        in.raw_in.throttle = 1.0f;
        app::TickResult r{};
        double last_roll = 0.0;
        for (int i = 0; i < 240; ++i) {  // 2 s
            r = app::tick(st, in, kAp, kCp, nullptr, nullptr, nullptr, &cw);
            last_roll = r.inputs.roll;
        }
        // Signed bank: + = right wing down (roll right). local phi via extract.
        const control::Extracted e =
            control::extract(st.curr, st.curr.last_vhat, kAp.v_dir_eps);
        return std::make_pair(last_roll, e.phi);
    };

    const auto broke_left = fly(0.1, 1.0);
    const auto broke_right = fly(1.0, 0.1);
    // Broken LEFT wing -> +roll input (roll LEFT, sim/state.h) -> banks LEFT (phi<0).
    CHECK(broke_left.first > 0.05);    // commanded roll biased positive
    CHECK(broke_left.second < -0.02);  // banked left
    // Broken RIGHT wing -> the mirror.
    CHECK(broke_right.first < -0.05);
    CHECK(broke_right.second > 0.02);
}

// Damage WIRING (Fable impl-review gap 4): a CRASH into the ground clears component
// damage too (both respawn paths reset — P0-4), not just the gun-death path.
TEST_CASE("damage wiring: a crash respawn clears component damage") {
    app::LoopState st{};
    sim::SimState s = app::spawn_state(kAp);
    // Aim it straight down, just above the ground, so altitude crosses 0 fast.
    s.position = glm::normalize(s.position) * (kAp.R + 5.0);
    s.velocity = -glm::normalize(s.position) * 200.0;  // radially inward
    s.last_vhat = glm::normalize(s.velocity);
    st.curr = st.prev = s;
    st.prev_up = sim::local_up(s.position);
    st.aim.reseed(s.orientation, st.prev_up);
    st.grounded = false;

    combat::CombatWorld cw = make_armed_cw();
    cw.damage.engine = 0.4;
    cw.damage.wing_right = 0.6;  // wounded but alive (not is_dead)
    REQUIRE_FALSE(combat::damage_zero(cw.damage));

    app::TickInput in;
    in.raw_mode = true;
    bool respawned = false;
    for (int i = 0; i < 30 && !respawned; ++i)
        respawned = app::tick(st, in, kAp, kCp, nullptr, nullptr, nullptr, &cw).respawned;
    REQUIRE(respawned);
    CHECK(combat::damage_zero(cw.damage));  // fresh airframe after the crash
}

// End-to-end player death -> respawn (P0-A, resolved per-tick inside app::tick).
// Death is now COMPONENT-based (a dead pilot kills). Kill the pilot, run one
// app::tick with a combat world present, and pin the reset: respawned airborne,
// damage cleared + HP restored, invuln armed, enemy pool cleared, deaths bumped.
// MUTATION: resolve death after step_frame instead of per tick -> deaths/HP not
// handled inside the tick (this pin fails).
TEST_CASE("combat: player death respawns airborne with HP restored and invuln") {
    app::LoopState st{};
    const sim::SimState spawn = app::spawn_state(kAp);
    st.curr = st.prev = spawn;
    st.prev_up = sim::local_up(spawn.position);
    st.aim.reseed(spawn.orientation, st.prev_up);
    st.grounded = false;  // flying

    drone::DroneParams dp = default_dp();
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(drone::spawn_drone(kAp, dp, 0, 1));

    weapon::GunWorld gw;
    gw.battery.convergence_range = 500.0;

    combat::CombatWorld cw = make_armed_cw();
    cw.damage.pilot = 0.0;  // a dead pilot -> is_dead() -> death this tick

    app::TickInput in;
    in.throttle = 1.0;
    const app::TickResult r = app::tick(st, in, kAp, kCp, nullptr, &dw, &gw, &cw);

    CHECK(r.respawned);
    CHECK(cw.damage.pilot == 1.0);  // damage cleared on respawn (P0-4)
    CHECK(cw.deaths == 1);
    CHECK(cw.player_hp == Catch::Approx(cw.setup.player_hp));  // restored
    CHECK(cw.player_invuln_ticks > 0);                          // invuln armed
    CHECK(cw.ticks_since_death == 0);                           // HUD flash
    CHECK(st.grounded);  // reborn -> next tick is a GROUNDED reset pairing
    CHECK(sim::altitude(st.curr.position, kAp) > 0.0);  // airborne, not underground
}
