// Section 7 prep — pin the shipped app tick (P1b,
// fable5_section6_fixreport.md).
//
// Before the app::instructor_tick extraction, main.cpp's crash predicate
// (altitude <= 0 -> respawn), its GROUNDED pairing + input neutralization, and
// its frame-carried camera call site executed under NO test: AT-13 verified the
// altitude crossing in a bare sim loop, and the poles test built its own
// aim_chase_camera call — neither routed through the app's wiring, so the named
// mutations (main.cpp `<= 0.0` -> `<= -50.0`; camera fed `local_up` instead of
// `aim.up()`) survived the whole suite. These legs drive app::tick / app::
// instructor_camera directly — the SAME functions main.cpp now calls.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>

#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/world.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

bool finite3(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool finite_state(const sim::SimState& s) {
    return finite3(s.position) && finite3(s.velocity) &&
           finite3(s.angular_vel) && std::isfinite(s.orientation.w);
}

// An airborne, flying LoopState (grounded=false) from a harness level state.
app::LoopState flying(const sim::SimState& s) {
    app::LoopState st;
    st.curr = s;
    st.prev = s;
    st.prev_up = sim::local_up(s.position);
    st.aim.reseed(s.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    return st;
}

app::TickInput instr_in(double throttle) {
    app::TickInput in;
    in.raw_mode = false;
    in.throttle = throttle;
    return in;
}

}  // namespace

// ===========================================================================
// Crash-reset — the app's OWN respawn wiring (not the bare-sim AT-13 mirror).
// Fly a low, straight-down dive into the ground NEAR THE -X ANTIPODE (max
// separation from the +X spawn pole, so the prev_up reseed on respawn is
// actually exercised — a +X-pole crash would make that line a no-op, S3's
// "bases must SEPARATE"). Pin the PREDICATE BOUNDARY, not the eventuality: the
// tick that respawns had altitude > 0 at its start (first crossing). A
// `<= -50.0` mutation still eventually respawns (a sustained dive crosses -50
// too) — but at alt_prev ~ -47, so the boundary CHECK fails (S4a: pin the
// predicate itself, not that a crash "eventually" happens).
// ===========================================================================
TEST_CASE("app::tick: crash-reset respawns on the first altitude<=0 crossing") {
    const glm::dvec3 up{-1.0, 0.0, 0.0},
        heading{0.0, 1.0, 0.0};  // -X antipode; nose +Y so the dead aim-forward
                                 // (frozen along heading through the radial
                                 // dive) SEPARATES from the +X-spawn's -Z nose.
                                 // A -Z heading would coincide with the spawn
                                 // nose and let a rebuild-up-only mutant (stale
                                 // forward, up:=local_up) pass the forward
                                 // check below — the P3c fwd/up checks must
                                 // BOTH separate (S3 "bases must SEPARATE").
    sim::SimState s = harness::level_state(kAp, 120.0, 5.0, up, heading);
    s.velocity =
        120.0 * (-up);  // straight down into the -X ground (radial;
                        // local_up stays -X so transport is identity
                        // and the dead aim-forward stays along heading)
    s.last_vhat = -up;
    app::LoopState loop = flying(s);

    bool respawned = false;
    double alt_at_death = -999.0;
    for (int i = 0; i < 200 && !respawned; ++i) {
        const double alt_prev = sim::altitude(loop.curr.position, kAp);
        const app::TickResult r =
            app::tick(loop, instr_in(0.3), kAp, kCp, nullptr);
        REQUIRE(finite_state(loop.curr));
        if (r.respawned) {
            respawned = true;
            alt_at_death = alt_prev;
        }
    }
    std::printf("[app-tick crash] respawned=%d  alt_at_death=%.4f m\n",
                respawned, alt_at_death);
    REQUIRE(respawned);
    CHECK(alt_at_death > 0.0);  // FIRST crossing (a <=-50 mutant dies at ~-47)
    CHECK(alt_at_death < 3.0);  // and it was just above the surface

    // Respawn state IS app::spawn_state (airborne ~2 km over +X), and prev was
    // paired to it so render::interpolate can't streak the dead life across a
    // frame; the next tick is GROUNDED.
    const sim::SimState spawn = app::spawn_state(kAp);
    CHECK(glm::length(loop.curr.position - spawn.position) < 1e-9);
    CHECK(glm::length(loop.prev.position - loop.curr.position) < 1e-9);
    CHECK(sim::altitude(loop.curr.position, kAp) ==
          Catch::Approx(2000.0).margin(1e-6));
    CHECK(loop.curr.throttle == 1.0);  // reborn at cruise power, not idle (F2)
    CHECK(loop.grounded);
    // prev_up reseeded to the spawn's local_up (so the next transport can't hit
    // transport_rotation's antiparallel assert — the guard L233 pinned).
    CHECK(glm::dot(loop.prev_up, sim::local_up(spawn.position)) > 1.0 - 1e-12);

    // P3c: the aim frame is reseeded IN the crash branch, so it is aim := nose
    // IMMEDIATELY on the respawning tick's return — BEFORE any next tick. This
    // is the state the render frame reads if the crash lands on the frame's
    // last tick; without the in-branch reseed the aim still holds the dead
    // life's frame (~-X, aimed astern of the +X spawn) and the camera pops for
    // one frame. This CHECK is what the GROUNDED-tick aim check below cannot
    // see: it runs a full tick first, which reseeds anyway (the bug's whole
    // point is the frame BETWEEN). Mutation: delete the crash-branch reseed and
    // this fails while everything downstream stays green.
    const glm::dvec3 spawn_nose =
        glm::normalize(spawn.orientation * glm::dvec3{0.0, 0.0, -1.0});
    std::printf("[app-tick P3c] fwd.nose=%.9f  up.local_up=%.9f\n",
                glm::dot(loop.aim.forward(), spawn_nose),
                glm::dot(loop.aim.up(), sim::local_up(spawn.position)));
    CHECK(glm::dot(loop.aim.forward(), spawn_nose) > 1.0 - 1e-9);
    CHECK(glm::dot(loop.aim.up(), sim::local_up(spawn.position)) > 1.0 - 1e-9);

    // The GROUNDED respawn tick pairs control::reset() with aim := nose and
    // zeroes emitted Inputs — EVEN with a hard override held, GROUNDED wins
    // (the fresh airframe cannot be steered by the dead life's stick). F6/F2.
    app::TickInput held = instr_in(0.7);
    held.override_mask[0] = true;
    held.override_sign[0] =
        +1.0;  // full pitch-up demand, ignored while grounded
    const app::TickResult g = app::tick(loop, held, kAp, kCp, nullptr);
    CHECK(g.inputs.pitch == 0.0f);
    CHECK(g.inputs.yaw == 0.0f);
    CHECK(g.inputs.roll == 0.0f);
    CHECK(loop.internal.integ == glm::dvec3{0.0});  // internal reset
    const glm::dvec3 nose = spawn.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(glm::dot(loop.aim.forward(), glm::normalize(nose)) >
          1.0 - 1e-9);  // aim := nose
    // The reseed seeds up from LOCAL_UP (§9.2 "camera-up STARTS at local_up"),
    // NOT the dead life's carried up (~ -X after the antipode dive) — a
    // reseed->snap mutation in the pairing would spawn an inverted camera
    // (F-D).
    CHECK(glm::dot(loop.aim.up(), sim::local_up(spawn.position)) > 1.0 - 1e-9);
    CHECK(loop.grounded == false);  // grounded consumed; next tick flies
}

// The crash predicate is MODE-COMMON (it sits outside main.cpp's if(raw_mode),
// so it fires in RAW mode too — F1). Extracting only "the instructor branch"
// would have forked an untested raw-side copy of `altitude <= 0`; app::tick
// runs the ONE predicate for both modes. Prove the raw path reaches it.
TEST_CASE("app::tick: raw mode respawns on the same crash predicate") {
    const glm::dvec3 up{-1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s = harness::level_state(kAp, 120.0, 5.0, up, heading);
    s.velocity = 120.0 * (-up);  // straight down
    s.last_vhat = -up;
    app::LoopState loop = flying(s);

    app::TickInput raw;
    raw.raw_mode = true;
    raw.raw_in.throttle = 0.3f;  // hands off the stick; just fall

    bool respawned = false;
    for (int i = 0; i < 200 && !respawned; ++i) {
        const app::TickResult r = app::tick(loop, raw, kAp, kCp, nullptr);
        REQUIRE(finite_state(loop.curr));
        respawned = r.respawned;
    }
    REQUIRE(respawned);
    CHECK(glm::length(loop.curr.position - app::spawn_state(kAp).position) <
          1e-9);  // same airborne respawn as the instructor path
    CHECK(loop.grounded);
}

// Focus loss (F10) uses snap_forward_to_nose (aim := nose, KEEP carried up),
// NOT reseed (which would re-seed up from local_up and level the camera roll —
// the S5 holonomy-preservation lesson). Pin the choice at THIS call site: with
// the carried up rolled off local_up, focus loss points the aim at the nose but
// leaves the roll intact; a reseed would snap up back onto local_up.
TEST_CASE("app::instructor_focus_loss: aim := nose but keeps the carried up") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    app::LoopState loop = flying(s);
    // Roll the aim 40 deg off local_up and yaw it well off the nose.
    loop.aim.q = glm::normalize(
        loop.aim.q * glm::angleAxis(rad(40.0), glm::dvec3{0, 0, -1}));
    loop.aim.apply_mouse(200.0, 0.0, rad(0.15));

    app::instructor_focus_loss(loop);

    const glm::dvec3 nose =
        glm::normalize(s.orientation * glm::dvec3{0, 0, -1});
    const glm::dvec3 lu = sim::local_up(s.position);
    std::printf("[app-tick focus] fwd.nose=%.7f  up.local_up=%.4f\n",
                glm::dot(loop.aim.forward(), nose),
                glm::dot(loop.aim.up(), lu));
    CHECK(glm::dot(loop.aim.forward(), nose) > 1.0 - 1e-9);  // aim := nose
    CHECK(glm::dot(loop.aim.up(), lu) <
          0.9);  // roll KEPT (reseed would give ~1)
}

// ===========================================================================
// Camera call site — app::instructor_camera feeds the caller's CARRIED aim-up
// (S7-cam3: cam_up = loop.aim.up()), forwarding it faithfully to
// aim_chase_camera, NOT rebuilding it from local_up or the airframe internally.
//
// Faithful forwarding is subtler than it looks: aim_chase_camera
// re-orthogonalizes whatever up it is handed against the view direction, so a
// mutant that rebuilt from local_up returns local_up PROJECTED onto the plane
// perpendicular to forward. If the passed cam_up is coplanar with {forward,
// local_up} — which a LEVEL cam_up keeps — that projection REPRODUCES it and
// the mutation is invisible. The discriminator is ROLL: leave forward level (⟂
// local_up) and pass a cam_up rolled OFF local_up about that forward. Then
// pose.up == the passed cam_up for the honest forward, == local_up for a
// local_up-rebuild mutant — a full `1 - cos(45°)` apart.
// ===========================================================================
TEST_CASE("app::instructor_camera: forwards the carried aim-up") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    const glm::dvec3 lu = sim::local_up(s.position);
    const glm::dvec3 nose =
        glm::normalize(s.orientation * glm::dvec3{0.0, 0.0, -1.0});
    REQUIRE(std::abs(glm::dot(nose, lu)) < 1e-9);  // level nose (⟂ local_up)

    // A cam_up rolled 45 deg off local_up about the (level) view forward: still
    // ⟂ the view, but distinct from local_up so a rebuild mutant separates.
    const glm::dvec3 cam_up =
        glm::normalize(glm::angleAxis(rad(45.0), nose) * lu);
    REQUIRE(glm::dot(cam_up, lu) < 0.75);  // genuinely rolled off level

    const render::ChaseParams chase;
    const render::CameraOrbit orbit;
    // Pass the nose as the (S7-cam) camera-forward: forward ⟂ cam_up, so
    // re-orthogonalizing leaves cam_up untouched — the pose up must be the
    // passed cam_up, not a local_up (or airframe) rebuild.
    const render::CameraPose pose =
        app::instructor_camera(s, nose, cam_up, kAp, chase, orbit);
    std::printf("[app-tick cam] pose.up.cam_up=%.7f  pose.up.local_up=%.4f\n",
                glm::dot(pose.up, cam_up), glm::dot(pose.up, lu));
    CHECK(glm::dot(pose.up, cam_up) > 1.0 - 1e-9);  // IS the passed cam_up
    CHECK(glm::dot(pose.up, lu) <
          0.8);  // NOT a local_up rebuild (mutation dies)
}

// ===========================================================================
// T9a CAVECAM — the shared inside_tunnel() predicate + the forwarding of the
// underground flag through app::instructor_camera. A DEFAULTED new param ships
// DEAD without a leg that drives it live on the shipped path (the
// moved-consumer house lesson): the forwarding leg flies a deep-underground
// nose-up state through app::instructor_camera(..., underground=true) and
// asserts the eye is NOT hoisted to bare radius (== the raw offset). The helper
// leg pins the predicate: null env => false, inside-egg point => true, surface
// point => false. One predicate feeds BOTH the crash yield and the camera flag.
// ===========================================================================
TEST_CASE("app::inside_tunnel: predicate is true only inside the net") {
    // A bare-sphere tunnel net (null ground): the hollow-core cavern shell sits
    // well below the surface (T10). A point in the middle of the shell is
    // inside.
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 20.0;
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
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    const world::TunnelNet net = world::build_tunnel_net(tp, nullptr);

    sim::Environment env;
    env.tunnels = &net;

    // A point in the middle of the flyable arena (deep below the surface): the
    // arena center itself (guaranteed inside the arena ellipsoid).
    const glm::dvec3 shell_pt = net.arena.center;
    const glm::dvec3 surface_pt = glm::normalize(shell_pt) * (kAp.R + 2000.0);

    // null env => false (the surface-flight clamp / altitude<=0 rule).
    REQUIRE(app::inside_tunnel(nullptr, shell_pt) == false);
    // env with null tunnels => false.
    sim::Environment env_no_tun;
    REQUIRE(app::inside_tunnel(&env_no_tun, shell_pt) == false);
    // inside the arena => true.
    REQUIRE(net.contains(shell_pt));  // premise
    REQUIRE(app::inside_tunnel(&env, shell_pt) == true);
    // a point 2 km above the surface => false.
    REQUIRE(!net.contains(surface_pt));  // premise
    REQUIRE(app::inside_tunnel(&env, surface_pt) == false);
    // the CORE interior (below core_r) is OUTSIDE the net (a crash wall).
    const glm::dvec3 core_pt =
        glm::normalize(shell_pt) * (tp.cavern_core_m * 0.5);
    REQUIRE(!net.contains(core_pt));
    REQUIRE(app::inside_tunnel(&env, core_pt) == false);
}

TEST_CASE("app::instructor_camera: forwards the underground CAVECAM flag") {
    // Deep-underground nose-UP state where the surface clamp would hoist the
    // eye to bare radius. Position ~ R-2500 (legally below the surface, in the
    // tunnel); nose (aim forward) points radially up so the chase eye lands
    // radially below the target.
    const glm::dvec3 pos_dir = glm::normalize(glm::dvec3{0.3, 1.0, -0.4});
    sim::SimState s;
    s.position = pos_dir * (kAp.R - 2500.0);
    const glm::dvec3 lu = sim::local_up(s.position);
    const glm::dvec3 fwd = lu;  // nose radially up
    // aim_up = a tangent (⟂ fwd) so the frame is well-formed.
    const glm::dvec3 tang =
        glm::normalize(glm::cross(fwd, glm::dvec3{0.0, 0.0, 1.0}));
    const glm::dvec3 aim_up =
        glm::length(tang) > 0.1
            ? tang
            : glm::normalize(glm::cross(fwd, glm::dvec3{1.0, 0.0, 0.0}));
    s.orientation = glm::normalize(glm::quat_cast(
        glm::dmat3{glm::normalize(glm::cross(aim_up, -fwd)), aim_up, -fwd}));

    const render::ChaseParams chase;
    const render::CameraOrbit orbit;
    // underground=true: the eye must NOT be hoisted — it stays at the raw
    // offset (below the target). underground=false (default): the clamp hoists
    // it to bare radius. Mutation (m1: hardcode false in instructor_camera) =>
    // the underground pose gets hoisted, failing the < |position| check.
    const render::CameraPose ug =
        app::instructor_camera(s, fwd, aim_up, kAp, chase, orbit,
                               /*underground=*/true);
    const render::CameraPose sf =
        app::instructor_camera(s, fwd, aim_up, kAp, chase, orbit,
                               /*underground=*/false);
    std::printf("[app-tick cavecam] |ug.eye|=%.1f |sf.eye|=%.1f |pos|=%.1f\n",
                glm::length(ug.eye), glm::length(sf.eye),
                glm::length(s.position));
    // underground: eye on the plane (below the target, not hoisted).
    CHECK(glm::length(ug.eye) < glm::length(s.position));
    // surface-flight arm: eye hoisted to bare radius (the bug this fixes).
    CHECK(glm::length(sf.eye) ==
          Catch::Approx(kAp.R + chase.min_eye_altitude).epsilon(1e-9));
    // The forwarded flag genuinely changed the pose.
    CHECK(glm::length(ug.eye - sf.eye) > 100.0);
}

// ===========================================================================
// S7-cam3 "the mouse never inverts" (the executable form of Chad's complaint
// "after a split-S the mouse goes all opposite"). With camera-up = the CARRIED
// aim-frame up, a MOUSE-UP nudge moves the reticle UP ON SCREEN at EVERY
// attitude — including after a loop has carried the aim frame past inverted.
// The reticle is project_dir(aim.forward()) through the camera basis; camera-up
// = aim.up() keeps screen-up == the frame's own up, so mouse-up (rotate the aim
// toward its own up, about its own right) always projects to +screen_y.
//
// The frame is rolled about its OWN forward by roll_deg (forward stays on the
// horizon, up/right roll off level) — exactly what a loop's carry + holonomy
// leave: the aim recovered to horizontal but "up-is-down". MUTATION = the
// S7-cam2 horizon-lock (camera-up = local_up): past inverted (aim.up() ~
// -local_up) the SAME mouse-up nudge projects to -screen_y — the flip. The test
// asserts the carried camera never flips AND that the horizon-lock choice DOES
// (so the contrast, not just a happy pass, is pinned).
// ===========================================================================
TEST_CASE("app::tick: mouse-up stays screen-up at any attitude (no invert)") {
    const double fovy = rad(60.0), aspect = 16.0 / 9.0;
    const double sens = 0.01;                  // ~0.57 deg mouse-up nudge
    const glm::dvec3 local_up{0.0, 1.0, 0.0};  // world up at this sample

    for (double roll_deg : {0.0, 60.0, 120.0, 180.0, 240.0, 300.0}) {
        // Default frame is level (forward -Z, up +Y == local_up); roll the
        // whole frame about its OWN forward so up/right roll off level but
        // forward stays on the horizon (the post-loop "up-is-down" attitude).
        input::AimFrame aim;
        aim.q = glm::normalize(
            aim.q * glm::angleAxis(rad(roll_deg), glm::dvec3{0.0, 0.0, -1.0}));
        const glm::dvec3 f = aim.forward();

        // Mouse UP (dy < 0 per apply_mouse) on a copy: rotates the aim toward
        // its own up about its own right.
        input::AimFrame nudged = aim;
        nudged.apply_mouse(0.0, -1.0, sens);

        // SHIPPED camera-up = the carried aim-up. Camera looks along the aim
        // (centered), so the reticle sign is the pure up-axis sign.
        const render::ScreenPoint c0 =
            render::project_dir(f, f, aim.up(), fovy, aspect);
        const render::ScreenPoint c1 =
            render::project_dir(nudged.forward(), f, aim.up(), fovy, aspect);
        // Horizon-lock MUTATION: camera-up = local_up.
        const render::ScreenPoint m1 =
            render::project_dir(nudged.forward(), f, local_up, fovy, aspect);

        std::printf(
            "[no-invert] roll=%3.0f  carried dy=%+.4f  horizon-lock dy=%+.4f\n",
            roll_deg, c1.y - c0.y, m1.y - c0.y);

        REQUIRE(c1.in_front);
        CHECK(c1.y > c0.y + 1e-6);  // carried camera: mouse-up -> UP, always

        // Past inverted, the horizon-lock choice FLIPS it (the bug we removed).
        if (roll_deg > 90.0 && roll_deg < 270.0)
            CHECK(m1.y <
                  c0.y - 1e-6);  // horizon-lock: mouse-up -> DOWN (invert)
    }
}

// ===========================================================================
// The CQ2 mouse->aim gate on the SHIPPED path. Moving the apply inside
// app::tick is only worth it if a leg exercises `fs.mouse_aim_live &&
// !grounded` (ClosedLoop never routes a mouse delta, so this was app-only,
// untested). Same airborne start, one tick, big vs zero mouse dx: the aim MOVES
// when live, DROPS when grounded (spawn tick) or freelook-held (delta goes to
// the orbit).
// ===========================================================================
namespace {
// Resulting aim-forward after one tick from `s`, with a mouse dx, under the
// given (grounded, freelook) gates.
glm::dvec3 tick_aim_forward(const sim::SimState& s, double aim_dx,
                            bool grounded, bool freelook) {
    app::LoopState loop = flying(s);
    loop.grounded = grounded;
    app::TickInput in = instr_in(0.7);
    in.freelook_held = freelook;
    in.aim_dx = aim_dx;
    app::tick(loop, in, kAp, kCp, nullptr);
    return loop.aim.forward();
}
}  // namespace

TEST_CASE(
    "app::tick: mouse->aim is live only when ungrounded and not freelook") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    const double D = 200.0;  // a big mouse dx (px)

    const glm::dvec3 live_move = tick_aim_forward(s, D, false, false);
    const glm::dvec3 live_still = tick_aim_forward(s, 0.0, false, false);
    const glm::dvec3 gnd_move = tick_aim_forward(s, D, true, false);
    const glm::dvec3 gnd_still = tick_aim_forward(s, 0.0, true, false);
    const glm::dvec3 fl_move = tick_aim_forward(s, D, false, true);
    const glm::dvec3 fl_still = tick_aim_forward(s, 0.0, false, true);

    const double live_delta = glm::length(live_move - live_still);
    const double gnd_delta = glm::length(gnd_move - gnd_still);
    const double fl_delta = glm::length(fl_move - fl_still);
    std::printf("[app-tick cq2] live=%.4f  grounded=%.6f  freelook=%.6f\n",
                live_delta, gnd_delta, fl_delta);
    CHECK(live_delta > 0.1);   // ungrounded MOUSE mode: the aim moved
    CHECK(gnd_delta < 1e-12);  // spawn/reset tick: the delta was dropped
    CHECK(fl_delta < 1e-12);   // freelook: mouse_aim_live is false -> dropped
}

// ===========================================================================
// CQ2 ease-back suspension on the SHIPPED path (SPEC §16 CQ2 / §9.5). After a
// freelook release the camera eases back to behind the nose; for that window
// mouse->aim is SUSPENDED so no easing-frame (smoothed camera) basis feeds the
// aim (RA9, banned). The gate is `fs.mouse_aim_live` — and the case above tests
// only where freelook_held and mouse_aim_live COINCIDE, so a
// `mouse_aim_live -> !freelook_held` mutation survives it while killing the CQ2
// ruling entirely. Drive an actual release edge through app::tick: on the tick
// AFTER release, freelook is no longer held but the aim delta must still be
// dropped ("pin the CONSUMER", the 4d-audit lesson).
// ===========================================================================
TEST_CASE("app::tick: mouse->aim stays suspended across the CQ2 ease-back") {
    // The window is RETIRED at the flown value (pilot ruling 2026-08-06:
    // easeback_time = 0), but the MECHANISM stays wired — so this leg pins its
    // own nonzero window rather than reading the shipped 0, which would make it
    // vacuous (the fixture-no-op class).
    control::ControllerParams cp = kCp;
    cp.freelook_easeback_time = 0.30;
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    const double D = 200.0;

    auto release_then_move = [&](double aim_dx) {
        app::LoopState loop = flying(s);
        app::TickInput hold = instr_in(0.7);
        hold.freelook_held = true;
        app::tick(loop, hold, kAp, cp, nullptr);  // hold freelook one tick
        app::TickInput rel = instr_in(0.7);
        rel.freelook_held = false;  // RELEASE this tick: ease-back armed
        rel.aim_dx = aim_dx;        // mouse delta during the suspension window
        app::tick(loop, rel, kAp, cp, nullptr);
        return loop.aim.forward();
    };
    const double delta =
        glm::length(release_then_move(D) - release_then_move(0.0));
    std::printf("[app-tick easeback] release-window delta=%.6e\n", delta);
    CHECK(delta < 1e-12);  // suspended: the release-window delta was dropped
}

// ===========================================================================
// The freelook aim-snap wiring on the SHIPPED path (SPEC §9.5 rules 2/3). First
// override activity of a hold snaps aim := nose at that instant (the parked
// cursor you can't see becoming a surprise). app::tick consumes
// Freelook::Step's snap_to_nose — but nothing drove that true through app::tick
// before, so deleting the consumer survived the suite. Hold freelook, point the
// aim well off the nose, then seize an override: the aim must snap back to the
// nose.
// ===========================================================================
TEST_CASE("app::tick: first override during freelook snaps aim to the nose") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    app::LoopState loop = flying(s);
    loop.aim.apply_mouse(500.0, 0.0,
                         rad(0.15));  // yaw the aim ~75 deg off nose
    const glm::dvec3 nose =
        glm::normalize(s.orientation * glm::dvec3{0, 0, -1});
    REQUIRE(glm::dot(loop.aim.forward(), nose) < 0.5);  // premise: aim is off

    app::TickInput in = instr_in(0.7);
    in.freelook_held = true;
    in.override_mask[0] = true;  // seize the pitch axis mid-freelook (rule 2)
    in.override_sign[0] = +1.0;
    app::tick(loop, in, kAp, kCp, nullptr);

    std::printf("[app-tick snap] fwd.nose=%.7f\n",
                glm::dot(loop.aim.forward(), nose));
    CHECK(glm::dot(loop.aim.forward(), nose) > 1.0 - 1e-9);  // snapped to nose
}

// ===========================================================================
// Mirror equivalence (F7) — main.cpp claims to mirror harness::ClosedLoop::tick
// "EXACTLY", but no ctest runs the app binary, so that was verified only by
// diff review. Drive app::tick and ClosedLoop from the SAME trim start with no
// mouse/freelook/override for 5 s and assert the trajectories track: the
// AimFrame forward vs the vector transport_aim agree to 1e-12 (AT-14c), so any
// transcription slip in the extracted tick (a dropped feedforward, a flipped
// sign, a mis-ordered stage) diverges the states by orders of magnitude here.
// ===========================================================================
TEST_CASE("app::tick mirrors harness::ClosedLoop::tick over a level cruise") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    app::LoopState loop = flying(s0);
    harness::ClosedLoop cl(s0, s0.orientation * glm::dvec3{0.0, 0.0, -1.0});

    for (int i = 0; i < 600; ++i) {  // 5 s
        app::tick(loop, instr_in(thr), kAp, kCp, nullptr);
        cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(loop.curr));
    }
    const double pos_err = glm::length(loop.curr.position - cl.state.position);
    const double orient_dot =
        std::abs(glm::dot(loop.curr.orientation, cl.state.orientation));
    // max_digits10 (S1: a %.6e instrument is quantization-blind — it read a
    // wide-band pass as bit-identical). Empirically this pair is EXACTLY
    // bit-identical over 5 s (pos_err == 0, orient_dot == 1): the AimFrame
    // quaternion and the vector transport_aim produce the identical fp forward
    // along this planar great-circle cruise, so the emitted Inputs match to the
    // bit. The bound is tight (not 1e-3) so a real transcription slip (dropped
    // feedforward -> metres, flipped sign -> huge, mis-ordered stage) can't
    // hide in a slack hide-band; it stays a hair above 0 rather than exact-==
    // only to not be brittle to a future stage that legitimately carries the
    // ~1e-12/tick quat-vs-vector difference (AT-14c) into position.
    std::printf("[app-tick mirror] pos_err=%.17e m  orient_dot=%.16f\n",
                pos_err, orient_dot);
    CHECK(pos_err < 1e-6);            // bit-identical path (observed 0.0)
    CHECK(orient_dot > 1.0 - 1e-12);  // and the identical attitude
}

// ===========================================================================
// Coverage round 3 (the AT-9 Fable consult re-attacked the P1b extraction and
// found two SPEC-mandated consumers the move onto the shipped path left with NO
// mutation coverage — the same class as the CQ2/freelook pair round 2
// repaired).
//
// P1-1: keyboard override -> control::step. app::tick copies in.override_mask/
// sign into the control::Input (instructor_tick.h:137-140). Delete that copy
// and the keyboard override goes DEAD in the live app while the whole suite
// stays green: the snap leg above reads `any_ovr` from in.override_mask
// (:122-123), NOT ci, so it can't see it; the crash leg's held override is
// grounded- swallowed (asserts zeros either way); the mirror leg drives none.
// Under override the held axis emits sign*ovr_ramp (controller.cpp:356),
// bypassing the rate loop; without the copy it falls to the rate-PI baseline
// (~0 at wings- level nose-aim). Drive the ROLL axis — curvature feedforward is
// on PITCH, so roll's unforced baseline is ~0, a clean separator — and assert
// the emitted roll tracks the override sign, not the baseline.
// ===========================================================================
TEST_CASE(
    "app::tick: keyboard override reaches control::step (roll passthrough)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);

    auto emit_roll = [&](bool held, double sign) {
        app::LoopState loop = flying(s);
        app::TickInput in = instr_in(0.7);
        in.override_mask[2] = held;  // roll axis (index 2)
        in.override_sign[2] = sign;
        float roll = 0.0f;
        for (int i = 0; i < 12; ++i)  // ~0.1 s: the 80 ms engage ramp saturates
            roll = app::tick(loop, in, kAp, kCp, nullptr).inputs.roll;
        return roll;
    };

    const float base = emit_roll(false, 0.0);  // no override: rate-PI baseline
    const float pos = emit_roll(true, +1.0);
    const float neg = emit_roll(true, -1.0);
    std::printf("[app-tick ovr] base=%.6f  +ovr=%.6f  -ovr=%.6f\n", base, pos,
                neg);
    CHECK(std::abs(base) < 0.05f);  // wings-level nose-aim: roll ~0 unforced
    CHECK(pos > 0.5f);  // +override drove the axis (sign*ramp, ramp -> 1)
    CHECK(neg <
          -0.5f);  // sign honoured (delete the copy -> both collapse to base)
}

// ===========================================================================
// P1-2: fl.reset() in the GROUNDED pairing (instructor_tick.h:113) wipes the
// freelook latches every grounded tick so a respawn "must never inherit the
// previous life's freelook state" (aim_state.h:96-100). The uncovered
// consequence: a crash landing DURING the CQ2 ease-back window would carry the
// dead life's easeback countdown (mouse_aim_live=false, ~300 ms) across
// respawn, suspending mouse->aim well into the new life. No prior test
// crashed/regrounded with ease-back armed, so deleting fl.reset() passed the
// suite. Arm the window (hold then release freelook), enter a GROUNDED pairing,
// consume the grounded tick, then offer a mouse delta on the first flight tick:
// it must be LIVE (the ease-back was cleared). Mutant: still counting down ->
// delta dropped.
// ===========================================================================
TEST_CASE("app::tick: GROUNDED clears a pending freelook ease-back") {
    // Own window: the shipped value is the retired 0 (pilot ruling
    // 2026-08-06), which would arm nothing for the GROUNDED reset to clear.
    control::ControllerParams cp = kCp;
    cp.freelook_easeback_time = 0.30;
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    const double D = 200.0;

    // D vs 0 on the first post-grounded flight tick isolates the mouse effect;
    // the aim moves iff mouse->aim went live (i.e. the ease-back was reset).
    auto post_grounded_aim = [&](double aim_dx) {
        app::LoopState loop = flying(s);
        app::TickInput hold = instr_in(0.7);
        hold.freelook_held = true;
        app::tick(loop, hold, kAp, cp, nullptr);  // freelook held one tick
        app::TickInput rel = instr_in(0.7);
        rel.freelook_held = false;
        app::tick(loop, rel, kAp, cp,
                  nullptr);    // release: ease-back armed (~300 ms)
        loop.grounded = true;  // a respawn/toggle GROUNDED pairing
        app::tick(loop, instr_in(0.7), kAp, cp,
                  nullptr);  // grounded tick: fl.reset()
        app::TickInput fly = instr_in(0.7);
        fly.aim_dx = aim_dx;
        app::tick(loop, fly, kAp, cp,
                  nullptr);  // first flight tick: mouse offered
        return loop.aim.forward();
    };
    const double delta =
        glm::length(post_grounded_aim(D) - post_grounded_aim(0.0));
    std::printf("[app-tick gnd-easeback] delta=%.6f\n", delta);
    CHECK(delta >
          0.1);  // ease-back cleared -> mouse live (mutant: ~0, suspended)
}

// ===========================================================================
// Mouse-only LOOP and SPLIT-S follow through (§7, Chad 2026-07-06). The pilot
// flies the instructor by MOUSE: a big vertical deflection (up = loop, down =
// split-S) must carry the aim ALL THE WAY OVER THE TOP / UNDER, and the plane
// must follow through inverted and capture. This drives the SHIPPED app::tick
// pipeline (aim frame + apply_mouse + control::step + sim::step) with a
// scripted steady vertical mouse, exactly as a mouse pilot does.
//
// This PINS the removal of the aim auto-leveling (level_up_to, S7-cam3): that
// re-leveled the mouse basis to the horizon each tick, which FLIPPED the mouse
// pitch axis at the zenith / across the rear hemisphere and PINNED the aim at
// vertical (mouse-only loops were impossible). MUTATION — re-add
// `if (!st.grounded) st.aim.level_up_to(up);` to app::tick: the aim sticks at
// ~90 deg, aim_min_behind never goes negative, and BOTH legs fail. So the legs
// have teeth against any future re-introduction of an auto-leveling of the aim
// basis. (There is no ctest of level_up_to otherwise — the feature shipped
// unpinned, §6 red-team.)
// ===========================================================================
namespace {
struct MouseVert {
    double aim_min_behind = 1e9;  // min dot(aim.forward, nose0): <0 = aim went
                                  //   PAST vertical into the rear hemisphere
    double min_cpt = 1e9;         // min cos_phi_theta: <0 = plane went inverted
    double final_capture = 0.0;   // final dot(nose, aim): ~1 = flew to the aim
    bool righting_seen = false;   // sticky: first MB-right fire ends the guard
    double max_abs_phi = 0.0;     // max |bank| (wings' tilt off horizontal): ~0
                               //   through a CLEAN loop (wings level even when
                               //   inverted), large if it CORKSCREWS
};
// Deflect-and-hold: sweep a steady VERTICAL mouse (sgn<0 up = loop, sgn>0 down
// = split-S) at rate_deg_s until the aim has been rotated ~deflect_deg from the
// nose, then HOLD (zero mouse) and let the plane fly to the parked aim.
// rate_deg_s defaults to a FAST 180; a SLOW ~35 is the realistic manual sweep
// that let the OLD screen-relative mouse's camera basis catch up and stall the
// aim at vertical (S7-raw removed that coupling — the RAW carried frame loops
// over cleanly at any sweep rate).
MouseVert drive_mouse_vertical(const sim::SimState& s0, int sgn,
                               double deflect_deg, int ticks,
                               double rate_deg_s = 180.0) {
    app::LoopState st = flying(s0);
    st.grounded = true;  // one seed tick (mouse dropped, aim := nose)
    const glm::dvec3 nose0 = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double rate = rad(rate_deg_s);  // aim sweep rate while pushing
    const double dy = sgn * rate * kAp.sim_dt / kCp.aim_sensitivity;
    const int push = static_cast<int>(rad(deflect_deg) / (rate * kAp.sim_dt));
    // The RAW mouse (S7-raw) ignores the camera basis, but drive the real
    // MiniCamera anyway so in.cam_fwd/cam_up carry the shipped
    // (vestigial-to-the- mouse) values — the loop proof is the shipped
    // app::tick path.
    harness::MiniCamera cam;
    cam.seed(st.curr);
    MouseVert m;
    for (int i = 0; i < ticks; ++i) {
        app::TickInput in = instr_in(1.0);
        in.aim_dy = (i < push) ? dy : 0.0;
        in.cam_fwd = cam.cam_fwd;  // previous tick's basis (one-tick lag)
        in.cam_up = cam.cam_up;
        const app::TickResult r = app::tick(st, in, kAp, kCp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), kCp, kAp.sim_dt);
        REQUIRE(finite_state(st.curr));
        m.aim_min_behind =
            std::min(m.aim_min_behind, glm::dot(st.aim.forward(), nose0));
        m.min_cpt = std::min(m.min_cpt, r.telem.extracted.cos_phi_theta);
        // The corkscrew guard reads |phi| only until the FIRST MB-right
        // righting fire (STICKY: the righting episode includes the
        // post-latch auto-level handoff tail — ~87 deg -> level with the
        // latch already released — which would otherwise read as a 69 deg
        // "corkscrew"): a maneuver that ENDS captured-inverted now
        // slow-rights after [auto_level] inverted_delay of rest (Chad
        // 2026-07-07) — the whole post-capture roll is RULED behavior, not
        // a corkscrew. The mid-loop corkscrew mutant (un-gated wings-hold)
        // rolls DURING the loop, BEFORE any righting fires, so the guard's
        // mutation coverage is intact.
        m.righting_seen = m.righting_seen || r.telem.righting;
        if (!m.righting_seen) {
            m.max_abs_phi =
                std::max(m.max_abs_phi, std::abs(r.telem.extracted.phi));
        }
    }
    const glm::dvec3 nose_f = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    m.final_capture = glm::dot(nose_f, st.aim.forward());
    return m;
}
}  // namespace

TEST_CASE("app::tick: mouse-UP deflection loops all the way over the top") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 220.0, 6000.0, up, heading, nullptr);
    const MouseVert m = drive_mouse_vertical(s0, /*sgn=*/-1, 160.0, 1400);
    std::printf(
        "[mouse loop] aim_min_behind=%.2f min_cpt=%.2f capture=%.2f "
        "max_bank_deg=%.1f\n",
        m.aim_min_behind, m.min_cpt, m.final_capture, deg(m.max_abs_phi));
    CHECK(m.aim_min_behind < -0.5);  // aim swept PAST vertical (mutant: ~0)
    CHECK(m.min_cpt < -0.5);         // plane went over the top / inverted
    CHECK(m.final_capture > 0.8);    // flew all the way to the aim
    // A pure loop keeps the WINGS LEVEL (zero roll — Chad's "no roll in a
    // loop"). This USED to corkscrew ~67 deg (the inverted wings-hold righting
    // the plane mid-loop); S7-loop-invert gated that off, so even a FAST sweep
    // now loops clean. MUTATION: remove the cos_phi_theta wings-level fade
    // (restore F2's un-gated wings-hold) -> the loop corkscrews and this fails.
    CHECK(deg(m.max_abs_phi) <
          15.0);  // clean loop, no corkscrew (S7-loop-invert)
}

TEST_CASE("app::tick: mouse-DOWN deflection split-S all the way through") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 220.0, 6000.0, up, heading, nullptr);
    const MouseVert m = drive_mouse_vertical(s0, /*sgn=*/+1, 160.0, 1400);
    std::printf(
        "[mouse split-s] aim_min_behind=%.2f min_cpt=%.2f capture=%.2f\n",
        m.aim_min_behind, m.min_cpt, m.final_capture);
    CHECK(m.aim_min_behind <
          -0.5);                   // aim swept PAST vertical DOWN (mutant: ~0)
    CHECK(m.min_cpt < -0.5);       // plane rolled through inverted
    CHECK(m.final_capture > 0.8);  // pulled through to the aim
}

// RAW mouse loops over at a SLOW sweep (S7-raw). A slow, deliberate vertical
// mouse (~35 deg/s, like a real hand) used to STALL the aim at ~vertical under
// the F1 screen-relative basis (the lagging camera caught up and its screen-
// right rotated under the input). The RAW carried §9.1 frame references nothing
// external, so a steady mouse-up sweeps the aim ALL the way over and around,
// the plane loops fully inverted, and — because no camera basis injects a
// lateral component — the loop stays CLEAN (wings ~level, no corkscrew) at this
// rate. MUTATION: restore the screen-relative
// apply_mouse(dx,dy,sens,cam_fwd,cam_up) in app::tick -> the slow aim
// stalls/retreats, never fully inverts -> the min_cpt / aim_min_behind CHECKs
// fail. (Both the fast leg above and this slow leg now loop CLEAN — 0 deg bank
// — since S7-loop-invert gated off the inverted wings-hold that used to
// corkscrew.)
TEST_CASE("app::tick: RAW mouse loops cleanly over the top at a slow sweep") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 220.0, 6000.0, up, heading, nullptr);
    const MouseVert m =
        drive_mouse_vertical(s0, /*sgn=*/-1, 300.0, 3000, /*rate_deg_s=*/35.0);
    std::printf(
        "[mouse loop SLOW-raw] aim_min_behind=%.2f min_cpt=%.2f "
        "max_bank_deg=%.1f\n",
        m.aim_min_behind, m.min_cpt, deg(m.max_abs_phi));
    CHECK(m.aim_min_behind < -0.7);  // aim swept around, well past vertical
    CHECK(m.min_cpt < -0.7);         // plane looped FULLY inverted (clean loop)
    CHECK(deg(m.max_abs_phi) <
          15.0);  // wings ~level — NO corkscrew at this rate
}

// ===========================================================================
// RAW mouse basis (S7-raw, §7 2026-07-06). apply_mouse rotates the aim about
// the aim frame's OWN carried up/right — the F1 screen-relative camera coupling
// is REMOVED (it was a smoothed basis feeding mouse->aim, RA9, and stalled a
// slow vertical mouse at the zenith). The prior "screen-relative climbs UP when
// the frame is inverted" test pinned that removed behavior and is deleted with
// it; the carried-frame's post-maneuver up-drift ("up-is-down") is now owned by
// the separate gentle roll-to-local_up dial, not the mouse basis. The
// mouse-only loop/split-S over-the-top legs above (drive_mouse_vertical)
// already exercise the raw carried-frame sweep.
// ===========================================================================

// ===========================================================================
// S7-loop-invert — an INVERTED plane at REST STAYS inverted (§7 Chad
// 2026-07-06, REVERSES the unflown F2). Chad's loop ruling: "if I stop the aim
// inverted it STAYS inverted; it only rights near level." So the wings-leveling
// roll sources (the deadzone rest-roll + the FINE wings-hold + the auto-level
// held_bank decay) are re-gated on cos_phi_theta > 0: UPRIGHT they level the
// wings; INVERTED they emit ZERO roll, so a paused loop / held Immelmann hangs
// belly-up until the pilot rolls out by AIMING (bank-to-turn) — never an
// uncommanded auto-right. This is what lets a held-up LOOP stay inverted and
// come around by pure pitch instead of Immelmanning at the top.
//
// Seed a belly-up state with the aim held ON THE NOSE each tick (REST — err ~
// 0, so the wings-leveling roll is the ONLY thing that could act; a fixed world
// aim would drift into MANEUVER and right the plane by bank-to-turn, hiding the
// mechanism). Runs the SHIPPED control::step through ClosedLoop (app::tick
// mirrors it). MUTATION: remove the new cos_phi_theta > 0 gate (restore F2) ->
// the wings-hold rolls it UPRIGHT (cos_phi_theta -> +1) and the "stays
// inverted" CHECK fails. The bank-to-turn roll_maneuver is untouched (a lateral
// aim still rolls / the DOWN split-S still rolls through) — this pins ONLY the
// rest/hold wings-leveling half-space gating.
// ===========================================================================
// MB-right RE-SCOPE, second pass (pilot ruling 2026-08-06): inverted righting
// carries NO added delay — inverted_delay is 0, so at rest the plane rights
// from the FIRST rest tick and there is no "within the window" left to fly.
// The S7-loop-invert half-space gate this leg exists for is UNCHANGED (inverted
// flight under active hands is still never fought), so the leg now flies the
// mechanism's own knob-off arm — inverted_rate = 0, the documented way back to
// the old stays-inverted law — where the wings-leveling gate is the ONLY thing
// that could roll the plane. Mutation coverage is intact and sharper: the
// un-gated-wings-hold mutant (wings_level_gate := 1.0, restore F2) rolls from
// TICK 1 with nothing else in the picture. The zero-delay righting itself is
// pinned in test_cascade "MB-right".
TEST_CASE(
    "control::step: an inverted plane at rest STAYS inverted with the "
    "righting knob off") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Belly-up: rolled 150 deg about the nose (cos_phi_theta = cos 150 ~ -0.87,
    // clearly inverted), otherwise flying ~level.
    const sim::SimState s = harness::flight_state(
        kAp, 160.0, 4000.0, up, heading, rad(150.0), 0.0, 0.0);
    harness::ClosedLoop cl(s, s.orientation * glm::dvec3{0.0, 0.0, -1.0});

    // The knob-off arm: no righting mechanism at all, so the window is a
    // plain 1 s of belly-up rest (config-independent — it no longer keys off
    // a delay dial that ships 0).
    control::ControllerParams cp = kCp;
    cp.inverted_rate = 0.0;
    const int window = 120;
    REQUIRE(cp.inverted_rate == 0.0);  // the leg really flies the off arm
    double cpt0 = 1.0, cpt_final = 1.0, max_roll = 0.0, max_pitch = 0.0;
    for (int i = 0; i < window; ++i) {
        cl.aim_nose();  // REST: aim tracks the nose so err ~ 0
        const control::Telemetry t = cl.tick(0.7, kAp, cp);
        REQUIRE(finite_state(cl.state));
        REQUIRE_FALSE(t.righting);  // knob off: the latch can never arm
        if (i == 0) cpt0 = t.extracted.cos_phi_theta;
        cpt_final = t.extracted.cos_phi_theta;
        max_roll = std::max(max_roll,
                            std::abs(static_cast<double>(cl.last_inputs.roll)));
        max_pitch = std::max(
            max_pitch, std::abs(static_cast<double>(cl.last_inputs.pitch)));
    }
    std::printf(
        "[loop-invert] cpt0=%.3f cpt_final=%.3f max|roll|=%.3f "
        "max|pitch|=%.3f (window=%d ticks)\n",
        cpt0, cpt_final, max_roll, max_pitch, window);
    CHECK(cpt0 < -0.5);      // started clearly inverted
    CHECK(cpt_final < 0.0);  // STAYED inverted (mutant rights it: cpt -> +1)
    CHECK(max_roll < 0.05);  // wings-leveling roll GATED off while inverted
    CHECK(max_pitch < 0.2);  // pitch untouched at rest
}

// ===========================================================================
// S7-hrz — horizon recovery on the SHIPPED tick (docs/horizon_recovery_plan.md,
// red-teamed plan §10). The mechanism-FIRING discipline throughout: every leg
// drives a REAL misaligned frame through app::tick (the S7-mouselevel red-team
// escape was testing only the no-op case). Tuning-independent: the legs pin an
// explicit rate/settle, not the live TOML values.
// ===========================================================================
namespace {

// kCp with the S7-hrz knobs pinned (tuning-independent legs). These legs pin
// the LEGACY release arm (release moves nothing but the capture), so the
// S-relorient release snap is explicitly OFF regardless of the shipped toml —
// the knob-on interaction has its own legs in test_relorient.cpp.
control::ControllerParams hrz_cp(double rate_deg_s, double settle) {
    control::ControllerParams c = kCp;
    c.horizon_recovery_rate = rad(rate_deg_s);
    c.horizon_recovery_settle = settle;
    c.freelook_release_orient = false;
    // S-relorient ADDENDUM (2026-07-28): the D9 cancel/never-arm legs below
    // pin the SEALED-V6 semantics, so pin the knob explicitly — the shipped
    // toml now sets release_orient_with_keys = true, which retires exactly
    // those clauses. The knob-ON arms live in test_relorient.cpp (cases
    // 9/9b/9c).
    c.freelook_release_orient_with_keys = false;
    return c;
}

// An airborne LoopState whose carried aim frame is rolled `roll_deg` off the
// local horizon (the post-split-S attitude, manufactured directly).
app::LoopState misaligned_loop(double roll_deg) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    app::LoopState loop =
        flying(harness::level_state(kAp, 150.0, 3000.0, up, heading));
    loop.aim.roll_about_forward(rad(roll_deg));
    return loop;
}

double misalign(const app::LoopState& loop) {
    return loop.aim.up_misalignment(sim::local_up(loop.curr.position));
}

// A freelook tap: hold `hold_ticks`, then the release tick (which captures).
void freelook_tap(app::LoopState& loop, const control::ControllerParams& cp,
                  int hold_ticks = 3) {
    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    for (int i = 0; i < hold_ticks; ++i)
        app::tick(loop, hold, kAp, cp, nullptr);
    app::tick(loop, instr_in(0.7), kAp, cp, nullptr);  // the release tick
}

// A tick input with the hand ON the mouse: a 1-px jiggle, alternating sign so
// the net aim rotation over a run is ~0. Any nonzero delta sets ci.aim_moved,
// which is the ONE cancel the v13 rest-edge camera recovery keys on (pilot
// ruling 2026-08-06 - the recovery fires only when the hand RESTS, and the
// hand always wins). Legs that must pin "nothing rights this frame" fly THIS,
// not a still mouse.
app::TickInput hand_on(int t, double thr = 0.7) {
    app::TickInput in = instr_in(thr);
    in.aim_dx = (t % 2 == 0) ? 1.0 : -1.0;
    return in;
}

}  // namespace

// v9 S-nosesnap (docs/DECISIONS.md @ b4c0751) RE-SCOPE of the S7-hrz family:
// the 150 deg/s open-loop D3 roll is retired at its only consumer — the
// release now rights the ENTIRE up-debt in the same tick as the snap ("no
// eased anything", Chad, superseding the 2026-07-07 "eased" ruling for this
// case). Every leg below that pinned a MID-ROLL latch state
// (recov.remaining > 0) is re-scoped to the instant law's honest equivalents:
// righted-on-the-release-tick, edge-only (never continuous — the
// S7-mouselevel ban's pin survives in that shape), and recov permanently
// inert. NOTHING here was deleted without a successor pin.
TEST_CASE("S7-hrz: freelook release rights the horizon INSTANTLY (v9)") {
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);
    const double m0 = misalign(loop);
    REQUIRE(std::abs(deg(m0)) == Catch::Approx(120.0).margin(0.1));

    freelook_tap(loop, cp);
    // The WHOLE debt retired on the release tick — no latch, no roll-in.
    CHECK(deg(std::abs(misalign(loop))) < 1.0);
    CHECK(loop.recov.remaining == 0.0);  // the latch never arms (inert struct)

    // And stays put: 3 s more changes nothing (transport preserves the
    // local_up-referenced misalignment on an open leg; righting is EDGE-only).
    const app::TickInput in = instr_in(0.7);
    const double m_after = std::abs(misalign(loop));
    for (int t = 0; t < 3 * 120; ++t) {
        app::tick(loop, in, kAp, cp, nullptr);
        CHECK(loop.recov.remaining == 0.0);
    }
    CHECK(std::abs(std::abs(misalign(loop)) - m_after) < 1e-6);
}

TEST_CASE(
    "S7-hrz: righting is EVENT-ONLY - a hand on the mouse is never "
    "auto-leveled") {
    // The S7-mouselevel ban, re-scoped by the v13 rest-edge recovery (pilot
    // ruling 2026-08-06): the frame may be righted by DISCRETE events - the
    // freelook release, and now the aim coming to REST - but NEVER
    // continuously underneath a pilot who is actively aiming. While the hand
    // is on the mouse the carried frame must stay exactly as rolled, for as
    // long as he flies. A per-tick horizon-lock mutant (the old F1/P0 class)
    // rights it regardless of the hand and FAILS here; so does a rest-edge
    // recovery that forgets to cancel on aim_moved.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);
    freelook_tap(loop, cp);
    REQUIRE(deg(std::abs(misalign(loop))) < 1.0);  // release righted it

    // Roll the frame off-horizon mid-flight (no freelook, no release edge).
    loop.aim.roll_about_forward(rad(60.0));
    const double m1 = std::abs(misalign(loop));
    REQUIRE(deg(m1) > 55.0);
    for (int t = 0; t < 3 * 120; ++t)
        app::tick(loop, hand_on(t), kAp, cp, nullptr);
    const double m_end = std::abs(deg(misalign(loop)));
    std::printf("[S7-hrz event-only] standing misalign after 3 s = %.2f deg\n",
                m_end);
    CHECK(m_end > 55.0);  // the hand is on the mouse: nothing rights it
    CHECK(loop.recov.remaining == 0.0);  // and nothing was ever captured
}

TEST_CASE("S7-hrz: edge legs - re-release rights again; keys-held knob-off") {
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    const app::TickInput in = instr_in(0.7);

    SECTION("every release edge rights: re-roll, re-tap, righted again") {
        // Successor of "re-press cancels; the next release recaptures": with
        // no latch there is nothing to cancel, and the surviving semantics is
        // that EACH release edge rights whatever debt exists at that instant.
        app::LoopState loop = misaligned_loop(120.0);
        freelook_tap(loop, cp);
        CHECK(deg(std::abs(misalign(loop))) < 1.0);

        loop.aim.roll_about_forward(rad(90.0));  // new debt, mid-flight
        REQUIRE(deg(std::abs(misalign(loop))) > 80.0);
        freelook_tap(loop, cp);                      // press + release again
        CHECK(deg(std::abs(misalign(loop))) < 1.0);  // righted again, instantly
    }

    SECTION("override keys after a release never touch the carried frame") {
        // Successor of the mid-roll D9 cancel leg (no mid-roll exists): the
        // plane's own keyboard roll moves the PLANE, never the carried frame,
        // and a key press is not a release edge so it rights nothing either.
        app::LoopState loop = misaligned_loop(120.0);
        freelook_tap(loop, cp);
        loop.aim.roll_about_forward(rad(60.0));  // standing debt
        const double m0 = std::abs(deg(misalign(loop)));

        for (int t = 0; t < 60; ++t) {
            app::TickInput ovr = hand_on(t);  // hand on the mouse: no rest edge
            ovr.override_mask[2] = true;      // roll axis keyboard jink
            ovr.override_sign[2] = -1.0;
            app::tick(loop, ovr, kAp, cp, nullptr);
        }
        CHECK(loop.recov.remaining == 0.0);
        CHECK(std::abs(deg(misalign(loop))) == Catch::Approx(m0).margin(2.0));
    }

    SECTION("release while a key is held rights NOTHING under knob-off (4d)") {
        // The sealed-v6 D9 arm, pinned via the BEHAVIOR now that no latch
        // exists: this fixture sets release_orient_with_keys = false, so a
        // keys-held release must leave the misalignment untouched.
        app::LoopState loop = misaligned_loop(120.0);
        const double m0 = std::abs(deg(misalign(loop)));
        app::TickInput hold_both = instr_in(0.7);
        hold_both.freelook_held = true;
        hold_both.override_mask[0] = true;
        hold_both.override_sign[0] = 1.0;
        for (int t = 0; t < 5; ++t)
            app::tick(loop, hold_both, kAp, cp, nullptr);

        app::TickInput key_only = instr_in(0.7);  // Space up, key still down
        key_only.override_mask[0] = true;
        key_only.override_sign[0] = 1.0;
        app::tick(loop, key_only, kAp, cp,
                  nullptr);  // the release edge, key held
        CHECK(std::abs(deg(misalign(loop))) == Catch::Approx(m0).margin(3.0));

        for (int t = 0; t < 120; ++t)
            app::tick(loop, hand_on(t), kAp, cp, nullptr);  // key up, aiming
        // No new edge and the hand is on the mouse -> still unrighted (the
        // one-shot-edge semantics; the v13 rest edge needs a RESTING hand).
        CHECK(std::abs(deg(misalign(loop))) == Catch::Approx(m0).margin(6.0));
        CHECK(loop.recov.remaining == 0.0);
    }
}

TEST_CASE("S7-hrz: recov is permanently INERT under the instant law (F9')") {
    // Successor of the F9 reset legs: there is no mid-roll latch to kill, and
    // the honest surviving pin is that recov NEVER arms across the whole
    // release sequence — its reset()s (respawn, focus loss, alt-tab) stay
    // harmless no-ops. Removing the struct outright is a recorded future
    // candidate (docs/DECISIONS.md), not v9 work.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    const app::TickInput in = instr_in(0.7);
    app::LoopState loop = misaligned_loop(120.0);

    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    for (int t = 0; t < 3; ++t) {
        app::tick(loop, hold, kAp, cp, nullptr);
        CHECK(loop.recov.remaining == 0.0);
    }
    app::tick(loop, in, kAp, cp,
              nullptr);  // the release tick (rights instantly)
    CHECK(loop.recov.remaining == 0.0);
    for (int t = 0; t < 30; ++t) {
        app::tick(loop, in, kAp, cp, nullptr);
        CHECK(loop.recov.remaining == 0.0);
    }
    app::instructor_focus_loss(loop);  // still callable, still a no-op
    CHECK(loop.recov.remaining == 0.0);
}

TEST_CASE("S7-hrz: rate = 0 is a structural no-op (knob-off superset, F8)") {
    const control::ControllerParams cp0 = hrz_cp(0.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);
    const double m0 = std::abs(deg(misalign(loop)));

    freelook_tap(loop, cp0);
    CHECK(loop.recov.remaining == 0.0);  // capture never ran
    const app::TickInput in = instr_in(0.7);
    for (int t = 0; t < 120; ++t) app::tick(loop, in, kAp, cp0, nullptr);
    CHECK(loop.recov.remaining == 0.0);
    // The horizon stayed exactly misaligned (transport preserves it; nothing
    // rolled it). The whole EXISTING suite is the rest of this proof: goldens
    // and the mirror leg never fire a misaligned release.
    CHECK(std::abs(deg(misalign(loop))) == Catch::Approx(m0).margin(1e-6));
}

TEST_CASE("S7-hrz: mouse-up stays screen-up AFTER the instant roll (F6')") {
    // The §2 relationship pinned right where the mechanism just fired: the
    // release tick applied a ~120 deg roll in ONE step, and the mouse basis
    // must have rolled WITH the camera (one quaternion) so mouse-up is still
    // screen-up on the very next input. Camera forward pinned ==
    // aim.forward() so the check is isolated from the chase-lag float (the
    // F6 attribution discipline, re-scoped from mid-roll to post-snap by the
    // v9 instant law, b4c0751).
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);
    freelook_tap(loop, cp);  // the release tick rights ~120 deg instantly
    REQUIRE(deg(std::abs(misalign(loop))) < 1.0);
    const app::TickInput in = instr_in(0.7);
    for (int t = 0; t < 60; ++t) app::tick(loop, in, kAp, cp, nullptr);

    const double fovy = rad(60.0), aspect = 16.0 / 9.0;
    const glm::dvec3 f = loop.aim.forward();
    input::AimFrame nudged = loop.aim;
    nudged.apply_mouse(0.0, -1.0, 0.01);  // mouse UP
    const render::ScreenPoint c0 =
        render::project_dir(f, f, loop.aim.up(), fovy, aspect);
    const render::ScreenPoint c1 =
        render::project_dir(nudged.forward(), f, loop.aim.up(), fovy, aspect);
    REQUIRE(c1.in_front);
    CHECK(c1.y > c0.y + 1e-6);  // mouse-up -> UP on screen, mid-recovery
}

TEST_CASE("S7-hrz: righting runs AFTER the rule-3 snap (diff red-team P2-1)") {
    // The load-bearing v6 ORDERING, unchanged by the v9 instant law: the
    // release tick fires BOTH the aim snap AND the righting, and the righting
    // must measure the up-misalignment about the SNAPPED forward, or it rolls
    // a wrong fixed angle and parks off-horizon. Mutation this kills: hoisting
    // the righting block above the snap handling. The kill needs the release
    // forward WELL OFF the held aim in BOTH axes (re-review round: a
    // level-flight release makes up_misalignment invariant under a yaw-only
    // snap — the geodesic axis is parallel to local_up — and second-order
    // under a pitch-only one; a ~90-tick pitch+yaw hold opens the gap; the
    // hoisted mutant parks at a multi-degree residual vs ~0 correct).
    // v9 re-scope (b4c0751): the oracle moved from "3 s open-loop roll then
    // righted" to "righted ON the release tick" — the instant law.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);

    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    for (int t = 0; t < 3; ++t) app::tick(loop, hold, kAp, cp, nullptr);
    app::TickInput hold_key = hold;  // pitch+yaw override held INSIDE the hold
    hold_key.override_mask[0] = true;
    hold_key.override_sign[0] = 1.0;
    hold_key.override_mask[1] = true;
    hold_key.override_sign[1] = 1.0;
    for (int t = 0; t < 90; ++t) app::tick(loop, hold_key, kAp, cp, nullptr);
    for (int t = 0; t < 3; ++t)
        app::tick(loop, hold, kAp, cp, nullptr);  // keys up

    app::tick(loop, instr_in(0.7), kAp, cp,
              nullptr);  // release: snap + instant right
    CHECK(loop.recov.remaining == 0.0);
    CHECK(std::abs(deg(misalign(loop))) < 1.0);  // righted about the NEW fwd
}

// ===========================================================================
// v13 REST-EDGE CAMERA HORIZON RECOVERY (pilot ruling 2026-08-06: "after a
// maneuver ending inverted — split-S, Immelmann — the CAMERA also rights
// itself automatically at rest: horizon level, planet below — without a
// freelook release"). Same gauge move as the release roll, second trigger:
// the aim coming to REST. Every leg drives a REAL misaligned frame through the
// shipped app::tick (the mechanism-FIRING discipline), and the cancel legs use
// a hand ON the mouse — the one and only cancel the mechanism reads.
// ===========================================================================
namespace {
// The dwell in ticks, derived from the dial the mechanism actually reads (a
// welded constant here would silently disarm on a retune — the AT-15 class).
int dwell_ticks(const control::ControllerParams& cp) {
    return static_cast<int>(cp.horizon_recovery_rest_dwell / kAp.sim_dt + 0.5);
}
}  // namespace

TEST_CASE("v13 rest-edge: a carried inversion rights itself at rest") {
    // The split-S case: the maneuver ends with the carried aim/camera frame
    // upside down (the world above the pilot), the hand comes off the mouse,
    // and the horizon must come back on its own — no freelook release.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(170.0);  // carried inversion
    const double m0 = std::abs(deg(misalign(loop)));
    REQUIRE(m0 > 165.0);

    const app::TickInput still = instr_in(0.7);
    const int dwell = dwell_ticks(cp);
    REQUIRE(dwell > 4);  // the dial is a real dwell on the shipped table

    // (a) BEFORE the dwell elapses nothing moves: the capture is EDGE
    // triggered, not a per-tick lock (mutation: capture every rest tick, or
    // drop the dwell -> the frame is already rolling here).
    for (int t = 0; t < dwell - 2; ++t)
        app::tick(loop, still, kAp, cp, nullptr);
    CHECK(std::abs(deg(misalign(loop))) == Catch::Approx(m0).margin(0.05));
    CHECK(loop.recov.remaining == 0.0);

    // (b) the rest EDGE captures ONCE and the debt rolls out at <= rate.
    double prev = deg(misalign(loop));
    double max_step = 0.0;
    bool rolled = false;
    int done_tick = -1;
    const double per_tick_cap = deg(cp.horizon_recovery_rate) * kAp.sim_dt;
    for (int t = 0; t < 900; ++t) {
        app::tick(loop, still, kAp, cp, nullptr);
        const double now = deg(misalign(loop));
        rolled = rolled || loop.recov.remaining > 0.0;
        max_step = std::max(max_step, std::abs(now - prev));
        prev = now;
        if (done_tick < 0 && rolled && loop.recov.remaining == 0.0)
            done_tick = t;
    }
    std::printf(
        "[v13 rest-edge] m0=%.1f -> %.4f deg, max step %.3f deg/tick "
        "(cap %.3f), done@%d ticks\n",
        m0, std::abs(prev), max_step, per_tick_cap, done_tick);
    CHECK(rolled);  // the recovery genuinely ran (non-vacuous)
    // Rate-limited: no tick may exceed the dial (mutation: retire the whole
    // debt in one tick, as the RELEASE edge does -> ~170 deg in one step).
    CHECK(max_step <= per_tick_cap + 0.05);
    // Terminates, structurally: remaining reaches EXACTLY 0 and the horizon is
    // level inside the finish epsilon (~0.57 deg).
    CHECK(done_tick > 0);
    CHECK(loop.recov.remaining == 0.0);
    CHECK(std::abs(prev) <= deg(input::HorizonRecovery::kFinishEps));
}

TEST_CASE("v13 rest-edge: a captured roll completes as one smooth motion") {
    // Pilot fly-ruling 2026-08-06 (first v13 build: "the camera rotation is
    // happening in steps — it should be one smooth motion"): mouse motion
    // never cancels a roll IN FLIGHT — the captured debt completes, exactly as
    // the freelook-RELEASE roll survives a mid-roll keypress (the S7-hrz
    // open-loop law). The hand owns the CAPTURE only: motion resets the dwell
    // and disarms a FINISHED roll. (Mutation: restore the aim_moved
    // recov.reset() -> the mid-roll jiggle below stalls the roll and the
    // misalignment parks; the smooth leg fails.)
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(170.0);
    const app::TickInput still = instr_in(0.7);
    const int dwell = dwell_ticks(cp);

    for (int t = 0; t < dwell + 30; ++t)
        app::tick(loop, still, kAp, cp, nullptr);
    REQUIRE(loop.recov.remaining > 0.0);  // mid-roll
    const double m_mid = std::abs(deg(misalign(loop)));
    REQUIRE(m_mid < 165.0);  // it really had rolled some
    REQUIRE(m_mid > 20.0);   // ...and is nowhere near finished

    // Resting-hand sensor jitter: a 1-px delta every 30 ticks, all the way
    // through. The roll must keep retiring monotonically — never stall, never
    // re-dwell — and finish inside the finish epsilon on schedule.
    double prev = std::abs(deg(misalign(loop)));
    double worst_stall = 0.0;  // longest run of ticks with no roll progress
    double stall = 0.0;
    for (int t = 0; t < 900; ++t) {
        const bool jig = (t % 30) == 0;
        app::tick(loop, jig ? hand_on(t) : still, kAp, cp, nullptr);
        const double now = std::abs(deg(misalign(loop)));
        // Roll progress is "misalignment decreased". Track the longest
        // no-progress run over every tick the debt is PHYSICALLY outstanding
        // (misalignment > eps) — NOT gated on recov.remaining: the cancel
        // mutant's re-dwell parks have remaining == 0, which is exactly the
        // stall this leg exists to catch (first draft was gated on remaining
        // and the mutant passed — the fixture-no-op class).
        if (now > deg(input::HorizonRecovery::kFinishEps) + 0.2 &&
            now >= prev - 1e-9)
            stall += 1.0;
        else
            stall = 0.0;
        worst_stall = std::max(worst_stall, stall);
        prev = now;
    }
    CHECK(loop.recov.remaining == 0.0);
    CHECK(prev <= deg(input::HorizonRecovery::kFinishEps) + 0.2);
    // One smooth motion: no re-dwell gaps. A cancel+re-arm mechanism parks for
    // at least a full dwell (18 ticks) after every jiggle; the completing roll
    // never pauses at all.
    CHECK(worst_stall <= 2.0);

    // The hand still owns the CAPTURE: after completion, motion disarms, and a
    // fresh inversion + rest re-captures only after a full new dwell.
    app::tick(loop, hand_on(0), kAp, cp, nullptr);
    CHECK_FALSE(loop.recov_armed);
    CHECK(loop.aim_rest == 0.0);
    loop.aim.roll_about_forward(rad(170.0));
    for (int t = 0; t < dwell - 2; ++t)
        app::tick(loop, still, kAp, cp, nullptr);
    CHECK(loop.recov.remaining == 0.0);  // not before the fresh dwell
    for (int t = 0; t < 900; ++t) app::tick(loop, still, kAp, cp, nullptr);
    CHECK(std::abs(deg(misalign(loop))) <=
          deg(input::HorizonRecovery::kFinishEps));
}

TEST_CASE("v13c arm gate: a sub-arm_min tilt never fires the recovery") {
    // The arm_min MECHANISM pin (fly-2 2026-08-06). The SHIPPED value is 0
    // since fly-4 ("any change in horizon from flat should automatically
    // adjust") — so this leg dials its own 60 deg gate locally, exactly as the
    // easeback tests pin their own window: the dial's machinery must survive
    // for the walk-back. (Mutation: drop the arm_min gate -> this tilt rolls
    // level.)
    control::ControllerParams cp = hrz_cp(150.0, 5.0);
    cp.horizon_recovery_arm_min = rad(60.0);
    const double tilt = deg(cp.horizon_recovery_arm_min) * 0.5;
    app::LoopState loop = misaligned_loop(tilt);
    const double m0 = std::abs(deg(misalign(loop)));

    const app::TickInput still = instr_in(0.7);
    for (int t = 0; t < dwell_ticks(cp) + 600; ++t)
        app::tick(loop, still, kAp, cp, nullptr);
    CHECK(loop.recov.remaining == 0.0);
    CHECK_FALSE(loop.recov_armed);
    // The tilt stays (small carried-transport drift aside): nothing rolled it.
    CHECK(std::abs(deg(misalign(loop))) > m0 - 3.0);
}

TEST_CASE("v13c arm gate: a held-off aim blocks the capture until on-path") {
    // The reticle-stability half of the fly-2 ruling: the roll sweeps every
    // off-center pixel of the picture, so the capture may only fire when the
    // aim is RESOLVED ON THE FLIGHT PATH (angle aim-vs-velocity <=
    // path_band) — then the reticle sits ~centered and the roll cannot
    // displace it perceptibly. A held-off aim (a carve) structurally cannot
    // fire even with the mouse dead still. (Mutation: drop the on_path gate ->
    // the capture fires on the first post-dwell tick at ~40 deg off path.)
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    const double band = deg(cp.horizon_recovery_path_band);
    REQUIRE(band > 1.0);

    app::LoopState loop = misaligned_loop(170.0);
    // Yank the aim ~40 deg off the flight path in ONE gesture (test-side frame
    // surgery, not a tick input — the mouse then rests for the whole leg).
    loop.aim.apply_mouse(1.0, 0.0, rad(40.0));

    const app::TickInput still = instr_in(0.7);
    int capture_tick = -1;
    double angle_at_capture = 1e9;
    bool ever_offpath_armed = false;
    for (int t = 0; t < 2400 && capture_tick < 0; ++t) {
        app::tick(loop, still, kAp, cp, nullptr);
        const double speed = glm::length(loop.curr.velocity);
        const double ang =
            deg(std::acos(glm::clamp(glm::dot(loop.aim.forward(),
                                              loop.curr.velocity / speed),
                                     -1.0, 1.0)));
        if (loop.recov.remaining > 0.0) {
            capture_tick = t;
            angle_at_capture = ang;
        } else if (ang > band + 2.0 && t > dwell_ticks(cp) + 2) {
            // Post-dwell, mouse still, debt huge — ONLY the path gate holds.
            ever_offpath_armed = ever_offpath_armed || loop.recov_armed;
        }
    }
    CHECK_FALSE(ever_offpath_armed);
    // The pursuit converges the path onto the held aim, and the capture fires
    // only once resolved — no fresh dwell needed (the gate binds the capture,
    // not the rest).
    CHECK(capture_tick > 0);
    CHECK(angle_at_capture <= band + 1.0);
}

TEST_CASE("v13d arm gate: a turning path blocks the capture until straight") {
    // Chad fly-3 2026-08-06: "wait until I fly straight to do so." A held
    // pitch override loops the plane with the mouse dead still — the dwell
    // banks, the sweeping path even CROSSES the aim (on_path goes true at the
    // crossings), and only the straightness gate (path rotation rate <=
    // [horizon_recovery] straight_max) holds the capture. (Mutation: drop the
    // `straight` conjunct -> a crossing tick captures mid-loop.)
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    const double band = deg(cp.horizon_recovery_path_band);
    app::LoopState loop = misaligned_loop(170.0);
    // Start the pull with the aim held OFF the path (fly-5 note: from an
    // on-path start the capture legitimately fires inside the override ramp's
    // first straight ticks — settled is settled; the blocking this leg pins is
    // the TURNING case, so the aim starts 30 deg off and only the mid-loop
    // path sweeps cross it, at full turn rate). The offset is PITCH (dy) so
    // the aim stays inside the elevator-loop's sweep plane — a yawed-off aim
    // is never crossed at all (crossings == 0, the leg goes vacuous).
    loop.aim.apply_mouse(0.0, 1.0, rad(30.0));

    app::TickInput pull = instr_in(0.9);
    pull.override_mask[0] = true;  // elevator held, mouse still
    pull.override_sign[0] = 1.0;
    int crossings = 0;
    glm::dvec3 prev_v = loop.curr.velocity;
    double max_rate = 0.0;
    for (int t = 0; t < 1400; ++t) {
        app::tick(loop, pull, kAp, cp, nullptr);
        const double speed = glm::length(loop.curr.velocity);
        REQUIRE(speed > 1.0);
        const glm::dvec3 path = loop.curr.velocity / speed;
        const double ang = deg(std::acos(
            glm::clamp(glm::dot(loop.aim.forward(), path), -1.0, 1.0)));
        if (ang <= band) ++crossings;
        const glm::dvec3 pv = prev_v / glm::length(prev_v);
        max_rate = std::max(
            max_rate,
            deg(std::acos(glm::clamp(glm::dot(path, pv), -1.0, 1.0))) /
                kAp.sim_dt);
        prev_v = loop.curr.velocity;
        // The whole pull: never a capture, never a roll.
        REQUIRE(loop.recov.remaining == 0.0);
    }
    REQUIRE(crossings > 0);  // non-vacuous: the path DID sweep through the aim
    // Non-vacuity margin: the pull must turn WELL above the qualifier. The
    // elevator loop peaks at ~35 deg/s (physics, not a dial); with
    // straight_max at 12 (fly 2026-09-10, "trigger a bit sooner") a 3x margin
    // would demand 36, so the margin is 2x -- still ~3 dial-widths of daylight
    // between the loop and the gate, and the blocking assertion above (no
    // capture through the whole pull) is unchanged.
    REQUIRE(max_rate > deg(cp.horizon_recovery_straight_max) * 2.0);
    CHECK_FALSE(loop.recov_armed);

    // Keys off, path settles straight onto the held aim: the capture fires.
    const app::TickInput still = instr_in(0.7);
    bool fired = false;
    for (int t = 0; t < 2400 && !fired; ++t) {
        app::tick(loop, still, kAp, cp, nullptr);
        fired = loop.recov.remaining > 0.0 || loop.recov_armed;
    }
    CHECK(fired);
}

TEST_CASE("v13 rest-edge: freelook held suppresses the recovery") {
    // Checking six is not resting: while the mouse is on the CAMERA the
    // carried frame is the pilot's to look around with, and nothing rights it.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);
    app::TickInput look = instr_in(0.7);
    look.freelook_held = true;
    for (int t = 0; t < 600; ++t) {
        app::tick(loop, look, kAp, cp, nullptr);
        REQUIRE(loop.recov.remaining == 0.0);  // never captured
        REQUIRE_FALSE(loop.recov_armed);
        REQUIRE(loop.aim_rest == 0.0);  // the dwell never accumulates
    }
    CHECK(std::abs(deg(misalign(loop))) > 100.0);  // still rolled off-horizon
}

TEST_CASE("v13 rest-edge: GROUNDED cancels an in-progress recovery") {
    // A respawn must never inherit the dead life's roll or its banked dwell
    // (the fl.reset() pairing discipline, applied to the v13 latches).
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(170.0);
    const app::TickInput still = instr_in(0.7);
    for (int t = 0; t < dwell_ticks(cp) + 30; ++t)
        app::tick(loop, still, kAp, cp, nullptr);
    REQUIRE(loop.recov.remaining > 0.0);  // mid-roll when the plane dies

    loop.grounded = true;
    app::tick(loop, still, kAp, cp, nullptr);  // the GROUNDED pairing tick
    CHECK(loop.recov.remaining == 0.0);
    CHECK(loop.aim_rest == 0.0);
    CHECK_FALSE(loop.recov_armed);
}

TEST_CASE("v13 rest-edge: the recovery never moves the aim direction") {
    // It is a GAUGE move: only the roll DOF about the aim's own forward is
    // steered. Pinned differentially (rate ON vs rate OFF, identical input
    // trace): the aim FORWARD and the whole trajectory must match while the
    // UP debt is retired on one arm only. This is also the RA9/§9.1 pin — a
    // recovery the instructor could see would be a control-loop smoothing.
    const control::ControllerParams cp_on = hrz_cp(150.0, 5.0);
    const control::ControllerParams cp_off = hrz_cp(0.0, 5.0);
    app::LoopState on = misaligned_loop(170.0);
    app::LoopState off = misaligned_loop(170.0);
    const app::TickInput still = instr_in(0.7);
    for (int t = 0; t < 900; ++t) {
        app::tick(on, still, kAp, cp_on, nullptr);
        app::tick(off, still, kAp, cp_off, nullptr);
    }
    REQUIRE(std::abs(deg(misalign(on))) <=
            deg(input::HorizonRecovery::kFinishEps));  // the roll ran
    REQUIRE(std::abs(deg(misalign(off))) > 160.0);     // and not on the other

    const double fwd_gap = glm::length(on.aim.forward() - off.aim.forward());
    const double pos_err = glm::length(on.curr.position - off.curr.position);
    std::printf("[v13 gauge] aim fwd gap=%.17e  pos_err=%.17e m\n", fwd_gap,
                pos_err);
    CHECK(fwd_gap < 1e-12);  // pointing untouched: ONLY the up rolled
    CHECK(pos_err < 1e-9);   // and the plane flew the identical path
}

TEST_CASE("v13 rest-edge: an upright rest is a structural no-op") {
    // Below kFinishEps the capture RESETS instead of latching, so a level rest
    // costs no quaternion math at all — the aim frame is bit-identical to the
    // knob-off arm tick for tick (the strict-superset proof shape). A mutant
    // that captures/rolls a sub-epsilon debt perturbs those bits.
    const control::ControllerParams cp_on = hrz_cp(150.0, 5.0);
    const control::ControllerParams cp_off = hrz_cp(0.0, 5.0);
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    app::LoopState on = flying(s);
    app::LoopState off = flying(s);
    REQUIRE(std::abs(misalign(on)) < input::HorizonRecovery::kFinishEps);
    const app::TickInput still = instr_in(0.7);
    for (int t = 0; t < 600; ++t) {
        app::tick(on, still, kAp, cp_on, nullptr);
        app::tick(off, still, kAp, cp_off, nullptr);
        REQUIRE(on.recov.remaining == 0.0);  // never fires at rest upright
    }
    CHECK(on.aim.q.w == off.aim.q.w);
    CHECK(on.aim.q.x == off.aim.q.x);
    CHECK(on.aim.q.y == off.aim.q.y);
    CHECK(on.aim.q.z == off.aim.q.z);
}

TEST_CASE("S7-hrz: the roll never moves the trajectory (diff red-team P2-2)") {
    // The end-to-end gauge pin: the same misaligned freelook-tap input trace,
    // rate = 150 vs rate = 0, must fly the SAME trajectory — the recovery is
    // invisible to control::step (ci.target_dir_world = aim.forward(), and a
    // roll about forward fixes forward to ~1e-16/tick). The mirror-equivalence
    // leg CANNOT cover this (harness ClosedLoop carries a bare vec3 aim — the
    // roll is unrepresentable there), so this leg is the wiring's only
    // trajectory-neutrality pin. Bound set just above the observed fp floor
    // (printed at max_digits10 below — the S1/AT-9 discipline, never an
    // eyeballed hide-band).
    const control::ControllerParams cp_on = hrz_cp(150.0, 5.0);
    const control::ControllerParams cp_off = hrz_cp(0.0, 5.0);

    app::LoopState on = misaligned_loop(120.0);
    app::LoopState off = misaligned_loop(120.0);
    freelook_tap(on, cp_on);
    freelook_tap(off, cp_off);
    const app::TickInput in = instr_in(0.7);
    for (int t = 0; t < 3 * 120; ++t) {
        app::tick(on, in, kAp, cp_on, nullptr);
        app::tick(off, in, kAp, cp_off, nullptr);
    }
    REQUIRE(on.recov.remaining == 0.0);  // the roll genuinely ran on one arm
    REQUIRE(std::abs(deg(misalign(on))) < 1.0);
    REQUIRE(std::abs(deg(misalign(off))) > 100.0);  // and not on the other

    const double pos_err = glm::length(on.curr.position - off.curr.position);
    std::printf("[S7-hrz gauge] pos_err(rate on vs off) = %.17e m\n", pos_err);
    // Observed EXACTLY 0.0 at max_digits10: the roll's ~1e-16/tick forward
    // perturbation vanishes where the controller output quantizes through the
    // float sim::Inputs seam, so the two arms fly bit-identically. Pinned just
    // above (S1: never an eyeballed hide-band — 1e-6 would hide a real leak);
    // any roll-contaminated control read lands orders of magnitude higher.
    CHECK(pos_err < 1e-9);
}

// ===========================================================================
// S7-nest — §5b freelook keyboard-flight aim nesting + D8 velocity release
// (docs/horizon_recovery_plan.md §5b, D7/D8/D10; SPEC §9.5 amendment). The
// mechanism-FIRING discipline again: every leg drives a real override
// maneuver, and the KEEP leg pins the path the change must NOT touch.
// ===========================================================================
namespace {
glm::dvec3 nose_dir(const app::LoopState& loop) {
    return loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
}
}  // namespace

TEST_CASE("S7-nest: aim rides the nose during freelook keyboard flight (D7)") {
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = flying(
        harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                             glm::dvec3{0.0, 0.0, -1.0}));
    app::TickInput in = instr_in(1.0);
    in.freelook_held = true;
    in.override_mask[0] = true;  // full pitch pull, nose on the move
    in.override_sign[0] = 1.0;
    for (int t = 0; t < 120; ++t) {
        app::tick(loop, in, kAp, cp, nullptr);
        // Nested every tick: the aim IS the nose (snapped to this tick's
        // pre-step nose; the post-step nose is at most one tick of body
        // rotation away, ~0.5 deg at the G-limited pitch rate).
        CHECK(glm::dot(loop.aim.forward(), nose_dir(loop)) >
              std::cos(rad(1.5)));
    }
}

TEST_CASE("v9 WELD: aim rides the nose during freelook with NO keys") {
    // The b4c0751 weld ruling's own pin (Chad, verbatim: "the nose aim
    // becomes welded to the nose directionality since mouse inputs now
    // control camera during the freelook phase" — keys or NOT). Entry does
    // the welding: a mid-turn Space press stops the old mouse command from
    // driving the plane, so the aim must sit on the nose EVERY freelook tick
    // even with zero override keys. This retires the MB-lean-era "freelook
    // carves a held lateral aim" side scope, deliberately (a direct either/or
    // answered by Chad). Mutation this kills: re-adding the any_ovr term to
    // the §5b nest condition (the pre-v9 code exactly).
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = flying(
        harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                             glm::dvec3{0.0, 0.0, -1.0}));
    // Park the aim WELL off the nose first (the mid-turn mouse command).
    app::TickInput sweep = instr_in(1.0);
    sweep.aim_dx = rad(45.0) / kCp.aim_sensitivity;
    app::tick(loop, sweep, kAp, cp, nullptr);
    REQUIRE(glm::dot(loop.aim.forward(), nose_dir(loop)) < std::cos(rad(20.0)));

    app::TickInput fl = instr_in(1.0);
    fl.freelook_held = true;  // NO override keys anywhere
    for (int t = 0; t < 120; ++t) {
        app::tick(loop, fl, kAp, cp, nullptr);
        CHECK(glm::dot(loop.aim.forward(), nose_dir(loop)) >
              std::cos(rad(1.5)));
    }
}

TEST_CASE(
    "S7-nest: vertical pull under nesting keeps the carried up "
    "continuous (the D10 instrument)") {
    // The plan-stage red-team's F3 predicted a ~180 deg one-tick roll-snap of
    // the view when the nose sweeps through the carried up. That analysis
    // assumed a STATIC carried up under a large forward gap; per-tick nesting
    // caps the gap at one tick of airframe rotation, so the up should rotate
    // RIGIDLY with the nose — this leg is the D10 ruling instrument: it drives
    // the nose >100 deg (through the initial up at +90) and pins per-tick
    // camera-up continuity. If this ever fails, implement the D10 cone-hold.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = flying(
        harness::level_state(kAp, 220.0, 4000.0, glm::dvec3{1.0, 0.0, 0.0},
                             glm::dvec3{0.0, 0.0, -1.0}));
    const glm::dvec3 nose0 = nose_dir(loop);
    app::TickInput in = instr_in(1.0);
    in.freelook_held = true;
    in.override_mask[0] = true;
    in.override_sign[0] = 1.0;

    double max_pitch = 0.0, max_up_step = 0.0;
    glm::dvec3 up_prev = loop.aim.up();
    for (int t = 0; t < 500; ++t) {
        app::tick(loop, in, kAp, cp, nullptr);
        REQUIRE(std::isfinite(loop.aim.up().x));
        const double cang = glm::dot(loop.aim.up(), up_prev);
        max_up_step = std::max(max_up_step, std::acos(std::min(1.0, cang)));
        up_prev = loop.aim.up();
        max_pitch = std::max(
            max_pitch,
            std::acos(std::clamp(glm::dot(nose_dir(loop), nose0), -1.0, 1.0)));
    }
    std::printf(
        "[S7-nest D10] max nose sweep %.1f deg, max per-tick up step "
        "%.3f deg\n",
        deg(max_pitch), deg(max_up_step));
    REQUIRE(deg(max_pitch) > 100.0);  // the F3 crossing genuinely happened
    CHECK(deg(max_up_step) < 3.0);    // no roll-snap: up rides the nose
}

TEST_CASE(
    "S7-nest: KEEP - override OUTSIDE freelook leaves the aim "
    "transport-pure") {
    // Chad's tested-and-relied-on behavior (§5b KEEP): keyboard flight without
    // freelook must leave the aim EXACTLY where the mouse put it — the only
    // thing that may move it is parallel transport. Pinned per tick against
    // the independent transport_rotation path. Mutation this kills: the
    // nesting condition losing its freelook_held guard.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = flying(
        harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                             glm::dvec3{0.0, 0.0, -1.0}));
    app::TickInput in = instr_in(1.0);  // NO freelook
    in.override_mask[0] = true;         // hard keyboard pull
    in.override_sign[0] = 1.0;
    for (int t = 0; t < 120; ++t) {
        const glm::dvec3 f0 = loop.aim.forward();
        const glm::dvec3 u0 = loop.prev_up;
        app::tick(loop, in, kAp, cp, nullptr);
        // Transport this tick used up(prev position) — st.prev holds it now.
        const glm::dvec3 u1 = sim::local_up(loop.prev.position);
        const glm::dvec3 expected = control::transport_rotation(u0, u1) * f0;
        REQUIRE(glm::length(loop.aim.forward() - expected) < 1e-12);
    }
}

TEST_CASE("S7-nest: release lands the aim on the NOSE (v9 S-nosesnap)") {
    // v9 re-scope of the D8 guarded-velocity oracle (docs/DECISIONS.md @
    // b4c0751): the release target is the NOSE, always — the S7-nest "one
    // composed settle behind velocity" premise died when the camera rest
    // target became the aim ([camera] lead = 1.0), and the velocity target
    // was term A of the v9 defect (a measured 18.6 deg aim/camera jump at
    // the mouse handover). The bases-must-separate discipline is kept: the
    // leg only pins anything when nose and vhat are genuinely apart, and the
    // aim must land on the NOSE side of that gap.
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);

    SECTION("after a keyboard pull: aim := NOSE, NOT vhat") {
        app::LoopState loop = flying(
            harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                                 glm::dvec3{0.0, 0.0, -1.0}));
        app::TickInput hold = instr_in(1.0);
        hold.freelook_held = true;
        app::TickInput pull = hold;
        pull.override_mask[0] = true;
        pull.override_sign[0] = 1.0;
        for (int t = 0; t < 60; ++t) app::tick(loop, pull, kAp, cp, nullptr);
        for (int t = 0; t < 2; ++t)
            app::tick(loop, hold, kAp, cp, nullptr);  // key up

        // POISON the sim's held v-hat before the release tick (S4a: a
        // two-copies seam test must poison the copy it claims isn't read —
        // S7-nest red-team P3): the snap must read NOTHING velocity-flavored
        // now, and certainly never SimState.last_vhat (F5 — this feeds a
        // CONTROL input). Kept so a velocity-target regression of EITHER
        // copy (caller's or the sim's held one) fails the nose check below.
        loop.curr.last_vhat = glm::normalize(glm::dvec3{0.3, -0.8, 0.5});
        app::tick(loop, instr_in(1.0), kAp, cp, nullptr);  // release
        const glm::dvec3 vhat = glm::normalize(loop.curr.velocity);
        const glm::dvec3 nose = nose_dir(loop);
        const double gap =
            deg(std::acos(std::clamp(glm::dot(vhat, nose), -1.0, 1.0)));
        std::printf("[S7-nest v9] nose-vs-velocity gap at release = %.2f deg\n",
                    gap);
        REQUIRE(gap > 2.0);  // the bases separated (else this pins nothing)
        CHECK(glm::dot(loop.aim.forward(), nose) > std::cos(rad(1.0)));
        CHECK(glm::dot(loop.aim.forward(), vhat) < std::cos(rad(1.0)));
    }

    SECTION("low speed falls back to the nose") {
        app::LoopState loop = flying(
            harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                                 glm::dvec3{0.0, 0.0, -1.0}));
        app::TickInput hold = instr_in(0.0);
        hold.freelook_held = true;
        app::TickInput pull = hold;
        pull.override_mask[0] = true;
        pull.override_sign[0] = 1.0;
        for (int t = 0; t < 5; ++t) app::tick(loop, pull, kAp, cp, nullptr);
        // Manufacture the degenerate state at the release edge: sub-ballistic
        // speed with the velocity DIRECTION well OFF the nose (S7-nest
        // red-team P2: a nose-parallel velocity made the v_ballistic guard
        // mutation-invisible — the bases must SEPARATE for the fall-back-to-
        // NOSE assertion to pin anything; guard deleted => aim lands ~35 deg
        // off the nose and the check below fails).
        loop.curr.velocity =
            5.0 * glm::normalize(nose_dir(loop) + 0.7 * loop.aim.up());
        app::tick(loop, hold, kAp, cp,
                  nullptr);  // key up (re-pin after the tick)
        loop.curr.velocity =
            5.0 * glm::normalize(nose_dir(loop) + 0.7 * loop.aim.up());
        app::tick(loop, instr_in(0.0), kAp, cp, nullptr);  // release
        CHECK(glm::dot(loop.aim.forward(), nose_dir(loop)) >
              std::cos(rad(1.0)));
    }

    SECTION("tail-slide (fast BACKWARD velocity) falls back to the nose") {
        app::LoopState loop = flying(
            harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                                 glm::dvec3{0.0, 0.0, -1.0}));
        app::TickInput hold = instr_in(0.0);
        hold.freelook_held = true;
        app::TickInput pull = hold;
        pull.override_mask[0] = true;
        pull.override_sign[0] = 1.0;
        for (int t = 0; t < 5; ++t) app::tick(loop, pull, kAp, cp, nullptr);
        app::tick(loop, hold, kAp, cp, nullptr);           // key up
        loop.curr.velocity = -100.0 * nose_dir(loop);      // vhat . nose < 0
        app::tick(loop, instr_in(0.0), kAp, cp, nullptr);  // release
        CHECK(glm::dot(loop.aim.forward(), nose_dir(loop)) >
              std::cos(rad(1.0)));
    }

    SECTION("freelook WITHOUT keys: the WELD holds; the release adds nothing") {
        // v9 re-scope (b4c0751): the old pin here — "release still moves
        // nothing (held turn)" — pinned the retired no-keys CARVE (the parked
        // aim surviving freelook). Chad retired it deliberately ("the nose
        // maintains its heading... my mouse has no control over the plane
        // [in freelook]"), so the surviving semantics is: the WELD owns the
        // aim during the hold (knob-off included — the weld is not behind
        // release_orient), and the release TICK itself still adds nothing
        // under this fixture's release_orient=false table.
        app::LoopState loop = flying(
            harness::level_state(kAp, 200.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                                 glm::dvec3{0.0, 0.0, -1.0}));
        // Park the aim off the nose first (a held turn), then a clean tap.
        app::TickInput mouse = instr_in(1.0);
        mouse.aim_dx = rad(30.0) / kCp.aim_sensitivity;
        app::tick(loop, mouse, kAp, cp, nullptr);
        REQUIRE(glm::dot(loop.aim.forward(), nose_dir(loop)) <
                std::cos(rad(20.0)));  // genuinely parked off the nose
        app::TickInput hold = instr_in(1.0);
        hold.freelook_held = true;
        for (int t = 0; t < 10; ++t) app::tick(loop, hold, kAp, cp, nullptr);
        // The weld put the aim on the nose during the hold.
        CHECK(glm::dot(loop.aim.forward(), nose_dir(loop)) >
              std::cos(rad(1.5)));
        const glm::dvec3 welded = loop.aim.forward();
        app::tick(loop, instr_in(1.0), kAp, cp,
                  nullptr);  // release, no override used
        // Knob-off: the release tick snapped nothing (transport only).
        CHECK(glm::dot(loop.aim.forward(), welded) > std::cos(rad(0.5)));
    }
}

// ===========================================================================
// RMB-zoom mouse-gain scale (2026-07-07): aim_gain_scale multiplies
// aim_sensitivity for the tick's apply_mouse, so a zoomed (magnified) view aims
// proportionally slower. A pure scalar on the raw delta (NOT a basis smoothing
// — RA9 holds); defaulted 1.0 so the pre-zoom tick is bit-identical (the whole
// suite is unchanged — the strict superset). Here: scale 0.5 turns HALF as far,
// and the UNSET default equals scale 1.0 (the additive-feature differential,
// the S8-drone P1 lesson: a defaulted field needs a leg that actually exercises
// it).
// ===========================================================================
TEST_CASE("aim_gain_scale: scales the mouse->aim rotation, default is 1.0",
          "[app][zoom]") {
    const sim::SimState s = app::spawn_state(kAp);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    auto ang_from_nose = [&](const glm::dvec3& f) {
        return std::acos(glm::clamp(glm::dot(f, nose), -1.0, 1.0));
    };
    // One tick, a 20-deg yaw offer at scale 1, measured against the spawn nose
    // (transport is identity on the first tick; only apply_mouse moves the
    // aim).
    auto swept = [&](double scale, bool set_scale) {
        app::LoopState st = flying(s);
        app::TickInput in = instr_in(0.7);
        in.aim_dx = rad(20.0) / kCp.aim_sensitivity;
        if (set_scale) in.aim_gain_scale = scale;
        app::tick(st, in, kAp, kCp, nullptr);
        return st.aim.forward();
    };
    const double a_full = ang_from_nose(swept(1.0, true));
    const double a_half = ang_from_nose(swept(0.5, true));
    CHECK(a_full == Catch::Approx(rad(20.0)).margin(1e-6));
    CHECK(a_half == Catch::Approx(rad(10.0)).margin(1e-6));  // 2x zoom => half
    // Unset field defaults to 1.0 (bit-identical to scale 1.0) — the superset.
    CHECK(ang_from_nose(swept(0.0, false)) ==
          Catch::Approx(a_full).margin(1e-12));
}

// ===========================================================================
// S-dz-motion app seam (red-team P1-1, the moved-consumer trap): ci.aim_moved
// is the ONE forwarded field the whole mechanism rides — deleting its
// assignment in app/instructor_tick.h left the entire 251-suite green while
// the shipped app flew the legacy latch. Pin it THROUGH app::tick: a
// SUB-THRESHOLD mouse delta (aim step << deadzone_lo, so the error cannot
// cross deadzone_hi and unlatch by the legacy clause — the bit is the only
// path) must unlatch the tick it lands; the SAME delta under freelook (mouse
// -> camera, CQ2 gate) must NOT count as motion.
// ===========================================================================
TEST_CASE("app::tick: aim_moved reaches control::step; freelook never counts") {
    // Self-armed (Fly-7): the seam pin must not depend on the shipped dial —
    // rest_dwell is Chad's feel A/B and may sit at 0 (legacy).
    control::ControllerParams gated = kCp;
    gated.deadzone_rest_dwell = 0.15;
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    // A delta whose aim step is ~20% of deadzone_lo: legacy could never
    // unlatch on it (err stays far below hi) — only the forwarded bit can.
    const double tiny_dx = 0.2 * gated.deadzone_lo / gated.aim_sensitivity;

    // Settle until latched with the error well inside the circle, so the
    // moved tick's err (parked + tiny step) stays below hi by construction.
    auto settle_latched = [&](app::LoopState& st) {
        app::TickResult r{};
        for (int i = 0; i < 4800; ++i) {
            r = app::tick(st, instr_in(thr), kAp, gated, nullptr);
            if (r.telem.deadzoned && r.telem.e < 0.5 * gated.deadzone_lo)
                return true;
        }
        return false;
    };

    app::LoopState st = flying(s0);
    REQUIRE(settle_latched(st));
    app::TickInput mv = instr_in(thr);
    mv.aim_dx = tiny_dx;
    CHECK_FALSE(app::tick(st, mv, kAp, gated, nullptr)
                    .telem.deadzoned);  // bit unlatches

    app::LoopState st2 = flying(s0);
    REQUIRE(settle_latched(st2));
    app::TickInput fl = instr_in(thr);
    fl.freelook_held = true;
    fl.aim_dx = tiny_dx;  // mouse -> camera: NOT aim motion
    CHECK(app::tick(st2, fl, kAp, gated, nullptr).telem.deadzoned);
}

// ===========================================================================
// R5e ESCAPE CLAIM (Chad fly-4: "I want no return to be at 4000m at 100m/s")
// — crossing E_spec = 0 with the GravityField live severs the plant inputs
// THAT tick and latches until respawn / field-off. The frozen path (env or
// grav null) must be structurally unable to claim.

namespace {

const sim::GravityField kGravField{};  // the 3.B defaults (5000 / 1500)
sim::Environment grav_env() {
    sim::Environment e;
    e.grav = &kGravField;
    return e;
}
const sim::Environment kGravEnv = grav_env();

const glm::dvec3 kEscDir = glm::normalize(glm::dvec3{0.41, -0.55, 0.62});

// Radial climb ABOVE the escape line: binding(6000) = K*erfc(1000/(1500*sqrt2))
// ~ 9313 J/kg at the defaults; 200 m/s carries E ~ +10.7 kJ/kg.
sim::SimState escaped_state() {
    sim::SimState s;
    s.position = kEscDir * (kAp.R + 6000.0);
    s.velocity = kEscDir * 200.0;
    return s;
}

// Ordinary fight-band flight: E ~ -38 kJ/kg (deep bound).
sim::SimState bound_state() {
    sim::SimState s;
    s.position = kEscDir * (kAp.R + 2000.0);
    s.velocity =
        140.0 * glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, kEscDir));
    return s;
}

app::TickInput raw_full_stick() {
    app::TickInput in;
    in.raw_mode = true;
    in.raw_in.pitch = 1.0f;
    in.raw_in.roll = -1.0f;
    in.raw_in.throttle = 1.0f;
    return in;
}

}  // namespace

TEST_CASE("R5e: crossing the escape line severs the stick that same tick") {
    // Fixture honesty: the state is genuinely past the line...
    REQUIRE(sim::specific_energy(200.0, 6000.0, &kGravEnv, kAp) > 0.0);
    app::LoopState st = flying(escaped_state());
    const app::TickResult res =
        app::tick(st, raw_full_stick(), kAp, kCp, &kGravEnv);
    CHECK(res.escape_claimed);
    CHECK(st.escape_claimed);
    // The plant flew NEUTRAL inputs despite a full stick (dead stick,
    // throttle-down — the sever, not a display).
    CHECK(res.inputs.pitch == 0.0f);
    CHECK(res.inputs.roll == 0.0f);
    CHECK(res.inputs.yaw == 0.0f);
    CHECK(res.inputs.throttle == 0.0f);
    // ...and the SAME state/stick with the field OFF flies the command (the
    // sever is the field's, not the fixture's — fixture-no-op guard).
    app::LoopState st_null = flying(escaped_state());
    const app::TickResult res_null =
        app::tick(st_null, raw_full_stick(), kAp, kCp, nullptr);
    CHECK_FALSE(res_null.escape_claimed);
    CHECK(res_null.inputs.pitch == 1.0f);
}

TEST_CASE(
    "R5e: the claim LATCHES -- falling back bound does not free the "
    "plane") {
    app::LoopState st = flying(bound_state());
    st.escape_claimed = true;  // claimed earlier in this life
    REQUIRE(sim::specific_energy(glm::length(st.curr.velocity), 2000.0,
                                 &kGravEnv, kAp) < 0.0);  // now bound again
    const app::TickResult res =
        app::tick(st, raw_full_stick(), kAp, kCp, &kGravEnv);
    CHECK(res.escape_claimed);           // still the sky's
    CHECK(res.inputs.pitch == 0.0f);     // still severed
    CHECK(res.inputs.throttle == 0.0f);  // engine stays surrendered
}

TEST_CASE("R5e: instructor mode is severed too") {
    app::LoopState st = flying(escaped_state());
    app::TickInput in = instr_in(0.9);  // pilot demands power
    const app::TickResult res = app::tick(st, in, kAp, kCp, &kGravEnv);
    CHECK(res.escape_claimed);
    CHECK(res.inputs.throttle == 0.0f);  // the cascade's output is discarded
    CHECK(res.inputs.pitch == 0.0f);
}

TEST_CASE("R5e: respawn and field-off both clear the claim") {
    // Respawn: the GROUNDED pairing tick starts the fresh life unclaimed.
    app::LoopState st = flying(bound_state());
    st.escape_claimed = true;
    st.grounded = true;  // this tick is the spawn tick
    app::tick(st, raw_full_stick(), kAp, kCp, &kGravEnv);
    CHECK_FALSE(st.escape_claimed);
    // Field-off (the T rescue): claim clears and the stick flows again.
    app::LoopState st2 = flying(bound_state());
    st2.escape_claimed = true;
    const app::TickResult res2 =
        app::tick(st2, raw_full_stick(), kAp, kCp, nullptr);
    CHECK_FALSE(st2.escape_claimed);
    CHECK(res2.inputs.pitch == 1.0f);
}

TEST_CASE("R5e: bound flight with the field LIVE never claims") {
    app::LoopState st = flying(bound_state());
    const app::TickResult res =
        app::tick(st, raw_full_stick(), kAp, kCp, &kGravEnv);
    CHECK_FALSE(res.escape_claimed);
    CHECK(res.inputs.pitch == 1.0f);  // the stick flows below the line
}
