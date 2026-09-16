#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "render/audio_dsp.h"   // shared DSP primitives (WSXor, ...)
#include "render/wind_audio.h"  // kWind* constants, atm scaling intent

// Real-time wind SYNTH. Chad's 2026-07-24 ruling: the wind is a SOFT, RELAXING
// PINK-NOISE AIR BED that grows with speed, plus an intermittent high WHISTLE at
// the top end. The old sustained FLUTE/ORGAN PAD (two pure-toned pentatonic
// voices) was a droning "hum" and is REMOVED — nothing tonal sustains here now.
// The pad machinery (WSFluteVoice, voices, maybe_move, kWindPad*/Mix, notes) is
// kept in the struct, unused, so the pad is a one-line remix away if ever asked
// for again. Cosmetic, READ-ONLY off SimState — firewalled like every render
// channel; nothing here feeds input/control/sim. Tunables are code constants
// (S9-zoom / wind-audio precedent).
//
// Loudness opens gently with airspeed so a dive lifts the wash and a zoom apex
// softens it — the energy is audible, just relaxing rather than annoying.

namespace render {

inline constexpr double kWSPI = kAudioPI;
// Drive into the soft-clip. Kept LOW — pure tones distort (crackle) if driven
// hard, so we stay well under saturation. Tune HERE for overall level.
inline constexpr double kWindOutDrive = 2.0;
// Pad gain span: soft at low speed, a touch fuller when fast (still gentle).
// Eased down a little (Chad, 2026-07-11: "turn the background hum/organ down").
inline constexpr double kWindPadLo = 0.13;
inline constexpr double kWindPadHi = 0.27;
// Distant intermittent WHISTLE — only at the very high end of speed. A high,
// quiet tone that fades in and out as if passing a whistly area. Low volume =
// distant; the speed gate keeps it off entirely until you're really moving.
inline constexpr double kWindWhistleAmp = 0.020;   // faint — well under the wash
inline constexpr double kWindWhistleGateLo = 0.85; // pushed toward the top end (higher speed)
inline constexpr double kWindWhistleGateHi = 1.00; // speed01 for full presence
// Soft pink-noise air BED — the RELAXING wind (Chad, 2026-07-24: "a soft wind
// at the mid range that grows with speed ... pink noise that is relaxing rather
// than annoying"). Comes in from low speed and swells through the mid range,
// but the top is kept GENTLE so it stays an ocean-like wash, never a tearing
// hiss/roar. Heavily low-passed (see init) for warmth. This is now the wind's
// continuous voice (the flute/organ pad is gone); the whistle sits on top of it.
inline constexpr double kWindAirLo = 0.16;  // clearly audible wash even at low speed
inline constexpr double kWindAirHi = 0.88;  // strong (still soft) wash near redline
// Growth shape: >1 makes the wash build MORE toward the top end, so speed is
// felt as the wind gaining body (Chad, 2026-07-24: "gain more volume with speed").
inline constexpr double kWindAirExpo = 1.5;
// Low-voice ("hum") mix weight — turned down so the drone reads as a bed, not
// a hum (Chad, 2026-07-11).
inline constexpr double kWindDroneMix = 0.42;
inline constexpr double kWindUpperMix = 0.50;

// ---- one soft flute voice: a held tone that glides to its target ------- //
struct WSFluteVoice {
    double phase = 0.0;
    double freq = 220.0;
    double target = 220.0;

    // glide_a is the per-sample portamento coefficient (set from a time const).
    double tick(double sr, double glide_a) {
        freq += (target - freq) * glide_a;  // smooth pitch glide (no onset)
        phase += 2.0 * kWSPI * freq / sr;
        if (phase > 2.0 * kWSPI) phase -= 2.0 * kWSPI;
        // warm flute timbre: fundamental + a soft octave + a whisper of a 3rd.
        return std::sin(phase) + 0.26 * std::sin(2.0 * phase) +
               0.09 * std::sin(3.0 * phase);
    }
};

struct WindSynth {
    double sr = 22050.0;
    // live params (set per buffer from the render snapshot)
    double speed01 = 0.0;  // mapped airspeed [0,1]
    double atm = 1.0;      // air-density fraction

    std::array<WSFluteVoice, 2> voices{};
    double glide_a = 0.0;      // per-sample glide coefficient (~0.6 s)
    double note_timer = 2.0;   // [s] until the next gentle note move
    uint64_t sched = 0xD1B54A32D192ED03ull;

    // gentle pink-noise air bed (under the flutes)
    WSXor rng;
    WSPink pink;
    WSOnePole air_lp1, air_lp2;  // 2-pole low-pass so the air stays soft

    // distant intermittent whistle (only near top speed)
    double whistle_phase = 0.0;
    double vib_phase = 0.0;        // slow vibrato so it's airy, not a test tone
    double whistle_freq = 2600.0;  // [Hz] high
    double whistle_env = 0.0;      // current fade level [0,1]
    double whistle_target = 0.0;   // 0 = silent, 1 = sounding
    double whistle_timer = 3.0;    // [s] until the next on/off toggle
    double whistle_env_a = 0.0;    // per-sample fade coefficient (~0.35 s)
    double whistle_dur = 1.0;      // [s] length of the current whistle
    double whistle_t = 0.0;        // [s] elapsed within the current whistle
    WSsvf whistle_bp;              // airy band-pass noise -> a windy whistle

    bool inited = false;

    // major pentatonic (G A B D E) — positive, consonant, calming.
    static constexpr std::array<double, 5> notes() {
        return {196.00, 220.00, 246.94, 293.66, 329.63};
    }

    void init(double sample_rate) {
        sr = sample_rate;
        // portamento time constant ~0.6 s -> slow, singing glides (meditative).
        glide_a = 1.0 - std::exp(-1.0 / (0.60 * sr));
        voices[0].freq = voices[0].target = 196.00;   // low sustained drone
        voices[1].freq = voices[1].target = 293.66;   // gentle upper voice
        air_lp1.set(1000.0, sr);  // roll the top off hard -> a soft, warm wash
        air_lp2.set(1000.0, sr);  // (relaxing, ocean-like; no hiss/tearing)
        // whistle fades in/out over ~0.35 s -> a soft swell, not a click.
        whistle_env_a = 1.0 - std::exp(-1.0 / (0.35 * sr));
        inited = true;
    }

    void set_drivers(double speed, double atm_frac, double /*load_n*/) {
        speed01 = std::clamp((speed - kWindVQuiet) / (kWindVRoar - kWindVQuiet),
                             0.0, 1.0);
        atm = std::clamp(atm_frac, 0.0, 1.0);
    }

    double next_rand() {
        sched ^= sched << 13;
        sched ^= sched >> 7;
        sched ^= sched << 17;
        return static_cast<double>(sched >> 40) / 16777216.0;  // [0,1)
    }

    void maybe_move() {
        note_timer -= 1.0 / sr;
        if (note_timer > 0.0) return;
        const auto n = notes();
        // MEDITATIVE: the low voice is a sustained drone that shifts only
        // occasionally; the upper voice wanders slowly in the register above
        // it. Long holds, few changes -> a calm pad, not a melody.
        const int iu = 1 + static_cast<int>(next_rand() * 4.0) % 4;  // 220..329
        voices[1].target = n[iu];
        if (next_rand() < 0.30) {  // drone rarely moves, and stays low
            voices[0].target = (next_rand() < 0.5) ? n[0] : n[1];  // 196 / 220
        }
        note_timer = 10.0 + 8.0 * next_rand();  // hold 10-18 s between moves
    }

    // toggle the whistle on/off intermittently; each "on" picks a fresh high
    // pitch and holds briefly, then a silent gap — "passing a whistly area".
    void whistle_step() {
        whistle_timer -= 1.0 / sr;
        if (whistle_timer <= 0.0) {
            if (whistle_target > 0.5) {
                whistle_target = 0.0;                    // fade out
                whistle_timer = 2.5 + 4.5 * next_rand();  // quiet gap 2.5-7 s
            } else {
                whistle_target = 1.0;                    // fade in
                whistle_freq = 2400.0 + 1000.0 * next_rand();  // peak, varies
                whistle_dur = 0.7 + 1.6 * next_rand();    // sounds 0.7-2.3 s
                whistle_timer = whistle_dur;
                whistle_t = 0.0;                          // restart the arc
            }
        }
        if (whistle_target > 0.5) whistle_t += 1.0 / sr;
        whistle_env += (whistle_target - whistle_env) * whistle_env_a;
    }

    // smooth 0->1 gate over the very-high-speed band.
    double whistle_gate() const {
        const double g = std::clamp(
            (speed01 - kWindWhistleGateLo) /
                (kWindWhistleGateHi - kWindWhistleGateLo),
            0.0, 1.0);
        return g * g * (3.0 - 2.0 * g);  // smoothstep
    }

    // fill an int16 mono buffer with the current wind, drivers held constant.
    void render(short* buf, int n) {
        // Chad, 2026-07-24: the wind is a soft pink-noise AIR bed that grows with
        // speed, PLUS the intermittent top-end whistle. The sustained flute/organ
        // PAD (voices[0/1]) stays REMOVED — that was the "hum" — so nothing tonal
        // droning here, just a relaxing wash and the occasional high whistle.
        const double air_amp =
            kWindAirLo +
            (kWindAirHi - kWindAirLo) * std::pow(speed01, kWindAirExpo);
        const double whistle_g = whistle_gate();  // constant within the buffer
        for (int i = 0; i < n; ++i) {
            // soft pink-noise air, heavily low-passed + speed-scaled -> a warm,
            // relaxing breath of slipstream that swells with speed (no tear).
            const double air =
                air_lp2.lp(air_lp1.lp(pink.process(rng.w()))) * air_amp;
            // distant whistle: a high, quiet tone that arcs LOW->HIGH->LOW over
            // its own duration (a pass-by swell), with a whisper of vibrato.
            // Gated to top speed and faded in/out intermittently.
            whistle_step();
            vib_phase += 2.0 * kWSPI * 5.0 / sr;  // ~5 Hz vibrato
            if (vib_phase > 2.0 * kWSPI) vib_phase -= 2.0 * kWSPI;
            const double wp =
                (whistle_dur > 0.0)
                    ? std::clamp(whistle_t / whistle_dur, 0.0, 1.0)
                    : 0.0;
            const double arc = std::sin(kWSPI * wp);  // 0 -> 1 -> 0 (low-hi-low)
            const double f_lo = 0.72 * whistle_freq;  // arc floor
            const double f_w = (f_lo + (whistle_freq - f_lo) * arc) *
                               (1.0 + 0.004 * std::sin(vib_phase));
            whistle_phase += 2.0 * kWSPI * f_w / sr;
            if (whistle_phase > 2.0 * kWSPI) whistle_phase -= 2.0 * kWSPI;
            // windy timbre: blend the pure tone with band-pass noise tracking
            // the same arcing pitch -> an airy whistle, not a test tone.
            const double fbp = 2.0 * std::sin(kWSPI * f_w / sr);
            const double breath = std::tanh(whistle_bp.bp(rng.w(), fbp, 1.0 / 9.0));
            const double tone = 0.55 * std::sin(whistle_phase) + 0.60 * breath;
            const double whistle =
                tone * whistle_env * whistle_g * kWindWhistleAmp;
            double s = air + whistle;  // soft wind wash + top-end whistle (no pad)
            // thin air hushes the wind; master + gentle soft-clip (stays clean).
            s *= (kWindAtmFloor + (1.0 - kWindAtmFloor) * atm) * kWindMaster;
            s = std::tanh(kWindOutDrive * s);
            int iv = static_cast<int>(s * 30000.0);
            if (iv > 32767) iv = 32767;
            if (iv < -32768) iv = -32768;
            buf[i] = static_cast<short>(iv);
        }
    }
};

}  // namespace render
