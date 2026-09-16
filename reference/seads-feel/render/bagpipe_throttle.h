#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "render/audio_dsp.h"  // shared DSP primitives (WSOnePole, kAudioPI)

// Real-time THROTTLE voice — a highland-bagpipe DRONE whose PITCH rides the
// throttle (Chad, 2026-07-24: "map bagpipe frequency range — higher pitch of
// the bagpipe drone file is max throttle, then down from there"). This REPLACES
// the old synthesized engine hum (engine_synth.h, re-and-re'd out): full
// throttle plays the drone sped up (higher), and it drops in pitch as the
// throttle comes back. Nothing else moves — it is one sustained, looping drone.
//
// The wind WHISTLES stay put — those live on the wind channel (wind_synth.h),
// untouched by this file.
//
// Cosmetic, READ-ONLY off a throttle snapshot — firewalled like every render
// channel (nothing here feeds sim/control/input). Tunables are code constants
// (engine_synth / wind-audio precedent). PURE DSP: the .wav is decoded by the
// app layer (raylib) and its mono samples handed to init(); this header never
// touches raylib or the disk.

namespace render {

// Pitch (playback-speed) span. 1.0 == the drone at its recorded pitch. Chad
// (2026-07-24) wants it "much lower still", VOLUME (below) doing far more than
// pitch, and the DEEPEST (idle) end lifted a touch — played too slow the three
// drones beat into a slow throb ("choking"), so idle can't sit at the very
// floor. Result: a deep register with only a whisper of pitch rise; the throttle
// is felt almost entirely as the drone swelling louder.
inline constexpr double kBagPipeIdleRatio = 0.42;  // deep drone, just off the choke floor
inline constexpr double kBagPipeFullRatio =
    0.47;  // barely higher at full — pitch is a minor cue

// SOFTENING low-pass (the key to "beautiful, not annoying"): a bagpipe drone's
// nasal buzz lives in its upper reed harmonics. We roll the top well off so the
// drone reads as a warm, round hum; the cutoff opens only a LITTLE with
// throttle so full power gains a touch of presence without getting reedy. Two
// cascaded poles = a smoother, mellower roll-off than one.
inline constexpr double kBagPipeToneIdleHz =
    750.0;  // very warm, muffled at idle
inline constexpr double kBagPipeToneFullHz =
    1500.0;  // a little brighter at full

// Volume is now the MAIN throttle differential (Chad, 2026-07-24: "volume to be
// the main differential ... a bit higher in range"). Wide swing: near-silent at
// idle, full-bodied at redline, so throttle is FELT chiefly as the drone
// swelling in and out rather than changing pitch.
inline constexpr double kBagPipeVolIdle = 0.34;  // quiet but not choking/cutting out
inline constexpr double kBagPipeVolFull = 1.25;  // big, full-bodied swell at full

// Output drive into the soft-clip. Kept UNDER unity so the tanh never adds
// grit — a soft, clean drone, never a saturated snarl.
inline constexpr double kBagPipeOutDrive = 0.85;

// Crossfade length for the seamless loop, as a fraction of the (resampled)
// clip. The drone is near-stationary, so a modest equal-position crossfade
// hides the wrap seam with no audible pumping.
inline constexpr double kBagPipeXfadeSec = 0.40;  // [s] loop-seam crossfade

// Per-sample smoothing time for the pitch ratio so a throttle slam glides in
// pitch instead of zippering.
inline constexpr double kBagPipeRatioGlideSec = 0.12;  // [s]

struct BagpipeThrottle {
    double sr = 22050.0;    // output sample rate
    double throttle = 0.0;  // [0,1] live driver

    // Seamless loop buffer, already resampled to the OUTPUT rate. Playback then
    // only has to apply the throttle pitch ratio on top (kBagPipe*Ratio), a
    // gentle shift that needs no further anti-aliasing beyond linear interp.
    std::vector<float> loop;
    double pos = 0.0;  // fractional read cursor into loop [0, loop.size())
    double ratio = kBagPipeIdleRatio;  // smoothed playback ratio
    double ratio_a = 0.0;              // per-sample glide coefficient
    WSOnePole tone1, tone2;  // 2-pole warmth filter (softens the buzz)
    bool inited = false;

    // Decode-side helper: downmix + resample the app-decoded samples to sr,
    // then build the crossfade loop. `mono_or_interleaved` holds `count` FRAMES
    // of `channels`-interleaved float samples at `src_sr`; we average channels.
    void init(double out_sr, const float* samples, size_t frames, int channels,
              double src_sr) {
        sr = out_sr;
        ratio_a = 1.0 - std::exp(-1.0 / (kBagPipeRatioGlideSec * sr));
        loop.clear();
        if (samples == nullptr || frames == 0 || channels < 1 ||
            src_sr <= 0.0) {
            inited = false;  // caller falls back to silence on this channel
            return;
        }

        // 1) Downmix to mono at the source rate.
        std::vector<float> mono(frames);
        for (size_t i = 0; i < frames; ++i) {
            double acc = 0.0;
            for (int c = 0; c < channels; ++c)
                acc += samples[i * static_cast<size_t>(channels) + c];
            mono[i] = static_cast<float>(acc / channels);
        }

        // 2) Resample to the output rate. A CASCADED one-pole low-pass ahead of
        //    the decimation tames aliasing when src_sr > out_sr (96k
        //    -> 22.05k). The cutoff also accounts for the live up-shift: at
        //    full throttle the loop is read at kBagPipeFullRatio, so content
        //    above out_Nyquist / full_ratio would transpose past Nyquist and
        //    alias — filter to that, not the raw output Nyquist. A single 6
        //    dB/oct pole is far too gentle across a 4.35x decimation, so
        //    cascade a few.
        const double decim = src_sr / sr;  // source samples per output sample
        std::vector<float> pre = mono;
        if (decim > 1.0) {
            // Never above the OUTPUT Nyquist (0.45*sr); and if we ever up-shift
            // (full ratio > 1) drop further so the transposed top stays legal.
            const double cutoff = std::min(
                {0.45 * sr, 0.45 * sr / kBagPipeFullRatio, 0.45 * src_sr});
            constexpr int kAAStages =
                4;  // ~24 dB/oct — kills the fold-back haze
            WSOnePole aa[kAAStages];
            for (int s = 0; s < kAAStages; ++s) aa[s].set(cutoff, src_sr);
            for (size_t i = 0; i < frames; ++i) {
                double x = mono[i];
                for (int s = 0; s < kAAStages; ++s) x = aa[s].lp(x);
                pre[i] = static_cast<float>(x);
            }
        }
        const size_t out_n =
            static_cast<size_t>(static_cast<double>(frames) / decim);
        if (out_n == 0) {
            inited = false;  // clip too short to yield even one output sample
            return;
        }
        std::vector<float> res(out_n, 0.0f);
        for (size_t i = 0; i < out_n; ++i) {
            const double sp = static_cast<double>(i) * decim;
            const size_t i0 = static_cast<size_t>(sp);
            const size_t i1 = (i0 + 1 < frames) ? i0 + 1 : i0;
            const double f = sp - static_cast<double>(i0);
            res[i] = static_cast<float>(pre[i0] * (1.0 - f) + pre[i1] * f);
        }

        // 3) Build a seamless loop via an equal-position crossfade: the head of
        //    the loop is faded in over the tail material that FOLLOWS the loop
        //    point, so playing loop[last] -> loop[0] steps across two adjacent
        //    source samples — no click.
        size_t xf = static_cast<size_t>(kBagPipeXfadeSec * sr);
        if (xf > res.size() / 4) xf = res.size() / 4;
        if (res.size() <= xf * 2 || xf == 0) {
            loop = std::move(res);  // clip too short to crossfade — loop raw
        } else {
            const size_t loop_len = res.size() - xf;
            loop.resize(loop_len);
            for (size_t i = 0; i < loop_len; ++i) {
                if (i < xf) {
                    const double t =
                        static_cast<double>(i + 1) / xf;  // ->1 at seam end
                    loop[i] = static_cast<float>(res[i] * t +
                                                 res[loop_len + i] * (1.0 - t));
                } else {
                    loop[i] = res[i];
                }
            }
        }

        pos = 0.0;
        ratio = kBagPipeIdleRatio;
        inited = !loop.empty();
    }

    bool ok() const { return inited; }

    void set_throttle(double t) { throttle = std::clamp(t, 0.0, 1.0); }

    void render(short* buf, int n) {
        if (!inited || loop.empty()) {
            for (int i = 0; i < n; ++i) buf[i] = 0;
            return;
        }
        const double L = static_cast<double>(loop.size());
        const double target_ratio =
            kBagPipeIdleRatio +
            throttle * (kBagPipeFullRatio - kBagPipeIdleRatio);
        const double ratio_span = kBagPipeFullRatio - kBagPipeIdleRatio;
        // Warmth-filter cutoff: set once per buffer off the (already smoothed)
        // ratio so the tone opens a little with throttle. A one-pole
        // coefficient steps inaudibly at this ~46 ms grain, so per-buffer is
        // plenty smooth.
        const double thr01_buf =
            ratio_span > 0.0
                ? std::clamp((ratio - kBagPipeIdleRatio) / ratio_span, 0.0, 1.0)
                : 0.0;
        const double cutoff =
            kBagPipeToneIdleHz +
            thr01_buf * (kBagPipeToneFullHz - kBagPipeToneIdleHz);
        tone1.set(cutoff, sr);
        tone2.set(cutoff, sr);
        for (int i = 0; i < n; ++i) {
            ratio += (target_ratio - ratio) * ratio_a;  // glide the pitch
            // Derive volume from the SMOOTHED ratio (not raw throttle) so a
            // throttle slam opens the level as smoothly as it lifts the pitch —
            // no per-buffer amplitude step (zipper).
            const double thr01 = ratio_span > 0.0
                                     ? (ratio - kBagPipeIdleRatio) / ratio_span
                                     : 0.0;
            const double vol =
                kBagPipeVolIdle + thr01 * (kBagPipeVolFull - kBagPipeVolIdle);
            const size_t i0 = static_cast<size_t>(pos);
            size_t i1 = i0 + 1;
            if (i1 >= loop.size()) i1 = 0;
            const double f = pos - static_cast<double>(i0);
            double s = loop[i0] * (1.0 - f) + loop[i1] * f;
            // Warm the tone BEFORE the soft-clip: roll off the reedy buzz so
            // the drone reads round and soft, then gently saturate the warmed
            // signal.
            s = tone2.lp(tone1.lp(s));
            s = std::tanh(kBagPipeOutDrive * vol * s);
            int v = static_cast<int>(s * 30000.0);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            buf[i] = static_cast<short>(v);
            pos += ratio;
            while (pos >= L) pos -= L;
        }
    }
};

}  // namespace render
