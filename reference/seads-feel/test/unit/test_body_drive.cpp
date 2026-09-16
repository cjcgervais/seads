// ★★★ R4a RUNG 2b -- THE BODY CHAIN'S DRIVE POLICY.
//
// WHY THIS FILE EXISTS. Rung 2 shipped with six green gate legs and Chad drove
// it and said "the legs were going all over erratically, broken." The suite and
// the drive disagreed, and the suite was the one that was wrong: every leg
// graded `body_blend_legs` against a STATIC chain handed in as a fixture, and
// the defect was in the three decisions that PRODUCED the chain -- decisions
// that lived in `render/sled_model.cpp`, a TU no test binary links.
//
// So the discipline for this file is sharper than "it must be able to fail".
// It is: THE FLAGSHIP LEG MUST FAIL ON THE CODE CHAD DROVE. Case 1 below does
// -- it measures 1.2 m of station thrash against the shipped policy, on the
// shipped body, with the shipped seat.

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>

#include "render/body_chain.h"
#include "render/body_drive.h"
#include "render/rider_load.h"
#include "render/rider_pose.h"
#include "render/trail_chain.h"

using namespace render;

namespace {

// The SHIPPED seat, from the same constants sled_model feeds the solver. Not a
// synthetic box: a keep-out defect is a defect about a particular solid, and a
// rounder, kinder seat is exactly the fixture that would have passed rung 2.
TrailChainInput shipped_seat() {
    TrailChainInput in;
    in.n_seat_samples = kSeatStations;
    in.seat_z_rear = kSeatZRearM;
    in.seat_z_front = kSeatZFrontM;
    in.seat_x_half = kSeatXHalfM;
    in.seat_y_bottom = kSeatYBottomM;
    const std::array<float, kSeatStations>& prof = seat_profile_y();
    for (int k = 0; k < kSeatStations; ++k)
        in.seat_top_y[k] = prof[static_cast<std::size_t>(k)];
    in.gravity_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    in.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
    return in;
}

// The drawn man at bind -- the pose the pin bakes when the machine is parked,
// and the pose the spring is aimed at.
struct Bind {
    glm::vec3 pin[kBodyChainStations];
    TrailChainParams pr;
    TrailChainInput in;

    Bind() {
        const std::array<BodyChainStation, kBodyChainStations>& t =
            body_chain_stations();
        for (int i = 0; i < kBodyChainStations; ++i)
            pin[i] = t[static_cast<std::size_t>(i)].rest_model;
        body_chain_fill(pr);
        pr.dt_s = 1.0f / 120.0f;
        pr.iterations = 4;
        pr.damping = 0.930f;
        pr.drag_k_per_m = 0.005f;
        pr.head_keepout_r_m = 0.0f;
        pr.back_keepout_m = 0.0f;
        pr.seat_keepout_m = 0.0f;
        in = shipped_seat();
        in.anchor = pin[0];
    }
};

// The shortest link in the measured body -- the natural scale for "a station
// moved a distance that is not physical". Nothing in a damped chain stepped at
// 1/120 s should displace a station further than its own shortest bone in one
// substep.
[[maybe_unused]] float shortest_link() {
    float m = body_chain_link_len(1);
    for (int i = 2; i <= kBodyChainSegments; ++i)
        m = std::min(m, body_chain_link_len(i));
    return m;
}

}  // namespace

// ---------------------------------------------------------------------------
// LEG 1 -- THE FLAGSHIP. The chain must not fight itself.
//
// ★ THIS IS THE LEG THAT FAILS ON WHAT HE DROVE. The seated pose is inside the
// seat keep-out BY MEASUREMENT AND BY DESIGN (render/body_chain.h: pelvis 52.5
// mm, thigh 38.6 mm), and rung 2 aimed the return-to-pose spring at exactly
// that pose. Spring pulls in, constraint pass throws out, Verlet reads the
// ejection as velocity, forever. Measured on the shipped policy: 0.49 m in one
// substep, peak 1.21 m.
//
// THE METRIC IS THE SETTLE, NOT THE SIZE, and getting that wrong once is worth
// recording. The first version of this leg bounded raw per-substep displacement
// and failed honestly at arm = 1.0 -- on 0.082 m/substep and a 0.32 m deviation
// that are NOT thrash: at full freedom the spring runs at its base 1 Hz, and a
// 1 Hz spring under gravity sags g/omega^2 = 0.248 m. That is the trail this
// rung exists to produce, and a gate that forbade it would have been a gate
// calibrated against the mechanism.
//
// What Chad's eye reads is motion that NEVER STOPS. So the leg lets the
// transient happen and grades the LAST HALF SECOND: a damped chain at a
// standstill, no wind and no jolt, must be quiet by then. On the policy he
// drove it is not -- the spring/keep-out fight is sustained, 0.77 m per substep
// for every one of 480 substeps.
TEST_CASE("body drive: a released chain does not fight its own keep-out") {
    for (const float arm : {0.01f, 0.5f, 1.0f}) {
        // TWO IDENTICAL RELEASES, stepped in lockstep, differing ONLY in
        // whether the seat solid exists.
        Bind b;             // the shipped seat
        Bind nb;            // no seat at all
        nb.in.n_seat_samples = 0;

        TrailChainState st, ns;
        for (TrailChainState* p : {&st, &ns}) {
            p->n = b.pr.segments;
            for (int i = 0; i <= p->n; ++i) {
                p->p[i] = b.pin[i];
                p->p_prev[i] = b.pin[i];
            }
            p->primed = true;  // primed AT the pose it arms in (body_chain.h)
        }

        BodyDriveIn di;
        di.pin = b.pin;
        di.n = b.pr.segments;
        di.arm = arm;
        di.pose_hz_base = 1.0f;  // the SHIPPED rung (body_pose_hz)
        di.chain_in = b.in;
        di.substeps = 1;
        di.dt_s = 1.0f / 120.0f;
        BodyDriveIn nd = di;
        nd.pin = nb.pin;
        nd.chain_in = nb.in;

        float settled_step = 0.0f;
        float worst_dev = 0.0f;
        float worst_excess = 0.0f;
        int worst_excess_station = -1;
        glm::vec3 prev[kBodyChainStations];
        for (int i = 0; i <= st.n; ++i) prev[i] = st.p[i];

        const int kFrames = 240;  // 2 s at 120 Hz
        for (int f = 0; f < kFrames; ++f) {
            const BodyDriveOut o = body_chain_drive(st, b.pr, di);
            body_chain_drive(ns, nb.pr, nd);
            REQUIRE_FALSE(o.pinned);
            for (int i = 0; i <= st.n; ++i) {
                const float d = glm::length(st.p[i] - prev[i]);
                if (f >= kFrames - 60) settled_step = std::max(settled_step, d);
                prev[i] = st.p[i];
                const float dev = glm::length(st.p[i] - b.pin[i]);
                const float dev_free = glm::length(ns.p[i] - b.pin[i]);
                worst_dev = std::max(worst_dev, dev);
                // ⚠ GRADED ONLY WHILE HE IS STILL AT HIS POSE. Once the
                // seatless arm has genuinely travelled (here: 100 mm, well past
                // the 52.5 mm the pose itself sits inside the pan), the seated
                // arm is being DEFLECTED AROUND A SOLID and that legitimately
                // increases its distance from the pose -- measured at 0.171 m
                // at the knee at full arm, which is the seat stopping a leg,
                // not the seat fighting one. The fight is a push applied where
                // there is nothing to push out of, so that is where it is
                // graded. Without this predicate the leg would forbid the
                // keep-out from working -- a gate calibrated against the
                // mechanism, which is how the last one shipped green.
                if (dev_free < 0.10f && dev - dev_free > worst_excess) {
                    worst_excess = dev - dev_free;
                    worst_excess_station = i;
                }
                REQUIRE(std::isfinite(st.p[i].x));
            }
        }
        INFO("arm = " << arm << "  settled = " << settled_step
                      << " m  deviation = " << worst_dev
                      << " m  excess over seatless = " << worst_excess
                      << " m at station " << worst_excess_station);

        // (a) IT STOPS. A damped chain at a standstill, no wind and no jolt,
        // must be quiet in the last half second. On the policy Chad drove it
        // never is -- 0.77 m per substep for every one of 480 substeps.
        REQUIRE(settled_step < 5.0e-3f);

        // (b) ★★★ THE KEEP-OUT MAY ONLY HOLD HIM CLOSER TO HIS POSE, NEVER
        // THROW HIM FURTHER FROM IT. This is the leg that names the defect
        // instead of fencing its symptom, and it is ONE-SIDED on purpose: the
        // seat legitimately stops him sinking into it (the seated arm sags
        // 50 mm LESS than the seatless one at arm = 0.5, which is the seat
        // doing its job -- a man cannot sink through his own seat). What it
        // may never do is ADD distance from the pose. On the shipped policy
        // this excess is about a metre, because the seat was ejecting a pose
        // it should have been supporting.
        REQUIRE(worst_excess < 0.01f);

        // (c) He stays recognisably in his own body. The bare chain pulls the
        // toe 1.703 m off the pose (body_pose_hz's measured ladder) and the
        // 1 Hz spring's static sag is g/omega^2 = 0.248 m; half a metre is the
        // honest fence between the two at a standstill with no jolt at all.
        REQUIRE(worst_dev < 0.5f);
    }
}

// ---------------------------------------------------------------------------
// LEG 2 -- the pin branch is EXACT, and it is exact by an early return rather
// than by arithmetic that lands on zero. A chain that starts the release with a
// velocity the pin invented flies on frame one.
TEST_CASE("body drive: at arm 0 the chain is the drawn man, frozen") {
    Bind b;
    TrailChainState st;
    st.primed = true;
    st.n = b.pr.segments;
    for (int i = 0; i <= st.n; ++i) {
        st.p[i] = glm::vec3(9.0f, 9.0f, 9.0f);  // junk, so the copy must happen
        st.p_prev[i] = glm::vec3(0.0f);
    }

    BodyDriveIn di;
    di.pin = b.pin;
    di.n = b.pr.segments;
    di.arm = 0.0f;
    di.pose_hz_base = 1.0f;
    di.chain_in = b.in;
    di.substeps = 8;
    di.dt_s = 1.0f / 120.0f;

    const BodyDriveOut o = body_chain_drive(st, b.pr, di);
    REQUIRE(o.pinned);
    for (int i = 0; i <= st.n; ++i) {
        REQUIRE(st.p[i] == b.pin[i]);       // bit-exact, not "close"
        REQUIRE(st.p_prev[i] == b.pin[i]);  // and at REST relative to him
    }
}

// ---------------------------------------------------------------------------
// LEG 3 -- THE ALLOWANCE IS REAL AND IT IS EXACTLY ENOUGH.
//
// Two claims, and the pair is the point. (a) The seated pose IS inside the
// solid, so the allowance is non-zero -- if it were zero the ruling would not
// be in the build and leg 1 could pass for the wrong reason. (b) With the
// allowance in hand the pose takes NO push at all: that is what stops the
// spring and the constraint pass fighting, and it is what the relocation fix
// could not deliver without moving his pelvis 115 mm.
TEST_CASE("body drive: the seated pose is allowed, and then pushed nowhere") {
    Bind b;
    body_chain_seat_allowance(b.pr, b.in, b.pin, b.pr.segments, b.in.width_axis);
    REQUIRE(b.pr.n_seat_allow == b.pr.segments + 1);

    float allow = 0.0f;
    for (int i = 0; i <= b.pr.segments; ++i)
        for (int f = 0; f < 6; ++f) allow = std::max(allow, b.pr.seat_allow[i][f]);
    INFO("deepest face allowance " << allow << " m");
    REQUIRE(allow > 0.005f);  // he really is sitting in it

    // And now step the chain from the pose. With the allowance the FIRST
    // constraint pass must find nothing to do at the leg and torso stations.
    TrailChainState st;
    st.n = b.pr.segments;
    for (int i = 0; i <= st.n; ++i) {
        st.p[i] = b.pin[i];
        st.p_prev[i] = b.pin[i];
    }
    st.primed = true;
    st.needs_solve = true;  // solve-and-freeze: the pure keep-out pass

    TrailChainInput ci = b.in;
    ci.anchor = b.pin[0];
    trail_chain_step(st, b.pr, ci);

    float moved = 0.0f;
    int worst = -1;
    for (int i = 0; i <= st.n; ++i) {
        const float d = glm::length(st.p[i] - b.pin[i]);
        if (d > moved) {
            moved = d;
            worst = i;
        }
    }
    INFO("the keep-out moved the seated pose " << moved << " m at station "
                                               << worst);
    // Sub-millimetre. Without the allowance this is 0.115 m at the pelvis.
    REQUIRE(moved < 1.0e-3f);
}

// ---------------------------------------------------------------------------
// LEG 4 -- CHAD'S RULING, graded directly. The leg stations are two limbs
// straddling the seat, and under the COMBINED rule each limb prices the other's
// lateral exit at the full width of the solid, leaving a VERTICAL face as the
// only cheap one and ejecting the midline bone a third of a metre.
//
// This leg fails if the ruling is reverted, and it fails in the direction that
// says so: it asserts the two rules DISAGREE, and that the per-limb answer is
// the small one.
TEST_CASE("body drive: leg stations are graded per limb, not as a midline") {
    Bind b;
    const int st = body_chain_station_of("thigh");
    REQUIRE(st >= 0);
    // ⚠ NOT asserted here: whether body_chain_fill SHIPS this station per-limb.
    // That is an open ruling (see render/body_chain.cpp), and welding an
    // unruled choice into a gate leg is how a question stops being asked. What
    // IS graded is the mechanism itself, on both settings.

    // A true STRADDLE: one limb either side of the midline, each only a little
    // inside the pan laterally, and both well inside vertically and fore-aft so
    // the lateral face is genuinely the cheap one PER LIMB.
    //
    // ⚠ The first version of this fixture put both boxes near the seat floor,
    // where the cheapest face for BOTH was straight down -- the two rules then
    // agree and the leg proves nothing. A fixture that makes the mechanism a
    // no-op is this program's most-repeated test defect; the numbers below are
    // chosen so the per-limb escapes are equal, opposite and LATERAL.
    const float y_mid =
        0.5f * (b.in.seat_y_bottom + b.in.seat_top_y[kSeatStations / 2]);
    const float z_mid = 0.5f * (b.in.seat_z_rear + b.in.seat_z_front);
    glm::vec3 q[2] = {glm::vec3(0.19f, y_mid, z_mid),
                      glm::vec3(-0.19f, y_mid, z_mid)};
    glm::vec3 r[2] = {glm::vec3(0.05f, 0.05f, 0.10f),
                      glm::vec3(0.05f, 0.05f, 0.10f)};
    const bool live[2] = {true, true};

    const glm::vec3 combined =
        trail_seat_station_push(b.pr, b.in, q, r, live, false);
    const glm::vec3 per_limb =
        trail_seat_station_push(b.pr, b.in, q, r, live, true);

    INFO("combined = " << glm::length(combined) << " m " << combined.x << ","
                       << combined.y << "," << combined.z
                       << "   per-limb = " << glm::length(per_limb) << " m");
    // The combined rule ejects the bone vertically; the per-limb rule does not,
    // because the two limbs' lateral escapes are equal and opposite and their
    // common mode is nothing. The bone between his legs was never in the seat.
    REQUIRE(glm::length(combined) > 0.10f);
    REQUIRE(glm::length(per_limb) < 0.01f);
    // And it is the VERTICAL face the combined rule chooses -- naming the
    // mechanism, not just its size.
    REQUIRE(std::fabs(combined.y) > std::fabs(combined.x));
}

// ---------------------------------------------------------------------------
// LEG 5 -- the signal, not the branch. An exponential decaying toward zero
// never arrives, and the pin/release test asks `arm <= 0`. That asymptote is
// why one throttle blip held the release open for a whole drive.
TEST_CASE("rider load: the low-pass settles to exactly zero") {
    const double h = 1.0 / 120.0;
    double v = 0.05;
    for (int i = 0; i < 240; ++i) v = rider_load_lp_step(v, 0.0, h);
    REQUIRE(v == 0.0);

    // ⚠ AND IT MUST NOT BE A LATCH. The floor fires only on an exactly-zero
    // target; a small NONZERO target is still tracked, because a threshold on
    // a live signal is the disease this program has paid for four times.
    double u = 0.0;
    for (int i = 0; i < 600; ++i) u = rider_load_lp_step(u, 1.0e-3, h);
    REQUIRE(u > 9.0e-4);
}

// ==========================================================================
// ★★★ THE ARMING MEMORY (Chad's ruling 2026-08-29: "the buck has to be
// remembered, harder the buck the longer the superman").
//
// These legs exist because rung 2b was lost exactly here: the stage machine
// lived in a TU no test could link, six green cases coexisted with a drive
// Chad called "erratic", and nothing in the gate could execute one line of the
// policy. The memory is therefore in the library, and every claim its banner
// makes is graded below.
// ==========================================================================

TEST_CASE("R4a buck memory: gain 0 is EXACTLY the pre-rung build") {
    render::BodyBuckMemory m;
    render::BodyBuckParams p;  // buck_gain defaults 0
    // Hit it with everything -- a 30 g landing, sustained, then free fall.
    for (int i = 0; i < 200; ++i) {
        const float div = render::body_buck_step(m, p, i < 100 ? 300.0f : 0.0f,
                                                 1.0f / 120.0f);
        // The divisor must be the literal 1.0f, not merely close: the shipped
        // stiffness is `base / arm / divisor`, and only an exact 1 leaves that
        // expression bit-identical to the one that shipped.
        REQUIRE(div == 1.0f);
        REQUIRE(m.mem == 0.0f);
    }
}

TEST_CASE("R4a buck memory: a HARDER buck keeps him out LONGER, in proportion") {
    render::BodyBuckParams p;
    p.buck_gain = 0.01f;
    p.decay_per_s = 1.0f;
    const float dt = 1.0f / 120.0f;

    // How long the memory stays above a fixed softening, for two bucks a
    // factor of 4 apart. Linear discharge => the DURATION scales with the
    // buck, which is the whole content of "harder the buck the longer the
    // superman". An exponential decay would give a log relation and fail this.
    auto dwell = [&](float buck) {
        render::BodyBuckMemory m;
        render::body_buck_step(m, p, buck, dt);  // the hit
        int n = 0;
        while (render::body_buck_step(m, p, 0.0f, dt) > 1.05f) {
            if (++n > 1000000) break;  // never hang the gate
        }
        return n;
    };
    const int small = dwell(9.81f + 100.0f);
    const int big = dwell(9.81f + 400.0f);
    REQUIRE(small > 0);
    // 4x the buck must buy ~4x the time out. Generous band: this pins the
    // PROPORTIONALITY, not a constant.
    const double ratio = static_cast<double>(big) / static_cast<double>(small);
    CHECK(ratio > 3.5);
    CHECK(ratio < 4.5);
}

TEST_CASE("R4a buck memory: ordinary riding never charges it") {
    render::BodyBuckMemory m;
    render::BodyBuckParams p;
    p.buck_gain = 0.01f;
    p.decay_per_s = 1.0f;
    // Steady 1 g. The charge is referenced to the REST STATE, so a man simply
    // sitting on his machine must never be softened -- if this fails, the
    // mechanism is firing during normal riding and "a little bit rare" is
    // already lost before any calibration.
    for (int i = 0; i < 1000; ++i) {
        const float div = render::body_buck_step(m, p, 9.81f, 1.0f / 120.0f);
        REQUIRE(div == 1.0f);
    }
}

TEST_CASE("R4a buck memory: it RECOVERS -- the softening is not a latch") {
    render::BodyBuckMemory m;
    render::BodyBuckParams p;
    p.buck_gain = 0.01f;
    p.decay_per_s = 1.0f;
    const float dt = 1.0f / 120.0f;
    REQUIRE(render::body_buck_step(m, p, 500.0f, dt) > 1.5f);
    // ★ Stage 0-3 are "continuous and REVERSIBLE" (LADDER 7.3). A memory that
    // could not fully discharge would be the one-way latch the ladder bans,
    // and he would ride the rest of the drive soft.
    for (int i = 0; i < 100000; ++i) render::body_buck_step(m, p, 0.0f, dt);
    REQUIRE(m.mem == 0.0f);
    REQUIRE(render::body_buck_step(m, p, 0.0f, dt) == 1.0f);
}

TEST_CASE("R4a buck memory: no threshold -- the response is continuous in the buck") {
    render::BodyBuckParams p;
    p.buck_gain = 0.01f;
    p.decay_per_s = 1.0f;
    // ★★★ This ladder's recorded disease, three rungs running, is a BINARY
    // READ OF A CONTINUOUS QUANTITY. Sweep the buck finely and require the
    // divisor to be monotone with NO step: the largest jump between adjacent
    // samples must stay proportional to the sample spacing, so a cliff
    // anywhere fails.
    float prev = 1.0f, worst = 0.0f;
    for (int i = 0; i <= 400; ++i) {
        render::BodyBuckMemory m;
        const float div =
            render::body_buck_step(m, p, 9.81f + static_cast<float>(i), 1.0f);
        REQUIRE(div >= prev);  // monotone
        worst = std::max(worst, div - prev);
        prev = div;
    }
    // One unit of buck buys buck_gain of divisor; anything much larger than
    // that between neighbours IS a threshold.
    CHECK(worst < 2.0f * p.buck_gain);
}
