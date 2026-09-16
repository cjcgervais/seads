#pragma once
// ★ REST-EDGE CAMERA HORIZON RECOVERY — the v13 law, factored so BOTH carried
// aim frames can run it: the aeroplane's (app/instructor_tick.h) and the
// Sting's (app/sting.h). Chad 2026-09-10: "I need the sting's horizon to
// auto-rotate just like for the airplane."
//
// This is a PURE MOVE of the block that lived inline in instructor_tick.h —
// the arithmetic, the ordering and every gate are byte-for-byte the same code,
// only the state it touches is passed by reference instead of read off the
// instructor state. The aeroplane path is therefore bit-identical (a refactor,
// not a rewrite); the Sting simply calls it with its own state instance.
//
// The law itself (unchanged, see the block comments below and the
// [horizon_recovery] config block): when the mouse has been still for
// rest_dwell while airborne and out of freelook, with the aim resolved on the
// flight path and the path turning below straight_max, the standing camera-up
// misalignment is CAPTURED ONCE and rolled out about the aim's OWN forward at
// `rate`, ramping in at `ease_in` and easing out through `settle`. Never a
// per-tick horizon lock (the banned S7-mouselevel shape): one finite debt,
// measured once at the rest edge.
//
// INPUTS the caller must supply: the carried aim frame, the four state fields
// below, the ENABLE (rate > 0 && airborne && not in freelook), the `aim_moved`
// bit (mouse delta on a live-aim, non-grounded tick), the machine's VELOCITY
// (the only legal camera read — the flight path, never body attitude, never
// keys), the local up, the controller params and the fixed dt.

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "control/params.h"
#include "input/aim_frame.h"
#include "input/aim_state.h"

namespace app {

// The four state fields the law carries between ticks. The aeroplane keeps its
// own equivalents as loose members of the instructor state (historical names,
// left alone so the plane path stays bit-identical); the Sting embeds this.
struct RestHorizonState {
    double aim_rest = 0.0;             // [s] mouse-still dwell
    bool armed = false;                // a capture has fired this rest
    input::HorizonRecovery recov{};    // the open-loop roll latch
    glm::dvec3 prev_path{0.0};         // last tick's path direction
};

// One fixed tick of the rest-edge law on `aim`. `enabled` is the caller's
// composed envelope gate (rate > 0 && airborne && !freelook); false runs the
// cancel arm.
inline void rest_horizon_tick(input::AimFrame& aim, double& aim_rest,
                              bool& armed, input::HorizonRecovery& recov,
                              glm::dvec3& prev_path, bool enabled,
                              bool aim_moved, const glm::dvec3& vel,
                              const glm::dvec3& up,
                              const control::ControllerParams& cp, double dt) {
    if (enabled) {
        // v13d straightness gate state (Chad fly-3: "wait until I fly
        // straight"): the path direction is tracked EVERY tick of the
        // envelope so the tick-to-tick rotation rate is honest — updating
        // it only on capture-eligible ticks would measure the first
        // attempt against a stale direction. Zero prev_path (reset /
        // sub-1 m/s) measures nothing and cannot arm.
        const double speed = glm::length(vel);
        double path_rate = -1.0;  // <0 = unmeasurable this tick
        glm::dvec3 path{0.0};
        if (speed > 1.0) {
            path = vel / speed;
            if (glm::length(prev_path) > 0.5) {
                const double cturn =
                    glm::clamp(glm::dot(path, prev_path), -1.0, 1.0);
                path_rate = std::acos(cturn) / dt;
            }
            prev_path = path;
        } else {
            prev_path = glm::dvec3{0.0};
        }
        if (aim_moved) {
            // ONE SMOOTH MOTION (pilot fly-ruling 2026-08-06 on the first
            // v13 build: "the camera rotation is happening in steps — it
            // should be one smooth motion"): mouse motion resets the DWELL
            // (and disarms a FINISHED roll so the next capture needs a
            // fresh rest edge), but it never cancels a roll IN FLIGHT. A
            // resting hand on an optical mouse emits occasional 1-px
            // deltas; cancel-on-any-motion turned those into
            // roll -> cancel -> re-dwell -> re-capture: the felt steps.
            // Completing the captured debt is the sanctioned S7-hrz
            // open-loop shape — the freelook-RELEASE roll already survives
            // a mid-roll keypress by the same law. The roll is about the
            // aim's own forward; the mouse rotates that forward: the two
            // compose, the roll never fights the hand. Debt is finite
            // (<= pi at 150 deg/s ~ 1.2 s worst case); a pilot who
            // maneuvers mid-roll ends NEAR upright — accepted, same as the
            // release-edge law. Freelook entry, GROUNDED and focus loss
            // (the else arm + instructor_focus_loss) still cancel outright.
            aim_rest = 0.0;
            if (!(recov.remaining > 0.0)) armed = false;
        } else {
            aim_rest = std::min(aim_rest + dt,
                                   cp.horizon_recovery_rest_dwell);
            if (!armed &&
                aim_rest >= cp.horizon_recovery_rest_dwell) {
                // The rest EDGE: capture the standing debt ONCE — but only
                // when the roll cannot disturb the pilot (v13c arm gates,
                // fly-ruling 2026-08-06 fly-2: "the rotation occurs too
                // easily... affecting the relative position of my mouse
                // aim on the screen"):
                //   (1) the debt is inversion-class (>= arm_min — a small
                //       horizon tilt stays the pilot's, retired on the
                //       freelook release as always), and
                //   (2) the aim is RESOLVED ON THE FLIGHT PATH (aim within
                //       path_band of the velocity direction) — the reticle
                //       sits ~centered so the roll displaces it
                //       imperceptibly; a held-off carve structurally
                //       cannot fire. Velocity is the LEGAL camera read
                //       (the flight path); body attitude and key state
                //       stay unread. Sub-1 m/s (undefined path) never
                //       arms.
                // The gates bind the CAPTURE only: an in-flight roll still
                // completes as one smooth motion (v13b), and a blocked
                // capture leaves recov_armed=false so the arrival on the
                // path later in the same rest fires without a fresh dwell.
                //   (3) the flight path is STRAIGHT (v13d, fly-3: "wait
                //       until I fly straight" — path rotation rate at or
                //       below straight_max; level great-circle flight's
                //       own V/R curvature passes by the loader wall, any
                //       real turn or loop blocks). With the path straight
                //       the nose rides it to within AoA, so gate (2) IS
                //       the "mouse and nose are resolved" condition
                //       through legal reads only.
                const double debt = aim.up_misalignment(up);
                bool on_path = false;
                if (speed > 1.0) {
                    const double cang = glm::clamp(
                        glm::dot(aim.forward(), path), -1.0, 1.0);
                    on_path = std::acos(cang) <=
                              cp.horizon_recovery_path_band;
                }
                const bool straight =
                    path_rate >= 0.0 &&
                    path_rate <= cp.horizon_recovery_straight_max;
                if (std::abs(debt) >= cp.horizon_recovery_arm_min &&
                    on_path && straight) {
                    armed = true;
                    recov.capture(debt);
                }
            }
        }
        const double d = recov.step(dt, cp.horizon_recovery_rate,
                                       cp.horizon_recovery_settle,
                                       cp.horizon_recovery_ease_in);
        if (d != 0.0) aim.roll_about_forward(d);
    } else {
        aim_rest = 0.0;
        armed = false;
        recov.reset();
        prev_path = glm::dvec3{0.0};  // v13d: no stale rate on re-entry
    }
}

// Convenience overload for callers that carry the packed state.
inline void rest_horizon_tick(input::AimFrame& aim, RestHorizonState& s,
                              bool enabled, bool aim_moved,
                              const glm::dvec3& vel, const glm::dvec3& up,
                              const control::ControllerParams& cp, double dt) {
    rest_horizon_tick(aim, s.aim_rest, s.armed, s.recov, s.prev_path, enabled,
                      aim_moved, vel, up, cp, dt);
}

}  // namespace app
