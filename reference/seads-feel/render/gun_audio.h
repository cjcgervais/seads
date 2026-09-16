#pragma once

#include <cmath>

// Procedural gun-audio PURE mappings + shaping constants for SEADS.
// Cosmetic, render-side only — reads nothing from sim, writes no game state.
// All constants are code constants per the S9-zoom/wind-audio precedent.
// Firewall: only <cmath> included; no sim/control/input/weapon/combat deps.

namespace render {

// ---------------------------------------------------------------------------
// Gunfire constants
// ---------------------------------------------------------------------------
// Cannon vs MG are deliberately SEPARATED in pitch, decay, and body so the two
// gun classes read as distinct (Chad fly 2026-07-13: "only hear one sound for
// two types"). Cannon = deep, broad, longer THUMP + a sub-bass sine punch for
// weight (a boom); MG = high, tight, short crack (a zippy rattle). ~2.5 octaves
// of pitch separation + the sub-thump is the primary separator.
inline constexpr double kGunCannonCrackTau = 0.032;  // [s] cannon decay (longer body)
inline constexpr double kGunMGCrackTau     = 0.006;  // [s] MG decay (short tick)
inline constexpr double kGunCannonFc       = 135.0;  // [Hz] cannon band center (deep)
inline constexpr double kGunCannonQ        = 0.7;    // cannon band Q (broad whump)
inline constexpr double kGunMGFc           = 780.0;  // [Hz] MG band center (high/zippy)
inline constexpr double kGunMGQ            = 1.7;    // MG band Q (tight ping)
// Cannon sub-thump: a decaying low sine mixed under the crack — the body/weight
// that says "cannon", absent from the MG. Retuned by ear (Chad).
inline constexpr double kGunCannonSubFc    = 78.0;   // [Hz] sub-thump sine
inline constexpr double kGunCannonSubTau   = 0.026;  // [s] sub-thump decay (short punch, no pile-up)
inline constexpr double kGunCannonSubMix   = 0.30;   // sub level relative to the crack (weight, not drone)
inline constexpr double kGunJitter         = 0.12;   // per-shot period jitter fraction
inline constexpr double kGunAttack         = 0.006;  // [s] firing-gain attack
inline constexpr double kGunRelease        = 0.045;  // [s] firing-gain release
inline constexpr double kGunMaster         = 0.9;    // master gain [0,1]

// ---------------------------------------------------------------------------
// Explosion constants
// ---------------------------------------------------------------------------
inline constexpr double kExplosionAudioLife  = 1.30;  // [s] total audio life
inline constexpr double kExplosionFc         = 90.0;  // [Hz] low rumble center
inline constexpr double kExplosionCrackFc    = 700.0; // [Hz] initial crack band
inline constexpr double kExplosionCrackTime  = 0.05;  // [s] initial crack duration
inline constexpr double kExplosionAttackTime = 0.012; // [s] attack ramp
inline constexpr double kExplosionDecayTau   = 0.35;  // [s] exp-decay tau
inline constexpr double kExplosionMaster     = 0.95;  // master gain [0,1]

// ---------------------------------------------------------------------------
// Hit constants
// ---------------------------------------------------------------------------
inline constexpr double kHitAudioLife  = 0.09;    // [s] total audio life
inline constexpr double kHitPingFc     = 1150.0;  // [Hz] metallic ping center
inline constexpr double kHitPingQ      = 6.0;     // hit ping Q
inline constexpr double kHitDecayTau   = 0.020;   // [s] hit exp-decay tau
inline constexpr double kHitMaster     = 0.6;     // master gain [0,1]

// ---------------------------------------------------------------------------
// Reload foley constants (FLAK STAGE C)
// ---------------------------------------------------------------------------
// ★ Two one-shots, one reload: the EMPTY drum coming OFF is a light, bright
// CLANK (a hollow 27 kg magazine lifted clear of its bracket -- high ring,
// short); the FRESH drum seating is a heavier THUNK-LATCH (a full drum
// dropping into the feed throat and the latch closing on it -- low body,
// longer). They are the two ends of the 4 s reload and they are what tells the
// gunner, without a HUD, that the gun is coming back.
//
// ★ COSMETIC. These are FOLEY, not the fire control: nothing here is read by
// the reload timer (app/flak_tick.h owns it, untouched), and the timer is not
// read back from the audio. Triggered on the reload_left_s EDGES.
inline constexpr double kClankLifeS    = 0.42;    // [s] total audio life
inline constexpr double kClankDecayTau = 0.085;   // [s] exp-decay tau
inline constexpr double kClankRingFc   = 2150.0;  // [Hz] bright metallic ring
inline constexpr double kClankRingQ    = 9.0;
inline constexpr double kClankBodyFc   = 640.0;   // [Hz] the drum's own body
inline constexpr double kClankBodyQ    = 3.0;
// ★ Round 5 (Chad 2026-08-30: "the reload sound effect is louder than the
// gunshot"): both foley masters cut ~5 dB. The drum swap must sit UNDER the
// 20 mm report -- it is a man handling steel beside a cannon, not the cannon.
inline constexpr double kClankMaster   = 0.30;    // master gain [0,1] (was 0.55)

inline constexpr double kLatchLifeS    = 0.58;    // [s] longer: it is heavy
inline constexpr double kLatchDecayTau = 0.130;
inline constexpr double kLatchRingFc   = 780.0;   // [Hz] the latch snapping
inline constexpr double kLatchRingQ    = 7.0;
inline constexpr double kLatchBodyFc   = 155.0;   // [Hz] 27 kg of drum seating
inline constexpr double kLatchBodyQ    = 1.4;
inline constexpr double kLatchMaster   = 0.45;    // (was 0.85, same -5 dB cut)

// ---------------------------------------------------------------------------
// Flak burst boom (ROUND 5)
// ---------------------------------------------------------------------------
// ★ Chad 2026-08-30: "the secondary boom of the proximity fuse has no sound.
// There is supposed to be a loud boom and an echo about the mountains."
// Until now a burst only PINGED (trigger_hit, a 90 ms confirmation tick) --
// the detonation itself, hundreds of metres out, was silent.
//
// The boom is the existing explosion voice at a per-voice GAIN; the mountain
// echo is the same voice re-fired on a PREDELAY (a negative starting age --
// explosion_amp is 0 for age <= 0 by contract, so a parked voice is silent
// until its clock crosses zero). Two repeats, each quieter: the far wall and
// the farther one.
inline constexpr double kFlakBoomHitGain     = 2.2;   // damaging prox burst
                                                      //   (round 5c: was 1.5)
inline constexpr double kFlakBoomCurtainGain = 1.0;   // self-destruct crump
                                                      //   (round 5c: was 0.5 --
                                                      //   Chad: "turn up the
                                                      //   volume on the bursts")
                                                      //   (~1800 m out, many)
inline constexpr int    kFlakEchoCount = 2;
inline constexpr double kFlakEchoDelayS[kFlakEchoCount] = {0.9, 2.1};
inline constexpr double kFlakEchoGain[kFlakEchoCount]   = {0.35, 0.16};

// ★ Round 5b (Chad: "lower the frequency of the burst in the distance and
// make it a bit more hollow ... not so sharp poppy harsh but earthy"): a
// DISTANT burst keeps only the low body. Air eats the top end over a
// kilometre, so the crump's rumble low-pass drops 90 -> 55 Hz and the 700 Hz
// crack layer (the "poppy" part) is nearly removed. Applied to the curtain
// crumps AND to the hit boom's mountain echoes (a reflection is farther
// still); the hit boom itself keeps the full close-up timbre.
inline constexpr double kFlakCrumpFc       = 40.0;  // [Hz] hollow rumble LP
                                                    //   (round 5c: 55 -> 40,
                                                    //   Chad: "make them deeper";
                                                    //   a lower cutoff passes
                                                    //   less energy -- the gain
                                                    //   bump above compensates)
inline constexpr double kFlakCrumpCrackMix = 0.12;  // crack layer, vs 1.0

// Foley envelope: instant attack, exp decay x linear taper -> exactly 0 at
// life. Same shape family as hit_amp (and the same age <= 0 convention), but
// the life/tau are per-voice so ONE function serves both foley voices.
inline double clank_amp(double age, double life, double tau) {
    if (!(life > 0.0) || age <= 0.0 || age >= life) return 0.0;
    const double t = tau > 1e-6 ? tau : 1e-6;
    double out = std::exp(-age / t) * (1.0 - age / life);
    if (out < 0.0) out = 0.0;
    if (out > 1.0) out = 1.0;
    return out;
}

// ---------------------------------------------------------------------------
// Output soft-clip (limiter)
// ---------------------------------------------------------------------------
// The synth output buses summed their voices then HARD-clipped (`if (s>1) s=1`).
// A single sound never reaches the ceiling, but a furball — many explosion + hit
// voices landing at once (now common with the bandit combat AI) — overshoots +-1,
// and the hard corner turns that overshoot into harsh digital distortion. Replace
// it with a soft limiter: identity inside |s| <= kSoftClipThreshold (so the
// single-sound timbre is UNTOUCHED — bit-identical below the knee), then a tanh
// knee that saturates smoothly toward +-1 above it. Fable-flagged P2 (2026-07-13).
inline constexpr double kSoftClipThreshold = 0.9;  // linear below this; knee above

// Odd, monotone, C1-continuous limiter. At |x| = T the value is T and the slope
// is 1 (tanh'(0) = 1 == the linear slope below T), so there is no audible kink;
// as |x| -> inf it approaches but never reaches +-1, so `x * 32767` stays inside
// the int16 range (a hard clip would pin exactly at 32767; this never does).
inline double soft_clip(double x) {
    const double t = kSoftClipThreshold;
    const double a = x < 0.0 ? -x : x;  // |x|
    if (a <= t) return x;               // linear region — the common case, untouched
    const double knee = 1.0 - t;        // > 0 headroom above the threshold
    const double y = t + knee * std::tanh((a - t) / knee);
    return x < 0.0 ? -y : y;
}

// ---------------------------------------------------------------------------
// Pure envelope helpers — unit-testable, no side-effects
// ---------------------------------------------------------------------------

// Single-shot crack envelope: 1 at age=0, monotone exponential decay.
// Returns 0 for negative age.
inline double gun_shot_amp(double age, double tau) {
    if (age < 0.0) return 0.0;
    const double t = tau > 1e-6 ? tau : 1e-6;
    return std::exp(-age / t);
}

// Explosion envelope: attack ramp -> exp-decay x linear taper -> exactly 0 at life.
// 0 at age<=0 or age>=life; always in [0,1].
inline double explosion_amp(double age) {
    if (age <= 0.0 || age >= kExplosionAudioLife) return 0.0;
    const double life = kExplosionAudioLife;
    const double att  = kExplosionAttackTime;
    // Attack ramp [0, att) -> 0..1
    double env;
    if (age < att) {
        env = age / att;
    } else {
        // Exp decay from 1.0 after the attack peak
        env = std::exp(-(age - att) / kExplosionDecayTau);
    }
    // Linear taper: reaches exactly 0 at life
    const double taper = 1.0 - age / life;
    double out = env * taper;
    if (out < 0.0) out = 0.0;
    if (out > 1.0) out = 1.0;
    return out;
}

// Hit envelope: exp-decay x linear taper, 0 outside (0, kHitAudioLife).
// Always in [0,1].
inline double hit_amp(double age) {
    if (age <= 0.0 || age >= kHitAudioLife) return 0.0;
    double out = std::exp(-age / kHitDecayTau) * (1.0 - age / kHitAudioLife);
    if (out < 0.0) out = 0.0;
    if (out > 1.0) out = 1.0;
    return out;
}

}  // namespace render
