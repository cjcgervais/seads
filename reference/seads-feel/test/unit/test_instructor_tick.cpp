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
        const app::TickResult r = app::tick(loop, instr_in(0.3), kAp, kCp);
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
    const app::TickResult g = app::tick(loop, held, kAp, kCp);
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
        const app::TickResult r = app::tick(loop, raw, kAp, kCp);
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
    app::tick(loop, in, kAp, kCp);
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
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 3000.0, up, heading);
    const double D = 200.0;

    auto release_then_move = [&](double aim_dx) {
        app::LoopState loop = flying(s);
        app::TickInput hold = instr_in(0.7);
        hold.freelook_held = true;
        app::tick(loop, hold, kAp, kCp);  // hold freelook one tick
        app::TickInput rel = instr_in(0.7);
        rel.freelook_held = false;  // RELEASE this tick: ease-back armed
        rel.aim_dx = aim_dx;        // mouse delta during the suspension window
        app::tick(loop, rel, kAp, kCp);
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
    app::tick(loop, in, kAp, kCp);

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
        app::tick(loop, instr_in(thr), kAp, kCp);
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
            roll = app::tick(loop, in, kAp, kCp).inputs.roll;
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
        app::tick(loop, hold, kAp, kCp);  // freelook held one tick
        app::TickInput rel = instr_in(0.7);
        rel.freelook_held = false;
        app::tick(loop, rel, kAp, kCp);  // release: ease-back armed (~300 ms)
        loop.grounded = true;            // a respawn/toggle GROUNDED pairing
        app::tick(loop, instr_in(0.7), kAp, kCp);  // grounded tick: fl.reset()
        app::TickInput fly = instr_in(0.7);
        fly.aim_dx = aim_dx;
        app::tick(loop, fly, kAp, kCp);  // first flight tick: mouse offered
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
        const app::TickResult r = app::tick(st, in, kAp, kCp);
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
// MB-right RE-SCOPE (Chad 2026-07-07, supersedes the 2026-07-06 loop ruling
// for LONG rests): "stays inverted" now holds only WITHIN [auto_level]
// inverted_delay — after ~2 s of belly-up rest the slow righting roll arms
// (pinned in test_cascade "MB-right", incl. the knob-off arm that preserves
// this test's old full-window behavior at inverted_rate = 0). This leg keeps
// the S7-loop-invert mutation coverage inside the window: the un-gated-
// wings-hold mutant (restore F2) rolls from TICK 1, well inside the delay,
// so the within-window asserts still catch it.
TEST_CASE(
    "control::step: an inverted plane at rest STAYS inverted (no auto-right "
    "within the righting delay)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Belly-up: rolled 150 deg about the nose (cos_phi_theta = cos 150 ~ -0.87,
    // clearly inverted), otherwise flying ~level.
    const sim::SimState s = harness::flight_state(
        kAp, 160.0, 4000.0, up, heading, rad(150.0), 0.0, 0.0);
    harness::ClosedLoop cl(s, s.orientation * glm::dvec3{0.0, 0.0, -1.0});

    // Strictly inside the righting delay (a 12-tick margin for the arm edge).
    const int window =
        static_cast<int>(kCp.inverted_delay / kAp.sim_dt + 0.5) - 12;
    // Premise re-derived for the dial inverted_delay 1.0 -> 0.5 s (Chad
    // 2026-07-28; window 108 -> 48 ticks): the un-gated-wings-hold mutant
    // (wings_level_gate := 1.0, restore F2) emits a roll input from TICK 1,
    // so max|roll| < 0.05 kills it inside ANY >= 0.3 s window —
    // MUTATION-RE-VERIFIED at window = 48 on this dial (max|roll| saturates
    // ~1.0 immediately). Below ~36 ticks the cpt_final "stayed inverted"
    // companion loses meaning; a delay dialed under 0.4 s must re-derive.
    REQUIRE(window > 36);
    double cpt0 = 1.0, cpt_final = 1.0, max_roll = 0.0, max_pitch = 0.0;
    for (int i = 0; i < window; ++i) {
        cl.aim_nose();  // REST: aim tracks the nose so err ~ 0
        const control::Telemetry t = cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        REQUIRE_FALSE(t.righting);  // the delay has not elapsed
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
    // those clauses. The knob-ON arms live in test_relorient.cpp (cases 9/9b/9c).
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
    for (int i = 0; i < hold_ticks; ++i) app::tick(loop, hold, kAp, cp);
    app::tick(loop, instr_in(0.7), kAp, cp);  // the release tick
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
        app::tick(loop, in, kAp, cp);
        CHECK(loop.recov.remaining == 0.0);
    }
    CHECK(std::abs(std::abs(misalign(loop)) - m_after) < 1e-6);
}

TEST_CASE(
    "S7-hrz: righting is EDGE-ONLY - a rolled frame is never auto-leveled") {
    // The successor of the open-loop pin, same ban, sharper shape: the ONLY
    // thing that may right the carried frame is the release edge itself. A
    // frame rolled off-horizon DURING mouse-aim flight must stay rolled for
    // as long as the pilot flies — any drift toward level here is the
    // S7-mouselevel banned continuous auto-leveling reappearing. This leg
    // fails under a per-tick recompute mutant (the old F1/P0 class).
    const control::ControllerParams cp = hrz_cp(150.0, 5.0);
    app::LoopState loop = misaligned_loop(120.0);
    freelook_tap(loop, cp);
    REQUIRE(deg(std::abs(misalign(loop))) < 1.0);  // release righted it

    // Roll the frame off-horizon mid-flight (no freelook, no release edge).
    loop.aim.roll_about_forward(rad(60.0));
    const double m1 = std::abs(misalign(loop));
    REQUIRE(deg(m1) > 55.0);
    const app::TickInput in = instr_in(0.7);
    for (int t = 0; t < 3 * 120; ++t) app::tick(loop, in, kAp, cp);
    const double m_end = std::abs(deg(misalign(loop)));
    std::printf("[S7-hrz edge-only] standing misalign after 3 s = %.2f deg\n",
                m_end);
    CHECK(m_end > 55.0);  // nothing rights it without a release edge
    CHECK(loop.recov.remaining == 0.0);
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
        freelook_tap(loop, cp);  // press + release again
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

        app::TickInput ovr = instr_in(0.7);
        ovr.override_mask[2] = true;  // roll axis keyboard jink
        ovr.override_sign[2] = -1.0;
        for (int t = 0; t < 60; ++t) app::tick(loop, ovr, kAp, cp);
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
        for (int t = 0; t < 5; ++t) app::tick(loop, hold_both, kAp, cp);

        app::TickInput key_only = instr_in(0.7);  // Space up, key still down
        key_only.override_mask[0] = true;
        key_only.override_sign[0] = 1.0;
        app::tick(loop, key_only, kAp, cp);  // the release edge, key held
        CHECK(std::abs(deg(misalign(loop))) == Catch::Approx(m0).margin(3.0));

        for (int t = 0; t < 120; ++t) app::tick(loop, in, kAp, cp);  // key up
        // No new edge -> still unrighted (the one-shot-edge semantics).
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
        app::tick(loop, hold, kAp, cp);
        CHECK(loop.recov.remaining == 0.0);
    }
    app::tick(loop, in, kAp, cp);  // the release tick (rights instantly)
    CHECK(loop.recov.remaining == 0.0);
    for (int t = 0; t < 30; ++t) {
        app::tick(loop, in, kAp, cp);
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
    for (int t = 0; t < 120; ++t) app::tick(loop, in, kAp, cp0);
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
    for (int t = 0; t < 60; ++t) app::tick(loop, in, kAp, cp);

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
    for (int t = 0; t < 3; ++t) app::tick(loop, hold, kAp, cp);
    app::TickInput hold_key = hold;  // pitch+yaw override held INSIDE the hold
    hold_key.override_mask[0] = true;
    hold_key.override_sign[0] = 1.0;
    hold_key.override_mask[1] = true;
    hold_key.override_sign[1] = 1.0;
    for (int t = 0; t < 90; ++t) app::tick(loop, hold_key, kAp, cp);
    for (int t = 0; t < 3; ++t) app::tick(loop, hold, kAp, cp);  // keys up

    app::tick(loop, instr_in(0.7), kAp, cp);  // release: snap + instant right
    CHECK(loop.recov.remaining == 0.0);
    CHECK(std::abs(deg(misalign(loop))) < 1.0);  // righted about the NEW fwd
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
        app::tick(on, in, kAp, cp_on);
        app::tick(off, in, kAp, cp_off);
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
        app::tick(loop, in, kAp, cp);
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
    app::tick(loop, sweep, kAp, cp);
    REQUIRE(glm::dot(loop.aim.forward(), nose_dir(loop)) < std::cos(rad(20.0)));

    app::TickInput fl = instr_in(1.0);
    fl.freelook_held = true;  // NO override keys anywhere
    for (int t = 0; t < 120; ++t) {
        app::tick(loop, fl, kAp, cp);
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
        app::tick(loop, in, kAp, cp);
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
        app::tick(loop, in, kAp, cp);
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
        for (int t = 0; t < 60; ++t) app::tick(loop, pull, kAp, cp);
        for (int t = 0; t < 2; ++t) app::tick(loop, hold, kAp, cp);  // key up

        // POISON the sim's held v-hat before the release tick (S4a: a
        // two-copies seam test must poison the copy it claims isn't read —
        // S7-nest red-team P3): the snap must read NOTHING velocity-flavored
        // now, and certainly never SimState.last_vhat (F5 — this feeds a
        // CONTROL input). Kept so a velocity-target regression of EITHER
        // copy (caller's or the sim's held one) fails the nose check below.
        loop.curr.last_vhat = glm::normalize(glm::dvec3{0.3, -0.8, 0.5});
        app::tick(loop, instr_in(1.0), kAp, cp);  // release
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
        for (int t = 0; t < 5; ++t) app::tick(loop, pull, kAp, cp);
        // Manufacture the degenerate state at the release edge: sub-ballistic
        // speed with the velocity DIRECTION well OFF the nose (S7-nest
        // red-team P2: a nose-parallel velocity made the v_ballistic guard
        // mutation-invisible — the bases must SEPARATE for the fall-back-to-
        // NOSE assertion to pin anything; guard deleted => aim lands ~35 deg
        // off the nose and the check below fails).
        loop.curr.velocity =
            5.0 * glm::normalize(nose_dir(loop) + 0.7 * loop.aim.up());
        app::tick(loop, hold, kAp, cp);  // key up (re-pin after the tick)
        loop.curr.velocity =
            5.0 * glm::normalize(nose_dir(loop) + 0.7 * loop.aim.up());
        app::tick(loop, instr_in(0.0), kAp, cp);  // release
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
        for (int t = 0; t < 5; ++t) app::tick(loop, pull, kAp, cp);
        app::tick(loop, hold, kAp, cp);                // key up
        loop.curr.velocity = -100.0 * nose_dir(loop);  // vhat . nose < 0
        app::tick(loop, instr_in(0.0), kAp, cp);       // release
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
        app::tick(loop, mouse, kAp, cp);
        REQUIRE(glm::dot(loop.aim.forward(), nose_dir(loop)) <
                std::cos(rad(20.0)));  // genuinely parked off the nose
        app::TickInput hold = instr_in(1.0);
        hold.freelook_held = true;
        for (int t = 0; t < 10; ++t) app::tick(loop, hold, kAp, cp);
        // The weld put the aim on the nose during the hold.
        CHECK(glm::dot(loop.aim.forward(), nose_dir(loop)) >
              std::cos(rad(1.5)));
        const glm::dvec3 welded = loop.aim.forward();
        app::tick(loop, instr_in(1.0), kAp, cp);  // release, no override used
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
        app::tick(st, in, kAp, kCp);
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
            r = app::tick(st, instr_in(thr), kAp, gated);
            if (r.telem.deadzoned && r.telem.e < 0.5 * gated.deadzone_lo)
                return true;
        }
        return false;
    };

    app::LoopState st = flying(s0);
    REQUIRE(settle_latched(st));
    app::TickInput mv = instr_in(thr);
    mv.aim_dx = tiny_dx;
    CHECK_FALSE(
        app::tick(st, mv, kAp, gated).telem.deadzoned);  // bit unlatches

    app::LoopState st2 = flying(s0);
    REQUIRE(settle_latched(st2));
    app::TickInput fl = instr_in(thr);
    fl.freelook_held = true;
    fl.aim_dx = tiny_dx;  // mouse -> camera: NOT aim motion
    CHECK(app::tick(st2, fl, kAp, gated).telem.deadzoned);
}
