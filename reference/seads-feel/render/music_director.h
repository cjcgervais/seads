#pragma once

#include <algorithm>
#include <cmath>

#include "render/audio_dsp.h"   // kAudioPI, audio_slew_coef
#include "render/mix_levels.h"  // THE gameplay balance (single source)

// The MUSIC bus director for SEADS -- which bed plays, how loud, and what
// pushes it out of the way.
//
// Chad, 2026-08-17: "Volatus_Aeturnus is perfect music for when the airplane
// enters the tunnels and while in the black stope... Play the explosion rumble
// as well (make the ambient music on the quieter side and make the expolsion
// come in and occulude the tunnel and balck stope music... As you exit that
// music fades."
//
// So: two beds, cross-faded on the tunnel-net predicate, with the deep bed
// deliberately quieter than the surface bed, and a duck that the blast drives.
// The black stope is INSIDE the tunnel net (it is the sealed unlit chamber
// holding the two DEEP conquest pumps), so app::inside_tunnel() covers both
// places Chad named with one predicate and they can never fork.
//
// Cosmetic, render-side only -- reads a bool and a dt, writes no game state.
// PURE: this header computes volumes; app/main.cpp owns every raylib call and
// merely applies what update() produced.
// Firewall: only <algorithm>, <cmath>, render/audio_dsp.h, render/mix_levels.h.
// No raylib, no <random>, no wall-clock, no allocation.

namespace render {

// ---------------------------------------------------------------------------
// Levels. Tunables are code constants per the S9-zoom / wind-audio precedent
// (render-side cosmetic knobs live in code, not config).
// ---------------------------------------------------------------------------

// The two bed levels now live in render/mix_levels.h with the wind and engine
// trims they are balanced AGAINST -- kMusicSurfaceGain and kMusicDeepGain are
// still spelled exactly the same at every call site, they are just no longer
// defined in four unrelated files.
//
// Why they moved (2026-08-17): Chad's readme asks the music to be "louder in
// the loading page and quieter during the gameplay", and gameplay sat 0.45 dB
// under the loading page -- no difference at all. Asked which end should move,
// he ruled "trim wind / engine further", so the whole gameplay bus drops
// together on one dial (kGameplayBusGain) and the wind/music ratio from his
// first fly is preserved by derivation instead of by hand.

// Crossfade time constants [s]. Entering is quicker than leaving: flying into
// a portal is an event and the bed should meet you there, whereas Chad asked
// for the deep bed to FADE on the way out rather than snap.
inline constexpr double kMusicEnterTau = 1.60;
inline constexpr double kMusicExitTau = 3.20;

// ---------------------------------------------------------------------------
// The duck -- "make the explosion come in and occlude the ... music".
//
// Occlusion is delivered HERE, by pulling the music down under the blast, and
// deliberately NOT by mastering the explosion louder: the blast's crest factor
// IS its content (LRA 19.3 in the source), and compressing it flat to win the
// same perceived contrast would cost the thing that makes it read as a blast.
// ---------------------------------------------------------------------------

// Floor the music is pushed to at full duck. 0.20 = -14 dB: the bed is still
// present underneath, so the blast reads as occluding it rather than as an
// audio dropout.
inline constexpr double kMusicDuckFloor = 0.20;

// Duck envelope [s]. Fast in (the blast arrives), hold across the body of the
// rumble, then a long release so the bed swells back rather than switching on.
//
// SIZED AGAINST THE ASSET, not by feel. sfx/black_stope_explosion.wav is a 13 s
// body measured at -18.2 / -17.3 LUFS over its first 6 s, decaying through
// -31.3 and -34.5 by 12 s. So the hold covers the loud body and the release
// rides the decay out. An earlier 1.10 s hold released the music back to full
// while ~29 s of rumble was still playing -- the opposite of the occlusion Chad
// asked for. If the asset is re-cut, re-derive these.
inline constexpr double kMusicDuckAttack = 0.12;
inline constexpr double kMusicDuckHold = 5.50;
inline constexpr double kMusicDuckRelease = 4.50;

inline constexpr double kMusicDuckLife =
    kMusicDuckAttack + kMusicDuckHold + kMusicDuckRelease;

// ---------------------------------------------------------------------------
// Pure mappings -- unit-testable without constructing anything.
// ---------------------------------------------------------------------------

// Duck gain at `age` seconds into a blast. 1 = untouched, kMusicDuckFloor =
// fully occluded. age < 0 (no blast in flight) and age >= life both yield 1,
// so the caller needs no special-casing. Continuous at every breakpoint.
inline double music_duck_gain(double age) {
    if (!(age > 0.0)) return 1.0;  // also catches NaN
    if (age >= kMusicDuckLife) return 1.0;
    const double d = 1.0 - kMusicDuckFloor;
    if (age < kMusicDuckAttack) {
        return 1.0 - d * (age / kMusicDuckAttack);
    }
    const double held = age - kMusicDuckAttack;
    if (held < kMusicDuckHold) return kMusicDuckFloor;
    const double rel = (held - kMusicDuckHold) / kMusicDuckRelease;
    return kMusicDuckFloor + d * rel;
}

// Equal-power crossfade weight for the OUTGOING (surface) bed at blend b,
// where b = 0 is fully surface and b = 1 is fully deep. cos/sin rather than a
// linear pair because two uncorrelated beds summed linearly dip ~3 dB through
// the middle of the fade -- an audible sag exactly at the portal.
inline double music_surface_weight(double b) {
    const double c = std::clamp(b, 0.0, 1.0);
    return std::cos(0.5 * kAudioPI * c);
}

inline double music_deep_weight(double b) {
    const double c = std::clamp(b, 0.0, 1.0);
    return std::sin(0.5 * kAudioPI * c);
}

// One-pole slew coefficient for time constant tau over a step dt. Clamped so a
// hitched frame can overshoot neither end.
//
// Now a thin alias over the shared audio_dsp.h primitive (same expression,
// bit-identical) so the reverb's wet ramp and this crossfade cannot drift into
// two different one-pole laws.
inline double music_slew(double dt, double tau) {
    return audio_slew_coef(dt, tau);
}

// ---------------------------------------------------------------------------
// The director. Holds the only two pieces of state: where the crossfade sits,
// and how long ago the blast went off.
// ---------------------------------------------------------------------------

struct MusicDirector {
    // 0 = surface bed only, 1 = deep bed only.
    double blend = 0.0;
    // Seconds since the last blast; negative means none in flight.
    double duck_age = -1.0;

    // `inside` is app::inside_tunnel(env, pos) -- tunnels AND the black stope.
    void update(bool inside, double dt) {
        const double target = inside ? 1.0 : 0.0;
        const double tau = inside ? kMusicEnterTau : kMusicExitTau;
        blend += (target - blend) * music_slew(dt, tau);
        blend = std::clamp(blend, 0.0, 1.0);
        if (duck_age >= 0.0) {
            duck_age += dt;
            if (duck_age >= kMusicDuckLife) duck_age = -1.0;  // retire
        }
    }

    // Re-triggering restarts the envelope rather than stacking, so a burst of
    // blasts holds the music down instead of fighting over it.
    void blast() { duck_age = 0.0; }

    double duck() const { return music_duck_gain(duck_age); }

    // Final volumes to hand raylib's SetMusicVolume, both already ducked.
    double surface_volume() const {
        return kMusicSurfaceGain * music_surface_weight(blend) * duck();
    }
    double deep_volume() const {
        return kMusicDeepGain * music_deep_weight(blend) * duck();
    }
};

// ---------------------------------------------------------------------------
// The stope rumble scheduler -- "Play the explosion rumble as well".
//
// Deep-pump workings are being blasted somewhere off in the rock while you fly
// the net. The rumble is therefore an AMBIENCE of the tunnels and the stope,
// not a combat event: it fires on its own cadence while you are inside, and
// stops mattering the moment you leave.
//
// The gaps cycle through a fixed table rather than an RNG. Three reasons, all
// binding here: <random> is banned in this layer, the gate needs the behaviour
// to be reproducible, and a table of four uneven gaps is audibly less
// metronomic than a naive uniform draw anyway.
// ---------------------------------------------------------------------------

// Grace after entering before the first blast -- long enough that the rumble
// never steps on the music crossfade at the portal.
inline constexpr double kStopeRumbleFirst = 45.0;  // [s]

// Chad's spec (D:/audio_tracks/sound_effects/readme.txt): "the ambient
// underground explosion an rumble ABOUT ONCE EVERY TWO MINUTES."
//
// ⚠ These were 23/37/16/30 s -- a blast every half minute, 4-5x too often. I
// set them by feel before reading his written spec, and a rumble that frequent
// stops reading as distant workings and starts reading as a firefight. Mean is
// now 125 s, spread either side of two minutes so it never feels metronomic.
inline constexpr int kStopeRumbleGapCount = 4;
inline constexpr double kStopeRumbleGaps[kStopeRumbleGapCount] = {105.0, 140.0,
                                                                  95.0, 160.0};

// Measured length of sfx/black_stope_explosion.wav as it ships (see the cut
// note in the soundbank manifest). The blast is ONE non-polyphonic Sound, so a
// re-trigger restarts it: every gap must exceed the asset or the rumble cuts
// itself off mid-decay. Pinned at compile time rather than left to the ear.
inline constexpr double kStopeBoomLen = 13.0;

static_assert(kStopeRumbleGaps[0] > kStopeBoomLen &&
                  kStopeRumbleGaps[1] > kStopeBoomLen &&
                  kStopeRumbleGaps[2] > kStopeBoomLen &&
                  kStopeRumbleGaps[3] > kStopeBoomLen,
              "a rumble gap is shorter than the blast: it would truncate "
              "itself. Lengthen the gap or re-cut the asset.");

// The duck must also finish inside the shortest gap, or two blasts' envelopes
// overlap and the music never comes back up between them.
static_assert(kMusicDuckLife < kStopeRumbleGaps[2],
              "the duck outlives the shortest rumble gap");

struct StopeRumble {
    double timer = 0.0;     // seconds accumulated inside the net
    double next = kStopeRumbleFirst;
    int gap_index = 0;

    // Returns true on the single frame a rumble should be triggered. Leaving
    // the net rearms the whole cadence, so re-entering always gets the grace
    // period rather than an immediate blast in your face at the portal.
    bool update(bool inside, double dt) {
        if (!inside) {
            timer = 0.0;
            next = kStopeRumbleFirst;
            gap_index = 0;
            return false;
        }
        if (!(dt > 0.0)) return false;
        timer += dt;
        if (timer < next) return false;
        timer = 0.0;
        next = kStopeRumbleGaps[gap_index];
        gap_index = (gap_index + 1) % kStopeRumbleGapCount;
        return true;
    }
};

}  // namespace render
