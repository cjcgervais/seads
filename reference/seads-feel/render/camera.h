#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "sim/params.h"
#include "sim/state.h"

// Chase-camera math (SPEC §15.3 / §9.2). PURE and raylib-free on purpose:
// it reads SimState and returns a pose — render/ never writes state, and
// keeping the math out of draw code lets the gate test it headlessly.
//
// Section-3 contract: camera-up = RAW local_up(position), recomputed from
// the interpolated position every call, never cached, never slewed. This is
// correct for raw mode only — the §9.2 frame-carried (parallel-transported)
// camera that removes the zenith pole this basis owns LANDED in Section 5
// (aim_chase_camera below, driven by input::AimFrame; SPEC §0 supersession
// S5-cam), and main.cpp now uses IT in instructor mode. This raw chase_camera
// is retained ONLY for the debug raw-fly path, where vertical flight still
// hits that pole:
// with the default offsets the fallback engages near vertical (|pitch| a
// degree or two shy of 90), so
// every vertical pass crosses it twice, and the up-hint swap is a hard
// switch (no hysteresis, deliberately — a blended basis is the §6.2 ban;
// a rolled pass through vertical may snap camera roll for a frame). Known,
// disclosed, and render-only; nothing here feeds aim or control.

namespace render {

// The chase camera's vertical field of view (the RESTING, unzoomed value).
// Single source (draw.cpp's Camera3D and the lens-shift helper below both read
// the CURRENT fov — kChaseFovyDeg when unzoomed) so the reticle centering can
// never drift from the projection it centers against.
constexpr double kChaseFovyDeg = 60.0;

// RMB-hold gunsight zoom (2026-07-07). While the right mouse button is held the
// vertical FOV eases from kChaseFovyDeg to kZoomFovyDeg (~2x magnification)
// over ~kZoomEaseTime, and — in mouse-aim mode only — the camera re-points onto
// the aim/pipper so the magnified detail sits under the reticle (freelook zooms
// the center-screen view in place). Entirely DOWNSTREAM of the aim (RA9): the
// eased fov and the re-centered camera-forward never feed the mouse->aim
// update. Code constants (Chad's ruling — the zoom amount lives with the other
// camera-framing numbers here, not the TOML), tuned for feel by the pilot
// flying it.
constexpr double kZoomFovyDeg = 15.0;  // [deg] zoomed FOV (4x from 60)
constexpr double kZoomEaseTime = 0.5;  // [s] fov ease time-constant — a
                                       // deliberate zoom-in (~95% settled by
                                       // 3*tau ~ 1.5 s)

// Angular margin the freelook overhead-pitch cap keeps between the orbit and
// the camera-up degeneracy cone (freelook_overhead_pitch_cap below). Shared by
// the caller (app/main.cpp) and its test so the two never drift.
constexpr double kFreelookPoleMargin = 0.05;  // [rad] ~2.9 deg

struct ChaseParams {
    double distance = 34.0;         // [m] eye offset behind the nose
    double height = 9.0;            // [m] eye offset toward local_up
    double min_eye_altitude = 2.0;  // [m] the §9.2 never-enter-the-planet
                                    // margin (bulge occlusion is by design)
    // |dot(view, up)| above this = LookAt basis about to degenerate ->
    // fall back to body-up for THIS pose. cos(~1.8 deg) off exact vertical.
    double degenerate_dot = 0.9995;
};

struct CameraPose {
    glm::dvec3 eye{0.0};
    glm::dvec3 target{0.0};
    glm::dvec3 up{0.0};  // unit; the LookAt up hint
};

CameraPose chase_camera(const sim::SimState& state,
                        const sim::AircraftParams& params,
                        const ChaseParams& chase);

// Freelook orbit offset (SPEC §9.2): mouse-driven camera orbit about the
// aircraft, composed ON TOP of the aim-carried pose and eased to zero on
// release (cosmetic, OUTSIDE the loop — the caller owns the ease animation;
// the aim frame is NEVER touched by it). Angles in radians about the aim
// frame's own up (yaw) and right (pitch).
struct CameraOrbit {
    double yaw = 0.0;
    double pitch = 0.0;
};

// S-globelook (v4 rung 3): freelook globe inertia — "move it like a globe
// with the hand" (Chad). While freelook is HELD the orbit carries an angular
// velocity: INSTANT GRAB while the mouse moves (the hand owns the globe — the
// caller's position path is untouched; this struct only ESTIMATES the hand's
// rate), exponential COAST while held-and-still (v decays exp(-dt/tau)),
// capped so a violent flick can never spin the view into vection. PURE and
// caller-owned like CameraOrbit itself: app/main.cpp owns the glue (which
// frame is a grab/pend/coast, the clamp bounds, the resets), this struct owns
// the velocity law — the flight-audio pattern, so the law is unit-testable
// while the glue stays minimal. Cosmetic (§9.2): nothing here ever feeds
// mouse→aim, input::Freelook, or CQ2.
//
// Law (all angles radians, rates rad/s):
//  - grab(dyaw, dpitch, dt): a held frame whose hand delta was APPLIED to the
//    orbit. Updates the hand-rate estimate — an EMA (kRateEmaTau ~ 50 ms) of
//    delta/window while the hand stays on — and REPLACES the coast velocity
//    with the capped estimate. A grab after a coast gap is a FRESH gesture:
//    the EMA reseeds to the instantaneous rate (the coast velocity is
//    replaced by the live hand, never blended — the DCC-tool convention;
//    inertia-without-instant-grab is nausea, v4 memo trap #4).
//  - pend(dt): a held frame where the hand moved but nothing was applied (a
//    0-tick frame — the delta is still pending upstream). Accrues wall time
//    into the next grab's rate window so the seeded rate stays honest; the
//    orbit position is untouched (bit-identical legacy).
//  - coast(dt, tau, cap, dwell): a held-and-still frame. Coast ENTRY is gated
//    on `dwell` seconds of CONSECUTIVE stillness (rung-3 red-team P1, the
//    S-dz-motion rest_dwell pattern): an integer mouse in a SLOW drag emits
//    0-count gap frames while the HAND is still moving — pre-dwell those
//    frames coast-classified, which dropped their wall time from the rate
//    window AND cleared hand_on, so every count reseeded v to the
//    INSTANTANEOUS per-step rate and coasted on it (measured ~2x drag
//    amplification at 1 count/2 frames, ~3.7x at 1 count/4, a lone 1-px
//    nudge gliding 2.4 deg vs the 0.2 commanded — the quantizer, not the
//    hand, left the globe). Still frames INSIDE the dwell classify as PEND
//    (no coast motion; the rate window keeps accruing wall time, so a
//    post-gap grab seeds delta/true-window; hand_on holds, so the EMA
//    blends instead of reseeding). Once the dwell of true stillness has
//    elapsed the coast engages, with the rate window ZEROED on entry (a
//    long rest never dilutes the NEXT gesture's instant-grab seed).
//    dwell = 0 is the pre-dwell coast-immediately arm, bit-identical — it
//    RE-OPENS the staircase amplification above. While coasting, returns
//    the {yaw, pitch} delta to add to the orbit — the EXACT integral
//    v·tau·(1 − exp(−dt/tau)) so two 8 ms frames integrate identically to
//    one 16 ms frame — then decays v.
//  - hit_yaw_clamp()/hit_pitch_clamp(): the caller detected clamp contact on
//    that axis — the wall absorbs the coast (no winding behind it).
//  - reset(): full wipe — freelook release, respawn, orient verb (the caller
//    calls it exactly where it zeroes/eases the orbit today).
// tau <= 0 is the structural OFF arm: every method is inert (zero deltas, no
// state), the S7-hrz rate=0 pattern — the caller additionally gates its glue
// on tau > 0 so the orbit expression tree stays the bit-identical legacy.
struct OrbitInertia {
    struct Delta {
        double yaw = 0.0;
        double pitch = 0.0;
    };

    double v_yaw = 0.0;      // [rad/s] coast velocity
    double v_pitch = 0.0;    // [rad/s]
    double ema_yaw = 0.0;    // [rad/s] hand-rate EMA (seeds the coast)
    double ema_pitch = 0.0;  // [rad/s]
    bool hand_on = false;    // last classified frame was a moving hand
    double window_dt = 0.0;  // [s] wall time accrued toward the next grab
    double still_dt = 0.0;   // [s] consecutive stillness accrued toward the
                             //     coast-entry dwell (P1); any hand motion
                             //     (grab OR pend) zeroes it

    static constexpr double kRateEmaTau = 0.05;  // [s] hand-rate EMA window

    void reset() { *this = OrbitInertia{}; }

    void grab(double dyaw, double dpitch, double dt, double tau, double cap) {
        if (!(tau > 0.0)) return;  // structural OFF: inert
        still_dt = 0.0;            // the hand moved — stillness broken
        const double w = window_dt + dt;
        window_dt = 0.0;
        if (!(w > 0.0)) return;  // no wall time — no rate sample (dt<=0 guard)
        const double r_yaw = dyaw / w;
        const double r_pitch = dpitch / w;
        if (hand_on) {  // continuous motion: EMA tracks the hand
            const double a = 1.0 - std::exp(-w / kRateEmaTau);
            ema_yaw += a * (r_yaw - ema_yaw);
            ema_pitch += a * (r_pitch - ema_pitch);
        } else {  // fresh gesture after a coast/still gap: the hand REPLACES
            ema_yaw = r_yaw;
            ema_pitch = r_pitch;
            hand_on = true;
        }
        v_yaw = std::clamp(ema_yaw, -cap, cap);
        v_pitch = std::clamp(ema_pitch, -cap, cap);
    }

    void pend(double dt) {
        window_dt += dt;
        still_dt = 0.0;  // a pending delta IS hand motion — stillness broken
    }

    Delta coast(double dt, double tau, double cap, double dwell = 0.0) {
        Delta d;
        if (!(tau > 0.0)) {  // structural OFF: inert, and nothing lingers
            hand_on = false;
            v_yaw = v_pitch = 0.0;
            return d;
        }
        // P1 stillness dwell (struct comment above). Gated on dwell > 0 so
        // dwell = 0 is the pre-dwell tree bit-identically (no still_dt
        // bookkeeping, window_dt untouched by coasting — exactly the old
        // arithmetic, staircase amplification and all).
        if (dwell > 0.0) {
            still_dt += dt;
            if (still_dt < dwell) {  // inside the dwell: PEND-classified
                window_dt += dt;     // wall time keeps accruing — a post-gap
                return d;            //   grab seeds delta/true-window
            }
            window_dt = 0.0;  // coast entry: a long rest never dilutes the
                              //   NEXT gesture's instant-grab seed
        }
        hand_on = false;  // the hand left the globe
        v_yaw = std::clamp(v_yaw, -cap, cap);
        v_pitch = std::clamp(v_pitch, -cap, cap);
        if (!(dt > 0.0)) return d;  // no-op on a zero frame (blend_toward conv)
        const double k = std::exp(-dt / tau);
        const double integ = tau * (1.0 - k);  // exact ∫ v·e^(−t/τ) dt / v
        d.yaw = v_yaw * integ;
        d.pitch = v_pitch * integ;
        v_yaw *= k;
        v_pitch *= k;
        return d;
    }

    void hit_yaw_clamp() { v_yaw = 0.0; }
    void hit_pitch_clamp() { v_pitch = 0.0; }
};

// Decoupled lagging chase-forward (SPEC §9.2, S7-cam Phase 1). Eases the
// persistent camera-forward `cam_fwd` one frame toward a rest target that sits
// behind `vel_dir` and leans `lead` of the way toward `aim_fwd`; the follow
// rate is `lag_base + lag_gain*deflection` (deflection = angle between velocity
// and aim), so a bigger gap catches faster ("not jumpy"). PURE: the caller
// stores cam_fwd and passes the result as the camera forward BELOW. This is a
// DOWNSTREAM consumer of the aim — nothing here (or its caller) feeds the
// mouse->aim update, so the RA9 rubber-band cannot form. Unit-length in/out;
// v||a or already-arrived are no-ops (no NaN).
glm::dvec3 ease_chase_forward(const glm::dvec3& cam_fwd,
                              const glm::dvec3& vel_dir,
                              const glm::dvec3& aim_fwd, double lead,
                              double lag_base, double lag_gain, double dt);

// S-keychase (Chad 2026-07-28, flying the S-relorient addendum: "it gives me an
// oblique view still"). A GUNNERY mechanism, not a comfort one — reclassified
// on Chad's 2026-07-29 refinement, which is the governing statement of intent:
//
//   "I can plan my aim for when I release the key override... but also, and
//    most importantly, the fact that I am looking through the nose line of the
//    plane from behind, or see its angle from behind its velocity — and it may
//    be obliquely aligned at an enemy, but I see where I am shooting and plan
//    my aim when hard-pressing maneuver without freelook."
//
// So the key-flown camera does TWO jobs, neither of them comfort: (1) it shows
// the GUN LINE — sitting behind the VELOCITY is what makes the nose-vs-velocity
// angle legible, and the nose is where the guns fire, so an aircraft flying one
// way while obliquely lined up on an enemy another way can still read its shot;
// (2) it leaves the aim free to be PRE-PLACED for the moment the keys come up.
//
// The chase camera's rest target is anchored to the AIM (`lead = 1.0`), which is
// right for mouse-aim — there the aim IS where you're going. Under keyboard
// flying the premise fails: the keys change the path while the aim stays parked.
// ⚠ The parked aim is NOT a stale target — it is DELIBERATE, the pilot's PLAN
// for the release. The defect was never that the aim sat still; it was that the
// CAMERA was spent on the plan at the moment the pilot needed the SHOT. So no
// amount of extra catch rate could fix it: a legitimate target answering the
// wrong question arrives at the wrong place sooner, and speeds up the flown-in
// mouse-aim float as collateral. Measured before the fix: a sustained turn
// stands at oblique_deg 95.8 and NEVER converges (`seads_harness comfort`).
//
// The fix changes only WHICH TARGET the (flown, approved, untouched) chase law
// chases: while keys are flying, rest target := the flight path (lead 0) with a
// fast constant catch (key_rate, gain 0). Mouse-aim flying is bit-identical by
// construction — `keys_flying` false returns the caller's own dials verbatim,
// and `key_rate <= 0` disables the mechanism STRUCTURALLY (the walk-back).
// PURE, so the app (main.cpp) and the comfort instrument (MiniCamera::advance)
// select the args through the SAME function and cannot fork.
//
// ⚠ THE HANDBACK IS A HARD SWITCH ON PURPOSE — DO NOT "IMPROVE" IT. On key
// release the target jumps back to the parked aim and the camera swings to it
// (~16 deg, ~0.7 s, no pop: the direction itself is carried and eased). A blend
// was held in reserve during the fly and proved UNNECESSARY: the swing is the
// pilot's own pre-placed aim BEING DELIVERED — intent arriving, not an artifact
// — so softening it would blur the moment the plan lands. Chad flew this exact
// behavior as "precisely perfect" (2026-07-29). Recorded here so a future
// session doesn't smooth it as obvious polish.
struct ChaseAnchor {
    double lead = 0.0;
    double lag_base = 0.0;
    double lag_gain = 0.0;
};
inline ChaseAnchor chase_anchor(bool keys_flying, double lead, double lag_base,
                                double lag_gain, double key_rate) {
    if (!keys_flying || key_rate <= 0.0) return {lead, lag_base, lag_gain};
    return {0.0, key_rate, 0.0};
}

// Ease a scalar one frame toward `target` with time-constant `tau` (exponential
// step 1 - exp(-dt/tau)). dt <= 0 => no-op (returns current); tau <= 0 => snap
// (returns target). PURE; the RMB-zoom amount animation (0 = resting, 1 =
// zoomed). Cosmetic — the eased value never feeds mouse->aim (RA9).
double blend_toward(double current, double target, double tau, double dt);

// Rotate the camera-forward `cam_fwd` a fraction `t` in [0,1] of the way toward
// `aim_fwd` along the shorter great-circle arc (t=0 => cam_fwd, t=1 =>
// aim_fwd). The RMB-zoom re-center in mouse-aim: pointing the eye at the pipper
// as the view narrows. Same BOTH-ENDS antiparallel NaN-guard as
// ease_chase_forward (a carried cam_fwd that ever goes NaN stays NaN) —
// parallel OR antiparallel => cam_fwd returned unchanged. DOWNSTREAM of the
// aim, never fed back to mouse->aim (RA9). Unit in/out. `t` is clamped to
// [0,1].
glm::dvec3 blend_forward(const glm::dvec3& cam_fwd, const glm::dvec3& aim_fwd,
                         double t);

// S-reticle (Chad 2026-07-08: "little jitters make me lose confidence"):
// DISPLAY-ONLY smoothing of the reticle draw direction. Integer mouse deltas
// staircase a slow deliberate sweep (a 1 px frame = aim_sensitivity deg ~ 2.6
// screen px at 1080p/60), and the RAW aim — correctly unsmoothed, SPEC 9.1 —
// shows every step. This ease produces a COPY (FrameInfo.reticle_dir) whose
// only reader is the reticle projection; it never feeds aim/camera/control
// (RA9 downstream-only, the orbit/zoom/speed_trend class). Exponential gap
// shrink with a HARD LAG CAP (cap_rad, caller-derived from quant_px *
// aim_sensitivity so it tracks any retune): the ease only has authority at
// the pixel-quantization scale — flicks and snaps land within cap in ONE
// call, so no rubber-band, no macro trailing, no reseed plumbing (any snap
// self-heals through the cap). dt <= 0 => no-op (the blend_toward
// convention). BOTH-ends NaN guard (S7-cam P1).
constexpr double kReticleSmoothTau = 0.05;  // [s] display ease time constant
glm::dvec3 reticle_smooth(const glm::dvec3& s, const glm::dvec3& target,
                          double dt, double cap_rad);

// The §9.2 chase camera. `forward` is the (lagged, S7-cam) camera-forward — no
// longer welded to the aim; the plane and the aim reticle lag into view. `up`
// is the carried aim_up (S7-cam3: camera-up = the aim frame's own up, so the
// camera shows the world from the mouse frame — mouse-up == screen-up at any
// attitude), re-orthogonalized against `forward` here (Gram-Schmidt), so the
// holonomy + zenith-avoidance of the carried frame survive the decoupled
// forward (AT-14). eye sits behind `forward`, lifted along up, clamped above
// the surface. Freelook orbit is composed on the eye only. PURE / raylib-free.
CameraPose aim_chase_camera(const sim::SimState& state,
                            const glm::dvec3& forward, const glm::dvec3& aim_up,
                            const sim::AircraftParams& params,
                            const ChaseParams& chase,
                            const CameraOrbit& orbit = {});

// Projection of a world-space DIRECTION into normalized device coords for the
// reticle/nose-marker pair (SPEC §9.2). Reticle = projection of the aim; nose
// marker = projection of the nose; the on-screen gap IS the controller's error.
// Directions (not points) — the aim/nose lines are effectively at infinity, so
// this is the far-limit projection. y is +up (NDC, [-1,1]); in_front is false
// when the direction is behind the camera. PURE; the raylib pixel glue is thin.
struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
    bool in_front = false;
};

// The screen basis project_dir builds from (cam_forward, cam_up): forward f,
// right r = normalize(cross(f, cam_up)) (with a seed fallback when cam_up ∥ f),
// and TRUE up u = cross(r, f). SINGLE SOURCE for the camera-projection
// convention — both project_dir AND the orient-cue ghost horizon build their
// basis HERE, so the two can never fork (the projection-basis-fork lesson). ok
// is false only when cam_forward is (near-)zero-length.
struct ScreenBasis {
    glm::dvec3 f{0.0};  // forward (unit)
    glm::dvec3 r{0.0};  // right (unit)
    glm::dvec3 u{0.0};  // true up = cross(r, f) (unit)
    bool ok = false;
};

inline ScreenBasis camera_screen_basis(const glm::dvec3& cam_forward,
                                       const glm::dvec3& cam_up) {
    ScreenBasis b;
    const double fl = glm::length(cam_forward);
    if (!(fl > 1e-12)) return b;
    b.f = cam_forward / fl;
    const double ul = glm::length(cam_up);
    const glm::dvec3 up_seed =
        ul > 1e-12 ? cam_up / ul : glm::dvec3{0.0, 1.0, 0.0};
    glm::dvec3 r = glm::cross(b.f, up_seed);
    if (glm::length(r) < 1e-9) {
        const glm::dvec3 seed =
            std::abs(b.f.x) < 0.9 ? glm::dvec3{1, 0, 0} : glm::dvec3{0, 1, 0};
        r = glm::cross(b.f, seed);
    }
    const double rl = glm::length(r);
    if (!(rl > 1e-12)) return b;
    b.r = r / rl;
    b.u = glm::cross(b.r, b.f);  // already unit (r ⟂ f, both unit)
    b.ok = true;
    return b;
}

// Wrap an angle to (-pi, pi] (S-freelook360, Chad 2026-07-17 "keep going
// around indefinitely"): the UNLIMITED freelook-orbit yaw accumulates raw
// mouse forever, so the caller wraps it each frame — the stored yaw stays
// bounded (no float creep after minutes of spinning) and the release decay
// (orbit.yaw *= k) always eases home the SHORT way around. Pure; shared by
// app/main.cpp and its unit legs so the wrap convention cannot fork.
inline double wrap_pi(double a) {
    constexpr double kPi = 3.14159265358979323846;
    a = std::fmod(a + kPi, 2.0 * kPi);  // (-2pi, 2pi)
    if (a <= 0.0) a += 2.0 * kPi;       // (0, 2pi]
    return a - kPi;                     // (-pi, pi]
}

ScreenPoint project_dir(const glm::dvec3& dir, const glm::dvec3& cam_forward,
                        const glm::dvec3& cam_up, double fovy_rad,
                        double aspect);

// Vertical lens shift (SPEC §9.2 framing): a constant NDC offset applied to the
// projection so the resting level-flight reticle sits at SCREEN CENTER instead
// of high (the camera looks DOWN at the plane, so the horizontal aim projects
// ~atan(height/distance) above center). This is a pure lens/frustum shift — the
// camera's look DIRECTION is unchanged (no rotation); the whole image slides
// down so the plane and reticle move down together. The value CENTERS the
// resting reticle for ANY distance/height, so zoom/elevation stay free knobs.
double lens_shift_ndc(double height, double distance, double fovy_rad);

// Off-center perspective frustum realizing `shift_ndc` as ndc_y' = ndc_y -
// shift. The vertical SCALE is preserved (t - b == 2*near*tan(fovy/2), no zoom
// or distortion); only the frustum center moves (t + b) / (t - b) == shift.
// draw.cpp feeds these to raylib's MatrixFrustum for the 3D scene; the 2D
// reticle overlay subtracts the SAME shift so the two stay locked.
struct FrustumBounds {
    double l, r, b, t;
};
FrustumBounds off_center_frustum(double fovy_rad, double aspect, double nearZ,
                                 double shift_ndc);

// Max |orbit.pitch| on the OVERHEAD side (eye above the plane, view rotating
// toward straight-down) before the camera-up degeneracy guard fires and flips
// the view. The resting chase view already tilts atan(height/distance) below
// forward, so the overhead pole sits at pi/2 - atan(h/d) — NOT pi/2. This
// returns that pole minus the guard cone acos(degenerate_dot) and `margin`.
// The eye-below side pole is at pi/2 + atan(h/d) (unreachable), so the caller
// caps only the overhead (negative-pitch) side with this; the below side is
// bounded by the config knob alone. Must track height/distance for the SAME
// reason the lens shift does — a ChaseParams retune moves the pole.
double freelook_overhead_pitch_cap(double height, double distance,
                                   double degenerate_dot, double margin);

}  // namespace render
