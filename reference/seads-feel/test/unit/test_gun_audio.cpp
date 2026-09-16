// Unit tests for the render-side gun / combat-SFX audio layer.
// Pure mapping tests (gun_audio.h) + real-time synth bounds (gun_synth.h).
// Cosmetic only — no sim/control/input deps.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "render/gun_audio.h"
#include "render/gun_synth.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

// ---------------------------------------------------------------------------
// gun_shot_amp
// ---------------------------------------------------------------------------
TEST_CASE("gun_shot_amp: ==1 at age 0") {
    REQUIRE(render::gun_shot_amp(0.0, render::kGunCannonCrackTau) == Catch::Approx(1.0));
    REQUIRE(render::gun_shot_amp(0.0, render::kGunMGCrackTau) == Catch::Approx(1.0));
}

TEST_CASE("gun_shot_amp: 0 for negative age") {
    REQUIRE(render::gun_shot_amp(-0.001, render::kGunCannonCrackTau) == Catch::Approx(0.0));
    REQUIRE(render::gun_shot_amp(-1.0, render::kGunMGCrackTau) == Catch::Approx(0.0));
}

TEST_CASE("gun_shot_amp: strictly decreasing for positive age") {
    const double tau = render::kGunCannonCrackTau;
    double prev = render::gun_shot_amp(0.0, tau);
    for (int i = 1; i <= 10; ++i) {
        double age = static_cast<double>(i) * 0.005;
        double cur = render::gun_shot_amp(age, tau);
        REQUIRE(cur < prev);
        prev = cur;
    }
}

TEST_CASE("gun_shot_amp: always in [0,1]") {
    const double tau = render::kGunMGCrackTau;
    for (int i = 0; i <= 20; ++i) {
        double age = static_cast<double>(i) * 0.005 - 0.01;
        double v = render::gun_shot_amp(age, tau);
        REQUIRE(v >= 0.0);
        REQUIRE(v <= 1.0);
    }
}

// ---------------------------------------------------------------------------
// explosion_amp
// ---------------------------------------------------------------------------
TEST_CASE("explosion_amp: 0 at age<=0") {
    REQUIRE(render::explosion_amp(0.0) == Catch::Approx(0.0));
    REQUIRE(render::explosion_amp(-0.1) == Catch::Approx(0.0));
}

TEST_CASE("explosion_amp: 0 at or after life") {
    REQUIRE(render::explosion_amp(render::kExplosionAudioLife) == Catch::Approx(0.0));
    REQUIRE(render::explosion_amp(render::kExplosionAudioLife + 0.1) == Catch::Approx(0.0));
}

TEST_CASE("explosion_amp: peak > 0.1 in the middle") {
    double peak = 0.0;
    int steps = 10000;
    for (int i = 0; i <= steps; ++i) {
        double age = render::kExplosionAudioLife * static_cast<double>(i) / static_cast<double>(steps);
        double v = render::explosion_amp(age);
        if (v > peak) peak = v;
    }
    REQUIRE(peak > 0.1);
}

TEST_CASE("explosion_amp: always in [0,1]") {
    int steps = 20000;
    for (int i = 0; i <= steps; ++i) {
        // Sweep a range wider than life to cover clamped regions
        double age = (render::kExplosionAudioLife + 0.2) * static_cast<double>(i) / static_cast<double>(steps) - 0.1;
        double v = render::explosion_amp(age);
        REQUIRE(v >= 0.0);
        REQUIRE(v <= 1.0);
    }
}

// ---------------------------------------------------------------------------
// hit_amp
// ---------------------------------------------------------------------------
TEST_CASE("hit_amp: 0 at age<=0") {
    REQUIRE(render::hit_amp(0.0) == Catch::Approx(0.0));
    REQUIRE(render::hit_amp(-0.001) == Catch::Approx(0.0));
}

TEST_CASE("hit_amp: 0 at or after life") {
    REQUIRE(render::hit_amp(render::kHitAudioLife) == Catch::Approx(0.0));
    REQUIRE(render::hit_amp(render::kHitAudioLife + 0.01) == Catch::Approx(0.0));
}

TEST_CASE("hit_amp: peak > 0.1 somewhere in the middle") {
    double peak = 0.0;
    int steps = 5000;
    for (int i = 0; i <= steps; ++i) {
        double age = render::kHitAudioLife * static_cast<double>(i) / static_cast<double>(steps);
        double v = render::hit_amp(age);
        if (v > peak) peak = v;
    }
    REQUIRE(peak > 0.1);
}

TEST_CASE("hit_amp: always in [0,1]") {
    int steps = 5000;
    for (int i = 0; i <= steps; ++i) {
        double age = (render::kHitAudioLife + 0.02) * static_cast<double>(i) / static_cast<double>(steps) - 0.005;
        double v = render::hit_amp(age);
        REQUIRE(v >= 0.0);
        REQUIRE(v <= 1.0);
    }
}

// ---------------------------------------------------------------------------
// GunSynth
// ---------------------------------------------------------------------------
TEST_CASE("GunSynth: silent when not firing") {
    render::GunSynth gs;
    gs.init(22050.0, 36.0, 38.0);
    gs.set_firing(false);

    // Render 512 samples — should be all zeros (no trigger ever fired)
    std::vector<short> buf(512, 99);
    gs.render(buf.data(), static_cast<int>(buf.size()));
    for (int i = 0; i < static_cast<int>(buf.size()); ++i) {
        REQUIRE(buf[i] == 0);
    }
}

TEST_CASE("GunSynth: audible when firing, bounded always") {
    render::GunSynth gs;
    gs.init(22050.0, 36.0, 38.0);
    gs.set_firing(true);

    // Render ~1 second worth (22050 samples in chunks of 512)
    const int total = 22050;
    const int chunk = 512;
    std::vector<short> buf(chunk, 0);

    int max_abs = 0;
    for (int off = 0; off < total; off += chunk) {
        int n = (off + chunk <= total) ? chunk : (total - off);
        gs.render(buf.data(), n);
        for (int i = 0; i < n; ++i) {
            int sv = static_cast<int>(buf[i]);
            int abs_val = sv < 0 ? -sv : sv;
            if (abs_val > max_abs) max_abs = abs_val;
            // Bounded within int16 range
            REQUIRE(sv >= -32767);
            REQUIRE(sv <= 32767);
        }
    }
    // Should be audible (some |sample| > a few hundred)
    REQUIRE(max_abs > 200);
}

TEST_CASE("GunSynth: returns to silence after firing stops") {
    // The regression this design most needs pinned: a stuck shot voice or a
    // stuck firing gain would keep the channel humming after release. Fire for
    // ~0.5 s, stop, drain well past the longest crack tail, then require dead
    // silence (Fable P2 test-gap 2026-07-13).
    render::GunSynth gs;
    gs.init(22050.0, 36.0, 38.0);

    const int chunk = 512;
    std::vector<short> buf(chunk, 0);

    gs.set_firing(true);
    for (int off = 0; off < 11025; off += chunk) {  // ~0.5 s of fire
        gs.render(buf.data(), chunk);
    }

    gs.set_firing(false);
    // Drain ~0.4 s — longer than the release slew (45 ms) + the longest crack
    // tail (cannon tau 20 ms decays to <1e-4 in ~0.18 s).
    std::vector<short> drain(8820, 0);
    gs.render(drain.data(), static_cast<int>(drain.size()));

    // Now it must be pin-silent.
    std::vector<short> tail(chunk, 99);
    gs.render(tail.data(), chunk);
    for (int i = 0; i < chunk; ++i) {
        REQUIRE(tail[i] == 0);
    }
}

TEST_CASE("GunSynth: external cannon one-shot audible while NOT firing") {
    // THE FLAK BOOM (Chad 2026-08-28: "I cant hear the gun"): while manned,
    // set_firing(false) shuts the aircraft's composite roar -- and the flak's
    // per-round trigger_cannon() voices were multiplied by the same decayed
    // firing_gain_, so the boom was synthesized straight into silence. An
    // external trigger is a FREE voice: it must sound through a closed gate.
    render::GunSynth gs;
    gs.init(22050.0, 36.0, 38.0);
    gs.set_firing(false);

    gs.trigger_cannon();
    gs.trigger_cannon();  // the shipped two-voices-per-round weight

    const int chunk = 512;
    std::vector<short> buf(chunk, 0);
    int max_abs = 0;
    for (int off = 0; off < 4096; off += chunk) {
        gs.render(buf.data(), chunk);
        for (int i = 0; i < chunk; ++i) {
            int sv = static_cast<int>(buf[i]);
            int abs_val = sv < 0 ? -sv : sv;
            if (abs_val > max_abs) max_abs = abs_val;
            REQUIRE(sv >= -32767);
            REQUIRE(sv <= 32767);
        }
    }
    REQUIRE(max_abs > 200);

    // And it must DIE: drain well past the crack + sub-thump tails, then
    // require pin silence -- a free voice must not hum forever.
    std::vector<short> drain(22050, 0);
    gs.render(drain.data(), static_cast<int>(drain.size()));
    std::vector<short> tail(chunk, 99);
    gs.render(tail.data(), chunk);
    for (int i = 0; i < chunk; ++i) {
        REQUIRE(tail[i] == 0);
    }
}

// ---------------------------------------------------------------------------
// CombatSfxSynth
// ---------------------------------------------------------------------------
TEST_CASE("CombatSfxSynth: silent before any trigger") {
    render::CombatSfxSynth sfx;
    sfx.init(22050.0);

    std::vector<short> buf(512, 99);
    sfx.render(buf.data(), static_cast<int>(buf.size()));
    for (int i = 0; i < static_cast<int>(buf.size()); ++i) {
        REQUIRE(buf[i] == 0);
    }
}

TEST_CASE("CombatSfxSynth: explosion non-silent after trigger, silent after life") {
    render::CombatSfxSynth sfx;
    sfx.init(22050.0);

    sfx.trigger_explosion();

    // Render a short segment near the start — should be non-silent
    const int short_seg = 512;
    std::vector<short> buf(short_seg, 0);
    sfx.render(buf.data(), short_seg);

    int max_abs = 0;
    for (int i = 0; i < short_seg; ++i) {
        int abs_val = buf[i] < 0 ? -buf[i] : buf[i];
        if (abs_val > max_abs) max_abs = abs_val;
    }
    REQUIRE(max_abs > 0);  // non-silent

    // Drain well past the audio life (1.30 s @ 22050 = ~28665 samples)
    const int drain = 30000;
    std::vector<short> drain_buf(drain, 0);
    sfx.render(drain_buf.data(), drain);

    // After life has passed, output should be silent
    std::vector<short> silent_buf(512, 99);
    sfx.render(silent_buf.data(), 512);
    for (int i = 0; i < 512; ++i) {
        REQUIRE(silent_buf[i] == 0);
    }
}

TEST_CASE("CombatSfxSynth: hit non-silent after trigger, silent after life") {
    render::CombatSfxSynth sfx;
    sfx.init(22050.0);

    sfx.trigger_hit();

    // Render early — non-silent
    const int short_seg = 256;
    std::vector<short> buf(short_seg, 0);
    sfx.render(buf.data(), short_seg);

    int max_abs = 0;
    for (int i = 0; i < short_seg; ++i) {
        int abs_val = buf[i] < 0 ? -buf[i] : buf[i];
        if (abs_val > max_abs) max_abs = abs_val;
    }
    REQUIRE(max_abs > 0);

    // Drain past life (0.09 s @ 22050 = ~1985 samples)
    const int drain = 3000;
    std::vector<short> drain_buf(drain, 0);
    sfx.render(drain_buf.data(), drain);

    // After life: silent
    std::vector<short> silent_buf(256, 99);
    sfx.render(silent_buf.data(), 256);
    for (int i = 0; i < 256; ++i) {
        REQUIRE(silent_buf[i] == 0);
    }
}

// ---------------------------------------------------------------------------
// soft_clip — output limiter (Fable P2 2026-07-13: replace the hard-clip corner)
// ---------------------------------------------------------------------------
TEST_CASE("soft_clip: identity inside the linear region (timbre untouched)") {
    // Below/at the threshold the limiter is a pure pass-through, so single-sound
    // samples are bit-identical to the old hard-clip path (no golden moves).
    REQUIRE(render::soft_clip(0.0) == Catch::Approx(0.0));
    REQUIRE(render::soft_clip(0.5) == Catch::Approx(0.5));
    REQUIRE(render::soft_clip(-0.5) == Catch::Approx(-0.5));
    REQUIRE(render::soft_clip(-0.25) == Catch::Approx(-0.25));
    // Exactly at the threshold is still identity (|x| <= t branch).
    REQUIRE(render::soft_clip(render::kSoftClipThreshold) ==
            Catch::Approx(render::kSoftClipThreshold));
}

TEST_CASE("soft_clip: odd-symmetric, non-decreasing, bounded in [-1, 1]") {
    // Odd symmetry holds everywhere, including the saturation plateau.
    for (double x : {0.3, 0.95, 1.5, 4.0, 50.0}) {
        REQUIRE(render::soft_clip(-x) == Catch::Approx(-render::soft_clip(x)));
    }
    // Strictly increasing THROUGH the active knee (x in [0.9, 1.5]); the tanh is
    // meaningfully rising here before it saturates.
    double prev = render::soft_clip(0.9);
    for (int i = 1; i <= 12; ++i) {
        const double cur = render::soft_clip(0.9 + 0.05 * static_cast<double>(i));
        REQUIRE(cur > prev);
        prev = cur;
    }
    // Bounded in [-1, 1] for arbitrarily large drive (a real limiter — an extreme
    // overshoot DOES saturate to the ceiling, but smoothly, never above it).
    for (double x : {1.0, 2.0, 5.0, 1000.0, 1e6}) {
        REQUIRE(render::soft_clip(x) <= 1.0);
        REQUIRE(render::soft_clip(x) > render::kSoftClipThreshold);
        REQUIRE(render::soft_clip(-x) >= -1.0);
    }
    // It genuinely approaches the ceiling (not a low plateau).
    REQUIRE(render::soft_clip(5.0) == Catch::Approx(1.0));
}

TEST_CASE("soft_clip: at the ceiling a hard clip would pin, the knee softens it") {
    // The point of the change: at input == 1.0 the OLD hard clip pinned exactly to
    // 1.0 (a corner); the soft knee compresses it BELOW full-scale, and likewise
    // through the moderate-overshoot band the round-trip stays under the ceiling.
    REQUIRE(render::soft_clip(1.0) < 1.0);
    REQUIRE(render::soft_clip(1.2) < 1.0);
    REQUIRE(static_cast<short>(render::soft_clip(1.0) * 32767.0) <
            static_cast<short>(32767));
    REQUIRE(static_cast<short>(render::soft_clip(1.5) * 32767.0) <
            static_cast<short>(32767));
    // Continuity at the knee: just above the threshold barely departs from it.
    REQUIRE(render::soft_clip(render::kSoftClipThreshold + 1e-4) ==
            Catch::Approx(render::kSoftClipThreshold).margin(1e-4));
}

TEST_CASE("CombatSfxSynth: a stacked furball stays bounded and audible") {
    // Overfill both voice pools so many voices sum at once (the case the soft-clip
    // exists for). Output must stay inside int16 and be audibly loud — the graceful
    // knee (vs the old hard corner) is pinned by the pure soft_clip tests above.
    render::CombatSfxSynth sfx;
    sfx.init(22050.0);
    for (int i = 0; i < 16; ++i) {
        sfx.trigger_explosion();
        sfx.trigger_hit();
    }
    const int n = 4096;
    std::vector<short> buf(n, 0);
    sfx.render(buf.data(), n);
    int max_abs = 0;
    for (int i = 0; i < n; ++i) {
        const int a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > max_abs) max_abs = a;
        REQUIRE(buf[i] <= 32767);
        REQUIRE(buf[i] >= -32767);
    }
    REQUIRE(max_abs > 200);  // a furball is loud
}
