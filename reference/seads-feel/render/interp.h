#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include "sim/sled.h"
#include "sim/state.h"
#include "sim/walker.h"

// Render-side interpolation between the last two fixed-dt states (SPEC §10:
// "rendering interpolates between the last two states"). STRICTLY one-way:
// the result is drawn and discarded — it never re-enters sim/ or (later)
// control/, so it is not a smoothing stage in any signal path (HARNESS §6
// loop-integrity: the smoothed thing here is pixels, not a basis).

namespace render {

inline sim::SimState interpolate(const sim::SimState& prev,
                                 const sim::SimState& curr, double alpha) {
    sim::SimState out = curr;  // scalars/guards: newest wins, draw-only
    out.position = glm::mix(prev.position, curr.position, alpha);
    out.velocity = glm::mix(prev.velocity, curr.velocity, alpha);
    out.orientation = glm::slerp(prev.orientation, curr.orientation, alpha);
    return out;
}

// ★ CAM-SMOOTH (2026-09-08): the SLED and the WALKER get the SAME draw-side
// interpolation the aeroplane has had since SPEC §10. Both are stepped on the
// 120 Hz fixed tick and were drawn RAW -- so every frame that carried 1 or 3
// ticks instead of 2 (about one frame in forty at a clean 60 Hz vsync, and
// every frame once the display and the tick beat against each other) hopped
// the machine, the man, and the camera anchored on them by half a tick of
// travel. That hop was the "jitter". Same contract as interpolate() above:
// drawn and discarded, never re-entering sim/. Continuous quantities mix;
// discrete/guard scalars take the newest.
inline sim::SledState interpolate_sled(const sim::SledState& prev,
                                       const sim::SledState& curr,
                                       double alpha) {
    sim::SledState out = curr;
    out.position = glm::mix(prev.position, curr.position, alpha);
    out.velocity = glm::mix(prev.velocity, curr.velocity, alpha);
    out.orientation = glm::slerp(prev.orientation, curr.orientation, alpha);
    for (int i = 0; i < sim::kPatches; ++i) {
        out.susp_x[i] = glm::mix(prev.susp_x[i], curr.susp_x[i], alpha);
        out.susp_v[i] = glm::mix(prev.susp_v[i], curr.susp_v[i], alpha);
        out.sink_m[i] = glm::mix(prev.sink_m[i], curr.sink_m[i], alpha);
    }
    out.rider_lat_m = glm::mix(prev.rider_lat_m, curr.rider_lat_m, alpha);
    out.rider_fwd_m = glm::mix(prev.rider_fwd_m, curr.rider_fwd_m, alpha);
    out.rider_up_m = glm::mix(prev.rider_up_m, curr.rider_up_m, alpha);
    out.steer_actual = glm::mix(prev.steer_actual, curr.steer_actual, alpha);
    return out;
}

inline sim::WalkerState interpolate_walker(const sim::WalkerState& prev,
                                           const sim::WalkerState& curr,
                                           double alpha) {
    sim::WalkerState out = curr;
    out.pos = glm::mix(prev.pos, curr.pos, alpha);
    out.vel = glm::mix(prev.vel, curr.vel, alpha);
    // The heading is a unit world tangent (sim/walker.h): nlerp, renormalized,
    // and a zero/degenerate blend falls back to the newest (a heading flip
    // through exactly 180 deg has no defined midpoint; keep it discrete).
    const glm::dvec3 h = glm::mix(prev.heading, curr.heading, alpha);
    const double hl = glm::length(h);
    out.heading = hl > 1e-6 ? h / hl : curr.heading;
    return out;
}

// ★ CAM-SMOOTH: the sled camera anchor's first-order lag, ONE discrete step.
// Pure so the fixed point is pinned by test_interp.cpp (the red-team of
// 2026-09-09 found the first cut's feed-forward, ff = dt/kp, was the fixed
// point of the WRONG recurrence and LED the body by v*dt every frame).
//
// The camera is posed against the body drawn THIS frame, so the update is
//     a_n = a_{n-1} + kp * (p_n + v*ff - a_{n-1}),   p_n - p_{n-1} = v*dt.
// Steady state a_n = p_n + c gives c = v * (dt + ff - dt/kp), which is zero
// only when ff = dt * (1/kp - 1). That ff -> tau - dt/2 as dt -> 0, and -> 0
// when tau == 0 (kp == 1), so tau 0 IS the welded camera, exactly.
// kp = 1 - exp(-dt/tau) is in (0, 1] for every dt >= 0, so the step is
// unconditionally stable, including the 0.25 s frame clamp.
inline glm::dvec3 cam_lag_step(const glm::dvec3& anchor, const glm::dvec3& pos,
                               const glm::dvec3& vel, double dt, double tau) {
    if (!(tau > 0.0) || !(dt > 0.0)) return pos;
    const double kp = 1.0 - std::exp(-dt / tau);
    const double ff = dt * (1.0 / kp - 1.0);
    return anchor + (pos + vel * ff - anchor) * kp;
}

}  // namespace render
