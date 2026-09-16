#include "render/body_drive.h"

#include <algorithm>
#include <cmath>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

namespace render {

void body_chain_seat_allowance(TrailChainParams& pr, const TrailChainInput& in,
                               const glm::vec3* pose, int n,
                               const glm::vec3& width_axis) {
    pr.n_seat_allow = 0;
    if (pose == nullptr || n <= 0 || pr.n_probe_stations <= 0) return;
    const int n_st = n + 1;

    // The frames come from `trail_chain_frames` on a state holding THE POSE --
    // the probes are evaluated in the station frame, so an allowance measured
    // in any other frame would describe a body the constraint pass never sees.
    TrailChainState fs;
    fs.n = n;
    for (int i = 0; i < n_st; ++i) {
        fs.p[i] = pose[i];
        fs.p_prev[i] = pose[i];
    }
    glm::mat4 fr[kTrailChainMaxSegments + 1];
    trail_chain_frames(fs, width_axis, fr);

    for (int i = 0; i < n_st; ++i)
        for (int f = 0; f < 6; ++f) pr.seat_allow[i][f] = 0.0f;

    for (int i = 1; i < n_st; ++i) {
        if (i >= pr.n_probe_stations) continue;
        // Frame i is the frame of LINK i; `trail_chain_frames` writes [0, n),
        // and the last station borrows its neighbour's -- the same index the
        // constraint pass and the debug draw use, so the picture, the grading
        // and the allowance cannot disagree about which frame a station is in.
        const glm::mat3 R(fr[i < n ? i : n - 1]);
        const glm::vec3 ax = R[0], ay = R[1], az = R[2];
        for (int s = 0; s < 2; ++s) {
            const TrailChainProbe& pb = pr.probe[i][s];
            if (!(pb.hx_m > 0.0f || pb.hy_m > 0.0f || pb.hz_m > 0.0f)) continue;
            const glm::vec3 q =
                pose[i] + ax * pb.u_m + ay * pb.v_m + az * pb.w_m;
            const glm::vec3 half(
                pb.hx_m * std::fabs(ax.x) + pb.hy_m * std::fabs(ay.x) +
                    pb.hz_m * std::fabs(az.x),
                pb.hx_m * std::fabs(ax.y) + pb.hy_m * std::fabs(ay.y) +
                    pb.hz_m * std::fabs(az.y),
                pb.hx_m * std::fabs(ax.z) + pb.hy_m * std::fabs(ay.z) +
                    pb.hz_m * std::fabs(az.z));
            float d[6];
            if (!trail_seat_depths(pr, in, q, half, d)) return;  // seat off
            // Only a probe that is INSIDE contributes an allowance. A probe
            // already clear of the solid is allowed nothing, which is what
            // keeps the allowance from quietly opening the keep-out for a limb
            // that never needed it.
            bool inside = true;
            for (int f = 0; f < 6; ++f)
                if (!(d[f] > 0.0f)) inside = false;
            if (!inside) continue;
            for (int f = 0; f < 6; ++f)
                if (d[f] > pr.seat_allow[i][f]) pr.seat_allow[i][f] = d[f];
        }
    }
    pr.n_seat_allow = n_st;
}

BodyDriveOut body_chain_drive(TrailChainState& st, TrailChainParams& pr,
                              const BodyDriveIn& in) {
    BodyDriveOut out;
    if (in.pin == nullptr || in.n <= 0) return out;

    // ---- PINNED ---------------------------------------------------------
    // The chain IS the drawn man, frozen (p_prev = p) so the release starts
    // from rest relative to him and not from a velocity the pin invented.
    //
    // ★ The test is `arm <= 0`, EXACTLY, and it stays exact on purpose. What
    // made it a latch before was not the comparison but the signal: an
    // exponential low-pass decaying toward a raw 0 never reaches 0, so one
    // throttle blip held the release open for the rest of the drive. That is
    // fixed where it lives, in `rider_load_lp_step`, and not by an epsilon
    // here -- an epsilon on a continuous weight is the disease, not the cure.
    if (in.arm <= 0.0f || !st.primed) {
        st.n = in.n;
        for (int i = 0; i <= in.n; ++i) {
            st.p[i] = in.pin[i];
            st.p_prev[i] = in.pin[i];
        }
        st.primed = true;
        st.needs_solve = false;
        out.pinned = true;
        return out;
    }

    // ---- RELEASED -------------------------------------------------------
    TrailChainInput ci = in.chain_in;
    ci.anchor = in.pin[0];

    // ★★★ THE POSE IS MADE LEGAL BY ALLOWANCE, NOT BY RELOCATION. The seated
    // pin is inside the seat keep-out by measurement and by design (he is
    // sitting in it), so aiming the spring at it made the spring and the
    // constraint pass permanent adversaries. The first cut of this fix
    // PROJECTED the pin out of the solid instead -- and measured what that
    // costs: his pelvis moves 115 mm and his worst leg station 73 mm, i.e. a
    // man sitting above his own machine. The allowance buys the same peace for
    // nothing: at the pose the push is exactly zero, and going deeper than the
    // pose still pushes back to the pose.
    body_chain_seat_allowance(pr, ci, in.pin, in.n, ci.width_axis);
    ci.n_pose = in.n + 1;
    for (int i = 0; i <= in.n; ++i) ci.pose_p[i] = in.pin[i];
    for (int i = 0; i <= in.n; ++i)
        for (int f = 0; f < 6; ++f)
            if (pr.seat_allow[i][f] > out.allow_max_m)
                out.allow_max_m = pr.seat_allow[i][f];

    // ★★★ THE ARMING MEMORY DIVIDES THE PULL-BACK. See BodyBuckMemory's banner
    // for the measurement that forced it: without this the spring returns him
    // at a fixed rate no matter how hard he was hit, so sustain collapses with
    // AIR TIME and is flat against BUCK -- the exact inverse of the ruling.
    // With `buck_gain` 0 the divisor is exactly 1.0f and this line is
    // bit-identical to the expression that shipped.
    out.buck_divisor = in.buck_mem != nullptr
                           ? body_buck_step(*in.buck_mem, in.buck, in.g_eff_mag,
                                            in.dt_s)
                           : 1.0f;
    pr.pose_stiff_hz =
        in.pose_hz_base / std::max(in.arm, 1.0e-3f) / out.buck_divisor;
    out.pose_stiff_hz = pr.pose_stiff_hz;
    out.buck_mem = in.buck_mem != nullptr ? in.buck_mem->mem : 0.0f;

    if (in.dt_s > 0.0f)
        for (int s = 0; s < in.substeps; ++s) trail_chain_step(st, pr, ci);
    return out;
}

}  // namespace render
