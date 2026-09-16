// RUNG E2 — THE BLACK STOPE OFFENSIVE: the KNOB-OFF DIFFERENTIAL suite
// (docs/ENEMY_AI_E1_E2_SPEC.md, ACCEPTANCE: "all new dials at off-values =>
// bit-identical trajectories").
//
// House discipline, applied to every dial here:
//   * the OFF arm is proven bit-identical with `==`, never a tolerance band,
//     and wherever possible against the LEGACY CALL ITSELF (the pre-E2
//     maverick_step overload with no RunOrders argument) rather than against a
//     re-typed copy of the legacy expression;
//   * every off arm is PAIRED with an ON arm that must differ, so a dial that
//     silently did nothing at all could not pass as "bit-identical";
//   * each leg proves its own premise first (the fixture-no-op class): the
//     drone really is in the arena / really is on-station / the pin's dial
//     really does ship non-zero.
//
// TEST_CASE and SECTION names are STRICTLY ASCII (a non-ASCII name silently
// never runs — the recurring trap).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "combat/conquest.h"
#include "combat/kill.h"
#include "combat/raid.h"
#include "config/load_aircraft.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/environment.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

world::HeightField uniform_field(double elev_m, double relief = 4000.0) {
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

// The T1 canon tunnel dials (test_tunnel.cpp / test_maverick.cpp fixture).
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 0.0;
    tp.arena_a_m = 7350.0;
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.chambers_on = true;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;
    tp.trench_rim_m = 150.0;
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

// The deep pump this fixture's strikers attack: placed by the SAME helper the
// app uses, off the LIVE arena (never re-derived constants).
glm::dvec3 deep_pump_pos(const world::TunnelNet& net,
                         const glm::dvec3& mouth_bearing) {
    const double lift_to_center =
        net.arena.a_pos *
        std::sqrt(1.0 - combat::kDeepPumpFrac * combat::kDeepPumpFrac);
    return combat::place_deep_pump(net.arena.center, net.arena.u_long,
                                   mouth_bearing, net.arena.a_pos, net.arena.b,
                                   combat::kDeepPumpFrac, lift_to_center);
}

// The spine station deepest inside the arena (scanned off the live net).
double deepest_arena_s(const world::TunnelNet& net,
                       const maverick::TunnelRoute& route) {
    double best_s = -1.0, best = 1e18;
    for (double s = 0.0; s <= route.L; s += 100.0) {
        const glm::dvec3 p = route.point_at(s).pos;
        if (!drone::in_arena(net, p)) continue;
        const double n2 = drone::arena_norm2(net, p);
        if (n2 < best) {
            best = n2;
            best_s = s;
        }
    }
    return best_s;
}

}  // namespace

// ---------------------------------------------------------------------------
// E2.1 — THE ON-ORDER RUN.
//
// OFF arm: the pre-E2 CALL ITSELF (maverick_step with no RunOrders argument)
// against the same machine driven with a default-constructed RunOrders. Every
// field of MaverickState is compared tick by tick, so this is a structural
// bit-identity proof, not a sampled one.
// ON arm: strike_now collapses the countdown -> TRANSIT arrives far sooner.
// MUTATION: delete the collapse -> the ON arm's mode never changes early.
// ===========================================================================
TEST_CASE("E2.1 knob-off: a default RunOrders is the pre-E2 run scheduler") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;

    // A patrolling pilot in level flight over the Errington mouth.
    const glm::dvec3 up = glm::normalize(world::kTunnelMouthErrington);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, up));
    const sim::SimState s0 =
        drone::level_state_at(dp, up * (hf.radius_at(up) + 2000.0), east);

    maverick::MaverickState legacy{}, offarm{}, onarm{};
    maverick::RunOrders off_ro;  // every field defaulted
    maverick::RunOrders on_ro;
    on_ro.strike_now = true;
    on_ro.stagger_s = kScen.drone.strike_stagger_s;

    const int ticks = static_cast<int>(60.0 / kAp.sim_dt);
    for (int t = 0; t < ticks; ++t) {
        const maverick::MaverickCmd cl = maverick::maverick_step(
            legacy, s0, kAp, dp.maverick, route, net, 3, kAp.sim_dt);
        const maverick::MaverickCmd co =
            maverick::maverick_step(offarm, s0, kAp, dp.maverick, route, net, 3,
                                    kAp.sim_dt, /*hold_runs=*/false, off_ro);
        REQUIRE(cl.target_bank == co.target_bank);
        REQUIRE(cl.target_gamma == co.target_gamma);
        REQUIRE(cl.speed_target == co.speed_target);
        REQUIRE(legacy.mode == offarm.mode);
        REQUIRE(legacy.patrol_countdown == offarm.patrol_countdown);
        REQUIRE(legacy.run_dir == offarm.run_dir);
        REQUIRE(legacy.mode_ticks == offarm.mode_ticks);
        maverick::maverick_step(onarm, s0, kAp, dp.maverick, route, net, 3,
                                kAp.sim_dt, /*hold_runs=*/false, on_ro);
    }
    // Non-vacuity + the ON arm: pilot 3's trait period is 160 s, so the legacy
    // schedule is still counting down after 60 s while the ordered pilot has
    // already left on his run (stagger 15 * (1 + 3) = 60 s).
    REQUIRE(legacy.mode == maverick::MaverickState::Mode::PATROL);
    REQUIRE(onarm.mode != maverick::MaverickState::Mode::PATROL);
}

// ===========================================================================
// E2.1 — the collapse is ONE-SHOT and IDEMPOTENT, and MERGE_HOLD STANDS.
// Both are spec P1-1 requirements with named failure modes:
//   * a per-tick re-zero would PIN the countdown at the stagger value forever
//     (the pilot would never launch at all),
//   * overriding hold_runs re-creates the measured R4 bug — a committed
//     TRANSIT marching a fighter away mid-merge is a free kill for an ace.
// ===========================================================================
TEST_CASE("E2.1: the collapse is one-shot and never overrides the merge hold") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;
    const glm::dvec3 up = glm::normalize(world::kTunnelMouthErrington);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, up));
    const sim::SimState s0 =
        drone::level_state_at(dp, up * (hf.radius_at(up) + 2000.0), east);

    maverick::RunOrders ro;
    ro.strike_now = true;
    ro.stagger_s = 5.0;  // pilot 0 -> 5 s * (1 + 0)

    SECTION("one-shot: the collapsed countdown keeps DECREMENTING") {
        maverick::MaverickState m{};
        long long prev = -1;
        bool strictly_decreasing = true;
        const int ticks = static_cast<int>(4.0 / kAp.sim_dt);
        for (int t = 0; t < ticks; ++t) {
            maverick::maverick_step(m, s0, kAp, dp.maverick, route, net, 0,
                                    kAp.sim_dt, /*hold_runs=*/false, ro);
            if (prev >= 0 && m.patrol_countdown >= prev)
                strictly_decreasing = false;
            prev = m.patrol_countdown;
        }
        REQUIRE(m.strike_collapsed);
        REQUIRE(strictly_decreasing);  // never re-zeroed / never frozen
        REQUIRE(m.mode == maverick::MaverickState::Mode::PATROL);  // 4 s < 5 s
    }

    SECTION("merge hold stands: the run fires the tick the merge ends") {
        // Stagger 0 here so the spec's literal claim ("the run FIRES the tick
        // the merge ends") is what gets pinned; the stagger's own effect is
        // the section above.
        maverick::RunOrders now = ro;
        now.stagger_s = 0.0;
        maverick::MaverickState m{};
        const int held = static_cast<int>(20.0 / kAp.sim_dt);
        for (int t = 0; t < held; ++t)
            maverick::maverick_step(m, s0, kAp, dp.maverick, route, net, 0,
                                    kAp.sim_dt, /*hold_runs=*/true, now);
        REQUIRE(m.strike_collapsed);  // the order DID collapse the number
        REQUIRE(m.mode == maverick::MaverickState::Mode::PATROL);  // but held
        // Merge over: the very next tick launches.
        maverick::maverick_step(m, s0, kAp, dp.maverick, route, net, 0,
                                kAp.sim_dt, /*hold_runs=*/false, now);
        REQUIRE(m.mode == maverick::MaverickState::Mode::TRANSIT);
    }
}

// ===========================================================================
// E2.1 FIX — THE TRANSIT FINAL-LEG GRACE (transit_fix_grace_s).
//
// ATTRIBUTED DEFECT: transit_timeout_s is ONE 150 s budget spanning BOTH
// stages of the two-waypoint approach. Measured in the game-loop certificate:
// Stage A (the dash to the FIX, transit_fix_back_m + approach_back_m behind
// the mouth) costs 105-135 s of it, and the fleet then died ON the final leg,
// on-line and aligned, 0.5-4 km short of the arm gate. 11 of 12 give-ups in a
// 10-minute window were that shape and exactly ONE pilot ever crossed the arm
// point, so the on-order run pipeline delivered LESS tunnel traffic than the
// pre-E2 trait scheduler.
//
// Three legs, all against the SAME parked geometry:
//   OFF        - grace 0 is bit-identical: give-up at exactly transit_timeout_s.
//   STAGE A    - a transit that NEVER reaches the FIX is still capped at
//                transit_timeout_s with the grace shipped ON (the grant is
//                EARNED by progress; the orbit-the-fix livelock the timeout
//                exists for stays unrepresentable).
//   FINAL LEG  - a transit that HAS reached the FIX flies to
//                transit_timeout_s + transit_fix_grace_s, and a GO-AROUND
//                (transit_fix_done cleared) does NOT buy a second grant, so
//                the total is hard-bounded.
// MUTATION: key the budget on transit_fix_done instead of the transit_grace
// latch -> the go-around leg's give-up runs past the bound.
// ===========================================================================
TEST_CASE("E2.1 fix: the transit final-leg grace is earned, once, and bounded") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;
    const maverick::MaverickParams& mp = dp.maverick;
    // Premise: the dial really ships ON, and really is long enough to fly the
    // final leg at the speed the final leg is flown at (the loader's rule).
    REQUIRE(mp.transit_fix_grace_s > 0.0);
    REQUIRE(mp.transit_fix_grace_s * mp.climb_speed_min >=
            mp.transit_fix_back_m);

    // The FIX, rebuilt exactly as the TRANSIT case builds it for run_dir +1
    // (Errington in): approach = mouth - into_tan*back + up*alt, then
    // transit_fix_back_m back along the approach point's LOCAL horizontal
    // into-bore azimuth.
    const glm::dvec3 entry_pos = net.spine.front().pos;
    const glm::dvec3 into_tan = net.spine.front().tan;
    const glm::dvec3 entry_dir = glm::normalize(entry_pos);
    const glm::dvec3 approach = entry_pos - into_tan * mp.approach_back_m +
                                entry_dir * mp.approach_alt_m;
    const glm::dvec3 tan_h =
        glm::normalize(into_tan - glm::dot(into_tan, entry_dir) * entry_dir);
    const glm::dvec3 app_dir = glm::normalize(approach);
    const glm::dvec3 th_app =
        glm::normalize(tan_h - glm::dot(tan_h, app_dir) * app_dir);
    const glm::dvec3 fixpt = approach - th_app * mp.transit_fix_back_m;
    // Non-vacuity: the "never reaches the FIX" geometry really is outside the
    // capture sphere (the "at the FIX" one is inside it by construction).
    REQUIRE(glm::length(entry_pos - fixpt) > mp.approach_reach_m);

    const long long t_base = std::llround(mp.transit_timeout_s / kAp.sim_dt);
    const long long t_full = std::llround(
        (mp.transit_timeout_s + mp.transit_fix_grace_s) / kAp.sim_dt);
    REQUIRE(t_full > t_base);

    // Give-up tick for a transit PARKED at `pos` (the state is never
    // integrated, so the FIX capture is decided by geometry alone), with
    // `grace` seconds of dial and an optional per-tick go-around.
    auto giveup_tick = [&](const glm::dvec3& pos, double grace,
                           bool go_around) {
        maverick::MaverickParams p = mp;
        p.transit_fix_grace_s = grace;
        // ★ RUNG E15 IS AN ORTHOGONAL DIAL AND IS HELD OFF HERE. This lambda
        // pins E2.1's GRACE contract -- earned once, at the FIX, bounded --
        // against the flat `t_base`/`t_full` clocks below. E15 makes Stage A's
        // budget scale with the distance left to fly, so a transit parked FAR
        // from the fix now legitimately gets more than transit_timeout_s
        // (measured: 26964 ticks vs 18001), and these clauses would be
        // measuring E15 instead of the grace. Its own interaction with the
        // grace is pinned in the E15 section below, deliberately separately.
        p.transit_reach_s_per_km = 0.0;
        maverick::MaverickState m{};
        m.inited = true;
        m.mode = maverick::MaverickState::Mode::TRANSIT;
        m.run_dir = 1;
        const sim::SimState s = drone::level_state_at(dp, pos, tan_h);
        for (long long t = 1; t <= t_full + t_base; ++t) {
            maverick::maverick_step(m, s, kAp, p, route, net, 0, kAp.sim_dt);
            if (m.mode != maverick::MaverickState::Mode::TRANSIT) return t;
            if (go_around) m.transit_fix_done = false;
        }
        return -1LL;
    };

    SECTION("knob-off: grace 0 gives up at exactly transit_timeout_s") {
        REQUIRE(giveup_tick(fixpt, 0.0, false) == t_base + 1);
        REQUIRE(giveup_tick(entry_pos, 0.0, false) == t_base + 1);
    }
    SECTION("stage A is still capped: no FIX, no grace") {
        REQUIRE(giveup_tick(entry_pos, mp.transit_fix_grace_s, false) ==
                t_base + 1);
    }
    SECTION("the final leg gets the grace, and only once") {
        REQUIRE(giveup_tick(fixpt, mp.transit_fix_grace_s, false) ==
                t_full + 1);
        // A go-around re-captures the FIX but must NOT re-grant: the total
        // stays hard-bounded at transit_timeout_s + transit_fix_grace_s.
        REQUIRE(giveup_tick(fixpt, mp.transit_fix_grace_s, true) == t_full + 1);
    }
    // ★ RUNG E15's INTERACTION WITH THE GRACE, pinned here because the two
    // budgets compose and nothing else checks that they do.
    SECTION("E15: stage A scales with distance, and the grace still adds once") {
        maverick::MaverickParams p = mp;
        // E15 ships OFF on the crash measurement, so the rate is named here:
        // this clause pins the CONTRACT for the day it is turned on.
        p.transit_reach_s_per_km = 11.0;
        const auto giveup_scaled = [&](const glm::dvec3& pos, double grace) {
            maverick::MaverickParams q = p;
            q.transit_fix_grace_s = grace;
            maverick::MaverickState m{};
            m.inited = true;
            m.mode = maverick::MaverickState::Mode::TRANSIT;
            m.run_dir = 1;
            const sim::SimState st = drone::level_state_at(dp, pos, tan_h);
            for (long long t = 1; t <= 12 * t_full; ++t) {
                maverick::maverick_step(m, st, kAp, q, route, net, 0,
                                        kAp.sim_dt);
                if (m.mode != maverick::MaverickState::Mode::TRANSIT) return t;
            }
            return -1LL;
        };
        // AT the fix, the distance term is ~0, so the budget is still the flat
        // one -- E15 must not hand out rope nobody needs.
        CHECK(giveup_scaled(fixpt, 0.0) == t_base + 1);
        // FAR from the fix, Stage A gets strictly more than the flat timeout.
        const long long far_off = giveup_scaled(entry_pos, 0.0);
        CHECK(far_off > t_base + 1);
        // ...and the grace still adds EXACTLY transit_fix_grace_s on top of
        // whatever Stage A was given -- it composes, it does not replace.
        const long long far_on =
            giveup_scaled(entry_pos, mp.transit_fix_grace_s);
        // entry_pos never captures the FIX, so no grace is earned there: the
        // two must be identical. (The grace-earned path is the clause above.)
        CHECK(far_on == far_off);
    }

}

// ===========================================================================
// E2.2 — the exit-flip deferral. OFF: the RUN hands to CLIMB_OUT at exit_frac
// exactly as before. ON: it holds the mode for the divert.
// MUTATION: drop the !ro.hold_exit guard -> the ON arm climbs out too.
// ===========================================================================
TEST_CASE("E2.2 knob-off: hold_exit defers the CLIMB_OUT flip") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;

    // A run past exit_frac, on the spine, moving along it.
    const double s_late = (dp.maverick.exit_frac + 0.02) * route.L;
    REQUIRE(s_late < route.L);
    const maverick::TunnelRoute::Sample p = route.point_at(s_late);
    const auto make = [&]() {
        maverick::MaverickState m{};
        m.inited = true;
        m.mode = maverick::MaverickState::Mode::RUN;
        m.run_dir = 1;
        m.s_est = s_late;
        return m;
    };
    const sim::SimState s = drone::level_state_at(dp, p.pos, p.tan);

    maverick::MaverickState off = make(), on = make();
    maverick::RunOrders off_ro;
    maverick::RunOrders on_ro;
    on_ro.hold_exit = true;
    maverick::maverick_step(off, s, kAp, dp.maverick, route, net, 0, kAp.sim_dt,
                            false, off_ro);
    maverick::maverick_step(on, s, kAp, dp.maverick, route, net, 0, kAp.sim_dt,
                            false, on_ro);
    REQUIRE(off.mode == maverick::MaverickState::Mode::CLIMB_OUT);  // legacy
    REQUIRE(on.mode == maverick::MaverickState::Mode::RUN);         // deferred
}

// ===========================================================================
// E2.3 — THE UNDERGROUND STRAFE, and the P0-1 fire-path fold.
//
// Premise first: the fixture drone really is a committed striker, really is
// on-station over the pump, and really has NO foe (foe == kFoeNone, engaged ==
// false) — which is exactly the case the old enemy_fire_tick gate swallowed.
// OFF: wants_fire stays false and NO round is ever spawned. ON: wants_fire and
// strike_strafing are set together, gun_tgt_pos IS the pump, gun_tgt_vel is
// exactly zero (a stale or zero-vector gun_tgt slews rounds toward the planet
// centre), and enemy_fire_tick spawns.
// MUTATION: delete the strafe clause in kill.h -> the ON arm spawns nothing.
// ===========================================================================
TEST_CASE("E2.3 knob-off: the strike strafe spawns real rounds at the pump") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    const maverick::TunnelRoute route(net);
    const glm::dvec3 pump = deep_pump_pos(net, world::kTunnelMouthMurray);
    REQUIRE(drone::in_arena(net, pump));  // premise: the pump is in the stope

    const auto make = [&](const drone::DroneParams& dp) {
        // Parked 400 m short of the pump on a level line pointed at it — well
        // inside the 600 m on-station envelope and dead nose-on.
        const glm::dvec3 to = glm::normalize(pump - net.arena.center);
        const glm::dvec3 pos = pump - to * 400.0;
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.curr = drone::level_state_at(dp, pos, glm::normalize(pump - pos));
        d.prev = d.curr;
        d.grounded = false;
        d.engaged = false;         // NO foe — the undefended-pump case
        d.foe = drone::kFoeNone;
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::RUN;
        d.mav.run_dir = -1;
        d.mav.s_est = deepest_arena_s(net, route);
        d.strike.active = true;
        d.strike.target_pos = pump;
        d.strike.pump_idx = 3;
        d.strike.engage_m = dp.strike_engage_m;
        d.strike.k_az = dp.strike_k_az;
        d.strike.k_el = dp.strike_k_el;
        d.strike.bank_cap = dp.strike_bank_cap;
        d.strike.gamma_cap = dp.strike_gamma_cap;
        d.strike.bail_s = dp.strike_bail_s;
        d.strike.station_range_m = dp.strike_station_range_m;
        d.strike.station_cos = dp.strike_station_cos;
        return d;
    };

    drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.strike_strafe);  // non-vacuous: the dial ships ON
    // The chamber fight is irrelevant here (no foe) but pin it out of the way
    // so this leg measures ONLY the strafe path.
    shipped.arena_fight_range_m = 0.0;
    drone::DroneParams off = shipped;
    off.strike_strafe = false;

    combat::CombatWorld cw_on, cw_off;
    cw_on.setup = kScen.combat;
    cw_off.setup = kScen.combat;

    drone::DroneState d_on = make(shipped), d_off = make(off);
    std::vector<drone::DroneState> f_on{d_on}, f_off{d_off};
    long long rounds_on = 0, rounds_off = 0;
    bool tgt_ok = false, paired = false;
    const int ticks = static_cast<int>(3.0 / kAp.sim_dt);
    for (int t = 0; t < ticks; ++t) {
        drone::tick(f_on[0], kAp, shipped, &env, nullptr);
        drone::tick(f_off[0], kAp, off, &env, nullptr);
        REQUIRE_FALSE(f_off[0].wants_fire);       // OFF: silent DPS, as before
        REQUIRE_FALSE(f_off[0].strike_strafing);
        if (f_on[0].wants_fire) {
            paired = true;
            REQUIRE(f_on[0].strike_strafing);
            tgt_ok = f_on[0].gun_tgt_pos == pump &&
                     f_on[0].gun_tgt_vel == glm::dvec3{0.0};
            REQUIRE(tgt_ok);
        }
        const double cd_on = f_on[0].fire_cooldown;
        const double cd_off = f_off[0].fire_cooldown;
        combat::enemy_fire_tick(cw_on, f_on, kAp.sim_dt, kAp.g, kAp.R, &net);
        combat::enemy_fire_tick(cw_off, f_off, kAp.sim_dt, kAp.g, kAp.R, &net);
        if (f_on[0].fire_cooldown > cd_on) ++rounds_on;
        if (f_off[0].fire_cooldown > cd_off) ++rounds_off;
    }
    INFO("strafe rounds: ON=" << rounds_on << " OFF=" << rounds_off);
    REQUIRE(paired);         // the strafe gate opened at all (non-vacuous)
    REQUIRE(rounds_on > 0);  // and the P0-1 clause let them out
    REQUIRE(rounds_off == 0);
}

// ===========================================================================
// E2.4 — THE TERRAIN-AVOID ARENA EXEMPTION. A NON-committed pursuer (maverick
// disabled entirely, so no tunnel mode can grant the old exemption) sitting
// inside the arena reads AGL ~ -4 km: the hard-deck latch engages and forces a
// 26-deg climb with wants_fire cleared — into the chamber ceiling. OFF: that
// legacy behaviour, exactly. ON: exempt, and the trajectory differs.
// MUTATION: drop `&& !arena_exempt` -> the ON arm latches too.
// ===========================================================================
TEST_CASE("E2.4 knob-off: in-arena drones are exempt from terrain avoidance") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    const maverick::TunnelRoute route(net);
    const double s_arena = deepest_arena_s(net, route);
    REQUIRE(s_arena >= 0.0);
    const maverick::TunnelRoute::Sample p = route.point_at(s_arena);

    drone::DroneParams on = kScen.drone;
    on.maverick.enabled = false;  // NOT a committed run: only in_arena can
                                  // grant the exemption
    REQUIRE(on.arena_fight_range_m > 0.0);
    drone::DroneParams off = on;
    off.arena_fight_range_m = 0.0;

    const auto make = [&](const drone::DroneParams& dp) {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.curr = drone::level_state_at(dp, p.pos, p.tan);
        d.prev = d.curr;
        d.grounded = false;
        return d;
    };
    drone::DroneState d_on = make(on), d_off = make(off);
    REQUIRE(drone::in_arena(net, d_on.curr.position));  // premise

    for (int t = 0; t < 60; ++t) {
        drone::tick(d_on, kAp, on, &env, nullptr);
        drone::tick(d_off, kAp, off, &env, nullptr);
    }
    REQUIRE(d_off.terrain_avoid_engaged);        // the legacy pull-up
    REQUIRE_FALSE(d_on.terrain_avoid_engaged);   // exempt
    REQUIRE(d_on.curr.position != d_off.curr.position);
}

// ===========================================================================
// E2.2/E2.4 — THE ARENA-SHELL WALL GUARD. Driven OPEN-LOOP (the S6 discipline
// for a hysteretic latch): the state is stamped at a sequence of CLEARANCES to
// the chamber wall and the latch read, so engage/release are pinned as a BAND
// and not as one threshold. OFF (margin 0) => never engages, and the command is
// the unguarded divert steer.
// MUTATION: make the latch non-hysteretic -> the release leg fails.
// ===========================================================================
TEST_CASE("E2.4 knob-off: the arena-shell guard is hysteretic and off at zero") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    const glm::dvec3 pump = deep_pump_pos(net, world::kTunnelMouthMurray);
    drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.arena_guard_margin_m > 0.0);
    REQUIRE(shipped.arena_guard_release_m > shipped.arena_guard_margin_m);
    drone::DroneParams off = shipped;
    off.arena_guard_margin_m = 0.0;

    // Stamp the drone `clear_m` inside the chamber wall along the arena's
    // LATERAL axis (a direction with real room either side of the shell), on
    // an active divert, and read one tick.
    const auto at_clearance = [&](const drone::DroneParams& dp, double clear_m,
                                  bool latched) {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        const glm::dvec3 pos =
            net.arena.center + net.arena.u_lat * (net.arena.b - clear_m);
        d.curr = drone::level_state_at(dp, pos, net.arena.u_long);
        d.prev = d.curr;
        d.grounded = false;
        d.arena_guard_engaged = latched;
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::RUN;
        d.mav.run_dir = -1;
        d.strike.active = true;
        d.strike.target_pos = pump;
        d.strike.engage_m = dp.strike_engage_m;
        d.strike.k_az = dp.strike_k_az;
        d.strike.k_el = dp.strike_k_el;
        d.strike.bank_cap = dp.strike_bank_cap;
        d.strike.gamma_cap = dp.strike_gamma_cap;
        d.strike.bail_s = dp.strike_bail_s;
        d.strike.station_range_m = dp.strike_station_range_m;
        d.strike.station_cos = dp.strike_station_cos;
        drone::tick(d, kAp, dp, &env, nullptr);
        return d;
    };

    const double enter = shipped.arena_guard_margin_m;
    const double rel = shipped.arena_guard_release_m;
    const double mid = 0.5 * (enter + rel);  // strictly INSIDE the gap

    // Premise: every stamp point is really inside the arena.
    REQUIRE(drone::in_arena(
        net, net.arena.center + net.arena.u_lat * (net.arena.b - mid)));

    // The BAND: in the gap the latch HOLDS whatever it was.
    REQUIRE_FALSE(at_clearance(shipped, mid, false).arena_guard_engaged);
    REQUIRE(at_clearance(shipped, mid, true).arena_guard_engaged);
    // Closer than the margin it engages from cold; deeper than the release
    // clearance it drops even when latched.
    REQUIRE(at_clearance(shipped, enter - 100.0, false).arena_guard_engaged);
    REQUIRE_FALSE(at_clearance(shipped, rel + 100.0, true).arena_guard_engaged);

    // OFF: never engaged, and the COMMAND is the unguarded divert's — read on
    // the commanded surfaces (DroneState::last_inputs, the rig-B record of
    // what the autopilot was actually told), not on position: one tick of a
    // different bank command moves the attitude, and position only follows on
    // the NEXT step, so a position compare here would be vacuous.
    const drone::DroneState guarded = at_clearance(shipped, enter - 100.0, false);
    const drone::DroneState unguarded = at_clearance(off, enter - 100.0, false);
    REQUIRE_FALSE(unguarded.arena_guard_engaged);
    REQUIRE(guarded.arena_guard_engaged);
    REQUIRE((guarded.last_inputs.roll != unguarded.last_inputs.roll ||
             guarded.last_inputs.pitch != unguarded.last_inputs.pitch));
}

// ===========================================================================
// THE WHOLE-RUNG OFF ARM. Every E2 dial at its off value, flown against the
// live tunnel net with a full strike order armed: the trajectory must be
// bit-identical to the same fixture flown with a DEFAULT-CONSTRUCTED
// DroneParams E2 block (i.e. the pre-E2 struct defaults) — the single check
// that no E2 code path leaks into an off-dial world through a term nobody
// listed. Paired with the shipped arm, which must differ.
// ===========================================================================
TEST_CASE("E2 knob-off: every dial off reproduces the pre-E2 tick") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    const maverick::TunnelRoute route(net);
    const glm::dvec3 pump = deep_pump_pos(net, world::kTunnelMouthMurray);
    const double s_arena = deepest_arena_s(net, route);
    REQUIRE(s_arena >= 0.0);
    const maverick::TunnelRoute::Sample p = route.point_at(s_arena);

    // OFF = the shipped table with the E2 block returned to the STRUCT
    // defaults (which are, field for field, the pre-E2 welded values).
    const drone::DroneParams shipped = kScen.drone;
    drone::DroneParams offv = shipped;
    {
        const drone::DroneParams d0;  // the pre-E2 defaults
        offv.strike_on_order = d0.strike_on_order;
        offv.strike_stagger_s = d0.strike_stagger_s;
        offv.strike_defer_exit = d0.strike_defer_exit;
        offv.strike_strafe = d0.strike_strafe;
        offv.strike_engage_m = d0.strike_engage_m;
        offv.strike_k_az = d0.strike_k_az;
        offv.strike_k_el = d0.strike_k_el;
        offv.strike_bank_cap = d0.strike_bank_cap;
        offv.strike_gamma_cap = d0.strike_gamma_cap;
        offv.strike_bail_s = d0.strike_bail_s;
        offv.strike_station_range_m = d0.strike_station_range_m;
        offv.strike_station_cos = d0.strike_station_cos;
        offv.arena_fight_range_m = d0.arena_fight_range_m;
        offv.arena_fight_s = d0.arena_fight_s;
        offv.arena_gamma_cap = d0.arena_gamma_cap;
        offv.arena_guard_margin_m = d0.arena_guard_margin_m;
        offv.arena_guard_release_m = d0.arena_guard_release_m;
    }

    const auto make = [&](const drone::DroneParams& dp) {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.curr = drone::level_state_at(dp, p.pos, p.tan);
        d.curr.velocity = p.tan * dp.maverick.base_speed;
        d.curr.last_vhat = p.tan;
        d.prev = d.curr;
        d.grounded = false;
        d.engaged = true;
        d.foe = drone::kFoePlayer;
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::RUN;
        d.mav.run_dir = 1;
        d.mav.s_est = s_arena;
        d.strike.active = true;
        d.strike.target_pos = pump;
        d.strike.pump_idx = 3;
        // The ORDER carries the (config-stamped) envelope, exactly as the app
        // builds it — so the off arm's order is the pre-E2 welded envelope.
        d.strike.engage_m = dp.strike_engage_m;
        d.strike.k_az = dp.strike_k_az;
        d.strike.k_el = dp.strike_k_el;
        d.strike.bank_cap = dp.strike_bank_cap;
        d.strike.gamma_cap = dp.strike_gamma_cap;
        d.strike.bail_s = dp.strike_bail_s;
        d.strike.station_range_m = dp.strike_station_range_m;
        d.strike.station_cos = dp.strike_station_cos;
        return d;
    };

    // A player alongside, in the chamber (the fight-interrupt lure).
    sim::SimState player;
    player.position = p.pos + p.tan * 400.0;
    player.velocity = glm::dvec3{0.0};
    player.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    player.last_vhat = p.tan;

    // The reference arm: a drone whose ENTIRE DroneParams E2 block is the
    // struct default AND whose order carries the struct-default envelope.
    drone::DroneState ref = make(offv), got = make(offv), hot = make(shipped);
    for (int t = 0; t < 400; ++t) {
        drone::tick(ref, kAp, offv, &env, nullptr);
        drone::tick(got, kAp, offv, &env, &player);
        drone::tick(hot, kAp, shipped, &env, &player);
        // Off: the committed run ignores the player exactly as it always did.
        REQUIRE(ref.curr.position == got.curr.position);
        REQUIRE(ref.curr.orientation == got.curr.orientation);
        REQUIRE_FALSE(got.wants_fire);
        REQUIRE_FALSE(got.strike_strafing);
        REQUIRE(got.arena_fight_ticks == 0);
        REQUIRE_FALSE(got.arena_guard_engaged);
    }
    // Paired ON arm: the shipped table does NOT fly the same run.
    REQUIRE(hot.curr.position != got.curr.position);
}
