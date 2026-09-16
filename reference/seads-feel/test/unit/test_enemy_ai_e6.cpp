// RUNG E6 — THE RELENTLESS PASS (docs/ENEMY_AI_E1_E2_SPEC.md "RUNG E6", from
// Chad's fly tape build-play/conquest_tape_2.jsonl, 2026-08-20 midday).
//
// CHAD'S VERDICT: died twice "for no reason" with random respawn; no enemy
// tunnel flying; "mostly they run"; enemies fly fine in no-air and use it;
// revived enemies stayed home; enemies poke a pump then abandon it — "they
// should relentlessly attack it and fight me at the same time, not fly away to
// safety and then fight me. We are literally fighting for air here"; and wave
// replacement kills victory-by-elimination.
//
// House discipline applied to every dial here:
//   * the OFF arm is proven bit-identical with `==`, never a tolerance band;
//   * every off arm is PAIRED with an ON arm that must DIFFER, so a dial that
//     silently did nothing could not pass as "bit-identical";
//   * each leg proves its own premise first (the fixture-no-op class).
//
// TEST_CASE and SECTION names are STRICTLY ASCII (a non-ASCII name silently
// never runs — the recurring trap).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/conquest_tape.h"
#include "app/instructor_tick.h"
#include "combat/conquest.h"
#include "combat/kill.h"
#include "combat/reinforce.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "sim/aero.h"
#include "sim/state.h"
#include "combat/raid.h"
#include "drone/bfm.h"
#include "drone/maverick.h"
#include "sim/environment.h"
#include "sim/step.h"
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
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

app::LoopState parked_player() {
    app::LoopState st{};
    st.curr = st.prev = app::spawn_state(kAp);
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = false;
    return st;
}

}  // namespace

// ---------------------------------------------------------------------------
// E6.1 — THE PLAYER-DEATH CAUSE RECORD
//
// THE MEASURED DEFECT this pins: `CombatWorld::deaths` is incremented by ONLY
// the component-death branch of app::tick, so a TERRAIN CRASH death booked
// nothing at all and the tape's 'pd' event never fired — Chad's tape 2 carried
// two deaths and ZERO 'pd' events. Both branches must book now, with a cause.
// ---------------------------------------------------------------------------

TEST_CASE("E6.1: a terrain crash books a player death with cause crash") {
    app::LoopState st = parked_player();
    // Below the crash sphere with a null ground field: app::tick's bare-sphere
    // predicate (altitude <= 0, not in a tunnel) fires on this very tick.
    const glm::dvec3 up = glm::normalize(st.curr.position);
    st.curr.position = up * (kAp.R - 50.0);
    st.prev = st.curr;

    combat::CombatWorld cw;
    REQUIRE(cw.death_events == 0);
    REQUIRE(cw.deaths == 0);

    app::TickInput in;
    app::tick(st, in, kAp, kCp, nullptr, nullptr, nullptr, &cw, nullptr,
              nullptr);

    // BOTH branches book here; the HUD/score `deaths` counter keeps its old
    // component-only meaning and must NOT have moved.
    REQUIRE(cw.death_events == 1);
    REQUIRE(cw.deaths == 0);
    REQUIRE(cw.death_cause == combat::CombatWorld::DeathCause::kCrash);
    // The record is taken BEFORE the reset, so it is the DEAD life's position
    // (this tick's post-step one — metres from where it started, not the
    // kilometres away the fresh spawn sits).
    REQUIRE(glm::length(cw.death_pos - up * (kAp.R - 50.0)) < 25.0);
    REQUIRE(glm::length(cw.death_pos - st.curr.position) > 100.0);
    REQUIRE(cw.death_in_net == false);
    // A null env => atm_frac_at is the plain altitude curve, == 1 on the deck.
    REQUIRE(cw.death_air_frac > 0.0);
}

TEST_CASE("E6.1: a component death carries the damage source as its cause") {
    // The component-death branch lives under `cw && dw`, so every arm below
    // wires an (empty) DroneWorld — the same guard the app itself runs under.
    app::DroneWorld dw;
    dw.dparams = kScen.drone;
    // The gun arm: the last damage that touched the player was an enemy round.
    {
        app::LoopState st = parked_player();
        combat::CombatWorld cw;
        cw.damage.pilot = 0.0;      // is_dead
        cw.last_damage_src = 1;     // an enemy round (combat_player_tick)
        app::TickInput in;
        app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, &cw, nullptr,
                  nullptr);
        REQUIRE(cw.death_events == 1);
        REQUIRE(cw.deaths == 1);  // the component branch DOES move this one
        REQUIRE(cw.death_cause == combat::CombatWorld::DeathCause::kGun);
        // The component picture is snapshotted BEFORE reset_damage.
        REQUIRE(cw.death_damage.pilot == 0.0);
        REQUIRE(cw.damage.pilot == 1.0);  // the fresh airframe
        // A fresh life inherits no attribution.
        REQUIRE(cw.last_damage_src == 0);
    }
    // The ground arm: the last damage was a terrain scrape.
    {
        app::LoopState st = parked_player();
        combat::CombatWorld cw;
        cw.damage.wing_left = 0.0;
        cw.last_damage_src = 2;
        app::TickInput in;
        app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, &cw, nullptr,
                  nullptr);
        REQUIRE(cw.death_cause == combat::CombatWorld::DeathCause::kGround);
    }
    // The unattributed arm.
    {
        app::LoopState st = parked_player();
        combat::CombatWorld cw;
        cw.damage.structure = 0.0;
        cw.last_damage_src = 0;
        app::TickInput in;
        app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, &cw, nullptr,
                  nullptr);
        REQUIRE(cw.death_cause == combat::CombatWorld::DeathCause::kComponent);
    }
}

TEST_CASE("E6.1: an enemy round latches the gun damage source") {
    combat::CombatWorld cw;
    cw.enemy_pool.resize(1);
    weapon::Projectile& p = cw.enemy_pool[0];
    p.active = true;
    p.friendly = false;
    const glm::dvec3 base{0.0, 0.0, kAp.R + 2000.0};
    sim::SimState prev;
    sim::SimState curr;
    prev.position = curr.position = base;
    // A round swept straight through the player hit sphere.
    p.prev_pos = base + glm::dvec3{-50.0, 0.0, 0.0};
    p.pos = base + glm::dvec3{50.0, 0.0, 0.0};
    p.vel = glm::dvec3{600.0, 0.0, 0.0};
    p.v_ref = 600.0;
    p.damage = 30.0;
    REQUIRE(cw.last_damage_src == 0);
    combat::combat_player_tick(cw, prev, curr);
    REQUIRE(cw.player_hits == 1);
    REQUIRE(cw.last_damage_src == 1);
}

TEST_CASE("E6.1: the death ledger is write only and moves no trajectory") {
    // The SAME tick flown twice: once with the ledger pre-poisoned to garbage,
    // once clean. If anything downstream read those fields the two states would
    // diverge; they must be bit-identical.
    const auto fly = [](bool poison) {
        app::LoopState st = parked_player();
        combat::CombatWorld cw;
        if (poison) {
            cw.death_events = 999;
            cw.death_cause = combat::CombatWorld::DeathCause::kGun;
            cw.death_pos = glm::dvec3{1e6, -1e6, 1e6};
            cw.death_air_frac = -7.0;
            cw.death_in_net = true;
            cw.last_damage_src = 2;
        }
        app::TickInput in;
        for (int t = 0; t < 240; ++t)
            app::tick(st, in, kAp, kCp, nullptr, nullptr, nullptr, &cw, nullptr,
                      nullptr);
        return st.curr;
    };
    const sim::SimState a = fly(false);
    const sim::SimState b = fly(true);
    REQUIRE(a.position == b.position);
    REQUIRE(a.velocity == b.velocity);
    REQUIRE(a.orientation == b.orientation);
}

TEST_CASE("E6.1: the tape emits pd with a cause on a crash death") {
    app::LoopState st = parked_player();
    const glm::dvec3 up = glm::normalize(st.curr.position);
    combat::CombatWorld cw;
    seads_tape::ConquestTape tape("e6");

    app::TickInput in;
    // Tick 1: no death — this is the snapshot-priming tick (spec 3.3).
    app::tick(st, in, kAp, kCp, nullptr, nullptr, nullptr, &cw, nullptr,
              nullptr);
    tape.on_tick(in, st, nullptr, &cw, nullptr, nullptr);
    // Tick 2: put him under the sphere so the crash branch fires.
    st.curr.position = up * (kAp.R - 50.0);
    st.prev = st.curr;
    app::tick(st, in, kAp, kCp, nullptr, nullptr, nullptr, &cw, nullptr,
              nullptr);
    tape.on_tick(in, st, nullptr, &cw, nullptr, nullptr);

    std::string out;
    REQUIRE(tape.drain(out));
    REQUIRE(out.find("\"t\":\"pd\"") != std::string::npos);
    REQUIRE(out.find("\"cause\":\"crash\"") != std::string::npos);
    REQUIRE(out.find("\"air\":") != std::string::npos);
    REQUIRE(out.find("\"comp\":") != std::string::npos);
}

// ---------------------------------------------------------------------------
// E6.2 — THE VACUUM TAX
//
// ATTRIBUTION FIRST (the rung's own hard rule: instrument, measure, then
// build). Chad's felt report is "they fly fine in no-air and use it". The
// measurement below asks whether the DRONE PLANT gets a vacuum discount the
// player's plant does not, and the answer is NO: drone::tick's last act is
// `sim::step(d.curr, in, ap, env, dt)` — the SAME kernel call, with the SAME
// sim::Environment, that app::tick feeds the player. Thrust (T_max * f_atm)
// and dynamic pressure (rho * f_atm) therefore lapse identically for both.
// There is no plant-side asymmetry to remove.
//
// What IS asymmetric is a BEHAVIOUR, and E6.2's dial addresses that one: see
// the extend_leash cases below.
// ---------------------------------------------------------------------------

namespace {

// A single-dome atmosphere: full air over `centre`, vacuum on the far side.
struct AirWorld {
    world::HeightField hf;
    sim::AtmosphereField af;
    sim::Environment env;
};

world::HeightField flat_field(double elev_m, double relief = 4000.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

void build_air_world(AirWorld& w, const glm::dvec3& centre) {
    w.hf = flat_field(0.0);
    w.af.deck_agl_m = 0.0;   // no global deck: the dome is the ONLY air, so
    w.af.deck_soft_m = 1.0;  // "outside the dome" really is vacuum
    sim::AtmosphereField::Bubble b;
    b.center_dir = glm::normalize(centre);
    b.ground_radius_m = 8000.0;
    b.minor_radius_m = 0.0;
    b.ceiling_m = 6000.0;
    b.ceil_soft_m = 500.0;
    b.edge_soft_m = 500.0;
    w.af.bubbles.push_back(b);
    w.env.ground = &w.hf;
    w.env.atm = &w.af;
    // A live ground field needs its GroundParams: normal_at asserts a positive
    // probe. Same table the tunnel/stope probes use.
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    w.env.ground_params = gp;
}

}  // namespace

TEST_CASE("E6.2 attribution: the drone plant pays the players vacuum tax") {
    AirWorld w;
    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    build_air_world(w, centre);
    // The far side of the planet from the dome: no deck, no bubble, no air.
    const glm::dvec3 far_dir = -centre;

    const auto at = [&](const glm::dvec3& dir) {
        return dir * (w.hf.radius_at(dir) + 2000.0);
    };
    const double air_in = sim::atm_frac_at(at(centre), &w.env, kAp);
    const double air_out = sim::atm_frac_at(at(far_dir), &w.env, kAp);
    INFO("atm_frac in-dome=" << air_in << "  out-of-dome=" << air_out);
    REQUIRE(air_in > 0.99);   // the fixture really is full air
    REQUIRE(air_out < 0.01);  // and really is vacuum

    // Fly the SAME kernel call both machines use, at full power and level,
    // from an identical level state at each point. This is the plant, with no
    // brain of either kind on top of it.
    const auto plant_run = [&](const glm::dvec3& dir) {
        const drone::DroneParams dp = kScen.drone;
        const glm::dvec3 pos = at(dir);
        glm::dvec3 ref{0.0, 1.0, 0.0};
        if (std::abs(glm::dot(ref, glm::normalize(pos))) > 0.9)
            ref = glm::dvec3{1.0, 0.0, 0.0};
        const glm::dvec3 east =
            glm::normalize(glm::cross(ref, glm::normalize(pos)));
        sim::SimState s = drone::level_state_at(dp, pos, east);
        sim::Inputs in;
        in.throttle = 1.0f;
        const double alt0 = sim::altitude(s.position, kAp);
        const double v0 = glm::length(s.velocity);
        for (int t = 0; t < 3600; ++t)  // 30 s
            s = sim::step(s, in, kAp, &w.env, kAp.sim_dt);
        return std::pair<double, double>(
            glm::length(s.velocity) - v0,
            sim::altitude(s.position, kAp) - alt0);
    };
    const std::pair<double, double> in_air = plant_run(centre);
    const std::pair<double, double> in_vac = plant_run(far_dir);
    INFO("PLANT 30 s at full power -- in air: dV=" << in_air.first
         << " m/s dAlt=" << in_air.second << " m | in vacuum: dV="
         << in_vac.first << " m/s dAlt=" << in_vac.second << " m");
    // The vacuum arm mushes: it cannot hold altitude, because lift AND thrust
    // both went with the air. This is the tax, and it is the SAME sim::step
    // the player flies -- there is nothing drone-side to take away.
    REQUIRE(in_vac.second < in_air.second - 100.0);
    REQUIRE(in_vac.second < 0.0);
}

namespace {

// One 7.5 s BFM-Extend flee from just outside the dome's steer fraction.
// `dial` arms E6.2; `arm_leash_data` decides whether the leash ORDER is even
// populated (used by the off-arm bit-identity leg).
sim::SimState extend_flee(const AirWorld& w, const glm::dvec3& centre,
                          bool dial, bool arm_leash_data, double* arc_out) {
    drone::DroneParams dp = kScen.drone;
    dp.maverick.enabled = false;
    dp.extend_leash = dial;
    // 6.5 km off the dome centre on an 8 km circle: frac ~0.81 -- past
    // steer_frac (0.55) and inside hard_frac, so the leash is live.
    glm::dvec3 ref{0.0, 1.0, 0.0};
    const glm::dvec3 east = glm::normalize(glm::cross(ref, centre));
    const double ang = 6500.0 / kAp.R;
    const glm::dvec3 dir =
        glm::normalize(std::cos(ang) * centre + std::sin(ang) * east);
    const glm::dvec3 pos = dir * (w.hf.radius_at(dir) + 2000.0);
    // Heading straight OUT of the dome -- the flee E6.2 is about.
    const glm::dvec3 out = glm::normalize(east - dir * glm::dot(east, dir));

    drone::DroneState d;
    d.curr = drone::level_state_at(dp, pos, out);
    d.prev = d.curr;
    d.hp = dp.hp;
    d.engaged = true;
    d.foe = drone::kFoePlayer;
    d.bfm.mode = bfm::BfmState::Mode::Extend;
    if (arm_leash_data) {
        d.leash.enabled = true;
        d.leash.center_dir = centre;
        d.leash.major_axis = east;
        d.leash.a_m = 8000.0;
        d.leash.b_m = 8000.0;
    }
    // The foe: fast, astern, so the energy deficit keeps him extending.
    sim::SimState foe;
    foe.position = pos - out * 1200.0;
    foe.velocity = out * 300.0;
    foe.orientation = d.curr.orientation;

    for (int t = 0; t < 900; ++t)  // 7.5 s
        drone::tick(d, kAp, dp, &w.env, &foe);
    if (arc_out != nullptr) {
        const glm::dvec3 u = glm::normalize(d.curr.position);
        *arc_out =
            kAp.R * std::acos(std::clamp(glm::dot(u, centre), -1.0, 1.0));
    }
    return d.curr;
}

}  // namespace

TEST_CASE("E6.2: the extend leash bends a flee back toward the air") {
    AirWorld w;
    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    build_air_world(w, centre);
    double arc_off = 0.0, arc_on = 0.0;
    const sim::SimState off = extend_flee(w, centre, false, true, &arc_off);
    const sim::SimState on = extend_flee(w, centre, true, true, &arc_on);
    INFO("arc from the dome centre after a 7.5 s extend: off=" << arc_off
         << " m  on=" << arc_on << " m (dome radius 8000 m)");
    // The dial must actually do something...
    REQUIRE_FALSE(on.position == off.position);
    // ...and what it does is bend the flee back toward the air.
    REQUIRE(arc_on < arc_off);
}

TEST_CASE("E6.2: extend leash off is bit identical to the pre E6 tick") {
    // The structural off-arm: with the dial false, the engaged branch is the
    // single `d.leash_engaged = false` line it has always been -- proven
    // against a run whose leash order is fully ARMED and would otherwise bite.
    AirWorld w;
    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    build_air_world(w, centre);
    const sim::SimState no_data = extend_flee(w, centre, false, false, nullptr);
    const sim::SimState armed = extend_flee(w, centre, false, true, nullptr);
    REQUIRE(no_data.position == armed.position);
    REQUIRE(no_data.velocity == armed.velocity);
    REQUIRE(no_data.orientation == armed.orientation);
}

// ---------------------------------------------------------------------------
// E6.3 — THE AGGRESSION POSTURE ("mostly they run")
// ---------------------------------------------------------------------------

TEST_CASE("E6.3: the aggression posture forbids Extend and aborts one running") {
    // Pure bfm: an Offensive fight that has run past frustration_s. Off, the
    // machine bails to Extend; on, it may not.
    const auto run = [](bool suppress, bfm::BfmState::Mode start) {
        bfm::BfmParams bp = kScen.drone.bfm;
        bp.enabled = true;
        bfm::BfmState st;
        st.mode = start;
        st.fight_ticks =
            static_cast<long long>(bp.frustration_s / kAp.sim_dt) + 10;
        st.mode_ticks = st.fight_ticks;
        bfm::BfmDials dl;
        dl.speed = kScen.drone.speed;
        dl.suppress_extend = suppress;
        // A co-speed target inside attack range, off the nose: a live fight
        // that the frustration clock has given up on.
        sim::SimState me;
        me.position = glm::dvec3{0.0, 0.0, kAp.R + 2000.0};
        me.velocity = glm::dvec3{120.0, 0.0, 0.0};
        sim::SimState tgt = me;
        tgt.position += glm::dvec3{500.0, 200.0, 0.0};
        tgt.velocity = glm::dvec3{120.0, 0.0, 0.0};
        bfm::bfm_step(st, me, tgt, bp, dl, kAp, kAp.sim_dt);
        return st.mode;
    };
    // Premise: without the posture the frustration exit really does fire.
    REQUIRE(run(false, bfm::BfmState::Mode::Offensive) ==
            bfm::BfmState::Mode::Extend);
    // Armed: the exit is structurally unavailable.
    REQUIRE(run(true, bfm::BfmState::Mode::Offensive) !=
            bfm::BfmState::Mode::Extend);
    // And an Extend already running is ABORTED on the spot (the Yoyo
    // air_abort shape) rather than flown out to extend_min_s.
    REQUIRE(run(true, bfm::BfmState::Mode::Extend) ==
            bfm::BfmState::Mode::Intercept);
}

TEST_CASE("E6.3: the posture arms from defend and raid, never from strike") {
    // drone::tick computes the posture from the orders it holds. The STRIKE
    // order is armed on every pilot every tick (100% of Chad's tape 2), so
    // using it as a trigger would be a permanent Extend repeal, not a posture.
    // This pins that it is NOT a trigger.
    AirWorld w;
    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    build_air_world(w, centre);

    // Which order is armed decides whether an Extend survives one tick.
    enum class Order { kNone, kStrike, kRaid, kDefend };
    const auto mode_after = [&](Order o, double range_m) {
        drone::DroneParams dp = kScen.drone;
        dp.maverick.enabled = false;
        dp.aggression_range_m = range_m;
        const glm::dvec3 pos = centre * (w.hf.radius_at(centre) + 2000.0);
        glm::dvec3 ref{0.0, 1.0, 0.0};
        const glm::dvec3 east = glm::normalize(glm::cross(ref, centre));
        drone::DroneState d;
        d.curr = drone::level_state_at(dp, pos, east);
        d.prev = d.curr;
        d.hp = dp.hp;
        d.engaged = true;
        d.foe = drone::kFoePlayer;
        d.bfm.mode = bfm::BfmState::Mode::Extend;
        d.bfm.mode_ticks = 1;
        // A pump target 500 m away -- comfortably inside any armed radius.
        const glm::dvec3 tgt_pos = pos + east * 500.0;
        if (o == Order::kStrike) {
            d.strike.active = true;
            d.strike.target_pos = tgt_pos;
        } else if (o == Order::kRaid) {
            // NOTE: a live raid ORDER also owns the steering branch, so the
            // posture is what we read, not the trajectory.
            d.raid.active = true;
            d.raid.target_pos = tgt_pos;
        } else if (o == Order::kDefend) {
            d.defend.active = true;
            d.defend.target_pos = tgt_pos;
        }
        sim::SimState foe;
        foe.position = pos + east * 600.0;
        foe.velocity = east * 260.0;
        foe.orientation = d.curr.orientation;
        drone::tick(d, kAp, dp, &w.env, &foe);
        return d.bfm.mode;
    };

    // Premise: with the posture OFF, the extend survives the tick.
    REQUIRE(mode_after(Order::kDefend, 0.0) == bfm::BfmState::Mode::Extend);
    // Defend arms it.
    REQUIRE(mode_after(Order::kDefend, 3000.0) !=
            bfm::BfmState::Mode::Extend);
    // A standing STRIKE order does not.
    REQUIRE(mode_after(Order::kStrike, 3000.0) == bfm::BfmState::Mode::Extend);
    // No order at all does not.
    REQUIRE(mode_after(Order::kNone, 3000.0) == bfm::BfmState::Mode::Extend);
}

// ---------------------------------------------------------------------------
// E6.4 — REVIVED ENEMIES MUST REJOIN THE WAR
//
// ATTRIBUTION (Chad's tape 2, measured): the enemy faction lost both pumps by
// 8:18, its radius_scale read 0.0 from 9:17, and its revived pilots stayed
// home. The app's leash arming only falls through to the SURVIVING faction's
// dome when its own ellipse is LITERALLY degenerate; a dome floored at
// kFactionScaleFloor is a live, tiny ellipse, so containment faithfully pinned
// a whole wing inside a bubble that barely exists. leash_min_radius_scale is
// the floor that makes "crushed" count as "extinct".
// ---------------------------------------------------------------------------

namespace {

// One conquest tick's worth of app leash arming, read back off the drone.
glm::dvec3 leash_centre_after_tick(double own_rs, double floor_rs,
                                   int own_faction) {
    app::LoopState st = parked_player();
    app::DroneWorld dw;
    dw.dparams = kScen.drone;
    dw.dparams.maverick.enabled = false;
    // spawn_index 0 and 5 are the two factions (combat::maverick_faction).
    const int idx = own_faction == combat::CQ_SUDBURY ? 0 : 5;
    REQUIRE(combat::maverick_faction(idx) == own_faction);
    dw.drones.push_back(
        drone::spawn_drone(kAp, dw.dparams, idx, combat::kNumMavericks));
    dw.drones[0].grounded = false;

    app::ConquestWorld cq;
    cq.state.player_faction = combat::CQ_VALLEY;
    cq.params.leash_min_radius_scale = floor_rs;
    cq.state.radius_scale[own_faction] = own_rs;
    cq.state.radius_scale[1 - own_faction] = 1.0;

    app::TickInput in;
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr, &cq);
    return dw.drones[0].leash.center_dir;
}

}  // namespace

TEST_CASE("E6.4: a crushed dome stops being a leash anchor") {
    world::FactionGrowth full[2]{};
    glm::dvec3 c0{0.0}, c1{0.0}, maj{0.0};
    double a = 0.0, b = 0.0;
    world::faction_ellipse(0, full, c0, maj, a, b);
    world::faction_ellipse(1, full, c1, maj, a, b);
    REQUIRE(glm::length(c0 - c1) > 1e-6);  // the two domes really are apart

    // OFF (floor 0): even a crushed 0.05 dome keeps its own pilots home --
    // the measured tape-2 behaviour, pinned so the fix has a premise.
    const glm::dvec3 off = leash_centre_after_tick(0.05, 0.0, 1);
    REQUIRE(glm::length(off - glm::normalize(c1)) < 1e-9);

    // ON (floor 0.25): the crushed faction is leashed to the enemy's air --
    // the same "confinement DRIVES the invasion" rule the extinct branch runs.
    const glm::dvec3 on = leash_centre_after_tick(0.05, 0.25, 1);
    REQUIRE(glm::length(on - glm::normalize(c0)) < 1e-9);

    // A HEALTHY dome is untouched by the floor (it is a floor, not a switch).
    const glm::dvec3 healthy = leash_centre_after_tick(1.0, 0.25, 1);
    REQUIRE(glm::length(healthy - glm::normalize(c1)) < 1e-9);
}

TEST_CASE("E6.4: a dead or reviving striker never wedges the concurrent cap") {
    // The E3.2 cap counts pilots holding a collapse-triggered run. If a WRECK
    // could hold a slot, one dead striker would shut the tunnel pipeline for
    // its whole faction -- which is one of the two candidate explanations for
    // tape 2's ZERO enemy tunnel entries. Pin that it cannot.
    app::LoopState st = parked_player();
    app::DroneWorld dw;
    dw.dparams = kScen.drone;
    dw.dparams.maverick.enabled = false;
    dw.dparams.strike_concurrent_max = 1;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        dw.drones.push_back(
            drone::spawn_drone(kAp, dw.dparams, i, combat::kNumMavericks));
        dw.drones.back().grounded = false;
    }
    // Pilot 0 is a WRECK mid-run (killed while transiting on order) — exactly
    // the tape-2 shape, where every enemy striker died in transit.
    dw.drones[0].inert = true;
    dw.drones[0].mav.run_on_order = true;
    dw.drones[0].mav.mode = maverick::MaverickState::Mode::TRANSIT;

    app::ConquestWorld cq;
    cq.state.player_faction = combat::CQ_VALLEY;
    app::TickInput in;
    app::tick(st, in, kAp, kCp, nullptr, &dw, nullptr, nullptr, nullptr, &cq);

    // Pilot 1 is the same faction as pilot 0. His order must NOT be held.
    REQUIRE(combat::maverick_faction(1) == combat::maverick_faction(0));
    REQUIRE(dw.drones[1].strike.order_hold == false);
}

// ---------------------------------------------------------------------------
// E6.5 — RELENTLESS RAIDS + PROBE P-F "the relentless raider"
// ---------------------------------------------------------------------------

namespace {

struct RaidProbe {
    double on_station_s = 0.0;   // seconds the raider held the pump envelope
    long long rounds_at_player = 0;
    double max_range_from_pump_m = 0.0;
    double end_range_from_pump_m = 0.0;
    double min_range_to_player_m = 1e300;
    long long band_ticks = 0;      // player inside the gun band
    long long wants_fire_ticks = 0;
};

// PROBE P-F. A designated raider on a live raid order, with the player parked
// 700 m off the pump — INSIDE raid_fight_yield_m, so today's merge yield hands
// the raider wholesale to the dogfight. The player is STEPPED (a real
// sim::step every tick from a real level state), never a stamped-velocity
// parked state: the fixture-phantom trap.
RaidProbe probe_pf(bool fight_in_place, double seconds) {
    AirWorld w;
    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    build_air_world(w, centre);

    drone::DroneParams dp = kScen.drone;
    dp.maverick.enabled = false;
    dp.raid_fight_in_place = fight_in_place;

    const glm::dvec3 pump_dir = centre;
    const glm::dvec3 pump = pump_dir * (w.hf.radius_at(pump_dir) + 10.0);

    glm::dvec3 ref{0.0, 1.0, 0.0};
    const glm::dvec3 east = glm::normalize(glm::cross(ref, pump_dir));
    const glm::dvec3 north = glm::cross(pump_dir, east);

    // The raider: 2.2 km out, 400 m up, nose on the pump.
    const double ang = 2200.0 / kAp.R;
    const glm::dvec3 rdir =
        glm::normalize(std::cos(ang) * pump_dir + std::sin(ang) * east);
    const glm::dvec3 rpos = rdir * (w.hf.radius_at(rdir) + 400.0);
    drone::DroneState d;
    d.curr = drone::level_state_at(dp, rpos, glm::normalize(pump - rpos));
    d.prev = d.curr;
    d.hp = dp.hp;
    d.engaged = true;
    d.foe = drone::kFoePlayer;
    d.raid.active = true;
    d.raid.target_pos = pump;
    d.raid.pump_idx = 0;

    // The PLAYER: parked ON the raider's inbound line, 700 m short of the
    // pump — the defender who showed up, which is the whole point of the raid
    // pause this rung overrides. He is STEPPED every tick (never a
    // stamped-velocity parked state: the fixture-phantom trap) and flown by a
    // SCRIPTED hard-banked orbit so he stays in the pump's airspace for the
    // whole window instead of flying out of the fixture in 20 s.
    const double kOrbitM = 700.0;
    const double pang = kOrbitM / kAp.R;
    const glm::dvec3 pdir =
        glm::normalize(std::cos(pang) * pump_dir + std::sin(pang) * north);
    const glm::dvec3 ppos = pdir * (w.hf.radius_at(pdir) + 350.0);
    // Tangential, so the commanded bank below closes a circle AROUND the pump.
    sim::SimState pl = drone::level_state_at(
        dp, ppos, glm::normalize(glm::cross(pdir, glm::cross(pump_dir, pdir))));

    combat::CombatWorld cw;
    cw.enemy_pool.resize(64);
    cw.setup.difficulty = 5;  // ace: the shipped fly difficulty

    RaidProbe out;
    const combat::RaidParams rp = combat::strike_params();
    const long long ticks = std::llround(seconds / kAp.sim_dt);
    std::vector<drone::DroneState> fleet;
    for (long long t = 0; t < ticks; ++t) {
        // THE SCRIPTED PLAYER: a DEFENDER. He flies at the raider with the
        // same steering law the AI uses, held by the same stabilizing
        // autopilot and autothrottle -- a real maneuvering aircraft, STEPPED
        // every tick (never a stamped-velocity parked state: the
        // fixture-phantom trap). Defending by showing up is exactly the
        // player behaviour the raid pause was built around, so it is the
        // right fixture for the ruling that replaces it.
        // He holds a tight banked ORBIT over the pump he is defending: the
        // hardest level turn the airframe flies, so he stays in its airspace
        // for the whole window rather than flying out of the fixture.
        const glm::dvec3 pu = glm::normalize(pl.position);
        const glm::dvec3 to_pump = pump - pl.position;
        const glm::dvec3 lat = to_pump - pu * glm::dot(to_pump, pu);
        const glm::dvec3 pnose = pl.orientation * glm::dvec3{0.0, 0.0, -1.0};
        // Bank toward whichever side the pump lies on, so the circle closes.
        const double sgn =
            glm::dot(glm::cross(pnose, lat), pu) > 0.0 ? -1.0 : 1.0;
        sim::Inputs pin = drone::autopilot(pl, kAp, sgn * dp.pursue_max_bank,
                                           0.0, dp.pursue_bank_gain,
                                           dp.pursue_pitch_gain, true);
        pin.throttle = static_cast<float>(std::clamp(
            0.40 + 0.02 * (dp.speed - glm::length(pl.velocity)), 0.0, 1.0));
        pl = sim::step(pl, pin, kAp, &w.env, kAp.sim_dt);
        drone::tick(d, kAp, dp, &w.env, &pl);
        if (combat::raider_on_station(d.curr, pump, rp))
            out.on_station_s += kAp.sim_dt;
        const double prng2 = glm::length(d.curr.position - pl.position);
        out.min_range_to_player_m =
            std::min(out.min_range_to_player_m, prng2);
        if (prng2 >= dp.fire_range_min && prng2 <= dp.snapshot_range_m)
            ++out.band_ticks;
        if (d.wants_fire) ++out.wants_fire_ticks;
        const double rng = glm::length(d.curr.position - pump);
        out.max_range_from_pump_m = std::max(out.max_range_from_pump_m, rng);
        out.end_range_from_pump_m = rng;
        // Fire the guns exactly the way the app does.
        fleet.clear();
        fleet.push_back(d);
        const double cd_before = d.fire_cooldown;
        combat::enemy_fire_tick(cw, fleet, kAp.sim_dt, kAp.g, kAp.R, nullptr);
        d.fire_cooldown = fleet[0].fire_cooldown;
        if (d.fire_cooldown > cd_before) ++out.rounds_at_player;
    }
    return out;
}

}  // namespace

TEST_CASE("probe P-F: the relentless raider keeps the pump and shoots back") {
    const RaidProbe pause = probe_pf(false, 90.0);
    const RaidProbe press = probe_pf(true, 90.0);
    INFO("P-F  PAUSE(today): on_station=" << pause.on_station_s
         << "s rounds_at_player=" << pause.rounds_at_player
         << " max_range_from_pump=" << pause.max_range_from_pump_m
         << "m end_range=" << pause.end_range_from_pump_m
         << "m min_rng_player=" << pause.min_range_to_player_m
         << "m band_ticks=" << pause.band_ticks
         << " wf_ticks=" << pause.wants_fire_ticks);
    INFO("P-F  PRESS(E6.5):  on_station=" << press.on_station_s
         << "s rounds_at_player=" << press.rounds_at_player
         << " max_range_from_pump=" << press.max_range_from_pump_m
         << "m end_range=" << press.end_range_from_pump_m
         << "m min_rng_player=" << press.min_range_to_player_m
         << "m band_ticks=" << press.band_ticks
         << " wf_ticks=" << press.wants_fire_ticks);

    // (0) THE PREMISE: today the raider ABANDONS the objective the moment the
    // player is inside the merge radius. On-station time is the behavioural
    // proof of pressing the pump (range + nose-on, the SAME envelope the
    // app's DPS site credits a raider on). This IS Chad's "they poke a pump
    // then abandon it".
    //
    // ★ A DELIBERATE RE-PIN (rung E9, 2026-08-22 — recorded in the spec's
    // re-pin ledger, never silent). This clause read `== 0.0` and was TRUE for
    // every table up to E8. E7.4's tracking pitch gain (1.5) damps the pitch
    // limit cycle, so the yielding raider's flight path through the merge is
    // smoother and it now drifts through the on-station envelope incidentally:
    // 0.00 s -> 1.18 s of 90. THE PREMISE IS UNCHANGED and the ruling's margin
    // is intact (pressing: 3.02 s, 2.5x), but an EXACT ZERO was never the
    // claim — "he does not work the pump while the defender is up" is. Pinned
    // as a fraction of the pressing arm so a future feel change moves it
    // without falsifying an absolute that was only ever incidental.
    REQUIRE(pause.on_station_s < 0.5 * press.on_station_s);

    // (1) THE RULING: it keeps working the pump through the merge instead.
    REQUIRE(press.on_station_s > pause.on_station_s);
    REQUIRE(press.on_station_s > 0.0);

    // (2) AND it never trades the objective for distance: the pressing raider
    // stays closer to the pump than the yielding one, which flies the fight
    // wherever the merge takes it.
    REQUIRE(press.max_range_from_pump_m < pause.max_range_from_pump_m);

    // (3) ★ THE MEASURED TENSION, recorded rather than asserted away.
    // Pressing the pump and holding a 5-deg gun solution on a maneuvering
    // defender are in real conflict: with the nose locked on the pump the
    // ballistic solution on the player is almost never available, so this
    // fixture's pressing raider fires far less than the yielding one that
    // simply dogfights. The guns are NOT structurally muted any more (the
    // raid branch used to clear wants_fire outright -- red-team P2-1: no
    // separately named pin exists; THIS case's legs carry the claim); what is
    // scarce here is the geometry, not the permission. This is ONE raider of
    // a ten-pilot wing in the real composition; the rounds-on-the-ace bar is
    // P-A's job, and P-A is what must not regress.
    INFO("P-F rounds: pause=" << pause.rounds_at_player
         << " press=" << press.rounds_at_player
         << " (see the tension note above -- P-A owns the rounds bar)");
}

TEST_CASE("E6.5: fight in place off is bit identical") {
    // Same probe, dial off, run against a raider whose merge is live: the
    // trajectory must be the pre-E6 one exactly. (Proven by construction --
    // the branch condition degenerates to `!fight_hot` -- and measured here.)
    AirWorld w;
    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    build_air_world(w, centre);
    const auto fly = [&](bool dial) {
        drone::DroneParams dp = kScen.drone;
        dp.maverick.enabled = false;
        dp.raid_fight_in_place = dial;
        const glm::dvec3 pump =
            centre * (w.hf.radius_at(centre) + 10.0);
        glm::dvec3 ref{0.0, 1.0, 0.0};
        const glm::dvec3 east = glm::normalize(glm::cross(ref, centre));
        const double ang = 1500.0 / kAp.R;
        const glm::dvec3 rdir =
            glm::normalize(std::cos(ang) * centre + std::sin(ang) * east);
        const glm::dvec3 rpos = rdir * (w.hf.radius_at(rdir) + 400.0);
        drone::DroneState d;
        d.curr = drone::level_state_at(dp, rpos, glm::normalize(pump - rpos));
        d.prev = d.curr;
        d.hp = dp.hp;
        d.raid.active = true;
        d.raid.target_pos = pump;
        // NOT engaged and no foe: fight_hot is structurally false, so BOTH
        // arms must fly the identical raid errand.
        for (int t = 0; t < 1200; ++t)
            drone::tick(d, kAp, dp, &w.env, nullptr);
        return d.curr;
    };
    const sim::SimState off = fly(false);
    const sim::SimState on = fly(true);
    REQUIRE(off.position == on.position);
    REQUIRE(off.velocity == on.velocity);
    REQUIRE(off.orientation == on.orientation);
}

// ---------------------------------------------------------------------------
// E6.6 — THE FINITE REINFORCEMENT POOL + PROBE P-G "pool exhaustion"
// ---------------------------------------------------------------------------

namespace {

struct AlwaysDeploy : combat::ReinforcePolicy {
    int deploys = 0;
    bool deploy(drone::DroneState& d, int spawn_index) override {
        (void)d;
        (void)spawn_index;
        ++deploys;
        return true;
    }
};

}  // namespace

TEST_CASE("E6.6: the pool bounds waves per faction and infinite is the E5 arm") {
    const auto run_pool = [](int pool_n, int cycles) {
        std::vector<drone::DroneState> fleet;
        for (int i = 0; i < combat::kNumMavericks; ++i) {
            drone::DroneState d;
            d.spawn_index = i;
            d.fleet_count = combat::kNumMavericks;
            d.hp = 100.0;
            fleet.push_back(d);
        }
        combat::ConquestState cs;
        combat::ReinforceState rs;
        combat::ReinforceParams rp;
        rp.delay_s = 1.0;
        rp.hp_full = 100.0;
        rp.pool_n = pool_n;
        AlwaysDeploy pol;
        const double dt = 1.0 / 120.0;
        for (int c = 0; c < cycles; ++c) {
            for (drone::DroneState& d : fleet) d.inert = true;
            for (int t = 0; t < 200; ++t)  // > delay_s at 120 Hz
                combat::reinforce_tick(rs, fleet, cs, rp, pol, dt);
        }
        return rs;
    };

    // INFINITE (-1) is rung E5: every wreck comes back, every cycle.
    const combat::ReinforceState inf = run_pool(-1, 3);
    REQUIRE(inf.waves == 3 * combat::kNumMavericks);

    // A pool of 2 per faction stops after 2 revives per faction, no matter how
    // many more times the wing is wiped.
    const combat::ReinforceState pooled = run_pool(2, 5);
    REQUIRE(pooled.used[0] == 2);
    REQUIRE(pooled.used[1] == 2);
    REQUIRE(pooled.waves == 4);

    // 0 = no waves at all (pre-E5), without touching delay_s.
    const combat::ReinforceState none = run_pool(0, 3);
    REQUIRE(none.waves == 0);
}

TEST_CASE("probe P-G: pool exhaustion re-arms victory by wipe") {
    // The chain Chad asked for, end to end: waves are live -> the wipe route
    // is vetoed; the enemy pool runs out -> the veto lifts -> wiping the wing
    // latches VICTORY, with the wipe route itself completely unchanged.
    app::ConquestWorld cq;
    cq.state.player_faction = combat::CQ_VALLEY;
    cq.params.reinforce_delay_s = 75.0;
    cq.params.reinforce_pool_n = 2;
    const int enemy = 1 - cq.state.player_faction;

    // (0) Premise: with the pool intact the enemy wing CAN be refilled, so the
    // veto is on and a wipe cannot crown a win.
    REQUIRE(app::enemy_waves_live(cq));
    cq.state.reinforcements_live = app::enemy_waves_live(cq);
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (combat::is_enemy(i, cq.state.player_faction))
            combat::on_player_kill(cq.state, i, cq.params);
    REQUIRE(cq.state.outcome == combat::Outcome::PLAYING);

    // (1) Spend the enemy pool. The player's own pool is irrelevant to the
    // wipe route, which only ever inspects enemies-of-the-player.
    cq.reinforce.used[enemy] = 2;
    REQUIRE_FALSE(app::enemy_waves_live(cq));
    cq.state.reinforcements_live = app::enemy_waves_live(cq);

    // (2) Re-kill the wing (the roster was restored by the waves in between).
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (combat::is_enemy(i, cq.state.player_faction))
            cq.state.mav_alive[i] = true;
    cq.state.outcome = combat::Outcome::PLAYING;
    for (int i = 0; i < combat::kNumMavericks; ++i)
        if (combat::is_enemy(i, cq.state.player_faction))
            combat::on_player_kill(cq.state, i, cq.params);
    REQUIRE(cq.state.outcome == combat::Outcome::VICTORY);
}

TEST_CASE("E6.6: pool -1 reproduces the E5 reinforcements_live stamp") {
    app::ConquestWorld cq;
    cq.state.player_faction = combat::CQ_VALLEY;
    cq.params.reinforce_pool_n = -1;
    cq.params.reinforce_delay_s = 0.0;
    REQUIRE(app::enemy_waves_live(cq) == false);  // waves off
    cq.params.reinforce_delay_s = 75.0;
    REQUIRE(app::enemy_waves_live(cq) == true);   // waves on, infinite
    // ...and stays true no matter how many have flown.
    cq.reinforce.used[0] = 99;
    cq.reinforce.used[1] = 99;
    REQUIRE(app::enemy_waves_live(cq) == true);
}

TEST_CASE("E6.6 P2-2: a crushed enemy dome lifts the wipe veto, pool or not") {
    // Red-team P2-2, confirmed on Chad's tape 3: dome crushed at 9:37 with a
    // wave still pooled -> every deploy refused forever while the veto stayed
    // true -> he wiped the wing and VICTORY never latched. The veto must mean
    // CAN ACTUALLY DEPLOY: pool remaining + unbreathable air = veto lifted.
    app::ConquestWorld cq;
    cq.state.player_faction = combat::CQ_VALLEY;
    cq.params.reinforce_delay_s = 75.0;
    cq.params.reinforce_pool_n = 4;  // pool NOT spent
    REQUIRE(app::enemy_waves_live(cq) == true);  // healthy dome: veto up
    const int ef = 1 - cq.state.player_faction;
    // Crush the enemy dome the way damage_pump does (scale to the floor):
    // the ceiling drops below the 500 m breathe-in bar deploy itself uses.
    cq.state.radius_scale[ef] = 0.0;
    cq.state.ceiling_scale[ef] = 0.0;
    REQUIRE(app::faction_air_breathable(cq, ef) == false);
    REQUIRE(app::enemy_waves_live(cq) == false);  // veto LIFTED: wipe wins
    // The player's own dome being crushed must NOT lift the enemy veto.
    cq.state.radius_scale[ef] = 1.0;
    cq.state.ceiling_scale[ef] = 1.0;
    cq.state.radius_scale[cq.state.player_faction] = 0.0;
    cq.state.ceiling_scale[cq.state.player_faction] = 0.0;
    REQUIRE(app::enemy_waves_live(cq) == true);
}

// NOTE (E6.5, recorded rather than pinned): a further leg was attempted here
// to separate "shooting" from "pressing" in a single constructed tick, and it
// cannot be built honestly -- the only geometry that offers a pressing raider
// a gun solution is one where the defender sits ON the run-in line, and there
// "nose at the player" and "nose at the pump" are the same attitude, so both
// arms score identically. The behavioural difference E6.5 makes is therefore
// measured where it is real: probe P-F above, on-station seconds and distance
// held from the pump.
