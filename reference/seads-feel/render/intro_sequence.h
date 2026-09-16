#pragma once

#include <algorithm>
#include <cmath>

#include "render/mix_levels.h"  // kLoadMusicGain -- THE gameplay/loading balance

// The SEADS intro sequence -- the timed soundscape that opens the game.
//
// ⭐ THE SHAPE IS TITLE FIRST. Chad's written spec
// (D:/audio_tracks/readme_audio.txt): the main music "will be LOUDER IN THE
// LOADING PAGE and quieter during the gameplay it will run on a loop"; and
// (D:/audio_tracks/sound_effects/readme.txt) "Resigned to fate for AFTER title
// screen and black screen to entry this voice is the beginning of the game,
// then a telephone ring ... then the phone_operator sound".
//
//     TITLE  "SCARCE SKIES"   music LOUD, alone   <- the LOADING page;
//                                                    the world builds behind it
//     BLACK                   music fades out, wind rises
//     0:00   wind alone
//     0:08   the answering machine   (resigned_to_fate, 29.37 s)
//     0:37   the phone ringing       (3 x 4.50 s)
//     0:50   the operator            (11.63 s)
//     ->     gameplay, music returns QUIETER
//
// An earlier build had this exactly backwards -- title card at the END, music
// held off throughout -- because it was assembled from chat answers while his
// two readmes were unreadable on disk. They are the authority; this is them.
//
// He ruled the voice order explicitly, choosing it over the conventional
// ring-then-machine reading, and the readme independently agrees:
//
//     wind bed alone  ->  answering machine  ->  ringing  ->  operator
//
// Every cue time below is derived from the MEASURED length of the mastered
// asset it follows, not from a guess, so the sequence has no dead air and no
// overlap. Re-cut an asset and these must be re-derived -- the unit test pins
// the arithmetic so a silent drift fails the gate rather than the ear.
//
// Cosmetic, render-side only -- a pure function of elapsed splash time.
// PURE: no raylib, no audio device, no window. app/main.cpp owns the playback
// and merely asks this header what should be happening at time t.
// Firewall: only <algorithm>, <cmath>, render/mix_levels.h. No raylib, no
// <random>, no wall-clock, no allocation.

namespace render {

// ---------------------------------------------------------------------------
// Measured asset lengths [s] -- from build/soundbank_report.txt, the report
// emitted by D:/audio_tracks/tools/build_soundbank.sh. These are the lengths
// AFTER cutting and loop-folding, i.e. what actually ships.
// ---------------------------------------------------------------------------
inline constexpr double kIntroMachineDur = 29.37;   // intro_machine.wav
inline constexpr double kIntroRingCycleDur = 4.50;  // intro_ring_cycle.wav
inline constexpr double kIntroOperatorDur = 11.63;  // intro_operator.wav

// The source phone take rang on a metronomic 4.5 s cadence (2.0 s of ring, then
// 2.5 s of line silence, three times over). We ship ONE cycle and re-trigger it,
// so the ring count is a dial rather than a property of the recording.
inline constexpr int kIntroRingCount = 3;

// ---------------------------------------------------------------------------
// The timeline [s].
// ---------------------------------------------------------------------------

// Wind alone before anything happens. Long enough to establish that nobody is
// coming, which is the whole point of opening on it.
inline constexpr double kIntroWindAlone = 8.00;
inline constexpr double kIntroWindFadeIn = 3.00;

inline constexpr double kIntroMachineAt = kIntroWindAlone;
inline constexpr double kIntroMachineEnd = kIntroMachineAt + kIntroMachineDur;

// The ring opens as the machine's message runs out. The 0.03 s nudge just lands
// the cue on a round number for the ledger; it is inaudible.
inline constexpr double kIntroRingAt = 37.40;

// Three full cycles, so the last one ends on its own 2.5 s of line silence --
// the line going dead is the beat before the operator speaks.
inline constexpr double kIntroRingEnd =
    kIntroRingAt + kIntroRingCount * kIntroRingCycleDur;

inline constexpr double kIntroOperatorAt = kIntroRingEnd;  // 50.90
inline constexpr double kIntroOperatorEnd = kIntroOperatorAt + kIntroOperatorDur;

// The operator hangs up, one beat of wind alone, and you are flying. No title
// card here any more -- it opened the game, it does not close the intro.
inline constexpr double kIntroTail = 2.00;
inline constexpr double kIntroWindFadeOut = 2.00;
inline constexpr double kIntroEnd = kIntroOperatorEnd + kIntroTail;  // 64.53

static_assert(kIntroWindFadeOut <= kIntroTail,
              "the wind fade-out would start before the operator stops "
              "speaking");

// ---------------------------------------------------------------------------
// THE LOADING PAGE -- the title screen the world loads behind.
//
// It has no fixed length: it lasts exactly as long as the world takes to
// build, which is the whole point of putting it there. What IS fixed is the
// fade-in (so the title arrives rather than pops) and the MINIMUM hold, so a
// fast machine still gets a title screen instead of a flash.
// ---------------------------------------------------------------------------

inline constexpr double kLoadTitleFadeIn = 1.60;
inline constexpr double kLoadMinHold = 5.00;

static_assert(kLoadMinHold > kLoadTitleFadeIn,
              "the loading page could end before its own title finished "
              "fading up");

// Music on the loading page lives in render/mix_levels.h as kLoadMusicGain,
// next to the gameplay levels it is supposed to be LOUDER than -- which is the
// only property that matters about it and could not be seen while the two ends
// were defined in different files. Chad ruled the gap open by trimming the
// gameplay end ("trim wind / engine further"); mix_levels.h holds the dial and
// static_asserts that the gap stays audible.

// ---------------------------------------------------------------------------
// Levels.
// ---------------------------------------------------------------------------

// Music across the phone sequence. It arrives from the loading page still LOUD
// and fades to nothing over the black -- "BLACK, music fades out, wind rises"
// -- and then stays out from under the voices. Chad, on the phone sequence
// specifically: "keep it off or turned right down for the splash screen". A bed
// under an answering machine and an operator only fights the words.
inline constexpr double kIntroMusicFadeOut = 3.00;

static_assert(kIntroMusicFadeOut <= kIntroWindAlone,
              "the music would still be fading when the first voice speaks");

// How long the bed takes to swell back once gameplay starts. Without this the
// music would snap from silence to full at the first flight frame, which reads
// as a bug rather than as a return.
inline constexpr double kIntroMusicReturn = 4.00;

inline constexpr double kIntroWindGain = 0.85;

// The wind ducks under the two voices so the words stay intelligible without
// having to master them louder. 0.45 = -7 dB: the storm never leaves, it just
// stops competing. This is the "wind ducks under" beat in the sequence Chad
// approved.
inline constexpr double kIntroWindDuckFloor = 0.45;
inline constexpr double kIntroWindDuckSlew = 0.60;  // [s] in and out

// ---------------------------------------------------------------------------
// Cues -- what app/main.cpp must actually start playing, and when.
//
// A flat table rather than branches: main.cpp keeps one cursor, fires every cue
// whose time has passed, and cannot fire one twice or skip one on a hitched
// frame. Times must be non-decreasing; the unit test pins that.
// ---------------------------------------------------------------------------
// The title card. Chad, 2026-08-17: "Interim name - Scarce Skies - Last Call".
// INTERIM by his own word -- the shortlist is Game_name_candidates.txt in the
// sound repo, and this is a placeholder that happens to be a real name rather
// than a bracketed stub. One constant, so renaming is one edit.
inline constexpr const char* kGameTitle = "SCARCE SKIES";
inline constexpr const char* kGameSubtitle = "L A S T   C A L L";

// How far past its moment a cue may still fire. One frame at 30 fps is 33 ms,
// so 1 s is generous for normal running and still suppresses a cue that a
// multi-second stall (a window drag, a minimize) jumped clean over -- firing
// several stale one-shots at once is worse than the silence.
inline constexpr double kIntroCueStale = 1.00;

enum class IntroCue { kMachine = 0, kRing = 1, kOperator = 2 };

struct IntroCueEntry {
    double t;
    IntroCue cue;
};

inline constexpr int kIntroCueCount = 2 + kIntroRingCount;

inline constexpr IntroCueEntry kIntroCues[kIntroCueCount] = {
    {kIntroMachineAt, IntroCue::kMachine},
    {kIntroRingAt + 0.0 * kIntroRingCycleDur, IntroCue::kRing},
    {kIntroRingAt + 1.0 * kIntroRingCycleDur, IntroCue::kRing},
    {kIntroRingAt + 2.0 * kIntroRingCycleDur, IntroCue::kRing},
    {kIntroOperatorAt, IntroCue::kOperator},
};

// ---------------------------------------------------------------------------
// Pure mappings.
// ---------------------------------------------------------------------------

// Ramp from 0 to 1 across [a, b]; flat outside. Continuous, monotone.
inline double intro_ramp(double t, double a, double b) {
    if (!(b > a)) return t >= b ? 1.0 : 0.0;
    return std::clamp((t - a) / (b - a), 0.0, 1.0);
}

// True while either voice is speaking -- the windows the wind ducks under.
inline bool intro_voice_active(double t) {
    return (t >= kIntroMachineAt && t < kIntroMachineEnd) ||
           (t >= kIntroOperatorAt && t < kIntroOperatorEnd);
}

// Wind bed volume at splash time t. Rises from silence as the music leaves,
// ducks under each voice, and fades out on the last beat. Always in [0, 1].
inline double intro_wind_volume(double t) {
    if (!(t > 0.0)) return 0.0;  // also catches NaN
    const double up = intro_ramp(t, 0.0, kIntroWindFadeIn);
    const double down = 1.0 - intro_ramp(t, kIntroEnd - kIntroWindFadeOut, kIntroEnd);

    // Duck envelope, slewed by hand rather than by a filter so this stays a
    // pure function of t (no state, no frame history, testable at any instant).
    double duck = 1.0;
    const double s = kIntroWindDuckSlew;
    const double f = kIntroWindDuckFloor;
    if (intro_voice_active(t)) {
        // How far into the current voice window are we?
        const double in = (t >= kIntroOperatorAt) ? (t - kIntroOperatorAt)
                                                  : (t - kIntroMachineAt);
        duck = 1.0 - (1.0 - f) * intro_ramp(in, 0.0, s);
    } else {
        // Recovering after a voice window closes.
        double since = -1.0;
        if (t >= kIntroOperatorEnd) since = t - kIntroOperatorEnd;
        else if (t >= kIntroMachineEnd) since = t - kIntroMachineEnd;
        if (since >= 0.0 && since < s) {
            duck = f + (1.0 - f) * intro_ramp(since, 0.0, s);
        }
    }
    return std::clamp(kIntroWindGain * up * down * duck, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// The title card -- now the FRONT of the sequence, not the back.
// ---------------------------------------------------------------------------

// Loading-page title opacity, 0..1, against time since the page first drew.
// Fades up and stays up for as long as the load takes.
inline double load_title_alpha(double t) {
    return intro_ramp(t, 0.0, kLoadTitleFadeIn);
}

// True once the loading page has served its minimum: the load may finish in
// under a second on a warm cache, and a title that flashes is worse than none.
inline bool load_page_done(double t) { return t >= kLoadMinHold; }

// The handoff: the title dissolves into the black the phone call happens in.
inline constexpr double kIntroTitleFadeOut = 1.50;

static_assert(kIntroTitleFadeOut < kIntroWindAlone,
              "the title would still be on screen when the first voice speaks");

// Title opacity during the phone sequence: full at t = 0 (it arrives still lit
// from the loading page) and gone by kIntroTitleFadeOut.
inline double intro_title_alpha(double t) {
    return 1.0 - intro_ramp(t, 0.0, kIntroTitleFadeOut);
}

// Music level during the phone sequence: arrives LOUD off the loading page,
// fades out under the rising wind, then stays out.
inline double intro_music_gain(double t) {
    return kLoadMusicGain * (1.0 - intro_ramp(t, 0.0, kIntroMusicFadeOut));
}

// Gameplay music swell, against time since the intro ended. Multiplies whatever
// the MusicDirector asks for, so the bed returns rather than snapping on.
inline double intro_music_return(double t) {
    return intro_ramp(t, 0.0, kIntroMusicReturn);
}

inline bool intro_finished(double t) { return t >= kIntroEnd; }

}  // namespace render
