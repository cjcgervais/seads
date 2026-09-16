// Unit tests for the two animals at the surface pumps -- render/pump_ambience.h
// (Chad, 2026-08-20: a wolf at the Sudbury surface pump, a wildcat at the
// Onaping/Levack one, "when you fly over the area or snomobile close to it").
//
// The policy header is pure -- no raylib, no world, no assets -- so everything
// about WHEN and HOW LOUD is testable headlessly. Two of the cases below reach
// for world/faction_bubbles.h and the real planet radius as well, because the
// header bakes a claim about the real map (kPumpSeparationM) that is worth
// checking against the map rather than against itself.
//
// What this CANNOT test, by construction: that any of it is audible. The gate
// never runs seads.exe, so a silent channel ships green. This pins BEHAVIOUR;
// the ear judges whether a wolf at 3 km sounds like a wolf at 3 km.
//
// Several cases are shaped against a specific wrong implementation rather than
// for coverage -- the boundary-dither case in particular is the whole reason
// the re-arm hold exists, and a scheduler without it passes every other test
// here.

#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "config/load_aircraft.h"
#include "render/mix_levels.h"
#include "render/pump_ambience.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Approx;

namespace {
constexpr double kDt = 1.0 / 60.0;

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

double great_circle_m(const glm::dvec3& a, const glm::dvec3& b) {
    const double c =
        glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0);
    return kP.R * std::acos(c);
}

// Hold the listener at a fixed distance for `secs` and count the cries.
int cries_over(render::CryScheduler& s, bool near_pump, render::CryContext ctx,
               double secs) {
    int n = 0;
    const int steps = static_cast<int>(secs / kDt);
    for (int i = 0; i < steps; ++i)
        if (s.update(near_pump, ctx, kDt)) ++n;
    return n;
}
}  // namespace

// ---------------------------------------------------------------------------
// RANGE -- what "close" means, and that the two animals cannot share a place
// ---------------------------------------------------------------------------

TEST_CASE("cry range: the air radius is wider than the ground radius") {
    // "Fly over the area" and "snomobile close to it" are two different acts at
    // two different speeds; one radius for both would either be a region on the
    // sled or a dot from the air.
    REQUIRE(render::cry_range_m(render::CryContext::kFlying) >
            render::cry_range_m(render::CryContext::kGround));
    REQUIRE(render::cry_range_m(render::CryContext::kGround) > 0.0);
}

TEST_CASE("cry range: in range at the pump, out past the edge, edge included") {
    for (auto ctx : {render::CryContext::kGround, render::CryContext::kFlying}) {
        const double r = render::cry_range_m(ctx);
        REQUIRE(render::in_cry_range(0.0, ctx));
        REQUIRE(render::in_cry_range(r * 0.5, ctx));
        REQUIRE(render::in_cry_range(r, ctx));
        REQUIRE(!render::in_cry_range(r * 1.001, ctx));
        REQUIRE(!render::in_cry_range(r + 5000.0, ctx));
    }
}

TEST_CASE("cry range: a negative distance is never in range") {
    // Defensive: a distance can only come out of a great-circle call, which
    // cannot be negative -- but a sign slip upstream must not arm both animals
    // everywhere on the planet at once.
    REQUIRE(!render::in_cry_range(-1.0, render::CryContext::kGround));
    REQUIRE(!render::in_cry_range(-1.0, render::CryContext::kFlying));
}

TEST_CASE("cry range: the ranges cannot overlap on the REAL map") {
    // The header static_asserts this against kPumpSeparationM, a number typed
    // into it. This case checks that number against the actual baked anchors
    // and the real planet radius, so a future move of either pump (they were
    // moved once already, 2026-07-25) cannot leave the header asserting a
    // separation the map no longer has.
    const double sep = great_circle_m(world::kPumpValleySurface,
                                      world::kPumpSudburySurface);
    CHECK(sep >= render::kPumpSeparationM);
    // ...and the property the constant exists for, stated directly.
    CHECK(sep > 2.0 * render::kCryRangeFlyingM);
}

TEST_CASE("cry range: standing at one pump is out of range of the other") {
    // The same claim from the listener's side: at either anchor, the OTHER
    // animal must be silent in both contexts.
    const double d = great_circle_m(world::kPumpValleySurface,
                                    world::kPumpSudburySurface);
    for (auto ctx : {render::CryContext::kGround, render::CryContext::kFlying})
        CHECK(!render::in_cry_range(d, ctx));
}

// ---------------------------------------------------------------------------
// THE SCHEDULER
// ---------------------------------------------------------------------------

TEST_CASE("CryScheduler: never cries while you are away, however long") {
    render::CryScheduler s;
    REQUIRE(cries_over(s, false, render::CryContext::kGround, 3600.0) == 0);
    REQUIRE(cries_over(s, false, render::CryContext::kFlying, 3600.0) == 0);
}

TEST_CASE("CryScheduler: greets you shortly after you arrive") {
    render::CryScheduler s;
    // Not instantly -- a cry on the exact tick the radius is crossed reads as a
    // trigger volume, which is what it would be.
    REQUIRE(cries_over(s, true, render::CryContext::kGround,
                       render::kCryArriveDelay - 0.5) == 0);
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 1.0) == 1);
}

TEST_CASE("CryScheduler: the greeting is prompt, not a minutes-long wait") {
    // The train's grace is 40 s because the train is not about you. This one
    // is: arriving at the pump is the event. Guard the intent rather than the
    // literal, so a retune that quietly turned the greeting into a cadence
    // trips here.
    REQUIRE(render::kCryArriveDelay > 0.0);
    REQUIRE(render::kCryArriveDelay < 10.0);
    for (int i = 0; i < render::kCryGapCount; ++i)
        REQUIRE(render::kCryArriveDelay < render::kCryGapsGround[i]);
}

TEST_CASE("CryScheduler: a rider who parks at the pump hears it again") {
    // The whole ground radius is only ~90 s of riding, so a cadence that never
    // produced a second cry would leave the animal as an arrival jingle.
    render::CryScheduler s;
    const int n = cries_over(s, true, render::CryContext::kGround, 300.0);
    CHECK(n >= 3);
    CHECK(n <= 8);
}

TEST_CASE("CryScheduler: from the air it calls less often than on the ground") {
    for (int i = 0; i < render::kCryGapCount; ++i)
        REQUIRE(render::kCryGapsFlying[i] > render::kCryGapsGround[i]);
    render::CryScheduler g, f;
    const int gc = cries_over(g, true, render::CryContext::kGround, 900.0);
    const int fc = cries_over(f, true, render::CryContext::kFlying, 900.0);
    CHECK(gc > fc);
}

TEST_CASE("CryScheduler: one pass overhead yields a cry, not a chorus") {
    // A realistic fly-over: 2 * kCryRangeFlyingM at ~110 m/s.
    //
    // ⚠ RELAXED from `== 1` on 2026-08-24, when the air range went 3200 -> 5000
    // m (see the header: the wolf was never heard because it was never armed).
    // The old number encoded a 58 s pass; the pass is now ~91 s, and the
    // scheduler answers that with a second call 66 s after the greeting. That
    // is not the failure this case exists to catch. The failure is a CHORUS --
    // calls stacked close enough to read as a loop rather than as an animal --
    // so the bound that means something is the SPACING, and it is checked
    // directly below rather than inferred from a count that moves whenever a
    // radius does.
    render::CryScheduler s;
    const double pass_s = 2.0 * render::kCryRangeFlyingM / 110.0;
    const int n = cries_over(s, true, render::CryContext::kFlying, pass_s);
    CHECK(n >= 1);  // the greeting always lands
    CHECK(n <= 2);  // and it is still not a chorus
    // The real anti-chorus property: the shortest possible spacing between two
    // calls is a whole flying gap, which is many times the cry's own length.
    double shortest = render::kCryGapsFlying[0];
    for (int i = 1; i < render::kCryGapCount; ++i)
        shortest = std::min(shortest, render::kCryGapsFlying[i]);
    CHECK(shortest > 5.0 * render::kLongestCryLen);
}

TEST_CASE("CryScheduler: the loiter cadence is not a metronome") {
    // Four uneven gaps, so a parked rider does not hear a clock. Kills a table
    // flattened to a single repeated value.
    bool uneven = false;
    for (int i = 1; i < render::kCryGapCount; ++i)
        uneven = uneven || render::kCryGapsGround[i] !=
                               Approx(render::kCryGapsGround[0]);
    REQUIRE(uneven);
}

TEST_CASE("CryScheduler: leaving does not bank time toward the next cry") {
    // The opposite of TrainAmbience, on purpose: the train runs whether or not
    // you are there, this animal is reacting to YOU. Sitting outside the radius
    // must not fill the gap so that stepping back in cries instantly.
    render::CryScheduler s;
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 5.0) == 1);  // greet
    REQUIRE(cries_over(s, false, render::CryContext::kGround, 600.0) == 0);
    // Back in, and re-armed by that long absence: it greets again after the
    // arrival grace, NOT on the first tick.
    REQUIRE(cries_over(s, true, render::CryContext::kGround,
                       render::kCryArriveDelay - 0.5) == 0);
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 1.0) == 1);
}

TEST_CASE("CryScheduler: riding the boundary does not machine-gun the wolf") {
    // THE case this whole re-arm hold exists for. Circling a pump, or sitting
    // where the distance dithers across the radius, crosses in and out
    // repeatedly. A scheduler that greets on every entry fires a cry per
    // crossing; with the hold, a dither costs nothing.
    render::CryScheduler s;
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 5.0) == 1);
    int n = 0;
    for (int cycle = 0; cycle < 40; ++cycle) {
        n += cries_over(s, false, render::CryContext::kGround, 1.0);
        n += cries_over(s, true, render::CryContext::kGround, 1.0);
    }
    // 80 s of dithering is well under the shortest loiter gap, so the honest
    // answer is zero further cries -- and certainly not one per crossing.
    CHECK(n == 0);
}

TEST_CASE("CryScheduler: a brief step outside does not re-greet you") {
    // Same property stated as a duration: an absence shorter than the re-arm
    // hold resumes the visit rather than starting a new one.
    render::CryScheduler s;
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 5.0) == 1);
    REQUIRE(cries_over(s, false, render::CryContext::kGround,
                       render::kCryReArmS - 2.0) == 0);
    // Re-entering resumes the LOITER gap (>= 41 s), so the arrival grace must
    // not fire here.
    CHECK(cries_over(s, true, render::CryContext::kGround,
                     render::kCryArriveDelay + 1.0) == 0);
}

TEST_CASE("CryScheduler: a real departure does re-arm the greeting") {
    render::CryScheduler s;
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 5.0) == 1);
    REQUIRE(cries_over(s, false, render::CryContext::kGround,
                       render::kCryReArmS + 1.0) == 0);
    CHECK(cries_over(s, true, render::CryContext::kGround,
                     render::kCryArriveDelay + 1.0) == 1);
}

TEST_CASE("CryScheduler: taking off mid-wait re-serves the flying gap") {
    // The context is re-read every tick, not frozen at the last cry -- the same
    // red-team point town_ambience.h records. A rider who banks 40 s at the
    // pump and then takes off must not get a cry almost immediately.
    render::CryScheduler s;
    REQUIRE(cries_over(s, true, render::CryContext::kGround, 5.0) == 1);
    cries_over(s, true, render::CryContext::kGround, 40.0);
    CHECK(cries_over(s, true, render::CryContext::kFlying, 20.0) == 0);
}

TEST_CASE("CryScheduler: a zero or negative frame changes nothing") {
    // Pause, a hitched frame clamped to zero, a debugger step.
    render::CryScheduler s;
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(!s.update(true, render::CryContext::kGround, 0.0));
        REQUIRE(!s.update(true, render::CryContext::kGround, -1.0));
    }
    REQUIRE(s.in_timer == Approx(0.0));
    // ...and the visit is still ungreeted, so it behaves normally afterwards.
    CHECK(cries_over(s, true, render::CryContext::kGround,
                     render::kCryArriveDelay + 1.0) == 1);
}

TEST_CASE("CryScheduler: a long absence cannot run the accumulator away") {
    // The out-timer is HELD at the hold rather than left to grow, so a session
    // parked away from both pumps for hours stays exact.
    render::CryScheduler s;
    cries_over(s, false, render::CryContext::kGround, 8.0 * 3600.0);
    CHECK(s.out_timer == Approx(render::kCryReArmS));
}

TEST_CASE("CryScheduler: a fresh session greets you on first arrival") {
    // out_timer starts at the hold on purpose -- flying straight to a pump from
    // spawn must be greeted, not treated as a re-entry.
    render::CryScheduler s;
    CHECK(cries_over(s, true, render::CryContext::kFlying,
                     render::kCryArriveDelay + 1.0) == 1);
}

TEST_CASE("CryScheduler: every gap outlasts the cry it would interrupt") {
    // Each cry is ONE non-polyphonic raylib Sound, so a re-trigger restarts it.
    // Derived from the header's own asset-length constants, not literals typed
    // here -- a re-cut wav moves ONE number and this follows it.
    for (int i = 0; i < render::kCryGapCount; ++i) {
        REQUIRE(render::kCryGapsGround[i] > render::kLongestCryLen);
        REQUIRE(render::kCryGapsFlying[i] > render::kLongestCryLen);
    }
    // And the re-arm hold, which is what stops a re-entry cry from chopping the
    // cry you left on (the arrival grace is deliberately shorter than an asset
    // and does NOT carry that job).
    REQUIRE(render::kCryReArmS > render::kLongestCryLen);
}

TEST_CASE("cry asset lengths match what the manifest builds") {
    // tools/audio/soundbank.manifest.tsv: wolf.wav cut `full` (5.0 s), and
    // cave_hiss.wav cut `0:7`. Pinned so a re-cut that forgets this header is
    // caught by the gate instead of by a truncated howl.
    REQUIRE(render::kWolfCryLen == Approx(5.0));
    REQUIRE(render::kWildcatCryLen == Approx(7.0));
    REQUIRE(render::kLongestCryLen == Approx(render::kWildcatCryLen));
}

// ---------------------------------------------------------------------------
// LEVEL
// ---------------------------------------------------------------------------

TEST_CASE("cry_gain: loudest at the pump, quietest at the edge, never zero") {
    for (auto which : {render::PumpCry::kWolfSudbury,
                       render::PumpCry::kWildcatValley}) {
        for (auto ctx :
             {render::CryContext::kGround, render::CryContext::kFlying}) {
            const double r = render::cry_range_m(ctx);
            const double at_pump = render::cry_gain(which, 0.0, ctx);
            const double at_half = render::cry_gain(which, r * 0.5, ctx);
            const double at_edge = render::cry_gain(which, r, ctx);
            CHECK(at_pump > at_half);
            CHECK(at_half > at_edge);
            // A cry that arrives silent the moment it becomes possible defeats
            // the arrival cue entirely -- the edge keeps a real floor.
            CHECK(at_edge > 0.2 * at_pump);
            CHECK(at_pump <= render::cry_gain_ceiling(which));
        }
    }
}

TEST_CASE("cry_gain: monotone and bounded across the whole approach") {
    // Kills a fall-off that folds back on itself or leaves [0,1] anywhere.
    for (auto which : {render::PumpCry::kWolfSudbury,
                       render::PumpCry::kWildcatValley}) {
        for (auto ctx :
             {render::CryContext::kGround, render::CryContext::kFlying}) {
            const double r = render::cry_range_m(ctx);
            double prev = render::cry_gain(which, r, ctx);
            for (int i = 200; i >= 0; --i) {
                const double g =
                    render::cry_gain(which, r * i / 200.0, ctx);
                CHECK(g >= 0.0);
                CHECK(g <= render::cry_gain_ceiling(which));
                CHECK(g >= prev - 1e-12);  // closing in never gets quieter
                prev = g;
            }
        }
    }
}

TEST_CASE("cry_gain: clamps outside the range instead of extrapolating") {
    for (auto ctx : {render::CryContext::kGround, render::CryContext::kFlying}) {
        const double r = render::cry_range_m(ctx);
        const double edge =
            render::cry_gain(render::PumpCry::kWolfSudbury, r, ctx);
        CHECK(render::cry_gain(render::PumpCry::kWolfSudbury, r * 10.0, ctx) ==
              Approx(edge));
        const double at_pump =
            render::cry_gain(render::PumpCry::kWolfSudbury, 0.0, ctx);
        CHECK(render::cry_gain(render::PumpCry::kWolfSudbury, -500.0, ctx) ==
              Approx(at_pump));
    }
}

TEST_CASE("cry_gain: from the air it sits lower, as distance rather than cut "
          "through") {
    // Same reasoning as train_gain: an animal you hear clearly at altitude, over
    // the wind bed, stops sounding like an animal a long way off.
    REQUIRE(render::kCryFlyingGain < render::kCryGroundGain);
    for (auto which : {render::PumpCry::kWolfSudbury,
                       render::PumpCry::kWildcatValley})
        CHECK(render::cry_gain(which, 0.0, render::CryContext::kFlying) <
              render::cry_gain(which, 0.0, render::CryContext::kGround));
}

TEST_CASE("cry_gain: the cat is held down by its own true peak") {
    // ⚠ THIS CASE CHANGED MEANING on 2026-08-24 (fly 4), and the old comment is
    // kept because the number it guarded is still doing a second job:
    //
    //   "the cat carries its measured-loudness correction" -- wildcat_cry.wav
    //   verifies 1.0 LU hot against the -24 AMBIENCE target, so it was trimmed
    //   ~1 dB below the wolf. That was the WHOLE reason for the trim at 0.89.
    //
    // At 0.68 the trim is dominated by a different constraint: the cat peaks at
    // -6.6 dBTP against the wolf's -16.6, and at the fly-4 context gain a
    // matched trim would deliver -0.7 dBTP. So this is now a PEAK limit that
    // happens to also cover the loudness correction, and it is checked as one.
    // Moving it up does not make the cat louder; it makes the cat clip.
    REQUIRE(render::kWildcatCryGain < render::kWolfCryGain);
    {
        constexpr double kWildcatPeakDbtp = -6.6;
        const double delivered = render::kGameplayBusGain *
                                 render::kCryGroundGain *
                                 render::kWildcatCryGain;
        INFO("cat delivered peak "
             << kWildcatPeakDbtp + 20.0 * std::log10(delivered) << " dBTP");
        CHECK(kWildcatPeakDbtp + 20.0 * std::log10(delivered) <= -3.0);
        // ...and it is not being trimmed further than the peak requires. A cat
        // ducked well under its own limit would be a taste call wearing a
        // safety argument.
        CHECK(kWildcatPeakDbtp + 20.0 * std::log10(delivered) > -8.0);
    }
    for (auto ctx : {render::CryContext::kGround, render::CryContext::kFlying})
        CHECK(render::cry_gain(render::PumpCry::kWildcatValley, 0.0, ctx) <
              render::cry_gain(render::PumpCry::kWolfSudbury, 0.0, ctx));
}

TEST_CASE("cry_gain: neither animal is loud enough to clip on the bus") {
    // ⚠ CORRECTED 2026-08-24. This case used to read:
    //
    //     CHECK(render::cry_gain(which, 0.0, ctx) <= 1.0);
    //
    // on the stated grounds that SetSoundVolume "has no headroom above unity".
    // That was wrong about raylib, and it was the assertion holding the
    // animals inaudible when Chad said he had never heard the wolf.
    // SetSoundVolume forwards to SetAudioBufferVolume, which is
    // `buffer->volume = volume;` with no bound (raudio.c) -- above 1 amplifies,
    // it does not saturate. The real ceiling is the asset's own true peak.
    //
    // So the check is now against the thing that can actually clip: the
    // delivered peak. build/soundbank_report.txt measures wolf_cry at
    // -16.6 dBTP and wildcat_cry at -6.6 dBTP, and the app multiplies by
    // kGameplayBusGain before the mixer sees it. Checked as the app composes
    // it, with the WORSE of the two assets' headroom applied to both.
    // ⚠ PER-ASSET since 2026-08-24 (fly 4). This used to apply the WORSE of the
    // two assets' peaks to both, which was safe but wrong-headed, and it became
    // actively misleading the moment the two animals stopped sharing a trim:
    // the wolf has 10 dB more headroom than the cat and is now being asked to
    // use it. Applying the cat's peak to the wolf would have capped the wolf
    // for a reason that is not about the wolf.
    constexpr double kWolfPeakDbtp = -16.6;     // sfx/wolf_cry.wav
    constexpr double kWildcatPeakDbtp = -6.6;   // sfx/wildcat_cry.wav
    for (auto which : {render::PumpCry::kWolfSudbury,
                       render::PumpCry::kWildcatValley})
        for (auto ctx :
             {render::CryContext::kGround, render::CryContext::kFlying}) {
            const double g =
                render::kGameplayBusGain * render::cry_gain(which, 0.0, ctx);
            const double asset_peak = which == render::PumpCry::kWolfSudbury
                                          ? kWolfPeakDbtp
                                          : kWildcatPeakDbtp;
            const double peak_dbtp = asset_peak + 20.0 * std::log10(g);
            INFO("delivered peak " << peak_dbtp << " dBTP");
            CHECK(peak_dbtp <= -3.0);  // 3 dB of margin under full scale
            CHECK(render::cry_gain(which, 0.0, ctx) <=
                  render::cry_gain_ceiling(which));
        }
}

// ⚠ RETRACTED 2026-08-24, left visible rather than deleted, exactly as the
// sled-vs-gun static_assert in render/mix_levels.h was:
//
//   TEST_CASE("cry_gain: the animals sit under the music, not over it")
//       ... CHECK(cry delivered LUFS < bed delivered LUFS);
//
// It asserted that an animal cry must arrive QUIETER than the music bed, on
// the argument that AMBIENCE is the furthest layer back and a cry over the bed
// would be a jump-scare. That was an argument about what an ambience deserves.
// Chad has now heard the cat and ruled the other way in one sentence -- bed
// down, cries up -- and his ear outranks the argument, the same way his ride
// outranked the assertion that a vehicle engine must sit above the guns.
//
// It was ALSO, in its first form, the more ordinary kind of wrong: it compared
// linear gains across assets mastered 4 LU apart. Fixing the units (earlier
// today) kept it alive one more fly. Fixing the units did not make the
// PROPERTY his.
//
// What survives is the property that is not a matter of taste -- the cries
// must not clip -- and it is checked in the case above, against measured true
// peak. There is no lower bound here on purpose: "loud enough" is an ear
// judgement, and this file has now twice been the thing standing between Chad
// and hearing an animal at all.
TEST_CASE("cry_gain: the animals stay in the mix, without a taste claim") {
    // AMBIENCE is the furthest layer back (-24 LUFS, the manifest's own
    // policy). An arrival cue that arrived louder than the bed would be a
    // jump-scare rather than the country being alive, and the mastering target
    // alone does not guarantee that once a trim is applied on top of it.
    for (auto which : {render::PumpCry::kWolfSudbury,
                       render::PumpCry::kWildcatValley})
        for (auto ctx :
             {render::CryContext::kGround, render::CryContext::kFlying})
            {
                // Delivered loudness, kept as a REPORT rather than a bound --
                // the number a future ear-ruling wants to see, printed where
                // it cannot go stale. Targets from
                // tools/audio/soundbank.manifest.tsv.
                constexpr double kCryTargetLufs = -24.0;    // AMBIENCE
                constexpr double kMusicTargetLufs = -20.0;  // MUSIC
                const double cry_out =
                    kCryTargetLufs +
                    20.0 * std::log10(render::kGameplayBusGain *
                                      render::cry_gain(which, 0.0, ctx));
                const double music_out =
                    kMusicTargetLufs +
                    20.0 * std::log10(render::kMusicSurfaceGain);
                INFO("cry " << cry_out << " LUFS vs bed " << music_out);
                // The only claim left is that the channel EXISTS in the mix:
                // audible against the bed rather than buried 20 dB under it,
                // and not so far over it that the bed has stopped being a bed.
                // Both bounds are wide on purpose -- taste lives in the dials.
                // Fly 4 moved these ABOVE the bed on Chad's ruling that an
                // environmental effect is "not very immersive if not quite a
                // bit louder than the music". Deliberately NOT pinned in
                // either direction -- that is the taste this file has twice
                // been caught encoding. Wide sanity only.
                CHECK(cry_out > music_out - 20.0);
                CHECK(cry_out < music_out + 20.0);
            }
}
