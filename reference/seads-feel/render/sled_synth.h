#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "render/audio_dsp.h"   // kAudioPI, WSOnePole
#include "render/gun_audio.h"   // soft_clip -- shared, never re-derived (H1)
#include "render/sled_audio.h"
#include "render/sled_drive.h"

// Real-time snowmachine engine synth (Polaris Indy 650) -- the three layers of
// render/sled_audio.h actually rendered into samples.
//
//   IDLE   assets/audio/loops/sled_idle_loop.wav, seamless-looped, resampled by
//          revs. Chad: "Idle engine indy 650 for idle".
//   RUN    assets/audio/loops/sled_run.wav, LOOPED as the riding bed and
//          re-attacked on each throttle onset. Chad: "and indy_650 engine sound
//          for riding ... Tapping keys gives the brap brap ramp up".
//   TONE   a synthesized two-stroke drone, firing frequency derived from the
//          real engine (see sled_audio.h).
//
// PURE DSP, like render/bagpipe_throttle.h: the .wav files are decoded by the
// app layer (raylib) and their samples handed to init(); this header never
// touches raylib or the disk. Renders int16 mono at the stream rate so it drops
// into main.cpp's existing 22050/16/1 refill block alongside wind and guns --
// correct for a vehicle engine, which is a SPEED-COUPLED layer.
//
// THREE DEFECTS A RED-TEAM FOUND HERE, all fixed below, all invisible to a
// green gate:
//   1. update() ran once per BUFFER (46 ms), so real key taps were dropped
//      entirely. Drivers now arrive per FRAME via set_drivers(); render()
//      consumes a latch.
//   2. One shared oscillator phase wrapped at the fundamental put a 0.76 step
//      (37% of the stack's peak) in the waveform every period -- broadband
//      aliasing -- AND re-phased every partial to a common origin, which made
//      the detune a no-op. Now per-harmonic phase.
//   3. Layer gains were held constant across a buffer and stepped at the
//      boundary: the loudest layer stepped ~0.055 per buffer, an audible click
//      train. Now slewed per sample from the previous buffer's value.
//
// Firewall: only <algorithm>, <cmath>, <cstddef>, <vector>,
// render/audio_dsp.h, render/gun_audio.h, render/sled_audio.h,
// render/sled_drive.h. No raylib, no <random>, no wall-clock, no allocation in
// render().

namespace render {

struct SledSynth {
    // ---- decoded, resampled, mono sample banks (owned) --------------------
    std::vector<float> idle_;
    std::vector<float> run_;
    double sr_ = 22050.0;
    double inv_sr_ = 1.0 / 22050.0;
    bool inited_ = false;

    // ---- playback cursors --------------------------------------------------
    double idle_pos_ = 0.0;
    double run_pos_ = 0.0;

    // ---- tone: ONE PHASE PER HARMONIC --------------------------------------
    // A single shared phase wrapped at the fundamental is only continuous for
    // partials whose ratio is an integer, i.e. k=1. Every detuned partial gets
    // a step. Per-harmonic accumulators are continuous by construction and let
    // the detune actually beat.
    double hphase_[kSledToneHarmonics] = {};
    WSOnePole tone_lp_{};

    // ---- gain slew (previous buffer's applied values) ----------------------
    double g_idle_ = 0.0, g_run_ = 0.0, g_accent_ = 0.0, g_tone_ = 0.0;

    SledDrive drive;

    bool ok() const { return inited_; }

    // Downmix interleaved source to mono at out_sr.
    //
    // The decimation guard is a 4-POLE cascade, not the single pole this
    // originally had. A first-order lowpass at 0.45*out_sr is only ~2.7 dB down
    // at Nyquist, so everything from 11 k to 22 k folded back into the audible
    // band at 62-74% of its amplitude -- the filter WAS the grit it claimed to
    // prevent. Four cascaded poles at a lower corner give 24 dB/oct; still not
    // a brickwall, but an honest guard rather than a decorative one.
    static void resample_mono(const float* src, std::size_t frames, int ch,
                              double src_sr, double out_sr,
                              std::vector<float>& out) {
        out.clear();
        if (src == nullptr || frames == 0 || ch < 1 || !(src_sr > 0.0) ||
            !(out_sr > 0.0)) {
            return;
        }
        std::vector<float> mono(frames);
        const double inv_ch = 1.0 / static_cast<double>(ch);
        for (std::size_t i = 0; i < frames; ++i) {
            double s = 0.0;
            for (int c = 0; c < ch; ++c) s += src[i * ch + c];
            mono[i] = static_cast<float>(s * inv_ch);
        }
        if (out_sr < src_sr) {
            WSOnePole aa[4];
            for (auto& f : aa) f.set(0.30 * out_sr, src_sr);
            for (std::size_t i = 0; i < frames; ++i) {
                double v = mono[i];
                for (auto& f : aa) v = f.lp(v);
                mono[i] = static_cast<float>(v);
            }
        }
        const double step = src_sr / out_sr;
        const std::size_t n =
            static_cast<std::size_t>(static_cast<double>(frames) / step);
        out.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double p = i * step;
            const std::size_t i0 = static_cast<std::size_t>(p);
            // Both banks are LOOPS, so the sample after the last is the first.
            const std::size_t i1 = (i0 + 1 < frames) ? i0 + 1 : 0;
            const double f = p - static_cast<double>(i0);
            out[i] = static_cast<float>(mono[i0] * (1.0 - f) + mono[i1] * f);
        }
    }

    // Both assets are already loop-folded seamless offline by the soundbank
    // build (D:/audio_tracks/tools/build_soundbank.sh), so no crossfade is
    // applied here -- a second fold would only shorten them and dull a seam
    // that is already clean. Measured on sled_idle_loop.wav: |last-first| = 504
    // LSB against a p99 adjacent-sample delta of 2037, i.e. well inside normal
    // signal excursion.
    void init(double out_sr, const float* idle_s, std::size_t idle_frames,
              int idle_ch, double idle_src_sr, const float* run_s,
              std::size_t run_frames, int run_ch, double run_src_sr) {
        sr_ = out_sr > 0.0 ? out_sr : 22050.0;
        inv_sr_ = 1.0 / sr_;
        resample_mono(idle_s, idle_frames, idle_ch, idle_src_sr, sr_, idle_);
        resample_mono(run_s, run_frames, run_ch, run_src_sr, sr_, run_);
        idle_pos_ = 0.0;
        run_pos_ = 0.0;
        for (auto& p : hphase_) p = 0.0;
        g_idle_ = g_run_ = g_accent_ = g_tone_ = 0.0;
        drive = SledDrive{};
        tone_lp_.set(kSledToneCutoffIdle, sr_);
        inited_ = !idle_.empty();
    }

    // Called at FRAME rate from the app, before the stream drain -- the same
    // idiom the wind and gun channels use. This is where onset detection lives;
    // see the header note.
    void set_drivers(double throttle, double dt) { drive.update(throttle, dt); }

    void render(short* buf, int n) {
        if (buf == nullptr || n <= 0) return;
        if (!inited_) {
            for (int i = 0; i < n; ++i) buf[i] = 0;
            return;
        }
        // Consume any onsets latched since the last buffer. Re-attacking the
        // bed is what makes the bark; the bed itself keeps running.
        if (drive.take_accents() > 0) run_pos_ = 0.0;

        // Target gains for this buffer; slewed per sample from the last one so
        // no layer steps at a boundary.
        const double t_idle = drive.idle_level();
        const double t_run = drive.run_level();
        const double t_accent = drive.accent_level();
        const double t_tone = drive.tone_level();
        const double inv_n = 1.0 / static_cast<double>(n);
        const double d_idle = (t_idle - g_idle_) * inv_n;
        const double d_run = (t_run - g_run_) * inv_n;
        const double d_accent = (t_accent - g_accent_) * inv_n;
        const double d_tone = (t_tone - g_tone_) * inv_n;

        const double idle_r = drive.idle_ratio();
        const double run_r = drive.run_ratio();
        // voice_hz(), NOT fire_hz(): the sounded fundamental, which is the
        // firing rate transposed by Chad's ruling. See render/sled_audio.h.
        const double f0 = drive.voice_hz();
        tone_lp_.set(drive.tone_cutoff(), sr_);

        const double idle_len = static_cast<double>(idle_.size());
        const double run_len = static_cast<double>(run_.size());

        for (int i = 0; i < n; ++i) {
            g_idle_ += d_idle;
            g_run_ += d_run;
            g_accent_ += d_accent;
            g_tone_ += d_tone;

            double mix = 0.0;

            // ---- IDLE: seamless loop, wrapped ---------------------------
            if (idle_len > 1.0) {
                const std::size_t i0 = static_cast<std::size_t>(idle_pos_);
                const std::size_t i1 = (i0 + 1 < idle_.size()) ? i0 + 1 : 0;
                const double f = idle_pos_ - static_cast<double>(i0);
                mix += (idle_[i0] * (1.0 - f) + idle_[i1] * f) * g_idle_;
                idle_pos_ += idle_r;
                while (idle_pos_ >= idle_len) idle_pos_ -= idle_len;
            }

            // ---- RUN: the riding bed, looped, plus the onset accent ------
            // ONE read, TWO gains: the bed level rides revs and the accent
            // rides the onset envelope, both on the same sample. That is why
            // holding the throttle keeps Chad's riding recording audible
            // instead of leaving only the synth drone.
            if (run_len > 1.0) {
                const std::size_t r0 = static_cast<std::size_t>(run_pos_);
                const std::size_t r1 = (r0 + 1 < run_.size()) ? r0 + 1 : 0;
                const double f = run_pos_ - static_cast<double>(r0);
                const double s = run_[r0] * (1.0 - f) + run_[r1] * f;
                mix += s * (g_run_ + g_accent_);
                run_pos_ += run_r;
                while (run_pos_ >= run_len) run_pos_ -= run_len;
            }

            // ---- TONE: the two-stroke drone -----------------------------
            if (g_tone_ > 1e-6) {
                double t = 0.0;
                for (int k = 0; k < kSledToneHarmonics; ++k) {
                    t += std::sin(2.0 * kAudioPI * hphase_[k]) / (k + 1);
                }
                mix += tone_lp_.lp(t * 0.45) * g_tone_;
            }
            // Advance every partial on its own phase, always -- so the tone
            // does not jump when it fades back in.
            for (int k = 0; k < kSledToneHarmonics; ++k) {
                const double dk = 1.0 + kSledToneDetune * k;
                hphase_[k] += f0 * (k + 1) * dk * inv_sr_;
                if (hphase_[k] >= 1.0) hphase_[k] -= std::floor(hphase_[k]);
            }

            buf[i] = static_cast<short>(soft_clip(mix * kSledMaster) * 32767.0);
        }
    }
};

}  // namespace render
