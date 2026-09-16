// AI PHASE-2 FIX SPEC tests (docs/ai_phase2_fix_spec.md, Fable 2026-08-06):
// two tape-attributed conquest AI fixes.
//
// FIX-F2 -- THE VACUUM RESPAWN-STRAND PEN. The tape showed 95 dc (crash)
// events in a 14-minute match: a crash-respawned drone landed via the stock
// +X scatter (drone::spawn_state), which for the conquest fleet often sits
// 1-5 km outside its faction's bubble edge in thin air, so it mushes, crashes
// again, forever. The fix relocates a crash respawn into breathable air on the
// SAME tick.
//
// ★★★ S4 (Chad's ruling 2026-08-26, "this games ai must not teleport but
// become skilled at deck flying" / "they respawn from their zone at the
// deck") -- FIX-F2's FALLBACK USED TO BE THE SURVIVING FACTION'S DOME. That
// was the quiet second copy of the SCRAMBLE ON COLLAPSE teleport: it moved a
// dead-dome faction's pilots into the enemy's bubble one at a time as they
// died. Both are DELETED. The ladder is now
//   own dome alive -> own dome (app::place_in_faction_air, unchanged);
//   own dome gone  -> OWN ZONE AT THE DECK (app::place_on_faction_deck);
//   no heightfield -> the stock respawn (a fixture with no world).
// The two placements share ONE bearing law (app::plant_at_slot) -- pinned by
// the "the deck respawn and the dome respawn share one bearing law" case
// below -- and differ only in what the altitude is measured from.
//
// FIX-F3 -- THE RAID PAUSE GETS ITS OWN DIAL. The tape showed an enemy raid
// order dropping at rng_p = 6010 m == dparams.engage_range (6000) -- the raid
// pause latch (instructor_tick.h's raid_range_ok) was welded to the FOE-
// ASSIGNMENT engage/disengage ranges (scenario.toml [combat], sized for
// dogfight assignment), so a raider fled while the player was still 6 km out
// -- unseeable. [conquest] now owns raid_pause_engage_m/raid_pause_disengage_m
// and the latch reads cq->params, never dw->dparams.
//
// Pure, headless: no raylib, no clock, no rng.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <glm/glm.hpp>

#include "app/instructor_tick.h"
#include "combat/conquest.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "config/load_game.h"
#include "drone/drone.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

// A drone parked 100 m below the crash sphere (the spec's "practical
// fixture": a curr set below the crash surface so the very next drone::tick
// call inside app::tick crashes it -- one physics step at this dt cannot
// climb 100 m). Level, tangent-consistent state (drone::level_state_at) so
// no invariant (unit quaternion, tangent velocity) is violated.
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

// A player parked well away from the drone (spawn_state's default), reused
// by every case below -- the crash-respawn/scramble logic under test never
// reads the player position, so its exact placement is unimportant as long
// as app::tick's own invariants (aim frame, local_up) stay well-formed.
app::LoopState parked_player() {
    app::LoopState st{};
    st.curr = st.prev = app::spawn_state(kAp);
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = true;
    return st;
}

// Containment check mirroring test_conquest_leash_repro.cpp's frac measure:
// `pos` is inside `faction`'s live ellipse (arc from the dome centre <=
// margin * the direction-dependent r_eff). place_in_faction_air always
// places at 0.6*b (comfortably inside in every bearing), so margin=1.0 is a
// generous, non-tautological bound.
bool inside_faction_ellipse(const glm::dvec3& pos, int faction,
                            const world::FactionGrowth grow[2],
                            double margin = 1.0) {
    glm::dvec3 cdir{0.0}, maj{0.0};
    double a = 0.0, b = 0.0;
    world::faction_ellipse(faction, grow, cdir, maj, a, b);
    if (!(a > 0.0 && b > 0.0)) return false;
    const glm::dvec3 up = glm::normalize(pos);
    const double c =
        std::clamp(glm::dot(up, glm::normalize(cdir)), -1.0, 1.0);
    const double arc = kAp.R * std::acos(c);
    const double reff = world::ellipse_r_eff(cdir, maj, a, b, up);
    return reff > 1e-6 && arc <= margin * reff;
}

// ★★★ S4 — A FLAT PLANET. w=h=2, every pixel 0 and relief_scale 0, so
// radius_at() returns exactly kAp.R at every bearing: a live world::HeightField
// with no terrain relief. That is what these cases need — the DECK respawn's
// altitude is measured FROM the terrain, so it needs a heightfield to exist,
// but a real DEM would only add noise to an AGL assertion. The real DEM is
// flown by P-H/[.s4collapse] in test_conquest_match.cpp.
world::HeightField flat_ground() {
    world::HeightField hf;
    hf.w = 2;
    hf.h = 2;
    hf.px.assign(4, 0);
    hf.R = kAp.R;
    hf.relief_scale = 0.0;
    return hf;
}

// The ground contract that goes with it. normal_at() asserts probe_m > 0, so
// a HeightField without matching GroundParams is not a usable world -- the
// same values test_conquest_match.cpp's tunnel_ground_params carries.
sim::GroundParams flat_ground_params() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    return gp;
}

// `pos` is inside `faction`'s BAKED BASELINE zone (growth 1.0/1.0) — the
// territory, which is what the S4 deck respawn places against, not the live
// scaled dome (which is a point once radius_scale hits 0).
bool inside_faction_zone(const glm::dvec3& pos, int faction) {
    const world::FactionGrowth base[2]{};
    return inside_faction_ellipse(pos, faction, base);
}

// ---- loader test helpers (the test_gravity.cpp pattern, duplicated -- each
// TU keeps its own copy; the temp-file tag prefix keeps the two TUs' scratch
// files from colliding). ---------------------------------------------------
std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_cqresp_") + tag + ".toml");
    std::ofstream(p) << text;
    return p.string();
}

std::string replace_all(std::string s, const std::string& from,
                        const std::string& to) {
    for (size_t i = s.find(from); i != std::string::npos;
         i = s.find(from, i + to.size())) {
        s.replace(i, from.size(), to);
    }
    return s;
}

// FIX-F3 wiring probe: build a fresh one-drone conquest world, seed the
// latch to `prev_ok`, place the player EXACTLY `player_range_m` from the
// drone (glm::length of the two positions -- the identical expression
// instructor_tick.h's raid_range_ok computes), and report the latch after
// ONE app::tick. dw->dparams.engage_range/disengage_range are deliberately
// set to values (6000/7000) that would flip the latch differently than
// cq->params' raid_pause_engage_m/raid_pause_disengage_m -- a regression to
// the pre-fix wiring (reading dparams) changes this function's answer.
bool raid_latch_after_one_tick(bool prev_ok, double player_range_m,
                               double raid_pause_engage_m,
                               double raid_pause_disengage_m) {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    dp.engage_range = 6000.0;     // the WRONG dial (pre-fix source), on
    dp.disengage_range = 7000.0;  // purpose far from the ranges under test

    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(drone::spawn_drone(kAp, dp, 5, 1));  // any maverick
    dw.drones[0].raid_range_ok = prev_ok;

    app::LoopState st = parked_player();
    const glm::dvec3 dpos = dw.drones[0].curr.position;
    const glm::dvec3 up = sim::local_up(dpos);
    glm::dvec3 tangent{0.0, 1.0, 0.0};
    if (std::abs(glm::dot(tangent, up)) > 0.9) tangent = glm::dvec3{1.0, 0.0, 0.0};
    tangent = glm::normalize(tangent - up * glm::dot(tangent, up));
    // Exact chord distance == player_range_m (the same glm::length the
    // conquest block computes -- no arc/chord approximation needed).
    st.curr.position = st.prev.position = dpos + tangent * player_range_m;
    st.aim.reseed(st.curr.orientation, sim::local_up(st.curr.position));

    app::ConquestWorld cq;
    cq.params.raid_pause_engage_m = raid_pause_engage_m;
    cq.params.raid_pause_disengage_m = raid_pause_disengage_m;

    app::TickInput in;
    in.raw_mode = true;  // clean stick: the player barely moves this one tick
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr, &cq);
    return dw.drones[0].raid_range_ok;
}

}  // namespace

// ---------------------------------------------------------------------------
// FIX-F2

TEST_CASE("conquest crash respawn lands in own faction air") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    // spawn_index 0 -> combat::maverick_faction(0) == CQ_SUDBURY ==
    // world::SUDBURY (the 1:1 mapping the conquest_world.h banner asserts).
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));

    app::LoopState st = parked_player();

    app::ConquestWorld cq;  // both domes alive: cq.state defaults 1.0/1.0

    const world::FactionGrowth grow[2]{};  // 1.0/1.0, matches cq.state
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const cfg::GameParams game =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
    world::build_faction_bubbles(game.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = game.atmosphere.deck_agl_m;
    af.deck_soft_m = game.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    app::TickInput in;
    app::tick(st, in, kAp, kCp, &env, &dw, nullptr, nullptr, nullptr, &cq);

    CHECK(inside_faction_ellipse(dw.drones[0].curr.position, world::SUDBURY,
                                 grow));
    const double frac = sim::atm_frac_at(dw.drones[0].curr.position, &env, kAp);
    CHECK(frac >= 0.9);
}

// S4 -- THE SECOND TELEPORT PATH, GONE. This case used to be called
// "dead own dome relocates to the survivor" and asserted the OPPOSITE of what
// it asserts now: that a pilot whose dome had died was planted inside the
// SURVIVING faction's bubble. Chad 2026-08-26: "they respawn from their zone
// at the deck". The CHECK_FALSE below is the anti-teleport clause -- it is the
// assertion that goes red if anyone re-adds the cross-faction fallback.
TEST_CASE("S4: a dead own dome respawns on its OWN zone at the deck") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));  // SUDBURY

    app::LoopState st = parked_player();

    app::ConquestWorld cq;
    cq.state.radius_scale[world::SUDBURY] = 0.0;  // own dome extinct
    cq.state.radius_scale[world::VALLEY] = 1.0;   // the survivor

    const world::HeightField hf = flat_ground();
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = flat_ground_params();

    app::TickInput in;
    app::tick(st, in, kAp, kCp, &env, &dw, nullptr, nullptr, nullptr, &cq);

    const glm::dvec3 pos = dw.drones[0].curr.position;
    // 1. HIS OWN GROUND, not the survivor's.
    CHECK(inside_faction_zone(pos, world::SUDBURY));
    world::FactionGrowth grow[2]{};
    grow[world::SUDBURY].radius_scale = 0.0;
    grow[world::VALLEY].radius_scale = 1.0;
    CHECK_FALSE(inside_faction_ellipse(pos, world::VALLEY, grow));
    // 2. AT THE DECK: exactly dp.deck_track_agl_m over the terrain, which is
    //    inside the (deck_avoid_agl_enter_m, deck_avoid_agl_release_m] band
    //    the loader pins it to and under the full-air deck lid.
    const double agl = glm::length(pos) - hf.radius_at(glm::normalize(pos));
    CHECK(agl == Catch::Approx(dp.deck_track_agl_m).margin(1e-6));
    // 3. And it is NOT the stock +X scatter it would be if the placement had
    //    refused (that sits at dp.spawn_alt, kilometres away).
    const sim::SimState ref = drone::spawn_state(kAp, dp, 0, 1);
    CHECK(glm::length(pos - ref.position) > 1000.0);
}

// S4 -- BOTH DOMES DEAD IS NO LONGER AN ENDGAME EXCEPTION. Before S4 this
// was the "air exists nowhere" case and the pilot was left on the stock +X
// scatter at spawn_alt in vacuum -- the exact respawn pen FIX-F2 exists to
// end, re-entered through the endgame door. The deck holds air whatever the
// dome war did, so there is now nowhere on this planet a crash respawn cannot
// breathe.
TEST_CASE("S4: both domes dead still respawns on the pilot's own deck") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));  // SUDBURY

    app::LoopState st = parked_player();

    app::ConquestWorld cq;
    cq.state.radius_scale[world::SUDBURY] = 0.0;
    cq.state.radius_scale[world::VALLEY] = 0.0;  // no dome anywhere

    const world::HeightField hf = flat_ground();
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = flat_ground_params();

    app::TickInput in;
    app::tick(st, in, kAp, kCp, &env, &dw, nullptr, nullptr, nullptr, &cq);

    const glm::dvec3 pos = dw.drones[0].curr.position;
    CHECK(inside_faction_zone(pos, world::SUDBURY));
    const double agl = glm::length(pos) - hf.radius_at(glm::normalize(pos));
    CHECK(agl == Catch::Approx(dp.deck_track_agl_m).margin(1e-6));
}

// S4 -- NO RESCUE AT THE COLLAPSE TICK. Chad's rule 1, asserted directly:
// a LIVING, FLYING pilot whose dome dies this tick is not moved. He keeps the
// air, the orientation and the speed he had. The old SCRAMBLE ON COLLAPSE hook
// rewrote d.curr for exactly this drone; the displacement clause below is what
// catches its return (tape 12 measured those jumps at 13,163-32,513 m against
// the metres one tick of cruise can cover).
TEST_CASE("S4: no rescue at the collapse tick -- a flying pilot is not moved") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    // Well clear of the ground and in level flight: nothing in this tick can
    // crash him, so the ONLY thing that could move him far is a teleport.
    drone::DroneState d;
    const glm::dvec3 pos0 =
        glm::normalize(world::kSudburyCenterDir) * (kAp.R + 2000.0);
    d.curr = drone::level_state_at(dp, pos0, glm::dvec3{0.0, 0.0, 1.0});
    d.prev = d.curr;
    d.spawn_index = 0;  // SUDBURY
    d.fleet_count = 1;
    d.hp = dp.hp;
    dw.drones.push_back(d);

    app::LoopState st = parked_player();

    app::ConquestWorld cq;
    cq.state.radius_scale[world::SUDBURY] = 0.0;  // his dome is GONE
    cq.state.radius_scale[world::VALLEY] = 1.0;

    app::TickInput in;
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr, &cq);

    const glm::dvec3 pos1 = dw.drones[0].curr.position;
    // One tick of flight, generously bounded: dp.speed * sim_dt, x8.
    const double one_tick_m = dp.speed * kAp.sim_dt;
    CHECK(glm::length(pos1 - pos0) < 8.0 * one_tick_m);
    world::FactionGrowth grow[2]{};
    grow[world::SUDBURY].radius_scale = 0.0;
    grow[world::VALLEY].radius_scale = 1.0;
    CHECK_FALSE(inside_faction_ellipse(pos1, world::VALLEY, grow));
}

// The ONLY surviving stock-respawn case: a fixture with no world at all. With
// env == nullptr there is no heightfield, so there is no AGL to place a deck
// respawn against and place_on_faction_deck refuses by contract.
TEST_CASE("conquest crash respawn: no heightfield leaves the stock respawn") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));

    app::LoopState st = parked_player();

    app::ConquestWorld cq;
    cq.state.radius_scale[world::SUDBURY] = 0.0;
    cq.state.radius_scale[world::VALLEY] = 0.0;  // no dome anywhere

    app::TickInput in;
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr, &cq);

    // The reference: the SAME pure spawn_state call respawn_in_place makes
    // internally (spawn_index 0, fleet_count 1) -- bitwise, no rng/clock.
    const sim::SimState ref = drone::spawn_state(kAp, dp, 0, 1);
    CHECK(dw.drones[0].curr.position.x == ref.position.x);
    CHECK(dw.drones[0].curr.position.y == ref.position.y);
    CHECK(dw.drones[0].curr.position.z == ref.position.z);
}

TEST_CASE("conquest crash respawn: no conquest is bit identical") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));

    app::LoopState st = parked_player();

    app::TickInput in;
    // cq == nullptr: the FIX-F2 block is skipped entirely.
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr,
             nullptr);

    const sim::SimState ref = drone::spawn_state(kAp, dp, 0, 1);
    CHECK(dw.drones[0].curr.position.x == ref.position.x);
    CHECK(dw.drones[0].curr.position.y == ref.position.y);
    CHECK(dw.drones[0].curr.position.z == ref.position.z);
}

// Red-team P1-2 (D:\seads_sandboxes\ai-probes\redteam\VERDICT.md): the
// helper decided "this dome has air" from radius_scale alone, ignoring
// ceiling_scale entirely, and placed at a hard 2000 m AGL regardless. The
// two fields are INDEPENDENT (radius_scale = ceiling_scale = 0.25 is
// reachable under the shipped [conquest] growth/shrink fracs: lose a pump
// (0.5) -> destroy an enemy pump (0.75) -> lose the second pump (0.25)), so
// a dome with a healthy footprint but a shrunken ceiling (4000*0.25 = 1000 m)
// was placing every relocated drone in VACUUM at the old fixed altitude --
// probe-measured atm_frac 0.000, reproducing the exact pen FIX-F2 exists to
// end.
TEST_CASE(
    "conquest crash respawn: a live but ceiling-starved dome places under "
    "its own soft band") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));  // SUDBURY

    app::LoopState st = parked_player();

    app::ConquestWorld cq;  // bubble_ceiling_m/bubble_ceil_soft_m default 4000/600
    cq.state.radius_scale[world::SUDBURY] = 1.0;    // ellipse alive
    cq.state.ceiling_scale[world::SUDBURY] = 0.25;  // ceiling_m = 1000 (< spawn_alt)
    cq.state.radius_scale[world::VALLEY] = 1.0;
    cq.state.ceiling_scale[world::VALLEY] = 1.0;

    // A real, growth-matched AtmosphereField (the red-team's own f2_ceiling
    // probe methodology) so atm_frac_at is a meaningful measurement, not
    // just a geometric containment check.
    world::FactionGrowth grow[2]{};
    grow[world::SUDBURY].radius_scale = 1.0;
    grow[world::SUDBURY].ceiling_scale = 0.25;
    grow[world::VALLEY].radius_scale = 1.0;
    grow[world::VALLEY].ceiling_scale = 1.0;
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const cfg::GameParams game =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
    world::build_faction_bubbles(game.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = game.atmosphere.deck_agl_m;
    af.deck_soft_m = game.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;
    // Premise: this worktree's cq defaults (bubble_ceiling_m) and the
    // config's [atmosphere] bubble_ceiling_m agree (both are the same
    // single-sourced value in the real app) -- else the AtmosphereField
    // built here would not match what the helper computed its placement
    // against, and the atm_frac_at measurement below would be meaningless.
    REQUIRE(cq.bubble_ceiling_m == Catch::Approx(game.atmosphere.bubble_ceiling_m));

    app::TickInput in;
    app::tick(st, in, kAp, kCp, &env, &dw, nullptr, nullptr, nullptr, &cq);

    CHECK(inside_faction_ellipse(dw.drones[0].curr.position, world::SUDBURY,
                                 grow));
    const double frac = sim::atm_frac_at(dw.drones[0].curr.position, &env, kAp);
    CHECK(frac >= 0.9);
}

// P1-2's second leg: a ceiling shrunk below the 500 m floor (ceiling_m = 200)
// reports "no air" even though the ellipse itself is still alive -- the
// helper falls through exactly like the radius_scale=0 case (test above),
// because a flat-topped dome cannot hold ANY placement, not because its
// footprint died. S4: what it falls through TO is now the pilot's own deck,
// never the survivor's dome.
TEST_CASE(
    "conquest crash respawn: a ceiling too flat to breathe falls through to "
    "the own-zone deck") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    app::DroneWorld dw;
    dw.dparams = dp;
    dw.drones.push_back(crash_primed_drone(dp, 0, 1));  // SUDBURY

    app::LoopState st = parked_player();

    app::ConquestWorld cq;
    cq.state.radius_scale[world::SUDBURY] = 1.0;    // ellipse alive
    cq.state.ceiling_scale[world::SUDBURY] = 0.05;  // ceiling_m = 200 (< 500 floor)
    cq.state.radius_scale[world::VALLEY] = 1.0;
    cq.state.ceiling_scale[world::VALLEY] = 1.0;  // the other side: full ceiling

    const world::HeightField hf = flat_ground();
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = flat_ground_params();

    app::TickInput in;
    app::tick(st, in, kAp, kCp, &env, &dw, nullptr, nullptr, nullptr, &cq);

    const glm::dvec3 pos = dw.drones[0].curr.position;
    world::FactionGrowth grow[2]{};
    grow[world::SUDBURY].radius_scale = 1.0;
    grow[world::SUDBURY].ceiling_scale = 0.05;
    grow[world::VALLEY].radius_scale = 1.0;
    grow[world::VALLEY].ceiling_scale = 1.0;
    // S4: HIS OWN ground at the deck, and NOT the other faction's air.
    CHECK(inside_faction_zone(pos, world::SUDBURY));
    CHECK_FALSE(inside_faction_ellipse(pos, world::VALLEY, grow));
    const double agl = glm::length(pos) - hf.radius_at(glm::normalize(pos));
    CHECK(agl == Catch::Approx(dp.deck_track_agl_m).margin(1e-6));
}

// S4 -- ONE BEARING LAW, TWO ALTITUDE REFERENCES. The old case here was
// "scramble and crash-respawn share the placement", pinning the deleted
// scramble against the dome placement. The single-source risk it guarded is
// unchanged in shape and now runs between the DOME respawn and the DECK
// respawn: both must plant the same slot on the same bearing, and differ ONLY
// in what the altitude is measured from. If someone forks plant_at_slot the
// two bearings separate and this goes red.
TEST_CASE("S4: the deck respawn and the dome respawn share one bearing law") {
    drone::DroneParams dp;
    app::DroneWorld dw;
    dw.dparams = dp;

    app::ConquestWorld cq;
    const world::FactionGrowth base[2]{};  // 1.0/1.0 -- the BAKED zone, which
                                           // is what the deck placement uses

    const world::HeightField hf = flat_ground();
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = flat_ground_params();

    drone::DroneState d_air;
    d_air.spawn_index = 0;
    d_air.fleet_count = 1;
    const bool placed_air = app::place_in_faction_air(
        d_air, world::SUDBURY, base, kAp, dw, 3,
        base[world::SUDBURY].ceiling_scale * cq.bubble_ceiling_m,
        cq.bubble_ceil_soft_m);
    REQUIRE(placed_air);

    drone::DroneState d_deck;
    d_deck.spawn_index = 0;
    d_deck.fleet_count = 1;
    const bool placed_deck =
        app::place_on_faction_deck(d_deck, world::SUDBURY, kAp, dw, &env, 3);
    REQUIRE(placed_deck);

    // SAME BEARING, bit for bit: the two positions differ only in radius.
    const glm::dvec3 u_air = glm::normalize(d_air.curr.position);
    const glm::dvec3 u_deck = glm::normalize(d_deck.curr.position);
    CHECK(u_air.x == Catch::Approx(u_deck.x).margin(1e-12));
    CHECK(u_air.y == Catch::Approx(u_deck.y).margin(1e-12));
    CHECK(u_air.z == Catch::Approx(u_deck.z).margin(1e-12));
    // DIFFERENT ALTITUDE, and each is what its own law says it should be.
    CHECK(glm::length(d_air.curr.position) - kAp.R ==
          Catch::Approx(std::clamp(cq.bubble_ceiling_m -
                                       cq.bubble_ceil_soft_m - 200.0,
                                   300.0, 2000.0))
              .margin(1e-6));
    CHECK(glm::length(d_deck.curr.position) - hf.radius_at(u_deck) ==
          Catch::Approx(dp.deck_track_agl_m).margin(1e-6));
}

// ---------------------------------------------------------------------------
// FIX-F3

TEST_CASE("raid pause latch reads the conquest dial not the combat ranges") {
    constexpr double kEngage = 1500.0;
    constexpr double kDisengage = 2500.0;

    // 1) Fresh latch (false), player at 3000 m: beyond the NEW disengage
    //    (2500) so it ARMS. Beyond neither of the WRONG dparams thresholds
    //    (6000/7000), so a regression to the old wiring would keep this
    //    false -- the assertion pins which dial is actually read.
    const bool armed =
        raid_latch_after_one_tick(false, 3000.0, kEngage, kDisengage);
    CHECK(armed);

    // 2) Already armed, player closes to 5000 m -- the tape's own number.
    //    Beyond the NEW engage (1500) so it STAYS armed (does not suspend).
    //    Under the pre-fix wiring (dparams.engage_range 6000), 5000 < 6000
    //    would have suspended it -- this is the F3 regression.
    const bool stays_armed =
        raid_latch_after_one_tick(true, 5000.0, kEngage, kDisengage);
    CHECK(stays_armed);

    // 3) Armed, player closes inside the NEW engage (1500): releases.
    const bool released =
        raid_latch_after_one_tick(true, 1000.0, kEngage, kDisengage);
    CHECK_FALSE(released);

    // 4) Released, player opens back to 2000 m -- inside the hysteresis band
    //    (engage 1500, disengage 2500): must NOT re-arm from the band (the
    //    house hysteresis rule -- only ONE switch across this whole
    //    sequence, at step 3's release; a non-hysteretic latch would flip
    //    back true here and chatter).
    const bool stays_released =
        raid_latch_after_one_tick(false, 2000.0, kEngage, kDisengage);
    CHECK_FALSE(stays_released);

    // The switch tally across the logical sequence true(2) -> false(3) ->
    // false(4): exactly one transition.
    const bool seq[3] = {stays_armed, released, stays_released};
    int switches = 0;
    for (int i = 1; i < 3; ++i)
        if (seq[i] != seq[i - 1]) ++switches;
    CHECK(switches == 1);
}

TEST_CASE(
    "load_game: [conquest] raid_pause_engage_m/raid_pause_disengage_m load "
    "strictly and reject their forks") {
    const std::string src = slurp(SEADS_CONFIG_DIR "/game.toml");
    const cfg::GameParams g =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
    CHECK(g.conquest.raid_pause_engage_m == Catch::Approx(1500.0));
    CHECK(g.conquest.raid_pause_disengage_m == Catch::Approx(2500.0));

    // disengage <= engage rejected (the hysteresis gap must be real).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "raid_pause_disengage_m = 2500.0",
                               "raid_pause_disengage_m = 1000.0"),
                   "dis_lt_eng"),
        kAp));
    // Equal is ALSO rejected (strictly greater required, the same
    // load_scenario.cpp hysteresis-gap discipline).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "raid_pause_disengage_m = 2500.0",
                               "raid_pause_disengage_m = 1500.0"),
                   "dis_eq_eng"),
        kAp));
    // engage <= 0 rejected.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "raid_pause_engage_m = 1500.0",
                               "raid_pause_engage_m = 0.0"),
                   "eng_zero"),
        kAp));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "raid_pause_engage_m = 1500.0",
                               "raid_pause_engage_m = -100.0"),
                   "eng_neg"),
        kAp));
    // Strict: a missing key throws (never a silent default).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "raid_pause_engage_m = 1500.0", ""),
                   "eng_gone"),
        kAp));
}
