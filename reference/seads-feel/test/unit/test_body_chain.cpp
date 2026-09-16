// ★ R4a SUPERMAN -- THE BODY CHAIN GATE.
//
// docs/SUDBURIAN_LADDER.md §7.6 ("superman IS the scarf") and the plumbing
// handoff's §12, which set this rung two jobs and one decision:
//
//   * a segment table anchored at the grips and BOUNDED BY THE 0.218 m PAN,
//     because the seat keep-out is a VERTEX keep-out;
//   * ★★★ and the decision it said to make up front -- WHICH SURFACE THE LEGS
//     ARE GRADED ON. The chain is a centreline and the legs are drawn
//     geometry; grading the centreline is `back_clr` / `surf_clr` a third
//     time, a number that cannot fail. The answer built here is that the legs
//     are graded on their OWN DRAWN SURFACE, via the per-station probes
//     measured off the shipped GLB (render/body_chain.h).
//
// Nothing here needs an asset at run time: render/body_chain.* IS the measured
// asset, baked, and the seat comes out of render/rider_pose.*'s own table.
//
// ASCII names only: a non-ASCII Catch2 name is silently never run in this repo.

#include <algorithm>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>

#include "render/body_chain.h"
#include "render/rider_pose.h"
#include "render/trail_chain.h"

using render::BodyChainStation;
using render::TrailChainInput;
using render::TrailChainParams;
using render::TrailChainState;

namespace {

constexpr float kDt = 1.0f / 120.0f;

// The seat solid, MEASURED -- copied out of render/rider_pose.*'s own emitted
// table (measure_seat_profile.py's downward raycast over the shipped GLB), the
// same way test_trail_chain.cpp does it. Not one number is retyped.
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

// Where the DRAWN limb actually is: the station's probe offset, carried in the
// chain's own frame. Uses the PUBLIC trail_chain_frames rather than the
// solver's private probe_axes -- the frame the draw pass will use is the frame
// the keep-out has to have meant.
const render::BodyChainProbe& probe_of(int i, int side) {
    return render::body_chain_stations()[static_cast<std::size_t>(i)]
        .probe[static_cast<std::size_t>(side)];
}

glm::vec3 probe_world(TrailChainState& st, const glm::vec3& width_axis, int i,
                      int side) {
    glm::mat4 fr[render::kTrailChainMaxSegments];
    render::trail_chain_frames(st, width_axis, fr);
    const int f = i < st.n ? i : st.n - 1;  // the tail inherits the last frame
    const render::BodyChainProbe& pb = probe_of(i, side);
    return st.p[i] + glm::vec3(fr[f][0]) * pb.u_m +
           glm::vec3(fr[f][1]) * pb.v_m + glm::vec3(fr[f][2]) * pb.w_m;
}

// The probe box's half-width along one WORLD axis -- its support function,
// written here from the frames the DRAW pass uses, not from the solver's
// private copy.
float probe_support(TrailChainState& st, const glm::vec3& width_axis, int i,
                    int side, int axis) {
    glm::mat4 fr[render::kTrailChainMaxSegments];
    render::trail_chain_frames(st, width_axis, fr);
    const int f = i < st.n ? i : st.n - 1;
    const render::BodyChainProbe& pb = probe_of(i, side);
    return pb.hx_m * std::fabs(fr[f][0][axis]) +
           pb.hy_m * std::fabs(fr[f][1][axis]) +
           pb.hz_m * std::fabs(fr[f][2][axis]);
}

// Does this station draw anything on this side at all?
bool probe_live(int i, int side) {
    const render::BodyChainProbe& pb = probe_of(i, side);
    return pb.hx_m > 0.0f || pb.hy_m > 0.0f || pb.hz_m > 0.0f;
}

// ★ HOW FAR THE DRAWN LIMB HAS SUNK BELOW THE SEAT'S TOP SURFACE, in metres.
// Positive = the drawn man is inside the machine, which is the felt defect in
// the units it is felt in ("his leg sank into the seat").
//
// Deliberately NOT the solver's own shallowest-face arithmetic -- a keep-out
// checked against its own formula can only ever agree with itself. This is a
// plain vertical question asked of rider_pose's measured profile: is the lowest
// point of the drawn limb below the seat top, at a place where the seat is?
float sink_below_seat(TrailChainState& st, const glm::vec3& width_axis, int i,
                      int side) {
    if (!probe_live(i, side)) return 0.0f;
    const glm::vec3 q = probe_world(st, width_axis, i, side);
    const float rx = probe_support(st, width_axis, i, side, 0);
    const float ry = probe_support(st, width_axis, i, side, 1);
    const float rz = probe_support(st, width_axis, i, side, 2);
    // ★★★ THE PENETRATION DEPTH, WHICH IS THE **SMALLEST** OF THE THREE AXIS
    // OVERLAPS -- and getting that wrong is the trap this program has now paid
    // for four times, once here, in my own instrument, in this rung.
    //
    // Draft 1 asked "is the box's bottom below the seat top?" A body hanging
    // BELOW the machine is tall enough to say yes while being nowhere near the
    // seat: a false 122 mm.
    //
    // Draft 2 asked for the three-axis overlap and then REPORTED THE VERTICAL
    // ONE. A boot whose box grazes the seat's rear edge by a micron overlaps in
    // z by 1e-6 and in y by 113 mm, and the metric shouted 113 mm of leg inside
    // a machine the leg was behind. That was the "0.113 m" this rung spent an
    // hour attributing to the solver, and the solver was right.
    //
    // The exit distance is the SHALLOWEST face -- which is what the solver's
    // own least-penetration projection uses, and what the eye sees.
    const float ox = std::fmin(render::kSeatXHalfM, q.x + rx) -
                     std::fmax(-render::kSeatXHalfM, q.x - rx);
    const float oz = std::fmin(render::kSeatZFrontM, q.z + rz) -
                     std::fmax(render::kSeatZRearM, q.z - rz);
    const float oy = std::fmin(render::seat_top_y(q.z), q.y + ry) -
                     std::fmax(render::kSeatYBottomM, q.y - ry);
    const float depth = std::fmin(ox, std::fmin(oy, oz));
    return depth > 0.0f ? depth : 0.0f;
}

// The clearance of the drawn limb ABOVE the seat top, or a big number where
// the seat is not underneath it. The counterpart of the above: a keep-out can
// also pass by holding the body nowhere near the machine.
float clearance_above_seat(TrailChainState& st, const glm::vec3& width_axis,
                           int i, int side) {
    if (!probe_live(i, side)) return 1.0e3f;
    const glm::vec3 q = probe_world(st, width_axis, i, side);
    const float rx = probe_support(st, width_axis, i, side, 0);
    const float ry = probe_support(st, width_axis, i, side, 1);
    const float rz = probe_support(st, width_axis, i, side, 2);
    const float ox = std::fmin(render::kSeatXHalfM, q.x + rx) -
                     std::fmax(-render::kSeatXHalfM, q.x - rx);
    const float oz = std::fmin(render::kSeatZFrontM, q.z + rz) -
                     std::fmax(render::kSeatZRearM, q.z - rz);
    // Only limbs actually OVER the seat, and only from above: a limb beside or
    // below the machine has no "clearance above the seat" to report.
    if (!(ox > 0.0f) || !(oz > 0.0f) || !(q.y > render::seat_top_y(q.z)))
        return 1.0e3f;
    return (q.y - ry) - render::seat_top_y(q.z);
}

// ★ THE SCENARIO, and it is the only state this chain is ever live in.
//
// SUPERMAN (LADDER §7.3 stage 3): a hard deceleration throws the body BACKWARD
// off the grips, and then a SUSTAINED brake holds it out there while gravity
// brings it down across the machine. Both halves matter. Throw and then release
// the field and the body simply hangs off the front of the machine, touching
// nothing -- which is a real state, and a useless test.
//
// ⚠ NOT primed at bind: the bind pose is SEATED and its drawn pelvis is
// authored 52 mm inside the seat (render/body_chain.h), so a bind prime would
// exit through the nearest face. This is the priming rule, obeyed.
constexpr int kThrowSteps = 240;
constexpr int kHoldSteps = 900;
constexpr float kThrowMps2 = 80.0f;  // the jolt
constexpr float kHoldMps2 = 6.0f;    // the brake that keeps him out there

TrailChainParams body_params() {
    TrailChainParams pr;
    render::body_chain_fill(pr);
    pr.dt_s = kDt;
    // Feel dials, not asset: the measurement rig's body drag (rho*Cd*A/2m for
    // 87.5 kg), and both scarf keep-outs off -- the chain IS the body.
    pr.drag_k_per_m = 0.005f;
    pr.head_keepout_r_m = 0.0f;
    pr.back_keepout_m = 0.0f;
    return pr;
}

TrailChainInput body_input() {
    TrailChainInput in;  // back_normal defaults to zero = the plane is off
    in.anchor = glm::vec3(0.0f, 1.0f, 0.30f);  // about where the grips are
    in.gravity_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    in.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
    return in;
}

}  // namespace

// -- 1 ------------------------------------------------------------------------
TEST_CASE("the measured body chain fits inside the vertex keep out bound",
          "[body_chain]") {
    // ★ THE BOUND THE LAST RUNG WROTE DOWN RATHER THAN LEAVING TO A DRIVE:
    // the seat keep-out is projected PER PARTICLE, so a link LONGER than the
    // pan is thick lies straight through the seat with a vertex either side
    // and nothing inside to find. This is the case that stops the body table
    // ever being authored past it.
    const float pan_thick = render::seat_top_y(0.0f) - render::kSeatYBottomM;
    REQUIRE(pan_thick == Catch::Approx(0.218).margin(0.001));

    float longest = 0.0f;
    for (int i = 1; i <= render::kBodyChainSegments; ++i) {
        const float l = render::body_chain_link_len(i);
        REQUIRE(l > 0.0f);  // no collapsed station
        REQUIRE(l < pan_thick);
        longest = std::fmax(longest, l);
    }
    // The measured worst case, pinned: the thigh, at 0.209 m against 0.218.
    // It is TIGHT, and it is meant to be -- the anatomy sets the joints and
    // the pan sets the ceiling, so a re-measure that moves either is a thing
    // whoever re-measures must see.
    REQUIRE(longest == Catch::Approx(0.209254f).margin(1e-4f));

    // ★ AND THE CAP MUST NOT BE SATURATED. clamp_segments() clamps SILENTLY,
    // so a table that exactly fills the solver's arrays is a landmine: the
    // next station added would vanish with no diagnostic. This is why R4a
    // raised the cap from 16 to 20.
    REQUIRE(render::kBodyChainSegments < render::kTrailChainMaxSegments);
}

// -- 2 ------------------------------------------------------------------------
TEST_CASE("the measured body chain is a body, grips to toe", "[body_chain]") {
    // Provenance, pinned: 17 stations / 16 links / 2.2009 m of man from the
    // grip socket to the ball of the boot, off assets/sled/indy650.glb. If the
    // rider is re-exported and these move, the table was re-measured -- which
    // is fine -- but nobody gets to move them by typing.
    REQUIRE(render::kBodyChainStations == 17);
    REQUIRE(render::body_chain_total_len() ==
            Catch::Approx(2.200920f).margin(1e-4f));

    // Every station draws something on BOTH sides, and the two sides are the
    // same limb: a table where one side lost its probes would keep-out half a
    // man and pass every other case in this file.
    for (int i = 0; i < render::kBodyChainStations; ++i) {
        const BodyChainStation& s =
            render::body_chain_stations()[static_cast<std::size_t>(i)];
        for (int side = 0; side < 2; ++side) {
            REQUIRE(s.probe[static_cast<std::size_t>(side)].hx_m > 0.0f);
            REQUIRE(s.probe[static_cast<std::size_t>(side)].hy_m > 0.0f);
            REQUIRE(s.probe[static_cast<std::size_t>(side)].hz_m > 0.0f);
        }
        REQUIRE(s.probe[0].u_m > 0.0f);  // +X side
        REQUIRE(s.probe[1].u_m < 0.0f);  // -X side
        // Measured, not imposed: the mesh is near enough symmetric that the
        // two boxes agree to a few centimetres. A limb that drifted far from
        // its mirror would be an authoring defect, not a keep-out dial.
        REQUIRE(std::fabs(s.probe[0].hx_m - s.probe[1].hx_m) < 0.03f);
        REQUIRE(std::fabs(s.probe[0].hz_m - s.probe[1].hz_m) < 0.03f);
    }

    // ★★★ THE MEASUREMENT THE WHOLE DECISION RESTS ON: the drawn legs are not
    // merely thicker than the centreline, they are NOT ON IT. The knee probes
    // sit at |x| ~ 0.26 while the seat is 0.206 m half-wide -- so a midline
    // particle is INSIDE the seat box exactly where the drawn rider is clear.
    const BodyChainStation& knee = render::body_chain_stations()[12];
    REQUIRE(std::fabs(knee.probe[0].u_m) > render::kSeatXHalfM);
    REQUIRE(std::fabs(knee.probe[1].u_m) > render::kSeatXHalfM);
}

// -- 3 ------------------------------------------------------------------------
TEST_CASE("the solver holds the non uniform segment table every step",
          "[body_chain]") {
    // The scarf's chain is uniform; a body's is not. This is the case that
    // says the table is USED and not quietly replaced by the scalar.
    const TrailChainParams pr = body_params();
    TrailChainInput in = body_input();
    TrailChainState st;
    render::trail_chain_reset(st, pr, in.anchor, glm::vec3(0.0f, -0.3f, -1.0f));
    REQUIRE(st.n == render::kBodyChainSegments);
    for (int k = 0; k < 400; ++k) {
        in.wind_mps = glm::vec3(0.0f, 0.0f, -12.0f - 0.02f * k);
        render::trail_chain_step(st, pr, in);
        for (int i = 1; i <= st.n; ++i)
            REQUIRE(
                glm::length(st.p[i] - st.p[i - 1]) ==
                Catch::Approx(render::body_chain_link_len(i)).margin(1.0e-4f));
    }
    // ...and the table really is non-uniform, or the case above proves nothing.
    REQUIRE(render::body_chain_link_len(1) !=
            Catch::Approx(render::body_chain_link_len(11)).margin(0.01f));
}

// -- 4 ------------------------------------------------------------------------
TEST_CASE("a uniform segment table is bit identical to the scalar",
          "[body_chain]") {
    // ★ A BIT-IDENTICAL ARM IS ONLY EVIDENCE WITH A LIVE ARM BESIDE IT (the
    // superman rung's own law). Arm A: n_seg_len = 0. Arm B: the same length
    // spelled out in the table. Arm C: a genuinely different table. A and B
    // must agree to the bit; C must not, or A == B was measuring nothing.
    TrailChainParams a;
    a.dt_s = kDt;
    a.segments = 6;
    a.seg_len_m = 0.08f;
    TrailChainParams b = a;
    b.n_seg_len = 6;
    for (int i = 0; i < 6; ++i) b.seg_len_tbl[i] = 0.08f;
    TrailChainParams c = b;
    c.seg_len_tbl[3] = 0.16f;

    TrailChainInput in;
    in.gravity_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    in.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
    TrailChainState sa, sb, sc;
    render::trail_chain_reset(sa, a, in.anchor, in.gravity_dir);
    render::trail_chain_reset(sb, b, in.anchor, in.gravity_dir);
    render::trail_chain_reset(sc, c, in.anchor, in.gravity_dir);
    for (int k = 0; k < 300; ++k) {
        in.wind_mps = glm::vec3(0.1f * k, 0.0f, -0.05f * k);
        render::trail_chain_step(sa, a, in);
        render::trail_chain_step(sb, b, in);
        render::trail_chain_step(sc, c, in);
    }
    REQUIRE(std::memcmp(sa.p, sb.p, sizeof(sa.p)) == 0);
    REQUIRE(std::memcmp(sa.p, sc.p, sizeof(sa.p)) != 0);
}

// -- 5 ------------------------------------------------------------------------
TEST_CASE("the DRAWN body stays out of the seat, and the centreline does not",
          "[body_chain]") {
    // ★★★ THE RUNG'S DECISION, AND THE ONLY CASE THAT CAN FALSIFY IT.
    //
    // Chad, 2026-08-25: "he can get thrown off in any direction if he
    // supermans, HIS LEGS WILL HIT THE SEAT." What must not enter the machine
    // is the DRAWN man. Three instruments have already been paid for on this
    // program by grading a centreline and reporting a number that could not
    // fail (`back_clr`, then `surf_clr`, then the wrap's own back plane).
    //
    // ARM A: the shipped configuration -- probes on, the drawn limbs graded.
    // ARM B, the LIVENESS ARM: the same body, the same throw, with
    // n_probe_stations = 0 -- the centreline graded, which is what the solver
    // did before this rung. B must sink DRAWN geometry into the machine, or A's
    // silence proves nothing (the superman rung's own law: a clean arm is only
    // evidence with a live arm beside it).
    TrailChainParams pr = body_params();
    TrailChainParams ctl = pr;
    ctl.n_probe_stations = 0;  // grade the centreline -- the pre-rung solver

    TrailChainInput in = body_input();
    give_seat(in);

    TrailChainState st, sc;
    render::trail_chain_reset(st, pr, in.anchor, glm::vec3(0.0f, 0.0f, -1.0f));
    render::trail_chain_reset(sc, ctl, in.anchor, glm::vec3(0.0f, 0.0f, -1.0f));

    float a_sink = 0.0f, b_sink = 0.0f;
    for (int k = 0; k < kThrowSteps + kHoldSteps; ++k) {
        in.frame_accel_mps2 =
            glm::vec3(0.0f, 0.0f, k < kThrowSteps ? kThrowMps2 : kHoldMps2);
        render::trail_chain_step(st, pr, in);
        render::trail_chain_step(sc, ctl, in);
        // EVERY step, not just the settled one: a keep-out that holds only at
        // rest is a keep-out the eye watches fail.
        for (int i = 1; i < render::kBodyChainStations; ++i) {
            for (int s = 0; s < 2; ++s) {
                a_sink =
                    std::fmax(a_sink, sink_below_seat(st, in.width_axis, i, s));
                b_sink =
                    std::fmax(b_sink, sink_below_seat(sc, in.width_axis, i, s));
            }
        }
    }
    INFO("drawn sink into the seat: probes " << a_sink << " m, centreline "
                                             << b_sink << " m");
    // A: nothing drawn gets meaningfully into the machine. The bound is 10 mm
    // against a MEASURED worst of 4.4 mm, and that 4.4 mm is a real bounded
    // property, not slop: a Gauss-Seidel projection with a FIXED iteration
    // count (LADDER §7.6 requires fixed) leaves a small residual while the
    // chain is whipping, because the distance pass and the keep-out are solved
    // in sequence and the whip moves a particle's neighbours after its own
    // frame was taken. It does NOT shrink with iterations -- measured 4.4 /
    // 5.3 / 6.2 mm at 4 / 8 / 20 -- so 4 stays, and this is a measured bound
    // rather than a dial turned until the case went green.
    REQUIRE(a_sink < 0.010f);
    // B: the centreline arm drives the drawn man 218 mm into the machine --
    // the FULL THICKNESS of the seat pan, which is the deepest a body can be
    // inside it. That is what grading the wrong surface costs, in the units it
    // is felt in.
    REQUIRE(b_sink > 0.20f);
}

// -- 6 ------------------------------------------------------------------------
TEST_CASE("the drawn surface RESTS on the seat and the bone rides above it",
          "[body_chain]") {
    // The other half of case 5, and the half that says the keep-out is doing
    // its job rather than passing by holding the body somewhere else entirely.
    //
    // Two statements, both measured:
    //   1. CONTACT -- some drawn limb settles ON the seat, clearance ~ 0. A
    //      keep-out that parks the man in the air passes case 5 vacuously.
    //   2. AND THE BONE IS NOWHERE NEAR IT. The chain particle under that limb
    //      rides its own drawn radius above the surface. That difference IS the
    //      decision: the thing being graded is the drawn surface, not the line
    //      the solver integrates.
    TrailChainParams pr = body_params();
    TrailChainInput in = body_input();
    give_seat(in);

    TrailChainState st;
    render::trail_chain_reset(st, pr, in.anchor, glm::vec3(0.0f, 0.0f, -1.0f));
    for (int k = 0; k < kThrowSteps + kHoldSteps; ++k) {
        in.frame_accel_mps2 =
            glm::vec3(0.0f, 0.0f, k < kThrowSteps ? kThrowMps2 : kHoldMps2);
        render::trail_chain_step(st, pr, in);
    }

    float best_clear = 1.0e3f;
    int contact_i = -1, contact_s = -1;
    for (int i = 1; i < render::kBodyChainStations; ++i) {
        for (int s = 0; s < 2; ++s) {
            const float c = clearance_above_seat(st, in.width_axis, i, s);
            if (c < best_clear) {
                best_clear = c;
                contact_i = i;
                contact_s = s;
            }
        }
    }
    INFO("closest drawn limb: station " << contact_i << " side " << contact_s
                                        << " clearance " << best_clear);
    REQUIRE(contact_i >= 0);
    // Resting ON it: touching to within the projection's own sub-step slack
    // (observed -6.6e-07 m), and not hovering above it.
    REQUIRE(best_clear > -0.001f);
    REQUIRE(best_clear < 0.02f);

    // ...and the bone under that limb is its own drawn half-thickness clear of
    // the surface.
    const glm::vec3 bone = st.p[contact_i];
    const float bone_clear = bone.y - render::seat_top_y(bone.z);
    const float ry = probe_support(st, in.width_axis, contact_i, contact_s, 1);
    INFO("bone clearance " << bone_clear << " m vs drawn half-height " << ry);
    REQUIRE(bone_clear > 0.5f * ry);
}

// -- 7 ------------------------------------------------------------------------
TEST_CASE("the bind pose is illegal against this keep out, on purpose",
          "[body_chain]") {
    // ⚠ RECORDED, NOT REPAIRED. He is SITTING in the bind pose: his backside is
    // authored INSIDE the seat solid, because that is what sitting looks like.
    // The probe radii are therefore NOT capped to make bind legal -- capping
    // them would under-protect superman, the only state this chain is ever live
    // in (LADDER §7.3 stage 3: seat load going to zero is what arms it).
    //
    // The cost is a PRIMING RULE, and this case is where it is written down:
    // PRIME THE BODY CHAIN WHEN IT ARMS, IN THE POSE IT ARMS IN. Prime it at
    // bind and the least-penetration exit takes his pelvis out through the
    // nearest face -- the same class as the straight-line prime the back plane
    // already documents in trail_chain.cpp.
    //
    // If someone later shrinks the radii until this case flips, they will have
    // changed the answer to "which surface are the legs graded on", and this is
    // the case that says so out loud.
    TrailChainInput in = body_input();
    give_seat(in);

    // The chain laid on its measured BIND stations exactly -- no solve.
    TrailChainState st;
    st.n = render::kBodyChainSegments;
    st.primed = true;
    for (int i = 0; i < render::kBodyChainStations; ++i) {
        st.p[i] = render::body_chain_stations()[static_cast<std::size_t>(i)]
                      .rest_model;
        st.p_prev[i] = st.p[i];
    }
    float deepest = 0.0f;
    for (int i = 1; i < render::kBodyChainStations; ++i)
        for (int s = 0; s < 2; ++s)
            deepest =
                std::fmax(deepest, sink_below_seat(st, in.width_axis, i, s));
    INFO("deepest drawn penetration in the seated bind pose: " << deepest);
    REQUIRE(deepest > 0.01f);
}

// -- 8 ------------------------------------------------------------------------
// ★★★ THE RETURN-TO-POSE SPRING, AND THE LADDER CHAD DIALS IT ON.
//
// He drove the bare chain 2026-08-27: "he is flailing all around ... like a
// firehose unattended." That is SUDBURIAN_LADDER §7.6's own stated reason for
// banning a ragdoll -- "floppy, weightless motion" -- reached by a trailing
// chain with no angular stiffness. Asked what the body should do instead he
// ruled HOLD HIS SHAPE, TRAIL FROM IT, and named the dial himself: HOW FAR A
// JOLT CAN PULL HIM OUT OF THE POSE.
//
// So this case measures exactly that number, on a jolt, at a ladder of
// stiffnesses -- and it is a MEASUREMENT the handoff quotes, not a threshold
// anybody tuned until it went green. What it ASSERTS is the two things that
// must be true of any honest spring:
//   * 0 Hz is BIT-IDENTICAL to the pre-spring solver (the scarf passes no
//     pose, and the arm Chad rejected is kept as the A/B rather than deleted);
//   * pull-out is MONOTONE in stiffness -- stiffer always means less. A
//     non-monotone dial is not a dial, it is a lottery.
TEST_CASE("the pose spring holds his shape and its pull out is monotone",
          "[body_chain]") {
    // The target pose IS the measured bind stations -- for the game it is the
    // live R3 pose, but the property under test does not care which pose.
    TrailChainInput in = body_input();
    in.n_pose = render::kBodyChainStations;
    for (int i = 0; i < render::kBodyChainStations; ++i)
        in.pose_p[i] = render::body_chain_stations()[
                           static_cast<std::size_t>(i)].rest_model;
    in.anchor = in.pose_p[0];

    // THE JOLT: a hard fore-aft frame acceleration held for a tenth of a
    // second, which is the shape of hitting something.
    const glm::vec3 jolt(0.0f, 0.0f, 40.0f);

    const float hz[] = {0.0f, 1.0f, 2.0f, 4.0f, 8.0f};
    float pull[5] = {0, 0, 0, 0, 0};
    glm::vec3 tip_off[5];
    for (int k = 0; k < 5; ++k) {
        TrailChainParams pr = body_params();
        pr.pose_stiff_hz = hz[k];
        TrailChainState st;
        trail_chain_reset(st, pr, in.anchor,
                          glm::vec3(0.0f, -1.0f, 0.0f));
        // settle onto the pose first, so the ladder measures the JOLT and not
        // the prime (the non-monotonic-sweep trap this program has paid for).
        TrailChainInput calm = in;
        for (int s = 0; s < 240; ++s) trail_chain_step(st, pr, calm);
        TrailChainInput hit = in;
        hit.frame_accel_mps2 = jolt;
        float worst = 0.0f;
        glm::vec3 worst_v(0.0f);
        for (int s = 0; s < 360; ++s) {
            trail_chain_step(st, pr, s < 12 ? hit : calm);
            const glm::vec3 d = st.p[st.n] - in.pose_p[st.n];
            if (glm::length(d) > worst) {
                worst = glm::length(d);
                worst_v = d;
            }
        }
        pull[k] = worst;
        tip_off[k] = worst_v;
    }
    INFO("toe pull-out [m] at 0/1/2/4/8 Hz: "
         << pull[0] << " / " << pull[1] << " / " << pull[2] << " / "
         << pull[3] << " / " << pull[4]);
    // MONOTONE, and strictly so between the ends -- a spring that does not
    // bite is not evidence of anything.
    for (int k = 1; k < 5; ++k) CHECK(pull[k] <= pull[k - 1] + 1.0e-4f);
    CHECK(pull[4] < 0.5f * pull[0]);
    // NON-VACUITY: the jolt must actually move the free chain, or the whole
    // ladder is a row of zeros telling us nothing (the disease this program
    // has now paid for five times).
    CHECK(pull[0] > 0.05f);
    (void)tip_off;

    // 0 Hz is BIT-IDENTICAL to a chain that was never told about a pose.
    TrailChainParams pr0 = body_params();
    TrailChainState a, c;
    trail_chain_reset(a, pr0, in.anchor, glm::vec3(0.0f, -1.0f, 0.0f));
    trail_chain_reset(c, pr0, in.anchor, glm::vec3(0.0f, -1.0f, 0.0f));
    TrailChainInput no_pose = in;
    no_pose.n_pose = 0;
    TrailChainInput hit_a = in, hit_c = no_pose;
    hit_a.frame_accel_mps2 = hit_c.frame_accel_mps2 = jolt;
    // ... and a LIVE third arm beside it, because a bit-identical arm is only
    // evidence with a live one next to it (the superman rung's own law).
    TrailChainParams pr_live = body_params();
    pr_live.pose_stiff_hz = 4.0f;
    TrailChainState live;
    trail_chain_reset(live, pr_live, in.anchor, glm::vec3(0.0f, -1.0f, 0.0f));
    float fid = 0.0f, dlive = 0.0f;
    for (int s = 0; s < 240; ++s) {
        trail_chain_step(a, pr0, s < 12 ? hit_a : in);
        trail_chain_step(c, pr0, s < 12 ? hit_c : no_pose);
        trail_chain_step(live, pr_live, s < 12 ? hit_a : in);
        for (int i = 0; i <= a.n; ++i) {
            fid = std::fmax(fid, glm::length(a.p[i] - c.p[i]));
            dlive = std::fmax(dlive, glm::length(a.p[i] - live.p[i]));
        }
    }
    INFO("0 Hz fidelity " << fid << " m, live 4 Hz arm differs by " << dlive
                          << " m");
    CHECK(fid == 0.0f);
    CHECK(dlive > 0.01f);
}

// -- 9 ------------------------------------------------------------------------
// ★★★ A SMALL STAGE WEIGHT MUST NOT CUT HIM LOOSE -- CHAD'S SECOND DRIVE.
//
// "it was flailing along mostly green." Green is PINNED and a pinned chain
// cannot flail, so it was not pinned: the release read the stage weight as a
// LATCH (`> 0` -> integrate), and a weight of 0.0007 freed him completely
// while the colour, reading the SAME number continuously, showed him seated.
// 51 % of the frames in his own log were released that way.
//
// The fix makes the weight modulate the pose stiffness -- his FREEDOM -- so
// stage 0 is welded to the pose and stage 3 is the shipped stiffness. This
// case pins the property that makes it true: pull-out must scale WITH the
// weight, and a nearly-seated man must be nearly immovable.
TEST_CASE("a small stage weight barely lets the jolt move him",
          "[body_chain]") {
    TrailChainInput in = body_input();
    in.n_pose = render::kBodyChainStations;
    for (int i = 0; i < render::kBodyChainStations; ++i)
        in.pose_p[i] = render::body_chain_stations()[
                           static_cast<std::size_t>(i)].rest_model;
    in.anchor = in.pose_p[0];
    const glm::vec3 jolt(0.0f, 0.0f, 40.0f);
    // the shipped law: stiffness = base / stage_arm, exactly as sled_model
    // applies it. Base 1 Hz is the shipped dial.
    const float arm[] = {0.01f, 0.05f, 0.25f, 1.0f};
    float pull[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; ++k) {
        TrailChainParams pr = body_params();
        pr.pose_stiff_hz = 1.0f / arm[k];
        TrailChainState st;
        trail_chain_reset(st, pr, in.anchor, glm::vec3(0.0f, -1.0f, 0.0f));
        for (int s = 0; s < 240; ++s) trail_chain_step(st, pr, in);
        TrailChainInput hit = in;
        hit.frame_accel_mps2 = jolt;
        float worst = 0.0f;
        for (int s = 0; s < 360; ++s) {
            trail_chain_step(st, pr, s < 12 ? hit : in);
            worst = std::fmax(worst,
                              glm::length(st.p[st.n] - in.pose_p[st.n]));
        }
        pull[k] = worst;
    }
    INFO("toe pull-out [m] at stage weight 0.01 / 0.05 / 0.25 / 1.0: "
         << pull[0] << " / " << pull[1] << " / " << pull[2] << " / "
         << pull[3]);
    // MONOTONE in the weight -- more unweighted always means more freedom.
    for (int k = 1; k < 4; ++k) CHECK(pull[k] >= pull[k - 1] - 1.0e-4f);
    // ★ THE DEFECT ITSELF: a nearly-seated man must be nearly immovable, and
    // an ORDER OF MAGNITUDE less free than a fully-unweighted one. Under the
    // latch this ratio was exactly 1 -- every weight above zero was total
    // freedom -- so this is the assertion the old code fails.
    CHECK(pull[0] < 0.1f * pull[3]);
    // NON-VACUITY: the fully-armed arm must actually move, or the ratio is a
    // comparison of two zeros.
    CHECK(pull[3] > 0.05f);
}
