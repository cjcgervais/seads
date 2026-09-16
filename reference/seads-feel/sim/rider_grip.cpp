#include "sim/rider_grip.h"

#include <cmath>


namespace sim {

float buck_step(BuckMemory& m, const BuckParams& p, float g_eff_mag,
                float dt_s) {
    // OFF is bit-identical: no charge, no decay, and a divisor of exactly 1.
    if (!(p.buck_gain > 0.0f)) {
        m.mem = 0.0f;
        return 1.0f;
    }
    // DISCHARGE FIRST, then peak-hold against this step's buck. The order
    // matters at the instant of the hit: charging first and decaying after
    // would shave one step off the very peak the mechanism is built to
    // remember, which is small but is exactly the kind of quiet asymmetry
    // that makes a measured calibration stop describing the shipped code.
    if (dt_s > 0.0f) {
        m.mem -= p.decay_per_s * dt_s;
        if (m.mem < 0.0f) m.mem = 0.0f;
    }
    const float excess = g_eff_mag - p.g0;
    if (excess > 0.0f) {
        const float charge = p.buck_gain * excess;
        if (charge > m.mem) m.mem = charge;
    }
    return 1.0f + m.mem;
}

void grip_step(GripState& s, const GripParams& gp, const BuckParams& bp,
               const GripStep& in) {
    // The memory is stepped whatever the gains are. At buck_gain 0 it is a
    // no-op returning exactly 1.0f, so this costs nothing when off.
    //
    // ⚠⚠ AND THEY DO NOT SEE THE SAME HISTORY. An earlier version of this
    // comment claimed "the kernel's copy and the drawn body's copy see the same
    // history"; a red-team refuted it and it is wrong twice over. The kernel
    // steps this per SUBSTEP (1440 Hz) off an instantaneous `force / mass`;
    // render steps it ONCE PER FRAME with one tick's dt regardless of how many
    // ticks that frame consumed, off a frame finite-difference. So at 60 fps on
    // a 120 Hz sim the drawn body's decay runs at roughly half the signed rate,
    // and the kernel sees peaks the chain never does. SHARED FUNCTION IS NOT
    // SHARED HISTORY: what the move down here buys is one LAW with one set of
    // constants, not two synchronised signals -- and any claim that depends on
    // them agreeing sample-for-sample is false until the rates are reconciled.
    // (The frame quantisation is pre-existing and shipped; only the claim was
    // new.)
    const double div = static_cast<double>(
        buck_step(s.mem, bp, static_cast<float>(in.g_eff_mag),
                  static_cast<float>(in.dt_s)));

    // ★ OFF IS STRUCTURAL, NOT APPROXIMATE. At unseat_gain 0 the charge is 0
    // and the return term is a decay on a state that starts at 0 and can never
    // leave it, so extension is identically 0 and the load with it -- not
    // "small", zero, for every input.
    if (in.dt_s > 0.0) {
        const double excess = in.g_eff_mag - static_cast<double>(bp.g0);
        const double charge = excess > 0.0 ? gp.unseat_gain * excess : 0.0;
        // The SAME pull-back the drawn body runs, softened by the SAME memory:
        // a spring whose rate is divided by (1 + mem) is a spring that
        // remembers how hard he was hit, which is the whole of Chad's ruling.
        const double ret = (gp.pose_hz / div) * s.extension_m;
        s.extension_m += (charge - ret) * in.dt_s;
        // A body cannot be pulled back THROUGH the pose by its own spring; the
        // explicit step can overshoot at a big dt and a negative extension is
        // not a thing that exists.
        if (s.extension_m < 0.0) s.extension_m = 0.0;
    }

    // ★★★ THE LOAD IS A PRODUCT, AND THE PRODUCT IS THE WHOLE MECHANISM. Chad:
    // "the hardness of the landing casue for letting go or the bars". A hard
    // landing while he is SEATED goes through his legs and the seat and he
    // keeps hold -- that is an ordinary landing. The same landing while he is
    // STRETCHED OUT has nowhere to go but his arms. A SUM would throw a seated
    // man off, and a seated man is not who he described.
    s.load = in.g_eff_mag * s.extension_m;

    // ★ AND THE FILTERED LOAD, WHICH IS WHAT THE CAPACITY IS COMPARED TO. One
    // pole, the house 0.1 s. At tau 0 it is the raw load exactly, so the
    // pre-filter kernel is one dial away and the A/B is bit-identical.
    if (gp.load_tau_s > 0.0 && in.dt_s > 0.0) {
        const double k = 1.0 - std::exp(-in.dt_s / gp.load_tau_s);
        s.load_lp += (s.load - s.load_lp) * k;
    } else {
        s.load_lp = s.load;
    }

    // ★ ONE-WAY. Stage 4 of §7.3 is the one irreversible transition in the
    // chain; stages 0-3 are continuous and reversible. Re-attaching is
    // R4e (the remount), not a threshold falling back below itself.
    if (s.attached && s.load_lp > gp.capacity) s.attached = false;
}

}  // namespace sim
