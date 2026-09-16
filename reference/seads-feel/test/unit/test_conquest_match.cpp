// PROBE P-H — THE CONQUEST MATCH (docs/ENEMY_AI_E1_E2_SPEC.md, RUNG E12).
//
// ★ THIS IS THE INSTRUMENT THE LADDER HAS BEEN MISSING SINCE E9. Every probe
// before it measured a FIGHT; none of them ever measured the GAME. The standing
// finding (handoff 2026-08-23, OPEN item 1): "P-A NEVER DESTROYS A PUMP, so its
// bubbles never shrink, so the air-seek dive never fires — the fixture is
// structurally blind to a whole class of conquest defects." Chad's ask on
// 2026-08-23 ("get out of easy mode and increase its ability to destroy pumps")
// is a question ONLY a whole-match fixture can answer, so here it is.
//
// WHAT IT MEASURES: the enemy faction's ability to destroy the PLAYER's pumps
// over a full 22-minute conquest match — on-station seconds, raid-duty
// coverage, pump HP remaining, and whether a player pump falls at all.
//
// PRIMARY-DATA LAW (the P-A/P-B discipline, verbatim):
//   * REAL LOADERS — aircraft.toml / scenario.toml / game.toml. Every dial
//     under test is the dial that ships; never a hand-built DroneParams.
//   * REAL WORLD — a live sim::Environment with ground, the two-faction
//     atmosphere domes AND the tunnel net with the arena body on, so terrain
//     avoidance, the flyable-air ceiling and the deep-pump strike all bite.
//   * REAL PLACEMENT — the app's own conquest spawn scatter, combat::make_pumps
//     off the live arena via combat::place_deep_pump, pump HP derived from
//     [conquest] pump_kill_seconds and the live battery.
//   * REAL ORDER — combat::assign_foes -> combat::assign_defense -> the app's
//     leash/raid/strike arming -> per-drone drone::tick against ITS OWN foe ->
//     combat::reinforce_tick -> the raid/strike DPS credit ->
//     combat::conquest_countdown_tick. The same sequence, in the same order,
//     as app/instructor_tick.h's conquest block. ★ S4: there is no "collapse
//     scramble" step any more, in either -- this banner used to claim the
//     fixture ran one while the code below only latched a flag, and both the
//     claim and the app hook are now deleted (Chad 2026-08-26).
//     The app's OWN helpers are called wherever they are reachable
//     (app::faction_growth_from_state, app::AirborneWavePolicy,
//     app::wave_params, app::place_in_faction_air, app::place_on_faction_deck)
//     so there is one implementation, not a copy that can go stale.
//   * ★ A REAL PLAYER — CHAD. Not a scripted circuit. The player is REPLAYED
//     from his own signed tape 7 (test/golden/conquest/tape7_chad_replay.txt,
//     distilled by offline_tool/distill_conquest_tape.py): his trajectory at
//     5 Hz, the 9 pilots his guns killed, and the 2 enemy pumps his side
//     destroyed. The scripted player was the thing that made P-A structurally
//     unable to reach this question (its hardest turn is 2.36 g against his
//     4.3 g median, and it flies 112% of v_redline forever).
//
// ★★★ THE HONEST LIMITS, stated up front so no claim outruns them:
//   1. THE REPLAY IS OPEN LOOP. Chad flew against the fleet as it actually
//      was. An arm that changes the AI changes where those pilots were, so a
//      replayed kill at t=249.7 means "the enemy lost this pilot here", NOT
//      "he would still have hit him". P-H is therefore a fair A/B of the
//      PUMP-ATTACK CHAIN UNDER A FIXED ATTRITION SCHEDULE — which is exactly
//      the rung's question — and NOT a prediction of a rematch.
//   2. HIS BANK IS NOT IN THE TAPE. The recorder writes pos+vel, so the
//      replayed orientation is nose-along-velocity: zero AoA, wings level to
//      the velocity/local-up frame. Aspect angle is faithful; roll is not.
//   3. THE PLAYER SIDE'S OFFENSE IS THE REPLAY, NOT THE SIM. His faction's own
//      raiders are flown (they are traffic, and they draw defenders) but their
//      pump DPS credit is SUPPRESSED, because the two enemy pump deaths are
//      replayed from the tape and crediting both would count the same damage
//      twice. So P-H measures ONE side's offense — the enemy's — against a
//      fixed record of what happened to the enemy. That asymmetry is the
//      point, not an oversight.
//   4. NO BALLISTICS. combat::enemy_fire_tick / combat_player_tick are NOT
//      run: air-to-air attrition is the replayed kill schedule. P-H says
//      nothing about lethality; that is P-A's job and P-A keeps it.
//
// TAPE-ABSENT READER RULE (the sled rung's precedent, test_sled_tape.cpp): if
// the replay track is missing the probe SUCCEEDS with a WARN, so a checkout
// without it is green rather than falsely red.
//
// TEST_CASE names are STRICTLY ASCII (a non-ASCII name silently never runs).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/conquest_world.h"
#include "app/instructor_tick.h"
#include "combat/conquest.h"
#include "combat/raid.h"
#include "combat/reinforce.h"
#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "config/load_world.h"
#include "drone/drone.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"
#include "world/tunnel_net.h"

// STB_IMAGE_IMPLEMENTATION lives in test_tunnel_mesh.cpp:33 (the one owner in
// this target); we take only the declarations and resolve at link -- the same
// pattern as test_tunnel_track_probe.cpp:46 and test_tunnel_audit_probe.cpp:23.
#define STBI_ONLY_PNG
#include "stb_image.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;
// ★★★ S4 — THE SPIKE WINDOW [s]. Chad ruled the deaths at "the moment of air
// loss" intended, so they get their own bucket. 30 s is a ceiling on how long
// a plane that lost its lift can stay up: the fleet cruises at 85-156 m/s from
// a placement ceiling of at most 2000 m, so a total mush is on the ground well
// inside it. Reporting only -- no assertion is keyed to this number.
constexpr double kSpikeS = 30.0;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

const std::string kReplayPath =
    SEADS_CONFIG_DIR "/../test/golden/conquest/tape7_chad_replay.txt";

// ---------------------------------------------------------------------------
// THE REPLAY TRACK
// ---------------------------------------------------------------------------

struct ReplaySample {
    double t = 0.0;
    glm::dvec3 pos{0.0};
    glm::dvec3 vel{0.0};
    bool alive = true;
};
struct ReplayKill {
    double t = 0.0;
    int spawn_index = 0;
};
struct ReplayPumpDeath {
    double t = 0.0;
    int pump_idx = 0;
    int faction = 0;
};

struct Replay {
    bool ok = false;
    std::vector<ReplaySample> track;
    std::vector<ReplayKill> kills;
    std::vector<ReplayPumpDeath> pump_deaths;
    double span_s() const {
        return track.empty() ? 0.0 : track.back().t - track.front().t;
    }
};

Replay load_replay(const std::string& path) {
    Replay r;
    std::ifstream in(path);
    if (!in) return r;  // TAPE-ABSENT: caller WARNs and passes
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        char tag = 0;
        ss >> tag;
        if (tag == 'P') {
            ReplaySample s;
            int alive = 1;
            ss >> s.t >> s.pos.x >> s.pos.y >> s.pos.z >> s.vel.x >> s.vel.y >>
                s.vel.z >> alive;
            s.alive = alive != 0;
            r.track.push_back(s);
        } else if (tag == 'K') {
            ReplayKill k;
            ss >> k.t >> k.spawn_index;
            r.kills.push_back(k);
        } else if (tag == 'D') {
            ReplayPumpDeath d;
            ss >> d.t >> d.pump_idx >> d.faction;
            r.pump_deaths.push_back(d);
        }
    }
    r.ok = r.track.size() > 2;
    return r;
}

// A mutually-consistent player state at time t: position and velocity linearly
// interpolated between the two bracketing 5 Hz samples, orientation built
// nose-along-velocity in the local-up frame (limit 2 in the banner). Never
// drone::level_state_at — that stamps a CRUISE velocity onto a state the
// fixture never moved, which is the documented FIXTURE PHANTOM.
sim::SimState replay_state_at(const Replay& r, double t, std::size_t& cursor) {
    const std::vector<ReplaySample>& tr = r.track;
    while (cursor + 2 < tr.size() && tr[cursor + 1].t <= t) ++cursor;
    const ReplaySample& a = tr[cursor];
    const ReplaySample& b = tr[std::min(cursor + 1, tr.size() - 1)];
    const double dt = b.t - a.t;
    const double u = dt > 1e-9 ? std::clamp((t - a.t) / dt, 0.0, 1.0) : 0.0;

    sim::SimState s;
    s.position = a.pos + (b.pos - a.pos) * u;
    s.velocity = a.vel + (b.vel - a.vel) * u;
    const double sp = glm::length(s.velocity);
    glm::dvec3 fwd = sp > 1e-6 ? s.velocity / sp : glm::dvec3{0.0, 0.0, -1.0};
    glm::dvec3 up_ref = sim::local_up(s.position);
    if (std::abs(glm::dot(fwd, up_ref)) > 0.999)  // degenerate: pick any
        up_ref = std::abs(fwd.x) < 0.9 ? glm::dvec3{1.0, 0.0, 0.0}
                                       : glm::dvec3{0.0, 1.0, 0.0};
    const glm::dvec3 right = glm::normalize(glm::cross(fwd, up_ref));
    const glm::dvec3 up = glm::cross(right, fwd);
    const glm::dmat3 m{right, up, -fwd};
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.last_vhat = fwd;
    s.throttle = 1.0;
    return s;
}

bool player_alive_at(const Replay& r, std::size_t cursor) {
    return r.track[std::min(cursor, r.track.size() - 1)].alive;
}

// ---------------------------------------------------------------------------
// THE WORLD (the test_stope_probe.cpp fixture: real net, real arena, real
// domes — the deep-pump strike is half of the enemy's measured offense, so a
// probe without the tunnel net would measure half a game).
// ---------------------------------------------------------------------------

// ★ REPAIR A1 (audit 2026-08-25, C5). This used to be
// `uniform_field(300.0)` -- a 8x4 constant heightfield returning
// kAp.R + 4915/65535*4000 = 15299.9924 m in EVERY direction. A featureless
// shell cannot represent the deck's terrain-relative frame, the vacuum crash,
// or the 11289.9 m in-net wreck radius: that radius does not exist inside it.
// So P-H flew a world the game does not ship -- for the second time in this
// fixture's life (see the E16 note in build_world below).
//
// This is the real Sudbury DEM, decoded headlessly: the donor shape is
// test_tunnel_track_probe.cpp:93, with two upgrades.
//
//  (1) relief_scale and u_offset are read from the SHIPPED loader
//      (cfg::load_world_toml), never a hand copy. The donor hard-codes
//      350.0 / 0.806; identical today (config/world.toml:240, :595) but a
//      constant that DESCRIBES the shipped table stops describing it the
//      moment the table moves. This ladder has paid for that law seven times.
//  (2) The DEM FILE ITSELF follows the same table. render/planet.cpp:659
//      selects sudbury_dem.png vs earth_dem_2048.png on ground.use_procedural
//      (config/world.toml:245 = 1 today). We do not add earth-DEM support
//      speculatively -- instead, if that flag is ever flipped, this returns an
//      EMPTY field and build_world's REQUIRE fires. A future table flip turns
//      P-H RED; it does not silently re-blind it.
//
// FIDELITY, measured not assumed: for the shipped procedural DEM the runtime
// does ZERO post-processing of the height store. render/planet.cpp:653 forces
// dem_blur_radius = 0 on the procedural path (pre-blur and lake-flatten are
// baked offline into the PNG), and :702-703 fills p.height.px with the direct
// world::dem16_unpack(r, g) with nothing after it. This raw decode through the
// same dem16_unpack is therefore the same crash surface the game retains.
// (The world/heightfield.h runtime-blur caveat applies only to the
// use_procedural = 0 earth stand-in, which this fixture refuses to fly.)
//
// COST: one ~36 MB 8192x4096 decode, cached in a function-local static; each
// MatchWorld then takes a ~67 MB px copy.
world::HeightField load_real_dem() {
    static const world::HeightField cached = [] {
        world::HeightField hf;
        const cfg::WorldParams wp =
            cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
        // The shipped selector, not a hand-picked filename.
        if (!wp.ground.procedural) return hf;  // w stays 0; REQUIRE fires
        const std::string path =
            std::string(SEADS_ASSET_DIR) + "/sudbury_dem.png";
        int w = 0, h = 0, comp = 0;
        unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 3);
        if (px == nullptr) return hf;  // w stays 0; REQUIRE fires
        hf.w = w;
        hf.h = h;
        hf.R = kAp.R;
        hf.relief_scale = wp.ground.relief_scale_m;
        hf.u_offset = wp.planet.u_offset;
        hf.px.resize(static_cast<std::size_t>(w) * h);
        for (std::size_t i = 0; i < hf.px.size(); ++i)
            hf.px[i] = world::dem16_unpack(px[i * 3 + 0], px[i * 3 + 1]);
        stbi_image_free(px);
        return hf;
    }();
    return cached;
}

// ★ THE SHIPPED [tunnel] TABLE, not the stope probe's test values. P-B may
// pick its own arena because it is measuring the strike mechanism; P-H is
// measuring the GAME, and the arena's size is what decides how much of a
// striker's run is spent on station. The first cut of this probe used
// test_tp() and reported the player's deep pump dying at 7.6 min against the
// tape's 32%-alive -- the arena, not the AI.
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = kGame.tunnel.tube_width_m;
    tp.tube_height_m = kGame.tunnel.tube_height_m;
    tp.depth_m = kGame.tunnel.depth_m;
    tp.soft_m = kGame.tunnel.soft_m;
    tp.ramp_frac = kGame.tunnel.ramp_frac;
    tp.spacing_m = kGame.tunnel.spacing_m;
    tp.floor_height_m = kGame.tunnel.floor_height_m;
    tp.arena_a_m = kGame.tunnel.arena_a_m;
    tp.arena_c_m = kGame.tunnel.arena_c_m;
    tp.arena_depth_m = kGame.tunnel.arena_depth_m;
    tp.cavern_core_m = kGame.tunnel.cavern_core_m;
    tp.breach_margin_m = kGame.tunnel.breach_margin_m;
    tp.chamber_long_m = kGame.tunnel.chamber_long_m;
    tp.chamber_lat_m = kGame.tunnel.chamber_lat_m;
    tp.chamber_vert_m = kGame.tunnel.chamber_vert_m;
    tp.chamber_breach_offset_m = kGame.tunnel.chamber_breach_offset_m;
    tp.connector_radius_m = kGame.tunnel.connector_radius_m;
    tp.chambers_on = kGame.tunnel.chambers_on;
    tp.bowl_radius_m = kGame.tunnel.bowl_radius_m;
    tp.bowl_depth_m = kGame.tunnel.bowl_depth_m;
    tp.mouth_sink_m = kGame.tunnel.mouth_sink_m;
    tp.min_cover_m = kGame.tunnel.min_cover_m;
    tp.trench_len_m = kGame.tunnel.trench_len_m;
    tp.trench_rim_m = kGame.tunnel.trench_rim_m;
    tp.headframe_h_m = kGame.tunnel.headframe_h_m;
    tp.headframe_on = kGame.tunnel.headframe_on;
    return tp;
}

sim::GroundParams tunnel_ground_params() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    return gp;
}

struct MatchWorld {
    world::HeightField hf;
    world::TunnelNet net;
    sim::AtmosphereField af;
    sim::Environment env;
    app::ConquestWorld cq;
    app::DroneWorld dw;
    glm::dvec3 home[2]{};
};

// The app's conquest fleet deployment (main.cpp's per-faction disc scatter,
// mirrored in test_conquest_furball.cpp).
sim::SimState faction_state(const MatchWorld& w, const drone::DroneParams& dp,
                            int i) {
    constexpr double kConquestSpreadM = 5000.0;
    const double golden = kPi * (3.0 - std::sqrt(5.0));
    const glm::dvec3 c = w.home[combat::maverick_faction(i)];
    const int slot = combat::team_slot(i);
    const double frac = (static_cast<double>(slot) + 0.5) /
                        combat::team_size(combat::maverick_faction(i));
    const double ang = (kConquestSpreadM / kAp.R) * std::sqrt(frac);
    const double brg = golden * static_cast<double>(i);
    glm::dvec3 ref{0.0, 1.0, 0.0};
    if (std::abs(glm::dot(ref, c)) > 0.9) ref = glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 east = glm::normalize(glm::cross(ref, c));
    const glm::dvec3 north = glm::cross(c, east);
    const glm::dvec3 dir = glm::normalize(
        std::cos(ang) * c +
        std::sin(ang) * (std::cos(brg) * east + std::sin(brg) * north));
    const glm::dvec3 pos = dir * (kAp.R + dp.spawn_alt);
    const glm::dvec3 heading = std::cos(brg) * east + std::sin(brg) * north;
    return drone::level_state_at(dp, pos, heading);
}

void build_world(MatchWorld& w, const drone::DroneParams& dp) {
    // ★ REPAIR A1: the real DEM, LOUDLY. A silent decode failure (or a
    // use_procedural flip) would put this probe straight back on the flat
    // 15299.99 m shell it was built to escape, and nothing would go red. The
    // DEM is a committed asset, so its absence is a hard failure -- the
    // TAPE-ABSENT WARN rule is reserved for the replay tape, which is not.
    w.hf = load_real_dem();
    REQUIRE(w.hf.w > 0);
    w.net = world::build_tunnel_net(test_tp(), &w.hf);
    w.af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    w.af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    // ★ RUNG E16, AND THE REASON THIS LINE IS HERE: the first cut of the E16
    // arms carried the deck's frame in ArmCfg ALONE and never read the shipped
    // one, so the probe's "SHIPPED" arm silently flew a world the game does
    // not ship -- this ladder's own recurring law (a fixture that does not
    // read the shipped table is not measuring the game) committed by the very
    // rung that is about it. Every atmosphere field the world uses is copied
    // from config HERE; an arm OVERRIDES, it never defines.
    w.af.deck_terrain_relative = kGame.atmosphere.deck_terrain_relative;
    w.af.dome_exponent = kGame.atmosphere.bubble_dome_exponent;
    w.env.ground = &w.hf;
    w.env.tunnels = &w.net;
    w.env.atm = &w.af;
    w.env.ground_params = tunnel_ground_params();

    w.home[combat::CQ_VALLEY] = glm::normalize(world::kValleyCenterDir);
    w.home[combat::CQ_SUDBURY] = glm::normalize(world::kSudburyCenterDir);

    // The shipped [conquest] table, exactly as main.cpp copies it.
    w.cq.params.growth_radius_frac = kGame.conquest.growth_radius_frac;
    w.cq.params.growth_ceiling_frac = kGame.conquest.growth_ceiling_frac;
    w.cq.params.match_countdown_s = kGame.conquest.match_countdown_s;
    w.cq.params.shrink_radius_frac = kGame.conquest.shrink_radius_frac;
    w.cq.params.shrink_ceiling_frac = kGame.conquest.shrink_ceiling_frac;
    w.cq.params.raid_dps_frac = kGame.conquest.raid_dps_frac;
    w.cq.params.raid_pause_engage_m = kGame.conquest.raid_pause_engage_m;
    w.cq.params.raid_pause_disengage_m = kGame.conquest.raid_pause_disengage_m;
    w.cq.params.all_vs_player = kGame.conquest.all_vs_player;
    w.cq.params.reinforce_delay_s = kGame.conquest.reinforce_delay_s;
    w.cq.params.reinforce_restores_roster =
        kGame.conquest.reinforce_restores_roster;
    w.cq.params.reinforce_pool_n = kGame.conquest.reinforce_pool_n;
    w.cq.params.leash_min_radius_scale = kGame.conquest.leash_min_radius_scale;
    w.cq.params.raid_no_pause = kGame.conquest.raid_no_pause;
    w.cq.state.player_faction = (kGame.conquest.player_faction == "valley")
                                    ? combat::CQ_VALLEY
                                    : combat::CQ_SUDBURY;
    w.cq.atm_field = &w.af;
    w.cq.bubble_ceiling_m = kGame.atmosphere.bubble_ceiling_m;
    w.cq.bubble_edge_soft_m = kGame.atmosphere.bubble_edge_soft_m;
    w.cq.bubble_ceil_soft_m = kGame.atmosphere.bubble_ceil_soft_m;

    const auto surf = [&](const glm::dvec3& dir) {
        const glm::dvec3 u = glm::normalize(dir);
        return u * (w.hf.radius_at(u) + 10.0);
    };
    const glm::dvec3 mouthE = w.net.spine.front().pos;
    const glm::dvec3 mouthM = w.net.spine.back().pos;
    const double lift =
        w.net.arena.a_pos *
        std::sqrt(1.0 - combat::kDeepPumpFrac * combat::kDeepPumpFrac);
    // ★ THE PUMP BUDGET, IN BATTERY-NORMALIZED UNITS. The app sets
    // max_hp = pump_kill_seconds * battery_dps and credits an on-station
    // raider raid_dps_frac * battery_dps * dt, so battery_dps CANCELS out of
    // the kill time: pump_kill_seconds / raid_dps_frac = 30 s on the shipped
    // table. Dividing both sides by battery_dps gives the probe's units --
    // max_hp = pump_kill_seconds, credit = raid_dps_frac * dt (see per_tick in
    // fly_match). ★ Cancel it on BOTH sides or nowhere: dividing the credit
    // but not the budget makes the probe's pumps exactly 1/raid_dps_frac times
    // too tough, and the probe then reports a weaker enemy than the game has.
    const double kill_s = kGame.conquest.pump_kill_seconds;
    combat::make_pumps(
        surf(world::kPumpValleySurface), surf(world::kPumpSudburySurface),
        combat::place_deep_pump(w.net.arena.center, w.net.arena.u_long,
                                glm::normalize(mouthE), w.net.arena.a_pos,
                                w.net.arena.b, combat::kDeepPumpFrac, lift),
        combat::place_deep_pump(w.net.arena.center, w.net.arena.u_long,
                                glm::normalize(mouthM), w.net.arena.a_pos,
                                w.net.arena.b, combat::kDeepPumpFrac, lift),
        kill_s, w.cq.state.pumps);
    app::rebuild_conquest_bubbles(w.cq);

    w.dw.dparams = dp;
    w.dw.drones.clear();
    for (int i = 0; i < combat::kNumMavericks; ++i)
        w.dw.drones.push_back(
            drone::spawn_drone(kAp, dp, i, combat::kNumMavericks));
    for (int i = 0; i < static_cast<int>(w.dw.drones.size()); ++i) {
        w.dw.drones[i].curr = faction_state(w, dp, i);
        w.dw.drones[i].prev = w.dw.drones[i].curr;
        w.dw.drones[i].grounded = false;
        w.dw.drones[i].hp = dp.hp;
    }
}

// ---------------------------------------------------------------------------
// WHAT ONE ARM MEASURES
// ---------------------------------------------------------------------------

struct MatchResult {
    // In ON-STATION SECONDS, the unit the whole DPS model reduces to.
    double pump_hp_frac[4] = {1.0, 1.0, 1.0, 1.0};
    bool pump_dead[4] = {false, false, false, false};
    double pump_death_s[4] = {-1.0, -1.0, -1.0, -1.0};
    // The ENEMY's offense (the faction that is not the player's).
    // ⚠ A2 (audit 2026-08-25): enemy_onstation_s SATURATES. Credit stops when
    // a pump dies (the credit loop skips !tgt.alive), so any arm that kills
    // both pumps reads EXACTLY 2*pump_kill_seconds/raid_dps_frac (verified
    // 4/4: 0.50->60.0, 0.65->46.2, 0.80->37.5, 1.00->30.0). At the ceiling it
    // is a RECEIPT of the TOML, not a measurement -- read onstation_rate().
    double enemy_onstation_s = 0.0;   // credited on-station seconds (ceiling!)
    double enemy_raidduty_s = 0.0;    // sum of live raider-seconds
    double enemy_strikeduty_s = 0.0;  // sum of live striker RUN-seconds
    double enemy_alive_s = 0.0;       // sum of live enemy plane-seconds
    double enemy_near_s = 0.0;        // seconds inside the strike envelope
    int enemy_crashes = 0;
    int enemy_waves = 0;
    // ★ RUNG E16 CRASH FORENSICS. Tape 8's wreck audit, reproduced INSIDE the
    // fixture so the mechanism is measured here and not merely believed from
    // the tape. Every field is sampled from the drone's PRE-TICK state (the
    // last state it flew, not the respawn that replaces it) and the air is
    // sim::atm_frac_at -- the same call the plant reads for lift, never a
    // re-derived copy.
    int crash_thin = 0;    // air below 0.25 where it died (unsupported)
    int crash_steep = 0;   // flight path steeper than -30 deg
    int crash_fast = 0;    // faster than 150 m/s
    int crash_seeking = 0; // the air-seek dive was commanding at the time
    double crash_frac_sum = 0.0;
    double crash_gamma_sum = 0.0;   // [deg]
    double crash_speed_sum = 0.0;
    double enemy_thin_s = 0.0;      // enemy plane-seconds below 0.25 air
    double enemy_air_sum = 0.0;     // for the mean air an enemy plane breathes
    // WHICH ORDER WAS FLYING IT when it died (the disposition, in the app's
    // own precedence: defend > raid > tunnel > engaged > patrol).
    int crash_by[5] = {0, 0, 0, 0, 0};
    double duty_s[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double player_pumps_lost_s = -1.0;  // when the FIRST player pump died
    // Pump-seconds the player still HAD to defend: the denominator any
    // "did the enemy reach them" rate has to carry (a dead pump stops
    // accruing both the threat and the credit).
    double player_pump_alive_s = 0.0;
    double minutes = 0.0;

    // ★ A5 RAID-COMMAND SATURATION TELEMETRY (audit 2026-08-25, root cause).
    // maverick::aim_at multiplies the FEED-FORWARD gc_elevation by a tracking
    // gain and clamps, so inside ~1 km of the pump the raid pitch command can
    // carry zero information about the pump. These make the clamp VISIBLE
    // from inside P-H instead of inferred from tapes. Telemetry only --
    // aim_at is pure (maverick.h), nothing here feeds back into the sim.
    //
    // ⚠ WHAT THESE DO AND DO NOT REACH -- read before quoting them:
    //   * The gate is `d.raid.active && !is_tunnel_mode`, chosen to be
    //     DENOMINATOR-CONSISTENT with enemy_raidduty_s (identical condition).
    //     It is NOT drone::tick's raid branch guard, which also yields to
    //     defend and to fight_hot (drone/drone.h:2230-2252). So these are
    //     "the raid law's WOULD-BE command while a raid order is live", and
    //     some of those ticks the aeroplane is actually flying defend or BFM.
    //   * During the REATTACK latch drone::tick OVERWRITES target_gamma after
    //     the aim_at call (drone/drone.h:2296-2309). On an arm with
    //     raid_reattack_m > 0 the recomputed gamma is NOT the flown command.
    //     The SHIPPED table has reattack 0.0, so shipped arms are unaffected.
    double max_alt_err_m = 0.0;       // max |radius error to the raid aim point|
    double max_gamma_cmd_frac = 0.0;  // max |gamma_cmd| / gamma_cap (1.0 = pinned)
    double raid_pinned_s = 0.0;       // raid-order seconds with cmd >= 99% of cap

    // ★ A2 THE PRESSURE RATE -- the replacement instrument. Credit seconds
    // per second the player still had a pump to lose. It de-saturates because
    // the ceiling's variance flows through the denominator: an arm that kills
    // faster banks the same 2*kill/dps seconds over FEWER pump-alive seconds
    // and reads HIGHER. Same /player_pump_alive_s shape the E12 gate already
    // carries for raid duty and near-seconds -- one law, third application.
    double onstation_rate() const {
        return enemy_onstation_s / std::max(1e-9, player_pump_alive_s);
    }

    // ★★★ THE 412 m FLOOR INSTRUMENTS (repair rung, handoff SS3). Observation
    // ONLY: every field below is written AFTER drone::tick from state the tick
    // already produced. Nothing here writes into DroneState or DroneParams, so
    // the shipped arm's trajectory is bit-identical to before this block
    // existed (mutation-verified, see the sweep's banner).
    //
    // The latch bit sampled is d.terrain_avoid_engaged -- the EXACT bool that
    // clamps bank, forces avoid_gamma and clears wants_fire (drone/drone.h:
    // 2539-2546). It is split by FACTION because arm B's dial is fleet-wide.
    long long avoid_ticks_enemy = 0;     // latch engaged, enemy drone, any mode
    long long avoid_ticks_friendly = 0;  // latch engaged, player-side drone
    long long avoid_raid_ticks = 0;      // latch engaged on ENEMY RAID duty
    long long raid_ticks = 0;            // enemy raid-order ticks (denominator)
    long long sat_raid_ticks = 0;        // |k_el*elev| >= gamma_cap on those
    double max_gamma_raw_frac = 0.0;     // max UNCLAMPED |k_el*elev|/cap
    // AGL of an enemy raider inside 2x raid_range_m of its target pump -- the
    // same statistic the tape's "median 412 m" is.
    std::vector<double> raid_agl;
    // ★★★ RUNG S1-DECK INSTRUMENTS. Observation only, all written AFTER
    // drone::tick from state the tick already produced.
    long long deck_ticks = 0;         // enemy ticks flying the DECK band
    long long deck_latch_ticks = 0;   // of which the pull-up was latched
                                      //   (= guns MUTED, drone.h's mute)
    long long deck_fight_ticks = 0;   // deck ticks with d.engaged
    long long deck_fight_latch_ticks = 0;
    std::vector<double> deck_agl;     // AGL on every deck tick
    std::vector<double> deck_errand_agl;  // ... on the NON-engaged ones, which
                                          //   is where the track law steers
    int deck_crashes = 0;             // wrecks that happened in deck scope
    long long deck_raid_ticks = 0;    // raid-order ticks flown in deck scope
                                      //   (the dome-RIM annulus watch)
    int deck_cross_attempts = 0;      // deck-scope episodes >= 10 s, or any
                                      //   episode that ended in a wreck
    int deck_cross_survived = 0;      // ... that did NOT end in a wreck
    // The MEASURED net perpendicular g of a latched pull-up: V*dgamma/dt/9.81
    // over consecutive latched ticks. This is the number avoid_pull_net_g is
    // supposed to model, measured instead of assumed.
    std::vector<double> pull_g;
    // ★★★ RUNG D3 — THE BALLISTIC DECK RUN INSTRUMENTS. Observation only,
    // post-tick, from state drone::tick already produced. Nothing here writes
    // into DroneState or DroneParams; the arms that predate this block are
    // bit-identical to before it existed (proven by the C-arm hash pin).
    //
    // ★ THE ARRIVAL. Chad's spec is "hit the deck and cross at maximum
    // speed". The number that says whether they arrive FLYING or FALLING is
    // the state at the moment they descend through the top of the deck's air
    // column (deck_agl_m + deck_soft_m = 320 m, game.toml:152,155) and again
    // through the full-air lid (deck_agl_m = 120 m). Recorded ONLY in deck
    // scope and ONLY outside the tunnel net — the bore probes thin air at
    // ground+400 m and latches deck_scope 2 km under the DEM, which is what
    // contaminated [.deckx]'s deck-AGL p10 to −842 m. That contamination is
    // structurally excluded here, not merely warned about.
    std::vector<double> arr_v_320;      // |v| [m/s] descending through 320 m
    std::vector<double> arr_sink_320;   // sink rate [m/s] there (+ = down)
    std::vector<double> arr_gamma_320;  // flight-path angle [deg] there
    std::vector<double> arr_v_120;      // |v| [m/s] descending through 120 m
    std::vector<double> arr_sink_120;
    // ★ TIME IN DEAD AIR, PER CROSSING. Ticks of an OPEN deck episode whose
    // air was below avoid_air_frac_hard (0.25, scenario.toml:639) — the band
    // where there is no authority to correct with. Pushed when the episode
    // closes, so it is per-crossing, not a fleet total.
    std::vector<double> cross_dead_s;
    // ★★★ THE WRECK STRATIFICATION — THE DESIGN'S OWN FALSIFIER. The premise
    // is that the dominant crossing death is the dome-exit transition (leave
    // high, mush down, hit fast). If instead the wrecks are inside the 60/110
    // band, the ballistic transition fixes deaths that no longer dominate.
    // Every field is the PRE-tick state of the wreck, never the respawn.
    int wreck_agl_bin[6] = {0, 0, 0, 0, 0, 0};  // <60 /110 /250 /400 /1000 /+
    int wreck_deck_scope = 0;   // pre-tick d.deck_scope was true
    int wreck_in_net = 0;       // pre-tick position was inside the bore
    int wreck_deck_surface = 0; // deck scope AND outside the bore
    std::vector<double> wreck_agl;    // pre-tick AGL of every enemy wreck
    std::vector<double> wreck_sink;   // pre-tick sink rate [m/s]
    // ★★★ RUNG S2-TUNNEL INSTRUMENTS. Observation only, post-tick, from
    // state drone::tick already produced.
    long long net_entries = 0;      // !in_net -> in_net transitions (enemy)
    int in_net_deaths = 0;          // wrecks whose PRE-tick position was in
                                    //   the net -- the grinder's numerator
    long long climbout_done = 0;    // CLIMB_OUT -> PATROL completions
    long long raid_vt_ticks = 0;    // raid ticks the router routed by tunnel
    long long raid_net_ticks = 0;   // raid ticks actually flown inside the net
    int max_in_net = 0;             // max simultaneous enemy ships in the net
    double min_pair_sep_m = 1.0e18; // min pairwise separation among them
    // ★★★ RUNG S4 INSTRUMENTS — THE COLLAPSE. Observation only, post-tick.
    // Chad expects a crash SPIKE the moment a faction loses its air and has
    // ruled it INTENDED, so it is reported as its OWN number, never folded
    // into the steady-state rate where a later reader could "fix" it out.
    int enemy_f_idx = -1;                 // which faction index is the enemy
    double collapse_s[2] = {-1.0, -1.0};  // first tick radius_scale[f] == 0
    int enemy_crash_spike = 0;   // enemy wrecks within kSpikeS of ITS collapse
    int enemy_crash_pre = 0;     // enemy wrecks before its collapse
    int enemy_crash_post = 0;    // enemy wrecks after the spike window closes
    int deck_respawns = 0;       // S4 own-zone deck respawns taken
    int stock_respawns = 0;      // no ground to place against (fixture only)
    // WHAT A COLLAPSED FACTION DOES AFTERWARDS. Every counter below is gated
    // on "this drone's own faction has collapsed" so it answers Chad's
    // question directly: do they fly, reach the tunnel, reach a pump, engage?
    double post_fly_s = 0.0;       // plane-seconds flown after own collapse
    double post_deck_s = 0.0;      // ... in deck scope
    double post_raid_s = 0.0;      // ... under a live raid order
    double post_engaged_s = 0.0;   // ... engaged with a foe
    double post_near_s = 0.0;      // ... inside the raid envelope of a pump
    long long post_net_entries = 0;  // tunnel entries after own collapse
    std::vector<double> post_agl;    // AGL every post-collapse tick
    // FNV-1a over every drone's pos+vel every tick. Bit-identity is EXACT and
    // has no noise floor: it is the determinism law used as a discriminator.
    unsigned long long hash_enemy = 1469598103934665603ull;
    unsigned long long hash_friendly = 1469598103934665603ull;
    // ★★★ RUNG D2 — the boundary-loiter population and the reposition's own
    // episodes. ENEMY faction only. The occupancy rows are measured off the
    // shared frac law INDEPENDENTLY of whether the feature is armed, so the
    // OFF arm reports the population and the ON arm reports what moved.
    long long loiter_samples = 0;   // non-tunnel enemy ticks with a live dome
    long long loiter_third = 0;     // ... in Chad's outer third, (0.667, 1.0]
    long long loiter_band = 0;      // ... in the whole trigger band, to the lip
    long long loiter_coincide = 0;  // ... AND my own pump under attack
    // ★★★ THE BLIND-FIXTURE / DEAD-BRANCH DISCRIMINATOR (rescue pass). The
    // first run of this probe found stage 0 armed ZERO episodes with stage 1
    // off, and "0 episodes" alone cannot tell you WHICH of the two conjuncts
    // starved. These two split it: attack_open is the pump-attack window's own
    // exposure (ignoring geometry), band_attacked is the EXACT arming
    // conjunction over the real band (arm, lip] -- not the (arm, 1.0] bin,
    // which under-reports it.
    long long loiter_attack_open = 0;
    long long loiter_band_attacked = 0;
    // ★★★ AND THE THIRD LEVEL, because the split above was still not enough:
    // the exact conjunction measured NON-ZERO (1123 ticks) while episodes
    // armed ZERO, so something downstream of the conjunction was eating it.
    // These three name it instead of leaving me to guess: `eligible` is the
    // app's full arming predicate, and the two shadow counters say WHICH of
    // its exclusions (Chad's unlimited pursuit, or a defend scramble that
    // already answers the same attack) is standing in front of stage 0.
    long long loiter_arming = 0;         // conjunction AND fully eligible
    long long loiter_shadow_engaged = 0; // conjunction AND engaged
    long long loiter_shadow_defend = 0;  // conjunction AND defending
    long long regroup_episodes = 0;   // episodes armed
    long long regroup_returns = 0;    // ... of which STAGE 1 returns
    long long regroup_ticks = 0;      // ticks flown under the order
    long long regroup_closed = 0;     // episodes ended
    long long regroup_completed = 0;  // ... by reaching the release band
    long long regroup_return_ends = 0;
    std::vector<double> regroup_ep_s;  // [s] per closed episode
    // ★★★ RUNG D3 — THE BALLISTIC DECK RUN. ENEMY faction only.
    // ★ THE LIVENESS DENOMINATOR FIRST. "N episodes is too few" is only a
    // wiring verdict if you know how many chances there WERE: eligible_
    // lifetimes counts the rank-0 non-tunnel raid orders that exist at all.
    // Without it a rarity result and a wiring defect read identically -- the
    // D2 shadow precedent, where the literal trigger armed ZERO and the cause
    // was exposure, not code.
    long long bal_eligible_ticks = 0;  // ro.active && deck_run && !via_tunnel
    long long bal_arm = 0;             // NONE -> CLIMB edges
    long long bal_commit = 0;          // CLIMB -> DIVE edges (THE COMMIT)
    long long bal_commit_timeout = 0;  // ... of which fired on the TIMEOUT
    long long bal_run = 0;             // DIVE -> RUN edges
    long long bal_spent = 0;           // RUN -> SPENT (a COMPLETE episode)
    long long bal_lost = 0;            // a live phase zeroed by the reset
    long long bal_ticks[6] = {0, 0, 0, 0, 0, 0};  // ticks per phase
    std::vector<double> bal_commit_agl;  // [m] AGL at each commit
    std::vector<double> bal_apex_agl;    // [m] peak AGL reached in each CLIMB
    // ★ THE PHASE-STRATIFIED ARRIVAL. The whole point: does the manoeuvre
    // arrive FASTER through the lid than the mush it replaces? Same seam as
    // arr_v_320/arr_v_120, split by whether a ballistic phase was live.
    std::vector<double> bal_v_320;    // |v| through 320 m AGL on a live phase
    std::vector<double> bal_gamma_320;
    std::vector<double> bal_v_120;
    std::vector<double> bal_agl;      // deck AGL on RUN/SPENT ticks
    std::vector<double> bal_arrive_v; // |v| at the RUN -> SPENT handover
    long long bal_mute_ticks = 0;     // gun-mute ticks during a live phase
    long long bal_phase_ticks = 0;    // ... out of this many
};

// FNV-1a over the raw bytes of a state's position+velocity.
void fold_state(unsigned long long& h, const sim::SimState& s) {
    const double v[6] = {s.position.x, s.position.y, s.position.z,
                         s.velocity.x, s.velocity.y, s.velocity.z};
    const unsigned char* p = reinterpret_cast<const unsigned char*>(v);
    for (std::size_t b = 0; b < sizeof v; ++b) {
        h ^= p[b];
        h *= 1099511628211ull;
    }
}

// The app's raid-order arming (app/instructor_tick.h), verbatim in shape.
// ★ MIRROR PIN (S2-TUNNEL): the ROUTER at the bottom of this function is a
// hand-mirror of app/instructor_tick.h's raid-arm site. A change there must be
// mirrored here or this fixture silently measures a machine with no router
// while the app flies one. Same pin the striker cap already carries.
void arm_raid(drone::DroneState& d, MatchWorld& w, const sim::SimState& player,
              const bool* live_mask, const drone::DroneParams& dp,
              const world::FactionGrowth grow[2]) {
    const int own = combat::maverick_faction(d.spawn_index);
    const int ef = 1 - own;
    drone::RaidOrder ro;
    const combat::Pump& sp = w.cq.state.pumps[ef];
    const double player_range = glm::length(player.position - d.curr.position);
    d.raid_range_ok = d.raid_range_ok
                          ? (player_range > w.cq.params.raid_pause_engage_m)
                          : (player_range > w.cq.params.raid_pause_disengage_m);
    if (w.cq.params.raid_no_pause) d.raid_range_ok = true;
    if (combat::faction_raider(d.spawn_index, live_mask) && sp.alive &&
        (d.friendly_side || d.raid_range_ok)) {
        ro.active = true;
        ro.target_pos = sp.pos;
        ro.pump_idx = ef;
        // ---- D2 (app/instructor_tick.h, mirrored): the two app-known facts.
        const int raid_rank = app::raider_rank(d.spawn_index, live_mask);
        const bool deck_slot = raid_rank < dp.raid_deck_run_slots;
        ro.deck_run = deck_slot;
        {
            glm::dvec3 hc{0.0, 1.0, 0.0};
            glm::dvec3 hm{0.0};
            double ha = 0.0;
            double hb = 0.0;
            world::faction_ellipse(own, grow, hc, hm, ha, hb);
            ro.home_alive = (w.cq.state.radius_scale[own] >=
                             w.cq.params.leash_min_radius_scale) &&
                            ha > 0.0 && hb > 0.0;
        }
        // ---- S2-TUNNEL — THE ROUTER (app/instructor_tick.h, mirrored) ----
        if (dp.raid_route_via_tunnel) {
            const double direct_gap =
                app::uncovered_arc_m(d.curr.position, sp.pos, grow, kAp.R);
            // ★ D3 (app/instructor_tick.h, mirrored): a drone MID-BALLISTIC-
            // RUN is not stamped for the bore either. Rank churn under
            // raid_backfill would otherwise yank a committed dive into a
            // TRANSIT out of a 245 m/s descent.
            const bool bal_committed =
                d.ballistic == drone::BallisticPhase::CLIMB ||
                d.ballistic == drone::BallisticPhase::DIVE ||
                d.ballistic == drone::BallisticPhase::RUN;
            if (!deck_slot && !d.regroup.active && !bal_committed &&
                direct_gap > dp.raid_route_gap_max_m &&
                !w.net.spine.empty()) {
                const glm::dvec3& own_deep = w.cq.state.pumps[2 + own].pos;
                const glm::dvec3 front = w.net.spine.front().pos;
                const glm::dvec3 back = w.net.spine.back().pos;
                const bool front_is_own = glm::length(front - own_deep) <=
                                          glm::length(back - own_deep);
                const glm::dvec3 mine = front_is_own ? front : back;
                const glm::dvec3 far_m = front_is_own ? back : front;
                const double tunnel_gap =
                    app::uncovered_arc_m(d.curr.position, mine, grow, kAp.R) +
                    app::uncovered_arc_m(far_m, sp.pos, grow, kAp.R);
                if (tunnel_gap < direct_gap) {
                    ro.via_tunnel = true;
                    ro.entry_dir = front_is_own ? +1 : -1;
                }
            }
        }
    }
    d.raid = ro;
}

// The app's leash arming (app/instructor_tick.h), including the E6.4 crushed-
// dome rule.
void arm_leash(drone::DroneState& d, const MatchWorld& w,
               const world::FactionGrowth grow[2]) {
    drone::BubbleLeash lz;
    const int own = combat::maverick_faction(d.spawn_index);
    glm::dvec3 cdir{0.0, 1.0, 0.0};
    glm::dvec3 maj{0.0};
    double a = 0.0;
    double b = 0.0;
    world::faction_ellipse(own, grow, cdir, maj, a, b);
    const bool own_crushed =
        w.cq.state.radius_scale[own] < w.cq.params.leash_min_radius_scale;
    if (own_crushed || !(a > 0.0 && b > 0.0))
        world::faction_ellipse(1 - own, grow, cdir, maj, a, b);
    if (a > 0.0 && b > 0.0) {
        lz.enabled = true;
        lz.center_dir = cdir;
        lz.major_axis = maj;
        lz.a_m = a;
        lz.b_m = b;
    }
    d.leash = lz;
    // ---- ENV-1 mirror: stamp BOTH dome ellipses, each tagged live. ⚠ A
    // HAND-MIRROR IS NOT A CALLER — this must track the app's E1 block in
    // app/instructor_tick.h or the fixture measures a fleet that is blind to
    // its own air while the app flies one that is not. Kept byte-for-byte
    // parallel to it on purpose, including the live test coming FIRST.
    {
        world::FactionGrowth unit[2];  // radius_scale = 1 => the BASE radii
        drone::AirDomes ad;
        for (int slot = 0; slot < 2; ++slot) {
            const int f = (slot == 0) ? own : (1 - own);
            drone::DomeEllipse& e = ad.dome[slot];
            e.radius_scale = w.cq.state.radius_scale[f];
            if (!(e.radius_scale >= 1e-9)) continue;
            world::faction_ellipse(f, grow, e.center_dir, e.major_axis, e.a_m,
                                   e.b_m);
            glm::dvec3 bdir{0.0, 1.0, 0.0};
            glm::dvec3 bmaj{1.0, 0.0, 0.0};
            world::faction_ellipse(f, unit, bdir, bmaj, e.base_a_m, e.base_b_m);
            e.live = e.a_m > 0.0 && e.b_m > 0.0;
        }
        d.air_domes = ad;
    }
}

// ★★★ RUNG D2 — the app's reposition/return arming (app/instructor_tick.h's
// D2 block), mirrored. ⚠ MIRROR PIN, same family as the router's above: a
// change to the app's D2 block must be mirrored here or this fixture measures
// a machine with no reposition while the app flies one.
// Fills `obs` with this tick's observation (the boundary-loiter occupancy the
// rung is graded on) and returns 1 on the tick an episode ARMS.
struct RegroupObs {
    bool countable = false;  // a non-tunnel tick with a live own dome
    double frac = 0.0;       // normalized radius in MY OWN dome
    bool in_third = false;   // Chad's outer third, (arm, 1.0]
    bool in_band = false;    // the whole trigger band, (arm, lip]
    bool attacked = false;   // my own faction's pump window is open
    bool eligible = false;   // the app's FULL arming predicate this tick
    bool engaged = false;    // ... and the two things that can shadow it
    bool defending = false;
    // ★★★ D3 — the ballistic stage machine's own witness.
    int phase_in = 0;           // BallisticPhase BEFORE this tick's edges
    int phase_out = 0;          // ... and after. in != out => a transition
    double agl = 0.0;           // this tick's AGL (D3 arms only)
    double align = -2.0;        // the HORIZONTAL commit alignment in CLIMB
    bool commit_timeout = false;  // the CLIMB->DIVE edge fired on the timeout
    double commit_agl = -1.0;     // ... and the AGL it committed at
};

int arm_regroup(drone::DroneState& d, const MatchWorld& w,
                const drone::DroneParams& dp,
                const world::FactionGrowth grow[2], double dt,
                RegroupObs& obs) {
    const int own = combat::maverick_faction(d.spawn_index);
    glm::dvec3 cdir{0.0, 1.0, 0.0};
    glm::dvec3 maj{0.0};
    double a = 0.0;
    double b = 0.0;
    world::faction_ellipse(own, grow, cdir, maj, a, b);
    const bool home_alive =
        (w.cq.state.radius_scale[own] >= w.cq.params.leash_min_radius_scale) &&
        a > 0.0 && b > 0.0;
    if (!d.raid.active || d.raid.via_tunnel) {
        d.ballistic = drone::BallisticPhase::NONE;
        d.ballistic_s = 0.0;  // D3: and the climb clock
    }
    const double own_frac =
        home_alive ? drone::ellipse_frac(d.curr.position, cdir, maj, a, b,
                                         kAp.R)
                   : 0.0;
    const bool in_tunnel = maverick::is_tunnel_mode(d.mav.mode);
    // ★ THE OCCUPANCY, measured on the SAME frac law the trigger uses and
    // INDEPENDENTLY of whether the feature is on — so the OFF arm reports the
    // population and the ON arm reports what happened to it. The arm threshold
    // is read from dp so the two arms bin identically; the OFF arm pins
    // regroup_frac_arm = 0, so use the shipped 0.667 explicitly as the bin.
    constexpr double kThirdBin = 0.667;
    obs.countable = home_alive && !in_tunnel;
    obs.frac = own_frac;
    obs.in_third = obs.countable && own_frac > kThirdBin && own_frac <= 1.0;
    obs.in_band = obs.countable && own_frac > kThirdBin &&
                  own_frac <= dp.regroup_frac_lip;
    obs.attacked = w.cq.pump_attacked_s[own] > 0.0;
    const bool eligible = dp.regroup_frac_arm > 0.0 && home_alive &&
                          !d.engaged && !d.defend.active && !in_tunnel;
    obs.eligible = eligible;
    obs.engaged = d.engaged;
    obs.defending = d.defend.active;
    bool want = false;
    int reason = d.regroup.reason;
    int armed = 0;
    if (eligible && d.regroup.active) {
        d.regroup_s += dt;
        want = own_frac >= dp.regroup_frac_release &&
               d.regroup_s < dp.regroup_max_s;
    } else if (eligible) {
        const bool attacked = !dp.regroup_require_pump_attack ||
                              w.cq.pump_attacked_s[own] > 0.0;
        if (attacked && own_frac > dp.regroup_frac_arm &&
            own_frac <= dp.regroup_frac_lip) {
            want = true;
            reason = 1;
        } else if (dp.regroup_return_deck_run &&
                   // ★ D3 (mirrored): the RETURN is gated on the master dead
                   // switch too, or climb_agl = 0 would still fly a lone
                   // homecoming and the OFF arm would not be the pre-D3
                   // machine.
                   dp.raid_ballistic_climb_agl_m > 0.0 && d.raid.active &&
                   d.raid.deck_run && d.raid.home_alive &&
                   d.ballistic == drone::BallisticPhase::NONE &&
                   own_frac > 1.0 && own_frac <= dp.regroup_frac_lip) {
            want = true;
            reason = 2;
        }
        if (want) {
            d.regroup_s = 0.0;
            armed = 1;
        }
    }
    if (want) {
        glm::dvec3 pdir{0.0};
        want = drone::regroup_dir(d.curr.position, cdir, maj, a, b, kAp.R,
                                  dp.regroup_pull_frac, pdir);
        if (want) {
            const double gr = w.hf.radius_at(pdir);
            d.regroup.active = true;
            d.regroup.reason = reason;
            d.regroup.target_pos = pdir * (gr + dp.regroup_agl_m);
        } else {
            armed = 0;
        }
    }
    if (!want) {
        if (d.regroup.active && d.regroup.reason == 2) {
            d.ballistic = drone::BallisticPhase::CLIMB;
            d.ballistic_s = 0.0;
        }
        d.regroup = drone::RegroupOrder{};
        d.regroup_s = 0.0;
    }

    // ---- ★★★ RUNG D3 — the app's ballistic stage machine, mirrored.
    // ⚠ MIRROR PIN, same family as the router's and D2's above: a change to
    // the app's D3 block must be mirrored here or this fixture measures a
    // machine with no parabolic dive while the app flies one.
    obs.phase_in = static_cast<int>(d.ballistic);
    if (dp.raid_ballistic_climb_agl_m > 0.0) {
        const double agl = glm::length(d.curr.position) -
                           w.hf.radius_at(glm::normalize(d.curr.position));
        obs.agl = agl;
        const bool bal_free = !d.defend.active && !d.regroup.active &&
                              !in_tunnel;
        if (d.ballistic == drone::BallisticPhase::NONE && bal_free &&
            !d.engaged && d.raid.active && d.raid.deck_run &&
            !d.raid.via_tunnel && d.raid.home_alive && home_alive &&
            own_frac <= 1.0) {
            d.ballistic = drone::BallisticPhase::CLIMB;
            d.ballistic_s = 0.0;
        } else if (d.ballistic == drone::BallisticPhase::CLIMB) {
            d.ballistic_s += dt;
            const double align = drone::horizontal_align(
                d.curr.position, d.curr.velocity, d.raid.target_pos);
            obs.align = align;
            if ((agl >= dp.raid_ballistic_climb_agl_m &&
                 align >= dp.raid_ballistic_align_min) ||
                d.ballistic_s > dp.raid_ballistic_climb_max_s) {
                d.ballistic = drone::BallisticPhase::DIVE;
                // ★ WHICH WAY DID THE COMMIT HAPPEN? A dive that only ever
                // commits by TIMEOUT means the align gate can never pass --
                // the exact defect the horizontal projection exists to
                // prevent -- and the climb-altitude dial would be inert in
                // the commit path. Counted, not assumed.
                obs.commit_timeout = !(agl >= dp.raid_ballistic_climb_agl_m &&
                                       align >= dp.raid_ballistic_align_min);
                obs.commit_agl = agl;
            }
        } else if (d.ballistic == drone::BallisticPhase::DIVE) {
            // ★ ALTITUDE ALONE -- no deck-scope conjunct. See the app: the
            // dive starts INSIDE the runner's own dome, where deck_scope is
            // false, so a conjunct there held the committed -18 deg for a
            // measured 161 s of in-dome porpoising.
            if (agl < dp.raid_ballistic_run_agl_m)
                d.ballistic = drone::BallisticPhase::RUN;
        } else if (d.ballistic == drone::BallisticPhase::RUN) {
            // ★ ARRIVED AT THE **ENEMY** BUBBLE, on the shared ellipse_frac.
            glm::dvec3 ec{0.0, 1.0, 0.0};
            glm::dvec3 em{1.0, 0.0, 0.0};
            double ea = 0.0;
            double eb = 0.0;
            world::faction_ellipse(1 - own, grow, ec, em, ea, eb);
            const bool arrived =
                ea > 0.0 && eb > 0.0 &&
                drone::ellipse_frac(d.curr.position, ec, em, ea, eb, kAp.R) <=
                    1.0;
            // ★ ...OR AT THE DERIVED BLEED RANGE (app/instructor_tick.h,
            // mirrored): mass / (0.5 rho S Cd0) * ln(v_sprint / v_errand).
            double bleed_m = 0.0;
            const double denom = 0.5 * kAp.rho * kAp.S * kAp.Cd0;
            if (denom > 1e-9 && dp.raid_speed_target > 0.0 &&
                dp.raid_ballistic_dive_speed > dp.raid_speed_target) {
                bleed_m = kAp.mass / denom *
                          std::log(dp.raid_ballistic_dive_speed /
                                   dp.raid_speed_target);
            }
            const double pump_rng =
                glm::length(d.raid.target_pos - d.curr.position);
            if (arrived || (bleed_m > 0.0 && pump_rng < bleed_m))
                d.ballistic = drone::BallisticPhase::SPENT;
        }
    }
    obs.phase_out = static_cast<int>(d.ballistic);
    return armed;
}

// ★★★ RUNG D2 — the app's per-faction pump HP-delta latch, mirrored
// (app/instructor_tick.h, top of the cq order block).
void tick_pump_attack_latch(MatchWorld& w, const drone::DroneParams& dp,
                            double dt) {
    for (int pi = 0; pi < combat::kNumPumps; ++pi) {
        const combat::Pump& p = w.cq.state.pumps[pi];
        const int pf_own = (p.faction == combat::CQ_VALLEY) ? 0 : 1;
        if (w.cq.prev_pump_hp[pi] >= 0.0 &&
            p.hp < w.cq.prev_pump_hp[pi] - 1e-9) {
            w.cq.pump_attacked_s[pf_own] = dp.regroup_attack_window_s;
            w.cq.attacked_pump[pf_own] = pi;
        }
        w.cq.prev_pump_hp[pi] = p.hp;
    }
    for (int f = 0; f < 2; ++f) {
        w.cq.pump_attacked_s[f] =
            std::max(0.0, w.cq.pump_attacked_s[f] - dt);
        if (w.cq.pump_attacked_s[f] <= 0.0) w.cq.attacked_pump[f] = -1;
    }
}

// The app's E3.2 concurrent-striker snapshot + strike-order arming.
void count_on_order_runs(const MatchWorld& w, const drone::DroneParams& dp,
                         int out[2]) {
    out[0] = 0;
    out[1] = 0;
    if (dp.strike_concurrent_max <= 0) return;
    for (const drone::DroneState& d : w.dw.drones) {
        if (d.inert) continue;
        if (d.mav.run_on_order &&
            d.mav.mode != maverick::MaverickState::Mode::PATROL)
            ++out[combat::maverick_faction(d.spawn_index)];
    }
}

void arm_strike(drone::DroneState& d, const MatchWorld& w,
                const drone::DroneParams& dp, const int on_order_runs[2]) {
    const int own = combat::maverick_faction(d.spawn_index);
    const int ef = 1 - own;
    drone::StrikeOrder so;
    so.order_hold = dp.strike_concurrent_max > 0 &&
                    on_order_runs[own] >= dp.strike_concurrent_max;
    so.engage_m = dp.strike_engage_m;
    so.k_az = dp.strike_k_az;
    so.k_el = dp.strike_k_el;
    so.bank_cap = dp.strike_bank_cap;
    so.gamma_cap = dp.strike_gamma_cap;
    so.bail_s = dp.strike_bail_s;
    so.station_range_m = dp.strike_station_range_m;
    so.station_cos = dp.strike_station_cos;
    const combat::Pump& dpump = w.cq.state.pumps[2 + ef];
    if (dpump.alive) {
        so.active = true;
        so.target_pos = dpump.pos;
        so.pump_idx = 2 + ef;
        const glm::dvec3& own_deep = w.cq.state.pumps[2 + own].pos;
        const double d_front = glm::length(w.net.spine.front().pos - own_deep);
        const double d_back = glm::length(w.net.spine.back().pos - own_deep);
        so.entry_dir = d_front <= d_back ? +1 : -1;
    }
    d.strike = so;
}

// One arm. `dp`/`cq_over` are the dials under test; `live_backfill` selects
// the E12.1 raider designation (nullptr mask = today's static table).
struct ArmCfg {
    bool raider_backfill = false;
    double raid_dps_frac = -1.0;   // <0 = the shipped value
    int reinforce_pool_n = -999;   // -999 = the shipped value
    double transit_reach_s_per_km = -1.0;  // <0 = the shipped value (E15)
    // ★ E16 candidates. <0 / <-0.5 = the shipped value, so an unset arm is the
    // shipped table exactly.
    double avoid_air_dive_gamma = -1.0;  // the air-seek dive strength
    // ⚠ CORRECTED 2026-08-26: this said "(shipped 0 = OFF)". It is NOT off --
    // avoid_air_dive_agl_m ships 1500.0 (config/scenario.toml:548, a REQUIRED
    // key at config/load_scenario.cpp:301-302) and avoid_air_dive_gamma is
    // 0.35 by struct default (drone/drone.h:352, no TOML key). The E11 ramp is
    // LIVE in the shipped game. The law, again: a comment that describes the
    // table stops describing it the moment the table moves. Read the loader.
    double avoid_air_dive_agl_m = -1.0;  // E11's runway fade (SHIPPED 1500.0)
    // E16.D the deck's frame: -1 = the SHIPPED value, 0 = force the R6
    // bare-sphere deck, 1 = force terrain-relative. Never a bare bool -- an
    // arm that cannot say "whatever ships" cannot measure the game.
    int deck_terrain_relative = -1;
    // ★ RUNG E17 candidates. <0 = the SHIPPED value, so an unset arm flies the
    // shipped table exactly (an arm OVERRIDES config, it never DEFINES it --
    // the E16 law this fixture was itself caught breaking). The OFF arms pin
    // 0.0, which is each dial's documented no-op value.
    double run_recover_alt_m = -1.0;   // the arena spiral fix
    double run_stall_s = -1.0;         // the RUN's own bail
    double raid_attack_alt_m = -1.0;   // the raid's attack altitude
    double raid_reattack_m = -1.0;     // the raid's reattack turn
    // ★ A5 THE LIVENESS DIAL: cruise speed. spawn/faction_state seed every
    // drone's velocity from dp.speed and the cruise governor holds it every
    // tick, so a changed value MUST move every trajectory from tick 0 in this
    // fully deterministic sim. An arm pinning it exists to be ABLE to fail:
    // a bit-identical arm means BLIND FIXTURE or DEAD BRANCH, and with this
    // arm you can finally tell which. <= 0 = the shipped value.
    double speed_mps = -1.0;

    // ★★★ THE 412 m FLOOR DIFFERENTIAL (repair rung, handoff SS3): does the
    // terrain-avoid LATCH make the raid floor (C4), or does the saturated
    // aim_at equilibrium merely SIT above a latch that never fires (C1)?
    // Two knobs, both -1 = the shipped table exactly, so every existing arm in
    // this file is bit-identical (the ArmCfg law).
    //
    // avoid_latch_off: 1 pins dp.avoid_agl_enter_m to -1e18. VERIFIED
    // SUFFICIENT by reading: avoid_agl_enter_m has exactly ONE reader
    // (drone/drone.h:2538), terrain_avoid_engaged starts false
    // (drone/drone.h:1085) and is reset on respawn (drone/drone.h:1692), and
    // every other write to it CLEARS it (:2549, :2554). Release_m is untouched
    // -- the E11 air-dive ramp (drone/drone.h:2475-2479) reads release_m and
    // air_dive_agl_m and is LEFT ALONE, so arm B moves only the latch.
    // ⚠ dp is FLEET-WIDE (build_world assigns w.dw.dparams = dp), so this
    // unlatches BOTH factions. The instruments below are split per faction for
    // exactly that reason.
    int avoid_latch_off = -1;  // 1 = terrain-avoid latch structurally OFF
    // raid_gamma_cap: RaidOrder's gains have NO TOML key -- for this branch
    // the HEADER is the shipped table (drone/drone.h:855, gamma_cap = 0.52).
    // Applied AFTER arm_raid, every tick, because arm_raid rebuilds RaidOrder
    // from struct defaults each tick (:548 `drone::RaidOrder ro;`).
    double raid_gamma_cap = -1.0;  // <0 = the shipped 0.52 rad

    // ★★★ RUNG S1-DECK. All <0 = the shipped table exactly, so every existing
    // arm in this file stays bit-identical (the ArmCfg law). The control arm
    // of [.deckx] pins deck_avoid_agl_enter_m = 0.0 — the documented
    // whole-feature walk-back, i.e. the machine as it was before this rung.
    double deck_avoid_agl_enter_m = -1.0;
    double deck_avoid_agl_release_m = -1.0;
    double deck_track_agl_m = -1.0;
    double avoid_pull_net_g = -1.0;

    // ★★★ RUNG S2-TUNNEL. -1 = the shipped table exactly (the ArmCfg law), so
    // every arm that predates this rung is bit-identical.
    int raid_route_via_tunnel = -1;  // 0 = router OFF, 1 = ON
    int raid_deck_run_slots = -1;    // >=0 pins the deck-runner count
    double raid_speed_target = -1.0; // >=0 pins C1's errand speed (0 = OFF)

    // ★★★ RUNG D2 — the reposition (stage 0) + the return (stage 1). All
    // <0 / -1 = the shipped table exactly (the ArmCfg law), so every arm that
    // predates this rung stays bit-identical. regroup_frac_arm = 0.0 is the
    // documented whole-feature walk-back: the OFF arm of the [.regroup] probe.
    double regroup_frac_arm = -1.0;
    int regroup_require_pump_attack = -1;  // 0 = dwell alone, 1 = Chad's AND
    int regroup_return_deck_run = -1;      // 0 = stage 1 off, 1 = on

    // ★★★ RUNG D3 — the parabolic dive. <0 = the shipped table exactly (the
    // ArmCfg law), so every arm that predates this rung stays bit-identical.
    // raid_ballistic_climb_agl_m = 0.0 is the documented WHOLE-FEATURE dead
    // switch AND it re-gates the stage-1 return, so an arm pinning it to 0 is
    // the pre-D3 machine -- the OFF arm of [.bdeck].
    double raid_ballistic_climb_agl_m = -1.0;
    double raid_ballistic_dive_gamma = -1.0;
    double raid_ballistic_dive_speed = -1.0;
    double raid_ballistic_run_agl_m = -1.0;
};

MatchResult fly_match(const Replay& rep, const ArmCfg& cfg) {
    MatchResult out;
    drone::DroneParams dp = kScen.drone;
    dp.count = combat::kNumMavericks;
    if (cfg.transit_reach_s_per_km >= 0.0)
        dp.maverick.transit_reach_s_per_km = cfg.transit_reach_s_per_km;
    if (cfg.avoid_air_dive_gamma >= 0.0)
        dp.avoid_air_dive_gamma = cfg.avoid_air_dive_gamma;
    if (cfg.avoid_air_dive_agl_m >= 0.0)
        dp.avoid_air_dive_agl_m = cfg.avoid_air_dive_agl_m;
    // RUNG E17.
    if (cfg.run_recover_alt_m >= 0.0)
        dp.maverick.run_recover_alt_m = cfg.run_recover_alt_m;
    if (cfg.run_stall_s >= 0.0) dp.maverick.run_stall_s = cfg.run_stall_s;
    if (cfg.raid_attack_alt_m >= 0.0)
        dp.raid_attack_alt_m = cfg.raid_attack_alt_m;
    if (cfg.raid_reattack_m >= 0.0) dp.raid_reattack_m = cfg.raid_reattack_m;
    if (cfg.speed_mps > 0.0) dp.speed = cfg.speed_mps;  // A5 liveness dial
    // S2-TUNNEL overrides.
    if (cfg.raid_route_via_tunnel >= 0)
        dp.raid_route_via_tunnel = cfg.raid_route_via_tunnel != 0;
    if (cfg.raid_deck_run_slots >= 0)
        dp.raid_deck_run_slots = cfg.raid_deck_run_slots;
    if (cfg.raid_speed_target >= 0.0)
        dp.raid_speed_target = cfg.raid_speed_target;
    // FLOOR-2x2 arm B/D. eff_agl < -1e18 is never true, so the latch can never
    // arm (see ArmCfg::avoid_latch_off for the single-reader proof).
    if (cfg.avoid_latch_off == 1) dp.avoid_agl_enter_m = -1.0e18;
    // S1-DECK overrides.
    if (cfg.deck_avoid_agl_enter_m >= 0.0)
        dp.deck_avoid_agl_enter_m = cfg.deck_avoid_agl_enter_m;
    if (cfg.deck_avoid_agl_release_m >= 0.0)
        dp.deck_avoid_agl_release_m = cfg.deck_avoid_agl_release_m;
    if (cfg.deck_track_agl_m >= 0.0) dp.deck_track_agl_m = cfg.deck_track_agl_m;
    if (cfg.avoid_pull_net_g >= 0.0)
        dp.avoid_pull_net_g = cfg.avoid_pull_net_g;
    // D2 overrides.
    if (cfg.regroup_frac_arm >= 0.0) dp.regroup_frac_arm = cfg.regroup_frac_arm;
    if (cfg.regroup_require_pump_attack >= 0)
        dp.regroup_require_pump_attack = cfg.regroup_require_pump_attack != 0;
    if (cfg.regroup_return_deck_run >= 0)
        dp.regroup_return_deck_run = cfg.regroup_return_deck_run != 0;
    // D3 overrides.
    if (cfg.raid_ballistic_climb_agl_m >= 0.0)
        dp.raid_ballistic_climb_agl_m = cfg.raid_ballistic_climb_agl_m;
    if (cfg.raid_ballistic_dive_gamma >= 0.0)
        dp.raid_ballistic_dive_gamma = cfg.raid_ballistic_dive_gamma;
    if (cfg.raid_ballistic_dive_speed >= 0.0)
        dp.raid_ballistic_dive_speed = cfg.raid_ballistic_dive_speed;
    if (cfg.raid_ballistic_run_agl_m >= 0.0)
        dp.raid_ballistic_run_agl_m = cfg.raid_ballistic_run_agl_m;

    MatchWorld w;
    build_world(w, dp);
    if (cfg.deck_terrain_relative >= 0)
        w.af.deck_terrain_relative = cfg.deck_terrain_relative != 0;
    if (cfg.raid_dps_frac >= 0.0) w.cq.params.raid_dps_frac = cfg.raid_dps_frac;
    if (cfg.reinforce_pool_n != -999)
        w.cq.params.reinforce_pool_n = cfg.reinforce_pool_n;

    const int pf = w.cq.state.player_faction;
    const int enemy_f = 1 - pf;
    out.enemy_f_idx = enemy_f;
    const int max_engaged =
        combat::difficulty_params(kScen.combat.difficulty).max_engaged;

    // The DPS model in ON-STATION SECONDS (the battery cancels, see
    // build_world): a credited tick removes raid_dps_frac * dt of the pump's
    // pump_kill_seconds/raid_dps_frac budget.
    const double dt = kAp.sim_dt;
    const double per_tick = w.cq.params.raid_dps_frac * dt;
    const double hp0 = w.cq.state.pumps[0].max_hp;

    const double span = rep.span_s();
    const long long ticks = static_cast<long long>(span / dt);
    out.minutes = span / 60.0;

    std::size_t cursor = 0;
    std::size_t next_kill = 0;
    std::size_t next_pdeath = 0;
    bool live_mask[combat::kNumMavericks];
    // ★ S1-DECK per-drone episode + slew bookkeeping (observation only).
    const std::size_t n_d = w.dw.drones.size();
    std::vector<char> ep_open(n_d, 0);
    std::vector<double> ep_s(n_d, 0.0);
    std::vector<double> ep_regroup_s(n_d, 0.0);  // D2 episode clock mirror
    std::vector<double> prev_gamma(n_d, 0.0);
    std::vector<char> prev_latched(n_d, 0);
    // ★ S2-TUNNEL per-drone bookkeeping (observation only).
    std::vector<char> prev_in_net(n_d, 0);
    std::vector<int> prev_mav(n_d, 0);
    // ★ D3-BALLISTIC per-drone bookkeeping (observation only). prev_agl is
    // seeded to the sentinel so the FIRST tick, and every tick after a
    // respawn teleport, can never register as a descent through a threshold.
    constexpr double kAglNone = -1.0e18;
    std::vector<double> prev_agl(n_d, kAglNone);
    std::vector<double> ep_dead_s(n_d, 0.0);
    // ★ D3 per-drone bookkeeping (observation only): this tick's phase, so the
    // fly loop below can stratify the arrival by it, and the peak AGL of the
    // CLIMB in progress.
    std::vector<char> bal_phase(n_d, 0);
    std::vector<double> bal_apex(n_d, 0.0);

    app::AirborneWavePolicy policy;
    policy.cq = &w.cq;
    policy.dw = &w.dw;
    policy.ap = &kAp;

    for (long long t = 0; t < ticks; ++t) {
        const double now = rep.track.front().t + static_cast<double>(t) * dt;
        const sim::SimState player = replay_state_at(rep, now, cursor);
        const bool p_alive = player_alive_at(rep, cursor);

        // ★★★ S4 — THE COLLAPSE TIMESTAMP. Stamped at the TOP so the drone
        // loop below reads a value that is already settled for this tick; a
        // collapse produced by THIS tick's damage_pump is therefore stamped on
        // the NEXT tick, one 1/60 s late. That slop is deliberate and harmless:
        // the spike window it feeds is 30 s, and no aeroplane can fall out of
        // the sky in the same tick its air disappears.
        for (int f = 0; f < 2; ++f) {
            if (out.collapse_s[f] >= 0.0) continue;
            if (w.cq.state.radius_scale[f] > 0.0) continue;
            out.collapse_s[f] = now;
        }

        // --- REPLAYED PLAYER GUNNERY: the pilots his guns killed, at the
        // recorded instant, through the SAME observable the wave machine arms
        // on (inert) and the SAME roster credit the real kill takes.
        while (next_kill < rep.kills.size() && rep.kills[next_kill].t <= now) {
            const int si = rep.kills[next_kill].spawn_index;
            for (drone::DroneState& d : w.dw.drones) {
                if (d.spawn_index != si || d.inert) continue;
                d.inert = true;
                d.hp = 0.0;
                combat::on_player_kill(w.cq.state, si, w.cq.params);
            }
            ++next_kill;
        }
        // --- REPLAYED PLAYER-SIDE PUMP KILLS (limit 3): only pumps the
        // player's side owns the offense against, through the SHARED
        // damage_pump path so shrink / growth / score / the clock all fire
        // exactly as they did.
        while (next_pdeath < rep.pump_deaths.size() &&
               rep.pump_deaths[next_pdeath].t <= now) {
            const ReplayPumpDeath& pdd = rep.pump_deaths[next_pdeath];
            combat::Pump& tgt = w.cq.state.pumps[pdd.pump_idx];
            if (tgt.faction != pf && tgt.alive &&
                w.cq.state.outcome == combat::Outcome::PLAYING) {
                const std::size_t n0 = w.cq.events.size();
                combat::damage_pump(w.cq.state, pdd.pump_idx, tgt.hp + 1.0, pf,
                                    w.cq.params, &w.cq.events);
                if (w.cq.events.size() != n0) app::rebuild_conquest_bubbles(w.cq);
            }
            ++next_pdeath;
        }

        // --- (1) FOES + DEFENSE (the app's order).
        combat::assign_foes(w.dw.drones, player, pf, p_alive ? max_engaged : 0,
                            w.dw.dparams);
        // ENV-4 mirror: the HP-delta trigger, exactly as the app forms it.
        // ⚠ A HAND-MIRROR IS NOT A CALLER — without this line the fixture
        // measures a fleet whose defence is still proximity-only while the app
        // flies one that is not, which is precisely how this ladder has been
        // fooled before. Surface only (attacked_pump[f] == f); deep pumps stay
        // undefended by Chad's 2026-08-29 ruling.
        const bool surface_hurt[2] = {
            w.cq.pump_attacked_s[0] > 0.0 && w.cq.attacked_pump[0] == 0,
            w.cq.pump_attacked_s[1] > 0.0 && w.cq.attacked_pump[1] == 1};
        combat::assign_defense(w.dw.drones, w.cq.state.pumps, player, pf,
                               app::kDefendThreatRadiusM,
                               app::kDefendersPerFaction, surface_hurt);

        // --- (2) ORDERS: leash, raid, D2 regroup, strike.
        world::FactionGrowth grow[2];
        app::faction_growth_from_state(w.cq.state, grow);
        tick_pump_attack_latch(w, dp, dt);  // D2, the app's own order
        for (int i = 0; i < combat::kNumMavericks; ++i)
            live_mask[i] = false;
        for (const drone::DroneState& d : w.dw.drones)
            if (!d.inert) live_mask[d.spawn_index] = true;
        int slots[2];
        count_on_order_runs(w, dp, slots);
        for (drone::DroneState& d : w.dw.drones) {
            if (d.inert) continue;
            arm_leash(d, w, grow);
            arm_raid(d, w, player, cfg.raider_backfill ? live_mask : nullptr,
                     dp, grow);
            // FLOOR-2x2 arm C/D: RaidOrder gains have no TOML key, and
            // arm_raid rebuilds the order from struct defaults every tick, so
            // the override lands HERE, after it, every tick.
            if (cfg.raid_gamma_cap >= 0.0 && d.raid.active)
                d.raid.gamma_cap = cfg.raid_gamma_cap;
            // ★ D2, after the raid order (it reads deck_run/home_alive) and
            // before the strike, exactly as the app orders it.
            {
                const std::size_t di =
                    static_cast<std::size_t>(&d - &w.dw.drones[0]);
                const bool was = d.regroup.active;
                const int reason_before = d.regroup.reason;
                RegroupObs obs;
                const int armed = arm_regroup(d, w, dp, grow, dt, obs);
                const bool now_on = d.regroup.active;
                if (combat::maverick_faction(d.spawn_index) == enemy_f) {
                    if (obs.countable) {
                        ++out.loiter_samples;
                        if (obs.in_third) ++out.loiter_third;
                        if (obs.in_band) ++out.loiter_band;
                        if (obs.in_third && obs.attacked) ++out.loiter_coincide;
                        if (obs.attacked) ++out.loiter_attack_open;
                        if (obs.in_band && obs.attacked) {
                            ++out.loiter_band_attacked;
                            if (obs.eligible) ++out.loiter_arming;
                            if (obs.engaged) ++out.loiter_shadow_engaged;
                            if (obs.defending) ++out.loiter_shadow_defend;
                        }
                    }
                    if (armed != 0) {
                        ++out.regroup_episodes;
                        if (d.regroup.reason == 2) ++out.regroup_returns;
                    }
                    if (now_on) ++out.regroup_ticks;
                    if (was && !now_on) {
                        ++out.regroup_closed;
                        out.regroup_ep_s.push_back(ep_regroup_s[di]);
                        // COMPLETED = reached the release band, not the clock.
                        if (ep_regroup_s[di] < dp.regroup_max_s - 1e-9)
                            ++out.regroup_completed;
                        if (reason_before == 2) ++out.regroup_return_ends;
                    }
                    // ---- ★★★ D3 — the phase edges, counted where they
                    // happen. obs.phase_in/out bracket the app's own stage
                    // machine, so an edge here IS an edge in the shipped
                    // code, not a re-derivation of it.
                    using BP = drone::BallisticPhase;
                    const int pi_ = obs.phase_in;
                    const int po_ = obs.phase_out;
                    if (d.raid.active && d.raid.deck_run && !d.raid.via_tunnel)
                        ++out.bal_eligible_ticks;
                    if (po_ >= 0 && po_ < 6) ++out.bal_ticks[po_];
                    if (pi_ != po_) {
                        if (po_ == static_cast<int>(BP::CLIMB) &&
                            pi_ == static_cast<int>(BP::NONE))
                            ++out.bal_arm;
                        if (po_ == static_cast<int>(BP::DIVE)) {
                            ++out.bal_commit;
                            if (obs.commit_timeout) ++out.bal_commit_timeout;
                            out.bal_commit_agl.push_back(obs.commit_agl);
                            out.bal_apex_agl.push_back(bal_apex[di]);
                        }
                        if (po_ == static_cast<int>(BP::RUN)) ++out.bal_run;
                        if (po_ == static_cast<int>(BP::SPENT)) {
                            ++out.bal_spent;
                            out.bal_arrive_v.push_back(
                                glm::length(d.curr.velocity));
                        }
                        // A LIVE PHASE ZEROED. Not a completion: the order
                        // dropped, the router took it, or it respawned.
                        if (po_ == static_cast<int>(BP::NONE) &&
                            pi_ != static_cast<int>(BP::NONE) &&
                            pi_ != static_cast<int>(BP::SPENT))
                            ++out.bal_lost;
                    }
                    bal_apex[di] = (po_ == static_cast<int>(BP::CLIMB))
                                       ? std::max(bal_apex[di], obs.agl)
                                       : 0.0;
                }
                ep_regroup_s[di] = now_on ? d.regroup_s : 0.0;
                bal_phase[di] = static_cast<char>(obs.phase_out);
            }
            arm_strike(d, w, dp, slots);
        }

        // --- (3) FLY. Each drone against ITS foe, off a pre-loop snapshot.
        std::vector<sim::SimState> foe_snap;
        foe_snap.reserve(w.dw.drones.size());
        for (const drone::DroneState& d : w.dw.drones)
            foe_snap.push_back(d.curr);
        for (std::size_t i = 0; i < w.dw.drones.size(); ++i) {
            drone::DroneState& d = w.dw.drones[i];
            if (d.inert) continue;
            const sim::SimState* target = nullptr;
            if (d.foe == drone::kFoePlayer)
                target = &player;
            else if (d.foe >= 0 && d.foe < static_cast<int>(foe_snap.size()))
                target = &foe_snap[d.foe];
            // E16 forensics: the state it was flying when it died, sampled
            // BEFORE the tick that kills it (foe_snap[i] is that same state,
            // but this is read explicitly so the coupling is not silent).
            const bool is_enemy = combat::maverick_faction(d.spawn_index) ==
                                  enemy_f;
            const sim::SimState pre = d.curr;
            // ★ D3: the latch bits AS FLOWN, before drone::tick's respawn
            // clears them. d.deck_scope is cleared by respawn_in_place, so a
            // wreck read post-tick reports "not on the deck" for pure
            // bookkeeping reasons -- the same trap the episode block above
            // documents.
            const bool pre_deck_scope = d.deck_scope;
            double pre_frac = 1.0;
            const int disp =
                d.defend.active ? 0
                : d.raid.active ? 1
                : (maverick::is_tunnel_mode(d.mav.mode) ||
                   d.mav.mode != maverick::MaverickState::Mode::PATROL)
                    ? 2
                : d.engaged ? 3
                            : 4;
            if (is_enemy) {
                // ★ S2-TUNNEL: the ROUTER's own duty, counted over EVERY
                // enemy raid tick (tunnel modes included -- the whole point is
                // that the raid is now flown inside the net, so filtering
                // tunnel modes out, as the A5 raid telemetry above must, would
                // structurally hide the thing being measured).
                if (d.raid.active) {
                    if (d.raid.via_tunnel) ++out.raid_vt_ticks;
                    if (w.net.contains(pre.position)) ++out.raid_net_ticks;
                }
                pre_frac = sim::atm_frac_at(pre.position, &w.env, kAp);
                out.enemy_air_sum += pre_frac * dt;
                if (pre_frac < 0.25) out.enemy_thin_s += dt;
                out.duty_s[disp] += dt;
                // ★ A5: observe the raid pitch command AT THE PRE-TICK STATE.
                // This mirrors the raid branch's own aim point and aim_at call
                // (drone/drone.h:2278-2293) verbatim, recomputed here because
                // drone::tick does not export its Steer. aim_at is pure -- this
                // observes, it never steers. See MatchResult's warning about
                // what this gate does and does not reach.
                if (d.raid.active && !maverick::is_tunnel_mode(d.mav.mode)) {
                    const double pump_r = glm::length(d.raid.target_pos);
                    const glm::dvec3 aim_pt =
                        pump_r > 1e-6
                            ? d.raid.target_pos +
                                  (d.raid.target_pos / pump_r) *
                                      w.dw.dparams.raid_attack_alt_m
                            : d.raid.target_pos;
                    const maverick::Steer st = maverick::aim_at(
                        pre, aim_pt, d.raid.k_az, d.raid.k_el, d.raid.bank_cap,
                        d.raid.gamma_cap);
                    const double alt_err =
                        glm::length(pre.position) - glm::length(aim_pt);
                    const double frac =
                        d.raid.gamma_cap > 1e-9
                            ? std::abs(st.target_gamma) / d.raid.gamma_cap
                            : 0.0;
                    out.max_alt_err_m =
                        std::max(out.max_alt_err_m, std::abs(alt_err));
                    out.max_gamma_cmd_frac =
                        std::max(out.max_gamma_cmd_frac, frac);
                    if (frac >= 0.99) out.raid_pinned_s += dt;
                    // ★ FLOOR-2x2. `frac` above is taken from st.target_gamma,
                    // which aim_at has ALREADY CLAMPED (drone/maverick.h:688),
                    // so it can never exceed 1.00 -- it says "pinned", never
                    // "how deep". The RAW ratio below is the unclamped demand;
                    // it is the number that says whether widening the cap can
                    // possibly change anything.
                    ++out.raid_ticks;
                    const double raw =
                        d.raid.gamma_cap > 1e-9
                            ? std::abs(d.raid.k_el * maverick::gc_elevation(
                                                         pre.position, aim_pt)) /
                                  d.raid.gamma_cap
                            : 0.0;
                    if (raw >= 1.0) ++out.sat_raid_ticks;
                    out.max_gamma_raw_frac =
                        std::max(out.max_gamma_raw_frac, raw);
                    const double pump_rng =
                        glm::length(d.raid.target_pos - pre.position);
                    if (pump_rng <= 2.0 * combat::RaidParams{}.raid_range_m) {
                        const glm::dvec3 up = glm::normalize(pre.position);
                        out.raid_agl.push_back(glm::length(pre.position) -
                                               w.env.ground->radius_at(up));
                    }
                }
            }
            const bool pre_in_net = w.net.contains(pre.position);
            const int pre_mode = static_cast<int>(d.mav.mode);
            const drone::DroneTickResult r =
                drone::tick(d, kAp, w.dw.dparams, &w.env, target);
            // ★★★ S2-TUNNEL — THE GRINDER INSTRUMENTS (observation only).
            // ENTRY is a !in_net -> in_net transition; an IN-NET DEATH is a
            // respawn whose PRE-tick position was inside the net (the wreck's
            // own position, never the respawn slot -- the same discipline the
            // E16 crash forensics carry). CLIMB_OUT -> PATROL is the
            // completion. All three read the SAME w.net.contains the plant's
            // crash-suspension predicate reads, never a copy.
            if (is_enemy) {
                const bool now_in_net = w.net.contains(d.curr.position);
                if (!prev_in_net[i] && now_in_net && !r.respawned) {
                    ++out.net_entries;
                    // ★ S4: the same transition, after the enemy's own dome
                    // died -- "do they still reach the tunnel?"
                    if (out.collapse_s[enemy_f] >= 0.0) ++out.post_net_entries;
                }
                if (r.respawned && pre_in_net) ++out.in_net_deaths;
                if (pre_mode ==
                        static_cast<int>(
                            maverick::MaverickState::Mode::CLIMB_OUT) &&
                    d.mav.mode == maverick::MaverickState::Mode::PATROL)
                    ++out.climbout_done;
                prev_in_net[i] = (now_in_net && !r.respawned) ? 1 : 0;
                prev_mav[i] = static_cast<int>(d.mav.mode);
            }
            // ★ FLOOR-2x2 (observation only, post-tick).
            if (is_enemy) {
                if (d.terrain_avoid_engaged) {
                    ++out.avoid_ticks_enemy;
                    if (disp == 1) ++out.avoid_raid_ticks;
                }
                // ★★★ S1-DECK. All post-tick reads of state the tick made.
                const double r_now = glm::length(d.curr.position);
                const glm::dvec3 up_n = d.curr.position / r_now;
                // ★★★ S4 — WHAT A COLLAPSED FACTION ACTUALLY DOES. Gated on
                // the enemy's OWN collapse timestamp, and skipped on a respawn
                // tick (the latches are cleared there, so a respawn tick would
                // read as "not flying the deck" for reasons that are pure
                // bookkeeping). Observation only.
                if (out.collapse_s[enemy_f] >= 0.0 && !r.respawned) {
                    out.post_fly_s += dt;
                    if (d.deck_scope) out.post_deck_s += dt;
                    if (d.raid.active) out.post_raid_s += dt;
                    if (d.engaged) out.post_engaged_s += dt;
                    out.post_agl.push_back(
                        r_now - w.env.ground->radius_at(up_n));
                }
                if (d.deck_scope) {
                    ++out.deck_ticks;
                    if (d.terrain_avoid_engaged) ++out.deck_latch_ticks;
                    if (d.engaged) {
                        ++out.deck_fight_ticks;
                        if (d.terrain_avoid_engaged)
                            ++out.deck_fight_latch_ticks;
                    }
                    if (d.raid.active) ++out.deck_raid_ticks;
                    const double a =
                        r_now - w.env.ground->radius_at(up_n);
                    out.deck_agl.push_back(a);
                    if (!d.engaged) out.deck_errand_agl.push_back(a);
                }
                // ⚠ d.deck_scope is CLEARED by respawn_in_place inside
                // drone::tick (like every other latch), so "d.deck_scope on
                // the respawn tick" is structurally 0 and measured 0/16 on the
                // first run. The deck attribution is the OPEN EPISODE below:
                // ep_open[i] means the PREVIOUS tick was flown in deck scope,
                // and at 85-156 m/s nothing crosses from the deck into dome
                // air or the tunnel net inside one 1/60 s tick.
                // THE CROSSING: a contiguous deck-scope episode. It is an
                // ATTEMPT if it ran >= 10 s or ended in a wreck; SURVIVED if
                // it ended by leaving deck scope (back into dome air / the
                // net) rather than by a respawn.
                if (r.respawned) {
                    if (ep_open[i]) {
                        ++out.deck_cross_attempts;
                        ++out.deck_crashes;  // the crossing was LOST
                        out.cross_dead_s.push_back(ep_dead_s[i]);
                    }
                    ep_open[i] = 0;
                    ep_s[i] = 0.0;
                    ep_dead_s[i] = 0.0;
                } else if (d.deck_scope) {
                    ep_open[i] = 1;
                    ep_s[i] += dt;
                    // ★ D3: DEAD AIR inside this crossing. pre_frac is
                    // sim::atm_frac_at at the state the plant actually flew --
                    // the same call the lift model reads, never a copy.
                    if (pre_frac < w.dw.dparams.avoid_air_frac_hard)
                        ep_dead_s[i] += dt;
                } else {
                    if (ep_open[i] && ep_s[i] >= 10.0) {
                        ++out.deck_cross_attempts;
                        ++out.deck_cross_survived;
                        out.cross_dead_s.push_back(ep_dead_s[i]);
                    }
                    ep_open[i] = 0;
                    ep_s[i] = 0.0;
                    ep_dead_s[i] = 0.0;
                }
                // ★★★ D3 — THE ARRIVAL. A DOWNWARD crossing of the top of the
                // deck's air column, and of the full-air lid, measured on the
                // real surface only: deck scope true (outside dome air) AND
                // outside the bore. The bore exclusion is not cosmetic -- the
                // deck-scope probe latches true 2 km under the DEM, which is
                // exactly what put a −842 m p10 into [.deckx]'s deck AGL row.
                {
                    const double a_now = r_now - w.env.ground->radius_at(up_n);
                    const bool surface_deck =
                        d.deck_scope && !w.net.contains(d.curr.position);
                    if (!r.respawned && surface_deck &&
                        prev_agl[i] > kAglNone * 0.5) {
                        const double spd_a = glm::length(d.curr.velocity);
                        const double sink =
                            -glm::dot(d.curr.velocity, up_n);
                        const double gam_a =
                            spd_a > 1e-6
                                ? std::asin(glm::clamp(
                                      -sink / spd_a, -1.0, 1.0)) *
                                      180.0 / kPi
                                : 0.0;
                        const double top = kGame.atmosphere.deck_agl_m +
                                           kGame.atmosphere.deck_soft_m;
                        // ★ THE PHASE STRATIFICATION. bal_phase[i] is this
                        // tick's app-side phase, so "arrived on a live
                        // ballistic run" is the shipped state machine's own
                        // word, not a heuristic reconstruction.
                        const int bp = static_cast<int>(bal_phase[i]);
                        const bool bal_live = bp >= 2 && bp <= 4;
                        if (prev_agl[i] > top && a_now <= top) {
                            out.arr_v_320.push_back(spd_a);
                            out.arr_sink_320.push_back(sink);
                            out.arr_gamma_320.push_back(gam_a);
                            if (bal_live) {
                                out.bal_v_320.push_back(spd_a);
                                out.bal_gamma_320.push_back(gam_a);
                            }
                        }
                        if (prev_agl[i] > kGame.atmosphere.deck_agl_m &&
                            a_now <= kGame.atmosphere.deck_agl_m) {
                            out.arr_v_120.push_back(spd_a);
                            out.arr_sink_120.push_back(sink);
                            if (bal_live) out.bal_v_120.push_back(spd_a);
                        }
                    }
                    // ★ THE POROPOISE WATCH. The red team's defect 2: if the
                    // DIVE hands over below where the terrain latch arms, the
                    // runner crosses HIGHER and MORE gun-muted than it does
                    // today. Both halves are measured on RUN/SPENT ticks in
                    // surface deck scope, against the fleet rows above.
                    if (!r.respawned && surface_deck &&
                        (bal_phase[i] == 4 || bal_phase[i] == 5)) {
                        out.bal_agl.push_back(a_now);
                    }
                    if (!r.respawned && bal_phase[i] >= 2 &&
                        bal_phase[i] <= 4) {
                        ++out.bal_phase_ticks;
                        if (d.terrain_avoid_engaged) ++out.bal_mute_ticks;
                    }
                    prev_agl[i] = r.respawned ? kAglNone : a_now;
                }
                // THE MEASURED PULL-UP g. Only across consecutive latched
                // ticks with no respawn between them, so it is a real rotation
                // and not a teleport.
                const double spd = glm::length(d.curr.velocity);
                const double gam =
                    spd > 1e-6 ? std::asin(glm::clamp(
                                     glm::dot(d.curr.velocity, up_n) / spd,
                                     -1.0, 1.0))
                               : 0.0;
                if (prev_latched[i] && d.terrain_avoid_engaged &&
                    !r.respawned) {
                    const double gn =
                        spd * ((gam - prev_gamma[i]) / dt) / 9.81;
                    if (gn > 0.0) out.pull_g.push_back(gn);
                }
                prev_gamma[i] = gam;
                prev_latched[i] =
                    (d.terrain_avoid_engaged && !r.respawned) ? 1 : 0;
                fold_state(out.hash_enemy, d.curr);
            } else {
                if (d.terrain_avoid_engaged) ++out.avoid_ticks_friendly;
                fold_state(out.hash_friendly, d.curr);
            }
            if (r.respawned) {
                if (is_enemy) {
                    ++out.enemy_crashes;
                    const double sp = glm::length(pre.velocity);
                    const glm::dvec3 up = glm::normalize(pre.position);
                    const double gam =
                        sp > 1e-6 ? std::asin(glm::clamp(
                                        glm::dot(pre.velocity / sp, up), -1.0,
                                        1.0)) * 180.0 / kPi
                                  : 0.0;
                    out.crash_frac_sum += pre_frac;
                    out.crash_gamma_sum += gam;
                    out.crash_speed_sum += sp;
                    if (pre_frac < 0.25) ++out.crash_thin;
                    if (gam < -30.0) ++out.crash_steep;
                    if (sp > 150.0) ++out.crash_fast;
                    if (pre_frac < w.dw.dparams.avoid_air_frac_full &&
                        w.dw.dparams.avoid_air_dive_gamma > 0.0)
                        ++out.crash_seeking;
                    // ★★★ D3 — WHERE, IN ALTITUDE, THIS WRECK HAPPENED. The
                    // design's falsifier: if the crossing wrecks are down in
                    // the 60/110 band rather than up in the dome-exit
                    // transition, the ballistic run fixes deaths that no
                    // longer dominate. Pre-tick state only.
                    const double w_agl =
                        glm::length(pre.position) - w.env.ground->radius_at(up);
                    const double w_sink = -glm::dot(pre.velocity, up);
                    out.wreck_agl.push_back(w_agl);
                    out.wreck_sink.push_back(w_sink);
                    const double bins[5] = {60.0, 110.0, 250.0, 400.0, 1000.0};
                    int bi = 5;
                    for (int bk = 0; bk < 5; ++bk)
                        if (w_agl < bins[bk]) {
                            bi = bk;
                            break;
                        }
                    ++out.wreck_agl_bin[bi];
                    if (pre_deck_scope) ++out.wreck_deck_scope;
                    if (pre_in_net) ++out.wreck_in_net;
                    if (pre_deck_scope && !pre_in_net) ++out.wreck_deck_surface;
                    ++out.crash_by[disp];
                    // ★★★ S4 — WHERE THIS WRECK SITS RELATIVE TO THE MOMENT
                    // ITS FACTION LOST ITS AIR. Chad ruled the spike INTENDED,
                    // so it is bucketed, never smoothed away.
                    const double cs = out.collapse_s[enemy_f];
                    if (cs < 0.0)
                        ++out.enemy_crash_pre;
                    else if (now - cs <= kSpikeS)
                        ++out.enemy_crash_spike;
                    else
                        ++out.enemy_crash_post;
                }
                // The app's FIX-F2 relocation into the drone's own air, with
                // S4's ladder: own dome alive -> own dome; own dome gone ->
                // OWN ZONE AT THE DECK. The cross-faction fallback is DELETED
                // here exactly as it is in app/instructor_tick.h -- this is a
                // hand-mirror, so a divergence here measures a machine the app
                // does not fly.
                const int own = combat::maverick_faction(d.spawn_index);
                const bool in_dome = app::place_in_faction_air(
                    d, own, grow, kAp, w.dw, d.spawn_index,
                    grow[own].ceiling_scale * w.cq.bubble_ceiling_m,
                    w.cq.bubble_ceil_soft_m);
                if (!in_dome) {
                    if (app::place_on_faction_deck(d, own, kAp, w.dw, &w.env,
                                                   d.spawn_index))
                        ++out.deck_respawns;
                    else
                        ++out.stock_respawns;
                }
            }
        }

        // ★ S2-TUNNEL — MULTI-SHIP BORE. The design's risk 5: no drone-vs-
        // drone airframe collision exists anywhere in this tree (rounds sweep
        // airframes, airframes never sweep each other), so concurrent bore
        // traffic carries no mechanical risk -- only a visual stacking one.
        // Reported, never asserted on, because Chad rules on how it LOOKS.
        {
            std::vector<glm::dvec3> in_net_pos;
            for (const drone::DroneState& d : w.dw.drones) {
                if (d.inert) continue;
                if (combat::maverick_faction(d.spawn_index) != enemy_f)
                    continue;
                if (w.net.contains(d.curr.position))
                    in_net_pos.push_back(d.curr.position);
            }
            out.max_in_net = std::max(
                out.max_in_net, static_cast<int>(in_net_pos.size()));
            for (std::size_t a = 0; a + 1 < in_net_pos.size(); ++a)
                for (std::size_t b = a + 1; b < in_net_pos.size(); ++b)
                    out.min_pair_sep_m = std::min(
                        out.min_pair_sep_m,
                        glm::length(in_net_pos[a] - in_net_pos[b]));
        }

        // --- (4) REINFORCEMENT WAVES (after the deaths are on the books).
        {
            const combat::ReinforceParams rfp =
                app::wave_params(w.cq, w.dw.dparams.hp);
            const long long before = w.cq.reinforce.waves;
            combat::reinforce_tick(w.cq.reinforce, w.dw.drones, w.cq.state, rfp,
                                   policy, dt);
            out.enemy_waves +=
                static_cast<int>(w.cq.reinforce.waves - before) > 0
                    ? static_cast<int>(w.cq.reinforce.used[enemy_f] > 0)
                    : 0;
        }

        // --- (5) THE MATCH CLOCK, then the collapse scramble.
        combat::conquest_countdown_tick(w.cq.state, dt, w.cq.params);

        // --- (6) THE RAID / STRIKE DPS CREDIT (app/instructor_tick.h's site,
        // verbatim), with the player side's credit SUPPRESSED (limit 3).
        if (w.cq.state.outcome == combat::Outcome::PLAYING) {
            const combat::RaidParams rp_surface;
            for (const drone::DroneState& d : w.dw.drones) {
                if (d.inert) continue;
                const int own = combat::maverick_faction(d.spawn_index);
                const bool is_enemy_side = own == enemy_f;
                if (is_enemy_side) out.enemy_alive_s += dt;
                const combat::RaidParams rp_deep = combat::strike_params(
                    d.strike.station_range_m, d.strike.station_cos);
                int pi = -1;
                const combat::RaidParams* rp = nullptr;
                const bool in_tunnel = maverick::is_tunnel_mode(d.mav.mode);
                if (d.raid.active && !in_tunnel) {
                    pi = d.raid.pump_idx;
                    rp = &rp_surface;
                    if (is_enemy_side) out.enemy_raidduty_s += dt;
                } else if (d.strike.active && !d.strike_bailed &&
                           d.mav.mode == maverick::MaverickState::Mode::RUN) {
                    pi = d.strike.pump_idx;
                    rp = &rp_deep;
                    if (is_enemy_side) out.enemy_strikeduty_s += dt;
                }
                if (pi < 0 || pi >= 4) continue;
                combat::Pump& tgt = w.cq.state.pumps[pi];
                if (!tgt.alive) continue;
                if (is_enemy_side &&
                    glm::length(tgt.pos - d.curr.position) <= rp->raid_range_m) {
                    out.enemy_near_s += dt;
                    // ★ S4: "do they still reach a pump?" after their own
                    // dome died.
                    if (out.collapse_s[enemy_f] >= 0.0) out.post_near_s += dt;
                }
                if (!combat::raider_on_station(d.curr, tgt.pos, *rp)) continue;
                if (!is_enemy_side) continue;  // limit 3: replay owns his side
                // ★ A2 (audit 2026-08-25): the !tgt.alive gate above makes
                // this counter SATURATE at 2*kill/dps once both pumps fall.
                // Kept alive-gated ON PURPOSE: a raid/strike order dies with
                // its pump (arm_raid requires sp.alive at :518; arm_strike
                // requires dpump.alive at :582; both re-run every tick at
                // :726-727), so "on-station vs a corpse" is not a state the
                // fleet can hold, and an unconditional twin would accrue only
                // the odd tick between the death and the order clearing. The
                // de-saturated instrument is MatchResult::onstation_rate().
                out.enemy_onstation_s += dt;
                const std::size_t n0 = w.cq.events.size();
                combat::damage_pump(w.cq.state, pi, per_tick, own,
                                    w.cq.params, &w.cq.events);
                if (w.cq.events.size() != n0) app::rebuild_conquest_bubbles(w.cq);
            }
        }

        // --- (7) ★★★ S4 — THE COLLAPSE HOOK IS GONE.
        // A block here used to mirror the app's SCRAMBLE ON COLLAPSE — and it
        // was already a FORK: it latched w.cq.scrambled[] and never relocated
        // anybody, while the fixture banner claimed it ran the scramble. The
        // app's hook is now DELETED (Chad 2026-08-26, "must not teleport"), so
        // the fork is closed by deletion rather than by writing a teleport into
        // the harness. Nothing happens on a collapse tick, here or in the app.
        // The timestamp the S4 numbers are measured against is stamped at the
        // TOP of this loop, before the drone loop that reads it.

        // --- Pump bookkeeping.
        for (int i : {0, 2})
            if (w.cq.state.pumps[i].alive &&
                w.cq.state.pumps[i].faction == pf)
                out.player_pump_alive_s += dt;
        for (int i = 0; i < 4; ++i) {
            if (!w.cq.state.pumps[i].alive && !out.pump_dead[i]) {
                out.pump_dead[i] = true;
                out.pump_death_s[i] = now;
                if (w.cq.state.pumps[i].faction == pf &&
                    out.player_pumps_lost_s < 0.0)
                    out.player_pumps_lost_s = now;
            }
        }
    }
    for (int i = 0; i < 4; ++i)
        out.pump_hp_frac[i] =
            std::max(0.0, w.cq.state.pumps[i].hp) / (hp0 > 0.0 ? hp0 : 1.0);
    return out;
}

void report(const char* label, const MatchResult& r) {
    std::printf(
        "\n[P-H %s] %.1f min\n"
        "  player pumps  surface %.0f%%   deep %.0f%%   (first loss %s)\n"
        "  enemy pumps   surface %.0f%%   deep %.0f%%\n"
        "  ENEMY on-station %.1f s   near %.1f s   raid duty %.0f s"
        "   strike RUN %.0f s\n"
        "  ENEMY PRESSURE %.4f on-station s / pump-alive s"
        "   (seconds ceiling = 2*kill/dps; the RATE is the instrument)\n"
        "  enemy plane-seconds %.0f   crashes %d\n",
        label, r.minutes, 100.0 * r.pump_hp_frac[0], 100.0 * r.pump_hp_frac[2],
        r.player_pumps_lost_s < 0.0 ? "none" : "YES",
        100.0 * r.pump_hp_frac[1], 100.0 * r.pump_hp_frac[3],
        r.enemy_onstation_s, r.enemy_near_s, r.enemy_raidduty_s,
        r.enemy_strikeduty_s, r.onstation_rate(), r.enemy_alive_s,
        r.enemy_crashes);
    // ★ A5: the raid law's WOULD-BE pitch command while a raid order is live
    // (NOT necessarily the flown command -- see MatchResult's warning).
    std::printf(
        "  RAID CMD max|alt_err| %.0f m   max|gamma_cmd|/cap %.2f"
        "   pinned(>=0.99 cap) %.2f s of %.0f raid-order s\n",
        r.max_alt_err_m, r.max_gamma_cmd_frac, r.raid_pinned_s,
        r.enemy_raidduty_s);
    if (r.player_pumps_lost_s >= 0.0)
        std::printf("  >>> A PLAYER PUMP FELL at %.1f s (%.1f min)\n",
                    r.player_pumps_lost_s, r.player_pumps_lost_s / 60.0);
    const double n = r.enemy_crashes > 0 ? r.enemy_crashes : 1.0;
    std::printf(
        "  CRASHES %d (%.2f/min, Chad signed 0.94)  thin-air %d  steep %d"
        "  fast %d  seeking %d\n"
        "    mean at death: air %.2f   gamma %.1f deg   speed %.0f m/s\n"
        "    enemy plane-seconds in air<0.25: %.0f s (%.0f%% of %.0f)\n",
        r.enemy_crashes, r.enemy_crashes / std::max(1e-9, r.minutes),
        r.crash_thin, r.crash_steep, r.crash_fast, r.crash_seeking,
        r.crash_frac_sum / n, r.crash_gamma_sum / n, r.crash_speed_sum / n,
        r.enemy_thin_s,
        100.0 * r.enemy_thin_s / std::max(1e-9, r.enemy_alive_s),
        r.enemy_alive_s);
    // ★★★ S4 — THE COLLAPSE. Chad ruled the deaths at the moment of air loss
    // INTENDED, so the spike is printed as its own number and is never folded
    // into the steady rate.
    {
        double med_agl = -1.0;
        if (!r.post_agl.empty()) {
            std::vector<double> v = r.post_agl;
            std::sort(v.begin(), v.end());
            med_agl = v[v.size() / 2];
        }
        std::printf(
            "  S4 COLLAPSE  valley %s   sudbury %s\n"
            "    ENEMY wrecks: before its collapse %d | in the %.0f s SPIKE "
            "after it %d | later %d\n"
            "    respawns taken: own-zone DECK %d   stock (no ground) %d\n"
            "    AFTER ITS DOME DIED, the enemy flew %.0f plane-s:"
            " deck %.0f s (%.0f%%)  raid-order %.0f s  engaged %.0f s"
            "  at-a-pump %.0f s  tunnel entries %lld  median AGL %.0f m\n",
            r.collapse_s[0] < 0.0 ? "alive" : "DEAD",
            r.collapse_s[1] < 0.0 ? "alive" : "DEAD", r.enemy_crash_pre,
            kSpikeS, r.enemy_crash_spike, r.enemy_crash_post, r.deck_respawns,
            r.stock_respawns, r.post_fly_s, r.post_deck_s,
            100.0 * r.post_deck_s / std::max(1e-9, r.post_fly_s),
            r.post_raid_s, r.post_engaged_s, r.post_near_s,
            r.post_net_entries, med_agl);
        if (r.collapse_s[0] >= 0.0)
            std::printf("    valley dome died at %.1f s\n", r.collapse_s[0]);
        if (r.collapse_s[1] >= 0.0)
            std::printf("    sudbury dome died at %.1f s\n", r.collapse_s[1]);
    }
    static const char* kDisp[5] = {"defend", "raid", "tunnel", "fight",
                                   "patrol"};
    std::printf("    by disposition (crashes / duty-seconds):");
    for (int i = 0; i < 5; ++i)
        std::printf("  %s %d/%.0f", kDisp[i], r.crash_by[i], r.duty_s[i]);
    std::printf("\n");
}

}  // namespace

// ---------------------------------------------------------------------------
// P-H.0 — THE FIXTURE ANSWERS THE TAPE (the calibration leg). Before any arm
// is believed, the SHIPPED arm has to land near the match it is replaying.
// ---------------------------------------------------------------------------
TEST_CASE("probe P-H: the shipped conquest match reproduces the signed tape") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent (" << kReplayPath
                                     << ") -- P-H skipped, not failed");
        SUCCEED();
        return;
    }
    REQUIRE(rep.track.size() > 6000);
    REQUIRE(rep.kills.size() == 9);
    REQUIRE(rep.pump_deaths.size() == 2);

    // ★ THE TAPE-7 ARM, PINNED EXPLICITLY -- not "whatever ships today".
    // Calibration compares the probe against the match Chad actually flew, so
    // it must fly the dials THAT MATCH had (backfill off, raid_dps_frac 0.5).
    // Reading them from config instead would silently re-point the calibration
    // at every later retune and quietly stop testing anything.
    ArmCfg tape7;
    tape7.raider_backfill = false;
    tape7.raid_dps_frac = 0.5;
    // ★ E16: THE WORLD IS PART OF THE ARM. Tape 7 was flown on the bare-sphere
    // deck with E15's tunnel rope off, so the tape-7 arm pins both. Letting
    // them read config would re-point this comparison at every later world
    // change and quietly stop testing what it names.
    tape7.deck_terrain_relative = 0;
    tape7.transit_reach_s_per_km = 0.0;
    const MatchResult r = fly_match(rep, tape7);
    report("TAPE-7 ARM", r);

    // The tape's own numbers, for the record and for the reader:
    //   player surface pump ended at 61% of max, deep pump at 32%,
    //   ~32 s of enemy on-station credit inferred from the HP slope.
    INFO("tape 7: surface 61%, deep 32%, ~32 s enemy on-station,"
         " 4419 enemy plane-seconds");
    // The fixture must MOVE the pumps -- a probe where the enemy never
    // touches a pump is the blind fixture this rung exists to replace.
    CHECK(r.enemy_onstation_s > 5.0);

    // (1) ATTRITION. The leg that says the replay is wired to the right
    // pilots: the tape's FIVE enemy slots were airborne for a combined 4419 s
    // of its 1343 s match, and the probe has to land near that without being
    // told it.
    //
    // ★★ SCALED BY THE WING, AND HERE IS WHY THE BAND IS WIDE. RUNG E13 re-split
    // the roster 7/3 on Chad's "we can be outnumbered", so the fixture can no
    // longer fly the 5/5 world tape 7 was flown in AT ALL -- and an absolute
    // 3900-4900 pin, which is what this leg used to be, went red the moment
    // the roster moved. That pin was the ladder's own recurring trap in a new
    // hat: a constant DESCRIBING the shipped table stops describing it the
    // moment the table moves.
    //
    // ⚠ SO BE HONEST ABOUT WHAT SURVIVES: with a wing that is not 5, the
    // tape-7 attrition calibration is INFORMATIONAL, not a certification. The
    // original calibration (4430 vs 4419, +0.2%) was performed once, at 5/5,
    // BEFORE any arm was believed, and it is recorded in the spec at §E12.I.
    // A fresh calibration needs a tape FLOWN at the shipped roster -- Chad's
    // next fly produces one. What this clause still forbids is the failure
    // that would actually invalidate an A/B: a fixture that has drifted an
    // order of magnitude, or one wired to the wrong faction's slots.
    const double expect_alive =
        4419.0 * static_cast<double>(combat::kSudburyTeamSize) / 5.0;
    INFO("wing-scaled expectation = " << expect_alive
                                      << " s (4419 at a wing of 5)");
    CHECK(r.enemy_alive_s > expect_alive * 0.75);
    CHECK(r.enemy_alive_s < expect_alive * 1.25);

    // (2) OFFENSIVE OUTPUT. Tape 7's enemy earned ~32 s of on-station credit
    // (inferred from the pump-HP slope: the player's surface pump ended at
    // 61% and his deep pump at 32% of a 30 s budget each). The probe is a
    // RELATIVE instrument -- flat terrain and the open-loop replay both
    // favour the enemy slightly, and it distributes its credit deeper than
    // the tape did -- so the band is generous on purpose. What it forbids is
    // a fixture that has drifted an order of magnitude either way, which is
    // the only failure that would invalidate an A/B run against it.
    CHECK(r.enemy_onstation_s > 15.0);
    CHECK(r.enemy_onstation_s < 70.0);
    // ★ A2 (audit 2026-08-25): this arm's ceiling (dps 0.5) is 60.0 s and
    // sits INSIDE the band, so these bounds catch order-of-magnitude fixture
    // drift ONLY -- never arm quality. Quality reads are onstation_rate()
    // and PUMPS DOWN.
    std::printf("  [P-H.0] pressure rate %.4f (ceiling-free instrument)\n",
                r.onstation_rate());
}

// ---------------------------------------------------------------------------
// P-H.1 — ★★★ RUNG E12.1, THE RAIDER BACKFILL. Chad's ruling 2026-08-23.
// ---------------------------------------------------------------------------
TEST_CASE("E12.1: the raider backfill keeps a faction's pump offense alive") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- E12.1 A/B skipped, not failed");
        SUCCEED();
        return;
    }

    // ONE DIAL AT A TIME: both arms hold raid_dps_frac at the TAPE's 0.5, so
    // this leg measures the backfill and nothing else (E12.2 moved the DPS
    // afterwards, and a shared-config arm would fold the two together).
    ArmCfg off;  // the pre-E12 static designation
    off.raid_dps_frac = 0.5;
    ArmCfg on;  // the live-roster mask
    on.raider_backfill = true;
    on.raid_dps_frac = 0.5;

    const MatchResult a = fly_match(rep, off);
    const MatchResult b = fly_match(rep, on);
    report("E12.1 OFF (shipped pre-E12)", a);
    report("E12.1 ON  (live-roster mask)", b);
    std::printf("  DELTA on-station %+.1f s   raid duty %+.0f s"
                "   PRESSURE RATE %+.4f\n",
                b.enemy_onstation_s - a.enemy_onstation_s,
                b.enemy_raidduty_s - a.enemy_raidduty_s,
                b.onstation_rate() - a.onstation_rate());

    // THE CLAIM, in the shape the defect had. The enemy's raid duty was going
    // dark for the back half of the match because its two designated pilots
    // were dead; with the mask the faction keeps sending whoever it still has.
    // RAID DUTY is the honest headline -- it is the thing the dial actually
    // changes. On-station follows it but is also bounded by the pump budget
    // (a dead pump credits nothing more), so it is checked as non-regression
    // rather than as a ratio.
    // ★ E16: PER PUMP-ALIVE SECOND (see the E12 regression leg for the full
    // reason): a raid order exists only while its target does, so an arm that
    // succeeds harder banks FEWER raid seconds. The rate is the claim.
    REQUIRE(a.player_pump_alive_s > 0.0);
    REQUIRE(b.player_pump_alive_s > 0.0);
    CHECK(b.enemy_raidduty_s / b.player_pump_alive_s >
          a.enemy_raidduty_s / a.player_pump_alive_s * 1.15);
    // ★ A2: the SECONDS clause below is CREDIT-TRUNCATED at this site, not
    // vacuous -- E12.1 runs at dps 0.5 (ceiling 60.0 s) and neither arm kills
    // both pumps, so both numbers sit under the wall. (In an arm where BOTH
    // pumps fall the same clause degenerates to an identity,
    // 2*pump_kill_seconds/raid_dps_frac on each side, which is why it must
    // never be the headline.) Kept for continuity; the RATE beneath is the
    // clause that cannot degenerate, because the ceiling's variance flows
    // through the time-to-kill denominator -- an arm that kills faster reads
    // HIGHER, never equal.
    CHECK(b.enemy_onstation_s >= a.enemy_onstation_s - 1e-6);
    CHECK(b.onstation_rate() >= a.onstation_rate() - 1e-9);
}

// ---------------------------------------------------------------------------
// P-H.2 — THE DIAL SWEEP for Chad's "he can actually lose" ruling. HIDDEN
// ([.e12sweep]): it flies a whole 22-minute match per value, so it is a
// measuring instrument the builder runs, not a gate. THE TARGET, stated as a
// number before any value is picked: both of his pumps carry a 30 s
// on-station budget at the shipped table (pump_kill_seconds 15 /
// raid_dps_frac 0.5), so "he can lose" means the enemy earns 60 s of credit
// inside 22 minutes -- and, because credit stops the instant a pump dies, the
// honest read is PUMPS DOWN, not the seconds counter.
// ---------------------------------------------------------------------------
TEST_CASE("E12 sweep: what it costs to make both player pumps reachable",
          "[.e12sweep]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- sweep skipped");
        SUCCEED();
        return;
    }
    const double dps[] = {0.5, 0.65, 0.8, 1.0};
    for (double f : dps) {
        ArmCfg c;
        c.raider_backfill = true;
        c.raid_dps_frac = f;
        const MatchResult r = fly_match(rep, c);
        char label[64];
        std::snprintf(label, sizeof label, "backfill ON, raid_dps_frac %.2f",
                      f);
        report(label, r);
        int down = 0;
        for (int i = 0; i < 4; ++i)
            if (r.pump_dead[i] && i % 2 == 0) ++down;  // pumps 0,2 are his
        std::printf("  >>> PLAYER PUMPS DOWN: %d of 2\n", down);
    }
    // ★ A5 LIVENESS ARM (diagnostic, never ships). raid_dps_frac moves
    // trajectories only THROUGH the pump-death -> bubble feedback, so two arms
    // of this sweep could legitimately fly identical air. Cruise 100 (vs the
    // shipped 85) must move every trajectory; if this line matches any arm
    // above to the digit, the fixture is blind.
    {
        ArmCfg c;
        c.raider_backfill = true;
        c.speed_mps = 100.0;
        report("LIVE cruise 100 m/s (diagnostic, never ships)",
               fly_match(rep, c));
    }
}

// ---------------------------------------------------------------------------
// ★★★ E12 THE GATE — CHAD'S RULING, AS A TEST. "You can actually lose."
//
// It pins the OUTCOME, not the dials, and it pins it AGAINST THE MATCH HE
// SIGNED: the enemy playing the shipped table must do strictly more to his
// pumps than the enemy that flew tape 7 did, and it must put a pump on the
// floor inside a match. Three clauses, because the first two can each be met
// while giving the rung away (raid duty can rise with nobody arriving; a pump
// can fall to a single lucky window while total pressure drops).
//
// ⚠ IF YOU NEED TO BREAK THIS, COME AND SAY SO OUT LOUD -- it quotes him.
// n=1 TRAJECTORY: one tape is one trajectory, not a distribution. The RAID
// DUTY clause is structural and does not depend on it; the PUMP clauses do.
// ---------------------------------------------------------------------------
TEST_CASE("E12: the enemy's pump offense does not regress below tape 7") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- E12 gate skipped, not failed");
        SUCCEED();
        return;
    }
    ArmCfg tape7;
    tape7.raider_backfill = false;
    tape7.raid_dps_frac = 0.5;
    // ★ E16: THE WORLD IS PART OF THE ARM. Tape 7 was flown on the bare-sphere
    // deck with E15's tunnel rope off, so the tape-7 arm pins both. Letting
    // them read config would re-point this comparison at every later world
    // change and quietly stop testing what it names.
    tape7.deck_terrain_relative = 0;
    tape7.transit_reach_s_per_km = 0.0;
    ArmCfg shipped;  // -1 / config-read: whatever [conquest] ships today
    shipped.raider_backfill = kGame.conquest.raid_backfill;

    const MatchResult a = fly_match(rep, tape7);
    const MatchResult b = fly_match(rep, shipped);
    report("tape-7 arm", a);
    report("SHIPPED", b);

    // (1) THE OFFENSE STAYS ON. Raid duty is the thing E12.1 fixed and the
    // one clause that is structural rather than trajectory-bound.
    //
    // ★ ALSO A RATE, AND FOR THE SAME REASON AS (2). A raid order is only
    // issued while the target pump is ALIVE (arm_raid's `sp.alive`), so an arm
    // that takes both pumps at minute six stops accruing raid duty for the
    // remaining sixteen -- SUCCESS SWITCHES THE COUNTER OFF. Measured: the
    // tape-7 arm never kills the surface pump and banks 2358 s; the shipped
    // arm kills both and banks 2028 s while being strictly the better
    // offense. Per pump-alive second the clause says what it always meant --
    // while there was something to raid, how much of the wing was on the job.
    REQUIRE(a.player_pump_alive_s > 0.0);
    REQUIRE(b.player_pump_alive_s > 0.0);
    CHECK(b.enemy_raidduty_s / b.player_pump_alive_s >
          a.enemy_raidduty_s / a.player_pump_alive_s * 1.15);

    // (2) IT REACHES THE PUMPS. Duty that never arrives is not an offense --
    // this is the clause that catches a raid order pointed at a pump the
    // fleet cannot get to.
    //
    // ★ AS A RATE, AND RUNG E16 IS WHY. `enemy_near_s` only accrues while the
    // pump is ALIVE (the credit loop skips a dead one), so an arm that kills
    // both pumps at minute six has FEWER seconds near them than one that never
    // gets there at all -- the counter inverts exactly when the offense starts
    // working. E12 already wrote that warning for on-station seconds ("it is
    // an INPUT, not a score -- read PUMPS DOWN") and this clause was the same
    // trap wearing the other counter's name. Per pump-alive second it cannot
    // be gamed by an early kill, and the bar stays a real one.
    REQUIRE(a.player_pump_alive_s > 0.0);
    REQUIRE(b.player_pump_alive_s > 0.0);
    CHECK(b.enemy_near_s / b.player_pump_alive_s >
          a.enemy_near_s / a.player_pump_alive_s);

    // (3) ★ HE CAN LOSE A PUMP. His ruling, as the only number that states
    // it: at least one player-faction pump on the floor inside the match.
    CHECK((b.pump_dead[0] || b.pump_dead[2]));
    CHECK(b.player_pumps_lost_s > 0.0);
}

// The knob-off differential the whole ladder ships with.
TEST_CASE("E12.1: backfill off is the pre-E12 designation, bit identical") {
    // The mask-free call IS the original function body: same ranking, same
    // strict tie-break, no aliveness term. Pinned directly so a future edit
    // to faction_raider cannot quietly move the off-arm.
    for (int i = 0; i < combat::kNumMavericks; ++i)
        CHECK(combat::faction_raider(i, nullptr) == combat::faction_raider(i));

    // An ALL-ALIVE mask must also reproduce it exactly -- that is what makes
    // the dial safe to leave on while nobody is dead.
    bool all_live[combat::kNumMavericks];
    for (int i = 0; i < combat::kNumMavericks; ++i) all_live[i] = true;
    for (int i = 0; i < combat::kNumMavericks; ++i)
        CHECK(combat::faction_raider(i, all_live) ==
              combat::faction_raider(i));
}

// The designation's own contract, away from the match fixture.
//
// ★ WRITTEN CONFIG-RELATIVE against combat::raiders_for_team. The first cut of
// this test hard-coded `REQUIRE(raiders.size() == 2)` -- and RUNG E13's uneven
// 7/3 wings broke it the same day it was written. That is the ladder's own
// recurring trap (a constant that DESCRIBES the shipped table stops describing
// it the moment the table moves) reproduced in the very test written to catch
// the same shape of bug. The BEHAVIOUR under test never depended on the number
// 2: it is "a faction keeps fielding its full raid complement as pilots die,
// and never zero."
TEST_CASE("E12.1: a dead raider hands its duty to the next pilot down") {
    bool live[combat::kNumMavericks];
    for (int i = 0; i < combat::kNumMavericks; ++i) live[i] = true;

    const auto count_raiders = [&](int f, const bool* mask) {
        int n = 0;
        for (int i = 0; i < combat::kNumMavericks; ++i)
            if (combat::maverick_faction(i) == f &&
                combat::faction_raider(i, mask))
                ++n;
        return n;
    };

    for (int f = 0; f < 2; ++f) {
        const int want = combat::raiders_for_team(f);
        std::vector<int> raiders;
        std::vector<int> others;
        for (int i = 0; i < combat::kNumMavericks; ++i) {
            if (combat::maverick_faction(i) != f) continue;
            (combat::faction_raider(i) ? raiders : others).push_back(i);
        }
        INFO("faction " << f << " wing " << combat::team_size(f) << " wants "
                        << want << " raiders");
        REQUIRE(static_cast<int>(raiders.size()) == want);

        // Kill the WHOLE standing raid complement one at a time. After each
        // death the faction must still field `want` raiders (backfilled from
        // the next pilot down) for as long as it has the pilots -- and the
        // dead ones must never be among them. Under the pre-E12 designation
        // the count would fall to zero, which is the twelve minutes of enemy
        // sky with no pump order in it.
        for (std::size_t d = 0; d < raiders.size(); ++d) {
            live[raiders[d]] = false;
            const int alive_in_wing = combat::team_size(f) -
                                      static_cast<int>(d) - 1;
            CHECK(count_raiders(f, live) == std::min(want, alive_in_wing));
            CHECK(!combat::faction_raider(raiders[d], live));
        }

        // Down to ONE live pilot in the wing: he raids alone, never nobody.
        for (int i : others) live[i] = false;
        REQUIRE(!others.empty());
        live[others[0]] = true;
        CHECK(count_raiders(f, live) == 1);
        CHECK(combat::faction_raider(others[0], live));

        // And a wing with NOBODY left raids with nobody -- no phantom order.
        live[others[0]] = false;
        CHECK(count_raiders(f, live) == 0);

        for (int i = 0; i < combat::kNumMavericks; ++i) live[i] = true;
    }
}

// ---------------------------------------------------------------------------
// P-H.3 — RUNG E15's TRADE CURVE. HIDDEN ([.e15sweep]): one whole match per
// value. THE QUESTION IT ANSWERS: E15 buys tunnel arrivals with TIME SPENT IN
// TRANSIT, and transit is leash-EXEMPT -- a striker crossing to the mouth is
// outside its own dome, in thin air, where lift lapses and is not floored.
// Tape 8's forensics put 14 of 22 AI crashes outside the dome. So more rope
// for the tunnel is, mechanically, more time in the place they die.
// READ THE CRASH COLUMN AGAINST CHAD'S SIGNED 0.94/min.
// ---------------------------------------------------------------------------
TEST_CASE("E15 sweep: tunnel arrivals bought with crashes", "[.e15sweep]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- sweep skipped");
        SUCCEED();
        return;
    }
    for (double per_km : {0.0, 3.0, 6.0, 11.0}) {
        ArmCfg c;
        c.raider_backfill = kGame.conquest.raid_backfill;
        c.transit_reach_s_per_km = per_km;
        const MatchResult r = fly_match(rep, c);
        char label[80];
        std::snprintf(label, sizeof label, "transit_reach_s_per_km %.1f",
                      per_km);
        report(label, r);
        std::printf("  >>> crashes/min %.2f   (Chad signed 0.94)\n",
                    r.enemy_crashes / r.minutes);
    }
    // ★ A5 LIVENESS ARM (diagnostic, never ships): cruise 100 m/s must move
    // every trajectory; bit-identity with any arm above = blind fixture.
    {
        ArmCfg c;
        c.raider_backfill = kGame.conquest.raid_backfill;
        c.speed_mps = 100.0;
        report("LIVE cruise 100 m/s (diagnostic, never ships)",
               fly_match(rep, c));
    }
}

// ---------------------------------------------------------------------------
// P-H.5 — ★★★ RUNG E16's CANDIDATE SWEEP. HIDDEN ([.e16sweep]): one whole
// 22-minute match per arm. THE HANDOFF'S STANDING INSTRUCTION: the five crash
// candidates were measured NULL on probe P-A, which never destroys a pump and
// so never shrinks a bubble -- the fixture was structurally blind. P-H is not.
// Re-measure here before building anything new.
// ---------------------------------------------------------------------------
TEST_CASE("E16 sweep: the vacuum crash candidates, on a fixture that can see"
          " them", "[.e16sweep]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- sweep skipped");
        SUCCEED();
        return;
    }
    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    // THE PRE-E16 WORLD, stated explicitly rather than inherited: the
    // bare-sphere deck and E15's rope off, which is what shipped before this
    // rung. Reading config here would silently re-point the baseline at every
    // later retune.
    ArmCfg base;
    base.raider_backfill = kGame.conquest.raid_backfill;
    base.deck_terrain_relative = 0;
    base.transit_reach_s_per_km = 0.0;

    ArmCfg seek_off = base;
    seek_off.avoid_air_dive_gamma = 0.0;
    ArmCfg fade = base;
    fade.avoid_air_dive_agl_m = 800.0;
    // ★ THE DECK'S FRAME -- the arm that actually moved. D2 adds the E15
    // tunnel rope this rung was blocking, so the two questions are answered
    // together.
    ArmCfg d1 = base;
    d1.deck_terrain_relative = 1;
    ArmCfg d2 = d1;
    d2.transit_reach_s_per_km = 11.0;
    ArmCfg e15 = base;
    e15.transit_reach_s_per_km = 11.0;
    // ★ A5 LIVENESS ARM: cruise 100 m/s off the PRE-E16 base -- must move
    // every trajectory. Compare against A0, which is the same base. A
    // bit-identical L-vs-A0 pair means the fixture or the plumbing is dead.
    // Diagnostic, never ships.
    ArmCfg live = base;
    live.speed_mps = 100.0;

    const Arm arms[] = {
        {"A0 PRE-E16 (sphere deck, no tunnel rope)", base},
        {"A1 air-seek OFF (dive_gamma 0)", seek_off},
        {"A2 E11 runway fade (dive_agl 800)", fade},
        {"A3 E15 rope 11.0 on the SPHERE deck (the blocked rung)", e15},
        {"D1 deck TERRAIN-RELATIVE, AI untouched", d1},
        {"D2 terrain deck + E15 rope 11.0", d2},
        {"L  LIVENESS cruise 100 m/s vs A0 (never ships)", live},
    };
    for (const Arm& a : arms) {
        const MatchResult r = fly_match(rep, a.cfg);
        report(a.label, r);
    }
}

// ---------------------------------------------------------------------------
// P-H.6 — ★★★ RUNG E17's SWEEP. HIDDEN ([.e17sweep]): one whole 22-minute
// match per arm, ~45-60 s each. Chad's tape 10 gave this rung two rulings:
//
//   (1) "they need to try to keep killing the pump, not shoot it and fly away"
//   (2) "they died in the tunnel run"  [3 of his 4 AI runs ended in a wreck]
//
// The two mechanisms turned out to be THE SAME SHAPE — both channels of a
// pursuit law pinned at their caps at once, and an aeroplane that then flies
// the opposite of what it is commanded (see drone/drone.h raid_attack_alt_m
// and drone/maverick.h run_recover_alt_m for the two measurements). So they
// are swept together, and separately, so a regression can be attributed.
//
// ★ THE BASELINE IS THE SHIPPED WORLD (terrain deck + E15's rope), because
// that is what he flew tape 10 on. Only the E17 dials move.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ★★★ THE 412 m FLOOR DIFFERENTIAL — WHO HOLDS THE RAIDER UP?
//
// The repair handoff SS3 names the one genuine causal disagreement in the
// six-consult audit:
//   C4  the terrain-avoid look-ahead latch arms at ~487 m in a dive and holds
//       the raider there (avoid_agl_enter_m 250 + 3.0 s of look-ahead against
//       a 156 m/s, -29.8 deg sink), and it MUTES THE GUNS (drone/drone.h:2546).
//   C1  the saturated aim_at equilibrium simply SETTLES ~30 m above
//       avoid_agl_release_m (400.0) and the latch never fires at all.
// They imply different fixes. This sweep settles it by measurement, on the
// real-DEM fixture (A1), and it asserts almost nothing on purpose: it is an
// instrument, not a gate.
//
// SIX ARMS, ONE REPLAY:
//   A   SHIPPED (raider_backfill from game.toml -- ArmCfg's bool has no
//       "-1 = shipped" sentinel, so it MUST be set explicitly or the arm flies
//       the pre-E12 static designation, a machine Chad never flew)
//   B   latch structurally OFF        (isolates C4)
//   C   raid gamma_cap 0.52 -> 1.13   (isolates C1: unpin the elevation channel)
//   D   both                          (the 2x2's interaction cell)
//   L   raid gamma_cap 0.30 (NARROW)  -- LIVENESS. Never ships. If the raid
//       command is pinned at all, halving the cap MUST move the flown gamma;
//       a bit-identical L means the gamma_cap override is not plumbed and no
//       other arm in this sweep means anything.
//   A'  cruise speed + 1e-9 m/s       -- the PERTURBATION REPLICATE. Repeat-run
//       noise in this sim is exactly 0; the floor that bites is sensitivity to
//       an IRRELEVANT perturbation ([.e18dive] flips on 9 m). dp.speed seeds
//       every spawn velocity and the cruise governor reads it every tick, so
//       A' is GUARANTEED to break bit-identity while being physically nil --
//       which is exactly what makes its median shift the noise floor of the
//       median statistic. ⚠ raid_dps_frac was rejected for this job: it
//       reaches trajectories ONLY through damage_pump's per-tick credit
//       (:699/:933), so a 1e-9 nudge is bit-inert and the noise gate would
//       pass vacuously.
//
// HOW TO READ IT, in order:
//   0. CALIBRATION. Arm A's raid-AGL median must land near the tape's 412 m
//      and n must be large. If it does not, P-H does not host the phenomenon
//      and NO arm is believed -- the measurement moves to a real tape.
//   1. avoid_raid_ticks(A) is THE DECISIVE RAID-SIDE READING. Zero means the
//      latch never fired on raid duty => C1 for the raid question, whatever
//      the hashes say. (The hash is CORROBORATION only: dp is fleet-wide, so
//      arm B unlatches the FRIENDLY fleet too, and a friendly latch alone will
//      break hash equality without the enemy raid latch ever arming.)
//   2. ATTRIBUTION. C4 iff avoid_raid_ticks(A) is a real fraction of
//      raid_ticks AND median(B) collapses while median(C) holds. C1 iff
//      median(C) moves while median(B) holds. If both move, D quantifies the
//      split.
//   3. SANITY. hash(L) != hash(A) or the harness is dead. |med(A')-med(A)| is
//      the ruling threshold's floor -- do not rule on a median delta smaller
//      than it.
// ---------------------------------------------------------------------------
TEST_CASE("FLOOR sweep: who makes the 412 m raid floor", "[.floor2x2]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- FLOOR sweep skipped, not failed");
        SUCCEED();
        return;
    }
    ArmCfg a;
    a.raider_backfill = kGame.conquest.raid_backfill;
    ArmCfg b = a;
    b.avoid_latch_off = 1;
    ArmCfg c = a;
    c.raid_gamma_cap = 1.13;
    ArmCfg d = a;
    d.avoid_latch_off = 1;
    d.raid_gamma_cap = 1.13;
    ArmCfg l = a;
    l.raid_gamma_cap = 0.30;
    ArmCfg ap = a;
    ap.speed_mps = kScen.drone.speed + 1e-9;

    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    const Arm arms[] = {
        {"A  SHIPPED", a},
        {"B  latch OFF", b},
        {"C  raid gamma_cap 1.13", c},
        {"D  latch OFF + cap 1.13", d},
        {"L  LIVENESS cap 0.30 (never ships)", l},
        {"A' PERTURBATION cruise +1e-9 m/s", ap},
    };
    const std::size_t n_arms = sizeof arms / sizeof arms[0];
    std::vector<double> med(n_arms, -1.0);
    std::vector<unsigned long long> he(n_arms, 0ull);
    std::printf("\n[FLOOR 2x2] shipped raid gamma_cap %.2f rad, k_el %.1f;"
                " avoid enter %.0f / release %.0f m, lookahead %.1f s\n",
                drone::RaidOrder{}.gamma_cap, drone::RaidOrder{}.k_el,
                kScen.drone.avoid_agl_enter_m, kScen.drone.avoid_agl_release_m,
                kScen.drone.avoid_lookahead_s);
    for (std::size_t i = 0; i < n_arms; ++i) {
        MatchResult r = fly_match(rep, arms[i].cfg);
        std::sort(r.raid_agl.begin(), r.raid_agl.end());
        const auto q = [&](double f) {
            return r.raid_agl.empty()
                       ? -1.0
                       : r.raid_agl[static_cast<std::size_t>(
                             f * static_cast<double>(r.raid_agl.size() - 1))];
        };
        med[i] = q(0.50);
        he[i] = r.hash_enemy;
        std::printf(
            "\n[FLOOR %s]\n"
            "  latch ticks  enemy %lld  (of which RAID DUTY %lld)"
            "   friendly %lld\n"
            "  raid ticks %lld   saturated (raw |k_el*elev| >= cap) %lld"
            "   max raw |g|/cap %.2f\n"
            "  raid AGL within 2 km of pump: p10 %.0f  MED %.0f  p90 %.0f m"
            "   (n=%zu)\n"
            "  hash enemy %016llx  friendly %016llx\n"
            "  crashes %d   pressure %.4f   player pump lost %s\n",
            arms[i].label, r.avoid_ticks_enemy, r.avoid_raid_ticks,
            r.avoid_ticks_friendly, r.raid_ticks, r.sat_raid_ticks,
            r.max_gamma_raw_frac, q(0.10), med[i], q(0.90), r.raid_agl.size(),
            r.hash_enemy, r.hash_friendly, r.enemy_crashes, r.onstation_rate(),
            r.player_pumps_lost_s < 0.0 ? "no" : "YES");
    }
    std::printf(
        "\n[FLOOR VERDICT INPUTS]\n"
        "  median  A %.0f   B %.0f (%+.0f)   C %.0f (%+.0f)   D %.0f (%+.0f)"
        "   L %.0f (%+.0f)\n"
        "  NOISE FLOOR |med(A')-med(A)| = %.0f m  -- rule on nothing smaller\n"
        "  hash B==A %s   C==A %s   L==A %s   A'==A %s\n",
        med[0], med[1], med[1] - med[0], med[2], med[2] - med[0], med[3],
        med[3] - med[0], med[4], med[4] - med[0], std::abs(med[5] - med[0]),
        he[1] == he[0] ? "YES" : "no", he[2] == he[0] ? "YES" : "no",
        he[4] == he[0] ? "YES" : "no", he[5] == he[0] ? "YES" : "no");
    // The only hard claims: the harness is alive. Everything else is a
    // measurement to be read, not a threshold to be passed.
    CHECK(he[1] != he[0]);  // B: the latch override is plumbed
    CHECK(he[4] != he[0]);  // L: the gamma_cap override is plumbed
    CHECK(he[5] != he[0]);  // A': the perturbation replicate really perturbs
}

// ---------------------------------------------------------------------------
// ★★★ RUNG S1-DECK — THE CROSS-CONFIG TRIPWIRE. This is the defect, refused
// structurally and forever.
//
// The terrain-avoid pull-up RELEASES at an AGL. The only breathable air
// outside the domes is the global deck, FULL below game.toml deck_agl_m. If
// the release altitude is ABOVE that lid, then every altitude the pull-up
// lets go at is DEAD AIR and deck flight is a forced limit cycle -- which is
// precisely what the shipped 400 m release against a 120 m lid produced, and
// what Chad reported "over and over again to no avail".
//
// ---------------------------------------------------------------------------
// ★★★ THE S4 GATE LEG (no hidden tag — it RUNS in the default gate; the
// last handoff called this "[.s4collapse]" before it existed, and it is a gate
// leg rather than a hidden probe on purpose). Chad, 2026-08-26: "this games ai
// must not teleport but become skilled at deck flying" / "the dome being gone
// means they have to ride the deck they should have to respawn, but if they
// crash due to the moment of air loss and their context of orientation and
// speed, well then they respawn from their zone at the deck".
//
// It flies ONE shipped 22-minute match on the real DEM and asks the three
// questions the deletion has to answer. It is a GATE leg (not hidden) because
// the property it pins -- nobody is ever dumped into vacuum, and the deck path
// is actually reached -- is the property the deletion trades on.
//
// ⚠ WHAT THIS FIXTURE CANNOT SEE, STATED. In the shipped arm ALL FOUR pumps
// are dead by 782 s and the enemy's own dome dies at that same tick, so after
// the collapse there is no pump left to raid and no deep pump to strike.
// "Do they still reach a pump / take the tunnel AFTER their dome dies" is
// therefore STRUCTURALLY UNMEASURABLE on this tape: post_near_s and
// post_net_entries read 0 for want of a target, not for want of ability. The
// report prints them anyway so nobody quotes a zero as a capability. E12's
// tape-7 arm is the only arm in the tree where the enemy dome dies with the
// opponent's pumps still alive, and it is a deliberately crippled world
// (deck_terrain_relative off), so it grades nothing here either.
// ⚠ NO COMMA IN THE NAME -- Catch2 splits a filter argument on commas and a
// comma'd name cannot be selected with `-R`/a quoted filter (it silently runs
// nothing, which reads exactly like a green test).
TEST_CASE("S4 collapse: the enemy rides the deck and is never teleported into "
          "vacuum or into the other faction's air") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- S4 probe skipped, not failed");
        SUCCEED();
        return;
    }
    ArmCfg shipped;
    shipped.raider_backfill = kGame.conquest.raid_backfill;
    const MatchResult r = fly_match(rep, shipped);
    report("S4 SHIPPED", r);

    // (0) THE FIXTURE CAN SEE A COLLAPSE AT ALL. Without this the three
    // clauses below are vacuously true -- the blind-fixture law.
    REQUIRE(r.enemy_f_idx >= 0);
    REQUIRE(r.collapse_s[r.enemy_f_idx] >= 0.0);

    // (1) THE DECK RESPAWN IS A LIVE BRANCH, NOT DEAD CODE. If this reads 0
    // the whole S4 placement law was never executed and everything else here
    // has measured nothing.
    CHECK(r.deck_respawns > 0);

    // (2) NOBODY IS EVER LEFT IN THE VACUUM PEN. Before S4, a crash respawn
    // with both domes gone fell through to the stock +X scatter at spawn_alt
    // in thin air -- FIX-F2's own defect, re-entered through the endgame door.
    // The deck holds air everywhere, so this must be exactly zero.
    CHECK(r.stock_respawns == 0);

    // (3) THEY RIDE THE DECK. After its dome dies the wing keeps flying, and
    // it flies the deck rather than porpoising in vacuum. The clause is a
    // MAJORITY, not a fraction fitted to the measured 100% -- a threshold
    // fitted to today's number grades nothing tomorrow.
    REQUIRE(r.post_fly_s > 0.0);
    CHECK(r.post_deck_s > 0.5 * r.post_fly_s);
}

// load_scenario.cpp cannot hold this check: it never sees game.toml. So it
// lives here, where both tables are loaded, and it is a GATE leg, not a hidden
// probe.
TEST_CASE("S1-DECK: the deck release altitude sits inside the full-air lane") {
    INFO("scenario deck_avoid_agl_release_m = "
         << kScen.drone.deck_avoid_agl_release_m
         << " ; game deck_agl_m = " << kGame.atmosphere.deck_agl_m);
    if (kScen.drone.deck_avoid_agl_enter_m <= 0.0) {
        WARN("the deck band ships OFF (deck_avoid_agl_enter_m = 0) -- the "
             "walk-back is engaged, so there is no band to check");
        SUCCEED();
        return;
    }
    // THE INVARIANT: the forced climb must be able to COMPLETE in full air.
    CHECK(kScen.drone.deck_avoid_agl_release_m <= kGame.atmosphere.deck_agl_m);
    // And the hold altitude the track law flies must be in the same lane.
    CHECK(kScen.drone.deck_track_agl_m <= kGame.atmosphere.deck_agl_m);
    // Non-vacuous: the SHIPPED in-bubble band really is outside the lane --
    // that is the defect this leg exists because of, stated as a measurement.
    CHECK(kScen.drone.avoid_agl_release_m > kGame.atmosphere.deck_agl_m);
}

// ★★★ RUNG D3 — THE PULL-OUT BOUND ON THE COMMITTED DIVE. Same reason it
// lives here and not in load_scenario.cpp: it needs game.toml's deck_soft_m,
// and the scenario loader never sees that table. A GATE leg, not a hidden
// probe. ⚠ EVERY TERM IS DERIVED FROM A SHIPPED DIAL -- nothing here is a
// literal that describes another table (the law paid for ten times).
TEST_CASE("D3: the committed dive can still pull out inside the air fade") {
    if (kScen.drone.raid_ballistic_climb_agl_m <= 0.0) {
        WARN("the ballistic deck run ships OFF (raid_ballistic_climb_agl_m = "
             "0) -- the dead switch is engaged, so there is no dive to bound");
        SUCCEED();
        return;
    }
    // THE INEQUALITY. Rotating the flight path from -gamma to level at a net
    // perpendicular load n_eff costs dh = v^2 * (1 - cos gamma) / (n_eff * g),
    // and the pull-out only has AIR to fly it in between the top of the fade
    // and the altitude the deck track law is trying to hold.
    //   n_eff  = 0.5 * avoid_pull_net_g -- HALF, as the mean-air proxy across
    //            a fade that runs from ~1.0 down to ~0. avoid_pull_net_g is
    //            itself measured and already under-claims the airframe by
    //            more than 3x ([.bdeck] control arm: pull-up net g p50 4.81,
    //            n=37682), so the compounded bound is deeply conservative.
    //   h_band = (deck_agl_m + deck_soft_m) - deck_track_agl_m: from where
    //            the air starts coming back down to where the track law is
    //            flying to. Both terms are shipped dials in two tables.
    const double v = kScen.drone.raid_ballistic_dive_speed;
    const double g_dive = kScen.drone.raid_ballistic_dive_gamma;
    const double n_eff = 0.5 * kScen.drone.avoid_pull_net_g;
    const double h_band = kGame.atmosphere.deck_agl_m +
                          kGame.atmosphere.deck_soft_m -
                          kScen.drone.deck_track_agl_m;
    const double dh = v * v * (1.0 - std::cos(g_dive)) / (n_eff * 9.81);
    INFO("dive " << (g_dive * 180.0 / 3.14159265358979323846)
                 << " deg at " << v << " m/s: pull-out dh = " << dh
                 << " m against the " << h_band << " m fade band, n_eff "
                 << n_eff);
    CHECK(dh <= h_band);
    // Non-vacuous in BOTH directions: the bound must be a real constraint, so
    // a dive twice as steep must FAIL it. Without this the clause could pass
    // on a degenerate table and prove nothing.
    const double dh2 = v * v * (1.0 - std::cos(2.0 * g_dive)) / (n_eff * 9.81);
    CHECK(dh2 > h_band);
    // And the commanded dive must be steeper than the track law's own cap, or
    // the whole manoeuvre is indistinguishable from ordinary deck tracking.
    CHECK(g_dive > kScen.drone.deck_track_dive_cap);
}

// ---------------------------------------------------------------------------
// ★★★ PROBE [.deckx] — DECK-CROSSING SURVIVAL. Chad, 2026-08-26: "I want them
// to survive the deck, survive the tunnels, attack the pump, attack me."
//
// Three arms on the real Sudbury DEM, the noise floor measured FIRST (the
// [.floor2x2] licence):
//   S  the machine BEFORE this rung (deck_avoid_agl_enter_m = 0, the
//      documented one-key walk-back)
//   D  the deck law as it ships
//   N  D + cruise +1e-9 m/s -- the NOISE FLOOR. Any D-vs-S delta smaller than
//      10x |D-N| is not a result.
//
// PRIMARY STATISTIC: deck crossings survived / attempted, where a crossing is
// a contiguous deck-scope episode (>= 10 s, or any episode that ended in a
// wreck) and it SURVIVED if it ended by leaving deck scope rather than by a
// respawn.
//
// ★ THE GUN-MUTE FRACTION IS A FIRST-CLASS OUTPUT. The latch mutes the guns
// (drone/drone.h). If this law leaves them latched MORE of their deck time,
// that is a capability REDUCTION and it must be visible, not discovered later.
// The fight-only fraction is printed separately because an ENGAGED drone
// chasing the player over the deck is the case Chad cares most about.
//
// ★ THE 412 m RAID FLOOR IS PRINTED PER ARM (red team, 2026-08-26): the pump
// sits in dome air where deck scope is structurally false, so the shipped
// 250/400 band should still own the terminal attack and raid AGL should not
// move. deck-scope RAID ticks are printed too -- the dome-RIM annulus where
// that claim could fail.
TEST_CASE("DECK sweep: do they survive crossing the thin-air deck",
          "[.deckx]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- DECK sweep skipped, not failed");
        SUCCEED();
        return;
    }
    ArmCfg d;
    d.raider_backfill = kGame.conquest.raid_backfill;  // the shipped table
    ArmCfg s = d;
    s.deck_avoid_agl_enter_m = 0.0;  // THE WALK-BACK = the pre-rung machine
    // B: the band + the track law WITHOUT the forward eyes, so the two halves
    // of the fix are attributable to each other.
    ArmCfg b = d;
    b.avoid_pull_net_g = 0.0;
    ArmCfg n = d;
    n.speed_mps = kScen.drone.speed + 1e-9;

    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    const Arm arms[] = {{"S  SHIPPED-BEFORE (deck band OFF)", s},
                        {"B  band + track, forward eyes OFF", b},
                        {"D  DECK LAW (shipped)", d},
                        {"N  NOISE (D + cruise 1e-9)", n}};
    constexpr std::size_t kArms = 4;
    const std::size_t n_arms = kArms;
    double surv[kArms] = {0.0, 0.0, 0.0, 0.0};
    double rate[kArms] = {0.0, 0.0, 0.0, 0.0};
    double med_deck[kArms] = {-1.0, -1.0, -1.0, -1.0};
    double latchf[kArms] = {0.0, 0.0, 0.0, 0.0};
    double cpm[kArms] = {0.0, 0.0, 0.0, 0.0};
    unsigned long long he[kArms] = {0ull, 0ull, 0ull, 0ull};

    std::printf(
        "\n[DECK] shipped band %.0f/%.0f m; deck band %.0f/%.0f m;"
        " full-air lid %.0f m (+%.0f fade); track %.0f m AGL;"
        " lookahead %.1f s; net_g %.2f\n",
        kScen.drone.avoid_agl_enter_m, kScen.drone.avoid_agl_release_m,
        kScen.drone.deck_avoid_agl_enter_m,
        kScen.drone.deck_avoid_agl_release_m, kGame.atmosphere.deck_agl_m,
        kGame.atmosphere.deck_soft_m, kScen.drone.deck_track_agl_m,
        kScen.drone.deck_lookahead_s, kScen.drone.avoid_pull_net_g);

    for (std::size_t i = 0; i < n_arms; ++i) {
        MatchResult r = fly_match(rep, arms[i].cfg);
        std::sort(r.deck_agl.begin(), r.deck_agl.end());
        std::sort(r.deck_errand_agl.begin(), r.deck_errand_agl.end());
        std::sort(r.raid_agl.begin(), r.raid_agl.end());
        std::sort(r.pull_g.begin(), r.pull_g.end());
        const auto q = [](const std::vector<double>& v, double f) {
            return v.empty() ? -1.0
                             : v[static_cast<std::size_t>(
                                   f * static_cast<double>(v.size() - 1))];
        };
        surv[i] = r.deck_cross_attempts > 0
                      ? static_cast<double>(r.deck_cross_survived) /
                            r.deck_cross_attempts
                      : -1.0;
        rate[i] = r.onstation_rate();
        med_deck[i] = q(r.deck_agl, 0.50);
        latchf[i] = r.deck_ticks > 0
                        ? static_cast<double>(r.deck_latch_ticks) /
                              static_cast<double>(r.deck_ticks)
                        : 0.0;
        cpm[i] = r.enemy_crashes / std::max(1e-9, r.minutes);
        he[i] = r.hash_enemy;
        const double fightf =
            r.deck_fight_ticks > 0
                ? static_cast<double>(r.deck_fight_latch_ticks) /
                      static_cast<double>(r.deck_fight_ticks)
                : 0.0;
        std::printf(
            "\n[DECK %s]\n"
            "  CROSSINGS survived %d / %d attempted   survival %.3f\n"
            "  deck ticks %lld   deck AGL p10 %.0f  MED %.0f  p90 %.0f m"
            "   (ERRAND ticks only: p10 %.0f  MED %.0f  p90 %.0f, n=%zu)\n"
            "  wrecks IN DECK SCOPE %d of %d\n"
            "  GUN-MUTE on the deck: latched %.1f%% of deck time"
            "   (FIGHT ticks only: %.1f%% of %lld)\n"
            "  enemy crashes %d (%.2f/min, Chad signed 0.94)"
            "   pressure %.4f\n"
            "  raid AGL within 2 km of pump: p10 %.0f  MED %.0f  p90 %.0f"
            "   (n=%zu)   raid ticks in DECK SCOPE %lld of %lld\n"
            "  MEASURED pull-up net g: p10 %.2f  p50 %.2f  p90 %.2f"
            "  max %.2f (n=%zu)\n"
            "  hash enemy %016llx\n",
            arms[i].label, r.deck_cross_survived, r.deck_cross_attempts,
            surv[i], r.deck_ticks, q(r.deck_agl, 0.10), med_deck[i],
            q(r.deck_agl, 0.90), q(r.deck_errand_agl, 0.10),
            q(r.deck_errand_agl, 0.50), q(r.deck_errand_agl, 0.90),
            r.deck_errand_agl.size(), r.deck_crashes, r.enemy_crashes,
            100.0 * latchf[i], 100.0 * fightf,
            r.deck_fight_ticks, r.enemy_crashes, cpm[i], rate[i],
            q(r.raid_agl, 0.10), q(r.raid_agl, 0.50), q(r.raid_agl, 0.90),
            r.raid_agl.size(), r.deck_raid_ticks, r.raid_ticks,
            q(r.pull_g, 0.10), q(r.pull_g, 0.50), q(r.pull_g, 0.90),
            r.pull_g.empty() ? -1.0 : r.pull_g.back(), r.pull_g.size(),
            r.hash_enemy);
    }

    const double noise = std::abs(surv[2] - surv[3]);
    std::printf(
        "\n[DECK VERDICT INPUTS]\n"
        "  survival   S %.3f   B %.3f   D %.3f   (D-S %+.3f)"
        "   NOISE |D-N| %.3f   ratio %.1fx\n"
        "  median deck AGL   S %.0f   B %.0f   D %.0f m\n"
        "  DECK GUN-MUTE fraction   S %.3f   B %.3f   D %.3f"
        "   (LOWER = MORE GUN TIME; this is the nerf watch)\n"
        "  crashes/min   S %.2f   B %.2f   D %.2f\n"
        "  pressure      S %.4f   D %.4f   (%+.4f; his ruling: full "
        "strength, report it, do not walk it back)\n"
        "  hash B==S %s   D==B %s   N==D %s\n",
        surv[0], surv[1], surv[2], surv[2] - surv[0], noise,
        noise > 1e-12 ? std::abs(surv[2] - surv[0]) / noise : 999.0,
        med_deck[0], med_deck[1], med_deck[2], latchf[0], latchf[1], latchf[2],
        cpm[0], cpm[1], cpm[2], rate[0], rate[2], rate[2] - rate[0],
        he[1] == he[0] ? "YES" : "no", he[2] == he[1] ? "YES" : "no",
        he[3] == he[2] ? "YES" : "no");
    // LIVENESS ONLY. Everything above is a measurement to READ, not a
    // threshold to pass -- the same discipline [.floor2x2] carries.
    CHECK(he[1] != he[0]);  // the band + track law is really plumbed
    CHECK(he[2] != he[1]);  // the forward eyes are really plumbed
    CHECK(he[3] != he[2]);  // the perturbation replicate really perturbs
}

// ---------------------------------------------------------------------------
// ★★★ PROBE [.bdeck] — THE BALLISTIC DECK RUN INSTRUMENT.
//
// ★ IT EXISTS BEFORE THE FEATURE DOES. Chad's spec (2026-08-27): "climb to a
// decent altitude and then parabolic dive to the deck to cross the no air zone
// to the enemy bubble to attack". Nothing in this tree flies that yet. This
// probe is the differential that will grade it, landed FIRST, with its noise
// floor measured FIRST, so no mechanism story gets believed before a number
// exists to refute it.
//
// ARMS (a hidden probe -- CHECKs here are LIVENESS ONLY, never thresholds):
//   N  NOISE   the shipped table + cruise 1e-9 m/s. PRINTED FIRST. Any delta
//              smaller than 10x |C-N| on a statistic is not a result.
//   C  CONTROL the shipped table exactly = HEAD. Its enemy hash is PINNED to
//              a literal recorded at b171f338d, so when the ballistic dials
//              land at 0 = OFF the C arm must still reproduce this bit for
//              bit, and a fixture-wide change that moves it goes RED HERE
//              instead of quietly re-baselining a headline (the law paid for
//              ten times).
//   L  LIVENESS a real structural dial moved 2x (deck_track_agl_m 100 -> 200).
//              A bit-identical arm means BLIND FIXTURE or DEAD BRANCH and
//              this arm is what tells them apart. When the ballistic dials
//              land, L becomes raid_ballistic_climb_agl_m at 2x shipped.
//
// NEW FIRST-CLASS STATISTICS, beside the standing signed rows:
//   * ARRIVAL |v| and sink at the moment they descend through the top of the
//     deck's air column (320 m AGL) and through the full-air lid (120 m),
//     measured on the SURFACE deck only (deck scope AND outside the bore --
//     the bore exclusion [.deckx] lacks, which is why its deck-AGL p10 reads
//     -842 m and nobody may quote it).
//   * DEAD-AIR SECONDS PER CROSSING (air below avoid_air_frac_hard 0.25) --
//     the exposure the ballistic run is supposed to shorten.
//   * THE WRECK STRATIFICATION BY AGL -- the design's OWN FALSIFIER. If the
//     wrecks sit down in the 60/110 band rather than up in the dome-exit
//     transition, the ballistic run fixes deaths that no longer dominate and
//     the rung must re-aim before any feature code is written.
TEST_CASE("BDECK: the instrument for the ballistic deck run", "[.bdeck]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- BDECK probe skipped, not failed");
        SUCCEED();
        return;
    }
    // ★ THE HEAD PIN. Measured on b171f338d with the shipped table, 2026-08-27:
    // [.deckx] arm D and this probe's arm C are the same machine.
    // ★★★ RUNG D2 MOVED THIS LITERAL TO 0x9eae92b57d76211b AND THE RESCUE PASS
    // MOVED IT BACK. The re-pin was honest at the time — D2 was authored with
    // its stage-1 return ON, which really did move the shipped machine. But
    // that return ships OFF (it is a measured -6.4% offense bought for nothing
    // until the parabolic dive exists; see config/scenario.toml), and stage 0's
    // two-condition trigger has zero exposure on this replay, so the shipped
    // machine is once again bit-for-bit b171f338d. The claim is not left
    // hanging on this comment: [.regroup] asserts BOTH that its shipped arm
    // reproduces this literal AND that two relaxed arms do not, so "no change"
    // is distinguished from "dead branch" by a gate leg.
    // Do not move this literal to make a red go green.
    // ★★★ RUNG D3 RE-PINNED IT, AND THE RE-PIN IS DECLARED, NOT DISCOVERED.
    // The parabolic dive SHIPS ON, so the shipped machine moves by
    // construction and this literal must move with it. What makes that
    // honest rather than a re-baselining is arm O below: the master dead
    // switch reproduces the OLD literal (kPreD3EnemyHash) bit-for-bit, and
    // that is asserted, so "OFF is exactly the pre-feature machine" is a gate
    // leg rather than a claim. Do not move EITHER literal to make a red go
    // green.
    constexpr unsigned long long kHeadEnemyHash = 0x2ec5baddc8346cfeull;
    constexpr unsigned long long kPreD3EnemyHash = 0x12dd30472363fcf5ull;

    ArmCfg c;
    c.raider_backfill = kGame.conquest.raid_backfill;  // the shipped table
    ArmCfg n_arm = c;
    n_arm.speed_mps = kScen.drone.speed + 1e-9;
    // ★ THE PRE-DECLARED RE-POINT, honoured. This arm read
    // deck_track_agl_m x2 while the ballistic dials did not exist; the probe's
    // own banner said "when the ballistic dials land, L becomes
    // raid_ballistic_climb_agl_m at 2x shipped". They landed, so it does.
    ArmCfg l = c;
    l.raid_ballistic_climb_agl_m = 2.0 * kScen.drone.raid_ballistic_climb_agl_m;
    // ★★★ ARM O — THE WALK-BACK. raid_ballistic_climb_agl_m = 0 is the master
    // dead switch, and it ALSO re-gates the stage-1 return (both in the app
    // and in this file's mirror), so O must be the PRE-D3 machine bit-for-bit.
    // That is what kPreD3EnemyHash below asserts. A stage-1 return flying on
    // its own would be a DIFFERENT machine (-6.4% pressure, measured on
    // [.regroup] arm F), which is precisely why the gate is on BOTH.
    ArmCfg o = c;
    o.raid_ballistic_climb_agl_m = 0.0;

    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    const Arm arms[] = {{"N  NOISE (shipped + cruise 1e-9)", n_arm},
                        {"C  CONTROL (shipped = HEAD)", c},
                        {"L  LIVENESS (raid_ballistic_climb_agl_m x2)", l},
                        {"O  WALK-BACK (climb_agl = 0 = the pre-D3 machine)",
                         o}};
    constexpr std::size_t kArms = 4;

    const auto q = [](const std::vector<double>& v, double f) {
        return v.empty() ? -1.0
                         : v[static_cast<std::size_t>(
                               f * static_cast<double>(v.size() - 1))];
    };
    const auto mean = [](const std::vector<double>& v) {
        if (v.empty()) return -1.0;
        double s = 0.0;
        for (double x : v) s += x;
        return s / static_cast<double>(v.size());
    };

    double surv[kArms] = {0.0, 0.0, 0.0, 0.0};
    double v320[kArms] = {0.0, 0.0, 0.0, 0.0};
    double v120[kArms] = {0.0, 0.0, 0.0, 0.0};
    double dead[kArms] = {0.0, 0.0, 0.0, 0.0};
    double cpm[kArms] = {0.0, 0.0, 0.0, 0.0};
    double rate[kArms] = {0.0, 0.0, 0.0, 0.0};
    unsigned long long he[kArms] = {0ull, 0ull, 0ull, 0ull};
    long long bcommit[kArms] = {0, 0, 0, 0};
    long long bspent[kArms] = {0, 0, 0, 0};

    std::printf(
        "\n[BDECK] deck air: FULL below %.0f m AGL, ZERO by %.0f"
        " (game.toml deck_agl_m/deck_soft_m); dead-air threshold"
        " avoid_air_frac_hard %.2f; in-dome band %.0f/%.0f m;"
        " deck band %.0f/%.0f m; track %.0f m AGL\n"
        "  ★ NOISE ARM PRINTED FIRST. Any statistic whose C-vs-X delta is"
        " under 10x the |C-N| delta is NOT a result.\n",
        kGame.atmosphere.deck_agl_m,
        kGame.atmosphere.deck_agl_m + kGame.atmosphere.deck_soft_m,
        kScen.drone.avoid_air_frac_hard, kScen.drone.avoid_agl_enter_m,
        kScen.drone.avoid_agl_release_m, kScen.drone.deck_avoid_agl_enter_m,
        kScen.drone.deck_avoid_agl_release_m, kScen.drone.deck_track_agl_m);

    for (std::size_t i = 0; i < kArms; ++i) {
        MatchResult r = fly_match(rep, arms[i].cfg);
        std::sort(r.deck_errand_agl.begin(), r.deck_errand_agl.end());
        std::sort(r.raid_agl.begin(), r.raid_agl.end());
        std::sort(r.pull_g.begin(), r.pull_g.end());
        std::sort(r.arr_v_320.begin(), r.arr_v_320.end());
        std::sort(r.arr_sink_320.begin(), r.arr_sink_320.end());
        std::sort(r.arr_gamma_320.begin(), r.arr_gamma_320.end());
        std::sort(r.arr_v_120.begin(), r.arr_v_120.end());
        std::sort(r.arr_sink_120.begin(), r.arr_sink_120.end());
        std::sort(r.cross_dead_s.begin(), r.cross_dead_s.end());
        std::sort(r.wreck_agl.begin(), r.wreck_agl.end());
        std::sort(r.bal_v_320.begin(), r.bal_v_320.end());
        std::sort(r.bal_gamma_320.begin(), r.bal_gamma_320.end());
        std::sort(r.bal_v_120.begin(), r.bal_v_120.end());
        std::sort(r.bal_agl.begin(), r.bal_agl.end());
        std::sort(r.bal_commit_agl.begin(), r.bal_commit_agl.end());
        std::sort(r.bal_apex_agl.begin(), r.bal_apex_agl.end());
        std::sort(r.bal_arrive_v.begin(), r.bal_arrive_v.end());
        bcommit[i] = r.bal_commit;
        bspent[i] = r.bal_spent;

        surv[i] = r.deck_cross_attempts > 0
                      ? static_cast<double>(r.deck_cross_survived) /
                            r.deck_cross_attempts
                      : -1.0;
        v320[i] = q(r.arr_v_320, 0.50);
        v120[i] = q(r.arr_v_120, 0.50);
        dead[i] = mean(r.cross_dead_s);
        cpm[i] = r.enemy_crashes / std::max(1e-9, r.minutes);
        rate[i] = r.onstation_rate();
        he[i] = r.hash_enemy;
        const double latchf =
            r.deck_ticks > 0 ? static_cast<double>(r.deck_latch_ticks) /
                                   static_cast<double>(r.deck_ticks)
                             : 0.0;
        const double fightf =
            r.deck_fight_ticks > 0
                ? static_cast<double>(r.deck_fight_latch_ticks) /
                      static_cast<double>(r.deck_fight_ticks)
                : 0.0;
        std::printf(
            "\n[BDECK %s]\n"
            "  CROSSINGS survived %d / %d attempted   survival %.3f\n"
            "  ARRIVAL through 320 m AGL: |v| p10 %.0f  MED %.0f  p90 %.0f"
            "   sink MED %.0f m/s   gamma MED %.1f deg   (n=%zu)\n"
            "  ARRIVAL through 120 m AGL: |v| p10 %.0f  MED %.0f  p90 %.0f"
            "   sink MED %.0f m/s   (n=%zu)\n"
            "  DEAD AIR per crossing (air < %.2f): mean %.1f s  MED %.1f"
            "  p90 %.1f  max %.1f  (n=%zu)   fleet thin-air %.0f s\n"
            "  deck ERRAND AGL p10 %.0f  MED %.0f  p90 %.0f m (n=%zu)\n"
            "  GUN-MUTE on the deck %.1f%%   (FIGHT ticks only %.1f%%)\n"
            "  enemy crashes %d (%.2f/min, Chad signed 0.94)   pressure %.4f\n"
            "  raid AGL within 2 km of pump: p10 %.0f  MED %.0f  p90 %.0f"
            "   (n=%zu)\n"
            "  pull-up net g p50 %.2f  p90 %.2f (n=%zu)\n"
            "  hash enemy %016llx\n",
            arms[i].label, r.deck_cross_survived, r.deck_cross_attempts,
            surv[i], q(r.arr_v_320, 0.10), v320[i], q(r.arr_v_320, 0.90),
            q(r.arr_sink_320, 0.50), q(r.arr_gamma_320, 0.50),
            r.arr_v_320.size(), q(r.arr_v_120, 0.10), v120[i],
            q(r.arr_v_120, 0.90), q(r.arr_sink_120, 0.50), r.arr_v_120.size(),
            kScen.drone.avoid_air_frac_hard, dead[i], q(r.cross_dead_s, 0.50),
            q(r.cross_dead_s, 0.90),
            r.cross_dead_s.empty() ? -1.0 : r.cross_dead_s.back(),
            r.cross_dead_s.size(), r.enemy_thin_s,
            q(r.deck_errand_agl, 0.10), q(r.deck_errand_agl, 0.50),
            q(r.deck_errand_agl, 0.90), r.deck_errand_agl.size(),
            100.0 * latchf, 100.0 * fightf, r.enemy_crashes, cpm[i], rate[i],
            q(r.raid_agl, 0.10), q(r.raid_agl, 0.50), q(r.raid_agl, 0.90),
            r.raid_agl.size(), q(r.pull_g, 0.50), q(r.pull_g, 0.90),
            r.pull_g.size(), r.hash_enemy);

        // ★★★ THE FALSIFIER. Printed for EVERY arm so the mode mix can be
        // compared, but it is the CONTROL arm's table that decides whether
        // the ballistic run aims at the deaths that actually happen.
        std::printf(
            "  WRECK STRATIFICATION (n=%d enemy wrecks)\n"
            "    by AGL:  <60 m %d | 60-110 %d | 110-250 %d | 250-400 %d"
            " | 400-1000 %d | >1000 %d\n"
            "    AGL p10 %.0f  MED %.0f  p90 %.0f m   sink at death MED"
            " %.0f m/s   speed MEAN %.0f m/s   gamma MEAN %.1f deg\n"
            "    in DECK SCOPE %d   of which UNDERGROUND (in the bore) %d"
            "   SURFACE deck %d   in thin air %d   steep %d   fast %d\n"
            "    by disposition: defend %d  raid %d  tunnel %d  engaged %d"
            "  patrol %d\n",
            r.enemy_crashes, r.wreck_agl_bin[0], r.wreck_agl_bin[1],
            r.wreck_agl_bin[2], r.wreck_agl_bin[3], r.wreck_agl_bin[4],
            r.wreck_agl_bin[5], q(r.wreck_agl, 0.10), q(r.wreck_agl, 0.50),
            q(r.wreck_agl, 0.90), q(r.wreck_sink, 0.50),
            r.crash_speed_sum / std::max(1, r.enemy_crashes),
            r.crash_gamma_sum / std::max(1, r.enemy_crashes),
            r.wreck_deck_scope, r.wreck_in_net, r.wreck_deck_surface,
            r.crash_thin, r.crash_steep, r.crash_fast, r.crash_by[0],
            r.crash_by[1], r.crash_by[2], r.crash_by[3], r.crash_by[4]);

        // ★★★ RUNG D3 — THE BALLISTIC DECK RUN'S OWN TABLE.
        // ★ THE DENOMINATOR IS PRINTED FIRST, deliberately. "Too few
        // episodes" is only a WIRING verdict if you know how many chances
        // there were; without the eligible-tick count a rarity result and a
        // dead branch read identically (the D2 shadow lesson, where Chad's
        // literal trigger armed ZERO because of EXPOSURE, not code).
        std::printf(
            "  ★ D3 BALLISTIC RUN  eligible ticks %lld (%.0f s of rank-0"
            " non-tunnel raid order)\n"
            "    EDGES  arm %lld -> commit %lld (of which by TIMEOUT %lld)"
            " -> run %lld -> SPENT %lld   |   lost mid-phase %lld\n"
            "    ticks per phase: NONE %lld RETURN %lld CLIMB %lld DIVE %lld"
            " RUN %lld SPENT %lld\n"
            "    climb apex AGL MED %.0f m   commit AGL MED %.0f m"
            "   (dial %.0f)\n"
            "    ARRIVAL ON A LIVE PHASE: through 320 m |v| MED %.0f"
            " (n=%zu) gamma MED %.1f deg ; through 120 m |v| MED %.0f"
            " (n=%zu)\n"
            "    FLEET, SAME SEAM, ALL PHASES: 320 m |v| MED %.0f ;"
            " 120 m |v| MED %.0f\n"
            "    RUN/SPENT deck AGL p10 %.0f MED %.0f p90 %.0f m (n=%zu)"
            "   [fleet errand MED %.0f]   ← the POROPOISE watch\n"
            "    gun-mute during a live phase %.1f%% of %lld ticks"
            "   [fleet deck %.1f%%]   ← arriving disarmed is a nerf\n"
            "    arrival |v| at the RUN->SPENT handover MED %.0f m/s"
            " (n=%zu)\n",
            r.bal_eligible_ticks,
            static_cast<double>(r.bal_eligible_ticks) * kAp.sim_dt,
            r.bal_arm, r.bal_commit, r.bal_commit_timeout, r.bal_run,
            r.bal_spent, r.bal_lost, r.bal_ticks[0], r.bal_ticks[1],
            r.bal_ticks[2], r.bal_ticks[3], r.bal_ticks[4], r.bal_ticks[5],
            q(r.bal_apex_agl, 0.50), q(r.bal_commit_agl, 0.50),
            arms[i].cfg.raid_ballistic_climb_agl_m >= 0.0
                ? arms[i].cfg.raid_ballistic_climb_agl_m
                : kScen.drone.raid_ballistic_climb_agl_m,
            q(r.bal_v_320, 0.50), r.bal_v_320.size(),
            q(r.bal_gamma_320, 0.50), q(r.bal_v_120, 0.50),
            r.bal_v_120.size(), v320[i], v120[i], q(r.bal_agl, 0.10),
            q(r.bal_agl, 0.50), q(r.bal_agl, 0.90), r.bal_agl.size(),
            q(r.deck_errand_agl, 0.50),
            r.bal_phase_ticks > 0
                ? 100.0 * static_cast<double>(r.bal_mute_ticks) /
                      static_cast<double>(r.bal_phase_ticks)
                : -1.0,
            r.bal_phase_ticks, 100.0 * latchf, q(r.bal_arrive_v, 0.50),
            r.bal_arrive_v.size());
    }

    const double nz_surv = std::abs(surv[1] - surv[0]);
    const double nz_v320 = std::abs(v320[1] - v320[0]);
    const double nz_dead = std::abs(dead[1] - dead[0]);
    std::printf(
        "\n[BDECK NOISE FLOOR — measured, not assumed]\n"
        "  survival        C %.3f  N %.3f   |C-N| %.4f\n"
        "  arrival |v| 320 C %.1f  N %.1f   |C-N| %.4f\n"
        "  dead-air/cross  C %.2f  N %.2f   |C-N| %.4f\n"
        "  crashes/min     C %.2f  N %.2f\n"
        "  pressure        C %.4f  N %.4f\n"
        "[BDECK HASH]\n"
        "  C %016llx   HEAD PIN %016llx   %s\n"
        "  N %016llx   (must DIFFER from C -- the perturbation is live)\n"
        "  L %016llx   (must DIFFER from C -- the fixture is not blind)\n"
        "  O %016llx   PRE-D3 PIN %016llx   %s   (climb_agl=0 IS the"
        " pre-feature machine)\n"
        "[BDECK D3 LIVENESS]  commits C %lld  L %lld  O %lld"
        "   complete episodes C %lld  L %lld  O %lld\n",
        surv[1], surv[0], nz_surv, v320[1], v320[0], nz_v320, dead[1], dead[0],
        nz_dead, cpm[1], cpm[0], rate[1], rate[0], he[1], kHeadEnemyHash,
        he[1] == kHeadEnemyHash ? "MATCH" : "MOVED",
        he[0], he[2], he[3], kPreD3EnemyHash,
        he[3] == kPreD3EnemyHash ? "MATCH" : "MOVED",
        bcommit[1], bcommit[2], bcommit[3], bspent[1], bspent[2], bspent[3]);

    // LIVENESS ONLY. Everything above is a measurement to READ.
    CHECK(he[1] == kHeadEnemyHash);  // the control really is HEAD
    CHECK(he[0] != he[1]);           // the 1e-9 perturbation really perturbs
    CHECK(he[2] != he[1]);           // a real dial really moves the machine
    // ★★★ D3 — THE WALK-BACK IS THE PRE-FEATURE MACHINE, BIT FOR BIT. This is
    // the whole licence for shipping ON: OFF is not "approximately before".
    CHECK(he[3] == kPreD3EnemyHash);
    // ... and it must DIFFER from the shipped arm, or the feature is dead.
    CHECK(he[3] != he[1]);
    // ★★★ THE FEATURE FIRES ON THE REAL MATCH. The bar is stated against the
    // measured denominator printed above, not against a wish: a COMMIT is the
    // CLIMB->DIVE edge, the manoeuvre Chad asked to watch.
    CHECK(bcommit[1] > 0);
    // ... and the OFF arm must fire NONE, or the dead switch is not dead.
    CHECK(bcommit[3] == 0);
    // NON-VACUOUS: the new statistics must actually have samples, or every
    // number above is a blind fixture reporting -1.
    CHECK(surv[1] >= 0.0);
}

// ★★★ PROBE [.regroup] — RUNG D2, THE REPOSITION AND THE RETURN.
//
// Chad, 2026-08-27: "if they are near the outer one third near the boundary
// and their own pump is getting attacked maybe they go down and move closer to
// the inside of their bubble" / "they should go back to whatever bubble if
// they have one left".
//
// ★ HONEST GRADING, decided BEFORE the run (the P-A delta law + the red
// team's correction 3). The literal two-condition trigger is RARE. An
// occupancy delta that small can sit inside the ensemble noise, so THIS PROBE
// ASSERTS NOTHING ABOUT OCCUPANCY. It REPORTS occupancy with the noise floor
// beside it and pre-declares "fires too rarely to move the match" as a
// legitimate outcome that routes to Chad's ruling, not as a red gate and not
// as a reason to quietly flip the fallback on. The CHECKs here are liveness
// and bit-identity only.
//
// ★★★ WHAT IT FOUND, and why the arm list below has the shape it has.
// Arm S arms ZERO episodes and is bit-identical to arm O. Bit-identity is the
// exact shape that means "blind fixture OR dead branch", so the probe does not
// get to stop there — the three-level split in the per-arm block runs the
// conjunction down to the tick:
//     window open 0.98%  ->  AND in band 0.46% (1123 ticks)  ->  AND eligible 0
//     ... all 1123 shadowed by ENGAGED, none by DEFEND.
// So the trigger is not rare here, it is SHADOWED: every tick Chad's two
// conditions hold, the pilot is in a fight, and his other ruling (pursuit
// unlimited, "chase you anywhere") outranks the reposition by construction.
// Arms D and F then prove the machinery flies when a conjunct is relaxed —
// that is what turns "bit-identical" from a silent pass into a measured fact.
//
// ARMS:
//   N  NOISE     shipped + cruise 1e-9 m/s. PRINTED FIRST.
//   O  OFF       regroup_frac_arm = 0 — the documented WHOLE-FEATURE
//                WALK-BACK. Its enemy hash is PINNED to the pre-D2 literal
//                measured at b171f338d, so "the walk-back is exactly the old
//                machine" is a gate leg.
//   S  SHIPPED   the shipped table = Chad's literal AND trigger, ON.
//   D  DWELL     regroup_require_pump_attack = false — the named fallback,
//                arm on outer-third dwell alone. Measured, not shipped; both
//                arms' numbers go to Chad with the one question.
TEST_CASE("REGROUP: the reposition and the return", "[.regroup]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- REGROUP probe skipped, not failed");
        SUCCEED();
        return;
    }
    // ★ THE PRE-D2 PIN, measured at b171f338d ([.bdeck] arm C, [.deckx] arm D).
    constexpr unsigned long long kPreD2EnemyHash = 0x12dd30472363fcf5ull;
    // ★ RUNG D3's shipped machine. Same literal as [.bdeck]'s kHeadEnemyHash
    // by construction (both arms are the shipped table on the same replay);
    // if these two ever disagree, one of the two mirrors has drifted from the
    // app and that is exactly what this duplication is for.
    constexpr unsigned long long kShippedD3EnemyHash = 0x2ec5baddc8346cfeull;

    ArmCfg s;
    s.raider_backfill = kGame.conquest.raid_backfill;  // the shipped table
    ArmCfg n_arm = s;
    n_arm.speed_mps = kScen.drone.speed + 1e-9;
    ArmCfg o = s;
    o.regroup_frac_arm = 0.0;  // the whole-feature walk-back
    // ★★★ D3: ...AND THE BALLISTIC RUN'S OWN DEAD SWITCH, or this arm is no
    // longer the pre-D2 machine it is pinned to. regroup_frac_arm = 0 kills
    // stages 0 and 1, but D3's ALREADY-HOME climb arming does not read that
    // key at all -- it reads raid_ballistic_climb_agl_m. Both must be off for
    // "the machine before any of this" to mean what the pin says.
    o.raid_ballistic_climb_agl_m = 0.0;
    ArmCfg dwell = s;
    dwell.regroup_require_pump_attack = 0;
    // ★★★ ARM F — THE STAGE-1 RETURN, ISOLATED. It was authored as "forced
    // ON" while the shipped table had it OFF; RUNG D3 shipped it ON (the CLIMB
    // it hands to now has a body), so the arm INVERTS to keep measuring the
    // same one thing: what the homecoming costs or buys NOW that the dive
    // exists. Its -6.4% pressure was "bought for nothing" only because CLIMB
    // was empty; this arm is where that verdict gets re-taken every gate run.
    ArmCfg full = s;
    full.regroup_return_deck_run = 0;

    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    const Arm arms[] = {
        {"N  NOISE (shipped + cruise 1e-9)", n_arm},
        {"O  OFF (regroup_frac_arm 0 = walk-back)", o},
        {"S  SHIPPED (stage 0 ON = Chad's AND trigger; stage 1 OFF)", s},
        {"D  DWELL-ALONE (no pump-attack condition)", dwell},
        {"F  RETURN-OFF (stage-1 forced OFF -- what the homecoming costs)",
         full}};
    constexpr std::size_t kArms = 5;

    const auto q = [](std::vector<double>& v, double f) {
        if (v.empty()) return -1.0;
        std::sort(v.begin(), v.end());
        return v[static_cast<std::size_t>(f *
                                          static_cast<double>(v.size() - 1))];
    };

    double third[kArms] = {};
    double coin[kArms] = {};
    double cpm[kArms] = {};
    double rate[kArms] = {};
    double surv[kArms] = {};
    long long eps[kArms] = {};
    long long rets[kArms] = {};   // STAGE-1 returns armed
    long long rtick[kArms] = {};  // ticks flown under ANY regroup order
    int thin[kArms] = {};
    unsigned long long he[kArms] = {};

    std::printf(
        "\n[REGROUP] arm %.3f  lip %.2f  release %.2f  pull %.2f"
        "  agl %.0f m  max %.0f s  window %.0f s  require_attack %d"
        "  return_deck_run %d\n"
        "  ★ NOISE ARM PRINTED FIRST. Any statistic whose S-vs-O delta is"
        " under 10x the |O-N| delta is NOT a result.\n"
        "  ★ THIS PROBE ASSERTS NO OCCUPANCY THRESHOLD. It reports.\n",
        kScen.drone.regroup_frac_arm, kScen.drone.regroup_frac_lip,
        kScen.drone.regroup_frac_release, kScen.drone.regroup_pull_frac,
        kScen.drone.regroup_agl_m, kScen.drone.regroup_max_s,
        kScen.drone.regroup_attack_window_s,
        kScen.drone.regroup_require_pump_attack ? 1 : 0,
        kScen.drone.regroup_return_deck_run ? 1 : 0);

    for (std::size_t i = 0; i < kArms; ++i) {
        MatchResult r = fly_match(rep, arms[i].cfg);
        const double ls = static_cast<double>(std::max(1LL, r.loiter_samples));
        third[i] = 100.0 * static_cast<double>(r.loiter_third) / ls;
        coin[i] = 100.0 * static_cast<double>(r.loiter_coincide) / ls;
        cpm[i] = r.enemy_crashes / std::max(1e-9, r.minutes);
        rate[i] = r.onstation_rate();
        surv[i] = r.deck_cross_attempts > 0
                      ? static_cast<double>(r.deck_cross_survived) /
                            r.deck_cross_attempts
                      : -1.0;
        eps[i] = r.regroup_episodes;
        rets[i] = r.regroup_returns;
        rtick[i] = r.regroup_ticks;
        thin[i] = r.crash_thin;
        he[i] = r.hash_enemy;
        const double band =
            100.0 * static_cast<double>(r.loiter_band) / ls;
        std::printf(
            "\n[REGROUP %s]\n"
            "  BOUNDARY LOITER (enemy, non-tunnel, own dome alive; n=%lld"
            " samples = %.0f s)\n"
            "    outer third (0.667,1.0]  %lld = %.2f%%"
            "   whole band to the lip  %lld = %.2f%%\n"
            "    third AND own pump under attack  %lld = %.2f%%\n"
            "  ★ THE TWO CONJUNCTS, SPLIT (which one starves the trigger?)\n"
            "    own pump-attack window OPEN (geometry ignored)  %lld = %.2f%%\n"
            "    BAND (arm,lip] AND window open = THE EXACT ARMING"
            " CONJUNCTION  %lld = %.2f%%\n"
            "    ... AND fully eligible (THE TICKS THAT CAN ARM)  %lld\n"
            "    ... shadowed by ENGAGED %lld   by DEFEND %lld"
            "   (both exclusions are deliberate)\n"
            "  EPISODES armed %lld (of which STAGE-1 returns %lld)"
            "   closed %lld   completed to the release band %lld\n"
            "    ticks under the order %lld = %.0f s"
            "   duration p10 %.1f  MED %.1f  p90 %.1f s\n"
            "  enemy crashes %d (%.2f/min, Chad signed 0.94)"
            "   IN THIN AIR %d   deck survival %.3f   pressure %.4f\n"
            "  hash enemy %016llx\n",
            arms[i].label, r.loiter_samples,
            static_cast<double>(r.loiter_samples) * kAp.sim_dt, r.loiter_third,
            third[i], r.loiter_band, band, r.loiter_coincide, coin[i],
            r.loiter_attack_open,
            100.0 * static_cast<double>(r.loiter_attack_open) / ls,
            r.loiter_band_attacked,
            100.0 * static_cast<double>(r.loiter_band_attacked) / ls,
            r.loiter_arming, r.loiter_shadow_engaged, r.loiter_shadow_defend,
            r.regroup_episodes, r.regroup_returns, r.regroup_closed,
            r.regroup_completed, r.regroup_ticks,
            static_cast<double>(r.regroup_ticks) * kAp.sim_dt,
            q(r.regroup_ep_s, 0.10), q(r.regroup_ep_s, 0.50),
            q(r.regroup_ep_s, 0.90), r.enemy_crashes, cpm[i], r.crash_thin,
            surv[i], rate[i], r.hash_enemy);
    }

    std::printf(
        "\n[REGROUP NOISE FLOOR — measured, not assumed]\n"
        "  ★ THE FLOOR IS |S-N|: two arms on the SAME table 1e-9 m/s apart,\n"
        "    i.e. arms that CANNOT differ for a reason. |O-N| is NOT a floor\n"
        "    (O and N differ by the whole feature AS WELL as the 1e-9).\n"
        "  outer-third %%   S %.2f  N %.2f   |S-N| %.4f\n"
        "  crashes/min     S %.2f  N %.2f\n"
        "  pressure        S %.4f  N %.4f   |S-N| %.6f\n"
        "  deck survival   S %.3f  N %.3f   |S-N| %.4f\n"
        "[REGROUP THE DELTAS THAT MATTER]\n"
        "  outer-third %%   O %.2f -> S %.2f (%+.2f)   DWELL %.2f\n"
        "  crashes/min     O %.2f -> S %.2f (%+.2f)   DWELL %.2f\n"
        "  crashes in THIN AIR  O %d -> S %d   DWELL %d\n"
        "  pressure        O %.4f -> S %.4f (%+.4f)   DWELL %.4f"
        "   ★ THE NERF WATCH\n"
        "  deck survival   O %.3f -> S %.3f   DWELL %.3f\n"
        "  episodes        O %lld -> S %lld   DWELL %lld\n"
        "[REGROUP ★ STAGE 1 -- ITS OWN PRICE, ISOLATED (arm F)]\n"
        "  ★★★ RUNG D3 SHIPPED THE RETURN **ON** AND IT COSTS EXACTLY ZERO\n"
        "    ON THIS REPLAY, because it is SHADOWED BY THE CLIMB. Stage 1\n"
        "    requires ballistic == NONE **and** own_frac > 1.0 (outside my\n"
        "    own dome); D3's already-home arming fires at own_frac <= 1.0 and\n"
        "    latches CLIMB. On this replay the rank-0 runner's raid order\n"
        "    always arms while it is still INSIDE its dome, so the CLIMB wins\n"
        "    the race every time and the RETURN never has a NONE phase left\n"
        "    to arm from. That is Chad's own sentence resolving itself -- \"go\n"
        "    back to whatever bubble IF THEY HAVE ONE\": it has one, and it is\n"
        "    in it. F (return forced OFF) is therefore BIT-IDENTICAL to S.\n"
        "  ⚠ STATED, NOT HIDDEN: stage 1's own flight is consequently NOT\n"
        "    exercised at this head. D2 measured it live (1 return, -6.4%%\n"
        "    pressure) on a machine where CLIMB could not arm. The -6.4%% is\n"
        "    GONE -- not paid off, SHADOWED -- and no capability was reduced.\n"
        "  STAGE-1 returns armed   S %lld   F %lld  (both 0 = the shadow)\n"
        "  regroup ticks           S %lld = %.0f s    F %lld = %.0f s\n"
        "  pressure                S %.4f -> F %.4f (%+.4f)"
        "   ★ THE PRICE\n"
        "  crashes/min             S %.2f -> F %.2f\n"
        "  deck survival           S %.3f -> F %.3f\n"
        "[REGROUP HASH]\n"
        "  O %016llx   PRE-D2 PIN %016llx   %s\n"
        "  N %016llx   (must DIFFER from O -- the perturbation is live)\n"
        "  S %016llx   (SHIPPED. RUNG D3 MOVED IT -- the parabolic dive is\n"
        "                       live, and stage 0's conjunction still has no\n"
        "                       exposure here. Both are REPORTED FACTS.)\n"
        "  D %016llx\n"
        "  F %016llx   (must MATCH S -- the RETURN is shadowed, cost zero)\n",
        third[2], third[0], std::abs(third[2] - third[0]), cpm[2], cpm[0],
        rate[2], rate[0], std::abs(rate[2] - rate[0]), surv[2], surv[0],
        std::abs(surv[2] - surv[0]), third[1], third[2], third[2] - third[1],
        third[3], cpm[1], cpm[2], cpm[2] - cpm[1], cpm[3], thin[1], thin[2],
        thin[3], rate[1], rate[2], rate[2] - rate[1], rate[3], surv[1],
        surv[2], surv[3], eps[1], eps[2], eps[3], rets[2], rets[4], rtick[2],
        static_cast<double>(rtick[2]) * kAp.sim_dt, rtick[4],
        static_cast<double>(rtick[4]) * kAp.sim_dt, rate[2], rate[4],
        rate[4] - rate[2], cpm[2], cpm[4], surv[2], surv[4], he[1],
        kPreD2EnemyHash, he[1] == kPreD2EnemyHash ? "MATCH" : "MOVED", he[0],
        he[2], he[3], he[4]);

    // ---- LIVENESS + BIT-IDENTITY ONLY. No occupancy threshold, by design.
    CHECK(he[1] == kPreD2EnemyHash);  // the walk-back IS the pre-D2 machine
    CHECK(he[0] != he[1]);            // the 1e-9 perturbation really perturbs
    // ★★★ RUNG D3 MOVED THE SHIPPED MACHINE, AND THE RE-PIN IS DECLARED.
    // At D2 this clause asserted the shipped table was bit-identical to
    // pre-D2, because stage 1 shipped OFF and stage 0 had no exposure. D3
    // shipped the parabolic dive AND stage 1 ON, so the shipped arm MUST
    // move -- it would be a dead branch if it did not. What keeps the re-pin
    // honest is arm O above: with BOTH dead switches off it still reproduces
    // kPreD2EnemyHash bit-for-bit, which is asserted one clause up.
    CHECK(he[2] != kPreD2EnemyHash);
    CHECK(he[2] == kShippedD3EnemyHash);
    // ★★★ ...AND THE ARMS THAT SAY "BLIND FIXTURE" RATHER THAN "DEAD BRANCH".
    // Bit-identity above is only allowed to mean "no exposure" if the same
    // machinery demonstrably does something when a conjunct is relaxed.
    //
    // ⚠⚠ READ THE LIMIT OF THESE TWO LEGS BEFORE TRUSTING THEM. They are
    // PERTURBATION checks, NOT flight checks, and MUT-3 proved the difference:
    // killing the entire regroup steering branch in drone::tick left he[3]
    // still != he[1], because the order also perturbs the match through the
    // tunnel router's `!d.regroup.active` term. A moved hash means the ORDER
    // exists, not that an aeroplane ever turned. THE ACTUAL FLIGHT LIVENESS
    // LIVES IN test_drone.cpp, "D2 an armed regroup order actually closes on
    // its point" -- closed-loop, and MUT-3 collapses it exactly (armed range
    // == idle range to the last digit). Do not delete that leg believing these
    // two cover it.
    CHECK(he[3] != he[1]);  // dwell-alone is a different machine
    CHECK(eps[3] > 0);      // ... and arms episodes
    // ★★★ THE FINDING OF THIS RUNG'S REGROUP HALF, ASSERTED RATHER THAN
    // NARRATED: the stage-1 RETURN ships ON and is SHADOWED BY THE CLIMB, so
    // flipping it cost NOTHING. Both counts are zero and the two arms are
    // bit-identical. ⚠ This is a MEASURED SHADOW, not a liveness claim: it
    // says stage 1 never armed here, and the report says so out loud rather
    // than letting a green light imply the return was exercised.
    CHECK(rets[2] == 0);
    CHECK(rets[4] == 0);
    CHECK(he[4] == he[2]);
}

TEST_CASE("E17 sweep: the attack pattern and the arena spiral", "[.e17sweep]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- sweep skipped");
        SUCCEED();
        return;
    }
    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    // The world Chad flew tape 10 on, stated explicitly (never inherited).
    ArmCfg base;
    base.raider_backfill = kGame.conquest.raid_backfill;
    base.deck_terrain_relative = 1;
    base.transit_reach_s_per_km = 11.0;
    // ALL FOUR E17 DIALS OFF -- the pre-E17 machine on the shipped world.
    base.run_recover_alt_m = 0.0;
    base.run_stall_s = 0.0;
    base.raid_attack_alt_m = 0.0;
    base.raid_reattack_m = 0.0;

    // The tunnel half alone (ruling 2).
    ArmCfg tun = base;
    tun.run_recover_alt_m = kScen.drone.maverick.run_recover_alt_m;
    tun.run_stall_s = kScen.drone.maverick.run_stall_s;
    // The spiral fix WITHOUT the bail, to attribute the two.
    ArmCfg tun_no_bail = base;
    tun_no_bail.run_recover_alt_m = kScen.drone.maverick.run_recover_alt_m;
    // The raid half alone (ruling 1).
    ArmCfg raid = base;
    raid.raid_attack_alt_m = kScen.drone.raid_attack_alt_m;
    raid.raid_reattack_m = kScen.drone.raid_reattack_m;
    // ★ SHIPPED: every E17 dial at whatever config says. Left at -1 so this
    // arm reads the shipped table rather than restating it.
    ArmCfg shipped;
    shipped.raider_backfill = kGame.conquest.raid_backfill;
    shipped.deck_terrain_relative = 1;
    shipped.transit_reach_s_per_km = 11.0;

    // ★ THE LIVENESS ARM. T1 came back BIT-IDENTICAL to E0 on the first run of
    // this sweep -- every number to the digit. That is either a fixture blind
    // to the defect or A DEAD BRANCH, and this ladder has been burned by the
    // second before. run_recover_alt_m = 1 m forces the recovery on for
    // essentially every tick of every run: if THIS is still identical to E0,
    // the code is never reached and the rung is a lie. It is a diagnostic, not
    // a candidate -- nothing would ever ship at 1 m.
    ArmCfg live = base;
    live.run_recover_alt_m = 1.0;
    // ★ A5: the RAID half's own liveness arm. The raid dials SHIP 0.0
    // (config/scenario.toml raid_attack_alt_m / raid_reattack_m), so R1 above
    // is E0 restated -- its bit-identity is ARITHMETIC, not evidence about the
    // branch. This arm pins values that MUST move the aim point (600 m
    // radially, drone/drone.h:2278-2282) and arm the reattack latch
    // (:2287-2290). Identical output means the raid branch never reads the
    // dials. Diagnostic, never ships.
    ArmCfg raid_live = base;
    raid_live.raid_attack_alt_m = 600.0;
    raid_live.raid_reattack_m = 2000.0;
    // ★ A5: the FIXTURE-WIDE liveness arm -- cruise 100 m/s vs the shipped 85.
    // Spawn velocity and the cruise governor both read dp.speed, so EVERY
    // trajectory must move. If THIS matches E0 to the digit, the fixture
    // itself is blind and no arm in this sweep means anything.
    ArmCfg fix_live = base;
    fix_live.speed_mps = 100.0;

    const Arm arms[] = {
        {"E0 PRE-E17 (shipped world, all four E17 dials OFF)", base},
        {"FL FIXTURE LIVENESS cruise 100 m/s (never ships)", fix_live},
        {"L  LIVENESS DIAGNOSTIC recover_alt 1 m (never ships)", live},
        {"T1 arena spiral fix only (no RUN bail)", tun_no_bail},
        {"T2 arena spiral fix + RUN bail", tun},
        {"R1 the attack pattern only", raid},
        {"RL RAID LIVENESS attack_alt 600 / reattack 2000 (never ships)",
         raid_live},
        {"S  SHIPPED (everything E17 ships with)", shipped},
    };
    for (const Arm& a : arms) {
        const MatchResult r = fly_match(rep, a.cfg);
        report(a.label, r);
    }
}

// ---------------------------------------------------------------------------
// P-H.4 — RUNG E16's AIR MAP. HIDDEN ([.e16air]), and it flies NOTHING: it
// samples sim::atm_frac_at -- the exact density the plant reads for lift --
// over the corridor every raid and every transit has to cross, at the
// altitudes they cross it. THE QUESTION: is there any air out there at all,
// and if so at what height? Nothing about the vacuum defect can be designed
// before that is a measured table rather than an assumption.
// ---------------------------------------------------------------------------
TEST_CASE("E16 air map: what the corridor between the domes actually holds",
          "[.e16air]") {
    drone::DroneParams dp = kScen.drone;
    dp.count = combat::kNumMavericks;
    MatchWorld w;
    build_world(w, dp);

    const int pf = w.cq.state.player_faction;
    const int ef = 1 - pf;
    world::FactionGrowth grow[2];
    app::faction_growth_from_state(w.cq.state, grow);
    for (int f = 0; f < 2; ++f) {
        glm::dvec3 cdir{0.0, 1.0, 0.0};
        glm::dvec3 maj{0.0};
        double a = 0.0;
        double b = 0.0;
        world::faction_ellipse(f, grow, cdir, maj, a, b);
        std::printf("[E16 air] faction %d  a %.0f m  b %.0f m  ceiling %.0f m\n",
                    f, a, b, grow[f].ceiling_scale * w.cq.bubble_ceiling_m);
    }
    const double gnd = w.hf.radius_at(w.home[ef]) - kAp.R;
    std::printf("[E16 air] ground elevation %.0f m   deck full<=%.0f fade->%.0f\n",
                gnd, w.af.deck_agl_m, w.af.deck_agl_m + w.af.deck_soft_m);

    // THE CORRIDOR: the enemy's home to the player's SURFACE pump -- the exact
    // line a raid sortie flies.
    const glm::dvec3 a_dir = glm::normalize(w.home[ef]);
    const glm::dvec3 b_dir = glm::normalize(w.cq.state.pumps[pf].pos);
    const double arc = kAp.R * std::acos(glm::clamp(
                                   glm::dot(a_dir, b_dir), -1.0, 1.0));
    std::printf("[E16 air] raid corridor %.1f km\n", arc / 1000.0);

    // ★ THE CROSSING TABLE: at each candidate altitude, how much of the
    // corridor is DEAD (air below 0.25 -- where the wing has effectively
    // stopped working), sampled every 50 m. This is the number that decides
    // what altitude a sortie should cross at, and it is measured, not assumed.
    std::printf("[E16 air] crossing cost by altitude\n"
                "      alt      dead km   worst-run km   mean air\n");
    for (double h : {200.0, 350.0, 500.0, 800.0, 1200.0, 1800.0, 2500.0,
                     3200.0, 4000.0}) {
        double dead = 0.0;
        double run = 0.0;
        double worst = 0.0;
        double sum = 0.0;
        const int n = static_cast<int>(arc / 50.0);
        for (int i = 0; i <= n; ++i) {
            const double u = static_cast<double>(i) / n;
            const glm::dvec3 dd =
                glm::normalize(a_dir * (1.0 - u) + b_dir * u);
            const double f = sim::atm_frac_at(dd * (kAp.R + h), &w.env, kAp);
            sum += f;
            if (f < 0.25) {
                dead += 50.0;
                run += 50.0;
                worst = std::max(worst, run);
            } else {
                run = 0.0;
            }
        }
        std::printf("   %7.0f   %8.2f   %12.2f   %8.2f\n", h, dead / 1000.0,
                    worst / 1000.0, sum / (n + 1));
    }

    const double alts[] = {350.0, 400.0, 600.0, 1000.0, 1500.0, 2000.0, 3000.0};
    std::printf("      s/km   ");
    for (double h : alts) std::printf("%7.0f", h);
    std::printf("\n");
    for (int i = 0; i <= 20; ++i) {
        const double u = static_cast<double>(i) / 20.0;
        const glm::dvec3 d = glm::normalize(a_dir * (1.0 - u) + b_dir * u);
        std::printf("   %7.1f", u * arc / 1000.0);
        for (double h : alts) {
            const double f = sim::atm_frac_at(d * (kAp.R + h), &w.env, kAp);
            std::printf("%7.2f", f);
        }
        std::printf("\n");
    }
    SUCCEED();
}

// ---------------------------------------------------------------------------
// ★★★ PROBE [.tunrt] — THE RAID ROUTER AND THE BORE. Chad, 2026-08-26:
// "if they are leaving for a raid on a pump, the surface one, they should
//  actually use the tunnel, that is what it is there for, no more crossing
//  over the thin air zone ... a few designated runs crossing the deck to make
//  a run on the pump is okay ... they need to survive the tunnels."
//
// FIVE arms on the real Sudbury DEM, the noise floor measured FIRST (the
// [.floor2x2] licence, third application):
//   S  the machine BEFORE this rung: router OFF, C1 OFF -- the ONE-KEY
//      WALK-BACK, exactly reproducible from the TOML
//   C  C1 only (the raid/defend errand speed), router still OFF -- so the
//      router's delta is attributable against a C1-only control and not
//      against a control that is missing both
//   R  THE SHIPPED TABLE: router ON, raid_deck_run_slots = 1, C1 ON
//   T  router ON with raid_deck_run_slots = 0 -- STRICTLY TUNNEL. This is the
//      grinder bar: every raider goes through the bore, so in-net deaths per
//      entry is measured at its maximum exposure.
//   N  R + cruise +1e-9 m/s -- THE NOISE FLOOR. Nothing smaller than 10x
//      |R-N| is a result.
//
// ★ IT IS A FOES-PRESENT MATCH, DELIBERATELY (the red team's first correction,
//   and it is the reason this probe exists at all rather than an isolated bore
//   fixture). Stratifying the 12 tapes by the entering drone's own divert
//   state gives divert-DEAD 21 entries -> 7 in-net deaths = 33%, three times
//   the design's own 10% bar, with the divert structurally unarmed. A
//   foe-free bore fixture is STRUCTURALLY BLIND to that second killer (the
//   arena fight interrupt needs a foe) and would have read ~1% while a real
//   match reads 33%. This one replays Chad's own tape into a full 22-minute
//   conquest match, so the foes are there.
//
// ★ NOTHING HERE IS A THRESHOLD except the liveness claims. The in-net death
//   rate, the pressure and the crash rate are MEASUREMENTS handed to Chad. He
//   ruled "leave it at full strength"; a probe that quietly asserted a
//   pressure ceiling would be the nerf-in-secret shape.
TEST_CASE("TUNNEL ROUTER sweep: do raids use the bore, and do they survive it",
          "[.tunrt]") {
    const Replay rep = load_replay(kReplayPath);
    if (!rep.ok) {
        WARN("replay track absent -- TUNNEL ROUTER sweep skipped, not failed");
        SUCCEED();
        return;
    }
    ArmCfg r_arm;
    r_arm.raider_backfill = kGame.conquest.raid_backfill;  // the shipped table
    ArmCfg s = r_arm;
    s.raid_route_via_tunnel = 0;   // THE WALK-BACK
    s.raid_speed_target = 0.0;     // C1 off too: the pre-rung machine
    ArmCfg c = r_arm;
    c.raid_route_via_tunnel = 0;   // C1 only
    ArmCfg t = r_arm;
    t.raid_deck_run_slots = 0;     // STRICTLY TUNNEL: the grinder bar
    ArmCfg n = r_arm;
    n.speed_mps = kScen.drone.speed + 1e-9;

    struct Arm {
        const char* label;
        ArmCfg cfg;
    };
    const Arm arms[] = {{"S  SHIPPED-BEFORE (router OFF, C1 OFF)", s},
                        {"C  C1 only (errand speed), router OFF", c},
                        {"R  ROUTER (shipped: slots=1, C1 on)", r_arm},
                        {"T  STRICTLY TUNNEL (slots=0)", t},
                        {"N  NOISE (R + cruise 1e-9)", n}};
    constexpr std::size_t kArms = 5;
    double vtf[kArms] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double netdeath[kArms] = {-1.0, -1.0, -1.0, -1.0, -1.0};
    double rate[kArms] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double cpm[kArms] = {0.0, 0.0, 0.0, 0.0, 0.0};
    long long ent[kArms] = {0, 0, 0, 0, 0};
    unsigned long long he[kArms] = {0ull, 0ull, 0ull, 0ull, 0ull};
    long long rnet[kArms] = {0, 0, 0, 0, 0};

    std::printf(
        "\n[TUNRT] shipped router: via_tunnel=%s slots=%d gap_max=%.0f m"
        " stagger=%.1f s ; C1 errand speed %.1f m/s\n",
        kScen.drone.raid_route_via_tunnel ? "true" : "false",
        kScen.drone.raid_deck_run_slots, kScen.drone.raid_route_gap_max_m,
        kScen.drone.raid_route_stagger_s, kScen.drone.raid_speed_target);

    for (std::size_t i = 0; i < kArms; ++i) {
        MatchResult res = fly_match(rep, arms[i].cfg);
        std::sort(res.raid_agl.begin(), res.raid_agl.end());
        const auto q = [](const std::vector<double>& v, double f) {
            return v.empty() ? -1.0
                             : v[static_cast<std::size_t>(
                                   f * static_cast<double>(v.size() - 1))];
        };
        vtf[i] = static_cast<double>(res.raid_vt_ticks);
        rnet[i] = res.raid_net_ticks;
        ent[i] = res.net_entries;
        netdeath[i] = res.net_entries > 0
                          ? static_cast<double>(res.in_net_deaths) /
                                static_cast<double>(res.net_entries)
                          : -1.0;
        rate[i] = res.onstation_rate();
        cpm[i] = res.enemy_crashes / std::max(1e-9, res.minutes);
        he[i] = res.hash_enemy;
        std::printf(
            "\n[TUNRT %s]\n"
            "  ROUTER: raid ticks routed VIA TUNNEL %lld ; raid ticks flown"
            " INSIDE THE NET %lld ; raid duty %.0f s\n"
            "  THE BORE: net entries %lld ; IN-NET DEATHS %d (%.1f%% per"
            " entry) ; CLIMB_OUT completions %lld (%.0f%% of entries)\n"
            "  multi-ship: max simultaneous in-net %d ; min pairwise"
            " separation %.0f m\n"
            "  enemy crashes %d (%.2f/min, Chad signed 0.94) ;"
            " PRESSURE %.4f ; player pumps lost at %.0f s\n"
            "  raid AGL within 2 km of pump: p10 %.0f MED %.0f p90 %.0f"
            " (n=%zu)\n"
            "  hash enemy %016llx\n",
            arms[i].label, res.raid_vt_ticks, res.raid_net_ticks,
            res.duty_s[1], res.net_entries, res.in_net_deaths,
            netdeath[i] < 0.0 ? -1.0 : 100.0 * netdeath[i], res.climbout_done,
            res.net_entries > 0 ? 100.0 * static_cast<double>(
                                              res.climbout_done) /
                                      static_cast<double>(res.net_entries)
                                : -1.0,
            res.max_in_net,
            res.min_pair_sep_m > 1e17 ? -1.0 : res.min_pair_sep_m,
            res.enemy_crashes, cpm[i], rate[i], res.player_pumps_lost_s,
            q(res.raid_agl, 0.10), q(res.raid_agl, 0.50),
            q(res.raid_agl, 0.90), res.raid_agl.size(), he[i]);
    }

    std::printf(
        "\n[TUNRT SUMMARY]\n"
        "  raid ticks VIA TUNNEL      S %.0f   C %.0f   R %.0f   T %.0f\n"
        "  in-net deaths / entry      S %.3f  C %.3f  R %.3f  T %.3f\n"
        "  net entries                S %lld  C %lld  R %lld  T %lld\n"
        "  enemy crashes/min          S %.2f   C %.2f   R %.2f   T %.2f"
        "   (signed 0.94)\n"
        "  PRESSURE (on-station rate) S %.4f  C %.4f  R %.4f  T %.4f\n"
        "  NOISE FLOOR |R-N| pressure %.6f ; crashes/min %.4f\n"
        "  hash S==R %s   C==R %s   T==R %s   N==R %s\n",
        vtf[0], vtf[1], vtf[2], vtf[3], netdeath[0], netdeath[1], netdeath[2],
        netdeath[3], ent[0], ent[1], ent[2], ent[3], cpm[0], cpm[1], cpm[2],
        cpm[3], rate[0], rate[1], rate[2], rate[3],
        std::abs(rate[2] - rate[4]), std::abs(cpm[2] - cpm[4]),
        he[0] == he[2] ? "YES" : "no", he[1] == he[2] ? "YES" : "no",
        he[3] == he[2] ? "YES" : "no", he[4] == he[2] ? "YES" : "no");

    // THE ONLY HARD CLAIMS ARE LIVENESS. Everything above is a measurement.
    CHECK(he[0] != he[2]);  // the router really moves the machine
    CHECK(he[1] != he[2]);  // the router is separable from C1
    CHECK(he[4] != he[2]);  // the noise replicate really perturbs
    // And the router must actually FIRE, or this probe measured nothing.
    CHECK(vtf[2] > 0.0);
    CHECK(vtf[3] >= vtf[2]);  // slots=0 routes at least as much as slots=1
    // ★★★ THE UNFREEZE, PINNED WHERE IT CAN FAIL. Routing a raid by the
    // tunnel is worthless unless the run scheduler is actually released:
    // hold_runs froze it whenever a RaidOrder was armed, so at HEAD a raider
    // could never enter the bore. The discriminator is RAID ticks FLOWN
    // INSIDE THE NET, and it is sharp. MUTATION APPLIED AND CONFIRMED RED
    // (drone/drone.h, raid_holds = d.raid.active, i.e. the freeze restored):
    // this number collapses from 25065 back to 6997 -- EXACTLY the S/C arm
    // value, to the tick -- and the leg below fails. Restored.
    INFO("raid ticks in net: S " << rnet[0] << "  C " << rnet[1]
                                 << "  R " << rnet[2]);
    CHECK(rnet[2] > 2 * rnet[0]);
}
