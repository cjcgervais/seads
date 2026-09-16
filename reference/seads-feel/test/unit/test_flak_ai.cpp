// FLAK STAGE D -- THE AI GUNNER'S GATE (docs/FLAK_GUN_SPEC.md §7,
// immersion ladder 4/4).
//
// The five things this rung claims, each pinned with a LIVENESS arm beside it
// (the E-ladder law: a bit-identical arm is a blind fixture or a dead branch
// unless something in the same case can tell the two apart):
//
//   1. An AI gunner ACQUIRES an in-range OPPOSING raider, tracks it with LEAD
//      (the bore points AHEAD of the line of sight, not at it), and fires.
//   2. No target, wrong faction, or out of range => not one round leaves the
//      barrel and fire_held is false.
//   3. The gun the PLAYER mans is never AI-driven: demand and fire_held are
//      untouched, the pool stays empty, and a release re-arms from where the
//      barrel actually is (no snap).
//   4. The no-AI-arm path is bit-identical: app::tick with the arm absent and
//      with an EMPTY arm produce the same state, tick for tick.
//   5. KILL CREDIT. An AI gun's combat sweep books its kills into a SINK --
//      cw.killed_spawn_indices (which the conquest wiring pays
//      on_player_kill for), cw.kills (the HUD counter) and cw.ticks_since_kill
//      (the KILL flash) do not move -- and combat::on_ai_kill takes the roster
//      down without paying the player a point.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/flak_ai.h"
#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Matchers::WithinAbs;

namespace {

constexpr double kDt = 1.0 / 120.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kR = 15000.0;  // the test sphere (test_flak_gun's)
constexpr double kG = 9.81;

// The site: a GENERIC local up, so no world axis is up (the "flat instrument
// certifies a flat controller" trap, CLAUDE.md Learned).
const glm::dvec3 kUp = glm::normalize(glm::dvec3(0.6, 1.0, -0.8));

// One AI gun, wired exactly the way main.cpp wires it.
app::FlakAiGun ai_gun(int faction, unsigned seed = 7u) {
    app::FlakAiGun ag;
    ag.faction = faction;
    ag.gun = 0;
    ag.ai.seed = seed;
    ag.ai.rng = seed ^ 0x9e3779b9u;
    ag.fk.mount = render::flak::make_mount_frame(kUp * kR, kUp,
                                                 glm::dvec3(0.2, 0.4, 0.9));
    ag.fk.gw.battery.convergence_range = 800.0;
    ag.fk.gw.battery.guns.push_back(
        {glm::dvec3(0.0, 1.623, -1.448), weapon::Round::Cannon20mm,
         render::flak::kMuzzleSpeedMps, render::flak::kRofHz, 0.0008, 30.0});
    ag.fk.drum_rounds = render::flak::kDrumRounds;
    ag.fk.rounds_left = render::flak::kDrumRounds;
    ag.fk.reload_s = 4.0;
    ag.fk.selfdestruct_s = 1.6;
    ag.fk.prox_radius_m = 4.0;
    ag.fk.manned = true;  // an AI gunner IS a man on the gun
    app::flak_rebuild_shooter(ag.fk);
    return ag;
}

// A drone of a chosen SIDE, crossing the gun's front. spawn_index 0-4 are
// SUDBURY, 5-9 are VALLEY (combat/conquest.h) -- so the index IS the faction
// declaration, and the test asks for it by faction, never by a literal.
drone::DroneState raider(int faction, const glm::dvec3& pos,
                         const glm::dvec3& vel) {
    drone::DroneState d;
    d.spawn_index = (faction == combat::CQ_SUDBURY) ? 2 : 7;
    REQUIRE(combat::maverick_faction(d.spawn_index) == faction);
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

// Advance the drone on a straight line, then run ONE fixed tick of the gun
// exactly the way app::tick does: controller first, then the pure flak tick.
int step(app::FlakAiGun& ag, std::vector<drone::DroneState>& fleet) {
    for (drone::DroneState& d : fleet) {
        d.prev = d.curr;
        d.curr.position += d.curr.velocity * kDt;
    }
    app::flak_ai_tick(ag.ai, ag.fk, ag.faction, fleet, kDt, kG);
    return app::flak_tick(ag.fk, false, kDt, kG, kR);
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. ACQUIRE, LEAD, FIRE.
// ---------------------------------------------------------------------------
TEST_CASE("flak AI: acquires an opposing raider, leads it, and fires",
          "[flak_ai]") {
    // The gun defends the VALLEY pump, so it shoots SUDBURY aeroplanes.
    app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
    const render::flak::MountFrame& mf = ag.fk.mount;
    // 700 m out along the threat bearing, 250 m up, crossing the gun's front
    // at 120 m/s -- inside the 1000 m raid orbit and well inside the 1500 m
    // acquire, and CROSSING so the lead is a real angle, not a rounding
    // error. (A head-on target would let a pure-pursuit bug pass.)
    std::vector<drone::DroneState> fleet{
        raider(combat::CQ_SUDBURY, mf.pos + mf.fwd0 * 700.0 + mf.up * 250.0,
               mf.right0 * 120.0)};

    int spawned = 0;
    for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);

    CHECK(ag.ai.target == 0);        // acquired, and held (the sticky latch)
    CHECK(spawned > 0);              // rounds actually left the barrel
    CHECK(ag.fk.spawned_total == spawned);

    // ★ THE BORE IS AHEAD OF THE TARGET, NOT ON IT. Compare the bore against
    // the raw line of sight and against the SOLVED lead: a gun that merely
    // pointed AT the aeroplane would score higher on the first, and this is
    // the assertion pure pursuit cannot pass.
    const glm::dvec3 bore =
        render::flak::bore_dir_world(mf, ag.fk.pose);
    const glm::dvec3 los =
        glm::normalize(fleet[0].curr.position - mf.pos);
    const render::LeadSolution sol = render::lead_solution(
        ag.fk.shooter, fleet[0].curr, render::flak::kMuzzleSpeedMps, 1500.0,
        -kG * mf.up, 0.0008, kDt);
    REQUIRE(sol.valid);
    CHECK(glm::dot(bore, sol.lead_dir) > glm::dot(bore, los));
    CHECK(glm::dot(bore, sol.lead_dir) > std::cos(2.0 * kDeg));
    // and the lead really is off the LOS at this crossing rate (the liveness
    // arm for the comparison above -- if the two directions coincided, the
    // assertion would be vacuous)
    CHECK(glm::dot(sol.lead_dir, los) < std::cos(3.0 * kDeg));

    // The BURST rhythm: he does not hold the trigger down for ten seconds.
    // Over 5 s at 7.5 Hz a continuous gunner spends ~37 rounds; bursts of
    // 8-15 with 0.5-1.0 s pauses spend materially fewer.
    CHECK(spawned < 37);
    CHECK(spawned >= 8);  // ...and he is not merely stuttering: a burst ran

    // The DRUM is his, and it drains: rounds_left tracks what he fired
    // (modulo any reload that completed).
    CHECK(ag.fk.rounds_left <= render::flak::kDrumRounds);
    CHECK(ag.fk.rounds_left >= 0);
}

TEST_CASE("flak AI: the drum runs dry and the gunner reloads", "[flak_ai]") {
    app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
    const render::flak::MountFrame& mf = ag.fk.mount;
    std::vector<drone::DroneState> fleet{
        raider(combat::CQ_SUDBURY, mf.pos + mf.fwd0 * 600.0 + mf.up * 200.0,
               mf.right0 * 40.0)};
    bool saw_reload = false;
    int refilled_to = 0;  // rounds on the drum the tick a reload COMPLETED
    for (int t = 0; t < 120 * 40; ++t) {
        const double prev_reload = ag.fk.reload_left_s;
        step(ag, fleet);
        if (ag.fk.reload_left_s > 0.0) saw_reload = true;
        // The EDGE, not the level: he starts spending the fresh drum on the
        // very next tick, so "rounds_left == 60" is true for at most one.
        if (prev_reload > 0.0 && ag.fk.reload_left_s <= 0.0)
            refilled_to = std::max(refilled_to, ag.fk.rounds_left);
    }
    CHECK(saw_reload);  // he emptied a 60-round drum
    CHECK(refilled_to >= render::flak::kDrumRounds - 1);  // a fresh one seated
    CHECK(ag.fk.spawned_total > render::flak::kDrumRounds);
}

// ---------------------------------------------------------------------------
// 2. NO TARGET / WRONG SIDE / OUT OF RANGE -> NOT A ROUND.
// ---------------------------------------------------------------------------
TEST_CASE("flak AI: no fire without a valid opposing target in range",
          "[flak_ai]") {
    const glm::dvec3 near_pos =
        ai_gun(combat::CQ_VALLEY).fk.mount.pos +
        ai_gun(combat::CQ_VALLEY).fk.mount.fwd0 * 600.0 +
        ai_gun(combat::CQ_VALLEY).fk.mount.up * 200.0;

    // (a) EMPTY SKY
    {
        app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
        std::vector<drone::DroneState> fleet;
        int spawned = 0;
        for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);
        CHECK(spawned == 0);
        CHECK(ag.ai.target == -1);
        CHECK_FALSE(ag.fk.fire_held);
        CHECK(ag.fk.rounds_left == render::flak::kDrumRounds);
    }
    // (b) SAME SIDE, right on top of him. ★ THE FACTION PIN: the geometry is
    // identical to the live case, so only the SIDE can explain the silence.
    {
        app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
        std::vector<drone::DroneState> fleet{
            raider(combat::CQ_VALLEY, near_pos, glm::dvec3(0.0))};
        int spawned = 0;
        for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);
        CHECK(spawned == 0);
        CHECK(ag.ai.target == -1);
        CHECK_FALSE(ag.fk.fire_held);
    }
    // (b') THE LIVENESS ARM for (b): flip ONLY the faction of the very same
    // aeroplane at the very same place and the gun opens up. Without this the
    // silence above could be a gun that never worked at all.
    {
        app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
        std::vector<drone::DroneState> fleet{
            raider(combat::CQ_SUDBURY, near_pos, glm::dvec3(0.0))};
        int spawned = 0;
        for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);
        CHECK(spawned > 0);
        CHECK(ag.ai.target == 0);
    }
    // (b'') ...and the MIRROR faction: a SUDBURY-pump gun shoots the VALLEY
    // aeroplane and ignores the Sudbury one. The rule is opposition, not a
    // hard-coded side.
    {
        app::FlakAiGun ag = ai_gun(combat::CQ_SUDBURY);
        std::vector<drone::DroneState> f1{
            raider(combat::CQ_SUDBURY, near_pos, glm::dvec3(0.0))};
        int s1 = 0;
        for (int t = 0; t < 600; ++t) s1 += step(ag, f1);
        CHECK(s1 == 0);
        app::FlakAiGun ag2 = ai_gun(combat::CQ_SUDBURY);
        std::vector<drone::DroneState> f2{
            raider(combat::CQ_VALLEY, near_pos, glm::dvec3(0.0))};
        int s2 = 0;
        for (int t = 0; t < 600; ++t) s2 += step(ag2, f2);
        CHECK(s2 > 0);
    }
    // (c) OUT OF RANGE: an opposing raider 4 km off is not his business
    // (acquire is 1500 m).
    {
        app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
        std::vector<drone::DroneState> fleet{raider(
            combat::CQ_SUDBURY,
            ag.fk.mount.pos + ag.fk.mount.fwd0 * 4000.0 + ag.fk.mount.up * 800.0,
            glm::dvec3(0.0))};
        int spawned = 0;
        for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);
        CHECK(spawned == 0);
        CHECK(ag.ai.target == -1);
    }
    // (d) A DEAD WRECK is not a target: `inert` is the conquest no-respawn
    // marker and the sky is full of them late in a match.
    {
        app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
        std::vector<drone::DroneState> fleet{
            raider(combat::CQ_SUDBURY, near_pos, glm::dvec3(0.0))};
        fleet[0].inert = true;
        int spawned = 0;
        for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);
        CHECK(spawned == 0);
        CHECK(ag.ai.target == -1);
    }
}

TEST_CASE("flak AI: the sticky target holds past acquire range and drops "
          "beyond the hold band",
          "[flak_ai]") {
    app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
    const render::flak::MountFrame mf = ag.fk.mount;
    const app::FlakAiParams pp;
    std::vector<drone::DroneState> drones{
        raider(combat::CQ_SUDBURY, mf.pos + mf.fwd0 * 1400.0, glm::dvec3(0.0))};
    // inside acquire -> taken
    CHECK(app::flak_ai_select_target(drones, mf.pos, combat::CQ_VALLEY, -1,
                                     pp) == 0);
    // just outside acquire but inside 1.15x -> KEPT if already held, and NOT
    // acquired fresh. (The anti-chatter latch, and the pair of assertions is
    // what makes it a latch rather than a wider range.)
    drones[0].curr.position = mf.pos + mf.fwd0 * (pp.engage_m * 1.10);
    CHECK(app::flak_ai_select_target(drones, mf.pos, combat::CQ_VALLEY, 0,
                                     pp) == 0);
    CHECK(app::flak_ai_select_target(drones, mf.pos, combat::CQ_VALLEY, -1,
                                     pp) == -1);
    // past the hold band -> dropped even when held
    drones[0].curr.position = mf.pos + mf.fwd0 * (pp.engage_m * 1.30);
    CHECK(app::flak_ai_select_target(drones, mf.pos, combat::CQ_VALLEY, 0,
                                     pp) == -1);
}

// ---------------------------------------------------------------------------
// 3. THE DOUBLE-DRIVE GUARD.
// ---------------------------------------------------------------------------
TEST_CASE("flak AI: the gun the player mans is never AI-driven, and the "
          "release re-arms without a snap",
          "[flak_ai]") {
    app::FlakAiGun ag = ai_gun(combat::CQ_VALLEY);
    const render::flak::MountFrame mf = ag.fk.mount;
    std::vector<drone::DroneState> fleet{
        raider(combat::CQ_SUDBURY, mf.pos + mf.fwd0 * 700.0 + mf.up * 250.0,
               mf.right0 * 120.0)};

    // The player takes it (main sets manned = false on the AI's world) and
    // aims it HIS way -- a pose the AI would never choose.
    ag.fk.manned = false;
    ag.fk.demand.train_rad = 2.1;
    ag.fk.demand.elev_rad = 0.9;
    ag.fk.pose = ag.fk.demand;
    ag.fk.fire_held = false;
    const render::flak::Pose held_demand = ag.fk.demand;
    const render::flak::Pose held_pose = ag.fk.pose;

    int spawned = 0;
    for (int t = 0; t < 600; ++t) spawned += step(ag, fleet);
    // NOT ONE WRITE: no demand, no pose, no trigger, no round.
    CHECK(spawned == 0);
    CHECK(ag.fk.spawned_total == 0);
    CHECK(ag.fk.demand.train_rad == held_demand.train_rad);
    CHECK(ag.fk.demand.elev_rad == held_demand.elev_rad);
    CHECK(ag.fk.pose.train_rad == held_pose.train_rad);
    CHECK(ag.fk.pose.elev_rad == held_pose.elev_rad);
    CHECK_FALSE(ag.fk.fire_held);
    CHECK(ag.ai.target == -1);

    // The player walks away. The AI re-arms FROM WHERE THE BARREL IS -- the
    // first tick after release must not teleport the pose (the gun has mass;
    // only the DEMAND may jump).
    ag.fk.manned = true;
    step(ag, fleet);
    const render::flak::SlewRates r;
    CHECK(std::abs(render::flak::wrap_pi(ag.fk.pose.train_rad -
                                         held_pose.train_rad)) <=
          r.train_rad_s * kDt + 1e-12);
    CHECK(std::abs(ag.fk.pose.elev_rad - held_pose.elev_rad) <=
          r.elev_rad_s * kDt + 1e-12);
    // ...and it IS re-armed: he re-acquires and eventually fires again.
    int after = 0;
    for (int t = 0; t < 600; ++t) after += step(ag, fleet);
    CHECK(after > 0);
}

// ---------------------------------------------------------------------------
// 4. THE FIREWALL: the arm absent == the arm empty.
// ---------------------------------------------------------------------------
namespace {
const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

app::LoopState flying_state() {
    sim::SimState s;
    s.position = glm::dvec3(kAp.R + 2000.0, 0.0, 0.0);
    s.velocity = glm::dvec3(0.0, 0.0, -90.0);
    s.orientation = glm::dquat{1, 0, 0, 0};
    app::LoopState st;
    st.curr = s;
    st.prev = s;
    st.prev_up = sim::local_up(s.position);
    st.aim.reseed(s.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    return st;
}
}  // namespace

TEST_CASE("flak AI: the absent arm and an EMPTY arm are bit-identical",
          "[flak_ai]") {
    app::LoopState a = flying_state();
    app::LoopState b = flying_state();
    std::vector<app::FlakAiGun> none;  // present but empty
    app::TickInput in;
    in.raw_mode = false;
    in.throttle = 0.7;
    for (int t = 0; t < 240; ++t) {
        app::tick(a, in, kAp, kCp, nullptr);
        app::tick(b, in, kAp, kCp, nullptr, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, &none);
    }
    CHECK(a.curr.position == b.curr.position);
    CHECK(a.curr.velocity == b.curr.velocity);
    CHECK(a.curr.angular_vel == b.curr.angular_vel);
    CHECK(a.curr.orientation.w == b.curr.orientation.w);
    CHECK(a.tick_count == b.tick_count);
    // the liveness arm: the flight was not a frozen no-op
    CHECK(a.curr.position != flying_state().curr.position);
}

// ---------------------------------------------------------------------------
// 5. KILL CREDIT.
// ---------------------------------------------------------------------------
namespace {
const sim::AircraftParams kCombatAp = [] {
    sim::AircraftParams ap;
    ap.R = 6371000.0;
    return ap;
}();

drone::DroneParams kill_dp() {
    drone::DroneParams dp;
    dp.hp = 8.0;  // one oblique 20 mm hit (~9 HP here) is lethal
    dp.hit_radius_m = 15.0;
    return dp;
}

weapon::Projectile lethal_round(const glm::dvec3& prev_pos,
                                const glm::dvec3& pos) {
    weapon::Projectile p;
    p.prev_pos = prev_pos;
    p.pos = pos;
    p.vel = pos - prev_pos;
    p.age = 0.01;
    p.drag_k = 0.0;
    p.damage = 30.0;
    p.v_ref = glm::length(p.vel);
    p.kind = weapon::Round::Cannon20mm;
    p.active = true;
    return p;
}

drone::DroneState kill_target(const glm::dvec3& pos, int spawn_index) {
    drone::DroneState d;
    d.spawn_index = spawn_index;
    sim::SimState s;
    s.position = pos;
    s.orientation = glm::dquat{1, 0, 0, 0};
    d.curr = s;
    d.prev = s;
    d.grounded = false;
    d.hp = 8.0;
    return d;
}
}  // namespace

TEST_CASE("flak AI: an AI gun's kill never lands on the player's ledger",
          "[flak_ai]") {
    const drone::DroneParams dp = kill_dp();
    const glm::dvec3 tgt{6371000.0 + 2000.0, 0.0, 0.0};
    // 5 m off centre: well inside the 15 m body, and NOT dead through the
    // middle -- a perfectly central hit leaves the impact normal degenerate
    // and ke_damage's obliquity term has nothing to read.
    const glm::dvec3 a = tgt + glm::dvec3{0.0, 5.0, 40.0};
    const glm::dvec3 b = tgt + glm::dvec3{0.0, 5.0, -40.0};

    // -- the PLAYER's own gun (no sink): the pre-Stage-D behaviour, untouched.
    {
        std::vector<drone::DroneState> drones{kill_target(tgt, 3)};
        std::vector<weapon::Projectile> pool{lethal_round(a, b)};
        combat::CombatWorld cw;
        cw.params.hit_radius_m = 15.0;
        cw.respawn_drones = false;  // conquest: a killed maverick STAYS dead
        combat::combat_tick(pool, drones, cw, kCombatAp, dp,
                            kCombatAp.sim_dt);
        REQUIRE(drones[0].inert);
        CHECK(cw.kills == 1);
        CHECK(cw.killed_spawn_indices.size() == 1);
        CHECK(cw.killed_spawn_indices[0] == 3);
        CHECK(cw.ticks_since_kill == 0);
        CHECK(cw.hits == 1);  // the player's hit counter books
    }
    // -- the AI GUN (sink): the SAME geometry, the SAME kill, and the
    //    player's ledger does not move a digit.
    {
        std::vector<drone::DroneState> drones{kill_target(tgt, 3)};
        std::vector<weapon::Projectile> pool{lethal_round(a, b)};
        combat::CombatWorld cw;
        cw.params.hit_radius_m = 15.0;
        cw.respawn_drones = false;
        // Something the PLAYER killed earlier in this same tick, which the
        // conquest wiring has not consumed yet. An AI sweep must not erase it
        // either (the sink does not clear that list at all).
        cw.killed_spawn_indices.push_back(9);
        cw.kills = 4;
        cw.ticks_since_kill = 11;
        std::vector<int> sink;
        combat::combat_tick(pool, drones, cw, kCombatAp, dp, kCombatAp.sim_dt,
                            4.0, /*burst_fx=*/true, &sink);
        REQUIRE(drones[0].inert);          // the AI gun DID kill it
        CHECK(sink.size() == 1);           // ...and the index went to the sink
        CHECK(sink[0] == 3);
        CHECK(cw.kills == 4);              // the HUD kill counter: unmoved
        CHECK(cw.ticks_since_kill == 11);  // the KILL flash: not restarted
        CHECK(cw.hits == 0);  // sink-gated too: an AI gun's hit must not
                              // flash the player's hit marker or hit sound
                              // (Chad's 2026-08-29 hit-indication wiring)
        REQUIRE(cw.killed_spawn_indices.size() == 1);
        CHECK(cw.killed_spawn_indices[0] == 9);  // the player's list: intact
        // The explosion still happens -- the aeroplane visibly dies.
        int booms = 0;
        for (const combat::Fx& f : cw.fx.pool)
            if (f.active && f.kind == combat::FxKind::Explosion) ++booms;
        CHECK(booms == 1);
    }
    // -- and the ROUTING: on_ai_kill takes the roster down and pays NOTHING.
    {
        combat::ConquestState cs;
        cs.player_faction = combat::CQ_VALLEY;
        const int score_before = cs.score[combat::CQ_VALLEY];
        REQUIRE(cs.mav_alive[3]);
        combat::on_ai_kill(cs, 3);
        CHECK_FALSE(cs.mav_alive[3]);
        CHECK(cs.score[combat::CQ_VALLEY] == score_before);
        CHECK(cs.score[combat::CQ_SUDBURY] == 0);
        // the liveness arm: the PLAYER's hook on the same index DOES pay.
        combat::ConquestState cs2;
        cs2.player_faction = combat::CQ_VALLEY;
        combat::ConquestParams cp;
        combat::on_player_kill(cs2, 3, cp);
        CHECK(cs2.score[combat::CQ_VALLEY] > score_before);
    }
}

TEST_CASE("player flak sweep APPENDS kills -- it must not clobber the "
          "aircraft cannon's booked kills in the same tick",
          "[flak_ai]") {
    // THE REGRESSION Stage D's audit surfaced: the manned flak's combat_tick
    // ran with the DEFAULT clear after the aircraft gun's sweep, erasing the
    // cannon's killed_spawn_indices before the conquest consumer read them --
    // aircraft-cannon kills paid no conquest score from F-FIRE (722957f83)
    // until clear_kills=false. This pins the append arm both ways.
    const drone::DroneParams dp = kill_dp();
    const glm::dvec3 tgt{6371000.0 + 2000.0, 0.0, 0.0};
    const glm::dvec3 a = tgt + glm::dvec3{0.0, 5.0, 40.0};
    const glm::dvec3 b = tgt + glm::dvec3{0.0, 5.0, -40.0};

    // The cannon's sweep booked spawn 9 earlier this tick...
    std::vector<drone::DroneState> drones{kill_target(tgt, 3)};
    std::vector<weapon::Projectile> pool{lethal_round(a, b)};
    combat::CombatWorld cw;
    cw.params.hit_radius_m = 15.0;
    cw.respawn_drones = false;
    cw.killed_spawn_indices.push_back(9);
    cw.kills = 1;

    // ...then the manned flak's sweep kills spawn 3 with clear_kills=false:
    // BOTH indices must reach the consumer, and the flak kill still counts.
    combat::combat_tick(pool, drones, cw, kCombatAp, dp, kCombatAp.sim_dt,
                        4.0, /*burst_fx=*/true, /*kill_sink=*/nullptr,
                        /*clear_kills=*/false);
    REQUIRE(drones[0].inert);
    REQUIRE(cw.killed_spawn_indices.size() == 2);
    CHECK(cw.killed_spawn_indices[0] == 9);  // the cannon's kill survives
    CHECK(cw.killed_spawn_indices[1] == 3);  // the flak's kill appended
    CHECK(cw.kills == 2);                    // both on the HUD counter

    // The liveness arm: the DEFAULT (a tick's first sweep) still clears.
    std::vector<drone::DroneState> drones2{kill_target(tgt, 3)};
    std::vector<weapon::Projectile> pool2{lethal_round(a, b)};
    combat::CombatWorld cw2;
    cw2.params.hit_radius_m = 15.0;
    cw2.respawn_drones = false;
    cw2.killed_spawn_indices.push_back(9);
    combat::combat_tick(pool2, drones2, cw2, kCombatAp, dp, kCombatAp.sim_dt);
    REQUIRE(cw2.killed_spawn_indices.size() == 1);
    CHECK(cw2.killed_spawn_indices[0] == 3);  // 9 cleared, only this tick's
}

TEST_CASE("player flak PROX-BURST damaging hit books cw.hits (the hit-X "
          "feed) without a kill",
          "[flak_ai]") {
    // Bug-B certification pin (Chad 2026-08-29 verdict round 2: "the hit X
    // never appears"): the full headless half of the X chain is the player
    // flak sweep's EXACT signature (prox fuze radius, burst_fx, NO sink,
    // clear_kills=false -- instructor_tick (6a-flak)) booking ++cw.hits for
    // a DAMAGING proximity burst that kills nothing. main.cpp's edge
    // detector (cw.hits > flak_prev_hits while manned) and draw.cpp's X are
    // screenshot-certified (renders/hitmark_hit_226.png / _kill_386.png);
    // this leg pins the counter feed so a future regression is headless.
    drone::DroneParams dp = kill_dp();
    dp.hp = 100.0;  // survives an oblique ~9 HP graze: hit, not kill
    const glm::dvec3 tgt{6371000.0 + 2000.0, 0.0, 0.0};
    // 17 m off centre: OUTSIDE the 15 m body, INSIDE 15 + 4 m prox -- only
    // the proximity fuze can register this pass.
    const glm::dvec3 a = tgt + glm::dvec3{0.0, 17.0, 40.0};
    const glm::dvec3 b = tgt + glm::dvec3{0.0, 17.0, -40.0};

    std::vector<drone::DroneState> drones{kill_target(tgt, 3)};
    drones[0].hp = dp.hp;
    std::vector<weapon::Projectile> pool{lethal_round(a, b)};
    combat::CombatWorld cw;
    cw.params.hit_radius_m = 15.0;
    cw.respawn_drones = false;
    // A kill the aircraft cannon booked earlier this same tick: the flak
    // sweep's clear_kills=false arm must leave it for the conquest consumer.
    cw.killed_spawn_indices.push_back(9);

    combat::combat_tick(pool, drones, cw, kCombatAp, dp, kCombatAp.sim_dt,
                        4.0, /*burst_fx=*/true, /*kill_sink=*/nullptr,
                        /*clear_kills=*/false);

    CHECK_FALSE(drones[0].inert);      // alive: this is a HIT, not a kill
    CHECK(drones[0].hp < dp.hp);       // ...and it DID damage
    CHECK(cw.hits == 1);               // the white-X feed books exactly once
    CHECK(cw.kills == 0);              // the red-X feed does not
    CHECK_FALSE(pool[0].active);       // the prox shell burst
    REQUIRE(cw.killed_spawn_indices.size() == 1);
    CHECK(cw.killed_spawn_indices[0] == 9);  // cannon's kill left intact

    // Ask 3 (same verdict round): the DAMAGING burst's FlakPuff is tagged
    // kFlakPuffVariantHit so draw.cpp renders it as the bigger hit BLAST
    // (kFlakHit* dials, combat/fx_curves.h) -- the harmless self-destruct
    // puff (spawned variant-0 by instructor_tick's retire path) keeps the
    // curtain look. Visual-only: the tag is written after damage is booked.
    {
        int puffs = 0, hit_variant = 0;
        for (const combat::Fx& f : cw.fx.pool) {
            if (!f.active || f.kind != combat::FxKind::FlakPuff) continue;
            ++puffs;
            if (f.variant == combat::kFlakPuffVariantHit) ++hit_variant;
        }
        CHECK(puffs == 1);
        CHECK(hit_variant == 1);
    }

    // The control arm: the SAME pass with the fuze dialed to 0 (bare 20 mm)
    // misses clean -- no hit is booked, so the X cannot flash. Pins that the
    // booking above really is the PROXIMITY burst, not body contact.
    std::vector<drone::DroneState> drones2{kill_target(tgt, 3)};
    drones2[0].hp = dp.hp;
    std::vector<weapon::Projectile> pool2{lethal_round(a, b)};
    combat::CombatWorld cw2;
    cw2.params.hit_radius_m = 15.0;
    cw2.respawn_drones = false;
    combat::combat_tick(pool2, drones2, cw2, kCombatAp, dp, kCombatAp.sim_dt,
                        0.0, /*burst_fx=*/true, /*kill_sink=*/nullptr,
                        /*clear_kills=*/false);
    CHECK(cw2.hits == 0);
    CHECK(drones2[0].hp == dp.hp);
    CHECK(pool2[0].active);  // the round flies on
}
