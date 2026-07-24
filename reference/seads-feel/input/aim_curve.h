#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "control/params.h"

// MB-aim (SPEC §0, docs/mb_aim_plan.md): the rate-keyed mouse acceleration
// curve — fine precision at small hand movements, accelerating gain for
// flicks. A MEMORYLESS pure gain shape (§9.1's ban is on smoothing, not
// shaping — the S9-zoom aim_gain_scale class: a scalar magnitude on the raw
// delta, never a basis smoothing), keyed on the frame-rate-NORMALIZED mouse
// rate (px/s = |delta| / frame_dt), NEVER the raw per-frame delta magnitude —
// a per-frame-delta curve would make the same physical hand motion rotate
// differently at 30 vs 240 fps (the S7-mouselevel frame-quantization class).
//
// It lives at the DEVICE ACCRUAL boundary (main.cpp's `pending_ +=` site) —
// the ONE place the per-frame delta and its true frame_dt coexist. Downstream
// (pending at consume time) a carried delta may span several frames (the
// 0-tick carry), so a consume-time curve would window the rate wrongly and
// re-open the frame-rate dependence. AT-9's injection model (deltas straight
// into pending_*) is therefore untouched: the tick loop stays bit-identical
// across frame rates given delivered deltas; THIS function's own frame-rate
// invariance is pinned separately (test_aim_curve, power-of-two scale leg).
//
// Plan-stage red-team fixes folded (mb_aim_plan.md §7 ledger):
//  P1-1  callers pass the RAW frame_dt (GetFrameTime), NEVER main.cpp's
//        clamped_dt — the sim-stall clamp under-reports elapsed wall time and
//        would convert a 2 s hitch of slow tracking into a spurious flick.
//        kDtFloor is the in-function totality guard (the vMin discipline).
//  P1-3  the quantization guard: at high/uncapped fps a constant SLOW hand
//        rate quantizes to 1-2 px frames whose estimated rate (~px/frame_dt)
//        is fps-scaled noise — without the guard a 1 px micro-adjust at
//        500 fps reads 500 px/s and accelerates inside the precision zone.
//        Deltas <= quant_px never curve; real flicks are >= 5 px/frame even
//        at 1000 fps. HONEST SCOPE: the residual (a delta of quant_px+1 at
//        extreme fps still over-reads its rate) is sampling noise inherent to
//        a per-frame device poll, bounded by the knee headroom above the
//        240 fps 1-2 px band.
//  P2-1  the knob-off arm (gain_max <= 1) returns FIRST, structurally —
//        bit-identity by construction (no sqrt, no -0.0 or denormal
//        reasoning), the S9-zoom strict-superset shape.

namespace input {

// Totality floor for the rate division [s] (a 0-dt frame cannot manufacture
// an infinite rate). Callers pass the RAW frame_dt; this is the guard.
inline constexpr double kAimCurveDtFloor = 1e-4;

inline glm::dvec2 aim_curve(double dx, double dy, double frame_dt,
                            const control::ControllerParams& p) {
    // Knob-off arm FIRST (P2-1): gain_max <= 1 is the pure linear gain,
    // bit-identical, with none of the curve's arithmetic executed.
    if (p.aim_curve_gain_max <= 1.0) return {dx, dy};
    const double mag = std::sqrt(dx * dx + dy * dy);
    // Quantization guard (P1-3): sub-few-pixel deltas carry fps-scaled rate
    // noise, never real flick speed. Also the mag == 0 no-op.
    if (mag <= p.aim_curve_quant_px) return {dx, dy};
    const double rate = mag / std::max(frame_dt, kAimCurveDtFloor);  // px/s
    // Below the knee the gain is EXACTLY 1 (branch untaken): the precision
    // zone is today's pure linear aim_sensitivity, untouched.
    if (rate <= p.aim_curve_knee) return {dx, dy};
    const double t = std::min(
        (rate - p.aim_curve_knee) / (p.aim_curve_rate_hi - p.aim_curve_knee),
        1.0);  // capped: a wild flick cannot spin the aim unboundedly
    const double g =
        1.0 + (p.aim_curve_gain_max - 1.0) * std::pow(t, p.aim_curve_expo);
    // ONE shared scalar gain (vector-magnitude rate): a diagonal flick curves
    // uniformly — per-axis curves would bend the delta's DIRECTION.
    return {dx * g, dy * g};
}

}  // namespace input
