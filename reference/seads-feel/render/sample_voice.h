#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "render/audio_dsp.h"  // WSOnePole (anti-alias), kAudioPI

// A ONE-SHOT SAMPLE VOICE -- decoded audio, played once from the top on
// trigger(), rendered into a hand-fed stream.
//
// It exists for exactly one job: Chad ruled (2026-08-17) that the black-stope
// explosion must ECHO like everything else down there. The blast had been a
// raylib `Sound` played with PlaySound, which is mixed inside raylib and hands
// us no buffer, so no amount of reverb could reach it. Routing a copy of it
// through this voice puts it somewhere render::StopeReverb can.
//
// ⚠ It is a SEND, not a replacement. app/main.cpp still plays the raylib Sound
// for the DRY blast -- full stereo, at the asset's own 44.1 kHz -- and this
// voice feeds only the reverb. That is the standard console shape and it is
// what keeps Chad's "I prefer stereo sound quality" intact: the dry blast is
// untouched, and only the send (a diffuse tail, where mono is inaudible as a
// limitation) runs through the 22050/16/1 pipe the synth channels use.
//
// Resampling follows render/bagpipe_throttle.h exactly -- downmix, cascaded
// anti-alias one-poles, linear interpolation -- because that is the decode path
// already flown in this repo, not a second one invented here.
//
// Cosmetic, render-side only. PURE DSP: the .wav is decoded by the app layer
// (raylib) and its samples handed to init(); this header never touches raylib
// or the disk. No allocation in render(), no <random>, no wall-clock.
// Firewall: only <algorithm>, <cmath>, <cstddef>, <vector>, audio_dsp.h.

namespace render {

// Anti-alias stages ahead of the decimation. 44.1 kHz -> 22.05 kHz is a 2x
// decimation; one 6 dB/oct pole is far too gentle across it and folds the
// blast's top end back down as a haze, so cascade four (~24 dB/oct), the same
// count and reasoning as the bagpipe path.
inline constexpr int kSampleVoiceAAStages = 4;

// Cutoff as a fraction of the OUTPUT rate. 0.45 leaves a little guard band
// under Nyquist without audibly dulling a sound that is mostly low rumble.
inline constexpr double kSampleVoiceAAFrac = 0.45;

struct SampleVoice {
    // The clip, downmixed to mono and already resampled to the output rate, so
    // playback is a plain 1:1 walk with no per-sample interpolation.
    std::vector<float> clip;
    std::size_t pos = 0;   // read cursor; == clip.size() means idle
    bool inited = false;

    // `samples` holds `frames` FRAMES of `channels`-interleaved floats at
    // `src_sr` -- exactly what raylib's LoadWaveSamples() produces.
    void init(double out_sr, const float* samples, std::size_t frames,
              int channels, double src_sr) {
        clip.clear();
        pos = 0;
        inited = false;
        if (samples == nullptr || frames == 0 || channels < 1 ||
            src_sr <= 0.0 || out_sr <= 0.0)
            return;  // caller falls back to a silent send

        // 1) Downmix to mono at the source rate.
        std::vector<float> mono(frames);
        for (std::size_t i = 0; i < frames; ++i) {
            double acc = 0.0;
            for (int c = 0; c < channels; ++c)
                acc += samples[i * static_cast<std::size_t>(channels) + c];
            mono[i] = static_cast<float>(acc / channels);
        }

        // 2) Anti-alias, then resample.
        const double decim = src_sr / out_sr;  // source samples per output one
        if (decim > 1.0) {
            const double cutoff =
                std::min(kSampleVoiceAAFrac * out_sr, kSampleVoiceAAFrac * src_sr);
            WSOnePole aa[kSampleVoiceAAStages];
            for (int s = 0; s < kSampleVoiceAAStages; ++s)
                aa[s].set(cutoff, src_sr);
            for (std::size_t i = 0; i < frames; ++i) {
                double x = mono[i];
                for (int s = 0; s < kSampleVoiceAAStages; ++s) x = aa[s].lp(x);
                mono[i] = static_cast<float>(x);
            }
        }
        const std::size_t out_n =
            static_cast<std::size_t>(static_cast<double>(frames) / decim);
        if (out_n == 0) return;  // clip too short to yield one output sample
        clip.assign(out_n, 0.0f);
        for (std::size_t i = 0; i < out_n; ++i) {
            const double sp = static_cast<double>(i) * decim;
            const std::size_t i0 = static_cast<std::size_t>(sp);
            const std::size_t i1 = (i0 + 1 < frames) ? i0 + 1 : i0;
            const double f = sp - static_cast<double>(i0);
            clip[i] = static_cast<float>(mono[i0] * (1.0 - f) + mono[i1] * f);
        }

        pos = clip.size();  // start IDLE, not playing
        inited = true;
    }

    bool ok() const { return inited && !clip.empty(); }
    bool playing() const { return ok() && pos < clip.size(); }
    double length_sec(double out_sr) const {
        return (out_sr > 0.0) ? static_cast<double>(clip.size()) / out_sr : 0.0;
    }

    // Start again from the top. RESTARTS rather than layering, matching the
    // non-polyphonic raylib Sound this send shadows -- the two must stay in
    // lockstep or the echo would be of a different blast than the one you
    // heard. render/music_director.h static_asserts that every rumble gap
    // exceeds the asset length, so a restart never actually truncates one.
    void trigger() {
        if (ok()) pos = 0;
    }

    void stop() { pos = clip.size(); }

    // Fills the whole buffer: the clip while it lasts, silence after. Silence
    // matters -- downstream is a reverb that must keep being fed to flush its
    // tail.
    void render(short* buf, int n) {
        if (buf == nullptr || n <= 0) return;
        for (int i = 0; i < n; ++i) {
            double x = 0.0;
            if (pos < clip.size()) x = static_cast<double>(clip[pos++]);
            const double s = std::clamp(x * 32767.0, -32768.0, 32767.0);
            buf[i] = static_cast<short>(s);
        }
    }
};

}  // namespace render
