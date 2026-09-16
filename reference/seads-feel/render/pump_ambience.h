#pragma once

#include <algorithm>

// PUMP AMBIENCE -- the two animals that live at the two surface pumps.
//
// Chad, 2026-08-20: "I want a wolf cry to sound when you get to the enemy
// surface pump in sudbury. And I will go find a wildcat [sound] for near levak
// and onaping falls allied pump ... cave_hiss in the sound effects ... please
// add those sounds when you fly over the area or snomobile close to it (those
// respective pumps)."
//
// So: one cry per SURFACE pump, fired on approach, from the air or from the
// machine. This header owns the whole POLICY -- what counts as "close", how
// often a cry repeats while you linger, and how loud it sits -- and nothing
// else. It is pure: no raylib, no world, no assets, no clock, no allocation,
// only <algorithm>. The app measures the distances and plays the sounds.
//
// ---------------------------------------------------------------------------
// BOUND TO THE PLACE, NOT TO THE ALLEGIANCE
// ---------------------------------------------------------------------------
//
// Chad named the pumps by faction ("the enemy surface pump in sudbury", "allied
// pump" at Onaping) because that is how he flies them, but what he actually
// described is two GEOGRAPHIES: a wolf out on the Coniston side, a cat up in
// the bush behind Levack and Onaping Falls. This layer therefore keys the wolf
// to world::kPumpSudburySurface and the cat to world::kPumpValleySurface as
// PLACES.
//
// That distinction is load-bearing, not pedantry. [conquest] player_faction is
// a config switch: flying as Sudbury makes the Onaping pump the enemy one and
// the whole naming inverts. Keyed to allegiance, the two animals would swap
// ends of the map when a line in game.toml changed -- a wolf that follows you
// around is a mechanic, and Chad asked for country, not a mechanic. Keyed to
// place, the bush behind Levack has a cat in it whoever you are flying for.
//
// ---------------------------------------------------------------------------
// WHY A CRY IS NOT A TRAIN
// ---------------------------------------------------------------------------
//
// render/town_ambience.h solves a neighbouring problem and the shape below is
// deliberately different from it in one way. The train is a thing that happens
// on its own schedule whether or not you are there to hear it, so its clock
// runs only near town and leaving does not reset it -- you accumulate toward
// the next one. These animals are the opposite: they are reacting to YOU
// arriving. The cry is therefore an ARRIVAL event with a hold-down, not a
// free-running cadence you happen to walk into, and the state below re-arms on
// a genuine departure rather than banking time.

namespace render {

// ---------------------------------------------------------------------------
// Which cry. Two of them, indexed so the app can hold one scheduler each in a
// plain array without inventing its own enum.
// ---------------------------------------------------------------------------
enum class PumpCry { kWolfSudbury = 0, kWildcatValley = 1 };
inline constexpr int kPumpCryCount = 2;

// Where the listener is. Same two contexts the train uses, same reason: on the
// machine you are down in it, from the air you are passing over at speed.
enum class CryContext { kGround = 0, kFlying = 1 };

// ---------------------------------------------------------------------------
// HOW CLOSE IS "CLOSE" -- the two audible radii, in metres of GROUND distance.
// ---------------------------------------------------------------------------
//
// Measured against the map these pumps actually sit on (world/faction_bubbles.h
// bakes both anchors): each sits ~5 km inside its own faction's hard bubble
// edge, and the two are 42.2 km apart along the ground on the R = 15 km sphere
// -- 45% of the way around the whole planet. So there is a lot of room here,
// and the radii below are chosen against the two ways Chad said he would
// arrive rather than against any crowding.
//
// (Do not reach for the "~30.8 km" in faction_bubbles.h for this: that is each
// pump's distance to the ENEMY CENTER, which is what the distal-end ruling was
// argued from. Pump-to-pump is the number that matters here, and it is larger.)
//
// GROUND, 1200 m. On the snowmachine "close to it" means close: at a sled's
// 15-25 m/s you are inside this for a minute and a half of riding, and the pump
// is in sight for most of it. Much wider and the cry stops belonging to the
// pump and becomes a region.
//
// AIR, 3200 m. "Fly over the area" is a different act -- at 90-120 m/s a 3.2 km
// radius is a ~55-70 s pass, which is long enough for the cry to land while you
// are still visibly over the place rather than a dot past it. This is the one
// number most likely to want moving after a fly; it moves alone.
//
// AIR RAISED 3200 -> 5000 m, 2026-08-24. Chad: "I havent heard the wolf howl in
// seads-recon play yet". It was never silent -- it was never ARMED. The wolf
// sits over Coniston and the cat over Onaping/Dowling, ~15 km and ~30 km from
// the Chelmsford side he flies; a 3.2 km bubble around a single point is a
// target you have to nearly aim at, and a route that misses it by 4 km hears
// nothing and gives no hint that anything was there. 5 km makes the bubble
// ~2.4x the area for the same flight path -- still a PLACE (10 km of arming
// diameter against 42 km of pump separation), just one you can stumble into.
// The trims below are the other half of the same complaint; neither alone
// would have been the answer.
inline constexpr double kCryRangeGroundM = 1200.0;
inline constexpr double kCryRangeFlyingM = 5000.0;

// The two radii must not overlap, or a position between the pumps would arm
// both animals at once. 2 * 3200 = 6.4 km against 42.2 km of separation, so
// this is nowhere near close -- asserted anyway, because the anchors have been
// moved once already (2026-07-25, to the distal ends) and could move again.
//
// A FLOOR, deliberately a little under the measured 42196 m: the assertion
// below only needs a lower bound, and a value pinned to the last digit would
// have to be re-derived on any harmless re-bake. The test re-measures the real
// separation from the baked directions and checks it against this, so the
// constant and the map cannot drift apart silently.
inline constexpr double kPumpSeparationM = 42000.0;
static_assert(2.0 * kCryRangeFlyingM < kPumpSeparationM,
              "the two pump ranges overlap -- one position would arm both "
              "animals, and the map would have a wolf and a cat in it at the "
              "same instant");

inline double cry_range_m(CryContext ctx) {
    return ctx == CryContext::kFlying ? kCryRangeFlyingM : kCryRangeGroundM;
}

// In range at all, for this context.
inline bool in_cry_range(double dist_m, CryContext ctx) {
    return dist_m >= 0.0 && dist_m <= cry_range_m(ctx);
}

// ---------------------------------------------------------------------------
// SHIPPED ASSET LENGTHS [s], as tools/audio/soundbank.manifest.tsv builds them.
// Declared here so every gap below can be checked against them by the compiler
// instead of by a literal typed into a test. Each cry plays as ONE
// non-polyphonic raylib Sound, so a re-trigger restarts it: any gap shorter
// than these truncates the animal mid-cry.
// ---------------------------------------------------------------------------
inline constexpr double kWolfCryLen = 5.0;     // sfx/wolf_cry.wav, cut full
inline constexpr double kWildcatCryLen = 7.0;  // sfx/wildcat_cry.wav, cut 0:7
inline constexpr double kLongestCryLen =
    kWolfCryLen > kWildcatCryLen ? kWolfCryLen : kWildcatCryLen;

// ---------------------------------------------------------------------------
// THE CADENCE.
// ---------------------------------------------------------------------------
//
// kCryArriveDelay -- the grace after crossing into range before the first cry.
// Not zero, on purpose: firing on the exact tick the radius is crossed makes
// the animal sound like a trigger volume in a game, because that is what it
// would be. Two and a half seconds puts the cry just far enough behind your
// arrival that it reads as something noticing you.
inline constexpr double kCryArriveDelay = 2.5;

// Then, while you stay, it calls again on an uneven table. A fixed table rather
// than an RNG for the same three reasons town_ambience.h gives: <random> is
// banned in this layer, the gate needs reproducibility, and four uneven gaps
// read less mechanical than a uniform draw anyway.
//
// These are much shorter than the train's minutes. You are only ever near a
// pump for a short while -- the whole ground radius is ~90 s of riding -- so a
// two-minute cadence would mean the second cry never happens and the animal has
// no presence beyond announcing you. Around a minute lets a rider who parks at
// the pump hear it two or three times.
inline constexpr int kCryGapCount = 4;
inline constexpr double kCryGapsGround[kCryGapCount] = {41.0, 67.0, 52.0, 78.0};

// From the air, longer. A fast pass should give you ONE cry, not a chorus: two
// calls 40 s apart while you circle would read as a loop rather than as an
// animal. 1.6x the ground table.
inline constexpr double kCryGapsFlying[kCryGapCount] = {66.0, 107.0, 83.0,
                                                        125.0};

// Every gap must clear the longest asset, or a cry cuts its own predecessor off
// mid-howl. Checked by the compiler rather than hoped for, because a re-cut of
// either wav moves the lengths above and this is the invariant it would break.
static_assert(kCryGapsGround[0] > kLongestCryLen &&
                  kCryGapsGround[1] > kLongestCryLen &&
                  kCryGapsGround[2] > kLongestCryLen &&
                  kCryGapsGround[3] > kLongestCryLen &&
                  kCryGapsFlying[0] > kLongestCryLen &&
                  kCryGapsFlying[1] > kLongestCryLen &&
                  kCryGapsFlying[2] > kLongestCryLen &&
                  kCryGapsFlying[3] > kLongestCryLen,
              "a cry gap is shorter than the cry itself -- the animal would "
              "cut itself off");

// The arrival grace does NOT clear an asset length (2.5 s against 7 s), and it
// is not meant to. Leave and come straight back and the entry cry would land
// 2.5 s after re-entry, chopping the cry you left on -- so that case is guarded
// by the re-arm hold below instead, which is both longer than any asset and the
// thing that decides whether a re-entry greets you at all. Named here because
// the two guards look interchangeable and are not.

// ---------------------------------------------------------------------------
// THE RE-ARM HOLD [s]. How long you must be OUT of range before the arrival cry
// can fire again.
// ---------------------------------------------------------------------------
//
// This is the piece that makes it an animal instead of a trigger volume.
// Without it, riding along the edge of the radius -- which is exactly what
// happens when you circle a pump, or when you sit near the boundary and the
// distance dithers by a metre -- re-fires the arrival cry every time you cross,
// and the wolf machine-guns. With it, a boundary-dither costs you nothing and
// only a real departure re-arms the greeting.
//
// 30 s: comfortably longer than any plausible dither or overshoot, short enough
// that going away to do something and coming back gets you greeted again. It
// also, incidentally, exceeds every asset length, which is what stops a
// re-entry cry from chopping the cry you left on.
inline constexpr double kCryReArmS = 30.0;
static_assert(kCryReArmS > kLongestCryLen,
              "the re-arm hold no longer guarantees the previous cry has "
              "finished before a re-entry can start another");

// ---------------------------------------------------------------------------
// MIX TRIMS.
// ---------------------------------------------------------------------------
//
// Both assets are mastered to the AMBIENCE bus (-24 LUFS) alongside the train,
// so these are BALANCE, not loudness -- and they are the trims Chad's ear will
// move, so they are one dial each rather than folded into the arithmetic.
//
// Flying sits lower than ground for the reason the train's does: from the air
// the animal is further away AND competing with the wind bed, and an animal you
// can hear clearly at 900 m of altitude stops sounding like distance. Pushing
// it up to cut through would trade the whole effect for audibility.
//
// RAISED 2026-08-24 on Chad's "the volume turned up on those": ground
// 0.92 -> 1.30 (+3.0 dB), air 0.60 -> 1.05 (+4.9 dB). The air moved further
// because the air is where he was listening and where it was quietest.
//
// ABOVE UNITY IS DELIBERATE AND IT IS MEASURED, not a slipped decimal. Two
// facts make it safe. First, raylib does NOT clamp: SetSoundVolume writes
// straight through to AudioBuffer::volume (raudio.c) with no bound, so >1
// amplifies rather than silently saturating -- the old comment in
// test_pump_ambience.cpp claiming "no headroom above unity" was wrong about
// the library. Second, these assets have the headroom to spend: build/
// soundbank_report.txt measures wolf_cry at -16.6 dBTP and wildcat_cry at
// -6.6 dBTP true peak, and everything here is multiplied by kGameplayBusGain
// (0.63, -4 dB) before it reaches the mixer. The loudest path in the whole
// system is the cat at ground zero-distance: 1.30 x 0.89 x 0.63 = 0.73, which
// lands its -6.6 dBTP peak at about -9.4 dBTP. Nothing is near clipping.
//
// Flying still sits under ground, for the reason it always did -- but the gap
// is narrower now (0.81 vs 0.71 of ceiling) because the original 0.65 ratio was
// applying a distance cue on top of a range that already applies one.
//
// RAISED AGAIN 2026-08-24. Chad heard the CAT on the fly that followed the
// first raise -- the first time either animal has been reported at all -- and
// put it in the same sentence as the train: "make the sound loud enough". Of
// the wolf: "I didnt test the wolf sound but I'd imagine it suffers a similar
// problem." So both move together, because they are one channel wearing two
// coats and splitting them on an untested guess would be inventing a balance
// nobody has heard. Ground 1.30 -> 1.75 (+2.6 dB), air 1.05 -> 1.55 (+3.4 dB).
//
// With the bed down 3 dB in the same pass, that is ~6 dB of relief against the
// thing he named. Headroom still measured, and the CAT is the binding case
// because it is the hotter asset (-6.6 dBTP): 1.75 x kWildcatCryGain (0.89)
// x kGameplayBusGain (0.63) = 0.98, a delivered peak near -6.8 dBTP. The wolf
// at the same trim lands near -15.8. Neither is close to full scale.
//
// RAISED AGAIN 2026-08-24 (fly 4): ground 1.75 -> 3.50, air 1.55 -> 3.10,
// +6.0 dB both. Chad, of the wolf: "The train should be quite loud and the howl
// as well, right now I have to question if I heard them over the music." Same
// +6 as the train and for the same reason -- a doubling of perceived loudness
// is the step that answers "did I hear that", where +3 leaves the question
// standing.
//
// ⚠ ONLY THE WOLF ACTUALLY GETS THE FULL +6. The cat cannot take it, and that
// is an ASSET fact, not a taste call -- see kWildcatCryGain below.
// ⭐ SIGNED 2026-08-24, fly 4: "good the sounds are right now". Note the CAT
// is signed at the level its true peak allows (+3.7 dB against the wolf's
// +6.0), so what he approved for that animal is a peak-limited compromise --
// if a quieter-peaked recording ever replaces it, that is a NEW question for
// his ear and not a free raise.
inline constexpr double kCryGroundGain = 3.50;
inline constexpr double kCryFlyingGain = 3.10;

// Per-animal trim, on top of the context gain.
//
// The wolf is 1.00 -- it lands on its mastering target and needs nothing.
//
// The cat is 0.89 (-1.0 dB) and that number has a specific job: sfx/
// wildcat_cry.wav verifies at -23.0 LUFS against the -24 AMBIENCE target, a
// meter disagreement documented in full on its manifest row (loudnorm and
// ebur128 gate that short mono file differently; re-aiming the master does not
// escape it). Rather than bend a mastering target to flatter a report, the 1 dB
// is corrected HERE, where balance belongs and where Chad can move it by ear
// without rebuilding an asset. Delete this trim if the row is ever re-mastered
// onto the house target.
// ⚠ THE CAT'S TRIM STOPPED BEING A LOUDNESS CORRECTION ON 2026-08-24 (fly 4).
// 0.89 -> 0.68. It is now a PEAK LIMIT, and the difference matters to whoever
// moves it next.
//
// The two assets are both mastered to -24 LUFS, so they are equally loud. They
// are NOT equally peaky: build/soundbank_report.txt measures wolf_cry at
// -16.6 dBTP and wildcat_cry at -6.6 -- a crest factor of 7.4 dB against 17.4.
// The cat is a shriek; its transient IS the sound. At +6 dB the cat would
// deliver a -0.7 dBTP peak, which is inside this lane's -3 dBTP margin and
// close enough to full scale to clip against whatever else is playing.
//
// So 0.68 is the largest trim that keeps the cat at or under -3 dBTP with the
// context gain above (3.50 x 0.68 x 0.63 = 1.50 -> -3.1 dBTP). It buys the cat
// +3.7 dB where the wolf got +6.0.
//
// THE TWO ANIMALS ARE THEREFORE NO LONGER LOUDNESS-MATCHED, deliberately. That
// is affordable HERE and would not be elsewhere: the pumps are 42 km apart and
// no listener can hear both, so this is not a set that must agree with itself
// the way the footstep set must (see the manifest's step_ rows). If they are
// ever heard together, this decision is wrong and should be revisited.
//
// TO GIVE THE CAT THE OTHER 2.3 dB you must RE-MASTER, not re-trim -- and the
// manifest forbids the obvious way to do it. Limiting the shriek to lower its
// crest is exactly what its row refuses ("a cry IS its crest"), on the same
// grounds the black-stope blast refuses it. A quieter-peaked RECORDING is the
// only honest route. Until then this number is a wall, and moving it up simply
// makes the cat clip.
inline constexpr double kWolfCryGain = 1.00;
inline constexpr double kWildcatCryGain = 0.68;

inline double cry_asset_gain(PumpCry which) {
    return which == PumpCry::kWolfSudbury ? kWolfCryGain : kWildcatCryGain;
}

// ---------------------------------------------------------------------------
// DISTANCE FALL-OFF.
// ---------------------------------------------------------------------------
//
// The cry is a one-shot: raylib fixes its volume when it starts, so this is
// evaluated once, at the trigger, against the distance you were at. That is the
// right model anyway -- a howl that faded as you flew away would be a howl
// attached to your camera.
//
// NOT a 1/r law, and not a fade to zero at the edge. Both would be wrong here:
// the sound is a distant animal even at 10 m (you never see it; it is off in
// the bush), and a cry that arrives at zero volume the moment it becomes
// possible is a cry nobody hears, which defeats the arrival cue entirely. So
// the edge keeps a real floor and the near field is only moderately louder.
// kCryEdgeFloor is the fraction of full level a cry retains at maximum range.
//
// RAISED 0.42 -> 0.55, 2026-08-24, and this is the third face of the same
// complaint. The edge is where you MEET the animal -- an arrival cry fires
// kCryArriveDelay after you cross in, which is by construction near the rim,
// so the edge level is the one Chad actually hears first and it was the
// quietest number in the file. Widening the air range to 5 km would otherwise
// have made that worse, not better: a bigger circle means more of it is rim.
inline constexpr double kCryEdgeFloor = 0.55;

// The ceiling the result is clamped to. 1.35, not 1.0: see the trims above for
// why unity is not the real limit here (raylib does not clamp; both assets are
// mastered to -24 LUFS with 6.6 dB of true-peak headroom on the WORSE of the
// two). It exists so a future ear-ruling that pushes a trim cannot walk the
// product somewhere the measured headroom no longer covers -- the guard is
// against a typo, not against the design.
// THE CEILING IS NOW PER-ANIMAL, because a ceiling is an ASSET property -- it
// is that asset's true peak and nothing else -- and the two assets are 10 dB
// apart on exactly that. One shared number could only ever be right for one of
// them: set for the cat it silently caps the wolf 10 dB below what it can do,
// set for the wolf it stops guarding the cat at all (which is what a single
// 2.00 became the moment kWildcatCryGain became the thing holding the cat
// down).
//
// Each is the product that lands its asset at -3.0 dBTP after
// kGameplayBusGain, rounded down:
//   wolf    -16.6 dBTP -> 4.00   (the trims use 3.50; a real guard, not a wall)
//   wildcat  -6.6 dBTP -> 2.40   (the trims use 2.38; deliberately near it)
// Still typo guards. The design limits are the trims.
inline constexpr double kCryCeilingWolf = 4.00;
inline constexpr double kCryCeilingWildcat = 2.40;

inline double cry_gain_ceiling(PumpCry which) {
    return which == PumpCry::kWolfSudbury ? kCryCeilingWolf
                                          : kCryCeilingWildcat;
}

// Level for a cry triggered at `dist_m`, in `ctx`, for `which` animal.
// Bounded to [0, cry_gain_ceiling(which)] and never above the context gain.
inline double cry_gain(PumpCry which, double dist_m, CryContext ctx) {
    const double r = cry_range_m(ctx);
    const double d = std::clamp(dist_m, 0.0, r);
    const double near_frac = 1.0 - d / r;  // 1 at the pump, 0 at the edge
    const double shape = kCryEdgeFloor + (1.0 - kCryEdgeFloor) * near_frac;
    const double base =
        (ctx == CryContext::kFlying) ? kCryFlyingGain : kCryGroundGain;
    return std::clamp(base * shape * cry_asset_gain(which), 0.0,
                      cry_gain_ceiling(which));
}

// ---------------------------------------------------------------------------
// THE SCHEDULER. One per animal; the app holds an array of kPumpCryCount.
// ---------------------------------------------------------------------------
struct CryScheduler {
    // Time held INSIDE the range since the last cry (or since arriving).
    double in_timer = 0.0;
    // Time held OUTSIDE the range since leaving. Only meaningful while out.
    double out_timer = kCryReArmS;  // start armed: a fresh session can greet
    bool inside = false;
    bool greeted = false;  // has the arrival cry fired for THIS visit
    int gap_index = 0;

    // Advance one frame. Returns true on the single tick this animal should
    // cry. `within` is in_cry_range(dist, ctx) as the app measured it. Named
    // `within` and not the obvious `near` on purpose: <windows.h> defines
    // `near` and `far` as legacy macros, this header is pulled into a
    // translation unit that includes raylib, and a build that ever reaches
    // them would fail here for a reason that reads like nonsense.
    //
    // The context is re-read every tick rather than frozen at the last cry --
    // the same red-team point town_ambience.h records. A rider who parks at the
    // pump for a minute and then takes off would otherwise serve out a short
    // GROUND gap in the air and get a cry almost immediately, which is not what
    // "fly over the area" asks for.
    bool update(bool within, CryContext ctx, double dt) {
        if (!(dt > 0.0)) return false;

        if (!within) {
            // Leaving does not fire anything and does not bank time toward the
            // next cry -- unlike the train, this animal is reacting to you, and
            // there is no you here. It only counts down the re-arm.
            if (inside) {
                inside = false;
                in_timer = 0.0;
                out_timer = 0.0;
            } else {
                out_timer += dt;
            }
            // A departure long enough to count re-arms the greeting. Held (not
            // wrapped) so a very long absence cannot overflow the accumulator
            // in a session left running.
            if (out_timer >= kCryReArmS) {
                out_timer = kCryReArmS;
                greeted = false;
            }
            return false;
        }

        // Inside. An entry that happened before the re-arm hold expired keeps
        // `greeted` true, so it resumes the loiter cadence instead of greeting
        // you a second time -- that is the boundary-dither case.
        if (!inside) {
            inside = true;
            in_timer = 0.0;
        }
        in_timer += dt;
        if (in_timer < gap_now(ctx)) return false;

        in_timer = 0.0;
        if (!greeted) {
            // The arrival cry. It does NOT consume a gap-table slot: the table
            // is the loiter cadence, and burning its first entry on the
            // greeting would mean the first two cries of every visit came from
            // the same place in the table.
            greeted = true;
        } else {
            gap_index = (gap_index + 1) % kCryGapCount;
        }
        return true;
    }

    // The wait currently in force, for this context: the arrival grace if this
    // visit has not been greeted yet, otherwise the loiter gap.
    double gap_now(CryContext ctx) const {
        if (!greeted) return kCryArriveDelay;
        return (ctx == CryContext::kFlying) ? kCryGapsFlying[gap_index]
                                            : kCryGapsGround[gap_index];
    }
};

}  // namespace render
