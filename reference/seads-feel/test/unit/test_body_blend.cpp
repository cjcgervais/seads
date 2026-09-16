// ★★★ R4a RUNG 2 -- THE DRAWN LEGS BLENDED ONTO THE CHAIN.
//
// Every leg here grades render/body_blend.*, which is PURE and in
// seads_render_core. That is the point of the extraction: the first draft of
// this rung put its gate against render/sled_model.cpp, which is compiled only
// into the `seads` executable, so not one of those legs could have run.
//
// The discipline each leg is written to: it must be able to FAIL. This program
// has paid six times for a test whose fixture makes the mechanism a no-op, or
// whose metric cannot go the wrong way.

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "render/body_blend.h"
#include "render/body_chain.h"
#include "render/trail_chain.h"

using namespace render;

namespace {

// A synthetic chain: the measured bind stations, straight out of the table, so
// the fixture is the shipped body and not an invented one.
struct Fixture {
    glm::vec3 p[kBodyChainStations];
    glm::mat4 f[kBodyChainStations];
    TrailChainParams pr;

    Fixture() {
        const std::array<BodyChainStation, kBodyChainStations>& t =
            body_chain_stations();
        for (int i = 0; i < kBodyChainStations; ++i) {
            p[i] = t[static_cast<std::size_t>(i)].rest_model;
            f[i] = glm::mat4(1.0f);
        }
        body_chain_fill(pr);
    }
};

// The R3 side, built so the legs START legal: hip at the thigh station, knee
// and ankle placed at their own measured joint offsets, which is exactly where
// the drawn man is at bind.
struct Legs {
    glm::vec3 hip[2], knee[2], ankle[2];
    float lt[2], lc[2];
};

const BodyChainLegJoint& row(int station) {
    for (const BodyChainLegJoint& r : body_chain_leg_joints())
        if (r.station == station) return r;
    throw std::runtime_error("no such leg station");
}

Legs bind_legs(const Fixture& fx) {
    const int st_thigh = body_chain_station_of("thigh");
    const int st_knee = body_chain_station_of("knee");
    const int st_ankle = body_chain_station_of("ankle");
    Legs L;
    for (int s = 0; s < 2; ++s) {
        const std::size_t si = static_cast<std::size_t>(s);
        L.hip[s] = fx.p[st_thigh] + row(st_thigh).off_m[si];
        L.knee[s] = fx.p[st_knee] + row(st_knee).off_m[si];
        L.ankle[s] = fx.p[st_ankle] + row(st_ankle).off_m[si];
        L.lt[s] = glm::length(L.knee[s] - L.hip[s]);
        L.lc[s] = glm::length(L.ankle[s] - L.knee[s]);
    }
    return L;
}

BodyBlendIn make_in(const Fixture& fx, const Legs& L, float w) {
    BodyBlendIn in;
    in.station_p = fx.p;
    in.frame = fx.f;
    in.w = w;
    for (int s = 0; s < 2; ++s) {
        in.hip[s] = L.hip[s];
        in.r3_knee[s] = L.knee[s];
        in.r3_ankle[s] = L.ankle[s];
        in.len_thigh[s] = L.lt[s];
        in.len_calf[s] = L.lc[s];
    }
    return in;
}

}  // namespace

// ---------------------------------------------------------------------------
// LEG 1 + 2. Stage 0 is bit-identical, AND the mechanism is alive.
//
// The pair is the point. "Returns its input at w = 0" is passed by a function
// that returns its input FOREVER, so the identity leg is worthless without a
// companion proving the thing moves. Non-vacuity is asserted first.
TEST_CASE("body blend: stage 0 is bit identical and the blend is alive") {
    Fixture fx;
    const Legs L = bind_legs(fx);

    // ALIVE: displace the chain's leg stations and the targets must follow.
    Fixture moved = fx;
    for (const char* n : {"thigh", "knee", "ankle", "toe"})
        moved.p[body_chain_station_of(n)] += glm::vec3(0.0f, 0.10f, -0.25f);
    const BodyBlendOut hot = body_blend_legs(make_in(moved, L, 1.0f));
    float travel = 0.0f;
    for (int s = 0; s < 2; ++s)
        travel = std::fmax(travel, glm::length(hot.leg[s].ankle - L.ankle[s]));
    REQUIRE(travel > 0.05f);  // it MOVED -- without this leg 1 is theatre

    // IDENTICAL: exactly zero weight returns the R3 inputs, bit for bit.
    const BodyBlendOut cold = body_blend_legs(make_in(fx, L, 0.0f));
    for (int s = 0; s < 2; ++s) {
        REQUIRE(cold.leg[s].ankle.x == L.ankle[s].x);
        REQUIRE(cold.leg[s].ankle.y == L.ankle[s].y);
        REQUIRE(cold.leg[s].ankle.z == L.ankle[s].z);
        REQUIRE(cold.leg[s].knee.x == L.knee[s].x);
        REQUIRE(cold.leg[s].knee.y == L.knee[s].y);
        REQUIRE(cold.leg[s].knee.z == L.knee[s].z);
        REQUIRE(cold.leg[s].w_eff == 0.0f);
    }
}

// ---------------------------------------------------------------------------
// LEG 3. The target NEVER leaves the reachable shell, so solve_chain's silent
// clamp can never engage.
//
// This is the leg that fails if the reach limit is deleted: solve_chain clamps
// an out-of-reach target into the shell WITHOUT TELLING ANYONE, so the foot
// stops moving while every caller believes it moved.
TEST_CASE("body blend: the ankle target stays inside the leg's own reach") {
    Fixture fx;
    const Legs L = bind_legs(fx);
    // Trail the chain far behind the machine -- the ordinary stage-3 state,
    // and far beyond what a leg can reach.
    Fixture far = fx;
    for (const char* n : {"thigh", "knee", "ankle", "toe"})
        far.p[body_chain_station_of(n)] += glm::vec3(0.0f, 0.6f, -1.6f);

    bool any_limited = false;
    for (int k = 0; k <= 20; ++k) {
        const float w = static_cast<float>(k) / 20.0f;
        const BodyBlendOut o = body_blend_legs(make_in(far, L, w));
        for (int s = 0; s < 2; ++s) {
            const float dmin = std::fabs(L.lt[s] - L.lc[s]) + 1.0e-3f;
            const float dmax = L.lt[s] + L.lc[s] - 1.0e-3f;
            const float d = glm::length(o.leg[s].ankle - L.hip[s]);
            REQUIRE(d <= dmax + 1.0e-4f);
            REQUIRE(d >= dmin - 1.0e-4f);
            if (o.leg[s].reach_limited) any_limited = true;
        }
    }
    // NON-VACUITY: this fixture must actually drive the limiter, or the leg
    // above is just restating that the bind pose is legal.
    REQUIRE(any_limited);
}

// ---------------------------------------------------------------------------
// LEG 4. The blend stays ON its own R3 -> chain segment, and advances
// monotonically along it.
//
// Both halves matter and they are why the reach limit clamps the WEIGHT and
// not the POINT: a radial clamp of the point into the annulus pulls it toward
// the hip -- toward the machine -- and leaves the segment, which would make
// the fork immeasurable and the motion non-monotone.
TEST_CASE("body blend: targets stay on the segment and advance monotonically") {
    Fixture fx;
    const Legs L = bind_legs(fx);
    Fixture moved = fx;
    for (const char* n : {"thigh", "knee", "ankle", "toe"})
        moved.p[body_chain_station_of(n)] += glm::vec3(0.05f, 0.35f, -0.9f);

    float prev[2] = {-1.0f, -1.0f};
    // ★ THE SEGMENT'S FAR END IS RECONSTRUCTED HERE, INDEPENDENTLY, from the
    // measured table -- NOT taken from the blend's own output at w = 1. That
    // output is itself reach-limited on this fixture, so using it would define
    // the segment by the very clamp this leg exists to grade, and the check
    // would pass no matter what the clamp did. (It did exactly that on the
    // first run: 23 mm off, and the defect was in the test.)
    glm::vec3 end[2];
    {
        const int st = body_chain_station_of("ankle");
        for (int s = 0; s < 2; ++s)
            end[s] = moved.p[st] + row(st).off_m[static_cast<std::size_t>(s)];
    }
    for (int k = 0; k <= 20; ++k) {
        const float w = static_cast<float>(k) / 20.0f;
        const BodyBlendOut o = body_blend_legs(make_in(moved, L, w));
        for (int s = 0; s < 2; ++s) {
            const float d = glm::length(o.leg[s].ankle - L.ankle[s]);
            REQUIRE(d >= prev[s] - 1.0e-5f);  // monotone in the weight
            prev[s] = d;
            // ON the segment: the point is the exact lerp at its own w_eff.
            const glm::vec3 want =
                L.ankle[s] + (end[s] - L.ankle[s]) * o.leg[s].w_eff;
            if (o.leg[s].w_eff > 0.0f)
                REQUIRE(glm::length(o.leg[s].ankle - want) < 1.0e-5f);
        }
    }
}

// ---------------------------------------------------------------------------
// LEG 5. THE SEAT. A lerp between two individually-legal points crosses
// non-convex free space, and neither endpoint nor the chain's own keep-out can
// see it -- the keep-out grades the CHAIN, and the blended drawn leg is a
// third geometry that nothing else grades.
TEST_CASE("body blend: the blended leg is pushed out of the seat solid") {
    Fixture fx;
    const Legs L = bind_legs(fx);

    TrailChainInput seat;
    seat.n_seat_samples = 8;
    seat.seat_z_rear = -1.005f;
    seat.seat_z_front = 0.3076f;
    seat.seat_x_half = 0.20597f;
    seat.seat_y_bottom = 0.3803f;
    for (int i = 0; i < seat.n_seat_samples; ++i) seat.seat_top_y[i] = 0.72f;

    // Drive the chain's ankle up INTO the seat box, straight through the pan.
    Fixture into = fx;
    into.p[body_chain_station_of("ankle")] = glm::vec3(0.0f, 0.55f, -0.45f);
    into.p[body_chain_station_of("knee")] = glm::vec3(0.0f, 0.58f, -0.30f);

    BodyBlendIn raw = make_in(into, L, 1.0f);
    const BodyBlendOut without = body_blend_legs(raw);

    raw.seat_pr = &fx.pr;
    raw.seat_in = &seat;
    const BodyBlendOut with = body_blend_legs(raw);

    // NON-VACUITY FIRST: the unguarded blend must really be inside the solid,
    // or "the guarded one is outside" says nothing at all.
    bool was_inside = false;
    for (int s = 0; s < 2; ++s)
        if (with.leg[s].seat_push_m > 0.0f) was_inside = true;
    REQUIRE(was_inside);

    // And the guard moved it.
    bool moved = false;
    for (int s = 0; s < 2; ++s)
        if (glm::length(with.leg[s].ankle - without.leg[s].ankle) > 1.0e-4f ||
            glm::length(with.leg[s].knee - without.leg[s].knee) > 1.0e-4f)
            moved = true;
    REQUIRE(moved);
}

// ---------------------------------------------------------------------------
// LEG 6. ONE RULE for the drawn leg and the graded box.
//
// The chain is the body's MIDLINE. A side is its measured joint offset carried
// through the live station frame -- the same carry a probe box gets. This leg
// pins that the two sides straddle the midline and that side [0] is +X, which
// is what makes "the leg the game draws" and "the leg the keep-out grades"
// reconstruct from one frame by one rule.
TEST_CASE("body blend: the two sides straddle the chain midline") {
    Fixture fx;
    const Legs L = bind_legs(fx);
    Fixture moved = fx;
    for (const char* n : {"thigh", "knee", "ankle", "toe"})
        moved.p[body_chain_station_of(n)] += glm::vec3(0.0f, 0.20f, -0.55f);
    const BodyBlendOut o = body_blend_legs(make_in(moved, L, 1.0f));

    const int st = body_chain_station_of("ankle");
    // Identity frames, so the station frame's +X is model +X.
    REQUIRE(o.leg[0].ankle.x > moved.p[st].x);
    REQUIRE(o.leg[1].ankle.x < moved.p[st].x);
    // Symmetric about the midline, because the measured offsets are.
    const float du0 = o.leg[0].ankle.x - moved.p[st].x;
    const float du1 = moved.p[st].x - o.leg[1].ankle.x;
    REQUIRE(std::fabs(du0 - du1) < 1.0e-3f);
}

// ---------------------------------------------------------------------------
// LEG 7. The bend normal actually aims the knee.
//
// body_blend_bend_normal inverts solve_chain's `bend = normalize(cross(n,
// dir))`. This leg re-runs solve_chain's OWN construction and checks the knee
// lands where it was asked to -- so the inversion is pinned against the
// formula it inverts, not against itself.
TEST_CASE("body blend: the bend normal puts the knee where it was asked") {
    const glm::vec3 hip(0.20f, 0.69f, -0.38f);
    const glm::vec3 ankle(0.30f, 0.33f, 0.10f);
    const glm::vec3 want_knee(0.27f, 0.70f, -0.17f);
    const float l0 = glm::length(want_knee - hip);
    const float l1 = glm::length(ankle - want_knee);

    const glm::vec3 fallback(0.0f, 0.0f, 1.0f);
    const glm::vec3 n = body_blend_bend_normal(hip, ankle, want_knee, fallback);
    REQUIRE(glm::length(n - fallback) > 1.0e-3f);  // it did NOT bail out

    // solve_chain's construction, verbatim.
    glm::vec3 st = ankle - hip;
    float d = glm::length(st);
    const float dmin = std::fabs(l0 - l1) + 1.0e-3f;
    const float dmax = l0 + l1 - 1.0e-3f;
    d = d < dmin ? dmin : (d > dmax ? dmax : d);
    const glm::vec3 dir = glm::normalize(st);
    const float a = (l0 * l0 + d * d - l1 * l1) / (2.0f * d);
    const float h2 = l0 * l0 - a * a;
    const float h = h2 > 0.0f ? std::sqrt(h2) : 0.0f;
    glm::vec3 bend = glm::cross(n, dir);
    const float bl = glm::length(bend);
    bend = bl > 1.0e-5f ? bend / bl : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 got = hip + dir * a + bend * h;

    REQUIRE(glm::length(got - want_knee) < 1.0e-3f);

    // Degenerate geometry names no plane and must hand the fallback BACK
    // rather than invent one -- that is how a limb spins.
    const glm::vec3 straight = hip + (ankle - hip) * 0.5f;
    REQUIRE(body_blend_bend_normal(hip, ankle, straight, fallback) ==
            fallback);
}
