// ★★★ L2 — REPAIR + THE CLOCK
// (docs/PLAN_20260901_game_loop_millwright.md §4.2). Pure, headless: no raylib,
// no wall clock, no rng. TEST_CASE names are pure ASCII (the 4x-recurred ctest
// trap: a non-ASCII name silently never runs here).
//
// CHAD, 2026-09-01: "fix the surface pumps to stop the game clock and revive a
// lost pump ... the fix should take about a minute to full restore." And, asked
// whether a revived pump gives the sky back: "yes the dome grows back if made
// operational again."
//
// ★ WHAT THESE LEGS ARE REALLY GRADING. The hard part of this rung is NOT the
// HP ramp -- it is that `radius_scale`/`ceiling_scale` are FLOORED,
// PATH-DEPENDENT accumulators fed by two different rewards (the destroyer grows
// per kill, the victim shrinks), so they cannot be inverted and cannot be
// derived from the alive set without deleting the growth term. Three separate
// wrong implementations reproduce the simple cases and fail here, and each one
// is named at the leg that kills it:
//   M1  derive the dome from the alive set (deletes tape 15's grown enemy dome)
//   M2  invert the shrink on revive (the floor makes it unrecoverable)
//   M3  reconstruct from a pump COUNT (drops the growth: 0.50, not 0.65)
//   M4  drop the slotless-faction guard (single-pump fixtures collapse at t=0)
//   M5  resume the held clock at match_countdown_s (a free extension)
//   M6  pay the regrow on repair COMPLETION rather than on REVIVAL

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/conquest_tape.h"
#include "app/instructor_tick.h"
#include "app/loop.h"
#include "combat/conquest.h"
#include "combat/pump_repair.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr int kVal = combat::CQ_VALLEY;
constexpr int kSud = combat::CQ_SUDBURY;

// The shipped pump layout: index == faction for the two SURFACE pumps
// (combat/raid.h:266, app/instructor_tick.h:2126), the two deep ones behind
// them. Every pump full and standing.
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

// Kill pump `idx` outright, credited to `destroyer`.
void kill_pump(combat::ConquestState& cs, int idx, int destroyer,
               const combat::ConquestParams& p) {
    REQUIRE(combat::damage_pump(cs, idx, cs.pumps[idx].max_hp + 1.0, destroyer,
                                p));
    REQUIRE_FALSE(cs.pumps[idx].alive);
}

// Turn the wrench on `idx` until it is done (or `max_ticks` runs out). Returns
// the number of ticks it took and reports whether it revived.
int repair_to_full(combat::ConquestState& cs, int idx, double dt,
                   const combat::RepairParams& rp, bool* revived_out = nullptr,
                   int max_ticks = 1000000) {
    int n = 0;
    bool rev = false;
    while (n < max_ticks && cs.pumps[idx].hp < cs.pumps[idx].max_hp) {
        const combat::PumpRepair r = combat::repair_pump_tick(cs, idx, dt, rp);
        if (r.revived) rev = true;
        ++n;
    }
    if (revived_out != nullptr) *revived_out = rev;
    return n;
}

// ---- tape helpers (test_conquest_tape.cpp's own discipline, reproduced for
// this TU: a line is located by its "t":"tag" + "k":N marker, never by index).
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

std::string find_line(const std::string& body, const std::string& tag,
                      long long k) {
    const std::string tag_marker = "\"t\":\"" + tag + "\"";
    const std::string k_marker = "\"k\":" + std::to_string(k) + ",";
    for (const std::string& line : split_lines(body)) {
        if (line.find(tag_marker) != std::string::npos &&
            line.find(k_marker) != std::string::npos)
            return line;
    }
    return {};
}

int count_lines_with_tag(const std::string& body, const std::string& tag) {
    const std::string tag_marker = "\"t\":\"" + tag + "\"";
    int n = 0;
    for (const std::string& line : split_lines(body))
        if (line.find(tag_marker) != std::string::npos) ++n;
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

}  // namespace

// ===========================================================================
// 1. THE RAMP. "about a minute to full restore" (Chad's R2), on the SIM TICK.
// ===========================================================================
TEST_CASE("pump repair: 60 s of sim ticks takes a dead pump from 0 to full") {
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps(200.0);
    kill_pump(cs, 0, kSud, params);

    combat::RepairParams rp;  // shipped defaults: full_s 60, reach 6, points 0
    REQUIRE(rp.full_s == Catch::Approx(60.0));
    const double dt = 1.0 / 120.0;

    // Half the budget: half the HP, and STILL DEAD. A pump is not back until
    // the job is finished -- that is the whole stake of the mechanic (the
    // strafing run that interrupts it).
    for (int t = 0; t < 60 * 120 / 2; ++t)
        combat::repair_pump_tick(cs, 0, dt, rp);
    CHECK(cs.pumps[0].hp == Catch::Approx(100.0).margin(1e-6));
    CHECK_FALSE(cs.pumps[0].alive);

    bool revived = false;
    const int more = repair_to_full(cs, 0, dt, rp, &revived);
    CHECK(cs.pumps[0].alive);
    CHECK(revived);
    CHECK(cs.pumps[0].hp == Catch::Approx(200.0));
    // The whole job is 60 s of sim time at 120 Hz. The budget is a RATE, not a
    // tick count, so the last tick is a partial one whenever 7200 additions of
    // max_hp/7200 land a float's breadth short -- the contract is "one tick,
    // never two", which is what pins the rate.
    CHECK(more >= 60 * 120 / 2);
    CHECK(more <= 60 * 120 / 2 + 1);

    // Idempotent afterwards: a wrench on a whole pump does nothing at all.
    const combat::PumpRepair again = combat::repair_pump_tick(cs, 0, dt, rp);
    CHECK_FALSE(again.worked);
    CHECK_FALSE(again.revived);
    CHECK(again.frac == Catch::Approx(1.0));
}

TEST_CASE("pump repair: the guards refuse a bad index, dt, dial and outcome") {
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps(200.0);
    kill_pump(cs, 0, kSud, params);
    combat::RepairParams rp;
    const double hp0 = cs.pumps[0].hp;

    CHECK_FALSE(combat::repair_pump_tick(cs, -1, 0.1, rp).worked);
    CHECK_FALSE(combat::repair_pump_tick(cs, combat::kNumPumps, 0.1, rp).worked);
    CHECK_FALSE(combat::repair_pump_tick(cs, 0, 0.0, rp).worked);
    CHECK_FALSE(combat::repair_pump_tick(cs, 0, -1.0, rp).worked);
    combat::RepairParams zero_dial;
    zero_dial.full_s = 0.0;
    CHECK_FALSE(combat::repair_pump_tick(cs, 0, 0.1, zero_dial).worked);
    CHECK(cs.pumps[0].hp == Catch::Approx(hp0));

    // A decided match is frozen, exactly like every other conquest hook.
    cs.outcome = combat::Outcome::VICTORY;
    CHECK_FALSE(combat::repair_pump_tick(cs, 0, 10.0, rp).worked);
    CHECK_FALSE(cs.pumps[0].alive);
}

TEST_CASE("pump repair: the reach law is the dial, squared, at the body") {
    combat::ConquestState cs = four_pumps();
    combat::RepairParams rp;
    rp.reach_m = 6.0;
    const glm::dvec3 p = cs.pumps[0].pos;
    // Offsets ALONG THE GROUND: a tangent at the pump, not a world axis.
    const glm::dvec3 up = glm::normalize(p);
    glm::dvec3 t = glm::cross(up, glm::dvec3{0.0, 0.0, 1.0});
    if (glm::length(t) < 1e-6) t = glm::cross(up, glm::dvec3{1.0, 0.0, 0.0});
    t = glm::normalize(t);
    CHECK(combat::pump_in_repair_reach(cs, 0, p, rp));
    CHECK(combat::pump_in_repair_reach(cs, 0, p + t * 5.9, rp));
    CHECK_FALSE(combat::pump_in_repair_reach(cs, 0, p + t * 6.1, rp));
    CHECK_FALSE(combat::pump_in_repair_reach(cs, -1, p, rp));
    // ★ 2026-09-03 THE MAST: the shipped surface pump is placed 10 m ABOVE the
    // terrain (app/main.cpp surf(): radius_at + 10.0) and the man stands ON
    // it. A straight-line law could never be met (Chad: "fix does not work
    // for the pumps"). The law is along the ground: 10 m straight below is
    // IN reach, 10 m below and 5.9 m aside is in reach, 6.1 m aside is not.
    CHECK(combat::pump_in_repair_reach(cs, 0, p - up * 10.0, rp));
    CHECK(combat::pump_in_repair_reach(cs, 0, p - up * 10.0 + t * 5.9, rp));
    CHECK_FALSE(
        combat::pump_in_repair_reach(cs, 0, p - up * 10.0 + t * 6.1, rp));
}

// ===========================================================================
// 2. A PARTIAL FIX IS NOT A FIX, AND DAMAGE STILL LANDS ON IT.
// ===========================================================================
TEST_CASE("pump repair: a partial fix then fresh damage keeps the net damage") {
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps(200.0);
    combat::RepairParams rp;
    // A DAMAGED but living pump: half its HP shot away, never killed.
    REQUIRE_FALSE(combat::damage_pump(cs, 0, 100.0, kSud, params));
    REQUIRE(cs.pumps[0].alive);
    REQUIRE(cs.pumps[0].hp == Catch::Approx(100.0));

    // 15 s of wrench = a quarter of the budget = 50 HP back.
    for (int t = 0; t < 15 * 120; ++t)
        combat::repair_pump_tick(cs, 0, 1.0 / 120.0, rp);
    CHECK(cs.pumps[0].hp == Catch::Approx(150.0).margin(1e-6));

    // Then a raider takes 120 off it. The net is what is left, not a reset.
    REQUIRE_FALSE(combat::damage_pump(cs, 0, 120.0, kSud, params));
    CHECK(cs.pumps[0].hp == Catch::Approx(30.0).margin(1e-6));
    CHECK(cs.pumps[0].alive);
    // ...and the dome never moved, because nothing ever died.
    CHECK(cs.radius_scale[kVal] == Catch::Approx(1.0));
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.0));
}

// ===========================================================================
// 3. ★★★ THE ARM THIS RUNG EXISTS FOR: DESTROYER, THEN VICTIM, THEN REVIVED.
// ===========================================================================
TEST_CASE("pump repair: a revive returns exactly what that loss took") {
    // ★ THE SHAPE THAT KILLS EVERY SHORTCUT. VALLEY kills one enemy pump first,
    // so its dome carries a GROWTH term (1.15) that no count of living pumps
    // knows about -- this is tape 15's Chad-reported 1.5x enemy dome, in
    // miniature. Then it loses both of its own: 1.15 -> 0.65 -> 0.15 banked,
    // masked to 0 by the collapse. Reviving the SECOND loss must give back
    // exactly the 0.5 that loss took: 0.65.
    //   M1 (derive from the alive set)  -> 0.5, and the growth is gone
    //   M3 (reconstruct from a count)   -> 0.50
    //   M2 (invert the shrink)          -> right here, wrong at the floor (§5)
    combat::ConquestParams params;  // shipped: growth 0.15, shrink 0.5
    REQUIRE(params.growth_radius_frac == Catch::Approx(0.15));
    REQUIRE(params.shrink_radius_frac == Catch::Approx(0.5));
    combat::ConquestState cs = four_pumps();
    combat::RepairParams rp;
    const double dt = 1.0;

    kill_pump(cs, 1, kVal, params);  // his kill: the enemy SURFACE pump
    CHECK(cs.radius_scale[kVal] == Catch::Approx(1.15));
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(1.15));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(1.15));

    kill_pump(cs, 0, kSud, params);  // his surface pump
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.65));
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.5));

    kill_pump(cs, 2, kSud, params);  // his deep pump: he is pumpless
    REQUIRE_FALSE(combat::faction_has_pump(cs, kVal));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.0));   // MASKED
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(0.0));
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.15));  // BANKED
    CHECK(cs.shrink_taken_radius[2] == Catch::Approx(0.5));

    // FIX THE DEEP ONE. ⚠ AT THE COMBAT LAYER, WHICH HAS NO OPINION ABOUT WHO
    // MAY FIX WHAT -- app/interact.h registers only an OWN SURFACE pump, so the
    // player cannot reach a deep one this rung (plan §3, §5 Q2). Exercised here
    // because the dome law must be the same law for every slot.
    // The mask lifts AND that loss's 0.5 comes back.
    bool revived = false;
    repair_to_full(cs, 2, dt, rp, &revived);
    REQUIRE(revived);
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.65));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(0.65));
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.65));
    CHECK(cs.shrink_taken_radius[2] == Catch::Approx(0.0));
    // ⚠ NOT 0.50 (a count formula) and NOT 0.15 (a bare mask lift).
    CHECK(cs.radius_scale[kVal] != Catch::Approx(0.50));
    CHECK(cs.radius_scale[kVal] != Catch::Approx(0.15));

    // FIX THE SURFACE ONE TOO: all the way back to the pre-loss banked value,
    // GROWTH INCLUDED, and not one part beyond it.
    repair_to_full(cs, 0, dt, rp, &revived);
    CHECK(cs.radius_scale[kVal] == Catch::Approx(1.15));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(1.15));
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(1.15));

    // The DESTROYER's growth was never touched by any of it.
    CHECK(cs.banked_radius_scale[kSud] ==
          Catch::Approx(1.0 - 0.5 + 0.15 + 0.15));
}

// ===========================================================================
// 4. THE FLOOR. What the shrink took is RECORDED, because it is not derivable.
// ===========================================================================
TEST_CASE("pump repair: the floored shrink is paid back exactly, never more") {
    // shrink 0.8 on a 1.0 dome across two pumps: 1.0 -> 0.2 -> floor 0.0.
    // The second loss only TOOK 0.2, and only 0.2 may come back for it.
    //   M2 (invert the shrink: banked += shrink_frac) -> 0.2 + 0.8 = 1.0 after
    //      ONE revive, a dome bigger than the faction ever had.
    combat::ConquestParams params;
    params.shrink_radius_frac = 0.8;
    params.shrink_ceiling_frac = 0.8;
    params.growth_radius_frac = 0.0;  // isolate the shrink
    params.growth_ceiling_frac = 0.0;
    combat::ConquestState cs = four_pumps();
    combat::RepairParams rp;

    kill_pump(cs, 0, kSud, params);
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.2));
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.8));

    kill_pump(cs, 2, kSud, params);
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.0));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.0));
    CHECK(cs.shrink_taken_radius[2] == Catch::Approx(0.2));  // NOT 0.8

    bool revived = false;
    repair_to_full(cs, 2, 1.0, rp, &revived);
    REQUIRE(revived);
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.2));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(0.2));

    repair_to_full(cs, 0, 1.0, rp, &revived);
    CHECK(cs.radius_scale[kVal] == Catch::Approx(1.0));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(1.0));
    // ★ THE CEILING ON THE WHOLE MECHANIC: a revive can never exceed the
    // pre-loss banked value, by construction.
    CHECK(cs.radius_scale[kVal] <= 1.0 + 1e-12);
}

// ★★★ 4b. THE SAME FLOOR, WALKED BACKWARDS.
//
// The leg above only ever repairs in the SAFE order -- the last pump lost is
// the first pump fixed -- so it never asks the question the floor actually
// raises: what happens when the payback that was CLIPPED (0.2) is banked last
// and the payback that was WHOLE (0.8) is banked first? A player has no reason
// to prefer either order, and the millwright loop makes the reverse one the
// natural one (you fix the surface pump you can drive to, not the deep one you
// cannot reach).
//
// What the code does, pinned: the two paybacks are the two RECORDED amounts,
// so their SUM is the same either way and only the INTERMEDIATE dome differs
// (0.8 here where the safe order shows 0.2). That intermediate is
// order-dependent by design -- it is "how much of what was taken has been paid
// back", and paying back the bigger loss first is genuinely worth more.
//
// ★ AND THE BOUND STILL HOLDS AT EVERY STEP, which is the guarantee the plan
// actually states (§4.2, "a revive can never exceed the pre-loss banked
// value"). It holds by construction and not by a clamp: every payback is a
// partial sum of the amounts the losses recorded, and those sum to at most
// what was there before them. So no clamp is added; the leg asserts the bound
// at every intermediate instead, which is the assertion a clamp would exist to
// make -- and which would fail the day somebody makes the payback anything
// other than the recorded amount.
//
// WRONG IMPLEMENTATION: M2, `banked += shrink_frac` (invert the shrink instead
// of reading the record). In THIS order it pays 0.8 for the clipped slot and
// the intermediate dome reaches 1.6 -- the bound below is red, loudly.
TEST_CASE("pump repair: the floored payback is order-independent in total") {
    combat::ConquestParams params;
    params.shrink_radius_frac = 0.8;
    params.shrink_ceiling_frac = 0.8;
    params.growth_radius_frac = 0.0;  // isolate the shrink, as in leg 4
    params.growth_ceiling_frac = 0.0;
    combat::ConquestState cs = four_pumps();
    combat::RepairParams rp;

    // The pre-loss banked value: the bound every later step is measured against.
    const double pre_loss_r = cs.banked_radius_scale[kVal];
    const double pre_loss_c = cs.banked_ceiling_scale[kVal];
    REQUIRE(pre_loss_r == Catch::Approx(1.0));

    kill_pump(cs, 0, kSud, params);  // FIRST loss: took the whole 0.8
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.8));
    kill_pump(cs, 2, kSud, params);  // SECOND loss: CLIPPED by the floor to 0.2
    CHECK(cs.shrink_taken_radius[2] == Catch::Approx(0.2));
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.0));

    // ★ THE REVERSE ORDER: the FIRST-lost pump is fixed FIRST.
    bool revived = false;
    repair_to_full(cs, 0, 1.0, rp, &revived);
    REQUIRE(revived);
    // The intermediate is 0.8, not the 0.2 the safe order shows at this point.
    // Both are truthful; the number means "what has been paid back so far".
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.8));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.8));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(0.8));
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.0));  // paid, and spent
    CHECK(cs.shrink_taken_radius[2] == Catch::Approx(0.2));  // still owed
    // ★ THE BOUND, AT THE INTERMEDIATE. This is where M2 blows it (1.6).
    CHECK(cs.banked_radius_scale[kVal] <= pre_loss_r + 1e-12);
    CHECK(cs.banked_ceiling_scale[kVal] <= pre_loss_c + 1e-12);

    // The clipped slot pays back its 0.2 -- exactly what it took, no more.
    repair_to_full(cs, 2, 1.0, rp, &revived);
    REQUIRE(revived);
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(1.0));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(1.0));
    CHECK(cs.ceiling_scale[kVal] == Catch::Approx(1.0));
    CHECK(cs.banked_radius_scale[kVal] <= pre_loss_r + 1e-12);
    CHECK(cs.banked_ceiling_scale[kVal] <= pre_loss_c + 1e-12);

    // ★ THE TOTAL IS THE SAME IN BOTH ORDERS -- leg 4 walked the other one to
    // the same 1.0 -- and the invariant closes: nothing is owed to any slot.
    for (int i = 0; i < combat::kNumPumps; ++i) {
        CHECK(cs.shrink_taken_radius[i] == Catch::Approx(0.0));
        CHECK(cs.shrink_taken_ceiling[i] == Catch::Approx(0.0));
    }
}

// ===========================================================================
// 5. THE TWO GUARDS OF THE MASK.
// ===========================================================================
TEST_CASE("pump repair: a slotless faction keeps its dome through a revive") {
    // ★ THE SINGLE-PUMP FIXTURE. Several conquest fixtures configure ONE pump
    // and leave the other three at their defaults, which leaves one faction
    // owning no slots at all. It is exempt from the collapse (it was never in
    // the pump economy) and it must stay exempt when the OTHER side's revive
    // re-applies the mask.
    //   M4 (drop the slotless guard) -> SUDBURY's dome is zeroed by a repair
    //      it had nothing to do with.
    combat::ConquestParams params;
    combat::ConquestState cs;  // ALL FOUR default to VALLEY: SUDBURY is
    cs.player_faction = kVal;  // slotless, exactly like the fixtures.
    for (int i = 0; i < combat::kNumPumps; ++i) {
        cs.pumps[i].max_hp = 100.0;
        cs.pumps[i].hp = 100.0;
    }
    REQUIRE_FALSE(combat::faction_owns_pump_slot(cs, kSud));

    kill_pump(cs, 0, kSud, params);
    CHECK(cs.radius_scale[kSud] == Catch::Approx(1.15));  // exempt, and grown

    combat::RepairParams rp;
    bool revived = false;
    repair_to_full(cs, 0, 1.0, rp, &revived);
    REQUIRE(revived);
    CHECK(cs.radius_scale[kSud] == Catch::Approx(1.15));
    CHECK(cs.ceiling_scale[kSud] == Catch::Approx(1.15));
    // ...and the victim got its own 0.5 back.
    CHECK(cs.radius_scale[kVal] == Catch::Approx(1.0));
}

TEST_CASE("pump repair: a damaged-but-alive pump completing moves no dome") {
    // ★ THE REGROW IS REVIVAL-GATED, NOT REPAIR-GATED.
    //   M6 (pay on completion) -> a pump that never died hands out a dome it
    //      never lost, every time somebody tops it up.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps(200.0);
    combat::RepairParams rp;
    kill_pump(cs, 0, kSud, params);  // the OTHER pump dies: banked 0.5
    REQUIRE(cs.banked_radius_scale[kVal] == Catch::Approx(0.5));
    REQUIRE_FALSE(combat::damage_pump(cs, 2, 50.0, kSud, params));  // damaged
    REQUIRE(cs.pumps[2].alive);

    const double before = cs.radius_scale[kVal];
    bool revived = false;
    repair_to_full(cs, 2, 1.0, rp, &revived);
    CHECK_FALSE(revived);
    CHECK(cs.pumps[2].hp == Catch::Approx(200.0));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(before));
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.5));
}

TEST_CASE("pump repair: shrink_taken is zero on every alive slot and paid once") {
    // THE INVARIANT: nonzero ONLY while a slot is dead-and-unpaid. Overwritten
    // on the shrink, never accumulated; zeroed the instant it is paid.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();
    combat::RepairParams rp;
    for (int i = 0; i < combat::kNumPumps; ++i) {
        CHECK(cs.shrink_taken_radius[i] == Catch::Approx(0.0));
        CHECK(cs.shrink_taken_ceiling[i] == Catch::Approx(0.0));
    }

    kill_pump(cs, 0, kSud, params);
    for (int i = 0; i < combat::kNumPumps; ++i) {
        if (cs.pumps[i].alive) {
            CHECK(cs.shrink_taken_radius[i] == Catch::Approx(0.0));
            CHECK(cs.shrink_taken_ceiling[i] == Catch::Approx(0.0));
        }
    }
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.5));

    bool revived = false;
    repair_to_full(cs, 0, 1.0, rp, &revived);
    REQUIRE(revived);
    const double banked_after = cs.banked_radius_scale[kVal];
    CHECK(cs.shrink_taken_radius[0] == Catch::Approx(0.0));

    // REPAIR THE SAME SLOT AGAIN with no loss in between: the record is spent,
    // so the second pass adds nothing. (A mutation that ACCUMULATES
    // shrink_taken, or that fails to zero it, doubles the dome here.)
    REQUIRE(cs.pumps[0].alive);
    for (int t = 0; t < 200; ++t) combat::repair_pump_tick(cs, 0, 1.0, rp);
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(banked_after));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(banked_after));
}

// ===========================================================================
// 6. THE LOSS PATH IS UNTOUCHED.
// ===========================================================================
TEST_CASE("pump repair: a loss-only sequence is unmoved and banked mirrors it") {
    // ★ THE ARITHMETIC OF THE LOSS PATH IS NOT EDITED -- the banked pair is an
    // ADDITION beside it -- so this pins that the live numbers are still the
    // documented ones, and that `live == mask(banked)` holds at every step.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();

    kill_pump(cs, 0, kSud, params);
    CHECK(cs.radius_scale[kSud] == Catch::Approx(1.15));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.5));
    kill_pump(cs, 1, kVal, params);
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.65));
    CHECK(cs.radius_scale[kSud] == Catch::Approx(0.65));
    kill_pump(cs, 2, kSud, params);
    CHECK(cs.radius_scale[kSud] == Catch::Approx(0.80));
    CHECK(cs.radius_scale[kVal] == Catch::Approx(0.0));  // pumpless: MASKED
    CHECK(cs.banked_radius_scale[kVal] == Catch::Approx(0.15));

    for (int f = 0; f < 2; ++f) {
        const bool live = combat::faction_bubble_live(cs, f);
        CHECK(cs.radius_scale[f] ==
              Catch::Approx(live ? cs.banked_radius_scale[f] : 0.0));
        CHECK(cs.ceiling_scale[f] ==
              Catch::Approx(live ? cs.banked_ceiling_scale[f] : 0.0));
    }
    CHECK(cs.shrink_taken_radius[3] == Catch::Approx(0.0));  // still alive
}

TEST_CASE("pump repair: a repair tick never lowers hp so the defence stays cold") {
    // ★ THE LOSS-ONLY DEFENCE LATCH (app/instructor_tick.h ~1478-1491) arms on
    // an HP DELTA DOWNWARD: `p.hp < prev_pump_hp[i] - 1e-9`. Its predicate is
    // reproduced here verbatim and driven by a repair, so a millwright can
    // never scramble his own faction's defenders onto himself. The latch code
    // is NOT edited by this rung; this leg is what says it did not need to be.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps(200.0);
    combat::RepairParams rp;
    kill_pump(cs, 0, kSud, params);

    double prev_hp[combat::kNumPumps];
    for (int i = 0; i < combat::kNumPumps; ++i) prev_hp[i] = cs.pumps[i].hp;
    bool latched = false;
    for (int t = 0; t < 60 * 120 + 10; ++t) {
        combat::repair_pump_tick(cs, 0, 1.0 / 120.0, rp);
        for (int i = 0; i < combat::kNumPumps; ++i) {
            if (cs.pumps[i].hp < prev_hp[i] - 1e-9) latched = true;
            prev_hp[i] = cs.pumps[i].hp;
        }
    }
    CHECK(cs.pumps[0].alive);
    CHECK_FALSE(latched);
}

// ===========================================================================
// 7. ★★★ THE CLOCK. "fix the surface pumps to STOP THE GAME CLOCK."
// ===========================================================================
TEST_CASE("pump repair: the clock is HELD and resumes from the seconds left") {
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();
    combat::RepairParams rp;
    REQUIRE(params.match_countdown_s == Catch::Approx(600.0));

    kill_pump(cs, 0, kSud, params);
    kill_pump(cs, 2, kSud, params);
    REQUIRE(cs.countdown_faction == kVal);
    REQUIRE(cs.countdown_s == Catch::Approx(600.0));
    REQUIRE_FALSE(cs.countdown_paused);

    combat::conquest_countdown_tick(cs, 120.0, params);
    REQUIRE(cs.countdown_s == Catch::Approx(480.0));

    // FIX ONE. The clock stops where it is -- and it is still ARMED and still
    // pointed at him, so the HUD keeps drawing it with "CLOCK HELD" beside it.
    bool revived = false;
    repair_to_full(cs, 0, 1.0, rp, &revived);
    REQUIRE(revived);
    CHECK(cs.countdown_paused);
    CHECK(combat::countdown_active(cs));
    // ★ L6 (Chad, 2026-09-03): A HELD CLOCK HAS NO TARGET. It used to stay
    // pointed at kVal; under the re-pointing ruling the target is DERIVED from
    // who is eliminated, and with his pump back nobody is -- so it parks at -1
    // and keeps every second. `countdown_active` is what says it is still
    // armed, which is why the line above is the one the HUD reads.
    CHECK(cs.countdown_faction == -1);
    for (int t = 0; t < 100; ++t)
        combat::conquest_countdown_tick(cs, 1.0, params);
    CHECK(cs.countdown_s == Catch::Approx(480.0));  // 100 s of held clock
    CHECK(cs.outcome == combat::Outcome::PLAYING);

    // LOSE IT AGAIN. ★ NO FREE EXTENSION: it resumes from 480, not from 600,
    // and it is never re-pointed. (M5 -- resuming at match_countdown_s -- is
    // exactly what this line is for.)
    kill_pump(cs, 0, kSud, params);
    CHECK_FALSE(cs.countdown_paused);
    CHECK(cs.countdown_faction == kVal);
    CHECK(cs.countdown_s == Catch::Approx(480.0));
    CHECK(cs.countdown_s != Catch::Approx(600.0));

    combat::conquest_countdown_tick(cs, 60.0, params);
    CHECK(cs.countdown_s == Catch::Approx(420.0));

    // And the buzzer still crowns the victor when it is allowed to run out.
    combat::conquest_countdown_tick(cs, 1000.0, params);
    CHECK(cs.countdown_s == Catch::Approx(0.0));
    CHECK(cs.outcome == combat::Outcome::DEFEAT);  // the player was clocked
}

TEST_CASE("pump repair: no repair means no pause -- the arm doctrine is intact") {
    // The 2026-08-30 "armed once, never re-armed, no free extension" doctrine
    // predates repair existing. The pause is entered ONLY BY a repair, so
    // without one nothing about the clock changes at all.
    combat::ConquestParams params;
    combat::ConquestState cs = four_pumps();
    kill_pump(cs, 0, kSud, params);
    kill_pump(cs, 2, kSud, params);
    CHECK_FALSE(cs.countdown_paused);
    combat::conquest_countdown_tick(cs, 300.0, params);
    CHECK(cs.countdown_s == Catch::Approx(300.0));
    // A SECOND collapse (the enemy loses theirs) never re-arms or extends.
    kill_pump(cs, 1, kVal, params);
    kill_pump(cs, 3, kVal, params);
    // ★ L6 (Chad, 2026-09-03): with EVERY pump on the map gone the clock has
    // nobody to be about, so it is HELD (no target) -- and it keeps its 300 s,
    // because a repair can still point them at whoever is left pumpless.
    CHECK(cs.countdown_paused);
    CHECK(cs.countdown_faction == -1);
    CHECK(cs.countdown_s == Catch::Approx(300.0));
    CHECK(cs.deathmatch);
}

// ===========================================================================
// 8. THE TAPE. A new tag is additive; every old row is byte-for-byte the same.
// ===========================================================================
TEST_CASE("pump repair: the tape emits pr on a revive and pk is unchanged") {
    seads_tape::ConquestTape tape("test");
    app::ConquestWorld cq;
    cq.state = four_pumps(100.0);
    app::LoopState st = flying_at(glm::dvec3{15000.0, 0.0, 0.0});

    st.tick_count = 0;
    tape.on_tick(app::TickInput{}, st, nullptr, nullptr, &cq, nullptr);

    // Tick 1: the pump dies. The pk row is EXACTLY the shape it always was.
    combat::ConquestParams params;
    kill_pump(cq.state, 0, kSud, params);
    st.tick_count = 1;
    tape.on_tick(app::TickInput{}, st, nullptr, nullptr, &cq, nullptr);

    // Tick 2: a millwright puts it back.
    combat::RepairParams rp;
    bool revived = false;
    repair_to_full(cq.state, 0, 1.0, rp, &revived);
    REQUIRE(revived);
    st.tick_count = 2;
    tape.on_tick(app::TickInput{}, st, nullptr, nullptr, &cq, nullptr);

    std::string out;
    REQUIRE(tape.drain(out));

    const std::string pk = find_line(out, "pk", 1);
    REQUIRE_FALSE(pk.empty());
    CHECK(pk.find("\"pump\":0") != std::string::npos);
    CHECK(pk.find("\"fac\":0") != std::string::npos);
    // ★ THE OLD ROW DID NOT GROW A FIELD. Its whole body is the four it always
    // had, so a pre-L2 decoder reads it unchanged.
    CHECK(pk == "{\"t\":\"pk\",\"k\":1,\"pump\":0,\"fac\":0}");

    const std::string pr = find_line(out, "pr", 2);
    REQUIRE_FALSE(pr.empty());
    CHECK(pr.find("\"pump\":0") != std::string::npos);
    CHECK(pr.find("\"fac\":0") != std::string::npos);
    CHECK(pr.find("\"rs\":[") != std::string::npos);
    // Exactly one of each: an edge, not a level.
    CHECK(count_lines_with_tag(out, "pk") == 1);
    CHECK(count_lines_with_tag(out, "pr") == 1);
}

TEST_CASE("pump repair: a tape with no repair in it carries no pr row at all") {
    // OLD TAPES DECODE UNCHANGED because a session that never repairs emits
    // nothing new -- the tag is additive and edge-triggered.
    seads_tape::ConquestTape tape("test");
    app::ConquestWorld cq;
    cq.state = four_pumps(100.0);
    app::LoopState st = flying_at(glm::dvec3{15000.0, 0.0, 0.0});
    combat::ConquestParams params;
    for (long long k = 0; k < 6; ++k) {
        if (k == 2) kill_pump(cq.state, 0, kSud, params);
        if (k == 4) kill_pump(cq.state, 2, kSud, params);
        st.tick_count = k;
        tape.on_tick(app::TickInput{}, st, nullptr, nullptr, &cq, nullptr);
    }
    std::string out;
    REQUIRE(tape.drain(out));
    CHECK(count_lines_with_tag(out, "pk") == 2);
    CHECK(count_lines_with_tag(out, "pr") == 0);
    CHECK(out.find("\"t\":\"pr\"") == std::string::npos);
}
