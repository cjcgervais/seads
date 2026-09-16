// ★★★ L10 — THE ENGINE FIX
// (Chad, 2026-09-07: "I want to allow the sudburian to fix the airplane engine
// when it says engine out, the same way the sudburian can fix the pump.")
//
// WHAT THESE LEGS EXIST TO CATCH.
//
// "The same way" is the spec, so the risk is not that the engine fix does
// nothing -- it is that it quietly does something ELSE than the pump fix does:
// a straight-line reach the man can never satisfy (the disease the pump rung
// paid for on 2026-09-03), a duration set by an arithmetic artefact instead of
// the dial, a wrench that repairs an airframe nobody asked it to touch, or a
// mode hook that survives the job. Every one of those is a leg below.
//
// No raylib, no window, no world -- the same discipline as
// test/unit/test_player_mode.cpp and test/unit/test_pump_repair.cpp.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <string>

#include "app/interact.h"
#include "app/player_mode.h"
#include "combat/damage.h"
#include "combat/engine_repair.h"
#include "combat/repair_reach.h"

namespace {

// The 15 km ball this game is played on, and an aeroplane parked on it with
// its CG a couple of metres up on its gear -- which is the whole reason the
// reach law is a GROUND distance.
constexpr double kR = 15000.0;
constexpr double kGearM = 2.0;
// Slack for a summed-`dt` stopwatch, not for the repair law: see the leg that
// uses it. Nanoseconds against durations of tens of seconds.
constexpr double kEps = 1e-9;

glm::dvec3 up_at(double east_m) {
    // A point `east_m` around the ball from the north pole, on the surface.
    const double a = east_m / kR;
    return glm::dvec3{std::sin(a), std::cos(a), 0.0};
}

glm::dvec3 parked_aircraft() { return up_at(0.0) * (kR + kGearM); }

// The man's feet, `d_m` along the ground from the aeroplane.
glm::dvec3 man_at(double d_m) { return up_at(d_m) * kR; }

combat::DamageState dead_stick() {
    combat::DamageState d;
    d.engine = 0.0;  // what app/instructor_tick.h's prop strike writes
    return d;
}

}  // namespace

// ★ THE DISEASE THE PUMP RUNG PAID FOR, ASKED OF THE ENGINE BEFORE IT COULD
// HAPPEN AGAIN. On 2026-09-03 a 6 m STRAIGHT-LINE reach to a pump standing 10 m
// up its mast could not be met from anywhere on the ground: the prompt never
// showed and the wrench never turned, and Chad reported it as "fix does not
// work for the pumps, no UI". The aeroplane is only ~2 m up, so the same
// mistake here would not be fatal -- it would just quietly eat 2 m of a 5 m
// reach and make the man walk under a wing to be served.
//
// WRONG IMPLEMENTATION: `glm::length(aircraft - man) <= reach_m`. Green on the
// far cases, RED on the two below.
TEST_CASE("engine repair: the reach is walked, not flown") {
    combat::EngineRepairParams rp;
    rp.reach_m = 5.0;

    // Standing exactly at the reach ON THE GROUND is inside it, even though
    // the straight line to the CG is longer by the gear height.
    const glm::dvec3 plane = parked_aircraft();
    CHECK(combat::engine_in_repair_reach(plane, man_at(4.9), rp));
    CHECK(glm::length(plane - man_at(4.9)) > 5.0);  // the wrong law says NO

    // And a step past it is outside, so the law is a reach and not "yes".
    CHECK(!combat::engine_in_repair_reach(plane, man_at(5.2), rp));

    // The ground law is the one both jobs share, so a direct call agrees.
    CHECK(combat::ground_reach_ok(plane, man_at(4.9), 5.0));
    CHECK(!combat::ground_reach_ok(plane, man_at(5.2), 5.0));

    // A misconfigured dial says no rather than "touching only".
    combat::EngineRepairParams bad;
    bad.reach_m = 0.0;
    CHECK(!combat::engine_in_repair_reach(plane, man_at(0.0), bad));
}

// ★ THE DURATION IS THE DIAL'S, AT ANY TICK SIZE. `full_s` is seconds of SIM
// time from a dead engine to a whole one -- the pump's law
// (combat/pump_repair.h) and its reason: "about a minute" must mean the same
// thing at 30 fps and at 300.
TEST_CASE("engine repair: full_s is the time from dead to whole") {
    combat::EngineRepairParams rp;
    rp.full_s = 45.0;

    for (const double dt : {1.0 / 120.0, 1.0 / 15.0, 0.5}) {
        combat::DamageState d = dead_stick();
        double t = 0.0;
        int guard = 0;
        while (d.engine < 1.0 && guard++ < 100000) {
            combat::repair_engine_tick(d, dt, rp);
            t += dt;
        }
        CHECK(d.engine == 1.0);
        // Within one tick of the dial, from below (the last tick clamps).
        // ⚠ THE SLACK IS THE ACCUMULATOR'S, NOT THE LAW'S: `t` is thousands of
        // `+= dt` on a binary fraction, so it drifts a few ulps off the exact
        // multiple. A bare `<=` here fails on arithmetic that is doing exactly
        // what it should.
        CHECK(t >= rp.full_s - kEps);
        CHECK(t <= rp.full_s + dt + kEps);
    }

    // A HALF-broken engine finishes in half the time, in exact proportion --
    // the rate is a rate, never a stopwatch that ignores what is left.
    combat::DamageState half;
    half.engine = 0.5;
    double t = 0.0;
    const double dt = 1.0 / 60.0;
    int guard = 0;
    while (half.engine < 1.0 && guard++ < 100000) {
        combat::repair_engine_tick(half, dt, rp);
        t += dt;
    }
    CHECK(t >= 0.5 * rp.full_s - kEps);
    CHECK(t <= 0.5 * rp.full_s + dt + kEps);
}

// ★ THE OVERSHOOT IS NOT DEBT, and the guard is here rather than trusted to
// the writers upstream. `route_damage` floors at 0 today and the prop strike
// writes a flat 0.0 -- but a negative engine reaching this function would make
// the fix take longer than `full_s` by however hard the last round landed,
// which is a felt duration set by an arithmetic artefact.
//
// WRONG IMPLEMENTATION: dropping the `std::max(0.0, d.engine)` line.
TEST_CASE("engine repair: a negative engine is floored before the work") {
    combat::EngineRepairParams rp;
    rp.full_s = 45.0;
    combat::DamageState d;
    d.engine = -0.4;
    const double dt = 1.0;
    combat::repair_engine_tick(d, dt, rp);
    // One second of a 45 s job from ZERO, not from -0.4.
    CHECK(d.engine > 0.0);
    CHECK(std::abs(d.engine - dt / rp.full_s) < 1e-12);
}

// ★★★ IT FIXES THE ENGINE AND NOTHING ELSE. A man with a wrench at the cowling
// does not un-shoot a pilot or re-attach a wing, and `combat::is_dead` (a
// separated wing / dead pilot / broken structure) is a KILL this file must
// never undo. Chad asked for the engine.
//
// WRONG IMPLEMENTATION: `reset_damage(d)` -- green on the engine, RED here.
TEST_CASE("engine repair: the airframe is not quietly made new") {
    combat::EngineRepairParams rp;
    combat::DamageState d;
    d.engine = 0.0;
    d.pilot = 0.4;
    d.wing_left = 0.2;
    d.wing_right = 0.9;
    d.structure = 0.6;
    for (int i = 0; i < 10000 && d.engine < 1.0; ++i)
        combat::repair_engine_tick(d, 1.0 / 60.0, rp);
    CHECK(d.engine == 1.0);
    CHECK(d.pilot == 0.4);
    CHECK(d.wing_left == 0.2);
    CHECK(d.wing_right == 0.9);
    CHECK(d.structure == 0.6);
    CHECK(!combat::damage_zero(d));  // still a hurt aeroplane
}

// ★ CALLING IT EVERY TICK IS SAFE AND CHEAP, which is what lets the caller
// stop thinking about whether there is work to do.
TEST_CASE("engine repair: no-ops report nothing done") {
    combat::EngineRepairParams rp;
    combat::DamageState whole;  // pristine
    const combat::EngineRepair a = combat::repair_engine_tick(whole, 1.0, rp);
    CHECK(!a.worked);
    CHECK(!a.restarted);
    CHECK(a.frac == 1.0);
    CHECK(whole.engine == 1.0);

    combat::DamageState d = dead_stick();
    CHECK(!combat::repair_engine_tick(d, 0.0, rp).worked);   // no dt
    CHECK(!combat::repair_engine_tick(d, -1.0, rp).worked);  // no dt
    combat::EngineRepairParams bad;
    bad.full_s = 0.0;
    CHECK(!combat::repair_engine_tick(d, 1.0, bad).worked);  // no dial
    CHECK(d.engine == 0.0);  // and none of them moved it
}

// ★ THE TWO EDGES THE HUD SPEAKS ON. `restarted` is the tick the ENGINE OUT
// plate goes out (it is gated on `engine <= 0` in render/draw.cpp); `restored`
// is the tick the job is done. Each fires on exactly ONE call, or the app
// prints its sentence every frame.
TEST_CASE("engine repair: restarted and restored are edges, not levels") {
    combat::EngineRepairParams rp;
    rp.full_s = 10.0;
    combat::DamageState d = dead_stick();
    int restarts = 0, restores = 0;
    for (int i = 0; i < 2000; ++i) {
        const combat::EngineRepair r =
            combat::repair_engine_tick(d, 1.0 / 60.0, rp);
        if (r.restarted) ++restarts;
        if (r.restored) ++restores;
    }
    CHECK(restarts == 1);
    CHECK(restores == 1);
    CHECK(d.engine == 1.0);
}

// ★ "IS THERE A JOB HERE" IS ONE PREDICATE, and the site table, the prompt and
// the finish condition all ask it. `engine_needs_repair` is deliberately
// `< 1.0` and not `<= 0.0`: the ENGINE OUT plate is the case Chad NAMED, but
// the pump he compared it to is repairable while dead OR merely damaged, and
// "the same way" is the ask.
TEST_CASE("engine repair: a damaged engine is a job, a whole one is not") {
    combat::DamageState d;
    CHECK(!combat::engine_needs_repair(d));
    d.engine = 0.999;
    CHECK(combat::engine_needs_repair(d));
    d.engine = 0.0;
    CHECK(combat::engine_needs_repair(d));
}

// ★★★ THE MODE HOOK, END TO END, WITH NO WORLD. The U key starts the job, the
// same key ends it, walking away ends it, and finishing it ends it with a
// DIFFERENT sentence -- all of that is the pump rung's law and the engine now
// rides it.
TEST_CASE("player mode: the engine is fixed the same way the pump is") {
    app::PlayerModeState st;
    app::player_mode_force(st, app::PlayerMode::Afoot);
    app::ModeContext ctx;
    ctx.man_upright = true;
    ctx.engine_in_reach = true;

    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
          app::ModeAction::BeginRepair);
    CHECK(st.mode == app::PlayerMode::Repairing);
    CHECK(st.repairing);
    CHECK(st.repair_target == app::RepairTarget::Engine);
    // ⚠ AND IT IS NOT A PUMP. `repair_pump` is "which pump", never "which
    // job", and a 0 here would aim the pump tick at pump zero.
    CHECK(st.repair_pump == -1);

    // Standing still with the job unfinished holds it.
    CHECK(app::player_mode_update(st, ctx) == app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Repairing);

    // The same key backs out, and clears the whole hook.
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
          app::ModeAction::EndRepair);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);
    CHECK(st.repair_target == app::RepairTarget::None);
}

// ★ WALKING AWAY FROM THE AEROPLANE ENDS THE ENGINE JOB -- and it is the
// AEROPLANE'S reach that ends it.
//
// WRONG IMPLEMENTATION: `player_mode_update` gated on `!ctx.pump_in_reach`
// alone (the code as it shipped for the pump). Under it the engine job is
// abandoned on the very first tick, because a man at an aeroplane is not at a
// pump. RED here, green on every pump leg.
TEST_CASE("player mode: the engine job is ended by its OWN site, not the pump's") {
    app::PlayerModeState st;
    app::player_mode_force(st, app::PlayerMode::Afoot);
    app::ModeContext ctx;
    ctx.man_upright = true;
    ctx.engine_in_reach = true;
    ctx.pump_in_reach = false;  // there is no pump for a hundred kilometres
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
            app::ModeAction::BeginRepair);
    CHECK(app::player_mode_update(st, ctx) == app::ModeAction::None);

    // He walks off. The engine site leaves reach and the job ends, abandoned.
    ctx.engine_in_reach = false;
    CHECK(app::player_mode_update(st, ctx) == app::ModeAction::EndRepair);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(st.repair_target == app::RepairTarget::None);

    // And a FINISHED engine leaves reach too (the site table drops it the
    // instant it is whole), which is a different sentence.
    app::player_mode_force(st, app::PlayerMode::Afoot);
    ctx.engine_in_reach = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
            app::ModeAction::BeginRepair);
    ctx.engine_in_reach = false;
    ctx.repair_complete = true;
    CHECK(app::player_mode_update(st, ctx) == app::ModeAction::FinishRepair);
}

// ★★★ THE PUMP WINS THE TIE, AND THE TIE IS REAL. Land a dead stick at your
// own damaged pump and the man is standing inside both reaches. The pump is
// the one with a match clock running against it.
TEST_CASE("player mode: with both in reach the U key takes the pump") {
    app::PlayerModeState st;
    app::player_mode_force(st, app::PlayerMode::Afoot);
    app::ModeContext ctx;
    ctx.man_upright = true;
    ctx.pump_in_reach = true;
    ctx.pump_index = 1;
    ctx.engine_in_reach = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
            app::ModeAction::BeginRepair);
    CHECK(st.repair_target == app::RepairTarget::Pump);
    CHECK(st.repair_pump == 1);
}

// ★ AND THE FIX IS REFUSED FROM A MAN WHO IS NOT ON HIS FEET -- the red-team
// finding of 2026-09-01, asked again of the new site so the two verbs cannot
// drift apart about it. `PlayerMode::Afoot` means the keys are his, not that
// he is standing.
TEST_CASE("player mode: no engine fix from a man face-down in the snow") {
    app::PlayerModeState st;
    app::player_mode_force(st, app::PlayerMode::Afoot);
    app::ModeContext ctx;
    ctx.engine_in_reach = true;
    ctx.man_upright = false;  // Falling / Buried / Down / CrawlProne
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
          app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(st.repair_target == app::RepairTarget::None);
}

// ★★★ A FORCED MODE NEVER STRANDS THE ENGINE HOOK. A death, a respawn or a
// righting can interrupt the job, and a `repair_target` left at Engine would
// make the next `player_mode_update` ask the aeroplane's reach about a man who
// has been reborn 300 m away on a fresh machine.
//
// WRONG IMPLEMENTATION: `player_mode_force` clearing `repair_pump` but not
// `repair_target`.
TEST_CASE("player mode: a forced mode never strands the engine hook") {
    app::PlayerModeState st;
    app::player_mode_force(st, app::PlayerMode::Afoot);
    app::ModeContext ctx;
    ctx.man_upright = true;
    ctx.engine_in_reach = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey, ctx) ==
            app::ModeAction::BeginRepair);
    app::player_mode_force(st, app::PlayerMode::Sled);
    CHECK(st.mode == app::PlayerMode::Sled);
    CHECK(!st.repairing);
    CHECK(st.repair_target == app::RepairTarget::None);
    CHECK(st.repair_pump == -1);
    // And with the hook clear, an update at a man nowhere near anything is a
    // no-op rather than a spurious "FIX ABANDONED".
    app::ModeContext nowhere;
    CHECK(app::player_mode_update(st, nowhere) == app::ModeAction::None);
}

// ★ THE SITE LAYER: the aeroplane carries TWO verbs, both keys stay armed, and
// the engine's is the one the PROMPT offers (registration order breaks the
// tie -- see app/main.cpp's site table).
TEST_CASE("interact: the aeroplane offers the wrench and the ladder at once") {
    app::InteractSite sites[2];
    const glm::dvec3 plane = parked_aircraft();
    sites[0].kind = app::SiteKind::EngineRepair;
    sites[0].pos_w = plane;
    sites[0].reach_m = 5.0;
    sites[1].kind = app::SiteKind::AircraftBoard;
    sites[1].pos_w = plane;
    sites[1].reach_m = 3.0;

    // Standing at the nose: the prompt is the wrench...
    const glm::dvec3 man = man_at(1.0);
    const app::InteractSite at_nose =
        app::nearest_site(sites, 2, man, app::PlayerMode::Afoot);
    CHECK(at_nose.kind == app::SiteKind::EngineRepair);
    CHECK(std::string(app::interact_prompt(at_nose)) == "U  FIX ENGINE");

    // ...and BOTH keys are armed, which is the per-kind context's whole point.
    const app::ModeContext c = app::mode_context_from(
        sites, 2, man, app::PlayerMode::Afoot, /*aircraft_ready=*/true,
        /*sled_seeded=*/false, /*sled_stopped=*/true, /*man_upright=*/true,
        /*repair_complete=*/false);
    CHECK(c.engine_in_reach);
    CHECK(c.aircraft_in_reach);

    // Out past the boarding reach but inside the wrench's: only the wrench.
    const app::ModeContext out_wide = app::mode_context_from(
        sites, 2, man_at(4.0), app::PlayerMode::Afoot, true, false, true, true,
        false);
    CHECK(out_wide.engine_in_reach);
    CHECK(!out_wide.aircraft_in_reach);
}

// ★ AND THE SITE STAYS LIVE WHILE HE WORKS, so walking away can end the job.
// A `Repairing` mode that hid its own site would abandon the fix on the frame
// after it started.
TEST_CASE("interact: the engine site is legal while he is repairing") {
    CHECK(app::site_legal_in_mode(app::SiteKind::EngineRepair,
                                  app::PlayerMode::Repairing));
    CHECK(app::site_legal_in_mode(app::SiteKind::PumpRepair,
                                  app::PlayerMode::Repairing));
    // The ladder is NOT: you do not board an aeroplane mid-wrench.
    CHECK(!app::site_legal_in_mode(app::SiteKind::AircraftBoard,
                                   app::PlayerMode::Repairing));
    // And nothing is live from the cockpit or the saddle.
    CHECK(!app::site_legal_in_mode(app::SiteKind::EngineRepair,
                                   app::PlayerMode::Pilot));
    CHECK(!app::site_legal_in_mode(app::SiteKind::EngineRepair,
                                   app::PlayerMode::Sled));
}
