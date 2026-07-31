#pragma once

#include <algorithm>
#include <cmath>

// The fixed-timestep accumulator (SPEC §10, "Fix Your Timestep"). PURE and
// header-only so the gate can test it without a window: the sim (and later
// the controller) advance ONLY in whole sim_dt ticks; the fractional
// remainder becomes the render interpolation alpha. Frame-rate variance in
// gains is structurally impossible because frame time never reaches the
// plant — AT-9 verifies the wiring anyway.

namespace app {

class Accumulator {
   public:
    // max_frame_dt caps a single frame's contribution (debugger pause, GPU
    // hitch): the classic spiral-of-death guard. Sim time falls behind wall
    // time in that frame, deliberately.
    explicit Accumulator(double sim_dt, double max_frame_dt = 0.25)
        : sim_dt_(sim_dt), max_frame_dt_(max_frame_dt) {}

    // Feed one frame's wall dt; returns how many fixed ticks to step now.
    int advance(double frame_dt) {
        acc_ += std::clamp(frame_dt, 0.0, max_frame_dt_);
        const int ticks = static_cast<int>(std::floor(acc_ / sim_dt_));
        // max(): the subtraction can round one ulp below zero when the
        // division rounded up across an integer — keep the [0, sim_dt)
        // contract exact, not just approximate.
        acc_ = std::max(acc_ - ticks * sim_dt_, 0.0);
        return ticks;
    }

    // Fraction of a tick left over -> interpolation between the last two
    // states. In [0, 1) — min() pins the one-ulp rounding case where the
    // remainder divides to exactly 1.0.
    double alpha() const {
        return std::min(acc_ / sim_dt_, 0x1.fffffffffffffp-1);
    }

    double sim_dt() const { return sim_dt_; }

   private:
    double sim_dt_;
    double max_frame_dt_;
    double acc_ = 0.0;
};

}  // namespace app
