// ★★★ DF1-P1 — THE CONTROLLED A/B. Written because THE FLY DID NOT SETTLE IT.
//
// Tape 18 (Chad's first fly of RUNG DF-1) showed the rung hitting its target
// exactly: of the climb commanded in a deck fight, the BFM share went
// 81.6% -> 0% and the peak command +44 deg -> +26.00 deg (pinned at the
// terrain-avoid value). Climbing departures out of good air fell 27 -> 3.
//
// ⚠ BUT THE SYMPTOM DID NOT MOVE. Starvation in deck scope (air fraction below
// 0.5) went 12.85% -> 14.33%, crashes per 1000 live samples 0.23 -> 0.25, and
// the deck-fight altitude p90 503 m -> 504 m. TWO DIFFERENT MATCHES CANNOT
// SETTLE THAT — different lengths, different pump states, different attrition.
//
// THE HYPOTHESIS THIS PROBE EXISTS TO REFUTE OR CONFIRM: the cap RELOCATES the
// climb rather than removing it. Terrain-avoid climb in deck scope tripled in
// the same tape (2.12% -> 6.87%) and terrain-avoid's share of starved ticks
// went 3.1% -> 8.6%. Flying flatter puts them nearer the ground, which trips
// the pull-up more often — and the pull-up commands +26 deg, STEEPER than most
// of what BFM was asking for.
//
// ONE world, ONE deterministic player script, ONE spawn, and the ONLY
// difference between the arms is deck_fight_climb_cap. That is the thing two
// tapes could not give.
//
// ★★★ THE ANSWER, MEASURED 2026-09-01 (180 s, 10 pilots, ~215k deck samples
// per arm, ONLY the dial moving):
//
//     arm    pursuit climb   pull-up climb   starved   crashes
//     OFF        12.50%          4.55%        22.44%      3
//     CAP         0.00%          6.86%         1.59%      0
//
// THE RELOCATION IS REAL BUT IT IS NOT THE STORY. Pull-up climb DOES rise
// (+2.31 points) exactly as the hypothesis predicted — flying flatter does trip
// the terrain latch more often. But starvation falls by a factor of FOURTEEN
// and the crashes go to zero, so the relocation is swamped many times over.
// The cap is a large net win on the very number it was suspected of harming.
//
// ⚠ THEREFORE THE TAPE 17 -> 18 STARVATION RISE (12.85% -> 14.33%) WAS NOT
// DF-1. Two matches of different length, pump state and attrition cannot carry
// a 1.5-point difference, and this A/B — which holds all of that fixed — moves
// the same statistic by 20.8 points in the OPPOSITE direction. The honest
// reading of the fly is "the rung worked and the match was different", and this
// probe is what makes that a measurement instead of a preference.
//
// ★ IT IS WRITTEN SO IT CAN REPORT AGAINST THE RUNG. The bounds below grade
// what DF-1 actually claims AND the relocation that would refute it; every
// number is printed via INFO on each run, so a failure names the mechanism
// instead of just going red.
//
// TEST_CASE names are strictly ASCII with NO COMMA (a Catch2 name containing a
// comma cannot be selected by the exe's own filter and silently runs nothing).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "combat/conquest.h"
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

struct Arm {
    double cap = 0.0;
    int deck = 0;
    int ta_climb = 0;
    int other_climb = 0;
    int starved = 0;
    int crashes = 0;
    int live = 0;
    double agl_sum = 0.0;
};

double pct(int n, int d) {
    return d > 0 ? 100.0 * static_cast<double>(n) / static_cast<double>(d) : 0.0;
}

}  // namespace

TEST_CASE("DF1-P1 probe: the cap removes pursuit climb without relocating it "
          "into the pull-up",
          "[df1][probe]") {
    const world::HeightField hf = flat_field();
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;

    // A fight far from BOTH dome centres, so deck scope is a fact of the world
    // and not a dial: the antipode of the two centres' midpoint.
    const glm::dvec3 mid =
        glm::normalize(glm::normalize(world::kValleyCenterDir) +
                       glm::normalize(world::kSudburyCenterDir));
    const glm::dvec3 c = -mid;

    Arm arms[2];
    arms[0].cap = -1.0;  // OFF = the pre-DF-1 baseline
    arms[1].cap = 0.0;   // the SHIPPED ruling

    for (Arm& a : arms) {
        sim::Environment env;
        env.ground = &hf;
        env.ground_params = test_gp();
        env.atm = &af;

        drone::DroneParams dp = kScen.drone;
        dp.maverick.enabled = false;
        dp.deck_fight_climb_cap = a.cap;  // THE ONLY THING THAT MOVES
        dp.count = combat::kNumMavericks;
        dp.engage_range = 1.0e9;
        dp.disengage_range = 1.0e9;

        // ⚠ THE PLAYER FLIES ABOVE THE FLEET ON PURPOSE. At 150 m against a
        // 250 m spawn the baseline arm commanded pursuit climb on only 0.98%
        // of deck ticks — below this probe's own non-vacuity floor, i.e. a
        // fixture that barely shows the defect it is grading. Chad's report is
        // specifically about the pull-up "when I'm near", so the merge geometry
        // must actually ASK for a climb: he sits ~350 m above their lane.
        const double kPlayerAgl = 600.0;
        sim::SimState player;
        player.position = c * (hf.radius_at(c) + kPlayerAgl);
        glm::dvec3 ref{0.0, 1.0, 0.0};
        if (std::abs(glm::dot(ref, c)) > 0.9) ref = glm::dvec3{1.0, 0.0, 0.0};
        const glm::dvec3 east = glm::normalize(glm::cross(ref, c));
        const glm::dvec3 north = glm::cross(c, east);
        glm::dvec3 phdg = east;
        player.velocity = 167.0 * phdg;
        player.last_vhat = phdg;

        std::vector<drone::DroneState> fleet;
        const double golden = kPi * (3.0 - std::sqrt(5.0));
        for (int i = 0; i < dp.count; ++i) {
            const double brg = golden * static_cast<double>(i);
            const double ang = 3000.0 / kAp.R;
            const glm::dvec3 dir = glm::normalize(
                std::cos(ang) * c +
                std::sin(ang) * (std::cos(brg) * east + std::sin(brg) * north));
            const glm::dvec3 pos = dir * (hf.radius_at(dir) + 250.0);
            const glm::dvec3 hdg =
                std::cos(brg) * east + std::sin(brg) * north;
            drone::DroneState d;
            d.curr = drone::level_state_at(dp, pos, hdg);
            d.prev = d.curr;
            d.hp = dp.hp;
            d.spawn_index = i;
            d.fleet_count = dp.count;
            fleet.push_back(d);
        }

        const int ticks = static_cast<int>(180.0 / kAp.sim_dt);
        for (int t = 0; t < ticks; ++t) {
            const glm::dvec3 up = glm::normalize(player.position);
            phdg = glm::normalize(phdg - glm::dot(phdg, up) * up);
            phdg = glm::normalize(glm::angleAxis(0.05 * kAp.sim_dt, up) * phdg);
            const glm::dvec3 axis = glm::normalize(glm::cross(up, phdg));
            glm::dvec3 npos = glm::angleAxis(167.0 * kAp.sim_dt / kAp.R, axis) *
                              player.position;
            const glm::dvec3 nd = glm::normalize(npos);
            npos = nd * (hf.radius_at(nd) + kPlayerAgl);
            player.position = npos;
            player.velocity = 167.0 * phdg;
            player.last_vhat = phdg;

            drone::assign_engagements(
                fleet, player, static_cast<int>(fleet.size()), dp);
            for (drone::DroneState& d : fleet) {
                if (d.inert) continue;
                d.leash = drone::BubbleLeash{};
                const drone::DroneTickResult r =
                    drone::tick(d, kAp, dp, &env, &player);
                if (r.respawned) ++a.crashes;
                ++a.live;
                a.agl_sum += agl_of(hf, d.curr.position);
                if (!d.deck_scope) continue;
                ++a.deck;
                if (sim::atm_frac_at(d.curr.position, &env, kAp) < 0.5)
                    ++a.starved;
                if (d.cmd_gamma > 0.05) {
                    if (d.terrain_avoid_engaged)
                        ++a.ta_climb;
                    else
                        ++a.other_climb;
                }
            }
        }
    }

    const Arm& off = arms[0];
    const Arm& on = arms[1];
    INFO("OFF deck=" << off.deck << " pursuit=" << pct(off.other_climb, off.deck)
                     << "% pullup=" << pct(off.ta_climb, off.deck)
                     << "% starved=" << pct(off.starved, off.deck)
                     << "% crashes=" << off.crashes << " meanAGL="
                     << (off.live ? off.agl_sum / off.live : 0.0));
    INFO("CAP deck=" << on.deck << " pursuit=" << pct(on.other_climb, on.deck)
                     << "% pullup=" << pct(on.ta_climb, on.deck)
                     << "% starved=" << pct(on.starved, on.deck)
                     << "% crashes=" << on.crashes << " meanAGL="
                     << (on.live ? on.agl_sum / on.live : 0.0));

    // Non-vacuous: the fixture really is a deck fight in BOTH arms.
    REQUIRE(off.deck > 1000);
    REQUIRE(on.deck > 1000);
    // The baseline arm must show the defect, or there is nothing to fix.
    REQUIRE(pct(off.other_climb, off.deck) > 1.0);
    // WHAT THE RUNG CLAIMS: the pursuit climb is removed.
    REQUIRE(pct(on.other_climb, on.deck) <
            0.25 * pct(off.other_climb, off.deck));
    // ★ AND THE CLAIMS THAT COULD REFUTE IT. Stated as bounds a relocation
    // would BREAK, not as a hope: the cap must not simply hand the climb to
    // the pull-up, and must not leave the aeroplane starving more than before.
    REQUIRE(pct(on.ta_climb, on.deck) <= pct(off.ta_climb, off.deck) + 5.0);
    REQUIRE(pct(on.starved, on.deck) <= pct(off.starved, off.deck) + 2.0);
}
