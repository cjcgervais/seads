// ★★★ RUNG DF-1 — THE DECK FIGHT. Chad's ruling 2026-08-31, off his fly of
// tape 17:
//
//   "their habit of pulling up when I'm near sets them into the no air spin
//    zone, when fighting on the deck they should only be performing low
//    altitude bank turning without a climb vector because it sends them flying
//    out of the fight ... My last two kills I got because they were flying out
//    of control and I got an easy snap shot."
//
// ⚠ IT NARROWS A RULING THAT WAS ALREADY SIGNED. drone/drone.h's track-law
// seam exempts every ENGAGED tick — "Chad's 'chase you anywhere' means BFM owns
// the nose." That still holds in the HORIZONTAL. What DF-1 takes back is the
// VERTICAL, and only in deck scope. A2 below grades exactly that split: gamma
// is capped, bank is NOT touched.
//
// ★ WHY THE EXISTING AIR CEILING CANNOT DO THIS JOB, and why this rung is not
// a duplicate of the flyable-air fade already covered in test_conquest_furball:
// that fade scales commanded climb by the air fraction AT THE DRONE'S OWN
// POSITION. In the deck lane that reads FULL air, so the fade is exactly 1.0
// and the pursuit climb passes through unopposed — it only bites once the
// aeroplane is already in the thin air it should never have entered. A4 pins
// that: with DF-1 off, a deck-scope pursuit DOES climb out into low air.
//
// Every arm here is DIFFERENTIAL against the shipped off-value
// (deck_fight_climb_cap < 0), so "the cap did something" is proven and never
// assumed — the house rule that an A/B proving equality must first prove its
// arms differed (memory: r4a-grip-release-rung).
//
// TEST_CASE/SECTION names are strictly ASCII with NO COMMA (a Catch2 name
// containing a comma cannot be selected by the exe's own filter and silently
// runs nothing — see the lane handoff §6).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"

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

// A generic surface point far from BOTH faction dome centres, so the drone is
// in deck scope by construction (no dome air overhead) rather than by a dial.
// Deliberately not aligned with any world axis (the CLAUDE.md generic-frame
// discipline).
const glm::dvec3 kUp = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
const glm::dvec3 kHeading =
    glm::normalize(glm::cross(kUp, glm::dvec3{0.2, 0.9, 0.4}));

world::HeightField flat_field() {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = 4000.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    return hf;
}

// The REAL atmosphere the plant's lift reads (never a flat stub): both faction
// domes at base growth plus the global terrain-relative deck.
struct Air {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    sim::AtmosphereField af;
    Air() {
        const world::FactionGrowth grow[2]{};
        world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
        af.deck_agl_m = kGame.atmosphere.deck_agl_m;
        af.deck_soft_m = kGame.atmosphere.deck_soft_m;
        af.bubbles = bubbles;
    }
};

// The ground-contact params sim::step reads (test_conquest_furball's shape).
// ⚠ NOT optional: world::HeightField asserts probe_m > 0 the first time a
// surface normal is taken, so an env with ground but no ground_params does not
// fail the assertion under test — it aborts the process.
sim::GroundParams test_gp() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 1.5;
    return gp;
}

double agl_of(const world::HeightField& hf, const glm::dvec3& p) {
    return glm::length(p) - hf.radius_at(glm::normalize(p));
}

// A drone in the deck lane at kUp, engaged, with the player HIGH above it —
// pursue()'s elevation term then commands the steepest allowed climb every
// tick, which is exactly the merge geometry Chad described ("pulling up when
// I'm near").
struct Fixture {
    world::HeightField hf = flat_field();
    Air air;
    sim::Environment env;
    drone::DroneState d;
    sim::SimState player;

    explicit Fixture(const drone::DroneParams& dp, double agl = 100.0) {
        env.ground = &hf;
        env.ground_params = test_gp();
        env.atm = &air.af;
        const glm::dvec3 pos = kUp * (hf.radius_at(kUp) + agl);
        d.curr = drone::level_state_at(dp, pos, kHeading);
        d.prev = d.curr;
        d.hp = dp.hp;
        d.spawn_index = 0;
        d.fleet_count = 1;
        d.engaged = true;  // the seam DF-1 acts on: an ENGAGED deck tick
        player.position = kUp * (hf.radius_at(kUp) + agl + 4000.0);
        player.velocity = glm::dvec3{0.0};
    }
};

drone::DroneParams shipped() {
    drone::DroneParams dp = kScen.drone;
    dp.maverick.enabled = false;  // isolate the fight seam from the run machine
    return dp;
}

}  // namespace

// ---------------------------------------------------------------------------

TEST_CASE("DF1-A1 the shipped dial is his ruling and the off value is reachable",
          "[df1]") {
    // 0.0 deg = "without a climb vector" verbatim. It is a REAL cap: the OFF
    // value is NEGATIVE, so a walk-back can never be confused with the ruling.
    REQUIRE(shipped().deck_fight_climb_cap == 0.0);
    // And the struct default must be the off value, so any caller that does not
    // load the shipped table is bit-identical to the pre-DF-1 tick.
    REQUIRE(drone::DroneParams{}.deck_fight_climb_cap < 0.0);
}

TEST_CASE("DF1-A2 an engaged deck pursuit is not allowed a climb vector and "
          "the OFF arm climbs on the identical setup",
          "[df1]") {
    drone::DroneParams on = shipped();
    drone::DroneParams off = on;
    off.deck_fight_climb_cap = -1.0;  // the one dial that moves

    Fixture fon(on);
    Fixture foff(off);

    int on_climb_ticks = 0, off_climb_ticks = 0;
    double on_max_gamma = -10.0, off_max_gamma = -10.0;
    int on_deck_ticks = 0, off_deck_ticks = 0;
    const int ticks = static_cast<int>(60.0 / kAp.sim_dt);
    for (int i = 0; i < ticks; ++i) {
        drone::tick(fon.d, kAp, on, &fon.env, &fon.player);
        drone::tick(foff.d, kAp, off, &foff.env, &foff.player);
        // Count only the ticks the rung actually governs: deck scope, engaged,
        // and NOT the terrain-avoid pull-up (which DF-1 deliberately leaves
        // able to override everything — graded on its own in A3).
        if (fon.d.deck_scope && !fon.d.terrain_avoid_engaged) {
            ++on_deck_ticks;
            on_max_gamma = std::max(on_max_gamma, fon.d.cmd_gamma);
            if (fon.d.cmd_gamma > 1.0e-9) ++on_climb_ticks;
        }
        if (foff.d.deck_scope && !foff.d.terrain_avoid_engaged) {
            ++off_deck_ticks;
            off_max_gamma = std::max(off_max_gamma, foff.d.cmd_gamma);
            if (foff.d.cmd_gamma > 1.0e-9) ++off_climb_ticks;
        }
    }

    INFO("ON  deck ticks " << on_deck_ticks << " climb ticks " << on_climb_ticks
                           << " max gamma " << on_max_gamma);
    INFO("OFF deck ticks " << off_deck_ticks << " climb ticks "
                           << off_climb_ticks << " max gamma "
                           << off_max_gamma);

    // THE FIXTURE MUST SHOW THE BASELINE DEFECT FIRST. Without this the arm
    // could pass on a drone that simply never wanted to climb.
    REQUIRE(off_deck_ticks > 0);
    REQUIRE(off_climb_ticks > 0);
    REQUIRE(off_max_gamma > 5.0 * kPi / 180.0);

    // The ruling: no climb vector on an engaged deck tick.
    REQUIRE(on_deck_ticks > 0);
    REQUIRE(on_climb_ticks == 0);
    REQUIRE(on_max_gamma <= shipped().deck_fight_climb_cap + 1.0e-12);
}

TEST_CASE("DF1-A5 the cap takes the vertical and leaves the turn alone",
          "[df1]") {
    // Chad kept "chase you anywhere" in the HORIZONTAL. On the first tick both
    // arms share identical state by construction, so any difference is the cap
    // itself: gamma must move and bank must not. (Later ticks diverge because
    // the trajectories do — that is not evidence about the seam.)
    drone::DroneParams on = shipped();
    drone::DroneParams off = on;
    off.deck_fight_climb_cap = -1.0;

    Fixture fon(on);
    Fixture foff(off);
    REQUIRE(fon.d.curr.position == foff.d.curr.position);

    drone::tick(fon.d, kAp, on, &fon.env, &fon.player);
    drone::tick(foff.d, kAp, off, &foff.env, &foff.player);

    INFO("gamma on " << fon.d.cmd_gamma << " off " << foff.d.cmd_gamma);
    INFO("bank  on " << fon.d.cmd_bank << " off " << foff.d.cmd_bank);
    REQUIRE(fon.d.deck_scope);
    REQUIRE(foff.d.cmd_gamma > fon.d.cmd_gamma);  // the arms DID differ
    REQUIRE(fon.d.cmd_bank == foff.d.cmd_bank);   // and only in the vertical
}

TEST_CASE("DF1-A6 the cap only ever removes climb and never restricts descent",
          "[df1]") {
    // ★ THIS ARM EXISTS BECAUSE A MUTATION SURVIVED WITHOUT IT. Replacing the
    // std::min with a plain assignment (`target_gamma = cap`) left A1-A5 all
    // green, and that build is a DIFFERENT law: at cap 0.0 it would RAISE a
    // commanded dive to level flight, fighting both the pursuit's descent and
    // the air-seek dive that exists to get a starved drone back down to the
    // lane. A constraint stage may only ever lower a command.
    drone::DroneParams on = shipped();
    drone::DroneParams off = on;
    off.deck_fight_climb_cap = -1.0;

    Fixture fon(on, 900.0);
    Fixture foff(off, 900.0);
    // Player BELOW, so pursue commands a descent on an engaged deck tick.
    fon.player.position = fon.d.curr.position - kUp * 5000.0;
    foff.player.position = foff.d.curr.position - kUp * 5000.0;

    bool saw_dive = false;
    const int ticks = static_cast<int>(20.0 / kAp.sim_dt);
    for (int i = 0; i < ticks; ++i) {
        drone::tick(fon.d, kAp, on, &fon.env, &fon.player);
        drone::tick(foff.d, kAp, off, &foff.env, &foff.player);
        if (fon.d.cmd_gamma < -1.0e-9) saw_dive = true;
        // A pure lowering cannot change a command that is already below the
        // cap, so the two arms must stay bit-identical for the whole descent.
        REQUIRE(fon.d.cmd_gamma == foff.d.cmd_gamma);
        REQUIRE(fon.d.curr.position == foff.d.curr.position);
    }
    // Non-vacuous: it really was diving, so the equality is about the cap.
    REQUIRE(saw_dive);
    REQUIRE(fon.d.deck_scope);
}

TEST_CASE("DF1-A3 pulling up still always wins over the cap", "[df1]") {
    // The safety law downstream must be untouched: a drone driven at the dirt
    // must still command a POSITIVE gamma with the cap at its most restrictive
    // shipped value. If DF-1 had been applied after the pull-up instead of
    // before it, this is the arm that catches it.
    drone::DroneParams on = shipped();
    Fixture f(on, 1800.0);
    // Player straight DOWN and deep, so pursue commands the steepest dive and
    // drives the drone into the avoid band.
    f.player.position = f.d.curr.position - kUp * 6000.0;

    bool ever_avoided = false;
    bool climbed_while_avoiding = false;
    double min_agl = 1800.0;
    const int ticks = static_cast<int>(90.0 / kAp.sim_dt);
    for (int i = 0; i < ticks; ++i) {
        const drone::DroneTickResult r =
            drone::tick(f.d, kAp, on, &f.env, &f.player);
        if (r.respawned) break;  // a crash here is the failure A3 exists to see
        min_agl = std::min(min_agl, agl_of(f.hf, f.d.curr.position));
        if (f.d.terrain_avoid_engaged) {
            ever_avoided = true;
            if (f.d.cmd_gamma > 0.0) climbed_while_avoiding = true;
        }
    }
    INFO("min AGL " << min_agl);
    REQUIRE(ever_avoided);              // the fixture reached the band
    REQUIRE(climbed_while_avoiding);    // and the pull-up out-ranked the cap
    REQUIRE(min_agl > 0.0);             // and it did not auger in
}

TEST_CASE("DF1-A4 the cap is deck scope only and is inert inside a dome",
          "[df1]") {
    // Inside a live dome deck_scope is false, so DF-1 must not fire at all —
    // the energy fight in good air is untouched (ENV-2's ruling).
    drone::DroneParams on = shipped();
    drone::DroneParams off = on;
    off.deck_fight_climb_cap = -1.0;

    Air air;
    sim::Environment env;
    env.atm = &air.af;
    const glm::dvec3 c = glm::normalize(world::kValleyCenterDir);
    const glm::dvec3 pos = c * (kAp.R + 2000.0);

    drone::DroneState a;
    a.curr = drone::level_state_at(on, pos, kHeading);
    a.prev = a.curr;
    a.hp = on.hp;
    a.engaged = true;
    drone::DroneState b = a;

    sim::SimState player;
    player.position = c * (kAp.R + 9000.0);  // high, inside the dome
    player.velocity = glm::dvec3{0.0};

    bool any_climb = false;
    const int ticks = static_cast<int>(30.0 / kAp.sim_dt);
    for (int i = 0; i < ticks; ++i) {
        drone::tick(a, kAp, on, &env, &player);
        drone::tick(b, kAp, off, &env, &player);
        if (a.cmd_gamma > 1.0e-9) any_climb = true;
        // Bit-identical: the cap cannot reach a non-deck tick at all.
        REQUIRE(a.cmd_gamma == b.cmd_gamma);
        REQUIRE(a.cmd_bank == b.cmd_bank);
        REQUIRE(a.curr.position == b.curr.position);
    }
    // Non-vacuous: the in-dome pursuit really did climb, so the equality above
    // is a statement about the cap and not about a drone that never climbed.
    REQUIRE(any_climb);
    REQUIRE_FALSE(a.deck_scope);
}
