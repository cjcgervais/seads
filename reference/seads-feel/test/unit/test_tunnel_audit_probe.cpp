// AUDIT PROBE (temporary, uncommitted) — quantify the SHIPPED tunnel net
// structure against the pilot's complaints C1/C2/C3. Builds the net with the
// exact config/game.toml [tunnel] dials over the test_ground uniform field
// (the shipped app uses the real Sudbury heightfield; the great-circle
// geometry, leg lengths, and arena position are terrain-insensitive to
// tens-of-metres, so a uniform field is faithful for the structural report —
// terrain height at the two mouths (Q4) is flagged where it needs the real
// field). Run: ctest -R AUDIT --output-on-failure  (prints to stdout).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "config/load_aircraft.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

// T13 A4 — the REAL Sudbury DEM decoded HEADLESSLY. STB_IMAGE_IMPLEMENTATION
// lives in test_tunnel_mesh.cpp (the one owner — that TU's T5d kill test); here
// we only need the DECLARATIONS (stbi_load / stbi_image_free), resolved at link
// against that TU's implementation. Do NOT define STB_IMAGE_IMPLEMENTATION here
// (a duplicate would collide at link).
#define STBI_ONLY_PNG
#include "stb_image.h"

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// The SHIPPED config/game.toml [tunnel] dials (verbatim as of this branch).
world::TunnelParams shipped_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kP.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 20.0;  // SHIPPED (test_tp uses 0.0)
    tp.arena_a_m = 4200.0;     // T13 centering (game.toml; was 7350.0)
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;  // T13 entry approach trench (game.toml)
    tp.trench_rim_m = 150.0;
    return tp;
}

world::HeightField uniform_field(double elev_m, double relief = 4000.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kP.R;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

double arc_at_node(const world::TunnelNet& net, std::size_t idx) {
    double s = 0.0;
    for (std::size_t i = 1; i <= idx && i < net.spine.size(); ++i)
        s += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
    return s;
}

// T13 A4 — load the real Sudbury DEM (the SAME path/params test_tunnel_mesh.cpp
// T5d uses). Returns hf.w == 0 if the PNG is missing (the caller skips).
world::HeightField load_real_dem() {
    world::HeightField hf;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/sudbury_dem.png";
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 3);  // force RGB
    if (px == nullptr) return hf;                                   // w stays 0
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

}  // namespace

TEST_CASE("AUDIT tunnel structural report") {
    const world::TunnelParams tp = shipped_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const auto& sp = net.spine;
    const std::size_t N = sp.size();

    std::printf("\n================ TUNNEL AUDIT PROBE ================\n");

    // ---- Q1: total arc length + node count ----
    std::vector<double> arc(N, 0.0);
    for (std::size_t i = 1; i < N; ++i)
        arc[i] = arc[i - 1] + glm::length(sp[i].pos - sp[i - 1].pos);
    const double total_arc = arc.back();
    std::printf(
        "Q1 node_count=%zu total_spine_arc_m=%.1f mean_spacing_m=%.2f\n", N,
        total_arc, total_arc / (N - 1));

    // Straight great-circle arc for reference.
    const double dotm = std::clamp(
        glm::dot(world::kTunnelMouthErrington, world::kTunnelMouthMurray), -1.0,
        1.0);
    const double gc_arc = std::acos(dotm) * kP.R;
    std::printf(
        "   great_circle_surface_arc_m=%.1f (angle=%.4f rad = %.3f deg)\n",
        gc_arc, std::acos(dotm), std::acos(dotm) * 180.0 / M_PI);

    // ---- Q2: breach nodes (first node inside arena from each end) ----
    auto inside_arena = [&](const glm::dvec3& p) {
        return world::TunnelNet{net}.arena_on
                   ? false
                   : false;  // placeholder, replaced below
    };
    (void)inside_arena;
    // Use the arena ellipsoid directly: reproduce ellipsoid_sd sign via net SDF
    // is confounded by the tube; instead test membership in the arena Pocket.
    auto in_arena = [&](const glm::dvec3& p) -> bool {
        const world::TunnelNet::Pocket& e = net.arena;
        const glm::dvec3 d = p - e.center;
        const glm::dvec3 q{glm::dot(d, e.u_long), glm::dot(d, e.u_lat),
                           glm::dot(d, e.u_up)};
        const double a = q.x >= 0.0 ? e.a_pos : e.a_neg;
        const double v = (q.x / a) * (q.x / a) + (q.y / e.b) * (q.y / e.b) +
                         (q.z / e.c) * (q.z / e.c);
        return v < 1.0;
    };
    std::size_t breach_E = 0, breach_M = 0;
    bool foundE = false, foundM = false;
    for (std::size_t i = 0; i < N; ++i)
        if (net.arena_on && in_arena(sp[i].pos)) {
            breach_E = i;
            foundE = true;
            break;
        }
    for (std::size_t i = N; i-- > 0;)
        if (net.arena_on && in_arena(sp[i].pos)) {
            breach_M = i;
            foundM = true;
            break;
        }
    std::printf("Q2 arena_on=%d\n", (int)net.arena_on);
    if (foundE && foundM) {
        const double leg_E = arc[breach_E];
        const double leg_M = total_arc - arc[breach_M];
        const double across = arc[breach_M] - arc[breach_E];
        std::printf(
            "   breach_E node=%zu arc=%.1f m | breach_M node=%zu "
            "arc=%.1f m\n",
            breach_E, arc[breach_E], breach_M, arc[breach_M]);
        std::printf("   LEG Errington->breach = %.1f m\n", leg_E);
        std::printf("   LEG across arena       = %.1f m\n", across);
        std::printf("   LEG breach->Murray     = %.1f m\n", leg_M);
        std::printf(
            "   *** C2 ASYMMETRY: E-leg %.1f vs M-leg %.1f  ratio "
            "M/E=%.2f  ratio E/M=%.2f\n",
            leg_E, leg_M, leg_M / leg_E, leg_E / leg_M);
        // Where does the arena CENTER project along the spine (nearest node)?
        std::size_t c_i = 0;
        double cbest = 1e30;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = glm::length(sp[i].pos - net.arena.center);
            if (d < cbest) {
                cbest = d;
                c_i = i;
            }
        }
        std::printf(
            "   arena.center nearest spine node=%zu arc=%.1f m "
            "(%.1f%% of total) dist_to_center=%.1f m\n",
            c_i, arc[c_i], 100.0 * arc[c_i] / total_arc, cbest);
    }

    // ---- Q3: arena center vs geodesic midpoint ----
    {
        const glm::dvec3 mid_dir = glm::normalize(
            glm::normalize(world::kTunnelMouthErrington) +
            glm::normalize(world::kTunnelMouthMurray));  // chord midpoint dir
        // True slerp midpoint direction:
        const double omega = std::acos(dotm);
        const double so = std::sin(omega);
        const glm::dvec3 slerp_mid =
            so < 1e-9 ? mid_dir
                      : glm::normalize(std::sin(0.5 * omega) / so *
                                           world::kTunnelMouthErrington +
                                       std::sin(0.5 * omega) / so *
                                           world::kTunnelMouthMurray);
        const glm::dvec3 c_dir = glm::normalize(net.arena.center);
        const double ang =
            std::acos(std::clamp(glm::dot(c_dir, slerp_mid), -1.0, 1.0));
        std::printf(
            "Q3 arena.center |r_c|=%.1f m  center_dir vs "
            "geodesic_midpoint offset=%.6f rad = %.2f m (arc)\n",
            glm::length(net.arena.center), ang, ang * kP.R);
        std::printf("   arena_r(r_c)=%.1f arena_apex_r=%.1f core_r=%.1f\n",
                    net.arena_r, net.arena_apex_r, net.core_r);
        // apex depth below local terrain at mid-path:
        const double terr_mid = hf.radius_at(slerp_mid);
        std::printf(
            "   terr_mid=%.1f apex_depth_below_terr=%.1f m  "
            "center_depth=%.1f m  floor_r=%.1f (core+? %.1f)\n",
            terr_mid, terr_mid - net.arena_apex_r, terr_mid - net.arena_r,
            net.arena_r - tp.arena_c_m,
            net.arena_r - tp.arena_c_m - net.core_r);
    }

    // ---- Q4: terrain radius at each mouth ----
    {
        const double rE = hf.radius_at(world::kTunnelMouthErrington);
        const double rM = hf.radius_at(world::kTunnelMouthMurray);
        std::printf(
            "Q4 [UNIFORM FIELD] terr_Errington=%.3f terr_Murray=%.3f "
            "diff=%.3f m  (REAL FIELD needed for true diff)\n",
            rE, rM, rE - rM);
        std::printf(
            "   |spine.front()|=%.3f (mouth_sink %.1f below surf) "
            "|spine.back()|=%.3f (bowl_depth %.1f below surf)\n",
            glm::length(sp.front().pos), tp.mouth_sink_m,
            glm::length(sp.back().pos), tp.bowl_depth_m);
    }

    // ---- Q5: side chambers + connectors ----
    std::printf("Q5 side chambers/connectors:\n");
    for (int k = 0; k < 2; ++k) {
        const world::TunnelNet::Pocket& ch = net.chamber[k];
        const world::TunnelNet::Capsule& cn = net.connector[k];
        // nearest spine node to the connector base (cn.a) & its arc:
        std::size_t bi = 0;
        double bbest = 1e30;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = glm::length(sp[i].pos - cn.a);
            if (d < bbest) {
                bbest = d;
                bi = i;
            }
        }
        const double conn_len = glm::length(cn.b - cn.a);
        // is the connector mouth (cn.a on the bore) inside the bore tube?
        const double sd_base = net.signed_distance(cn.a);
        const double sd_chamber_end = net.signed_distance(cn.b);
        // chamber center offset above breach: radius vs r_plateau+offset
        const double ch_r = glm::length(ch.center);
        // Does the chamber read as a dead end? Probe: is there any net-inside
        // continuation beyond the chamber (opposite the connector)?  Probe a
        // point past the far chamber wall along -u_up (away from bore):
        const glm::dvec3 far_wall = ch.center - ch.u_up * (ch.c * 1.3);
        const bool beyond_solid = !net.contains(far_wall);
        std::printf(
            "   chamber[%d]: center_r=%.1f  base_node=%zu arc=%.1f m  "
            "conn_len=%.1f m conn_radius=%.1f\n",
            k, ch_r, bi, arc[bi], conn_len, cn.r);
        std::printf(
            "     sd(connector base on bore)=%.2f m (%s)  sd(chamber "
            "end)=%.2f m (%s)  base_node_radius=%.1f\n",
            sd_base, sd_base < 0 ? "INSIDE bore" : "OUTSIDE bore",
            sd_chamber_end, sd_chamber_end < 0 ? "inside" : "OUTSIDE",
            glm::length(sp[bi].pos));
        std::printf(
            "     chamber offset above base node = %.1f m ; far wall (-u_up "
            "1.3c) solid=%d => %s\n",
            ch_r - glm::length(sp[bi].pos), (int)beyond_solid,
            beyond_solid ? "DEAD-END (no continuation past room)"
                         : "open past room");
    }
    std::printf("   chamber separation=%.1f m\n",
                glm::length(net.chamber[0].center - net.chamber[1].center));

    // ---- Q6: descent grade profile ----
    std::printf("Q6 grade profile (dr/ds) + tangent turn angle:\n");
    double max_grade = 0.0;
    std::size_t max_grade_i = 0;
    double max_turn = 0.0;
    std::size_t max_turn_i = 0;
    int kinked = 0;
    std::size_t plateau_start = 0, plateau_end = 0;
    double min_r = 1e30;
    for (std::size_t i = 0; i < N; ++i)
        min_r = std::min(min_r, glm::length(sp[i].pos));
    // plateau = nodes within 5 m of min_r
    bool inplat = false;
    for (std::size_t i = 0; i < N; ++i) {
        const double r = glm::length(sp[i].pos);
        if (r <= min_r + 5.0) {
            if (!inplat) {
                plateau_start = i;
                inplat = true;
            }
            plateau_end = i;
        }
    }
    for (std::size_t i = 1; i < N; ++i) {
        const double dr = glm::length(sp[i].pos) - glm::length(sp[i - 1].pos);
        const double ds = glm::length(sp[i].pos - sp[i - 1].pos);
        const double g = ds > 0 ? std::fabs(dr / ds) : 0.0;
        if (g > max_grade) {
            max_grade = g;
            max_grade_i = i;
        }
    }
    for (std::size_t i = 1; i + 1 < N; ++i) {
        const glm::dvec3 t0 = glm::normalize(sp[i].pos - sp[i - 1].pos);
        const glm::dvec3 t1 = glm::normalize(sp[i + 1].pos - sp[i].pos);
        const double a =
            std::acos(std::clamp(glm::dot(t0, t1), -1.0, 1.0)) * 180.0 / M_PI;
        if (a > max_turn) {
            max_turn = a;
            max_turn_i = i;
        }
        if (a > 15.0) kinked++;
    }
    std::printf(
        "   max_grade=%.4f (%.2f deg) at node=%zu arc=%.1f | plateau nodes "
        "[%zu..%zu] arc[%.1f..%.1f] min_r=%.1f\n",
        max_grade, std::atan(max_grade) * 180.0 / M_PI, max_grade_i,
        arc[max_grade_i], plateau_start, plateau_end, arc[plateau_start],
        arc[plateau_end], min_r);
    std::printf(
        "   max_tangent_turn=%.2f deg at node=%zu arc=%.1f | nodes with "
        "turn>15deg = %d\n",
        max_turn, max_turn_i, arc[max_turn_i], kinked);
    // monotonicity check on Errington leg to deep node
    std::size_t deep_i = 0;
    for (std::size_t i = 1; i < N; ++i)
        if (glm::length(sp[i].pos) < glm::length(sp[deep_i].pos)) deep_i = i;
    int nonmono = 0;
    for (std::size_t i = 1; i <= deep_i; ++i)
        if (glm::length(sp[i].pos) > glm::length(sp[i - 1].pos) + 1e-6)
            nonmono++;
    std::printf(
        "   deep_node=%zu arc=%.1f ; non-monotone stations on E-leg=%d\n",
        deep_i, arc[deep_i], nonmono);

    // ---- Q7: entry geometry both ends ----
    std::printf("Q7 entry geometry:\n");
    std::printf(
        "   Errington PIT: rim=%.1f (=%.2fx tube_width %.1f) sink=%.1f "
        "pit_depth=%.1f floor_r=%.1f surf_r=%.1f\n",
        net.pit.bowl_r, net.pit.bowl_r / tp.tube_width_m, tp.tube_width_m,
        tp.mouth_sink_m, net.pit.bowl_depth, net.pit.floor_r,
        net.pit.surface_r);
    std::printf(
        "   Murray BOWL:   rim=%.1f (=%.2fx tube_width) depth=%.1f "
        "floor_r=%.1f "
        "surf_r=%.1f\n",
        net.bowl.bowl_r, net.bowl.bowl_r / tp.tube_width_m, net.bowl.bowl_depth,
        net.bowl.floor_r, net.bowl.surface_r);
    // sight distance at both ends
    const double sight0 =
        world::centerline_sight_distance(net, 0.0, 3000.0, 10.0);
    const double sightEnd =
        world::centerline_sight_distance(net, total_arc, 3000.0, 10.0);
    const double sightMid =
        world::centerline_sight_distance(net, arc[deep_i], 3000.0, 10.0);
    std::printf(
        "   centerline_sight: s=0 -> %.1f m | s=deep(%.1f) -> %.1f m | "
        "s=end(%.1f) -> %.1f m\n",
        sight0, arc[deep_i], sightMid, total_arc, sightEnd);
    // Approach cone: straight chord from 500 m out along the mouth axis into
    // the mouth — does it stay inside cut+bore? Test at each mouth: eye 500 m
    // above the pit floor on the mouth axis, target = the bore mouth node.
    auto approach_ok = [&](const glm::dvec3& axis, const glm::dvec3& mouth_node,
                           double out_m) -> int {
        const glm::dvec3 eye = axis * (glm::length(mouth_node) + out_m);
        const glm::dvec3 chord = mouth_node - eye;
        const double clen = glm::length(chord);
        const glm::dvec3 dir = chord / clen;
        int outside_ticks = 0;
        for (double m = 5.0; m < clen; m += 5.0)
            if (net.signed_distance(eye + dir * m) >= 0.0) outside_ticks++;
        return outside_ticks;
    };
    std::printf(
        "   straight-down approach (500m out, on-axis): Errington "
        "outside_ticks=%d | Murray outside_ticks=%d (0 = clear dive)\n",
        approach_ok(world::kTunnelMouthErrington, sp.front().pos, 500.0),
        approach_ok(world::kTunnelMouthMurray, sp.back().pos, 500.0));

    // ---- Q8: anomalies ----
    std::printf("Q8 anomalies:\n");
    // (a) bore re-entering arena on the Murray up-ramp (a second breach exit):
    int arena_runs = 0;
    bool prev_in = false;
    for (std::size_t i = 0; i < N; ++i) {
        const bool in = net.arena_on && in_arena(sp[i].pos);
        if (in && !prev_in) arena_runs++;
        prev_in = in;
    }
    std::printf(
        "   arena membership runs along spine = %d (1 = single "
        "contiguous breach span; >1 = bore re-enters arena)\n",
        arena_runs);
    // (b) crown above terrain outside cut regions:
    const int seg = (int)N - 1;
    const int ramp_nodes = (int)(0.3 * seg);
    int crown_pokes = 0;
    double worst_poke = -1e30;
    for (std::size_t i = 0; i < N; ++i) {
        if ((int)i < ramp_nodes || (int)(N - 1 - i) < ramp_nodes) continue;
        const glm::dvec3 dir = glm::normalize(sp[i].pos);
        const double crown = glm::length(sp[i].pos) + net.tube_height;
        const double cover = hf.radius_at(dir) - crown;
        if (cover < 0) crown_pokes++;
        worst_poke = std::max(worst_poke, -cover);  // positive = poke amount
    }
    std::printf(
        "   crown-above-terrain nodes (outside cuts)=%d worst "
        "poke=%.1f m (neg = covered)\n",
        crown_pokes, worst_poke);
    // (c) connector actually intersects the bore?
    for (int k = 0; k < 2; ++k) {
        const double sd = net.signed_distance(net.connector[k].a);
        std::printf("   connector[%d] base sd on bore = %.2f m (%s)\n", k, sd,
                    sd < 0 ? "intersects bore OK" : "DOES NOT reach bore");
    }
    // ---- Q2b: DEEPER C2 analysis — where does each leg actually BREACH the
    // flyable arena (apex), and where are the visible destinations (chambers)?
    // C3: approach ALONG the bore tangent (the correct entry vector) vs the
    // radial dive tested in Q7. The first-segment tangent points down-bore.
    {
        const glm::dvec3 tanE = glm::normalize(sp[1].pos - sp[0].pos);
        const glm::dvec3 tanM = glm::normalize(sp[N - 2].pos - sp[N - 1].pos);
        auto tan_approach = [&](const glm::dvec3& mouth,
                                const glm::dvec3& into_dir, double out_m) {
            const glm::dvec3 eye = mouth - into_dir * out_m;  // back up-tube
            int bad = 0;
            for (double m = 5.0; m < out_m; m += 5.0)
                if (net.signed_distance(eye + into_dir * m) >= 0.0) bad++;
            return bad;
        };
        std::printf(
            "Q7b tangent-approach (along bore axis, 500m out): Errington "
            "outside_ticks=%d | Murray outside_ticks=%d\n",
            tan_approach(sp.front().pos, tanE, 500.0),
            tan_approach(sp.back().pos, tanM, 500.0));
        // grade angle at each mouth (bore tangent vs local horizontal):
        const glm::dvec3 upE = glm::normalize(sp.front().pos);
        const glm::dvec3 upM = glm::normalize(sp.back().pos);
        std::printf(
            "   mouth bore-dip angle: Errington=%.1f deg | Murray=%.1f deg "
            "(below local horizontal)\n",
            std::asin(std::clamp(glm::dot(tanE, -upE), -1.0, 1.0)) * 180.0 /
                M_PI,
            std::asin(std::clamp(glm::dot(tanM, -upM), -1.0, 1.0)) * 180.0 /
                M_PI);
    }

    std::printf("Q2b C2 deep-dive (why the pilot feels asymmetry):\n");
    // First node on each leg whose radius drops to/below the arena APEX radius
    // (arena_apex_r) — the true "you are now in the open room" crossing, since
    // the arena is oblate (vertical semi only 2600) and the wide horizontal
    // reach makes the ellipsoid-interior test fire far up-bore.
    std::size_t apexE = 0, apexM = 0;
    for (std::size_t i = 0; i < N; ++i)
        if (glm::length(sp[i].pos) <= net.arena_apex_r) {
            apexE = i;
            break;
        }
    for (std::size_t i = N; i-- > 0;)
        if (glm::length(sp[i].pos) <= net.arena_apex_r) {
            apexM = i;
            break;
        }
    std::printf(
        "   apex-radius crossing: E node=%zu arc=%.1f | M node=%zu arc=%.1f\n",
        apexE, arc[apexE], apexM, arc[apexM]);
    std::printf(
        "   LEG(apex) E->apex=%.1f  across=%.1f  apex->M=%.1f  ratio "
        "M/E=%.2f\n",
        arc[apexE], arc[apexM] - arc[apexE], total_arc - arc[apexM],
        (total_arc - arc[apexM]) / arc[apexE]);
    // chamber arc positions (the pilot's visible side-room destinations):
    for (int k = 0; k < 2; ++k) {
        std::size_t bi = 0;
        double bb = 1e30;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = glm::length(sp[i].pos - net.connector[k].a);
            if (d < bb) {
                bb = d;
                bi = i;
            }
        }
        std::printf("   chamber[%d] hangs at arc=%.1f (%.1f%% of total)\n", k,
                    arc[bi], 100.0 * arc[bi] / total_arc);
    }
    // Node radius at the breach nodes (how deep the arena reaches up-bore):
    std::printf(
        "   node radii: front=%.1f breachE(n%zu)=%.1f deep=%.1f breachM(n%zu)"
        "=%.1f back=%.1f  arena_apex_r=%.1f\n",
        glm::length(sp.front().pos), breach_E, glm::length(sp[breach_E].pos),
        min_r, breach_M, glm::length(sp[breach_M].pos),
        glm::length(sp.back().pos), net.arena_apex_r);

    // ---- T13: chamber-centering summary at the shipped arena_a_m ----
    // The three comparable "thirds" S2 demands: E-leg, room crossing, M-leg.
    // (These are the numbers the T13 chamber-centering gating leg in
    // test_tunnel.cpp pins — printed here for the ledger.)
    if (foundE && foundM) {
        const double legE = arc[breach_E];
        const double legM = total_arc - arc[breach_M];
        const double cross = arc[breach_M] - arc[breach_E];
        const double mean_leg = 0.5 * (legE + legM);
        std::printf(
            "T13 CENTERING (arena_a_m=%.1f): E-leg=%.1f  crossing=%.1f  "
            "M-leg=%.1f  leg-skew=%.1f%%  crossing/mean-leg=%.3f\n",
            tp.arena_a_m, legE, cross, legM,
            100.0 * std::fabs(legE - legM) / mean_leg, cross / mean_leg);
    }

    // ---- T13 (S3): entry-approach blocked-tick count, trench ON vs OFF ----
    // The geometric proxy for the LIVE crash predicate: a chord sample is
    // "blocked" when it is BELOW local terrain AND OUTSIDE the net+cuts (the
    // deep-penetration clause's inside-terrain-outside-volume condition). Walk
    // a straight chord along the mouth bore-tangent from `out_m` up-tangent
    // (above grade, approach side) into the mouth, counting blocked samples.
    // Report the Errington count ON and OFF (the trench must drive it to ~0) +
    // Murray as the easy reference.
    {
        auto blocked_ticks = [&](const world::TunnelNet& n,
                                 const glm::dvec3& mouth_node,
                                 const glm::dvec3& tan_into, double out_m) {
            const glm::dvec3 eye =
                mouth_node - tan_into * out_m;  // back up-tangent, above grade
            int bad = 0;
            for (int t = 0; t < 100; ++t) {
                const glm::dvec3 p = eye + tan_into * (out_m * t / 100.0);
                const glm::dvec3 dir = glm::normalize(p);
                const bool below = glm::length(p) < hf.radius_at(dir);
                if (below && !n.contains(p)) ++bad;  // in solid rock (a crash)
            }
            return bad;
        };
        // The mouth bore-tangent (down-bore) at each mouth.
        const glm::dvec3 tanE =
            glm::normalize(net.spine[1].pos - net.spine[0].pos);
        const glm::dvec3 tanM =
            glm::normalize(net.spine[N - 1].pos - net.spine[N - 2].pos);
        world::TunnelParams tp_off = tp;
        tp_off.trench_len_m = 0.0;  // trench OFF
        const world::TunnelNet net_off = world::build_tunnel_net(tp_off, &hf);
        std::printf(
            "T13 ENTRY (S3): Errington blocked_ticks ON=%d OFF=%d | Murray "
            "blocked_ticks=%d (out=600m, 100 samples)\n",
            blocked_ticks(net, net.spine.front().pos, tanE, 600.0),
            blocked_ticks(net_off, net_off.spine.front().pos, tanE, 600.0),
            blocked_ticks(net, net.spine.back().pos, tanM, 600.0));
    }

    std::printf("===================================================\n\n");

    // Always pass — this is a report, not a gate.
    CHECK(N > 3);
}

// T13 A4 — REAL-DEM MEASUREMENT. Builds the net over the real Sudbury
// heightfield at the shipped dials and reports the real terrain radius at each
// mouth, the real leg arcs, and the leg asymmetry ratio. The uniform-field
// report above is faithful for the great-circle geometry, but the real terrain
// heights at the two mouths (and thus the sink/bowl-relative geometry) need the
// real field. GATES ONLY on sanity (builds, arcs positive, ratio in
// [0.5, 2.0]); if the real leg SKEW exceeds 10% the supervisor defers
// auto-centering to a pilot ruling (WARN'd loud so --output-on-failure surfaces
// it).
TEST_CASE("AUDIT tunnel real-DEM leg measurement") {
    const world::HeightField hf = load_real_dem();
    if (hf.w == 0) {
        WARN("sudbury_dem.png missing — real-DEM audit leg skipped");
        SUCCEED();
        return;
    }
    const world::TunnelParams tp = shipped_tp();
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const auto& sp = net.spine;
    const std::size_t N = sp.size();
    REQUIRE(N >= 3);

    std::vector<double> arc(N, 0.0);
    for (std::size_t i = 1; i < N; ++i)
        arc[i] = arc[i - 1] + glm::length(sp[i].pos - sp[i - 1].pos);
    const double total_arc = arc.back();

    // Arena-interior breach nodes from each end (the room crossing), the same
    // predicate as the uniform report.
    auto in_arena = [&](const glm::dvec3& p) -> bool {
        const world::TunnelNet::Pocket& e = net.arena;
        const glm::dvec3 d = p - e.center;
        const glm::dvec3 q{glm::dot(d, e.u_long), glm::dot(d, e.u_lat),
                           glm::dot(d, e.u_up)};
        const double a = q.x >= 0.0 ? e.a_pos : e.a_neg;
        const double v = (q.x / a) * (q.x / a) + (q.y / e.b) * (q.y / e.b) +
                         (q.z / e.c) * (q.z / e.c);
        return v < 1.0;
    };
    std::size_t bE = 0, bM = 0;
    bool fE = false, fM = false;
    for (std::size_t i = 0; i < N; ++i)
        if (net.arena_on && in_arena(sp[i].pos)) {
            bE = i;
            fE = true;
            break;
        }
    for (std::size_t i = N; i-- > 0;)
        if (net.arena_on && in_arena(sp[i].pos)) {
            bM = i;
            fM = true;
            break;
        }

    const double rE = hf.radius_at(world::kTunnelMouthErrington);
    const double rM = hf.radius_at(world::kTunnelMouthMurray);
    std::printf(
        "\n=========== T13 REAL-DEM AUDIT (Sudbury heightfield) "
        "===========\n");
    std::printf(
        "real terr: Errington=%.3f m (%.1f AMSL) | Murray=%.3f m "
        "(%.1f AMSL) | diff=%.3f m\n",
        rE, rE - kP.R, rM, rM - kP.R, rE - rM);
    std::printf("total_spine_arc=%.1f m  nodes=%zu\n", total_arc, N);

    double skew_pct = 0.0;
    if (fE && fM) {
        const double leg_E = arc[bE];
        const double leg_M = total_arc - arc[bM];
        const double crossing = arc[bM] - arc[bE];
        const double mean_leg = 0.5 * (leg_E + leg_M);
        skew_pct = 100.0 * std::fabs(leg_E - leg_M) / mean_leg;
        std::printf(
            "real legs: E-leg=%.1f  crossing=%.1f  M-leg=%.1f  "
            "leg-skew=%.1f%%  ratio E/M=%.3f\n",
            leg_E, crossing, leg_M, skew_pct, leg_E / leg_M);
        // Sanity gate.
        REQUIRE(leg_E > 0.0);
        REQUIRE(leg_M > 0.0);
        const double ratio = leg_E / leg_M;
        REQUIRE(ratio >= 0.5);
        REQUIRE(ratio <= 2.0);
        if (skew_pct > 10.0)
            WARN("T13 REAL-DEM leg skew "
                 << skew_pct
                 << "% EXCEEDS 10% — auto-centering deferred to a pilot ruling "
                    "(the uniform-field arena_a_m=4200 assumes symmetric mouth "
                    "terrain).");
    }
    std::printf(
        "=================================================="
        "==========\n\n");
}
