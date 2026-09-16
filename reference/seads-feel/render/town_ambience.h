#pragma once

#include <algorithm>

// TOWN AMBIENCE -- the distant train.
//
// Chad, 2026-08-17: "The train sounds every once in a couple of minutes when on
// the snowmachine near a town or in a town and can be heard also when flying
// over a town sometimes."
//
// And, on the slag train already in the world: "that train is different it is
// for the slag dump, I dont have a chelmsford train model created yet but still
// want the sound of it for now."
//
// So this is deliberately NOT attached to render/train.cpp. That one is an
// electric trolley hauling slag pots around a closed spur at Copper Cliff; this
// is a mainline train somewhere out past the treeline that you never see. It is
// gated on being near a town and on a slow cadence, and it has no object.
//
// WHAT COUNTS AS A TOWN. Not a curated list -- there isn't one, and a town is
// exactly where the buildings are. world::BuildingColliders already indexes
// every building into an equirect grid, so the count of prisms in the player's
// own cell IS the local building density, readable in O(1) from the prefix
// spans. The app does that lookup and passes the count here; this header owns
// only the POLICY, so it stays pure and testable with no world dependency.
//
// Cosmetic, render-side only. Firewall: only <algorithm>. No raylib, no
// <random>, no wall-clock, no allocation.

namespace render {

// How many buildings in the local neighbourhood before it reads as a town
// rather than a farmstead. A dial -- the honest tuning is by flying over
// Chelmsford and Copper Cliff and seeing where it arms.
inline constexpr int kTownPrismCount = 6;

inline bool is_town(int prisms_nearby) { return prisms_nearby >= kTownPrismCount; }

// Where the listener is. The two contexts Chad named get different cadences:
// on the machine you are IN the town and hear it regularly; from the air you
// are passing over at speed and only catch it "sometimes".
enum class TownContext { kGround = 0, kFlying = 1 };

// ---------------------------------------------------------------------------
// HOW BIG "NEARBY" IS -- added 2026-08-24, and it is why this never fired.
// ---------------------------------------------------------------------------
//
// Chad, 2026-08-24: "where is that train sound when flying over chelmsford?"
//
// The scheduler below was finished, unit-tested and correct three days before
// that question, and it had no caller at all -- app/main.cpp never included
// this header. But wiring it as originally specified would STILL have produced
// silence from the air, and that is the more interesting half of the bug.
//
// The app reads density out of world::BuildingColliders, whose index cells are
// ~92 m of ground. Ask "how many buildings in my cell" from an aircraft and
// the answer is yes-for-about-a-second: you cross a 92 m cell at 100 m/s in
// under a second, so `near_town` flickers true a handful of times per pass and
// the clock below accumulates maybe 5 s per overflight -- against a 40 s grace
// and gaps of several minutes. It would have taken tens of passes over
// Chelmsford to hear one train, which is indistinguishable from broken, and
// which is exactly the report we got.
//
// So the neighbourhood is a RADIUS, per context, and the radius is what makes
// the clock run at a rate the gaps were written for:
//
//   GROUND 300 m -- you are in the streets; the buildings around you ARE the
//   town, and 300 m is about where Chelmsford stops being Chelmsford.
//
//   AIR 4000 m -- "flying over a town" is an act with a size. At 90-120 m/s a
//   4 km radius is a 65-90 s pass, which is one real gap's worth of clock per
//   overflight rather than a rounding error.
inline constexpr double kTownRadiusGroundM = 300.0;
inline constexpr double kTownRadiusFlyingM = 4000.0;

inline double town_radius_m(TownContext ctx) {
    return ctx == TownContext::kFlying ? kTownRadiusFlyingM
                                       : kTownRadiusGroundM;
}

// The threshold scales with the circle, or the wider air radius would call a
// pair of farmsteads 4 km apart a town. 6 buildings inside 300 m is a cluster;
// 30 inside 4 km is a settlement and not a roadside.
//
// These count DISTINCT colliders (world::BuildingColliders::prisms_near_m does
// the de-duplication), so they mean what they say and are not inflated by the
// index's span insertion.
inline constexpr int kTownPrismCountFlying = 30;

inline int town_prism_count(TownContext ctx) {
    return ctx == TownContext::kFlying ? kTownPrismCountFlying
                                       : kTownPrismCount;
}

inline bool is_town(int prisms_nearby, TownContext ctx) {
    return prisms_nearby >= town_prism_count(ctx);
}

// Measured length of sfx/train_chelmsford_pass.wav as it ships (cut 30:57 of
// the source; see the soundbank manifest). It plays as ONE non-polyphonic
// sound, so a re-trigger restarts it: every gap below must clear this or the
// pass truncates itself. Declared here so a re-cut moves ONE number and the
// tests follow it, instead of a literal typed into an assertion.
inline constexpr double kTrainPassLen = 27.0;

// Cadence [s]. "every once in a couple of minutes" on the ground. A fixed table
// rather than an RNG -- <random> is banned in this layer, the gate needs
// reproducibility, and four uneven gaps read less mechanical than a uniform
// draw anyway. (Same reasoning as StopeRumble in music_director.h.)
inline constexpr int kTrainGapCount = 4;
inline constexpr double kTrainGapsGround[kTrainGapCount] = {105.0, 165.0, 132.0,
                                                            198.0};

// From the air, rarer -- "sometimes".
//
// RE-CUT 2026-08-24, from {255, 390, 310, 470}. Those were ~2.4x the ground
// cadence, written when the clock was imagined to run continuously. It does
// not: it only advances while you are over a town, so the AIR IS ALREADY RARE
// BY CONSTRUCTION -- a 65-90 s pass against a 130-225 s gap is "about every
// second or third overflight", which is what "sometimes" sounds like. The old
// numbers multiplied that rarity by another 2.4 and put the first train
// somewhere past the fifth pass over Chelmsford.
//
// Still LONGER than the ground gaps, so the ordering the tests pin is intact
// and riding into town remains the reliable way to hear it.
inline constexpr double kTrainGapsFlying[kTrainGapCount] = {130.0, 190.0, 155.0,
                                                            225.0};

// Grace after first arriving somewhere before the first one can fire, so the
// train never announces itself the instant a town scrolls into range.
inline constexpr double kTrainFirstGap = 40.0;

// Mix trims. The asset is mastered to the AMBIENCE bus (-24 LUFS); these are
// balance, not loudness. From the air it is further away and competing with the
// wind, so it sits lower rather than being pushed up to cut through -- a train
// you can clearly hear at altitude stops sounding like distance.
//
// RAISED 2026-08-24 with the animals, same ruling ("the volume turned up on
// those"), same reasoning and the same measured headroom: ground 0.90 -> 1.20
// (+2.5 dB), air 0.62 -> 0.95 (+3.7 dB). sfx/train_chelmsford_pass.wav
// measures -11.8 dBTP (build/soundbank_report.txt), the app multiplies by
// kGameplayBusGain (0.63), and raylib's SetSoundVolume does not clamp -- so
// the loudest path here is 1.20 x 0.63 = 0.76, landing that peak near
// -14 dBTP. The air keeps its trim under the ground for the reason it always
// did: a train you hear clearly at altitude stops sounding like distance.
//
// RAISED AGAIN 2026-08-24, after the fly that first had a train in it. Chad:
// "I heard the train ... I can barely hear the train through the music and the
// train is not loud enough." Ground 1.20 -> 1.70 (+3.0 dB), air 0.95 -> 1.45
// (+3.7 dB); the air moved further because that is where he was listening.
// Together with the bed coming down 3 dB (kMusicFlownSurface, mix_levels.h)
// the train sits ~6-7 dB clear of where he heard it.
//
// Headroom, again measured rather than assumed: the asset is -11.8 dBTP
// (build/soundbank_report.txt) and the app multiplies by kGameplayBusGain
// (0.63), so the loudest path is 1.70 x 0.63 = 1.07 -- a delivered peak near
// -11.2 dBTP. Eight dB of margin left; raylib does not clamp, so this is real
// gain and not a number that saturates on the way out.
//
// RAISED AGAIN 2026-08-24 (fly 4): ground 1.70 -> 3.40, air 1.45 -> 2.90, +6.0 dB
// both. Chad: "The train should be quite loud ... right now I have to question
// if I heard them over the music."
//
// THAT SENTENCE IS THE SPEC, and it is a harder one than "louder": a sound you
// have to ASK whether you heard has failed even when it was technically
// audible. +6 dB is a doubling of perceived loudness, which is the step that
// answers a question like that; +3 would have moved it and left the question.
//
// The air keeps a trim under the ground, but the gap is down to 1.4 dB now. It
// is nearly spent as a distance cue -- if he wants the air louder again, the
// honest move is to raise BOTH and accept that the cue lives in the reverb-less
// dryness of the asset rather than in level.
//
// Peaks, measured as always (asset -11.8 dBTP, x kGameplayBusGain 0.63, raylib
// does not clamp): ground delivers 2.14 for a peak near -5.2 dBTP, air 1.83 for
// -6.6. Both still clear of the -3 dBTP margin this lane holds, but this is the
// last +6 the train has; the next one needs a re-master, not a trim.
// ⭐ SIGNED 2026-08-24, fly 4: "good the sounds are right now". These two are
// heard levels now, not reasoned ones -- see the rulings list at the top of
// render/mix_levels.h for what that changes about moving them.
inline constexpr double kTrainGroundGain = 3.40;
inline constexpr double kTrainFlyingGain = 2.90;

inline double train_gain(TownContext ctx) {
    return ctx == TownContext::kFlying ? kTrainFlyingGain : kTrainGroundGain;
}

// The scheduler.
//
// The clock ONLY advances while you are near a town. Leaving does not reset it,
// so wandering in and out of Chelmsford accumulates toward the next one instead
// of restarting the wait every time -- the train is out there regardless; being
// near town is just when you can hear it.
struct TrainAmbience {
    double timer = 0.0;
    int gap_index = 0;
    bool fired_once = false;

    // Returns true on the single tick the train should be triggered.
    //
    // The gap in force is re-read from the CURRENT context every tick, not
    // frozen at the last firing. Taking the gap from where you were when the
    // previous one fired meant a rider who banked 100 s in town then took off
    // would serve out a short GROUND gap in the air and hear the train almost
    // immediately -- against the "sometimes" Chad asked for. (Red-team P2-3.)
    bool update(bool near_town, TownContext ctx, double dt) {
        if (!near_town || !(dt > 0.0)) return false;
        timer += dt;
        if (timer < gap_now(ctx)) return false;
        timer = 0.0;
        gap_index = (gap_index + 1) % kTrainGapCount;
        fired_once = true;
        return true;
    }

    // The gap currently being waited out, for this context.
    double gap_now(TownContext ctx) const {
        if (!fired_once) return kTrainFirstGap;
        return (ctx == TownContext::kFlying) ? kTrainGapsFlying[gap_index]
                                             : kTrainGapsGround[gap_index];
    }
};

}  // namespace render
