// Unit tests for the Chelmsford church bell -- render/bell_ambience.h.
//
// Chad, 2026-08-24: "make it play different amounts of plays at random,
// sometimes it rings 3 times, sometimes 7 times, up to twelve to simulate the
// number of chimes it plays based on the hour ... about two minutes after the
// train might, in and over chelmsford" / "from one chime up to 12, 3 was an
// example, it should be random though".
//
// The policy header is pure, so WHEN it rings, HOW MANY TIMES, and what happens
// when you leave town are all testable headlessly. What this CANNOT test is
// that any of it is audible: the gate never runs seads.exe (see the lane's
// handoff -- a channel with no caller ships green). This pins BEHAVIOUR.
//
// Several cases below are shaped against a specific wrong implementation rather
// than for coverage. The abandon-on-leaving case is the whole reason
// abandon_peal() is not a pause, and a scheduler that paused instead passes
// every other case here. The repeat-guard case fails on a plain uniform draw.

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render/bell_ambience.h"
#include "render/mix_levels.h"
#include "render/town_ambience.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Approx;

namespace {

constexpr double kDt = 1.0 / 60.0;

// Run the tower for `seconds` of in-town frames, collecting every strike.
// Returns the peal counts observed, in order.
std::vector<int> ring_for(render::BellTower& tower, double seconds,
                          bool near_town = true) {
    std::vector<int> peals;
    const int steps = static_cast<int>(seconds / kDt);
    for (int i = 0; i < steps; ++i) {
        const render::BellTick t = tower.update(near_town, kDt);
        if (t.peal_start) peals.push_back(t.chime_total);
    }
    return peals;
}

// Count the strikes in the next `seconds`.
int strikes_for(render::BellTower& tower, double seconds,
                bool near_town = true) {
    int n = 0;
    const int steps = static_cast<int>(seconds / kDt);
    for (int i = 0; i < steps; ++i) {
        if (tower.update(near_town, kDt).strike) ++n;
    }
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------
// THE COUNT -- Chad's sentence, which is the whole point of this channel
// ---------------------------------------------------------------------------

TEST_CASE("the bell rings between one and twelve times, never outside it") {
    render::BellTower tower;
    std::vector<int> counts;
    // 60 peals is enough to exercise the draw well past its guard.
    for (int p = 0; p < 60; ++p) {
        tower.on_train();
        // Long enough for the delay plus a full twelve-count peal.
        const std::vector<int> got = ring_for(tower, 200.0);
        REQUIRE(got.size() == 1u);
        counts.push_back(got[0]);
    }
    for (int c : counts) {
        REQUIRE(c >= render::kBellChimeMin);
        REQUIRE(c <= render::kBellChimeMax);
    }
}

TEST_CASE("the count is drawn, not cycled -- it is not a fixed table") {
    // The failure this is shaped against is the obvious one: reaching for the
    // four-gap table pattern next door in town_ambience.h and cycling a fixed
    // list of counts. That reads as a bell ringing the same hours in the same
    // order forever, and Chad ruled specifically against it ("it should be
    // random though"). A cycled table of length N repeats exactly every N.
    render::BellTower tower;
    std::vector<int> counts;
    for (int p = 0; p < 48; ++p) {
        tower.on_train();
        const std::vector<int> got = ring_for(tower, 200.0);
        REQUIRE(got.size() == 1u);
        counts.push_back(got[0]);
    }
    // No period from 1 to 12 explains the sequence.
    for (int period = 1; period <= 12; ++period) {
        bool periodic = true;
        for (std::size_t i = 0; i + period < counts.size(); ++i) {
            if (counts[i] != counts[i + period]) {
                periodic = false;
                break;
            }
        }
        REQUIRE_FALSE(periodic);
    }
    // And it actually uses the range rather than hovering in the middle.
    const std::set<int> distinct(counts.begin(), counts.end());
    REQUIRE(distinct.size() >= 8u);
}

TEST_CASE("the same count never rings twice running") {
    // A bell that rings seven and then seven again reads as a stuck loop, not
    // as two hours. At 1-in-12 a plain uniform draw hits this often enough for
    // a player to notice, so it is guarded -- and this case fails without the
    // guard.
    render::BellTower tower;
    int prev = 0;
    for (int p = 0; p < 200; ++p) {
        tower.on_train();
        const std::vector<int> got = ring_for(tower, 200.0);
        REQUIRE(got.size() == 1u);
        REQUIRE(got[0] != prev);
        prev = got[0];
    }
}

TEST_CASE("a peal sounds exactly as many strikes as it announces") {
    // The count is the content. A peal that announces seven and sounds six is
    // the one failure mode this channel cannot have.
    render::BellTower tower;
    for (int p = 0; p < 24; ++p) {
        tower.on_train();
        int announced = 0;
        int struck = 0;
        const int steps = static_cast<int>(300.0 / kDt);
        for (int i = 0; i < steps; ++i) {
            const render::BellTick t = tower.update(true, kDt);
            if (t.peal_start) announced = t.chime_total;
            if (t.strike) ++struck;
        }
        REQUIRE(announced > 0);
        REQUIRE(struck == announced);
    }
}

TEST_CASE("the chime index runs 1..total exactly once each") {
    render::BellTower tower;
    tower.on_train();
    std::vector<int> indices;
    int total = 0;
    const int steps = static_cast<int>(300.0 / kDt);
    for (int i = 0; i < steps; ++i) {
        const render::BellTick t = tower.update(true, kDt);
        if (t.peal_start) total = t.chime_total;
        if (t.strike) indices.push_back(t.chime_index);
    }
    REQUIRE(total > 0);
    REQUIRE(static_cast<int>(indices.size()) == total);
    for (int i = 0; i < total; ++i) REQUIRE(indices[static_cast<std::size_t>(i)] == i + 1);
}

// ---------------------------------------------------------------------------
// "ABOUT TWO MINUTES AFTER THE TRAIN"
// ---------------------------------------------------------------------------

TEST_CASE("nothing rings until a train has passed") {
    render::BellTower tower;
    // Ten minutes in town with no train: silence. The bell is keyed to the
    // train, not to a clock of its own.
    REQUIRE(strikes_for(tower, 600.0) == 0);
}

TEST_CASE("the first strike lands about two minutes after the train") {
    render::BellTower tower;
    tower.on_train();
    double t_first = -1.0;
    const int steps = static_cast<int>(300.0 / kDt);
    for (int i = 0; i < steps; ++i) {
        if (tower.update(true, kDt).peal_start) {
            t_first = static_cast<double>(i) * kDt;
            break;
        }
    }
    REQUIRE(t_first > 0.0);
    // "About two minutes" -- the four delays span 105..135 s and nothing may
    // land outside that. A bell 30 s behind the train is a different sound.
    REQUIRE(t_first >= 100.0);
    REQUIRE(t_first <= 140.0);
}

TEST_CASE("the delays are uneven and average two minutes") {
    double sum = 0.0;
    double lo = 1e9;
    double hi = -1e9;
    for (int i = 0; i < render::kBellDelayCount; ++i) {
        const double d = render::kBellDelaysSec[i];
        sum += d;
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    REQUIRE(sum / render::kBellDelayCount == Approx(120.0));
    // Non-vacuity: a table of four identical 120s would pass the mean check
    // and be a metronome.
    REQUIRE(hi - lo > 20.0);
}

TEST_CASE("a second train mid-countdown does not stack a second peal") {
    // Train gaps run 105-225 s and this delay is ~120 s, so a second train
    // arriving before the bell is the normal case, not an edge one. Two
    // overlapping peals would give a count nobody can read.
    //
    // ⚠ THE WINDOW IS BOUNDED BY THE FIRST PEAL, and that bound is the test
    // being right rather than being lenient. The first draft ran a flat 300 s
    // and failed at 2 == 1 -- correctly, because once a peal COMPLETES the
    // tower is free again and the next train is entitled to arm another one.
    // That is the channel working. What must not stack is a train arriving
    // while one is pending or ringing, so that is exactly the window to watch.
    render::BellTower tower;
    tower.on_train();
    int peals = 0;
    int total = 0;
    int struck = 0;
    const int steps = static_cast<int>(300.0 / kDt);
    for (int i = 0; i < steps; ++i) {
        if (i % 600 == 0) tower.on_train();  // a train every 10 s, absurdly often
        const render::BellTick t = tower.update(true, kDt);
        if (t.peal_start) {
            ++peals;
            total = t.chime_total;
        }
        if (t.strike && ++struck == total) break;  // that peal is done
    }
    REQUIRE(peals == 1);
    REQUIRE(struck == total);
    // Non-vacuity: the window really did span the countdown AND the peal, so
    // several trains were refused rather than none being offered.
    REQUIRE(total >= 1);
}

// ---------------------------------------------------------------------------
// "IN AND OVER CHELMSFORD" -- the town gate
// ---------------------------------------------------------------------------

TEST_CASE("the countdown only advances while near the town") {
    render::BellTower away;
    away.on_train();
    REQUIRE(strikes_for(away, 600.0, /*near_town=*/false) == 0);

    // ...and it PAUSES rather than resetting: the schedule is a fact about the
    // world, so wandering out of Chelmsford and back banks the wait instead of
    // restarting it. Sixty seconds in, ten minutes away, then back -- the
    // remaining wait is what is left, not a fresh 105 s.
    render::BellTower there;
    there.on_train();
    REQUIRE(strikes_for(there, 60.0) == 0);
    REQUIRE(strikes_for(there, 600.0, /*near_town=*/false) == 0);
    REQUIRE(strikes_for(there, 80.0) > 0);
}

TEST_CASE("leaving town ABANDONS a peal in progress, it does not pause it") {
    // This is the case abandon_peal() exists for, and a scheduler that paused
    // the peal the way it pauses the countdown passes every other test in this
    // file. Resuming at chime four of seven, minutes later and miles away, is a
    // bell that waited for you.
    render::BellTower tower;
    // Draw until a peal long enough to actually interrupt. Bailing out with a
    // SUCCEED on a short draw would let this case pass without ever testing
    // anything, which is the vacuity the pump-ambience review caught.
    int total = 0;
    for (int attempt = 0; attempt < 40 && total < 4; ++attempt) {
        tower.abandon_peal();
        tower.on_train();
        const int steps = static_cast<int>(300.0 / kDt);
        for (int i = 0; i < steps; ++i) {
            const render::BellTick t = tower.update(true, kDt);
            if (t.peal_start) {
                total = t.chime_total;
                break;
            }
        }
        if (total < 4) {
            // Let the short peal finish so the next on_train() is accepted.
            (void)strikes_for(tower, 40.0);
        }
    }
    REQUIRE(total >= 4);
    // Sound two more chimes, then leave.
    REQUIRE(strikes_for(tower, 2.5 * render::kBellChimeGapSec) >= 2);
    REQUIRE(tower.ringing());
    (void)tower.update(/*near_town=*/false, kDt);
    REQUIRE_FALSE(tower.ringing());
    // Coming back must not resume the remainder.
    REQUIRE(strikes_for(tower, 60.0) == 0);
}

TEST_CASE("abandoning a peal keeps the pending countdown") {
    // The two halves are governed separately on purpose: the peal is about your
    // ears, the countdown is about the world. Abandoning one must not cancel
    // the other.
    render::BellTower tower;
    tower.on_train();
    REQUIRE(strikes_for(tower, 30.0) == 0);
    tower.abandon_peal();
    REQUIRE(tower.armed);
    REQUIRE(strikes_for(tower, 150.0) > 0);
}

TEST_CASE("a zero or negative dt moves nothing") {
    render::BellTower tower;
    tower.on_train();
    for (int i = 0; i < 1000; ++i) {
        REQUIRE_FALSE(tower.update(true, 0.0).strike);
        REQUIRE_FALSE(tower.update(true, -1.0).strike);
    }
    REQUIRE(tower.wait == Approx(0.0));
}

// ---------------------------------------------------------------------------
// THE ASSET, THE RING, AND THE MIX
// ---------------------------------------------------------------------------

TEST_CASE("the strikes are evenly spaced at the chime gap") {
    render::BellTower tower;
    tower.on_train();
    std::vector<double> times;
    int total = 0;
    const int steps = static_cast<int>(300.0 / kDt);
    for (int i = 0; i < steps; ++i) {
        const render::BellTick t = tower.update(true, kDt);
        if (t.peal_start) total = t.chime_total;
        if (t.strike) times.push_back(static_cast<double>(i) * kDt);
    }
    REQUIRE(total > 0);
    for (std::size_t k = 1; k < times.size(); ++k) {
        // Within one frame of the gap -- the scheduler carries the remainder
        // forward, so error must not accumulate across a twelve-count.
        REQUIRE(times[k] - times[k - 1] == Approx(render::kBellChimeGapSec).margin(kDt * 1.5));
    }
    if (times.size() >= 3u) {
        REQUIRE(times.back() - times.front() ==
                Approx(render::kBellChimeGapSec *
                       static_cast<double>(times.size() - 1))
                    .margin(kDt * 2.0));
    }
}

TEST_CASE("the alias ring is large enough for the bell's own decay") {
    // The property, re-derived here rather than trusting the header's own
    // static_assert: with the asset ringing longer than the gap, two strikes
    // are live at once, and one spare voice keeps the one about to be struck
    // from being the one still ringing.
    REQUIRE(render::kBellChimeLen > render::kBellChimeGapSec);
    const int live = static_cast<int>(
        std::ceil(render::kBellChimeLen / render::kBellChimeGapSec));
    REQUIRE(render::kBellVoiceCount >= live + 1);
    REQUIRE(render::bell_voices_needed() == live + 1);
}

TEST_CASE("a twelve-count peal is a bounded event, not a minute of bell") {
    const double longest =
        render::kBellChimeGapSec *
            static_cast<double>(render::kBellChimeMax - 1) +
        render::kBellChimeLen;
    // Comfortably inside the shortest delay, so a peal can never still be
    // ringing when the next countdown would be due.
    double shortest_delay = 1e9;
    for (int i = 0; i < render::kBellDelayCount; ++i)
        shortest_delay = std::min(shortest_delay, render::kBellDelaysSec[i]);
    REQUIRE(longest < shortest_delay);
}

TEST_CASE("two overlapping strikes stay inside the -3 dBTP margin") {
    // The header derives kBellGroundGain from exactly this and asserts it at
    // compile time; this is the runtime statement of the same measurement, so
    // the reasoning is visible to someone reading the tests rather than only to
    // someone reading the header. Peak of the shipped asset x the gameplay bus
    // x the worst-case coherent sum of a fresh strike and the tail under it.
    const double ground = render::kBellGroundGain * render::kBellAssetPeak *
                          render::kGameplayBusGain * render::kBellOverlapSum;
    REQUIRE(ground < 0.708);
    // Non-vacuity: it is not merely quiet, it is near the ceiling it was
    // derived from. A bell 10 dB under this would pass the bound and fail the
    // ear, which is the failure this lane keeps making.
    REQUIRE(ground > 0.45);

    const double air = render::kBellFlyingGain * render::kBellAssetPeak *
                       render::kGameplayBusGain * render::kBellOverlapSum;
    REQUIRE(air < ground);
}

TEST_CASE("the bell reads the town gate, it does not define a second one") {
    // bell_gain takes render::TownContext -- the same type the train's gain
    // takes. If someone declares a BellContext here, this stops compiling, and
    // that is the point: two answers to "am I over the town" can disagree.
    REQUIRE(render::bell_gain(render::TownContext::kGround) ==
            Approx(render::kBellGroundGain));
    REQUIRE(render::bell_gain(render::TownContext::kFlying) ==
            Approx(render::kBellFlyingGain));
    REQUIRE(render::bell_gain(render::TownContext::kFlying) <
            render::bell_gain(render::TownContext::kGround));
}
