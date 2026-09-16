#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "render/audio_dsp.h"

// THE STOPE ECHO.
//
// Chad's written spec, D:/audio_tracks/sound_effects/readme.txt:
//
//     "Black Stope explosion for sound effect ambience in the tunnel.
//      Make all sounds like guns echo when in the stope as well."
//
// Read literally: it is not only the blast that belongs to the rock — the
// things YOU make down there (the guns, the hits, the engine) must come back
// off the walls too. Nothing in this codebase had any reverb at all, so this
// header is it: a Schroeder/Moorer feedback network sized for a large blasted
// chamber, applied to the hand-fed synth streams while, and only while, the
// tunnel-net predicate says you are inside.
//
// WHICH CHANNELS. The guns and the combat one-shots (the sounds Chad named) and
// the engine, because an engine in a rock chamber is the single loudest
// confirmation that you are in one.
//
// ⚠ THREE EXCLUSIONS, all of them a reading of "all sounds" that Chad has NOT
// signed off. Surface them as one ruling; each is one line to undo:
//   - the WIND bed: reverberating broadband noise is inaudible as reverb (it is
//     already diffuse) and would only smear the speed cue the wind synth exists
//     to carry.
//   - the MUSIC: score, not something happening in the room.
//   - `sfx/black_stope_explosion.wav`: it is a RECORDED cave blast that already
//     carries the rock it was recorded in, and it plays through raylib's Sound
//     path (PlaySound), which hands us no buffer to process. Routing it through
//     a hand-fed AudioStream would put it in this room too — but it would then
//     be a room inside a room. This is the exclusion most worth asking about,
//     since Chad names that blast in the same sentence as the echo.
//
// Cosmetic, render-side only -- processes audio buffers in place, reads nothing
// from sim/control.
// PURE: no raylib, no audio device, no window, no allocation, no <random>, no
// wall-clock. app/main.cpp owns every raylib call and merely hands this the
// buffer the synth just rendered.
// Firewall: only <algorithm>, <cmath>, <cstdint>, render/audio_dsp.h.

namespace render {

// ---------------------------------------------------------------------------
// The room.
//
// Every delay below is a TIME, in seconds, so the geometry of the chamber is
// legible and survives a sample-rate change; the sample counts are derived in
// init(). The four comb delays are mutually incommensurate (no ratio near a
// small integer) -- equal or harmonically related delays sum into a ringing
// pitch instead of a diffuse tail, which is the classic way a reverb ends up
// sounding like a metal pipe rather than a room.
//
// Sized against the real thing: a blasted stope is tens of metres across, so
// the first reflections land ~25 ms out (predelay) and the tail runs a couple
// of seconds. Nothing here is tuned by loudness numbers -- it is a room, and
// the room's size is the dial.
// ---------------------------------------------------------------------------

inline constexpr int kStopeCombCount = 4;

inline constexpr double kStopePredelaySec = 0.025;

inline constexpr double kStopeCombSec[kStopeCombCount] = {0.0430, 0.0473,
                                                          0.0519, 0.0571};

// Two series allpass diffusers smear the comb pattern into something that has
// no audible repeat rate. Short, and much shorter than any comb.
inline constexpr int kStopeAllpassCount = 2;
inline constexpr double kStopeAllpassSec[kStopeAllpassCount] = {0.0050, 0.0017};
inline constexpr double kStopeAllpassG = 0.70;

// Reverberation time [s] -- how long the tail takes to fall 60 dB. The comb
// feedbacks are DERIVED from this and their own delay, so re-sizing the room
// is one number rather than four hand-tuned gains.
inline constexpr double kStopeRT60 = 2.60;

// Damping: rock and air both eat the top end, so each pass round a comb loop is
// low-passed. Without this the tail is a bright metallic hiss that sits on top
// of the mix instead of behind it.
inline constexpr double kStopeDampHz = 2200.0;

// Wet level at full immersion, and how much of the dry signal steps aside to
// make room for it. Dry is only partly pulled down: the gun still cracks in
// your ear, it just now has a room behind it.
inline constexpr double kStopeWetMax = 0.60;
inline constexpr double kStopeDryDuck = 0.35;

// ⚠ INPUT TRIM INTO THE COMB BANK — this is a clipping fix, not a taste dial.
//
// A feedback comb's steady-state gain AT ITS RESONANCE is 1/(1-g), and g here
// is 0.86-0.89, so each comb rings up ~8x on a sustained tone parked on one of
// its ~23 Hz harmonics. Divided by four and mixed at kStopeWetMax that measured
// peak/input = 1.79 — i.e. HALF the samples railing at the int16 clamp on a
// full-scale drone. The channel this lands on is the bagpipe throttle voice,
// which is sustained AND sweeps its pitch, so it slides through those
// resonances by design. Freeverb applies ~0.015 here; this applies a tenth of
// its RT60 in a room ten times the size, so 0.5 plus the soft clip below is the
// pair that holds it. Transients (the guns, the blast) never build up like this
// — the drone is the case that needs it.
inline constexpr double kStopeCombInputTrim = 0.50;

// Soft-clip knee. Below this the output is EXACTLY the linear mix (so the dry
// crack is untouched); above it, a tanh knee bounded at 1.0. A hard clamp on a
// reverb tail is a buzz; this is a limiter.
inline constexpr double kStopeSoftKnee = 0.70;

// How fast the room arrives and leaves [s]. Slower on exit than on entry, the
// same asymmetry the music crossfade uses and for the same reason: flying into
// a portal is an event, flying out is a release.
inline constexpr double kStopeWetEnterTau = 1.20;
inline constexpr double kStopeWetExitTau = 2.20;

// Below this the wet mix is snapped to EXACTLY zero and the network is cleared
// and bypassed. That matters more than the handful of cycles it saves: it makes
// "once the room has faded out, the audio path is bit-identical to before this
// feature existed" a provable property rather than an approximate one.
//
// Note the "once it has faded out": during the ~2.2 s exit slew the signal IS
// still being processed, because that fade is the room leaving. The bypass is a
// claim about the steady state outside the net, not about the portal itself.
inline constexpr double kStopeWetEpsilon = 1e-4;

// The design sample rate. Every hand-fed stream in app/main.cpp is opened as
// LoadAudioStream(22050, 16, 1), so the delay lines are capacity-sized from
// this rate. init() derives the actual lengths from the rate it is given and
// clamps them to that capacity -- a higher rate therefore yields a SMALLER room
// (at 44.1 kHz every line clamps and the room halves in duration) rather than a
// buffer overrun. The comb feedbacks are re-derived from the POST-clamp delay,
// so the clamped room still decays over the right wall-clock time.
inline constexpr double kStopeDesignSR = 22050.0;

// Capacity of each line, in samples, at the design rate (+1 so a delay of
// exactly N samples is representable).
inline constexpr int stope_cap(double sec) {
    return static_cast<int>(sec * kStopeDesignSR) + 2;
}

// Bounded soft clip: identity below the knee, tanh knee above it, |out| < 1
// always. Continuous and monotone at the knee (d/dx tanh(0) == 1).
inline double stope_soft_clip(double x) {
    const double k = kStopeSoftKnee;
    const double a = std::abs(x);
    if (a <= k) return x;
    const double over = (a - k) / (1.0 - k);
    const double y = k + (1.0 - k) * std::tanh(over);
    return (x < 0.0) ? -y : y;
}

// ---------------------------------------------------------------------------
// The network.
//
// Fixed-size arrays, no allocation anywhere: this runs on the audio refill path
// beside the synths, which have the same rule.
// ---------------------------------------------------------------------------

struct StopeReverb {
    static constexpr int kPreCap = stope_cap(kStopePredelaySec);
    static constexpr int kCombCap0 = stope_cap(kStopeCombSec[0]);
    static constexpr int kCombCap1 = stope_cap(kStopeCombSec[1]);
    static constexpr int kCombCap2 = stope_cap(kStopeCombSec[2]);
    static constexpr int kCombCap3 = stope_cap(kStopeCombSec[3]);
    static constexpr int kApCap0 = stope_cap(kStopeAllpassSec[0]);
    static constexpr int kApCap1 = stope_cap(kStopeAllpassSec[1]);

    // Delay lines. float rather than double: these are ~5000 samples across
    // three instances and live on the stack in main(), and a 24-bit mantissa is
    // ~50 dB below the 16-bit signal they carry.
    float pre_[kPreCap]{};
    float comb0_[kCombCap0]{};
    float comb1_[kCombCap1]{};
    float comb2_[kCombCap2]{};
    float comb3_[kCombCap3]{};
    float ap0_[kApCap0]{};
    float ap1_[kApCap1]{};

    int pre_len_ = 0, pre_i_ = 0;
    int comb_len_[kStopeCombCount]{};
    int comb_i_[kStopeCombCount]{};
    double comb_g_[kStopeCombCount]{};
    double comb_lp_[kStopeCombCount]{};  // damping filter state, one per comb
    double damp_a_ = 0.0;                // damping one-pole coefficient
    int ap_len_[kStopeAllpassCount]{};
    int ap_i_[kStopeAllpassCount]{};

    double wet_ = 0.0;  // current wet mix, slewed toward the target
    bool ready_ = false;

    // SEND-BUS MODE. false (default) = an INSERT: dry passes through with a
    // small duck and the room is added behind it, which is what the gun, sfx
    // and engine streams want. true = the output is the ROOM ONLY, for a
    // channel whose dry signal is already being heard somewhere else.
    //
    // That is how the black-stope blast echoes (Chad, 2026-08-17: "yes echos
    // the stope explosion itself"): raylib plays the recorded blast dry, in
    // full stereo at its own rate, and a mono copy is fed here to supply the
    // tail. Sending rather than inserting is what lets the dry blast keep the
    // stereo quality Chad asked for while still coming back off the walls.
    bool wet_only = false;
    // Set whenever a sample has been pushed through the lines; cleared by
    // clear(). Without it the bypass path would memset ~20 kB per buffer per
    // stream for the entire flight, every frame you are NOT in a tunnel --
    // which is nearly all of them.
    bool dirty_ = false;

    // Line lengths and the derived feedbacks. Safe to call again (it clears).
    void init(double sr) {
        const double rate = (sr > 0.0) ? sr : kStopeDesignSR;
        auto len = [rate](double sec, int cap) {
            int n = static_cast<int>(sec * rate + 0.5);
            return std::clamp(n, 1, cap - 1);
        };
        pre_len_ = len(kStopePredelaySec, kPreCap);
        const int caps[kStopeCombCount] = {kCombCap0, kCombCap1, kCombCap2,
                                           kCombCap3};
        for (int c = 0; c < kStopeCombCount; ++c) {
            comb_len_[c] = len(kStopeCombSec[c], caps[c]);
            // g such that the loop decays 60 dB in kStopeRT60 seconds, using
            // the delay the line ACTUALLY got (post-clamp), so a clamped line
            // still decays over the right wall-clock time.
            const double d = static_cast<double>(comb_len_[c]) / rate;
            comb_g_[c] = std::clamp(std::pow(10.0, -3.0 * d / kStopeRT60), 0.0,
                                    0.98);
        }
        const int ap_caps[kStopeAllpassCount] = {kApCap0, kApCap1};
        for (int a = 0; a < kStopeAllpassCount; ++a)
            ap_len_[a] = len(kStopeAllpassSec[a], ap_caps[a]);
        damp_a_ = std::exp(-2.0 * kAudioPI * kStopeDampHz / rate);
        ready_ = true;
        clear();
    }

    void clear() {
        for (int i = 0; i < kPreCap; ++i) pre_[i] = 0.0f;
        for (int i = 0; i < kCombCap0; ++i) comb0_[i] = 0.0f;
        for (int i = 0; i < kCombCap1; ++i) comb1_[i] = 0.0f;
        for (int i = 0; i < kCombCap2; ++i) comb2_[i] = 0.0f;
        for (int i = 0; i < kCombCap3; ++i) comb3_[i] = 0.0f;
        for (int i = 0; i < kApCap0; ++i) ap0_[i] = 0.0f;
        for (int i = 0; i < kApCap1; ++i) ap1_[i] = 0.0f;
        pre_i_ = 0;
        for (int c = 0; c < kStopeCombCount; ++c) {
            comb_i_[c] = 0;
            comb_lp_[c] = 0.0;
        }
        for (int a = 0; a < kStopeAllpassCount; ++a) ap_i_[a] = 0;
        dirty_ = false;
    }

    // Once per frame: `inside` is app::inside_tunnel(env, pos) -- the SAME
    // single-source predicate the camera and the music director read, so the
    // room can never disagree with the bed about where the rock is.
    void set_inside(bool inside, double dt) {
        const double target = inside ? 1.0 : 0.0;
        const double tau = inside ? kStopeWetEnterTau : kStopeWetExitTau;
        wet_ += (target - wet_) * audio_slew_coef(dt, tau);
        wet_ = std::clamp(wet_, 0.0, 1.0);
        if (wet_ < kStopeWetEpsilon) wet_ = 0.0;
    }

    double wet() const { return wet_; }

    // True while the network is doing nothing at all -- the bypass state that
    // makes the outside-the-net path bit-identical.
    bool bypassed() const { return !ready_ || wet_ <= 0.0; }

    // Process one mono int16 buffer IN PLACE, exactly as the synths render it.
    void process(short* buf, int n) {
        if (buf == nullptr || n <= 0) return;
        if (bypassed()) {
            // Nothing to add, and no tail worth carrying: drop the state so a
            // re-entry starts from an empty room rather than from a stale one.
            // Once, on the transition -- not every buffer for the whole flight.
            if (ready_ && dirty_) clear();
            // ⚠ A SEND bypasses to SILENCE, never to its input. Returning the
            // buffer untouched here would leak a second, dry copy of the blast
            // into the mix the moment the room faded out -- the one failure
            // mode a send bus has that an insert does not.
            if (wet_only)
                for (int i = 0; i < n; ++i) buf[i] = 0;
            return;
        }
        dirty_ = true;
        const double w = wet_;
        const double dry = wet_only ? 0.0 : (1.0 - kStopeDryDuck * w);
        const double wet_g = kStopeWetMax * w;
        float* combs[kStopeCombCount] = {comb0_, comb1_, comb2_, comb3_};
        float* aps[kStopeAllpassCount] = {ap0_, ap1_};

        for (int i = 0; i < n; ++i) {
            const double x = static_cast<double>(buf[i]) / 32768.0;

            // Predelay: the walls are not in your lap.
            const double pd = static_cast<double>(pre_[pre_i_]);
            pre_[pre_i_] = static_cast<float>(x);
            if (++pre_i_ >= pre_len_) pre_i_ = 0;

            // Four damped feedback combs in parallel = the tail.
            double sum = 0.0;
            for (int c = 0; c < kStopeCombCount; ++c) {
                float* line = combs[c];
                const double y = static_cast<double>(line[comb_i_[c]]);
                sum += y;
                // Damp inside the loop, so each round trip loses more top end.
                comb_lp_[c] = damp_a_ * comb_lp_[c] + (1.0 - damp_a_) * y;
                line[comb_i_[c]] = static_cast<float>(
                    kStopeCombInputTrim * pd + comb_g_[c] * comb_lp_[c]);
                if (++comb_i_[c] >= comb_len_[c]) comb_i_[c] = 0;
            }
            double v = sum * 0.25;

            // Two allpass diffusers in series = no audible repeat rate.
            for (int a = 0; a < kStopeAllpassCount; ++a) {
                float* line = aps[a];
                const double y = static_cast<double>(line[ap_i_[a]]);
                const double in = v + kStopeAllpassG * y;
                line[ap_i_[a]] = static_cast<float>(in);
                if (++ap_i_[a] >= ap_len_[a]) ap_i_[a] = 0;
                v = y - kStopeAllpassG * in;
            }

            const double out = stope_soft_clip(dry * x + wet_g * v);
            const double s = std::clamp(out * 32767.0, -32768.0, 32767.0);
            buf[i] = static_cast<short>(s);
        }
    }
};

}  // namespace render
