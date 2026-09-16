// ★★★ L6 — THE RESPAWN LOCK, AND THE CLOCK THAT RE-POINTS
// (docs/PLAN_20260901_game_loop_millwright.md §5a, Chad's ruling R9 as revised
// 2026-09-03). Pure/headless except for the app::tick legs, which run with no
// raylib and no world. TEST_CASE names are pure ASCII (the recurring ctest
// trap: a non-ASCII name silently never runs).
//
// CHAD, 2026-09-03, VERBATIM: "no respawns once the clock has been activated.
// The clock can be paused and a bubble revived, but once the match clock is
// initiated once, even if paused, then there are no new respawns from either
// side, and win or lose is determined by elimination (team deathmatch, kill
// them all) or by destroying their remaining pumps and having one of yours
// repaired: it makes the clock be advantage for whoever has a remaining
// functional pump. So you could rebuild your own pump then destroy the
// remaining enemy pumps, which would turn the clock's remaining time against
// your enemy. Still no respawns."
//
// ★ THE CLAIMS THESE LEGS GRADE, AND THE MUTATIONS THAT KILL THEM:
//   L1  the lock is set by the CLOCK ARMING, not by a faction being pumpless
//       -- mutation: latch it in collapse_bubble_if_pumpless instead (a
//       slotless-faction fixture then latches at t=0).
//   L2  it is ONE-WAY -- mutation: clear it on a revive in repair_pump_tick.
//   L3  it is MATCH-WIDE -- mutation: gate the refusals on the victim faction
//       only (the other side keeps respawning).
//   C1  the clock is ONE POOL that RE-POINTS -- mutation: leave the target
//       welded to the faction it armed on (his rebuild-then-kill sequence then
//       does nothing).
//   C2  a re-point is NOT a re-arm -- mutation: write match_countdown_s again
//       on the re-point (a free extension, the M5 disease one rung on).
//   C3  an all-dead map KEEPS its seconds -- mutation: restore
//       `countdown_s = 0.0` in null_countdown_if_all_pumps_dead (the repaired
//       pump then has no time left to turn against anybody).
//   V1  the VICTORY-BY-WIPE VETO reads the lock -- mutation: drop the
//       `respawn_locked` line from faction_can_reinforce, so a locked match
//       with a repaired dome still vetoes the wipe on waves that can never
//       come, and "kill them all" resolves nothing.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "app/conquest_tape.h"
#include "app/instructor_tick.h"
#include "app/loop.h"
#include "combat/conquest.h"
#include "combat/pump_repair.h"
#include "combat/reinforce.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "drone/drone.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr int kVal = combat::CQ_VALLEY;
constexpr int kSud = combat::CQ_SUDBURY;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

// The shipped pump layout: index == faction for the two SURFACE pumps, the two
// deep ones behind them. Every pump full and standing.
combat::ConquestState four_pumps(double max_hp = 200.0) {
    combat::ConquestState cs;
    cs.player_faction = kVal;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].faction = (i % 2 == 0) ? kVal : kSud;
        cs.pumps[i].surface = (i < 2);
        cs.pumps[i].max_hp = max_hp;
        cs.pumps[i].hp = max_hp;
        cs.pumps[i].alive = true;
        cs.pumps[i].pos = glm::dvec3{1000.0 * (i + 1), 0.0, 0.0};
    }
    return cs;
}

void kill_pump(combat::ConquestState& cs, int idx, int destroyer,
               const combat::ConquestParams& p) {
    REQUIRE(combat::damage_pump(cs, idx, cs.pumps[idx].max_hp + 1.0, destroyer,
                                p));
    REQUIRE_FALSE(cs.pumps[idx].alive);
}

void repair_to_full(combat::ConquestState& cs, int idx, double dt,
                    const combat::RepairParams& rp) {
    for (int n = 0; n < 1000000 && cs.pumps[idx].hp < cs.pumps[idx].max_hp; ++n)
        combat::repair_pump_tick(cs, idx, dt, rp);
    REQUIRE(cs.pumps[idx].alive);
}

// ---- tape helpers (test_conquest_tape.cpp's discipline: locate a line by its
// tag, never by index).
std::vector<std::string> split_lines(const std::string& body) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= body.size()) {
        const std::size_t nl = body.find('\n', start);
        if (nl == std::string::npos) break;
        out.push_back(body.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

int count_lines_with_tag(const std::string& body, const std::string& tag) {
    const std::string marker = "\"t\":\"" + tag + "\"";
    int n = 0;
    for (const std::string& line : split_lines(body))
        if (line.find(marker) != std::string::npos) ++n;
    return n;
}

app::LoopState flying_at(const glm::dvec3& pos) {
    sim::SimState s;
    s.position = pos;
    s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    app::LoopState st;
    st.curr = s;
    st.prev = s;
    st.prev_up = sim::local_up(s.position);
    st.aim.reseed(s.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    return st;
}

// A player already BELOW the crash surface: with env == nullptr the crash
// predicate is the bare `altitude <= 0` rule, so the very next app::tick call
// resolves a death.
app::LoopState crash_primed_player() {
    app::LoopState st{};
    st.curr = st.prev = app::spawn_state(kAp);
    st.curr.position = glm::dvec3{1.0, 0.0, 0.0} * (kAp.R - 100.0);
    st.prev.position = st.curr.position;
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = false;
    return st;
}

// A drone parked 100 m below the crash sphere (test_conquest_respawn.cpp's own
// fixture): one drone::tick inside app::tick cannot climb 100 m, so it crashes
// on the tick under test.
drone::DroneState crash_primed_drone(const drone::DroneParams& dp,
                                     int spawn_index, int fleet_count) {
    const glm::dvec3 pos = glm::dvec3{1.0, 0.0, 0.0} * (kAp.R - 100.0);
    drone::DroneState d;
    d.curr = drone::level_state_at(dp, pos, glm::dvec3{0.0, 1.0, 0.0});
    d.prev = d.curr;
    d.spawn_index = spawn_index;
    d.fleet_count = fleet_count;
    d.hp = dp.hp;
    d.grounded = false;
    return d;
}

// A conquest state with the clock ARMED on `victim` -- i.e. respawns locked --
// built the only way the game can build it: by killing that faction's pumps.
combat::ConquestState clocked_on(int victim, const combat::ConquestParams& p) {
    combat::ConquestState cs = four_pumps();
    kill_pump(cs, victim, 1 - victim, p);
    kill_pump(cs, victim + 2, 1 - victim, p);
    REQUIRE(cs.countdown_faction == victim);
    REQUIRE(cs.respawn_locked);
    return cs;
}

// A wave policy that ALWAYS accepts, so the only thing that can refuse a
// deploy is the machinery under test.
struct AlwaysDeploy : combat::ReinforcePolicy {
    int deploys = 0;
    bool deploy(drone::DroneState& d, int spawn_index) override {
        (void)d;
        (void)spawn_index;
        ++deploys;
        return true;
    }
};

bool wave_deployed(combat::ConquestState& cs, int spawn_index) {
    combat::ReinforceParams rp;
    rp.delay_s = 1.0;
    rp.hp_full = 100.0;
    rp.pool_n = -1;  // infinite: the pool can never be the refusal
    combat::ReinforceState rs;
    std::vector<drone::DroneState> fleet(1);
    fleet[0].spawn_index = spawn_index;
    fleet[0].fleet_count = 1;
    fleet[0].inert = true;  // a wreck, waiting
    AlwaysDeploy pol;
    for (int t = 0; t < 4; ++t)
        combat::reinforce_tick(rs, fleet, cs, rp, pol, 1.0);
    return pol.deploys > 0;
}

}  // namespace

// ===========================================================================
// 1. THE LATCH. It is the CLOCK ARMING that locks respawns -- not the first
//    loss, not a faction being pumpless in the abstract.
// ===========================================================================
TEST_CASE("respawn lock: latches when the clock arms, and not one loss before") {
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();
    REQUIRE_FALSE(combat::respawns_locked(cs));

    // ONE pump lost. The dome halves, the clock does not arm, nobody loses a
    // life pool.
    kill_pump(cs, kVal, kSud, params);
    CHECK(cs.countdown_faction == -1);
    CHECK_FALSE(combat::respawns_locked(cs));

    // THE SECOND. The clock arms -- and that is the trigger.
    kill_pump(cs, kVal + 2, kSud, params);
    REQUIRE(cs.countdown_faction == kVal);
    CHECK(combat::respawns_locked(cs));
}

TEST_CASE("respawn lock: it is the ARM that latches, not the collapse") {
    // ★★★ MUTATION L1, AND WHY IT NEEDS ITS OWN LEG. With the shipped layout
    // "the faction went pumpless" and "the clock armed" are the SAME EVENT, so
    // latching the lock inside collapse_bubble_if_pumpless passes every
    // sequence test above -- it survived, measured, until this case existed.
    // The two are only separable on a state that is pumpless WITHOUT the arm
    // having run, which is exactly the shape test_conquest.cpp's E11 leg builds
    // (no living pump, a dome scale still positive from its own kills).
    combat::ConquestParams params;
    combat::ConquestState cs;
    cs.player_faction = kSud;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].faction = (i % 2 == 0) ? kSud : kVal;
        cs.pumps[i].alive = (i % 2 == 0);  // VALLEY owns nothing living
        cs.pumps[i].hp = 100.0;
        cs.pumps[i].max_hp = 100.0;
    }
    REQUIRE(combat::faction_owns_pump_slot(cs, kVal));
    REQUIRE_FALSE(combat::faction_has_pump(cs, kVal));

    // The dome collapses -- and NOTHING about respawns changes. No clock has
    // been started, so nobody has lost a life pool.
    combat::collapse_bubble_if_pumpless(cs);
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.0));
    CHECK_FALSE(combat::respawns_locked(cs));

    // THE ARM is the trigger, and only the arm.
    combat::arm_countdown_if_bubble_lost(cs, kVal, params);
    REQUIRE(cs.countdown_faction == kVal);
    CHECK(combat::respawns_locked(cs));
}

TEST_CASE("respawn lock: a faction with no pump SLOTS never latches") {
    // ★ Single-pump fixtures leave one faction out of the pump economy
    // entirely, and "it has lost every pump it owns" is vacuously true of it.
    // The lock rides on arm_countdown_if_bubble_lost, which carries the
    // slotless guard, so the exemption is INHERITED rather than re-derived --
    // this leg is what says so.
    combat::ConquestParams params;
    combat::ConquestState cs;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].faction = kVal;  // SUDBURY owns nothing at all
        cs.pumps[i].alive = true;
        cs.pumps[i].hp = 100.0;
        cs.pumps[i].max_hp = 100.0;
    }
    REQUIRE_FALSE(combat::faction_owns_pump_slot(cs, kSud));
    REQUIRE_FALSE(combat::faction_has_pump(cs, kSud));

    // Ask the arm directly for the slotless side, the way a tick would.
    combat::arm_countdown_if_bubble_lost(cs, kSud, params);
    CHECK(cs.countdown_faction == -1);
    CHECK_FALSE(combat::respawns_locked(cs));

    // ...and a tick of the clock does not invent one either.
    combat::conquest_countdown_tick(cs, 10000.0, params);
    CHECK_FALSE(combat::respawns_locked(cs));
    CHECK(cs.outcome == combat::Outcome::PLAYING);
}

TEST_CASE("respawn lock: a repair holds the clock and does NOT unlock") {
    // ★★★ THE HEART OF R9. "The clock can be paused and a bubble revived, but
    // once the match clock is initiated once, EVEN IF PAUSED, then there are no
    // new respawns from either side."
    // MUTATION L2: clearing cs.respawn_locked on the revive in
    // repair_pump_tick turns the last CHECK red.
    combat::ConquestParams params;
    combat::RepairParams rp;
    combat::ConquestState cs = clocked_on(kVal, params);

    combat::conquest_countdown_tick(cs, 120.0, params);
    const double left = cs.countdown_s;
    REQUIRE(left == Catch::Approx(params.match_countdown_s - 120.0));

    repair_to_full(cs, kVal, 1.0, rp);
    // The bubble is back and the clock is HELD -- both R7 and R8, unchanged.
    CHECK(cs.countdown_paused);
    CHECK(cs.radius_scale[kVal] > 0.0);
    CHECK(cs.countdown_s == Catch::Approx(left));
    // ...and the lock is exactly where it was.
    CHECK(combat::respawns_locked(cs));
}

// ===========================================================================
// 2. THE REFUSALS. Both sides, one law.
// ===========================================================================
TEST_CASE("respawn lock: a wave is refused for BOTH factions once locked") {
    // MUTATION L3: gate the refusal on the pumpless faction and the other
    // side's wave comes back.
    combat::ConquestParams params;

    // Unlocked control FIRST, so the leg cannot pass by refusing everything.
    {
        combat::ConquestState cs = four_pumps();
        CHECK(wave_deployed(cs, 0));  // spawn_index 0 -> SUDBURY
        CHECK(wave_deployed(cs, 1));  // spawn_index 1 -> VALLEY
    }

    combat::ConquestState cs = clocked_on(kVal, params);
    CHECK_FALSE(wave_deployed(cs, 0));
    CHECK_FALSE(wave_deployed(cs, 1));

    // And a repair does not buy the waves back either -- the reason the refusal
    // lives in reinforce_tick and not in the shipped policy, whose own "no
    // breathable air" refusal evaporates the moment R8 gives the dome back.
    combat::RepairParams rp;
    repair_to_full(cs, kVal, 1.0, rp);
    REQUIRE(cs.radius_scale[kVal] > 0.0);
    CHECK_FALSE(wave_deployed(cs, 0));
    CHECK_FALSE(wave_deployed(cs, 1));
}

TEST_CASE("respawn lock: the player gets no respawn, and the match resolves") {
    combat::ConquestParams params;

    // ---- unlocked control: the ordinary crash respawn, untouched.
    {
        app::ConquestWorld cq;
        cq.state = four_pumps();
        app::LoopState st = crash_primed_player();
        app::TickInput in;
        const app::TickResult r = app::tick(st, in, kAp, kCp, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, &cq);
        CHECK(r.respawned);
        CHECK_FALSE(r.respawn_refused);
        CHECK(sim::altitude(st.curr.position, kAp) > 0.0);
        CHECK(cq.state.planes_left == 7);  // one life spent, as ever
        CHECK(cq.state.outcome == combat::Outcome::PLAYING);
    }

    // ---- locked: no aeroplane is placed and the match ends.
    app::ConquestWorld cq;
    cq.state = clocked_on(kVal, params);
    cq.state.player_faction = kVal;
    REQUIRE(cq.state.planes_left > 0);
    app::LoopState st = crash_primed_player();
    const glm::dvec3 where_he_died = st.curr.position;
    app::TickInput in;
    const app::TickResult r = app::tick(st, in, kAp, kCp, nullptr, nullptr,
                                        nullptr, nullptr, nullptr, &cq);
    CHECK(r.respawn_refused);
    CHECK_FALSE(r.respawned);  // nothing was born: no life spent, no menu
    // He is still down there -- not re-placed at spawn altitude.
    CHECK(sim::altitude(st.curr.position, kAp) <= 0.0);
    CHECK(glm::length(st.curr.position - where_he_died) < 500.0);
    // ★ RESOLVED THROUGH THE EXISTING RULE, not a new one: the pool is spent
    // and conquest_eval_outcome decides.
    CHECK(cq.state.planes_left == 0);
    CHECK(cq.state.outcome == combat::Outcome::DEFEAT);

    // ...and the next tick books nothing more: the crash predicate keeps firing
    // on a body that is never re-placed, so the refusal must be once-only.
    const app::TickResult r2 = app::tick(st, in, kAp, kCp, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, &cq);
    CHECK_FALSE(r2.respawned);
    CHECK(cq.state.outcome == combat::Outcome::DEFEAT);
}

TEST_CASE("respawn lock: a locked drone crash leaves a wreck, not a respawn") {
    combat::ConquestParams params;
    drone::DroneParams dp;
    dp.maverick.enabled = false;

    // ---- unlocked control: today's relocate-into-own-air behaviour.
    {
        app::DroneWorld dw;
        dw.dparams = dp;
        dw.drones.push_back(crash_primed_drone(dp, 0, 1));
        app::LoopState st = flying_at(glm::dvec3{kAp.R + 3000.0, 0.0, 0.0});
        app::ConquestWorld cq;
        cq.state = four_pumps();
        app::TickInput in;
        app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr,
                  &cq);
        CHECK_FALSE(dw.drones[0].inert);
        CHECK(cq.state.mav_alive[0]);
    }

    // ---- locked: he stays dead. spawn_index 0 is SUDBURY and the clock was
    // armed on VALLEY -- the lock is MATCH-WIDE, so the other side's pilot is
    // refused too (mutation L3 dies here).
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));
    app::LoopState st = flying_at(glm::dvec3{kAp.R + 3000.0, 0.0, 0.0});
    app::ConquestWorld cq;
    cq.state = clocked_on(kVal, params);
    REQUIRE(combat::maverick_faction(0) == kSud);
    REQUIRE(cq.state.mav_alive[0]);
    // The pump kills that armed the clock already moved the scoreboard, so the
    // "no credit for a crash" claim is a DELTA, not an absolute.
    const int score_before[2] = {cq.state.score[0], cq.state.score[1]};
    app::TickInput in;
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr, &cq);
    CHECK(dw.drones[0].inert);  // a frozen wreck: no fly, no fire, no draw
    CHECK_FALSE(cq.state.mav_alive[0]);  // ...and off the roster, which is what
                                         // victory-by-elimination reads
    CHECK(cq.state.score[0] == score_before[0]);  // nobody is credited for a
    CHECK(cq.state.score[1] == score_before[1]);  // terrain crash
}

TEST_CASE("respawn lock: elimination resolves at once -- no wave veto stalemate") {
    // ★★★ THE HOLE THIS CLOSES. Chad: "win or lose is determined by
    // ELIMINATION (team deathmatch, kill them all)". But
    // `ConquestState::reinforcements_live` VETOES the victory-by-wipe route,
    // and it is stamped from app::enemy_waves_live, which asked two questions:
    // does the enemy have pooled revives, and is its dome breathable. Both come
    // back TRUE the moment an R8 repair hands a dome back -- so a locked match
    // could reach "every enemy maverick dead" and latch nothing, waiting out a
    // clock on waves that can never come. That is the tape-3 defect
    // faction_can_reinforce was written to end, one rung later and with a new
    // reason, so the fix is in the same place: the lock is the FIRST thing that
    // predicate answers, and its two readers cannot disagree.
    //
    // MUTATION V1: delete the `respawn_locked` line from
    // faction_can_reinforce. Every CHECK below that mentions the veto goes red.
    combat::ConquestParams params;
    combat::RepairParams rp;
    app::ConquestWorld cq;
    cq.state = four_pumps();
    cq.state.player_faction = kVal;
    // WAVES FULLY ARMED, so the veto is a real force and not a vacuous one.
    cq.params.reinforce_delay_s = 75.0;
    cq.params.reinforce_pool_n = 4;
    const int enemy = 1 - cq.state.player_faction;

    // Before the clock: the veto is TRUE, exactly as it always was. (Without
    // this line the leg could pass on a veto that was never live.)
    REQUIRE(app::enemy_waves_live(cq));

    // HIS last pump dies -> clock arms, respawns lock.
    kill_pump(cq.state, kVal, kSud, params);
    kill_pump(cq.state, kVal + 2, kSud, params);
    REQUIRE(cq.state.respawn_locked);

    // ...and he REPAIRS one (R8), which is what puts the old veto back on its
    // feet: the enemy never lost a pump, so its dome was breathable throughout,
    // and its wave pool is untouched.
    repair_to_full(cq.state, kVal, 1.0, rp);
    // the OLD veto term is back on its feet...
    REQUIRE(app::faction_air_breathable(cq, enemy));
    CHECK_FALSE(app::enemy_waves_live(cq));  // ...and the lock overrules it
    CHECK_FALSE(combat::faction_can_reinforce(cq.reinforce,
                                              app::wave_params(cq, 100.0),
                                              cq.state, enemy));

    // The tick stamps the veto from that same predicate, every tick.
    cq.state.reinforcements_live = app::enemy_waves_live(cq);
    REQUIRE_FALSE(cq.state.reinforcements_live);

    // KILL THEM ALL. The wipe route is re-armed, so the last enemy maverick
    // resolves the match ON THAT TICK -- not when a clock happens to run out.
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        if (!combat::is_enemy(i, cq.state.player_faction)) continue;
        REQUIRE(cq.state.outcome == combat::Outcome::PLAYING);
        combat::on_player_kill(cq.state, i, cq.params);
    }
    CHECK(cq.state.outcome == combat::Outcome::VICTORY);
    // decided by elimination, with time still on the clock
    CHECK(cq.state.countdown_s > 0.0);
}

// ===========================================================================
// 3. THE CLOCK RE-POINTS. Chad's own sequence, step for step.
// ===========================================================================
TEST_CASE("respawn lock: the clock re-points and keeps the seconds it had") {
    // ★★★ HIS SEQUENCE, VERBATIM: "you could rebuild your own pump then destroy
    // the remaining enemy pumps, which would turn the clock's remaining time
    // against your enemy."
    combat::ConquestParams params;
    combat::RepairParams rp;
    combat::ConquestState cs = four_pumps();
    cs.player_faction = kVal;

    // 1. HIS last pump dies. The clock arms on him and the respawns lock.
    kill_pump(cs, kVal, kSud, params);
    kill_pump(cs, kVal + 2, kSud, params);
    REQUIRE(cs.countdown_faction == kVal);
    REQUIRE(cs.respawn_locked);

    // 2. It burns 400 s against him.
    combat::conquest_countdown_tick(cs, 400.0, params);
    const double left = cs.countdown_s;
    REQUIRE(left == Catch::Approx(200.0));

    // 3. HE REBUILDS ONE. Both sides now have a pump -> HELD, no target.
    repair_to_full(cs, kVal, 1.0, rp);
    CHECK(cs.countdown_paused);
    CHECK(cs.countdown_faction == -1);
    for (int t = 0; t < 300; ++t)
        combat::conquest_countdown_tick(cs, 1.0, params);
    CHECK(cs.countdown_s == Catch::Approx(left));  // held, not spent
    CHECK(cs.outcome == combat::Outcome::PLAYING);

    // 4. HE DESTROYS THEIRS. ★ THE CLOCK IS NOW THEIR PROBLEM -- with the very
    // seconds he had left. (MUTATION C2: writing match_countdown_s on the
    // re-point hands out a fresh 600 and this goes red.)
    kill_pump(cs, kSud, kVal, params);
    kill_pump(cs, kSud + 2, kVal, params);
    CHECK(cs.countdown_faction == kSud);
    CHECK_FALSE(cs.countdown_paused);
    CHECK(cs.countdown_s == Catch::Approx(left));
    CHECK(cs.countdown_s != Catch::Approx(params.match_countdown_s));

    // 5. ...and the buzzer crowns HIM.
    combat::conquest_countdown_tick(cs, left + 1.0, params);
    CHECK(cs.countdown_s == Catch::Approx(0.0));
    CHECK(cs.outcome == combat::Outcome::VICTORY);
    // Still no respawns, through all of it.
    CHECK(combat::respawns_locked(cs));
}

TEST_CASE("respawn lock: with every pump dead the clock is held, seconds kept") {
    // ★ MUTATION C3: restore `countdown_s = 0.0` in
    // null_countdown_if_all_pumps_dead and the repaired pump below has no time
    // left to turn against anybody. The half of the 2026-08-30 ruling that must
    // NOT move is the TARGET: with no pump standing anywhere, no buzzer fires.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();
    kill_pump(cs, kVal, kSud, params);
    kill_pump(cs, kVal + 2, kSud, params);
    combat::conquest_countdown_tick(cs, 100.0, params);
    const double left = cs.countdown_s;

    kill_pump(cs, kSud, kVal, params);
    kill_pump(cs, kSud + 2, kVal, params);
    CHECK(cs.deathmatch);
    CHECK(cs.countdown_faction == -1);
    CHECK(cs.countdown_paused);
    CHECK(cs.countdown_s == Catch::Approx(left));
    combat::conquest_countdown_tick(cs, 10.0 * params.match_countdown_s,
                                    params);
    CHECK(cs.outcome == combat::Outcome::PLAYING);
    CHECK(cs.countdown_s == Catch::Approx(left));

    // And a repair from THERE points the survivor's time at the other side --
    // which is the whole reason the seconds are kept.
    combat::RepairParams rp;
    repair_to_full(cs, kSud, 1.0, rp);
    CHECK(cs.countdown_faction == kVal);
    CHECK_FALSE(cs.countdown_paused);
    CHECK(cs.countdown_s == Catch::Approx(left));
}

TEST_CASE("respawn lock: a loss-only match never re-arms and never extends") {
    // The 2026-08-30 "armed once, no free extension" doctrine, re-checked
    // against the re-pointing machinery: without a repair the armed faction
    // stays pumpless, so it stays the target and the seconds only fall.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();
    kill_pump(cs, kVal, kSud, params);
    kill_pump(cs, kVal + 2, kSud, params);
    REQUIRE(cs.countdown_s == Catch::Approx(params.match_countdown_s));
    combat::conquest_countdown_tick(cs, 60.0, params);
    const double left = cs.countdown_s;
    // Ask the arm directly for the other side while it still owns pumps.
    combat::arm_countdown_if_bubble_lost(cs, kSud, params);
    CHECK(cs.countdown_faction == kVal);
    CHECK(cs.countdown_s == Catch::Approx(left));
    combat::conquest_countdown_tick(cs, 60.0, params);
    CHECK(cs.countdown_s == Catch::Approx(left - 60.0));
}

// ===========================================================================
// 4. THE TAPE. `rl` is a new tag: additive, edge-triggered, emitted once.
// ===========================================================================
TEST_CASE("respawn lock: the tape emits rl once, on the tick it latched") {
    seads_tape::ConquestTape tape("test");
    app::ConquestWorld cq;
    cq.state = four_pumps(100.0);
    app::LoopState st = flying_at(glm::dvec3{15000.0, 0.0, 0.0});
    combat::ConquestParams params;

    for (long long k = 0; k < 8; ++k) {
        if (k == 1) kill_pump(cq.state, 0, kSud, params);  // one loss: no lock
        if (k == 3) kill_pump(cq.state, 2, kSud, params);  // the arm
        if (k == 5) {  // ...and a repair, which must NOT emit a second rl
            combat::RepairParams rp;
            repair_to_full(cq.state, 0, 1.0, rp);
        }
        st.tick_count = k;
        tape.on_tick(app::TickInput{}, st, nullptr, nullptr, &cq, nullptr);
    }
    std::string out;
    REQUIRE(tape.drain(out));
    CHECK(count_lines_with_tag(out, "rl") == 1);
    CHECK(out.find("{\"t\":\"rl\",\"k\":3,") != std::string::npos);
}

TEST_CASE("respawn lock: a tape that never arms the clock carries no rl row") {
    // OLD TAPES DECODE UNCHANGED: the tag is additive and edge-triggered, so a
    // session in which nobody loses their last pump emits nothing new at all.
    seads_tape::ConquestTape tape("test");
    app::ConquestWorld cq;
    cq.state = four_pumps(100.0);
    app::LoopState st = flying_at(glm::dvec3{15000.0, 0.0, 0.0});
    combat::ConquestParams params;
    for (long long k = 0; k < 6; ++k) {
        if (k == 2) kill_pump(cq.state, 0, kSud, params);
        if (k == 4) kill_pump(cq.state, 1, kVal, params);
        st.tick_count = k;
        tape.on_tick(app::TickInput{}, st, nullptr, nullptr, &cq, nullptr);
    }
    std::string out;
    REQUIRE(tape.drain(out));
    CHECK(count_lines_with_tag(out, "rl") == 0);
    CHECK(out.find("\"t\":\"rl\"") == std::string::npos);
}
