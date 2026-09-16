// Unit tests for the SEADS soundscape layer.
// Pure mapping tests only -- render/music_director.h (the MUSIC bus: which bed,
// how loud, and the blast duck), render/intro_sequence.h (the loading page and
// the intro cue timeline) and render/stope_reverb.h (the stope echo). None of
// them touches raylib, so all three are testable headlessly.
//
// What this file CANNOT test, by construction: that any of it is audible. The
// gate never runs seads.exe, so a silent stream ships green. Levels and timing
// are pinned here; the ear is the only judge of the mix.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render/intro_sequence.h"
#include "render/team_color.h"
#include "render/music_director.h"
#include "render/mix_levels.h"
#include "render/sample_voice.h"
#include "render/sled_audio.h"
#include "render/stope_reverb.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Approx;

// ---------------------------------------------------------------------------
// MUSIC bus -- the blast duck
// ---------------------------------------------------------------------------

TEST_CASE("music_duck_gain: no blast in flight leaves the music untouched") {
    REQUIRE(render::music_duck_gain(-1.0) == Approx(1.0));
    REQUIRE(render::music_duck_gain(0.0) == Approx(1.0));
}

TEST_CASE("music_duck_gain: retires to unity past the envelope life") {
    REQUIRE(render::music_duck_gain(render::kMusicDuckLife) == Approx(1.0));
    REQUIRE(render::music_duck_gain(render::kMusicDuckLife + 10.0) == Approx(1.0));
}

TEST_CASE("music_duck_gain: reaches the floor and holds there") {
    const double hold_mid = render::kMusicDuckAttack + 0.5 * render::kMusicDuckHold;
    REQUIRE(render::music_duck_gain(render::kMusicDuckAttack) ==
            Approx(render::kMusicDuckFloor));
    REQUIRE(render::music_duck_gain(hold_mid) == Approx(render::kMusicDuckFloor));
}

TEST_CASE("music_duck_gain: stays inside the floor and unity at all ages") {
    for (int i = 0; i <= 800; ++i) {
        const double age = -1.0 + 0.01 * i;
        const double g = render::music_duck_gain(age);
        REQUIRE(g <= 1.0 + 1e-12);
        REQUIRE(g >= render::kMusicDuckFloor - 1e-12);
    }
}

TEST_CASE("music_duck_gain: falls through the attack and recovers on release") {
    // STRICT comparisons. With <= / >= a constant function passes both loops,
    // which would let the duck silently stop ducking.
    double prev = render::music_duck_gain(1e-6);
    for (int i = 1; i <= 40; ++i) {
        const double g = render::music_duck_gain(render::kMusicDuckAttack * i / 40.0);
        REQUIRE(g < prev);
        prev = g;
    }
    const double r0 = render::kMusicDuckAttack + render::kMusicDuckHold;
    prev = render::music_duck_gain(r0);
    for (int i = 1; i <= 40; ++i) {
        const double g = render::music_duck_gain(r0 + render::kMusicDuckRelease * i / 40.0);
        REQUIRE(g > prev);
        prev = g;
    }
}

TEST_CASE("music_duck_gain: the duck outlives the blast it is hiding") {
    // Sized against the shipped asset, not by feel. A duck shorter than the
    // audible body of the blast releases the music back to full while the
    // rumble is still going -- the opposite of the occlusion Chad asked for.
    // sfx/black_stope_explosion.wav ships as a 13 s body (kStopeBoomLen).
    REQUIRE(render::kMusicDuckLife > 0.5 * render::kStopeBoomLen);
    // ...and must still finish inside the shortest gap, or two blasts' ducks
    // overlap and the bed never comes back up between them.
    REQUIRE(render::kMusicDuckLife < render::kStopeRumbleGaps[2]);
}

TEST_CASE("music_duck_gain: continuous at every breakpoint") {
    const double e = 1e-6;
    const double b1 = render::kMusicDuckAttack;
    const double b2 = render::kMusicDuckAttack + render::kMusicDuckHold;
    const double b3 = render::kMusicDuckLife;
    REQUIRE(render::music_duck_gain(b1 - e) == Approx(render::music_duck_gain(b1 + e)).margin(1e-3));
    REQUIRE(render::music_duck_gain(b2 - e) == Approx(render::music_duck_gain(b2 + e)).margin(1e-3));
    REQUIRE(render::music_duck_gain(b3 - e) == Approx(render::music_duck_gain(b3 + e)).margin(1e-3));
}

// ---------------------------------------------------------------------------
// MUSIC bus -- the surface/deep crossfade
// ---------------------------------------------------------------------------

TEST_CASE("music crossfade: equal power across the whole blend") {
    // The reason for cos/sin: a linear pair sags ~3 dB mid-fade, audibly, right
    // at the portal. Summed power must stay flat instead.
    for (int i = 0; i <= 100; ++i) {
        const double b = i / 100.0;
        const double s = render::music_surface_weight(b);
        const double d = render::music_deep_weight(b);
        REQUIRE(s * s + d * d == Approx(1.0).margin(1e-9));
    }
}

TEST_CASE("music crossfade: endpoints are pure surface and pure deep") {
    REQUIRE(render::music_surface_weight(0.0) == Approx(1.0));
    REQUIRE(render::music_deep_weight(0.0) == Approx(0.0).margin(1e-12));
    REQUIRE(render::music_surface_weight(1.0) == Approx(0.0).margin(1e-12));
    REQUIRE(render::music_deep_weight(1.0) == Approx(1.0));
}

TEST_CASE("music crossfade: out-of-range blend is clamped, never extrapolated") {
    REQUIRE(render::music_surface_weight(-5.0) == Approx(1.0));
    REQUIRE(render::music_deep_weight(5.0) == Approx(1.0));
}

TEST_CASE("MusicDirector: entering the tunnel net crosses over to the deep bed") {
    render::MusicDirector md;
    REQUIRE(md.blend == Approx(0.0));
    for (int i = 0; i < 600; ++i) md.update(true, 1.0 / 60.0);  // 10 s inside
    REQUIRE(md.blend > 0.99);
    REQUIRE(md.deep_volume() > md.surface_volume());
}

TEST_CASE("MusicDirector: leaving fades the deep bed back out") {
    render::MusicDirector md;
    md.blend = 1.0;
    for (int i = 0; i < 900; ++i) md.update(false, 1.0 / 60.0);  // 15 s outside
    REQUIRE(md.blend < 0.01);
    REQUIRE(md.surface_volume() > md.deep_volume());
}

TEST_CASE("MusicDirector: the exit fade is slower than the entry fade") {
    // Chad asked for the deep bed to FADE on the way out rather than snap, so
    // the asymmetry is intentional and pinned.
    REQUIRE(render::kMusicExitTau > render::kMusicEnterTau);

    render::MusicDirector in;
    for (int i = 0; i < 60; ++i) in.update(true, 1.0 / 60.0);  // 1 s entering
    render::MusicDirector out;
    out.blend = 1.0;
    for (int i = 0; i < 60; ++i) out.update(false, 1.0 / 60.0);  // 1 s leaving
    REQUIRE(in.blend > 1.0 - out.blend);
}

TEST_CASE("MusicDirector: the deep bed is mixed quieter than the surface bed") {
    // Chad: "make the ambient music on the quieter side".
    REQUIRE(render::kMusicDeepGain < render::kMusicSurfaceGain);
}

TEST_CASE("MusicDirector: a blast occludes the deep bed then lets it back up") {
    render::MusicDirector md;
    for (int i = 0; i < 600; ++i) md.update(true, 1.0 / 60.0);  // settle inside
    const double open = md.deep_volume();

    md.blast();
    for (int i = 0; i < 12; ++i) md.update(true, 1.0 / 60.0);  // 0.2 s in
    const double ducked = md.deep_volume();
    REQUIRE(ducked < open);
    REQUIRE(ducked == Approx(open * render::kMusicDuckFloor).margin(1e-6));

    for (int i = 0; i < 600; ++i) md.update(true, 1.0 / 60.0);  // let it retire
    REQUIRE(md.deep_volume() == Approx(open).margin(1e-6));
}

TEST_CASE("MusicDirector: a second blast restarts the duck rather than stacking") {
    render::MusicDirector md;
    md.blast();
    for (int i = 0; i < 60; ++i) md.update(true, 1.0 / 60.0);
    md.blast();
    REQUIRE(md.duck_age == Approx(0.0));
    REQUIRE(md.duck() == Approx(1.0));
}

TEST_CASE("MusicDirector: volumes stay bounded under a hitched frame") {
    render::MusicDirector md;
    md.blast();
    md.update(true, 5.0);  // a 5 s stall must not overshoot anything
    REQUIRE(md.blend >= 0.0);
    REQUIRE(md.blend <= 1.0);
    REQUIRE(md.surface_volume() >= 0.0);
    REQUIRE(md.deep_volume() >= 0.0);
    REQUIRE(md.surface_volume() <= render::kMusicSurfaceGain);
    REQUIRE(md.deep_volume() <= render::kMusicDeepGain);
}

// ---------------------------------------------------------------------------
// The stope rumble scheduler
// ---------------------------------------------------------------------------

TEST_CASE("StopeRumble: never fires while outside the tunnel net") {
    render::StopeRumble r;
    for (int i = 0; i < 60 * 600; ++i) {  // 10 minutes of surface flight
        REQUIRE(!r.update(false, 1.0 / 60.0));
    }
}

TEST_CASE("StopeRumble: holds a grace period before the first blast") {
    render::StopeRumble r;
    bool fired = false;
    const int grace = static_cast<int>(render::kStopeRumbleFirst * 60.0) - 2;
    for (int i = 0; i < grace; ++i) fired = fired || r.update(true, 1.0 / 60.0);
    REQUIRE(!fired);
    // and it does arrive shortly after.
    for (int i = 0; i < 10; ++i) fired = fired || r.update(true, 1.0 / 60.0);
    REQUIRE(fired);
}

TEST_CASE("StopeRumble: leaving the net rearms the whole cadence") {
    render::StopeRumble r;
    for (int i = 0; i < 60 * 60; ++i) r.update(true, 1.0 / 60.0);  // settle in
    r.update(false, 1.0 / 60.0);                                   // step out
    REQUIRE(r.timer == Approx(0.0));

    // Re-entering must wait the GRACE period specifically -- not merely
    // "not immediately". Asserting only a quiet first second would still pass
    // with the rearm deleted, because the leftover mid-cadence gap is longer
    // than a second either way. So: pin that nothing fires just BEFORE the
    // grace elapses, and that something fires just after.
    bool early = false;
    const int grace = static_cast<int>(render::kStopeRumbleFirst * 60.0) - 2;
    for (int i = 0; i < grace; ++i) early = early || r.update(true, 1.0 / 60.0);
    REQUIRE(!early);
    bool on_time = false;
    for (int i = 0; i < 10; ++i) on_time = on_time || r.update(true, 1.0 / 60.0);
    REQUIRE(on_time);
}

TEST_CASE("StopeRumble: keeps firing on a non-metronomic cadence") {
    render::StopeRumble r;
    int count = 0;
    double t = 0.0, last = 0.0;
    double gaps[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < 60 * 600 && count < 3; ++i) {
        t += 1.0 / 60.0;
        if (r.update(true, 1.0 / 60.0)) {
            if (count > 0) gaps[count - 1] = t - last;
            last = t;
            ++count;
        }
    }
    REQUIRE(count == 3);
    // Consecutive gaps must differ, or the rock sounds like a drum machine.
    REQUIRE(gaps[0] != Approx(gaps[1]));
}

// ---------------------------------------------------------------------------
// Intro sequence -- the splash timeline
// ---------------------------------------------------------------------------

TEST_CASE("intro cues: fire in order and never go backwards") {
    for (int i = 1; i < render::kIntroCueCount; ++i) {
        REQUIRE(render::kIntroCues[i].t >= render::kIntroCues[i - 1].t);
    }
}

TEST_CASE("intro cues: the table fires machine, then ringing, then operator") {
    // THE load-bearing pin in this file. Chad ruled this order explicitly, over
    // the conventional ring-then-machine reading. Every other test here asserts
    // on the kIntro*At TIMES; without asserting the .cue VALUES, swapping
    // kMachine and kOperator in the table inverts the sequence Chad chose and
    // still ships green, because app/main.cpp dispatches on .cue.
    REQUIRE(render::kIntroCues[0].cue == render::IntroCue::kMachine);
    for (int i = 1; i <= render::kIntroRingCount; ++i) {
        REQUIRE(render::kIntroCues[i].cue == render::IntroCue::kRing);
    }
    REQUIRE(render::kIntroCues[render::kIntroCueCount - 1].cue ==
            render::IntroCue::kOperator);

    // And exactly one of each voice, with the ruled number of rings -- so a
    // duplicated or dropped row cannot slip through the positional checks.
    int machines = 0, rings = 0, operators = 0;
    for (int i = 0; i < render::kIntroCueCount; ++i) {
        switch (render::kIntroCues[i].cue) {
            case render::IntroCue::kMachine: ++machines; break;
            case render::IntroCue::kRing: ++rings; break;
            case render::IntroCue::kOperator: ++operators; break;
        }
    }
    REQUIRE(machines == 1);
    REQUIRE(operators == 1);
    REQUIRE(rings == render::kIntroRingCount);
}

TEST_CASE("intro cues: the ring cycles tile without gap or overlap") {
    // Ring cues are entries 1..kIntroRingCount; each must sit exactly one cycle
    // after the last, or the phone stutters or double-rings.
    for (int i = 2; i <= render::kIntroRingCount; ++i) {
        REQUIRE(render::kIntroCues[i].t - render::kIntroCues[i - 1].t ==
                Approx(render::kIntroRingCycleDur));
    }
}

TEST_CASE("intro cues: the operator cue lands exactly as the ringing ends") {
    // Asserted on the TABLE, not on the constants -- kIntroOperatorAt is
    // DEFINED as kIntroRingEnd, so comparing those two is a tautology. What
    // actually matters is that the operator ROW carries that time.
    const render::IntroCueEntry& op =
        render::kIntroCues[render::kIntroCueCount - 1];
    const render::IntroCueEntry& last_ring =
        render::kIntroCues[render::kIntroRingCount];
    REQUIRE(op.t == Approx(last_ring.t + render::kIntroRingCycleDur));
}

TEST_CASE("intro timeline: the machine finishes before the phone starts ringing") {
    REQUIRE(render::kIntroRingAt >= render::kIntroMachineEnd);
}

TEST_CASE("intro timeline: runs wind, machine, ringing, operator, in that order") {
    // The order Chad ruled, over the conventional ring-then-machine reading.
    REQUIRE(render::kIntroMachineAt > 0.0);
    REQUIRE(render::kIntroRingAt > render::kIntroMachineAt);
    REQUIRE(render::kIntroOperatorAt > render::kIntroRingAt);
    REQUIRE(render::kIntroEnd > render::kIntroOperatorEnd);
}

TEST_CASE("intro wind: silent at the start and bounded everywhere") {
    REQUIRE(render::intro_wind_volume(-1.0) == Approx(0.0));
    REQUIRE(render::intro_wind_volume(0.0) == Approx(0.0));
    double lo = 2.0, hi = -1.0;
    for (int i = 0; i <= 7000; ++i) {
        const double v = render::intro_wind_volume(0.01 * i);
        REQUIRE(v >= 0.0);
        REQUIRE(v <= 1.0);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    // Non-vacuity: the bounds above are enforced by a std::clamp inside the
    // function, so on their own they only prove the clamp exists. Require the
    // curve to actually MOVE across the sequence.
    REQUIRE(hi > 0.5);
    REQUIRE(lo < 0.05);
    REQUIRE(hi - lo > 0.5);
}

TEST_CASE("intro wind: continuous across every breakpoint") {
    // The wind duck is the more fragile of the two envelopes -- its
    // breakpoints are DERIVED from measured asset lengths, so a re-cut moves
    // them. A step here is an audible click under the voices.
    const double e = 1e-4;
    const double bp[4] = {render::kIntroMachineAt, render::kIntroMachineEnd,
                          render::kIntroOperatorAt, render::kIntroOperatorEnd};
    for (double b : bp) {
        REQUIRE(render::intro_wind_volume(b - e) ==
                Approx(render::intro_wind_volume(b + e)).margin(2e-3));
    }
}

TEST_CASE("intro timeline: the voice windows are farther apart than the duck slew") {
    // The duck-recovery branch assumes the wind has finished returning before
    // the next voice opens. If the gap ever shrank below the slew, the recovery
    // would still be mid-climb when the voice branch takes over and the wind
    // would SWELL UP on the operator's first word before ducking again.
    REQUIRE(render::kIntroOperatorAt - render::kIntroMachineEnd >
            render::kIntroWindDuckSlew);
    // And the windows must not overlap at all, or the duck is attributed to
    // the wrong voice by the ternary in intro_wind_volume().
    REQUIRE(render::kIntroMachineEnd < render::kIntroOperatorAt);
}

TEST_CASE("intro wind: comes up alone before the first voice") {
    const double before = render::intro_wind_volume(render::kIntroMachineAt - 1.0);
    REQUIRE(before == Approx(render::kIntroWindGain).margin(1e-9));
}

TEST_CASE("intro wind: ducks under the machine and under the operator") {
    const double open = render::intro_wind_volume(render::kIntroMachineAt - 1.0);
    const double under_machine =
        render::intro_wind_volume(render::kIntroMachineAt + 2.0);
    const double under_operator =
        render::intro_wind_volume(render::kIntroOperatorAt + 2.0);
    REQUIRE(under_machine < open);
    REQUIRE(under_operator < open);
    REQUIRE(under_machine ==
            Approx(render::kIntroWindGain * render::kIntroWindDuckFloor).margin(1e-9));
}

TEST_CASE("intro wind: returns between the machine and the ringing") {
    // The gap must actually recover, or the duck reads as a stuck level.
    const double gap = render::intro_wind_volume(render::kIntroMachineEnd +
                                                 render::kIntroWindDuckSlew + 0.05);
    REQUIRE(gap == Approx(render::kIntroWindGain).margin(1e-9));
}

TEST_CASE("intro wind: fades out by the end of the sequence") {
    REQUIRE(render::intro_wind_volume(render::kIntroEnd) == Approx(0.0).margin(1e-9));
}

// ---------------------------------------------------------------------------
// ⭐ TITLE FIRST -- the shape Chad ruled and his readmes confirm.
//
// The previous build had the title card at the END of the sequence and the
// music off throughout. These pins exist so that shape cannot come back by
// accident: they assert the ORDER (title, then black, then voices) and the
// LEVELS (loud on the loading page, out under the voices, back for gameplay).
// ---------------------------------------------------------------------------

TEST_CASE("loading page: the title is lit before anything else happens") {
    // Chad's readme: the music is "louder in the LOADING PAGE and quieter
    // during the gameplay"; the voice comes "AFTER title screen and black
    // screen". So the title belongs to the load, not to the finale.
    REQUIRE(render::load_title_alpha(0.0) == Approx(0.0));
    REQUIRE(render::load_title_alpha(render::kLoadTitleFadeIn) == Approx(1.0));
    // ...and it is still fully lit for as long as the load runs, however long
    // that turns out to be.
    REQUIRE(render::load_title_alpha(300.0) == Approx(1.0));
    // Monotone: a title that dips mid-load reads as a flicker.
    double prev = -1.0;
    for (int i = 0; i <= 600; ++i) {
        const double a = render::load_title_alpha(0.02 * i);
        REQUIRE(a >= prev - 1e-12);
        prev = a;
    }
}

TEST_CASE("loading page: a fast load still gets a title, not a flash") {
    REQUIRE(!render::load_page_done(0.0));
    REQUIRE(!render::load_page_done(render::kLoadTitleFadeIn));
    REQUIRE(render::load_page_done(render::kLoadMinHold));
    // The hold must outlast the fade-in, or the page can end mid-dissolve.
    REQUIRE(render::kLoadMinHold > render::kLoadTitleFadeIn);
}

TEST_CASE("loading page: the music is at its loudest there") {
    // "louder in the loading page and quieter during the gameplay."
    REQUIRE(render::kLoadMusicGain == Approx(1.0));
    REQUIRE(render::kLoadMusicGain > render::kMusicSurfaceGain);
    REQUIRE(render::kLoadMusicGain > render::kMusicDeepGain);
}

TEST_CASE("intro: the title hands off to black before the first voice") {
    // "Resigned to fate for AFTER title screen and BLACK SCREEN to entry."
    // The card arrives still lit from the loading page and is gone by the time
    // the answering machine speaks -- otherwise there is no black screen.
    REQUIRE(render::intro_title_alpha(0.0) == Approx(1.0));
    REQUIRE(render::intro_title_alpha(render::kIntroTitleFadeOut) ==
            Approx(0.0));
    REQUIRE(render::intro_title_alpha(render::kIntroMachineAt) == Approx(0.0));
    // And it never comes back -- the old build's finale title.
    REQUIRE(render::intro_title_alpha(render::kIntroEnd) == Approx(0.0));
    REQUIRE(render::intro_title_alpha(render::kIntroOperatorEnd) ==
            Approx(0.0));
}

TEST_CASE("intro: the music fades out over the black, under the rising wind") {
    // "BLACK, music fades out, wind rises."
    REQUIRE(render::intro_music_gain(0.0) == Approx(render::kLoadMusicGain));
    REQUIRE(render::intro_music_gain(render::kIntroMusicFadeOut) ==
            Approx(0.0));
    // Gone before the answering machine speaks: a bed under the voices is
    // exactly what Chad ruled against for this stretch.
    REQUIRE(render::intro_music_gain(render::kIntroMachineAt) == Approx(0.0));
    REQUIRE(render::intro_music_gain(render::kIntroOperatorAt) == Approx(0.0));
    // Monotone down, and the wind is rising across the same window.
    double prev = 2.0;
    for (int i = 0; i <= 400; ++i) {
        const double g = render::intro_music_gain(0.02 * i);
        REQUIRE(g <= prev + 1e-12);
        prev = g;
    }
    REQUIRE(render::intro_wind_volume(render::kIntroMusicFadeOut) >
            render::intro_wind_volume(0.1));
}

TEST_CASE("intro: the gameplay bed returns rather than snapping on") {
    // "-> gameplay, music returns QUIETER."
    REQUIRE(render::intro_music_return(0.0) == Approx(0.0));
    REQUIRE(render::intro_music_return(render::kIntroMusicReturn) ==
            Approx(1.0));
    REQUIRE(render::intro_music_return(1e6) == Approx(1.0));
    // A swell, not a step: it must be genuinely partway up in the middle.
    const double mid = render::intro_music_return(0.5 * render::kIntroMusicReturn);
    REQUIRE(mid > 0.1);
    REQUIRE(mid < 0.9);
    // And what it settles at IS quieter than the loading page.
    REQUIRE(render::intro_music_return(1e6) * render::kMusicSurfaceGain <
            render::kLoadMusicGain);
}

TEST_CASE("intro: the splash paints the game's real complementary pair") {
    // Chad: "the splash screen needs to have our thematic orange / blue colors
    // that are opposite on the color wheel and complimentary.. Aleady chosen
    // and predefined in this game." They are predefined -- in render/team_color.h
    // as the faction palette -- so the splash must READ them, not retype them.
    //
    // This pins the property the splash depends on. The eye-plausible channel
    // reversal (0.12, 0.55, 1.00) sits 181.36 deg away: invisible on screen,
    // and it would make "precisely opposite" a lie.
    REQUIRE(render::is_exact_complement(render::kSlagOrange,
                                        render::kComplementBlue));
    REQUIRE(render::hue_separation_deg(
                render::rgb_to_hsv(render::kSlagOrange).h,
                render::rgb_to_hsv(render::kComplementBlue).h) ==
            Approx(180.0).margin(1e-9));
    // The near-miss must NOT satisfy it -- non-vacuity for the check above.
    REQUIRE(!render::is_exact_complement(render::kSlagOrange,
                                         glm::dvec3{0.12, 0.55, 1.00}));
}

TEST_CASE("intro: the title card carries a name") {
    // Interim per Chad, 2026-08-17: "Scarce Skies - Last Call". Pinned so the
    // splash can never regress to drawing an empty string.
    REQUIRE(render::kGameTitle != nullptr);
    REQUIRE(std::string(render::kGameTitle).size() > 0);
    REQUIRE(std::string(render::kGameSubtitle).size() > 0);
}

TEST_CASE("intro: the sequence reports finished exactly at its end") {
    REQUIRE(!render::intro_finished(render::kIntroEnd - 0.01));
    REQUIRE(render::intro_finished(render::kIntroEnd));
    // The operator must have finished speaking before the sequence ends, or
    // gameplay cuts him off mid-sentence.
    REQUIRE(render::kIntroEnd > render::kIntroOperatorEnd);
}

TEST_CASE("StopeRumble: fires about once every two minutes, per Chad's spec") {
    // D:/audio_tracks/sound_effects/readme.txt: "the ambient underground
    // explosion an rumble about once every two minutes." Pinned as a measured
    // mean, because the first version ran every ~26 s and nothing caught it.
    double sum = 0.0;
    for (int i = 0; i < render::kStopeRumbleGapCount; ++i)
        sum += render::kStopeRumbleGaps[i];
    const double mean = sum / render::kStopeRumbleGapCount;
    REQUIRE(mean > 90.0);
    REQUIRE(mean < 150.0);

    // And measured end to end, not just from the table.
    render::StopeRumble r;
    int fires = 0;
    for (int i = 0; i < 60 * 600; ++i)  // 10 minutes underground
        if (r.update(true, 1.0 / 60.0)) ++fires;
    REQUIRE(fires >= 3);
    REQUIRE(fires <= 7);
}

// ---------------------------------------------------------------------------
// THE STOPE ECHO -- render/stope_reverb.h
//
// Chad's written spec: "Make all sounds like guns echo when in the stope as
// well." Nothing in the codebase had any reverb at all before this, so these
// pins cover the two things that can silently go wrong: that it does nothing
// (a reverb with no tail is just a volume trim) and that it does something
// everywhere (a room that never switches off would follow him into open sky).
//
// What this file CANNOT test: whether it sounds like rock. That is the ear's.
// ---------------------------------------------------------------------------

namespace {

// Drive the wet mix to (near) full, the way a few seconds inside the net does.
render::StopeReverb make_immersed() {
    render::StopeReverb r;
    r.init(22050.0);
    for (int i = 0; i < 600; ++i) r.set_inside(true, 1.0 / 60.0);
    return r;
}

double abs_sum(const short* b, int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += std::abs(static_cast<double>(b[i]));
    return s;
}

}  // namespace

TEST_CASE("stope reverb: outside the net the audio path is bit-identical") {
    // The whole feature must be provably ABSENT in open sky -- this is the
    // property that makes it safe to put on the gun, engine and sfx channels.
    render::StopeReverb r;
    r.init(22050.0);
    r.set_inside(false, 1.0 / 60.0);
    REQUIRE(r.bypassed());
    REQUIRE(r.wet() == 0.0);  // EXACTLY zero, not merely small

    short buf[512], ref[512];
    for (int i = 0; i < 512; ++i) {
        buf[i] = static_cast<short>((i * 977) % 30000 - 15000);
        ref[i] = buf[i];
    }
    r.process(buf, 512);
    for (int i = 0; i < 512; ++i) REQUIRE(buf[i] == ref[i]);
}

TEST_CASE("stope reverb: an uninitialised room is a bypass, never a crash") {
    render::StopeReverb r;  // no init()
    REQUIRE(r.bypassed());
    short buf[16] = {1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7, 8, -8};
    r.process(buf, 16);
    REQUIRE(buf[0] == 1);
    REQUIRE(buf[15] == -8);
    r.process(nullptr, 16);  // must not dereference
    r.process(buf, 0);
    REQUIRE(buf[3] == -2);
}

TEST_CASE("stope reverb: the room arrives on entry and leaves more slowly") {
    render::StopeReverb settled;
    settled.init(22050.0);
    for (int i = 0; i < 600; ++i) settled.set_inside(true, 1.0 / 60.0);
    REQUIRE(settled.wet() > 0.99);
    REQUIRE(!settled.bypassed());

    render::StopeReverb entering;
    entering.init(22050.0);
    int entry_steps = 0;
    while (entering.wet() < 0.6 && entry_steps < 100000) {
        entering.set_inside(true, 1.0 / 60.0);
        ++entry_steps;
    }
    int exit_steps = 0;
    while (settled.wet() > 0.4 && exit_steps < 100000) {
        settled.set_inside(false, 1.0 / 60.0);
        ++exit_steps;
    }
    // Flying into a portal is an event; flying out is a release.
    REQUIRE(entry_steps > 0);
    REQUIRE(exit_steps > entry_steps);
    // And it does reach the hard OFF rather than asymptoting forever.
    for (int i = 0; i < 6000; ++i) settled.set_inside(false, 1.0 / 60.0);
    REQUIRE(settled.wet() == 0.0);
    REQUIRE(settled.bypassed());
}

TEST_CASE("stope reverb: a crack in the stope comes back off the walls") {
    // The mechanism itself: energy AFTER the input has stopped. A reverb that
    // only scaled the dry signal would pass every level check and fail this.
    render::StopeReverb r = make_immersed();

    short first[4096] = {};
    first[0] = 32767;  // one crack
    r.process(first, 4096);

    short tail[4096] = {};
    r.process(tail, 4096);  // silence in...

    REQUIRE(abs_sum(tail, 4096) > 0.0);  // ...sound out
    // Non-vacuity: it must be a real return, not a dither-level trickle.
    REQUIRE(abs_sum(tail, 4096) > 1000.0);
}

TEST_CASE("stope reverb: the first reflection is late by the room's size") {
    // The walls are not in your lap: nothing comes back until the predelay
    // plus the shortest comb path has run. Pinned against the network's own
    // derived lengths, so re-sizing the room moves the pin with it.
    render::StopeReverb r = make_immersed();

    const int n = 8192;
    std::vector<short> buf(static_cast<size_t>(n), static_cast<short>(0));
    buf[0] = 32767;
    r.process(buf.data(), n);

    int first_return = -1;
    for (int i = 1; i < n; ++i) {
        if (buf[static_cast<size_t>(i)] != 0) {
            first_return = i;
            break;
        }
    }
    REQUIRE(first_return > 0);

    int min_comb = r.comb_len_[0];
    for (int c = 1; c < render::kStopeCombCount; ++c)
        min_comb = std::min(min_comb, r.comb_len_[c]);
    REQUIRE(first_return == r.pre_len_ + min_comb);
    // And that is a real delay, not one sample: ~25 ms + ~43 ms at 22050 Hz.
    REQUIRE(first_return > 1000);
}

TEST_CASE("stope reverb: the tail decays instead of ringing forever") {
    render::StopeReverb r = make_immersed();

    const int n = 8192;
    std::vector<short> buf(static_cast<size_t>(n), static_cast<short>(0));
    buf[0] = 32767;
    r.process(buf.data(), n);

    std::vector<short> later(static_cast<size_t>(n), static_cast<short>(0));
    r.process(later.data(), n);  // ~0.37 s in

    std::vector<short> much_later(static_cast<size_t>(n), static_cast<short>(0));
    for (int k = 0; k < 8; ++k) {
        std::fill(much_later.begin(), much_later.end(), static_cast<short>(0));
        r.process(much_later.data(), n);  // ~3 s in
    }

    REQUIRE(abs_sum(later.data(), n) > 0.0);
    REQUIRE(abs_sum(much_later.data(), n) < abs_sum(later.data(), n));
    // Past RT60 the room is effectively quiet -- a feedback gain >= 1 would
    // hold or grow here instead.
    REQUIRE(abs_sum(much_later.data(), n) < 0.05 * abs_sum(later.data(), n));
}

TEST_CASE("stope reverb: sustained full scale does not run away") {
    // Worst case for a feedback network: seconds of full-scale input straight
    // into the combs. If the loop gain were >= 1 anywhere, the state would
    // grow without bound and the tail below would hold at the clamp rail
    // forever instead of decaying. (Range itself is not assertable on a short
    // -- every short is in range -- so the pin has to be the DECAY.)
    render::StopeReverb r = make_immersed();
    for (int pass = 0; pass < 100; ++pass) {  // ~4.6 s of it
        short buf[1024];
        for (int i = 0; i < 1024; ++i)
            buf[i] = (i % 2 == 0) ? static_cast<short>(32767)
                                  : static_cast<short>(-32767);
        r.process(buf, 1024);
    }
    // Now silence. A stable room empties; a runaway one keeps roaring.
    short quiet[4096] = {};
    r.process(quiet, 4096);
    const double first = abs_sum(quiet, 4096);
    for (int k = 0; k < 20; ++k) {
        std::fill(quiet, quiet + 4096, static_cast<short>(0));
        r.process(quiet, 4096);
    }
    const double after = abs_sum(quiet, 4096);
    REQUIRE(first > 0.0);  // non-vacuity: there WAS a tail to decay
    REQUIRE(after < 0.01 * first);
}

TEST_CASE("stope reverb: the comb delays are mutually incommensurate") {
    // Equal or near-harmonic comb delays sum into an audible pitch -- the
    // classic way a reverb ends up sounding like a metal pipe. Pinned as a
    // property of the table so a "tidier" set of round numbers fails here.
    render::StopeReverb r;
    r.init(22050.0);
    for (int a = 0; a < render::kStopeCombCount; ++a) {
        for (int b = a + 1; b < render::kStopeCombCount; ++b) {
            REQUIRE(r.comb_len_[a] != r.comb_len_[b]);
            const double ratio =
                static_cast<double>(std::max(r.comb_len_[a], r.comb_len_[b])) /
                static_cast<double>(std::min(r.comb_len_[a], r.comb_len_[b]));
            // Not within 2% of any small integer ratio.
            REQUIRE(std::abs(ratio - std::round(ratio)) > 0.02);
        }
    }
}

TEST_CASE("stope reverb: every feedback path is stable by derivation") {
    // The comb gains are DERIVED from RT60 and the line's own delay, so this
    // pins the derivation rather than four hand-typed numbers.
    render::StopeReverb r;
    r.init(22050.0);
    for (int c = 0; c < render::kStopeCombCount; ++c) {
        REQUIRE(r.comb_g_[c] > 0.0);
        REQUIRE(r.comb_g_[c] < 1.0);
        // A longer line loses more per round trip, for the same wall-clock
        // decay: g = 10^(-3 d / RT60) is monotone DOWN in d.
        if (c > 0 && r.comb_len_[c] > r.comb_len_[c - 1])
            REQUIRE(r.comb_g_[c] < r.comb_g_[c - 1]);
    }
    REQUIRE(render::kStopeAllpassG < 1.0);
    REQUIRE(render::kStopeAllpassG > 0.0);
}

TEST_CASE("stope reverb: a re-entry starts from an empty room") {
    // Leaving clears the network. Otherwise a tail recorded on the way out
    // would be waiting, seconds later, at the next portal.
    render::StopeReverb r = make_immersed();
    short loud[4096];
    for (int i = 0; i < 4096; ++i) loud[i] = 30000;
    r.process(loud, 4096);

    for (int i = 0; i < 6000; ++i) r.set_inside(false, 1.0 / 60.0);
    short drain[64] = {};
    r.process(drain, 64);  // the bypass pass that clears it
    REQUIRE(r.bypassed());

    for (int i = 0; i < 600; ++i) r.set_inside(true, 1.0 / 60.0);
    short quiet[4096] = {};
    r.process(quiet, 4096);
    REQUIRE(abs_sum(quiet, 4096) == 0.0);  // silence in, silence out
}

TEST_CASE("stope reverb: the dry signal survives -- it is a room, not a swap") {
    // Chad asked for the guns to ECHO, not to be replaced by their echo. The
    // crack must still arrive dry and first.
    //
    // Pinned as a VALUE against the constant, not as a loose bound: `> 0.5` is
    // satisfied by dry == 1.0, i.e. by deleting the duck entirely (a mutant
    // that survived the first version of this suite). At the impulse there is
    // no wet yet (the predelay has not elapsed), so the sample IS the dry gain.
    render::StopeReverb r = make_immersed();
    const int n = 4096;
    std::vector<short> buf(static_cast<size_t>(n), static_cast<short>(0));
    buf[0] = 32767;
    r.process(buf.data(), n);
    const double expect = (1.0 - render::kStopeDryDuck) * 32767.0;
    REQUIRE(static_cast<double>(buf[0]) == Approx(expect).margin(2.0));
    REQUIRE(render::kStopeDryDuck > 0.0);   // it does step aside...
    REQUIRE(render::kStopeDryDuck < 0.5);   // ...but the crack is still yours
}

// ---------------------------------------------------------------------------
// The four legs below exist because a fresh-context red-team MUTATED the reverb
// and these four mutants survived the whole suite:
//   dry duck deleted / both allpass diffusers deleted / damping deleted /
//   the wet RAMP deleted from the audio (wet_g pinned at kStopeWetMax).
// The last is the sharp one: with it the room snaps to full the instant you
// cross the portal and stays full through open sky, i.e. the entire "arrives on
// entry, leaves more slowly" feature is gone and the gate is still green,
// because wet_ was only ever pinned through its ACCESSOR.
// ---------------------------------------------------------------------------

TEST_CASE("stope reverb: the wet ramp reaches the AUDIO, not just the accessor") {
    // Half-immersed must sound half-immersed. Mutation killed: wet_g fixed at
    // kStopeWetMax (the ramp applied to nothing).
    render::StopeReverb half;
    half.init(22050.0);
    while (half.wet() < 0.5) half.set_inside(true, 1.0 / 240.0);
    const double w_half = half.wet();
    REQUIRE(w_half < 0.6);  // non-vacuity: we really are partway

    render::StopeReverb full = make_immersed();
    REQUIRE(full.wet() > 0.99);

    const int n = 8192;
    auto tail_energy = [n](render::StopeReverb& r) {
        std::vector<short> buf(static_cast<size_t>(n), static_cast<short>(0));
        buf[0] = 32767;
        r.process(buf.data(), n);
        // Skip the dry impulse itself; measure only what came off the walls.
        double s = 0.0;
        for (int i = 1; i < n; ++i)
            s += std::abs(static_cast<double>(buf[static_cast<size_t>(i)]));
        return s;
    };
    const double e_half = tail_energy(half);
    const double e_full = tail_energy(full);
    REQUIRE(e_full > 0.0);
    // Proportional to the wet mix, within the int16 quantisation of a quiet
    // tail. A fixed wet_g would make these two equal.
    REQUIRE(e_half == Approx(e_full * w_half).epsilon(0.10));
    REQUIRE(e_half < 0.75 * e_full);
}

TEST_CASE("stope reverb: the first reflection carries the whole signal chain") {
    // The first return's VALUE is the product of every stage the signal passed
    // through: comb input trim, the 1/4 comb sum, both allpass coefficients and
    // the wet mix. Pinning the value therefore pins the CHAIN -- deleting the
    // two allpass diffusers doubles it and is caught here, where an
    // is-it-nonzero check is not.
    render::StopeReverb r = make_immersed();
    const int n = 8192;
    std::vector<short> buf(static_cast<size_t>(n), static_cast<short>(0));
    buf[0] = 32767;
    r.process(buf.data(), n);

    int min_comb = r.comb_len_[0];
    for (int c = 1; c < render::kStopeCombCount; ++c)
        min_comb = std::min(min_comb, r.comb_len_[c]);
    const int at = r.pre_len_ + min_comb;

    // One comb has arrived, the other three have not; the pair of allpasses
    // each contribute a factor of -g, so two of them give +g^2.
    const double expect = render::kStopeWetMax * r.wet() *
                          render::kStopeCombInputTrim * 0.25 *
                          render::kStopeAllpassG * render::kStopeAllpassG *
                          32767.0;
    REQUIRE(expect > 4.0);  // non-vacuity: the pin is above the quantiser
    REQUIRE(static_cast<double>(buf[static_cast<size_t>(at)]) ==
            Approx(expect).margin(2.0));
}

TEST_CASE("stope reverb: the walls are damped -- the tail darkens as it decays") {
    // Rock and air eat the top end, so each round trip loses more of it. The
    // coefficient is DERIVED from the cutoff, so the derivation is pinned
    // rather than a typed number. Mutation killed: damp_a_ = 0 (no damping),
    // which leaves the tail as bright at the end as at the start.
    render::StopeReverb r;
    r.init(22050.0);
    REQUIRE(r.damp_a_ ==
            Approx(std::exp(-2.0 * render::kAudioPI * render::kStopeDampHz /
                            22050.0))
                .margin(1e-12));
    REQUIRE(r.damp_a_ > 0.0);
    REQUIRE(r.damp_a_ < 1.0);

    for (int i = 0; i < 600; ++i) r.set_inside(true, 1.0 / 60.0);
    const int n = 8192;
    // Brightness proxy: mean |first difference| per unit amplitude. A damped
    // tail loses high frequencies faster than it loses level, so this ratio
    // must FALL between an early window and a late one.
    auto brightness = [n](std::vector<short>& b) {
        double d = 0.0, a = 0.0;
        for (int i = 1; i < n; ++i) {
            d += std::abs(static_cast<double>(b[static_cast<size_t>(i)]) -
                          static_cast<double>(b[static_cast<size_t>(i - 1)]));
            a += std::abs(static_cast<double>(b[static_cast<size_t>(i)]));
        }
        return (a > 0.0) ? d / a : 0.0;
    };
    std::vector<short> first(static_cast<size_t>(n), static_cast<short>(0));
    first[0] = 32767;
    r.process(first.data(), n);
    std::vector<short> early(static_cast<size_t>(n), static_cast<short>(0));
    r.process(early.data(), n);
    std::vector<short> late(static_cast<size_t>(n), static_cast<short>(0));
    for (int k = 0; k < 4; ++k) {
        std::fill(late.begin(), late.end(), static_cast<short>(0));
        r.process(late.data(), n);
    }
    REQUIRE(brightness(early) > 0.0);
    REQUIRE(brightness(late) < brightness(early));
}

TEST_CASE("stope reverb: a sustained tone on a comb resonance cannot rail") {
    // The measured failure this guards: a comb's steady-state gain at its own
    // resonance is 1/(1-g) ~ 8, and the throttle drone is a SUSTAINED tone that
    // SWEEPS its pitch, so it parks on those resonances by design. Before the
    // input trim and the soft knee, a full-scale drone railed half its samples
    // at the int16 clamp. Drive exactly that case.
    render::StopeReverb r = make_immersed();
    const double f = 1.0 / (static_cast<double>(r.comb_len_[0]) / 22050.0);
    int railed = 0, total = 0;
    double phase = 0.0;
    for (int pass = 0; pass < 60; ++pass) {  // ~2.8 s, well past RT60
        short buf[1024];
        for (int i = 0; i < 1024; ++i) {
            buf[i] = static_cast<short>(
                32000.0 * std::sin(2.0 * render::kAudioPI * phase));
            phase += f / 22050.0;
        }
        r.process(buf, 1024);
        if (pass >= 40) {  // measure only the settled steady state
            for (int i = 0; i < 1024; ++i) {
                ++total;
                if (buf[i] >= 32760 || buf[i] <= -32760) ++railed;
            }
        }
    }
    REQUIRE(total > 0);
    // Was ~50% of samples. The soft knee means a few may still touch the top.
    REQUIRE(static_cast<double>(railed) / static_cast<double>(total) < 0.01);
}

TEST_CASE("stope reverb: the soft clip is transparent below its knee") {
    // It must be a limiter on the tail, not a colouring on the dry crack.
    REQUIRE(render::stope_soft_clip(0.0) == Approx(0.0));
    REQUIRE(render::stope_soft_clip(0.5) == Approx(0.5));
    REQUIRE(render::stope_soft_clip(render::kStopeSoftKnee) ==
            Approx(render::kStopeSoftKnee));
    REQUIRE(render::stope_soft_clip(-0.5) == Approx(-0.5));
    // ...and bounded, monotone and continuous above it. The bound is <= 1: at
    // a large enough argument tanh saturates to exactly 1.0 in double, which is
    // the full-scale sample and is precisely what the limiter is for.
    REQUIRE(std::abs(render::stope_soft_clip(50.0)) <= 1.0);
    REQUIRE(std::abs(render::stope_soft_clip(-50.0)) <= 1.0);
    REQUIRE(std::abs(render::stope_soft_clip(1.2)) < 1.0);  // in the knee
    double prev = -2.0;
    for (int i = 0; i <= 400; ++i) {
        const double x = -2.0 + 0.01 * i;
        const double y = render::stope_soft_clip(x);
        REQUIRE(y > prev);
        REQUIRE(std::abs(y) <= 1.0);
        prev = y;
    }
    const double e = 1e-7;
    REQUIRE(render::stope_soft_clip(render::kStopeSoftKnee - e) ==
            Approx(render::stope_soft_clip(render::kStopeSoftKnee + e))
                .margin(1e-6));
}

TEST_CASE("audio_slew_coef: the shared one-pole law, pinned directly") {
    // A new single-source primitive (H1): music_slew() and the reverb's wet
    // ramp both call it, and replacing it with the naive dt/tau survives every
    // MusicDirector test because the internal clamp absorbs the difference.
    REQUIRE(render::audio_slew_coef(0.0, 1.0) == Approx(0.0));
    REQUIRE(render::audio_slew_coef(-1.0, 1.0) == Approx(0.0));
    REQUIRE(render::audio_slew_coef(1.0, 0.0) == Approx(0.0));
    // One time constant covers 1 - 1/e of the remaining distance. The naive
    // dt/tau would answer exactly 1.0 here.
    REQUIRE(render::audio_slew_coef(1.0, 1.0) ==
            Approx(1.0 - std::exp(-1.0)).margin(1e-12));
    REQUIRE(render::audio_slew_coef(1.0, 1.0) < 0.99);
    REQUIRE(render::audio_slew_coef(0.5, 2.0) ==
            Approx(1.0 - std::exp(-0.25)).margin(1e-12));
    // A hitched frame saturates rather than overshooting.
    REQUIRE(render::audio_slew_coef(1e6, 1.0) == Approx(1.0));
    // And music_slew() IS this function, not a second copy of the law.
    REQUIRE(render::music_slew(0.37, 1.9) ==
            Approx(render::audio_slew_coef(0.37, 1.9)).margin(0.0));
}

// ---------------------------------------------------------------------------
// THE GAMEPLAY MIX -- render/mix_levels.h
//
// Chad, 2026-08-17, asked which end should move to make the loading page
// audibly louder than gameplay: "trim wind / engine further". So the whole
// gameplay bus drops on one dial while the wind/music ratios from his first fly
// -- which he has NOT yet judged -- are preserved by derivation.
// ---------------------------------------------------------------------------

TEST_CASE("mix: the loading page is AUDIBLY louder than gameplay, not just louder") {
    // The bug this replaces: 1.00 vs 0.95 satisfied `>` while delivering
    // 0.45 dB, i.e. nothing. Assert the SIZE of the gap, not its sign.
    REQUIRE(render::kMusicSurfaceGain < render::kLoadMusicGain);
    const double db = 20.0 * std::log10(render::kMusicSurfaceGain /
                                        render::kLoadMusicGain);
    REQUIRE(db < -3.0);   // at least 3 dB down: a heard difference
    REQUIRE(db > -12.0);  // ...but the bed must still be a bed in gameplay
}

TEST_CASE("mix: the bus dial reaches every channel it should, and no other") {
    // Two dials, two jobs. kGameplayBusGain carries EVERY gameplay channel
    // (that is what makes the loading page audibly the loud one). The extra
    // wind/engine trim carries only the two channels Chad named.
    const double g = render::kGameplayBusGain;
    REQUIRE(g > 0.0);
    REQUIRE(g < 1.0);
    REQUIRE(render::kMusicSurfaceGain ==
            Approx(render::kMusicFlownSurface * g).epsilon(1e-12));
    REQUIRE(render::kGunStreamLevel ==
            Approx(render::kGunFlownLevel * g).epsilon(1e-12));
    REQUIRE(render::kSfxStreamLevel ==
            Approx(render::kSfxFlownLevel * g).epsilon(1e-12));
    // The snowmachine engine is on the bus and NOT on the wind/engine trim:
    // "trim wind / engine further" was said about the aircraft, while flying,
    // about a mix in which this channel did not exist.
    REQUIRE(render::kSledStreamLevel ==
            Approx(render::kSledFlownLevel * g).epsilon(1e-12));
    REQUIRE(render::kSledStreamLevel !=
            Approx(render::kSledFlownLevel * g * render::kWindEngineTrim)
                .epsilon(1e-12));
    // ...and wind/engine carry BOTH, which is the whole of ruling 2.
    REQUIRE(render::kWindStreamTrim ==
            Approx(render::kWindFlownTrim * g * render::kWindEngineTrim)
                .epsilon(1e-12));
    REQUIRE(render::kEngineStreamTrim ==
            Approx(render::kEngineFlownTrim * g * render::kWindEngineTrim)
                .epsilon(1e-12));
    // The deep bed stays "on the quieter side" of the surface bed.
    REQUIRE(render::kMusicDeepGain < render::kMusicSurfaceGain);
    REQUIRE(render::kMusicDeepGain ==
            Approx(render::kMusicSurfaceGain * render::kMusicDeepRatio)
                .epsilon(1e-12));
}

TEST_CASE("mix: the snowmachine engine sits under the music bed") {
    // The one complaint Chad has made twice about this mix is that the music is
    // buried. A brand-new full-scale engine channel is exactly the thing that
    // would do it again, so the relationship is pinned rather than left to the
    // constant's comment.
    REQUIRE(render::kSledStreamLevel > 0.0);
    // ⚠ RETRACTED 2026-08-24 (fly 4), with its static_assert twin in
    // render/mix_levels.h:
    //     REQUIRE(render::kSledStreamLevel < render::kMusicSurfaceGain);
    // Chad ruled the world louder than the bed -- "its an environmental sound
    // effect, not very immersive if not quite a bit louder than the music" --
    // and this pinned the opposite. What is checked instead is that the
    // channel still FITS, which is not a taste claim.
    REQUIRE(render::kSledStreamLevel * render::kSledMaster < 1.0);
    // ⚠ There used to be a REQUIRE here that the sled sat ABOVE the guns,
    // matching a static_assert in mix_levels.h. Chad's first ride (2026-08-20:
    // "and decibels") took the engine down past both. The claim was an argument
    // about what a vehicle engine deserves, not a property of correct code, and
    // it is retracted in both places -- see the note in mix_levels.h. What
    // replaces it is a FLOOR: the machine under you must not be quieter than
    // the music behind it.
    REQUIRE(render::kSledStreamLevel > 0.25 * render::kMusicSurfaceGain);
    // And it carries the synth's own internal master on top of this, so the
    // product is what actually reaches the mixer.
    REQUIRE(render::kSledStreamLevel * render::kSledMaster < 1.0);
}

TEST_CASE("mix: nothing in the gameplay bus is loud enough to clip on its own") {
    REQUIRE(render::kLoadMusicGain <= 1.0);
    REQUIRE(render::kSledStreamLevel > 0.0);
    REQUIRE(render::kSledStreamLevel < 1.0);
    REQUIRE(render::kMusicSurfaceGain > 0.0);
    REQUIRE(render::kWindStreamTrim > 0.0);
    REQUIRE(render::kEngineStreamTrim > 0.0);
    REQUIRE(render::kWindStreamTrim < 1.0);
    REQUIRE(render::kEngineStreamTrim < 1.0);
}

// ---------------------------------------------------------------------------
// THE BLAST'S OWN ECHO -- render/sample_voice.h + StopeReverb::wet_only
//
// Chad, 2026-08-17: "yes echos the stope explosion itself". The recorded blast
// keeps playing DRY through raylib's Sound path (stereo, native rate); a mono
// copy runs through a SampleVoice into a wet_only StopeReverb, which supplies
// the tail. The send is what lets the dry blast keep its stereo quality.
// ---------------------------------------------------------------------------

namespace {

// A stand-in for raylib's LoadWaveSamples output: `frames` frames of
// interleaved stereo float at 44.1 kHz, with a decaying burst in it.
std::vector<float> fake_stereo_clip(std::size_t frames) {
    std::vector<float> v(frames * 2, 0.0f);
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / 44100.0;
        const double env = std::exp(-t * 3.0);
        const double x = env * std::sin(2.0 * render::kAudioPI * 90.0 * t);
        v[i * 2 + 0] = static_cast<float>(x);
        v[i * 2 + 1] = static_cast<float>(x);
    }
    return v;
}

}  // namespace

TEST_CASE("blast send: a bad decode leaves a silent send, never a crash") {
    render::SampleVoice v;
    REQUIRE(!v.ok());
    REQUIRE(!v.playing());
    v.trigger();  // must be safe before init
    REQUIRE(!v.playing());
    short buf[64];
    for (int i = 0; i < 64; ++i) buf[i] = 999;
    v.render(buf, 64);
    for (int i = 0; i < 64; ++i) REQUIRE(buf[i] == 0);  // fills with silence

    v.init(22050.0, nullptr, 100, 2, 44100.0);
    REQUIRE(!v.ok());
    std::vector<float> tiny(8, 0.0f);
    v.init(22050.0, tiny.data(), 0, 2, 44100.0);
    REQUIRE(!v.ok());
    v.init(22050.0, tiny.data(), 4, 0, 44100.0);
    REQUIRE(!v.ok());
    v.render(nullptr, 64);  // must not dereference
    v.render(buf, 0);
}

TEST_CASE("blast send: the clip is resampled to the stream's rate") {
    // 1 s of 44.1 kHz stereo in, 22050 mono samples out.
    const std::size_t frames = 44100;
    std::vector<float> src = fake_stereo_clip(frames);
    render::SampleVoice v;
    v.init(22050.0, src.data(), frames, 2, 44100.0);
    REQUIRE(v.ok());
    REQUIRE(v.clip.size() == 22050);
    REQUIRE(v.length_sec(22050.0) == Approx(1.0).margin(0.01));
}

TEST_CASE("blast send: it is a ONE-SHOT -- idle until triggered, then done") {
    const std::size_t frames = 4410;  // 0.1 s
    std::vector<float> src = fake_stereo_clip(frames);
    render::SampleVoice v;
    v.init(22050.0, src.data(), frames, 2, 44100.0);
    REQUIRE(v.ok());

    // Idle after init: a voice that started playing on load would fire the
    // blast at the title screen.
    REQUIRE(!v.playing());
    short quiet[2048] = {};
    v.render(quiet, 2048);
    REQUIRE(abs_sum(quiet, 2048) == 0.0);

    v.trigger();
    REQUIRE(v.playing());
    short loud[2048] = {};
    v.render(loud, 2048);
    REQUIRE(abs_sum(loud, 2048) > 0.0);

    // Runs out and stays out -- it does not loop.
    short rest[4096] = {};
    v.render(rest, 4096);
    REQUIRE(!v.playing());
    short after[2048] = {};
    v.render(after, 2048);
    REQUIRE(abs_sum(after, 2048) == 0.0);
}

TEST_CASE("blast send: re-triggering restarts, matching the Sound it shadows") {
    // The dry blast is one non-polyphonic Sound: a re-trigger restarts it. The
    // send MUST do the same or the echo would be of a different blast than the
    // one you just heard.
    const std::size_t frames = 4410;
    std::vector<float> src = fake_stereo_clip(frames);
    render::SampleVoice v;
    v.init(22050.0, src.data(), frames, 2, 44100.0);
    v.trigger();
    short a[512] = {};
    v.render(a, 512);
    v.trigger();
    short b[512] = {};
    v.render(b, 512);
    for (int i = 0; i < 512; ++i) REQUIRE(a[i] == b[i]);  // same opening again
}

TEST_CASE("blast send: wet_only emits the ROOM and never a second dry blast") {
    // The one failure mode a send has that an insert does not: leaking its
    // input into the mix, which would double the blast instead of echoing it.
    render::StopeReverb send;
    send.init(22050.0);
    send.wet_only = true;
    for (int i = 0; i < 600; ++i) send.set_inside(true, 1.0 / 60.0);
    REQUIRE(send.wet() > 0.99);

    const int n = 8192;
    std::vector<short> buf(static_cast<std::size_t>(n), static_cast<short>(0));
    buf[0] = 32767;
    send.process(buf.data(), n);
    REQUIRE(buf[0] == 0);  // the dry impulse is GONE from the send's output

    // ...but the room is there, and it arrives late, as a room does.
    int min_comb = send.comb_len_[0];
    for (int c = 1; c < render::kStopeCombCount; ++c)
        min_comb = std::min(min_comb, send.comb_len_[c]);
    REQUIRE(buf[static_cast<std::size_t>(send.pre_len_ + min_comb)] != 0);
    double tail = 0.0;
    for (int i = 1; i < n; ++i)
        tail += std::abs(static_cast<double>(buf[static_cast<std::size_t>(i)]));
    REQUIRE(tail > 0.0);
}

TEST_CASE("blast send: a bypassed send goes SILENT, not through") {
    // Outside the tunnel net an INSERT passes its dry through untouched -- that
    // is what makes open sky bit-identical. A SEND doing the same would inject
    // a bare second copy of the blast into the mix. Opposite requirement, same
    // code path, so pin both.
    render::StopeReverb insert, send;
    insert.init(22050.0);
    send.init(22050.0);
    send.wet_only = true;
    insert.set_inside(false, 1.0 / 60.0);
    send.set_inside(false, 1.0 / 60.0);
    REQUIRE(insert.bypassed());
    REQUIRE(send.bypassed());

    short a[256], b[256];
    for (int i = 0; i < 256; ++i) {
        a[i] = static_cast<short>((i * 613) % 20000 - 10000);
        b[i] = a[i];
    }
    insert.process(a, 256);
    send.process(b, 256);
    bool insert_passed = false;
    for (int i = 0; i < 256; ++i) {
        if (a[i] != 0) insert_passed = true;
        REQUIRE(b[i] == 0);
    }
    REQUIRE(insert_passed);  // non-vacuity: the insert really did pass audio
}

// ---------------------------------------------------------------------------
// The legs below exist because a fresh-context red-team showed the first
// version of the "mix:" tests passed with FLY 1 ENTIRELY REVERTED -- set the
// flown constants back to the values Chad complained about (music 0.55, wind
// 0.85, drone 0.42) and every assertion, being an algebraic identity over
// those same constants, still held. Identities pin the SHAPE of a derivation;
// they pin no value. These pin the values.
// ---------------------------------------------------------------------------

TEST_CASE("mix: the flown balance itself is pinned, not just its derivation") {
    // Fly 1 (2026-08-17): "The wind and the engine are too loud to hear the
    // music." These four numbers ARE that fix. If a retune walks them back,
    // this fails -- loudly, and by name.
    // ⚠ MUSIC RE-PINNED 0.95 -> 0.67 on 2026-08-24, by the same ear that set
    // 0.95. Chad, after the first fly that had a train in it: "turns the music
    // down a bit ... I can barely hear the train through the music." Fly 1's
    // ruling was made in a mix with no train, no animals and a silent
    // snowmachine; three channels have arrived since. The number is re-pinned
    // rather than unpinned -- the point of this case is that a RETUNE cannot
    // walk the balance back silently, and a ruling is not a retune.
    REQUIRE(render::kMusicFlownSurface == Approx(0.67));
    REQUIRE(render::kWindFlownTrim == Approx(0.55));
    REQUIRE(render::kEngineFlownTrim == Approx(0.30));
    // ...and the pre-fix values must NOT be what is shipping.
    REQUIRE(render::kMusicFlownSurface > 0.55);  // was 0.55 and inaudible
    REQUIRE(render::kWindFlownTrim < 0.8);       // was 0.85 and masking
    REQUIRE(render::kEngineFlownTrim < 0.40);    // was 0.42
}

TEST_CASE("mix: 'trim wind / engine further' actually moved wind and engine") {
    // Chad's ruling, verbatim: "trim wind / engine further". FURTHER means
    // further than the music -- a uniform volume change moves neither channel
    // relative to the thing they were burying, and the first draft of this
    // header shipped exactly that (and asserted it could not be otherwise).
    const double wind_before =
        render::kWindFlownTrim / render::kMusicFlownSurface;
    const double wind_now = render::kWindStreamTrim / render::kMusicSurfaceGain;
    REQUIRE(wind_now < wind_before);
    const double eng_before =
        render::kEngineFlownTrim / render::kMusicFlownSurface;
    const double eng_now =
        render::kEngineStreamTrim / render::kMusicSurfaceGain;
    REQUIRE(eng_now < eng_before);

    // And the move is a real one, not a rounding: at least 2 dB of relief.
    REQUIRE(20.0 * std::log10(wind_now / wind_before) < -2.0);
    REQUIRE(20.0 * std::log10(eng_now / eng_before) < -2.0);

    // ...but the wind is still THERE. It carries the speed cue; trimmed into
    // inaudibility it trades one complaint for a worse one.
    REQUIRE(wind_now > 0.15);
    REQUIRE(eng_now > 0.10);
}

TEST_CASE("mix: every gameplay channel rides the bus, so none floats up") {
    // Dropping only music/wind/engine would have left the guns, the combat
    // one-shots and the dry blast +4 dB relative to the bed -- the opposite of
    // every ruling Chad has made about this mix. The dry blast is the sharp
    // one: it had no volume set at all, so raylib played it at unity.
    const double g = render::kGameplayBusGain;
    REQUIRE(render::kGunStreamLevel == Approx(render::kGunFlownLevel * g));
    REQUIRE(render::kSfxStreamLevel == Approx(render::kSfxFlownLevel * g));
    REQUIRE(render::kBoomDryLevel == Approx(1.0 * g));
    REQUIRE(render::kBoomSendLevel == Approx(0.85 * g));
    // Non-vacuity: they really are below where they used to sit.
    REQUIRE(render::kGunStreamLevel < render::kGunFlownLevel);
    REQUIRE(render::kBoomDryLevel < 1.0);
    // The echo sits under its own dry blast -- a room, not a second boom.
    REQUIRE(render::kBoomSendLevel < render::kBoomDryLevel);
}

TEST_CASE("mix: the two dials do different jobs and neither is a no-op") {
    // If they ever collapse into one number, the header's whole claim -- that
    // "quieter overall" and "wind trimmed further" are separable -- is false.
    REQUIRE(render::kGameplayBusGain < 1.0);
    REQUIRE(render::kWindEngineTrim < 1.0);
    // Dial 1 alone must not deliver dial 2's job...
    REQUIRE(render::kWindStreamTrim <
            render::kWindFlownTrim * render::kGameplayBusGain);
    // ...and dial 1 must be doing something the music can feel.
    REQUIRE(render::kMusicSurfaceGain < render::kMusicFlownSurface);
}

// ---------------------------------------------------------------------------
// SampleVoice resampling. The first version pinned only clip.size() and
// "some output", which the red-team showed survives nearest-neighbour
// playback, an INVERTED interpolation, and deleting the anti-alias filter
// outright. These probe the arithmetic and the filter directly.
//
// Note the source rates: 44.1 kHz -> 22.05 kHz is an EXACT 2:1 decimation, so
// every output sample lands on a source sample and the interpolation never
// runs. A resampling test at the shipped rate is structurally blind to the
// interpolator -- these use 48 kHz, where the fractional part is live.
// ---------------------------------------------------------------------------

TEST_CASE("blast send: the resampler interpolates rather than picking nearest") {
    // A linear ramp resampled linearly is still the same ramp. Nearest
    // neighbour staircases it; an inverted lerp tilts it the wrong way.
    //
    // TWO probes, because the shipped path has an anti-alias filter in front
    // of the interpolator and that filter has a group delay -- on a ramp it
    // shows up as a constant OFFSET (measured: 3.1e-4, exactly the 1.5-sample
    // delay of four one-poles at this cutoff times the ramp slope). Comparing
    // the downsampled ramp to the ideal line therefore CANNOT be exact, and a
    // test that demanded exactness would be failing the filter, not the
    // interpolator.
    //
    // (a) UPSAMPLING skips the filter entirely (decim < 1), so the
    //     interpolator stands alone and the line is reproduced exactly.
    {
        const std::size_t frames = 1600;
        std::vector<float> src(frames);
        for (std::size_t i = 0; i < frames; ++i)
            src[i] = static_cast<float>(i) / static_cast<float>(frames);
        render::SampleVoice v;
        v.init(22050.0, src.data(), frames, 1, 16000.0);  // 16k -> 22.05k
        REQUIRE(v.ok());
        const double decim = 16000.0 / 22050.0;
        REQUIRE(decim < 1.0);  // no anti-alias stage on this path
        double worst = 0.0;
        for (std::size_t i = 0; i + 1 < v.clip.size(); ++i) {
            const double sp = static_cast<double>(i) * decim;
            const double expect = sp / static_cast<double>(frames);
            worst = std::max(
                worst, std::abs(static_cast<double>(v.clip[i]) - expect));
        }
        REQUIRE(worst < 1e-6);
    }
    // (b) DOWNSAMPLING, the shipped direction: the error against the ideal
    //     line must be a CONSTANT (a pure delay), not a sawtooth. Nearest
    //     neighbour produces a sawtooth of ~decim/2 source steps; linear
    //     interpolation plus a fixed filter delay produces a flat offset.
    {
        const std::size_t frames = 4800;
        std::vector<float> src(frames);
        for (std::size_t i = 0; i < frames; ++i)
            src[i] = static_cast<float>(i) / static_cast<float>(frames);
        render::SampleVoice v;
        v.init(22050.0, src.data(), frames, 1, 48000.0);
        REQUIRE(v.ok());
        const double decim = 48000.0 / 22050.0;
        REQUIRE(decim != Approx(std::round(decim)));  // the fraction is live
        double lo = 1e9, hi = -1e9;
        // Skip the filter settling head and the very tail.
        for (std::size_t i = 50; i + 2 < v.clip.size(); ++i) {
            const double sp = static_cast<double>(i) * decim;
            const double err = static_cast<double>(v.clip[i]) -
                               sp / static_cast<double>(frames);
            lo = std::min(lo, err);
            hi = std::max(hi, err);
        }
        const double ripple = hi - lo;
        const double nearest_sawtooth = 0.5 * decim / static_cast<double>(frames);
        REQUIRE(nearest_sawtooth > 2e-4);       // non-vacuity: the mutant is big
        REQUIRE(ripple < 0.1 * nearest_sawtooth);  // ours is flat
    }
}

TEST_CASE("blast send: the anti-alias filter is doing its job") {
    // A tone above the OUTPUT Nyquist must not survive the decimation. Without
    // the filter it folds back down as a loud phantom whine sitting on top of
    // the blast's low rumble -- audible, wrong, and invisible to any "is there
    // output" check.
    //
    // The bound is set from the MEASURED rejection (21.3 dB at 18 kHz for the
    // four one-poles this path uses, inherited from the flown bagpipe decode),
    // not from a number the filter was never going to deliver. That is still
    // decisive: with the filter deleted, 18 kHz folds to 4 kHz at FULL
    // amplitude, i.e. a ratio near 1.0 against a bound of 0.12.
    const std::size_t frames = 48000;  // 1 s at 48 kHz
    const double out_sr = 22050.0;
    auto tone = [&](double hz) {
        std::vector<float> v(frames);
        for (std::size_t i = 0; i < frames; ++i)
            v[i] = static_cast<float>(std::sin(2.0 * render::kAudioPI * hz *
                                               static_cast<double>(i) / 48000.0));
        return v;
    };
    auto rms = [](const std::vector<float>& c) {
        double s = 0.0;
        for (float x : c) s += static_cast<double>(x) * static_cast<double>(x);
        return c.empty() ? 0.0 : std::sqrt(s / static_cast<double>(c.size()));
    };

    // In band: passes through essentially untouched.
    std::vector<float> low = tone(500.0);
    render::SampleVoice a;
    a.init(out_sr, low.data(), frames, 1, 48000.0);
    REQUIRE(a.ok());
    REQUIRE(rms(a.clip) > 0.5);  // a unit sine has rms ~0.707

    // Well above the output Nyquist: must be crushed, not folded.
    REQUIRE(18000.0 > out_sr * 0.5);
    std::vector<float> high = tone(18000.0);
    render::SampleVoice b;
    b.init(out_sr, high.data(), frames, 1, 48000.0);
    REQUIRE(b.ok());
    REQUIRE(rms(b.clip) < 0.12 * rms(a.clip));  // > 18 dB of rejection
}

TEST_CASE("blast send: a stereo source is downmixed, not just left-channel'd") {
    // The blast asset is stereo. Taking only channel 0 would silently throw
    // away half the recording -- and would pass every size and non-silence
    // check. Feed channels that CANCEL: a correct downmix gives silence, a
    // pick-one gives full scale.
    const std::size_t frames = 8000;
    std::vector<float> lr(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        const float x = static_cast<float>(
            std::sin(2.0 * render::kAudioPI * 300.0 *
                     static_cast<double>(i) / 48000.0));
        lr[i * 2 + 0] = x;
        lr[i * 2 + 1] = -x;  // exactly out of phase
    }
    render::SampleVoice v;
    v.init(22050.0, lr.data(), frames, 2, 48000.0);
    REQUIRE(v.ok());
    double peak = 0.0;
    for (float s : v.clip) peak = std::max(peak, std::abs(static_cast<double>(s)));
    REQUIRE(peak < 1e-3);  // L + R averaged to nothing, as it should
}
