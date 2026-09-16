#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Shared real-time audio DSP primitives for the render-side procedural sound
// (wind_synth.h, bagpipe_throttle.h, ...). Cosmetic, read-only; no state touches
// sim/control. Kept tiny and header-only — no allocations, no <random>.

namespace render {

inline constexpr double kAudioPI = 3.14159265358979323846;

// One-pole slew coefficient for time constant tau over a step dt: the fraction
// of the remaining distance to cover this step. Clamped so a hitched frame can
// overshoot neither end, and so dt <= 0 (a paused/first frame) moves nothing.
//
// SINGLE SOURCE for the per-frame slews in this layer — music_director.h's
// music_slew() and stope_reverb.h's wet ramp are the same law, and a second
// hand-rolled copy is exactly the fork H1 forbids.
inline double audio_slew_coef(double dt, double tau) {
    if (!(dt > 0.0) || !(tau > 0.0)) return 0.0;
    return std::clamp(1.0 - std::exp(-dt / tau), 0.0, 1.0);
}

struct WSXor {  // white noise, xorshift PRNG
    uint64_t s = 0x9E3779B97F4A7C15ull;
    double w() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<double>(static_cast<int32_t>(s)) / 2147483648.0;
    }
    double uni() { return 0.5 * (w() + 1.0); }  // [0,1)
};

struct WSPink {  // Paul Kellet pinking filter -> soft, natural (not hissy)
    double b0 = 0, b1 = 0, b2 = 0;
    double process(double white) {
        b0 = 0.99765 * b0 + white * 0.0990460;
        b1 = 0.96300 * b1 + white * 0.2965164;
        b2 = 0.57000 * b2 + white * 1.0526913;
        return (b0 + b1 + b2 + white * 0.1848) * 0.11;  // ~unit variance
    }
};

struct WSOnePole {  // low-pass; high-pass via x - lp (own state)
    double a = 0, y = 0;
    void set(double cutoff, double sr) {
        a = std::exp(-2.0 * kAudioPI * cutoff / sr);
    }
    double lp(double x) {
        y = a * y + (1.0 - a) * x;
        return y;
    }
};

struct WSBiquad {  // RBJ band-pass (constant skirt gain)
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void set_bp(double f0, double q, double sr) {
        const double w0 = 2.0 * kAudioPI * f0 / sr;
        const double al = std::sin(w0) / (2.0 * q);
        const double c = std::cos(w0);
        const double a0 = 1.0 + al;
        b0 = al / a0;
        b1 = 0.0;
        b2 = -al / a0;
        a1 = -2.0 * c / a0;
        a2 = (1.0 - al) / a0;
    }
    double process(double x) {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

struct WSsvf {  // Chamberlin state-variable band-pass (time-varying fc/Q)
    double low = 0, band = 0;
    double bp(double x, double f, double qc) {  // f=2 sin(pi fc/sr), qc=1/Q
        low += f * band;
        const double high = x - low - qc * band;
        band += f * high;
        return band;
    }
};

}  // namespace render
