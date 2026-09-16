#pragma once

#include <algorithm>

#include "render/audio_dsp.h"     // WSXor -- the fixed-seed PRNG this layer already owns
#include "render/mix_levels.h"    // kGameplayBusGain, for the headroom assert below
#include "render/town_ambience.h" // TownContext + the town gate this bell shares

// THE CHELMSFORD CHURCH BELL -- a peal of 1 to 12 chimes, two minutes behind
// the train.
//
// Chad, 2026-08-24: "please make it play different amounts of plays at random,
// sometimes it rings 3 times, sometimes 7 times, up to twelve to simulate the
// number of chimes it plays based on the hour ... It should play about two
// minutes after the train might, in and over chelmsford" -- and, when asked
// where the floor was: "from one chime up to 12, 3 was an example, it should be
// random though."
//
// So the count is 1..12 and it is DRAWN, not cycled. That is the one place this
// header deliberately parts company with render/town_ambience.h next door, and
// the difference is Chad's sentence. The train's cadence is a fixed four-gap
// table because four uneven gaps read less mechanical than a uniform draw and
// nobody counts the seconds between trains. A CHIME COUNT IS COUNTED -- it is
// the whole content of the sound, it is what "what time is it" means -- and a
// fixed table would be a bell that rings the same twelve hours in the same
// order forever. He asked for random and random is what the ear can check.
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT <random>, AND STILL IS RANDOM
// ---------------------------------------------------------------------------
// <random> is banned in this layer (music_director.h states the rule: the gate
// needs reproducibility) and it is not needed. render::WSXor in audio_dsp.h is
// the fixed-seed xorshift the gun and wind synths already draw from -- the same
// sequence every run, so a test can assert the exact peal it produces, while
// the sequence itself is long enough that no player will ever hear it repeat.
// Deterministic and random are not opposites here; unseeded is the thing that
// was banned.
//
// ---------------------------------------------------------------------------
// PURE. <algorithm> and audio_dsp.h only -- no raylib, no wall-clock, no
// allocation, nothing written back. Cosmetic, render-side, like every channel
// in this lane.

namespace render {

// The bell rings where the train runs: the same town-density gate, the same
// per-context radius, from render/town_ambience.h. "In and over chelmsford" is
// exactly the predicate that header already computes, and computing a second
// one here would be a fork -- two answers to "am I over the town" that can
// disagree. The app passes in the ONE answer, as it does for the train.
//
// Context is likewise TownContext, reused rather than redeclared, so a bell and
// a train can never disagree about whether you are riding or flying.

// ---------------------------------------------------------------------------
// THE ASSET -- measured, and load-bearing in a way no other row's length is
// ---------------------------------------------------------------------------
//
// sfx/church_bell.wav as it ships: 4.44 s, mono, one strike. The attack is
// inside the first 85 ms and it decays to digital silence by 4.35 s
// (build/soundbank_report.txt in the sound lane, and the row's own comment in
// tools/audio/soundbank.manifest.tsv).
//
// It is ONE strike, and that is the design: the peal is built by triggering it
// N times. A canned peal asset could only ever ring one number of times, which
// is the thing Chad asked for the opposite of.
inline constexpr double kBellChimeLen = 4.44;

// The gap between strikes. Chosen against the asset, not from taste: at 2.4 s
// the previous strike has fallen ~10 dB off its own peak (measured envelope:
// -9.4 dBFS at the attack, -19.7 by 2.4 s), so a new strike lands INTO the
// ring-out of the last one rather than on top of its attack. That overlap is
// what a bell tower sounds like; strikes spaced past kBellChimeLen would be
// twelve separate bells.
//
// It also sits inside the 2-3 s a real tower bell of this size takes between
// strikes, so a twelve-count runs 28.8 s -- about the length of the train pass
// it follows, which is a useful sanity check on "is this too long an event".
inline constexpr double kBellChimeGapSec = 2.4;

// ⚠ THE CONSEQUENCE, and the reason this channel is the first in the bank to
// need POLYPHONY: the asset rings for 4.44 s and is struck every 2.4 s, so two
// strikes are audible at once. Every other one-shot in this lane (the train,
// the blast, both animals) plays through a single raylib Sound, where a
// re-trigger RESTARTS the voice -- which here would truncate a ringing bell
// mid-decay, once per chime, up to eleven times a peal.
//
// So the app plays this through a ring of raylib Sound ALIASES (LoadSoundAlias,
// raylib 5.5 -- they share the sample data and cost nothing but a handle). This
// is how many it needs, and the assert below is a REAL property rather than a
// balance opinion: outgrow the ring by re-cutting the asset longer or by
// shortening the gap, and the truncation comes back silently.
inline constexpr int kBellVoiceCount = 4;

inline constexpr int bell_voices_needed() {
    // ceil(len / gap) strikes can be sounding, +1 so the voice about to be
    // struck is never the one still ringing.
    int n = 1;
    while (static_cast<double>(n) * kBellChimeGapSec < kBellChimeLen) ++n;
    return n + 1;
}

static_assert(kBellVoiceCount >= bell_voices_needed(),
              "the church bell's alias ring is too small for its own decay: a "
              "strike will restart a voice that is still ringing, which is the "
              "truncation the ring exists to prevent");

// ---------------------------------------------------------------------------
// THE COUNT -- 1 to 12
// ---------------------------------------------------------------------------
//
// Chad: "from one chime up to 12". Both ends are inclusive and both are real
// hours -- a single bong is one o'clock, not a bug, and it is left in for that
// reason rather than floored at 3 to look more deliberate.
inline constexpr int kBellChimeMin = 1;
inline constexpr int kBellChimeMax = 12;

// ...but never the SAME count twice running. A bell that rings seven and then
// seven again does not read as two hours, it reads as a loop that got stuck --
// and at a 1-in-12 chance it would happen to a player roughly every twelfth
// peal, which over a long ride is often enough to notice. This is the only
// constraint on the draw; everything else is uniform.
inline constexpr int kBellRepeatGuardTries = 8;

// ---------------------------------------------------------------------------
// "ABOUT TWO MINUTES AFTER THE TRAIN"
// ---------------------------------------------------------------------------
//
// Keyed to the train's firing, not to a clock of its own. That is what Chad's
// sentence says -- "two minutes after the train might" -- and it is the better
// sound anyway: the town gets a train out past the treeline, and then, while
// you are still listening to the country, the bell. Two unrelated schedules
// would collide at random and read as noise.
//
// A TABLE here, not the PRNG, and the split is deliberate. The COUNT is what
// the ear counts, so it is drawn. A delay of "about two minutes" is not
// counted by anyone -- four uneven values are already indistinguishable from
// random at this timescale, and this is the same reasoning (and the same shape)
// as kTrainGapsGround next door. Mean 120 s exactly.
inline constexpr int kBellDelayCount = 4;
inline constexpr double kBellDelaysSec[kBellDelayCount] = {105.0, 126.0, 114.0,
                                                           135.0};

// ---------------------------------------------------------------------------
// MIX -- and this pair is DERIVED FROM A CEILING, not from taste
// ---------------------------------------------------------------------------
//
// The natural anchor is the train: same AMBIENCE bus, same -24 master target,
// same class of sound (a town heard from outside it), and Chad has SIGNED the
// train's levels by ear -- 3.40 ground / 2.90 air, "good the sounds are right
// now". Starting a new channel at a level he has approved is better evidence
// than starting it at one I reasoned to.
//
// It cannot have that level, and the reason is measured rather than felt.
// sfx/church_bell.wav delivers -8.8 dBTP (the report), the app multiplies by
// kGameplayBusGain (0.63), and raylib's SetSoundVolume does not clamp. At the
// train's 3.40 one strike alone lands near -2.2 dBTP -- and TWO STRIKES ARE
// AUDIBLE AT ONCE here, which no other channel in this bank can say. Worst-case
// coherent sum of a fresh strike and the -10 dB tail under it is x1.3, +2.3 dB,
// which puts it over full scale.
//
// So the ground gain is the loudest value that keeps the OVERLAPPED peak inside
// the -3.0 dBTP the animals' ceilings are already derived against
// (kCryCeilingWolf / kCryCeilingWildcat in render/pump_ambience.h are each "the
// product that lands its asset at -3.0 dBTP after kGameplayBusGain" -- this is
// the same house rule, with the overlap factor this channel adds):
//
//     0.363 (asset TP) x 0.63 (bus) x 1.30 (overlap) x gain <= 0.708 (-3 dBTP)
//     => gain <= 2.38        -> 2.35 ground, deliberately just under it
//
// The air keeps the train's ratio under the ground (2.90/3.40 = 0.853), for the
// train's reason: a town sound you hear as clearly at altitude as on the street
// stops sounding like distance. 2.35 x 0.853 -> 2.00.
//
// WHERE THAT LANDS AGAINST THE SIGNED MIX, which is the check that matters more
// than the arithmetic: ONE bell strike delivers 0.363 x 2.35 x 0.63 = 0.537, a
// peak near -5.4 dBTP. The signed train pass delivers 0.257 x 3.40 x 0.63 =
// 0.551, near -5.2. The bell is peak-matched to the train Chad flew and called
// right, and only reaches the -3 line when two strikes coincide. The 3.4 dB the
// TRIM sits under the train's is not the bell arriving quieter -- it is the
// asset being 3 dB peakier to start with.
//
// (The row also verifies 1.0 LU hot, -23.0 against the -24 target -- the same
// short-mono-file meter disagreement the wildcat row documents. It is left
// uncorrected rather than trimmed away: this derivation is against the MEASURED
// peak, which already contains it, and fly 3 ruled that this class of sound errs
// on the loud side.)
//
// ⭐ SIGNED 2026-08-24, on the first listen: "ookay bell sounds good!" and,
// when I wrote the caveat below as though only the peal had been approved,
// "I heard it, it passes my review so thats why I said to push it."
//
// SO THIS PAIR IS HEARD, NOT DERIVED, and that changes its status rather than
// just its confidence. The rulings list at the top of render/mix_levels.h says
// what an approval buys: DO NOT RETUNE THESE TO MAKE A POLICY TIDIER. Move one
// only when Chad says to, or when a new channel arrives and changes what the mix
// is. The derivation above is kept because it is still the CEILING these numbers
// have to live under -- but it is no longer the argument for them. The ear is.
//
// ⚠ AND NOTE WHAT THAT MEANS IF HE EVER ASKS FOR THIS LOUDER: the honest lever
// is NOT this trim, which is already near the overlapped ceiling. In order of
// honesty: widen kBellChimeGapSec past kBellChimeLen so the strikes stop
// overlapping (buys the whole 2.3 dB back, and costs the tower's ring), or
// re-master the row to a lower true peak and raise the trim against it.
// Trimming past the ceiling buys loudness by clipping the attack, which is the
// part of a bell that carries.
//
// ⚠ What is signed is the BALANCE at this bus setting, as always. The gate still
// cannot hear any of it -- ctest never runs seads.exe -- so the protection for
// these numbers is this comment and the mix_levels.h rule it points at, not a
// test. Do not write a test that pins them: this lane has retracted three
// assertions for exactly that, and a signed level is the last thing that should
// have to argue with a compiler.
inline constexpr double kBellGroundGain = 2.35;
inline constexpr double kBellFlyingGain = 2.00;

// The delivered-peak claim above, kept as an assertion so a future raise has to
// argue with the measurement rather than slide past it. Asset true peak in
// linear terms, and the overlap factor the ring makes possible.
inline constexpr double kBellAssetPeak = 0.363;   // -8.8 dBTP, measured
inline constexpr double kBellOverlapSum = 1.30;   // fresh strike + -10 dB tail

// ⚠ kBellOverlapSum is a BOUND, not a measurement, and it is deliberately the
// pessimistic one: it assumes the fresh strike and the tail under it are
// coherent at the same instant, which two uncorrelated bell partials are not.
// tools/bell_peal_preview.cpp renders the real scheduler against the real asset
// and measures what actually comes out -- -4.4 dBFS on the ground, -5.8 in the
// air, against the -3.3 this bound predicts. So there is about a dB of real
// headroom beyond what the assert below claims. It is left claimed anyway: a
// bound you can prove beats a measurement of one particular peal, and the ear
// ruling that wants that dB has not happened yet.

// -3 dBTP, the margin this lane holds. A real property of the delivered signal,
// not a balance opinion -- which is the whole distinction mix_levels.h was
// rewritten around after two assertions here encoded guesses and had to be
// retracted. Raise kBellGroundGain past this and the peal clips; the comment
// above says where the next dB has to come from instead.
static_assert(kBellGroundGain * kBellAssetPeak * kGameplayBusGain *
                      kBellOverlapSum <
                  0.708,
              "two overlapping church-bell strikes now exceed the -3 dBTP "
              "margin: the peal will clip its own attacks");

inline double bell_gain(TownContext ctx) {
    return ctx == TownContext::kFlying ? kBellFlyingGain : kBellGroundGain;
}

// The air stays under the ground, for the train's reason. Stated because it is
// the property, not the two numbers, that has to survive a retune.
static_assert(kBellFlyingGain < kBellGroundGain,
              "the bell is as loud from the air as from the street, which "
              "stops it sounding like a town you are passing over");

// ---------------------------------------------------------------------------
// THE SCHEDULER
// ---------------------------------------------------------------------------

// What one tick produced. `strike` is the single frame on which the app should
// sound a chime; `peal_start` marks the first strike of a peal, which is when
// the app fixes the level for the whole thing (a raylib Sound has one volume,
// and re-evaluating it per chime would step the level mid-peal if you took off
// halfway through).
struct BellTick {
    bool strike = false;
    bool peal_start = false;
    int chime_index = 0;  // 1-based, within this peal
    int chime_total = 0;  // how many this peal will ring
};

struct BellTower {
    // The pending countdown: a train has passed and the bell is coming.
    bool armed = false;
    double wait = 0.0;
    int delay_index = 0;

    // The peal in progress.
    int chimes_left = 0;
    int chime_total = 0;
    double since_strike = 0.0;

    // The draw. Fixed seed, so the gate sees the same peal every run.
    WSXor rng{};
    int last_count = 0;

    bool ringing() const { return chimes_left > 0 || chime_total > 0; }

    // Called on the tick render::TrainAmbience fires.
    //
    // Armed off the SCHEDULER, not off the PlaySound: if the train asset failed
    // to load, the town still has a church. Tying the bell to whether another
    // asset decoded would make a missing file silence two channels.
    //
    // Ignored while already armed or already ringing. Train gaps run 105-225 s
    // and this delay is ~120 s, so a second train can easily arrive mid-peal --
    // stacking them would give two overlapping peals and a count nobody can
    // read, which is the one thing this channel must not do.
    void on_train() {
        if (armed || ringing()) return;
        armed = true;
        wait = 0.0;
    }

    // Stop a peal in progress. Keeps `armed` and its countdown intact.
    //
    // ⚠ THIS IS NOT THE SAME RULE THE TRAIN USES, and the difference is which
    // question each answers. The train's clock PAUSES when you leave town,
    // because the schedule is a fact about the world: the train is out there
    // whether or not you are, and being near town is only when you can hear it.
    // Same here for the countdown. But a peal in progress is a fact about YOUR
    // EARS -- you were listening to a bell and you flew out of earshot. Pausing
    // that would resume the peal at chime four of seven when you came back
    // minutes later: a bell that waited for you, which is the "stuck to the
    // camera" failure this lane keeps designing against.
    void abandon_peal() {
        chimes_left = 0;
        chime_total = 0;
        since_strike = 0.0;
    }

    // Advance. `near_town` is render::is_town(...) with the same per-context
    // radius the train uses; dt is the same clamped frame dt.
    BellTick update(bool near_town, double dt) {
        BellTick out;
        if (!near_town) {
            abandon_peal();
            return out;
        }
        if (!(dt > 0.0)) return out;

        if (chimes_left > 0) {
            since_strike += dt;
            if (since_strike >= kBellChimeGapSec) {
                since_strike -= kBellChimeGapSec;
                --chimes_left;
                out.strike = true;
                out.chime_total = chime_total;
                out.chime_index = chime_total - chimes_left;
                if (chimes_left == 0) chime_total = 0;  // peal complete
            }
            return out;
        }

        if (!armed) return out;
        wait += dt;
        if (wait < kBellDelaysSec[delay_index]) return out;

        // The countdown is up: draw the hour and strike it immediately. The
        // first chime IS the arrival of the peal -- waiting one more gap before
        // the first strike would put the bell 2.4 s later than the delay says.
        wait = 0.0;
        armed = false;
        delay_index = (delay_index + 1) % kBellDelayCount;
        chime_total = draw_count();
        chimes_left = chime_total - 1;
        since_strike = 0.0;
        out.strike = true;
        out.peal_start = true;
        out.chime_total = chime_total;
        out.chime_index = 1;
        if (chimes_left == 0) chime_total = 0;
        return out;
    }

    // Uniform over [kBellChimeMin, kBellChimeMax], rejecting a repeat of the
    // last count. Bounded tries so this always terminates even if the generator
    // were ever replaced by something degenerate -- an audio scheduler that can
    // spin is worse than a bell that rings seven twice.
    int draw_count() {
        const int span = kBellChimeMax - kBellChimeMin + 1;
        int n = last_count;
        for (int t = 0; t < kBellRepeatGuardTries; ++t) {
            n = kBellChimeMin + static_cast<int>(rng.uni() * span);
            n = std::clamp(n, kBellChimeMin, kBellChimeMax);
            if (n != last_count) break;
        }
        last_count = n;
        return n;
    }
};

}  // namespace render
