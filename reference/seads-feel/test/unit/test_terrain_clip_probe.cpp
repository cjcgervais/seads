// terrain-clip T1 — THE INSTRUMENT (no fix). Chad's report (2026-09-15): "I was
// able to fly into the small earth and fly inside it ... at the Murray entrance
// ... and then at Onaping pump and area."
//
// The lane law is numbers before cut. This file MEASURES, it never repairs:
//
//   P1  THE DRAWN-vs-CRASH GAP. sim::ground_contact grades the aircraft against
//       world::HeightField::radius_at (the FIELD). The eye sees the render mesh,
//       whose surface between vertices is render::facet_radius_at(hf, d, subdiv,
//       tiles) — the shipped planet is subdiv 200, tiles 2 (config/world.toml
//       [planet]), an effective ~59 m cell over an 8192x4096 DEM (~11.5 m
//       texel). Where the FACET rides ABOVE the FIELD the aircraft is legally
//       airborne INSIDE the visible ground. Sampled on a 2 m grid within 500 m
//       of Murray, Errington, the Onaping (Valley) surface pump, and two
//       controls.
//   P2  THE SUSPENSION FOOTPRINT. sim/step.cpp gates ground_contact on
//       !env->tunnels->contains(next.position): inside the net there is NO
//       crash surface at all. Measures how far out, and how high above local
//       grade, that suspension reaches at the two mouths.
//   P3  TUNNELLING / BELOW-SURFACE BEHAVIOUR. What the shipped cadence is and
//       what ground_contact actually does to an airframe placed UNDER the
//       surface at speed.
//
// Real DEM decoded headlessly with stb_image; STB_IMAGE_IMPLEMENTATION lives in
// test_tunnel_mesh.cpp (the one owner) — declarations only here.
//
// CAVEAT, stated up front: the runtime HeightField is post-ImageBlurGaussian
// (dem_blur_radius 3) and post-lake-flatten; this probe reads the raw baked DEM.
// Both the field and the facet are drawn from the SAME buffer either way, so the
// MECHANISM and the sign of the gap are exact; the magnitudes are the unblurred
// bound. Flagged in docs/terrain_clip/CLIP_INSTRUMENT.md.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "config/load_aircraft.h"
#include "render/sphere_param.h"
#include "sim/environment.h"
#include "sim/ground.h"
#include "sim/state.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

#define STBI_ONLY_PNG
#include "stb_image.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// config/world.toml [planet] + [ground], config/game.toml [ground] — the
// SHIPPED numbers, quoted so the probe grades the world Chad flies.
constexpr int kSubdiv = 200;
constexpr int kTiles = 2;
constexpr double kReliefScale = 350.0;
constexpr double kUOffset = 0.806;
constexpr double kContactHeightM = 2.45;
constexpr double kDeepPenetrationM = 50.0;
constexpr double kAmbientSnowM = 0.77;  // [snowpack] signed ambient fold

world::HeightField real_field() {
    world::HeightField hf;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/sudbury_dem.png";
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 3);
    if (px == nullptr) return hf;  // w stays 0 — caller skips
    hf.w = w;
    hf.h = h;
    hf.R = kAp.R;
    hf.relief_scale = kReliefScale;
    hf.u_offset = kUOffset;
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < hf.px.size(); ++i)
        hf.px[i] = world::dem16_unpack(px[i * 3 + 0], px[i * 3 + 1]);
    stbi_image_free(px);
    return hf;
}

world::HeightField uniform_field(double elev_m, double relief = 350.0) {
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

// The SHIPPED config/game.toml [tunnel] dials (the same set
// test_tunnel_audit_probe quotes; kept local so this probe grades what it reads).
world::TunnelParams shipped_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 20.0;
    tp.arena_a_m = 4200.0;
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.chambers_on = false;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;
    tp.trench_rim_m = 150.0;
    return tp;
}

struct Site {
    const char* name;
    glm::dvec3 dir;
};

// An orthonormal tangent pair at d (generic seed, no world axis aligned).
void tangents(glm::dvec3 d, glm::dvec3& e1, glm::dvec3& e2) {
    e1 = glm::normalize(glm::cross(d, glm::dvec3{0.31, 0.77, 0.56}));
    e2 = glm::normalize(glm::cross(d, e1));
}

glm::dvec3 offset_dir(glm::dvec3 d, glm::dvec3 e1, glm::dvec3 e2, double x,
                      double y) {
    return glm::normalize(d + (e1 * x + e2 * y) / kAp.R);
}

double pct(std::vector<double>& v, double q) {
    if (v.empty()) return 0.0;
    const std::size_t i =
        static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(i),
                     v.end());
    return v[i];
}

}  // namespace

// ---------------------------------------------------------------------------
// P1 + P2 — the grid probe over the real DEM.
// ---------------------------------------------------------------------------
TEST_CASE("CLIPPROBE drawn-vs-crash surface at the reported sites") {
    const world::HeightField hf = real_field();
    if (hf.w == 0) {
        WARN("assets/sudbury_dem.png missing — CLIPPROBE skipped");
        return;
    }
    REQUIRE(hf.w == 8192);

    const world::TunnelNet net = world::build_tunnel_net(shipped_tp(), &hf);

    const Site sites[] = {
        {"MURRAY_MOUTH", world::kTunnelMouthMurray},
        {"ERRINGTON_MOUTH", world::kTunnelMouthErrington},
        {"ONAPING_VALLEY_PUMP", world::kPumpValleySurface},
        {"CTRL_SUDBURY_PUMP", world::kPumpSudburySurface},
        {"CTRL_VALLEY_CENTER", world::kValleyCenterDir},
    };

    const std::string out =
        std::string(SEADS_ASSET_DIR) + "/../docs/terrain_clip/probe.tsv";
    std::FILE* f = std::fopen(out.c_str(), "wb");
    REQUIRE(f != nullptr);
    std::fprintf(f,
                 "# terrain-clip T1 probe. subdiv=%d tiles=%d contact_h=%.2f "
                 "ambient_snow=%.2f dem=%dx%d relief=%.1f (raw DEM, unblurred)\n",
                 kSubdiv, kTiles, kContactHeightM, kAmbientSnowM, hf.w, hf.h,
                 kReliefScale);
    std::fprintf(f, "site\tkind\tx_m\ty_m\tfield_r\tfacet_r\tgap_m\telev_m\tnote\n");

    const double half = 500.0, step = 2.0;
    std::printf("\n=========== TERRAIN-CLIP PROBE (P1/P2) ===========\n");
    for (const Site& s : sites) {
        glm::dvec3 e1, e2;
        tangents(s.dir, e1, e2);
        std::vector<double> gaps;
        gaps.reserve(251001);
        double worst = -1e9, worst_x = 0, worst_y = 0, worst_fi = 0,
               worst_fa = 0;
        double most_negative = 1e9;
        long n_half = 0, n_2 = 0, n_5 = 0, n_clip = 0, n_susp = 0, n = 0,
             n_coarse = 0;
        for (double x = -half; x <= half + 1e-9; x += step) {
            for (double y = -half; y <= half + 1e-9; y += step) {
                const glm::dvec3 d = offset_dir(s.dir, e1, e2, x, y);
                const double fi = hf.radius_at(d);
                const double fa = render::facet_radius_at(hf, d, kSubdiv, kTiles);
                const double gap = fa - fi;
                ++n;
                gaps.push_back(gap);
                if (gap > 0.5) ++n_half;
                if (gap > 2.0) ++n_2;
                if (gap > 5.0) ++n_5;
                // CLIP: the DRAWN surface (facet + ambient snow) rides above
                // the lowest legal airborne CG (field + contact height). The
                // airframe is then inside the visible ground with no crash.
                if (fa + kAmbientSnowM > fi + kContactHeightM) ++n_clip;
                // SUSPENSION: the net kills ground_contact at the contact
                // shell. Sampled on the 20 m sub-grid (the SDF mins over the
                // whole spine — too dear for every 2 m cell, and the net's
                // features are hundreds of metres across).
                const bool coarse = std::fmod(x, 20.0) == 0.0 &&
                                    std::fmod(y, 20.0) == 0.0;
                if (coarse) {
                    ++n_coarse;
                    if (net.contains(d * (fi + kContactHeightM))) ++n_susp;
                }
                if (gap > worst) {
                    worst = gap;
                    worst_x = x;
                    worst_y = y;
                    worst_fi = fi;
                    worst_fa = fa;
                }
                most_negative = std::min(most_negative, gap);
                if (coarse)
                    std::fprintf(
                        f, "%s\tgrid\t%.0f\t%.0f\t%.3f\t%.3f\t%.3f\t%.2f\t-\n",
                        s.name, x, y, fi, fa, gap, fi - kAp.R);
            }
        }
        const double p50 = pct(gaps, 0.50), p99 = pct(gaps, 0.99),
                     p999 = pct(gaps, 0.999);
        std::fprintf(f,
                     "%s\tsummary\t%.0f\t%.0f\t%.3f\t%.3f\t%.3f\t%.2f\t"
                     "n=%ld p50=%.3f p99=%.3f p999=%.3f max=%.3f min=%.3f "
                     "gt0.5=%ld gt2=%ld gt5=%ld clip=%ld susp=%ld\n",
                     s.name, worst_x, worst_y, worst_fi, worst_fa, worst,
                     worst_fi - kAp.R, n, p50, p99, p999, worst, most_negative,
                     n_half, n_2, n_5, n_clip, n_susp, n_coarse);
        std::printf(
            "%-20s n=%ld  gap p50=%.2f p99=%.2f p99.9=%.2f MAX=%.2f m at "
            "(%.0f,%.0f)  MIN=%.2f  >0.5m=%.2f%%  >2m=%.3f%%  >5m=%.4f%%  "
            "CLIP=%.3f%%  SUSPENDED=%.3f%%\n",
            s.name, n, p50, p99, p999, worst, worst_x, worst_y, most_negative,
            100.0 * static_cast<double>(n_half) / static_cast<double>(n),
            100.0 * static_cast<double>(n_2) / static_cast<double>(n),
            100.0 * static_cast<double>(n_5) / static_cast<double>(n),
            100.0 * static_cast<double>(n_clip) / static_cast<double>(n),
            n_coarse > 0 ? 100.0 * static_cast<double>(n_susp) /
                               static_cast<double>(n_coarse)
                         : 0.0);
        // The instrument's own no-op guard: the grid must be non-trivial.
        REQUIRE(n > 200000);
    }

    // P1b — "AND AREA": the same gap swept COARSE (20 m) over a 6 km box about
    // each reported site plus the two controls, because the 500 m anchor grids
    // above are not where Chad necessarily was. Reports the CLIP census (the
    // drawn surface above the lowest legal airborne CG) and the worst cell.
    std::printf("\n--- P1b wide sweep (6 km box, 20 m step) ---\n");
    for (const Site& s : sites) {
        glm::dvec3 e1, e2;
        tangents(s.dir, e1, e2);
        double worst = -1e9, wx = 0, wy = 0, worst_neg = 1e9, nx = 0, ny = 0;
        long n = 0, n_clip = 0, n_wall = 0;
        for (double x = -3000.0; x <= 3000.0 + 1e-9; x += 20.0) {
            for (double y = -3000.0; y <= 3000.0 + 1e-9; y += 20.0) {
                const glm::dvec3 d = offset_dir(s.dir, e1, e2, x, y);
                const double fi = hf.radius_at(d);
                const double fa = render::facet_radius_at(hf, d, kSubdiv, kTiles);
                const double gap = fa - fi;
                ++n;
                // CLIP = airborne INSIDE the drawn ground.
                if (fa + kAmbientSnowM > fi + kContactHeightM) ++n_clip;
                // WALL = the crash shell pokes above the drawn ground: an
                // invisible wall (the contrast class, same root cause).
                if (fi + kContactHeightM > fa + kAmbientSnowM + 2.0) ++n_wall;
                if (gap > worst) { worst = gap; wx = x; wy = y; }
                if (gap < worst_neg) { worst_neg = gap; nx = x; ny = y; }
            }
        }
        std::printf("%-20s n=%ld  CLIP=%.3f%%  INVISIBLE_WALL(>2m)=%.3f%%  "
                    "max_drawn_above=%.2f m at (%.0f,%.0f)  "
                    "max_drawn_below=%.2f m at (%.0f,%.0f)\n",
                    s.name, n,
                    100.0 * static_cast<double>(n_clip) / static_cast<double>(n),
                    100.0 * static_cast<double>(n_wall) / static_cast<double>(n),
                    worst, wx, wy, worst_neg, nx, ny);
        std::fprintf(f, "%s\twide6km\t%.0f\t%.0f\t0\t0\t%.3f\t0\t"
                        "n=%ld clip=%ld wall=%ld worst_neg=%.3f at(%.0f,%.0f)\n",
                     s.name, wx, wy, worst, n, n_clip, n_wall, worst_neg, nx, ny);
    }

    // P2 — how HIGH above local grade the net suspends the crash surface at the
    // two mouths, and how far out the suspension reaches.
    for (int si = 0; si < 2; ++si) {
        const Site& s = sites[si];
        glm::dvec3 e1, e2;
        tangents(s.dir, e1, e2);
        double max_h = -1e9, max_reach = 0.0;
        for (double x = -700.0; x <= 700.0; x += 25.0) {
            for (double y = -700.0; y <= 700.0; y += 25.0) {
                const glm::dvec3 d = offset_dir(s.dir, e1, e2, x, y);
                const double gr = hf.radius_at(d);
                double h = -1e9;
                for (double a = 0.0; a <= 300.0; a += 5.0)
                    if (net.contains(d * (gr + a))) h = a;
                if (h > -1e8) {
                    max_h = std::max(max_h, h);
                    max_reach = std::max(max_reach, std::sqrt(x * x + y * y));
                }
            }
        }
        std::printf("%-20s net suspension: max %.0f m ABOVE local grade, "
                    "reach %.0f m from the mouth axis\n",
                    s.name, max_h, max_reach);
        std::fprintf(f, "%s\tsuspension\t0\t0\t0\t0\t%.1f\t0\treach_m=%.0f\n",
                     s.name, max_h, max_reach);
    }
    std::fclose(f);
}

// ---------------------------------------------------------------------------
// P3 — cadence + the below-surface behaviour of the shipped contact law.
// ---------------------------------------------------------------------------
TEST_CASE("CLIPPROBE cadence: the contact test is a POINT sample, not swept") {
    const double dt = kAp.sim_dt;
    REQUIRE(dt > 0.0);
    const double per_tick_120 = 120.0 * dt;
    const double per_tick_200 = 200.0 * dt;
    std::printf("\nP3 sim_dt=%.8f s  ->  %.3f m/tick at 120 m/s, %.3f m/tick "
                "at 200 m/s; deep_penetration_m=%.1f\n",
                dt, per_tick_120, per_tick_200, kDeepPenetrationM);
    // The step is FAR under the deep-penetration threshold at any flyable
    // speed: a point sample cannot skip the surface. Tunnelling is killed as a
    // suspect by arithmetic, not by opinion.
    REQUIRE(per_tick_200 < kDeepPenetrationM);
    REQUIRE(per_tick_200 < 2.0);
}

TEST_CASE("CLIPPROBE below-surface: what happens to an airframe under ground") {
    const world::HeightField hf = uniform_field(120.0);
    sim::GroundParams gp;
    gp.contact_height_m = kContactHeightM;
    gp.slope_limit_cos = std::cos(20.0 * 3.14159265358979323846 / 180.0);
    gp.normal_probe_m = 60.0;
    gp.max_sink_ms = 6.0;
    gp.deep_penetration_m = kDeepPenetrationM;
    gp.friction = 0.02;
    gp.brake_friction = 0.0;
    gp.ground_loop_lat_g = 0.0;

    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const glm::dvec3 head =
        glm::normalize(glm::cross(up, glm::dvec3{0.2, 0.9, 0.4}));
    const double r_s = hf.radius_at(up) + gp.contact_height_m;
    const double dt = kAp.sim_dt;

    // (1) ONE METRE under the contact shell, 100 m/s LEVEL (no sink).
    {
        sim::SimState st{};
        st.position = up * (r_s - 1.0);
        st.velocity = head * 100.0;
        st.on_ground = false;
        sim::SimState next = st;
        next.position = st.position + next.velocity * dt;
        sim::ground_contact(st, next, hf, gp, kAp, 0.0, dt);
        const double r_after = glm::length(next.position);
        std::printf("P3a  1 m under, 100 m/s level: crashed=%d on_ground=%d "
                    "r_after-r_s=%+.3f m\n",
                    static_cast<int>(next.crashed),
                    static_cast<int>(next.on_ground), r_after - r_s);
        REQUIRE(next.crashed == false);
        REQUIRE(next.on_ground == true);
        REQUIRE(r_after > r_s - 1e-6);  // EJECTED UP onto the shell
    }
    // (2) 60 m under (past deep_penetration_m), same speed.
    {
        sim::SimState st{};
        st.position = up * (r_s - 60.0);
        st.velocity = head * 100.0;
        st.on_ground = false;
        sim::SimState next = st;
        next.position = st.position + next.velocity * dt;
        sim::ground_contact(st, next, hf, gp, kAp, 0.0, dt);
        std::printf("P3b  60 m under, 100 m/s level: crashed=%d (deep-"
                    "penetration wall strike)\n",
                    static_cast<int>(next.crashed));
        REQUIRE(next.crashed == true);
    }
    // (3) The SIGN-CONVENTION leg: there is no altitude = r - R path that can
    // read a below-surface state as airborne. The airborne gate is the direct
    // compare r > radius_at + contact_height, so 1 mm under is CONTACT.
    {
        sim::SimState st{};
        st.position = up * (r_s - 0.001);
        st.velocity = head * 100.0 - up * 30.0;  // a real descent, not a graze
        st.on_ground = false;
        sim::SimState next = st;
        next.position = st.position + next.velocity * dt;
        sim::ground_contact(st, next, hf, gp, kAp, 0.0, dt);
        std::printf("P3c  1 mm under, 30 m/s sink: crashed=%d (the gate is a "
                    "radius compare, never a signed altitude)\n",
                    static_cast<int>(next.crashed));
        REQUIRE(next.crashed == true);
    }
}
