// ============================================================================
// E17 TRACK PROBE (hidden, a measuring instrument — never a gate leg).
//
// WHY: Chad's tape 10 (2026-08-24) shows THREE of four AI tunnel runs ending in
// a terrain crash, and all three died at a nearly IDENTICAL RADIUS (11240 /
// 11224 / 11239 m) at lateral points kilometres apart — while the fourth run
// threaded the bore and came out the far mouth, and Chad himself flew 440 m
// DEEPER than any of them and lived. A constant death radius across unrelated
// lateral positions is a SHELL, not terrain relief.
//
// WHAT IT DOES: replays the recorded tracks through the SHIPPED tunnel net and
// prints, per sample, the net signed distance, containment, radius, and the
// arc-length estimate the maverick brain would be carrying. It flies nothing —
// it is a pure geometry query against the same net the crash surface uses.
//
// ★ THE PARAMS COME FROM THE REAL LOADER, never a hand-copied table. The
// sibling probe in test_tunnel_audit_probe.cpp carries a `shipped_tp()`
// transcription of config/game.toml; a constant that DESCRIBES the shipped
// table stops describing it the moment the table moves, and nothing goes red.
// This probe calls cfg::load_game_toml and mirrors app/main.cpp's assignment
// block, so it cannot fork from what the game builds.
//
// RUN: ./build/seads_tests.exe "[.e17track]" -s
//      SEADS_TRACK_CSV=<path> overrides the default track file.
// ============================================================================

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <glm/glm.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "world/heightfield.h"
#include "world/tunnel_net.h"

// STB_IMAGE_IMPLEMENTATION lives in test_tunnel_mesh.cpp (the one owner); we
// take only the declarations and resolve at link.
#define STBI_ONLY_PNG
#include "stb_image.h"

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// The net params EXACTLY as app/main.cpp builds them, off the real loader.
world::TunnelParams shipped_tp_from_loader() {
    const cfg::GameParams game =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kP);
    world::TunnelParams tp;
    tp.sphere_R = kP.R;
    tp.tube_width_m = game.tunnel.tube_width_m;
    tp.tube_height_m = game.tunnel.tube_height_m;
    tp.depth_m = game.tunnel.depth_m;
    tp.soft_m = game.tunnel.soft_m;
    tp.ramp_frac = game.tunnel.ramp_frac;
    tp.spacing_m = game.tunnel.spacing_m;
    tp.floor_height_m = game.tunnel.floor_height_m;
    tp.arena_a_m = game.tunnel.arena_a_m;
    tp.arena_c_m = game.tunnel.arena_c_m;
    tp.arena_depth_m = game.tunnel.arena_depth_m;
    tp.cavern_core_m = game.tunnel.cavern_core_m;
    tp.breach_margin_m = game.tunnel.breach_margin_m;
    tp.chamber_long_m = game.tunnel.chamber_long_m;
    tp.chamber_lat_m = game.tunnel.chamber_lat_m;
    tp.chamber_vert_m = game.tunnel.chamber_vert_m;
    tp.chamber_breach_offset_m = game.tunnel.chamber_breach_offset_m;
    tp.connector_radius_m = game.tunnel.connector_radius_m;
    tp.chambers_on = game.tunnel.chambers_on;
    tp.bowl_radius_m = game.tunnel.bowl_radius_m;
    tp.bowl_depth_m = game.tunnel.bowl_depth_m;
    tp.mouth_sink_m = game.tunnel.mouth_sink_m;
    tp.min_cover_m = game.tunnel.min_cover_m;
    tp.trench_len_m = game.tunnel.trench_len_m;
    tp.trench_rim_m = game.tunnel.trench_rim_m;
    tp.headframe_h_m = game.tunnel.headframe_h_m;
    tp.headframe_on = game.tunnel.headframe_on;
    return tp;
}

// The real Sudbury DEM, decoded headlessly (same path/params as the T13 A4 leg
// in test_tunnel_audit_probe.cpp).
world::HeightField load_real_dem() {
    world::HeightField hf;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/sudbury_dem.png";
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 3);
    if (px == nullptr) return hf;  // w stays 0
    hf.w = w;
    hf.h = h;
    hf.R = kP.R;
    hf.relief_scale = 350.0;  // config/world.toml [ground] relief_scale_m
    hf.u_offset = 0.806;      // config/world.toml [planet] u_offset
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < hf.px.size(); ++i)
        hf.px[i] = world::dem16_unpack(px[i * 3 + 0], px[i * 3 + 1]);
    stbi_image_free(px);
    return hf;
}

struct Sample {
    std::string track;
    long long tick = 0;
    glm::dvec3 pos{0.0};
    int mav = 0;
    int net = 0;
};

std::vector<Sample> load_tracks(const std::string& path) {
    std::vector<Sample> out;
    std::ifstream f(path);
    if (!f) return out;
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string tok;
        Sample s;
        std::getline(ss, s.track, ',');
        std::getline(ss, tok, ',');
        s.tick = std::atoll(tok.c_str());
        std::getline(ss, tok, ',');
        s.pos.x = std::atof(tok.c_str());
        std::getline(ss, tok, ',');
        s.pos.y = std::atof(tok.c_str());
        std::getline(ss, tok, ',');
        s.pos.z = std::atof(tok.c_str());
        std::getline(ss, tok, ',');
        s.mav = std::atoi(tok.c_str());
        std::getline(ss, tok, ',');
        s.net = std::atoi(tok.c_str());
        out.push_back(s);
    }
    return out;
}

const char* mav_name(int m) {
    switch (m) {
        case 0: return "PATROL";
        case 1: return "TRANSIT";
        case 2: return "DIVE_IN";
        case 3: return "RUN";
        case 4: return "CLIMB_OUT";
        default: return "?";
    }
}

}  // namespace

TEST_CASE("E17 tunnel track probe", "[.e17track]") {
    const world::HeightField hf = load_real_dem();
    if (hf.w == 0) {
        WARN("sudbury_dem.png missing — E17 track probe skipped");
        SUCCEED();
        return;
    }
    const world::TunnelParams tp = shipped_tp_from_loader();
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const maverick::TunnelRoute route(net);
    const maverick::MaverickParams mp;  // the shipped tracking-law table

    // ---- the spine's own depth profile (what the bore actually does) -------
    std::printf("\n=========== E17: THE SHIPPED SPINE ===========\n");
    std::printf("nodes=%zu  arc_L=%.1f m  arena_on=%d\n", net.spine.size(),
                route.L, net.arena_on ? 1 : 0);
    {
        double s = 0.0, min_r = 1e300;
        std::size_t min_i = 0;
        for (std::size_t i = 0; i < net.spine.size(); ++i) {
            const double r = glm::length(net.spine[i].pos);
            if (r < min_r) {
                min_r = r;
                min_i = i;
            }
        }
        for (std::size_t i = 1; i <= min_i; ++i)
            s += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
        const double surf =
            hf.radius_at(glm::normalize(net.spine[min_i].pos));
        std::printf(
            "deepest spine node: i=%zu  s=%.1f m  |pos|=%.1f  local_surf=%.1f  "
            "depth_below_terrain=%.1f m\n",
            min_i, s, min_r, surf, surf - min_r);
    }
    if (net.arena_on) {
        const auto& a = net.arena;
        std::printf(
            "arena: |center|=%.1f  a_pos=%.1f a_neg=%.1f b=%.1f c=%.1f\n",
            glm::length(a.center), a.a_pos, a.a_neg, a.b, a.c);
        std::printf("arena radial span: apex~%.1f  floor~%.1f\n",
                    glm::length(a.center) + a.c, glm::length(a.center) - a.c);
    }
    std::printf("cavern_core_m (solid |p| floor) = %.1f\n", tp.cavern_core_m);

    // ---- the recorded tracks through that net -----------------------------
    const char* env_csv = std::getenv("SEADS_TRACK_CSV");
    const std::string csv =
        env_csv != nullptr ? std::string(env_csv) : std::string("tunnel_tracks.csv");
    const std::vector<Sample> samples = load_tracks(csv);
    if (samples.empty()) {
        std::printf("\n(no track CSV at '%s' — geometry report only)\n\n",
                    csv.c_str());
        SUCCEED();
        return;
    }
    std::printf("\n=========== E17: THE RECORDED TRACKS (%zu samples) ==========\n",
                samples.size());

    std::string cur;
    double s_est = 0.0;
    double last_r = 0.0;
    bool was_in = false;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const Sample& s = samples[i];
        if (s.track != cur) {
            cur = s.track;
            s_est = 0.0;
            was_in = false;
            last_r = 0.0;
            std::printf("\n--- track %s ---\n", cur.c_str());
        }
        const double r = glm::length(s.pos);
        const double sd = net.signed_distance(s.pos);
        const bool in = net.contains(s.pos);
        const double surf = hf.radius_at(glm::normalize(s.pos));
        // The brain's own progress estimate, stepped exactly as bore_track does.
        s_est = route.nearest_s(s.pos, s_est);
        const double frac = route.L > 0.0 ? s_est / route.L : 0.0;
        // ★ WHAT THE TRACKING LAW ITSELF COMMANDED at this point. The GAMMA
        // channel does not depend on orientation (only on position and the
        // reference arc), so it can be reconstructed faithfully from the tape
        // even though attitude is not a recorded field — and gamma is the
        // channel that matters here, because these three SANK. This is the
        // differential that decides whether the law asked them to descend or
        // asked them to climb and they could not.
        const glm::dvec3 lup = glm::normalize(s.pos);
        const glm::dvec3 here_c = route.point_ext(s_est).pos;
        const double alt_err = glm::dot(here_c - s.pos, lup);
        const double gpath = maverick::path_climb_angle(
            route, s_est + mp.gamma_lookahead_m, 1, lup);
        const double cmd_gamma =
            std::clamp(mp.slope_ff_gain * gpath + mp.track_gain * alt_err,
                       -mp.run_gamma_cap, mp.run_gamma_cap);
        const bool guard_would_fire = sd > -mp.wall_margin_m;

        // Print sparsely: every 60 samples, plus every containment edge, plus
        // the last two samples of the track (the wreck).
        const bool edge = (in != was_in);
        const bool last = (i + 2 >= samples.size()) ||
                          (samples[i + 1].track != s.track) ||
                          (i + 2 < samples.size() && samples[i + 2].track != s.track);
        if (edge || last || (i % 60) == 0 || std::fabs(r - last_r) > 900.0) {
            std::printf(
                "  t=%7.2f  R=%8.1f  depth=%7.1f  sd=%9.2f %s  s_est=%8.1f "
                "(%.3f)  alt_err=%8.1f  cmd_gam=%6.1f %s  mav=%-9s "
                "tape_net=%d%s\n",
                static_cast<double>(s.tick) * 0.008333, r, surf - r, sd,
                in ? "IN " : "OUT", s_est, frac, alt_err,
                cmd_gamma * 180.0 / 3.14159265358979,
                guard_would_fire ? "GUARD" : "  -  ", mav_name(s.mav), s.net,
                edge ? "   <== CONTAINMENT EDGE" : "");
            last_r = r;
        }
        was_in = in;
    }
    std::printf("\n==============================================\n\n");
    SUCCEED();
}

// ============================================================================
// E18 DIVE PROBE (hidden, [.e18dive]) — ★ THE CLOSED-LOOP TUNNEL PROBE ON THE
// SHIPPED NET + REAL DEM that E17's handoff named as the missing instrument.
//
// It FLIES. Seeded from the exact states Chad's tape 10 recorded at the tick
// each striker's divert armed (assigned pump inside strike_engage_m), on the
// shipped tunnel net over the real Sudbury DEM, and ticked through the real
// drone::tick. The question it answers is the only one that matters here:
// DOES THE STRIKER REACH THE DEEP PUMP, OR DOES IT BURY ITSELF SHORT OF IT?
//
// THE DEFECT IT REPRODUCES (tapes 10 and 11, seven wrecks, both tapes):
//   divert arms  R ~15020 (at the MOUTH), slant ~8990 m to the assigned pump
//                -- i.e. right at the SHIPPED strike_engage_m of 9000
//   ~63 s later  R ~11245, DEAD, still 2.3-3.6 km SHORT of the pump
//   every wreck  33-54 m BELOW the deep pumps' own radius (11289.9)
// The pump is ~3750 m below over ~8160 m of ground track: a 24.7 deg glide.
// strike_k_el 3.0 times that is 74 deg, so the command SATURATES at the
// shipped 50 deg cap and holds there -- and at 50 deg over 8160 m you spend
// 9715 m of altitude when only 3750 m exists. A 2.6x overspend, so the
// striker reaches the arena FLOOR kilometres before it reaches the pump.
//
// ⚠ THE FIRST CUT OF THIS ANALYSIS USED StrikeOrder{}'s STRUCT DEFAULTS
// (6500 m / 35 deg / 600 m) INSTEAD OF THE SHIPPED TABLE (9000 / 50 / 2000),
// and "arms at ~6490 m" was an artifact of a 6500 threshold in the analysis
// script rather than anything the game does. This probe reads config.
//
// The pump positions are the SHIPPED ones, read out of Chad's own tape header
// ('pmp'), so this probe cannot drift from the world he flew.
// ============================================================================
TEST_CASE("E18 deep-pump dive probe", "[.e18dive]") {
    const world::HeightField hf = load_real_dem();
    if (hf.w == 0) {
        WARN("sudbury_dem.png missing — E18 dive probe skipped");
        SUCCEED();
        return;
    }
    const world::TunnelParams tp = shipped_tp_from_loader();
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const cfg::GameParams game =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kP);

    sim::AtmosphereField af;
    af.deck_agl_m = game.atmosphere.deck_agl_m;
    af.deck_soft_m = game.atmosphere.deck_soft_m;
    af.deck_terrain_relative = game.atmosphere.deck_terrain_relative;
    af.dome_exponent = game.atmosphere.bubble_dome_exponent;

    sim::Environment env;
    env.ground = &hf;
    env.atm = &af;
    env.tunnels = &net;  // ★ the net is live: the crash predicate yields inside
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    env.ground_params = gp;

    // The SHIPPED deep pumps, verbatim from tape 10's 'pmp' header.
    const glm::dvec3 kPump2{-4280.7, -1483.2, 10341.1};
    const glm::dvec3 kPump3{509.4, -2953.0, 10885.0};

    // The recorded divert-arm states (tape 10), pos + vel + assigned pump.
    struct Seed {
        const char* who;
        glm::dvec3 pos;
        glm::dvec3 vel;
        glm::dvec3 pump;
    };
    const Seed seeds[] = {
        {"i=7 @ t=204.0", {-8069.499, -1006.702, 12649.030},
         {164.911, -27.725, -66.486}, kPump3},
        {"i=2 @ t=240.6", {3407.614, -4521.828, 13894.053},
         {-149.143, 44.981, -72.896}, kPump2},
        {"i=9 @ t=334.8", {-8094.110, -1035.351, 12652.894},
         {160.871, -29.074, -67.455}, kPump3},
    };

    const cfg::ScenarioParams scen =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kP);

    struct Arm {
        const char* label;
        double attack_alt_m;
        bool glide_limit;
    };
    // ★ AN ARM OVERRIDES CONFIG, IT NEVER DEFINES IT. The OFF arm pins the
    // documented no-op values; SHIPPED reads whatever config says.
    const Arm arms[] = {
        {"OFF     (pre-E18 divert)", 0.0, false},
        {"alt only", 300.0, false},
        {"glide only", 0.0, true},
        {"alt+glide", 300.0, true},
        {"alt 150  ", 150.0, false},
        {"alt 500  ", 500.0, false},
        {"SHIPPED ", scen.drone.strike_attack_alt_m,
         scen.drone.strike_glide_limit},
    };

    std::printf("\n=========== E18: DOES THE STRIKER REACH THE PUMP? ==========\n");
    std::printf("deep pump radii: |pump2|=%.1f  |pump3|=%.1f\n",
                glm::length(kPump2), glm::length(kPump3));
    std::printf("strike dials: engage %.0f m  k_el %.2f  gamma_cap %.0f deg  "
                "station %.0f m\n",
                scen.drone.strike_engage_m, scen.drone.strike_k_el,
                scen.drone.strike_gamma_cap * 180.0 / 3.14159265358979,
                scen.drone.strike_station_range_m);

    for (const Arm& a : arms) {
        std::printf("\n--- arm: %s  (attack_alt %.0f m, glide_limit %d) ---\n",
                    a.label, a.attack_alt_m, a.glide_limit ? 1 : 0);
        std::printf("  %-14s %10s %12s %12s %10s %8s %5s\n", "seed", "outcome",
                    "closest[m]", "min_R", "R-R_pump", "strk_tk", "mav");
        for (const Seed& sd : seeds) {
            drone::DroneParams dp = scen.drone;
            dp.strike_attack_alt_m = a.attack_alt_m;
            dp.strike_glide_limit = a.glide_limit;

            drone::DroneState d;
            d.spawn_index = 7;
            const glm::dvec3 up = glm::normalize(sd.pos);
            const glm::dvec3 fwd =
                glm::normalize(sd.vel - glm::dot(sd.vel, up) * up);
            d.curr = drone::level_state_at(dp, sd.pos, fwd);
            d.curr.velocity = sd.vel;  // the RECORDED velocity, not a fresh one
            d.prev = d.curr;
            // A committed run, with the divert armed on the assigned pump.
            d.mav.inited = true;
            d.mav.mode = maverick::MaverickState::Mode::RUN;
            d.mav.run_dir = 1;
            d.mav.s_est = 0.0;
            d.strike.active = true;
            d.strike.target_pos = sd.pump;
            d.strike.pump_idx = 2;
            d.strike.engage_m = dp.strike_engage_m;
            d.strike.k_az = dp.strike_k_az;
            d.strike.k_el = dp.strike_k_el;
            d.strike.bank_cap = dp.strike_bank_cap;
            d.strike.gamma_cap = dp.strike_gamma_cap;
            d.strike.bail_s = dp.strike_bail_s;
            d.strike.station_range_m = dp.strike_station_range_m;
            d.strike.station_cos = dp.strike_station_cos;
            d.foe = drone::kFoeNone;
            d.engaged = false;
            d.leash.enabled = false;
            d.raid.active = false;

            double closest = 1e300, min_r = 1e300;
            const char* outcome = "flew on";
            bool reached_station = false;
            const double dt = kP.sim_dt;
            const int ticks = static_cast<int>(90.0 / dt);
            for (int k = 0; k <= ticks; ++k) {
                closest = std::min(closest,
                                   glm::length(sd.pump - d.curr.position));
                min_r = std::min(min_r, glm::length(d.curr.position));
                const drone::DroneTickResult tr =
                    drone::tick(d, kP, dp, &env, nullptr);
                if (tr.respawned) {
                    outcome = "CRASHED";
                    break;
                }
                // NOTE: do NOT stop at on-station. The first cut of this
                // probe broke out the moment `closest` fell inside
                // strike_station_range_m, which hid the outcome that matters:
                // in Chad's game these strikers died 2.3-3.6 km SHORT, i.e.
                // they never reached station at all. Fly the whole budget and
                // report whether the aeroplane survived it.
                if (closest <= dp.strike_station_range_m) reached_station = true;
            }
            // ★ strk_tk == 0 means THE DIVERT BRANCH NEVER RAN, and the arm
            // measures nothing about E18 — the liveness lesson E17 paid for.
            std::printf("  %-14s %10s %12.0f %12.0f %+10.1f %8lld %5d\n",
                        sd.who, outcome, closest, min_r,
                        min_r - glm::length(sd.pump),
                        static_cast<long long>(d.strike_ticks),
                        static_cast<int>(d.mav.mode));
        }
    }
    std::printf("\n===========================================================\n\n");
    SUCCEED();
}
