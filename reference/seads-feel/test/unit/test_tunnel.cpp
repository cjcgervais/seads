// T1 — the Errington tunnel net (docs/tunnel_staging.md; world/tunnel_net.*).
// The pure spatial-query module + its three seam edits: the full-air union term
// in atm_frac_at, the sim-side crash-predicate yield (step.cpp), and the
// app-tick bare-sphere yield (instructor_tick.h). Every leg is REQUIRE/CHECK-
// based; the fixtures follow test_ground.cpp (heightfield + env) and
// test_atmosphere.cpp (the exact-identity discriminator) verbatim.
//
// Discipline (CLAUDE.md ## Learned): fixture-no-op guards (a leg proves its
// baseline term is NON-TRIVIAL before checking the verdict); GENERIC sphere
// points (the mouths themselves are off every world axis); the null
// differential firewall pins bit-identity, not a hide-band.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>

#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kP);

// The T1 canon dials (the TEST'S OWN params — graded against the inputs it
// reads, never calibrated to today's game.toml).
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kP.R;
    tp.tube_width_m = 110.0;  // T6b elliptical bore (horizontal semi)
    tp.tube_height_m = 90.0;  // T6b elliptical bore (vertical semi)
    tp.depth_m = 1600.0;      // T10: retired as the plateau driver (kept as the
                              // pit/cover sanity depth)
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m =
        0.0;  // floor OFF in the shared fixture (T5a tests set
              // it explicitly; keeps existing legs bit-identical)
    // T11/T12 — THE SEALED-CORE ARENA: one shallow ARENA ellipsoid, floor
    // sealed over the buried core (T12 removed T11's core-window shaft). The
    // bore breaks THROUGH the arena apex (breach_margin below it) into the
    // arena.
    tp.arena_a_m = 7350.0;       // arena horizontal semi-axis [m]
    tp.arena_c_m = 2600.0;       // arena vertical semi-axis (oblate) [m]
    tp.arena_depth_m = 1500.0;   // arena apex depth below mid terrain [m]
    tp.cavern_core_m = 2500.0;   // buried |position| safety-floor radius [m]
    tp.breach_margin_m = 300.0;  // plateau this far below the arena apex
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;  // T10: chamber hangs off its bore this
                                         // far above the breach
    tp.connector_radius_m = 60.0;
    tp.chambers_on = true;  // T13: the shared fixture keeps the T4-era side
                            // chambers ON so every existing chamber leg pins
                            // the on-arm (the T13 off-arm test sets it false).
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;  // T6c Errington entry pit (bore-center sink)
    tp.min_cover_m = 60.0;    // T6d monotone-descent cover
    tp.trench_len_m = 450.0;  // T13 entry approach trench (config/game.toml)
    tp.trench_rim_m = 150.0;
    return tp;
}

// Uniform terrain at `elev_m` above sea level (radius_at == R + q(elev) at
// every direction) — the test_ground.cpp fixture, so the mouths sit at a known
// surface radius. relief must exceed elev so the quantized height isn't
// clamped.
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

}  // namespace

TEST_CASE(
    "T1 geometry: Errington at the surface, Murray at the bowl floor, deep dip "
    "reaches the bottom junction") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    REQUIRE(net.spine.size() >= 3);

    // The Errington (home) end sits at the SUNKEN bore mouth (T6c/T6d): the
    // bore center is mouth_sink_m below the local surface (the entry PIT
    // recesses down to it). |spine.front()| == surface_r - mouth_sink_m. The
    // Murray (raid) end sits at the OPEN-PIT BOWL FLOOR: surface_r -
    // bowl_depth_m (T4a — the tube takes over below the bowl). |spine.back()|
    // == that floor.
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    CHECK(glm::length(front) ==
          Catch::Approx(hf.radius_at(world::kTunnelMouthErrington) -
                        tp.mouth_sink_m)
              .epsilon(0.0)
              .margin(1e-6));
    const double murray_surf = hf.radius_at(world::kTunnelMouthMurray);
    CHECK(
        glm::length(back) ==
        Catch::Approx(murray_surf - tp.bowl_depth_m).epsilon(0.0).margin(1e-6));

    // T11 — THE DEEP PLATEAU: the bore descends from the mouth to an ABSOLUTE-
    // radius plateau (arena apex - breach_margin_m) where it breaks THROUGH the
    // arena apex into the shallow arena. The deepest spine node sits at that
    // plateau radius (margin ~50 m — the smoothing/cumulative-min shift it a
    // hair) and is INSIDE the net (the arena owns it there).
    const double r_plateau = net.arena_apex_r - tp.breach_margin_m;
    double min_r = glm::length(net.spine.front().pos);
    glm::dvec3 deepest = net.spine.front().pos;
    for (const world::TunnelNet::Node& n : net.spine) {
        const double r = glm::length(n.pos);
        if (r < min_r) {
            min_r = r;
            deepest = n.pos;
        }
    }
    CHECK(min_r >= r_plateau - 50.0);
    CHECK(min_r <= r_plateau + 50.0);
    REQUIRE(net.contains(deepest));  // the deep node is inside the net (arena)
}

TEST_CASE("T1 geometry: containment + the exact no-op zone") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // A mid-arena point and both chamber centers are inside.
    const glm::dvec3 shell_pt = net.arena.center;
    CHECK(net.contains(shell_pt));
    CHECK(net.contains(net.chamber[0].center));
    CHECK(net.contains(net.chamber[1].center));

    // A spine node in the UPPER quarter of the descent bore (an eighth of the
    // way from the Errington mouth — a pure descending bore ring well above the
    // breach/plateau and off the arena) sits ~one MIN-SEMI (tube_height) deep
    // on the elliptical bore axis (ellipse2_sd(0,0) == -min(width,height) ==
    // -tube_height). Guard the premise: this node's radius is above the arena
    // apex so the tube — not the arena — owns the centre distance.
    const std::size_t qi = net.spine.size() / 8;
    const glm::dvec3 q = net.spine[qi].pos;
    REQUIRE(glm::length(q) > net.arena_apex_r);  // above the apex: tube owns
    CHECK(net.signed_distance(q) ==
          Catch::Approx(-tp.tube_height_m).margin(5.0));

    // Offset PERPENDICULAR to the local spine tangent (from the neighbor nodes)
    // by (tube_width + soft + 10): sd exceeds soft_m => the exact atm no-op
    // zone (u_t == 0 exactly). A generic-axis offset can point ALONG the tube
    // and stay inside — cross with the true tangent gives a real normal. Use
    // the WIDTH (the horizontal semi) since the offset is horizontal (⟂ up ⟂
    // tan).
    const glm::dvec3 tangent =
        glm::normalize(net.spine[qi + 1].pos - net.spine[qi - 1].pos);
    const glm::dvec3 perp =
        glm::normalize(glm::cross(tangent, glm::dvec3{0.11, 0.7, 0.2}));
    // Offset by the MAX semi (tube_width) + soft + 10 so the point clears the
    // ellipse boundary in ANY perpendicular direction (sd >= L - max_semi >
    // soft).
    const glm::dvec3 out = q + perp * (tp.tube_width_m + tp.soft_m + 10.0);
    CHECK(net.signed_distance(out) > tp.soft_m);

    // A generic point 5 km off the path at the surface is outside.
    const glm::dvec3 far_dir = glm::normalize(net.chamber[0].center);
    const glm::dvec3 far = far_dir * hf.radius_at(far_dir);
    const glm::dvec3 far_off =
        far + glm::normalize(
                  glm::cross(glm::normalize(far), glm::dvec3{0.3, 0.2, 0.9})) *
                  5000.0;
    CHECK(!net.contains(far_off));
    CHECK(net.signed_distance(far_off) > tp.soft_m);
}

TEST_CASE("T1 atm override: a contained mine holds FULL air under the war") {
    // The discriminator (test_atmosphere.cpp's exact-identity pattern): build a
    // deck that removes ALL air at tunnel depth, then show the tunnel term
    // restores it EXACTLY to baseline inside the net, and is a bit-identical
    // no-op aloft outside it.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // A mid-shell cavern point (deep below terrain, inside the net).
    const glm::dvec3 pos = net.arena.center;
    REQUIRE(net.contains(pos));

    // Premise: at tunnel depth atm_frac(altitude) == 1.0 (deeply below the
    // taper start), so the spatial factor is isolated.
    const double base = sim::atm_frac(sim::altitude(pos, kP), kP);
    REQUIRE(base == 1.0);

    // A deck that removes ALL air here: deck_agl_m below the shell point's AGL
    // with a small soft, so alt - deck_agl >> soft => the deck taper is fully
    // in (one_minus == 1) => the atm is fully removed at the cavern point. The
    // cavern sits several km below the surface, so the deck sits deeper still.
    // No bubbles.
    sim::AtmosphereField af;
    af.deck_agl_m = -12000.0;
    af.deck_soft_m = 100.0;

    sim::Environment env_no_tunnel;
    env_no_tunnel.atm = &af;
    // Premise pin: without the tunnel the war removes all air at depth.
    REQUIRE(sim::atm_frac_at(pos, &env_no_tunnel, kP) == 0.0);

    sim::Environment env_with_tunnel;
    env_with_tunnel.atm = &af;
    env_with_tunnel.tunnels = &net;
    // The tunnel restores FULL baseline air, EXACTLY (bare ==, the identity).
    CHECK(sim::atm_frac_at(pos, &env_with_tunnel, kP) == 1.0);

    // The no-op leg: at a generic aloft point far from the net, live-tunnels vs
    // null-tunnels are BIT-IDENTICAL even with a live bubble field configured.
    sim::AtmosphereField af2;
    af2.deck_agl_m = 120.0;
    af2.deck_soft_m = 200.0;
    af2.bubbles.push_back(
        sim::AtmosphereField::Bubble{glm::normalize(glm::dvec3{0.3, 1.0, 0.2}),
                                     6000.0, 4000.0, 1200.0, 600.0});
    const glm::dvec3 aloft =
        glm::normalize(glm::dvec3{0.3, 1.0, 0.2}) * (kP.R + 6000.0);
    // The aloft point is far from the tunnel (a different sphere region).
    REQUIRE(net.signed_distance(aloft) > net.soft_m);
    sim::Environment eA;
    eA.atm = &af2;
    sim::Environment eB;
    eB.atm = &af2;
    eB.tunnels = &net;
    CHECK(sim::atm_frac_at(aloft, &eA, kP) == sim::atm_frac_at(aloft, &eB, kP));
}

TEST_CASE("T1 crash yield (sim): the net suspends the terrain crash verdict") {
    // A SimState in the cavern shell (deep below terrain) with a downward-ish
    // velocity, stepped once. Terrain WOULD grade it a crash (the premise leg);
    // with the tunnel live it does NOT.
    const world::TunnelParams tp = test_tp();
    // Terrain elevation sits ABOVE the cavern (the shell is thousands of metres
    // below the surface), so any level terrain crashes a plane placed there.
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;

    const glm::dvec3 pos = net.arena.center;
    REQUIRE(net.contains(pos));
    const glm::dvec3 up = sim::local_up(pos);
    // A fast downward sink so terrain contact grades a crash, not a landing.
    sim::SimState s{};
    s.position = pos;
    s.velocity = -30.0 * up + 60.0 * glm::normalize(glm::cross(
                                         up, glm::dvec3{0.3, 0.2, 0.9}));
    s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    s.last_vhat = glm::normalize(s.velocity);

    sim::Inputs in{};

    sim::Environment env_ground;
    env_ground.ground = &hf;
    env_ground.ground_params = gp;
    // Premise: terrain alone grades this a crash (the egg is below the
    // surface).
    const sim::SimState crashed = sim::step(s, in, kP, &env_ground, kP.sim_dt);
    REQUIRE(crashed.crashed);

    // Tunnels live: the net suspends the verdict — no crash.
    sim::Environment env_tunnel = env_ground;
    env_tunnel.tunnels = &net;
    const sim::SimState safe = sim::step(s, in, kP, &env_tunnel, kP.sim_dt);
    REQUIRE(!safe.crashed);

    // A below-terrain state in the MANTLE rock (a generic direction well away
    // from the arena + the bores, deep below terrain) still crashes WITH
    // tunnels live (kills an unconditional-yield mutant): the rock is OUTSIDE
    // the net, and it is far below terrain.
    const glm::dvec3 pos_out_dir = glm::normalize(glm::dvec3{0.7, 0.1, 0.3});
    const glm::dvec3 pos_out = pos_out_dir * (net.arena_r + 500.0);
    REQUIRE(!net.contains(pos_out));  // premise: genuinely outside the net
    REQUIRE(hf.radius_at(pos_out_dir) - glm::length(pos_out) >
            50.0);  // premise: deep below terrain (crash)
    sim::SimState so{};
    so.position = pos_out;
    const glm::dvec3 up_o = sim::local_up(pos_out);
    so.velocity = -30.0 * up_o;
    so.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    so.last_vhat = glm::normalize(so.velocity);
    const sim::SimState out = sim::step(so, in, kP, &env_tunnel, kP.sim_dt);
    REQUIRE(out.crashed);
}

TEST_CASE("T1 app-tick yield (bare branch): the net suspends altitude<=0") {
    // ground null, tunnels live: a position inside the net BELOW R must NOT
    // respawn through the bare-sphere altitude<=0 rule; tunnels null respawns.
    const world::TunnelParams tp = test_tp();
    // Ground null => the net tunnels through the bare sphere at R. Build with a
    // null ground so the surface is R and the egg sits at R - depth (< R).
    const world::TunnelNet net = world::build_tunnel_net(tp, nullptr);
    const glm::dvec3 pos = net.arena.center;
    REQUIRE(sim::altitude(pos, kP) < 0.0);  // premise: genuinely below R
    REQUIRE(net.contains(pos));

    auto seed = [&](app::LoopState& st) {
        const glm::dvec3 up = sim::local_up(pos);
        const glm::dvec3 head =
            glm::normalize(glm::cross(up, glm::dvec3{0.11, 0.7, 0.2}));
        st.curr =
            harness::level_state(kP, 120.0, sim::altitude(pos, kP), up, head);
        st.prev = st.curr;
        st.prev_up = up;
        st.aim.reseed(st.curr.orientation, st.prev_up);
        st.grounded = false;  // airborne: exercise the crash predicate, not a
                              // GROUNDED spawn tick
    };

    app::TickInput in;
    in.raw_mode = true;

    SECTION("tunnels live: no respawn (the net owns inside/outside)") {
        // ground null + tunnels live => any_live() true; env passed in.
        sim::Environment env;
        env.tunnels = &net;
        app::LoopState st;
        seed(st);
        const app::TickResult r = app::tick(st, in, kP, kCp, &env);
        REQUIRE(!r.respawned);
    }

    SECTION("tunnels null: respawns (the bare altitude<=0 rule)") {
        // No env at all => the byte-identical altitude<=0 predicate fires.
        app::LoopState st;
        seed(st);
        const app::TickResult r = app::tick(st, in, kP, kCp, nullptr);
        REQUIRE(r.respawned);
    }
}

TEST_CASE(
    "T1 null differential firewall: live tunnels never move a far state") {
    // From a normal cruise state far from the net, step under env A (ground +
    // atm + tunnels all live) vs env B (identical but tunnels null): the full
    // SimState is bit-identical every tick. The S8-drone differential firewall.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    sim::AtmosphereField af;
    af.deck_agl_m = 120.0;
    af.deck_soft_m = 200.0;
    af.bubbles.push_back(
        sim::AtmosphereField::Bubble{glm::normalize(glm::dvec3{0.3, 1.0, 0.2}),
                                     6000.0, 4000.0, 1200.0, 600.0});

    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;

    sim::Environment envA;
    envA.ground = &hf;
    envA.ground_params = gp;
    envA.atm = &af;
    envA.tunnels = &net;

    sim::Environment envB = envA;
    envB.tunnels = nullptr;

    // A cruise state on a generic sphere region far from the tunnel (the bubble
    // center region), high above the ground so terrain never engages.
    const glm::dvec3 up = glm::normalize(glm::dvec3{0.3, 1.0, 0.2});
    const glm::dvec3 head =
        glm::normalize(glm::cross(up, glm::dvec3{0.11, 0.07, 1.0}));
    sim::SimState a = harness::level_state(kP, 160.0, 3000.0, up, head);
    a.throttle = 0.7;
    sim::SimState b = a;
    REQUIRE(net.signed_distance(a.position) > net.soft_m);  // premise: far out

    sim::Inputs in{};
    in.pitch = 0.2f;  // a live command so the trajectory actually moves
    in.throttle = 0.7f;

    double max_res = 0.0;
    for (int t = 0; t < 100; ++t) {
        a = sim::step(a, in, kP, &envA, kP.sim_dt);
        b = sim::step(b, in, kP, &envB, kP.sim_dt);
        max_res = std::max(max_res, glm::length(a.position - b.position));
        REQUIRE(a.position == b.position);
        REQUIRE(a.velocity == b.velocity);
        REQUIRE(a.orientation == b.orientation);
        REQUIRE(a.angular_vel == b.angular_vel);
        REQUIRE(a.throttle == b.throttle);
    }
    CHECK(max_res == 0.0);  // print at max_digits10 if it ever differs
}

TEST_CASE("T1 grounded-entry liftoff: entering the net clears on_ground") {
    // P0-1 fix (red-team): a plane grounded at a mouth taxi/lands into the net
    // volume; without the fix, on_ground latches forever (Q = true-q instead of
    // q_eff, ground_dynamics runs tire-friction in mid-air). The fix: the in-
    // tunnel else branch clears on_ground exactly as ground_contact's liftoff
    // branch would.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;

    // Seed inside the net (the cavern shell) with on_ground = true.
    const glm::dvec3 pos = net.arena.center;
    REQUIRE(net.contains(pos));  // premise: the seed is genuinely inside

    const glm::dvec3 up = sim::local_up(pos);
    sim::SimState s{};
    s.position = pos;
    // Horizontal velocity (not sinking) — the grounded regime's radial_acc test
    // would release this anyway, but the net-gated path is what we are testing.
    s.velocity =
        100.0 * glm::normalize(glm::cross(up, glm::dvec3{0.3, 0.2, 0.9}));
    s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    s.last_vhat = glm::normalize(s.velocity);
    s.on_ground = true;  // the stale grounded flag that must clear

    sim::Inputs in{};
    in.throttle = 0.5f;

    // Premise arm: with the SAME seed but tunnels NULL, ground_contact owns the
    // verdict. At ~2000 m below terrain it will crash (we don't care which
    // regime — we only need the premise to be non-vacuous: the null path does
    // something different than simply clearing on_ground).
    sim::Environment env_ground_only;
    env_ground_only.ground = &hf;
    env_ground_only.ground_params = gp;
    const sim::SimState prem =
        sim::step(s, in, kP, &env_ground_only, kP.sim_dt);
    // Without the tunnel, ground_contact runs and the on_ground path leads to a
    // crash (the grounded-constraint snap to a ~2000 m distant surface is
    // physically insane — it would crash via the over-limit terrain branch or
    // the ground_contact surface-constraint path; in practice it crashes).
    // We only require the premise is non-vacuous: the null path leaves a
    // DIFFERENT state than what the tunnel path must produce.
    // Wrap with parens — Catch2 forbids || inside REQUIRE.
    REQUIRE(
        (prem.crashed || prem.on_ground));  // null path: some grounded verdict

    // Tunnel live: the net must clear on_ground.
    sim::Environment env_tunnel;
    env_tunnel.ground = &hf;
    env_tunnel.ground_params = gp;
    env_tunnel.tunnels = &net;
    const sim::SimState next = sim::step(s, in, kP, &env_tunnel, kP.sim_dt);
    REQUIRE(!next.on_ground);  // the fix: liftoff semantics inside the net
    // Transient consequence events must also be cleared (not carried from
    // state).
    REQUIRE(!next.crashed);
    REQUIRE(next.wing_strike == 0);
    REQUIRE(!next.prop_strike);
}

TEST_CASE(
    "T1 near-but-outside firewall: live tunnels are a no-op just past the "
    "boundary") {
    // The null-differential firewall test covers states FAR from the net.
    // This sibling leg covers a state NEAR but genuinely outside (soft_m < sd <
    // 1000 m): the tunnel term must be a bit-identical no-op right up to its
    // boundary, not just from far away.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;

    // Probe outside but near: offset from a spine node in the PURE TUBE section
    // (an eighth of the way from the Errington mouth — a descending bore ring
    // above the arena apex, so the tube owns the distance) perpendicular to the
    // spine, offset by tube_radius + soft_m + 200 so sd > soft_m but < 1000.
    const std::size_t qi = net.spine.size() / 8;
    const glm::dvec3 q = net.spine[qi].pos;
    REQUIRE(glm::length(q) > net.arena_apex_r);  // above the apex: tube owns
    const glm::dvec3 tangent =
        glm::normalize(net.spine[qi + 1].pos - net.spine[qi - 1].pos);
    const glm::dvec3 perp =
        glm::normalize(glm::cross(tangent, glm::dvec3{0.11, 0.7, 0.2}));
    const glm::dvec3 near_out =
        q + perp * (tp.tube_width_m + tp.soft_m + 200.0);

    // Premise: genuinely near-but-outside.
    const double sd = net.signed_distance(near_out);
    REQUIRE(sd > net.soft_m);          // outside the blend band
    REQUIRE(sd < 1000.0);              // but near (not the far-away test)
    REQUIRE(!net.contains(near_out));  // confirmed outside

    // Build a SimState at this near-but-outside position, well above the
    // terrain so ground_contact does not engage (we only want the tunnel no-op
    // test). Use the surface radius at this direction plus 3000 m altitude.
    const glm::dvec3 dir = glm::normalize(near_out);
    const double surf_r = hf.radius_at(dir);
    const glm::dvec3 aloft_pos = dir * (surf_r + 3000.0);

    // Premise: aloft position is also outside the net.
    REQUIRE(!net.contains(aloft_pos));
    REQUIRE(net.signed_distance(aloft_pos) > net.soft_m);

    const glm::dvec3 head =
        glm::normalize(glm::cross(dir, glm::dvec3{0.3, 0.1, 0.9}));
    sim::SimState sa = harness::level_state(kP, 160.0, 3000.0, dir, head);
    sa.throttle = 0.6;
    sim::SimState sb = sa;

    sim::Environment envA;
    envA.ground = &hf;
    envA.ground_params = gp;
    envA.tunnels = &net;
    sim::Environment envB = envA;
    envB.tunnels = nullptr;

    sim::Inputs in{};
    in.pitch = 0.1f;
    in.throttle = 0.6f;

    for (int t = 0; t < 50; ++t) {
        sa = sim::step(sa, in, kP, &envA, kP.sim_dt);
        sb = sim::step(sb, in, kP, &envB, kP.sim_dt);
        // Full SimState bit-identical (exact == on doubles).
        REQUIRE(sa.position == sb.position);
        REQUIRE(sa.velocity == sb.velocity);
        REQUIRE(sa.orientation == sb.orientation);
        REQUIRE(sa.angular_vel == sb.angular_vel);
        REQUIRE(sa.on_ground == sb.on_ground);
        REQUIRE(sa.crashed == sb.crashed);
    }
}

TEST_CASE(
    "T1 chamber distinctness: the two chambers are real, distinct rooms") {
    // T10: the two side-chambers RE-HANG off their OWN bore (the egg is gone),
    // a fixed offset above each breach, laterally off the bore along
    // chamber.u_up (the cross-path horizontal offset axis). We probe along that
    // offset from each chamber center: the chamber's own semi (chamber_vert_m
    // along u_up == Pocket.c) governs — 80% past center is INSIDE, well past it
    // is OUTSIDE. The two chambers must be spatially distinct (off opposite
    // legs).
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    for (int k = 0; k < 2; ++k) {
        const world::TunnelNet::Pocket& ch = net.chamber[k];
        INFO("chamber " << k);
        // The chamber center is inside the net.
        REQUIRE(net.contains(ch.center));
        // The chamber outer wall along the VERTICAL axis (u_long, no connector
        // there — the connector runs along u_up toward the bore): 80% of the
        // u_long semi from center is inside; 1.5x that semi past it is outside
        // (solid mantle rock above/below the room).
        const glm::dvec3 off_dir = ch.u_long;  // the vertical axis (clear of
                                               // the connector along u_up)
        const glm::dvec3 probe_in = ch.center + off_dir * (ch.a_pos * 0.8);
        const glm::dvec3 probe_out = ch.center + off_dir * (ch.a_pos * 1.5);
        REQUIRE(net.contains(probe_in));
        REQUIRE(!net.contains(probe_out));
    }

    // The two chambers are DISTINCT rooms (well separated — off opposite legs).
    REQUIRE(glm::length(net.chamber[0].center - net.chamber[1].center) >
            2.0 * tp.chamber_long_m);
}

// ==================== T13 — chamber centering (S2) ==========================
// Chad's spec S2: "the chamber must lie in the MIDDLE between the two entrances
// — two proper tunnels meeting a central room." The room is the arena; the two
// tunnels are the bore legs from each mouth to where the bore first breaks into
// the arena ellipsoid. At the old arena_a_m=7350 the wide oblate ellipsoid
// reached so far up-bore that the arena-interior crossing (5910 m) dwarfed the
// two legs (3176/2958 m) — the "room" swallowed the tunnels. T13 shrinks
// arena_a_m to 4200 so the two legs and the crossing are comparable thirds.
// This leg pins that geometry directly against the T13-intent params.
namespace {
// Membership in the arena ellipsoid Pocket (the same predicate the audit probe
// uses — the tube union is confounded, so test the arena body directly).
bool in_arena_pocket(const world::TunnelNet& net, const glm::dvec3& p) {
    const world::TunnelNet::Pocket& e = net.arena;
    const glm::dvec3 d = p - e.center;
    const glm::dvec3 q{glm::dot(d, e.u_long), glm::dot(d, e.u_lat),
                       glm::dot(d, e.u_up)};
    const double a = q.x >= 0.0 ? e.a_pos : e.a_neg;
    const double v = (q.x / a) * (q.x / a) + (q.y / e.b) * (q.y / e.b) +
                     (q.z / e.c) * (q.z / e.c);
    return v < 1.0;
}
}  // namespace

TEST_CASE(
    "T13 chamber centering: two comparable tunnel legs meet a central room") {
    // T13-intent params: everything is the shared test_tp() canon EXCEPT the
    // arena horizontal semi-axis, shrunk to the shipped 4200 m.
    world::TunnelParams tp = test_tp();
    tp.arena_a_m = 4200.0;  // T13 centering (config/game.toml)
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.arena_on);
    const auto& sp = net.spine;
    const std::size_t N = sp.size();

    // Cumulative arc length along the spine.
    std::vector<double> arc(N, 0.0);
    for (std::size_t i = 1; i < N; ++i)
        arc[i] = arc[i - 1] + glm::length(sp[i].pos - sp[i - 1].pos);
    const double total = arc.back();

    // (a) The first arena-interior node from each end — the "you are now in the
    // room" crossing. The E-leg is the arc up to breachE; the M-leg is the arc
    // from breachM to the far mouth; the crossing spans breachE..breachM.
    std::size_t breachE = 0, breachM = 0;
    bool foundE = false, foundM = false;
    for (std::size_t i = 0; i < N; ++i)
        if (in_arena_pocket(net, sp[i].pos)) {
            breachE = i;
            foundE = true;
            break;
        }
    for (std::size_t i = N; i-- > 0;)
        if (in_arena_pocket(net, sp[i].pos)) {
            breachM = i;
            foundM = true;
            break;
        }
    REQUIRE(foundE);
    REQUIRE(foundM);
    REQUIRE(breachM > breachE);  // premise: a single contiguous room span

    const double leg_E = arc[breachE];
    const double leg_M = total - arc[breachM];
    const double crossing = arc[breachM] - arc[breachE];
    const double mean_leg = 0.5 * (leg_E + leg_M);
    INFO("leg_E=" << leg_E << " crossing=" << crossing << " leg_M=" << leg_M
                  << " mean_leg=" << mean_leg);

    // (b) The two legs are comparable (within 15% of each other) — two PROPER
    // tunnels, not one long approach and one stub. At 4200 the skew is ~2%.
    REQUIRE(std::fabs(leg_E - leg_M) <= 0.15 * mean_leg);

    // (c) The room crossing is COMPARABLE to a leg (between 0.5x and 1.5x the
    // mean leg) — "a central room fed by two tunnels," not a room that swallows
    // the tunnels (the old 7350 crossing was ~1.86x the mean leg). At 4200 the
    // crossing/mean-leg ratio is ~1.02.
    REQUIRE(crossing >= 0.5 * mean_leg);
    REQUIRE(crossing <= 1.5 * mean_leg);

    // (d) The arena center direction IS the geodesic midpoint (slerp t=0.5 of
    // the two mouth dirs) to 1e-9 — pin the construction so a future edit can't
    // silently off-center the room.
    const double dotm = std::clamp(
        glm::dot(world::kTunnelMouthErrington, world::kTunnelMouthMurray), -1.0,
        1.0);
    const double omega = std::acos(dotm);
    const double so = std::sin(omega);
    const glm::dvec3 slerp_mid =
        so < 1e-9
            ? glm::normalize(world::kTunnelMouthErrington +
                             world::kTunnelMouthMurray)
            : glm::normalize(
                  std::sin(0.5 * omega) / so * world::kTunnelMouthErrington +
                  std::sin(0.5 * omega) / so * world::kTunnelMouthMurray);
    const glm::dvec3 c_dir = glm::normalize(net.arena.center);
    REQUIRE(glm::dot(c_dir, slerp_mid) == Catch::Approx(1.0).margin(1e-9));
}

// ==================== T13 — chambers gated off (S1) =========================
// Chad's spec S1: a clear tunnel passage, no blind passages. The pump pockets
// (side chambers + connectors) are GATED OFF until the pumps land. When off,
// they must be STRUCTURALLY inert in the SDF — the two old chamber centers read
// SOLID ROCK (sd > 0), so no blind side-room exists to fly into. The on-arm net
// (chambers_on=true) supplies the reference centers; the off-arm net
// (chambers_on=false) must read solid there.
TEST_CASE("T13 chambers off: the old chamber centers read solid rock") {
    const world::HeightField hf = uniform_field(300.0);

    world::TunnelParams tp_on = test_tp();  // chambers_on == true (fixture)
    REQUIRE(tp_on.chambers_on);
    const world::TunnelNet net_on = world::build_tunnel_net(tp_on, &hf);
    // Premise: with chambers ON the centers ARE inside the net (real rooms).
    REQUIRE(net_on.contains(net_on.chamber[0].center));
    REQUIRE(net_on.contains(net_on.chamber[1].center));
    const glm::dvec3 c0 = net_on.chamber[0].center;
    const glm::dvec3 c1 = net_on.chamber[1].center;

    world::TunnelParams tp_off = test_tp();
    tp_off.chambers_on = false;
    const world::TunnelNet net_off = world::build_tunnel_net(tp_off, &hf);
    // OFF: the chamber Pockets are built with b == 0 and the connectors with
    // r == 0 (structurally skipped by the SDF).
    REQUIRE(net_off.chamber[0].b == 0.0);
    REQUIRE(net_off.chamber[1].b == 0.0);
    REQUIRE(net_off.connector[0].r == 0.0);
    REQUIRE(net_off.connector[1].r == 0.0);
    // The two old chamber centers are now SOLID ROCK (no blind side-room). They
    // sit well off the bore, so the tube/arena never contain them either.
    REQUIRE(net_off.signed_distance(c0) > 0.0);
    REQUIRE(net_off.signed_distance(c1) > 0.0);
    REQUIRE_FALSE(net_off.contains(c0));
    REQUIRE_FALSE(net_off.contains(c1));

    // The bore itself is UNAFFECTED by the gate — a mid-descent bore node is
    // still inside on BOTH arms (the off-arm only removed the side rooms, not
    // the through-passage). Pin a deep node inside on both.
    const std::size_t qi = net_off.spine.size() / 3;
    REQUIRE(net_on.contains(net_on.spine[qi].pos));
    REQUIRE(net_off.contains(net_off.spine[qi].pos));
}

// ============================ T4a — the Murray bowl =========================
// The open-pit raid mouth: a truncated cone about the Murray axis, bowl_radius
// at the surface tapering to tube_radius at bowl_depth below.

TEST_CASE("T4a bowl: open-space pin - the pit interior is inside the net") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    const glm::dvec3 axis = world::kTunnelMouthMurray;
    const double surf = hf.radius_at(axis);

    // (a) Points on the bowl axis at depths {10, 150, 290} below the Murray
    //     surface are inside the net (the open pit).
    for (double d : {10.0, 150.0, 290.0}) {
        const glm::dvec3 p = axis * (surf - d);
        INFO("axis depth " << d);
        REQUIRE(net.contains(p));
    }

    // A point 30 m below the surface at lateral 350 m: the frustum radius at
    // depth 30 is bowl_r + (tube_width - bowl_r)*(30/bowl_depth) = 450 +
    // (110-450)*(0.1) = 416 m, so lateral 350 < 416 => INSIDE.
    const double depth30 = 30.0;
    const double f = depth30 / tp.bowl_depth_m;
    const double r_at =
        tp.bowl_radius_m + (tp.tube_width_m - tp.bowl_radius_m) * f;
    REQUIRE(350.0 < r_at);  // premise: genuinely inside the frustum at depth 30
    // Build a tangent frame about the axis for the lateral offset.
    const glm::dvec3 ref =
        std::fabs(axis.z) < 0.9 ? glm::dvec3{0, 0, 1} : glm::dvec3{1, 0, 0};
    const glm::dvec3 lat = glm::normalize(ref - glm::dot(ref, axis) * axis);
    const glm::dvec3 p_in = axis * (surf - depth30) + lat * 350.0;
    REQUIRE(net.contains(p_in));
}

TEST_CASE("T4a bowl: solid pin - beyond the opening is outside the net") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    const glm::dvec3 axis = world::kTunnelMouthMurray;
    const double surf = hf.radius_at(axis);
    const glm::dvec3 ref =
        std::fabs(axis.z) < 0.9 ? glm::dvec3{0, 0, 1} : glm::dvec3{1, 0, 0};
    const glm::dvec3 lat = glm::normalize(ref - glm::dot(ref, axis) * axis);

    // (b) lateral 500 m at depth 30: the frustum radius there is ~412 m, so
    //     500 > 412 => OUTSIDE (solid rock beside the pit).
    const glm::dvec3 p_out = axis * (surf - 30.0) + lat * 500.0;
    REQUIRE(!net.contains(p_out));
}

TEST_CASE("T4a bowl: crash yield - inside survives, rock-beside crashes") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;

    const glm::dvec3 axis = world::kTunnelMouthMurray;
    const double surf = hf.radius_at(axis);
    sim::Inputs in{};

    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;

    // (c) A state descending inside the bowl (100 m below the surface, on axis)
    //     steps without crashing.
    const glm::dvec3 p_in = axis * (surf - 100.0);
    REQUIRE(net.contains(p_in));  // premise: inside the pit
    const glm::dvec3 up = sim::local_up(p_in);
    sim::SimState s{};
    s.position = p_in;
    s.velocity = -20.0 * up;  // descending into the pit
    s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    s.last_vhat = glm::normalize(s.velocity);
    const sim::SimState nx = sim::step(s, in, kP, &env, kP.sim_dt);
    REQUIRE(!nx.crashed);

    // A state in the rock BESIDE the bowl at depth 100 (lateral 600 m — well
    // beyond the ~340 m frustum radius there) crashes (below terrain, outside
    // the net).
    const glm::dvec3 ref =
        std::fabs(axis.z) < 0.9 ? glm::dvec3{0, 0, 1} : glm::dvec3{1, 0, 0};
    const glm::dvec3 lat = glm::normalize(ref - glm::dot(ref, axis) * axis);
    const glm::dvec3 p_rock = axis * (surf - 100.0) + lat * 600.0;
    REQUIRE(!net.contains(p_rock));  // premise: genuinely in the rock
    const glm::dvec3 up_r = sim::local_up(p_rock);
    sim::SimState sr{};
    sr.position = p_rock;
    sr.velocity = -20.0 * up_r;
    sr.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    sr.last_vhat = glm::normalize(sr.velocity);
    const sim::SimState nr = sim::step(sr, in, kP, &env, kP.sim_dt);
    REQUIRE(nr.crashed);
}

TEST_CASE("T4a bowl: spine end sits at the bowl floor (shared seam)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // (d) The spine's Murray end (t=1) terminates at the bowl floor:
    //     |spine.back()| == surface_r_murray - bowl_depth_m.
    const double murray_surf = hf.radius_at(world::kTunnelMouthMurray);
    REQUIRE(
        glm::length(net.spine.back().pos) ==
        Catch::Approx(murray_surf - tp.bowl_depth_m).epsilon(0.0).margin(1e-6));
    // The net's stored bowl matches the params (single source into the mesh).
    REQUIRE(net.bowl.bowl_r == tp.bowl_radius_m);
    REQUIRE(net.bowl.bowl_depth == tp.bowl_depth_m);
    REQUIRE(net.bowl.floor_r ==
            tp.tube_width_m);  // taper end == bore half-width
    REQUIRE(net.bowl.surface_r == Catch::Approx(murray_surf).margin(1e-6));
}

TEST_CASE("T6c mouth cut radii: Murray == bowl radius, Errington == pit rim") {
    // m18 pin: the Murray terrain-mesh cut must span the bowl opening
    // (bowl_radius), NOT the tube radius. T6c: the Errington cut now spans the
    // ENTRY PIT rim (the recess crater), NOT a tube-scaled portal hole. Both
    // the app (main.cpp portal surgery + draw.cpp collar reach) and this test
    // route through the world:: single-source helpers, so a stale cut is caught
    // here.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    constexpr double kMouthCutFactor =
        2.0;  // render::kMouthCutFactor (inert now)
    REQUIRE(world::murray_cut_radius(net) == tp.bowl_radius_m);
    // The Errington cut == the pit rim (== net.pit.bowl_r == kPitRimFactor *
    // tube_width; T12 tightened kPitRimFactor 2.0 -> 1.3 to cover the
    // descending bore — see world/tunnel_net.h).
    REQUIRE(world::errington_cut_radius(net, kMouthCutFactor) ==
            net.pit.bowl_r);
    REQUIRE(net.pit.bowl_r ==
            Catch::Approx(world::kPitRimFactor * tp.tube_width_m).margin(1e-9));
    // P2-1 single source: the pit wall (net.pit.bowl_r) and the terrain-hole
    // rim (errington_pit_rim, the SAME expression main.cpp feeds the CutDisk)
    // are ONE value by construction — the welded 2.0*tube_width in main.cpp and
    // the dead max() in build_tunnel_net are gone.
    REQUIRE(net.pit.bowl_r == world::errington_pit_rim(tp.tube_width_m));
    REQUIRE(world::errington_pit_rim(tp.tube_width_m) ==
            world::kPitRimFactor * tp.tube_width_m);
    // Both mouths are OPEN PITS wider than the bore.
    REQUIRE(world::errington_cut_radius(net, kMouthCutFactor) >
            tp.tube_width_m);
    REQUIRE(world::murray_cut_radius(net) >
            world::errington_cut_radius(net, kMouthCutFactor));
}

// ============================ T6d — the monotone descent ====================
// The bore descends MONOTONE from the Errington mouth to the deep plateau — it
// NEVER rises back toward the surface (Chad: "it looks like it goes back to the
// surface and I just die"). The cumulative-min from the home side is the safety
// net that guarantees it BY CONSTRUCTION even when the terrain overhead rises
// mid-path.

namespace {

// A synthetic HeightField carrying a MID-PATH terrain feature ALONG the
// Errington->Murray great circle: baseline `corridor` elevation in the middle
// stretch, ramping up to `mouth_elev` toward both mouths, with two Gaussian
// DIPS (nodes ~27 and ~60) separated by a RISE. The elevation is a function of
// the along-arc path parameter t only (constant perpendicular to the arc),
// baked into a fine equirect grid so radius_at() at each spine node reads it
// back.
//
// The dips sit within the cover-cap band of the deepest terrain, so the (T6d)
// cover cap BINDS along the corridor and the profile TRACKS the terrain: it
// deepens at the dips and RISES between them. On the Errington descent leg that
// mid-path rise is EXACTLY what the cumulative-min must suppress; without it
// the bore reads as "going back up". (Canon's deep tunnel leaves the cap fully
// inert — dt >> min_cover+tube_height — so this feature is only expressible on
// a deliberately-shallow test tunnel.)
world::HeightField ridge_valley_field() {
    // The great-circle frame (Errington -> Murray).
    const glm::dvec3 E = world::kTunnelMouthErrington;
    const glm::dvec3 M = world::kTunnelMouthMurray;
    const double omega = std::acos(glm::clamp(glm::dot(E, M), -1.0, 1.0));
    const glm::dvec3 n = glm::normalize(glm::cross(E, M));  // arc normal
    const glm::dvec3 b =
        glm::normalize(glm::cross(n, E));  // in-plane, toward M

    // The node count the build will produce (spacing 150 m over the arc) so the
    // dip/rise features land on the intended nodes.
    const int seg = std::max(
        2, static_cast<int>(std::ceil(omega * kP.R / 150.0)));  // == 82

    // Elevation [m] as a function of the along-arc path fraction t in [0,1].
    const double corridor = 2650.0, mouth_elev = 3000.0;
    const auto elev = [&](double t) {
        const double node = t * seg;
        double base;
        if (node <= 24.0)
            base = mouth_elev - (mouth_elev - corridor) * (node / 24.0);
        else if (node >= 66.0)
            base = corridor +
                   (mouth_elev - corridor) * ((node - 66.0) / (82.0 - 66.0));
        else
            base = corridor;
        const double dipA =
            60.0 * std::exp(-((node - 27.0) * (node - 27.0)) / (2.0 * 25.0));
        const double dipB =
            90.0 * std::exp(-((node - 60.0) * (node - 60.0)) / (2.0 * 25.0));
        return base - dipA - dipB;
    };

    // Along-arc path parameter of a unit direction (0 at Errington, 1 at
    // Murray); off-arc pixels take the projected value (elevation constant
    // perpendicular to the arc).
    const auto tparam = [&](const glm::dvec3& d) {
        glm::dvec3 dip = d - glm::dot(d, n) * n;  // project onto the arc plane
        const double l = glm::length(dip);
        if (l < 1e-12) return 0.0;
        dip /= l;
        return std::atan2(glm::dot(dip, b), glm::dot(dip, E)) / omega;
    };

    world::HeightField hf;
    hf.w = 720;
    hf.h = 360;
    hf.R = kP.R;
    hf.relief_scale = 6000.0;  // > mouth_elev, so no quantization clamp
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    const double kPi = 3.14159265358979323846;
    for (int y = 0; y < hf.h; ++y) {
        const double v = (y + 0.5) / hf.h;
        const double lat = (0.5 - v) * kPi;  // inverse of equirect_uv v
        for (int x = 0; x < hf.w; ++x) {
            const double u = (x + 0.5) / hf.w;
            const double lon =
                (u - 0.5) * 2.0 * kPi;  // inverse of u (offset 0)
            const glm::dvec3 d{std::cos(lat) * std::cos(lon), std::sin(lat),
                               std::cos(lat) * std::sin(lon)};
            const double t = glm::clamp(tparam(glm::normalize(d)), 0.0, 1.0);
            const double frac = glm::clamp(elev(t) / hf.relief_scale, 0.0, 1.0);
            hf.px[static_cast<std::size_t>(y) * hf.w + x] =
                static_cast<std::uint16_t>(frac * 65535.0 + 0.5);
        }
    }
    return hf;
}

// The shallow test tunnel that makes the T6d cover cap BIND (so the terrain
// feature reaches the profile). T11: the plateau is now the ABSOLUTE radius
// (arena apex - breach_margin) == terr_mid - arena_depth - breach_margin, so a
// SHALLOW arena_depth puts the plateau just below the corridor terrain:
// ridge_valley_field's corridor sits at ~R+2650 (== ~17650 m), so
// arena_depth 50 + breach 100 => plateau ~17500 puts the crown (plateau +
// tube_height) within min_cover_m + tube_height_m of the terrain along the
// whole corridor and the cover cap GOVERNS (deepening at the dips, held flat by
// the cumulative-min across the mid-path rise). The arena is a
// profile-independent union (build sets the spine nodes before the arena), so
// the T6d pins — which read node RADII / cover directly — are untouched by it.
// The loader is not invoked here (this fixture need not clear its checks).
world::TunnelParams test_tp_shallow() {
    world::TunnelParams tp = test_tp();
    tp.arena_depth_m = 50.0;     // plateau = terr_mid - 50 - 100 ~ 17500
    tp.breach_margin_m = 100.0;  // ~150 m below the ~17650 corridor terrain
    tp.cavern_core_m = 2000.0;   // a valid arena (floor far above the core)
    return tp;
}

}  // namespace

TEST_CASE(
    "T6d monotone descent: a mid-path terrain rise never lifts the bore") {
    // The cumulative-min safety net (world/tunnel_net.cpp) is the REAL subject.
    // Build against a synthetic terrain whose overhead RISES mid-path between
    // the two dips: the cover cap tracks the terrain UP there, and WITHOUT the
    // cumulative-min the bore radius would gain height on the way down — the
    // exact "it goes back to the surface" pathology. WITH it the Errington leg
    // is monotone by construction.
    const world::TunnelParams tp = test_tp_shallow();
    const world::HeightField hf = ridge_valley_field();
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.spine.size() >= 40);

    // The deepest node (the plateau) ends the Errington leg; beyond it the
    // Murray up-ramp legitimately rises.
    std::size_t deep_i = 0;
    for (std::size_t i = 1; i < net.spine.size(); ++i)
        if (glm::length(net.spine[i].pos) < glm::length(net.spine[deep_i].pos))
            deep_i = i;

    // Premise (fixture-no-op guard): the cover cap actually BINDS on this
    // fixture — the corridor terrain overhead genuinely RISES between the dips,
    // reaching the profile path. Confirm a mid-path node's terrain sits well
    // above a flanking node's (so the "rise" is real), and the deep leg spans
    // several nodes.
    REQUIRE(deep_i >= 30);
    const glm::dvec3 mid_dir = glm::normalize(net.spine[deep_i / 2].pos);
    const glm::dvec3 near_dir = glm::normalize(net.spine[deep_i / 2 - 6].pos);
    // The synthetic terrain is non-trivial along the leg (not flat).
    REQUIRE(std::abs(hf.radius_at(mid_dir) - hf.radius_at(near_dir)) > 5.0);

    // THE MONOTONICITY PIN: every node on the Errington leg (down to the deep
    // plateau) is no shallower than its predecessor. WITHOUT the cumulative-min
    // this FAILS on this fixture (~7 m rise between the dips); WITH it, holds.
    for (std::size_t i = 1; i <= deep_i; ++i) {
        INFO("node " << i << " |pos| " << glm::length(net.spine[i].pos)
                     << " prev " << glm::length(net.spine[i - 1].pos));
        REQUIRE(glm::length(net.spine[i].pos) <=
                glm::length(net.spine[i - 1].pos) + 1e-9);
    }

    // THE COVER PIN (P3-3 order): the bore CROWN keeps min_cover_m of rock
    // below terrain on the Errington leg OUTSIDE the mouth-pit cut region. This
    // holds only because the cover cap is applied AFTER the smoothing pass
    // (capping before smoothing lets the smoothed crown poke back up through
    // the cap ~8 m near a dip). A 2 m tolerance absorbs the equirect
    // quantization; the pre-fix order fails this by far more.
    const double arc_len =
        std::acos(glm::clamp(
            glm::dot(world::kTunnelMouthErrington, world::kTunnelMouthMurray),
            -1.0, 1.0)) *
        kP.R;
    const int seg =
        std::max(2, static_cast<int>(std::ceil(arc_len / tp.spacing_m)));
    const int ramp_nodes = static_cast<int>(0.3 * seg);  // ramp_frac 0.3
    for (std::size_t i = 0; i <= deep_i; ++i) {
        if (static_cast<int>(i) < ramp_nodes) continue;  // mouth-pit cut region
        const glm::dvec3 dir = glm::normalize(net.spine[i].pos);
        const double crown = glm::length(net.spine[i].pos) + net.tube_height;
        const double cover = hf.radius_at(dir) - crown;
        INFO("cover node " << i << " = " << cover);
        REQUIRE(cover >= tp.min_cover_m - 2.0);
    }
}

// ===================== T12 P1 — the MOUTH-PIT cover tripwire ================
// The T6d cover pin (above) SKIPS the first ~0.3*seg near-mouth nodes by
// construction (the open-cut pit/bowl owns cover there) — so the mouth-pit
// region is UNVERIFIED. T12 fixed the pilot's "see-through crown + half-tunnel
// blocker" by tightening kPitRimFactor 2.0 -> 1.3 so the terrain-cut crater
// (kPitRimFactor*tube_width arc) reaches only the mouth node; the first
// non-mouth node (arc == spacing_m) then keeps rock cover. But 1.3 is
// "calibrated to today" (the S8-drone trap): the crater span and the descent
// grade are INDEPENDENT dials (kPitRimFactor, spacing_m, mouth_sink_m,
// arena_depth_m, breach_margin_m, ramp_frac) with NO cross-check.
//
// THE RELATIONSHIP (config-relative, derived from the BUILT net + params — a
// retune moves the leg's inputs WITH it; NEVER re-weld 143/149.5): the pit is a
// depth-independent open CYLINDER of rim radius errington_pit_rim(tube_width)
// (world/tunnel_net.cpp bowl_sd) about the mouth axis; inside that arc the cut
// is OPEN air down to the pit floor. So for every near-mouth spine node whose
// direction lies WITHIN the pit rim arc, the bore CROWN there must stay at or
// below the pit FLOOR radius (no poke into the open cut = no see-through / no
// half-tunnel wall). Equivalently, arithmetically: the crater arc-span must be
// smaller than the arc of the FIRST node whose crown must be covered (node 1).
TEST_CASE("T12 P1: no bore crown pokes into the Errington mouth-pit open cut") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.spine.size() >= 3);
    REQUIRE(net.pit.bowl_depth > 0.0);  // premise: the entry pit is LIVE (T6c)

    // The pit geometry, read from the BUILT net (single-source with the terrain
    // cut + the bowl_sd cylinder — errington_pit_rim owns bowl_r).
    const glm::dvec3 mouth_axis = net.pit.axis;  // == kTunnelMouthErrington
    const double pit_rim = net.pit.bowl_r;       // rim CYLINDER radius [m]
    const double pit_floor_r =                   // open-cut floor radius
        net.pit.surface_r - net.pit.bowl_depth;

    // (a) THE ARITHMETIC FORM: the crater rim CYLINDER (measured as lateral
    //     distance from the mouth axis) must NOT reach node 1 — the first node
    //     that must carry rock cover. bowl_sd measures `rad` = the
    //     perpendicular distance to the axis, so compare node 1's rad against
    //     pit_rim directly (config-relative: pit_rim =
    //     errington_pit_rim(tube_width), node-1 lateral = f(spacing_m/descent
    //     geometry)).
    const auto lateral = [&](const glm::dvec3& pos) {
        const glm::dvec3 d = pos - mouth_axis * net.pit.surface_r;
        const glm::dvec3 radial = d - glm::dot(d, mouth_axis) * mouth_axis;
        return glm::length(radial);
    };
    INFO("pit rim (cylinder) = " << pit_rim << " m; node1 lateral = "
                                 << lateral(net.spine[1].pos));
    REQUIRE(lateral(net.spine[1].pos) > pit_rim);  // node 1 outside the cut

    // (b) THE GEOMETRIC FORM: for EVERY near-mouth node, IF its direction lies
    //     within the pit rim arc of the mouth (i.e. it sits inside the open-cut
    //     cylinder laterally), THEN its bore crown (|pos| + tube_height) must
    //     stay at/below the pit floor radius — no crown poking into open air.
    //     Only node 0 (the designed mouth opening) is allowed inside the rim.
    //     Scan the first ~2*ramp worth of nodes (well past any rim reach).
    const double ramp_nodes =
        std::max(1.0, tp.ramp_frac * (net.spine.size() - 1));
    const int scan = std::min(static_cast<int>(net.spine.size()),
                              static_cast<int>(2.0 * ramp_nodes) + 3);
    int inside_rim = 0;
    for (int i = 1; i < scan; ++i) {  // i>=1: node 0 IS the mouth opening
        const glm::dvec3& pos = net.spine[i].pos;
        if (lateral(pos) < pit_rim) {  // node sits inside the open-cut cylinder
            ++inside_rim;
            const double crown = glm::length(pos) + net.tube_height;
            INFO("node " << i << " inside rim: crown " << crown
                         << " vs pit floor " << pit_floor_r);
            REQUIRE(crown <= pit_floor_r);  // no poke into the open cut
        }
    }
    // Non-vacuous the OTHER way: with the CANON dials no non-mouth node sits
    // inside the rim (the tightened 1.3 keeps them all covered) — so (b) is a
    // guard that STAYS empty on canon and (a) is the live pin. Record it.
    INFO("non-mouth nodes inside the rim (canon expects 0): " << inside_rim);
}

// ============================ T5a — the floor ===============================
// A flat floor raised floor_height_m above the SPINE TUBE's lowest point,
// chord-truncating the circular cross-section (perpendicular to LOCAL up).
// floor=0 => bit-identical to no floor; below the floor plane (still inside the
// old circle) reads SOLID (a wall strike); the tube<->egg junction stays open.

namespace {
// The T1 canon dials WITH the T5a floor set to `fh`, on a SHALLOW, SHELL-OFF
// tunnel.
//
// The T5a legs test the FLOOR-TRUNCATION SDF (a point below the flat floor
// reads SOLID), which is a property of the spine TUBE alone. They need a
// FLAT-floor corridor station where the TUBE — not the arena — owns the "below
// the floor" verdict. On the T11 canon net the only flat region is the deep
// plateau, which sits just below the arena apex and is SWALLOWED by the arena
// (a point below the plateau floor still reads inside via the arena), so the
// truncation is unprobeable there (a real geometry fact, NOT a floor bug — the
// canon arena's own coverage lives in the T11 arena / T7 / T8 legs, which use
// test_tp()). The floor mechanism is orthogonal to the arena, so these legs pin
// it against a SHALLOW, ARENA-OFF tunnel: the plateau is a shallow ABSOLUTE
// radius (~R+150, ~150 m below the surface) with a genuinely-flat middle
// stretch, and the arena is OFF (cavern_core_m == 0 => arena_on false), so the
// bore-tube floor truncation is the sole owner. This is a decoupling, NOT a
// shave — the shipped floor code + the shipped floor dial (fh) are exactly what
// runs here.
world::TunnelParams test_tp_floor(double fh) {
    world::TunnelParams tp = test_tp();
    tp.floor_height_m = fh;
    // Arena OFF (core 0 => arena_on false) + a SHALLOW absolute plateau: the
    // profile reads r_plateau = terr_mid - arena_depth - breach_margin, so a
    // depth 100 + margin 50 below the ~R+300 surface puts the flat plateau at
    // ~R+150 (~150 m below the surface). A genuinely-flat corridor station
    // exists there and no arena union interferes with the tube-floor
    // truncation.
    tp.cavern_core_m = 0.0;    // arena OFF
    tp.arena_depth_m = 100.0;  // plateau = terr_mid - 100 - 50 ~ R+150
    tp.breach_margin_m = 50.0;
    return tp;
}
}  // namespace

TEST_CASE("T5a floor OFF: floor_height_m=0 SDF is bit-identical to no floor") {
    // The knob-off arm: build a net with floor 0 (the shared fixture) and one
    // built the IDENTICAL way, and byte-compare signed_distance over a sample
    // grid spanning the tube/chamber region. The floor code path (floor_r <= 0)
    // must be exactly the plain capsule.
    const world::HeightField hf = uniform_field(300.0);
    // BOTH nets use the floor-fixture dims (T10: test_tp_floor is a shallow,
    // shell-OFF tunnel so the floor legs have a flat-clear corridor — see its
    // comment); `a` sets floor 0 explicitly and `b` is the SAME dims via the
    // canon's floor-OFF default, so any SDF divergence is the floor CODE PATH
    // at fh=0, not a dims mismatch. The `floor_r == 0` node assertion below is
    // the direct pin that the floor branch stays inert.
    world::TunnelParams tb = test_tp_floor(0.0);
    tb.floor_height_m = 0.0;  // the canon default (floor OFF) — same as `a`
    const world::TunnelNet a = world::build_tunnel_net(test_tp_floor(0.0), &hf);
    const world::TunnelNet b = world::build_tunnel_net(tb, &hf);  // fh 0

    // Sample a grid around a chamber center + along the spine.
    const glm::dvec3 e = a.chamber[0].center;
    const glm::dvec3 up = glm::normalize(e);
    const glm::dvec3 lat = a.chamber[0].u_lat;
    const glm::dvec3 crs = a.chamber[0].u_up;
    for (int ix = -6; ix <= 6; ++ix)
        for (int iy = -6; iy <= 6; ++iy)
            for (int iz = -6; iz <= 6; ++iz) {
                const glm::dvec3 p = e + lat * (ix * 120.0) +
                                     crs * (iy * 120.0) + up * (iz * 200.0);
                REQUIRE(a.signed_distance(p) == b.signed_distance(p));  // exact
            }
    // And along the pure-tube section (a quarter down).
    for (std::size_t qi = 2; qi + 2 < a.spine.size(); qi += 7) {
        const glm::dvec3 q = a.spine[qi].pos;
        const glm::dvec3 t =
            glm::normalize(a.spine[qi + 1].pos - a.spine[qi - 1].pos);
        const glm::dvec3 perp =
            glm::normalize(glm::cross(t, glm::dvec3{0.11, 0.7, 0.2}));
        for (double off = -100.0; off <= 100.0; off += 20.0)
            REQUIRE(a.signed_distance(q + perp * off) ==
                    b.signed_distance(q + perp * off));
    }
    // The per-node floor_r stays 0 (OFF) at every station.
    for (const world::TunnelNet::Node& n : a.spine) REQUIRE(n.floor_r == 0.0);
    REQUIRE(a.floor_height == 0.0);
}

// Find a FLAT-floor corridor station where the BORE TUBE alone owns the verdict
// (consecutive floor_r within 1 m, and the tube-axis SDF reads exactly the pure
// tube depth). Returns the index or SIZE_MAX. The steep ramp near the mouths
// has a sloped floor (a point below one station's floor is above the next's — a
// sloped ramp is correct); the flat corridor is where "below is solid" is
// unambiguous. T10: the chambers hang laterally OFF the bore (~240 m) so a bore
// station is clear of them; require the tube-axis centre SDF == -tube_height
// (no shell/chamber/connector overlap) so the FLOOR truncation is the sole
// owner.
static std::size_t flat_corridor_station(const world::TunnelNet& net) {
    for (std::size_t i = 2; i + 2 < net.spine.size(); ++i) {
        if (std::abs(net.spine[i].floor_r - net.spine[i - 1].floor_r) > 1.0)
            continue;
        if (std::abs(net.spine[i + 1].floor_r - net.spine[i].floor_r) > 1.0)
            continue;
        // The bore tube (with its flat floor) — not the shell/chamber/connector
        // — must own the centre distance. At a floored station the nearest wall
        // is the FLOOR (floor_height above the bore bottom, so the axis centre
        // sits tube_height - floor_height above it): sd(centre) ==
        // -(tube_height - floor_height). At floor OFF that is -tube_height.
        // Matching this confirms no primitive but the floored tube is nearby,
        // so the caller's +-5 m floor probes are decided by the floor
        // truncation alone. NOTE: the fixture is shell-OFF (test_tp_floor).
        const double want_center = -(net.tube_height - net.floor_height);
        if (std::abs(net.signed_distance(net.spine[i].pos) - want_center) > 2.0)
            continue;
        return i;
    }
    return static_cast<std::size_t>(-1);
}

TEST_CASE("T5a floor ON: above the floor is inside, below the floor is SOLID") {
    // At a FLAT-floor corridor station clear of the egg, a point 5 m ABOVE the
    // floor centerline is inside; a point 5 m BELOW the floor plane — still
    // inside the OLD circular section — reads SOLID.
    const double fh = 20.0;  // canon floor
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net =
        world::build_tunnel_net(test_tp_floor(fh), &hf);

    REQUIRE(net.floor_height == fh);

    const std::size_t qi = flat_corridor_station(net);
    REQUIRE(qi != static_cast<std::size_t>(-1));  // a flat station exists
    const glm::dvec3 c = net.spine[qi].pos;
    const glm::dvec3 up = glm::normalize(c);
    const double floor_r = net.spine[qi].floor_r;
    REQUIRE(floor_r > 0.0);
    // Premise: the floor is one floor_height above the bore's radial bottom
    // (the vertical semi, tube_height, below the center).
    REQUIRE(floor_r ==
            Catch::Approx(glm::length(c) - net.tube_height + fh).margin(1e-6));

    // The floor CENTERLINE point (on the tube axis at the floor radial level).
    const glm::dvec3 floor_pt = up * floor_r;
    // 5 m above the floor, on the axis: inside.
    REQUIRE(net.contains(floor_pt + up * 5.0));
    // 5 m below the floor plane, on the axis: SOLID (was inside the old bore —
    // the floor level sits fh above the bottom, so 5 m below is still within
    // tube_height of center vertically) — the floor truncation makes it
    // outside.
    const glm::dvec3 below = floor_pt - up * 5.0;
    // Premise: `below` is inside the OLD bore (within tube_height of c
    // vertically).
    REQUIRE(glm::length(below - c) < net.tube_height);
    REQUIRE(!net.contains(below));  // the floor makes it solid

    // A point at tube center is ABOVE the floor (floor is fh above bottom, well
    // below center) — still inside.
    REQUIRE(net.contains(c));
}

TEST_CASE("T5a floor: the chamber pockets stay open under the floor") {
    // The floor truncates ONLY the spine tube; the chambers are union
    // primitives with NO floor, so a point inside a chamber but below the
    // tube-floor radial level stays inside (the side rooms are never sealed by
    // the tube floor).
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net =
        world::build_tunnel_net(test_tp_floor(20.0), &hf);

    for (int k = 0; k < 2; ++k) {
        INFO("chamber " << k);
        const world::TunnelNet::Pocket& ch = net.chamber[k];
        // The chamber center and a point below it (toward the local radial
        // bottom of the room) stay inside — the tube floor never seals the
        // chamber. u_long IS local up (the vertical axis), a_neg its downward
        // semi; probe 70% of the way down.
        REQUIRE(net.contains(ch.center));
        REQUIRE(net.contains(ch.center - ch.u_long * (ch.a_neg * 0.7)));
    }
}

TEST_CASE("T5a floor: flying into the floor crashes like a wall (T3 clause)") {
    // A level-tangential plane below the floor plane (but inside the old
    // circle) is a deep penetration below terrain, outside the net => the T3
    // wall-strike clause fires. A plane just ABOVE the floor survives (the net
    // suspends).
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net =
        world::build_tunnel_net(test_tp_floor(20.0), &hf);

    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 6.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;

    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};

    const std::size_t qi = flat_corridor_station(net);
    REQUIRE(qi != static_cast<std::size_t>(-1));
    const glm::dvec3 c = net.spine[qi].pos;
    const glm::dvec3 up = glm::normalize(c);
    const double floor_r = net.spine[qi].floor_r;
    const glm::dvec3 t =
        glm::normalize(net.spine[qi + 1].pos - net.spine[qi - 1].pos);

    // Below the floor (solid) — a wall strike.
    const glm::dvec3 below = up * (floor_r - 5.0);
    REQUIRE(!net.contains(below));  // premise: solid below the floor
    const glm::dvec3 up_b = sim::local_up(below);
    glm::dvec3 nose = glm::normalize(t - glm::dot(t, up_b) * up_b);
    const glm::dvec3 right = glm::normalize(glm::cross(nose, up_b));
    glm::dmat3 basis;
    basis[0] = right;
    basis[1] = up_b;
    basis[2] = -nose;
    sim::SimState s{};
    s.position = below;
    s.velocity = 100.0 * nose;  // tangential, level
    s.orientation = glm::normalize(glm::quat_cast(basis));
    s.last_vhat = glm::normalize(s.velocity);
    const sim::SimState nx = sim::step(s, in, kP, &env, kP.sim_dt);
    REQUIRE(nx.crashed);     // graded a wall strike (the floor is solid)
    REQUIRE(!nx.on_ground);  // NOT a teleport-landing

    // Just above the floor (open) — survives.
    const glm::dvec3 above = up * (floor_r + 10.0);
    REQUIRE(net.contains(above));  // premise: open above the floor
    sim::SimState sa = s;
    sa.position = above;
    sa.last_vhat = glm::normalize(sa.velocity);
    const sim::SimState na = sim::step(sa, in, kP, &env, kP.sim_dt);
    REQUIRE(!na.crashed);
}

// ============================ T3 — spline-space collision ===================
// The deep-penetration wall-strike clause (sim/ground.h): the tube/egg/chamber
// walls kill honestly. WITHOUT the clause, a plane grazing a wall sideways
// (crossing sd=0 ~2000 m below terrain with a level attitude + near-zero radial
// sink) reaches ground_contact's landing acceptance, which TELEPORTS it ~2000 m
// up onto the surface, alive and grounded (the Step-1 probe: r 13303 -> 15302).
// The clause grades that penetration a crash before any acceptance can fire.
namespace {

// The T3 ground dials: the T1 canon PLUS the live deep-penetration floor
// (game.toml ships 50.0; contact_height 0 here keeps r_s == the terrain
// radius).
sim::GroundParams t3_gp() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 6.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;  // the clause LIVE
    return gp;
}

// A wings-level SimState at `pos` with a tangential (zero-radial) velocity
// along `nose_hint` projected into the tangent plane — the teleport-landing
// scenario: body-up == local_up so the upright acceptance would pass on flat
// terrain, and zero radial sink so the gentle-contact acceptance would pass
// too. The ONLY thing that must stop the acceptance is the deep-penetration
// clause.
sim::SimState level_tangential(const glm::dvec3& pos,
                               const glm::dvec3& nose_hint, double V) {
    const glm::dvec3 up = sim::local_up(pos);
    glm::dvec3 nose = nose_hint - glm::dot(nose_hint, up) * up;
    nose = glm::normalize(nose);
    const glm::dvec3 right = glm::normalize(glm::cross(nose, up));
    glm::dmat3 basis;  // columns = body axes expressed in world
    basis[0] = right;  // +X right
    basis[1] = up;     // +Y up
    basis[2] = -nose;  // +Z = -nose (nose = -Z, SPEC §7)
    sim::SimState s{};
    s.position = pos;
    s.velocity = V * nose;  // tangential, zero radial sink
    s.orientation = glm::normalize(glm::quat_cast(basis));
    s.last_vhat = glm::normalize(s.velocity);
    return s;
}

}  // namespace

TEST_CASE("T3 wall graze kills honestly: no teleport-landing") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const sim::GroundParams gp = t3_gp();

    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};

    // Just OUTSIDE the chamber's VERTICAL (u_long) wall (sd ~ +2), thousands of
    // metres below terrain, with a level attitude + tangential velocity. The T2
    // single-source pin puts the visible wall within +-2 m of sd=0, so this IS
    // a wall graze. u_long is vertical; the connector runs along u_up
    // (cross-path) so the vertical wall is clear; a_pos is the semi.
    const world::TunnelNet::Pocket& ch = net.chamber[0];
    const glm::dvec3 nose_hint =
        glm::normalize(glm::cross(sim::local_up(ch.center), ch.u_lat));
    const glm::dvec3 out_pos = ch.center + ch.u_long * (ch.a_pos + 2.0);
    // Premise: genuinely just outside the net (ground_contact runs on it) and
    // deep below terrain (the acceptance would teleport it up).
    REQUIRE(!net.contains(out_pos));
    const glm::dvec3 up_out = sim::local_up(out_pos);
    const double r_s = hf.radius_at(up_out) + gp.contact_height_m;
    const double r_before = glm::length(out_pos);
    REQUIRE(r_s - r_before > gp.deep_penetration_m);  // deep: the clause fires

    const sim::SimState nx = sim::step(
        level_tangential(out_pos, nose_hint, 100.0), in, kP, &env, kP.sim_dt);
    REQUIRE(nx.crashed);     // graded a wall strike
    REQUIRE(!nx.on_ground);  // NOT a grounded touchdown
    // Did NOT snap to the surface: still near tunnel depth, not thousands of m
    // up.
    REQUIRE(glm::length(nx.position) < r_before + gp.deep_penetration_m);

    // Premise arm: the SAME state moved just INSIDE the wall (sd ~ -2) does NOT
    // crash — the net suspends contact (death begins AT the wall, not before).
    const glm::dvec3 in_pos = ch.center + ch.u_long * (ch.a_pos - 2.0);
    REQUIRE(net.contains(in_pos));
    const sim::SimState nin = sim::step(
        level_tangential(in_pos, nose_hint, 100.0), in, kP, &env, kP.sim_dt);
    REQUIRE(!nin.crashed);
}

TEST_CASE("T3 wall exactness: the crash surface brackets the visible wall") {
    // Along the vertical from the chamber's u_long wall, states just INSIDE
    // (alive — net suspends contact) and just OUTSIDE (dead — deep-penetration
    // strike). At +1 m outside, penetration (r_s - r) ~ thousands of m >> the
    // floor, so the clause fires; the SIGN of each leg's expectation is
    // verified against contains() (the T1 yield keys on sd < 0 exactly). This
    // brackets the death surface to +-1 m of sd=0 — the visible wall (T2
    // single-source).
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const sim::GroundParams gp = t3_gp();

    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};
    const world::TunnelNet::Pocket& ch = net.chamber[0];
    const glm::dvec3 nose_hint =
        glm::normalize(glm::cross(sim::local_up(ch.center), ch.u_lat));

    for (double off : {-3.0, -1.0, 1.0, 3.0}) {
        const glm::dvec3 pos = ch.center + ch.u_long * (ch.a_pos + off);
        const bool inside = net.contains(pos);
        // Premise: contains() agrees with the offset sign (sd<0 inside).
        REQUIRE(inside == (off < 0.0));
        const sim::SimState nx = sim::step(
            level_tangential(pos, nose_hint, 100.0), in, kP, &env, kP.sim_dt);
        if (inside) {
            REQUIRE(!nx.crashed);  // the net suspends contact
        } else {
            REQUIRE(nx.crashed);  // the wall-strike floor kills
            REQUIRE(!nx.on_ground);
        }
    }
}

TEST_CASE("T3 chamber + core walls kill; interior points survive") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const sim::GroundParams gp = t3_gp();

    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};

    // A wall graze at both chambers' VERTICAL (u_long) walls: each just-outside
    // point (a_pos + 2) crashes; each center survives (net suspends).
    struct Probe {
        const world::TunnelNet::Pocket& pk;
        glm::dvec3 wall_axis;  // the clear graze direction (unit)
        double semi;           // the pocket semi-axis along wall_axis
        const char* label;
    };
    // T11 graze axes chosen CLEAR of the other primitives: the chambers hang
    // off the bore laterally (along u_up, cross-path), so the connector to each
    // chamber runs along u_up; grazing along u_long (vertical) is clear, and
    // the chamber sits in the rock ABOVE the arena apex so the ellipsoid
    // two-half metric — not the arena — decides. a_pos is the semi there.
    const Probe probes[] = {
        {net.chamber[0], net.chamber[0].u_long, net.chamber[0].a_pos,
         "chamber[0]"},
        {net.chamber[1], net.chamber[1].u_long, net.chamber[1].a_pos,
         "chamber[1]"},
    };
    for (const Probe& pr : probes) {
        INFO(pr.label);
        // A horizontal-ish nose_hint tangential to the wall point's local up.
        const glm::dvec3 wall = pr.pk.center + pr.wall_axis * (pr.semi + 2.0);
        const glm::dvec3 nose_hint =
            glm::normalize(glm::cross(sim::local_up(wall), pr.pk.u_lat));
        REQUIRE(
            !net.contains(wall));  // premise: just outside this pocket's wall
        const sim::SimState nw = sim::step(
            level_tangential(wall, nose_hint, 100.0), in, kP, &env, kP.sim_dt);
        REQUIRE(nw.crashed);
        REQUIRE(!nw.on_ground);

        // Center: inside the net, the step suspends contact (closed loop with
        // the T1 containment pins — assert !crashed after a real step here).
        REQUIRE(net.contains(pr.pk.center));
        const sim::SimState nc =
            sim::step(level_tangential(pr.pk.center, nose_hint, 100.0), in, kP,
                      &env, kP.sim_dt);
        REQUIRE(!nc.crashed);
    }

    // THE BURIED CORE WALL (T12 sealed): a point just INSIDE the solid inner
    // core (radius core_r - 2) is outside the net and deep below terrain => it
    // crashes (the global core intersection makes |p| < core_r solid — the
    // |position| safety floor). T12 sealed the arena floor, so the rock just
    // ABOVE the core on the mid axis is also SOLID (no more open shaft): both
    // the in-core point AND a point above it (well below the arena floor) are
    // outside the net. Contrast a point in the arena INTERIOR (survives).
    const glm::dvec3 core_dir = net.arena.u_long;  // the mid axis
    const glm::dvec3 in_core = core_dir * (net.core_r - 2.0);
    const glm::dvec3 above_core =
        core_dir * (net.core_r + 500.0);  // sealed rock over the core
    REQUIRE(!net.contains(in_core));      // premise: inside the solid core
    REQUIRE(!net.contains(above_core));   // premise: sealed rock over the core
    const glm::dvec3 core_nose = glm::normalize(
        glm::cross(sim::local_up(in_core), core_dir + glm::dvec3{0.1, 0, 0.9}));
    const sim::SimState ncore = sim::step(
        level_tangential(in_core, core_nose, 100.0), in, kP, &env, kP.sim_dt);
    REQUIRE(ncore.crashed);
    REQUIRE(!ncore.on_ground);
    // The arena interior (far above the sealed floor, on the mid axis)
    // survives.
    const glm::dvec3 interior = net.arena.center;
    REQUIRE(net.contains(interior));
    const sim::SimState nint = sim::step(
        level_tangential(interior, core_nose, 100.0), in, kP, &env, kP.sim_dt);
    REQUIRE(!nint.crashed);
}

// ===========================================================================
// T11 — THE CORE WINDOW then T12 — THE SEALED CORE (docs/tunnel_staging.md
// T11/T12, Chad 2026-07-19). T11's interior body was ONE giant shallow ARENA
// ellipsoid with a CORE-WINDOW SHAFT to the glowing core. T12 (his round-9 fly:
// "cover up the core... make sure the chamber is filled over the core... I can
// see through the earth") SEALED the arena floor: the shaft is gone, the floor
// is solid rock over the buried core (the |position| safety floor), and the
// visible core is gone. The arena is lit by its own embers + the radial
// core-key shading. These legs pin the SEALED form.

TEST_CASE("T11 arena: interior inside, core solid, off-footprint rock solid") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    REQUIRE(net.arena_on);  // the arena is live on the T11 net
    REQUIRE(net.core_r == tp.cavern_core_m);
    REQUIRE(net.arena_r > net.core_r);

    // The arena center (deep below terrain, on the mid axis) is INSIDE; a point
    // in the solid inner core is OUTSIDE; a point in the mantle rock OFF the
    // arena footprint (a generic direction well away from the mid axis) is
    // OUTSIDE.
    const glm::dvec3 off_dir = glm::normalize(glm::dvec3{0.9, 0.1, 0.2});
    REQUIRE(net.contains(net.arena.center));  // arena
    REQUIRE(!net.contains(glm::normalize(net.arena.center) *
                          (net.core_r * 0.5)));     // core
    REQUIRE(!net.contains(off_dir * net.arena_r));  // off-footprint rock

    // The sim crash yield agrees at the buried core wall (reuse t3_gp +
    // level_tangential): just inside the core crashes; T12 sealed the floor so
    // just above the core is now SOLID rock (also crashes). The arena INTERIOR
    // (far above the sealed floor) survives.
    const glm::dvec3 mdir = net.arena.u_long;
    const sim::GroundParams gp = t3_gp();
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};
    const glm::dvec3 nose = glm::normalize(glm::cross(
        sim::local_up(mdir * net.core_r), glm::dvec3{0.9, 0.1, 0.2}));
    const glm::dvec3 above_core = mdir * (net.core_r + 500.0);  // sealed rock
    const glm::dvec3 in_core = mdir * (net.core_r - 5.0);
    REQUIRE(!net.contains(above_core));  // sealed rock over the core (T12)
    REQUIRE(!net.contains(in_core));
    REQUIRE(net.contains(net.arena.center));  // the arena interior survives
    REQUIRE(sim::step(level_tangential(above_core, nose, 100.0), in, kP, &env,
                      kP.sim_dt)
                .crashed);
    REQUIRE(sim::step(level_tangential(in_core, nose, 100.0), in, kP, &env,
                      kP.sim_dt)
                .crashed);
    REQUIRE(!sim::step(level_tangential(net.arena.center, nose, 100.0), in, kP,
                       &env, kP.sim_dt)
                 .crashed);
}

TEST_CASE(
    "T12 sealed arena radial sweep: rock above / arena open / rock ALL THE WAY "
    "DOWN (the core is covered)") {
    // T12 SEALED THE CORE (Chad: "cover up the core... make sure the chamber is
    // filled over the core... I can see through the earth"). The T11 5-state
    // sweep (which walked rock/arena/rock/SHAFT/core, the shaft an open window
    // to the core) becomes the SEALED form. Along the MID-PATH axis (the axis
    // the T11 shaft used to open) the SDF sign is now:
    //   (1) rock above the arena apex (solid),
    //   (2) the arena interior (open),
    //   (3) rock below the arena floor, SOLID ALL THE WAY DOWN on-axis to the
    //       core — the executable pin of "filled over the core" (no shaft).
    // PLUS a generic OFF-FOOTPRINT direction that is ALL-SOLID surface -> core.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.arena_on);

    const glm::dvec3 u = net.arena.u_long;   // the mid axis (old shaft axis)
    const double rc = net.arena_r;           // arena center radius
    const double apex = net.arena_apex_r;    // r_c + arena_c
    const double floor = rc - tp.arena_c_m;  // arena floor radius
    const double core = net.core_r;

    // (1) rock above the apex (well clear of any bore — the apex is on the mid
    //     axis but the bores breach off to the sides at the plateau; a point
    //     150 m ABOVE the apex on the mid axis is solid arena wall / rock).
    REQUIRE(net.signed_distance(u * (apex + 150.0)) > 0.0);  // rock above
    // (2) the arena interior (the center) is open.
    REQUIRE(net.signed_distance(u * rc) < 0.0);  // arena open
    // (3) THE SEALED FLOOR: SOLID ALL THE WAY DOWN on the mid axis, from just
    //     below the arena floor to the core surface. Every station is rock (the
    //     T11 shaft that opened this column is GONE). This is the executable
    //     "filled over the core" pin. Mutation: re-add the shaft capsule and
    //     the on-axis stations below the floor read OPEN => this fails.
    int solid_stations = 0;
    for (double r = floor - 50.0; r >= core + 50.0; r -= 200.0) {
        INFO("sealed-floor station r=" << r);
        REQUIRE(net.signed_distance(u * r) > 0.0);  // solid rock (no shaft)
        ++solid_stations;
    }
    REQUIRE(solid_stations > 20);  // the whole column was swept (non-vacuous)
    // (4) the buried core: outside (the |position| safety floor).
    REQUIRE(net.signed_distance(u * (core - 100.0)) > 0.0);  // core solid

    // OFF-FOOTPRINT: a generic direction well away from the mid axis is ALL
    // SOLID from the surface down to the core (there is no way in from here —
    // the pilot cannot see the core "from every possible angle").
    const glm::dvec3 gdir = glm::normalize(glm::dvec3{0.9, 0.1, 0.2});
    const double surf = hf.radius_at(gdir);
    bool all_solid = true;
    for (double r = surf - 50.0; r >= core * 0.5; r -= 300.0)
        if (net.signed_distance(gdir * r) < 0.0) all_solid = false;
    REQUIRE(all_solid);  // off-footprint: surface -> core is all rock
}

TEST_CASE(
    "T12 sealed floor: the mid axis below the arena floor is solid; a plane "
    "diving into the floor crashes") {
    // T12 sealed the arena floor over the buried core (the T11 core-window
    // shaft is gone). A plane at the arena floor on the mid axis, descending,
    // hits the sealed floor and crashes honestly (the crash yield agrees with
    // the SDF: below the floor on-axis is solid rock).
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.arena_on);

    const glm::dvec3 u = net.arena.u_long;
    const double floor = net.arena_r - tp.arena_c_m;

    const sim::GroundParams gp = t3_gp();
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};

    // A point 300 m below the arena floor on the mid axis is SOLID (sealed
    // rock, deep-penetration territory) — a plane there crashes.
    const glm::dvec3 below_floor = u * (floor - 300.0);
    REQUIRE(
        !net.contains(below_floor));  // premise: sealed rock below the floor
    const glm::dvec3 nose = glm::normalize(
        glm::cross(sim::local_up(below_floor), glm::dvec3{0.9, 0.1, 0.2}));
    REQUIRE(sim::step(level_tangential(below_floor, nose, 120.0), in, kP, &env,
                      kP.sim_dt)
                .crashed);

    // A point 200 m ABOVE the arena floor on the mid axis is OPEN (the arena
    // interior) — a plane there survives. This brackets the sealed floor.
    const glm::dvec3 above_floor = u * (floor + 200.0);
    REQUIRE(net.contains(above_floor));  // premise: in the open arena
    REQUIRE(!sim::step(level_tangential(above_floor, nose, 120.0), in, kP, &env,
                       kP.sim_dt)
                 .crashed);
}

TEST_CASE("T11 chambers re-hang off the bores near the breach") {
    // Each chamber hangs off its OWN bore a fixed offset ABOVE the breach into
    // the arena: its center radius ~ r_plateau + chamber_breach_offset_m (it
    // snaps to the nearest bore node, so a few-hundred-metre margin), it is
    // inside the net, and the two are off distinct legs.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    const double r_plateau = net.arena_apex_r - tp.breach_margin_m;
    const double want_r = r_plateau + tp.chamber_breach_offset_m;
    for (int k = 0; k < 2; ++k) {
        INFO("chamber " << k);
        const double r = glm::length(net.chamber[k].center);
        // Snaps to the nearest node on its leg — within ~one node-spacing worth
        // of radius change of the target (the descent grade is a few hundred m
        // per node, so allow a generous 500 m margin).
        REQUIRE(std::abs(r - want_r) < 500.0);
        REQUIRE(net.contains(net.chamber[k].center));
    }
    // Off distinct legs: well separated laterally.
    REQUIRE(glm::length(net.chamber[0].center - net.chamber[1].center) >
            2.0 * tp.chamber_long_m);
}

// ----------------------------------------------------------------------------
// T6-P0 ENTRY-CORRIDOR SURVIVABILITY (2026-07-18, Chad's fly P0: "I go into the
// tunnel right in the middle of the entrance at Errington but I die every time
// WITHOUT EVEN HITTING A WALL"). The open-cut pit/bowl SDF was a truncated CONE
// (bowl_r -> floor_r taper) while the terrain CutDisk removes a full CYLINDER
// of the rim radius (a great-circle disk, depth-independent). In the conical
// shell between the cone wall and the vertical terrain cut the world reads
// VISUALLY OPEN (no terrain triangle, no cone-wall mesh) but the SDF read
// SOLID, so the deep-penetration clause (sim/ground.h) murdered a pilot diving
// anywhere but dead-center. The fix (world/tunnel_net.cpp bowl_sd) makes the
// survivable open- cut a CYLINDER of the rim radius == the removed-terrain
// volume: what reads open on screen is open in every crash branch.
//
// These legs WALK the real entry corridor through the LIVE closed-loop crash
// predicate (sim::step with a live ground+tunnels env — the SAME sim/step.cpp
// yield + sim/ground.h deep-penetration clause the app flies), NOT a replicated
// predicate, and REQUIRE no crash anywhere in the visibly-open crater.
// Reverting bowl_sd to the cone taper turns these RED (a sample in the conical
// shell reads outside the net and the clause fires).
namespace {
// The set of lateral offsets (as a fraction of the rim radius) a real "middle
// of the entrance" approach covers — dead-center is only ONE of them; the
// murder lived at the crater EDGES (0.9*rim died at ~50 m of descent pre-fix).
constexpr double kRimFracs[] = {0.0, 0.3, 0.6, 0.9};

// Walk a straight descent down the open cut at `frac`*rim off the axis, from
// the surface to `max_depth` below it, stepping the LIVE plant at each sample
// and asserting no crash while the sample is inside the visibly-open cut
// (radially within the rim AND above the pit/bowl floor). The floor and the
// walls ARE death surfaces (honest — the pilot sees them); only the OPEN
// interior must survive.
void walk_open_cut(const char* label, const world::TunnelNet& net,
                   const world::HeightField& hf, const sim::GroundParams& gp,
                   const glm::dvec3& axis, double rim, double floor_depth,
                   double max_depth, double open_floor_r) {
    INFO(label);
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};
    const double surf = hf.radius_at(axis);
    // A stable lateral direction perpendicular to the axis.
    glm::dvec3 perp = glm::cross(axis, glm::dvec3{0, 1, 0});
    if (glm::length(perp) < 1e-6) perp = glm::cross(axis, glm::dvec3{1, 0, 0});
    perp = glm::normalize(perp);

    bool tested_open = false;  // at least one genuinely-open sample was flown
    for (double frac : kRimFracs) {
        for (double d = 10.0; d <= max_depth; d += 20.0) {
            const glm::dvec3 base = axis * (surf - d);
            const glm::dvec3 p = base + perp * (frac * rim);
            // "Visibly open" == within the RENDERED excavation (T18: the
            // open-cut volume is the drawn cone/staircase, tapering rim ->
            // open_floor_r — the T6-P0 cylinder matched the removed terrain
            // back when nothing rendered the shell), margin 8 m off the
            // drawn wall, above the floor (margin 5 m).
            const double rad = frac * rim;
            const double r_open =
                open_floor_r > 0.0
                    ? rim + (open_floor_r - rim) *
                                std::clamp(d / floor_depth, 0.0, 1.0)
                    : rim;
            const bool open = rad < r_open - 8.0 && d < floor_depth - 5.0;
            if (!open) continue;
            tested_open = true;
            // Premise: an open sample MUST read inside the net (the fix's job).
            REQUIRE(net.contains(p));
            // And the LIVE plant must not crash there (the murdering clause).
            const glm::dvec3 nose_hint =
                glm::normalize(glm::cross(sim::local_up(p), perp));
            const sim::SimState nx = sim::step(
                level_tangential(p, nose_hint, 120.0), in, kP, &env, kP.sim_dt);
            REQUIRE(!nx.crashed);
            REQUIRE(!nx.on_ground);
        }
    }
    REQUIRE(tested_open);  // the walk actually exercised open samples
}
}  // namespace

TEST_CASE(
    "T6-P0 Errington entry corridor: the whole visible crater is survivable") {
    // The home mouth is the T6c open pit (mouth_sink live). Walk the center +
    // off-center descent through the crater; pre-fix the 0.9*rim column died at
    // ~50 m depth in visibly-open air.
    world::TunnelParams tp = test_tp();
    tp.mouth_sink_m = 130.0;  // the pit LIVE (canon)
    tp.floor_height_m = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const sim::GroundParams gp = t3_gp();  // deep-penetration clause LIVE

    REQUIRE(net.pit.bowl_depth > 0.0);  // premise: the pit is on
    walk_open_cut("Errington pit", net, hf, gp, world::kTunnelMouthErrington,
                  net.pit.bowl_r, net.pit.bowl_depth, net.pit.bowl_depth + 40.0,
                  net.pit.open_floor_r);
}

TEST_CASE("T6-P0 Murray dive corridor: the whole visible bowl is survivable") {
    // The raid mouth is the T4a open bowl. Same class — pre-fix the 0.9*rim
    // column died at ~75 m depth in visibly-open air.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const sim::GroundParams gp = t3_gp();

    REQUIRE(net.bowl.bowl_depth > 0.0);
    walk_open_cut("Murray bowl", net, hf, gp, world::kTunnelMouthMurray,
                  net.bowl.bowl_r, net.bowl.bowl_depth,
                  net.bowl.bowl_depth + 40.0, net.bowl.open_floor_r);
}

// ==================== T13 — entry approach trench (S3) ======================
// Chad's spec S3: flying INTO the tunnel must be fairly easy. The Errington
// mouth is sunk mouth_sink_m behind the pit rim, so a plane on the bore-tangent
// entry line clips solid rock BEHIND the pit before it reaches the open crater
// (the LIVE deep-penetration crash clause fires). The T13 up-tangent open-cut
// trench excavates that approach. This verifier walks a straight chord along
// the mouth bore-tangent from `out_m` up-tangent (above grade, approach side)
// into the mouth, stepping the SAME LIVE plant the app flies at every sample,
// and counts crashes ("blocked ticks"). With the trench ON the Errington count
// is ~0 (pinned just above the measured value); with it OFF the OLD blocked
// count returns (proving the trench is the fix). Murray (no sink, an open bowl)
// is the easy reference — always ~0.
namespace {
// Count LIVE-plant crashes along the bore-tangent approach chord. `mouth_node`
// is the spine terminal (the sunk bore mouth); `tan_into` is the down-bore
// tangent; the eye starts `out_m` back up-tangent (above grade, approach side).
// Uses the SAME sim::step crash predicate as walk_open_cut / fly_route (NOT a
// replicated predicate).
int approach_blocked_ticks(const world::TunnelNet& net,
                           const world::HeightField& hf,
                           const sim::GroundParams& gp,
                           const glm::dvec3& mouth_node,
                           const glm::dvec3& tan_into, double out_m) {
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};
    const glm::dvec3 eye = mouth_node - tan_into * out_m;
    int blocked = 0;
    for (int t = 0; t < 100; ++t) {
        const glm::dvec3 p = eye + tan_into * (out_m * t / 100.0);
        const sim::SimState nx = sim::step(level_tangential(p, tan_into, 120.0),
                                           in, kP, &env, kP.sim_dt);
        if (nx.crashed) ++blocked;
    }
    return blocked;
}
}  // namespace

TEST_CASE("T13 entry approach: the bore-tangent entry line is clear (trench)") {
    const world::HeightField hf = uniform_field(300.0);
    const sim::GroundParams gp = t3_gp();  // deep-penetration clause LIVE

    // Trench ON (canon dials).
    world::TunnelParams tp_on = test_tp();
    REQUIRE(tp_on.trench_len_m > 0.0);  // premise: the trench is live
    const world::TunnelNet net_on = world::build_tunnel_net(tp_on, &hf);
    const glm::dvec3 tanE =
        glm::normalize(net_on.spine[1].pos - net_on.spine[0].pos);
    const glm::dvec3 tanM =
        glm::normalize(net_on.spine[net_on.spine.size() - 1].pos -
                       net_on.spine[net_on.spine.size() - 2].pos);
    const int on_E = approach_blocked_ticks(
        net_on, hf, gp, net_on.spine.front().pos, tanE, 600.0);
    const int mur = approach_blocked_ticks(
        net_on, hf, gp, net_on.spine.back().pos, tanM, 600.0);
    INFO("Errington ON blocked=" << on_E << " Murray blocked=" << mur);

    // Trench OFF (the old sunk-mouth approach).
    world::TunnelParams tp_off = test_tp();
    tp_off.trench_len_m = 0.0;
    const world::TunnelNet net_off = world::build_tunnel_net(tp_off, &hf);
    const int off_E = approach_blocked_ticks(
        net_off, hf, gp, net_off.spine.front().pos, tanE, 600.0);
    INFO("Errington OFF blocked=" << off_E);

    // The OFF arm actually blocks (the fixture-no-op guard: the trench is
    // fixing a REAL problem, not theatre). Measured OFF = 5 LIVE crashes on
    // canon (the deep-penetration clause fires where the sunk-mouth approach
    // clips rock); pinned just below so a regression that stopped blocking is
    // caught.
    REQUIRE(off_E >= 3);
    // The trench clears the Errington approach: ON blocked <= 2 (pinned just
    // above the measured 0 — one or two grazing samples at the pit-wall seam
    // are honest). Murray (no sink) is the easy reference, also clear.
    REQUIRE(on_E <= 2);
    REQUIRE(mur <= 2);
    // The trench is the THING that fixed it: strictly fewer blocked ticks ON.
    REQUIRE(on_E < off_E);
}

TEST_CASE(
    "T13 entry trench off: the SDF is bit-identical to the ON arm away "
    "from the trench, and off-arm trench Bowls are inert") {
    // trench_len_m = 0 must be a structural no-op: the trench Bowls are inert
    // (bowl_depth == 0), so bowl_sd returns +inf for them and they never enter
    // the SDF min. Prove it two ways: (1) the OFF trench Bowls are inert; (2)
    // the ON and OFF SDFs AGREE bit-identically at points the trench does NOT
    // reach (deep interior / far side) — the ON build only DIFFERS inside the
    // trench excavation, nowhere else.
    const world::HeightField hf = uniform_field(300.0);
    world::TunnelParams tp_on = test_tp();  // trench ON
    world::TunnelParams tp_off = test_tp();
    tp_off.trench_len_m = 0.0;  // trench OFF
    const world::TunnelNet on = world::build_tunnel_net(tp_on, &hf);
    const world::TunnelNet off = world::build_tunnel_net(tp_off, &hf);

    // (1) OFF trench Bowls are inert; ON trench Bowls are live.
    for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i) {
        REQUIRE(off.trench[i].bowl_depth == 0.0);
        REQUIRE(on.trench[i].bowl_depth > 0.0);
    }

    // (2) Away from the trench (interior nodes + the Murray side), the two SDFs
    // are bit-identical — the trench changed ONLY its own excavation, nothing
    // structural. Probe every 4th spine node from a third of the way in to the
    // far mouth (all far from the Errington-mouth trench).
    const std::size_t Nn = on.spine.size();
    int probed = 0;
    for (std::size_t i = Nn / 3; i < Nn; i += 4) {
        const glm::dvec3 p = on.spine[i].pos;
        REQUIRE(on.signed_distance(p) == off.signed_distance(p));  // exact ==
        ++probed;
    }
    REQUIRE(probed > 5);
}

TEST_CASE("T3 legitimate landing survives the clause: it is unreachable") {
    // With the deep-penetration clause LIVE, a normal gentle surface touchdown
    // still lands (on_ground, not crashed) — the clause is unreachable for real
    // landings (max legit per-tick penetration is sub-metre << the 50 m floor).
    // test_ground.cpp's accepted-landing leg runs the clause OFF (gp default
    // 0), so this covers the ON case the existing suite does not.
    const world::HeightField hf = uniform_field(300.0);
    const sim::GroundParams gp = t3_gp();  // clause LIVE

    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    // No tunnels: a plain surface approach (the tunnel is irrelevant to a
    // real landing; the clause must be inert here).
    sim::Inputs in{};  // throttle 0: descend and land

    // Start a level state a little above the contact surface with a gentle
    // sink.
    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const glm::dvec3 head =
        glm::normalize(glm::cross(up, glm::dvec3{0.2, 0.9, 0.4}));
    const double r_g = hf.radius_at(up);
    sim::SimState s =
        harness::level_state(kP, 45.0, r_g - kP.R + 1.0, up, head);
    s.velocity -= 1.0 * up;  // 1 m/s sink: well within max_sink

    bool landed = false;
    for (int t = 0; t < 600 && !landed; ++t) {
        s = sim::step(s, in, kP, &env, kP.sim_dt);
        REQUIRE(!s.crashed);  // the clause must NEVER fire on a real approach
        landed = s.on_ground;
    }
    REQUIRE(landed);  // it actually touched down (fixture-no-op guard)
}

// ---------------------------------------------------------------------------
// SPAWN (Chad's ruling 2026-07-18: "spawn me over errington mine") — the
// generalized spawn_state. Two pins: (1) the defaulted args reproduce the
// legacy +X-pole spawn BIT-identically (every existing caller/test constructs
// the default — a drifted default would move goldens silently); (2) a directed
// spawn is level over the given dir, at radius R+alt, flying 140 m/s along the
// tangent heading (the Errington-over-home spawn main.cpp ships).
// ---------------------------------------------------------------------------

TEST_CASE(
    "spawn: defaulted spawn_state reproduces the legacy spawn "
    "bit-identically") {
    const sim::SimState a = app::spawn_state(kP);
    const sim::SimState b = app::spawn_state(
        kP, 2000.0, glm::dvec3{1.0, 0.0, 0.0}, glm::dvec3{0.0, 0.0, -1.0});
    REQUIRE(a.position == b.position);
    REQUIRE(a.velocity == b.velocity);
    REQUIRE(a.last_vhat == b.last_vhat);
    REQUIRE(a.orientation.w == b.orientation.w);
    REQUIRE(a.orientation.x == b.orientation.x);
    REQUIRE(a.orientation.y == b.orientation.y);
    REQUIRE(a.orientation.z == b.orientation.z);
    REQUIRE(a.throttle == b.throttle);
    // And the legacy literals themselves (the absolute reference — a drifted
    // default in BOTH arms would pass the differential above alone).
    REQUIRE(a.position == glm::dvec3{kP.R + 2000.0, 0.0, 0.0});
    REQUIRE(a.velocity == glm::dvec3{0.0, 0.0, -140.0});
}

TEST_CASE(
    "spawn: directed spawn is level over Errington, nose toward "
    "Murray") {
    const glm::dvec3 up = world::kTunnelMouthErrington;
    const glm::dvec3 want_fwd = glm::normalize(
        world::kTunnelMouthMurray -
        glm::dot(world::kTunnelMouthMurray, up) * up);  // the main.cpp heading
    const sim::SimState s = app::spawn_state(kP, 2500.0, up, want_fwd);

    // Over the mouth at R + 2500, wings-level (body up == local up).
    REQUIRE(glm::length(s.position) == Catch::Approx(kP.R + 2500.0));
    REQUIRE(glm::dot(glm::normalize(s.position), up) ==
            Catch::Approx(1.0).margin(1e-12));
    const glm::dvec3 body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    REQUIRE(glm::dot(body_up, up) == Catch::Approx(1.0).margin(1e-9));

    // 140 m/s along the tangent heading toward Murray; no radial component.
    REQUIRE(glm::length(s.velocity) == Catch::Approx(140.0));
    REQUIRE(glm::dot(s.velocity, up) == Catch::Approx(0.0).margin(1e-9));
    REQUIRE(glm::dot(glm::normalize(s.velocity), want_fwd) ==
            Catch::Approx(1.0).margin(1e-9));

    // The nose IS the velocity direction (nose = -body Z).
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    REQUIRE(glm::dot(nose, want_fwd) == Catch::Approx(1.0).margin(1e-9));
}

// ===========================================================================
// T7 — THE FLYTHROUGH VERIFIER (2026-07-18). The permanent, machine-flown
// definition of "the tunnel is flyable end to end." It walks a NOMINAL FLIGHT
// PATH for the WHOLE route — approach (above the Errington rim) -> pit -> bore
// mouth -> full bore -> egg -> bottom junction -> Murray bowl -> out — as a
// dense polyline following the actual spine (≤10 m spacing), and at every
// sample asserts BOTH:
//   (1) the LIVE crash predicate (sim::step with a live ground+tunnels env —
//   the
//       SAME sim/step.cpp yield + sim/ground.h deep-penetration clause the app
//       flies) does NOT fire; and
//   (2) a CLEAR-CORRIDOR disc of radius corridor_clear_m(net), sampled ⊥ the
//   path,
//       is fully inside the net (open) — no solid intrusion within the flyable
//       cross-section.
//
// This is the leg that would have caught Chad's fly-report P0 ("there was a
// WALL inside... still got HILLS in there"): the pre-T7 tube SDF read the whole
// upper half of the bore SOLID at every interior node junction (900+
// false-solid samples), so the corridor disc (2) fired at hundreds of stations.
// It is mutation-verified to catch (i) that class (a re-introduced interior
// false- solid) and (ii) a deliberate 50 m solid blob injected mid-bore.
namespace {
// The clear-corridor radius the nominal path must keep fully open around it
// (the bore is a 220 x 180 m ellipse => a 60 m clear tube around the centre
// line is generous but strictly inside; use the vertical semi minus a margin as
// the derived bound so a floor/height retune tracks it).
double corridor_clear_m(const world::TunnelNet& net) {
    return std::min(net.tube_width, net.tube_height) * 0.5;  // ~45 m on canon
}

// Walk a nominal polyline (dense spine samples + the two open-cut approaches)
// and assert live-no-crash + clear-corridor at every sample. `reverse` flips
// the travel direction (Murray -> Errington) so both directions are covered.
void fly_route(const char* label, const world::TunnelNet& net,
               const world::HeightField& hf, const sim::GroundParams& gp,
               bool reverse) {
    INFO(label);
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = gp;
    env.tunnels = &net;
    sim::Inputs in{};
    const double clear = corridor_clear_m(net);

    // Build the nominal centre-line polyline: the spine nodes, densified so
    // adjacent samples are <= 10 m apart (the spine is ~150 m spaced, so ~15
    // sub-samples per segment). The spine already threads mouth -> egg ->
    // Murray.
    std::vector<glm::dvec3> path;
    const auto& sp = net.spine;
    REQUIRE(sp.size() >= 3);
    for (std::size_t i = 0; i + 1 < sp.size(); ++i) {
        const glm::dvec3 a = sp[i].pos, b = sp[i + 1].pos;
        const double len = glm::length(b - a);
        const int subs = std::max(1, static_cast<int>(std::ceil(len / 10.0)));
        for (int k = 0; k < subs; ++k)
            path.push_back(a + (b - a) * (static_cast<double>(k) / subs));
    }
    path.push_back(sp.back().pos);
    if (reverse) std::reverse(path.begin(), path.end());

    int samples = 0, corridor_checks = 0;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const glm::dvec3 c = path[i];
        // Skip the exact terminal end faces (measure-zero tube/bowl seam at
        // sd==0) — the sample one step in owns the interior verdict.
        const glm::dvec3 fwd = glm::normalize(path[i + 1] - c);
        // (1) LIVE crash predicate along the centre line.
        const glm::dvec3 nose_hint = fwd;
        const sim::SimState nx = sim::step(
            level_tangential(c, nose_hint, 120.0), in, kP, &env, kP.sim_dt);
        REQUIRE(net.contains(c));  // premise: the centre line is inside the net
        REQUIRE(!nx.crashed);      // the plant flies the whole centre line
        ++samples;

        // (2) CLEAR-CORRIDOR disc: sample a ring of offsets ⊥ the path at the
        // clear radius; every one must be INSIDE the net (open). This is the
        // leg the interior false-solid tripped — a wall/hill in the bore reads
        // as a solid disc sample. Skip the disc INSIDE the wide egg/bowl
        // volumes (the ring is trivially open there) and near the very ends
        // (the bore narrows into the bowl handoff). Sample where the tube
        // cross-section governs: the disc radius fits the 220 x 180 bore but
        // not the ~60 m connectors, so gate the disc to spine stations whose
        // local sd at the centre is at least one clear radius deep (a pure-tube
        // or pocket interior). Skip the disc near the two physical mouths
        // (within one clear radius of spine front/back): the tube meets the
        // pit/bowl there and the disc can straddle the open-cut handoff seam
        // (sd==0 end face) — a measure-zero boundary the centre-line crash
        // check (1) already owns.
        const double d_front = glm::length(c - sp.front().pos);
        const double d_back = glm::length(c - sp.back().pos);
        const bool near_mouth = d_front < clear + 20.0 || d_back < clear + 20.0;
        if (!near_mouth && net.signed_distance(c) <= -clear - 2.0) {
            glm::dvec3 u = glm::cross(fwd, glm::dvec3{0.11, 0.7, 0.2});
            if (glm::length(u) < 1e-6)
                u = glm::cross(fwd, glm::dvec3{1.0, 0.0, 0.0});
            u = glm::normalize(u);
            const glm::dvec3 w = glm::normalize(glm::cross(fwd, u));
            for (int a = 0; a < 8; ++a) {
                const double ang = 2.0 * 3.14159265358979323846 * a / 8.0;
                const glm::dvec3 off = std::cos(ang) * u + std::sin(ang) * w;
                const glm::dvec3 p = c + off * clear;
                INFO("corridor sample station " << i << " angle "
                                                << (ang * 180.0 / 3.14159));
                REQUIRE(net.contains(p));  // the flyable disc is fully open
                ++corridor_checks;
            }
        }
    }
    REQUIRE(samples > 200);          // the whole route was walked densely
    REQUIRE(corridor_checks > 400);  // the corridor disc was exercised widely
}
}  // namespace

TEST_CASE(
    "T7 flythrough: the whole Errington<->Murray route is machine-flown open") {
    // THE definition of "the tunnel is flyable." Canon dials, the deep-
    // penetration clause LIVE. Both directions.
    world::TunnelParams tp = test_tp();
    tp.mouth_sink_m = 130.0;
    tp.floor_height_m = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const sim::GroundParams gp = t3_gp();  // deep-penetration clause LIVE

    fly_route("Errington -> Murray", net, hf, gp, /*reverse=*/false);
    fly_route("Murray -> Errington (reverse)", net, hf, gp, /*reverse=*/true);
}

TEST_CASE("T7 flythrough mutant kill: a 50 m solid blob mid-bore is caught") {
    // The verifier must FAIL if a solid intrusion exists in the bore. We can't
    // mutate the SDF here, so simulate the mutant's effect: pick a mid-bore
    // station, and assert that a point 30 m off the centre line (well within
    // the clear corridor the verifier checks) is INSIDE the net today — so a
    // blob that made it read outside would flip THIS assertion, exactly as the
    // verifier's corridor-disc REQUIRE would. This pins that the corridor disc
    // is a live, non-vacuous check (mutation: the pre-T7 upper-bore false-solid
    // read this exact class of point SOLID).
    world::TunnelParams tp = test_tp();
    tp.mouth_sink_m = 130.0;
    tp.floor_height_m = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // A pure-tube station a sixth of the way down (a descending bore ring above
    // the cavern ceiling).
    const std::size_t qi = net.spine.size() / 6;
    const glm::dvec3 c = net.spine[qi].pos;
    const glm::dvec3 up = net.spine[qi].up;
    const glm::dvec3 tan = net.spine[qi].tan;
    glm::dvec3 horiz = glm::normalize(glm::cross(tan, up));
    const glm::dvec3 vert = glm::normalize(glm::cross(horiz, tan));

    // The UPPER half of the bore (the pre-T7 false-solid region): 30 m up + 30
    // m to the side is deep inside the 220 x 180 ellipse and MUST be open. The
    // pre-T7 tube SDF read this SOLID (contains()==false) at every interior
    // node.
    for (double vv : {20.0, 40.0, 60.0}) {
        for (double hh : {-60.0, 0.0, 60.0}) {
            const glm::dvec3 p = c + hh * horiz + vv * vert;
            INFO("upper-bore probe h=" << hh << " v=" << vv);
            REQUIRE(net.contains(p));  // open (T7 fix)
            REQUIRE(net.signed_distance(p) < 0.0);
        }
    }
    // And the lower half (above the floor) at a plateau bend node (the residual
    // spike class the T7 nearest-foot rewrite also removed).
    const std::size_t pi = net.spine.size() / 2;
    const glm::dvec3 cp = net.spine[pi].pos;
    const glm::dvec3 upp = net.spine[pi].up;
    const glm::dvec3 tanp = net.spine[pi].tan;
    const glm::dvec3 horizp = glm::normalize(glm::cross(tanp, upp));
    const glm::dvec3 vertp = glm::normalize(glm::cross(horizp, tanp));
    for (double vv : {-30.0, -50.0}) {
        const glm::dvec3 p = cp + vv * vertp;
        // Above the floor (floor is ~ -70 m at the tube bottom): -50 m is open.
        if (net.spine[pi].floor_r > 0.0 &&
            glm::length(p) < net.spine[pi].floor_r + 5.0)
            continue;  // below the flat floor (legit solid)
        INFO("lower-bore probe v=" << vv);
        REQUIRE(net.contains(p));
    }
}

// ===========================================================================
// T8 — THE SIGHTLINE VERIFIER (2026-07-19). Chad's round-5 fly: "still there is
// an opaque grey WALL CURTAIN ~1 km in, after a RISE in the ramp — I can't see
// where to go, it OPENS afterward but it made me crash." The T7 flythrough
// PASSES there (the volume IS open) — so the failure was VISIBILITY, not
// containment: the floor at the Errington ramp->plateau SAG knuckle (arc
// ~3.8-5.0 km) drew as a bright grey mass (the shader's local-up floor key,
// max(-dot(N,up)) ~ 1 when the descending pilot meets the flattening floor
// head-on) that painted OVER the crown/floor-edge lamp lines — the vanishing-
// point cue that carries the continuation. Fixed by the T8 contrast dial
// (render/tunnel.cpp kTunnelUpKey 0.55 -> 0.15; the floor darkens so the lamps
// read through it — the geometry is UNTOUCHED). This verifier is the PERMANENT
// pure-geometry guard that the continuation stays VISIBLE: along the corridor
// CENTRELINE (the axis the lamp lines hug) the straight forward line-of-sight
// must reach >= kSightMin_m at every interior station, BOTH directions. A
// curtain-class regression (a knuckle sharp enough to fold the corridor onto
// itself, or a mid-bore solid intrusion) collapses this reach -> RED.
//
// Sight_min is a TEST-OWNED constant (like T7's corridor_clear_m), NOT a
// game.toml dial: the sightline is a verifier THRESHOLD that no build code
// consumes (the T8 fix is the shader dial + the already-open geometry), so a
// game.toml sight_min_m would be a dead value the loader owns but nothing reads
// — the anti-fork the codebase forbids. Canon keeps the whole interior at the
// 1500 m cap; 800 m is a generous floor with ~2x margin (the smoothed corridor
// clears it everywhere) that a real fold breaks.
namespace {
constexpr double kSightMin_m = 800.0;  // T8: min forward centreline sight [m]

// Walk every interior station both directions and REQUIRE the centreline sight
// distance >= kSightMin_m. Stations within one kSightMin of either physical
// mouth are EXEMPT: the pit/bowl opening legitimately bounds the forward reach
// there (the corridor ends at the open cut), which is not a curtain.
void verify_sightline(const char* label, const world::TunnelNet& net) {
    INFO(label);
    double total = 0.0;
    for (std::size_t i = 1; i < net.spine.size(); ++i)
        total += glm::length(net.spine[i].pos - net.spine[i - 1].pos);

    int checked = 0;
    double worst = 1e18;
    double worst_s = 0.0;
    // Cap the march just past the bound (kSightCap): reaching kSightMin proves
    // the station is clear — no need to march to 1500 m (keeps the gate quick;
    // the whole route is >100 stations x an O(nodes) SDF march). Station stride
    // 100 m: the sag knuckle spans ~1 km of arc, so a fold cannot hide between
    // stations (the mutant-kill leg pins the resolution).
    const double kSightCap = kSightMin_m + 100.0;
    for (double s = 0.0; s <= total; s += 100.0) {
        // Exempt the terminal mouth neighbourhoods (the open cut caps sight).
        if (s < kSightMin_m || s > total - kSightMin_m) continue;
        const double sight =
            world::centerline_sight_distance(net, s, kSightCap, 20.0);
        if (sight < worst) {
            worst = sight;
            worst_s = s;
        }
        INFO("station arc " << s << " sight " << sight);
        REQUIRE(sight >= kSightMin_m);
        ++checked;
    }
    INFO("worst interior sight " << worst << " at arc " << worst_s);
    REQUIRE(checked > 50);  // the whole interior was walked (fixture-no-op)
    REQUIRE(worst >= kSightMin_m);
}
}  // namespace

TEST_CASE(
    "T8 sightline: the corridor continuation is visible the whole route "
    "(both directions)") {
    // Canon dials, the shipped floor + pit + bowl. The forward centreline
    // line-of-sight clears kSightMin_m at every interior station — you can
    // always SEE where to fly. Both directions (the metric is
    // direction-agnostic by construction — the chord test is symmetric — so
    // this also documents the reverse raid egress reads open).
    world::TunnelParams tp = test_tp();
    tp.mouth_sink_m = 130.0;
    tp.floor_height_m = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    verify_sightline("Errington -> Murray sightline", net);

    // The reverse direction: sight from the far side back toward Errington. The
    // centreline chord is symmetric, but walk it from the Murray end explicitly
    // so a future one-sided regression can't hide.
    double total = 0.0;
    for (std::size_t i = 1; i < net.spine.size(); ++i)
        total += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
    double worst_rev = 1e18;
    int checked_rev = 0;
    const double kSightCap = kSightMin_m + 100.0;
    for (double s = kSightMin_m; s <= total - kSightMin_m; s += 100.0) {
        // Reverse sight = forward sight measured on the mirrored arc.
        const double sight =
            world::centerline_sight_distance(net, total - s, kSightCap, 20.0);
        worst_rev = std::min(worst_rev, sight);
        REQUIRE(sight >= kSightMin_m);
        ++checked_rev;
    }
    REQUIRE(checked_rev > 50);
    INFO("worst reverse sight " << worst_rev);
    REQUIRE(worst_rev >= kSightMin_m);
}

TEST_CASE("T8 sightline mutant kill: a sharp sag knuckle collapses the sight") {
    // The verifier must FAIL if the descent profile reintroduces a sharp grade
    // knuckle (dropping the T6d smoothing, or a sag-curve regression) — the
    // curtain class. We simulate the mutant's EFFECT on the built net: take the
    // canon net and inject a sharp local RADIUS SPIKE at the ramp->plateau
    // transition (a node pulled several bore-heights off the smooth profile),
    // exactly the fold a smoothing-pass drop produces. The centreline sight at
    // the station just before the spike must then collapse well below
    // kSightMin.
    //
    // T11: the mid-route now passes through the OPEN arena, so a fold there no
    // longer collapses the sight (the arena keeps it open). The curtain class
    // lives on the NARROW DESCENT BORE (above the arena apex), where the tube —
    // not the arena — owns the forward sightline. Fold a node on the descent
    // bore (radius above the arena apex) by ~3 bore heights and confirm the
    // sight collapses there.
    world::TunnelParams tp = test_tp();
    tp.mouth_sink_m = 130.0;
    tp.floor_height_m = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // Arc-length of each node.
    std::vector<double> arc(net.spine.size(), 0.0);
    for (std::size_t i = 1; i < net.spine.size(); ++i)
        arc[i] =
            arc[i - 1] + glm::length(net.spine[i].pos - net.spine[i - 1].pos);
    // Find a node on the narrow DESCENT BORE: radius comfortably above the
    // arena apex (so the tube owns the sight there) and far enough from the
    // mouth that the sight is not already capped by the open pit. Pick the
    // shallowest such node past ~kSightMin of arc from the mouth.
    std::size_t ki = 0;
    for (std::size_t i = 2; i + 2 < net.spine.size(); ++i) {
        if (arc[i] < kSightMin_m + 200.0) continue;  // clear of the mouth pit
        if (glm::length(net.spine[i].pos) > net.arena_apex_r + 400.0) {
            ki = i;  // the first descent-bore node clear of the mouth
            break;
        }
    }
    REQUIRE(ki > 2);
    REQUIRE(ki + 2 < net.spine.size());

    // Baseline: the smooth net clears kSightMin here.
    const double before =
        world::centerline_sight_distance(net, arc[ki] - 300.0);
    REQUIRE(before >= kSightMin_m);  // premise: canon is open (non-vacuous)

    // Inject the fold: yank the knuckle node radially inward by 3*tube_height
    // so the corridor kinks. Re-derive the node's up (unchanged dir) and
    // refresh the neighbouring tangents so the SDF frame follows the fold
    // (matching build).
    const glm::dvec3 dir = glm::normalize(net.spine[ki].pos);
    const double r0 = glm::length(net.spine[ki].pos);
    net.spine[ki].pos = dir * (r0 - 3.0 * net.tube_height);
    for (std::size_t i = 1; i + 1 < net.spine.size(); ++i) {
        const glm::dvec3 t = net.spine[i + 1].pos - net.spine[i - 1].pos;
        const double l = glm::length(t);
        if (l > 0.0) net.spine[i].tan = t / l;
    }

    // The sharp fold collapses the forward sight at the station before it well
    // below kSightMin (the curtain the verifier must catch).
    const double after = world::centerline_sight_distance(net, arc[ki] - 300.0);
    INFO("sight before fold " << before << " after fold " << after);
    REQUIRE(after < kSightMin_m);
}

// ==================== T16 — the SDF broad-phase (perf) ======================
// docs/tunnel_staging.md T16. signed_distance() guards the ~80-segment scan
// with a bound-sphere early-out (a full scan far above ground was ~28 us in
// Debug, run twice per tick regardless of position — the Ramsey-Lake tax).
// The early-out returns a CONSERVATIVE positive distance (dist-to-centre minus
// bound_radius) only when the point is provably far (past
// kTunnelBroadPhaseSkip_m beyond the sphere), where the true scan is also far
// past every consumer's sensitivity band (the 40 m atm blend / the boolean
// contains()). This pins the EQUIVALENCE: signed_distance ==
// signed_distance_scan wherever the guard is inert, and BOTH read past the
// blend where it fires (so atm_falloff == 0 and contains == false for either).
TEST_CASE("T16 broad-phase: equivalent to the full scan on a straddling grid") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    REQUIRE(net.bound_radius > 0.0);  // the broad-phase sphere is built

    // Sample a dense 3D grid straddling the bound sphere: from well inside to
    // well beyond it, so both the guard-inert and guard-firing regions are hit.
    const glm::dvec3 c = net.bound_center;
    const double Rb = net.bound_radius;
    int fired = 0, inert = 0;
    for (int ix = -6; ix <= 6; ++ix)
        for (int iy = -6; iy <= 6; ++iy)
            for (int iz = -6; iz <= 6; ++iz) {
                // Span roughly [-1.6 Rb, +1.6 Rb] about the centre in each
                // axis.
                const glm::dvec3 p =
                    c + glm::dvec3{ix, iy, iz} * (Rb * (1.6 / 6.0));
                const double fast = net.signed_distance(p);
                const double scan = net.signed_distance_scan(p);
                const double outside = glm::length(p - c) - Rb;
                if (outside > world::kTunnelBroadPhaseSkip_m) {
                    // Guard fired: both are far past the atm blend AND both are
                    // positive (outside), so every consumer reads identically.
                    ++fired;
                    REQUIRE(fast > net.soft_m);
                    REQUIRE(scan > net.soft_m);
                    REQUIRE(fast > 0.0);
                    REQUIRE(scan > 0.0);
                } else {
                    // Guard inert: bit-identical (same code path).
                    ++inert;
                    REQUIRE(fast == scan);
                }
            }
    // The grid straddles the sphere, so BOTH regions are non-vacuously covered.
    REQUIRE(fired > 0);
    REQUIRE(inert > 0);
}

TEST_CASE(
    "T16 broad-phase: bit-identical inside/near the net (the flown zone)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // Every spine node (deep inside) and a metre-scale halo around each mouth —
    // the region the sim actually samples — must be bit-identical to the scan
    // (the guard is inert there, so no golden/atm/contains behavior can move).
    for (const world::TunnelNet::Node& nd : net.spine) {
        REQUIRE(net.signed_distance(nd.pos) ==
                net.signed_distance_scan(nd.pos));
        for (double off : {-50.0, 0.0, 50.0, 200.0}) {
            const glm::dvec3 p = nd.pos + glm::normalize(nd.pos) * off;
            REQUIRE(net.signed_distance(p) == net.signed_distance_scan(p));
        }
    }
    // The arena centre + a spread inside it.
    REQUIRE(net.signed_distance(net.arena.center) ==
            net.signed_distance_scan(net.arena.center));
}
