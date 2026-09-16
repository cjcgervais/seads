// ★ THE TRAILING-CHAIN SOLVER GATE -- docs/SCARF_SPEC.md §5, test by test.
//
// Every case feeds render/trail_chain.* directly: no GLB, no raylib, no clock.
// The solver is the thing R4 superman re-anchors onto the body (§7.6), so what
// is pinned here is the CONTRACT (determinism, invariants, the lift law, the
// keep-outs, the frames a future skinned cloth will ride), not a picture.
//
// ASCII names only: a non-ASCII Catch2 name is silently never run in this repo.
//
// ★ THE SHARED TEST RIG (§5 preamble): dt = 1/120, anchor at the origin, width
// axis (1,0,0), gravity (0,-1,0), back plane normal (0,0,-1) through
// (0,0,0.05) (the anchor 50 mm behind the "neck") keep-out 0.06, head sphere at
// (0,0.17,0.10) r 0.18 (the anchor 17 mm outside it -- the shipped rig's own
// margin is 12.5 mm, and the effective-size rule makes both safe).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <glm/glm.hpp>

#include "render/rider_pose.h"  // the MEASURED seat profile, never a retype
#include "render/trail_chain.h"

using render::TrailChainInput;
using render::TrailChainParams;
using render::TrailChainState;

namespace {

constexpr float kDt = 1.0f / 120.0f;
constexpr int kSettleSteps = 2400;  // 20 s -- margin over the ~3 s settle
constexpr float kDeg = 57.29577951308232f;

TrailChainParams base_params() {
    TrailChainParams pr;
    pr.dt_s = kDt;
    return pr;
}

TrailChainInput base_input() {
    TrailChainInput in;
    in.anchor = glm::vec3(0.0f);
    in.wind_mps = glm::vec3(0.0f);
    in.gravity_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    in.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
    in.head_center = glm::vec3(0.0f, 0.17f, 0.10f);
    in.back_origin = glm::vec3(0.0f, 0.0f, 0.05f);
    in.back_normal = glm::vec3(0.0f, 0.0f, -1.0f);
    return in;
}

// The head sphere in the shared rig is r 0.18 with the anchor 0.1972 m away,
// so the EFFECTIVE radius (§3, red-team P2-1) is 0.1972 - 0.02 = 0.1772.
float head_r_eff(const TrailChainParams& pr, const TrailChainInput& in) {
    const float d = glm::length(in.anchor - in.head_center);
    return pr.head_keepout_r_m > 0.0f
               ? std::fmin(pr.head_keepout_r_m,
                           d - render::kTrailChainHeadMarginM)
               : 0.0f;
}

bool finite3(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

void settle(TrailChainState& st, const TrailChainParams& pr,
            const TrailChainInput& in, int steps) {
    for (int i = 0; i < steps; ++i) render::trail_chain_step(st, pr, in);
}

// Prime the way sled_model does at bind: straight down from the anchor.
void prime(TrailChainState& st, const TrailChainParams& pr,
           const TrailChainInput& in) {
    render::trail_chain_reset(st, pr, in.anchor, in.gravity_dir);
}

// The 300-step input sequence tests 1 and 2 share: wind ramps 0 -> 20 m/s with
// a 30 deg yaw sweep, so the chain is never at equilibrium.
TrailChainInput swept_input(int k) {
    TrailChainInput in = base_input();
    const float f = static_cast<float>(k) / 299.0f;
    const float v = 20.0f * f;
    const float yaw = 30.0f * f / kDeg;
    in.wind_mps = glm::vec3(v * std::sin(yaw), 0.0f, -v * std::cos(yaw));
    return in;
}


// ===== THE R4a SUPERMAN RIG ==================================================
// The BODY as a chain, hips-to-boots trailing from the grips -- section 7.6's
// "same solver, anchor moved to the grips". The numbers are the measurement
// rig's (tools/sled_probe.cpp, `Rig`): 5 x 0.35 m of man, and drag_k 0.005 /m
// which is a BODY (rho*Cd*A/2m for 87.5 kg), NOT the scarf's ribbon 0.153 --
// reuse that and a man streams to 54 deg of lift at 3 m/s.
//
// Both keep-outs are OFF, which is what 7.6 means by "the chain IS the body":
// with head_keepout_r_m = 0 and a zero back normal, apply_constraints reduces
// to exactly the distance pass.
TrailChainParams superman_params() {
    TrailChainParams pr;
    pr.dt_s = kDt;
    pr.segments = 5;
    pr.seg_len_m = 0.35f;
    pr.drag_k_per_m = 0.005f;
    pr.head_keepout_r_m = 0.0f;
    return pr;
}

TrailChainInput superman_input() {
    TrailChainInput in;  // back_normal defaults to zero = the plane is off
    in.gravity_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    in.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
    return in;
}

float lift_deg(const TrailChainState& st, const TrailChainInput& in) {
    return render::trail_chain_lift_rad(st, in.gravity_dir) * kDeg;
}

// Run a fixed field to rest and hand back the settled state.
TrailChainState settled(const TrailChainParams& pr, const TrailChainInput& in,
                        int steps = kSettleSteps) {
    TrailChainState st;
    render::trail_chain_reset(st, pr, in.anchor, in.gravity_dir);
    settle(st, pr, in, steps);
    return st;
}

// The seat solid, MEASURED: render/rider_pose.* carries
// measure_seat_profile.py's own emitted table (a downward raycast at x = 0 over
// the shipped assets/sled/indy650.glb `seat` triangles). This copies the table
// into the solver's input -- it does not retype a single number.
void give_seat(TrailChainInput& in) {
    const std::array<float, render::kSeatStations>& prof =
        render::seat_profile_y();
    REQUIRE(render::kSeatStations <= render::kTrailChainMaxSeatSamples);
    in.n_seat_samples = render::kSeatStations;
    in.seat_z_rear = render::kSeatZRearM;
    in.seat_z_front = render::kSeatZFrontM;
    in.seat_x_half = render::kSeatXHalfM;
    in.seat_y_bottom = render::kSeatYBottomM;
    for (int k = 0; k < render::kSeatStations; ++k)
        in.seat_top_y[k] = prof[static_cast<std::size_t>(k)];
}

// Is p strictly inside the measured seat solid? Written from rider_pose's OWN
// functions, deliberately NOT from trail_chain's private copy -- a keep-out
// checked against its own arithmetic can only ever agree with itself.
bool inside_seat(const glm::vec3& p) {
    return std::fabs(p.x) < render::kSeatXHalfM &&
           p.z > render::kSeatZRearM && p.z < render::kSeatZFrontM &&
           p.y > render::kSeatYBottomM && p.y < render::seat_top_y(p.z);
}

}  // namespace

// -- 1 ------------------------------------------------------------------------
TEST_CASE("trail chain is bit deterministic over an identical input sequence",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    TrailChainState a, b;
    prime(a, pr, base_input());
    prime(b, pr, base_input());
    for (int k = 0; k < 300; ++k) {
        const TrailChainInput in = swept_input(k);
        render::trail_chain_step(a, pr, in);
        render::trail_chain_step(b, pr, in);
    }
    REQUIRE(a.n == b.n);
    // The state arrays are value-initialised, so even the unused tail compares.
    for (int i = 0; i <= render::kTrailChainMaxSegments; ++i) {
        REQUIRE(a.p[i].x == b.p[i].x);
        REQUIRE(a.p[i].y == b.p[i].y);
        REQUIRE(a.p[i].z == b.p[i].z);
        REQUIRE(a.p_prev[i].x == b.p_prev[i].x);
        REQUIRE(a.p_prev[i].y == b.p_prev[i].y);
        REQUIRE(a.p_prev[i].z == b.p_prev[i].z);
    }
}

// -- 2 ------------------------------------------------------------------------
TEST_CASE("trail chain holds the pin and the segment lengths every step",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    TrailChainState st;
    prime(st, pr, base_input());
    for (int k = 0; k < 300; ++k) {
        const TrailChainInput in = swept_input(k);
        render::trail_chain_step(st, pr, in);
        REQUIRE(st.p[0].x == in.anchor.x);
        REQUIRE(st.p[0].y == in.anchor.y);
        REQUIRE(st.p[0].z == in.anchor.z);
        for (int i = 0; i <= st.n; ++i) REQUIRE(finite3(st.p[i]));
        for (int i = 1; i <= st.n; ++i) {
            const float l = glm::length(st.p[i] - st.p[i - 1]);
            REQUIRE(std::fabs(l - pr.seg_len_m) < 1.0e-4f);
        }
    }
}

// -- 3 ------------------------------------------------------------------------
TEST_CASE("trail chain hangs straight down in still air", "[trail_chain]") {
    const TrailChainParams pr = base_params();
    const TrailChainInput in = base_input();
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, kSettleSteps);
    const float lift = render::trail_chain_lift_rad(st, in.gravity_dir) * kDeg;
    REQUIRE(lift < 1.0f);
    const float chord = glm::length(st.p[st.n] - st.p[0]);
    REQUIRE(std::fabs(chord - static_cast<float>(st.n) * pr.seg_len_m) < 1.0e-3f);
    REQUIRE(std::fabs(st.p[st.n].x) < 2.0e-3f);
    REQUIRE(std::fabs(st.p[st.n].z) < 2.0e-3f);
}

// -- 4 ------------------------------------------------------------------------
TEST_CASE("trail chain lift rises monotonically with speed", "[trail_chain]") {
    const TrailChainParams pr = base_params();
    const float speeds[9] = {0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f, 20.0f,
                             25.0f};
    float lift[9] = {0.0f};
    for (int s = 0; s < 9; ++s) {
        TrailChainInput in = base_input();
        // Wind blows the chain BACKWARD (model -Z), the direction the back
        // half-space allows -- forward travel is what makes this wind.
        in.wind_mps = glm::vec3(0.0f, 0.0f, -speeds[s]);
        TrailChainState st;
        prime(st, pr, in);
        settle(st, pr, in, 1200);
        lift[s] = render::trail_chain_lift_rad(st, in.gravity_dir) * kDeg;
    }
    for (int s = 1; s < 9; ++s) REQUIRE(lift[s] > lift[s - 1]);
    REQUIRE(lift[1] < 10.0f);                            // 2 m/s
    REQUIRE(lift[4] > 40.0f);                            // 8 m/s
    REQUIRE(lift[4] < 50.0f);
    REQUIRE(lift[7] > 75.0f);                            // 20 m/s
}

// -- 5 ------------------------------------------------------------------------
TEST_CASE("trail chain trails downwind and follows a side change",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    TrailChainInput in = base_input();
    in.wind_mps = glm::vec3(12.0f, 0.0f, 0.0f);  // from the rider's left
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, 1200);
    auto azimuth_err = [](const TrailChainState& s, const glm::vec3& w) {
        const glm::vec3 c = s.p[s.n] - s.p[0];
        const glm::vec2 ch(c.x, c.z);
        const glm::vec2 wh(w.x, w.z);
        return std::acos(glm::dot(glm::normalize(ch), glm::normalize(wh))) *
               kDeg;
    };
    REQUIRE(azimuth_err(st, in.wind_mps) < 10.0f);
    in.wind_mps = glm::vec3(-12.0f, 0.0f, 0.0f);  // and now from the right
    settle(st, pr, in, 180);                      // 1.5 s
    REQUIRE(azimuth_err(st, in.wind_mps) < 10.0f);
}

// -- 6 ------------------------------------------------------------------------
TEST_CASE("trail chain never enters the head sphere", "[trail_chain]") {
    const TrailChainParams pr = base_params();
    // ★ THE BACK PLANE IS OFF HERE, and it has to be: the head sits FORWARD of
    // the torso plane, so with both keep-outs armed the plane stops the chain
    // long before the sphere does and the case would be vacuous. This is the
    // sphere's own test (§5 case 6), so the sphere is the only keep-out armed.
    const float r_eff = head_r_eff(pr, base_input());
    SECTION("wind straight at the head, +Z, 15 m/s") {
        TrailChainInput in = base_input();
        in.back_normal = glm::vec3(0.0f);
        in.wind_mps = glm::vec3(0.0f, 0.0f, 15.0f);
        TrailChainState st;
        prime(st, pr, in);
        for (int k = 0; k < 600; ++k) {
            render::trail_chain_step(st, pr, in);
            for (int i = 1; i <= st.n; ++i)
                REQUIRE(glm::length(st.p[i] - in.head_center) >
                        r_eff - 1.0e-4f);
        }
        REQUIRE(glm::length(st.p[st.n] - st.p[0]) >= 4.0f * pr.seg_len_m);
    }
    SECTION("wind aimed at the sphere centre -- proves the projection fires") {
        TrailChainInput in = base_input();
        in.back_normal = glm::vec3(0.0f);
        in.wind_mps = glm::normalize(in.head_center - in.anchor) * 15.0f;
        TrailChainState st;
        prime(st, pr, in);
        float closest = 1.0e9f;
        for (int k = 0; k < 600; ++k) {
            render::trail_chain_step(st, pr, in);
            for (int i = 1; i <= st.n; ++i) {
                const float d = glm::length(st.p[i] - in.head_center);
                REQUIRE(d > r_eff - 1.0e-4f);
                if (d < closest) closest = d;
            }
        }
        // NON-VACUITY: something actually rode the surface of the sphere.
        REQUIRE(closest < r_eff + 1.0e-3f);
        REQUIRE(glm::length(st.p[st.n] - st.p[0]) >= 4.0f * pr.seg_len_m);
    }
}

// -- 7 ------------------------------------------------------------------------
TEST_CASE("trail chain stays behind the tilted torso plane", "[trail_chain]") {
    // ★ §3b, and this is the case the red-team found (P1-1): the POSED torso
    // leans 33 deg FORWARD, so a plane that is merely "vertical through the
    // neck" would let the scarf hang through the chest. Neck at the origin,
    // pelvis at (0,-0.52,+0.34) -- the spine climbs FORWARD (-Z here), so the
    // axis UP the spine is normalize((0, 0.52, -0.34)) and "back" is +Z.
    TrailChainParams pr = base_params();
    pr.head_keepout_r_m = 0.0f;  // the plane is what is on trial here
    const glm::vec3 neck(0.0f);
    const glm::vec3 pelvis(0.0f, -0.52f, 0.34f);
    const glm::vec3 axis = glm::normalize(neck - pelvis);
    REQUIRE(std::fabs(std::acos(glm::dot(axis, glm::vec3(0, 1, 0))) * kDeg -
                      33.0f) < 0.5f);  // the 33 deg is measured, not assumed
    const glm::vec3 back = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f) -
                                          axis * axis.z);
    TrailChainInput in = base_input();
    in.anchor = neck + back * 0.075f;  // the knot, 75 mm behind the neck
    in.back_origin = neck;
    // §3b derives the normal from where the rig put the knot, never from an axis.
    glm::vec3 b = in.anchor - neck;
    b -= axis * glm::dot(b, axis);
    in.back_normal = glm::normalize(b);
    REQUIRE(std::fabs(glm::dot(in.back_normal, axis)) < 1.0e-5f);

    TrailChainState st;
    prime(st, pr, in);
    float worst = 1.0e9f;
    auto run = [&](const glm::vec3& wind, int steps) {
        in.wind_mps = wind;
        for (int k = 0; k < steps; ++k) {
            render::trail_chain_step(st, pr, in);
            for (int i = 1; i <= st.n; ++i) {
                const float s =
                    glm::dot(st.p[i] - in.back_origin, in.back_normal);
                if (s < worst) worst = s;
            }
        }
    };
    run(glm::vec3(0.0f), kSettleSteps);
    const float hang_lift =
        render::trail_chain_lift_rad(st, in.gravity_dir) * kDeg;
    run(glm::vec3(0.0f, 0.0f, 6.0f), 1200);   // 6 m/s "backward" (downwind)
    run(glm::vec3(0.0f, 0.0f, -6.0f), 1200);  // 6 m/s straight at the chest
    REQUIRE(worst >= pr.back_keepout_m - 1.0e-4f);

    // ★ SPEC DEVIATION, stated with the arithmetic (§5 case 7 asks for
    // "lift < 25 deg"). The scarf at rest LIES ON THE BACK, and this back is
    // tilted 33.2 deg from vertical, so gravity projected into the keep-out
    // plane points 33.2 deg off down: the drape angle IS the torso tilt and
    // 25 deg is arithmetically unreachable. MEASURED here: 31.4 deg (the chord
    // is a little shallower than 33.2 because the anchor sits 15 mm proud of
    // the plane). The INTENT of the case -- "it lies on the back, it is not
    // pushed into the air" -- is what is gated, at the torso tilt plus margin.
    REQUIRE(hang_lift < 35.0f);
    REQUIRE(hang_lift > 25.0f);  // and it really is lying along the tilt
}

// -- 7b -----------------------------------------------------------------------
TEST_CASE("trail chain at rest against the plane COMES TO REST",
          "[trail_chain]") {
    // ★ THE POGO GATE (2026-09-06, Chad: "scarf still bouncing when parked").
    // Case 7 runs this exact contact and gates only penetration and lift --
    // so a chain that BOUNCED on the plane forever passed every case in this
    // file, twice (the seat/spring adversary, then the back-plane pogo: bare
    // `p += d` projections read by Verlet as velocity, restitution ~1,
    // sustained 10-48 mm/frame at a standstill). This case gates the missing
    // phenomenon: with zero wind and a static rig, a chain resting ON the
    // keep-out plane must actually STOP. rest_push (the inelastic contact in
    // apply_constraints) is what makes it pass.
    TrailChainParams pr = base_params();
    pr.head_keepout_r_m = 0.0f;
    const glm::vec3 neck(0.0f);
    const glm::vec3 pelvis(0.0f, -0.52f, 0.34f);
    const glm::vec3 axis = glm::normalize(neck - pelvis);
    TrailChainInput in = base_input();
    in.anchor = neck + glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f) -
                                      axis * axis.z) *
                           0.075f;
    in.back_origin = neck;
    glm::vec3 b = in.anchor - neck;
    b -= axis * glm::dot(b, axis);
    in.back_normal = glm::normalize(b);
    in.wind_mps = glm::vec3(0.0f);

    TrailChainState st;
    prime(st, pr, in);
    for (int k = 0; k < 1200; ++k) render::trail_chain_step(st, pr, in);
    // The still window: 100 further steps, every station's per-step implied
    // displacement under 10 microns. Pre-rest_push this measured MILLIMETRES.
    float still_worst = 0.0f;
    float closest = 1.0e9f;
    for (int k = 0; k < 100; ++k) {
        render::trail_chain_step(st, pr, in);
        for (int i = 1; i <= st.n; ++i) {
            const float d = glm::length(st.p[i] - st.p_prev[i]);
            if (d > still_worst) still_worst = d;
            const float s = glm::dot(st.p[i] - in.back_origin,
                                     in.back_normal);
            if (s < closest) closest = s;
        }
    }
    REQUIRE(still_worst < 1.0e-5f);
    // NON-VACUITY: the chain is resting ON the plane during the still window
    // (within 5 mm of the keep-out), not hanging free of the contact this
    // case exists to grade.
    REQUIRE(closest < pr.back_keepout_m + 5.0e-3f);
}

// -- 8 ------------------------------------------------------------------------
TEST_CASE("trail chain survives absurd wind, zero dt and a teleport",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    TrailChainState st;
    {
        TrailChainInput in = base_input();
        in.wind_mps = glm::vec3(0.0f, 0.0f, -1000.0f);
        prime(st, pr, in);
        for (int k = 0; k < 300; ++k) {
            render::trail_chain_step(st, pr, in);
            for (int i = 0; i <= st.n; ++i) REQUIRE(finite3(st.p[i]));
            for (int i = 1; i <= st.n; ++i)
                REQUIRE(std::fabs(glm::length(st.p[i] - st.p[i - 1]) -
                                  pr.seg_len_m) < 1.0e-4f);
        }
        in.wind_mps = glm::vec3(0.0f);
        settle(st, pr, in, kSettleSteps);
        REQUIRE(render::trail_chain_lift_rad(st, in.gravity_dir) * kDeg < 2.0f);
    }
    SECTION("dt = 0 poses but does not advance") {
        TrailChainParams p0 = pr;
        p0.dt_s = 0.0f;
        TrailChainInput in = base_input();
        in.wind_mps = glm::vec3(0.0f, 0.0f, -8.0f);
        const TrailChainState before = st;
        render::trail_chain_step(st, p0, in);
        for (int i = 0; i <= st.n; ++i) {
            REQUIRE(st.p[i].x == before.p[i].x);
            REQUIRE(st.p[i].y == before.p[i].y);
            REQUIRE(st.p[i].z == before.p[i].z);
        }
    }
    SECTION("an anchor teleport re-primes straight along gravity") {
        TrailChainInput in = base_input();
        in.anchor = glm::vec3(3.0f, 0.0f, 0.0f);
        render::trail_chain_step(st, pr, in);
        for (int i = 0; i <= st.n; ++i) REQUIRE(finite3(st.p[i]));
        REQUIRE(glm::length(st.p[0] - in.anchor) == 0.0f);
        // one step of gravity from a straight-down prime is still straight down
        REQUIRE(render::trail_chain_lift_rad(st, in.gravity_dir) * kDeg < 1.0f);
    }
    SECTION("both keep-outs disabled is the R4 superman configuration") {
        TrailChainParams p0 = pr;
        p0.head_keepout_r_m = 0.0f;
        TrailChainInput in = base_input();
        in.back_normal = glm::vec3(0.0f);
        in.wind_mps = glm::vec3(3.0f, 2.0f, 14.0f);
        TrailChainState s2;
        prime(s2, p0, in);
        settle(s2, p0, in, 1200);
        for (int i = 0; i <= s2.n; ++i) REQUIRE(finite3(s2.p[i]));
        // nothing was clipped: the chain sits in free air, through the sphere
        REQUIRE(glm::length(s2.p[s2.n] - s2.p[0]) >
                5.9f * p0.seg_len_m);  // essentially straight
    }
}

// -- 9 ------------------------------------------------------------------------
TEST_CASE("trail chain frames are orthonormal right handed and continuous",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    TrailChainInput in = base_input();
    in.wind_mps = glm::vec3(0.0f, 0.0f, -12.0f);
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, 1200);
    glm::mat4 f[render::kTrailChainMaxSegments];
    render::trail_chain_frames(st, in.width_axis, f);
    for (int i = 0; i < st.n; ++i) {
        const glm::vec3 x(f[i][0]), y(f[i][1]), z(f[i][2]);
        REQUIRE(std::fabs(glm::length(x) - 1.0f) < 1.0e-4f);
        REQUIRE(std::fabs(glm::length(y) - 1.0f) < 1.0e-4f);
        REQUIRE(std::fabs(glm::length(z) - 1.0f) < 1.0e-4f);
        REQUIRE(std::fabs(glm::dot(x, y)) < 1.0e-4f);
        REQUIRE(std::fabs(glm::dot(y, z)) < 1.0e-4f);
        REQUIRE(std::fabs(glm::dot(x, z)) < 1.0e-4f);
        REQUIRE(glm::length(glm::cross(x, y) - z) < 1.0e-4f);  // right-handed
        REQUIRE(std::fabs(glm::determinant(glm::mat3(f[i])) - 1.0f) < 1.0e-4f);
        // +Y is the segment, and the origin is the station
        const glm::vec3 seg = glm::normalize(st.p[i + 1] - st.p[i]);
        REQUIRE(glm::length(y - seg) < 1.0e-4f);
        REQUIRE(glm::length(glm::vec3(f[i][3]) - st.p[i]) < 1.0e-6f);
        if (i > 0) REQUIRE(glm::dot(x, glm::vec3(f[i - 1][0])) > 0.9f);
    }
}

// -- 9b -----------------------------------------------------------------------
TEST_CASE("trail chain frames reproduce the GLB rest bone frames",
          "[trail_chain]") {
    // ★ MEASURED out of assets/sled/indy650.glb, 2026-08-19, by composing
    // sudburian_rig * root * pelvis * spine_01..03 * neck_01 * scarf_01 (stdlib
    // python, struct+json). This is the ONLY thing standing between a future
    // authored cloth and a flipped X/Z at the skin:
    //   scarf_01 rest WORLD pos (0.000000, 1.130831, -0.371228)
    //   scarf_01 rest WORLD +Y  (0.000000, -0.893377, -0.449307)
    //   scarf_01 rest WORLD +X  (1.000000,  0.000000, -0.000001)
    //   scarf_01 rest WORLD +Z  (-0.000001, 0.449306, -0.893375)
    // and scarf_02..06 each +0.080000 m along the parent's +Y, identity
    // rotation, so every bone in the chain shares those axes at rest. The
    // spec's (0,1.1308,-0.3712)/(0,-0.893,-0.449)/(0,0.449,-0.893) agree to
    // better than 1 mm / 4e-4.
    const glm::vec3 anchor(0.0f, 1.130831f, -0.371228f);
    const glm::vec3 bone_y(0.0f, -0.893377f, -0.449307f);
    const glm::vec3 bone_x(1.0f, 0.0f, 0.0f);
    const glm::vec3 bone_z(0.0f, 0.449306f, -0.893375f);
    const TrailChainParams pr = base_params();
    TrailChainState st;
    render::trail_chain_reset(st, pr, anchor, bone_y);
    // the reset IS the rest chain: p[k] = anchor + k * 0.08 * bone_y
    for (int k = 0; k <= st.n; ++k)
        REQUIRE(glm::length(st.p[k] -
                            (anchor + bone_y * (static_cast<float>(k) *
                                                pr.seg_len_m))) < 1.0e-5f);
    glm::mat4 f[render::kTrailChainMaxSegments];
    render::trail_chain_frames(st, bone_x, f);
    for (int i = 0; i < st.n; ++i) {
        REQUIRE(glm::length(glm::vec3(f[i][0]) - bone_x) < 1.0e-3f);
        REQUIRE(glm::length(glm::vec3(f[i][1]) - bone_y) < 1.0e-3f);
        REQUIRE(glm::length(glm::vec3(f[i][2]) - bone_z) < 1.0e-3f);
    }
}

// -- 9c -----------------------------------------------------------------------
TEST_CASE("trail chain frames are finite when the chain streams along the width",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    SECTION("a 25 m/s side blast, settled") {
        TrailChainInput in = base_input();
        in.wind_mps = glm::vec3(25.0f, 0.0f, 0.0f);  // straight along width_axis
        TrailChainState st;
        prime(st, pr, in);
        settle(st, pr, in, 1200);
        glm::mat4 f[render::kTrailChainMaxSegments];
        render::trail_chain_frames(st, in.width_axis, f);
        for (int i = 0; i < st.n; ++i) {
            const glm::vec3 x(f[i][0]), y(f[i][1]), z(f[i][2]);
            REQUIRE(finite3(x));
            REQUIRE(finite3(y));
            REQUIRE(finite3(z));
            REQUIRE(std::fabs(glm::length(x) - 1.0f) < 1.0e-4f);
            REQUIRE(std::fabs(glm::dot(x, y)) < 1.0e-4f);
            REQUIRE(std::fabs(glm::determinant(glm::mat3(f[i])) - 1.0f) <
                    1.0e-4f);
        }
    }
    SECTION("a chain EXACTLY along the width axis takes the fallback") {
        // Constructed, not settled: the degenerate case the fallback exists for.
        TrailChainState st;
        render::trail_chain_reset(st, pr, glm::vec3(0.0f),
                                  glm::vec3(1.0f, 0.0f, 0.0f));
        st.last_x = glm::vec3(1.0f, 0.0f, 0.0f);  // also degenerate, on purpose
        glm::mat4 f[render::kTrailChainMaxSegments];
        render::trail_chain_frames(st, glm::vec3(1.0f, 0.0f, 0.0f), f);
        for (int i = 0; i < st.n; ++i) {
            const glm::vec3 x(f[i][0]), y(f[i][1]), z(f[i][2]);
            REQUIRE(finite3(x));
            REQUIRE(finite3(y));
            REQUIRE(finite3(z));
            REQUIRE(std::fabs(glm::length(x) - 1.0f) < 1.0e-4f);
            REQUIRE(std::fabs(glm::length(z) - 1.0f) < 1.0e-4f);
            REQUIRE(std::fabs(glm::dot(x, y)) < 1.0e-4f);
            REQUIRE(std::fabs(glm::determinant(glm::mat3(f[i])) - 1.0f) <
                    1.0e-4f);
        }
    }
}

// -- 10 -----------------------------------------------------------------------
TEST_CASE("trail chain tail lags the root on a wind step -- the whip",
          "[trail_chain]") {
    const TrailChainParams pr = base_params();
    TrailChainInput in = base_input();
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, kSettleSteps);  // hang first
    in.wind_mps = glm::vec3(0.0f, 0.0f, -15.0f);
    const glm::vec3 down = in.gravity_dir;
    auto seg_lift = [&down](const TrailChainState& s, int i) {
        const glm::vec3 d = glm::normalize(s.p[i + 1] - s.p[i]);
        return std::acos(glm::dot(d, down)) * kDeg;
    };
    // final values first (a fresh chain, same step, fully settled)
    TrailChainState s2;
    prime(s2, pr, in);
    settle(s2, pr, in, kSettleSteps);
    const float root_63 = 0.63f * seg_lift(s2, 0);
    const float tail_63 = 0.63f * seg_lift(s2, s2.n - 1);
    int root_k = -1, tail_k = -1;
    for (int k = 0; k < 600 && (root_k < 0 || tail_k < 0); ++k) {
        render::trail_chain_step(st, pr, in);
        if (root_k < 0 && seg_lift(st, 0) >= root_63) root_k = k;
        if (tail_k < 0 && seg_lift(st, st.n - 1) >= tail_63) tail_k = k;
    }
    REQUIRE(root_k >= 0);
    REQUIRE(tail_k >= 0);
    const float lag_ms = static_cast<float>(tail_k - root_k) * kDt * 1000.0f;
    // MEASURED on the prototype: root 67 ms, tail 142 ms, lag 75 ms. The band
    // is what "a light damped-spring lag per segment" (§5) means as a number:
    // below 30 ms the chain is a rigid stick, above 600 ms it is a wet rope.
    REQUIRE(lag_ms >= 30.0f);
    REQUIRE(lag_ms <= 600.0f);
}


// =============================================================================
// R4a SUPERMAN -- THE NON-INERTIAL FRAME AND THE SEAT
//
// The four terms were MEASURED over 31 replayable tapes before a line of the
// plumbing was written (tools/sled_probe.cpp `superman`, commit fa94c351e);
// what these cases pin is that each of the four is actually IN the integrator
// and pointing the right way, and that the seat BOUNDS the fold.
//
// Each case is built so deleting the term it names makes it fail -- that is the
// point of the odd/even split in case 17 and of the two-arm shape everywhere
// else. A keep-out test that only asserts "did not penetrate" passes with the
// scenario never reaching the solid, so case 18 also proves the UNGATED arm
// goes inside.
// =============================================================================

// -- 14 -----------------------------------------------------------------------
TEST_CASE("trail chain hangs along g minus the frame acceleration",
          "[trail_chain]") {
    // A sled decelerating hard: in MODEL SPACE that is a uniform pseudo-force
    // forward, and this is the term that throws the man over the bars. Set the
    // frame acceleration to exactly one g along +z and the effective field is
    // (0, -g, -g): 45 degrees from down, pointing -z. Analytic, not a golden.
    const TrailChainParams pr = superman_params();
    TrailChainInput in = superman_input();
    in.frame_accel_mps2 = glm::vec3(0.0f, 0.0f, pr.gravity_mps2);
    const TrailChainState st = settled(pr, in);
    const glm::vec3 chord = st.p[st.n] - st.p[0];
    REQUIRE(lift_deg(st, in) == Catch::Approx(45.0).margin(0.5));
    REQUIRE(chord.z < 0.0f);                // away from the acceleration
    REQUIRE(std::fabs(chord.x) < 1.0e-3f);  // no out-of-plane drift
    // And the control: with no frame acceleration the same chain hangs.
    TrailChainInput ctl = superman_input();
    REQUIRE(lift_deg(settled(pr, ctl), ctl) < 0.1f);
}

// -- 15 -----------------------------------------------------------------------
TEST_CASE("trail chain is thrown outward by the centrifugal term",
          "[trail_chain]") {
    // A sustained flat turn. The chain hangs off an anchor 2 m from the spin
    // axis, so the centrifugal term has a real lever arm -- take r about the
    // wrong point and it silently vanishes.
    const TrailChainParams pr = superman_params();
    TrailChainInput in = superman_input();
    const float radius = 2.0f;
    in.frame_origin = glm::vec3(0.0f, 0.0f, radius);
    // omega^2 * R = g at the anchor, so the anchor end wants 45 degrees.
    const float w = std::sqrt(pr.gravity_mps2 / radius);
    in.frame_omega_rps = glm::vec3(0.0f, w, 0.0f);
    const TrailChainState st = settled(pr, in);
    const glm::vec3 chord = st.p[st.n] - st.p[0];
    // The field is NOT uniform -- it grows with radius -- so the settled chord
    // is bracketed by the two ends' own steady angles rather than equal to one.
    const float len = pr.seg_len_m * static_cast<float>(pr.segments);
    const float lo = std::atan(w * w * radius / pr.gravity_mps2) * kDeg;
    const float hi =
        std::atan(w * w * (radius + len) / pr.gravity_mps2) * kDeg;
    REQUIRE(lift_deg(st, in) > lo - 0.5f);
    REQUIRE(lift_deg(st, in) < hi + 0.5f);
    REQUIRE(chord.z < 0.0f);  // outward, away from the axis
    // Control: same rig, no rotation -> it hangs. The lever arm alone does
    // nothing; it is omega that makes it a force.
    TrailChainInput ctl = superman_input();
    ctl.frame_origin = in.frame_origin;
    REQUIRE(lift_deg(settled(pr, ctl), ctl) < 0.1f);
}

// -- 16 -----------------------------------------------------------------------
TEST_CASE("trail chain is swung sideways by the Euler term", "[trail_chain]") {
    // A machine whose rotation rate is CHANGING -- the snap of a roll starting
    // or being caught. alpha = (0, 0, a) about +z, chain hanging along -y so
    // r = (0, -Y, 0). Then alpha x r = (a*Y, 0, 0) and the pseudo-force is
    // MINUS that: (-a*Y, 0, 0), toward -x, growing with distance down the
    // chain. (The sign was got wrong in this comment first and the solver was
    // right -- which is what a directional assert is for.)
    const TrailChainParams pr = superman_params();
    TrailChainInput in = superman_input();
    const float len = pr.seg_len_m * static_cast<float>(pr.segments);
    in.frame_alpha_rps2 = glm::vec3(0.0f, 0.0f, pr.gravity_mps2 / len);
    const TrailChainState st = settled(pr, in);
    const glm::vec3 chord = st.p[st.n] - st.p[0];
    TrailChainInput ctl = superman_input();
    const float ctl_deg = lift_deg(settled(pr, ctl), ctl);
    REQUIRE(chord.x < 0.0f);
    REQUIRE(ctl_deg < 0.1f);
    REQUIRE(lift_deg(st, in) > ctl_deg + 5.0f);
}

// -- 17 -----------------------------------------------------------------------
TEST_CASE("trail chain out of plane response is odd in omega -- Coriolis",
          "[trail_chain]") {
    // HOW THIS ISOLATES ONE TERM OF FOUR. Put every particle in the plane
    // x = 0, spin about +y, and drive the chain with an in-plane (z) frame
    // acceleration. Then, by the algebra:
    //   - the centrifugal term on a point with r_x = 0 is (0, 0, +w^2 r_z):
    //     IN PLANE, and EVEN in w;
    //   - the uniform and Euler terms here are in plane and independent of w;
    //   - the Coriolis term -2 omega x v with v = (0, v_y, v_z) is
    //     (-2 w v_z, 0, 0): OUT OF PLANE, and ODD in w.
    // So any x displacement at all is Coriolis, and reversing the spin must
    // mirror it exactly. Delete the term and BOTH runs stay at x = 0, which is
    // the failure this case is built to catch.
    const TrailChainParams pr = superman_params();
    const int kSteps = 240;  // 2 s: a transient, deliberately not a steady state
    float tail_x[2] = {0.0f, 0.0f};
    for (int sgn = 0; sgn < 2; ++sgn) {
        TrailChainInput in = superman_input();
        in.frame_omega_rps = glm::vec3(0.0f, sgn == 0 ? 2.0f : -2.0f, 0.0f);
        in.frame_accel_mps2 = glm::vec3(0.0f, 0.0f, pr.gravity_mps2);
        TrailChainState st;
        render::trail_chain_reset(st, pr, in.anchor, in.gravity_dir);
        settle(st, pr, in, kSteps);
        tail_x[sgn] = st.p[st.n].x;
        for (int i = 0; i <= st.n; ++i) REQUIRE(finite3(st.p[i]));
    }
    REQUIRE(std::fabs(tail_x[0]) > 1.0e-3f);  // it left the plane
    REQUIRE(tail_x[0] * tail_x[1] < 0.0f);    // and reversing mirrored it
    REQUIRE(tail_x[0] == Catch::Approx(-tail_x[1]).margin(1.0e-6));
}

// -- 18 -----------------------------------------------------------------------
TEST_CASE(
    "trail chain never enters the seat solid, and the seat is what stops it",
    "[trail_chain]") {
    // Chad, 2026-08-25: "his legs will hit the seat." Anchor the body a metre
    // up and lay it out BACKWARD, clear of the machine -- superman, at the
    // moment the jolt ends -- then let gravity bring it down onto the seat.
    //
    // ★ THE LAYOUT MATTERS AND IS NOT COSMETIC. A least-penetration projection
    // pushes a point out through its SHALLOWEST face, so a chain primed
    // straight DOWN THROUGH the pan starts deep inside and legitimately exits
    // DOWNWARD -- it sinks through instead of resting on top. That is the same
    // class as the straight-line prime the back plane already documents in
    // trail_chain.cpp: prime the body in a LEGAL pose (which sled_model does,
    // at bind, off the rest pose) and the top face catches it, which is the
    // case here and the case the game is in.
    // The chain is FINER than the seat is thick -- see the case below, which
    // is why: this is a VERTEX keep-out, and 0.20 m links clear the 0.218 m
    // pan.
    TrailChainParams pr = superman_params();
    pr.segments = 9;
    pr.seg_len_m = 0.20f;
    TrailChainInput in = superman_input();
    in.anchor = glm::vec3(0.0f, 1.0f, 0.0f);
    TrailChainInput bare = in;  // same rig, no seat -- the liveness arm
    give_seat(in);

    const glm::vec3 lay(0.0f, 0.0f, -1.0f);  // out the back, above the seat
    TrailChainState st, ctl;
    render::trail_chain_reset(st, pr, in.anchor, lay);
    render::trail_chain_reset(ctl, pr, bare.anchor, lay);
    bool ctl_went_inside = false;
    for (int k = 0; k < 600; ++k) {
        render::trail_chain_step(st, pr, in);
        render::trail_chain_step(ctl, pr, bare);
        // EVERY step, not just the last: a keep-out that only holds at rest is
        // a keep-out the eye sees fail.
        for (int i = 1; i <= st.n; ++i) REQUIRE_FALSE(inside_seat(st.p[i]));
        for (int i = 1; i <= ctl.n; ++i)
            if (inside_seat(ctl.p[i])) ctl_went_inside = true;
    }
    REQUIRE(ctl_went_inside);  // the scenario really does drive into the seat
    // ...and the seat held him up: the ungated arm ends up hanging below the
    // machine, the gated one is draped on it.
    REQUIRE(st.p[st.n].y > ctl.p[ctl.n].y + 0.1f);
}

// -- 19 -----------------------------------------------------------------------
TEST_CASE("the seat keep out is a VERTEX keep out -- the chain must be finer "
          "than the seat is thick", "[trail_chain]") {
    // ★ THE BOUND THIS PROJECTION HAS, MEASURED AND WRITTEN DOWN RATHER THAN
    // DISCOVERED ON A DRIVE. Like the head sphere and the back planes beside
    // it, the seat is projected PER PARTICLE. A chain whose links are longer
    // than the solid is thick can therefore lie straight THROUGH it with a
    // vertex on either side and nothing inside for the projection to find.
    //
    // The measured pan is 0.598322 - 0.380322 = 0.218 m thick, so the shipped
    // body chain's segment length is bounded by that. This case pins both
    // halves: the measurement rig's coarse 0.35 m links STRADDLE the pan, and
    // 0.20 m links cannot. It is a constraint on the segment table the next
    // rung authors, not a defect to fix here -- closing it any other way means
    // a segment-vs-solid sweep, which is the contact solver LADDER 7.6 bans.
    const float pan_top = render::seat_top_y(0.0f);
    const float pan_thick = pan_top - render::kSeatYBottomM;
    REQUIRE(pan_thick == Catch::Approx(0.218).margin(0.001));

    // Straight down the centreline from 1.0 m up, coarse links: the vertices
    // land at 0.65 and 0.30, one either side of the pan, none inside.
    const float coarse = 0.35f;
    REQUIRE(coarse > pan_thick);
    bool coarse_straddles = false;
    for (int i = 0; i < 6; ++i) {
        const glm::vec3 a(0.0f, 1.0f - coarse * static_cast<float>(i), 0.0f);
        const glm::vec3 b(0.0f, a.y - coarse, 0.0f);
        if (a.y > pan_top && b.y < render::kSeatYBottomM) {
            coarse_straddles = true;
            REQUIRE_FALSE(inside_seat(a));
            REQUIRE_FALSE(inside_seat(b));
        }
    }
    REQUIRE(coarse_straddles);

    // Fine links: no segment can cross the pan without a vertex inside it.
    const float fine = 0.20f;
    REQUIRE(fine < pan_thick);
    for (int i = 0; i < 10; ++i) {
        const glm::vec3 a(0.0f, 1.0f - fine * static_cast<float>(i), 0.0f);
        const glm::vec3 b(0.0f, a.y - fine, 0.0f);
        if (a.y > pan_top && b.y < render::kSeatYBottomM)
            FAIL("a link shorter than the pan is thick straddled it");
    }
}

// -- 20 -----------------------------------------------------------------------
TEST_CASE("trail chain seat keep out disables when the anchor is inside it",
          "[trail_chain]") {
    // The anchor-pin rule, third occurrence (SCARF_SPEC section 3 red-team
    // P2-1): a root inside its own keep-out makes every constraint pass fight
    // the pin. The sphere and the plane shrink to clear the root; a solid
    // cannot, so it disables -- and disabling must be EXACT, not approximate.
    const TrailChainParams pr = superman_params();
    TrailChainInput in = superman_input();
    in.anchor = glm::vec3(0.0f, 0.5f, -0.3f);  // between the pan and the floor
    REQUIRE(inside_seat(in.anchor));
    TrailChainInput bare = in;
    give_seat(in);

    TrailChainState st, ctl;
    render::trail_chain_reset(st, pr, in.anchor, in.gravity_dir);
    render::trail_chain_reset(ctl, pr, bare.anchor, bare.gravity_dir);
    for (int k = 0; k < 300; ++k) {
        render::trail_chain_step(st, pr, in);
        render::trail_chain_step(ctl, pr, bare);
    }
    for (int i = 0; i <= render::kTrailChainMaxSegments; ++i) {
        REQUIRE(st.p[i].x == ctl.p[i].x);
        REQUIRE(st.p[i].y == ctl.p[i].y);
        REQUIRE(st.p[i].z == ctl.p[i].z);
    }
}

// -- 21 -----------------------------------------------------------------------
TEST_CASE(
    "trail frame field differentiates backward and never guesses the first "
    "sample",
    "[trail_chain]") {
    // The rule this encodes is structural (handoff section 2.2): a_frame and
    // alpha are finite-differenced off the ALREADY-SHIPPED velocity and
    // angular_vel, render-side, so nothing has to pass a debug sink through the
    // shipped kernel to get them.
    render::TrailFrameTracker tr;
    const glm::vec3 v0(3.0f, 0.0f, -1.0f), w0(0.0f, 0.5f, 0.0f);
    render::TrailFrameField f = render::trail_frame_field(tr, v0, w0, kDt);
    // First call: the field is reported, the DERIVATIVES are zero. Inventing a
    // previous sample out of the current one reads a respawn as hundreds of g.
    REQUIRE(f.omega_rps.y == w0.y);
    REQUIRE(f.accel_mps2 == glm::vec3(0.0f));
    REQUIRE(f.alpha_rps2 == glm::vec3(0.0f));

    const glm::vec3 v1 = v0 + glm::vec3(0.0f, 0.0f, -kDt * 9.81f);
    const glm::vec3 w1 = w0 + glm::vec3(kDt * 2.0f, 0.0f, 0.0f);
    f = render::trail_frame_field(tr, v1, w1, kDt);
    REQUIRE(f.accel_mps2.z == Catch::Approx(-9.81).margin(1.0e-3));
    REQUIRE(f.alpha_rps2.x == Catch::Approx(2.0).margin(1.0e-3));
    REQUIRE(f.omega_rps.x == w1.x);

    // A 0-tick frame poses, it does not differentiate.
    f = render::trail_frame_field(tr, v1 * 100.0f, w1, 0.0f);
    REQUIRE(f.accel_mps2 == glm::vec3(0.0f));
    REQUIRE(f.alpha_rps2 == glm::vec3(0.0f));
}

// -- 22 -----------------------------------------------------------------------
TEST_CASE("trail chain survives an absurd non inertial frame", "[trail_chain]") {
    // The no-NaN claim of the header, extended to the four new terms: a crash
    // spike is hundreds of g and tens of rad/s, and the solver must pose
    // through it rather than hand the renderer a NaN skeleton.
    const TrailChainParams pr = superman_params();
    TrailChainInput in = superman_input();
    in.frame_accel_mps2 = glm::vec3(0.0f, 3000.0f, -2000.0f);
    in.frame_omega_rps = glm::vec3(50.0f, -80.0f, 30.0f);
    in.frame_alpha_rps2 = glm::vec3(-900.0f, 400.0f, 700.0f);
    in.frame_origin = glm::vec3(0.0f, 0.0f, 5.0f);
    give_seat(in);
    TrailChainState st;
    render::trail_chain_reset(st, pr, in.anchor, in.gravity_dir);
    for (int k = 0; k < 600; ++k) {
        render::trail_chain_step(st, pr, in);
        for (int i = 0; i <= st.n; ++i) REQUIRE(finite3(st.p[i]));
        // The distance constraint is the one invariant that must hold no matter
        // how violent the field.
        for (int i = 1; i <= st.n; ++i)
            REQUIRE(glm::length(st.p[i] - st.p[i - 1]) ==
                    Catch::Approx(pr.seg_len_m).margin(1.0e-4));
    }
}

// -- 23/24 shared rig (SCARF-DRAPE) ------------------------------------------
// Staggered banded planes + per-station DISTINCT probe boxes, so the asserts
// pin the parts a mutation could delete (red-team 2026-09-03, finding 1):
//   - band 1 (upper torso) plane at z = 0.00 is STRICTER than band 0's at
//     z = +0.05, and stations 4-5 hang in band 0 -- only the NEIGHBOUR-band
//     test can hold them to band 1's plane. Delete the neighbour loop and
//     they fail by ~15 mm.
//   - hz grows per station (0.020 + 0.004 i) -- a station index off-by-one
//     shifts the required depth by 4 mm against a 1 mm tolerance.
//   - the anchor hangs at z = -0.06, BELOW band-0's requirement and ABOVE
//     band-1's, so gravity alone satisfies nothing the probes must enforce
//     (delete the support term and station 1 fails by 3 mm), while the
//     anchor pin never relaxes (s_anchor 0.06 > keep-out 0.04 in band 1).
// NOT pinned here, recorded honestly: the `c += dp` centre-ride inside the
// multi-band loop -- at equilibrium no second push fires, so only a settle
// transient could see it, and its failure direction is proud (safe).
namespace {
render::TrailChainParams drape_params() {
    render::TrailChainParams pr;
    pr.dt_s = kDt;
    pr.back_keepout_m = 0.04f;
    pr.back_keepout_min_m = 0.0f;
    pr.head_keepout_r_m = 0.0f;  // isolate the plane pass
    pr.n_probe_stations = render::kScarfSegments;
    for (int i = 0; i < render::kScarfSegments; ++i) {
        pr.probe[i][0].hx_m = 0.015f;
        pr.probe[i][0].hy_m = 0.030f;
        pr.probe[i][0].hz_m = 0.020f + 0.004f * static_cast<float>(i);
    }
    return pr;
}
render::TrailChainInput drape_input() {
    render::TrailChainInput in;
    in.anchor = glm::vec3(0.0f, 0.0f, -0.06f);
    in.wind_mps = glm::vec3(0.0f);
    in.gravity_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    in.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
    in.head_center = glm::vec3(0.0f, 100.0f, 0.0f);
    in.back_origin = glm::vec3(0.0f, 0.0f, 0.05f);
    in.back_normal = glm::vec3(0.0f, 0.0f, -1.0f);
    in.torso_origin = glm::vec3(0.0f, -0.6f, 0.0f);
    in.torso_axis = glm::vec3(0.0f, 1.0f, 0.0f);
    in.n_back_planes = 2;
    in.bp_origin[0] = glm::vec3(0.0f, 0.0f, 0.05f);  // band 0: lower torso
    in.bp_origin[1] = glm::vec3(0.0f, 0.0f, 0.00f);  // band 1: upper, STRICT
    in.bp_normal[0] = glm::vec3(0.0f, 0.0f, -1.0f);
    in.bp_normal[1] = glm::vec3(0.0f, 0.0f, -1.0f);
    in.bp_t[0] = 0.0f;
    in.bp_t[1] = 0.30f;
    in.bp_t[2] = 0.60f;
    return in;
}
// the depth every station owes against band 1's plane (the strict one).
float drape_need(const render::TrailChainParams& pr, int i) {
    return pr.back_keepout_m + pr.probe[i][0].hz_m;
}
}  // namespace

// -- 23 -----------------------------------------------------------------------
TEST_CASE(
    "banded planes hold each pod BOX off the coat, own band and neighbour",
    "[trail_chain]") {
    const render::TrailChainParams pr = drape_params();
    const render::TrailChainInput in = drape_input();
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, kSettleSteps);
    for (int i = 1; i < pr.n_probe_stations; ++i) {
        // clearance against the STRICT band-1 plane: for stations 1-3 that is
        // their own band; for 4-5 it is the NEIGHBOUR -- both must hold.
        const float s1 = glm::dot(st.p[i] - in.bp_origin[1], in.bp_normal[1]);
        REQUIRE(s1 >= drape_need(pr, i) - 1.0e-3f);
        // and no gross over-push: the box face rests near the keep-out.
        REQUIRE(s1 <= drape_need(pr, i) + 0.012f);
    }
    // control: no probes. Gravity hangs the chain at the anchor's z = -0.06,
    // which the centreline keep-out (0.04) already satisfies -- so it rests
    // BELOW every probed requirement above. The gap is what a deleted support
    // term, a deleted neighbour loop, or a station off-by-one erases.
    render::TrailChainParams ctl = pr;
    ctl.n_probe_stations = 0;
    TrailChainState cst;
    prime(cst, ctl, in);
    settle(cst, ctl, in, kSettleSteps);
    for (int i = 1; i <= cst.n; ++i) {
        const float s1 =
            glm::dot(cst.p[i] - in.bp_origin[1], in.bp_normal[1]);
        REQUIRE(s1 >= 0.04f - 1.0e-3f);
        REQUIRE(s1 < drape_need(pr, 1) - 1.0e-3f);
    }
}

// -- 24 -----------------------------------------------------------------------
TEST_CASE(
    "the presentation clamp re-applies the pod law and is a no-op without "
    "probes",
    "[trail_chain]") {
    // SCARF-DRAPE: the flutter wave displaces a COPY the solver never sees;
    // at 11.7 m/s that put the drawn tail -0.027 m inside the coat
    // (measured). trail_chain_present_clamp must push the displaced copy back
    // to box clearance -- in the particle's own band AND its neighbour (same
    // staggered rig as case 23, so stations 4-5 pin the neighbour rule here
    // too) -- and must not touch a chain with no probes.
    const render::TrailChainParams pr = drape_params();
    const render::TrailChainInput in = drape_input();
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, kSettleSteps);
    TrailChainState vis = st;
    for (int i = 1; i <= vis.n; ++i) vis.p[i].z += 0.05f;  // the "wave"
    render::trail_chain_present_clamp(vis, pr, in);
    for (int i = 1; i < pr.n_probe_stations; ++i) {
        const float s1 =
            glm::dot(vis.p[i] - in.bp_origin[1], in.bp_normal[1]);
        REQUIRE(s1 >= drape_need(pr, i) - 1.0e-3f);
    }
    // no probes: the clamp must not move a single component.
    render::TrailChainParams bare = pr;
    bare.n_probe_stations = 0;
    TrailChainState vis2 = st;
    for (int i = 1; i <= vis2.n; ++i) vis2.p[i].z += 0.05f;
    TrailChainState before = vis2;
    render::trail_chain_present_clamp(vis2, bare, in);
    for (int i = 0; i <= vis2.n; ++i) {
        REQUIRE(vis2.p[i].x == before.p[i].x);
        REQUIRE(vis2.p[i].y == before.p[i].y);
        REQUIRE(vis2.p[i].z == before.p[i].z);
    }
}

// -- 25 -----------------------------------------------------------------------
TEST_CASE("the coat slab ends at the collar and still ejects inside it",
          "[trail_chain]") {
    // SCARF-DRAPE slab (2026-09-04, Chad's back-lean report): the banded
    // keep-out used to CLAMP the band pick at the t ends, so the coat
    // followed the chain past the collar forever and blocked the one exit
    // gravity wants at a bent-forward pose -- the chain wadded above the
    // arch (measured: six particles in 0.09 of t, stacked to s +0.18).
    // With a live lateral bound the slab is bounded in t: a box past
    // [bp_t[0] - band, bp_t[n] + band] is past the coat and FREE, while one
    // inside the extent is ejected exactly as before.
    TrailChainParams pr = drape_params();
    pr.gravity_mps2 = 0.0f;  // hold the primed shape; only constraints act
    TrailChainInput in = drape_input();
    for (int b = 0; b < 2; ++b) {
        in.bp_lat_half[b] = 0.5f;   // wide: the lateral exit is never cheap
        in.bp_front[b] = -0.30f;    // far: the front exit is never cheap
    }
    // anchor ABOVE the collar (t = anchor.y + 0.6 = 0.75 > bp_t[2] = 0.60),
    // 20 mm on the coat side of the strict band-1 plane; chain primed
    // straight down through the banded range.
    in.anchor = glm::vec3(0.0f, 0.15f, -0.02f);
    TrailChainState st;
    prime(st, pr, in);
    settle(st, pr, in, 60);
    // p[1] (t 0.68, past the collar + grace 0.30/2=0.15? no: grace is one
    // band = 0.30, so free above t 0.90) -- compute honestly: band width =
    // (0.60 - 0.00) / 2 = 0.30, free above t = 0.90. p[1] at t ~0.68 is NOT
    // free; the free zone needs a particle above t 0.90 = y > 0.30. So
    // re-anchor higher instead of guessing:
    in.anchor = glm::vec3(0.0f, 0.45f, -0.02f);
    prime(st, pr, in);
    settle(st, pr, in, 60);
    // p[1] at y ~0.382, t ~0.98 > 0.90: past the coat, FREE -- it keeps the
    // primed z (the old clamped-band code pushed it behind band 1's plane,
    // z <= -0.06).
    REQUIRE(st.p[1].z == Catch::Approx(-0.02f).margin(1.0e-3));
    // the tail is inside the slab's t extent and violating band planes:
    // still ejected backward. p[5] is the last PROBED station (box support
    // hz 0.040 + the pin-relaxed keep-out 0.020 => z <= -0.060); p[6] has no
    // probe and takes the centreline rule, so it is not the one to pin.
    REQUIRE(st.p[5].z < -0.055f);
}

// -- 26 -----------------------------------------------------------------------
TEST_CASE("the presentation clamp honours every slab gate", "[trail_chain]") {
    // The clamp is the constraint loop's law applied to the drawn copy; each
    // gate is pinned on a hand-built state so a mutation to EITHER copy of
    // the gate arithmetic fails a case.
    TrailChainParams pr = drape_params();
    TrailChainInput in = drape_input();
    for (int b = 0; b < 2; ++b) {
        in.bp_lat_half[b] = 0.20f;
        in.bp_front[b] = -0.25f;
    }
    const auto mk = [&](const glm::vec3& p) {
        TrailChainState st;
        st.n = render::kScarfSegments;
        for (int i = 0; i <= st.n; ++i) {
            st.p[i] = p;
            st.p_prev[i] = p;
        }
        st.p[0] = in.anchor;
        st.primed = true;
        return st;
    };
    // (a) inside the slab (t mid-band, on the spine, 10 mm deep vs band 1's
    // plane): pushed OUT (backward -- the lateral and front exits are
    // farther).
    {
        TrailChainState st = mk(glm::vec3(0.0f, -0.15f, 0.01f));
        render::trail_chain_present_clamp(st, pr, in);
        REQUIRE(st.p[3].z < -0.02f);
    }
    // (b) beside the torso (|lat| beyond 0.20 + margins): untouched.
    {
        const glm::vec3 p(-0.30f, -0.15f, 0.01f);
        TrailChainState st = mk(p);
        render::trail_chain_present_clamp(st, pr, in);
        REQUIRE(st.p[3].x == p.x);
        REQUIRE(st.p[3].z == p.z);
    }
    // (c) hanging clear in FRONT of the chest (past bp_front - margin):
    // untouched -- this is the hang a bent-forward pose actually wants.
    // (z 0.45: clear of BOTH bands' front faces -- bp_front is measured
    // against each band's own pushed origin, so band 0's face sits 50 mm
    // further out in world z than band 1's.)
    {
        const glm::vec3 p(0.0f, -0.15f, 0.45f);
        TrailChainState st = mk(p);
        render::trail_chain_present_clamp(st, pr, in);
        REQUIRE(st.p[3].z == p.z);
    }
    // (d) past the collar in t (t = y + 0.6 > 0.60 + 0.30): untouched.
    {
        const glm::vec3 p(0.0f, 0.45f, 0.01f);
        TrailChainState st = mk(p);
        render::trail_chain_present_clamp(st, pr, in);
        REQUIRE(st.p[3].z == p.z);
    }
}
