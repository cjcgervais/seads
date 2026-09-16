#include "render/rider_pose.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace render {

static_assert(kAbsorbHoldHalfFrac > 0.0f,
              "absorb hold half-scale must be positive");
static_assert(kAbsorbHoldOnFrac >= 0.0f, "absorb hold dead band is non-negative");
static_assert(kAbsorbHitHalfMs > 0.0f, "absorb hit half-scale must be positive");
static_assert(kAbsorbHoldWeight >= 0.0f && kAbsorbHitWeight >= 0.0f,
              "absorb weights are non-negative");
static_assert(kAbsorbRootFrac >= 0.0f && kAbsorbRootFrac <= 1.0f,
              "the absorb split is a fraction of ONE drop, not a second drop");
static_assert(kAbsorbDropM > 0.0f, "absorb must not be inert (gate criterion)");
static_assert(kHingeFwdMaxRad > 0.0f,
              "the forward hinge limit is a positive magnitude");
static_assert(kHingeAftMaxRad >= 0.0f,
              "the aft hinge limit is a magnitude; 0 means aft stays a "
              "CG-solved translation (measured, see rider_pose.h)");
static_assert(kHingeNewtonSteps >= 1, "the residual must be solved, not seeded");
static_assert(kHingeDemandShare > 0.0f && kHingeDemandShare <= 1.0f,
              "the hinge covers a share of the demand, never more than all");

namespace {

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Gram-Schmidt: the component of `v` perpendicular to the UNIT vector `dir`.
// Returns false (and leaves `out` untouched) when v is parallel to dir.
bool perp_unit(const glm::vec3& v, const glm::vec3& dir, glm::vec3* out) {
    const glm::vec3 p = v - dir * glm::dot(v, dir);
    const float l = glm::length(p);
    if (!(l > kBendDegenerate)) return false;
    *out = p / l;
    return true;
}

}  // namespace

glm::mat4 socket_weld(const glm::mat4& socket_rest_world,
                      const glm::mat4& tip_rest_world,
                      const glm::vec3& reseat) {
    return glm::inverse(socket_rest_world) *
           glm::translate(glm::mat4(1.0f), reseat) * tip_rest_world;
}

glm::vec3 rest_bend_normal(const glm::vec3& root, const glm::vec3& mid,
                           const glm::vec3& tip,
                           const glm::mat3& root_rest_rot) {
    const glm::vec3 st = tip - root;
    const glm::vec3 se = mid - root;
    // PRIMARY: the authored elbow/knee direction. n such that cross(n, dir)
    // points from the chain line TOWARD the authored joint (the reverse order
    // points away and folds the joint through itself).
    const glm::vec3 n = glm::cross(st, se);
    const float nl = glm::length(n);
    if (nl > kBendDegenerate) return n / nl;

    // FALLBACK (defect 14): the chain's own parent frame, never a world
    // constant. See the header for why +Z, why it cannot flip, and what it
    // still does not solve.
    const float stl = glm::length(st);
    // A chain of zero length has no direction at all; return the root frame's
    // +Z unrotated rather than dividing by zero. Unreachable on any real rig,
    // present so this function is total.
    if (!(stl > kBendDegenerate)) return glm::normalize(root_rest_rot[2]);
    const glm::vec3 dir = st / stl;
    glm::vec3 out{0.0f};
    // ★★ F1 (verifier finding, 2026-08-16), FIXED HERE. Attempt 1 used the root
    // bone's +Z column. A bend NORMAL must ANTI-mirror between sides, because
    // the bend DIRECTION is cross(n, dir) and cross is handedness-flipping: if
    // n mirrors then the bend anti-mirrors and the two knees fold opposite ways.
    //
    // MEASURED on the shipped rest frames (thigh_L vs thigh_R, upperarm_L vs
    // upperarm_R, M = diag(-1,1,1)): the +X column satisfies R_col = -M*L_col to
    // dot -1.0000 -- it ANTI-mirrors exactly. +Y and +Z both MIRROR (dot
    // +1.0000). So +Z was precisely the wrong choice and +X is precisely the
    // right one. Cross-checked against the AUTHORED normal, the +X-derived
    // fallback scores dot +0.861592 on BOTH arms and -0.815822 on BOTH legs:
    // same sign within each pair, which is the invariant that was broken
    // (attempt 1's +Z column scores -0.795544/+0.795544 on the arms and
    // +0.874618/-0.874618 on the legs). ★ R1c re-took the LEG figures through
    // the real capture path -- they were documented from pre-§C geometry.
    if (perp_unit(root_rest_rot[0], dir, &out)) return out;  // bone +X
    // Fall-throughs must ALSO anti-mirror, so they cannot just be the +Z or +Y
    // columns (both mirror). cross(dir, u) with u mirroring anti-mirrors, and is
    // perpendicular to dir by construction.
    if (perp_unit(glm::cross(dir, root_rest_rot[2]), dir, &out)) return out;
    if (perp_unit(glm::cross(dir, root_rest_rot[1]), dir, &out)) return out;
    // Three mutually orthogonal axes cannot all be parallel to one direction;
    // keep the branch total anyway.
    return glm::vec3(0.0f, 0.0f, 1.0f);
    // ★ THE HONEST LIMIT, UNCHANGED. This makes the fallback deterministic,
    // continuous and SIDE-CONSISTENT. It does not make it agree with an authored
    // bend that does not exist, and the measured signs above say the rig's arm
    // and leg rolls disagree with each other (+0.86 vs -0.83), so no single
    // frame axis can point both an elbow and a knee the right way. That is a RIG
    // observation (filed for R1b/R2) and the structural answer is still §2.1's
    // pre-bends. On the shipped asset the branch is unreachable -- |cross| is
    // 0.109817 (arms) / 0.123604 (legs) against a 1e-4 threshold. ★ R1c: the
    // leg figure was 0.11552 here and 0.1454 in the header; both were pre-§C.
}

float rider_absorb(const float susp_x_m[3], const float susp_v_ms[3],
                   float susp_rest_m) {
    // max over the three kernel patches {ski L, ski R, track}: one ski finding
    // a rock is a hit, and averaging it away is the mush this defect is about.
    float x_max = susp_x_m[0], v_max = susp_v_ms[0];
    for (int i = 1; i < 3; ++i) {
        x_max = std::max(x_max, susp_x_m[i]);
        v_max = std::max(v_max, susp_v_ms[i]);
    }
    x_max = std::max(0.0f, x_max);
    v_max = std::max(0.0f, v_max);  // only COMPRESSION; rebound is not a hit

    float hold = 0.0f;
    if (susp_rest_m > 1e-6f) {
        // §B: dead band, then the SAME saturating map the rate term uses --
        // susp_x is heavy-tailed by the same two orders of magnitude and the
        // linear ramp is what jumped its full range in one tick. Bounded in
        // [0,1) by construction for every finite non-negative excess.
        const float s = std::max(0.0f, x_max - kAbsorbHoldOnFrac * susp_rest_m);
        hold = s / (s + kAbsorbHoldHalfFrac * susp_rest_m);
    }
    // Saturating, not a ramp -- susp_v is heavy-tailed by two orders of
    // magnitude (header). Bounded in [0,1) for every finite non-negative v.
    const float hit = v_max / (v_max + kAbsorbHitHalfMs);
    return clamp01(kAbsorbHoldWeight * hold + kAbsorbHitWeight * hit);
}

// ---------------------------------------------------------------------------
// §D -- the solved torso hinge
// ---------------------------------------------------------------------------

const std::array<RiderSegment, kRiderSegmentCount>& rider_segments() {
    static const std::array<RiderSegment, kRiderSegmentCount> s = {{
        // trunk: hip joint -> shoulder
        {kRiderPelvis, kRiderSpine3, 0.4970f, 0.500f},
        // head + neck: Winter's CG is at 1.000 of C7-T1 -> ear canal
        {kRiderSpine3, kRiderHead, 0.0810f, 1.000f},
        {kRiderUpperarmL, kRiderForearmL, 0.0280f, 0.436f},
        {kRiderForearmL, kRiderHandL, 0.0160f, 0.430f},
        {kRiderHandL, kRiderHandL, 0.0060f, 0.000f},
        {kRiderUpperarmR, kRiderForearmR, 0.0280f, 0.436f},
        {kRiderForearmR, kRiderHandR, 0.0160f, 0.430f},
        {kRiderHandR, kRiderHandR, 0.0060f, 0.000f},
        {kRiderThighL, kRiderShinL, 0.1000f, 0.433f},
        {kRiderShinL, kRiderFootL, 0.0465f, 0.433f},
        {kRiderFootL, kRiderFootL, 0.0145f, 0.000f},
        {kRiderThighR, kRiderShinR, 0.1000f, 0.433f},
        {kRiderShinR, kRiderFootR, 0.0465f, 0.433f},
        {kRiderFootR, kRiderFootR, 0.0145f, 0.000f},
    }};
    return s;
}

glm::vec3 rider_cg(const std::array<glm::vec3, kRiderJointCount>& joint_pos) {
    glm::vec3 sum(0.0f);
    float mass = 0.0f;
    for (const RiderSegment& s : rider_segments()) {
        const glm::vec3& a = joint_pos[static_cast<std::size_t>(s.prox)];
        const glm::vec3& b = joint_pos[static_cast<std::size_t>(s.dist)];
        sum += s.mass_frac * (a + s.cg_frac * (b - a));
        mass += s.mass_frac;
    }
    return mass > 0.0f ? sum / mass : sum;
}

RiderHingeModel capture_hinge_model(
    const std::array<glm::vec3, kRiderJointCount>& rest_joint_pos,
    float k_hat) {
    // The HINGED SET: the segments a pelvis rotation carries rigidly. In this
    // rig `pelvis`'s only joint children are the spine chain (and the arms
    // below it); `thigh_L/R` hang off `root`, which is why the legs are NOT in
    // here and why §A's split exists at all.
    glm::vec3 sum(0.0f);
    float mass = 0.0f;
    for (const RiderSegment& s : rider_segments()) {
        const bool hinged =
            (s.prox == kRiderPelvis && s.dist == kRiderSpine3) ||
            (s.prox == kRiderSpine3 && s.dist == kRiderHead);
        if (!hinged) continue;
        const glm::vec3& a = rest_joint_pos[static_cast<std::size_t>(s.prox)];
        const glm::vec3& b = rest_joint_pos[static_cast<std::size_t>(s.dist)];
        sum += s.mass_frac * (a + s.cg_frac * (b - a));
        mass += s.mass_frac;
    }
    RiderHingeModel m;
    m.k_hat = (std::fabs(k_hat) > 1e-3f) ? k_hat : 1.0f;
    if (!(mass > 0.0f)) return m;
    // hinge axis passes through the hip (the pelvis joint origin)
    const glm::vec3 hip = rest_joint_pos[kRiderPelvis];
    const glm::vec3 off = sum / mass - hip;
    m.a = mass * off.y;   // the sin(theta) arm
    m.b = mass * off.z;   // the (cos(theta) - 1) arm
    return m;
}

float hinge_cg_dz(const RiderHingeModel& m, float theta) {
    return m.a * std::sin(theta) + m.b * (std::cos(theta) - 1.0f);
}

float hinge_cg_dy(const RiderHingeModel& m, float theta) {
    // The same rotation, read on the other axis. Nothing new is measured here:
    // a and b are the SAME two captured numbers hinge_cg_dz uses.
    return m.a * (std::cos(theta) - 1.0f) - m.b * std::sin(theta);
}

float hinge_theta_for(const RiderHingeModel& m, float target_m) {
    // a*sin(t) + b*cos(t) = target + b  ->  R*sin(t + phi) = target + b
    const float R = std::sqrt(m.a * m.a + m.b * m.b);
    if (!(R > 1e-6f)) return 0.0f;
    const float phi = std::atan2(m.b, m.a);
    // kHingeDemandShare: how much of the demand the hinge tries to cover. At
    // 1.0 (shipped) this is "hinge first"; see rider_pose.h for the measured
    // clearance-vs-onset-rate frontier it rides.
    float s = (target_m * kHingeDemandShare + m.b) / R;
    s = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
    const float theta = std::asin(s) - phi;
    return theta < -kHingeAftMaxRad
               ? -kHingeAftMaxRad
               : (theta > kHingeFwdMaxRad ? kHingeFwdMaxRad : theta);
}

RiderLeanShift lean_apply_inv_jacobian(const RiderHingeModel& m, float res_z,
                                       float res_y) {
    // J = [[k_hat, k_zy], [k_yz, k_yy]] maps a root shift (fwd, up) onto a CG
    // displacement (dz, dy). This applies J^-1 to a residual. The off-diagonal
    // terms are small but REAL: a root rise changes how far forward the pinned
    // arms and legs fold, so it moves CG_z too, and vice versa. Ignoring them
    // would still converge (the Newton steps close on the true CG either way)
    // but it would need more of them.
    const float det = m.k_hat * m.k_yy - m.k_zy * m.k_yz;
    RiderLeanShift out;
    if (std::fabs(det) > 1e-6f) {
        out.fwd = (m.k_yy * res_z - m.k_zy * res_y) / det;
        out.up = (m.k_hat * res_y - m.k_yz * res_z) / det;
        return out;
    }
    // Degenerate Jacobian: fall back to the diagonal so the function is total
    // and can never divide by zero. Unreachable on any real rig (the measured
    // determinant is ~0.67), present because a solver that can NaN is worse
    // than one that is merely slower.
    out.fwd = std::fabs(m.k_hat) > 1e-6f ? res_z / m.k_hat : res_z;
    out.up = std::fabs(m.k_yy) > 1e-6f ? res_y / m.k_yy : res_y;
    return out;
}

RiderLeanShift lean_seed(const RiderHingeModel& m, float theta,
                         float lean_fwd_m, float lean_up_m) {
    return lean_apply_inv_jacobian(m, lean_fwd_m - hinge_cg_dz(m, theta),
                                   lean_up_m - hinge_cg_dy(m, theta));
}

RiderLeanShift lean_newton_step(const RiderHingeModel& m, RiderLeanShift d,
                                float cg_now_z, float cg_now_y,
                                float cg_target_z, float cg_target_y) {
    const RiderLeanShift s = lean_apply_inv_jacobian(
        m, cg_target_z - cg_now_z, cg_target_y - cg_now_y);
    RiderLeanShift out;
    out.fwd = d.fwd + s.fwd;
    out.up = d.up + s.up;
    return out;
}

// ---------------------------------------------------------------------------
// R2c-5 -- THE CONTROL-INPUT POSE
// ---------------------------------------------------------------------------

namespace {

// The probe angle the two sign measurements use. Small enough that it reads the
// LOCAL derivative of the motion (which is what a sign is), large enough to sit
// far above float noise on a 0.3 m arm: 0.1 rad moves an elbow ~30 mm.
constexpr float kSignProbeRad = 0.1f;

}  // namespace

glm::vec3 swing_bend_normal(const glm::vec3& bend_n, const glm::vec3& chain_dir,
                            float angle_rad) {
    const float l = glm::length(chain_dir);
    if (!(l > kBendDegenerate)) return bend_n;
    return glm::angleAxis(angle_rad, chain_dir / l) * bend_n;
}

glm::mat4 roll_about_axis(const glm::mat4& m, const glm::vec3& pivot,
                          const glm::vec3& axis, float angle_rad) {
    const float l = glm::length(axis);
    if (!(l > kBendDegenerate)) return m;
    const glm::mat4 r = glm::mat4(glm::mat3_cast(
        glm::angleAxis(angle_rad, axis / l)));
    return glm::translate(glm::mat4(1.0f), pivot) * r *
           glm::translate(glm::mat4(1.0f), -pivot) * m;
}

float swing_down_sign(const glm::vec3& shoulder, const glm::vec3& elbow,
                      const glm::vec3& chain_dir) {
    const float l = glm::length(chain_dir);
    if (!(l > kBendDegenerate)) return 0.0f;
    const glm::vec3 dir = chain_dir / l;
    // The elbow's RADIAL offset from the chain axis is the only part of it the
    // pole rotation moves, and it rotates by exactly the pole angle -- the
    // elbow sits at `s + dir*a + bend*h` and `bend = cross(bend_n, dir)` turns
    // with `bend_n`. So the sign is the sign of the radial vector's rise under
    // a positive probe, and it needs no lengths, no target and no solver.
    const glm::vec3 r = (elbow - shoulder) - dir * glm::dot(elbow - shoulder, dir);
    if (!(glm::length(r) > kBendDegenerate)) return 0.0f;
    const float dy = (glm::angleAxis(kSignProbeRad, dir) * r).y - r.y;
    if (!(std::fabs(dy) > 0.0f)) return 0.0f;
    return dy < 0.0f ? 1.0f : -1.0f;  // "+1 is DOWN"
}

// ---------------------------------------------------------------------------
// R3-WS -- THE FORE-AFT WEIGHT-SHIFT LADDER
// ---------------------------------------------------------------------------

static_assert(kLadderPelvisAftM > 0.30f,
              "SPEC 7.4 non-vacuity: the ladder must actually move the man");
static_assert(kArmRatioLimit < 1.0f,
              "the arm ratio ceiling must sit UNDER the IK clamp, or the wrist "
              "separates from the forearm -- that is the defect, not the gate");
static_assert(kArmRatioPull < kArmRatioLimit, "the design target has margin");
static_assert(kKneelUHi > kKneelULo, "the kneel band is a band");
// ★★ R3-WS(d) / D1. The stand band is the ONLY lever on the kneel kill-band
// pop (the pose is a pure function; a slew limiter would need state and would
// break the determinism law), so it must stay a real band and it must stay
// wide enough that the kernel's own slew cannot cross it in a couple of ticks.
static_assert(kKneelSHi > kKneelSLo, "the kneel KILL band is a band too");
static_assert(kKneelSHi - kKneelSLo >= 1.00f,
              "D1: the stand slew covers up to 0.0884 of `s` per 60 Hz tick, "
              "so a narrow kill band unwinds the whole kneel in 2-3 frames "
              "under the player's own stand key -- widen the band, never the "
              "continuity bound. The measured sweep bottoms out at the widest "
              "band `s` allows, which is why this is 0.80 and not the 0.20 "
              "SPEC 13.4 D1 started from");
// ★ R3-WS(d) / SPEC 13.2.1. The shard guard may only ever ADD margin: a floor
// under the anatomical chord means a SHALLOWER fold, never a deeper one.
static_assert(kKneelChordFloorM > 0.25f,
              "SPEC 11.4 A7's absolute chord floor plus SPEC 13.2.1's margin");
static_assert(kKneelTrunkMaxRad > kHeadUpTrunkMaxRad,
              "SPEC 13.2.2: the kneel's ceiling RELAXES the head-up one; a "
              "tighter cap would shorten the kneel retreat to nothing and "
              "ship a seated crouch wearing the kneel's name");

// ★ R3-WS(c) / SPEC 11.4 A4. kFootTrack is RETIRED; these are its successors.
static_assert(kFootSlideStandHiU >= 1.0f,
              "the stand band may only SLOW the slide, never speed it up: "
              "under 1 the standing foot would reach the rear limit before "
              "u = 1 and then sit clamped, which is a flat spot in the pose "
              "and a jump in the ladder coordinate");
static_assert(kFootZRearLimitM > kDeckZRearM,
              "the heel must stay ON the running board");
static_assert(kFootZRearLimitM < 0.0f, "the rear limit is aft of the origin");
static_assert(kSeatRetreatAftM > 0.0f && kSeatRetreatAftM < kLadderPelvisAftM,
              "A1: the seated retreat is a SHORTER, explicit dial -- the 0.320 "
              "kneel/reach anchor stays frozen");
static_assert(kHeadUpTrunkMaxRad < kReachHingeMaxRad,
              "the head-up cap must actually bind before the anatomical one");
static_assert(kAftSquatDropM > 0.0f && kAftSquatDropM < 0.25f,
              "a knees-bent brace, not a collapse");
static_assert(kSquatUHi > kSquatULo, "the squat band is a band");
static_assert(kNeckCounterMaxRad > 0.0f &&
                  kNeckCounterMaxRad <= kHeadUpTrunkMaxRad,
              "the neck may not counter more than the trunk pitched");

namespace {

float smoothstep01(float e0, float e1, float x) {
    if (!(e1 > e0)) return x < e0 ? 0.0f : 1.0f;
    const float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

// The seat profile, MEASURED by assets/character/sudburian_src/
// measure_seat_profile.py on 2026-08-20 off the shipped assets/sled/
// indy650.glb: a DOWNWARD RAYCAST at x = 0 (max y over the `seat` triangles
// whose plan footprint covers the station), 33 stations evenly spaced over
// z [-1.005000, +0.307615]. NOT a bounding box -- the bbox top 0.663307 is
// the crown of the front hump, 65 mm above the pan the pelvis rides.
//
// The SHAPE, in words: the back edge sits at 0.543322 and rises to the rear
// crest 0.642664 at z = -0.922962; the FLAT PAN 0.598322 runs z [-0.676848,
// -0.020544] and is where the pelvis rides; the front hump crests 0.662279 at
// z = +0.225570 and the nose falls to 0.380322.
//
// ★ R3-WS(b), HYGIENE: the block below is the script's OWN emitted table,
// pasted unedited -- numbers and station comments both. The shipped table had
// drifted from its generator in station 0 (0.543343 baked vs 0.543322
// emitted), which is exactly the "measure, never retype" law failing quietly:
// the asset test's 1e-4 tolerance was wide enough to hide it. Re-run the
// script and paste ITS output; never hand-edit a value here.
const std::array<float, kSeatStations> kSeatProfile = {{
    0.543322f, 0.619693f, 0.642664f, 0.633666f,  // z -1.0050 ..
    0.621018f, 0.610303f, 0.602266f, 0.600215f,  // z -0.8409 ..
    0.598322f, 0.598322f, 0.598322f, 0.598322f,  // z -0.6768 ..
    0.598322f, 0.598322f, 0.598322f, 0.598322f,  // z -0.5128 ..
    0.598322f, 0.598322f, 0.598322f, 0.598322f,  // z -0.3487 ..
    0.598322f, 0.598322f, 0.598322f, 0.598322f,  // z -0.1846 ..
    0.598322f, 0.598472f, 0.611233f, 0.623995f,  // z -0.0205 ..
    0.636756f, 0.649518f, 0.662279f, 0.527749f,  // z +0.1435 ..
    0.380322f,                                   // z +0.3076 ..
}};

// Fixed-count bisections. No tolerance loop, no accumulator: the pose stays a
// pure function of one tick (SPEC 2 / the determinism law).
constexpr int kBisectSteps = 48;

}  // namespace

const std::array<float, kSeatStations>& seat_profile_y() { return kSeatProfile; }

float seat_top_y(float z) {
    const float span = kSeatZFrontM - kSeatZRearM;
    const float t = clamp01((z - kSeatZRearM) / span) *
                    static_cast<float>(kSeatStations - 1);
    int i = static_cast<int>(t);
    if (i > kSeatStations - 2) i = kSeatStations - 2;
    const float f = t - static_cast<float>(i);
    const std::size_t k = static_cast<std::size_t>(i);
    return kSeatProfile[k] + f * (kSeatProfile[k + 1] - kSeatProfile[k]);
}

float seat_top_slope(float z) {
    const float span = kSeatZFrontM - kSeatZRearM;
    const float step = span / static_cast<float>(kSeatStations - 1);
    const float t = clamp01((z - kSeatZRearM) / span) *
                    static_cast<float>(kSeatStations - 1);
    int i = static_cast<int>(t);
    if (i > kSeatStations - 2) i = kSeatStations - 2;
    const std::size_t k = static_cast<std::size_t>(i);
    return (kSeatProfile[k + 1] - kSeatProfile[k]) / step;
}

WeightShiftWeights ws_weights(float u, float s, float s_lo, float s_hi) {
    WeightShiftWeights w;
    // ★★★ R3-WS(d) / D1. The stand band is an ARGUMENT now, because the gate
    // that sized it had to sweep it -- see render/rider_pose.h. Defaulted to
    // the shipped [kKneelSLo, kKneelSHi], so every existing caller is
    // unchanged and the runtime is on the constants.
    w.kneel = smoothstep01(kKneelULo, kKneelUHi, u) *
              (1.0f - smoothstep01(s_lo, s_hi, s));
    w.stand = clamp01(s) * (1.0f - w.kneel);
    w.seat = 1.0f - w.stand - w.kneel;
    return w;
}

float ws_reach_gate(float u, float s, float hi_stand) {
    // ★★★ R3-WS(b), THE STAND-ENTRY POP FIX, AND IT IS A RE-TIMING, NOT A
    // PATCH ON THE INVERSE. See render/rider_pose.h for the whole story: the
    // trunk-flexion ramp used to finish inside u <= kFootBlendHiU at EVERY
    // stand, which at high `s` spends more CG forward than the 48 mm of pelvis
    // retreat available there earns back -- so the baked C_s(u) DIPPED, the
    // inverse's monotone envelope skipped the band, and `u` stepped 0 -> 0.28
    // in one frame. The band is therefore widened WITH the stand, so the
    // flexion is always paid for by retreat that has already happened.
    //
    // At s = 0 this is kFootBlendHiU exactly, so the seated slice is
    // bit-identical to R3-WS as landed. At u = 0 it is 0 at every s, which is
    // the 0-OFF law.
    const float hi = kFootBlendHiU + (hi_stand - kFootBlendHiU) * clamp01(s);
    return smoothstep01(0.0f, hi, u);
}

float ws_rho(float u, float s, float rho0) {
    // The stand term is gated on the LADDER, not on `s` alone -- see the header
    // for why (a pure stand with no aft demand must be bit-identical to HEAD).
    // ★ R3-WS(b): the SAME re-timed gate the trunk flexion uses. rho and theta
    // are one schedule; letting them ramp on different bands would put a
    // second kink into C_s(u) at the very place this fix exists to smooth.
    const float gate = ws_reach_gate(u, s);
    const float x = std::max(u, kStandRhoGain * clamp01(s) * gate);
    const float k = smoothstep01(0.0f, 1.0f, x);
    return rho0 + (kArmRatioPull - rho0) * k;
}

ReachSolve reach_solve(const glm::vec3& pelvis, const glm::vec3& shoulder_off,
                       const glm::vec3& hand, float target_dist) {
    ReachSolve out;
    const glm::vec3 v = shoulder_off;
    const glm::vec3 u = hand - pelvis;
    const float d0 = glm::length(v - u);
    out.dist_m = d0;
    if (d0 <= target_dist) return out;  // rest already reaches: theta = 0

    const float vyz = std::sqrt(v.y * v.y + v.z * v.z);
    const float q = std::sqrt(u.y * u.y + u.z * u.z);
    // A rotation of theta about model +X maps the (z, y) argument to
    // arg - theta (y' = y cos - z sin, z' = y sin + z cos), so the angles below
    // are taken in that plane with z as the real axis.
    const float phi_v = std::atan2(v.y, v.z);
    const float phi_u = std::atan2(u.y, u.z);
    const float base = phi_v - phi_u;  // the CLOSEST-APPROACH angle
    if (!(vyz > 1e-6f) || !(q > 1e-6f)) return out;  // degenerate: no circle

    const float ax = u.x - v.x;
    const float rp2 = target_dist * target_dist - ax * ax;
    float theta = base;
    if (rp2 > 0.0f) {
        const float rp = std::sqrt(rp2);
        const float c = (vyz * vyz + q * q - rp * rp) / (2.0f * vyz * q);
        if (c <= 1.0f) {
            const float delta = std::acos(c < -1.0f ? -1.0f : c);
            const float twopi = 6.28318530718f;
            float best = -1.0f;
            for (const float cand : {base - delta, base + delta}) {
                float t = std::fmod(cand, twopi);
                if (t < 0.0f) t += twopi;
                if (best < 0.0f || t < best) best = t;
            }
            theta = best;
            out.reached = true;
        } else {
            out.reached = false;  // the circle never gets that close
        }
    } else {
        out.reached = false;  // even the x offset alone exceeds the target
    }
    if (theta < 0.0f) theta = 0.0f;
    if (theta > kReachHingeMaxRad) {
        theta = kReachHingeMaxRad;
        out.reached = false;
    }
    out.theta_rad = theta;
    const float ct = std::cos(theta), stt = std::sin(theta);
    const glm::vec3 sh(v.x, v.y * ct - v.z * stt, v.y * stt + v.z * ct);
    out.dist_m = glm::length(sh - u);
    return out;
}

float reach_pair_ratio(const glm::vec3& pelvis, const ReachArm& a,
                       const ReachArm& b, float theta_rad) {
    const float c = std::cos(theta_rad), sn = std::sin(theta_rad);
    float worst = 0.0f;
    for (const ReachArm* arm : {&a, &b}) {
        if (!(arm->dmax > 1e-6f)) continue;
        const glm::vec3& v = arm->shoulder_off;
        const glm::vec3 sh(v.x, v.y * c - v.z * sn, v.y * sn + v.z * c);
        worst = std::max(worst,
                         glm::length(pelvis + sh - arm->hand) / arm->dmax);
    }
    return worst;
}

ReachPair reach_pair_solve(const glm::vec3& pelvis, const ReachArm& a,
                           const ReachArm& b, float rho_design) {
    ReachPair out;
    out.ratio = reach_pair_ratio(pelvis, a, b, 0.0f);
    if (out.ratio <= rho_design) {   // rest already reaches: theta stays 0
        out.met_design = true;
        return out;
    }
    const float step = kReachHingeMaxRad / static_cast<float>(kReachScanSteps);
    float best_v = out.ratio;
    int best_i = 0, hit = -1;
    for (int i = 1; i <= kReachScanSteps; ++i) {
        const float r =
            reach_pair_ratio(pelvis, a, b, step * static_cast<float>(i));
        if (r < best_v) { best_v = r; best_i = i; }
        if (hit < 0 && r <= rho_design) hit = i;
    }
    if (hit >= 0) {
        // the SMALLEST theta that attains the design ratio, refined
        float lo = step * static_cast<float>(hit - 1);
        float hi = step * static_cast<float>(hit);
        for (int i = 0; i < kReachRefineSteps; ++i) {
            const float mid = 0.5f * (lo + hi);
            if (reach_pair_ratio(pelvis, a, b, mid) <= rho_design)
                hi = mid;
            else
                lo = mid;
        }
        out.theta_rad = hi;
        out.ratio = reach_pair_ratio(pelvis, a, b, hi);
        out.met_design = true;
        return out;
    }
    // unattainable: take the BEST the man can do. Ternary search inside the
    // bracketing scan cell -- fixed count, no tolerance loop.
    float lo = step * static_cast<float>(std::max(0, best_i - 1));
    float hi = step * static_cast<float>(std::min(kReachScanSteps, best_i + 1));
    for (int i = 0; i < kReachRefineSteps; ++i) {
        const float m1 = lo + (hi - lo) / 3.0f;
        const float m2 = hi - (hi - lo) / 3.0f;
        if (reach_pair_ratio(pelvis, a, b, m1) <=
            reach_pair_ratio(pelvis, a, b, m2))
            hi = m2;
        else
            lo = m1;
    }
    out.theta_rad = 0.5f * (lo + hi);
    out.ratio = reach_pair_ratio(pelvis, a, b, out.theta_rad);
    return out;
}

float ws_trunk_cap(float w_kneel) {
    // ★ SPEC 13.2.2. A LINEAR blend of two ceilings, on the same w_kneel the
    // branch itself rides -- so at w = 0 it is EXACTLY kHeadUpTrunkMaxRad and
    // R3-WS(c)'s head-up law is bit-identical wherever the kneel is absent,
    // which is every seated cell, every standing cell and all of 0-OFF.
    const float k = clamp01(w_kneel);
    return kHeadUpTrunkMaxRad + (kKneelTrunkMaxRad - kHeadUpTrunkMaxRad) * k;
}

WsStation ws_solve_station(const glm::vec3& p0, float pelvis_rest_z,
                           float seat_follow, float extra_dy,
                           const ReachArm& a, const ReachArm& b,
                           float want_aft, float rho, float gate,
                           float ceiling, float cap) {
    const float y0 = seat_top_y(pelvis_rest_z);
    const auto eval = [&](float aft, WsStation* out) {
        const float dy =
            seat_follow * (seat_top_y(pelvis_rest_z - aft) - y0) + extra_dy;
        const glm::vec3 p = p0 + glm::vec3(0.0f, dy, -aft);
        ReachPair r = reach_pair_solve(p, a, b, rho);
        // ★★★ R3-WS(c) / A8. THE HEAD-UP CAP, AND ITS ORDER MATTERS.
        // `min(theta * gate, cap)` is a CEILING on the delivered angle;
        // `min(theta, cap) * gate` would be a re-timing that still ramps to
        // the cap and lets the trunk lie over further at intermediate u. Chad
        // asked for the head up, not for a slower fold. The reach shortfall a
        // capped trunk leaves is then paid by the SPEC 4 shortening below --
        // the same mechanism, so want/got stay converged and the arm ratio is
        // still measured at the angle actually posed.
        // ★ R3-WS(d): the ceiling is the CALLER'S now (ws_trunk_cap blends the
        // kneel's own 55 deg in by w_kneel). The A8 ORDER is unchanged --
        // min(theta * gate, cap), a ceiling on the delivered angle and never a
        // re-timing of the ramp.
        const float th = std::fmin(r.theta_rad * gate, cap);
        out->aft_m = aft;
        out->dy_m = dy;
        out->theta_rad = th;
        out->ratio = reach_pair_ratio(p, a, b, th);
        return out->ratio;
    };
    WsStation st;
    if (eval(want_aft, &st) <= ceiling) return st;
    // ★ SPEC 4: SHORTEN THE TRAVEL, AND SAY SO. The worse arm ratio rises
    // monotonically with the retreat, so a fixed bisection finds the furthest
    // honest station. The arm never clamps; the CG that costs is REPORTED.
    float lo = 0.0f, hi = want_aft;
    WsStation probe;
    for (int i = 0; i < 20; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (eval(mid, &probe) > ceiling) hi = mid; else lo = mid;
    }
    eval(lo, &st);
    st.shortened = true;
    return st;
}

float reach_max_aft(const glm::vec3& pelvis, const glm::vec3& shoulder_off,
                    const glm::vec3& hand, float limit_dist) {
    const glm::vec3 v = shoulder_off;
    const glm::vec3 u = hand - pelvis;
    const float ax = u.x - v.x;
    const float rp2 = limit_dist * limit_dist - ax * ax;
    if (!(rp2 > 0.0f)) return 0.0f;
    const float vyz = std::sqrt(v.y * v.y + v.z * v.z);
    const float qmax = vyz + std::sqrt(rp2);
    const float dy2 = u.y * u.y;
    if (!(qmax * qmax > dy2)) return 0.0f;
    // Sliding the pelvis aft by `a` grows u.z by a and leaves u.y alone, so the
    // feasibility band q <= qmax closes at exactly this a. Closed form.
    const float zmax = std::sqrt(qmax * qmax - dy2);
    const float a = zmax - u.z;
    return a > 0.0f ? a : 0.0f;
}

float kneel_chord_m(float thigh_m, float shin_m) {
    // The included angle at the knee at maximum flexion. Flexion is measured
    // from the straight leg, so the interior angle is pi - flexion.
    const float interior = 3.14159265f - kKneeFlexMaxRad;
    return std::sqrt(thigh_m * thigh_m + shin_m * shin_m -
                     2.0f * thigh_m * shin_m * std::cos(interior));
}

KneelPose kneel_construct(const glm::vec3& pelvis, float thigh_m,
                          float shin_m, bool guard_chord) {
    KneelPose out;
    // 1. the KNEE: the FORWARD intersection of the thigh sphere with the seat
    //    surface raised by the measured knee-pad radius. Bisection on z, which
    //    is monotone here because the surface is single-valued in z and the
    //    pelvis sits behind the pan.
    {
        float lo = pelvis.z, hi = kSeatZFrontM;
        const auto f = [&](float z) {
            const glm::vec3 k(pelvis.x, seat_top_y(z) + kKneePadRM, z);
            return glm::length(k - pelvis) - thigh_m;
        };
        if (!(f(hi) > 0.0f)) return out;  // the sphere misses the seat entirely
        for (int i = 0; i < kBisectSteps; ++i) {
            const float mid = 0.5f * (lo + hi);
            if (f(mid) < 0.0f) lo = mid; else hi = mid;
        }
        const float z = 0.5f * (lo + hi);
        out.knee = glm::vec3(pelvis.x, seat_top_y(z) + kKneePadRM, z);
    }
    // 2. the ANKLE: the REARWARD intersection of the shin sphere with the seat
    //    surface raised by the measured ankle height, so the shin LIES ALONG
    //    the seat instead of being tilted by a made-up "heel lift".
    // ★ SPEC 3c.3: "the foot may hang past the seat rear -- that is 'feet all
    // the way to the back' and is correct". seat_top_y clamps to the rear
    // station behind the seat, so the surface continues flat and the shin can
    // finish there instead of the branch failing at the back edge.
    {
        float lo = kSeatZRearM - 0.250f, hi = out.knee.z;
        const auto f = [&](float z) {
            const glm::vec3 a(pelvis.x, seat_top_y(z) + kAnkleAboveSoleM, z);
            return glm::length(a - out.knee) - shin_m;
        };
        if (!(f(lo) > 0.0f)) return out;  // the seat ends before the shin does
        for (int i = 0; i < kBisectSteps; ++i) {
            const float mid = 0.5f * (lo + hi);
            if (f(mid) < 0.0f) hi = mid; else lo = mid;
        }
        const float z = 0.5f * (lo + hi);
        out.foot = glm::vec3(pelvis.x, seat_top_y(z) + kAnkleAboveSoleM, z);
    }
    // 3. ★★★ R3-WS(d) / SPEC 13.2.1 -- THE SHARD GUARD, ENFORCED **HERE**, AT
    //    THE LIVE HIP, AND NOT ONLY AT THE DESIGN STATION.
    //
    //    kneel_sit_height solves the hip height that puts the chord on the
    //    floor at ONE station. The runtime rebuilds this construction at the
    //    hip the pose actually has -- after SPEC 4's shortening, the lateral
    //    channel, the steer and the R1c Newton's root nudge -- and at those
    //    hips the two seat contacts can close the chord back down: measured
    //    0.2231 m at full kneel and 0.2019 m across the blend, against a
    //    0.197 m shard chord. That is millimetres from the tear this guard
    //    exists to prevent, so the floor is enforced on the CONSTRUCTION, not
    //    only on the design.
    //
    //    The lever SPEC 13.2.1 names is the heel: "a shallower heel-sit". So
    //    the ankle is pushed OUT along the hip->ankle ray until the chord
    //    meets the floor -- the heel lifts off the seat and the amount is
    //    REPORTED (heel_lift_m), never swallowed. The seat still owns the
    //    KNEE, which is the contact Chad's words are about ("knees on the rear
    //    seat surface"); the heel is the one that leaves.
    const float want = kneel_target_chord_m(thigh_m, shin_m);
    const float reach = thigh_m + shin_m - 1e-3f;
    const float floor_c = guard_chord ? (want < reach ? want : reach) : 0.0f;
    {
        const glm::vec3 r = out.foot - pelvis;
        const float d = glm::length(r);
        if (d > 1e-6f && d < floor_c) {
            out.foot = pelvis + r * (floor_c / d);
            out.heel_lift_m = floor_c - d;
        }
    }
    // 4. ★ AND THE KNEE IS RE-DERIVED FROM THE TRIANGLE THAT ACTUALLY SHIPS.
    //    SPEC 3c.4 gates |solved knee - K| <= 1 mm, and that holds only if
    //    (hip, K, ankle) is a CONSISTENT triangle -- lifting the heel breaks
    //    the one the seat solve built. So K is recomputed by the same analytic
    //    two-bone rule the IK uses, in the plane the SEAT knee defines (which
    //    is a measured direction off the profile, never a typed sign). When
    //    the heel did not move this reproduces the seat knee exactly, so the
    //    unguarded path is bit-identical.
    {
        const glm::vec3 st = out.foot - pelvis;
        const float d = glm::length(st);
        if (!(d > 1e-6f)) return out;   // degenerate: leave `ok` false
        const glm::vec3 dir = st / d;
        const glm::vec3 to_k = out.knee - pelvis;
        glm::vec3 perp = to_k - dir * glm::dot(to_k, dir);
        const float pl = glm::length(perp);
        if (!(pl > 1e-6f)) return out;  // the seat knee is on the chord axis
        perp /= pl;
        const float a =
            (thigh_m * thigh_m + d * d - shin_m * shin_m) / (2.0f * d);
        const float h2 = thigh_m * thigh_m - a * a;
        out.knee = pelvis + dir * a + perp * (h2 > 0.0f ? std::sqrt(h2) : 0.0f);
    }
    out.ok = true;
    return out;
}

float kneel_target_chord_m(float thigh_m, float shin_m) {
    // ★★★ R3-WS(d) / SPEC 13.2.1. THE SHARD GUARD. The knee joint stops at
    // kKneeFlexMaxRad; the shipped SKIN stops earlier, and the mesh wins,
    // because what ships is the artefact. Neither number is a taste dial: one
    // is anatomy, the other is the measured chord at which `sudburian_proxy`
    // fans into shards plus SPEC 13.2.1's 10 mm margin.
    const float anat = kneel_chord_m(thigh_m, shin_m);
    return anat > kKneelChordFloorM ? anat : kKneelChordFloorM;
}

float kneel_flex_rad(float thigh_m, float shin_m, float chord_m) {
    // The inverse of kneel_chord_m: report what the knee ACTUALLY folded to,
    // never what it was asked for (SPEC 13.4 D6).
    const float den = 2.0f * thigh_m * shin_m;
    if (!(den > 1e-9f)) return 0.0f;
    float c = (thigh_m * thigh_m + shin_m * shin_m - chord_m * chord_m) / den;
    if (c > 1.0f) c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    return 3.14159265f - std::acos(c);
}

glm::vec3 ws_kneel_foot_mix(const glm::vec3& deck, const glm::vec3& kneel,
                            float w_kneel) {
    // ★★★ R3-WS(d) / D4(b). THE STRAIGHT MIX, AND THE CLIP IT LEAVES IS
    // DISCLOSED RATHER THAN DESIGNED AWAY -- render/rider_pose.h carries the
    // three routes that were measured and why none of them clears the seat
    // without failing a continuity gate. This is the endpoint-exact blend the
    // C1 law and 0-OFF want; the residual 0.1298 m penetration is reported in
    // the sweep and judged in the rear-three-quarter shot.
    const float w = clamp01(w_kneel);
    return deck + (kneel - deck) * w;
}

glm::vec3 ws_blend_bend_normal(const glm::vec3& n0, const glm::vec3& n1,
                               const glm::vec3& chain_dir, float w) {
    // See render/rider_pose.h: a straight mix() of two near-antiparallel
    // normals collapses mid-blend and tears the leg. Rotate the POLE about the
    // chain instead -- unit length everywhere, no degenerate crossing.
    const float t = clamp01(w);
    const float cl = glm::length(chain_dir);
    if (!(cl > kBendDegenerate) || !(t > 0.0f)) return n0;
    const glm::vec3 axis = chain_dir / cl;
    // The components that solve_chain actually reads: the parts of each normal
    // perpendicular to the chain.
    const glm::vec3 p0 = n0 - axis * glm::dot(n0, axis);
    const glm::vec3 p1 = n1 - axis * glm::dot(n1, axis);
    const float l0 = glm::length(p0), l1 = glm::length(p1);
    if (!(l0 > kBendDegenerate)) return t >= 1.0f ? n1 : n0;
    if (!(l1 > kBendDegenerate)) return n0;   // n1 has no pole: keep n0's
    const glm::vec3 a = p0 / l0, b = p1 / l1;
    // The SIGNED angle from a to b about the chain -- measured, never assumed,
    // and taken the short way round so the pole never sweeps the long side.
    const float ang = std::atan2(glm::dot(glm::cross(a, b), axis),
                                 glm::dot(a, b));
    return swing_bend_normal(n0, axis, ang * t);
}

float kneel_sit_height(float z_p, float x_side, float thigh_m, float shin_m) {
    const float want = kneel_target_chord_m(thigh_m, shin_m);
    // |P - ankle| rises monotonically with the hip height: lifting the hip
    // opens the knee. Bisect over a band that brackets seated (0.02) and
    // upright-on-the-knees (thigh_m).
    float lo = 0.02f, hi = thigh_m;
    const auto f = [&](float h) {
        const glm::vec3 p(x_side, seat_top_y(z_p) + h, z_p);
        // ★ R3-WS(d): UNGUARDED. This bisection is solving for the hip height
        // at which the seat's own two contacts deliver `want`; running the
        // guard inside it would clamp every probe to `want` and leave the
        // function with no sign change to bisect on.
        const KneelPose k = kneel_construct(p, thigh_m, shin_m, false);
        if (!k.ok) return 1.0f;  // out of the seat = "too extended", push down
        return glm::length(k.foot - p) - want;
    };
    if (f(lo) > 0.0f) return lo;
    if (f(hi) < 0.0f) return hi;
    for (int i = 0; i < kBisectSteps; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (f(mid) < 0.0f) lo = mid; else hi = mid;
    }
    return 0.5f * (lo + hi);
}

// ★★★ R3-WS(c). THE FULL RUNNER SLIDE. 1.0054 m of travel, 3.1x v1's, on its
// OWN band rather than the pelvis's -- and the band may widen with the stand
// (the A3 mechanism) if the bake's continuity gate ever needs it to. It does
// not today: the sweep in render/rider_pose.h shows the FLEXION band is the
// lever, so this ships at 1.00 and the feet go all the way back at every stand
// fraction. hi_f(0) is 1 by construction, whatever hi_stand is.
float ws_foot_slide(float u, float s, float hi_stand) {
    const float hi = 1.0f + (hi_stand - 1.0f) * clamp01(s);
    return smoothstep01(0.0f, hi, u);
}

glm::vec3 deck_foot_target(const glm::vec3& foot_rest, float slide) {
    glm::vec3 f = foot_rest;
    f.y = kDeckSheetYM + kAnkleAboveSoleM;
    // ★ A5: the rear limit is the HEEL's, not the boot centre's -- +81 mm of
    // honest travel over the naive half-length figure.
    f.z = foot_rest.z + (kFootZRearLimitM - foot_rest.z) * clamp01(slide);
    const float zmax = kDeckZFrontM - kBootHalfLenM - 0.02f;
    if (f.z < kFootZRearLimitM) f.z = kFootZRearLimitM;
    if (f.z > zmax) f.z = zmax;
    return f;
}

float ws_want_aft(float u, float w_kneel) {
    // ★ A1. The kneel's geometry is solved against kLadderPelvisAftM and must
    // not move; the seated/standing ladder rides the eye-dial. The kneel gain
    // ships at 0, so the runtime is on kSeatRetreatAftM alone -- but the blend
    // is written once, here, rather than at the (untested) wiring site.
    const float k = clamp01(w_kneel);
    return clamp01(u) *
           (kSeatRetreatAftM + (kLadderPelvisAftM - kSeatRetreatAftM) * k);
}

float ws_squat_drop(float u, float w_stand) {
    // SPEC 11.2.3, and it is STANDING-only by construction: seated, the pelvis
    // rides the measured seat profile and this term is multiplied by zero.
    return -kAftSquatDropM * smoothstep01(kSquatULo, kSquatUHi, u) *
           clamp01(w_stand);
}

float neck_up_sign(const glm::vec3& neck, const glm::vec3& head) {
    // The same probe every other sign on this rig is measured with: rotate the
    // radial offset by a small POSITIVE angle about model +X and read whether
    // the head rose. Nothing is typed and nothing is assumed to mirror.
    const glm::vec3 r = head - neck;
    if (!(glm::length(r) > kBendDegenerate)) return 0.0f;
    const float dy =
        (glm::angleAxis(kSignProbeRad, glm::vec3(1.0f, 0.0f, 0.0f)) * r).y -
        r.y;
    if (!(std::fabs(dy) > 0.0f)) return 0.0f;
    return dy > 0.0f ? 1.0f : -1.0f;  // "+1 is UP"
}

glm::quat ws_neck_counter(float theta_r, float up_sign,
                          const glm::quat& parent_rest_q) {
    // ★ SPEC 11.2.4 / A9. "his head comes up". The trunk pitches theta_r
    // forward about model +X through the pelvis and the whole neck rides it
    // rigidly, so countering by the same angle at the neck puts the helmet
    // back on the horizon -- capped, because a real neck runs out of extension
    // long before a 38 deg trunk does.
    const float mag = std::fmin(std::fmax(theta_r, 0.0f), kNeckCounterMaxRad);
    if (!(mag > 0.0f)) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // 0-OFF
    const glm::quat q_model =
        glm::angleAxis(up_sign * mag, glm::vec3(1.0f, 0.0f, 0.0f));
    return glm::conjugate(parent_rest_q) * q_model * parent_rest_q;
}

bool ws_monotone(const float* c, int n) {
    for (int i = 1; i < n; ++i)
        if (!(c[i] > c[i - 1])) return false;
    return true;
}

float ws_min_slope(const float* c, int n) {
    if (n < 2) return 0.0f;
    float worst = c[1] - c[0];
    for (int i = 2; i < n; ++i) worst = std::min(worst, c[i] - c[i - 1]);
    return worst;
}

float ws_invert(const float* c, int n, float a_m) {
    if (n < 2) return 0.0f;
    if (!(a_m > c[0])) return 0.0f;
    // ★ R3-WS(b). THE ENDPOINT IS THE ENVELOPE'S, NOT THE RAW TABLE'S. The
    // early return used to read `c[n-1]`, which is the same ambiguity the
    // running maximum below exists to remove -- just at the other end: a curve
    // that PEAKS before its last station and falls back would answer u = 1.0
    // for every demand above that falling endpoint, jumping the ladder to its
    // far stop. Comparing against max(c) instead makes the two ends agree, and
    // the synthetic end-dipping case in test_rider_pose pins it.
    float cmax = c[0];
    for (int i = 1; i < n; ++i) cmax = std::max(cmax, c[i]);
    if (a_m >= cmax) return 1.0f;
    // ★ THE MONOTONE ENVELOPE, AND WHY IT IS NO LONGER LOAD-BEARING.
    //
    // As R3-WS landed, the measured curve DIPPED at high stand -- the reach
    // hinge ramped in over u in [0, 0.15] and pulled the trunk forward before
    // the butt had retreated far, so C_s(u) reached -0.0195 m at s = 1 before
    // it climbed. The envelope below hid that from the inverse's monotonicity
    // but NOT from the rider: a flat spot in the envelope is a JUMP in u, and
    // the tick where the demand crossed the dip band snapped u 0 -> 0.28 in
    // one frame (feet off the weld, trunk folding, pelvis 60-90 mm) entering
    // AND leaving aft. R3-WS(b) removes the dip BY CONSTRUCTION instead
    // (ws_reach_gate re-times the flexion ramp with the stand), and
    // render/sled_model.cpp's bake now HARD-GATES every shipped slice on BOTH
    // dC/du > 0 and kWsContinuityBoundM, so a flat spot cannot reach a player
    // again.
    //
    // The envelope stays because it costs nothing and it is the honest answer
    // for an out-of-band table (a hand-built curve in a test, a future bake
    // that has not been gated yet): it picks the FIRST u that delivers the
    // demand and never goes backwards.
    float run = c[0];
    float prev = run;
    for (int i = 1; i < n; ++i) {
        const float cur = c[i] > run ? c[i] : run;
        if (a_m <= cur) {
            const float d = cur - prev;
            const float f = d > 1e-9f ? (a_m - prev) / d : 0.0f;
            return (static_cast<float>(i - 1) + f) / static_cast<float>(n - 1);
        }
        run = cur;
        prev = cur;
    }
    return 1.0f;
}

float ws_ladder_u(const float c[kWsBakeS][kWsBakeU], float a_m, float s) {
    const float t = clamp01(s) * static_cast<float>(kWsBakeS - 1);
    int i = static_cast<int>(t);
    if (i > kWsBakeS - 2) i = kWsBakeS - 2;
    const float f = t - static_cast<float>(i);
    const float u0 = ws_invert(c[i], kWsBakeU, a_m);
    const float u1 = ws_invert(c[i + 1], kWsBakeU, a_m);
    return u0 + f * (u1 - u0);
}

float wrist_up_sign(const glm::vec3& wrist, const glm::vec3& pivot,
                    const glm::vec3& axis) {
    const float l = glm::length(axis);
    if (!(l > kBendDegenerate)) return 0.0f;
    const glm::vec3 a = axis / l;
    const glm::vec3 r = (wrist - pivot) - a * glm::dot(wrist - pivot, a);
    if (!(glm::length(r) > kBendDegenerate)) return 0.0f;
    const float dy = (glm::angleAxis(kSignProbeRad, a) * r).y - r.y;
    if (!(std::fabs(dy) > 0.0f)) return 0.0f;
    return dy > 0.0f ? 1.0f : -1.0f;  // "+1 is UP"
}

// ---- R3-HANDS: the control curl -------------------------------------------
// See the header for the ruling. NOTHING here is typed: the axis, the
// direction and the angle are all MEASURED against the lever the bone reaches
// for, so a re-export that moves the levers moves the animation with them
// rather than silently leaving it pointing at where the lever used to be.

CurlAim curl_aim(const glm::mat4& bone_world, const glm::vec3& target_w) {
    CurlAim out;
    const glm::vec3 p(bone_world[3]);
    // The bone's own frame. Normalised because a skinned node may carry scale,
    // and an un-normalised "axis" would smuggle that scale into the angle.
    const glm::vec3 ax[3] = {glm::vec3(bone_world[0]), glm::vec3(bone_world[1]),
                             glm::vec3(bone_world[2])};
    glm::vec3 e[3];
    for (int i = 0; i < 3; ++i) {
        const float l = glm::length(ax[i]);
        if (!(l > kBendDegenerate)) return out;  // degenerate frame: no aim
        e[i] = ax[i] / l;
    }
    // ★ THE BONE'S LENGTH IS DELIBERATELY NOT USED. A leaf bone has no child,
    // so the shipped GLB does not carry its length, and inventing one to place
    // a "tip" would be exactly the guess this repo bans. It is not needed: the
    // tip's motion under a rotation about e[i] runs along cross(e[i], e[Y]),
    // whose DIRECTION is independent of length -- and a direction is all the
    // axis choice needs. The magnitude stays where R2c-5 put it: a constant
    // Chad rules, not a number this function invents.
    const glm::vec3 to_t = target_w - p;
    if (!(glm::length(to_t) > kBendDegenerate)) return out;  // already there
    // Which of the bone's OWN axes swings the tip toward the target fastest.
    // +Y is excluded by construction (a bone cannot swing about itself), which
    // falls out of cross(Y, Y) == 0 rather than being special-cased.
    int best = -1;
    float best_score = 0.0f;
    for (const int i : {0, 2}) {
        const float s = glm::dot(glm::cross(e[i], e[1]), glm::normalize(to_t));
        if (std::fabs(s) > std::fabs(best_score)) {
            best_score = s;
            best = i;
        }
    }
    if (best < 0 || !(std::fabs(best_score) > kBendDegenerate)) return out;
    out.axis = best;
    out.sign = best_score > 0.0f ? 1.0f : -1.0f;
    return out;
}

glm::quat curl_rotation(const CurlAim& c, float rad, float amt) {
    // ★ 0-OFF. amt == 0 returns the identity EXACTLY, so the signed rest
    // visual is reproduced bit-for-bit and a control bone that never moves
    // cannot perturb the pose it was added to.
    if (c.axis < 0 || !(amt > 0.0f) || !(rad > 0.0f))
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const float a = clamp01(amt);
    glm::vec3 axis(0.0f);
    axis[c.axis] = 1.0f;  // the bone's OWN local axis: a post-multiply
    return glm::angleAxis(c.sign * rad * a, axis);
}

}  // namespace render
