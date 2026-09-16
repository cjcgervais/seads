#pragma once

#include <algorithm>
#include <cmath>

// Snowmachine engine audio -- the PURE mappings. Polaris Indy 650.
//
// Chad, 2026-08-17: "Idle sngine for snowmachine is [the idle] sample loop and
// the riding is the other engine snowmachine brap brap sounds that can ramp up
// en key pressed. Tapping keys gives the brap brap ramp up... then chose a tone
// for the sustained machine sound... Make sure to balance the sounds."
// Asset assignment, his words: "Idle engine indy 650 for idle and indy_650
// engine sound for riding."
//
// THREE layers, and which one you hear is a function of how the throttle is
// being USED, not just how far it is open:
//
//   IDLE   the idle loop. Owns the voice at rest, retreats as revs come up.
//   RUN    the riding recording. It is a LOOPING BED whose level rides revs,
//          plus an ONSET ACCENT on each throttle stab -- that accent is the
//          "brap". Tapping gives you one per tap; holding leaves the bed
//          playing underneath, because it is literally the sound of riding.
//   TONE   a synthesized two-stroke drone for the sustained machine sound.
//
// ⚠ THE RUN LAYER WAS ORIGINALLY A ONE-SHOT with an 0.85 s envelope over a
// 7.37 s asset -- 88% of the recording Chad supplied "for the riding" was
// unreachable, and holding the throttle left only the synth drone. A red-team
// caught it. The bed-plus-accent shape below is the fix: the accent keeps the
// brap, the bed makes riding actually use the riding recording.
//
// Cosmetic, render-side only -- reads a throttle scalar, writes no game state.
// Firewall: only <algorithm>, <cmath>. No raylib, no <random>, no wall-clock.

namespace render {

// ---------------------------------------------------------------------------
// Rev follower. The engine's own inertia, not the throttle position.
// ---------------------------------------------------------------------------

// Time constants [s]. Spin-up is quicker than spin-down: a 650 two-stroke picks
// up fast and coasts back down against its own flywheel. The asymmetry is what
// makes repeated taps STACK into a rising blat instead of returning to idle
// between each one -- Chad's "tapping keys gives the brap brap ramp up".
inline constexpr double kSledRevUpTau = 0.22;
inline constexpr double kSledRevDownTau = 0.55;

// One-pole follower step toward `target` over dt with time constant tau.
inline double sled_rev_step(double revs, double target, double dt) {
    if (!(dt > 0.0)) return revs;
    const double tau = (target > revs) ? kSledRevUpTau : kSledRevDownTau;
    const double a = std::clamp(1.0 - std::exp(-dt / tau), 0.0, 1.0);
    return std::clamp(revs + (target - revs) * a, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// The throttle gate -- HYSTERETIC.
//
// CLAUDE.md: "Every gate/regime/latch leg is hysteretic. Shared thresholds
// chatter." A single threshold on an analog throttle dwelling at its edge would
// re-trigger the onset accent every crossing, machine-gunning restarted sample
// attacks. Two thresholds with a band between them cannot.
// ---------------------------------------------------------------------------
inline constexpr double kSledOnsetOn = 0.18;   // opens above this
inline constexpr double kSledOnsetOff = 0.10;  // closes below this

// Minimum spacing between accents [s]. Even with hysteresis, a throttle that is
// stabbed faster than a real engine can bark should not stack attacks on top of
// each other -- the pipe cannot re-load that fast.
inline constexpr double kSledAccentRefractory = 0.09;

// ---------------------------------------------------------------------------
// ⭐ THE ENGINE'S PITCH -- two constants, two different jobs.
//
// Chad, first ride 2026-08-20: "the engine sound is good but it need to come
// down one or two octaves it really high pitched." Second ride, after one
// octave: "it still reads like it needs to come down an octave."
//
// Those two rulings are answered by two SEPARATE numbers below, because they
// are two separate problems that happened to point the same way. Folding them
// into one dial was how the first pass ended up arguing with him.
//
// ---------------------------------------------------------------------------
// (1) kSledSourceCorrection -- the RECORDINGS are not what they claim.
//
// Measured off the shipped assets (direct spectrum, 0.75 s window, 2.5 Hz bins,
// tools/sled_synth_preview.cpp renders and ffmpeg measures):
//
//   sled_idle_loop.wav   dominant partials 135-150 Hz, nothing below
//   sled_run.wav         dominant 460-520 Hz (peak 517), lesser 247-260 Hz
//
// A 650 triple two-stroke fires at rpm/60*3, so 140 Hz implies ~2800 rpm for a
// machine the kernel idles at 1700, and 517 Hz implies ~10 000 rpm on an engine
// that tops out at 8000. Neither take is at the rpm its filename claims. 0.50
// lands them on the firing rate the tone derives, which is a MEASUREMENT and
// applies to the SAMPLES ONLY -- the synthesized tone was never mis-recorded.
inline constexpr double kSledSourceCorrection = 0.50;

// ---------------------------------------------------------------------------
// (2) kSledVoiceTranspose -- CHAD'S EAR, and it outranks the derivation.
//
// After (1) had every layer sitting exactly on the derived firing rate -- 72.5
// Hz at rest against 60, 350.0 Hz pinned against 350, verified by rendering the
// real synth -- he rode it again and said it still reads an octave high.
//
// So the firing-rate model is not wrong about the ENGINE; it is wrong about the
// GAME. SPEC/CLAUDE.md rule this project by FEEL, NOT FIDELITY, and this is
// exactly that line: rpm/60*3 is fidelity, and his ear is feel. A derivation
// that keeps producing a sound he does not want is a derivation being used as
// an argument, which is the same mistake as the static_assert retracted in
// mix_levels.h an hour ago. It is not repeated here -- the physics stays
// visible and honest in sled_fire_hz(), and this number says, in the open, how
// far the SOUNDED voice sits below it.
//
// Applied to EVERYTHING that carries pitch, so the machine transposes as one
// instrument rather than coming apart:
//   - both sample playback ratios (via kSledPitchPedestal below)
//   - the tone's fundamental          (sled_voice_hz)
//   - the tone's lowpass sweep        (sled_tone_cutoff)
//
// That last one is not an extra: resampling a recording moves its WHOLE
// spectrum, so leaving the synth tone's brightness envelope at 420-4200 Hz
// while dropping its fundamental would leave the one layer whose timbre did not
// come down with it -- a thin bright stack over a low fundamental, which reads
// high no matter where f0 sits. Transposing an instrument moves all of it.
//
// ⚠ THE ONE MEASURED RISK AT 0.50, recorded so it is not rediscovered as a
// mystery. Rendering the real synth and band-splitting the output:
//
//   RIDING (throttle 1)   RMS -18.3 dB above 50 Hz, -18.6 above 100 Hz
//                         -> essentially all of it is in the audible band.
//                            This is the layer Chad was complaining about and
//                            it is healthy at the new pitch.
//   IDLE   (throttle 0)   RMS -25.0 dB BELOW 50 Hz, -29.0 above, -36.4 above
//                         100 Hz -> the idle bed now has more energy under
//                            50 Hz than over it.
//
// So a small speaker that rolls off below ~150 Hz will keep the ride and lose
// most of the idle. On monitors or headphones it is a deep burble, which is
// what it should be. If the idle goes MISSING rather than deep, that is this
// -- and the fix is not to undo the transpose (the ride would go back up with
// it) but to lift the idle bed alone: kSledSourceCorrection is measured per
// the pair, and the idle take is the one whose own number is really 1700/2800
// = 0.61 rather than 0.50.
//
// ⭐ THIS IS THE ONE NUMBER TO MOVE if he says it again. Nothing else needs
// touching, and the tests derive from it.
//
// ---------------------------------------------------------------------------
// ⭐ HE SAID IT AGAIN, 2026-08-28: "maybe lower frequency so not so high
// pitched a bit lower too, though close to where it should be". 0.50 -> 0.40.
//
// The note above pre-authorized 0.25 -- a WHOLE third octave -- and that is
// deliberately not what this takes, because his sentence this time is not the
// sentence that number was written for. Twice before he said "come down an
// octave", flat, and got an octave. Here he says "a bit lower" and, in the same
// breath, "close to where it should be". A pre-computed step is a convenience,
// not a standing order; taking 0.25 because it was written down would overshoot
// a man who just told me the pitch is nearly right.
//
// 0.40 is 0.80x, about a major third (-3.9 semitones). That is comfortably
// above the "is it my imagination" floor for pitch -- a third is a step anyone
// hears -- while leaving most of the pre-authorized octave unspent for a next
// ride. If he says it a fourth time, 0.31 is the rest of that octave and 0.25
// is the octave itself.
//
// ⚠ MEASURED BEFORE AND AFTER, 2026-08-28, rather than argued. Both pitches
// rendered through tools/sled_synth_preview.cpp and measured with ffmpeg
// (2-stage biquad splits at 50/100 Hz, mean_volume; 0.75 s Hann window at
// 2.5 Hz bins for the partials). Old = pedestal 0.25, new = 0.20:
//
//   RIDING   dominant  175.0 Hz -> 140.0 Hz   (0.80x, exactly the transpose)
//            partial   352.5 Hz -> 282.5 Hz   (the fire-rate partial, same
//                                              0.80x -- the instrument moved
//                                              as ONE, which is the whole
//                                              claim of the cutoff sweep note
//                                              above, now checked and true)
//            >50 Hz    -18.3 dB -> -18.4 dB   (unchanged; healthy)
//            >100 Hz   -18.8 dB -> -19.5 dB   (-0.7 dB; nothing)
//
//   IDLE     dominant   27.5 Hz ->  27.5 Hz
//            <50 Hz    -26.0 dB -> -24.7 dB
//            >50 Hz    -32.4 dB -> -34.9 dB
//
// ⚠ AND THAT CORRECTS THE RISK NOTE ABOVE rather than confirming it. I expected
// this move to spend the idle-bed margin. The measurement says the margin was
// ALREADY SPENT AT 0.50: the idle's dominant partial is 27.5 Hz at BOTH
// pitches, which is under the point where a laptop or phone speaker reproduces
// anything at all. This change widens the sub-50/over-50 split from 6.4 dB to
// 10.2 dB -- real, but a worsening of a condition that predates it, not the
// cause of it.
//
// So if the idle burble is reported MISSING on small speakers, do not read it
// as this ruling's fault and do not undo the transpose -- the ride would climb
// back up with it, and the ride is the only layer he has ever complained about.
// The fix is to lift the idle take alone, whose own honest source number is
// 1700/2800 = 0.61 rather than the 0.50 measured across the pair. That needs
// kSledSourceCorrection split per sample (sled_drive.h applies one pedestal to
// both ratios today), which is a real change and not a dial, so it is named
// here rather than done on speculation.
//
// ⚠ AND IT COMPOUNDS WITH THE -10 dB in the same ruling (kSledFlownLevel
// 0.98 -> 0.31, mix_levels.h). Lower voice plus lower gain is more than either
// alone. If the machine now reads too quiet rather than too harsh, the LEVEL is
// the dial that answers that; this one is answering "high pitched".
inline constexpr double kSledVoiceTranspose = 0.40;

// The SAMPLE playback pedestal: both jobs, since a recording carries pitch and
// timbre together and resampling moves both at once. 0.50 * 0.50 = 0.25.
//
// A PEDESTAL, not a re-range -- it scales each ratio whole (see sled_drive.h),
// so the sweep every layer makes across the rev range is untouched and only the
// floor moves. Downward resampling cannot alias, so there is no quality cost;
// at 0.25 the source is being oversampled 4x.
inline constexpr double kSledPitchPedestal =
    kSledSourceCorrection * kSledVoiceTranspose;

// ---------------------------------------------------------------------------
// The TONE -- the sustained machine sound. Chad: "chose a tone".
//
// Chosen from the real engine, not by ear-picking a pitch. The Indy 650 is a
// 650 cc THREE-CYLINDER TWO-STROKE. A two-stroke fires once per revolution per
// cylinder (no wasted stroke), so the firing rate is:
//
//     f_fire [Hz] = rpm / 60 * cylinders = rpm / 60 * 3
//
// Idle ~1200 rpm -> 60 Hz. Pinned ~7000 rpm -> 350 Hz. That span is the tone's
// range, and it is why the sled sits an octave-and-a-half BELOW the aircraft's
// bagpipe drone: they can share a mix without fighting for the same band.
// ---------------------------------------------------------------------------
inline constexpr double kSledIdleRpm = 1200.0;
inline constexpr double kSledMaxRpm = 7000.0;
inline constexpr double kSledCylinders = 3.0;

inline double sled_rpm(double revs) {
    const double r = std::clamp(revs, 0.0, 1.0);
    return kSledIdleRpm + (kSledMaxRpm - kSledIdleRpm) * r;
}

// The PHYSICAL firing rate: what the engine actually does. Kept exact and
// unscaled -- it is the reference every other number here is stated against,
// and burying a taste factor inside it would make the physics unreadable.
inline double sled_fire_hz(double revs) {
    return sled_rpm(revs) / 60.0 * kSledCylinders;
}

// The SOUNDED fundamental: what the tone layer actually sings, which is the
// firing rate transposed down by Chad's ruling. This is what the synth uses;
// sled_fire_hz is what it is derived FROM. Keeping them as two named functions
// means a reader can see the gap instead of discovering it inside a multiply.
inline double sled_voice_hz(double revs) {
    return sled_fire_hz(revs) * kSledVoiceTranspose;
}

// Timbre: a two-stroke is BUZZY, not smooth -- strong harmonics well up the
// series. We stack `kSledToneHarmonics` at 1/n amplitude (a saw-like edge) and
// open a lowpass with revs, because a loaded engine gets brighter as well as
// higher. A pure sine here would read as an electric motor.
inline constexpr int kSledToneHarmonics = 8;

// Lowpass cutoff [Hz] across the rev range. At idle the tone is a muffled
// burble; pinned, it is a hard bright blare.
inline constexpr double kSledToneCutoffIdle = 420.0;
inline constexpr double kSledToneCutoffMax = 4200.0;

// Carries kSledVoiceTranspose for the reason spelled out at that constant: a
// transposed instrument moves its whole spectrum, and the sampled layers get
// that for free from resampling. Without it the tone keeps its old brightness
// over a lowered fundamental and stays the layer that reads high.
inline double sled_tone_cutoff(double revs) {
    const double r = std::clamp(revs, 0.0, 1.0);
    const double base =
        kSledToneCutoffIdle + (kSledToneCutoffMax - kSledToneCutoffIdle) * r;
    return base * kSledVoiceTranspose;
}

// Three cylinders never fire perfectly evenly. A few cents of detune between
// the stacked partials gives slow beating that stops the tone sounding
// synthetic. ⚠ This only works if each partial keeps its OWN phase: a single
// shared phase wrapped at the fundamental re-phases every partial to a common
// origin once per period, which both kills the beating and puts a hard step in
// the waveform. render/sled_synth.h therefore accumulates per-harmonic phase.
inline constexpr double kSledToneDetune = 0.006;  // fraction of f


// ---------------------------------------------------------------------------
// Layer gains. THE BALANCE -- Chad: "Make sure to balance the sounds."
// ---------------------------------------------------------------------------

// Master trim for the whole engine channel. The engine is the loudest thing a
// rider hears, but it must not bury the wind or the world.
inline constexpr double kSledMaster = 0.78;

inline constexpr double kSledIdleGain = 0.85;
inline constexpr double kSledRunGain = 0.90;    // the riding bed
inline constexpr double kSledAccentGain = 0.55; // the brap, ON TOP of the bed
inline constexpr double kSledToneGain = 0.42;

// Idle owns the voice at rest and is gone by the time the engine is working.
inline double sled_idle_level(double revs) {
    const double r = std::clamp(revs / 0.45, 0.0, 1.0);
    return kSledIdleGain * (1.0 - r) * (1.0 - r);  // squared = quicker retreat
}

// The riding bed rises with revs and is the complement of the idle bed, so the
// two hand over rather than both being present or both absent.
inline double sled_run_level(double revs) {
    const double r = std::clamp(revs / 0.45, 0.0, 1.0);
    return kSledRunGain * r * (2.0 - r) * 0.5 + kSledRunGain * 0.5 * r;
}

// The onset accent -- the bark of the pipe taking the load. Short by design;
// the BED carries the sustain, so this envelope no longer has to.
inline constexpr double kSledAccentLife = 0.55;  // [s]

inline double sled_accent_level(double age) {
    if (!(age > 0.0)) return 0.0;  // also catches NaN
    if (age >= kSledAccentLife) return 0.0;
    const double x = 1.0 - age / kSledAccentLife;
    return kSledAccentGain * x * x;
}

// The tone is gated on SUSTAIN, not on revs. `sustain` is how long the throttle
// has been continuously held [s]; below the knee a tap produces no tone at all,
// which is what keeps a tap a brap.
inline constexpr double kSledSustainKnee = 0.35;  // [s] taps stay under this
inline constexpr double kSledSustainFull = 1.10;  // [s] fully sung by here

// ⚠ Release must not be a cliff. An earlier version zeroed `sustain` the moment
// the throttle dipped, which took the tone from full to nothing in one sample
// -- a full-amplitude step click, and a 0.35 s dead zone before it could even
// begin to return. The synth now slews the applied gain, and this stays the
// TARGET only.
inline double sled_tone_level(double revs, double sustain) {
    if (!(sustain > kSledSustainKnee)) return 0.0;
    const double s = std::clamp(
        (sustain - kSledSustainKnee) / (kSledSustainFull - kSledSustainKnee),
        0.0, 1.0);
    return kSledToneGain * s * std::clamp(revs, 0.0, 1.0);
}

}  // namespace render
