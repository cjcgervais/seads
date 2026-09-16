// ★★★ L3 — WHERE THE PLAYER IS BORN
// (docs/PLAN_20260901_game_loop_millwright.md §4.3).
//
// WHAT THESE LEGS EXIST TO CATCH.
//
// A spawn policy is the easiest thing in this repo to get wrong invisibly. It
// runs ONCE, before anything is on the screen, and every failure mode looks the
// same from the cockpit: you are somewhere. Three of them are silent:
//
//   1. THE AIRCRAFT PATH MOVING. Every existing respawn, every golden, and the
//      two copy-pasted `spawn_state` calls in `app/instructor_tick.h` are now
//      routed through ONE function. If that routing is not byte-for-byte the
//      old call, the whole flight suite drifts from a rung that never claimed
//      to touch flight. `player_spawn is spawn_state on the Aircraft path`
//      compares the two states field by field, with an exact `==` on the
//      doubles, because "close enough" is precisely what would hide it.
//
//   2. THE TANGENT-PLANE SPAWN. 300 m of tangent on a 15 km ball leaves the
//      sphere by 3.0 m -- a machine hovering three metres over the snow, or
//      buried in it, depending on which way the ground rounds. The dismount
//      block already paid this at 0.9 m. `the machine is set down 300 m from
//      the pump` measures the GREAT-CIRCLE arc, which a tangent offset fails.
//
//   3. THE BEARING RUNNING THE WRONG WAY. "On the bearing from that pump toward
//      the own bubble centre (your side of it)" is one sign. Get it backwards
//      and the distance is still 300 m, the heading still points at the pump,
//      and the player is simply spawned on the CONTESTED side every time --
//      which reads as bad luck, not as a bug. The named wrong implementation is
//      "the bearing runs from the pump AWAY from the centre", and
//      `the machine is set down on the friendly side of the pump` is red under
//      exactly that and green under nothing else.
//
// And one that is loud but easy to ship: the MENU GATE. The plan's rule has
// three inputs and four cases; `the menu is offered at the first spawn and only
// at a damaged respawn` walks all of them, including the headless one, because
// a menu that opens under --smoke is a HANG in CI, not a menu.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/spawn_policy.h"
#include "combat/conquest.h"
#include "sim/params.h"
#include "sim/sled.h"
#include "world/faction_bubbles.h"  // the baked pump anchors + dome centres
#include "world/tunnel_geo.h"       // the tunnel-home spawn spec

namespace {

constexpr double kR = 15000.0;  // the planet, SPEC §1

sim::AircraftParams test_ap() {
    sim::AircraftParams p;
    p.R = kR;
    return p;
}

// Surface arc between two directions [m].
double arc_m(const glm::dvec3& a, const glm::dvec3& b) {
    const double c =
        glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0);
    return kR * std::acos(c);
}

// A conquest fixture with the shipped pump roster shape: [0] VALLEY surface,
// [1] SUDBURY surface, [2]/[3] deep. The player flies VALLEY, which is the
// shipped default (config/game.toml [conquest] player_faction).
combat::ConquestState fixture_state() {
    combat::ConquestState cs;
    cs.player_faction = combat::CQ_VALLEY;
    const glm::dvec3 valley = glm::normalize(world::kPumpValleySurface) * kR;
    const glm::dvec3 sudbury = glm::normalize(world::kPumpSudburySurface) * kR;
    combat::make_pumps(valley, sudbury, valley * 0.5, sudbury * 0.5, 60.0,
                       cs.pumps);
    return cs;
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. The aircraft path did not move.

TEST_CASE("player_spawn is spawn_state on the Aircraft path",
          "[spawn][policy]") {
    const sim::AircraftParams ap = test_ap();
    // The three shipped spawn specs: the legacy +X pole (every default), the
    // tunnel home over Errington, and a smoke-rig direction with an altitude
    // override. All three go through the routing function unchanged.
    struct Case {
        double alt;
        glm::dvec3 up;
        glm::dvec3 fwd;
    } cases[] = {
        {2000.0, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},
        {2500.0, world::kTunnelMouthErrington, world::kTunnelMouthMurray},
        {120.0, glm::normalize(glm::dvec3{0.3, -0.7, 0.5}),
         glm::dvec3{0.0, 1.0, 0.0}},
    };
    for (const Case& c : cases) {
        const sim::SimState a = app::spawn_state(ap, c.alt, c.up, c.fwd);
        const sim::SimState b = app::player_spawn(
            app::PlayerSpawnChoice::Aircraft, ap, c.alt, c.up, c.fwd);
        // EXACT. Not Approx: the claim is bit-identity, and an epsilon here
        // would pass a policy that quietly re-derived the same numbers a
        // different way.
        CHECK(b.position == a.position);
        CHECK(b.velocity == a.velocity);
        CHECK(b.orientation.w == a.orientation.w);
        CHECK(b.orientation.x == a.orientation.x);
        CHECK(b.orientation.y == a.orientation.y);
        CHECK(b.orientation.z == a.orientation.z);
        CHECK(b.last_vhat == a.last_vhat);
        CHECK(b.throttle == a.throttle);
        CHECK(b.gear == a.gear);
        CHECK(b.on_ground == a.on_ground);
    }
}

TEST_CASE("a Snowmachine choice with no placement falls back to the aircraft",
          "[spawn][policy]") {
    const sim::AircraftParams ap = test_ap();
    const sim::SimState want = app::spawn_state(ap);
    // Null placement, and an INVALID one: both are "the world could not put a
    // machine anywhere", and neither may leave the player at the origin.
    const app::SledSpawnPlacement dead;  // valid == false
    CHECK(app::player_spawn(app::PlayerSpawnChoice::Snowmachine, ap).position ==
          want.position);
    CHECK(app::player_spawn(app::PlayerSpawnChoice::Snowmachine, ap, 2000.0,
                            {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, &dead)
              .position == want.position);
}

// ---------------------------------------------------------------------------
// 2 + 3. The placement: distance, bearing, heading, and the parked aeroplane.

TEST_CASE("the machine is set down at the spawn distance from the pump",
          "[spawn][placement]") {
    app::SpawnDials d;  // shipped: 300 m, 5 m
    const glm::dvec3 pump = glm::normalize(world::kPumpValleySurface) * kR;
    const app::SledSpawnPlacement p =
        app::solve_sled_spawn(pump, world::kValleyCenterDir, kR, d);
    REQUIRE(p.valid);
    // The ARC, not the chord. A tangent-plane offset lands 3.0 m long here.
    CHECK(std::abs(arc_m(p.sled_dir, pump) - d.sled_dist_m) < 0.01);
    // ... and it is on the sphere at all.
    CHECK(std::abs(glm::length(p.sled_dir) - 1.0) < 1e-12);
    // A different dial moves it by exactly that much.
    d.sled_dist_m = 1200.0;
    const app::SledSpawnPlacement q =
        app::solve_sled_spawn(pump, world::kValleyCenterDir, kR, d);
    REQUIRE(q.valid);
    CHECK(std::abs(arc_m(q.sled_dir, pump) - 1200.0) < 0.01);
}

// ★★★ THE MUTATION LEG. Named wrong implementation: "the bearing runs from the
// pump AWAY from its own faction centre" (a sign flip on the tangent inside
// solve_sled_spawn). Under it the arc is still 300 m and the heading still
// points at the pump -- every other leg in this file stays green -- and the
// millwright is born on the contested side of his own pump every single time.
TEST_CASE("the machine is set down on the friendly side of the pump",
          "[spawn][placement]") {
    const app::SpawnDials d;
    for (int f = 0; f < 2; ++f) {
        const glm::dvec3 pump =
            glm::normalize(f == 0 ? world::kPumpValleySurface
                                  : world::kPumpSudburySurface) *
            kR;
        const glm::dvec3 centre =
            f == 0 ? world::kValleyCenterDir : world::kSudburyCenterDir;
        const app::SledSpawnPlacement p =
            app::solve_sled_spawn(pump, centre, kR, d);
        REQUIRE(p.valid);
        // CLOSER to the centre of his own dome than the pump is. The pumps sit
        // at the DISTAL end of each faction's major axis, so "toward the
        // centre" is unambiguously "deeper into friendly territory".
        CHECK(arc_m(p.sled_dir, centre) < arc_m(pump, centre));
        // And by very nearly the whole spawn distance -- the two arcs are
        // along the same great circle, so the difference is the step itself.
        const double closer = arc_m(pump, centre) - arc_m(p.sled_dir, centre);
        CHECK(std::abs(closer - d.sled_dist_m) < 1.0);
    }
}

TEST_CASE("the machine faces the pump it was sent to fix",
          "[spawn][placement]") {
    const app::SpawnDials d;
    const glm::dvec3 pump = glm::normalize(world::kPumpValleySurface) * kR;
    const app::SledSpawnPlacement p =
        app::solve_sled_spawn(pump, world::kValleyCenterDir, kR, d);
    REQUIRE(p.valid);
    // Tangent at his own feet (never a chord to the pump: on a sphere those are
    // different vectors, and the machine drives on the tangent).
    const glm::dvec3 want =
        app::tangent_toward(p.sled_dir, glm::normalize(pump));
    CHECK(glm::dot(p.sled_fwd, want) > 0.999999);
    CHECK(std::abs(glm::dot(p.sled_fwd, p.sled_dir)) < 1e-12);  // tangent
    // The seeded machine's own nose is that heading: sim/sled.h's body frame is
    // -Z forward, and this is the pair the drawn machine is built from.
    sim::SledState sled;
    app::seed_sled_at(sled, p.sled_dir * kR, p.sled_dir, p.sled_fwd);
    const glm::dvec3 nose = sled.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(glm::dot(nose, p.sled_fwd) > 0.999999);
    // ... and the R4a re-arm travelled with it (the reason seed_sled_at exists
    // at all: those four fields are not obvious enough to survive being
    // written twice).
    CHECK(sled.right_charge == 1.0);
    CHECK(sled.right_assist_armed);
}

TEST_CASE("the aeroplane is parked beside the machine, not on it",
          "[spawn][placement]") {
    const app::SpawnDials d;
    const glm::dvec3 pump = glm::normalize(world::kPumpValleySurface) * kR;
    app::SledSpawnPlacement p =
        app::solve_sled_spawn(pump, world::kValleyCenterDir, kR, d);
    REQUIRE(p.valid);
    CHECK(std::abs(arc_m(p.aircraft_dir, p.sled_dir) - d.aircraft_beside_m) <
          0.01);
    // Ground them both the way the app does, then ask the routing function for
    // the aeroplane: PARKED. Stopped, throttle shut, gear down, on the ground.
    p.sled_pos = p.sled_dir * (kR + 0.5);
    p.aircraft_pos = p.aircraft_dir * (kR + 1.0);
    const sim::AircraftParams ap = test_ap();
    const sim::SimState s =
        app::player_spawn(app::PlayerSpawnChoice::Snowmachine, ap, 2000.0,
                          {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, &p);
    CHECK(s.position == p.aircraft_pos);
    CHECK(glm::length(s.velocity) == 0.0);
    CHECK(s.throttle == 0.0);
    CHECK(s.gear == 1.0);
    CHECK(s.on_ground);
    // Level on the local surface, nose the way the machine faces (so "walk to
    // the plane and take off" starts pointed somewhere sane).
    const glm::dvec3 up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    CHECK(glm::dot(up, p.aircraft_dir) > 0.999999);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(glm::dot(nose, p.sled_fwd) > 0.999);
}

TEST_CASE("a degenerate pump bearing still places a machine",
          "[spawn][placement]") {
    const app::SpawnDials d;
    const glm::dvec3 pump{0.0, kR, 0.0};
    // Pump exactly AT its own faction centre: the bearing is undefined. A birth
    // function may not answer "nowhere".
    const app::SledSpawnPlacement p =
        app::solve_sled_spawn(pump, glm::dvec3{0.0, 1.0, 0.0}, kR, d);
    REQUIRE(p.valid);
    CHECK(std::abs(arc_m(p.sled_dir, pump) - d.sled_dist_m) < 0.01);
    // A pump at the origin is not a pump. That one IS refused.
    CHECK_FALSE(
        app::solve_sled_spawn(glm::dvec3{0.0}, world::kValleyCenterDir, kR, d)
            .valid);
}

// ---------------------------------------------------------------------------
// 4. The menu gate.

TEST_CASE(
    "the menu is offered at the first spawn and only at a damaged respawn",
    "[spawn][menu]") {
    combat::ConquestState cs = fixture_state();

    // First spawn: always, whatever the pumps are doing.
    CHECK(app::spawn_menu_should_show(true, true, true, cs));
    CHECK(app::spawn_menu_should_show(true, true, false, cs));
    // ... except headless. A menu waiting for a keypress under --smoke is a
    // hang, and a hang in CI reads as a timeout.
    CHECK_FALSE(app::spawn_menu_should_show(false, true, true, cs));

    // Respawn, every pump whole: the aeroplane, as today. This is the leg a
    // "menu on every respawn" mutation turns red.
    CHECK_FALSE(app::spawn_menu_should_show(true, false, true, cs));

    // Respawn with the OWN surface pump merely DAMAGED (alive, hp below max):
    // Chad's ask is "revive a lost pump", and the site is live while it is
    // dead OR damaged (the L1 site table's own rule, read the same way here).
    cs.pumps[combat::CQ_VALLEY].hp = 10.0;
    CHECK(app::spawn_menu_should_show(true, false, true, cs));
    CHECK(app::own_surface_pump_to_fix(cs) == combat::CQ_VALLEY);
    // ... and DEAD.
    cs.pumps[combat::CQ_VALLEY].alive = false;
    cs.pumps[combat::CQ_VALLEY].hp = 0.0;
    CHECK(app::spawn_menu_should_show(true, false, true, cs));
    // No conquest, no pumps, no menu -- even damaged.
    CHECK_FALSE(app::spawn_menu_should_show(true, false, false, cs));
}

TEST_CASE("the damaged pump the spawn aims at is the player's own surface pump",
          "[spawn][menu]") {
    combat::ConquestState cs = fixture_state();
    // Nothing broken: nothing to FIX, but the first spawn still has to put a
    // machine somewhere, so the target falls back to the own surface pump.
    CHECK(app::own_surface_pump_to_fix(cs) == -1);
    CHECK(app::spawn_target_pump(cs) == combat::CQ_VALLEY);

    // The ENEMY's surface pump in ruins is not the millwright's problem: it
    // arms no menu and it is never the spawn target.
    cs.pumps[combat::CQ_SUDBURY].alive = false;
    cs.pumps[combat::CQ_SUDBURY].hp = 0.0;
    CHECK(app::own_surface_pump_to_fix(cs) == -1);
    CHECK_FALSE(app::spawn_menu_should_show(true, false, true, cs));
    CHECK(app::spawn_target_pump(cs) == combat::CQ_VALLEY);

    // A DEEP pump of his own is not repairable by the player this rung (plan
    // §4.3 / question 2, default NO): it must not arm the menu either.
    cs.pumps[2].alive = false;
    cs.pumps[2].hp = 0.0;
    REQUIRE_FALSE(cs.pumps[2].surface);
    CHECK(app::own_surface_pump_to_fix(cs) == -1);
    CHECK_FALSE(app::spawn_menu_should_show(true, false, true, cs));

    // Flying the other side reads the other pump -- index == faction for the
    // surface pumps, and it is READ, never assumed.
    cs.player_faction = combat::CQ_SUDBURY;
    CHECK(app::own_surface_pump_to_fix(cs) == combat::CQ_SUDBURY);
    CHECK(app::spawn_target_pump(cs) == combat::CQ_SUDBURY);
}

// ---------------------------------------------------------------------------
// ★★★ THE SIDE'S HOME (Chad 2026-09-10: "I picked central city but my spawn
// was over the valley"). app/main.cpp used to type ONE spawn anchor --
// Errington, nose at Murray -- with no faction term anywhere in it, so both
// sides were born over the Valley. app::faction_home_dir/_fwd is now that
// anchor's one source, and every birth reads LoopState::spawn_up/fwd.
// ---------------------------------------------------------------------------

TEST_CASE("L11b the two sides are born at DIFFERENT homes", "[spawn]") {
    const glm::dvec3 v = app::faction_home_dir(combat::CQ_VALLEY);
    const glm::dvec3 s = app::faction_home_dir(combat::CQ_SUDBURY);
    // Two distinct, real, already-baked sites -- nothing invented here.
    CHECK(glm::length(v - world::kTunnelMouthErrington) == 0.0);
    CHECK(glm::length(s - world::kTunnelMouthMurray) == 0.0);
    // Both are unit directions (the spawn takes up * (R + alt)).
    CHECK(std::fabs(glm::length(v) - 1.0) < 1e-9);
    CHECK(std::fabs(glm::length(s) - 1.0) < 1e-9);
    // They are genuinely apart, not two names for one place.
    CHECK(glm::dot(v, s) < 0.9999);
}

TEST_CASE("L11b VALLEY's home is BIT-IDENTICAL to what main.cpp typed",
          "[spawn]") {
    // The expression app/main.cpp carried before this rung, re-typed here as
    // an independent witness. The shipped side must not move by one bit.
    const glm::dvec3 was_up = world::kTunnelMouthErrington;
    const glm::dvec3 was_fwd = glm::normalize(
        world::kTunnelMouthMurray -
        glm::dot(world::kTunnelMouthMurray, world::kTunnelMouthErrington) *
            world::kTunnelMouthErrington);
    const glm::dvec3 up = app::faction_home_dir(combat::CQ_VALLEY);
    const glm::dvec3 fwd = app::faction_home_fwd(combat::CQ_VALLEY);
    CHECK(up.x == was_up.x);
    CHECK(up.y == was_up.y);
    CHECK(up.z == was_up.z);
    CHECK(fwd.x == was_fwd.x);
    CHECK(fwd.y == was_fwd.y);
    CHECK(fwd.z == was_fwd.z);
}

TEST_CASE("L11b each side's nose points at the OTHER side's home", "[spawn]") {
    // "the raid is dead ahead" (Chad 2026-07-18) -- now true for both sides.
    for (int fac : {combat::CQ_VALLEY, combat::CQ_SUDBURY}) {
        const int other =
            fac == combat::CQ_SUDBURY ? combat::CQ_VALLEY : combat::CQ_SUDBURY;
        const glm::dvec3 up = app::faction_home_dir(fac);
        const glm::dvec3 fwd = app::faction_home_fwd(fac);
        // TANGENT at the spawn point: spawn_state re-orthogonalizes anyway,
        // and a nose with an up component is a plane born climbing or diving.
        CHECK(std::fabs(glm::dot(fwd, up)) < 1e-12);
        CHECK(std::fabs(glm::length(fwd) - 1.0) < 1e-12);
        // It leans TOWARD the other home, not away from it.
        CHECK(glm::dot(fwd, app::faction_home_dir(other)) > 0.0);
    }
}

TEST_CASE("L11b the spawn STATE really lands over the chosen home", "[spawn]") {
    // End to end through the one routing function, the way main.cpp calls it:
    // a Central City birth is over Murray, a Valley birth over Errington.
    const sim::AircraftParams p;
    for (int fac : {combat::CQ_VALLEY, combat::CQ_SUDBURY}) {
        const glm::dvec3 up = app::faction_home_dir(fac);
        const sim::SimState st =
            app::player_spawn(app::PlayerSpawnChoice::Aircraft, p, 2500.0, up,
                              app::faction_home_fwd(fac));
        CHECK(glm::dot(glm::normalize(st.position), up) > 1.0 - 1e-12);
        CHECK(std::fabs(glm::length(st.position) - (p.R + 2500.0)) < 1e-6);
    }
    // And the two births are nowhere near each other -- the defect was that
    // they were the SAME point.
    const sim::SimState a =
        app::player_spawn(app::PlayerSpawnChoice::Aircraft, p, 2500.0,
                          app::faction_home_dir(combat::CQ_VALLEY),
                          app::faction_home_fwd(combat::CQ_VALLEY));
    const sim::SimState b =
        app::player_spawn(app::PlayerSpawnChoice::Aircraft, p, 2500.0,
                          app::faction_home_dir(combat::CQ_SUDBURY),
                          app::faction_home_fwd(combat::CQ_SUDBURY));
    CHECK(glm::length(a.position - b.position) > 1000.0);
}
