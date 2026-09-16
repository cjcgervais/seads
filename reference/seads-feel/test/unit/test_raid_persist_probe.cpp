// ============================================================================
// E17 RAID-PERSISTENCE PROBE (hidden, a measuring instrument — never a gate leg).
//
// WHY: Chad's ruling on tape 10 (2026-08-24) — "they need to try to keep
// killing the pump, not shoot it and fly away."
//
// THE TAPE'S NUMBERS, drone i=0's raid on pump 0 (a 652.8 s order window):
//     t=220 s   range to pump   335 m     <- the one pass
//     t=298 s   range to pump  9602 m     <- 80 s later, ten kilometres away
//     t=403 s   range to pump   374 m     <- the second pass, three minutes on
//   on-station credited over the whole window: 15.8 s.
// Through the entire departure: raid=1 (order live), foe=-1, eng=0, leash=0,
// def=0, mav=PATROL, inert=0 — NOTHING was competing for the stick, and the
// heading held dot(vel, to_pump) ~ -0.85 for eighty seconds. A raider under
// RaidOrder's own defaults (k_az 3.5, bank_cap 1.13 rad = 65 deg) should
// reverse heading in ~15 s at a ~1.5 km radius. It did not.
//
// WHAT THIS PROBE DOES: reduces that to one repeatable question. Place a
// raider PAST its pump, flying away, with a live RaidOrder and nothing else
// armed — no foe, no leash, no defend order, no tunnel run — and tick it.
// Report the range and heading-alignment history. A raider that presses the
// objective comes back around; a raider that "shoots it and flies away" does
// not. Nothing here asserts a balance number: it prints the curve.
//
// The AIRFRAME comes from the SHIPPED loader (kScen.drone).
// ⚠ The first cut of this banner made that claim while `dp` was a bare
// `drone::DroneParams{}` — struct defaults, a 115 m/s cruise against the
// shipped 85 — and turn radius goes as V^2, so the probe measured an
// aeroplane turning 1.83x wider than the game's. The banner WAS the bug
// report (audit 2026-08-25, finding 1). Fixed 2026-08-26.
//
// The WORLD here is deliberately SYNTHETIC — a uniform 300 m field plus one
// dome — because this probe isolates raid GEOMETRY, not terrain. That is
// stated rather than implied so nobody reads "SHIPPED loaders" as covering
// the heightfield.
//
// RUN: ./build/seads_tests.exe "[.e17raid]" -s
// ============================================================================

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "sim/fields.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/heightfield.h"

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
// ★ THE SHIPPED TUNING (test_maverick.cpp's pattern). kScen.drone is the
// airframe the game actually flies -- cruise 85.0 m/s as of 2026-08-25
// (config/scenario.toml:26), NOT DroneParams{}'s 115.0 default
// (drone/drone.h:76). Read the loader, never the struct.
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

}  // namespace

TEST_CASE("E17 raid persistence probe", "[.e17raid]") {
    world::HeightField hf = uniform_field(300.0);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.deck_terrain_relative = kGame.atmosphere.deck_terrain_relative;
    af.dome_exponent = kGame.atmosphere.bubble_dome_exponent;
    // A DOME over the pump. Without one the raider is in near-vacuum at any
    // useful altitude, mushes, and falls out of the sky in ~14 s — which is
    // what the second cut of this probe measured, and it is not the question.
    // Pumps stand inside domes in the real world; this fixture must too.
    {
        sim::AtmosphereField::Bubble b;
        b.center_dir = glm::dvec3{0.0, 1.0, 0.0};
        b.ground_radius_m = 17000.0;
        b.ceiling_m = 4000.0;
        af.bubbles.push_back(b);
    }

    sim::Environment env;
    env.ground = &hf;
    env.atm = &af;
    env.tunnels = nullptr;  // the surface raid never touches the net
    // Same ground table the match fixture flies (test_conquest_match.cpp's
    // tunnel_ground_params) — a zero normal_probe_m trips heightfield's assert.
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    env.ground_params = gp;

    // ★ THE SHIPPED AIRFRAME (kScen.drone), never struct defaults. The first
    // cut of this probe declared a bare `drone::DroneParams dp;` here -- a
    // 115 m/s cruise against the game's 85 (as of 2026-08-25) -- and turn
    // radius goes as V^2, so it measured an aeroplane turning 1.83x wider
    // than the game's. Turn radius is the exact quantity this probe exists to
    // measure. Only the two E17 dials move per-arm below.
    drone::DroneParams dp = kScen.drone;

    // The pump: a point on the surface. The raider starts PAST it, flying
    // away — the exact state the tape catches at t=224 s.
    // The uniform field sits at 300 m elevation, so the pump stands ON it and
    // the raider flies WELL above it — a spawn below the terrain crashes and
    // respawns on tick one (which is what the first cut of this probe did, and
    // it made every arm identical: the tell was three arms printing the same
    // numbers from t=5 s on).
    constexpr double kElev = 300.0;
    constexpr double kFlyAlt = 1200.0;
    const glm::dvec3 up{0.0, 1.0, 0.0};
    const glm::dvec3 east{1.0, 0.0, 0.0};
    const glm::dvec3 pump_pos = up * (kAp.R + kElev);

    struct Arm {
        const char* name;
        double start_past_m;  // how far beyond the pump it starts
        double attack_alt_m;  // dp.raid_attack_alt_m for this arm
        double reattack_m;    // dp.raid_reattack_m for this arm
    };
    // ★ AN ARM OVERRIDES CONFIG, IT NEVER DEFINES IT (this ladder's law). The
    // SHIPPED arms read config/scenario.toml through the loader so they cannot
    // drift from what the game flies; the OFF arms pin 0/0 to reproduce the
    // pre-E17 point chase as the differential.
    // (The lambda that used to stand here WAS the trap in miniature: a
    // DroneParams{} of struct defaults with exactly two shipped fields copied
    // onto it. With dp = kScen.drone the SHIPPED arms need only the two dial
    // values, and they come from the same single loader as everything else.)
    const drone::DroneParams& shipped = kScen.drone;
    const Arm arms[] = {
        {"OFF  just past the pump (400 m)", 400.0, 0.0, 0.0},
        {"SHIPPED just past      (400 m)", 400.0, shipped.raid_attack_alt_m,
         shipped.raid_reattack_m},
        {"OFF  one pass out     (1500 m)", 1500.0, 0.0, 0.0},
        {"SHIPPED one pass out  (1500 m)", 1500.0, shipped.raid_attack_alt_m,
         shipped.raid_reattack_m},
        {"OFF  well out         (4000 m)", 4000.0, 0.0, 0.0},
        {"SHIPPED well out      (4000 m)", 4000.0, shipped.raid_attack_alt_m,
         shipped.raid_reattack_m},
        // ★ A5 LIVENESS ARM (diagnostic, never ships). The raid dials SHIP 0.0,
        // so every SHIPPED arm above is arithmetic-identical to its OFF
        // partner today -- bit-identity proves NOTHING about the branch. This
        // arm pins values that MUST move the trajectory: the aim point rises
        // 600 m radially and the reattack latch arms at 2000 m. If its curve
        // matches OFF-1500 to the digit, the raid branch never reads the dials
        // (dead branch) or the fixture is blind.
        {"LIVE attack_alt 600 / reattack 2000 (never ships)", 1500.0, 600.0,
         2000.0},
    };

    std::printf(
        "\n=========== E17: DOES A RAIDER COME BACK? ===========\n"
        "raid order defaults: k_az=%.2f k_el=%.2f bank_cap=%.3f rad (%.0f deg)"
        " gamma_cap=%.3f\n",
        drone::RaidOrder{}.k_az, drone::RaidOrder{}.k_el,
        drone::RaidOrder{}.bank_cap,
        drone::RaidOrder{}.bank_cap * 180.0 / 3.14159265358979,
        drone::RaidOrder{}.gamma_cap);

    for (const Arm& arm : arms) {
        dp.raid_attack_alt_m = arm.attack_alt_m;
        dp.raid_reattack_m = arm.reattack_m;
        // Start `start_past_m` beyond the pump, flying directly AWAY from it.
        const double ang = arm.start_past_m / kAp.R;
        const glm::dvec3 dir =
            glm::normalize(std::cos(ang) * up + std::sin(ang) * east);
        const glm::dvec3 pos = dir * (kAp.R + kFlyAlt);
        // Heading: directly AWAY from the pump, projected into the local
        // tangent plane (level_state_at re-projects, but be explicit).
        const glm::dvec3 from_pump = pos - pump_pos;
        const glm::dvec3 away =
            glm::normalize(from_pump - glm::dot(from_pump, dir) * dir);

        drone::DroneState d;
        d.spawn_index = 0;
        d.curr = drone::level_state_at(dp, pos, away);
        d.prev = d.curr;
        // The ONLY order armed. No foe, no leash, no defend, no strike.
        d.raid.active = true;
        d.raid.target_pos = pump_pos;
        d.raid.pump_idx = 0;
        d.foe = drone::kFoeNone;
        d.engaged = false;
        d.leash.enabled = false;
        d.defend.active = false;
        d.strike.active = false;

        std::printf("\n--- arm: %s ---\n", arm.name);
        std::printf("  %6s %10s %9s %9s %8s %7s %8s %8s\n", "t[s]", "range[m]",
                    "align", "cmd_bank", "cmd_gam", "bank", "agl", "closest");

        double closest = 1e300;
        const double dt = kAp.sim_dt;
        const int ticks = static_cast<int>(120.0 / dt);  // two minutes
        for (int k = 0; k <= ticks; ++k) {
            const glm::dvec3 to = pump_pos - d.curr.position;
            const double range = glm::length(to);
            closest = std::min(closest, range);
            if ((k % static_cast<int>(5.0 / dt)) == 0) {
                const glm::dvec3 v = d.curr.velocity;
                const double vl = glm::length(v);
                const double align =
                    vl > 1e-6 ? glm::dot(v / vl, to / std::max(range, 1e-9))
                              : 0.0;
                // Recover the achieved bank about the local up.
                const glm::dvec3 lup = glm::normalize(d.curr.position);
                const glm::dvec3 body_up =
                    d.curr.orientation * glm::dvec3{0.0, 1.0, 0.0};
                const double bank =
                    std::acos(std::clamp(glm::dot(body_up, lup), -1.0, 1.0)) *
                    180.0 / 3.14159265358979;
                // What the RAID branch itself is asking for this tick — the
                // same aim point drone::tick's raid block builds, INCLUDING
                // the E17 attack altitude. Printing the bare aim_at(pump)
                // here would report a law the branch no longer flies: a true
                // number answering a question nobody asked.
                const glm::dvec3 aim_pt =
                    d.raid.target_pos +
                    glm::normalize(d.raid.target_pos) * dp.raid_attack_alt_m;
                const maverick::Steer cmd_st = maverick::aim_at(
                    d.curr, aim_pt, d.raid.k_az, d.raid.k_el, d.raid.bank_cap,
                    d.raid.gamma_cap);
                const double agl =
                    glm::length(d.curr.position) - hf.radius_at(lup);
                std::printf(
                    "  %6.1f %10.0f %9.3f %9.1f %8.1f %7.0f %8.0f %8.0f\n",
                    static_cast<double>(k) * dt, range, align,
                    cmd_st.target_bank * 180.0 / 3.14159265358979,
                    cmd_st.target_gamma * 180.0 / 3.14159265358979, bank, agl,
                    closest);
            }
            const drone::DroneTickResult tr = drone::tick(d, kAp, dp, &env, nullptr);
            // A respawn teleports the drone and silently invalidates the whole
            // arm — the first cut of this probe spawned below terrain and every
            // arm respawned on tick one. Report it loudly rather than printing
            // a curve that is not about the question.
            if (tr.respawned) {
                std::printf(
                    "  !! RESPAWNED at t=%.1f s — this arm measures nothing\n",
                    static_cast<double>(k) * dt);
                break;
            }
        }
        std::printf("  => closest approach over 120 s: %.0f m%s\n", closest,
                    closest <= 1000.0 ? "   (INSIDE the 1 km raid envelope)"
                                      : "   (NEVER re-entered the envelope)");
    }
    std::printf("\n=====================================================\n\n");
    SUCCEED();
}
