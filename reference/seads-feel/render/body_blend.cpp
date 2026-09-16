#include "render/body_blend.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <glm/mat3x3.hpp>

namespace render {
namespace {

// solve_chain's own reachable shell, mirrored EXACTLY (render/sled_model.cpp:
// `const float dmin = fabs(len[0]-len[1]) + 1e-3f; dmax = len[0]+len[1]-1e-3f`).
// It is duplicated here on purpose and the duplication is the point: the IK
// CLAMPS a target into this shell silently, so a blend that hands it an
// unreachable point gets a foot that stops moving while every caller believes
// it moved. Reproducing the bound lets the blend refuse to build such a point
// at all -- and the gate pins the two against each other.
constexpr float kReachEps = 1.0e-3f;

float shell_min(float l0, float l1) { return std::fabs(l0 - l1) + kReachEps; }
float shell_max(float l0, float l1) { return l0 + l1 - kReachEps; }

// Smallest t in (0, t_hi] where |a + t*(b-a) - c| crosses `r`, or t_hi if the
// segment never crosses it. `inside` selects which crossing we care about:
// leaving a ball of radius r (true) or entering one (false).
float cross_t(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              float r, float t_hi, bool leaving) {
    const glm::vec3 d = b - a;
    const glm::vec3 f = a - c;
    const float A = glm::dot(d, d);
    if (!(A > 1.0e-12f)) return t_hi;  // a and b coincide: nothing to cross
    const float B = 2.0f * glm::dot(f, d);
    const float C = glm::dot(f, f) - r * r;
    const float disc = B * B - 4.0f * A * C;
    if (!(disc > 0.0f)) return t_hi;  // never touches the sphere
    const float sq = std::sqrt(disc);
    const float t0 = (-B - sq) / (2.0f * A);
    const float t1 = (-B + sq) / (2.0f * A);
    // Leaving an OUTER sphere: the first root ahead of us is the exit.
    // Entering an INNER sphere: likewise the first root ahead of us.
    (void)leaving;
    float t = t_hi;
    if (t0 > 0.0f && t0 < t) t = t0;
    if (t1 > 0.0f && t1 < t) t = t1;
    return t;
}

// One drawn limb, pushed out of the seat solid on its own. See the call site
// for why the centre is not the joint and the extents are not world extents.
glm::vec3 leg_seat_push(const TrailChainParams& pr, const TrailChainInput& in,
                        int station, std::size_t side, const glm::mat3& R,
                        const glm::vec3& joint_off, const glm::vec3& joint) {
    const TrailChainProbe& pb = pr.probe[station][side];
    if (!(pb.hx_m > 0.0f || pb.hy_m > 0.0f || pb.hz_m > 0.0f))
        return glm::vec3(0.0f);  // this station draws nothing on this side
    const glm::vec3 c =
        joint + R * (glm::vec3(pb.u_m, pb.v_m, pb.w_m) - joint_off);
    const glm::vec3 ax = R[0], ay = R[1], az = R[2];
    const glm::vec3 half(
        pb.hx_m * std::fabs(ax.x) + pb.hy_m * std::fabs(ay.x) +
            pb.hz_m * std::fabs(az.x),
        pb.hx_m * std::fabs(ax.y) + pb.hy_m * std::fabs(ay.y) +
            pb.hz_m * std::fabs(az.y),
        pb.hx_m * std::fabs(ax.z) + pb.hy_m * std::fabs(ay.z) +
            pb.hz_m * std::fabs(az.z));
    return trail_seat_escape(pr, in, c, half);
}

}  // namespace

float body_blend_reach_limit(const glm::vec3& a, const glm::vec3& b,
                             const glm::vec3& hip, float dmin, float dmax,
                             float w) {
    if (!(w > 0.0f)) return 0.0f;
    // ★ THE PREMISE, AND IT IS CHECKED RATHER THAN ASSUMED: the R3 end is
    // reachable, because pose_pass just solved the leg to it. If it is NOT --
    // a rig whose bind stance already breaks its own IK shell -- there is no
    // honest segment to walk and the blend stays at 0 rather than inventing a
    // start point. Silence here would be a limb that snaps on frame one.
    const float d0 = glm::length(a - hip);
    if (d0 > dmax || d0 < dmin) return 0.0f;

    float t = w;
    t = std::min(t, cross_t(a, b, hip, dmax, t, true));   // do not over-reach
    t = std::min(t, cross_t(a, b, hip, dmin, t, false));  // do not fold inside
    return std::max(0.0f, std::min(w, t));
}

glm::vec3 body_blend_bend_normal(const glm::vec3& hip, const glm::vec3& ankle,
                                 const glm::vec3& knee,
                                 const glm::vec3& fallback) {
    const glm::vec3 chord = ankle - hip;
    const float cl = glm::length(chord);
    if (!(cl > 1.0e-5f)) return fallback;
    const glm::vec3 dir = chord / cl;
    // The knee's offset from the chord, perpendicular component only.
    const glm::vec3 off = knee - hip;
    const glm::vec3 bend = off - dir * glm::dot(off, dir);
    const float bl = glm::length(bend);
    if (!(bl > 1.0e-5f)) return fallback;  // straight leg: no plane to name
    // solve_chain forms bend = normalize(cross(bend_n, dir)); invert it.
    const glm::vec3 n = glm::cross(dir, bend / bl);
    const float nl = glm::length(n);
    if (!(nl > 1.0e-5f)) return fallback;
    return n / nl;
}

BodyBlendOut body_blend_legs(const BodyBlendIn& in) {
    BodyBlendOut out;
    // ★★★ STAGE 0 IS AN EARLY RETURN, NOT ARITHMETIC. `a + 0*(b-a)` is `a` for
    // finite a, but it also turns an exact -0.0 into +0.0, and the midline
    // stations carry exact zeros in x. A bit-identity claim that rests on the
    // sign of zero is not one. Branching at exactly 0 also costs nothing in
    // the overwhelmingly common frame -- and it is EXACTLY 0, never an eps:
    // an eps threshold on a continuous weight is the latch disease this rung
    // has already paid for three times (handoff 8.3, 8.4).
    for (int s = 0; s < 2; ++s) {
        out.leg[s].knee = in.r3_knee[s];
        out.leg[s].ankle = in.r3_ankle[s];
        out.leg[s].w_eff = 0.0f;
        out.leg[s].reach_limited = false;
        out.leg[s].seat_push_m = 0.0f;
    }
    if (!(in.w > 0.0f) || in.station_p == nullptr || in.frame == nullptr)
        return out;

    const int st_knee = body_chain_station_of("knee");
    const int st_ankle = body_chain_station_of("ankle");
    if (st_knee < 0 || st_ankle < 0) return out;

    const std::array<BodyChainLegJoint, kBodyChainLegTargets>& lj =
        body_chain_leg_joints();
    // Find the two rows by STATION, never by table position -- the table has
    // already moved once in this program's history.
    const BodyChainLegJoint* row_knee = nullptr;
    const BodyChainLegJoint* row_ankle = nullptr;
    for (const BodyChainLegJoint& r : lj) {
        if (r.station == st_knee) row_knee = &r;
        if (r.station == st_ankle) row_ankle = &r;
    }
    if (row_knee == nullptr || row_ankle == nullptr) return out;

    for (int s = 0; s < 2; ++s) {
        // ---- reconstruct this side from the MIDLINE chain -----------------
        // The station table is the midpoint of each L/R joint pair, so a side
        // is its measured offset carried through the LIVE station frame. The
        // rotation only -- the frame's translation IS the station, which the
        // position below already adds.
        const glm::mat3 Rk(in.frame[st_knee]);
        const glm::mat3 Ra(in.frame[st_ankle]);
        const std::size_t si = static_cast<std::size_t>(s);
        const glm::vec3 ch_knee =
            in.station_p[st_knee] + Rk * row_knee->off_m[si];
        const glm::vec3 ch_ankle =
            in.station_p[st_ankle] + Ra * row_ankle->off_m[si];

        // ---- how far along that segment may we actually go? --------------
        const float dmin = shell_min(in.len_thigh[s], in.len_calf[s]);
        const float dmax = shell_max(in.len_thigh[s], in.len_calf[s]);
        const float w_eff = body_blend_reach_limit(
            in.r3_ankle[s], ch_ankle, in.hip[s], dmin, dmax, in.w);
        out.leg[s].w_eff = w_eff;
        out.leg[s].reach_limited = w_eff < in.w;

        // ★ ONE weight for both joints of the leg. Letting the knee run to a
        // different weight than the ankle would bend the limb by an amount
        // nothing measured.
        out.leg[s].knee = in.r3_knee[s] + (ch_knee - in.r3_knee[s]) * w_eff;
        out.leg[s].ankle = in.r3_ankle[s] + (ch_ankle - in.r3_ankle[s]) * w_eff;

        // ---- and then out of the seat ------------------------------------
        // The segment between two legal points crosses non-convex free space:
        // the seated ankle is on the running board, the trailing ankle is aft
        // and high, and the chord between them cuts through the seat and the
        // tunnel. Neither endpoint can see that and neither can the chain's
        // own keep-out, which grades the CHAIN. Same solid, one implementation.
        if (in.seat_pr != nullptr && in.seat_in != nullptr) {
            // ★★★ THE BOX IS NOT CENTRED ON THE JOINT, AND ITS EXTENTS ARE NOT
            // WORLD EXTENTS. Both were wrong here until the drive that found
            // the erratic legs sent this file back for review.
            //
            // A probe is the LIMB's oriented box about the MIDLINE station, in
            // the station frame. The joint is that same station plus its own
            // measured offset. So the box's centre relative to the joint is
            // `R * (probe_centre - joint_offset)` -- and its half-extents have
            // to be carried into world as the box's SUPPORT along each world
            // axis, because a rotated box is not a sphere and handing raw
            // frame extents to a world-axis test grades a body that does not
            // exist.
            //
            // ★ AND THIS IS THE HALF OF THE KEEP-OUT THE MIDLINE CANNOT DO.
            // Chad ruled the leg stations per-limb (see
            // TrailChainParams::probe_per_limb): the chain particle takes the
            // COMMON MODE of the two limbs' escapes, because one translation
            // cannot move two legs in opposite directions. The DIFFERENTIAL is
            // this -- each drawn limb, pushed out of the solid on its own,
            // here, where the two limbs finally exist as separate geometry.
            const glm::vec3 push_k =
                leg_seat_push(*in.seat_pr, *in.seat_in, st_knee, si, Rk,
                              row_knee->off_m[si], out.leg[s].knee);
            const glm::vec3 push_a =
                leg_seat_push(*in.seat_pr, *in.seat_in, st_ankle, si, Ra,
                              row_ankle->off_m[si], out.leg[s].ankle);
            out.leg[s].knee += push_k;
            out.leg[s].ankle += push_a;
            out.leg[s].seat_push_m =
                std::max(glm::length(push_k), glm::length(push_a));
        }
    }
    return out;
}

}  // namespace render
