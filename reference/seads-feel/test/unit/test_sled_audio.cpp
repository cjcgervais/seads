// Unit tests for the snowmachine engine audio (Polaris Indy 650) and the
// town-ambience train. render/sled_audio.h, sled_drive.h, sled_synth.h,
// town_ambience.h -- all pure, no raylib, no assets on disk.
//
// What this CANNOT test: whether it sounds like a sled. The gate never runs the
// exe. This pins BEHAVIOUR (a tap is a brap, a hold rides, the idle bed is
// really audible) and the derivation of the tone; the ear judges the rest.
//
// Several tests are shaped oddly on purpose, because a red-team proved the
// previous versions vacuous: a synth emitting one DC sample forever passed both
// idle tests, and the RUN and TONE render paths could be deleted outright with
// the suite still green.

#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render/sled_audio.h"
#include "render/sled_drive.h"
#include "render/sled_synth.h"
#include "render/mix_levels.h"  // kGameplayBusGain -- the bus the train rides
#include "render/town_ambience.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Approx;

namespace {
constexpr double kDt = 1.0 / 60.0;
constexpr double kPi = 3.14159265358979323846;

int run(render::SledDrive& d, double throttle, double secs) {
    int n_on = 0;
    const int n = static_cast<int>(secs / kDt);
    for (int i = 0; i < n; ++i)
        if (d.update(throttle, kDt)) ++n_on;
    return n_on;
}

// A synthetic "recording": a steady tone, so a layer can be shown AUDIBLE
// rather than merely wired.
std::vector<float> tone_bank(double hz, double secs, double sr, double amp = 0.5) {
    const std::size_t n = static_cast<std::size_t>(secs * sr);
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = static_cast<float>(amp * std::sin(2.0 * kPi * hz * i / sr));
    return v;
}

double rms(const short* b, int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = b[i] / 32768.0;
        s += x * x;
    }
    return std::sqrt(s / n);
}

// Sign changes across a buffer. A frozen cursor emitting DC scores 0 -- this is
// what separates "audible" from "stuck on one sample", which RMS cannot.
int sign_changes(const short* b, int n) {
    int c = 0;
    for (int i = 1; i < n; ++i)
        if ((b[i - 1] < 0) != (b[i] < 0)) ++c;
    return c;
}

// How many sign changes a `hz` tone bank SHOULD produce in `n` output samples
// when played back at `ratio`, halved to leave slack for the resampler's edges.
//
// ⚠ This exists because the sign-change assertions below are COUPLED TO THE
// PLAYBACK PITCH, and a hard-coded threshold hides that. They were written as
// `> 4` against a 90 Hz bank at ratio 1.0 (~8 changes per 1024-sample buffer,
// a 2x margin). Chad's first-ride octave drop (kSledPitchPedestal = 0.5) took
// the same bank to 45 Hz and exactly 4 changes -- so two tests failed on a
// pitch change that was entirely correct, while still asserting nothing about
// the property they exist for.
//
// That property is "a real waveform, not held DC" -- a frozen cursor emitting
// one sample forever clears any RMS floor, and only this catches it. Deriving
// the threshold keeps that intact at any pedestal (halve it again for the
// second octave and these follow) instead of pinning an unrelated constant.
double min_sign_changes(double hz, double ratio, int n, double sr) {
    return 0.5 * (2.0 * hz * ratio * static_cast<double>(n) / sr);
}

void settle(render::SledSynth& s, double throttle, double secs, short* buf,
            int n) {
    const int frames = static_cast<int>(secs / kDt);
    for (int i = 0; i < frames; ++i) {
        s.set_drivers(throttle, kDt);
        s.render(buf, n);
    }
}
}  // namespace

// ---------------------------------------------------------------------------
// The tone -- derived from the real engine, not picked by ear
// ---------------------------------------------------------------------------

TEST_CASE("sled_fire_hz: a three-cylinder two-stroke fires once per rev each") {
    REQUIRE(render::sled_fire_hz(0.0) == Approx(60.0));
    REQUIRE(render::sled_fire_hz(1.0) == Approx(350.0));
}

TEST_CASE("sled_fire_hz: rises monotonically with revs") {
    double prev = render::sled_fire_hz(0.0);
    for (int i = 1; i <= 50; ++i) {
        const double f = render::sled_fire_hz(i / 50.0);
        REQUIRE(f > prev);
        prev = f;
    }
}

TEST_CASE("sled tone: stays below the aircraft drone's register") {
    REQUIRE(render::sled_fire_hz(1.0) < 400.0);
}

TEST_CASE("sled_tone_cutoff: opens as the engine loads up") {
    // The endpoints carry kSledVoiceTranspose, like the fundamental they filter
    // -- a transposed instrument moves its whole spectrum, so the brightness
    // sweep comes down with the pitch. Stated as the constant times the
    // transpose rather than as bare kSledToneCutoff*, so a future octave moves
    // one number and this follows instead of failing on a correct change.
    REQUIRE(render::sled_tone_cutoff(0.0) ==
            Approx(render::kSledToneCutoffIdle * render::kSledVoiceTranspose));
    REQUIRE(render::sled_tone_cutoff(1.0) ==
            Approx(render::kSledToneCutoffMax * render::kSledVoiceTranspose));
    // The property the case is really for, and the one that is transpose-free.
    REQUIRE(render::sled_tone_cutoff(0.5) > render::sled_tone_cutoff(0.2));
}

// ---------------------------------------------------------------------------
// A TAP is a brap. A HOLD rides.
// ---------------------------------------------------------------------------

TEST_CASE("SledDrive: a tap fires exactly one accent") {
    render::SledDrive d;
    REQUIRE(run(d, 1.0, 0.10) == 1);
}

TEST_CASE("SledDrive: holding the throttle does NOT re-fire accents") {
    render::SledDrive d;
    REQUIRE(run(d, 1.0, 5.0) == 1);
}

TEST_CASE("SledDrive: four taps give four accents") {
    render::SledDrive d;
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        n += run(d, 1.0, 0.12);
        n += run(d, 0.0, 0.15);
    }
    REQUIRE(n == 4);
}

TEST_CASE("SledDrive: the throttle gate is HYSTERETIC and cannot chatter") {
    // CLAUDE.md hard rule: every gate leg is hysteretic; shared thresholds
    // chatter. An analog throttle rippling at the edge must not machine-gun
    // restarted sample attacks.
    REQUIRE(render::kSledOnsetOff < render::kSledOnsetOn);
    render::SledDrive d;
    const double mid = 0.5 * (render::kSledOnsetOn + render::kSledOnsetOff);
    int accents = 0;
    for (int i = 0; i < 600; ++i) {
        const double t = mid + 0.03 * std::sin(i * 0.7);  // never clears both
        if (d.update(t, kDt)) ++accents;
    }
    REQUIRE(accents <= 1);
}

TEST_CASE("SledDrive: a tap produces no sustained tone") {
    render::SledDrive d;
    run(d, 1.0, 0.10);
    REQUIRE(d.tone_level() == Approx(0.0));
    REQUIRE(d.accent_level() > 0.0);
}

TEST_CASE("SledDrive: a held throttle does sing") {
    render::SledDrive d;
    run(d, 1.0, 2.0);
    REQUIRE(d.tone_level() > 0.0);
}

TEST_CASE("SledDrive: repeated taps RAMP UP rather than repeating identically") {
    REQUIRE(render::kSledRevDownTau > render::kSledRevUpTau);
    render::SledDrive d;
    double revs_at_tap[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        run(d, 1.0, 0.12);
        revs_at_tap[i] = d.revs;
        run(d, 0.0, 0.12);
    }
    REQUIRE(revs_at_tap[1] > revs_at_tap[0]);
    REQUIRE(revs_at_tap[2] > revs_at_tap[1]);
    REQUIRE(revs_at_tap[3] > revs_at_tap[2]);
}

TEST_CASE("SledDrive: an onset is LATCHED, so a sub-buffer tap is never lost") {
    // THE defect the latch exists for. The synth renders 1024 frames at a time
    // (46 ms); a 40 ms keypress opens and closes entirely between two renders.
    // Sampling the throttle LEVEL at buffer rate sees nothing on either side
    // and drops the tap -- precisely Chad's "tapping keys gives the brap brap".
    render::SledDrive d;
    d.update(1.0, 0.02);  // key down
    d.update(0.0, 0.02);  // key up, all inside one audio buffer
    REQUIRE(d.pending_accents == 1);
    REQUIRE(d.take_accents() == 1);
    REQUIRE(d.take_accents() == 0);  // consumed exactly once
}

TEST_CASE("SledDrive: a burst of taps inside one buffer is not collapsed to one") {
    render::SledDrive d;
    for (int i = 0; i < 3; ++i) {
        d.update(1.0, 0.05);
        d.update(0.0, 0.05);
    }
    REQUIRE(d.take_accents() == 3);
}

TEST_CASE("SledDrive: accents respect a refractory so the pipe can re-load") {
    render::SledDrive d;
    d.update(1.0, 0.001);
    d.update(0.0, 0.001);
    d.update(1.0, 0.001);  // absurdly fast re-stab
    d.update(0.0, 0.001);
    REQUIRE(d.take_accents() < 3);
}

// ---------------------------------------------------------------------------
// Layer balance and bounds
// ---------------------------------------------------------------------------

TEST_CASE("sled_idle_level: owns the voice at rest and retreats under power") {
    REQUIRE(render::sled_idle_level(0.0) == Approx(render::kSledIdleGain));
    REQUIRE(render::sled_idle_level(1.0) == Approx(0.0));
    double prev = render::sled_idle_level(0.0);
    for (int i = 1; i <= 40; ++i) {
        const double g = render::sled_idle_level(i / 40.0);
        REQUIRE(g <= prev + 1e-12);
        prev = g;
    }
}

TEST_CASE("sled_run_level: the riding bed rises as the idle bed retreats") {
    REQUIRE(render::sled_run_level(0.0) == Approx(0.0));
    REQUIRE(render::sled_run_level(1.0) > 0.0);
    double prev = render::sled_run_level(0.0);
    for (int i = 1; i <= 40; ++i) {
        const double g = render::sled_run_level(i / 40.0);
        REQUIRE(g >= prev - 1e-12);
        prev = g;
    }
}

TEST_CASE("sled_accent_level: decays to nothing over its life") {
    REQUIRE(render::sled_accent_level(-1.0) == Approx(0.0));
    REQUIRE(render::sled_accent_level(0.0) == Approx(0.0));
    REQUIRE(render::sled_accent_level(0.01) > 0.0);
    REQUIRE(render::sled_accent_level(render::kSledAccentLife) == Approx(0.0));
    REQUIRE(render::sled_accent_level(render::kSledAccentLife + 5.0) == Approx(0.0));
}

TEST_CASE("sled balance: the beds top the tone, and the engine sits under unity") {
    REQUIRE(render::kSledRunGain > render::kSledToneGain);
    REQUIRE(render::kSledIdleGain > render::kSledToneGain);
    REQUIRE(render::kSledMaster < 1.0);
}

TEST_CASE("engine pitch: every layer sings at the SAME transposed pitch") {
    // ⚠ THIS CASE USED TO ASSERT AGAINST sled_fire_hz() -- the raw physical
    // firing rate -- on the argument that landing the recordings on the real
    // rpm/60*3 was self-evidently correct. Chad rode that and said it still
    // read an octave high, so the assertion was encoding MY reasoning, not a
    // property of working code. Same failure as the static_assert retracted in
    // mix_levels.h the same afternoon, and rewritten rather than relaxed.
    //
    // The property that actually has to hold is COHERENCE: whatever pitch the
    // machine has been transposed to, all three layers must be at it, or the
    // sled comes apart into two engines a fifth away from each other. That is
    // structural, it survives any future octave, and it is what is checked
    // here -- against sled_voice_hz(), the SOUNDED reference.
    const double kIdleSourceHz = 140.0;  // sled_idle_loop.wav, 135-150 band
    const double kRunSourceHz = 517.0;   // sled_run.wav, peak of the 460-520 band

    render::SledDrive d;
    d.revs = 0.0;
    // At rest the idle bed is the only sampled layer with any level.
    REQUIRE(std::abs(std::log2(kIdleSourceHz * d.idle_ratio() /
                               render::sled_voice_hz(0.0))) < 0.34);
    d.revs = 1.0;
    // Pinned, the riding bed is the one carrying it.
    REQUIRE(std::abs(std::log2(kRunSourceHz * d.run_ratio() /
                               render::sled_voice_hz(1.0))) < 0.34);

    // The physics itself is untouched and still stated exactly -- the whole
    // point of keeping sled_fire_hz and sled_voice_hz as two functions is that
    // the derivation stays readable next to the taste applied on top of it.
    REQUIRE(render::sled_fire_hz(0.0) == Approx(60.0));   // 1200 rpm /60 *3
    REQUIRE(render::sled_fire_hz(1.0) == Approx(350.0));  // 7000 rpm /60 *3
    REQUIRE(render::sled_voice_hz(1.0) ==
            Approx(350.0 * render::kSledVoiceTranspose));
}

TEST_CASE("engine pitch: the transposition moves TIMBRE, not just pitch") {
    // Resampling a recording moves its whole spectrum for free. The synth tone
    // does not get that for free: drop its fundamental and leave the lowpass
    // where it was, and you have a thin bright harmonic stack over a low note,
    // which reads high however low f0 goes. This is the assertion that stops
    // the tone becoming the layer that reads high again.
    for (double r : {0.0, 0.5, 1.0}) {
        const double cut = render::sled_tone_cutoff(r);
        const double base = render::kSledToneCutoffIdle +
                            (render::kSledToneCutoffMax -
                             render::kSledToneCutoffIdle) * r;
        REQUIRE(cut == Approx(base * render::kSledVoiceTranspose));
    }
    // ...and the cutoff must still clear the fundamental it is filtering, or
    // the tone is lowpassed into silence rather than darkened.
    for (double r : {0.0, 0.5, 1.0})
        REQUIRE(render::sled_tone_cutoff(r) > 2.0 * render::sled_voice_hz(r));
}

TEST_CASE("engine pitch: the two constants keep their separate jobs") {
    // One measured, one ruled. Folding them into a single dial is how the
    // first pass ended up unable to answer "which of these is the taste?".
    REQUIRE(render::kSledPitchPedestal ==
            Approx(render::kSledSourceCorrection *
                   render::kSledVoiceTranspose));
    // The source correction is a measurement of the recordings and should not
    // be moved by an ear ruling; the transpose is the one that moves.
    REQUIRE(render::kSledSourceCorrection > 0.0);
    REQUIRE(render::kSledSourceCorrection <= 1.0);
    REQUIRE(render::kSledVoiceTranspose > 0.0);
    REQUIRE(render::kSledVoiceTranspose <= 1.0);
}

TEST_CASE("pitch pedestal: it is a pedestal, not a re-range") {
    // It must scale both ratios WHOLE, so each layer's sweep across the rev
    // range survives. A retune that moved the offset instead would flatten the
    // rev cue -- the thing the whole sample-based-but-rpm-coupled design is
    // for -- while still passing a "is it lower" check.
    render::SledDrive d;
    d.revs = 0.0;
    const double idle_lo = d.idle_ratio(), run_lo = d.run_ratio();
    d.revs = 1.0;
    const double idle_hi = d.idle_ratio(), run_hi = d.run_ratio();
    REQUIRE(idle_hi / idle_lo == Approx(1.35));
    REQUIRE(run_hi / run_lo == Approx(1.875));
    // ...and the floor really did move by the pedestal.
    REQUIRE(idle_lo == Approx(render::kSledPitchPedestal));
}

TEST_CASE("SledDrive: playback ratios rise with revs and stay resamplable") {
    // Monotonicity is the point -- a constant ratio sits inside the bounds
    // while deleting the rev-pitch ramp entirely.
    render::SledDrive d;
    d.revs = 0.0;
    const double idle_lo = d.idle_ratio(), run_lo = d.run_ratio();
    d.revs = 1.0;
    REQUIRE(d.idle_ratio() > idle_lo);
    REQUIRE(d.run_ratio() > run_lo);
    for (int i = 0; i <= 100; ++i) {
        d.revs = i / 100.0;
        // Bounds DERIVED from the pedestal, not typed as literals: Chad can
        // halve kSledPitchPedestal again for his second octave and these
        // follow him instead of failing. (They were 0.5/2.0 flat, which the
        // first octave drop broke immediately.)
        REQUIRE(d.idle_ratio() > 0.9 * render::kSledPitchPedestal);
        REQUIRE(d.idle_ratio() < 2.0 * render::kSledPitchPedestal);
        REQUIRE(d.run_ratio() > 0.7 * render::kSledPitchPedestal);
        REQUIRE(d.run_ratio() < 2.0 * render::kSledPitchPedestal);
    }
}

TEST_CASE("SledDrive: survives dt=0 and a hitched frame without leaving bounds") {
    render::SledDrive d;
    d.update(1.0, 0.0);
    REQUIRE(d.revs >= 0.0);
    d.update(1.0, 5.0);
    REQUIRE(d.revs >= 0.0);
    REQUIRE(d.revs <= 1.0);
    REQUIRE(d.idle_level() >= 0.0);
    REQUIRE(d.tone_level() >= 0.0);
}

// ---------------------------------------------------------------------------
// SledSynth -- the layer that actually plays the WAVs
// ---------------------------------------------------------------------------

TEST_CASE("SledSynth: a missing idle bank leaves the channel silent, not broken") {
    render::SledSynth s;
    s.init(22050.0, nullptr, 0, 1, 44100.0, nullptr, 0, 1, 44100.0);
    REQUIRE(!s.ok());
    short buf[256];
    s.render(buf, 256);
    for (int i = 0; i < 256; ++i) REQUIRE(buf[i] == 0);
}

TEST_CASE("SledSynth: THE IDLE LOOP IS AUDIBLE AT REST") {
    // The direct check on Chad's instruction: "Idle engine indy 650 for idle".
    // At zero throttle the idle bank is the only live layer.
    //
    // RMS alone is not enough: a frozen cursor emitting one DC sample forever
    // clears any RMS floor. Requiring sign changes is what makes this fail when
    // the cursor stops advancing.
    const std::vector<float> idle = tone_bank(90.0, 1.0, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, nullptr, 0, 1, 44100.0);
    REQUIRE(s.ok());

    short buf[1024];
    settle(s, 0.0, 0.5, buf, 1024);
    REQUIRE(rms(buf, 1024) > 0.01);
    // A real waveform, not held DC. At rest idle_ratio() is the bare pedestal.
    REQUIRE(sign_changes(buf, 1024) >
            min_sign_changes(90.0, render::kSledPitchPedestal, 1024, 22050.0));
}

TEST_CASE("SledSynth: the idle cursor keeps advancing across many wraps") {
    const std::vector<float> idle = tone_bank(90.0, 0.2, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, nullptr, 0, 1, 44100.0);
    short buf[1024];
    for (int b = 0; b < 400; ++b) {
        s.set_drivers(0.0, kDt);
        s.render(buf, 1024);
        REQUIRE(s.idle_pos_ >= 0.0);
        REQUIRE(s.idle_pos_ < static_cast<double>(s.idle_.size()));
    }
    REQUIRE(rms(buf, 1024) > 0.01);
    REQUIRE(sign_changes(buf, 1024) >
            min_sign_changes(90.0, render::kSledPitchPedestal, 1024, 22050.0));
}

TEST_CASE("SledSynth: THE RIDING BED IS AUDIBLE WHILE RIDING") {
    // Chad: "the riding is the other engine snowmachine brap brap sounds".
    // At full revs the idle bed has handed over, so with the tone silenced too,
    // whatever remains IS the riding recording. An earlier version windowed
    // this layer to 0.85 s of a 7.37 s asset, leaving only the synth drone
    // while riding -- this fails if the run block is deleted or re-truncated.
    const std::vector<float> idle = tone_bank(90.0, 0.5, 44100.0);
    const std::vector<float> ride = tone_bank(220.0, 0.5, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, ride.data(),
           ride.size(), 1, 44100.0);
    short buf[1024];
    settle(s, 1.0, 4.0, buf, 1024);  // long hold, well past any accent envelope

    REQUIRE(s.drive.idle_level() == Approx(0.0));  // idle has handed over
    s.drive.sustain = 0.0;                         // silence the tone only
    for (int b = 0; b < 40; ++b) s.render(buf, 1024);  // let the slew settle

    REQUIRE(rms(buf, 1024) > 0.01);
    REQUIRE(sign_changes(buf, 1024) >
            min_sign_changes(220.0, render::kSledPitchPedestal * 0.8, 1024,
                             22050.0));
}

TEST_CASE("SledSynth: THE TONE IS AUDIBLE WHEN THE THROTTLE IS HELD") {
    // With no riding bank supplied and revs pinned (idle retreated to 0), the
    // tone is the only possible source. Compared against the same state with
    // the sustain knocked below its knee, so the difference IS the tone block.
    const std::vector<float> idle = tone_bank(90.0, 0.5, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, nullptr, 0, 1, 44100.0);
    short buf[1024];

    settle(s, 1.0, 4.0, buf, 1024);
    REQUIRE(s.drive.tone_level() > 0.0);
    for (int b = 0; b < 40; ++b) s.render(buf, 1024);
    const double with_tone = rms(buf, 1024);

    s.drive.sustain = 0.0;  // below the knee -> tone target 0
    for (int b = 0; b < 40; ++b) s.render(buf, 1024);
    const double without = rms(buf, 1024);

    REQUIRE(with_tone > 0.005);
    REQUIRE(with_tone > without * 1.5);
}

TEST_CASE("SledSynth: the tone's partials each keep their own phase") {
    // One shared phase wrapped at the fundamental is continuous only for k=1;
    // every detuned partial gets a step, which aliases AND destroys the beating
    // the detune exists to create. So the per-harmonic phases must diverge.
    const std::vector<float> idle = tone_bank(90.0, 0.5, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, nullptr, 0, 1, 44100.0);
    short buf[1024];
    settle(s, 1.0, 3.0, buf, 1024);

    bool any_different = false;
    for (int k = 1; k < render::kSledToneHarmonics; ++k)
        if (std::fabs(s.hphase_[k] - s.hphase_[0]) > 1e-6) any_different = true;
    REQUIRE(any_different);
    for (int k = 0; k < render::kSledToneHarmonics; ++k) {
        REQUIRE(s.hphase_[k] >= 0.0);
        REQUIRE(s.hphase_[k] < 1.0);
    }
}

TEST_CASE("SledSynth: layer gains slew instead of stepping at buffer edges") {
    // The loudest layer used to step ~0.055 per buffer through its decay -- a
    // click train riding every bark. Gains are now interpolated across the
    // buffer, so the applied value must ARRIVE at the target over the buffer
    // rather than jumping to it on sample 0.
    const std::vector<float> idle = tone_bank(90.0, 0.5, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, nullptr, 0, 1, 44100.0);
    short buf[64];
    s.set_drivers(0.0, kDt);
    s.render(buf, 64);
    REQUIRE(s.g_idle_ == Approx(render::kSledIdleGain).margin(1e-9));
    double first = 0.0, last = 0.0;
    for (int i = 0; i < 8; ++i) first += std::fabs(buf[i] / 32768.0);
    for (int i = 56; i < 64; ++i) last += std::fabs(buf[i] / 32768.0);
    REQUIRE(last > first);  // the ramp is visible in the samples
}

TEST_CASE("SledSynth: a stereo source is AVERAGED, not just length-converted") {
    // The old fixture wrote identical data to both channels, so "left only" and
    // "sum without averaging" both passed. Anti-phase channels make the
    // downmix arithmetic observable: a correct average cancels to ~0.
    const std::size_t frames = 4410;
    std::vector<float> st(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        const double v = 0.4 * std::sin(2.0 * kPi * 90.0 * i / 44100.0);
        st[i * 2] = static_cast<float>(v);
        st[i * 2 + 1] = static_cast<float>(-v);
    }
    render::SledSynth s;
    s.init(22050.0, st.data(), frames, 2, 44100.0, nullptr, 0, 1, 44100.0);
    REQUIRE(s.ok());
    REQUIRE(s.idle_.size() > 2000);
    REQUIRE(s.idle_.size() < 2400);
    double peak = 0.0;
    for (float v : s.idle_) peak = std::max(peak, static_cast<double>(std::fabs(v)));
    REQUIRE(peak < 0.02);  // a left-only downmix would read ~0.4
}

TEST_CASE("SledSynth: the mix keeps real headroom under the limiter") {
    // NOT "does it exceed int16" -- soft_clip returns strictly under 1.0 for
    // every finite input, so that form is a tautology that a x100 mix passes.
    // The useful question is whether the limiter is being leaned on.
    const std::vector<float> idle = tone_bank(90.0, 0.5, 44100.0);
    const std::vector<float> ride = tone_bank(220.0, 0.5, 44100.0);
    render::SledSynth s;
    s.init(22050.0, idle.data(), idle.size(), 1, 44100.0, ride.data(),
           ride.size(), 1, 44100.0);
    short buf[1024];
    double peak = 0.0;
    for (int b = 0; b < 240; ++b) {
        s.set_drivers(1.0, kDt);
        s.render(buf, 1024);
        for (int i = 0; i < 1024; ++i)
            peak = std::max(peak, std::fabs(buf[i] / 32768.0));
    }
    REQUIRE(peak > 0.02);  // non-vacuity: something is actually playing
    REQUIRE(peak < 0.95);  // and the limiter is not what shapes the mix
}

// ---------------------------------------------------------------------------
// Town ambience -- the distant Chelmsford train
// ---------------------------------------------------------------------------

TEST_CASE("is_town: a farmstead is not a town, a cluster is") {
    REQUIRE(!render::is_town(0));
    REQUIRE(!render::is_town(render::kTownPrismCount - 1));
    REQUIRE(render::is_town(render::kTownPrismCount));
    REQUIRE(render::is_town(500));
}

TEST_CASE("TrainAmbience: never fires away from a town, however long you wait") {
    render::TrainAmbience t;
    for (int i = 0; i < 60 * 3600; ++i)
        REQUIRE(!t.update(false, render::TownContext::kGround, kDt));
}

TEST_CASE("TrainAmbience: holds a grace period before the first one") {
    render::TrainAmbience t;
    bool fired = false;
    const int grace = static_cast<int>(render::kTrainFirstGap * 60.0) - 2;
    for (int i = 0; i < grace; ++i)
        fired = fired || t.update(true, render::TownContext::kGround, kDt);
    REQUIRE(!fired);
    for (int i = 0; i < 10; ++i)
        fired = fired || t.update(true, render::TownContext::kGround, kDt);
    REQUIRE(fired);
}

TEST_CASE("TrainAmbience: on the ground it lands every couple of minutes") {
    render::TrainAmbience t;
    int fires = 0;
    for (int i = 0; i < 60 * 600; ++i)
        if (t.update(true, render::TownContext::kGround, kDt)) ++fires;
    REQUIRE(fires >= 3);
    REQUIRE(fires <= 6);
}

TEST_CASE("TrainAmbience: from the air it is rarer than on the ground") {
    // Still rarer, but no longer 2.4x rarer -- the air is already rare by
    // construction, because the clock only runs while you are over the town
    // and an overflight is short. See the re-cut note in town_ambience.h.
    for (int i = 0; i < render::kTrainGapCount; ++i)
        REQUIRE(render::kTrainGapsFlying[i] > render::kTrainGapsGround[i]);
    render::TrainAmbience g, f;
    int gc = 0, fc = 0;
    for (int i = 0; i < 60 * 900; ++i) {
        if (g.update(true, render::TownContext::kGround, kDt)) ++gc;
        if (f.update(true, render::TownContext::kFlying, kDt)) ++fc;
    }
    REQUIRE(gc > fc);
}

TEST_CASE("TrainAmbience: leaving town pauses the clock, it does not reset it") {
    render::TrainAmbience t;
    for (int i = 0; i < 60 * 30; ++i) t.update(true, render::TownContext::kGround, kDt);
    const double banked = t.timer;
    REQUIRE(banked > 0.0);
    for (int i = 0; i < 60 * 120; ++i) t.update(false, render::TownContext::kGround, kDt);
    REQUIRE(t.timer == Approx(banked));
}

TEST_CASE("TrainAmbience: taking off mid-wait re-serves the flying gap") {
    // The cadence must follow where you ARE, not where you were when the last
    // one fired -- otherwise a rider who banks time in town then takes off
    // hears the train almost immediately from the air, against the "sometimes"
    // Chad asked for.
    render::TrainAmbience t;
    for (int i = 0; i < 60 * 100; ++i) t.update(true, render::TownContext::kGround, kDt);
    bool soon = false;
    for (int i = 0; i < 60 * 30; ++i)
        soon = soon || t.update(true, render::TownContext::kFlying, kDt);
    REQUIRE(!soon);
}

TEST_CASE("TrainAmbience: every gap clears the shipped asset's length") {
    // Derived from the header's own constant, not a number typed here -- a
    // re-cut asset moves ONE value and this follows it, or the pass truncates
    // itself on re-trigger (the repo's config-relative-bounds rule).
    REQUIRE(render::kTrainFirstGap > render::kTrainPassLen);
    for (int i = 0; i < render::kTrainGapCount; ++i) {
        REQUIRE(render::kTrainGapsGround[i] > render::kTrainPassLen);
        REQUIRE(render::kTrainGapsFlying[i] > render::kTrainPassLen);
    }
}

TEST_CASE("train_gain: quieter from the air, and clear of clipping") {
    REQUIRE(render::train_gain(render::TownContext::kFlying) <
            render::train_gain(render::TownContext::kGround));
    REQUIRE(render::train_gain(render::TownContext::kFlying) > 0.0);
    // ⚠ The old `<= 1.0` here went the same way as its twin in
    // test_pump_ambience.cpp on 2026-08-24: raylib's SetSoundVolume does not
    // clamp (SetAudioBufferVolume is a bare assignment, raudio.c), so unity
    // was never the ceiling -- the asset's true peak is. The train measures
    // -11.8 dBTP (build/soundbank_report.txt) and the app multiplies by
    // kGameplayBusGain before the mixer sees it.
    constexpr double kTrainPeakDbtp = -11.8;
    for (auto ctx : {render::TownContext::kGround, render::TownContext::kFlying}) {
        const double delivered =
            kTrainPeakDbtp +
            20.0 * std::log10(render::kGameplayBusGain * render::train_gain(ctx));
        INFO("delivered peak " << delivered << " dBTP");
        CHECK(delivered <= -3.0);
    }
}

TEST_CASE("town radius: the air looks much wider than the ground, and the "
          "threshold scales with it") {
    // The bug this pair exists to prevent is the one that shipped: a
    // neighbourhood the size of one ~92 m index cell is a sub-second event at
    // flying speed, so the clock never accumulates and the train never fires
    // from the air however many times you cross Chelmsford.
    REQUIRE(render::town_radius_m(render::TownContext::kFlying) >
            render::town_radius_m(render::TownContext::kGround));
    // A pass at 90-120 m/s must be worth a real fraction of a gap, not a
    // rounding error. Diameter / speed against the SHORTEST flying gap.
    const double pass_s =
        2.0 * render::town_radius_m(render::TownContext::kFlying) / 120.0;
    double shortest = render::kTrainGapsFlying[0];
    for (int i = 1; i < render::kTrainGapCount; ++i)
        shortest = std::min(shortest, render::kTrainGapsFlying[i]);
    CHECK(pass_s > 0.25 * shortest);
    // ...but a wider circle must not turn two farmsteads into a town.
    REQUIRE(render::town_prism_count(render::TownContext::kFlying) >
            render::town_prism_count(render::TownContext::kGround));
    REQUIRE(render::town_prism_count(render::TownContext::kGround) ==
            render::kTownPrismCount);
}

TEST_CASE("is_town: the context-aware form agrees with the ground-only one") {
    REQUIRE(!render::is_town(0, render::TownContext::kGround));
    REQUIRE(!render::is_town(0, render::TownContext::kFlying));
    REQUIRE(render::is_town(render::kTownPrismCount,
                            render::TownContext::kGround));
    // The same count that is a town on the ground is NOT one from the air.
    REQUIRE(!render::is_town(render::kTownPrismCount,
                             render::TownContext::kFlying));
    REQUIRE(render::is_town(render::kTownPrismCountFlying,
                            render::TownContext::kFlying));
    for (int n = 0; n < 200; ++n)
        CHECK(render::is_town(n, render::TownContext::kGround) ==
              render::is_town(n));
}
