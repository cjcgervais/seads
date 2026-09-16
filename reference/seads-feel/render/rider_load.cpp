#include "render/rider_load.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace render {

glm::dvec3 rider_load_to_body(const glm::dvec3& m, double drop) {
    return glm::dvec3(-m.x, m.y - drop, -m.z);
}

RiderLoad rider_load_solve(const RiderLoadModel& M, const glm::dvec3& a_body,
                           const glm::dvec3& omega, const glm::dvec3& alpha,
                           const glm::dvec3& g_body, const glm::dvec3& d) {
    RiderLoad L;
    // r_G = seated offset + (1 - rider_frac) * d   (sections 1.1 / 3)
    const glm::dvec3 r_G = M.cg_body + (1.0 - M.rider_frac) * d;
    const glm::dvec3 a_G = a_body + glm::cross(alpha, r_G) +
                           glm::cross(omega, glm::cross(omega, r_G));
    L.A = a_G - g_body;
    L.alpha_x = alpha.x;

    // Arms: station - rider CG. Station = nominal - rider_frac*d and
    // r_G = cg + (1 - rider_frac)*d, so the difference is nominal - cg - d.
    const double dz_h = M.grip_body.z - M.cg_body.z - d.z;
    const double dy_h = M.grip_body.y - M.cg_body.y - d.y;
    const double dz_s = M.seat_body.z - M.cg_body.z - d.z;
    const double dz_b = M.boot_body.z - M.cg_body.z - d.z;

    L.H_z = M.m_r * L.A.z;  // equation (1), always
    L.H_x = M.m_r * L.A.x;  // the same statement, sideways (see H_x above)
    const double V = M.m_r * L.A.y;
    const double K = M.I_pitch * alpha.x - dy_h * L.H_z;
    L.V_dem = V;
    L.W_supp = -(K + dz_h * V);

    auto case_free = [&]() {
        L.rcase = kRiderFree;
        L.N_s = L.N_b = L.N_m = 0.0;
        L.H_y = V;
        L.M_h = M.I_pitch * alpha.x - (dy_h * L.H_z - dz_h * L.H_y);
        L.z_m_defined = false;
    };

    // ★ CASE F is "NOTHING BELOW CARRIES", which is NOT `V <= 0`. Moment
    // balance about the rider CG (0 = K + dz_h*H_y + z_m*N_m) with the
    // vertical N_m = V - H_y gives the pinned-wrist family
    //
    //     N_m(z_m) = W / (z_m - dz_h),      W = -(K + dz_h * V)
    //
    // and EVERY support station is aft of the grips (dz_h = -0.626 is the
    // forward-most), so z_m - dz_h > 0 and N_m >= 0 <=> W >= 0 -- for EITHER
    // sign of V. The old `V <= 0` test released seat and boards on the sign of
    // the RESULTANT vertical demand, which is a proxy, and it discarded real
    // solutions where the hands haul DOWN on the bar while the seat presses up
    // -- every crest and every fall-away. It dumped the balance into the wrist
    // couple M_h, which grip_n does not contain, and under-reported by up to
    // 572 N with a jump of |K|/0.854 at V = 0.
    //
    // Below zero the family is a continuum (unlike V > 0, where CASE S pins
    // z_m = -K/V). Report the SEAT member: it is the minimal |H_y| = N_m + |V|
    // because it has the longest arm (0.854 m vs the boots' 0.167 m), which is
    // section 4.3's own closure -- the kinematically REQUIRED load, never
    // self-stress -- and it is the unique continuous extension of the V -> 0+
    // limit, where zm = -K/V -> +inf already lands in CASE A.
    if (V <= 0.0) {
        const double W = -(K + dz_h * V);
        if (W < 0.0) {
            case_free();
        } else {
            L.rcase = kRiderSeatEdge;
            L.H_y = (K + dz_s * V) / (dz_s - dz_h);
            L.N_s = V - L.H_y;  // == W / (dz_s - dz_h) >= 0
            L.N_b = 0.0;
            L.N_m = L.N_s;
            L.z_m = dz_s;
            L.z_m_defined = true;
        }
    } else {
        const double zm = -K / V;  // from -(z_m - z_G) V = K
        const double span = dz_s - dz_b;
        if (zm >= dz_b && zm <= dz_s) {
            L.rcase = kRiderSeated;
            L.H_y = 0.0;
            L.N_m = V;
            L.z_m = zm;
            L.z_m_defined = true;
            L.N_s = V * (zm - dz_b) / span;
            L.N_b = V * (dz_s - zm) / span;
        } else if (zm > dz_s) {
            L.rcase = kRiderSeatEdge;
            L.H_y = (K + dz_s * V) / (dz_s - dz_h);
            L.N_s = V - L.H_y;
            L.N_b = 0.0;
            L.N_m = L.N_s;
            L.z_m = dz_s;
            L.z_m_defined = true;
        } else {
            L.H_y = (K + dz_b * V) / (dz_b - dz_h);
            L.N_b = V - L.H_y;
            if (L.N_b < 0.0) {
                case_free();
            } else {
                L.rcase = kRiderBoardEdge;
                L.N_s = 0.0;
                L.N_m = L.N_b;
                L.z_m = dz_b;
                L.z_m_defined = true;
            }
        }
    }
    L.z_m_body = L.z_m + r_G.z;
    L.grip_n = std::hypot(L.H_z, L.H_y);
    // Reported ALONGSIDE grip_n, not folded into it: folding would move every
    // published percentile in one step, and that is Chad's call to make with
    // the difference in front of him, not a change smuggled in as a fix.
    L.grip3_n = std::sqrt(L.H_x * L.H_x + L.H_y * L.H_y + L.H_z * L.H_z);
    const double w = M.m_r * kRiderLoadG;
    L.seat_frac = L.N_s / w;
    L.board_frac = L.N_b / w;
    return L;
}

// The house low-pass, the same shape sim/sled.cpp:87 `slew_toward` gives
// SledState::hull_engage_lp: tau = 0.1 s, rate_cap = 1e9 (the filter is the
// only shaping), and h is the SUBSTEP, never dt (section 5.3). Re-stated here
// because slew_toward is file-static in sim/sled.cpp; the values are quoted
// from that call site, and the mutation leg for it is "use dt instead of h".
double rider_load_lp_step(double cur, double target, double h) {
    const double tau = 0.1, rate_cap = 1e9;
    double step = (target - cur) * (1.0 - std::exp(-h / tau));
    step = std::clamp(step, -rate_cap * h, rate_cap * h);
    const double next = cur + step;
    // ★★★ AN EXPONENTIAL NEVER ARRIVES, AND SOMETHING DOWNSTREAM WAS WAITING
    // FOR IT TO. This filter feeds the stage weight, and the chain's pin/
    // release branch asks `arm <= 0`. Decaying toward a raw 0 from 0.05, the
    // published value stays strictly positive for order ten seconds -- longer
    // than any quiet stretch in a drive -- so ONE throttle blip left the chain
    // released for the rest of the ride. Chad saw the result as legs "going
    // all over erratically"; the LP tail is why the state he was in never
    // ended.
    //
    // ⚠ THIS IS A NUMERIC NOISE FLOOR, NOT A FEEL THRESHOLD, and the
    // distinction is the whole reason it is allowed to exist. It fires ONLY
    // when the target is exactly zero and the remaining value is 1e-4 -- a
    // hundredth of a percent of a [0, 1] weight, below anything a body could
    // express and far below the resolution of every dial that reads it. A
    // threshold on a NONZERO target would be a latch, and this program has
    // paid for that four times.
    if (target == 0.0 && std::fabs(next) < kRiderLoadLpZeroFloor) return 0.0;
    return next;
}

const char* rider_case_name(int c) {
    switch (c) {
        case kRiderSeated:
            return "S";
        case kRiderSeatEdge:
            return "A";
        case kRiderBoardEdge:
            return "B";
        default:
            return "F";
    }
}

// ★★★ ONE SUPPORT'S RELEASE -- the single expression both stages read.
//
// R4a rung 2 needs stage 2 (LADDER 7.3: "foot load -> 0, boots leave the
// running boards") on its OWN signal, not on the product. Writing
// `1 - board/ref` a second time at that call site would be a FORK of a
// measured model, and this program has paid for that seven times: an
// expression that describes the shipped behaviour stops describing it the
// moment either copy moves. So the term is factored out HERE and
// `rider_stage_arm` is now its product -- provably the same arithmetic, in one
// place, with one degenerate guard.
//
// A support with no resting share has no ladder to come off it; normalising by
// ~0 would release on the first frame.
// ★ RETURNS DOUBLE, and that is load-bearing, not a style choice: the
// original computed `us * ub` in double and cast ONCE. Casting each term to
// float first and multiplying floats is DIFFERENT ARITHMETIC and would move
// the shipped weight in the last bits -- a "pure refactor" that is not one.
double rider_support_release(double frac, double ref) {
    if (!(ref > 1.0e-6)) return 0.0;
    return 1.0 - std::min(1.0, std::max(0.0, frac) / ref);
}

double rider_support_release_free(double frac, double ref, double free_frac) {
    const double f = std::min(1.0, std::max(0.0, free_frac));
    if (!(f > 0.0)) return 0.0;
    // Same reading as the stage weight's degenerate branch, and the same
    // reason: "no support is possible at this attitude" is TOTAL release off
    // the ground, not zero release. On the ground `f` is 0 and this is inert.
    if (!(ref > 1.0e-6)) return f;
    return f * rider_support_release(frac, ref);
}

double rider_air_free_frac(double air_s, double grace_s) {
    const double a = air_s > 0.0 ? air_s : 0.0;
    // No window is not the same statement as no freedom. See the header.
    if (!(grace_s > 0.0)) return a > 0.0 ? 1.0 : 0.0;
    return std::min(1.0, a / grace_s);
}

// See the header for the ruling, the two logs and the measured table. The
// ratio below is UNTOUCHED and still carries its own tests; this composes it.
float rider_stage_arm_free(double seat_frac, double board_frac, double seat_ref,
                           double board_ref, double free_frac) {
    const double f = std::min(1.0, std::max(0.0, free_frac));
    if (!(f > 0.0)) return 0.0f;
    // A degenerate reference means the zero-accel solve found NO support
    // possible at this attitude. On the ground `f` is 0 and this never runs
    // (his STAND ruling); off it, "no support possible" IS total freedom.
    const bool degenerate = !(seat_ref > 1.0e-6) || !(board_ref > 1.0e-6);
    const double base =
        degenerate ? 1.0
                   : static_cast<double>(rider_stage_arm(seat_frac, board_frac,
                                                         seat_ref, board_ref));
    return static_cast<float>(f * base);
}

float rider_stage_arm(double seat_frac, double board_frac, double seat_ref,
                      double board_ref) {
    // The guard is per-support and must stay ALL-or-nothing here: a degenerate
    // reference on EITHER support zeroes the arm, which is the behaviour
    // `stagesel`'s ladder and gate case 8 were measured against.
    if (!(seat_ref > 1.0e-6) || !(board_ref > 1.0e-6)) return 0.0f;
    const double us = rider_support_release(seat_frac, seat_ref);
    const double ub = rider_support_release(board_frac, board_ref);
    return static_cast<float>(us * ub);
}

}  // namespace render
