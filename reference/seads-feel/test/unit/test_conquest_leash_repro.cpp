// CONQUEST BUBBLE-LEASH CONTAINMENT — reproduction + regression (Chad's report,
// 2026-07-26): "AI planes fly outside their faction bubble/zone and stay out
// — and NOT toward the tunnel or pumps." Isolates the PATROL/leash path from
// tunnel runs and raids (both are sanctioned exceptions) by spawning the
// conquest fleet EXACTLY as app::main.cpp does (per-faction golden-angle disc
// scatter at spawn_alt AGL, the leash wired from world::faction_ellipse each
// tick) and flying it for real sim-minutes with drone::tick, measuring the
// arc/r_eff containment fraction over time.
//
// ROOT CAUSE (see the investigation report): the conquest fleet spawns and
// patrols at spawn_alt = 2000 m AGL — well above the global deck's full-air
// band (deck_agl_m=120, fading to 0 by ~320 m AGL). So a patrolling drone that
// strays past its faction ellipse's hard edge is ALSO past the deck: real
// dynamic pressure (rho*atm_frac_at, sim/step.cpp's TRUE q — NOT the
// authority floor q_att_floor) collapses toward zero, so LIFT collapses too.
// The leash still computes a correct home bank and the ROTATIONAL authority
// is floored (q_att_floor keeps roll/pitch/yaw torque non-zero even in
// vacuum), so the drone dutifully ROLLS toward home — but with ~no lift it
// can't generate the lateral acceleration to actually CURVE the flight path,
// so it coasts on its last heading, drifting further outside while banked
// "toward home" and never actually turning. The old steer_frac=0.80 engages
// the leash too late relative to the edge_soft_m=1200 m air taper at these
// arc rates, so by the time the plant starts really losing lift the bank
// hasn't yet rotated the velocity vector.
//
// THE FIX (world/faction_bubbles.h is unchanged; drone/drone.h's
// BubbleLeash tightens its default hysteresis band): engage the leash
// EARLIER (steer_frac 0.80 -> 0.55) and release earlier too (release_frac
// 0.72 -> 0.42, hard_frac 0.95 -> 0.75), with a stronger home gain (k_home
// 3.0 -> 4.5) so the bank command is committed sooner and harder, while the
// plant still has enough real air (well inside edge_soft_m of the hard edge)
// to actually curve the path before authority starves. Off-arm (leash
// disabled, or a drone deep inside its bubble) stays bit-identical by
// construction — apply_bubble_leash only reads the BubbleLeash fields the
// caller supplies; no code path changed, only the tuning DATA.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "combat/conquest.h"
#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "drone/drone.h"
#include "sim/aero.h"
#include "sim/fields.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);

constexpr double kPi = 3.14159265358979323846;

// Exact spawn placement app::main.cpp uses for the conquest fleet (fly-1
// fix): golden-angle disc scatter, 5 km arc radius, around the drone's OWN
// faction ellipse center, at spawn_alt AGL. Mirrored here (not called
// through main.cpp — no seam exists to reuse app-level main() code from a
// test) so the repro flies the SAME geometry the real match does.
drone::DroneState conquest_spawn(const drone::DroneParams& dp, int i,
                                 int total) {
    constexpr double kConquestSpreadM = 5000.0;
    const double golden = kPi * (3.0 - std::sqrt(5.0));
    const bool valley = combat::maverick_faction(i) == combat::CQ_VALLEY;
    const glm::dvec3 c = glm::normalize(
        valley ? world::kValleyCenterDir : world::kSudburyCenterDir);
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
    drone::DroneState d;
    d.curr = drone::level_state_at(dp, pos, heading);
    d.prev = d.curr;
    d.grounded = true;
    d.spawn_index = i;
    d.fleet_count = total;
    d.hp = dp.hp;
    (void)total;
    return d;
}

struct FracStats {
    double max_frac = 0.0;
    double max_frac_after_60s = 0.0;  // steady-state (past initial settle)
    double final_frac = 0.0;
    bool ever_engaged = false;
};

// Fly `d` for `ticks` sim ticks under its own faction's LIVE leash (no
// growth, no raid, patrol/pursuit only — player == nullptr) and report the
// worst containment fraction observed. `env` supplies the atmosphere field
// (real bubbles) so the fleet actually feels the air edge it's supposed to
// respect.
FracStats fly_and_measure(drone::DroneState& d, const drone::DroneParams& dp,
                          const sim::Environment* env, int ticks,
                          const world::FactionGrowth grow[2] = nullptr) {
    FracStats stats;
    const int faction = combat::maverick_faction(d.spawn_index);
    const world::FactionGrowth grow_default[2]{};  // 1.0/1.0, no-growth
    if (grow == nullptr) grow = grow_default;
    glm::dvec3 center_dir, major_axis;
    double a_m = 0.0, b_m = 0.0;
    world::faction_ellipse(faction, grow, center_dir, major_axis, a_m, b_m);

    const int settle_ticks = static_cast<int>(60.0 / kAp.sim_dt);
    for (int t = 0; t < ticks; ++t) {
        drone::BubbleLeash lz;
        lz.enabled = true;
        lz.center_dir = center_dir;
        lz.major_axis = major_axis;
        lz.a_m = a_m;
        lz.b_m = b_m;
        d.leash = lz;
        drone::tick(d, kAp, dp, env);

        const glm::dvec3 up = glm::normalize(d.curr.position);
        const double c = std::clamp(glm::dot(up, glm::normalize(center_dir)),
                                    -1.0, 1.0);
        const double arc = kAp.R * std::acos(c);
        const double reff =
            world::ellipse_r_eff(center_dir, major_axis, a_m, b_m, up);
        const double frac = reff > 1e-6 ? arc / reff : 0.0;
        stats.max_frac = std::max(stats.max_frac, frac);
        if (t >= settle_ticks)
            stats.max_frac_after_60s = std::max(stats.max_frac_after_60s, frac);
        if (d.leash_engaged) stats.ever_engaged = true;
        stats.final_frac = frac;
    }
    return stats;
}

}  // namespace

// PHASE-1 DIAGNOSTIC (not a gate assertion): fly each of the 10 conquest
// drones for 300 sim-seconds and print, per drone, the worst frac + the
// frac at the very end of the run (a rising final_frac vs max_frac close
// together means it's STILL drifting outward, not oscillating back home).
TEST_CASE("conquest leash: DIAGNOSTIC per-drone frac trace (300s, patrol)") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    drone::DroneParams dp;
    dp.maverick.enabled = false;

    const int ticks = static_cast<int>(300.0 / kAp.sim_dt);
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        drone::DroneState d = conquest_spawn(dp, i, combat::kNumMavericks);
        const FracStats stats = fly_and_measure(d, dp, &env, ticks);
        const double alt_agl = sim::altitude(d.curr.position, kAp);
        WARN("drone " << i << " (faction "
                       << combat::maverick_faction(i) << "): max_frac="
                       << stats.max_frac << " final_frac=" << stats.final_frac
                       << " alt_agl=" << alt_agl
                       << " engaged=" << stats.ever_engaged);
    }
    CHECK(true);
}

// PHASE-1 DIAGNOSTIC #2 (not a gate assertion): the compounding case — a
// drone comfortably contained (frac 0.6 of the BASELINE dome) when its OWN
// faction's dome then SHRINKS mid-flight (a pump-loss event, ruling C:
// radius_scale -= shrink_radius_frac, floored at kFactionScaleFloor). The
// absolute position doesn't move at the instant of the shrink, but frac
// (measured against the new, smaller ellipse) jumps in one tick. Does the
// leash recover, or does the plant's now-real thin air (the drone is
// instantly on/past the new, smaller edge) strand it?
TEST_CASE("conquest leash: DIAGNOSTIC bubble shrinks under a contained "
          "drone (pump-loss event mid-flight)") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;

    const world::FactionGrowth grow_base[2]{};  // 1.0/1.0
    glm::dvec3 center_dir, major_axis;
    double a0 = 0.0, b0 = 0.0;
    world::faction_ellipse(world::VALLEY, grow_base, center_dir, major_axis,
                          a0, b0);

    // Place the drone at frac 0.6 of the BASELINE dome, heading OUTWARD
    // along the major axis (the worst case: already moving away from home).
    const double theta = 0.6 * a0 / kAp.R;
    const glm::dvec3 cn = glm::normalize(center_dir);
    const glm::dvec3 mn = glm::normalize(major_axis);
    const glm::dvec3 dir = std::cos(theta) * cn + std::sin(theta) * mn;
    const glm::dvec3 pos = glm::normalize(dir) * (kAp.R + dp.spawn_alt);
    drone::DroneState d;
    d.curr = drone::level_state_at(dp, pos, mn);
    d.prev = d.curr;
    d.spawn_index = 5;  // a VALLEY slot (5-9)
    d.fleet_count = combat::kNumMavericks;
    d.hp = dp.hp;

    // Simulate TWO pump losses on VALLEY's own side (a realistic mid-match
    // state): radius_scale = 1.0 - 2*0.25 = 0.5 (matches
    // combat::ConquestParams defaults / kFactionScaleFloor 0.4 not yet hit).
    world::FactionGrowth grow_shrunk[2]{};
    grow_shrunk[world::VALLEY].radius_scale = 0.5;
    grow_shrunk[world::SUDBURY].radius_scale = 1.0;

    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, grow_shrunk, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    glm::dvec3 c2, m2;
    double a2 = 0.0, b2 = 0.0;
    world::faction_ellipse(world::VALLEY, grow_shrunk, c2, m2, a2, b2);
    {
        const glm::dvec3 up = glm::normalize(d.curr.position);
        const double c = std::clamp(glm::dot(up, glm::normalize(c2)), -1.0, 1.0);
        const double arc = kAp.R * std::acos(c);
        const double reff = world::ellipse_r_eff(c2, m2, a2, b2, up);
        WARN("frac AT THE INSTANT of the shrink (new dome) = " << arc / reff);
    }

    const FracStats stats = fly_and_measure(
        d, dp, &env, static_cast<int>(180.0 / kAp.sim_dt), grow_shrunk);
    WARN("after 180s post-shrink: max_frac=" << stats.max_frac
                       << " final_frac=" << stats.final_frac
                       << " engaged=" << stats.ever_engaged);
    CHECK(true);
}

// PHASE 1 evidence (patrol only, maverick disabled): a fleet spawned inside
// its own faction ellipse, flown by the pure patrol autopilot with the leash
// live and a REAL atmosphere field (so thin air outside the bubble is felt),
// must stay contained over a full patrol session. Before the fix this drifts
// well past frac 1.0 and stays there (the leash engages but can't turn —
// starved lift); after the fix it stays comfortably inside.
TEST_CASE("conquest leash: patrol fleet stays inside its bubble over 100 s "
          "with a real atmosphere field") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    drone::DroneParams dp;
    dp.maverick.enabled = false;  // isolates PATROL/leash from tunnel runs

    const int ticks = static_cast<int>(100.0 / kAp.sim_dt);  // 100 sim-s
    double worst = 0.0;
    double worst_settled = 0.0;
    bool any_engaged = false;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        drone::DroneState d = conquest_spawn(dp, i, combat::kNumMavericks);
        const FracStats stats = fly_and_measure(d, dp, &env, ticks);
        worst = std::max(worst, stats.max_frac);
        worst_settled = std::max(worst_settled, stats.max_frac_after_60s);
        any_engaged = any_engaged || stats.ever_engaged;
    }
    INFO("worst frac over 100s (all 10 patrol drones) = " << worst);
    INFO("worst frac after the first 60s (steady state) = " << worst_settled);
    CHECK(any_engaged);  // the leash must actually fire during a real patrol
    // Containment: nobody should sustain past the hard edge (frac 1.0), and
    // comfortably before it in steady state.
    CHECK(worst_settled < 0.85);
}

// FULL CONQUEST config (maverick enabled, no live tunnel net so every drone
// stays in PATROL mode — the honest approximation of "not a tunnel run": the
// exemption is gated on d.mav.mode via maverick::is_tunnel_mode, which is
// false in PATROL regardless of dp.maverick.enabled). Same containment bar.
TEST_CASE("conquest leash: full conquest fleet (maverick enabled, no tunnel "
          "run in progress) stays inside its bubble") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;  // env->tunnels stays null -> maverick brain never commits
                    // a run (drone::tick's maverick block is skipped), so
                    // every drone is in the PATROL disposition the whole time.

    drone::DroneParams dp;
    dp.maverick.enabled = true;  // conquest_on forces this in app::main.cpp

    const int ticks = static_cast<int>(90.0 / kAp.sim_dt);
    double worst_settled = 0.0;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        drone::DroneState d = conquest_spawn(dp, i, combat::kNumMavericks);
        const FracStats stats = fly_and_measure(d, dp, &env, ticks);
        worst_settled = std::max(worst_settled, stats.max_frac_after_60s);
    }
    INFO("worst frac after 60s, full conquest config = " << worst_settled);
    CHECK(worst_settled < 0.85);
}
