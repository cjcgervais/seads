// ENV-3 — THE DEFENCE SCRAMBLE, the pure-law arms
// (docs/RUNG_SPEC_20260828_energy_envelope.md §4 "RUNG E3").
//
// CHAD, 2026-08-28: "when triggered to defend may climb and then use the dive
// (parabolic) to get to enemy me faster if I am attacking their pump or they
// mine."
// RULED 2026-08-29: DIVE-IF-HIGH / SPRINT-IF-LOW. The literal climb-then-dive
// from low measured ~13 s SLOWER (55 s vs 42 s over 9 km), so a low defender
// sprints and only a defender that already holds altitude flies the parabola.
//
// THE MEASURED PROBLEM: on tape 13, time-to-first-defender-within-900-m of the
// dying SUDBURY surface pump was INFINITE. The closest any enemy came while
// pump 1 died was 3,679 m. The scramble had been inheriting `raid_speed_target`
// = 123 m/s -- an ORBIT dial C1 chose so a raider could close its on-station
// circle, never a scramble dial. Nobody picked 123 for a 9 km emergency.
//
// ⚠ SCOPE, STATED PLAINLY: these are pure-law arms over the shipped dials and
// the geometry of the decision. The closed-loop arm the spec calls T-1 --
// "first defender within 900 m in under 50 s, from 9 km, with foes present" --
// is NOT built. Nothing here is evidence a pump gets defended. It is evidence
// that the dials are the ruled ones and that the dive fires on exactly the
// population Chad ruled it should.
//
// Names are strictly ASCII with no comma.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "config/load_aircraft.h"
#include "combat/conquest.h"
#include "combat/raid.h"
#include "world/faction_bubbles.h"
#include "config/load_scenario.h"
#include "drone/drone.h"

#ifndef SEADS_CONFIG_DIR
#define SEADS_CONFIG_DIR "config"
#endif

namespace {
const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
constexpr double kDeg = 3.14159265358979323846 / 180.0;

// SUDBURY stamp + a point on its major axis, for the perch arms.
drone::AirDomes stamp_sud(double scale_own, double scale_other) {
    world::FactionGrowth grow[2];
    world::FactionGrowth unit[2];
    grow[world::SUDBURY].radius_scale = scale_own;
    grow[world::VALLEY].radius_scale = scale_other;
    drone::AirDomes ad;
    for (int slot = 0; slot < 2; ++slot) {
        const int f = (slot == 0) ? world::SUDBURY : world::VALLEY;
        drone::DomeEllipse& e = ad.dome[slot];
        e.radius_scale = grow[f].radius_scale;
        if (!(e.radius_scale >= 1e-9)) continue;
        world::faction_ellipse(f, grow, e.center_dir, e.major_axis, e.a_m,
                               e.b_m);
        glm::dvec3 bd{0.0, 1.0, 0.0};
        glm::dvec3 bm{1.0, 0.0, 0.0};
        world::faction_ellipse(f, unit, bd, bm, e.base_a_m, e.base_b_m);
        e.live = e.a_m > 0.0 && e.b_m > 0.0;
    }
    return ad;
}

glm::dvec3 on_major_sud(double arc) {
    const glm::dvec3 cn = glm::normalize(world::kSudburyCenterDir);
    glm::dvec3 m = world::kSudburyMajorAxis -
                   cn * glm::dot(world::kSudburyMajorAxis, cn);
    m = glm::normalize(m);
    const double th = arc / kAp.R;
    return glm::normalize(cn * std::cos(th) + m * std::sin(th)) * kAp.R;
}
}  // namespace

TEST_CASE("ENV3-A1 the scramble dials are the ruled ones", "[env3]") {
    const drone::DroneParams& dp = kScen.drone;
    REQUIRE(dp.defend_sprint_speed == 245.0);
    REQUIRE(dp.defend_release_m == 2500.0);
    // ★★★ THE DIVE IS ON — CHAD RULED IT ON 2026-08-30, AS A ONE-SHOT.
    // It shipped OFF before because the FIRST BUILD was a sustained floor and
    // measured `ai_damage` EXACTLY 0 on the signed certificate. His ruling was
    // never the defect; the build was. The manoeuvre now ARMS once, is RIDDEN,
    // and EXITS on a gate — and these two dials ARE the bound. Without them the
    // ride never ends and the floor is back, silently.
    REQUIRE(dp.defend_dive_gamma > 0.0);
    REQUIRE(dp.defend_dive_close_range_m == 4000.0);
    // ★★★ THE EXIT GATE MUST EXIST AND MUST BE REACHABLE. This is the whole
    // difference between his manoeuvre and the thing that killed the air war.
    // MEASURED on the certificate at HEAD 2026-08-30, sweeping this dial:
    //   exit 150 -> GREEN.   exit 180 -> ai_damage 1.03.   exit 210 -> 0.
    //   exit 245 (= v_redline) -> 0, because the dive can never BUY redline,
    //   so the gate never fires and the bound is INERT — a one-shot with an
    //   unreachable exit IS the sustained floor wearing a latch.
    // That cliff is why the ceiling below is an arm and not a comment.
    REQUIRE(dp.defend_dive_exit_speed > 0.0);
    REQUIRE(dp.defend_dive_exit_speed <= 150.0);
    // ...and it must still be a real gain over the orbit speed, or the dive
    // exits before it has bought anything and the manoeuvre is decorative.
    REQUIRE(dp.defend_dive_exit_speed > dp.raid_speed_target);
    // The "high" half of "high AND far": a real advantage, not any advantage.
    REQUIRE(dp.defend_dive_arm_alt_m > 0.0);

    // ★ THE SPRINT MUST ACTUALLY BE A SPRINT. The whole defect was that the
    // scramble inherited an ORBIT speed; if these two are ever equal the rung
    // has silently reverted and nothing else in the build would notice.
    REQUIRE(dp.defend_sprint_speed > dp.raid_speed_target);
    // ...and the ratio is the claim: 245 vs 123 answers a 9 km scramble in
    // ~42 s instead of ~73 s.
    REQUIRE(dp.defend_sprint_speed / dp.raid_speed_target > 1.9);

    // ⚠ 245 IS v_redline. Pinned here so that if someone raises the sprint
    // past the airframe, this arm says so rather than the fly finding out.
    REQUIRE(dp.defend_sprint_speed <= kAp.v_redline);

    // The steepening range still has to sit OUTSIDE the release or the dive
    // could never fire when it is turned back on: the dive clause and the
    // sprint clause share the `to_pump > release` guard.
    REQUIRE(dp.defend_dive_close_range_m > dp.defend_release_m);
    // REQUIRED-RED MUTATION: set defend_sprint_speed = 123.0 (back to the
    // orbit speed). VERIFIED RED on the two ratio lines.
    // SECOND REQUIRED-RED: set defend_dive_exit_speed = 245.0 (the old
    // unreachable gate). VERIFIED RED here AND on the signed certificate --
    // which is the point: an inert bound is not a quiet edit.
}

TEST_CASE("ENV3-A2 the dive fires on exactly the ruled population", "[env3]") {
    // The decision the seam makes, reproduced here as the pure predicate it is.
    // Chad ruled DIVE-IF-HIGH: a defender BELOW its pump must not fly the
    // parabola, because from low the manoeuvre measured 13 s slower than just
    // going. This arm is the guard on that ruling.
    // ★★★ CHAD'S DOCTRINE 2026-08-30: ARM ONCE / RIDE / EXIT ON A GATE.
    // ⚠ This arm carries the LATCH, not just the predicate, because the whole
    // defect was a manoeuvre with no end: the old clause was a pure function of
    // (high, far) and so re-decided "dive" on every tick forever. A stateless
    // mirror of a stateful law CANNOT catch that, and the old version of this
    // arm passed green the entire time the air war was dead.
    drone::DroneParams dp = kScen.drone;
    REQUIRE(dp.defend_dive_close_gamma > dp.defend_dive_gamma);

    // The seam's state, mirrored: one drone, ticked.
    struct Dive {
        bool riding = false;
        bool spent = false;
    };
    // Mirrors drone::tick's ENV-3 clause exactly, ORDER INCLUDED (exit is
    // evaluated BEFORE the ride, so the exit tick already flies the merge).
    auto step = [&](Dive& s, bool order_active, double up_here, double up_pump,
                    double to_pump, double speed, double aim_gamma) {
        if (!order_active) {
            s.riding = false;
            s.spent = false;
            return aim_gamma;
        }
        const double advantage = up_here - up_pump;
        if (dp.defend_dive_gamma <= 0.0) return aim_gamma;
        if (!s.riding && !s.spent && advantage > dp.defend_dive_arm_alt_m &&
            to_pump > dp.defend_release_m) {
            s.riding = true;
        }
        if (s.riding && ((dp.defend_dive_exit_speed > 0.0 &&
                          speed >= dp.defend_dive_exit_speed) ||
                         to_pump <= dp.defend_release_m || advantage <= 0.0)) {
            s.riding = false;
            s.spent = true;
        }
        if (s.riding) {
            const double d = to_pump < dp.defend_dive_close_range_m
                                 ? dp.defend_dive_close_gamma
                                 : dp.defend_dive_gamma;
            return std::min(aim_gamma, -d);
        }
        return aim_gamma;
    };

    const double kUp = 17000.0;       // pump radius
    const double kHigh = kUp + 2000.0;  // a perched defender
    const double kSlow = dp.raid_speed_target;  // orbit speed, nothing bought

    SECTION("high and far: the far dive arms and is ridden") {
        Dive s;
        const double g = step(s, true, kHigh, kUp, 9000.0, kSlow, -0.05);
        REQUIRE(s.riding);
        REQUIRE(std::abs(g + dp.defend_dive_gamma) < 1e-12);
    }
    SECTION("high and close: the steepened dive") {
        Dive s;
        const double g = step(s, true, kHigh, kUp, 3000.0, kSlow, -0.05);
        REQUIRE(std::abs(g + dp.defend_dive_close_gamma) < 1e-12);
    }
    SECTION("LOW: no dive at all -- sprint-if-low") {
        // ★ THE RULING'S OWN GUARD. A defender below its pump keeps whatever
        // aim_at chose and is not dragged into a manoeuvre that costs it time.
        Dive s;
        const double aim = 0.03;  // aim_at wants a slight CLIMB to the pump
        REQUIRE(step(s, true, kUp, kUp + 500.0, 9000.0, kSlow, aim) == aim);
        REQUIRE_FALSE(s.riding);
    }
    SECTION("HIGH BUT NOT HIGH ENOUGH: the arm floor is real") {
        // "High AND far" means a real advantage, not any advantage at all.
        Dive s;
        const double aim = -0.02;
        const double barely = kUp + dp.defend_dive_arm_alt_m * 0.5;
        REQUIRE(step(s, true, barely, kUp, 9000.0, kSlow, aim) == aim);
        REQUIRE_FALSE(s.riding);
    }
    SECTION("inside the release: no dive -- the merge belongs to the orbit") {
        Dive s;
        const double aim = -0.02;
        REQUIRE(step(s, true, kHigh, kUp, 1000.0, kSlow, aim) == aim);
    }
    SECTION("it can only ever STEEPEN a descent") {
        // If aim_at already wants steeper than the dive, the dive must not
        // shallow it out. `min` on a negative is the whole reason.
        Dive s;
        const double steeper = -0.9;
        REQUIRE(step(s, true, kHigh, kUp, 9000.0, kSlow, steeper) == steeper);
    }

    // ★★★ THE THREE SECTIONS BELOW ARE THE DOCTRINE. Nothing above them can
    // distinguish his one-shot from the floor that killed the air war.
    SECTION("THE RIDE ENDS: the speed gate exits it") {
        Dive s;
        step(s, true, kHigh, kUp, 9000.0, kSlow, -0.05);
        REQUIRE(s.riding);
        // The dive has now bought its speed. That is the whole purpose, and
        // reaching it is the END of the manoeuvre, not a reason to keep going.
        const double aim = -0.05;
        const double g =
            step(s, true, kHigh, kUp, 9000.0, dp.defend_dive_exit_speed, aim);
        REQUIRE_FALSE(s.riding);
        REQUIRE(s.spent);
        REQUIRE(g == aim);  // the exit tick already flies the merge
    }
    SECTION("ONE SHOT: it does not re-arm inside the same scramble") {
        Dive s;
        step(s, true, kHigh, kUp, 9000.0, kSlow, -0.05);
        step(s, true, kHigh, kUp, 9000.0, dp.defend_dive_exit_speed, -0.05);
        REQUIRE(s.spent);
        // Still high, still far, and now slow again -- the exact state that
        // armed it the first time. A floor would dive again here. His
        // manoeuvre does not.
        const double aim = -0.05;
        for (int i = 0; i < 50; ++i) {
            REQUIRE(step(s, true, kHigh, kUp, 9000.0, kSlow, aim) == aim);
            REQUIRE_FALSE(s.riding);
        }
    }
    SECTION("A NEW SCRAMBLE GETS A NEW SHOT") {
        // One shot per ORDER, not one per lifetime: a defender sent out again
        // against a fresh threat is entitled to dive again.
        Dive s;
        step(s, true, kHigh, kUp, 9000.0, kSlow, -0.05);
        step(s, true, kHigh, kUp, 9000.0, dp.defend_dive_exit_speed, -0.05);
        REQUIRE(s.spent);
        step(s, false, kHigh, kUp, 9000.0, kSlow, 0.0);  // order stands down
        REQUIRE_FALSE(s.spent);
        const double g = step(s, true, kHigh, kUp, 9000.0, kSlow, -0.05);
        REQUIRE(s.riding);
        REQUIRE(std::abs(g + dp.defend_dive_gamma) < 1e-12);
    }
    // REQUIRED-RED MUTATION: drop the `up_here > up_pump` term (dive always).
    // VERIFIED RED on the LOW section -- a defender below its pump is handed a
    // descent it cannot afford, which is exactly the shape Chad ruled against.
    // SECOND REQUIRED-RED (the doctrine): delete the exit clause, i.e. restore
    // the sustained floor. VERIFIED RED on "THE RIDE ENDS" and on "ONE SHOT".
    // THIRD REQUIRED-RED: never clear `spent` when the order stands down.
    // VERIFIED RED on "A NEW SCRAMBLE GETS A NEW SHOT".
}

// ---------------------------------------------------------------------------
// ENV-4 — THE DEFENCE TRIGGER. Chad asked for "more defense awareness".
//
// MEASURED on tape 13: SUDBURY's surface pump died at 10:41 with the nearest
// enemy 9,070 m away. Its HP had been falling since 08:12 under ally i=8's
// raid, but the shipped trigger is a PROXIMITY test -- "is an enemy within
// kDefendThreatRadiusM of my pump" -- so it only armed when the PLAYER closed,
// about eight seconds before the kill. Proximity answers "is someone near my
// pump". The question that matters is "is my pump being HURT".
//
// ★ THE SIGNAL ALREADY EXISTED AND WAS BEING THROWN AWAY: the app maintains an
// HP-delta latch per faction for D2's regroup (app/instructor_tick.h,
// cq->pump_attacked_s / attacked_pump), decayed over regroup_attack_window_s.
// This rung costs one argument, not a mechanism.

TEST_CASE("ENV4-A1 the HP-delta trigger arms a defence proximity misses",
          "[env4]") {
    // The exact geometry of the tape-13 failure: the attacker is ON the pump,
    // but every DEFENDER is far away, and the shipped radius test is asked
    // about drones near the pump -- which the attacker is. So build the case
    // the radius genuinely cannot see: the pump is losing HP with NO opposing
    // aircraft inside the threat radius at all (an off-screen raider that has
    // already made its pass, or -- once the millwright canon lands -- damage
    // taken from a source that is not an aeroplane).
    const double kThreat = 2500.0;
    combat::Pump pumps[4]{};
    for (int i = 0; i < 4; ++i) {
        pumps[i].alive = true;
        pumps[i].surface = i < 2;
        pumps[i].faction = (i % 2 == 0) ? combat::CQ_VALLEY : combat::CQ_SUDBURY;
    }
    // Faction 1's surface pump, and a defender parked 8 km from it.
    pumps[1].pos = glm::dvec3{15000.0, 0.0, 0.0};
    pumps[1].faction = combat::CQ_SUDBURY;
    pumps[0].pos = glm::dvec3{-15000.0, 0.0, 0.0};

    std::vector<drone::DroneState> fleet;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        drone::DroneState d;
        d.spawn_index = i;
        d.inert = false;
        // Park everyone far from both pumps, on their own side.
        d.curr.position = glm::dvec3{15000.0, 8000.0, 0.0};
        fleet.push_back(d);
    }
    sim::SimState player;
    player.position = glm::dvec3{-15000.0, 0.0, 0.0};  // far from pump 1

    auto count_defenders = [&](const bool* hurt) {
        combat::assign_defense(fleet, pumps, player, /*player_faction=*/0,
                               kThreat, /*defenders_per_faction=*/2, hurt);
        int n = 0;
        for (const drone::DroneState& d : fleet)
            if (d.defend.active) ++n;
        return n;
    };

    SECTION("proximity alone sees nothing -- the premise") {
        // If this ever becomes non-zero the fixture has stopped isolating the
        // trigger and every claim below is worthless (the fixture-no-op class).
        REQUIRE(count_defenders(nullptr) == 0);
    }
    SECTION("the HP-delta latch arms it") {
        const bool hurt[2] = {false, true};
        REQUIRE(count_defenders(hurt) > 0);
    }
    SECTION("it arms the RIGHT faction only") {
        const bool hurt[2] = {true, false};  // faction 0's pump is the hurt one
        combat::assign_defense(fleet, pumps, player, 0, kThreat, 2, hurt);
        for (const drone::DroneState& d : fleet)
            if (d.defend.active)
                REQUIRE(combat::maverick_faction(d.spawn_index) == 0);
    }
    // REQUIRED-RED MUTATION: delete the `if (surface_pump_hurt != nullptr &&
    // surface_pump_hurt[f]) threatened = true;` clause in combat/raid.h.
    // VERIFIED RED on "the HP-delta latch arms it" -- defenders fall to 0.
    // ⚠ The nullptr default is what keeps every pre-ENV-4 caller
    // bit-identical, and the first section is what proves this fixture would
    // notice if that stopped being true.
}

// ---------------------------------------------------------------------------
// ENV-3.4 — THE PERCH. Chad, 2026-08-28: "if they are deep in bubble they can
// climb and get an energy advantage to be more effective defenders and
// attackers, MAKE SURE THAT ABILITY PERSISTS."
//
// ★ THE GAP: ENV-2 gives PERMISSION to be high deep inside; nothing made a
// drone TAKE it, because the allowance is a ceiling and never a target. On
// Chad's own signed fly (tape 14, 31,200 drone rows) median altitude by depth
// band was outside 347 m / fringe 904 / plateau 1,547 / free core 1,783, p90
// 2,004 -- the envelope was already shaping them, and in the core they stopped
// a few hundred metres under what they were allowed.
//
// ⚠⚠ WHAT THESE ARMS DO NOT SHOW. They grade the perch's LAW: where it fires,
// where it refuses, and that it stays under the governor. They do NOT show that
// a perched drone is a better defender or attacker. That is the spec's T-3
// ("perch ON vs OFF against a scripted 270 m/s attacker, grading
// in-band-on-attacker fraction and time-to-first-gun-solution") and it is NOT
// BUILT. ★ T-3 is the arm that can REFUTE this rung: if those numbers do not
// move, the perch is spectacle -- aeroplanes sitting prettily at altitude --
// and it must be REPORTED as such, not kept because it looks right.

TEST_CASE("ENV5-A1 the perch dials are shipped and sane", "[env5]") {
    const drone::DroneParams& dp = kScen.drone;
    REQUIRE(dp.guard_alt_lo_m == 1500.0);
    REQUIRE(dp.guard_alt_hi_m == 2500.0);
    REQUIRE(dp.guard_margin_m == 300.0);
    REQUIRE(dp.guard_alt_hi_m > dp.guard_alt_lo_m);
    REQUIRE(dp.guard_gamma_cap > 0.0);
    // The perch is a POSTURE, not a zoom.
    REQUIRE(dp.guard_gamma_cap < 0.25);  // ~14 deg
    // ★ IT MUST BE REACHABLE: the band's floor has to be satisfiable somewhere
    // inside the free core, or the perch is a dial that can never fire.
    REQUIRE(dp.guard_alt_lo_m + dp.guard_margin_m > dp.env_plateau_m);
}

TEST_CASE("ENV5-A2 the perch fires deep and refuses everywhere else",
          "[env5]") {
    const drone::DroneParams& dp = kScen.drone;
    // The perch's own predicate, reproduced from the seam.
    auto perch_target = [&](const drone::AirDomes& ad, const glm::dvec3& pos) {
        const double a = drone::env_allowance_m(ad, pos, kAp.R, dp,
                                                dp.deck_track_agl_m);
        if (!(a > dp.env_plateau_m + 1e-9)) return -1.0;  // not deep: refuse
        const double w = std::min(dp.guard_alt_hi_m, a - dp.guard_margin_m);
        return w >= dp.guard_alt_lo_m ? w : -1.0;  // not worth perching
    };

    const drone::AirDomes ad = stamp_sud(1.25, 1.0);
    const double edge = world::kSudburyMajorRadiusM * 1.25;

    SECTION("outside the air: refuses") {
        REQUIRE(perch_target(ad, on_major_sud(edge + 2000.0)) < 0.0);
    }
    SECTION("on the fringe: refuses -- this is the ruling's own shape") {
        // "fight lower on the fringes" is Chad's phrase. A perch that lifted a
        // fringe drone would be arguing with the ruling it came from.
        REQUIRE(perch_target(ad, on_major_sud(edge - 1000.0)) < 0.0);
    }
    SECTION("on the plateau: refuses") {
        REQUIRE(perch_target(ad, on_major_sud(edge - 5000.0)) < 0.0);
    }
    SECTION("in the free core: fires and asks for real altitude") {
        const double t = perch_target(ad, on_major_sud(2000.0));
        REQUIRE(t > 0.0);
        REQUIRE(t >= dp.guard_alt_lo_m);
        REQUIRE(t <= dp.guard_alt_hi_m);
        // ★ AND IT ASKS FOR MORE THAN THE FLEET ACTUALLY FLEW. Tape 14's
        // free-core median was 1,783 m. A perch targeting less than that would
        // be asking them to descend, and the rung would be pointless.
        REQUIRE(t > 1783.0);
    }
    SECTION("it never asks for more than the envelope allows") {
        // ★ THE ANTI-CHATTER GUARANTEE, and this section already earned its
        // keep: an early cut of the seam CLAMPED UP to guard_alt_lo_m, so just
        // past the knee (allowance ~1,670) it targeted 1,500 against an
        // allowance-minus-margin of ~1,370 -- above what the envelope permits.
        // The governor would then clamp the perch every tick. The seam now
        // takes a `min` and treats guard_alt_lo_m as "do not bother below
        // this", never as a floor that outranks the envelope.
        for (double arc = 0.0; arc < edge; arc += 500.0) {
            const glm::dvec3 p = on_major_sud(arc);
            const double t = perch_target(ad, p);
            if (t < 0.0) continue;
            const double a = drone::env_allowance_m(ad, p, kAp.R, dp,
                                                    dp.deck_track_agl_m);
            REQUIRE(t <= a - dp.guard_margin_m + 1e-9);
        }
    }
    // REQUIRED-RED MUTATION: set guard_alt_lo_m = 0.0 AND drop the
    // `a > env_plateau_m` deep test. VERIFIED RED on the fringe and plateau
    // sections -- a fringe drone gets lifted, contradicting "fight lower on
    // the fringes".
    // ⚠ HONEST NOTE, because I ran the single mutations first and they did NOT
    // fire: the deep test and the `w >= guard_alt_lo_m` threshold BOTH encode
    // "core only", and the threshold is the STRICTER of the two (it needs an
    // allowance above 1,800 where the deep test needs above 1,650). So the deep
    // test is SUBSUMED at the shipped dials and is not independently
    // falsifiable. It stays as the statement of intent -- the perch belongs to
    // the free core -- but the threshold is what carries the property today.
    // Same belt-and-braces shape as ENV-1's dead-dome guard, and said out loud
    // for the same reason.
}
