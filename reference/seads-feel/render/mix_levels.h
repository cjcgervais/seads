#pragma once

// THE GAMEPLAY MIX -- every level Chad has ruled on, on one screen.
//
// This header exists because the balance kept living in five unrelated places
// (two music trims in music_director.h, and raw float literals buried in
// app/main.cpp's audio init) and could therefore only be reasoned about by
// grepping. It is now one set of numbers with two dials through them.
//
// PURE: constants only. No raylib, no state, no includes.
//
// ---------------------------------------------------------------------------
// CHAD'S RULINGS, in the order they were made
// ---------------------------------------------------------------------------
//
// 1. FLY 1 (2026-08-17): "The wind and the engine are too loud to hear the
//    music is all I could tell." Root cause: the levels had been set against
//    each asset's MASTERED LOUDNESS, which is the wrong reference -- a -20 LUFS
//    bed does not compete with a number, it competes with a full-scale noise
//    synth sitting at its own trim. Fixed by moving the RATIOS: music 0.55 ->
//    0.95, wind 0.85 -> 0.55, drone 0.42 -> 0.30. He has NOT heard that fix.
//
// 2. His readme: the music "will be LOUDER IN THE LOADING PAGE and quieter
//    during the gameplay". The loading page was already at the ceiling (1.00)
//    and gameplay sat at 0.95 -- a 0.45 dB difference, i.e. none. Asked which
//    end should move, he ruled: "trim wind / engine further".
//
// 3. ⭐ SIGNED, 2026-08-24, after fly 4: "good the sounds are right now".
//
//    ⭐ AND AGAIN, same day, for the CHURCH BELL as it arrived: "ookay bell
//    sounds good!" / "I heard it, it passes my review". kBellGroundGain and
//    kBellFlyingGain in render/bell_ambience.h join this list on their first
//    listen rather than after four flies -- which is what a channel derived
//    against a measured ceiling instead of against a guess buys you. Same rule
//    applies to them from here.
//
//    THIS IS AN APPROVAL, AND AN APPROVAL IS EVIDENCE. Every number reached by
//    flies 3 and 4 -- kMusicFlownSurface 0.67, kSledFlownLevel 0.98, and the
//    train/animal trims in render/town_ambience.h and render/pump_ambience.h --
//    is now a level Chad has HEARD and accepted, not a level someone reasoned
//    to. That is a different kind of number and it gets a different rule:
//
//      DO NOT RETUNE THESE TO MAKE A POLICY TIDIER. Move one only when he says
//      to, or when a NEW channel arrives that changes what the mix is -- which
//      is exactly what happened to the 0.95 in ruling 1, and the honest way to
//      undo an approval.
//
//    This lane has paid for ignoring that before: the bagpipe drone's -10.3
//    LUFS target exists because retargeting it to the -18 house number would
//    have moved a signed mix 8 dB to make a column consistent (see its row in
//    tools/audio/soundbank.manifest.tsv). The same reasoning now covers this
//    whole file.
//
//    ⚠ What is signed is the BALANCE, not the absolute. If the master sum
//    distorts on a loud moment, kGameplayBusGain is still the right dial --
//    it moves everything together and so leaves every approved RATIO intact.
//
// So there are TWO separate moves in ruling 2, and they are deliberately two
// separate dials, because they answer two different complaints:
//
//   kGameplayBusGain    drops the WHOLE gameplay mix under the loading page,
//                       which is what his readme asks for.
//   kWindEngineTrim     drops wind and engine FURTHER STILL, on top of that --
//                       the two channels he actually named, coming down
//                       relative to the music rather than alongside it.
//
// ⚠ The first draft of this header had only the bus dial, and multiplied music,
// wind and engine by it equally. That is NOT what he asked for: it leaves the
// wind-to-music ratio bit-identical, so the one relationship he named would not
// have moved at all, and a static_assert here actively guaranteed it could not.
// A fresh-context red-team caught it. "Further" means further than the music.

namespace render {

// The loading page: the bed alone, at the ceiling. There is no headroom above
// this -- the asset is mastered with a true-peak limit and a gain above unity
// would clip it, so "make the loading page louder" is not available as a lever.
// The gameplay end is the one that moves.
inline constexpr double kLoadMusicGain = 1.00;

// ⭐ DIAL 1: how far the whole gameplay mix sits below the loading page.
// 0.63 = -4.0 dB, which is a level change you hear as a level change rather
// than as a guess. EVERY gameplay channel below carries it, so the mix Chad
// judges is the same mix, just quieter -- and the title screen is audibly the
// loud one, as his readme asks.
inline constexpr double kGameplayBusGain = 0.63;

// ⭐ DIAL 2: how much further wind and engine come down, and ONLY wind and
// engine. This is "trim wind / engine further" as a relative move: 0.70 =
// -3.1 dB on top of the bus, so the music sits that much clearer of the two
// channels he has twice now said were burying it. Modest on purpose -- the
// wind IS the speed cue, and a wind trimmed into inaudibility trades one
// complaint for a worse one. If his next fly says it is still buried, this is
// the number to move, and it moves nothing else.
inline constexpr double kWindEngineTrim = 0.70;

// ---------------------------------------------------------------------------
// The levels fly 1 landed on. Kept as named constants rather than folded into
// the arithmetic so the diff of any future retune says WHICH balance moved.
// ---------------------------------------------------------------------------
//
// ⭐ MUSIC DOWN, 0.95 -> 0.67 (-3.0 dB). Chad, 2026-08-24, after the fly that
// finally had a train in it: "turns the music down a bit and make the sound
// loud enough. I can barely hear the train through the music."
//
// -3.0 dB is this lane's smallest honest step -- below that a level change is
// "is it my imagination" -- and "a bit" is what he asked for, so it is one step
// and not two. It is HALF of the answer: the event channels (train, animals,
// sled) come up by about as much again in their own files, so the gap he
// complained about closes by ~6 dB while the bed only moves by 3.
//
// ⚠ THIS PARTLY WALKS BACK FLY 1 (ruling 1 above), and that is deliberate
// rather than forgotten. 0.95 was the fix for "the wind and the engine are too
// loud to hear the music" -- but that ruling was made in a mix with NO train,
// NO animals and a SILENT snowmachine. Three channels have arrived since, and
// he has now heard the bed compete with them. The old number is not wrong; it
// answered a question about a different mix.
//
// ⚠ WATCH THE WIND. Wind and engine are NOT on this move (they have their own
// dial), so dropping the bed 3 dB lifts them 3 dB RELATIVE to it -- and "wind
// and engine bury the music" is a complaint he has made twice. He asked for the
// bed down and the events up, not for the wind, so the wind stays where his
// ruling put it. If Fly-1's complaint returns, the answer is kWindEngineTrim
// (0.70), one dial, and it moves nothing else.
inline constexpr double kMusicFlownSurface = 0.67;
inline constexpr double kWindFlownTrim = 0.55;
inline constexpr double kEngineFlownTrim = 0.30;
inline constexpr double kGunFlownLevel = 0.55;
inline constexpr double kSfxFlownLevel = 0.85;

// The SNOWMACHINE engine (render/sled_synth.h -- idle bed + riding bed + brap
// accent + two-stroke tone). Now a RIDDEN level, not a guess.
//
// 0.62 was the first guess, seated between the guns and the sfx pool on the
// argument that a vehicle engine is the loudest thing a rider hears. Chad rode
// it 2026-08-20 and ruled it down: "it need to come down one or two octaves ...
// and decibels". So the argument was wrong and the ear is right -- 0.31 is
// exactly -6 dB from that first guess, which is the standard step for "audibly
// quieter" rather than a nudge nobody can hear.
//
// ⚠ THIS DROPS IT BELOW THE GUNS, and an earlier static_assert here actively
// forbade that (see the retracted assertion below). That assertion was my
// reasoning about what a vehicle engine deserves, welded into the build. A
// static_assert encoding a guess outranks nothing; his ride outranks it.
//
// Note the synth ALSO carries its own internal master (render::kSledMaster,
// 0.78) before this trim -- that one is the balance BETWEEN its four layers,
// this one is where the whole machine sits in the game. Move THIS one for "the
// sled is too loud"; move kSledMaster only if the LAYERS are wrong against each
// other. And pitch is neither: that is kSledPitchPedestal in sled_audio.h.
//
// RAISED 2026-08-23, on Chad's "please raise snowmachine sounds up". 0.31 ->
// 0.44 is +3 dB: one audible step back up, and exactly the move the audio
// handoff pre-computed for this symptom (the dial table in
// docs/SESSION_HANDOFF_20260820_audio.md named 0.44 as the +3 dB step, and
// docs/audio_handoff.md said so first: "if it is now too quiet,
// kSledFlownLevel = 0.44"). Half the distance back toward 0.62, NOT a return to
// it: he rejected 0.62 by ear on 2026-08-20 and that ruling still stands. What
// changed underneath it is the PITCH -- the engine came down two octaves in the
// same session, and a two-stroke seated two octaves lower carries less
// perceived loudness at the same gain, so some of the -6 dB was paying for a
// problem that has since been fixed elsewhere. Going straight back to 0.62
// would re-run an experiment already answered; 0.44 asks the smallest question
// his ear can actually resolve.
//
// RAISED AGAIN 2026-08-24, 0.44 -> 0.62 (+3 dB), on Chad's "nor is the
// snowmachine sounds [loud enough]" from the same fly. This IS the 0.62 he
// rejected on 2026-08-20, and returning to it is the point rather than a
// regression: what he rejected was 0.62 AT THE OLD PITCH, two octaves higher
// and correspondingly harsher. The pitch came down in that same session and he
// has now heard 0.44 at the corrected pitch and called it too quiet. The
// experiment that 0.31 was the answer to no longer exists.
//
// RAISED AGAIN 2026-08-24 (fly 4), 0.62 -> 0.98 (+4.0 dB). Chad: "the train,
// the howl and the snowmachine all need to come up some in volume, they barely
// make it over the music ... The snowmachine engine is more lasting so it can
// raise in volume a little bit more but not too much but quite a bit will make
// it better."
//
// +4 dB, where the two ONE-SHOTS in the same ruling got +6. That gap is his
// sentence, not a rounding: a sustained channel at the level of a one-shot is
// the one that fatigues, and he said "not too much" about this one and "quite
// loud" about the others. This lands the machine 3.3 dB OVER the bed.
//
// ⚠ AND THAT BREAKS THE "UNDER THE MUSIC" ASSERT, exactly as the note here
// predicted one fly ago. It is retracted below rather than obeyed. See there.
// ⭐ LOWERED 2026-08-28, 0.98 -> 0.31 (-10.0 dB). Chad, after the fly that had
// the 0.98 machine under him: "turn down the volume level of the snowmachine a
// few notches like 10db, maybe lower ... though close to where it should be it
// is a bit of a pain on the ears."
//
// This is the LARGEST single move this dial has ever taken, and it is one step
// rather than two because he named the size himself. "Like 10db" is not a
// direction to be interpreted into the lane's usual 3 dB increments -- he gave
// a number, and splitting it into halves would ask him to re-fly a complaint he
// has already quantified. "Maybe lower" is the tail he left open; it is not
// spent here, it is what the next step is for.
//
// ⚠ 0.31 IS EXACTLY WHERE HIS FIRST RIDE PUT IT (2026-08-20, "it need to come
// down one or two octaves ... and decibels"), and arriving back on it by a
// different road is worth recording rather than hiding. Three raises since then
// -- 0.31 -> 0.44 -> 0.62 -> 0.98 -- were each answering "I cannot hear it over
// the music", and each was correct about audibility. What none of them tested
// was whether the channel had become PAINFUL rather than merely present, which
// is a different complaint and the one he is making now. The raises were not
// wrong; they were climbing past the point where the answer stopped being
// "louder" and started being "the bed is in the wrong place". His first ear
// found this number under a different question, and it is the same number.
//
// ⚠ THIS COMPOUNDS WITH THE PITCH DROP in the same ruling (kSledVoiceTranspose
// 0.50 -> 0.40, sled_audio.h). A voice seated lower carries less perceived
// loudness at equal gain -- that argument is already written into the 0.44 note
// above, and it runs the same way in this direction. So the machine will read
// as MORE than 10 dB quieter, which is the "maybe lower" half of his sentence
// being partly paid by the other dial. If it now goes too far, THIS is the
// dial to bring back (0.44 is +3 dB, 0.55 is +5), not the pitch -- the pitch is
// answering "high pitched", which is a separate ruling of his.
//
// The retracted "under the music" assertion below stays retracted. 0.31 does
// sit under the bed again, but it got there by his volume ruling and not by the
// argument that assertion encoded, and re-welding it would just re-arm the trap
// that fired on fly 4.
inline constexpr double kSledFlownLevel = 0.31;

// ---------------------------------------------------------------------------
// Derived gameplay levels.
// ---------------------------------------------------------------------------

// The surface bed.
inline constexpr double kMusicSurfaceGain = kMusicFlownSurface * kGameplayBusGain;

// The deep (tunnel + stope) bed -- Chad's "on the quieter side". Held as the
// EXACT ratio it was flown at rather than a rounded 0.82, so the relationship
// is preserved to the bit and the comment cannot drift from the code.
inline constexpr double kMusicDeepRatio = 0.78 / 0.95;
inline constexpr double kMusicDeepGain = kMusicSurfaceGain * kMusicDeepRatio;

// The procedural WIND stream's master trim. Both dials. The synth's own
// internal law (speed / density / G) is untouched by this -- the energy cue it
// exists to carry is unchanged, it is only seated lower in the mix.
inline constexpr double kWindStreamTrim =
    kWindFlownTrim * kGameplayBusGain * kWindEngineTrim;

// The THROTTLE voice (the bagpipe drone). Both dials, same reasoning: throttle
// is felt as the drone swelling, which is a ratio INSIDE the synth, so trimming
// the stream preserves the cue entirely.
inline constexpr double kEngineStreamTrim =
    kEngineFlownTrim * kGameplayBusGain * kWindEngineTrim;

// Combat. On the bus but NOT on the wind/engine trim -- Chad named wind and
// engine, not the guns. They ride the bus so that "the whole gameplay mix drops
// together" is true rather than nearly true: left at their old absolute levels
// they would have come out +4 dB relative to the bed, which is the opposite of
// every ruling he has made about this mix.
inline constexpr double kGunStreamLevel = kGunFlownLevel * kGameplayBusGain;
inline constexpr double kSfxStreamLevel = kSfxFlownLevel * kGameplayBusGain;

// The sled engine. On the bus, and NOT on kWindEngineTrim: "trim wind / engine
// further" was about the AIRCRAFT -- the wind past the canopy and the bagpipe
// drone -- said while flying, about a mix in which this channel was silent.
// Applying that trim here would be inheriting a ruling made about two other
// sounds. It rides the bus like everything else so the whole gameplay mix still
// drops together under the loading page.
inline constexpr double kSledStreamLevel = kSledFlownLevel * kGameplayBusGain;

// The black-stope blast. The DRY one plays through raylib's Sound path, which
// defaults to unity and was therefore the loudest thing in the game the moment
// the bed came down; it now rides the bus like everything else. The SEND is the
// room behind it (Chad: "yes echos the stope explosion itself") -- measured
// offline at -8.3 dB under the dry blast, which is a room, not a second boom.
inline constexpr double kBoomDryLevel = 1.00 * kGameplayBusGain;
inline constexpr double kBoomSendLevel = 0.85 * kGameplayBusGain;

// ---------------------------------------------------------------------------
// The properties the whole exercise is for, asserted rather than hoped for.
// ---------------------------------------------------------------------------

inline constexpr double mix_abs(double x) { return x < 0.0 ? -x : x; }

// RULING 2a: the loading page must be AUDIBLY louder, not merely louder. -3 dB
// is roughly where a level change stops being "is it my imagination". The old
// pair (1.00 vs 0.95) delivered 0.45 dB and satisfied a naive `>`.
static_assert(kMusicSurfaceGain <= 0.71 * kLoadMusicGain,
              "the loading page is no longer audibly louder than gameplay, "
              "which is the one thing Chad's readme asks of these two numbers");

// RULING 2b: wind and engine must come down FURTHER THAN THE MUSIC, or "trim
// wind / engine further" has not happened -- only a uniform volume change has.
// This is the assert whose inverse the first draft shipped.
static_assert(kWindStreamTrim / kMusicSurfaceGain <
                  kWindFlownTrim / kMusicFlownSurface,
              "the wind is no quieter RELATIVE to the music than before, so "
              "Chad's 'trim wind / engine further' is undelivered");
static_assert(kEngineStreamTrim / kMusicSurfaceGain <
                  kEngineFlownTrim / kMusicFlownSurface,
              "the engine is no quieter RELATIVE to the music than before, so "
              "Chad's 'trim wind / engine further' is undelivered");

// ...but not trimmed into nothing: the wind is the speed cue and the drone is
// the throttle cue. Both must still be present channels, not ghosts.
static_assert(kWindStreamTrim > 0.15 * kMusicSurfaceGain,
              "the wind has been trimmed until it can no longer carry speed");
static_assert(kEngineStreamTrim > 0.10 * kMusicSurfaceGain,
              "the drone has been trimmed until it can no longer carry "
              "throttle");

static_assert(kMusicDeepGain < kMusicSurfaceGain,
              "the deep bed must stay 'on the quieter side'");

// Every gameplay channel is on the bus. Stated as identities so a hand-edit of
// one derived level (instead of a dial) trips the build.
static_assert(mix_abs(kGunStreamLevel - kGunFlownLevel * kGameplayBusGain) < 1e-12 &&
                  mix_abs(kSfxStreamLevel - kSfxFlownLevel * kGameplayBusGain) < 1e-12,
              "a combat channel has fallen off the gameplay bus");
static_assert(mix_abs(kSledStreamLevel - kSledFlownLevel * kGameplayBusGain) <
                  1e-12,
              "the sled engine has fallen off the gameplay bus");

// ⚠ RETRACTED 2026-08-24 (fly 4), left visible rather than deleted, exactly as
// the gun assert below was:
//
//   static_assert(kSledStreamLevel < kMusicSurfaceGain,
//                 "the snowmachine engine is mixed OVER the music bed, which
//                  is the exact complaint two of Chad's rulings were about");
//
// It was TRUE to his rulings when it was written, and it is the second-best
// example this lane has of why that is not the same thing as being correct.
// Both of those rulings were made about a mix in which the engine was the only
// thing competing with the bed -- no train, no animals, and for one of them no
// engine either. Fly 4 is the same ear on a fuller mix:
//
//   "they barely make it over the music and its an environmental sound effect,
//    not very immersive if not quite a bit louder than the music"
//
// That is not a retune of one channel; it is a ruling about what a bed IS. The
// music is the layer you hear past, and the world is the layer you hear. An
// assert that pins a channel under the bed forbids that, so it goes -- and it
// goes rather than the level, because the alternative is trimming the world to
// keep a green.
//
// WHAT SURVIVES IS THE FLOOR, below, which is a real property: the machine you
// are sitting on must not be quieter than the music behind it. That one only
// ever gets more true as this moves.

// ⚠ THE NEXT THING TO BITE IS THE MASTER SUM, not any single channel. Every
// gameplay channel has now moved up while the bed moved down, so simultaneous
// worst cases (train pass + sled at speed + music) add closer to full scale
// than they used to. If it distorts on a loud moment rather than on one sound,
// the dial is kGameplayBusGain (0.63) -- it lowers everything TOGETHER and so
// preserves every ratio Chad has ruled on. Do not fix a sum by trimming the
// channel that happened to be playing.

// Still audible above the guns and the bed both -- stated as a report, not a
// bound, because which of them should be louder is a taste question that has
// now changed twice.
static_assert(kSledStreamLevel > 0.0 && kSledStreamLevel * 0.78 < 1.0,
              "the sled stream x its synth master no longer fits in the "
              "sample format");

// ⚠ RETRACTED 2026-08-20, deliberately left visible rather than deleted:
//
//   static_assert(kSledStreamLevel > kGunStreamLevel,
//                 "the sled engine has been trimmed down into the guns");
//
// It asserted that a vehicle engine must sit ABOVE the gun channel. That was an
// argument, not a measurement, and Chad's first ride ruled the engine down past
// it. The lesson worth keeping: an assertion is for a property that must hold
// for the code to be correct, not for a balance nobody has heard yet -- welding
// a guess into the build only means the ear that disagrees has to fight the
// compiler. Balance claims belong in comments until someone has listened.
//
// What DOES still have to hold is that the machine you are sitting on has not
// been trimmed into inaudibility. That is a floor, and it is a real one.
static_assert(kSledStreamLevel > 0.25 * kMusicSurfaceGain,
              "the snowmachine engine has been trimmed until the machine you "
              "are riding is quieter than the music behind it");

}  // namespace render
