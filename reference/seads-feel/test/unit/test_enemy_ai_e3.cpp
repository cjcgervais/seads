// RUNG E3 — THE FELT PASS: the KNOB-OFF DIFFERENTIAL suite
// (docs/ENEMY_AI_E1_E2_SPEC.md "RUNG E3", from Chad's fly tape
// build-play/conquest_tape_1).
//
// CHAD'S VERDICT on the E2 build: "they behaved the same as always, didn't
// engage." The tape attribution: E2.1's on-order collapse put the three
// non-raid ENEMY strikers into committed TRANSIT for 53-80% of their lives
// toward a pump 20+ km away, and a committed TRANSIT ignores the player — so
// they flew straight lines past an ace and were executed at 15-706 m, all
// three dead by 3:34 (permanent, in conquest). E2.1 made the SURFACE felt
// problem WORSE.
//
// House discipline, applied to every dial here:
//   * the OFF arm is proven bit-identical with `==`, never a tolerance band,
//     and against the LEGACY CALL ITSELF (the maverick_step overload with no
//     RunOrders argument) wherever the seam allows it;
//   * every off arm is PAIRED with an ON arm that must differ, so a dial that
//     silently did nothing could not pass as "bit-identical";
//   * each leg proves its own premise first (the fixture-no-op class).
//
// TEST_CASE and SECTION names are STRICTLY ASCII (a non-ASCII name silently
// never runs — the recurring trap).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/instructor_tick.h"  // S2: uncovered_arc_m, raider_rank
#include "combat/raid.h"
#include "config/load_aircraft.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/state.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/faction_bubbles.h"
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

// The T1 canon tunnel dials (the test_tunnel.cpp / test_maverick.cpp fixture).
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

// Every field of MaverickState, compared with `==`. A structural bit-identity
// proof rather than a sampled one — a new field added to the machine without a
// thought for the off arm shows up HERE.
bool same_state(const maverick::MaverickState& a,
                const maverick::MaverickState& b) {
    return a.mode == b.mode && a.inited == b.inited &&
           a.mode_ticks == b.mode_ticks &&
           a.patrol_countdown == b.patrol_countdown && a.s_est == b.s_est &&
           a.run_dir == b.run_dir && a.wall_guard == b.wall_guard &&
           a.climb_free == b.climb_free &&
           a.transit_fix_done == b.transit_fix_done &&
           a.transit_grace == b.transit_grace &&
           a.strike_collapsed == b.strike_collapsed &&
           a.order_countdown == b.order_countdown &&
           a.run_on_order == b.run_on_order;
}

bool same_cmd(const maverick::MaverickCmd& a, const maverick::MaverickCmd& b) {
    return a.target_bank == b.target_bank && a.target_gamma == b.target_gamma &&
           a.bank_p == b.bank_p && a.level_p == b.level_p &&
           a.aoa_protect == b.aoa_protect && a.speed_target == b.speed_target &&
           a.throttle_ff == b.throttle_ff &&
           a.pursue_player == b.pursue_player;
}

}  // namespace

// ===========================================================================
// E3.1 — THE TRANSIT FIGHT-YIELD.
//
// OFF arm: the pre-E3 CALL ITSELF (maverick_step with no RunOrders argument)
// against the same machine driven with a default-constructed RunOrders, over a
// live TRANSIT. Every MaverickState field and every command field is compared
// tick by tick.
// ON arm: fight_yield aborts the transit on the very first tick, and hands the
// tick to pursue() (pursue_player) instead of to the mode machine's all-zero
// default command.
// MUTATION: delete the `if (ro.fight_yield)` block -> the ON arm stays in
// TRANSIT and the abort leg fails.
// ===========================================================================
TEST_CASE("E3.1 knob-off: a default RunOrders leaves the transit committed") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;
    const maverick::MaverickParams& mp = dp.maverick;

    // A transit on the entry line, run_dir +1 (Errington in) — the geometry
    // the E2.1-fix leg uses, rebuilt exactly as the TRANSIT case builds it.
    const glm::dvec3 entry_pos = net.spine.front().pos;
    const glm::dvec3 into_tan = net.spine.front().tan;
    const glm::dvec3 entry_dir = glm::normalize(entry_pos);
    const glm::dvec3 approach =
        entry_pos - into_tan * mp.approach_back_m + entry_dir * mp.approach_alt_m;
    const glm::dvec3 tan_h =
        glm::normalize(into_tan - glm::dot(into_tan, entry_dir) * entry_dir);
    const glm::dvec3 app_dir = glm::normalize(approach);
    const glm::dvec3 th_app =
        glm::normalize(tan_h - glm::dot(tan_h, app_dir) * app_dir);
    const glm::dvec3 fixpt = approach - th_app * mp.transit_fix_back_m;

    const auto make = [&]() {
        maverick::MaverickState m{};
        m.inited = true;
        m.mode = maverick::MaverickState::Mode::TRANSIT;
        m.run_dir = 1;
        return m;
    };
    const sim::SimState s = drone::level_state_at(dp, fixpt, tan_h);

    SECTION("off: bit-identical to the legacy call over a live transit") {
        maverick::MaverickState legacy = make(), offarm = make();
        maverick::RunOrders off_ro;  // every field defaulted
        const int ticks = static_cast<int>(30.0 / kAp.sim_dt);
        for (int t = 0; t < ticks; ++t) {
            const maverick::MaverickCmd cl = maverick::maverick_step(
                legacy, s, kAp, mp, route, net, 0, kAp.sim_dt);
            const maverick::MaverickCmd co =
                maverick::maverick_step(offarm, s, kAp, mp, route, net, 0,
                                        kAp.sim_dt, /*hold_runs=*/false, off_ro);
            REQUIRE(same_state(legacy, offarm));
            REQUIRE(same_cmd(cl, co));
        }
        // Non-vacuity: the off arm really did stay committed the whole time.
        REQUIRE(legacy.mode == maverick::MaverickState::Mode::TRANSIT);
    }

    SECTION("on: the transit yields on the first tick and hands to pursue") {
        maverick::MaverickState on = make();
        maverick::RunOrders ro;
        ro.fight_yield = true;
        const maverick::MaverickCmd c = maverick::maverick_step(
            on, s, kAp, mp, route, net, 0, kAp.sim_dt, /*hold_runs=*/false, ro);
        REQUIRE(on.mode == maverick::MaverickState::Mode::PATROL);
        // The handback is CLEAN (the existing give-up path's own semantics):
        // a reloaded countdown, the transit sub-state reset, and the E2.1
        // one-shot cleared so the standing order can collapse the new
        // countdown and relaunch the run.
        REQUIRE(on.patrol_countdown > 0);
        REQUIRE_FALSE(on.transit_fix_done);
        REQUIRE_FALSE(on.transit_grace);
        REQUIRE_FALSE(on.strike_collapsed);
        REQUIRE(on.order_countdown == -1);
        REQUIRE_FALSE(on.run_on_order);
        // ...and this very tick goes to pursue()+BFM, not to the mode
        // machine's all-zero default command.
        REQUIRE(c.pursue_player);
    }

    SECTION("only TRANSIT yields: DIVE_IN, RUN and CLIMB_OUT stay committed") {
        // The ruling that stands (spec E3.1). Written as a DIFFERENTIAL rather
        // than as an absolute claim about the mode: what has to be true is
        // that the yield does not REACH these modes, and each of them can hand
        // itself back to PATROL for its own reasons at any geometry (a
        // DIVE_IN's own go-around, CLIMB_OUT's timeout). Same state, same
        // order, fight_yield the only thing that varies — if the dial touched
        // a committed mode anywhere, these compares would part.
        maverick::RunOrders yes, no;
        yes.fight_yield = true;
        for (const maverick::MaverickState::Mode mode :
             {maverick::MaverickState::Mode::DIVE_IN,
              maverick::MaverickState::Mode::RUN,
              maverick::MaverickState::Mode::CLIMB_OUT}) {
            maverick::MaverickState a = make(), b = make();
            a.mode = mode;
            b.mode = mode;
            const int ticks = static_cast<int>(10.0 / kAp.sim_dt);
            for (int t = 0; t < ticks; ++t) {
                const maverick::MaverickCmd ca = maverick::maverick_step(
                    a, s, kAp, mp, route, net, 0, kAp.sim_dt, false, yes);
                const maverick::MaverickCmd cb = maverick::maverick_step(
                    b, s, kAp, mp, route, net, 0, kAp.sim_dt, false, no);
                REQUIRE(same_state(a, b));
                REQUIRE(same_cmd(ca, cb));
                if (a.mode != maverick::MaverickState::Mode::PATROL)
                    REQUIRE_FALSE(ca.pursue_player);
            }
        }
    }
}

// ===========================================================================
// E3.1 — THE LOOP ACTUALLY CLOSES. The spec's whole claim in one sequence:
// yield -> fight (countdown held by the merge hold) -> the standing order
// re-collapses the reloaded countdown -> the run RELAUNCHES when the merge
// ends. This is the leg that would catch a "fix" that merely stopped the
// pilots dying by abandoning the offensive.
// MUTATION: leave strike_collapsed latched at the yield -> the standing order
// can never collapse the reloaded countdown and the relaunch takes a full
// trait period instead of the stagger.
// ===========================================================================
TEST_CASE("E3.1: the yielded run relaunches once the merge ends") {
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

    maverick::MaverickState m{};
    const auto step = [&](const maverick::RunOrders& o, bool hold) {
        return maverick::maverick_step(m, s0, kAp, dp.maverick, route, net, 0,
                                       kAp.sim_dt, hold, o);
    };

    // (1) The ordered launch.
    int launch_tick = -1;
    for (int t = 0; t < static_cast<int>(20.0 / kAp.sim_dt); ++t) {
        step(ro, false);
        if (m.mode == maverick::MaverickState::Mode::TRANSIT) {
            launch_tick = t;
            break;
        }
    }
    REQUIRE(launch_tick >= 0);
    REQUIRE(m.run_on_order);  // E3.2 bookkeeping: an ORDERED run

    // (2) The ace shows up. The drone's own predicate ANDs d.engaged into
    // fight_yield, and the loader forces transit_fight_yield_m <=
    // raid_fight_yield_m, so a live yield ALWAYS comes with the merge hold —
    // drive both together, exactly as drone::tick would.
    maverick::RunOrders fight = ro;
    fight.fight_yield = true;
    const maverick::MaverickCmd c = step(fight, /*hold_runs=*/true);
    REQUIRE(m.mode == maverick::MaverickState::Mode::PATROL);
    REQUIRE(c.pursue_player);
    REQUIRE_FALSE(m.run_on_order);  // the striker slot is freed for a mate

    // (3) The fight. The merge hold freezes the reloaded countdown, so the
    // pilot cannot march away mid-merge (the measured R4 bug) — but the
    // standing order still collapses the NUMBER exactly once.
    for (int t = 0; t < static_cast<int>(60.0 / kAp.sim_dt); ++t)
        step(fight, /*hold_runs=*/true);
    REQUIRE(m.mode == maverick::MaverickState::Mode::PATROL);
    REQUIRE(m.strike_collapsed);
    REQUIRE(m.order_countdown >= 0);  // the ordered launch is loaded and armed

    // (4) The ace leaves: the run relaunches, on the stagger and not on a
    // fresh trait period (130 s for pilot 0).
    int relaunch = -1;
    for (int t = 0; t < static_cast<int>(60.0 / kAp.sim_dt); ++t) {
        step(ro, false);
        if (m.mode == maverick::MaverickState::Mode::TRANSIT) {
            relaunch = t;
            break;
        }
    }
    REQUIRE(relaunch >= 0);
    INFO("relaunch " << relaunch * kAp.sim_dt << " s after the merge ended");
    REQUIRE(static_cast<double>(relaunch) * kAp.sim_dt <= 6.0);
    REQUIRE(m.run_on_order);
}

// ===========================================================================
// E3.2 — THE CONCURRENT-STRIKER CAP.
//
// The mechanism is TWO COUNTDOWNS: the order's own (order_countdown, which the
// cap freezes) carried alongside the pilot's trait countdown (patrol_countdown,
// which it never touches). The three legs are the three sentences of the spec:
//   OFF (order_hold false, what strike_concurrent_max 0 produces) launches on
//     the collapsed countdown exactly as E2.1 did;
//   HELD defers the ORDERED launch without cancelling it — the one-shot latch
//     stays unspent and the launch fires the moment the slot frees;
//   NATURAL runs are never capped — a held pilot still flies his own
//     trait-period run, on his own clock, and it does not consume a slot.
// MUTATION: freeze patrol_countdown as well as order_countdown -> the natural
// leg never launches (this is the measured P-B regression, written down);
// drop the `ordered && !natural` guard -> a natural run is marked as the
// order's and eats a striker slot it should not.
// ===========================================================================
TEST_CASE("E3.2 knob-off: the concurrent-striker cap holds, never discards") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;
    const glm::dvec3 up = glm::normalize(world::kTunnelMouthErrington);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, up));
    const sim::SimState s0 =
        drone::level_state_at(dp, up * (hf.radius_at(up) + 2000.0), east);

    // Pilot 2 (CANARY): trait period 200 s, so his first countdown is
    // 200 * (0.15 + 0.09 * 2) = 66 s — comfortably clear of the 15 s ordered
    // stagger below, so the two clocks can be told apart without either leg
    // riding on a near-tie.
    constexpr int kPilot = 2;
    const maverick::MaverickTraits& tr = maverick::traits_for(kPilot);
    REQUIRE(tr.period_s == 200.0);
    const double natural_s =
        tr.period_s * (0.15 + 0.09 * kPilot) * dp.maverick.period_scale;
    const double stagger_s = 5.0;                       // -> 5 * (1 + 2) = 15 s
    const double ordered_s = stagger_s * (1 + kPilot);  // = 15 s
    REQUIRE(natural_s > ordered_s + 30.0);              // the legs are separable

    maverick::RunOrders ordered;
    ordered.strike_now = true;
    ordered.stagger_s = stagger_s;

    // Fly `secs` of PATROL with `o` and return the launch tick (-1 = none).
    const auto fly = [&](maverick::MaverickState& m,
                         const maverick::RunOrders& o, double secs) {
        const int n = static_cast<int>(secs / kAp.sim_dt);
        for (int t = 0; t < n; ++t) {
            maverick::maverick_step(m, s0, kAp, dp.maverick, route, net, kPilot,
                                    kAp.sim_dt, false, o);
            if (m.mode != maverick::MaverickState::Mode::PATROL) return t;
        }
        return -1;
    };

    SECTION("off: order_hold false launches on the ORDERED countdown") {
        maverick::MaverickState m{};
        const int launch = fly(m, ordered, natural_s - 10.0);
        REQUIRE(launch >= 0);
        INFO("ordered launch at " << launch * kAp.sim_dt << " s");
        // E2.1's `min(patrol_countdown, stagger)` tick, to the tick.
        REQUIRE(launch == std::llround(ordered_s / kAp.sim_dt) - 1);
        REQUIRE(m.run_on_order);
    }

    SECTION("held: the ORDERED launch defers, and the order is not lost") {
        maverick::RunOrders held = ordered;
        held.order_hold = true;
        maverick::MaverickState m{};
        // Twice the ordered countdown, held: no ordered launch, and the
        // natural one is still far away.
        REQUIRE(fly(m, held, 2.0 * ordered_s) < 0);
        REQUIRE(m.mode == maverick::MaverickState::Mode::PATROL);
        // The order STANDS: the one-shot latch is spent (it fired once) but
        // the countdown it loaded is intact and simply frozen — the cap
        // defers, it never cancels.
        REQUIRE(m.strike_collapsed);
        REQUIRE(m.order_countdown == std::llround(ordered_s / kAp.sim_dt));
        // The slot frees: he launches on the countdown he was holding.
        const int launch = fly(m, ordered, 2.0 * ordered_s);
        REQUIRE(launch >= 0);
        INFO("launched " << launch * kAp.sim_dt << " s after the slot freed");
        REQUIRE(launch == std::llround(ordered_s / kAp.sim_dt) - 1);
        REQUIRE(m.run_on_order);
    }

    SECTION("a natural trait-period run is NOT capped") {
        // The order stands and the cap is FULL for the whole window. His own
        // trait countdown is untouched by either, so he still flies HIS run,
        // on HIS clock — and it does not consume a striker slot (spec E3.2,
        // "natural trait-period runs are NOT capped"). This is the leg the
        // first E3.2 build failed, and it cost probe P-B the stope.
        maverick::RunOrders capped = ordered;
        capped.order_hold = true;
        maverick::MaverickState m{};
        const int launch = fly(m, capped, natural_s + 20.0);
        REQUIRE(launch >= 0);
        INFO("natural launch at " << launch * kAp.sim_dt << " s (trait "
                                  << natural_s << " s)");
        REQUIRE(launch == std::llround(natural_s / kAp.sim_dt) - 1);
        REQUIRE(m.strike_collapsed);    // the order WAS seen
        REQUIRE_FALSE(m.run_on_order);  // but this run is his own
    }
}

// ===========================================================================
// E3 — THE WHOLE-RUNG OFF ARM at the drone::tick seam. Every E3 dial at its
// off value must reproduce the E2 tick bit-for-bit through the REAL tick, with
// a live foe well inside the yield radius (the case the dials exist for) —
// the single check that no E3 code path leaks into an off-dial world through a
// term nobody listed. Paired with the shipped arm, which must differ.
// ===========================================================================
TEST_CASE("E3 knob-off: every dial off reproduces the pre-E3 tick") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    env.ground_params.friction = 0.08;
    env.ground_params.max_sink_ms = 5.0;
    env.ground_params.normal_probe_m = 60.0;
    env.ground_params.contact_height_m = 0.0;
    env.ground_params.deep_penetration_m = 50.0;

    const drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.transit_fight_yield_m > 0.0);  // premise: it ships ON
    REQUIRE(shipped.strike_concurrent_max > 0);
    drone::DroneParams offv = shipped;
    {
        const drone::DroneParams d0;  // the pre-E3 defaults
        offv.transit_fight_yield_m = d0.transit_fight_yield_m;
        offv.strike_concurrent_max = d0.strike_concurrent_max;
    }

    const glm::dvec3 up = glm::normalize(world::kTunnelMouthErrington);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, up));
    const glm::dvec3 pos = up * (hf.radius_at(up) + 3000.0);
    const auto make = [&](const drone::DroneParams& dp) {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.curr = drone::level_state_at(dp, pos, east);
        d.curr.velocity = east * dp.maverick.base_speed;
        d.curr.last_vhat = east;
        d.prev = d.curr;
        d.grounded = false;
        d.engaged = true;
        d.foe = drone::kFoePlayer;
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::TRANSIT;
        d.mav.run_dir = 1;
        return d;
    };

    // A foe 800 m away — well inside both yield radii, so the ON arm's dial
    // is genuinely live (fixture-no-op guard).
    sim::SimState player = drone::level_state_at(shipped, pos + east * 800.0, east);
    REQUIRE(glm::length(player.position - pos) < shipped.transit_fight_yield_m);

    drone::DroneState ref = make(offv), got = make(offv), hot = make(shipped);
    for (int t = 0; t < 300; ++t) {
        drone::tick(ref, kAp, offv, &env, &player);
        drone::tick(got, kAp, offv, &env, &player);
        drone::tick(hot, kAp, shipped, &env, &player);
        REQUIRE(ref.curr.position == got.curr.position);
        REQUIRE(ref.curr.orientation == got.curr.orientation);
        // OFF: the committed transit ignores the ace exactly as it always did.
        REQUIRE(got.mav.mode == maverick::MaverickState::Mode::TRANSIT);
    }
    // Paired ON arm: the shipped table yields, and does NOT fly the same line.
    REQUIRE(hot.mav.mode == maverick::MaverickState::Mode::PATROL);
    REQUIRE(hot.curr.position != got.curr.position);
}

// ===========================================================================
// ★★★ RUNG S2-TUNNEL — THE RAID LAUNCH, pinned where it can fail.
//
// Chad, 2026-08-26: "if they are leaving for a raid on a pump, the surface
// one, they should actually use the tunnel, that is what it is there for."
//
// THE MEASURED BLOCKER this rung opened, restated as a test: raid duty froze
// the run scheduler (drone::tick's hold_runs), so a designated raider could
// NEVER enter the bore. These legs pin the maverick half of the fix — the
// raid's OWN countdown, its one-shot, and the property that makes it a
// different mission from the deep-pump strike: it must NOT set run_on_order,
// because the app counts that against strike_concurrent_max and freezes it
// with ro.order_hold.
//
// OFF ARM: RunOrders::raid_now false must be bit-identical to the legacy call.
// MUTATIONS APPLIED AND CONFIRMED RED, each restored:
//   (a) drop `|| raid_ordered` from the launch condition -> the ON leg never
//       leaves PATROL and fails.
//   (b) change the launch to `m.run_on_order = ordered || raid_ordered` ->
//       the striker-slot leg fails.
//   (c) delete `m.raid_countdown = -1` from the launch -> the countdown goes
//       negative-and-fires forever; the "one launch per order" leg fails.
// ===========================================================================
TEST_CASE("S2 raid launch: the raid clock fires, and it is NOT a striker run") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const drone::DroneParams dp = kScen.drone;
    const maverick::MaverickParams& mp = dp.maverick;

    // A patrolling pilot well clear of the mouths (PATROL is where the run
    // scheduler lives).
    const glm::dvec3 pos =
        glm::normalize(net.spine.front().pos) * (kAp.R + 2000.0);
    const sim::SimState s = drone::level_state_at(dp, pos, glm::dvec3{0, 1, 0});
    const auto make = [&]() {
        maverick::MaverickState m{};
        m.inited = true;
        m.mode = maverick::MaverickState::Mode::PATROL;
        m.run_dir = 1;
        m.patrol_countdown = 1000000;  // the trait run is far away
        return m;
    };

    SECTION("off: raid_now false is bit-identical to the legacy call") {
        maverick::MaverickState legacy = make(), offarm = make();
        maverick::RunOrders off_ro;  // raid_now false, raid_stagger_s 0
        const int ticks = static_cast<int>(60.0 / kAp.sim_dt);
        for (int t = 0; t < ticks; ++t) {
            const maverick::MaverickCmd cl = maverick::maverick_step(
                legacy, s, kAp, mp, route, net, 0, kAp.sim_dt);
            const maverick::MaverickCmd co = maverick::maverick_step(
                offarm, s, kAp, mp, route, net, 0, kAp.sim_dt,
                /*hold_runs=*/false, off_ro);
            REQUIRE(same_state(legacy, offarm));
            REQUIRE(same_cmd(cl, co));
        }
        // Non-vacuity: the arm really did stay in PATROL the whole time, so
        // the identity is not the identity of two dead machines.
        REQUIRE(legacy.mode == maverick::MaverickState::Mode::PATROL);
        REQUIRE(legacy.raid_countdown == -1);
    }

    SECTION("on: raid_now collapses its own clock and launches the run") {
        maverick::MaverickState on = make();
        maverick::RunOrders ro;
        ro.raid_now = true;
        ro.raid_stagger_s = 2.0;  // spawn_index 0 -> (1+0)*2.0 = 2 s
        ro.entry_dir = -1;        // the app's own-mouth verdict

        // Tick ONE: the one-shot latches and loads the raid clock. It must
        // NOT have touched the trait countdown or the striker's clock.
        maverick::maverick_step(on, s, kAp, mp, route, net, 0, kAp.sim_dt,
                                /*hold_runs=*/false, ro);
        REQUIRE(on.raid_collapsed);
        REQUIRE(on.raid_countdown > 0);
        REQUIRE(on.order_countdown == -1);  // NOT the striker's clock
        REQUIRE(on.run_dir == -1);          // the app's entry direction taken
        REQUIRE(on.mode == maverick::MaverickState::Mode::PATROL);

        // Fly it out. The run must launch inside the stagger + one tick.
        const int budget = static_cast<int>(3.0 / kAp.sim_dt);
        int launched_at = -1;
        for (int t = 0; t < budget; ++t) {
            maverick::maverick_step(on, s, kAp, mp, route, net, 0, kAp.sim_dt,
                                    /*hold_runs=*/false, ro);
            if (on.mode != maverick::MaverickState::Mode::PATROL) {
                launched_at = t;
                break;
            }
        }
        INFO("launched at tick " << launched_at);
        REQUIRE(launched_at >= 0);
        REQUIRE(on.mode == maverick::MaverickState::Mode::TRANSIT);
        // ★ THE PROPERTY THAT MAKES IT A DIFFERENT MISSION. run_on_order is
        // what app/instructor_tick.h counts into on_order_runs and caps with
        // strike_concurrent_max, and what ro.order_hold freezes. A raid run
        // must be neither counted nor paced by the striker pipeline.
        REQUIRE_FALSE(on.run_on_order);
        // The launch is spent: the clock is cleared so it cannot re-fire.
        REQUIRE(on.raid_countdown == -1);
        // And the trait countdown survived underneath, un-consumed.
        REQUIRE(on.patrol_countdown > 0);
    }

    SECTION("hold_runs still freezes a raid clock that has been loaded") {
        // The freeze is drone::tick's, and it still applies to everything the
        // app did NOT route by the tunnel (and to a live merge). Proven here
        // on the raid clock specifically: loaded, then frozen, then released.
        maverick::MaverickState on = make();
        maverick::RunOrders ro;
        ro.raid_now = true;
        ro.raid_stagger_s = 1.0;
        maverick::maverick_step(on, s, kAp, mp, route, net, 0, kAp.sim_dt,
                                /*hold_runs=*/false, ro);
        const long long loaded = on.raid_countdown;
        REQUIRE(loaded > 0);
        for (int t = 0; t < 200; ++t)
            maverick::maverick_step(on, s, kAp, mp, route, net, 0, kAp.sim_dt,
                                    /*hold_runs=*/true, ro);
        CHECK(on.raid_countdown == loaded);  // frozen, not reset
        CHECK(on.mode == maverick::MaverickState::Mode::PATROL);
    }

    SECTION("the raid one-shot clears at the CLIMB_OUT->PATROL reload") {
        // WHEN YOU ADD AN EXIT TO A STATE MACHINE IT INHERITS EVERY DEFERRAL
        // THE OLD EXITS CARRY (the standing law). The one-shot must clear
        // wherever patrol_countdown reloads, or a raider that completed one
        // bore run could never be ordered into another.
        maverick::MaverickState on = make();
        maverick::RunOrders ro;
        ro.raid_now = true;
        ro.raid_stagger_s = 1.0;
        maverick::maverick_step(on, s, kAp, mp, route, net, 0, kAp.sim_dt,
                                /*hold_runs=*/false, ro);
        REQUIRE(on.raid_collapsed);
        // Drive the CLIMB_OUT timeout exit (maverick.h's 40 s clause).
        on.mode = maverick::MaverickState::Mode::CLIMB_OUT;
        on.mode_ticks = 0;
        const int ticks = static_cast<int>(45.0 / kAp.sim_dt);
        for (int t = 0; t < ticks; ++t) {
            maverick::RunOrders none;  // no standing order during the climb
            maverick::maverick_step(on, s, kAp, mp, route, net, 0, kAp.sim_dt,
                                    /*hold_runs=*/false, none);
            if (on.mode == maverick::MaverickState::Mode::PATROL) break;
        }
        REQUIRE(on.mode == maverick::MaverickState::Mode::PATROL);
        CHECK_FALSE(on.raid_collapsed);
        CHECK(on.raid_countdown == -1);
    }
}

// ===========================================================================
// ★★★ RUNG S2-TUNNEL — THE ROUTE LEGALITY MEASUREMENT, pinned.
//
// app::uncovered_arc_m is the whole router's input: metres of a great circle
// that lie outside BOTH faction domes. It reads world::ellipse_r_eff, the same
// air edge the containment leash reads.
//
// MUTATIONS APPLIED AND CONFIRMED RED, each restored:
//   (a) drop the `if (!(ea[f] > 0 && eb[f] > 0)) continue;` dead-dome guard ->
//       the crushed-dome leg reports 0 uncovered and fails (a dead dome would
//       "cover" the whole planet through a degenerate r_eff).
//   (b) invert the `arc <= reff` test -> the in-dome leg fails.
// ===========================================================================
TEST_CASE("S2 route legality: uncovered arc is zero in-dome and real across") {
    world::FactionGrowth grow[2];
    grow[0].radius_scale = 1.0;
    grow[0].ceiling_scale = 1.0;
    grow[1].radius_scale = 1.0;
    grow[1].ceiling_scale = 1.0;
    const double R = kAp.R;

    glm::dvec3 c0, m0, c1, m1;
    double a0 = 0.0, b0 = 0.0, a1 = 0.0, b1 = 0.0;
    world::faction_ellipse(0, grow, c0, m0, a0, b0);
    world::faction_ellipse(1, grow, c1, m1, a1, b1);
    REQUIRE(a0 > 0.0);
    REQUIRE(a1 > 0.0);

    // (1) A short leg wholly inside one dome: ZERO uncovered.
    {
        const glm::dvec3 p = glm::normalize(c0) * R;
        // 0.3 * semi-minor along the major axis -- inside in every bearing.
        const glm::dvec3 tang =
            glm::normalize(m0 - c0 * glm::dot(m0, glm::normalize(c0)));
        const double ang = (0.3 * b0) / R;
        const glm::dvec3 q =
            (std::cos(ang) * glm::normalize(c0) + std::sin(ang) * tang) * R;
        CHECK(app::uncovered_arc_m(p, q, grow, R) == Catch::Approx(0.0));
    }
    // (2) Centre to centre across the map: a REAL uncovered arc, and strictly
    //     less than the whole separation (both ends are in air).
    {
        const glm::dvec3 p = glm::normalize(c0) * R;
        const glm::dvec3 q = glm::normalize(c1) * R;
        const double sep =
            R * std::acos(std::clamp(
                    glm::dot(glm::normalize(c0), glm::normalize(c1)), -1.0,
                    1.0));
        const double gap = app::uncovered_arc_m(p, q, grow, R);
        INFO("separation " << sep << " m, uncovered " << gap << " m");
        CHECK(gap > 1000.0);
        CHECK(gap < sep);
    }
    // (3) A CRUSHED dome covers nothing: shrink faction 1 to zero and the
    //     same track must report MORE uncovered arc, not less.
    {
        const glm::dvec3 p = glm::normalize(c0) * R;
        const glm::dvec3 q = glm::normalize(c1) * R;
        const double before = app::uncovered_arc_m(p, q, grow, R);
        world::FactionGrowth dead[2] = {grow[0], grow[1]};
        dead[1].radius_scale = 0.0;
        const double after = app::uncovered_arc_m(p, q, dead, R);
        INFO("before " << before << " after " << after);
        CHECK(after > before);
    }
    // (4) Degenerate inputs never produce a NaN or a negative (the
    //     normalize-guard law: RaidOrder's default target is the ORIGIN).
    {
        const glm::dvec3 p = glm::normalize(c0) * R;
        CHECK(app::uncovered_arc_m(p, p, grow, R) == Catch::Approx(0.0));
        CHECK(app::uncovered_arc_m(p, glm::dvec3{0.0}, grow, R) ==
              Catch::Approx(0.0));
    }
}

// ===========================================================================
// ★★★ RUNG S2-TUNNEL — ONE RANKING AUTHORITY.
//
// app::raider_rank exposes combat::faction_raider's OWN out_rank loop. If the
// two ever forked, "who is a raider" and "who keeps the deck run" would
// disagree and the deck-runner slots would be handed to pilots who are not
// raiding. This leg holds them together by construction.
//
// MUTATION APPLIED AND CONFIRMED RED: flip raider_rank's tie-break to
// `i > idx` -> the agreement leg fails on the tied pair.
// ===========================================================================
TEST_CASE("S2 raider_rank agrees with combat::faction_raider, exactly") {
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        const int own = combat::maverick_faction(i);
        const int rank = app::raider_rank(i, nullptr);
        const bool is_raider = combat::faction_raider(i, nullptr);
        INFO("pilot " << i << " faction " << own << " rank " << rank);
        CHECK(is_raider == (rank < combat::raiders_for_team(own)));
    }
    // Ranks are a PERMUTATION within each faction: exactly one rank 0, and no
    // duplicates. A ranking that collapsed would hand every slot to everybody.
    for (int f = 0; f < 2; ++f) {
        std::vector<int> ranks;
        for (int i = 0; i < combat::kNumMavericks; ++i)
            if (combat::maverick_faction(i) == f)
                ranks.push_back(app::raider_rank(i, nullptr));
        std::sort(ranks.begin(), ranks.end());
        for (std::size_t k = 0; k < ranks.size(); ++k)
            CHECK(ranks[k] == static_cast<int>(k));
    }
    // The LIVE MASK is honoured: mask everyone off except one pilot and he is
    // rank 0 in his own faction (a wreck ranks nothing -- E12.1's law).
    {
        bool live[combat::kNumMavericks] = {};
        int who = -1;
        for (int i = 0; i < combat::kNumMavericks; ++i)
            if (combat::maverick_faction(i) == 0) { who = i; break; }
        REQUIRE(who >= 0);
        live[who] = true;
        CHECK(app::raider_rank(who, live) == 0);
    }
}
