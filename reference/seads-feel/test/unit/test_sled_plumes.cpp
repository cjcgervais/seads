// R5 rows 4/5/6 -- render/sled_plumes.h trail BOOKKEEPING legs. The header is
// pure (glm + std, zero raylib -- the wingtip_smoke.h shape) precisely so
// these run headlessly. The legs a green build would otherwise hide:
//
//   1. smoke_hash is deterministic and channel-independent (no clock: same
//      seed/channel must reproduce bitwise, or the turbulence boils).
//   2. THE STILL CASE (the R4b lesson, stated in the rung brief): roost
//      intensity is EXACTLY 0.0 at rest -- even at full flux (full-throttle
//      trenching on the spot) -- and grows with ground speed.
//   3. Emission is frame-rate independent: the same span sliced into
//      different frame_dt's emits the same puff count.
//   4. Ring caps hold: no trail ever exceeds its kMax.
//   5. Age/expiry: with emission off, every puff dies within its life.
//   6. Breath is PERIODIC at the breathing rate (bursts/sec = 1/period), and
//      the period shortens with throttle.
//   7. Zero dial gains emit NOTHING (the A/B kill arm is a true kill).
//   8. Full determinism: two identical update sequences produce bitwise
//      identical trails.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/sled_plumes.h"

namespace {

// A moving-at-speed input set with every emitter armed.
render::SledPlumeInputs moving_inputs() {
    render::SledPlumeInputs in;
    in.pos = glm::dvec3(0.0, 15000.0, 0.0);  // planet-ish radius up
    in.basis = glm::dmat3(1.0);
    in.vel = glm::dvec3(0.0, 0.0, -20.0);  // 20 m/s along body forward (-Z)
    in.up = glm::dvec3(0.0, 1.0, 0.0);
    in.ground_speed = 20.0;
    in.roost_flux = 0.8;
    in.throttle = 0.7;
    in.head_pos = in.pos + glm::dvec3(0.0, 0.7, 0.0);
    in.head_fwd = glm::dvec3(0.0, 0.0, -1.0);
    in.head_valid = true;
    return in;
}

std::size_t total(const render::SledPlumes& t) {
    return t.roost.size() + t.exhaust.size() + t.breath.size();
}

}  // namespace

TEST_CASE("smoke_hash is deterministic and channels are independent") {
    // Bitwise reproducible (leg 1): the whole no-boil guarantee.
    REQUIRE(render::smoke_hash(1234u, 1) == render::smoke_hash(1234u, 1));
    REQUIRE(render::smoke_hash(1234u, 1) != render::smoke_hash(1234u, 2));
    REQUIRE(render::smoke_hash(1234u, 1) != render::smoke_hash(1235u, 1));
    for (std::uint32_t s = 1; s < 200; ++s) {
        const float v = render::smoke_hash(s, 3);
        REQUIRE(v >= 0.0f);
        REQUIRE(v <= 1.0f);
    }
}

TEST_CASE("roost intensity is EXACTLY zero at rest, grows with speed") {
    // Leg 2 -- the still case. Full flux, zero speed: nothing. Not "small":
    // exactly 0.0, the same bit-identical discipline as deform_at off-track.
    REQUIRE(render::sled_roost_intensity(1.0, 0.0) == 0.0);
    REQUIRE(render::sled_roost_intensity(1.0, render::kRoostSpeedLo) == 0.0);
    // Zero flux (no loose snow / buried tunnel): nothing at any speed.
    REQUIRE(render::sled_roost_intensity(0.0, 30.0) == 0.0);
    // Monotone rise between the knees, saturating at flux by kRoostSpeedHi.
    const double a = render::sled_roost_intensity(0.8, 3.0);
    const double b = render::sled_roost_intensity(0.8, 6.0);
    const double c = render::sled_roost_intensity(0.8, render::kRoostSpeedHi);
    REQUIRE(a > 0.0);
    REQUIRE(b > a);
    REQUIRE(c == 0.8);

    // And through the update: a still machine at full flux emits NOTHING.
    render::SledPlumes t;
    render::SledPlumeInputs in = moving_inputs();
    in.vel = glm::dvec3(0.0);
    in.ground_speed = 0.0;
    in.roost_flux = 1.0;
    for (int i = 0; i < 200; ++i) render::sled_plumes_update(t, in, 1.0 / 60.0);
    REQUIRE(t.roost.empty());
}

TEST_CASE("emission count is frame-rate independent") {
    // Leg 3 -- the same 2 s span at 60 Hz vs 240 Hz vs a ragged mix.
    const render::SledPlumeInputs in = moving_inputs();
    const auto run = [&](int frames, double frame_dt) {
        render::SledPlumes t;
        for (int i = 0; i < frames; ++i)
            render::sled_plumes_update(t, in, frame_dt);
        return t.emit_count;
    };
    const std::uint32_t n60 = run(120, 1.0 / 60.0);    // 2.0 s
    const std::uint32_t n240 = run(480, 1.0 / 240.0);  // the same 2.0 s
    // Accumulator emission: totals agree to within one emit interval's worth
    // per trail (the fractional remainders).
    REQUIRE(n60 > 0);
    REQUIRE(std::abs(static_cast<long>(n60) - static_cast<long>(n240)) <= 3);
}

TEST_CASE("ring caps hold at the wildest dial") {
    // Leg 4 -- gain 4 (the top of the dial range) for 10 s: never over kMax.
    render::SledPlumeInputs in = moving_inputs();
    in.roost_gain = 4.0;
    in.exhaust_gain = 4.0;
    in.breath_gain = 4.0;
    in.throttle = 1.0;
    in.roost_flux = 1.0;
    render::SledPlumes t;
    std::size_t roost_peak = 0, exhaust_peak = 0, breath_peak = 0;
    for (int i = 0; i < 600; ++i) {
        render::sled_plumes_update(t, in, 1.0 / 60.0);
        REQUIRE(t.roost.size() <= render::kRoostMax);
        REQUIRE(t.exhaust.size() <= render::kExhaustMax);
        REQUIRE(t.breath.size() <= render::kBreathMax);
        roost_peak = std::max(roost_peak, t.roost.size());
        exhaust_peak = std::max(exhaust_peak, t.exhaust.size());
        breath_peak = std::max(breath_peak, t.breath.size());
    }
    // Every trail actually lived at some point (a cap test over empty trails
    // certifies nothing -- the R4b still-case lesson). Peaks, not the final
    // frame: a breath cluster's 1.1 s life ends between exhales.
    REQUIRE(roost_peak > 0);
    REQUIRE(exhaust_peak > 0);
    REQUIRE(breath_peak > 0);
    // And the roost at gain 4 genuinely PRESSES its cap (rate 4x steady-state
    // ~= 264 > 240): the ring is doing work, not head-room.
    REQUIRE(roost_peak == render::kRoostMax);
}

TEST_CASE("age and expiry: emission off drains every trail") {
    // Leg 5 -- populate, then age with all gains 0 for the longest life + one
    // frame: everything must be gone (no immortal puffs, no leak).
    render::SledPlumes t;
    render::SledPlumeInputs in = moving_inputs();
    for (int i = 0; i < 120; ++i) render::sled_plumes_update(t, in, 1.0 / 60.0);
    REQUIRE(total(t) > 0);
    in.roost_gain = 0.0;
    in.exhaust_gain = 0.0;
    in.breath_gain = 0.0;
    const double longest =
        std::max(render::kRoostLife,
                 std::max(render::kExhaustLife, render::kBreathLife));
    const int frames = static_cast<int>(longest * 60.0) + 2;
    for (int i = 0; i < frames; ++i)
        render::sled_plumes_update(t, in, 1.0 / 60.0);
    REQUIRE(total(t) == 0);
}

TEST_CASE("breath is periodic and quickens with throttle") {
    // Leg 6 -- count bursts (emit_count deltas) over 12.5 s (the half-period
    // margin keeps a last wrap sitting exactly on the window edge from
    // flapping on float accumulation). At throttle 0 the period is
    // kBreathPeriodRest (4 s -> 3 exhales); at full throttle
    // kBreathPeriodWork (2 s -> 6).
    const auto bursts = [](double throttle) {
        render::SledPlumes t;
        render::SledPlumeInputs in = moving_inputs();
        in.roost_gain = 0.0;    // isolate the breath channel
        in.exhaust_gain = 0.0;
        in.throttle = throttle;
        std::uint32_t emitted = 0;
        int wraps = 0;
        for (int i = 0; i < 750; ++i) {  // 12.5 s at 60 Hz
            render::sled_plumes_update(t, in, 1.0 / 60.0);
            if (t.emit_count != emitted) ++wraps;
            emitted = t.emit_count;
        }
        return wraps;
    };
    REQUIRE(bursts(0.0) == 3);
    REQUIRE(bursts(1.0) == 6);
}

TEST_CASE("zero gains emit nothing, ever") {
    // Leg 7 -- the dial's 0 arm is a TRUE kill (the A/B baseline).
    render::SledPlumes t;
    render::SledPlumeInputs in = moving_inputs();
    in.roost_gain = 0.0;
    in.exhaust_gain = 0.0;
    in.breath_gain = 0.0;
    in.throttle = 1.0;
    in.roost_flux = 1.0;
    for (int i = 0; i < 600; ++i) render::sled_plumes_update(t, in, 1.0 / 60.0);
    REQUIRE(total(t) == 0);
    REQUIRE(t.emit_count == 0);
}

TEST_CASE("identical update sequences reproduce bitwise") {
    // Leg 8 -- no clock, no unseeded randomness: two runs of the same
    // sequence must match to the bit, positions and velocities included.
    const render::SledPlumeInputs in = moving_inputs();
    render::SledPlumes a, b;
    for (int i = 0; i < 300; ++i) {
        render::sled_plumes_update(a, in, 1.0 / 60.0);
        render::sled_plumes_update(b, in, 1.0 / 60.0);
    }
    REQUIRE(a.emit_count == b.emit_count);
    REQUIRE(a.roost.size() == b.roost.size());
    REQUIRE(a.exhaust.size() == b.exhaust.size());
    REQUIRE(a.breath.size() == b.breath.size());
    for (std::size_t i = 0; i < a.roost.size(); ++i) {
        REQUIRE(a.roost[i].pos == b.roost[i].pos);
        REQUIRE(a.roost[i].vel == b.roost[i].vel);
        REQUIRE(a.roost[i].age == b.roost[i].age);
        REQUIRE(a.roost[i].seed == b.roost[i].seed);
    }
    for (std::size_t i = 0; i < a.breath.size(); ++i) {
        REQUIRE(a.breath[i].pos == b.breath[i].pos);
    }
}
