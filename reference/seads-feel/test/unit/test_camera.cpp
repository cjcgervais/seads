// Chase-camera pose (SPEC §15.3 / §9.2, Section-3 contract): camera-up is
// the RAW local_up recomputed from the interpolated position — never cached,
// never a fixed axis — and the pose stays a valid LookAt basis everywhere,
// axis "poles" and vertical flight included, with the eye always above the
// surface. Pure math, no window.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "render/camera.h"
#include "sim/params.h"
#include "sim/state.h"
#include "sim/world.h"

namespace {

sim::AircraftParams world_params() {
    sim::AircraftParams p;
    p.R = 15000.0;
    return p;
}

// Body->world orientation from a desired nose (-Z_b) and body-up (+Y_b).
glm::dquat attitude(const glm::dvec3& nose, const glm::dvec3& body_up) {
    const glm::dvec3 z = -glm::normalize(nose);
    const glm::dvec3 y = glm::normalize(body_up);
    const glm::dvec3 x = glm::normalize(glm::cross(y, z));
    return glm::normalize(glm::quat_cast(glm::dmat3{x, y, z}));
}

// Any tangent direction at a position (test helper only — the tree itself
// never needs "any tangent"; this constructs attitudes to aim the nose).
glm::dvec3 some_tangent(const glm::dvec3& up) {
    const glm::dvec3 seed =
        std::abs(up.x) < 0.9 ? glm::dvec3{1, 0, 0} : glm::dvec3{0, 1, 0};
    return glm::normalize(glm::cross(up, seed));
}

bool finite(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// What raylib's LookAt needs to survive: view and up-hint not parallel.
void require_valid_pose(const render::CameraPose& pose) {
    REQUIRE(finite(pose.eye));
    REQUIRE(finite(pose.target));
    REQUIRE(finite(pose.up));
    const glm::dvec3 offset = pose.target - pose.eye;
    REQUIRE(glm::length(offset) > 1e-9);
    const glm::dvec3 view = glm::normalize(offset);
    REQUIRE(std::abs(glm::length(pose.up) - 1.0) < 1e-9);
    REQUIRE(glm::length(glm::cross(view, pose.up)) > 1e-3);
}

const std::vector<glm::dvec3> kSurfaceDirs = {
    {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},    {0, -1, 0},  {0, 0, 1},
    {0, 0, -1}, {1, 1, 1},  {-1, 2, 0.5}, {0.3, -1, 2}};

}  // namespace

TEST_CASE("camera: up is raw local_up recomputed per pose, everywhere",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    for (const glm::dvec3& dir : kSurfaceDirs) {
        sim::SimState s;
        s.position = glm::normalize(dir) * (p.R + 2000.0);
        const glm::dvec3 up = sim::local_up(s.position);
        s.orientation = attitude(some_tangent(up), up);  // level flight
        const render::CameraPose pose = render::chase_camera(s, p, chase);
        require_valid_pose(pose);
        // Level flight is nowhere near the degenerate cone: the up hint
        // must be EXACTLY the recomputed local_up — no cache, no blend, no
        // world axis (the axis-aligned positions above catch any of them).
        REQUIRE(glm::dot(pose.up, up) > 1.0 - 1e-12);
        REQUIRE(pose.target == s.position);
    }
}

TEST_CASE("camera: banked attitude keeps up = local_up, NOT body up",
          "[render][camera]") {
    // The mutation this section's gate exists to forbid: a camera that
    // banks with the airframe (up built from body axes, or any blend of
    // body up into the hint). Level-flight cases can't see it — there
    // body_up == local_up. A 60-degree bank separates them: require the
    // hint to be EXACTLY the recomputed local_up and measurably far from
    // body up. (Red-team P1: the suite passed with {body_up, up, nose}
    // preference order until this case existed.)
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    for (const glm::dvec3& dir : kSurfaceDirs) {
        sim::SimState s;
        s.position = glm::normalize(dir) * (p.R + 2500.0);
        const glm::dvec3 up = sim::local_up(s.position);
        const glm::dvec3 nose = some_tangent(up);
        const glm::dvec3 wing_right = glm::cross(nose, up);  // unit: nose+up
        const double bank = glm::pi<double>() / 3.0;         // 60 deg right
        const glm::dvec3 body_up =
            std::cos(bank) * up + std::sin(bank) * wing_right;
        s.orientation = attitude(nose, body_up);
        const render::CameraPose pose = render::chase_camera(s, p, chase);
        require_valid_pose(pose);
        REQUIRE(glm::dot(pose.up, up) > 1.0 - 1e-12);
        REQUIRE(glm::dot(pose.up, body_up) < 0.9);
    }
}

TEST_CASE("camera: pose stays valid through the vertical-flight pole",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    for (const glm::dvec3& dir : kSurfaceDirs) {
        sim::SimState s;
        s.position = glm::normalize(dir) * (p.R + 3000.0);
        const glm::dvec3 up = sim::local_up(s.position);
        const glm::dvec3 tangent = some_tangent(up);
        // Straight up, straight down, and a sweep through the cone edge.
        for (double pitch_deg : {90.0, -90.0, 88.0, -88.0, 89.9, 60.0}) {
            const double a = pitch_deg * glm::pi<double>() / 180.0;
            const glm::dvec3 nose = std::cos(a) * tangent + std::sin(a) * up;
            const glm::dvec3 body_up =
                std::abs(pitch_deg) > 89.0
                    ? tangent * (pitch_deg > 0 ? -1.0 : 1.0)
                    : glm::normalize(up - nose * glm::dot(up, nose));
            sim::SimState v = s;
            v.orientation = attitude(nose, body_up);
            require_valid_pose(render::chase_camera(v, p, chase));
        }
    }
}

TEST_CASE("camera: eye never enters the planet (SPEC 9.2 clamp)",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    const glm::dvec3 up{1, 0, 0};
    const glm::dvec3 tangent{0, 0, -1};

    SECTION("vertical climb at 5 m puts the naive eye underground") {
        sim::SimState s;
        s.position = up * (p.R + 5.0);
        s.orientation = attitude(up, -tangent);  // nose straight up
        const render::CameraPose pose = render::chase_camera(s, p, chase);
        require_valid_pose(pose);
        REQUIRE(glm::length(pose.eye) >= p.R + chase.min_eye_altitude - 1e-6);
    }

    SECTION(
        "aircraft below the margin itself: pose still finite and above "
        "ground") {
        sim::SimState s;
        s.position = up * (p.R + 1.0);  // margin is 2 m; crash is at 0
        s.orientation = attitude(up, -tangent);
        const render::CameraPose pose = render::chase_camera(s, p, chase);
        require_valid_pose(pose);
        REQUIRE(glm::length(pose.eye) > p.R);
    }

    SECTION("level low pass keeps the full offset when it already clears") {
        sim::SimState s;
        s.position = up * (p.R + 500.0);
        s.orientation = attitude(tangent, up);
        const render::CameraPose pose = render::chase_camera(s, p, chase);
        require_valid_pose(pose);
        const glm::dvec3 expected =
            s.position - tangent * chase.distance + up * chase.height;
        REQUIRE(glm::length(pose.eye - expected) < 1e-9);
    }
}

TEST_CASE("camera: pure function of state (same in, same out)",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    sim::SimState s;
    s.position = glm::normalize(glm::dvec3{0.2, -0.7, 0.4}) * (p.R + 1200.0);
    const glm::dvec3 up = sim::local_up(s.position);
    s.orientation = attitude(some_tangent(up), up);
    const render::CameraPose a = render::chase_camera(s, p, chase);
    const render::CameraPose b = render::chase_camera(s, p, chase);
    REQUIRE(a.eye == b.eye);
    REQUIRE(a.target == b.target);
    REQUIRE(a.up == b.up);
}

// ---------------------------------------------------------------------------
// The §9.2 frame-carried aim camera: up = the carried aim_up (RAW), rotation
// follows the aim 1:1, eye clamped above the surface. Same "which up" trap as
// the raw camera, now on the aim frame instead of the airframe.
// ---------------------------------------------------------------------------
TEST_CASE("aim camera: up is the carried aim_up, NOT local_up",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    for (const glm::dvec3& dir : kSurfaceDirs) {
        sim::SimState s;
        s.position = glm::normalize(dir) * (p.R + 2500.0);
        const glm::dvec3 local_up = sim::local_up(s.position);
        const glm::dvec3 fwd = some_tangent(local_up);
        const glm::dvec3 wing = glm::normalize(glm::cross(fwd, local_up));
        const double bank = 40.0 * glm::pi<double>() / 180.0;
        // aim frame banked 40 deg off the horizon: aim_up separates from
        // local_up (the mutation a body/local_up-rebuilt up would hide).
        const glm::dvec3 aim_up =
            std::cos(bank) * local_up + std::sin(bank) * wing;
        const render::CameraPose pose =
            render::aim_chase_camera(s, fwd, aim_up, p, chase);
        require_valid_pose(pose);
        REQUIRE(glm::dot(pose.up, aim_up) > 1.0 - 1e-9);  // carried aim_up
        REQUIRE(glm::dot(pose.up, local_up) < 0.9);       // NOT local_up
    }
}

TEST_CASE("aim camera: reduces to the raw chase when aim = nose, up = local_up",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    sim::SimState s;
    s.position = glm::normalize(glm::dvec3{0.3, 1.0, -0.4}) * (p.R + 1800.0);
    const glm::dvec3 up = sim::local_up(s.position);
    const glm::dvec3 nose = some_tangent(up);
    s.orientation = attitude(nose, up);
    const render::CameraPose raw = render::chase_camera(s, p, chase);
    const render::CameraPose aim =
        render::aim_chase_camera(s, nose, up, p, chase);
    REQUIRE(glm::length(aim.eye - raw.eye) < 1e-9);
    REQUIRE(glm::dot(aim.up, raw.up) > 1.0 - 1e-12);
}

TEST_CASE("aim camera: rotation is raw 1:1, deterministic, no smoothing",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    sim::SimState s;
    s.position = glm::normalize(glm::dvec3{1.0, 0.2, 0.1}) * (p.R + 2000.0);
    const glm::dvec3 up = sim::local_up(s.position);
    const glm::dvec3 fwd_a = some_tangent(up);
    const glm::dvec3 wing = glm::normalize(glm::cross(fwd_a, up));
    // A different aim (yawed 25 deg): the view must swing with it THIS call —
    // a pure function has no lag state to smooth through.
    const double a = 25.0 * glm::pi<double>() / 180.0;
    const glm::dvec3 fwd_b =
        glm::normalize(std::cos(a) * fwd_a + std::sin(a) * wing);

    const render::CameraPose pa =
        render::aim_chase_camera(s, fwd_a, up, p, chase);
    const render::CameraPose pa2 =
        render::aim_chase_camera(s, fwd_a, up, p, chase);
    const render::CameraPose pb =
        render::aim_chase_camera(s, fwd_b, up, p, chase);
    REQUIRE(pa.eye == pa2.eye);  // deterministic
    const glm::dvec3 view_a = glm::normalize(pa.target - pa.eye);
    const glm::dvec3 view_b = glm::normalize(pb.target - pb.eye);
    REQUIRE(glm::length(view_a - view_b) > 0.1);  // moved with the aim, at once
}

TEST_CASE("aim camera: eye never enters the planet", "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    const glm::dvec3 up{1, 0, 0};
    sim::SimState s;
    s.position = up * (p.R + 5.0);
    const render::CameraPose pose =
        render::aim_chase_camera(s, up, {0, 0, -1}, p, chase);
    require_valid_pose(pose);
    REQUIRE(glm::length(pose.eye) >= p.R + chase.min_eye_altitude - 1e-6);
}

// ---------------------------------------------------------------------------
// Reticle/nose projection math (SPEC §9.2): the on-screen gap between the aim
// and the nose IS the controller error. Pin the signs and the aim==nose null.
// ---------------------------------------------------------------------------
TEST_CASE("project_dir: aim==nose projects to the same point (zero gap)",
          "[render][camera]") {
    const glm::dvec3 cf{0, 0, -1}, cu{0, 1, 0};
    const double fovy = 60.0 * glm::pi<double>() / 180.0;
    const glm::dvec3 aim = glm::normalize(glm::dvec3{0.05, 0.03, -1.0});
    const render::ScreenPoint pa =
        render::project_dir(aim, cf, cu, fovy, 16.0 / 9.0);
    const render::ScreenPoint pb =
        render::project_dir(aim, cf, cu, fovy, 16.0 / 9.0);
    REQUIRE(pa.in_front);
    REQUIRE(pa.x == Catch::Approx(pb.x));
    REQUIRE(pa.y == Catch::Approx(pb.y));
}

TEST_CASE("project_dir: right/up/forward/behind signs", "[render][camera]") {
    const glm::dvec3 cf{0, 0, -1}, cu{0, 1, 0};
    const double fovy = 60.0 * glm::pi<double>() / 180.0;
    const double aspect = 16.0 / 9.0;

    // Forward -> screen center.
    const render::ScreenPoint c = render::project_dir(cf, cf, cu, fovy, aspect);
    REQUIRE(c.in_front);
    REQUIRE(c.x == Catch::Approx(0.0).margin(1e-12));
    REQUIRE(c.y == Catch::Approx(0.0).margin(1e-12));

    // Aim to the right of the nose -> reticle x greater (rightward on screen).
    const glm::dvec3 nose = cf;
    const glm::dvec3 aim_right =
        glm::normalize(glm::dvec3{std::sin(0.1), 0.0, -std::cos(0.1)});
    REQUIRE(render::project_dir(aim_right, cf, cu, fovy, aspect).x >
            render::project_dir(nose, cf, cu, fovy, aspect).x);

    // Aim above the nose -> reticle y greater (up on screen, NDC +y = up).
    const glm::dvec3 aim_up =
        glm::normalize(glm::dvec3{0.0, std::sin(0.1), -std::cos(0.1)});
    REQUIRE(render::project_dir(aim_up, cf, cu, fovy, aspect).y >
            render::project_dir(nose, cf, cu, fovy, aspect).y);

    // Behind the camera -> flagged, not projected.
    REQUIRE_FALSE(
        render::project_dir({0, 0, 1}, cf, cu, fovy, aspect).in_front);
}

// ---------------------------------------------------------------------------
// ease_chase_forward (S7-cam Phase 1): the decoupled camera-forward eases
// toward a velocity-anchored rest target that leans `lead` toward the aim; the
// follow rate grows with deflection (faster catch on a big gap). PURE,
// unit-length, no NaN at v||aim or already-arrived. The RA9 rubber-band
// guarantee is STRUCTURAL (the aim never reads this function) — here we only
// pin the easing math.
// ---------------------------------------------------------------------------
namespace {
double ang_between(const glm::dvec3& a, const glm::dvec3& b) {
    return std::acos(
        std::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0));
}
double rad(double deg) { return deg * glm::pi<double>() / 180.0; }
}  // namespace

TEST_CASE("ease_chase_forward: no deflection rests on velocity",
          "[render][camera]") {
    const glm::dvec3 v{0, 0, -1};
    const double dt = 1.0 / 60.0;
    // aim == velocity: a cam already on velocity stays put (no NaN at v||aim).
    const glm::dvec3 held =
        render::ease_chase_forward(v, v, v, 0.6, 3.0, 4.0, dt);
    REQUIRE(ang_between(held, v) < 1e-9);
    REQUIRE(glm::length(held) == Catch::Approx(1.0));
    // A cam offset from velocity eases back onto it (aim==vel -> target==vel).
    glm::dvec3 off = glm::normalize(glm::dvec3{0.3, 0.0, -1.0});
    for (int i = 0; i < 600; ++i)
        off = render::ease_chase_forward(off, v, v, 0.6, 3.0, 4.0, dt);
    REQUIRE(ang_between(off, v) < 1e-3);  // converged onto the velocity anchor
}

TEST_CASE("ease_chase_forward: settles between velocity and aim by lead",
          "[render][camera]") {
    const glm::dvec3 v{0, 0, -1};
    const double defl = 0.5;  // aim 0.5 rad off velocity
    const glm::dvec3 aim =
        glm::normalize(glm::dvec3{std::sin(defl), 0.0, -std::cos(defl)});
    const double lead = 0.6, dt = 1.0 / 60.0;
    glm::dvec3 cam = v;
    for (int i = 0; i < 4000; ++i)
        cam = render::ease_chase_forward(cam, v, aim, lead, 3.0, 4.0, dt);
    // Rest target sits lead*deflection from velocity, (1-lead)*deflection from
    // the aim — behind velocity, leaning toward the aim.
    REQUIRE(ang_between(cam, v) == Catch::Approx(lead * defl).margin(2e-3));
    REQUIRE(ang_between(cam, aim) ==
            Catch::Approx((1.0 - lead) * defl).margin(2e-3));
    REQUIRE(glm::length(cam) == Catch::Approx(1.0));
}

TEST_CASE("ease_chase_forward: bigger deflection catches faster (rate grows)",
          "[render][camera]") {
    const glm::dvec3 v{0, 0, -1};
    const double dt = 1.0 / 60.0;
    const glm::dvec3 aim_small =
        glm::normalize(glm::dvec3{std::sin(0.1), 0.0, -std::cos(0.1)});
    const glm::dvec3 aim_big =
        glm::normalize(glm::dvec3{std::sin(0.8), 0.0, -std::cos(0.8)});
    // One step from cam==velocity: both are rate-limited (gap > rate*dt), so
    // the angle moved this tick IS rate*dt = (lag_base +
    // lag_gain*deflection)*dt — strictly larger for the bigger deflection.
    const glm::dvec3 c_small =
        render::ease_chase_forward(v, v, aim_small, 0.6, 3.0, 4.0, dt);
    const glm::dvec3 c_big =
        render::ease_chase_forward(v, v, aim_big, 0.6, 3.0, 4.0, dt);
    REQUIRE(ang_between(c_big, v) > ang_between(c_small, v));
}

TEST_CASE("ease_chase_forward: antiparallel stays finite and unit (no NaN)",
          "[render][camera]") {
    const double dt = 1.0 / 60.0;
    const glm::dvec3 v{0, 0, -1};
    auto finite_unit = [](const glm::dvec3& d) {
        return std::isfinite(d.x) && std::isfinite(d.y) && std::isfinite(d.z) &&
               std::abs(glm::length(d) - 1.0) < 1e-9;
    };
    // aim directly ASTERN of velocity (defl == pi): the lean axis is undefined
    // — the old code did normalize(cross(v,-v)) = NaN, poisoning the carried
    // cam.
    REQUIRE(
        finite_unit(render::ease_chase_forward(v, v, -v, 0.6, 3.0, 4.0, dt)));
    // cam exactly OPPOSITE the (velocity) rest target (gap == pi).
    REQUIRE(
        finite_unit(render::ease_chase_forward(-v, v, v, 0.6, 3.0, 4.0, dt)));
    // Near-antiparallel (a tailslide vel_dir flip that isn't exactly pi): still
    // finite/unit, no ill-conditioned-axis spin blow-up.
    const glm::dvec3 near_anti = glm::normalize(glm::dvec3{1e-4, 0.0, 1.0});
    REQUIRE(finite_unit(
        render::ease_chase_forward(near_anti, v, -v, 0.6, 3.0, 4.0, dt)));
}

// ---------------------------------------------------------------------------
// RMB-zoom (2026-07-07) pure pieces: blend_toward (the fov-amount ease) and
// blend_forward (the mouse-aim re-center that points the eye at the pipper).
// Both are cosmetic / DOWNSTREAM of the aim (RA9) — pinned here as math.
// ---------------------------------------------------------------------------
TEST_CASE("blend_toward: eases monotonically toward target, settles",
          "[render][camera][zoom]") {
    const double dt = 1.0 / 60.0, tau = 0.08;
    double x = 0.0;
    double prev = -1.0;
    for (int i = 0; i < 300; ++i) {
        const double next = render::blend_toward(x, 1.0, tau, dt);
        REQUIRE(next >= x);    // monotone up toward 1
        REQUIRE(next <= 1.0);  // never overshoots
        prev = x;
        x = next;
    }
    (void)prev;
    REQUIRE(x == Catch::Approx(1.0).margin(1e-3));  // settled
    // And back down toward 0.
    for (int i = 0; i < 300; ++i) x = render::blend_toward(x, 0.0, tau, dt);
    REQUIRE(x == Catch::Approx(0.0).margin(1e-3));
    // dt <= 0 is a no-op (no time passed); tau <= 0 snaps.
    REQUIRE(render::blend_toward(0.3, 1.0, tau, 0.0) == Catch::Approx(0.3));
    REQUIRE(render::blend_toward(0.3, 1.0, 0.0, dt) == Catch::Approx(1.0));
    // One step moves the expected exponential fraction (mutation on 1-exp
    // dies).
    REQUIRE(render::blend_toward(0.0, 1.0, tau, dt) ==
            Catch::Approx(1.0 - std::exp(-dt / tau)));
}

TEST_CASE("blend_forward: rotates cam_fwd toward the aim by fraction t",
          "[render][camera][zoom]") {
    const glm::dvec3 cam = glm::normalize(glm::dvec3{0.0, 0.0, -1.0});
    const glm::dvec3 aim =  // 50 deg off cam, in the x-z plane
        glm::normalize(
            glm::dvec3{std::sin(rad(50.0)), 0.0, -std::cos(rad(50.0))});
    // t = 0 => cam_fwd unchanged; t = 1 => exactly the aim.
    REQUIRE(ang_between(render::blend_forward(cam, aim, 0.0), cam) < 1e-9);
    REQUIRE(ang_between(render::blend_forward(cam, aim, 1.0), aim) < 1e-9);
    // A fraction lands proportionally along the arc (great-circle interp).
    const glm::dvec3 half = render::blend_forward(cam, aim, 0.5);
    REQUIRE(ang_between(half, cam) ==
            Catch::Approx(0.5 * rad(50.0)).margin(1e-9));
    REQUIRE(glm::length(half) == Catch::Approx(1.0));
    // t clamps to [0,1] (an eased amount can't overshoot the aim).
    REQUIRE(ang_between(render::blend_forward(cam, aim, 1.7), aim) < 1e-9);
    // Antiparallel (aim directly astern of cam): normalize(cross) = NaN in the
    // naive form — a carried render_fwd would be poisoned. Guarded => cam back.
    auto finite_unit = [](const glm::dvec3& d) {
        return std::isfinite(d.x) && std::isfinite(d.y) && std::isfinite(d.z) &&
               std::abs(glm::length(d) - 1.0) < 1e-9;
    };
    REQUIRE(finite_unit(render::blend_forward(cam, -cam, 1.0)));
    REQUIRE(finite_unit(render::blend_forward(cam, cam, 1.0)));  // parallel
}

// ---------------------------------------------------------------------------
// (S7-cam3, 2026-07-07 removed render::ease_level_up: the camera-up is no
// longer horizon-locked — it is the CARRIED aim-frame up, so the camera shows
// the world from the aim/mouse frame and mouse-up == screen-up at any attitude.
// The forwarding of that carried up is pinned in test_instructor_tick
// ("app::instructor_camera: forwards the carried aim-up") and the no-invert
// behavior in the mouse-loop legs there. No leveling math remains to
// unit-test.)
// ---------------------------------------------------------------------------
TEST_CASE(
    "project_dir: cam_up parallel cam_fwd stays finite (no doubled reticle)",
    "[render][camera]") {
    // At the zenith a degenerate basis could hand project_dir a cam_up ∥
    // cam_fwd; without the guard normalize(cross) is NaN -> (int)NaN pixels ->
    // the phantom/doubled nose+aim reticles Chad saw at the top of a loop. The
    // marker directions must still project to FINITE ndc. Mutation: drop the
    // guard -> NaN -> these REQUIREs fail.
    const double fovy = rad(60.0), aspect = 16.0 / 9.0;
    const glm::dvec3 cf{0, 0, 1};
    const glm::dvec3 cu = cf;  // degenerate: camera-up ∥ camera-forward
    for (const glm::dvec3& d : {glm::dvec3{0, 0, 1}, glm::dvec3{0.2, 0.1, 0.97},
                                glm::dvec3{-0.3, 0.4, 0.87}}) {
        const render::ScreenPoint p =
            render::project_dir(d, cf, cu, fovy, aspect);
        REQUIRE(std::isfinite(p.x));
        REQUIRE(std::isfinite(p.y));
    }
}

// ---------------------------------------------------------------------------
// The DECOUPLED camera-up (S7-cam): aim_chase_camera now receives a forward
// that is NOT aim.forward(), and re-orthogonalizes the carried aim-up against
// it (Gram-Schmidt). Pin that the up (a) stays finite/unit, (b) IS the carried
// up re-orthogonalized — never a local_up rebuild (the zenith pattern §9.2
// bans), (c) keeps the carried-up roll (holonomy survives the projection). The
// existing instructor_camera test passes cam_forward == aim.forward()
// (Gram-Schmidt a no-op), so it never exercised this — the whole point of
// S7-cam.
// ---------------------------------------------------------------------------
TEST_CASE("aim_chase_camera: up re-orthogonalizes against a decoupled forward",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    sim::SimState s;
    const glm::dvec3 up_dir = glm::normalize(glm::dvec3{1.0, 0.2, 0.1});
    s.position = up_dir * (p.R + 2000.0);
    s.orientation = attitude(some_tangent(up_dir), up_dir);
    const glm::dvec3 lu = sim::local_up(s.position);

    // Carried aim-up ROLLED ~40 deg off local_up (holonomy-bearing); aim
    // forward level; camera-forward LAGGED ~35 deg off the aim forward (the
    // decoupling).
    const glm::dvec3 aim_fwd = some_tangent(lu);
    const glm::dvec3 wing = glm::normalize(glm::cross(aim_fwd, lu));
    const double roll = 40.0 * glm::pi<double>() / 180.0;
    const glm::dvec3 aim_up =
        glm::normalize(std::cos(roll) * lu + std::sin(roll) * wing);
    const double lag = 35.0 * glm::pi<double>() / 180.0;
    const glm::dvec3 cam_fwd =
        glm::normalize(std::cos(lag) * aim_fwd + std::sin(lag) * wing);

    const render::CameraPose pose =
        render::aim_chase_camera(s, cam_fwd, aim_up, p, chase);

    REQUIRE((std::isfinite(pose.up.x) && std::isfinite(pose.up.y) &&
             std::isfinite(pose.up.z)));
    REQUIRE(glm::length(pose.up) == Catch::Approx(1.0));
    // Derived from the CARRIED up, not rebuilt from local_up (mutation dies):
    // it stays nearer the rolled carried up than local_up.
    REQUIRE(glm::dot(pose.up, aim_up) > glm::dot(pose.up, lu));
    // The carried-up roll survived the re-orthogonalization (not leveled to
    // lu).
    REQUIRE(glm::dot(pose.up, lu) < 0.9);
}

// ---------------------------------------------------------------------------
// Vertical lens shift (SPEC §9.2 framing): centers the resting reticle without
// rotating the camera. The value CENTERS the level-flight reticle, and the
// off-center frustum realizes it as a pure ndc_y' = ndc_y - shift (no zoom).
// ---------------------------------------------------------------------------
TEST_CASE("lens_shift_ndc centers the resting reticle", "[render][camera]") {
    const double h = 9.0, d = 28.0;
    const double fovy = 60.0 * glm::pi<double>() / 180.0;
    const double shift = render::lens_shift_ndc(h, d, fovy);
    // Closed form: (height/distance) / tan(fovy/2). Mutation dropping the /tan
    // leaves shift = h/d and the centering CHECK below (reticle ndc != shift)
    // fails.
    CHECK(shift == Catch::Approx((h / d) / std::tan(0.5 * fovy)));

    // The resting reticle: the horizontal aim, viewed by a camera looking DOWN
    // atan(h/d) (as the chase pose does). Its natural NDC-y equals `shift`, so
    // subtracting `shift` lands it at screen center (ndc_y - shift == 0).
    const double theta = std::atan2(h, d);
    const glm::dvec3 cu{0, 1, 0};
    const glm::dvec3 cf =
        glm::normalize(glm::dvec3{0.0, -std::sin(theta), -std::cos(theta)});
    const glm::dvec3 aim{0, 0, -1};  // horizontal level-flight aim
    const render::ScreenPoint r =
        render::project_dir(aim, cf, cu, fovy, 16.0 / 9.0);
    REQUIRE(r.in_front);
    CHECK(r.y == Catch::Approx(shift).margin(1e-9));
    CHECK((r.y - shift) == Catch::Approx(0.0).margin(1e-9));  // centered
}

// RMB-zoom: the lens shift is recomputed at the ZOOMED fov, so the resting
// reticle stays centered at any zoom. A narrower fov => larger NDC shift for
// the same atan(height/distance) tilt (things appear bigger). PURE math only —
// the main.cpp wiring that feeds the current fov (not the fixed 60) into
// lens_shift_ndc is caller glue with no ctest (verified by reading, Fable
// P2-1).
TEST_CASE("lens_shift_ndc grows as the fov narrows (zoom stays centered)",
          "[render][camera][zoom]") {
    const double h = render::ChaseParams{}.height,
                 d = render::ChaseParams{}.distance;
    const double wide = render::kChaseFovyDeg * glm::pi<double>() / 180.0;
    const double zoom = render::kZoomFovyDeg * glm::pi<double>() / 180.0;
    const double shift_wide = render::lens_shift_ndc(h, d, wide);
    const double shift_zoom = render::lens_shift_ndc(h, d, zoom);
    REQUIRE(shift_zoom > shift_wide);  // narrower fov => bigger shift
    // At the zoomed fov the reticle still centers when the shift subtracts.
    const double theta = std::atan2(h, d);
    const glm::dvec3 cu{0, 1, 0};
    const glm::dvec3 cf =
        glm::normalize(glm::dvec3{0.0, -std::sin(theta), -std::cos(theta)});
    const render::ScreenPoint r =
        render::project_dir(glm::dvec3{0, 0, -1}, cf, cu, zoom, 16.0 / 9.0);
    CHECK((r.y - shift_zoom) == Catch::Approx(0.0).margin(1e-9));
}

// The "zoom in on what's in front of my pipper" behavioral pin: when the
// re-center points the eye at the aim (render_fwd = blend_forward(cam_fwd,
// aim_fwd, 1) == aim_fwd), the reticle lands at SCREEN CENTER (ndc x ~ 0, y -
// lens_shift ~ 0). The contrast — recenter OFF (render_fwd = the lagged,
// deflected cam_fwd) — leaves the reticle far off-center. This is what makes
// the zoom magnify the pipper rather than camera-center. (main.cpp caller glue
// has no ctest; this exercises the same pure pieces it composes.)
TEST_CASE("aim_chase_camera: zoom re-center puts the pipper at screen center",
          "[render][camera][zoom]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;
    const double fovy = render::kZoomFovyDeg * glm::pi<double>() / 180.0;
    const double aspect = 16.0 / 9.0;

    const glm::dvec3 up_dir = glm::normalize(glm::dvec3{1.0, 0.2, 0.1});
    sim::SimState s;
    s.position = up_dir * (p.R + 2000.0);
    s.orientation = attitude(some_tangent(up_dir), up_dir);
    const glm::dvec3 lu = sim::local_up(s.position);

    // Aim level; carried aim-up = local_up. The lagged cam_fwd sits 30 deg off
    // the aim toward the wing (the reticle would float far off-center
    // unzoomed).
    const glm::dvec3 aim_fwd = some_tangent(lu);
    const glm::dvec3 wing = glm::normalize(glm::cross(aim_fwd, lu));
    const glm::dvec3 cam_fwd = glm::normalize(std::cos(rad(30.0)) * aim_fwd +
                                              std::sin(rad(30.0)) * wing);
    const double lens =
        render::lens_shift_ndc(chase.height, chase.distance, fovy);

    auto reticle = [&](const glm::dvec3& render_fwd) {
        const render::CameraPose pose =
            render::aim_chase_camera(s, render_fwd, lu, p, chase);
        const glm::dvec3 cf = glm::normalize(pose.target - pose.eye);
        return render::project_dir(aim_fwd, cf, pose.up, fovy, aspect);
    };

    // Fully re-centered (t = 1): render_fwd == aim_fwd => pipper at center.
    const glm::dvec3 recentered = render::blend_forward(cam_fwd, aim_fwd, 1.0);
    const render::ScreenPoint on = reticle(recentered);
    REQUIRE(on.in_front);
    CHECK(std::abs(on.x) < 1e-6);         // horizontally centered
    CHECK(std::abs(on.y - lens) < 1e-6);  // vertically centered after shift

    // Recenter OFF (t = 0): render_fwd == the deflected cam_fwd => far
    // off-center (the contrast that proves the re-center is what centers the
    // pipper).
    const render::ScreenPoint off = reticle(cam_fwd);
    CHECK(std::abs(off.x) > 0.3);
}

TEST_CASE("off_center_frustum shifts the center, preserves the scale",
          "[render][camera]") {
    const double fovy = 60.0 * glm::pi<double>() / 180.0;
    const double aspect = 16.0 / 9.0;
    const double nearZ = 2.0;
    const double th = nearZ * std::tan(0.5 * fovy);

    // shift 0 -> symmetric frustum (unchanged projection).
    const render::FrustumBounds s0 =
        render::off_center_frustum(fovy, aspect, nearZ, 0.0);
    CHECK(s0.t == Catch::Approx(-s0.b));
    CHECK(s0.l == Catch::Approx(-s0.r));
    CHECK(s0.t == Catch::Approx(th));

    // shift s -> vertical SCALE unchanged (t-b == 2*th, no zoom/distortion),
    // horizontal untouched, and the frustum center moved by exactly s
    // ((t+b)/(t-b) == s == ndc_y offset). Mutation biasing the bounds the wrong
    // way changes the scale and trips the first CHECK.
    const double s = 0.4;
    const render::FrustumBounds fs =
        render::off_center_frustum(fovy, aspect, nearZ, s);
    CHECK((fs.t - fs.b) == Catch::Approx(2.0 * th));
    CHECK(fs.l == Catch::Approx(-fs.r));
    CHECK(((fs.t + fs.b) / (fs.t - fs.b)) == Catch::Approx(s));
}

// §6 red-team P1/P2: the freelook camera-up flip pole sits at pi/2 - atan(h/d)
// (~75 deg, NOT 90) on the OVERHEAD side because the resting chase view already
// tilts atan(h/d) down. The overhead cap must hold short of it; the eye-below
// side is safe. This drives the SHIPPED pose through the orbit path (previously
// untested) across the clamp range and confirms the cap is tight.
TEST_CASE("freelook orbit stays clear of the camera-up flip within the cap",
          "[render][camera]") {
    const sim::AircraftParams p = world_params();
    const render::ChaseParams chase;  // distance 34, height 9, degenerate_dot
    sim::SimState s;
    s.position = glm::normalize(glm::dvec3{0.4, 1.0, -0.3}) * (p.R + 2000.0);
    const glm::dvec3 up = sim::local_up(s.position);
    const glm::dvec3 fwd = some_tangent(up);
    s.orientation = attitude(fwd, up);
    // A banked carried up (aim_up separates from local_up, still ⟂ fwd so the
    // re-orthogonalization is a no-op): the flip is attitude-invariant, so a
    // banked frame must be safe across the range too.
    const glm::dvec3 wing = glm::normalize(glm::cross(fwd, up));
    const double bank = 30.0 * glm::pi<double>() / 180.0;
    const glm::dvec3 aim_up = std::cos(bank) * up + std::sin(bank) * wing;

    const double cap = render::freelook_overhead_pitch_cap(
        chase.height, chase.distance, chase.degenerate_dot,
        render::kFreelookPoleMargin);
    REQUIRE(cap > 0.0);

    // Across the shipped clamp [-cap (overhead), +below], the up-hint never
    // flips — pose.up stays the carried aim_up. A cap loosened toward 90 deg
    // would pull the pole (below) into this range and trip the CHECK.
    const double below = 89.0 * glm::pi<double>() / 180.0;
    for (double pitch = -cap; pitch <= below; pitch += 0.02) {
        const render::CameraPose pose = render::aim_chase_camera(
            s, fwd, aim_up, p, chase, render::CameraOrbit{0.0, pitch});
        CHECK(glm::dot(pose.up, aim_up) > 0.99);
    }

    // The cap is TIGHT/meaningful: AT the overhead pole (just past the cap) the
    // guard DOES fire and the up-hint swaps off aim_up.
    const double pole =
        -(1.57079632679489661923 - std::atan2(chase.height, chase.distance));
    const render::CameraPose flipped = render::aim_chase_camera(
        s, fwd, aim_up, p, chase, render::CameraOrbit{0.0, pole});
    CHECK(glm::dot(flipped.up, aim_up) < 0.5);
}

// ===========================================================================
// S-reticle (Chad 2026-07-08): display-only capped exponential ease of the
// reticle draw direction — hides the integer-mouse staircase on slow sweeps.
// Pure helper; the caller-side firewall (its ONLY consumer is the reticle
// projection, FrameInfo.reticle_dir) is by-construction/one-consumer-grep —
// caller glue, honest ledger like the orbit/zoom cosmetics.
// ===========================================================================
TEST_CASE("s-reticle: staircase input is smoothed, flicks stay capped") {
    constexpr double kPi2 = 3.14159265358979323846;
    const auto rad = [](double d) { return d * kPi2 / 180.0; };
    const glm::dvec3 up{0.0, 1.0, 0.0};
    const auto yawed = [&](double a) {
        return glm::normalize(glm::angleAxis(a, up) *
                              glm::dvec3{0.0, 0.0, -1.0});
    };
    const double dt = 1.0 / 120.0;
    const double cap = rad(0.48);  // quant_px 3 * sensitivity 0.16 (shipped)

    SECTION("staircase smoothing: alternating hops become uniform motion") {
        // A slow integer-delta drag at 120 fps: the TARGET jumps a full
        // sensitivity step (0.16 deg) every second frame and holds between —
        // the staircase Chad sees (per-frame input steps ALTERNATE 0.16/0).
        // The jitter is that ALTERNATION, not the mean rate (which must be
        // preserved): the eased output must move nearly UNIFORMLY — every
        // frame steps, none carries the whole hop. (Measured ~0.087 deg
        // uniform vs the 0.16/0 input; this drag rate sits right at the
        // cap/tau boundary, so "less than half" is not the honest bound.)
        glm::dvec3 s = yawed(0.0);
        double target_a = 0.0;
        double max_out_step = 0.0, min_out_step = 1e9;
        glm::dvec3 prev = s;
        for (int f = 0; f < 240; ++f) {
            if (f % 2 == 0) target_a += rad(0.16);
            s = render::reticle_smooth(s, yawed(target_a), dt, cap);
            if (f > 20) {  // warmed up
                const double step =
                    std::acos(std::clamp(glm::dot(prev, s), -1.0, 1.0));
                max_out_step = std::max(max_out_step, step);
                min_out_step = std::min(min_out_step, step);
            }
            prev = s;
        }
        CAPTURE(max_out_step, min_out_step);
        CHECK(max_out_step < 0.7 * rad(0.16));  // no frame carries the hop
        CHECK(min_out_step > 0.3 * rad(0.16));  // every frame moves (uniform)
        // Mutation killed: tau -> 0 (passthrough) alternates the full 0.16
        // hop with a 0 frame — FAILS both bounds.
    }

    SECTION("lag cap binds at flick rates (no macro trailing)") {
        // 100 deg/s sweep: measured IMMEDIATELY POST-CALL the residual gap
        // must sit at/under the cap — the ease has no authority beyond the
        // quantization scale.
        glm::dvec3 s = yawed(0.0);
        double target_a = 0.0;
        for (int f = 0; f < 240; ++f) {
            target_a += rad(100.0) * dt;
            s = render::reticle_smooth(s, yawed(target_a), dt, cap);
            if (f > 20) {
                const double gap = std::acos(
                    std::clamp(glm::dot(s, yawed(target_a)), -1.0, 1.0));
                REQUIRE(gap <= cap + 1e-9);
            }
        }
        // Mutation killed: cap deleted -> steady lag = rate*tau ~ 5 deg.
    }

    SECTION("slow-sweep fidelity: steady lag ~ rate*tau, no overshoot") {
        glm::dvec3 s = yawed(0.0);
        double target_a = 0.0;
        double last_gap = 0.0;
        for (int f = 0; f < 480; ++f) {
            target_a += rad(2.0) * dt;
            s = render::reticle_smooth(s, yawed(target_a), dt, cap);
            last_gap =
                std::acos(std::clamp(glm::dot(s, yawed(target_a)), -1.0, 1.0));
            // Never past the target along the sweep (no overshoot): the
            // smoothed dir's yaw angle stays <= the target's.
            const double s_a = std::atan2(s.x, -s.z);
            REQUIRE(s_a <= target_a + 1e-9);
        }
        // Steady-state lag of a rate ease ~ rate*tau (0.1 deg), << cap.
        CHECK(last_gap < rad(0.2));
        CHECK(last_gap > rad(0.02));
    }

    SECTION(
        "snap safety: parallel, antiparallel, AND the near-antiparallel "
        "live path") {
        const glm::dvec3 s0 = yawed(0.0);
        // Parallel: exact passthrough.
        const glm::dvec3 same = render::reticle_smooth(s0, s0, dt, cap);
        CHECK(glm::dot(same, s0) == Catch::Approx(1.0).margin(1e-12));
        // 90-deg snap: lands within the cap in ONE call.
        const glm::dvec3 snap =
            render::reticle_smooth(s0, yawed(rad(90.0)), dt, cap);
        CHECK(std::acos(std::clamp(glm::dot(snap, yawed(rad(90.0))), -1.0,
                                   1.0)) <= cap + 1e-9);
        // EXACT antiparallel: finite (no NaN through normalize(cross ~ 0) —
        // the S7-cam both-ends trap), and the display SNAPS to the new aim.
        const glm::dvec3 flip = render::reticle_smooth(s0, -s0, dt, cap);
        REQUIRE(std::isfinite(flip.x));
        CHECK(glm::dot(flip, -s0) == Catch::Approx(1.0).margin(1e-9));
        // NEAR-antiparallel (gap = pi - 1e-3): the LIVE rotate path (the
        // exact -s case is absorbed by the snap branch and would leave the
        // rotate math untested — the "accidentally contained" trap mirrored).
        const glm::dvec3 near_t = yawed(kPi2 - 1e-3);
        const glm::dvec3 moved = render::reticle_smooth(s0, near_t, dt, cap);
        REQUIRE(std::isfinite(moved.x));
        const double g0 =
            std::acos(std::clamp(glm::dot(s0, near_t), -1.0, 1.0));
        const double g1 =
            std::acos(std::clamp(glm::dot(moved, near_t), -1.0, 1.0));
        CHECK(g1 < g0);           // moved toward the target
        CHECK(g1 <= cap + 1e-9);  // and the cap clamped the remainder
        // Mutation killed: antiparallel-guard deleted -> NaN at -s0.
    }

    SECTION("dt <= 0 is a no-op (the blend_toward convention)") {
        const glm::dvec3 s0 = yawed(0.0);
        const glm::dvec3 t0 = yawed(rad(30.0));
        const glm::dvec3 z = render::reticle_smooth(s0, t0, 0.0, cap);
        CHECK(glm::dot(z, s0) == Catch::Approx(1.0).margin(1e-12));
        const glm::dvec3 n = render::reticle_smooth(s0, t0, -0.01, cap);
        CHECK(glm::dot(n, s0) == Catch::Approx(1.0).margin(1e-12));
    }
}

// ===========================================================================
// S-keychase's two TEST_CASEs ("the anchor selector" and "a parked aim + a
// turning flight path parks the camera") lived here and are DELETED with the
// mechanism (S-keyprec, SEALED KERNEL v8, 2026-07-29). They pinned a law Chad
// has since ruled out: that override keys re-anchor the camera on the flight
// path. Keeping them would pin retired behavior.
//
// They are NOT replaced by a unit leg, deliberately. The obvious candidate —
// "drive two MiniCamera traces, one with keys held and one without, assert
// cam_fwd bit-identical" — is VACUOUS once keys_flying leaves the signature:
// both arms are the same call with the same arguments, so it passes
// tautologically and can never go red. A re-introduction would arrive with its
// own parameter or read cp, which such a leg exercises neither way. A pin that
// cannot fail is not a pin (this repo's re-fire-every-tick mutant survived a
// whole green suite on exactly that shape).
//
// What holds the line instead:
//   1. `seads_harness comfort` scenario `mouseaim_keys` — the real regression
//      instrument, because it drives actual override_mask through app::tick and
//      measures the camera's lag behind the AIM across a keypress. It read
//      0.000 -> 67.781 deg under S-keychase and must stay continuous.
//   2. harness::MiniCamera::advance taking NO override state — a COMPILE-TIME
//      property. The absence of the parameter is the invariant; see the banner
//      there and the full record in render/camera.h.
// ===========================================================================
