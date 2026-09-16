// ★★★ GAIT LADDER G1 — FEET MEET THE GROUND (docs/PLAN_20260904_gait_ladder.md
// §4). These legs live beside `test_walker.cpp` and reuse its fixture style
// (`flat_hf`, `snow_of_depth`, `dials`) rather than re-deriving a second one.
//
// ★ EACH LEG FAILS FOR ITS OWN REASON (L-GATE): the pin leg (now over a
// MOVING walker) never recomputes the target during stance, the plant leg
// checks the sampled ground at the PLANT's own direction (not the walker's),
// the constant-grade leg walks ~20+ strides up an ordinary 10% hill and
// checks every single plant (the leg that catches a guard latching on
// terrain that is not a glitch), the pitch leg re-derives the same heel/toe
// formula independently and checks the production value against it, the
// jump leg hand-builds a `prev` state and forces exactly one bad sample, the
// hard-stop leg drives a real walker to a cruise and then cuts the input
// mid-swing to bound every frame's target jump, the two-walkers leg proves
// stepping one never perturbs the other's trajectory, and the no-pop leg
// bounds the very first output (tightly, 0.5 m) against the under-hip rest
// position.
//
// ★★★ G1b: RED-TEAM FIXES ON fcb125127 (see sim/gait.h / gait.cpp / render/
// sled_model.cpp banners for the individual derivations): the ground guard
// was redesigned from reject-and-hold (which latched forever on an ordinary
// grade) to a same-point rate limiter; the stance-entry pin now anchors on
// the swing blend's own last published position, not the far plant, so a
// speed-triggered early landing cannot teleport the foot; the foot-pitch
// axis was corrected (it drove the toe DOWN for a sim value that means
// toe-UP); and the reach clamp gained a render-side excursion-scale
// mitigation (G2's pelvis lowering is the real fix).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "sim/gait.h"
#include "sim/walker.h"
#include "world/heightfield.h"
#include "world/snowpack.h"

namespace {

constexpr double kR = 15000.0;

world::HeightField flat_hf() {
    world::HeightField hf;
    hf.px.assign(4, 0);
    hf.w = 2;
    hf.h = 2;
    hf.R = kR;
    hf.relief_scale = 0.0;
    return hf;
}

world::SnowpackField snow_of_depth(const world::HeightField& hf, double d) {
    world::SnowpackField f;
    f.hf = &hf;
    f.p.base_m = d;
    f.p.curv_gain = 0.0;
    f.p.drain_gain = 0.0;
    f.p.aspect_lee = 0.0;
    f.p.elev_gain_per_km = 0.0;
    f.p.slope_shed = 0.0;
    f.p.depth_max_m = 5.0;
    return f;
}

sim::WalkerParams dials() {
    sim::WalkerParams p;
    return p;  // shipped defaults
}

// A walker fixture that needs no fall/get-up sequence: gait math only ever
// reads the fields it is handed, so a hand-built `WalkerState` is legitimate
// here the way it is not in test_walker.cpp's own (behavioural) legs.
sim::WalkerState flat_walker(const sim::WalkerParams& p, double speed_mps) {
    sim::WalkerState w;
    w.pos = glm::dvec3(kR, 0.0, 0.0);       // up = (1,0,0)
    w.heading = glm::dvec3(0.0, 1.0, 0.0);  // tangent to up
    w.vel = w.heading * speed_mps;
    w.depth_m = 0.0;
    w.stride_m = sim::walker_stride(p, 0.0);
    w.gait_phase = 0.0;
    return w;
}

}  // namespace

TEST_CASE("gait_the_pinned_foot_does_not_move_in_world", "[g1][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 2.0);

    sim::GaitState g;
    const double dt = 1.0 / 120.0;
    int stance_frames_checked = 0;
    for (int i = 0; i < 4000; ++i) {
        // ★ RED-TEAM FIX (G1b, P3 test hardening): a MOVING walker, not a
        // phase advanced in place with a stationary `pos` -- "the pin does
        // not move in WORLD" is a trivial, vacuous claim if the man himself
        // never goes anywhere. He has to actually walk across the field for
        // this to test anything.
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        const sim::GaitState prev = g;
        g = sim::step_gait(prev, w, p, f, dt);
        for (int s = 0; s < 2; ++s) {
            // The output target IS the pin, every stance frame, bit for bit.
            if (g.foot[s].in_stance) {
                REQUIRE(g.foot[s].target_w.x == g.foot[s].pin_w.x);
                REQUIRE(g.foot[s].target_w.y == g.foot[s].pin_w.y);
                REQUIRE(g.foot[s].target_w.z == g.foot[s].pin_w.z);
            }
            // And across two CONSECUTIVE stance frames (no transition
            // between them) the pin itself has not moved, bit for bit --
            // while the walker's own `w.pos` (and therefore every direction
            // this function samples from) has moved every single tick.
            // ⚠ `prev.valid` guards the very first frame: a default
            // `GaitFoot` reads `in_stance == true` (its struct default) with
            // no pin behind it at all, which would otherwise read as a false
            // "stance continued" on frame zero.
            if (g.foot[s].in_stance && prev.foot[s].valid &&
                prev.foot[s].in_stance) {
                REQUIRE(g.foot[s].pin_w.x == prev.foot[s].pin_w.x);
                REQUIRE(g.foot[s].pin_w.y == prev.foot[s].pin_w.y);
                REQUIRE(g.foot[s].pin_w.z == prev.foot[s].pin_w.z);
                ++stance_frames_checked;
            }
        }
    }
    // A walk this long crosses many stance windows; if this is 0 the leg
    // above is vacuously true and proves nothing.
    REQUIRE(stance_frames_checked > 100);
    // And he genuinely translated -- otherwise "a moving walker" is not what
    // was actually exercised above.
    REQUIRE(glm::length(w.pos - glm::dvec3(kR, 0.0, 0.0)) > 1.0);
}

TEST_CASE("gait_plants_track_a_constant_grade_across_many_strides",
          "[g1][gait]") {
    // ★★★ RED-TEAM LEG (G1b, P1): the leg that would have caught the
    // reject-and-hold guard latching on ORDINARY TERRAIN. A 10% grade is a
    // perfectly normal hill, not a glitch -- consecutive plants a stride
    // apart on this slope differ in radius by more than the guard's old
    // ">0.15 m" absolute threshold, which is exactly what made the first cut
    // freeze the foot at a stale radius after a step or two. Every plant
    // across ~20+ strides must land exactly on the ground actually sampled
    // there.
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = snow_of_depth(hf, 0.0);
    const double kGrade = 0.10;  // 10%, an everyday hill
    // A gain such that travelling an arc-length `d` along `dir.z` (small
    // angle: dz ~= d / R0) changes the radius by `kGrade * d` -- an honest
    // 10% grade along the line of travel.
    const double slope_gain = kGrade * kR;
    f.sample_override = [&](const glm::dvec3& dir) {
        world::SnowpackField::GroundSample g;
        g.drive_r = kR + slope_gain * dir.z;
        g.depth_m = 0.0;
        return g;
    };
    const sim::WalkerParams p = dials();
    sim::WalkerState w;
    w.pos = glm::dvec3(0.0, kR, 0.0);        // up = (0,1,0)
    w.heading = glm::dvec3(0.0, 0.0, -1.0);  // straight up/down the grade
    w.vel = w.heading * 2.0;
    w.depth_m = 0.0;
    w.stride_m = sim::walker_stride(p, 0.0);
    w.gait_phase = 0.0;

    sim::GaitState g;
    const double dt = 1.0 / 120.0;
    int plants_checked = 0;
    // ~10 s at 2 m/s over a sub-metre-to-few-metre human stride is well
    // past 20 strides per foot.
    for (int i = 0; i < 1200; ++i) {
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        g = sim::step_gait(g, w, p, f, dt);
        for (int s = 0; s < 2; ++s) {
            if (g.foot[s].valid && !g.foot[s].in_stance) {
                const double got = glm::length(g.foot[s].plant_w);
                const double want =
                    f.sample_at(glm::normalize(g.foot[s].plant_w)).drive_r;
                REQUIRE(std::abs(got - want) < 1.0e-6);
                ++plants_checked;
            }
        }
    }
    REQUIRE(plants_checked > 100);
}

TEST_CASE("gait_a_hard_stop_mid_swing_does_not_teleport_the_target",
          "[g1][gait]") {
    // ★★★ RED-TEAM LEG (G1b, P2): `ds` (stance/swing split) is recomputed
    // EVERY FRAME from the CURRENT speed, so decelerating hard enough can
    // land a still-mid-air foot in stance before the phase has actually
    // reached the plant. The first cut pinned at the far `plant_w` regardless
    // -- up to a stride away from where the foot visibly was -- and every
    // frame's `target_w` must never jump like that. Hardpack (depth 0) keeps
    // the swing lift small (`lift_base_m` ~0.08 m default) so even a
    // worst-timed cutoff's vertical settle-to-ground cannot itself account
    // for a big jump; what is being tested is the HORIZONTAL fix.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    const double dt = 1.0 / 120.0;

    sim::WalkerState w;
    w.pos = glm::dvec3(kR, 0.0, 0.0);
    w.heading = glm::dvec3(0.0, 1.0, 0.0);
    w.mode = sim::WalkerMode::Afoot;  // BY NAME -- skip the fall/get-up chain

    sim::WalkerInputs go;
    go.forward = 1.0f;
    for (int i = 0; i < 400; ++i) w = sim::step_walker(w, go, p, f, dt);
    REQUIRE(glm::length(w.vel) > 2.0);  // he is genuinely cruising

    sim::GaitState g;
    for (int i = 0; i < 200; ++i) g = sim::step_gait(g, w, p, f, dt);

    // Now cut the input hard, mid-cycle, and watch every frame's jump.
    sim::WalkerInputs stop;  // forward = 0: the slew (accel_tau_s) decelerates
    glm::dvec3 last[2] = {g.foot[0].target_w, g.foot[1].target_w};
    double worst = 0.0;
    for (int i = 0; i < 400; ++i) {
        w = sim::step_walker(w, stop, p, f, dt);
        g = sim::step_gait(g, w, p, f, dt);
        for (int s = 0; s < 2; ++s) {
            const double d = glm::length(g.foot[s].target_w - last[s]);
            worst = std::max(worst, d);
            last[s] = g.foot[s].target_w;
        }
    }
    REQUIRE(worst <= 0.10 + 1.0e-9);
}

TEST_CASE("gait_the_plant_lands_on_the_sampled_ground", "[g1][gait]") {
    // A field whose radius is NOT constant, so "lands on the sampled ground"
    // is a real claim about the PLANT's own direction and not a tautology a
    // uniform field would pass by accident.
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = snow_of_depth(hf, 0.0);
    f.sample_override = [](const glm::dvec3& dir) {
        world::SnowpackField::GroundSample g;
        g.drive_r = kR + 5.0 * dir.x;
        g.depth_m = 0.0;
        return g;
    };
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 2.0);
    const double ds = sim::walker_stance_frac(p, glm::length(w.vel));
    w.gait_phase = std::clamp(ds, 0.05, 0.95) + 0.02;  // foot 1 (offset +0.0)
                                                        // is mid-swing already

    const sim::GaitState g = sim::step_gait(sim::GaitState{}, w, p, f, 1.0 / 120.0);
    const sim::GaitFoot& foot = g.foot[1];
    REQUIRE_FALSE(foot.in_stance);
    REQUIRE(glm::length(foot.plant_w) > 0.0);

    const double got_len = glm::length(foot.plant_w);
    const double want_r = f.sample_at(glm::normalize(foot.plant_w)).drive_r;
    REQUIRE(std::abs(got_len - want_r) < 1.0e-6);
}

TEST_CASE("gait_foot_pitch_matches_the_slope", "[g1][gait]") {
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = snow_of_depth(hf, 0.0);
    // Tilted along +z: the walker's heading (0,0,-1) below runs straight down
    // this gradient, so the heel/toe pair actually sees the slope.
    f.sample_override = [](const glm::dvec3& dir) {
        world::SnowpackField::GroundSample g;
        g.drive_r = kR + 40.0 * dir.z;
        g.depth_m = 0.0;
        return g;
    };
    const sim::WalkerParams p = dials();
    sim::WalkerState w;
    w.pos = glm::dvec3(0.0, kR, 0.0);        // up = (0,1,0)
    w.heading = glm::dvec3(0.0, 0.0, -1.0);  // tangent
    w.vel = w.heading * 2.0;
    w.depth_m = 0.0;
    w.stride_m = sim::walker_stride(p, 0.0);
    const double ds = sim::walker_stance_frac(p, glm::length(w.vel));
    w.gait_phase = std::clamp(ds, 0.05, 0.95) + 0.02;

    const sim::GaitState g = sim::step_gait(sim::GaitState{}, w, p, f, 1.0 / 120.0);
    const sim::GaitFoot& foot = g.foot[1];
    REQUIRE_FALSE(foot.in_stance);

    // Re-derive the SAME formula independently, off the SAME two directions,
    // and require the production value to agree with it exactly (both read
    // the pure override function, so this is not a tolerance on the field --
    // it is a check that the production code used this formula and these two
    // points and no others).
    const glm::dvec3 fwd = w.heading;
    const glm::dvec3 heel_dir = glm::normalize(foot.plant_w - fwd * 0.11);
    const glm::dvec3 toe_dir = glm::normalize(foot.plant_w + fwd * 0.11);
    const double heel_r = f.sample_at(heel_dir).drive_r;
    const double toe_r = f.sample_at(toe_dir).drive_r;
    const double want = std::atan2(toe_r - heel_r, 0.22);
    REQUIRE(std::abs(foot.pitch_rad - want) < 1.0e-9);
    // And it is genuinely non-zero on a tilted field -- a leg that always
    // reads 0.0 would pass the line above vacuously.
    REQUIRE(std::abs(foot.pitch_rad) > 1.0e-6);
}

TEST_CASE("gait_a_ground_step_jump_is_rejected", "[g1][gait]") {
    // ★ RED-TEAM FIX (G1b, P1): the guard no longer REJECTS-AND-HOLDS (that
    // was the latch bug) -- it RATE-LIMITS a same-point re-read toward the
    // new sample by at most 0.15 m per event. So a one-off 0.5 m glitch is
    // damped to a 0.15 m step on this event (nowhere near the full jump),
    // and a second same-point event closes the rest of the gap -- it
    // converges rather than freezing.
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = snow_of_depth(hf, 0.0);
    const double baseline_r = kR;
    // Whatever direction is queried, the field answers with a radius 0.5 m
    // above the pin's own history -- a one-off discontinuity, injected on
    // purpose, well past the 0.15 m/event rate.
    f.sample_override = [&](const glm::dvec3&) {
        world::SnowpackField::GroundSample g;
        g.drive_r = baseline_r + 0.5;
        g.depth_m = 0.0;
        return g;
    };
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 2.0);
    const double ds = sim::walker_stance_frac(p, glm::length(w.vel));
    // Foot 1 (offset +0.0): phase inside [0, ds) => this frame is STANCE.
    w.gait_phase = 0.5 * std::clamp(ds, 0.05, 0.95);

    sim::GaitState prev;
    prev.foot[1].valid = true;
    prev.foot[1].in_stance = false;  // was swinging -- this frame plants
    prev.foot[1].plant_w = glm::dvec3(baseline_r, 0.0, 0.0);
    prev.foot[1].target_w = glm::dvec3(baseline_r, 0.0, 0.0);
    prev.foot[1].last_ground_r = baseline_r;
    // The SAME direction the stance-entry below will resample (its `dir` is
    // `normalize(pf.target_w)` here) -- this is what makes it a "same
    // point re-read", the case the rate limiter (not the accept-outright
    // path) applies to.
    prev.foot[1].last_ground_dir = glm::dvec3(1.0, 0.0, 0.0);

    const sim::GaitState g = sim::step_gait(prev, w, p, f, 1.0 / 120.0);
    const sim::GaitFoot& foot = g.foot[1];
    REQUIRE(foot.in_stance);
    // One event moves the radius by exactly the 0.15 m/event rate, not the
    // full 0.5 m jump.
    REQUIRE(std::abs(glm::length(foot.pin_w) - (baseline_r + 0.15)) < 1.0e-9);
    REQUIRE(std::abs(glm::length(foot.pin_w) - (baseline_r + 0.5)) > 0.1);
    REQUIRE(std::abs(foot.last_ground_r - (baseline_r + 0.15)) < 1.0e-9);

    // And it CONVERGES rather than latching: a second same-point event (the
    // field still answering with the same jumped value) closes the rest of
    // the gap.
    sim::GaitState prev2 = g;
    prev2.foot[1].in_stance = false;  // force the edge to fire again
    prev2.foot[1].target_w = foot.pin_w;
    prev2.foot[1].plant_w = foot.pin_w;
    const sim::GaitState g2 = sim::step_gait(prev2, w, p, f, 1.0 / 120.0);
    REQUIRE(std::abs(glm::length(g2.foot[1].pin_w) - (baseline_r + 0.30)) <
            1.0e-9);
}

TEST_CASE("gait_two_walkers_do_not_share_state", "[g1][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    const double dt = 1.0 / 120.0;

    // Walker B: a different path entirely (different phase seed and speed),
    // stepped between every one of A's steps.
    sim::WalkerState wb = flat_walker(p, 3.7);
    wb.gait_phase = 0.61;

    // Run A ALONE first, recording every target it ever publishes.
    sim::WalkerState wa = flat_walker(p, 2.0);
    sim::GaitState ga;
    std::vector<glm::dvec3> alone[2];
    for (int i = 0; i < 400; ++i) {
        wa.gait_phase += glm::length(wa.vel) * dt / wa.stride_m;
        wa.gait_phase -= std::floor(wa.gait_phase);
        ga = sim::step_gait(ga, wa, p, f, dt);
        alone[0].push_back(ga.foot[0].target_w);
        alone[1].push_back(ga.foot[1].target_w);
    }

    // Now run A again, byte-for-byte the same inputs, but interleaved with B
    // stepping on ITS OWN state between every one of A's steps.
    sim::WalkerState wa2 = flat_walker(p, 2.0);
    sim::WalkerState wb2 = wb;
    sim::GaitState ga2, gb2;
    for (int i = 0; i < 400; ++i) {
        wa2.gait_phase += glm::length(wa2.vel) * dt / wa2.stride_m;
        wa2.gait_phase -= std::floor(wa2.gait_phase);
        wb2.gait_phase += glm::length(wb2.vel) * dt / wb2.stride_m;
        wb2.gait_phase -= std::floor(wb2.gait_phase);
        gb2 = sim::step_gait(gb2, wb2, p, f, dt);  // B steps first every tick
        ga2 = sim::step_gait(ga2, wa2, p, f, dt);
        REQUIRE(ga2.foot[0].target_w.x == alone[0][i].x);
        REQUIRE(ga2.foot[0].target_w.y == alone[0][i].y);
        REQUIRE(ga2.foot[0].target_w.z == alone[0][i].z);
        REQUIRE(ga2.foot[1].target_w.x == alone[1][i].x);
        REQUIRE(ga2.foot[1].target_w.y == alone[1][i].y);
        REQUIRE(ga2.foot[1].target_w.z == alone[1][i].z);
    }
}

TEST_CASE("gait_first_step_has_no_pop", "[g1][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 1.5);
    w.gait_phase = 0.1;  // an arbitrary phase, not the seam

    const glm::dvec3 up(1.0, 0.0, 0.0);
    const glm::dvec3 fwd(0.0, 1.0, 0.0);
    const glm::dvec3 left = glm::normalize(glm::cross(up, fwd));

    const sim::GaitState g = sim::step_gait(sim::GaitState{}, w, p, f, 1.0 / 120.0);
    for (int s = 0; s < 2; ++s) {
        REQUIRE(g.foot[s].valid);
        const double side_sign = (s == 0) ? 1.0 : -1.0;
        const glm::dvec3 rest_dir =
            glm::normalize(w.pos + side_sign * 0.095 * left);
        const glm::dvec3 rest_w = rest_dir * f.sample_at(rest_dir).drive_r;
        const double dist = glm::length(g.foot[s].target_w - rest_w);
        // ★ RED-TEAM FIX (G1b, P3 test hardening): tightened from `w.stride_m`
        // (2.3 m -- vacuously wide) to 0.5 m, a real "no pop" bound.
        REQUIRE(dist < 0.5);
        // And nothing came out non-finite.
        REQUIRE(std::isfinite(g.foot[s].target_w.x));
        REQUIRE(std::isfinite(g.foot[s].target_w.y));
        REQUIRE(std::isfinite(g.foot[s].target_w.z));
    }
}

// ★★★ GAIT LADDER G2 -- THE PELVIS LIVES (+ THE RULED RUN SPEED),
// docs/PLAN_20260904_gait_ladder.md §4. These legs grade the pure functions
// `sim/gait.h` exposes for exactly this (`gait_spring_step`,
// `gait_vertical_bob_m`, `gait_lateral_sway_m`,
// `gait_trendelenburg_roll_rad`) plus the ruled speed cap in `sim/walker.h`,
// and one integration leg through `step_gait` itself for the drop law's own
// acceptance criterion (zero the STANCE over-reach on flat ground).
//
// ★★★ THE FIXTURE TRAP THIS RUNG'S OWN BUILD FOUND, NAMED SO IT IS NEVER
// RE-DISCOVERED: `flat_walker(p, speed_mps)` sets `w.vel` to `speed_mps`
// directly, but `w.stride_m = walker_stride(p, depth_m)` -- and
// `walker_stride` derives its own speed from `walker_speed_cap(p, depth_m)`
// (the ANCHOR for that depth), NOT from the velocity just set. G1's own
// legs never noticed because their probe speeds (2.0, 1.5 m/s) sat close
// enough to G1's OLD 3.5 m/s hardpack anchor that the mismatch stayed small.
// G2's ruling moved that anchor to 5.5 -- and a first draft of the leg below,
// calling `flat_walker(p, 1.5)` against the SHIPPED `dials()` (now anchored
// at 5.5), got a stride sized for a 5.5 m/s run at an actual 1.5 m/s walk: a
// 1.9x-too-long stride whose late-stance trail runs to a genuinely
// unreachable ~1.4 m -- which LOOKED exactly like a rediscovery of G1's own
// debt, and was instead a fixture bug. The fix, and the law for any new leg
// that calls `flat_walker`: build a LOCAL `WalkerParams` copy with
// `speed_hardpack_mps` set to the SAME speed handed to `flat_walker`, so the
// derived stride and the probed velocity describe the same walk.
TEST_CASE("g2_spring_converges_without_overshoot", "[g2][gait]") {
    // Critically damped, by construction: a STEP in target never overshoots
    // and never oscillates on the way to settling -- this is the whole
    // reason the plan calls for critical damping rather than any other
    // damping ratio.
    double x = 0.0, v = 0.0;
    const double target = 0.5;
    const double omega = 16.0;  // this rung's own retuned value (see gait.cpp)
    const double dt = 1.0 / 120.0;
    double prev_x = x;
    for (int i = 0; i < 2000; ++i) {
        x = sim::gait_spring_step(x, v, target, omega, dt);
        REQUIRE(x <= target + 1.0e-9);       // never overshoots
        REQUIRE(x >= prev_x - 1.0e-12);      // never backslides (no ring)
        prev_x = x;
    }
    REQUIRE(std::abs(x - target) < 1.0e-6);  // and it actually gets there
}

TEST_CASE("g2_pelvis_drop_zeroes_stance_overreach_on_flat_ground_at_walk_speed",
          "[g2][gait]") {
    // The rung's own acceptance test, off the PURE pipeline: on flat,
    // hardpack ground, at an ordinary, SELF-CONSISTENT walk speed (see the
    // fixture-trap banner above -- no turning, G5's own scope, not this
    // one's), the settled pelvis drop must keep every STANCE foot's
    // hip-to-target distance inside render's own 0.98*reach safety net, so
    // that net is never the thing doing the work for a PLANTED leg (a
    // clamped stance target is the literal straight-kneed-lunge defect this
    // rung exists to kill). Swing is a separate, wider excursion by
    // construction (the blend spans a FULL stride between consecutive
    // plants of the SAME foot, not the half-stride a stance sees) and is not
    // graded here -- a bent, lifted swing leg reading as fully extended for
    // an instant is not the visual defect either.
    const double walk_speed = 1.5;
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    sim::WalkerParams p = dials();
    p.speed_hardpack_mps = walk_speed;  // self-consistent with flat_walker below
    sim::WalkerState w = flat_walker(p, walk_speed);
    const sim::GaitBodyGeom body;  // the measured mirror, shipped defaults

    sim::GaitState g;
    const double dt = 1.0 / 120.0;
    double worst_stance = 0.0;
    for (int i = 0; i < 6000; ++i) {
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        g = sim::step_gait(g, w, p, f, dt, body);
        if (i > 3000) {  // after the spring has long settled
            const double r = glm::length(w.pos);
            const glm::dvec3 up = w.pos / r;
            const glm::dvec3 fwdn =
                glm::normalize(w.heading - glm::dot(w.heading, up) * up);
            const glm::dvec3 left = glm::normalize(glm::cross(up, fwdn));
            for (int s = 0; s < 2; ++s) {
                if (!g.foot[s].in_stance) continue;
                const double side_sign = (s == 0) ? 1.0 : -1.0;
                const glm::dvec3 hip =
                    w.pos +
                    up * (body.pelvis_rest_h_m - p.lie_clearance_m -
                          g.pelvis_drop_m) +
                    side_sign * body.hip_half_span_m * left;
                worst_stance = std::max(
                    worst_stance, glm::length(hip - g.foot[s].target_w));
            }
        }
    }
    REQUIRE(worst_stance < 0.98 * body.leg_reach_m);
}

TEST_CASE("g2_vertical_bob_period_and_phase", "[g2][gait]") {
    // Two dips per full phase cycle (period 0.5 -- half the stride period),
    // and at the walk end (stance_frac 0.68, run_blend_t == 0) the dip sits
    // at MID-STANCE (ds/2 into the stance window), not at contact.
    const double ds = 0.68;
    const double bob_walk = 0.035, bob_run = 0.09;
    const double off = 0.5 * ds;

    double min_v = 1.0e9, min_phase = -1.0;
    for (int i = 0; i < 2000; ++i) {
        const double ph = i / 2000.0;
        const double v = sim::gait_vertical_bob_m(ph, ds, bob_walk, bob_run);
        if (v < min_v) {
            min_v = v;
            min_phase = ph;
        }
    }
    REQUIRE(std::abs(min_phase - off) < 0.01);
    REQUIRE(min_v < 0.0);  // the LOW point is a drop, not a rise

    // Period 0.5: a phase and phase+0.5 must read identically -- both feet's
    // dip looks the same, which is what distinguishes bob from sway below.
    for (double ph : {0.05, 0.13, 0.31, 0.47}) {
        const double a = sim::gait_vertical_bob_m(ph, ds, bob_walk, bob_run);
        const double b =
            sim::gait_vertical_bob_m(ph + 0.5, ds, bob_walk, bob_run);
        REQUIRE(std::abs(a - b) < 1.0e-9);
    }
}

TEST_CASE("g2_lateral_sway_sign_toward_stance_foot_for_both_feet",
          "[g2][gait]") {
    // ★ THE DRUNK TEST. Foot index 1 ("right", `side_sign -1`, sim/gait.h's
    // own convention) is in stance at phase in [0, ds); its own mid-stance
    // is `phase = ds/2`, and the sway there must be NEGATIVE (toward
    // `-left`, matching index 1's own side). Foot index 0 ("left") is in
    // stance half a cycle later, mid-stance `ds/2 + 0.5`, and the sway there
    // must be POSITIVE. Getting either sign backwards independently is the
    // defect this leg exists to catch.
    const double ds = 0.68;
    const double sway_walk = 0.045, sway_run = 0.02;
    const double sway_right_mid =
        sim::gait_lateral_sway_m(0.5 * ds, ds, sway_walk, sway_run);
    const double sway_left_mid = sim::gait_lateral_sway_m(
        0.5 * ds + 0.5, ds, sway_walk, sway_run);
    REQUIRE(sway_right_mid < 0.0);
    REQUIRE(sway_left_mid > 0.0);
    // Same walk, mirrored: equal magnitude.
    REQUIRE(std::abs(sway_right_mid + sway_left_mid) < 1.0e-9);
}

TEST_CASE("g2_trendelenburg_roll_opposes_the_sway_at_every_phase",
          "[g2][gait]") {
    // At the same instant the pelvis sways toward the stance foot, it must
    // tip toward the SWING side -- opposite sign, always, by construction
    // (gait.cpp). Getting either sign wrong on its own is the drunk test;
    // this leg catches the two signs agreeing when they must not.
    const double ds = 0.68;
    const double roll_amp = 0.026179939;
    const double sway_walk = 0.045, sway_run = 0.02;
    for (double ph : {0.1, 0.3, 0.55, 0.8}) {
        const double sway =
            sim::gait_lateral_sway_m(ph, ds, sway_walk, sway_run);
        const double roll =
            sim::gait_trendelenburg_roll_rad(ph, ds, roll_amp);
        REQUIRE(sway * roll <= 0.0);
    }
}

TEST_CASE("g2_ruled_run_speed_and_flight_phase", "[g2][walker]") {
    // Chad's ruling, PLAN §1.1: "RUN = 5-6 m/s on hard pack." The mid/deep
    // anchors are untouched -- this is the ONLY number the ruling moves.
    const sim::WalkerParams p = dials();
    REQUIRE(p.speed_hardpack_mps == 5.5);
    REQUIRE(p.speed_mid_mps == 1.6);
    REQUIRE(p.speed_deep_mps == 0.6);

    // A REAL flight phase at the new cap: stance_frac < 0.5 means a foot
    // spends less than half the cycle planted, i.e. there is a double-float
    // phase -- running, not walking. (The measured curve saturates at its
    // jog-ceiling value for every speed at or past 3.5 m/s; that ceiling
    // value already clears this bar, unextended -- see walker.h's banner.)
    const double ds_at_cap = sim::walker_stance_frac(p, p.speed_hardpack_mps);
    REQUIRE(ds_at_cap < 0.5);

    // Continuity: `row3`'s own piecewise-linear read is exact at every
    // anchor by construction regardless of an endpoint's value, but this
    // pins it as MEASURED behaviour rather than an assumption -- no jump
    // anywhere in depth, including across the mid anchor the ruling did not
    // touch.
    double prev = sim::walker_speed_cap(p, 0.0);
    for (int i = 1; i <= 200; ++i) {
        const double d = p.depth_deep_m * i / 200.0;
        const double v = sim::walker_speed_cap(p, d);
        REQUIRE(std::abs(v - prev) < 0.06);
        prev = v;
    }
}

TEST_CASE("g2_pelvis_terms_frozen_while_riding", "[g2][gait]") {
    // ★ THE RENDER GATE, MIRRORED SIM-SIDE. `render/sled_model.cpp`'s pelvis
    // channel gates every G2 term on `gait_on && !welded` so the SIGNED
    // riding pose can never move -- but that file is render-only and outside
    // ctest's reach. The guarantee it RESTS ON is testable here:
    // `step_gait` runs every tick regardless of mode (app/main.cpp calls it
    // unconditionally beside `step_walker`), and while Riding the walker has
    // never been thrown -- `w.pos` is the degenerate origin a bare
    // `WalkerState{}` leaves it at -- so `step_gait`'s own "nothing to plant
    // against" early return must hand `prev` back byte-for-byte, G2's new
    // fields included. A leg that fails if that early return is ever
    // narrowed to skip the new fields.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    const sim::WalkerState w;  // WalkerState{}: mode Riding, pos = (0,0,0)

    sim::GaitState prev;
    prev.pelvis_drop_m = 0.31;
    prev.pelvis_drop_vel_mps = 0.02;
    prev.bob_m = -0.01;
    prev.sway_m = 0.02;
    prev.roll_rad = 0.005;

    const sim::GaitState out = sim::step_gait(prev, w, p, f, 1.0 / 120.0);
    REQUIRE(out.pelvis_drop_m == prev.pelvis_drop_m);
    REQUIRE(out.pelvis_drop_vel_mps == prev.pelvis_drop_vel_mps);
    REQUIRE(out.bob_m == prev.bob_m);
    REQUIRE(out.sway_m == prev.sway_m);
    REQUIRE(out.roll_rad == prev.roll_rad);
}

TEST_CASE("g2_run_crouch_is_a_posture_not_a_burial", "[g2][gait]") {
    // ★ G2e (Chad's drive: "walking underneath the road up to my waist"):
    // at the RULED 5.5 m/s run the first cut's drop law chased a
    // horizontally-unreachable pin all the way to foot level (v_comp),
    // and the mis-centred plant (a flat 0.5*stride, walk-duty thinking)
    // made that the COMMON case, not the edge. Two claims, one leg:
    //  (a) the pelvis drop NEVER exceeds the 0.35 m posture cap, and
    //  (b) with the plant centred by duty ((1 - 0.5*ds)*stride) the drop
    //      SETTLES well under the cap -- a crouching run, not a squat
    //      pinned at its own ceiling.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, p.speed_hardpack_mps);
    sim::GaitState g;
    const double dt = 1.0 / 120.0;
    double drop_late_max = 0.0;
    for (int i = 0; i < 4000; ++i) {
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        g = sim::step_gait(g, w, p, f, dt);
        REQUIRE(g.pelvis_drop_m <= 0.351);  // (a) the cap holds, always
        if (i > 2000) drop_late_max = std::max(drop_late_max, g.pelvis_drop_m);
    }
    REQUIRE(drop_late_max < 0.15);  // (b) an upright run: the crouch is a
                                    // walk law, faded out by duty (G2e)
}

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2i -- THE SHIFT KEY (jump / sprint / dash). Chad: "make the
// shift key jump press and also speed up his run a tad while pressing and
// holding; for about 5 seconds a dash will start on a jump press when landing
// from it" + "sprint only if shift is held."
//
// ★ EACH LEG FAILS FOR ITS OWN REASON (L-GATE): the arc leg grades the
// BALLISTIC (apex against v0^2/2g, symmetry, exactly one landing), the sprint
// leg grades the LEVEL read (held = fast, released = walk, and no latch), the
// charge leg grades the 5-second arm and that a short hold does NOT arm, the
// dash leg drives the whole gesture end to end and proves the burst appears
// only at the LANDING and decays, the no-double-jump leg proves a press in
// mid-air is inert, the airborne leg proves both feet leave the ground and
// ride up with him, and the frozen-walk leg proves an untouched Shift key
// leaves `step_gait` bit-identical to what it did before this rung.

TEST_CASE("g2i_jump_arc_is_ballistic_and_lands_once", "[g2i][gait]") {
    const sim::HopParams p;
    const double dt = 1.0 / 120.0;
    sim::HopState h;
    sim::HopInputs in;
    in.shift_press = true;
    in.shift_down = true;
    h = sim::step_hop(h, in, dt, p);
    REQUIRE(h.airborne);
    REQUIRE(h.launched);
    in.shift_press = false;  // the key is HELD, not re-pressed
    double apex = 0.0;
    int landings = 0, air_steps = 0;
    for (int i = 0; i < 600; ++i) {
        h = sim::step_hop(h, in, dt, p);
        apex = std::max(apex, h.height_m);
        if (h.landed) ++landings;
        if (h.airborne) ++air_steps;
        REQUIRE(h.height_m >= 0.0);  // he never goes through the snow
    }
    // The apex is v0^2/2g to within one integrator step (semi-implicit Euler
    // undershoots by ~g*dt^2/2, which is well under a centimetre here).
    const double want =
        p.jump_speed_mps * p.jump_speed_mps / (2.0 * p.gravity_mps2);
    REQUIRE(std::fabs(apex - want) < 0.02);
    REQUIRE(landings == 1);  // exactly ONE landing per jump
    REQUIRE(!h.airborne);    // and he is back on the ground
    // Flight time is 2*v0/g -- the arc is symmetric, not a hang.
    const double t_air = static_cast<double>(air_steps) * dt;
    REQUIRE(std::fabs(t_air - 2.0 * p.jump_speed_mps / p.gravity_mps2) < 0.05);
}

TEST_CASE("g2i_a_press_in_mid_air_is_not_a_second_jump", "[g2i][gait]") {
    const sim::HopParams p;
    const double dt = 1.0 / 120.0;
    sim::HopState h;
    sim::HopInputs in;
    in.shift_press = true;
    in.shift_down = true;
    h = sim::step_hop(h, in, dt, p);
    double v_prev = h.vert_vel_mps;
    // Press again, hard, while he is still climbing.
    for (int i = 0; i < 10; ++i) {
        h = sim::step_hop(h, in, dt, p);  // shift_press stays true
        REQUIRE(h.vert_vel_mps < v_prev);  // gravity, never a re-boost
        REQUIRE(!h.launched);
        v_prev = h.vert_vel_mps;
    }
}

TEST_CASE("g2i_sprint_is_the_held_level_and_never_latches", "[g2i][gait]") {
    const sim::HopParams p;
    sim::HopState h;
    sim::HopInputs down, up;
    down.shift_down = true;
    // Held: the multiplier is the sprint's, exactly -- "a tad", one number.
    REQUIRE(sim::hop_speed_mul(h, down, p) == Catch::Approx(p.sprint_cap_mul));
    // Released: back to 1.0 on the very next read, no decay and no latch.
    REQUIRE(sim::hop_speed_mul(h, up, p) == Catch::Approx(1.0));
    // And the multiplier is what makes the RULED 5.5 m/s hardpack cap a
    // sprint: the shape in depth is untouched, every anchor scales the same.
    sim::WalkerParams wp = dials();
    const double walk_cap = sim::walker_speed_cap(wp, 0.0);
    wp.speed_hardpack_mps *= p.sprint_cap_mul;
    wp.speed_mid_mps *= p.sprint_cap_mul;
    wp.speed_deep_mps *= p.sprint_cap_mul;
    REQUIRE(sim::walker_speed_cap(wp, 0.0) ==
            Catch::Approx(walk_cap * p.sprint_cap_mul));
    REQUIRE(sim::walker_speed_cap(wp, 0.77) ==
            Catch::Approx(sim::walker_speed_cap(dials(), 0.77) *
                          p.sprint_cap_mul));
    // ... and the STRIDE follows through the shipped path (sqrt in speed), so
    // cadence is not a second law anywhere.
    REQUIRE(sim::walker_stride(wp, 0.0) >
            sim::walker_stride(dials(), 0.0) * 1.05);
}

TEST_CASE("g2i_the_dash_charges_in_five_seconds_and_not_before",
          "[g2i][gait]") {
    const sim::HopParams p;
    const double dt = 1.0 / 120.0;
    sim::HopInputs down;
    down.shift_down = true;
    {  // a SHORT hold does not arm
        sim::HopState h;
        for (int i = 0; i < static_cast<int>((p.dash_charge_s - 0.5) / dt); ++i)
            h = sim::step_hop(h, down, dt, p);
        REQUIRE(!h.dash_armed);
    }
    {  // the full hold does
        sim::HopState h;
        for (int i = 0; i < static_cast<int>((p.dash_charge_s + 0.1) / dt); ++i)
            h = sim::step_hop(h, down, dt, p);
        REQUIRE(h.dash_armed);
        // ... and letting go for longer than the re-press grace throws it away
        sim::HopInputs up;
        for (int i = 0; i < static_cast<int>((p.dash_arm_grace_s + 0.2) / dt);
             ++i)
            h = sim::step_hop(h, up, dt, p);
        REQUIRE(!h.dash_armed);
        REQUIRE(h.shift_held_s == 0.0);
    }
}

TEST_CASE("g2i_the_dash_starts_on_the_landing_and_decays", "[g2i][gait]") {
    // The WHOLE gesture, as Chad described it: hold Shift ~5 s (sprinting),
    // flick it to re-press (a jump), and the burst arrives when he LANDS.
    const sim::HopParams p;
    const double dt = 1.0 / 120.0;
    sim::HopState h;
    sim::HopInputs down, up;
    down.shift_down = true;
    for (int i = 0; i < static_cast<int>((p.dash_charge_s + 0.2) / dt); ++i)
        h = sim::step_hop(h, down, dt, p);
    REQUIRE(h.dash_armed);
    // The flick: released for a few frames (inside the grace), then pressed.
    for (int i = 0; i < 10; ++i) h = sim::step_hop(h, up, dt, p);
    REQUIRE(h.dash_armed);  // the grace kept it -- see HopParams' own banner
    sim::HopInputs press;
    press.shift_down = true;
    press.shift_press = true;
    h = sim::step_hop(h, press, dt, p);
    REQUIRE(h.airborne);
    REQUIRE(h.jump_carries_dash);
    // ★ THE BURST IS NOT LIVE WHILE HE IS IN THE AIR -- it is a LANDING.
    REQUIRE(sim::hop_speed_mul(h, down, p) == Catch::Approx(p.sprint_cap_mul));
    double peak = 0.0;
    bool landed_once = false;
    double mul_at_landing = 0.0;
    for (int i = 0; i < 600; ++i) {
        h = sim::step_hop(h, down, dt, p);
        const double m = sim::hop_speed_mul(h, down, p);
        if (h.landed) {
            landed_once = true;
            mul_at_landing = m;
        }
        peak = std::max(peak, m);
    }
    REQUIRE(landed_once);
    REQUIRE(mul_at_landing > p.sprint_cap_mul * 1.2);  // a BURST, not a nudge
    REQUIRE(peak == Catch::Approx(p.dash_cap_mul).margin(0.02));
    REQUIRE(!h.dash_armed);  // one charge buys exactly one dash
    // ... and by `dash_decay_s` past the landing it has fallen back onto the
    // sprint he is still holding -- never THROUGH it.
    REQUIRE(sim::hop_speed_mul(h, down, p) == Catch::Approx(p.sprint_cap_mul));
}

TEST_CASE("g2i_airborne_unpins_both_feet_and_lifts_them_with_him",
          "[g2i][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 1.4);
    const double dt = 1.0 / 120.0;
    sim::GaitState g;
    for (int i = 0; i < 200; ++i) {  // settle a normal walk first
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        g = sim::step_gait(g, w, p, f, dt);
    }
    REQUIRE((g.foot[0].in_stance || g.foot[1].in_stance));  // one is planted
    const glm::dvec3 up = glm::normalize(w.pos);
    // The SAME frame, once on the ground and once 0.5 m in the air.
    const sim::GaitState ground = sim::step_gait(g, w, p, f, dt);
    // ★ G2i-b: the RIGID ride is still exactly the rigid ride when nothing
    // is tucking -- `tuck01` defaults to 0, which is G2i's own behaviour.
    const sim::GaitState air =
        sim::step_gait(g, w, p, f, dt, sim::GaitBodyGeom{}, sim::GaitAir{0.5});
    for (int s = 0; s < 2; ++s) {
        REQUIRE(!air.foot[s].in_stance);  // nobody is standing on anything
        // and the target rode UP with him by exactly the hop height
        const double rise =
            glm::dot(air.foot[s].target_w - ground.foot[s].target_w, up);
        REQUIRE(rise == Catch::Approx(0.5).margin(0.02));
    }
    REQUIRE(air.pelvis_drop_m <= ground.pelvis_drop_m);  // no crouch in flight
}

TEST_CASE("g2i_an_untouched_shift_key_changes_nothing", "[g2i][gait]") {
    // The regression that matters most: G2h's walk, unchanged. `hop.height_m`
    // is 0 on every frame nobody pressed anything, and `step_gait` at 0 is the
    // same function it was.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.30);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 1.1);
    const double dt = 1.0 / 120.0;
    sim::GaitState a, b;
    sim::HopState h;
    const sim::HopInputs none;
    for (int i = 0; i < 900; ++i) {
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        h = sim::step_hop(h, none, dt, sim::HopParams{});
        REQUIRE(h.height_m == 0.0);
        REQUIRE(sim::hop_speed_mul(h, none, sim::HopParams{}) ==
                Catch::Approx(1.0));
        a = sim::step_gait(a, w, p, f, dt);
        // ★ G2i-b: the WHOLE air value, tuck and lead knee included -- an
        // untouched Shift key must leave every one of them inert.
        REQUIRE(h.tuck01 == 0.0);
        sim::GaitAir air;
        air.height_m = h.height_m;
        air.tuck01 = h.tuck01;
        air.lead_side = h.lead_side;
        b = sim::step_gait(b, w, p, f, dt, sim::GaitBodyGeom{}, air);
        REQUIRE(glm::length(a.foot[0].target_w - b.foot[0].target_w) < 1e-12);
        REQUIRE(glm::length(a.foot[1].target_w - b.foot[1].target_w) < 1e-12);
        REQUIRE(a.pelvis_drop_m == Catch::Approx(b.pelvis_drop_m));
    }
}

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2i-b -- THE JUMP'S AIR POSE. Chad, flying G2i: "jump
// should have a knee forward or both knees bent a bit while in the air
// because they kind of hang back there right now... let's fix the jump to
// make it look a little more believable."
//
// Four legs, each failing for its own reason: the SHAPE of the tuck against
// the arc (zero at both ends, full over the top, never discontinuous), the
// POSTURE it produces (both knees inside the chain, the lead knee out in
// front, the trail foot under the hips, neither boot below the snow), the
// LEAD side (derived from the phase he was walking at, and latched for the
// whole hop), and the 0-case (tuck_gain 0 reproduces G2i's rigid ride bit
// for bit through a real hop).

TEST_CASE("g2ib_the_tuck_is_the_height_zero_at_both_ends_full_over_the_top",
          "[g2ib][gait]") {
    const sim::HopParams p;
    const double dt = 1.0 / 120.0;
    sim::HopState h;
    sim::HopInputs press;
    press.shift_press = true;
    h = sim::step_hop(h, press, dt, p);
    REQUIRE(h.airborne);
    // One frame off the ground: he has pushed off a nearly straight leg.
    REQUIRE(h.tuck01 < 0.05);

    double peak = 0.0, peak_h = 0.0, last = h.tuck01, max_jump = 0.0;
    double tuck_at_top = -1.0, tuck_before_landing = -1.0;
    const sim::HopInputs none;
    for (int i = 0; i < 400 && h.airborne; ++i) {
        const double prev_h = h.height_m;
        h = sim::step_hop(h, none, dt, p);
        max_jump = std::max(max_jump, std::fabs(h.tuck01 - last));
        last = h.tuck01;
        if (h.airborne) {
            if (h.height_m > peak_h) { peak_h = h.height_m; }
            if (h.height_m < prev_h && tuck_before_landing < 0.0 &&
                h.height_m < 0.06)
                tuck_before_landing = h.tuck01;
            if (h.vert_vel_mps > 0.0 && h.vert_vel_mps < 0.15)
                tuck_at_top = h.tuck01;
            peak = std::max(peak, h.tuck01);
        }
    }
    REQUIRE(!h.airborne);
    // FULL over the top ...
    REQUIRE(tuck_at_top > 0.98);
    REQUIRE(peak == Catch::Approx(1.0).margin(0.01));
    // ... and UNFOLDED again before the boots meet the snow, so the landing
    // re-enters stance out of an already-extending leg rather than snapping.
    REQUIRE(tuck_before_landing >= 0.0);
    REQUIRE(tuck_before_landing < 0.30);
    REQUIRE(h.tuck01 == 0.0);  // and it is exactly 0 the moment he is down
    // NOTHING POPS: no single 1/120 s step moves the tuck more than a few
    // percent (the whole reason it is driven off a parabola's own height).
    REQUIRE(max_jump < 0.08);
}

TEST_CASE("g2ib_the_air_pose_bends_both_knees_and_the_lead_knee_leads",
          "[g2ib][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 1.4);
    // ★ THIS LEG NEEDS THE REAL CONVENTION, AND THE SHARED FIXTURE DOES NOT
    // CARRY IT: `step_walker` pins `pos` at `drive_r + lie_clearance_m` --
    // his DRAWN ORIGIN, ~0.564 m above the surface, never his feet (the
    // defect `step_gait`'s own drop-law banner records). `flat_walker` puts
    // him at exactly `kR`, which is fine for every leg that only reads
    // directions, and 0.564 m wrong for one that asks HOW HIGH A TUCKED BOOT
    // IS. The ground samples are by DIRECTION, so raising him here changes
    // nothing else in the fixture.
    w.pos = glm::dvec3(kR + p.lie_clearance_m, 0.0, 0.0);
    const double dt = 1.0 / 120.0;
    sim::GaitState g;
    for (int i = 0; i < 200; ++i) {  // an ordinary settled walk first
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        g = sim::step_gait(g, w, p, f, dt);
    }
    const glm::dvec3 up = glm::normalize(w.pos);
    const glm::dvec3 fwd = glm::normalize(w.heading -
                                          glm::dot(w.heading, up) * up);
    const glm::dvec3 left = glm::normalize(glm::cross(up, fwd));
    const sim::GaitBodyGeom body;
    const sim::AirTuckParams tk;
    // The measured boot-sole -> ankle height the render consumer adds on top
    // of every target (flak::GunnerAnthro::ankle_up_m). The chain the solver
    // drives ends at the ANKLE, so that is where "can a knee do this" is
    // asked -- re-stated here rather than included, so this leg fails if the
    // POSTURE stops being reachable, not if a header moves.
    constexpr double kAnkleUp = 0.10;

    const sim::GaitState ground = sim::step_gait(g, w, p, f, dt);
    sim::GaitAir air;
    air.height_m = 0.5;
    air.tuck01 = 1.0;
    air.lead_side = 0;  // left leads
    const sim::GaitState tuck =
        sim::step_gait(g, w, p, f, dt, body, air);

    for (int s = 0; s < 2; ++s) {
        const double side_sign = (s == 0) ? 1.0 : -1.0;
        const glm::dvec3 hip =
            w.pos +
            up * (body.pelvis_rest_h_m - p.lie_clearance_m + air.height_m) +
            side_sign * body.hip_half_span_m * left;
        const glm::dvec3 ankle = tuck.foot[s].target_w + up * kAnkleUp;
        const glm::dvec3 d = ankle - hip;
        const double down = -glm::dot(d, up);
        const double along = glm::dot(d, fwd);

        // BOTH KNEES BEND -- but only a HAPPY-MEDIUM bend (Chad's re-ruling:
        // "just a slight bend in the knees... not knees up to the nipple
        // line"): the ankle sits inside the 0.908 m chain so the two-bone
        // solve must fold, and stays short of the 0.98*reach clamp a
        // straight strut rides.
        const double reach = glm::length(d);
        REQUIRE(reach < 0.93 * body.leg_reach_m);
        // ... and each foot is measurably higher under him than the ~0.88 m
        // a standing ankle hangs below the hip -- a bend, not a dangle.
        REQUIRE(down < body.pelvis_rest_h_m - kAnkleUp - 0.04);

        // NO LEG THROUGH THE GROUND: every tucked foot is above the target it
        // would have had standing on the snow.
        REQUIRE(glm::dot(tuck.foot[s].target_w - ground.foot[s].target_w,
                         up) > 0.30);
        REQUIRE(!tuck.foot[s].in_stance);

        if (s == air.lead_side) {
            REQUIRE(along > 0.20);   // the lead knee DRIVES FORWARD
            REQUIRE(down == Catch::Approx(tk.lead_drop_m - kAnkleUp));
        } else {
            REQUIRE(along < 0.0);    // the trail foot tucks UNDER the hips
            REQUIRE(down == Catch::Approx(tk.trail_drop_m - kAnkleUp));
        }
    }
    // The lead knee is genuinely AHEAD of the trail one, whichever way round
    // the sides are -- the asymmetry Chad asked for, not two mirrored legs.
    const double lead_along =
        glm::dot(tuck.foot[air.lead_side].target_w -
                     tuck.foot[1 - air.lead_side].target_w, fwd);
    REQUIRE(lead_along > 0.30);

    // AND IT EASES: sweeping the tuck from the rigid ride to the full pose
    // never moves a target more than a centimetre a step, so the fold cannot
    // pop on the way in or on the way out.
    glm::dvec3 last[2] = {glm::dvec3(0.0), glm::dvec3(0.0)};
    for (int i = 0; i <= 100; ++i) {
        sim::GaitAir a2 = air;
        a2.tuck01 = i / 100.0;
        const sim::GaitState st = sim::step_gait(g, w, p, f, dt, body, a2);
        for (int s = 0; s < 2; ++s) {
            if (i > 0)
                REQUIRE(glm::length(st.foot[s].target_w - last[s]) < 0.012);
            last[s] = st.foot[s].target_w;
        }
    }
}

TEST_CASE("g2ib_the_lead_knee_is_the_swinging_leg_and_it_is_latched",
          "[g2ib][gait]") {
    const sim::HopParams p;
    const double dt = 1.0 / 120.0;
    // `step_gait`'s crossover: foot 1 (right) runs at phase+0.0, foot 0
    // (left) at phase+0.5. Early in the cycle the right foot is planted and
    // the LEFT is the one in the air, so the left knee leads -- and the other
    // way round past the half cycle.
    for (int k = 0; k < 2; ++k) {
        sim::HopInputs press;
        press.shift_press = true;
        press.gait_phase01 = (k == 0) ? 0.20 : 0.70;
        sim::HopState h = sim::step_hop(sim::HopState{}, press, dt, p);
        REQUIRE(h.airborne);
        REQUIRE(h.lead_side == (k == 0 ? 0 : 1));
        // ... AND IT IS LATCHED: the walker keeps striding under him and the
        // phase keeps turning, but the knee that came through stays the knee
        // that came through for the whole hop.
        const int at_launch = h.lead_side;
        sim::HopInputs air;
        for (int i = 0; i < 400 && h.airborne; ++i) {
            air.gait_phase01 = std::fmod(0.20 + 0.013 * i, 1.0);
            h = sim::step_hop(h, air, dt, p);
            REQUIRE(h.lead_side == at_launch);
        }
        REQUIRE(!h.airborne);
    }
}

TEST_CASE("g2ib_tuck_gain_zero_is_g2i_bit_for_bit_through_a_whole_hop",
          "[g2ib][gait]") {
    // The 0-case, through a REAL arc rather than a hand-set number: with the
    // gain off, every frame of the hop publishes exactly the rigid ride G2i
    // shipped -- so the dial that turns the air pose off turns it ALL the way
    // off, and Chad can A/B against the thing he flew.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.30);
    const sim::WalkerParams p = dials();
    sim::WalkerState w = flat_walker(p, 1.1);
    const double dt = 1.0 / 120.0;
    sim::HopParams hp;
    hp.tuck_gain = 0.0;
    sim::HopState h;
    sim::HopInputs press;
    press.shift_press = true;
    h = sim::step_hop(h, press, dt, hp);
    sim::GaitState a, b;
    const sim::HopInputs none;
    int air_frames = 0;
    while (h.airborne) {
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
        REQUIRE(h.tuck01 == 0.0);
        sim::GaitAir air;
        air.height_m = h.height_m;
        air.tuck01 = h.tuck01;
        air.lead_side = h.lead_side;
        a = sim::step_gait(a, w, p, f, dt, sim::GaitBodyGeom{},
                           sim::GaitAir{h.height_m});  // G2i's own call
        b = sim::step_gait(b, w, p, f, dt, sim::GaitBodyGeom{}, air);
        REQUIRE(glm::length(a.foot[0].target_w - b.foot[0].target_w) < 1e-12);
        REQUIRE(glm::length(a.foot[1].target_w - b.foot[1].target_w) < 1e-12);
        ++air_frames;
        h = sim::step_hop(h, none, dt, hp);
    }
    REQUIRE(air_frames > 60);  // a real ~0.65 s hop was actually flown
}

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2i-d -- THE DESCENT BELONGS TO THE JUMP. Chad, flying
// G2i-c: "when he is going to land his feet are swinging and trailing back --
// I didn't want that at all. I want it to look like a normal jump is all."
//
// Two legs, each failing for its own reason: the DESCENT (the gait phase is
// gone from it, nothing hangs behind the hip, and the ASCENT is unchanged
// bit-for-bit), and the TOUCHDOWN (no frame snaps across it, and the first
// steps after it are a walk).

TEST_CASE("g2id_the_descent_hands_the_legs_to_the_landing_not_the_stride",
          "[g2id][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    const sim::GaitBodyGeom body;
    const sim::AirLandParams land;
    constexpr double kAnkleUp = 0.10;  // flak::GunnerAnthro::ankle_up_m
    const double dt = 1.0 / 120.0;

    // THREE MEN ON ONE ARC. Arms 0 and 1 differ ONLY in the stride they are
    // part-way through -- the one thing the old descent depended on, since it
    // blended the tuck against the LIVE pin/plant swing. Arm 2 is arm 0 fed
    // G2i-c's own air value (no `vert_vel_mps`, so `land01` is 0 for the
    // whole flight): the ascent must match it exactly, or this rung has
    // regressed the half Chad already accepted.
    sim::WalkerState w[3];
    sim::GaitState g[3];
    sim::HopState h[3];
    const double seed_phase[3] = {0.13, 0.61, 0.13};
    for (int k = 0; k < 3; ++k) {
        w[k] = flat_walker(p, 1.4);
        // The real convention (see the G2i-b posture leg): `step_walker` pins
        // `pos` at `drive_r + lie_clearance_m`, not at the surface.
        w[k].pos = glm::dvec3(kR + p.lie_clearance_m, 0.0, 0.0);
        w[k].gait_phase = seed_phase[k];
    }
    for (int i = 0; i < 200; ++i)
        for (int k = 0; k < 3; ++k) {
            w[k].pos += w[k].vel * dt;
            w[k].gait_phase += glm::length(w[k].vel) * dt / w[k].stride_m;
            w[k].gait_phase -= std::floor(w[k].gait_phase);
            g[k] = sim::step_gait(g[k], w[k], p, f, dt);
        }

    const sim::HopParams hp;
    sim::HopInputs press;
    press.shift_press = true;
    for (int k = 0; k < 3; ++k) h[k] = sim::step_hop(h[k], press, dt, hp);
    const sim::HopInputs none;

    int descent_frames = 0, ascent_frames = 0;
    double worst_behind = 0.0, worst_reach = 0.0;
    double worst_stride_disagreement = 0.0, worst_ascent_drift = 0.0;
    while (h[0].airborne) {
        for (int k = 0; k < 3; ++k) {
            w[k].pos += w[k].vel * dt;
            w[k].gait_phase += glm::length(w[k].vel) * dt / w[k].stride_m;
            w[k].gait_phase -= std::floor(w[k].gait_phase);
            sim::GaitAir air;
            air.height_m = h[k].height_m;
            air.tuck01 = h[k].tuck01;
            // ★ THE SAME LEAD KNEE FOR ALL THREE. `step_hop` latches the lead
            // off the launch phase, and arm 1 launches at a different one --
            // a legitimate difference that would mask the one being graded.
            air.lead_side = 1;
            if (k != 2) air.vert_vel_mps = h[k].vert_vel_mps;
            g[k] = sim::step_gait(g[k], w[k], p, f, dt, body, air);
        }
        const glm::dvec3 up = glm::normalize(w[0].pos);
        const glm::dvec3 fwd =
            glm::normalize(w[0].heading - glm::dot(w[0].heading, up) * up);
        const glm::dvec3 left = glm::normalize(glm::cross(up, fwd));
        if (h[0].vert_vel_mps > 0.0) {
            ++ascent_frames;
            for (int s = 0; s < 2; ++s)
                worst_ascent_drift = std::max(
                    worst_ascent_drift,
                    glm::length(g[0].foot[s].target_w -
                                g[2].foot[s].target_w));
        } else {
            ++descent_frames;
            for (int s = 0; s < 2; ++s) {
                const double side_sign = (s == 0) ? 1.0 : -1.0;
                const glm::dvec3 hip =
                    w[0].pos +
                    up * (body.pelvis_rest_h_m - p.lie_clearance_m +
                          h[0].height_m) +
                    side_sign * body.hip_half_span_m * left;
                const glm::dvec3 d = g[0].foot[s].target_w + up * kAnkleUp - hip;
                worst_behind = std::min(worst_behind, glm::dot(d, fwd));
                worst_reach = std::max(worst_reach, glm::length(d));
                // Once the fall is at full speed the landing pose owns the
                // legs OUTRIGHT, so the two strides must publish the same
                // world point. The tolerance is a micron: these are
                // planet-radius doubles, so "identical" cannot be spelled
                // 0.0, and a stride's worth of disagreement is a metre.
                if (h[0].vert_vel_mps <= -land.full_at_fall_mps)
                    worst_stride_disagreement = std::max(
                        worst_stride_disagreement,
                        glm::length(g[0].foot[s].target_w -
                                    g[1].foot[s].target_w));
            }
        }
        for (int k = 0; k < 3; ++k) h[k] = sim::step_hop(h[k], none, dt, hp);
    }

    REQUIRE(ascent_frames > 20);
    REQUIRE(descent_frames > 20);
    // ★ THE ASCENT IS UNTOUCHED, bit for bit: he still leaves the ground off
    // the stride he had.
    REQUIRE(worst_ascent_drift == 0.0);
    // ★ THE DESCENT NO LONGER KNOWS WHAT A STRIDE IS.
    REQUIRE(worst_stride_disagreement < 1.0e-6);
    // ★ AND NOTHING TRAILS. The most a boot may sit behind its own hip on the
    // way down is the tuck's own `trail_back` (0.08 m) -- a heel a hand's
    // width back, never a leg swung out behind him.
    REQUIRE(worst_behind > -0.10);
    // ... on a leg the two-bone solver can still fold: short of the 0.98
    // clamp a straight strut rides (L-MEASURE).
    REQUIRE(worst_reach < 0.98 * body.leg_reach_m);
}

TEST_CASE("g2id_the_touchdown_lands_where_the_boots_are_and_walks_on",
          "[g2id][gait]") {
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    const sim::GaitBodyGeom body;
    const double dt = 1.0 / 120.0;
    sim::WalkerState w = flat_walker(p, 1.4);
    w.pos = glm::dvec3(kR + p.lie_clearance_m, 0.0, 0.0);
    w.gait_phase = 0.44;  // deliberately mid-cycle at the launch
    sim::GaitState g;
    const auto advance = [&]() {
        w.pos += w.vel * dt;
        w.gait_phase += glm::length(w.vel) * dt / w.stride_m;
        w.gait_phase -= std::floor(w.gait_phase);
    };
    for (int i = 0; i < 200; ++i) {
        advance();
        g = sim::step_gait(g, w, p, f, dt);
    }
    // The walk's OWN worst per-frame foot travel, measured on this exact
    // fixture rather than guessed -- the landing is graded against what
    // walking already does, so this leg cannot pass by being generous.
    double walk_worst = 0.0;
    for (int i = 0; i < 300; ++i) {
        const sim::GaitState pre = g;
        advance();
        g = sim::step_gait(g, w, p, f, dt);
        for (int s = 0; s < 2; ++s)
            walk_worst = std::max(walk_worst,
                                  glm::length(g.foot[s].target_w -
                                              pre.foot[s].target_w));
    }
    REQUIRE(walk_worst > 0.0);

    const sim::HopParams hp;
    sim::HopState h;
    sim::HopInputs press;
    press.shift_press = true;
    press.gait_phase01 = w.gait_phase;
    h = sim::step_hop(h, press, dt, hp);
    const sim::HopInputs none;
    double hop_worst = 0.0;
    int air_frames = 0;
    bool landed_seen = false;
    double land_along[2] = {0.0, 0.0};
    // Fly the arc, then keep walking for a full second and a half so the
    // steps that FOLLOW the landing are graded too.
    for (int i = 0; i < 260; ++i) {
        const sim::GaitState pre = g;
        advance();
        sim::GaitAir air;
        air.height_m = h.height_m;
        air.tuck01 = h.tuck01;
        air.lead_side = h.lead_side;
        air.vert_vel_mps = h.vert_vel_mps;
        g = sim::step_gait(g, w, p, f, dt, body, air);
        if (h.airborne) ++air_frames;
        if (h.landed) {
            landed_seen = true;
            // Measured ON the touchdown frame, against the man as he was
            // THEN -- he keeps walking for another second and a half below.
            const glm::dvec3 up = glm::normalize(w.pos);
            const glm::dvec3 fwd = glm::normalize(
                w.heading - glm::dot(w.heading, up) * up);
            for (int s = 0; s < 2; ++s)
                land_along[s] =
                    glm::dot(g.foot[s].target_w - w.pos, fwd);
        }
        for (int s = 0; s < 2; ++s)
            hop_worst = std::max(hop_worst, glm::length(g.foot[s].target_w -
                                                       pre.foot[s].target_w));
        h = sim::step_hop(h, none, dt, hp);
    }
    REQUIRE(air_frames > 60);
    REQUIRE(landed_seen);
    // ★ NO SNAP, ANYWHERE ON THE ARC OR AFTER IT -- graded against what
    // WALKING already does on this same fixture, so the leg cannot pass by
    // being generous. Before this rung the PUSH-OFF alone moved a foot 1.10 m
    // in one 1/120 s frame (a mid-stance foot thrown into the swing branch at
    // whatever blend parameter its phase sat at) -- nineteen times a walking
    // step's fastest frame, an outright teleport, and the touchdown had its
    // own. What is left is the honest one: a boot the landing catches
    // mid-swing has to cover the rest of its step in the time the phase has
    // left, so it hustles for about a tenth of a second. That is a man
    // catching his balance, and it is bounded.
    REQUIRE(hop_worst < 2.6 * walk_worst);
    // ★ AND HE LANDS UNDER HIMSELF: on the touchdown frame both boots are
    // beneath their own hips, not out behind him.
    for (int s = 0; s < 2; ++s) {
        REQUIRE(land_along[s] > -0.15);
        REQUIRE(land_along[s] < 0.35);
    }
}

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2j -- THE PUMP-REPAIR WORK ANIMATION. Chad: "a holding
// still out front with his left hand and a torquing of a wrench in another,
// then a hammer fist with the right as the settling of the machine blow."

TEST_CASE("g2j_the_wrench_stroke_is_a_ratchet_not_a_sine", "[g2j][gait]") {
    // A ratchet's LOADED half is slower than its reset. Grade that directly:
    // the stroke travels the same 2 units each way, but spends more of the
    // cycle doing it on the pull -- and it is continuous and bounded.
    constexpr double tf = 0.62;
    double prev = sim::repair_wrench_stroke(0.0, tf);
    double pull_travel = 0.0, reset_travel = 0.0;
    double worst_jump = 0.0;
    for (int i = 1; i <= 2000; ++i) {
        const double u = static_cast<double>(i) / 2000.0;
        const double v = sim::repair_wrench_stroke(u, tf);
        REQUIRE(v >= -1.0001);
        REQUIRE(v <= 1.0001);
        const double d = std::fabs(v - prev);
        worst_jump = std::max(worst_jump, d);
        (u <= tf ? pull_travel : reset_travel) += d;
        prev = v;
    }
    REQUIRE(sim::repair_wrench_stroke(0.0, tf) == Catch::Approx(1.0));
    REQUIRE(sim::repair_wrench_stroke(tf, tf) == Catch::Approx(-1.0));
    REQUIRE(pull_travel == Catch::Approx(2.0).margin(0.01));
    REQUIRE(reset_travel == Catch::Approx(2.0).margin(0.01));
    REQUIRE(worst_jump < 0.02);  // continuous: no teleport at either reversal
    // ... and it repeats, so a phase that has wrapped means the same thing.
    REQUIRE(sim::repair_wrench_stroke(0.3, tf) ==
            Catch::Approx(sim::repair_wrench_stroke(2.3, tf)));
}

TEST_CASE("g2j_the_blow_raises_then_strikes_then_recovers", "[g2j][gait]") {
    constexpr double total = 0.55;
    REQUIRE(sim::repair_blow_swing(0.0, total) == 0.0);    // not striking
    REQUIRE(sim::repair_blow_swing(total, total) == 0.0);  // at rest at t=0
    double peak_up = -2.0, peak_down = 2.0;
    double t_peak_up = 0.0, t_peak_down = 0.0;
    for (int i = 0; i <= 1000; ++i) {
        const double u = static_cast<double>(i) / 1000.0;  // 0 -> 1 forwards
        const double v = sim::repair_blow_swing(total * (1.0 - u), total);
        REQUIRE(v >= -1.0001);
        REQUIRE(v <= 1.0001);
        if (v > peak_up) {
            peak_up = v;
            t_peak_up = u;
        }
        if (v < peak_down) {
            peak_down = v;
            t_peak_down = u;
        }
    }
    REQUIRE(peak_up == Catch::Approx(1.0).margin(0.01));  // the fist comes up
    REQUIRE(peak_down == Catch::Approx(-1.0).margin(0.01));  // and comes DOWN
    REQUIRE(t_peak_up < t_peak_down);  // in that order, never the reverse
    REQUIRE(std::fabs(sim::repair_blow_swing(1e-6, total)) < 0.01);  // settles
}

TEST_CASE("g2j_the_work_runs_while_repairing_and_the_blow_outlives_it",
          "[g2j][gait]") {
    const sim::RepairWorkParams p;
    const double dt = 1.0 / 120.0;
    sim::RepairWork w;
    REQUIRE(!w.on);
    // Not working: the phase does not advance (nothing is being turned).
    for (int i = 0; i < 60; ++i)
        w = sim::step_repair_work(w, false, false, dt, p);
    REQUIRE(w.phase == 0.0);
    // Working: the phase advances at the ruled rate, wrapping in [0,1).
    for (int i = 0; i < 120; ++i)
        w = sim::step_repair_work(w, true, false, dt, p);
    REQUIRE(w.on);
    REQUIRE(w.phase >= 0.0);
    REQUIRE(w.phase < 1.0);
    REQUIRE(w.phase == Catch::Approx(std::fmod(p.cycle_hz, 1.0)).margin(0.02));
    // ★★★ THE ONE THAT MATTERS: `FinishRepair` is the transition OUT of
    // Repairing, so the blow fires on the SAME step `working` goes false and
    // must still swing for its whole duration afterwards. A blow gated on
    // `on` would be a hammer nobody ever sees.
    w = sim::step_repair_work(w, false, true, dt, p);
    REQUIRE(!w.on);
    REQUIRE(w.blow_s > 0.0);
    int swinging = 0;
    for (int i = 0; i < 400; ++i) {
        w = sim::step_repair_work(w, false, false, dt, p);
        if (w.blow_s > 0.0) ++swinging;
    }
    REQUIRE(static_cast<double>(swinging) * dt ==
            Catch::Approx(p.blow_s - dt).margin(0.03));
    REQUIRE(w.blow_s == 0.0);  // and it ends, once, cleanly
}
