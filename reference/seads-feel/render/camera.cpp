#include "render/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <initializer_list>

#include "sim/world.h"

namespace render {

namespace {

// Pull the eye toward the target along the view axis until its altitude
// clears the margin (SPEC §9.2: the chase camera never enters the planet).
// eye(t) = target + (eye - target) * t, t in [0, 1]; keep the largest t
// with |eye(t)| >= R + margin. f(t) = |eye(t)|^2 - margin_radius^2 is an
// upward parabola: when the full offset fails (f(1) < 0) but the target
// clears (f(0) >= 0), the in-segment crossing is the SMALLER root — the
// larger one lies beyond the eye (t > 1).
glm::dvec3 clamp_eye_above_surface(const glm::dvec3& target,
                                   const glm::dvec3& eye, double min_radius) {
    const glm::dvec3 d = eye - target;
    const double dd = glm::dot(d, d);
    if (dd == 0.0) return eye;
    const double m2 = min_radius * min_radius;
    const double f0 = glm::dot(target, target) - m2;
    const double f1 = glm::dot(eye, eye) - m2;
    if (f1 >= 0.0) return eye;
    if (f0 < 0.0) {
        // The aircraft itself is below the margin (a frame from the
        // crash-reset): no point on the segment clears — keep the higher
        // endpoint; the caller's degenerate-offset guard re-seats a
        // collapsed eye.
        return f0 >= f1 ? target : eye;
    }
    const double ad = glm::dot(target, d);
    const double disc = ad * ad - dd * f0;  // > 0 here (f(1) < 0 <= f(0))
    const double t =
        disc <= 0.0 ? 0.0 : glm::clamp((-ad - std::sqrt(disc)) / dd, 0.0, 1.0);
    return target + d * t;
}

}  // namespace

CameraPose chase_camera(const sim::SimState& state,
                        const sim::AircraftParams& params,
                        const ChaseParams& chase) {
    // Recomputed from position every call (SPEC §6): the sole up source.
    const glm::dvec3 up = sim::local_up(state.position);
    const glm::dvec3 nose = state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 body_up = state.orientation * glm::dvec3{0.0, 1.0, 0.0};

    CameraPose pose;
    pose.target = state.position;
    pose.eye = state.position - nose * chase.distance + up * chase.height;
    pose.eye = clamp_eye_above_surface(pose.target, pose.eye,
                                       params.R + chase.min_eye_altitude);

    // Surface clamp can collapse the offset (aircraft below the margin, one
    // frame before crash-reset): re-seat the eye straight up so the pose
    // stays finite for that frame.
    glm::dvec3 offset = pose.eye - pose.target;
    if (glm::dot(offset, offset) < 1e-12) {
        // |target + up*h| == |target| + h (up is the target's own radial
        // direction), so lift by whatever the margin still needs if a
        // config ever sets min_eye_altitude >= height.
        const double lift_needed =
            params.R + chase.min_eye_altitude - glm::length(pose.target);
        pose.eye = pose.target + up * std::max(chase.height, lift_needed);
        offset = pose.eye - pose.target;
    }

    // Camera-up = raw local_up (the Section-3 contract). Fallbacks apply
    // only inside the near-parallel cone where LookAt's cross(view, up)
    // vanishes: body-up first (vertical flight — the S3 zenith pole), then
    // the nose (surface-clamped view straight down a level aircraft, where
    // body-up is parallel to the view too). body_up and nose are mutually
    // perpendicular, so the chain cannot exhaust.
    const glm::dvec3 view = glm::normalize(-offset);
    pose.up = up;
    for (const glm::dvec3& candidate : {up, body_up, nose}) {
        if (std::abs(glm::dot(view, candidate)) <= chase.degenerate_dot) {
            pose.up = candidate;
            break;
        }
    }
    return pose;
}

glm::dvec3 ease_chase_forward(const glm::dvec3& cam_fwd,
                              const glm::dvec3& vel_dir,
                              const glm::dvec3& aim_fwd, double lead,
                              double lag_base, double lag_gain, double dt) {
    constexpr double kPi = 3.14159265358979323846;
    // A cross-product rotation axis vanishes at BOTH 0 and pi. The near-pi
    // (antiparallel) end is the trap: normalize(cross) there is normalize(~0)
    // -> NaN, and cam_fwd is carried across frames so ONE NaN poisons the
    // camera forever until a reseed. Reachable when vel_dir snaps ~180 in a
    // tailslide (the 1 m/s floor flip). Guard BOTH ends on BOTH rotations; at
    // pi the geodesic is genuinely directionless, so freezing that frame is
    // correct (the reversal resolves off pi on the next tick).
    const auto degenerate = [](double angle) {
        return angle < 1e-9 || angle > kPi - 1e-6;
    };
    const glm::dvec3 v = glm::normalize(vel_dir);
    const glm::dvec3 a = glm::normalize(aim_fwd);
    glm::dvec3 c = glm::normalize(cam_fwd);

    // Deflection: how far the aim is off the velocity vector (the rest anchor).
    const double defl = std::acos(std::clamp(glm::dot(v, a), -1.0, 1.0));

    // Rest target: behind velocity, rotated `lead` of the way toward the aim.
    // At defl -> 0 the target IS velocity; at defl -> pi the aim is directly
    // astern and "lean toward it" is directionless -> rest behind velocity.
    glm::dvec3 target = v;
    if (!degenerate(defl)) {
        const glm::dvec3 axis = glm::normalize(glm::cross(v, a));
        target = glm::normalize(glm::angleAxis(lead * defl, axis) * v);
    }

    // Ease cam_fwd toward the target; the rate grows with deflection so a big
    // gap is caught faster (the user's "not jumpy but determined by the
    // deflection"). Rotate-toward by min(gap, rate*dt) — no overshoot.
    const double gap = std::acos(std::clamp(glm::dot(c, target), -1.0, 1.0));
    if (!degenerate(gap)) {
        const double rate = lag_base + lag_gain * defl;
        const double step = std::min(gap, rate * dt);
        const glm::dvec3 axis = glm::normalize(glm::cross(c, target));
        c = glm::normalize(glm::angleAxis(step, axis) * c);
    }
    return c;
}

double blend_toward(double current, double target, double tau, double dt) {
    if (dt <= 0.0) return current;  // no time passed
    if (tau <= 0.0) return target;  // instant
    return current + (target - current) * (1.0 - std::exp(-dt / tau));
}

glm::dvec3 blend_forward(const glm::dvec3& cam_fwd, const glm::dvec3& aim_fwd,
                         double t) {
    constexpr double kPi = 3.14159265358979323846;
    const glm::dvec3 c = glm::normalize(cam_fwd);
    const glm::dvec3 a = glm::normalize(aim_fwd);
    const double ang = std::acos(std::clamp(glm::dot(c, a), -1.0, 1.0));
    // Directionless at BOTH ends: t*0 at parallel is a no-op anyway, and at
    // antiparallel normalize(cross) is normalize(~0) -> NaN (the ease_chase
    // trap; cam_fwd is carried, so one NaN poisons it). Return cam_fwd.
    if (ang < 1e-9 || ang > kPi - 1e-6) return c;
    const double step = std::clamp(t, 0.0, 1.0) * ang;
    const glm::dvec3 axis = glm::normalize(glm::cross(c, a));
    return glm::normalize(glm::angleAxis(step, axis) * c);
}

glm::dvec3 reticle_smooth(const glm::dvec3& s, const glm::dvec3& target,
                          double dt, double cap_rad) {
    constexpr double kPi = 3.14159265358979323846;
    const glm::dvec3 c = glm::normalize(s);
    const glm::dvec3 a = glm::normalize(target);
    if (dt <= 0.0) return c;  // no time passed (blend_toward convention)
    const double gap = std::acos(std::clamp(glm::dot(c, a), -1.0, 1.0));
    // Parallel: exact passthrough. Antiparallel: the geodesic is
    // DIRECTIONLESS (normalize(cross) -> NaN, the S7-cam trap) — for a
    // DISPLAY the right move is the snap (show the new aim), unlike the
    // carried cam_fwd which must freeze; the carried state self-heals.
    if (gap < 1e-9 || gap > kPi - 1e-6) return a;
    const double cap = std::max(cap_rad, 0.0);
    const double new_gap =
        std::min(gap * std::exp(-dt / kReticleSmoothTau), cap);
    const glm::dvec3 axis = glm::normalize(glm::cross(c, a));
    return glm::normalize(glm::angleAxis(gap - new_gap, axis) * c);
}

CameraPose aim_chase_camera(const sim::SimState& state,
                            const glm::dvec3& forward, const glm::dvec3& aim_up,
                            const sim::AircraftParams& params,
                            const ChaseParams& chase,
                            const CameraOrbit& orbit) {
    const glm::dvec3 fwd = glm::normalize(forward);
    glm::dvec3 up = glm::normalize(aim_up);
    // The decoupled S7-cam forward can be (near-)parallel to the carried aim_up
    // (a hard aim yank ~90 off the lagged forward), collapsing cross(fwd, up).
    // Fall back through the AIRFRAME's OWN axes (body-up then nose — mutually
    // perpendicular, so fwd cannot be parallel to both; the chain cannot
    // exhaust), NEVER local_up (that is the zenith rebuild §9.2 bans, and it
    // would resurrect the pole in exactly this cone) nor a fixed world axis
    // (not guaranteed perpendicular to fwd — the sibling chase_camera's
    // lesson).
    glm::dvec3 right = glm::cross(fwd, up);
    if (glm::length(right) < 1e-9) {
        const glm::dvec3 body_up =
            state.orientation * glm::dvec3{0.0, 1.0, 0.0};
        const glm::dvec3 nose = state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        up = std::abs(glm::dot(fwd, body_up)) < 0.9999 ? body_up : nose;
        right = glm::cross(fwd, up);
    }
    right = glm::normalize(right);
    up = glm::cross(right, fwd);  // re-orthogonalize (unit)

    CameraPose pose;
    pose.target = state.position;
    // Behind the AIM direction, lifted along the aim-up (the plane lags into
    // view). Freelook orbit rotates this offset about the frame's own axes —
    // composed on the eye only, never the aim.
    glm::dvec3 offset = -fwd * chase.distance + up * chase.height;
    if (orbit.yaw != 0.0 || orbit.pitch != 0.0) {
        offset = glm::angleAxis(orbit.yaw, up) *
                 (glm::angleAxis(orbit.pitch, right) * offset);
    }
    pose.eye = clamp_eye_above_surface(pose.target, pose.target + offset,
                                       params.R + chase.min_eye_altitude);

    glm::dvec3 seated = pose.eye - pose.target;
    if (glm::dot(seated, seated) < 1e-12) {
        const glm::dvec3 radial = sim::local_up(pose.target);
        const double lift_needed =
            params.R + chase.min_eye_altitude - glm::length(pose.target);
        pose.eye = pose.target + radial * std::max(chase.height, lift_needed);
        seated = pose.eye - pose.target;
    }

    // Camera-up = the carried aim_up, RAW (§9.2). Fallbacks stay WITHIN the
    // aim frame's own orthonormal axes (aim_up -> aim_right -> aim_forward,
    // mutually perpendicular so the chain cannot exhaust) — never local_up,
    // which is the zenith pole this frame removes. Triggers only if the view
    // is driven near-parallel to aim_up (steep orbit); the nominal chase never
    // enters the cone.
    const glm::dvec3 view = glm::normalize(-seated);
    pose.up = up;
    for (const glm::dvec3& candidate : {up, right, fwd}) {
        if (std::abs(glm::dot(view, candidate)) <= chase.degenerate_dot) {
            pose.up = candidate;
            break;
        }
    }
    return pose;
}

ScreenPoint project_dir(const glm::dvec3& dir, const glm::dvec3& cam_forward,
                        const glm::dvec3& cam_up, double fovy_rad,
                        double aspect) {
    // Single-source basis (camera.h): forward f, right r (with the cam_up ∥ f
    // seed fallback — screen ROLL is undefined there, so any right ⟂ f keeps
    // x/y finite instead of (int)NaN phantom reticles), and TRUE up u =
    // cross(r, f). The SAME basis feeds the orient-cue ghost horizon, so the
    // projection convention cannot fork.
    const ScreenBasis b = camera_screen_basis(cam_forward, cam_up);
    ScreenPoint p;
    if (!b.ok) return p;  // degenerate forward: not in front, x/y = 0
    const glm::dvec3 f = b.f;
    const glm::dvec3 r = b.r;
    const glm::dvec3 u = b.u;
    const glm::dvec3 d = glm::normalize(dir);

    const double z = glm::dot(d, f);  // forward component
    p.in_front = z > 1e-9;
    if (!p.in_front) return p;  // behind the camera: x/y left at 0, flagged
    const double tan_half_y = std::tan(0.5 * fovy_rad);
    const double tan_half_x = aspect * tan_half_y;
    p.x = (glm::dot(d, r) / z) / tan_half_x;
    p.y = (glm::dot(d, u) / z) / tan_half_y;
    return p;
}

double lens_shift_ndc(double height, double distance, double fovy_rad) {
    // The resting level-flight aim (horizontal nose) sits atan(height/distance)
    // above the camera-forward (which points down AT the plane). Its projected
    // NDC-y is tan(atan(h/d)) / tan(fovy/2) = (h/d) / tan(fovy/2); shifting the
    // image down by exactly that value lands the reticle at screen center.
    return (height / distance) / std::tan(0.5 * fovy_rad);
}

FrustumBounds off_center_frustum(double fovy_rad, double aspect, double nearZ,
                                 double shift_ndc) {
    // Symmetric half-extents at the near plane, then bias top/bottom by the
    // same amount so t - b (the vertical scale) is unchanged and the frustum
    // center (t + b)/(t - b) == shift_ndc. That is exactly ndc_y' = ndc_y -
    // shift: the image slides down without any zoom or vertical distortion.
    const double th = nearZ * std::tan(0.5 * fovy_rad);
    const double tw = th * aspect;
    FrustumBounds fb;
    fb.l = -tw;
    fb.r = tw;
    fb.t = th * (1.0 + shift_ndc);
    fb.b = th * (shift_ndc - 1.0);
    return fb;
}

double freelook_overhead_pitch_cap(double height, double distance,
                                   double degenerate_dot, double margin) {
    // Overhead pole = pi/2 - atan(h/d) (the resting view is already tilted
    // atan(h/d) down, so it reaches straight-down that much sooner). Stop a
    // guard cone acos(degenerate_dot) plus `margin` short of it. Clamp to >= 0
    // so a pathological geometry (view already inside the cone) yields no
    // overhead orbit rather than a negative bound.
    constexpr double kHalfPi = 1.57079632679489661923;
    const double cap = kHalfPi - std::atan2(height, distance) -
                       std::acos(degenerate_dot) - margin;
    return std::max(0.0, cap);
}

}  // namespace render
