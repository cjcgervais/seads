#pragma once

#include <algorithm>
#include <cmath>

#include "render/audio_dsp.h"   // kAudioPI, WSOnePole
#include "render/gun_audio.h"   // soft_clip -- shared, never re-derived (H1)
#include "render/mix_levels.h"  // kGameplayBusGain -- the ONE gameplay bus

// ★★★ THE STING'S BUZZ -- the multirotor motor voice, pure DSP.
//
// Chad, 2026-09-05 (ST-5 polish, verdict c): "need some sound for the model
// increasing pitch and volume with throttle".
//
// Cloned in SHAPE from render/sled_audio.h + render/sled_synth.h: the pure
// mappings live at the top as named constants with their derivation written
// out, the synth underneath renders them into int16 mono at the stream rate,
// and the app layer owns the raylib stream, the getenv kill switch and the
// per-frame driver push. Firewall: only <algorithm>, <cmath> and the three
// render audio headers above. No raylib, no <random>, no wall-clock, no
// allocation in render().
//
// ⚠ AND UNLIKE THE SLED THIS IS FULLY SYNTHETIC -- there is no drone
// recording in the soundbank, so nothing here resamples an asset and none of
// sled_audio.h's kSledSourceCorrection reasoning carries over. What DOES carry
// over is the two-number discipline: a PHYSICAL derivation (blade-pass rate)
// kept visible and honest, and a separate VOICE number that says how far the
// sounded thing sits from it, so a later "too high / too low" ruling has one
// dial to move and the physics stays readable.

namespace render {

// ---------------------------------------------------------------------------
// THE BLADE-PASS RATE -- the physical derivation.
//
// A rotor of B blades turning at f_rot Hz beats the air B times per rev:
//
//     f_blade [Hz] = f_rot * B
//
// A 5-inch racing quad idles its motors around 4-6 krpm and pins them near
// 25 krpm; with two-blade props that is roughly 130 Hz to 830 Hz of blade
// pass. The Sting is a bigger, heavier airframe than a 5-inch racer, so the
// band below sits in the lower half of that: a hornet drone, not a mosquito.
// ---------------------------------------------------------------------------
//
// ★★★ RE-SPANNED at the ST-5 prop/audio round. Chad, verbatim: "sound even
// faster (but not go faster), also I would like to have a lower bottom end
// ... by pressing s, I should hear it go down to a lower frequency and volume
// ... and it shall go down to 150 m/s as a bottom end." Two ends, two moves:
//
//   BOTTOM 55 -> 47 Hz. The band's floor went 250 -> 150 m/s (app/sting.h),
//     so there is now a real loiter to sound, and the old idle sat 110 Hz --
//     a mid hum, not a machine holding station. 47 Hz rotor = 94 Hz of blade
//     pass, down in the chest where a big loaded rotor loafing actually
//     lives, and a clear seventh below where the old floor sat so the S key
//     is audible as a PITCH DROP and not just a level change.
//   TOP 150 -> 177 Hz. +18% of sounded fundamental (300 -> 354 Hz) for
//     "sound even faster" without touching the true rate law below -- the
//     drone does not fly one m/s quicker, it only screams higher when pinned.
//
// Still inside the physical envelope the derivation above sets out: 47 Hz is
// ~2800 rpm and 177 Hz is ~10600 rpm, both squarely inside the 4-25 krpm a
// real multirotor works over, and the ratio (3.77x) is if anything more
// honest than the old 2.7x for a machine that goes from loitering to chasing.
inline constexpr double kStingBlades = 2.0;
inline constexpr double kStingRotorIdleHz = 47.0;   // ~2800 rpm, loitering
inline constexpr double kStingRotorFullHz = 177.0;  // ~10600 rpm, pinned

inline double sting_rotor_hz(double thr) {
    const double t = std::clamp(thr, 0.0, 1.0);
    return kStingRotorIdleHz + (kStingRotorFullHz - kStingRotorIdleHz) * t;
}

// The SOUNDED fundamental. Identity for now -- the Sting has never been flown
// with sound, so there is no ruling to answer and inventing a transpose before
// he has heard it would be tuning against nobody. This is the ONE dial to move
// if he says "too high pitched" (the kSledVoiceTranspose seat).
inline constexpr double kStingVoiceTranspose = 1.0;

inline double sting_blade_hz(double thr) {
    return sting_rotor_hz(thr) * kStingBlades * kStingVoiceTranspose;
}

// FOUR MOTORS, FOUR RATES. Real quads never sit at one speed: the flight
// controller trims each motor differently to hold attitude, and the resulting
// few-Hz beating between four near-identical tones is the single thing that
// most makes a recording read as "drone" rather than "sawtooth oscillator".
// Fractions of the nominal rate; deliberately not symmetric (a symmetric set
// beats at one rate and sounds like chorus, not like four motors).
inline constexpr int kStingMotors = 4;
inline constexpr double kStingMotorDetune[kStingMotors] = {1.000, 1.013, 0.991,
                                                           1.021};

// Harmonics per motor, 1/n amplitude. A prop is an impulsive source, so the
// series runs well up -- a pure sine per motor reads as a theremin.
inline constexpr int kStingHarmonics = 5;

// Brightness opens with throttle, exactly the sled tone's reasoning: a loaded
// rotor is brighter as well as faster, and moving the fundamental without the
// timbre leaves a thin stack over a low root.
//
// ★ ST-5 round: both ends move with the band. 900 -> 700 Hz shuts the loiter
// down to a dull thrum (an unloaded rotor is DARK, and leaving the brightness
// where it was would have made the new low fundamental sound like the same
// tone transposed rather than like a machine backing off); 5200 -> 6400 Hz
// opens the pinned end wider, which is the other half of "sound even faster"
// -- a rotor reads as fast through its top harmonics well before the ear
// resolves the fundamental. The 5-harmonic stack at f0 = 354 Hz tops out at
// 1770 Hz, so 6400 is still comfortably ABOVE the whole series (it shapes the
// stack, it does not clip it off) and nothing here is a resonance.
inline constexpr double kStingCutoffIdle = 700.0;
inline constexpr double kStingCutoffFull = 6400.0;

inline double sting_cutoff(double thr) {
    const double t = std::clamp(thr, 0.0, 1.0);
    return (kStingCutoffIdle + (kStingCutoffFull - kStingCutoffIdle) * t) *
           kStingVoiceTranspose;
}

// ---------------------------------------------------------------------------
// LEVEL. Chad asked for volume to rise with throttle as well as pitch.
// ---------------------------------------------------------------------------
//
// ★ ST-5 round: "hear it go down to a lower frequency AND VOLUME" -- the old
// 0.34 floor was only 9 dB under the pinned level, which with the squared law
// meant the S key barely moved the needle. 0.18 is 15 dB down: the loiter now
// sits back in the mix where the wind and the sled can be heard over it, and
// pinning it is an event. The top goes 1.00 -> 1.12 for the "even faster"
// half; that is +1.0 dB of the WITHIN-CHANNEL law and changes no headline
// level -- kStingFlownLevel below still owns what the channel is worth on the
// bus, still 0.26, still under the snowmachine's 0.31. Pre-clip peak stays
// legal: 1.12 x kStingMaster 0.70 = 0.78, inside soft_clip's linear region,
// so the extra gain is loudness and not distortion.
inline constexpr double kStingIdleGain = 0.18;  // loitering, backed right off
inline constexpr double kStingFullGain = 1.12;  // pinned

inline double sting_throttle_gain(double thr) {
    const double t = std::clamp(thr, 0.0, 1.0);
    // Squared toward the top: perceived loudness of a rotor climbs faster than
    // linearly once it is loaded, and a linear ramp reads as "it got a bit
    // louder" rather than as the machine leaving.
    return kStingIdleGain + (kStingFullGain - kStingIdleGain) * t * t;
}

// DISTANCE. The chase camera normally sits ~7 m off the drone, but freelook on
// a parked man leaves the eye on the ground while the Sting is kilometres out
// -- so an un-attenuated buzz would follow it to the horizon at full level.
// One over (1 + d/ref) rather than an inverse square: the square is correct
// for a point source in free air and completely wrong for a game, where it
// makes the sound vanish inside two chase lengths.
inline constexpr double kStingAudioRefM = 30.0;

inline double sting_distance_gain(double dist_m) {
    const double d = (dist_m > 0.0) ? dist_m : 0.0;  // also catches NaN
    return 1.0 / (1.0 + d / kStingAudioRefM);
}

// Master trim for the channel, before the stream level.
inline constexpr double kStingMaster = 0.70;

// The channel's own flown level and its bus-scaled stream level -- the
// render/mix_levels.h law (every gameplay channel is stated as flown x
// kGameplayBusGain, never as a raw stream number). Sits BELOW the
// snowmachine's 0.31: the sled is the machine you are sitting on, the Sting is
// a small thing in the distance, and the distance term above already carries
// most of that. This is the dial for "the drone is too loud / too quiet".
inline constexpr double kStingFlownLevel = 0.26;
inline constexpr double kStingStreamLevel = kStingFlownLevel * kGameplayBusGain;

static_assert(kStingStreamLevel > 0.0 && kStingStreamLevel < 1.0,
              "sting channel must be a legal stream volume");

// ---------------------------------------------------------------------------
// THE SPIN RATE -- shared with the VISUAL prop spin (render/sting_model.h),
// which is why it lives in this header and not in main.cpp. Blade blur and
// motor buzz coming from two different throttle laws would be a fork; here the
// app steps ONE phase off ONE rate and both the picture and the sound read it.
//
// Not the audio rate: 150 rad/s is 24 rev/s, which at 60 fps is 0.4 rev per
// frame -- fast enough to blur, slow enough that consecutive frames are
// visibly different (which is how the smoke rig certifies it). The AUDIO
// blade-pass above is the acoustic rate and is deliberately independent.
// ---------------------------------------------------------------------------
inline constexpr double kStingSpinIdleRadS = 40.0;
inline constexpr double kStingSpinFullRadS = 150.0;
// The rail: the last stretch of the 3 s deploy blend spins them up slowly, so
// what leaves his shoulder is visibly already running. Below kStingRailStart
// the props are dead still (he is pulling it out from under the seat).
inline constexpr double kStingRailStart = 0.85;  // deploy t
inline constexpr double kStingRailRadS = 22.0;   // at t = 1

inline double sting_spin_rate(double thr) {
    const double t = std::clamp(thr, 0.0, 1.0);
    return kStingSpinIdleRadS + (kStingSpinFullRadS - kStingSpinIdleRadS) * t;
}

// ★★★ THE APPARENT RATE -- Chad, fly-2 of the polish round (verbatim):
// "They propellor spinning animation should visibly vary spin speed starting
// to be able to perceive the spinning blades but as the speed ramps up, the
// blades should still be apparent but a circular blur."
//
// THE PROBLEM IS THE FRAME, NOT THE MODEL. A prop turning at the TRUE rate is
// sampled once per frame, and a two-blade prop is symmetric every 180 deg, so
// at 60 fps anything past ~47 rad/s aliases: the blades stop, crawl backwards,
// or strobe -- the wagon-wheel effect. That is what "can't perceive the
// spinning blades" is. Clamping the drawn rate is the classic fix and it is
// what makes a spin READ; the impression of SPEED then comes from the blur
// disc (render/sting_model.cpp), which is how every flight sim and every
// hand-drawn cartoon has ever done it.
//
// Not a hard clip: a clip makes 40 rad/s and 150 rad/s draw IDENTICALLY, and
// he asked for the spin to "visibly vary". A soft knee keeps 1:1 up to the
// knee -- so the rail idle and a slow hover read TRUE -- and then compresses,
// so a pinned drone still turns visibly faster than a hovering one while
// staying under the alias limit.
//
// ★★★ RAISED at the ST-5 prop/audio round -- Chad, verbatim: "I would like
// the blades to appear to move even faster at the top end and sound even
// faster (but not go faster)". "But not go faster" is the whole instruction:
// the TRUE rate law above is untouched (the airframe is signed), and only the
// DRAWN top of this ladder moves. The old cap of 25 rad/s was biting well
// before the pinned rate -- the compressed ramp wanted 33.8 at 150 rad/s and
// got clipped to 25 -- so the whole top third of the throttle drew at ONE
// rate and a pinned drone looked like a cruising one. The cap now sits where
// the ramp lands at the pinned rate, so nothing at the top is thrown away and
// the cap is purely the alias guard for a smoke-pinned override.
//
//   knee 18 rad/s  = 2.9 rev/s = 17 deg per frame at 60 fps. Comfortably
//                    unambiguous, and above the 22 rad/s rail idle only
//                    slightly, so the deploy spin-up is honest. UNCHANGED --
//                    idle stays 1:1, which is the half of the law he liked
//                    ("very nice with the animation").
//   slope 0.1212   = (34 - 18) / (150 - 18): chosen so the TRUE pinned rate
//                    kStingSpinFullRadS lands exactly ON the cap.
//   max  34 rad/s  = 5.41 rev/s = 32.5 deg per frame at 60 fps.
//
// ALIAS MARGIN, stated. A two-blade prop is symmetric every 180 deg, so the
// sampling limit is 90 deg per frame = 47.12 rad/s at 60 fps. 34 rad/s is
// 72% of that -- 13.1 rad/s / 28% of margin, i.e. the drawn blade advances
// 32.5 deg where 90 deg would be ambiguous. At 30 fps it is 65 deg per frame,
// still under the 90 deg limit (28% margin there too, since both the limit
// and the per-frame step scale with the frame time). The old 25 rad/s bought
// a whole extra frame of margin nobody was spending; this keeps a real one
// and buys back the top-end read he asked for.
//
// The apparent rate now spans 20.6 rad/s (the 150 m/s loiter, true 40) to
// 34.0 rad/s (pinned, true 150) -- a 1.65x drawn spread where the old ladder
// managed 1.21x, which is the "even faster at the top end" in one number.
inline constexpr double kStingSpinKneeRadS = 18.0;
inline constexpr double kStingSpinSlope = 0.1212;  // compression above the knee
inline constexpr double kStingSpinApparentMax = 34.0;

// The alias law written as an assertion rather than only as prose, so a later
// "faster still" ruling cannot quietly walk the cap into the wagon wheel.
//
// A kStingBlades-blade prop repeats every 2*pi/B radians, so the hard Nyquist
// bound is HALF that per frame: (pi/B) * fps = 94.2 rad/s for two blades at
// 60 fps. Sampling theory's bound is where direction becomes ambiguous, not
// where motion still READS -- a blade landing 89 deg on from the last frame
// is technically resolvable and looks like a strobe -- so this header has
// always worked to half of it (the "~47 rad/s" in the banner above), which is
// 45 deg of blade travel per frame. The cap is held under 3/4 of THAT
// working limit.
inline constexpr double kStingAliasNyquistRadS =
    kAudioPI * 60.0 / kStingBlades;  // 94.2 rad/s -- direction ambiguous above
inline constexpr double kStingAliasWorkingRadS =
    0.5 * kStingAliasNyquistRadS;  // 47.1 rad/s -- the readable limit
static_assert(kStingSpinApparentMax < 0.75 * kStingAliasWorkingRadS,
              "drawn prop rate must keep a margin under the two-blade "
              "wagon-wheel limit at 60 fps");

inline double sting_apparent_spin_rate(double rate) {
    const double r = (rate > 0.0) ? rate : 0.0;  // also catches NaN
    if (r <= kStingSpinKneeRadS) return r;
    return std::min(kStingSpinApparentMax,
                    kStingSpinKneeRadS + (r - kStingSpinKneeRadS) *
                                             kStingSpinSlope);
}

inline double sting_rail_spin_rate(double deploy_t) {
    if (!(deploy_t > kStingRailStart)) return 0.0;  // also catches NaN
    const double u =
        std::clamp((deploy_t - kStingRailStart) / (1.0 - kStingRailStart), 0.0,
                   1.0);
    return kStingRailRadS * u;
}

// ---------------------------------------------------------------------------
// THE SYNTH. Four detuned harmonic stacks through one brightness lowpass.
// int16 mono at the stream rate, so it drops into main.cpp's existing
// 22050/16/1 refill block beside the wind, the guns and the snowmachine.
// ---------------------------------------------------------------------------
struct StingSynth {
    double sr_ = 22050.0;
    double inv_sr_ = 1.0 / 22050.0;
    bool inited_ = false;

    // Per-motor, per-harmonic phase. ONE phase per partial, never a shared
    // phase wrapped at the fundamental -- render/sled_synth.h defect (2): a
    // shared wrap re-phases every detuned partial to a common origin once per
    // period, which puts a step in the waveform AND makes the detune a no-op,
    // i.e. it silently deletes the very thing four motors are here for.
    double phase_[kStingMotors][kStingHarmonics] = {};
    WSOnePole lp_{};

    // Drivers, pushed at FRAME rate (the sled's set_drivers idiom -- a buffer
    // is 46 ms and a throttle sweep read there is a staircase).
    double thr_ = 0.0;    // [0,1]
    double gate_ = 0.0;   // [0,1] flying/rail gate x distance
    // Applied values, slewed per sample from the previous buffer so no layer
    // steps at a buffer boundary (sled_synth defect 3).
    double g_ = 0.0;
    double f_ = 0.0;

    bool ok() const { return inited_; }

    void init(double sr) {
        sr_ = (sr > 1.0) ? sr : 22050.0;
        inv_sr_ = 1.0 / sr_;
        inited_ = true;
    }

    // throttle [0,1]; gate = the audible-ness of the channel this frame
    // (0 while stowed/dead, distance-attenuated while flying). Both clamped
    // here so no caller can drive the synth out of band.
    void set_drivers(double throttle, double gate) {
        thr_ = std::clamp(throttle, 0.0, 1.0);
        gate_ = std::clamp(gate, 0.0, 1.0);
    }

    // What the channel is singing right now, for a TraceLog / a test: the
    // sounded fundamental and the applied linear gain.
    double voice_hz() const { return sting_blade_hz(thr_); }
    double voice_gain() const { return sting_throttle_gain(thr_) * gate_; }

    void render(short* buf, int n) {
        if (buf == nullptr || n <= 0) return;
        if (!inited_) {
            for (int i = 0; i < n; ++i) buf[i] = 0;
            return;
        }
        const double t_g = voice_gain();
        const double t_f = voice_hz();
        const double inv_n = 1.0 / static_cast<double>(n);
        const double dg = (t_g - g_) * inv_n;
        const double df = (t_f - f_) * inv_n;
        lp_.set(sting_cutoff(thr_), sr_);
        // 1/n series over kStingHarmonics summed over kStingMotors would peak
        // near motors * H_n; normalise by that so the soft clip is a safety
        // net rather than the level control.
        double hn = 0.0;
        for (int k = 0; k < kStingHarmonics; ++k) hn += 1.0 / (k + 1);
        const double norm = 1.0 / (hn * static_cast<double>(kStingMotors));

        for (int i = 0; i < n; ++i) {
            g_ += dg;
            f_ += df;
            double mix = 0.0;
            for (int m = 0; m < kStingMotors; ++m) {
                const double fm = f_ * kStingMotorDetune[m];
                for (int k = 0; k < kStingHarmonics; ++k) {
                    mix += std::sin(2.0 * kAudioPI * phase_[m][k]) / (k + 1);
                    // Advance ALWAYS, gated or not, so a re-entry does not
                    // jump the waveform (the sled tone's own rule).
                    phase_[m][k] += fm * (k + 1) * inv_sr_;
                    if (phase_[m][k] >= 1.0)
                        phase_[m][k] -= std::floor(phase_[m][k]);
                }
            }
            const double v = lp_.lp(mix * norm) * g_;
            buf[i] = static_cast<short>(soft_clip(v * kStingMaster) * 32767.0);
        }
    }
};

}  // namespace render
