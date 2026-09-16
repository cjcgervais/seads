// ★★★ R4c §7.3 STAGES 4-7 — THE MAN'S OWN GATE.
//
// He lives in `sim/walker.{h,cpp}` and not in `render/sled_model.cpp` for one
// reason: CMakeLists compiles that TU only into the `seads` executable, so a
// launch rule or a gait law living there could not be executed by any test.
// This ladder has lost two rungs to exactly that, and the record is in
// render/body_drive.h's banner. These legs execute the policy.
//
// ★ AND THE DEPARTURE'S LEGS CAME WITH HIM. Stage 2 built the free body in
// `render/rider_flight.*`; R4c moved it into the kernel, because a man who
// takes input and samples the snowpack is a body and not an animation. The
// legs below are the same legs, re-aimed at the same law in its new home --
// which is the whole point of a MOVE rather than a rewrite.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>

#include "sim/walker.h"
#include "world/heightfield.h"
#include "world/snowpack.h"

namespace {

// A featureless 15 km ball with a chosen, uniform snow depth -- so a leg that
// is about DEPTH varies exactly one thing and the ground under him is the same
// everywhere he walks.
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
    return p;  // the shipped defaults, never a set invented for the assert
}

// Seed him where a man on a machine is when the grip actually breaks: IN THE
// AIR. The load is hardness x extension and the extension is what a buck gives
// him, so a release happens at a landing, not while parked.
//
// ⚠ AND THE FIRST DRAFT OF THIS HELPER PUT HIM AT 0.564 m -- the machine's own
// CG height -- WHICH IS EXACTLY WHERE HE COMES TO REST (`lie_clearance_m`
// defaults to the same number). He therefore landed on the first step and
// never fell at all, and a leg asserting "he is falling" failed for a reason
// that had nothing to do with the law. That is a real property worth naming
// rather than hiding: a man released at rest height IS already down.
glm::dvec3 seat_pos(const world::SnowpackField& f, double air_m = 2.0) {
    const glm::dvec3 up(0.0, 1.0, 0.0);
    return up * (f.drive_radius_at(up) + 0.564 + air_m);
}

// Run him at the sim's own tick, never a frame time.
sim::WalkerState run(sim::WalkerState s, const sim::WalkerParams& p,
                     const world::SnowpackField& f, double secs,
                     const sim::WalkerInputs& in = sim::WalkerInputs{}) {
    const double dt = 1.0 / 120.0;
    for (int i = 0; i < static_cast<int>(secs / dt); ++i)
        s = sim::step_walker(s, in, p, f, dt);
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// THE DEPARTURE (stage 2's legs, in their new home)

TEST_CASE("walker_he_leaves_with_the_machines_velocity_and_nothing_added",
          "[r4c][walker]") {
    // ★★★ THE ONE THAT WOULD CATCH A FORWARD GAIN BEING SNEAKED IN. The
    // measured 8-of-10 forward departure is EMERGENT -- he keeps his velocity
    // and the machine sheds it into the snow -- so the throw must add nothing
    // along the direction of travel. At zero lateral lean his departure
    // velocity is the machine's, exactly, to the last bit.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    sim::WalkerState s;
    const sim::WalkerParams p = dials();
    const glm::dvec3 v(0.0, 0.0, -18.0);
    sim::walker_throw(s, p, seat_pos(f), v, glm::dvec3(1.0, 0.0, 0.0), 0.0);
    REQUIRE(s.mode == sim::WalkerMode::Falling);
    REQUIRE(s.vel.x == v.x);
    REQUIRE(s.vel.y == v.y);
    REQUIRE(s.vel.z == v.z);
}

TEST_CASE("walker_the_side_is_the_side_he_was_already_leaning",
          "[r4c][walker]") {
    // CHAD'S Q1, 2026-08-30: "whichever way he's already leaning out." The sign
    // of the departure follows the sign of the lean and nothing else -- it does
    // not read the machine's roll and it does not invent a side.
    //
    // ⚠ BOTH SIGNS ARE DRIVEN. A one-sided leg here would pass just as happily
    // on a law that always threw him right.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    const sim::WalkerParams p = dials();
    const glm::dvec3 right(1.0, 0.0, 0.0);
    const glm::dvec3 v(0.0, 0.0, -18.0);

    sim::WalkerState out_right, out_left;
    sim::walker_throw(out_right, p, seat_pos(f), v, right, +0.20);
    sim::walker_throw(out_left, p, seat_pos(f), v, right, -0.20);

    REQUIRE(out_right.vel.x > 0.0);
    REQUIRE(out_left.vel.x < 0.0);
    // Equal and opposite, and proportional to the lean: the gain is a scale on
    // what he already had, not a constant shove.
    REQUIRE(std::abs(out_right.vel.x + out_left.vel.x) < 1.0e-12);
    REQUIRE(std::abs(out_right.vel.x - p.lat_gain_per_s * 0.20) < 1.0e-12);
    // ...and the forward component is untouched by any of it.
    REQUIRE(out_right.vel.z == v.z);
}

TEST_CASE("walker_zero_gain_is_structurally_off_not_merely_small",
          "[r4c][walker]") {
    // The kill switch for the only dial in the departure (SEADS_GRIP_LATGAIN=0):
    // no lateral term at all, for any lean, not a small one.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    sim::WalkerParams p = dials();
    p.lat_gain_per_s = 0.0;
    const glm::dvec3 v(0.0, 0.0, -18.0);
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), v, glm::dvec3(1.0, 0.0, 0.0), 0.85);
    REQUIRE(s.vel.x == v.x);
}

TEST_CASE("walker_the_throw_is_one_way", "[r4c][walker]") {
    // §7.3 stage 4 is the one irreversible transition in the chain, and the
    // kernel's grip latch is one-way for the same reason. A second throw on a
    // man already off the machine would teleport him back onto it.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    const sim::WalkerParams p = dials();
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    s = run(s, p, f, 0.2);
    const glm::dvec3 moved = s.pos;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    REQUIRE(s.pos.x == moved.x);
    REQUIRE(s.pos.y == moved.y);
    REQUIRE(s.pos.z == moved.z);
}

TEST_CASE("walker_a_state_still_riding_ignores_every_step", "[r4c][walker]") {
    // Nothing moves before the grip breaks. The whole rung is off-by-absence in
    // the strongest sense: a man still on the machine is inert against any
    // input, including a walk command.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    sim::WalkerInputs walk;
    walk.forward = 1.0f;
    const sim::WalkerState s = run(sim::WalkerState{}, dials(), f, 5.0, walk);
    REQUIRE(s.mode == sim::WalkerMode::Riding);
    REQUIRE(s.pos == glm::dvec3(0.0));
    REQUIRE(s.vel == glm::dvec3(0.0));
}

TEST_CASE("walker_no_ticks_is_no_motion", "[r4c][walker]") {
    // A frame that consumed no sim ticks advances nothing -- the same
    // discipline the scarf and the body chain already run on.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    const sim::WalkerParams p = dials();
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    const glm::dvec3 p0 = s.pos, v0 = s.vel;
    s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 0.0);
    REQUIRE(s.pos == p0);
    REQUIRE(s.vel == v0);
}

TEST_CASE("walker_he_falls_lands_and_the_snow_answers", "[r4c][walker]") {
    // The landing, and §7.5's poof asked for on exactly the step he hits.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    const sim::WalkerParams p = dials();
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    const double r0 = glm::length(s.pos);

    s = run(s, p, f, 0.15);
    REQUIRE(glm::length(s.pos) < r0);  // he is falling
    REQUIRE(s.mode == sim::WalkerMode::Falling);

    // Step until he hits, and catch the edge on the step it happens.
    bool saw_poof = false, saw_landed = false;
    for (int i = 0; i < 2000 && s.mode == sim::WalkerMode::Falling; ++i) {
        s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
        saw_poof = saw_poof || s.poof;
        saw_landed = saw_landed || s.landed;
    }
    REQUIRE(saw_poof);
    REQUIRE(saw_landed);
    REQUIRE(s.mode != sim::WalkerMode::Falling);
    // He is ON the snow, not floating over it and not through it.
    REQUIRE(std::abs(glm::length(s.pos) -
                     (f.drive_radius_at(glm::normalize(s.pos)) +
                      p.lie_clearance_m)) < 1.0e-6);
    // ★ AND THE EDGES ARE ONE-SHOT. A consumer that fires the spray twice, or
    // dents the helmet twice, is the defect this line refuses.
    const sim::WalkerState next =
        sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
    REQUIRE_FALSE(next.landed);
}

// ---------------------------------------------------------------------------
// ★★★ THE DEPTH LADDER — CHAD'S RULING, 2026-08-31

TEST_CASE("walker_depth_sets_the_pace_across_all_three_of_his_anchors",
          "[r4c][walker]") {
    // "quick gait on hardpack ... the intermediate midly handicapped by about a
    // foot or two, but slowed ... large stepping ... in deep snow."
    //
    // ★ THE MIDDLE ANCHOR IS THE WHOLE POINT and it is why this is a ROW and
    // not a lerp: a straight line from hardpack to deep would make him fast in
    // a foot of snow, which is exactly what "midly handicapped ... but slowed"
    // refuses. So the leg checks the ORDER, and then checks that the middle is
    // genuinely below the straight line between the ends.
    const sim::WalkerParams p = dials();
    const double hard = sim::walker_speed_cap(p, 0.0);
    const double mid = sim::walker_speed_cap(p, p.depth_mid_m);
    const double deep = sim::walker_speed_cap(p, p.depth_deep_m);
    REQUIRE(hard > mid);
    REQUIRE(mid > deep);
    const double straight =
        hard + (deep - hard) * (p.depth_mid_m / p.depth_deep_m);
    REQUIRE(mid < straight);

    // Monotone all the way down, with no step anywhere: a class switch would
    // put one in, and a binary read of a continuous quantity is this ladder's
    // own recorded disease.
    double prev = 1.0e30;
    for (int i = 0; i <= 100; ++i) {
        const double d = p.depth_deep_m * i / 100.0;
        const double v = sim::walker_speed_cap(p, d);
        REQUIRE(v <= prev + 1.0e-12);
        prev = v;
    }
    // Deeper than the signed law does not keep slowing him without bound --
    // the law is what "deep" MEANS here.
    REQUIRE(sim::walker_speed_cap(p, 3.0) == deep);
}

TEST_CASE("walker_deep_snow_is_large_stepping_and_hardpack_is_a_quick_gait",
          "[r4c][walker]") {
    // ★★★ HIS TWO PHRASES ARE TWO MOTIONS, NOT ONE MOTION AT TWO RATES, and
    // this is the leg that makes that true rather than said: the STRIDE grows
    // with the snow while the SPEED falls, so the cadence -- which is authored
    // nowhere and is only ever speed/stride -- collapses. A man heaving one leg
    // at a time out of deep snow, and a man trotting on a packed trail.
    // ★★★ "LARGE STEPPING" IS RELATIVE TO HIS SPEED, NOT AN ABSOLUTE METRE,
    // and the first version of this leg had it wrong. A jog covers more ground
    // per stride than a trudge does -- of course it does. What deep snow
    // actually does is buy a LONGER step at a DISPROPORTIONATELY lower cadence,
    // because every step costs a hole. That is what the measured walk ratio
    // stretches, and it is what his phrase describes.
    const sim::WalkerParams p = dials();
    const double v_deep = sim::walker_speed_cap(p, p.depth_deep_m);
    const double stride_deep = sim::walker_stride(p, p.depth_deep_m);
    // A man walking that slowly on a PACKED trail, from the same law:
    sim::WalkerParams flat = p;
    flat.walk_ratio_deep_gain = 0.0;
    flat.speed_hardpack_mps = v_deep;  // same speed, no snow
    const double stride_same_speed = sim::walker_stride(flat, 0.0);
    REQUIRE(stride_deep > 1.2 * stride_same_speed);  // LARGER steps for the pace

    // And the cadences: a heave versus a trot. Cadence is authored nowhere --
    // it is only ever speed/stride -- so this is a consequence, not a setting.
    const double stride_hard = sim::walker_stride(p, 0.0);
    const double cad_hard = sim::walker_speed_cap(p, 0.0) / stride_hard;
    const double cad_deep = v_deep / stride_deep;
    REQUIRE(cad_hard > 3.0 * cad_deep);
    // ★ AND IT IS A HUMAN CADENCE NOW. One phase cycle is TWO steps (the legs
    // are half a cycle apart), and the first version of this law forgot that --
    // it ran at 468 steps/min against a human sprint maximum near 260. This is
    // the leg that refuses that.
    REQUIRE(cad_hard * 2.0 * 60.0 < 260.0);
}

TEST_CASE("walker_the_gait_advances_on_distance_not_on_time",
          "[r4c][walker]") {
    // ★★★ THE ANTI-FOOTSKATE LAW, EXECUTED -- AND STATED SHARPLY ENOUGH TO
    // FAIL. The first draft compared the same walk stepped at two dt values and
    // asked for the phases to agree; they did agree, to 0.06 of a cycle out of
    // 155 accumulated cycles, and the bound I had written was simply tighter
    // than the integrator. A leg whose threshold is an accident of how long you
    // ran it is not measuring the law.
    //
    // The law itself is exact: the phase IS distance / stride. So measure that,
    // and measure it ACROSS THE ACCELERATION RAMP, where a time-driven phase
    // and a distance-driven one visibly part company -- a man starting from a
    // standstill covers `v*tau` less ground than a clock thinks he does, which
    // here is 1.2 m, or a third of a cycle. Nothing about this depends on dt.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerInputs walk;
    walk.forward = 1.0f;

    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0), glm::dvec3(1, 0, 0),
                      0.0);
    s = run(s, p, f, 6.0);  // land, get up -- standing still at the end of it
    REQUIRE(s.mode == sim::WalkerMode::Afoot);
    REQUIRE(glm::length(s.vel) == 0.0);

    // Walk from a dead stop, measuring the ground he actually covers.
    const double dt = 1.0 / 120.0;
    const double phase0 = s.gait_phase;
    double dist = 0.0;
    double t = 0.0;
    for (int i = 0; i < 240; ++i) {
        const glm::dvec3 was = s.pos;
        s = sim::step_walker(s, walk, p, f, dt);
        dist += glm::length(s.pos - was);
        t += dt;
    }
    // The phase advanced by exactly the strides he took.
    const double turns = dist / s.stride_m;
    double got = s.gait_phase - phase0;
    if (got < 0.0) got += 1.0;
    double want = turns - std::floor(turns);
    REQUIRE(std::abs(got - want) < 1.0e-6);

    // ...and a CLOCK would have been a third of a cycle ahead of him, which is
    // the margin that makes the line above a measurement and not a tautology.
    const double clock_turns = t * p.speed_hardpack_mps / s.stride_m;
    REQUIRE(clock_turns - turns > 0.25);
}

TEST_CASE("walker_he_actually_walks_and_the_snow_holds_him_back",
          "[r4c][walker]") {
    // The felt claim, measured: the same man, the same command, the same
    // seconds -- and the deep-snow one is still near where he started.
    const world::HeightField hf = flat_hf();
    const sim::WalkerParams p = dials();
    sim::WalkerInputs walk;
    walk.forward = 1.0f;

    const world::SnowpackField packed = snow_of_depth(hf, 0.0);
    const world::SnowpackField deep = snow_of_depth(hf, 0.77);

    auto travelled = [&](const world::SnowpackField& f) {
        sim::WalkerState s;
        sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0),
                          glm::dvec3(1, 0, 0), 0.0);
        // Let him land and get up, THEN measure only the walking.
        s = run(s, p, f, 40.0, walk);
        REQUIRE(s.mode == sim::WalkerMode::Afoot);
        const glm::dvec3 start = s.pos;
        s = run(s, p, f, 10.0, walk);
        return glm::length(s.pos - start);
    };
    const double d_packed = travelled(packed);
    const double d_deep = travelled(deep);
    REQUIRE(d_packed > 25.0);           // a jog, ten seconds of it
    REQUIRE(d_deep < 0.5 * d_packed);   // and a slog
    REQUIRE(d_deep > 1.0);              // but he is NOT stuck
}

// ---------------------------------------------------------------------------
// ★★★ GETTING UP — §7.5, AND HIS "DISSAPEAR ... IN A POOF"

TEST_CASE("walker_getting_up_costs_what_the_snow_says_it_costs",
          "[r4c][walker]") {
    // §7.5, in his own arithmetic: "the same crash costs you fifteen seconds in
    // the bush and two on the trail, and the player learns that from the snow,
    // not from a UI."
    const sim::WalkerParams p = dials();
    REQUIRE(sim::walker_rise_s(p, 0.0) == p.rise_s_hardpack);
    REQUIRE(sim::walker_rise_s(p, p.depth_deep_m) == p.rise_s_deep);
    REQUIRE(sim::walker_rise_s(p, p.depth_mid_m) > p.rise_s_hardpack);
    REQUIRE(sim::walker_rise_s(p, p.depth_mid_m) < p.rise_s_deep);

    // And it is the CLOCK he actually runs on, not a number in a table: the
    // same crash on packed ground puts him on his feet while the deep-snow one
    // is still climbing out.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField packed = snow_of_depth(hf, 0.0);
    const world::SnowpackField deep = snow_of_depth(hf, 0.77);
    auto after = [&](const world::SnowpackField& f, double secs) {
        sim::WalkerState s;
        sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0),
                          glm::dvec3(1, 0, 0), 0.0);
        return run(s, p, f, secs).mode;
    };
    REQUIRE(after(packed, 4.0) == sim::WalkerMode::Afoot);
    REQUIRE(after(deep, 4.0) != sim::WalkerMode::Afoot);
    REQUIRE(after(deep, 25.0) == sim::WalkerMode::Afoot);
}

TEST_CASE("walker_deep_snow_swallows_him_and_hardpack_never_does",
          "[r4c][walker]") {
    // ★★★ HIS RULING: "he should dissapear and / reappear in deepest snow in a
    // poof". The burial is a real stage in deep snow -- and in hardpack it is
    // STRUCTURALLY absent, zero seconds long, not merely short: `bury_frac` is
    // scaled by the same continuous depth fraction everything else reads.
    const world::HeightField hf = flat_hf();
    const sim::WalkerParams p = dials();

    auto modes_seen = [&](double depth) {
        const world::SnowpackField f = snow_of_depth(hf, depth);
        sim::WalkerState s;
        sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0),
                          glm::dvec3(1, 0, 0), 0.0);
        bool buried = false, crawled = false;
        int poofs = 0;
        for (int i = 0; i < 120 * 40; ++i) {
            s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
            buried = buried || s.mode == sim::WalkerMode::Buried;
            crawled = crawled || s.mode == sim::WalkerMode::CrawlProne ||
                                 s.mode == sim::WalkerMode::CrawlKnees;
            if (s.poof) ++poofs;
        }
        return std::make_tuple(buried, crawled, poofs);
    };

    const auto deep = modes_seen(0.77);
    REQUIRE(std::get<0>(deep));      // he went under
    REQUIRE(std::get<1>(deep));      // ...and crawled out
    REQUIRE(std::get<2>(deep) == 2); // a poof going IN and one coming OUT

    const auto hard = modes_seen(0.0);
    REQUIRE_FALSE(std::get<0>(hard));  // hardpack never swallows him
    REQUIRE(std::get<2>(hard) == 1);   // just the landing burst
}

TEST_CASE("walker_the_planted_foot_does_not_skate", "[r4c][walker]") {
    // ★★★ THE CLAIM THE WHOLE GAIT RESTS ON. Through STANCE the foot is on the
    // snow, so in the man's own frame it must travel backwards at exactly the
    // rate he travels forwards -- one stride back over half a cycle. Any easing
    // in there is a boot sliding on the ground, which is the single most
    // visible tell in a procedural walk.
    // ★★★ THE INVARIANT, AND THE FIRST VERSION OF THIS LEG ENCODED THE BUG.
    // Phase advances by distance/stride, so a foot planted for `ds` of a cycle
    // is planted while the body covers `ds * stride` -- the excursion is
    // `ds * stride`, NOT a whole stride. The old law swept a whole stride over
    // half a cycle, i.e. the planted foot slid backwards at 1x body speed, and
    // this leg asserted exactly that and passed.
    const double stride = 0.9, lift = 0.2, ds = 0.6;
    const double excur = ds * stride;
    double f0 = 0.0, u0 = 0.0, f1 = 0.0, u1 = 0.0;
    sim::walker_foot_offset(0.0, stride, lift, ds, &f0, &u0);
    sim::walker_foot_offset(ds - 1.0e-9, stride, lift, ds, &f1, &u1);
    REQUIRE(std::abs(f0 - 0.5 * excur) < 1.0e-9);
    REQUIRE(std::abs(f1 + 0.5 * excur) < 1.0e-6);
    // ★ AND THE SLOPE IS THE WHOLE CLAIM: d(fwd)/d(phase) == -stride exactly,
    // everywhere in stance. That IS "the foot does not slide", stated so it can
    // fail -- at any other slope the boot skates through the snow.
    for (int i = 0; i <= 20; ++i) {
        const double ph = ds * i / 20.0;
        double f = 0.0, u = 0.0;
        sim::walker_foot_offset(ph, stride, lift, ds, &f, &u);
        REQUIRE(std::abs(f - (0.5 * excur - stride * ph)) < 1.0e-9);
        // ★ AND IT IS ON THE GROUND THE WHOLE TIME -- exactly 0, not nearly.
        // A foot that lifts while it is bearing weight is the floaty motion
        // §0.4 exists to prevent.
        REQUIRE(u == 0.0);
    }
}

TEST_CASE("walker_the_swing_clears_the_snow_and_sets_down_softly",
          "[r4c][walker]") {
    // The other half. It comes back forward, it gets clear of what he is
    // standing in, and it arrives with zero vertical rate -- a step, not a
    // stamp.
    const double stride = 1.1, lift = 0.45, ds = 0.6;
    double peak = 0.0;
    for (int i = 0; i <= 100; ++i) {
        const double ph = 0.5 + 0.5 * i / 100.0;
        double f = 0.0, u = 0.0;
        sim::walker_foot_offset(ph, stride, lift, ds, &f, &u);
        peak = std::max(peak, u);
        REQUIRE(u >= 0.0);  // he never puts a foot THROUGH the ground
    }
    REQUIRE(std::abs(peak - lift) < 1.0e-6);  // it clears exactly what it was told

    // Continuous across BOTH seams, which is what stops the foot teleporting
    // once per stride -- the defect a cycle written as two halves invites.
    double fa = 0.0, ua = 0.0, fb = 0.0, ub = 0.0;
    sim::walker_foot_offset(ds - 1.0e-7, stride, lift, ds, &fa, &ua);
    sim::walker_foot_offset(ds + 1.0e-7, stride, lift, ds, &fb, &ub);
    REQUIRE(std::abs(fa - fb) < 1.0e-5);
    REQUIRE(std::abs(ua - ub) < 1.0e-5);
    sim::walker_foot_offset(1.0 - 1.0e-7, stride, lift, ds, &fa, &ua);
    sim::walker_foot_offset(0.0, stride, lift, ds, &fb, &ub);
    REQUIRE(std::abs(fa - fb) < 1.0e-5);
    REQUIRE(std::abs(ua - ub) < 1.0e-5);
}

TEST_CASE("walker_the_two_legs_are_half_a_cycle_apart", "[r4c][walker]") {
    // One leg is planted while the other swings, always -- a man with both feet
    // in the air is jumping and a man with both planted is not walking.
    const double stride = 0.9, lift = 0.2, ds = 0.6;
    for (int i = 0; i < 40; ++i) {
        const double ph = i / 40.0;
        double ul = 0.0, ur = 0.0;
        sim::walker_foot_offset(ph, stride, lift, ds, nullptr, &ul);
        sim::walker_foot_offset(ph + 0.5, stride, lift, ds, nullptr, &ur);
        REQUIRE((ul == 0.0 || ur == 0.0));
        REQUIRE_FALSE((ul > 0.0 && ur > 0.0));
    }
}

TEST_CASE("walker_the_ground_stops_him_sinking_not_sliding", "[r4c][walker]") {
    // ★★★ THE LEG THAT DID NOT SURVIVE THE MOVE, AND THE REGRESSION IT WOULD
    // HAVE CAUGHT. `render/rider_flight.cpp` bled his tangential velocity off
    // over a distance and had a leg pinning exactly this; when the body moved
    // into `sim/walker.cpp` the landing was written to ZERO the velocity and
    // neither the behaviour nor the leg came with it. The commit message for
    // that move claimed "the legs came with him"; for this one it was false.
    // Chad drove it: "he needs to maintain some forward velocity / skid upon
    // falling off."
    //
    // The contact kills the INWARD component ONLY. He stops sinking; he does
    // not stop sliding.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    sim::WalkerParams p = dials();
    p.mu_bush = 0.0;   // nothing to eat the tangential part
    p.plow_rho = 0.0;  // ...and nothing to plough
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    s = run(s, p, f, 2.0);
    REQUIRE(s.mode != sim::WalkerMode::Falling);
    REQUIRE(std::abs(s.vel.z) > 10.0);  // still sliding, hard
    // ...and not sinking: no inward radial velocity survives the contact.
    REQUIRE(glm::dot(s.vel, glm::normalize(s.pos)) > -1.0e-9);
}

TEST_CASE("walker_he_travels_before_he_settles", "[r4c][walker]") {
    // The felt version of the same claim, at the SHIPPED dials -- because a leg
    // that only holds when the drag is switched off proves nothing about what
    // he actually does. A man arriving at 18 m/s covers real ground before the
    // snow has taken it all.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);
    const sim::WalkerParams p = dials();
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    // Where he first touches down...
    while (s.mode == sim::WalkerMode::Falling)
        s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
    const glm::dvec3 touchdown = s.pos;
    // ...versus where the snow finally has him.
    s = run(s, p, f, 6.0);
    // ⚠ NOT `== 0.0`, AND THE REASON IS NOT SLOP. The Coulomb skid DOES land
    // on exactly zero (the Karnopp clamp), but by 6 s on hardpack he is back on
    // his feet, and a standing man's speed is his own first-order slew toward
    // the zero command -- which is asymptotic. The bound is on HIS residual,
    // not on the snow's.
    REQUIRE(glm::length(s.vel) < 0.01);
    const double skid = glm::length(s.pos - touchdown);
    // ★★★ AND THE NUMBER IS NOW MEASURED, SO THE BOUND IS TIGHT. Coulomb on
    // hardpack: d = v^2 / (2 mu g). At the arrival speed and the shipped
    // coefficient that is a specific distance, and a law of the wrong SHAPE
    // cannot hit it -- the exponential this replaced stopped him in 2.5 m and
    // would fail here by a factor of eight.
    const double v_touch = 18.0;  // he departs at 18 and the fall is short
    const double want = v_touch * v_touch / (2.0 * p.mu_bush * p.gravity_mps2);
    REQUIRE(skid > 0.55 * want);
    REQUIRE(skid < 1.30 * want);
    // ★ AND IT IS QUADRATIC IN THE ARRIVAL SPEED, WHICH IS THE HALF THAT
    // MAKES IT READ. An exponential's distance is LINEAR in v -- so under it a
    // crash at double the speed skidded double, when it should skid nearly
    // four times as far. This is the leg that refuses the wrong shape.
    sim::WalkerState fast;
    sim::walker_throw(fast, p, seat_pos(f), glm::dvec3(0.0, 0.0, -36.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);
    while (fast.mode == sim::WalkerMode::Falling)
        fast = sim::step_walker(fast, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
    const glm::dvec3 td2 = fast.pos;
    fast = run(fast, p, f, 12.0);
    const double skid2 = glm::length(fast.pos - td2);
    REQUIRE(skid2 > 3.0 * skid);  // ~4x, and never the 2x a decay would give
}

TEST_CASE("walker_deep_snow_ploughs_him_to_a_stop_in_a_few_metres",
          "[r4c][walker]") {
    // ★★★ THE CONTRAST THAT IS THE WHOLE RUNG. Same man, same arrival speed,
    // same surface class -- only the DEPTH differs. On hardpack he slides a
    // long way on dry friction; in deep snow the plough term (quadratic, gated
    // on immersion) stops him in a few metres and buries him. A single law
    // with one set of coefficients has to produce both, or the "poof, he's
    // gone" beat and the long skid cannot coexist.
    const world::HeightField hf = flat_hf();
    const sim::WalkerParams p = dials();
    auto skid_in = [&](double depth) {
        const world::SnowpackField f = snow_of_depth(hf, depth);
        sim::WalkerState s;
        sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                          glm::dvec3(1.0, 0.0, 0.0), 0.0);
        while (s.mode == sim::WalkerMode::Falling)
            s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
        const glm::dvec3 td = s.pos;
        // Only while the snow owns him -- once he crawls, the distance is his.
        while (s.mode == sim::WalkerMode::Buried || s.mode == sim::WalkerMode::Down)
            s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
        return glm::length(s.pos - td);
    };
    const double hard = skid_in(0.0);
    const double deep = skid_in(0.77);
    REQUIRE(deep < 6.0);          // he does not slide, he ploughs
    REQUIRE(hard > 4.0 * deep);   // and the contrast is the beat
}

TEST_CASE("walker_he_comes_out_belly_then_knees_then_feet", "[r4c][walker]") {
    // ★★★ CHAD'S ORDER, 2026-09-01, VERBATIM: "they should start coming out by
    // crawling prone, crawling on knees then walking through deep snow."
    //
    // A single crawl stage ran the WALK cycle on a man pitched face-down, which
    // is the "legs animated like a wheel" he saw. Getting out of a hole is
    // three motions and this leg pins that they happen, IN THAT ORDER, and that
    // none is skipped.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    const sim::WalkerParams p = dials();
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -8.0),
                      glm::dvec3(1.0, 0.0, 0.0), 0.0);

    std::vector<sim::WalkerMode> seen;
    for (int i = 0; i < 120 * 60; ++i) {
        const sim::WalkerMode was = s.mode;
        s = sim::step_walker(s, sim::WalkerInputs{}, p, f, 1.0 / 120.0);
        if (s.mode != was) seen.push_back(s.mode);
        if (s.mode == sim::WalkerMode::Afoot) break;
    }
    REQUIRE(s.mode == sim::WalkerMode::Afoot);
    // Every stage, once, in his order.
    REQUIRE(seen.size() == 5);
    REQUIRE(seen[0] == sim::WalkerMode::Buried);
    REQUIRE(seen[1] == sim::WalkerMode::Down);
    REQUIRE(seen[2] == sim::WalkerMode::CrawlProne);
    REQUIRE(seen[3] == sim::WalkerMode::CrawlKnees);
    REQUIRE(seen[4] == sim::WalkerMode::Afoot);

    // ★ AND THE BELLY CRAWL IS THE SLOWER ONE. He drags himself clear before
    // he can push up onto his hands -- if these were equal, the sequence would
    // be a rename rather than a motion.
    sim::WalkerParams q = p;
    REQUIRE(q.crawl_prone_share > 0.5);
}

TEST_CASE("walker_a_crawl_is_slower_than_a_walk", "[r4c][walker]") {
    // The stages have to DIFFER, not just be named differently. A belly crawl
    // is half the pace of hands-and-knees, and both are far short of walking.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.0);  // hardpack: fast walk
    const sim::WalkerParams p = dials();
    sim::WalkerInputs go;
    go.forward = 1.0f;
    auto cap_in_mode = [&](sim::WalkerMode want) {
        sim::WalkerState s;
        sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0), glm::dvec3(1, 0, 0),
                          0.0);
        for (int i = 0; i < 120 * 60; ++i) {
            s = sim::step_walker(s, go, p, f, 1.0 / 120.0);
            if (s.mode == want) {
                // ⚠ ONE MORE STEP BEFORE READING. The tick that ENTERS a mode
                // returns out of the previous branch, so the cap it carries is
                // still the depth row's -- the new mode's own override runs on
                // the tick after. Reading on the transition tick measured the
                // stage he had just left, which is a leg that looks like it
                // works and reports the wrong stage.
                s = sim::step_walker(s, go, p, f, 1.0 / 120.0);
                return s.speed_cap_mps;
            }
        }
        return -1.0;
    };
    const double prone = cap_in_mode(sim::WalkerMode::CrawlProne);
    const double knees = cap_in_mode(sim::WalkerMode::CrawlKnees);
    const double afoot = cap_in_mode(sim::WalkerMode::Afoot);
    REQUIRE(prone > 0.0);
    REQUIRE(knees > prone);
    REQUIRE(afoot > 3.0 * knees);
}

TEST_CASE("walker_the_remount_ends_him", "[r4c][walker]") {
    // KEY_R is the interim remount until R4e (§7.9). It is the one transition
    // that goes back, and it must leave nothing behind -- a stale position or a
    // half-finished get-up would come back to life on the next fall.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = snow_of_depth(hf, 0.77);
    const sim::WalkerParams p = dials();
    sim::WalkerState s;
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1, 0, 0), 0.0);
    s = run(s, p, f, 3.0);
    REQUIRE(s.mode != sim::WalkerMode::Riding);
    sim::walker_remount(s);
    REQUIRE(s.mode == sim::WalkerMode::Riding);
    REQUIRE(s.pos == glm::dvec3(0.0));
    REQUIRE(s.vel == glm::dvec3(0.0));
    // ...and he can be thrown again, which is what makes the loop a loop.
    sim::walker_throw(s, p, seat_pos(f), glm::dvec3(0.0, 0.0, -18.0),
                      glm::dvec3(1, 0, 0), 0.0);
    REQUIRE(s.mode == sim::WalkerMode::Falling);
}
