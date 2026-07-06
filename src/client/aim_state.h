// SEADS mouse-aim instructor — the AIM STATE (SOLUTION §3.1 / SPEC §9.1, grafted onto SEADS).
//
// The aim is NOT a screen offset. It is a WORLD-FRAME unit vector `forward` (where the pilot wants
// the nose), carried in a small orthonormal aim frame {forward, up, right} so mouse deltas rotate
// it about the frame's OWN axes — a raw, pole-free basis (a basis built from local_up degenerates
// exactly at the zenith aim this representation exists to keep legal).
//
// Two rules from the spec, load-bearing:
//   * Parallel transport EVERY tick, EVERY mode (including a freelook-held aim): rotate the frame by
//     the minimal rotation taking local_up(t-1) -> local_up(t). A world-frozen aim on R = 15 km
//     drifts off the horizon in seconds otherwise.
//   * NOTHING smoothed touches mouse -> aim. Deltas rotate the raw frame; the tracking error the
//     instructor consumes is recomputed fresh from `forward` vs the nose each tick.
//
// Client-side, presentation-only (libm ok). Header-only pure state + tiny methods.
#pragma once
#include <cmath>
#include "client_frame.h"   // Vec3, rotate_about, transport, normalize, cross, dot

namespace seads {
namespace client {

class AimState {
public:
    // Seed the aim on the nose in the current frame. `up` is the aircraft's local_up at seed time
    // (the aim frame's up starts aligned with it; it then carries its own holonomy).
    void seed(Vec3 nose, Vec3 up) {
        fwd_ = normalize(nose);
        // Build a right/up for the aim frame from the seed up, re-orthogonalized against fwd.
        right_ = cross(fwd_, up);
        if (length(right_) < 1e-9) right_ = cross(fwd_, Vec3{0, 1, 0});  // fallback if nose ~ up
        right_ = normalize(right_);
        up_ = cross(right_, fwd_);
        last_up_valid_ = false;
    }

    // Carry the aim frame by parallel transport for this tick's local_up change. Call once per tick
    // in EVERY mode. `local_up` is the aircraft's current radial up.
    void carry(Vec3 local_up) {
        if (last_up_valid_) {
            fwd_   = normalize(transport(fwd_,   last_up_, local_up));
            up_    = normalize(transport(up_,    last_up_, local_up));
            right_ = normalize(transport(right_, last_up_, local_up));
            reorthonormalize_();
        }
        last_up_ = local_up;
        last_up_valid_ = true;
    }

    // Apply a raw mouse delta (radians): dx yaws the aim about the frame's UP axis (ruled over
    // local_up so the zenith stays legal); dy pitches about the frame's RIGHT axis. dy > 0 = aim up.
    void mouse(double dx, double dy) {
        fwd_   = rotate_about(fwd_,   up_,    -dx);
        right_ = rotate_about(right_, up_,    -dx);
        fwd_   = rotate_about(fwd_,   right_, -dy);
        up_    = rotate_about(up_,    right_, -dy);
        reorthonormalize_();
    }

    // Snap the aim onto the nose (freelook-with-override release, override-in-freelook, crash reset).
    void snap_to_nose(Vec3 nose) { seed(nose, up_); }

    Vec3 forward() const { return fwd_; }
    Vec3 up() const { return up_; }
    Vec3 right() const { return right_; }

private:
    void reorthonormalize_() {
        fwd_ = normalize(fwd_);
        right_ = normalize(right_ - fwd_ * dot(right_, fwd_));
        up_ = cross(right_, fwd_);
    }

    Vec3 fwd_{1, 0, 0}, up_{0, 1, 0}, right_{0, 0, 1};
    Vec3 last_up_{0, 1, 0};
    bool last_up_valid_ = false;
};

}  // namespace client
}  // namespace seads
