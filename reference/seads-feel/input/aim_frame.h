#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "control/transport.h"

// The aim/camera frame (SPEC §9.1 / §9.2): a world-frame ORIENTATION that
// carries BOTH the aim direction (targetDir = forward) AND the camera roll
// reference (camera-up = up). Per §9.2 "the §9.1 mouse basis IS this frame":
// the mouse rotates it, parallel transport carries it, and the render camera
// reads its rotation 1:1. One quaternion, so both stay a single orthonormal
// object — mouse rotation and transport are both rotations, so it can never
// shear or lose orthogonality (the pole-free representation §9.1 mandates over
// Euler/local_up bases that degenerate at the zenith aim).
//
// This is the ONE genuinely new caller-side piece the app-wiring adds. It is
// PURE (no I/O, no clock, no raylib) and lives in input/ beside aim_state.h;
// it may include control/ (the shared transport) but never render/. Like
// input::Freelook it is a SHARED module: the app drives it live and the §6
// AT-14 holonomy test exercises the SAME transport — never two implementations
// (CLAUDE.md: "route the live path THROUGH the tested code").
//
// Frame convention (SPEC §7, body axes of the frame): +X right, +Y up,
// -Z forward. So forward = q*(0,0,-1), up = q*(0,1,0), right = q*(1,0,0).

namespace input {

struct AimFrame {
    glm::dquat q{1.0, 0.0, 0.0, 0.0};  // world orientation of the aim frame

    glm::dvec3 forward() const { return q * glm::dvec3{0.0, 0.0, -1.0}; }
    glm::dvec3 up() const { return q * glm::dvec3{0.0, 1.0, 0.0}; }
    glm::dvec3 right() const { return q * glm::dvec3{1.0, 0.0, 0.0}; }

    // Parallel transport across one tick (SPEC §9.1/§9.2): rotate the WHOLE
    // frame by the IDENTICAL quaternion that carries a bare aim vector
    // (control::transport_rotation, the shared source) — so aim and camera-up
    // accumulate the exact same holonomy (AT-14). A world-frozen frame drifts
    // off the horizon in seconds on R = 15 km; this keeps a "level" aim level.
    void transport(const glm::dvec3& up_prev, const glm::dvec3& up_cur) {
        q = glm::normalize(control::transport_rotation(up_prev, up_cur) * q);
    }

    // Mouse delta -> aim rotation about the frame's OWN up (dx) and right (dy)
    // axes — the RAW, pole-free basis (SPEC §9.1, ruled: the dx axis is the
    // aim frame's up, NOT local_up, which degenerates exactly at the zenith
    // aim this frame keeps legal). Right-multiplying by a body-axis rotation
    // rotates about the frame's own world axis (angleAxis(t, q*v)*q ==
    // q*angleAxis(t, v)). NOTHING here is smoothed/slewed/eased — the whole
    // mouse->aim->error path is raw (§9.1; the sole exception is the filtered
    // AoA in the clamp, which is downstream of this).
    //
    // Signs (non-inverted mouse aim, WT-style "reticle follows the cursor"):
    //   dx > 0 (mouse right) -> aim moves RIGHT of the nose -> yaw-right demand
    //   dy > 0 (mouse down)  -> aim moves DOWN below the nose -> pitch-down
    // Both compose through control::rotation_demand_body to the SPEC §7 omega
    // signs (pinned in test_aim_frame, composed with AT-0).
    void apply_mouse(double dx, double dy, double sens) {
        q = glm::normalize(
            q * glm::angleAxis(-dx * sens, glm::dvec3{0.0, 1.0, 0.0}) *
            glm::angleAxis(-dy * sens, glm::dvec3{1.0, 0.0, 0.0}));
    }

    // SCREEN-RELATIVE mouse (F1, §7 Chad 2026-07-06 — a §9.1 supersession):
    // rotate the aim about the RENDERED CAMERA screen basis instead of this
    // frame's OWN carried up/right. The camera is horizon-locked (cam_up eases
    // toward local_up ⟂ view, render::ease_level_up) AND carried through the
    // zenith by its cone-hold, so its right/up are screen-consistent regardless
    // of the airframe's attitude: mouse-up == up ON SCREEN, always. This fixes
    // the "mouse inverts after a maneuver" bug — the carried frame rolls ~180°
    // through a loop while the horizon-locked camera stays upright, so the old
    // frame-axis apply_mouse pushed the aim the wrong way.
    //
    // The screen basis is rebuilt EXACTLY as the render camera does
    // (render::aim_chase_camera re-orthogonalizes cam_up against cam_fwd):
    //   r = normalize(cross(cam_fwd, cam_up))   screen-right
    //   u = cross(r, cam_fwd)                   screen-up (re-orthogonalized)
    // Screen-right comes from CAM_FWD, never aim.forward() — an aim-sourced
    // right flips sign at the aim's own zenith crossing and STALLS the loop
    // (the exact level_up_to failure §7 rejected). The aim FORWARD stays a free
    // carried world vector; only the rotation AXES become camera-referenced, so
    // a sustained vertical mouse still sweeps the aim cleanly over the top (the
    // camera's cone-hold freezes r near vertical -> the aim keeps rotating
    // about one fixed horizontal axis).
    //
    // WORLD-axis LEFT-multiply (angleAxis(t, world_axis) * q), same signs as
    // the frame-axis overload: dx>0 (mouse right) -> aim moves screen-RIGHT
    // (+r); dy>0 (mouse down) -> aim moves screen-DOWN (-u). Nothing is
    // smoothed on the mouse->aim->error path: the eased camera basis only
    // ORIENTS the pilot's RAW delta (it is not a setpoint the aim CHASES), and
    // with zero mouse this is a STRICT no-op (angleAxis(0,·) is identity) -> no
    // hands-off dynamics, so no RA9 rubber-band, and the mirror-equivalence
    // path stays bit-identical. Degenerate cam basis (cam_fwd ∥ cam_up — the
    // render camera's own zenith cone-hold frame) collapses cross(): fall back
    // to the carried-frame overload (a measure-zero, continuous fallback; also
    // keeps zero-mouse a clean no-op instead of a NaN axis).
    void apply_mouse(double dx, double dy, double sens,
                     const glm::dvec3& cam_fwd, const glm::dvec3& cam_up) {
        glm::dvec3 r = glm::cross(cam_fwd, cam_up);  // raw: direction only
        const double rl = glm::length(r);
        // !(rl > eps) also catches a ZERO or NaN cam basis (a
        // default-constructed TickInput passes {0,0,0}): NaN fails the > test,
        // so we fall back rather than poison q with a NaN axis (angleAxis(0,
        // NaN) is NOT identity).
        if (!(rl > 1e-9)) {  // cam_fwd ∥ cam_up / unset: carried-frame fallback
            apply_mouse(dx, dy, sens);
            return;
        }
        r /= rl;
        const glm::dvec3 f = glm::normalize(cam_fwd);
        const glm::dvec3 u = glm::cross(r, f);  // re-orthogonalized screen-up
        q = glm::normalize(glm::angleAxis(-dx * sens, u) *
                           glm::angleAxis(-dy * sens, r) * q);
    }

    // Roll the WHOLE frame about its own forward by a signed angle (S7-hrz,
    // docs/horizon_recovery_plan.md — the horizon-recovery gauge move). The aim
    // DIRECTION is invariant (a rotation about forward fixes forward, up to fp
    // ~1e-16/call), so the instructor cannot see it; only the mouse basis and
    // the camera-up (the same up) roll — together, so mouse-up == screen-up is
    // preserved through the roll. Positive angle = right-handed about the world
    // forward axis.
    void roll_about_forward(double angle) {
        q = glm::normalize(glm::angleAxis(angle, forward()) * q);
    }

    // The signed roll angle (about forward, right-handed) that would align this
    // frame's up() with ref_up projected perpendicular to forward — i.e.
    // roll_about_forward(up_misalignment(u)) rights the frame's horizon to u.
    // Returns a value in (-pi, +pi] (the SHORT way; exactly-antiparallel up
    // returns +pi or -pi deterministically via atan2's signed-zero convention —
    // total, no normalize(cross), so no NaN at either degenerate end, the
    // S7-cam lesson). Returns exactly 0.0 at the zenith (forward within ~0.81
    // deg of +/-ref_up, the same |dot| > 0.9999 cone as level_up_to): there is
    // no meaningful "upright" aiming straight up/down the radial.
    //
    // S7-hrz contract: this is a CAPTURE primitive — the caller reads it ONCE
    // (at the freelook-release edge) and counts the captured angle down
    // OPEN-LOOP. Re-evaluating it per tick would chase a target that moves with
    // the mouse-driven forward (the banned control-driving-quaternion loop; the
    // red-teamed P0, plan section 10 F1) and flip across the zenith.
    double up_misalignment(const glm::dvec3& ref_up) const {
        const glm::dvec3 fwd = forward();
        const glm::dvec3 rn = glm::normalize(ref_up);
        if (std::abs(glm::dot(fwd, rn)) > 0.9999) return 0.0;  // zenith
        const glm::dvec3 proj = rn - glm::dot(rn, fwd) * fwd;
        const glm::dvec3 ut = glm::normalize(proj);  // guarded by the cone
        const glm::dvec3 u = up();
        return std::atan2(glm::dot(fwd, glm::cross(u, ut)), glm::dot(u, ut));
    }

    // Level the frame's UP to a reference up (S7-cam3 / screen-relative mouse):
    // roll about the CURRENT forward so up() aligns with ref_up projected onto
    // the plane perpendicular to forward. forward() (the aim direction) is
    // UNCHANGED — only the mouse basis (this frame's up/right) rolls. The
    // caller passes local_up each tick, so the mouse becomes SCREEN-RELATIVE
    // (its up/ right track the world horizon, matching the horizon-locked
    // camera): mouse- up == screen-up regardless of the airframe's attitude —
    // the aim stops "flipping" (up-is-down) after big maneuvers. This
    // SUPERSEDES §9.1's carried pole-free basis: it re-imports the zenith
    // singularity (ref_up ∥ forward, i.e. the aim pointing straight up/down the
    // radial), guarded here as a no-op (keep the current roll that frame — a
    // measure-zero, continuous fallback).
    void level_up_to(const glm::dvec3& ref_up) {
        const glm::dvec3 fwd = forward();
        if (std::abs(glm::dot(glm::normalize(fwd), glm::normalize(ref_up))) >
            0.9999)
            return;
        set_from(fwd, ref_up);
    }

    // Full reseed (SPEC §9.2 "camera-up STARTS at local_up"): forward := nose,
    // up := local_up (Gram-Schmidt against forward). Spawn / crash-reset ONLY
    // (focus-loss uses snap_forward_to_nose to preserve the carried-up
    // holonomy) — this is the legitimate seed, NOT the banned continuous
    // rebuild-from-local_up (§9.2). A banked spawn seeds a wings-level horizon
    // reference, by design. Degenerate only if the nose is along local_up
    // (a vertical spawn); there the local_up seed is meaningless, so fall back
    // to the airframe's own body-up.
    void reseed(const glm::dquat& orientation, const glm::dvec3& local_up) {
        const glm::dvec3 fwd = orientation * glm::dvec3{0.0, 0.0, -1.0};
        glm::dvec3 up_ref = local_up;
        if (std::abs(glm::dot(glm::normalize(fwd), glm::normalize(up_ref))) >
            0.9999) {
            up_ref = orientation * glm::dvec3{0.0, 1.0, 0.0};  // vertical nose
        }
        set_from(fwd, up_ref);
    }

    // Snap the aim forward to an arbitrary world direction while KEEPING the
    // carried up (the §5b per-tick nose WELD and the release snap — aim :=
    // NOSE since v9 S-nosesnap, b4c0751 — both route through here). Re-orthogonalize the
    // carried up against the new forward so camera roll stays continuous.
    // Falls back to the airframe's body-up only if the new forward lands
    // along the carried up — reachable only across a LARGE one-shot gap (the
    // parked-aim rule-2 snap); per-tick nesting caps the gap at one tick of
    // airframe rotation (~w*dt), so under nesting the carried up rotates
    // RIGIDLY with the nose and this fallback (and the D10 pole) is
    // unreachable — pinned by the vertical-pull continuity leg.
    void snap_forward_to_dir(const glm::dvec3& dir,
                             const glm::dquat& orientation) {
        const glm::dvec3 fwd = dir;
        glm::dvec3 up_ref = up();
        if (std::abs(glm::dot(glm::normalize(fwd), glm::normalize(up_ref))) >
            0.9999) {
            up_ref = orientation * glm::dvec3{0.0, 1.0, 0.0};
        }
        set_from(fwd, up_ref);
    }

    // Snap the aim forward to the nose while KEEPING the carried up (SPEC §9.5
    // "aim := nose" during flight — the freelook snap events, focus loss, and
    // the §5b nesting tick). No surprise leveling roll, the whole point of the
    // conditional reset (CLAUDE.md 4d).
    void snap_forward_to_nose(const glm::dquat& orientation) {
        snap_forward_to_dir(orientation * glm::dvec3{0.0, 0.0, -1.0},
                            orientation);
    }

   private:
    // Build q from a forward + up reference (right-handed frame, forward=-Z,
    // up=+Y, right=+X). Columns of the rotation matrix are the world images of
    // the body axes: [right, up, -forward].
    void set_from(const glm::dvec3& fwd_in, const glm::dvec3& up_in) {
        const glm::dvec3 fwd = glm::normalize(fwd_in);
        const glm::dvec3 right = glm::normalize(glm::cross(fwd, up_in));
        const glm::dvec3 up_o = glm::cross(right, fwd);  // already unit
        const glm::dmat3 m{right, up_o, -fwd};  // columns = axis images
        q = glm::normalize(glm::quat_cast(m));
    }
};

}  // namespace input
