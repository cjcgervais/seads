// ★ THE TRAILING-CHAIN SOLVER -- see render/trail_chain.h for the contract,
// docs/SCARF_SPEC.md §3 for the algorithm this file reproduces line for line.
//
// PURE: glm + std only. No raylib, no clock, no env, no asset, no allocation.

#include "render/trail_chain.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace render {
namespace {

// Below this a vector has no usable direction. One named epsilon per job so a
// reader can tell "this segment collapsed" from "this axis is degenerate".
constexpr float kLenEps = 1.0e-6f;      // a segment / a chord with no direction
constexpr float kAxisEps = 1.0e-3f;     // a width axis too parallel to the bone
constexpr float kNormalEps = 1.0e-12f;  // squared length of a "zero" normal

int clamp_segments(int s) {
    if (s < 1) return 1;
    if (s > kTrailChainMaxSegments) return kTrailChainMaxSegments;
    return s;
}

// normalize() with a stated fallback -- glm's normalize on a zero vector is
// NaN, and this solver's whole no-NaN claim rests on never calling it blind.
glm::vec3 safe_dir(const glm::vec3& v, const glm::vec3& fallback) {
    const float l = glm::length(v);
    return l > kLenEps ? v / l : fallback;
}

// The keep-out sizes ACTUALLY used this step, both derived from where the
// ANCHOR is (SCARF_SPEC §3, red-team P2-1). A pinned root inside its own
// keep-out makes every constraint pass fight the pin; shrinking the keep-out to
// just clear the root is the fix, and it is a no-op in the shipped rig (anchor
// 212.5 mm from the head centre vs r 200; 67.7 mm behind the torso plane vs
// keep-out 60).
struct KeepOut {
    float head_r = 0.0f;  // <= 0 disables
    bool has_back = false;
    glm::vec3 back_n{0.0f};
    float back_d = 0.0f;
};

KeepOut effective_keepouts(const TrailChainParams& pr,
                           const TrailChainInput& in) {
    KeepOut k;
    k.head_r = pr.head_keepout_r_m;
    if (k.head_r > 0.0f) {
        const float r_eff =
            glm::length(in.anchor - in.head_center) - kTrailChainHeadMarginM;
        if (r_eff < k.head_r) k.head_r = r_eff;
    }
    k.has_back = glm::dot(in.back_normal, in.back_normal) > kNormalEps ||
                 in.n_back_planes > 0;
    if (glm::dot(in.back_normal, in.back_normal) > kNormalEps) {
        k.back_n = glm::normalize(in.back_normal);
        k.back_d = pr.back_keepout_m;
        // ★ THE SAME RULE FOR THE PLANE, and it is an ADDITION to §3 (reported
        // in the rung's handoff): §3 gave the effective-size rule to the sphere
        // only, but §5's own test rig pins the anchor 50 mm behind the neck
        // with a 60 mm keep-out -- the root would sit 10 mm inside its own
        // half-space and "hangs straight down" could not pass by arithmetic.
        // Same principle, same one line; a no-op whenever the rig puts the knot
        // outside, which the shipped rig does.
        const float s_anchor = glm::dot(in.anchor - in.back_origin, k.back_n);
        if (s_anchor < k.back_d) k.back_d = s_anchor;
    }
    return k;
}

// ★ R4a: THE SEAT, AS A SOLID (Chad 2026-08-25, "his legs will hit the seat").
// The THIRD analytic projection -- not a contact solver (§7.6's ban stands, and
// this is the same shape as the sphere and the half-space above it). The caller
// hands over the MEASURED centreline profile; this file knows no asset.
//
// Everything is resolved ONCE per step, outside the particle loop, so the
// projection itself is a handful of compares per particle.
struct Seat {
    bool on = false;
    int n = 0;
    float z_rear = 0.0f, dz = 0.0f;
    float x_half = 0.0f, y_bot = 0.0f, z_front = 0.0f;
    float clear = 0.0f;
    const float* top = nullptr;
};

// Top of the seat at model z. Piecewise linear between samples, CLAMPED at the
// measured extents -- the same total function rider_pose::seat_top_y is, for
// the same reason: a clamp is what stops a chain sampling a surface that was
// never measured.
float seat_top_at(const Seat& s, float z) {
    float t = (z - s.z_rear) / s.dz;
    if (!(t > 0.0f)) t = 0.0f;
    const float tmax = static_cast<float>(s.n - 1);
    if (t > tmax) t = tmax;
    int i = static_cast<int>(t);
    if (i > s.n - 2) i = s.n - 2;
    const float f = t - static_cast<float>(i);
    return s.top[i] + f * (s.top[i + 1] - s.top[i]);
}

bool seat_inside(const Seat& s, const glm::vec3& p) {
    return p.x < s.x_half + s.clear && p.x > -(s.x_half + s.clear) &&
           p.z < s.z_front + s.clear && p.z > s.z_rear - s.clear &&
           p.y > s.y_bot - s.clear && p.y < seat_top_at(s, p.z) + s.clear;
}

Seat effective_seat(const TrailChainParams& pr, const TrailChainInput& in) {
    Seat s;
    // Two samples is the minimum that spans anything; a zero-width or
    // inverted box is a caller error and DISABLES rather than folds inside out.
    if (in.n_seat_samples < 2 || !(in.seat_z_front > in.seat_z_rear) ||
        !(in.seat_x_half > 0.0f))
        return s;
    s.n = in.n_seat_samples < kTrailChainMaxSeatSamples
              ? in.n_seat_samples
              : kTrailChainMaxSeatSamples;
    s.z_rear = in.seat_z_rear;
    s.z_front = in.seat_z_front;
    s.dz = (in.seat_z_front - in.seat_z_rear) / static_cast<float>(s.n - 1);
    s.x_half = in.seat_x_half;
    s.y_bot = in.seat_y_bottom;
    s.clear = pr.seat_keepout_m;
    s.top = in.seat_top_y;
    // ★ THE ANCHOR-PIN RULE, THIRD TIME (SCARF_SPEC §3 red-team P2-1, and the
    // §5 addition above): a root INSIDE its own keep-out makes every constraint
    // pass fight the pin. The sphere and the plane shrink to clear the root;
    // a solid cannot be shrunk in one direction without picking a direction, so
    // the seat DISABLES instead. A no-op in the shipped rig -- the grips are
    // well forward of and above the seat -- and it can only ever LOSE a
    // keep-out, never invent one.
    if (seat_inside(s, in.anchor)) return Seat{};
    s.on = true;
    return s;
}

// The length of link i (1-based: p[i-1] -> p[i]). The scarf's chain is uniform
// and takes the scalar; the R4a body chain hands over a measured table.
float link_len(const TrailChainParams& pr, int i) {
    return (pr.n_seg_len > 0 && i - 1 < pr.n_seg_len) ? pr.seg_len_tbl[i - 1]
                                                      : pr.seg_len_m;
}

// ★ THE CHAIN'S BONE FRAMES: +Y down the segment, +X the seed Gram-Schmidt'd
// against it and carried down the chain, +Z = X x Y.
//
// ★ ONE construction, used by BOTH `trail_chain_frames` (what the draw pass
// and any skinned cloth ride) and the R4a seat keep-out's drawn-surface
// probes. They have to be the same function: a probe offset means what it was
// measured to mean only if the keep-out and the DRAW agree on the frame it is
// carried in, and two copies of a fallback ladder are free to diverge exactly
// where it matters -- on a whipping chain with a near-collapsed segment.
//
// Particle i takes segment i's frame; the tail particle inherits the last
// segment's, which is the rule
// assets/character/sudburian_src/measure_body_chain.py measured the offsets
// under.
//
// ⚠ It READS `st.last_x` (the width fallback) and does not write it. The write
// belongs to `trail_chain_frames`, once per frame: this also runs inside the
// constraint loop, and a keep-out pass must not mutate the ribbon axis the
// draw pass then reads.
void chain_axes(const TrailChainState& st, int n, const glm::vec3& width_axis,
                glm::vec3* ax, glm::vec3* ay, glm::vec3* az) {
    glm::vec3 y_prev(0.0f, -1.0f, 0.0f);
    glm::vec3 x_prev = safe_dir(width_axis, st.last_x);
    for (int i = 0; i < n; ++i) {
        // +Y down the segment. A collapsed segment reuses the previous bone's
        // Y rather than inventing one (SCARF_SPEC §3).
        const glm::vec3 seg = st.p[i + 1] - st.p[i];
        const glm::vec3 y =
            glm::length(seg) > kLenEps ? glm::normalize(seg) : y_prev;
        // +X: the seed, Gram-Schmidt'd against Y. The ladder of fallbacks is
        // §3's, in order, and every one is tried the same way -- so the ONLY
        // way out of this loop is with a unit X perpendicular to Y.
        const glm::vec3 seeds[4] = {x_prev, st.last_x,
                                    glm::cross(y, glm::vec3(0.0f, 1.0f, 0.0f)),
                                    glm::vec3(1.0f, 0.0f, 0.0f)};
        glm::vec3 x(0.0f);
        bool got = false;
        for (int s = 0; s < 4 && !got; ++s) {
            const glm::vec3 c = seeds[s] - y * glm::dot(seeds[s], y);
            if (glm::length(c) >= kAxisEps) {
                x = glm::normalize(c);
                got = true;
            }
        }
        if (!got) {
            // Unreachable for a unit Y: (0,1,0) and (1,0,0) cannot both be
            // parallel to it. Kept so the function has no undefined branch.
            const glm::vec3 c = glm::cross(y, glm::vec3(0.0f, 0.0f, 1.0f));
            x = safe_dir(c, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        ax[i] = x;
        ay[i] = y;
        az[i] = glm::cross(x, y);  // right-handed
        x_prev = x;
        y_prev = y;
    }
    ax[n] = ax[n - 1];
    ay[n] = ay[n - 1];
    az[n] = az[n - 1];
}

// The half-width of the probe box along one WORLD axis: hx|X.e| + hy|Y.e| +
// hz|Z.e|, the box's support function. Exact along the axes (which is all the
// seat's faces ever ask for) and conservative in between.
float box_support(const TrailChainProbe& pb, const glm::vec3& x,
                  const glm::vec3& y, const glm::vec3& z, int axis) {
    return pb.hx_m * std::fabs(x[axis]) + pb.hy_m * std::fabs(y[axis]) +
           pb.hz_m * std::fabs(z[axis]);
}

// The SIX face depths of one point against the seat solid, inflated per axis
// by (rx, ry, rz). All six positive == inside. Written out rather than looped
// so each face keeps its name.
void seat_depths(const Seat& s, const glm::vec3& p, float rx, float ry,
                 float rz, float* d) {
    d[0] = (seat_top_at(s, p.z) + s.clear + ry) - p.y;  // up
    d[1] = p.y - (s.y_bot - s.clear - ry);              // down
    d[2] = (s.x_half + s.clear + rx) - p.x;             // +x
    d[3] = p.x + (s.x_half + s.clear + rx);             // -x
    d[4] = (s.z_front + s.clear + rz) - p.z;            // +z
    d[5] = p.z - (s.z_rear - s.clear - rz);             // -z
}

// ★ ONE DISPLACEMENT THAT CLEARS EVERY PROBE ON THIS PARTICLE, through the
// face that costs least.
//
// ⚠ THIS IS NOT AN OPTIMISATION OF THE PER-PROBE VERSION, IT IS THE FIX FOR A
// DEADLOCK IT HAD. A station carries TWO probes -- his left limb and his right
// -- and near the middle of the machine BOTH can be inside the seat with
// OPPOSITE shallowest faces (+x for one, -x for the other). Projected one at a
// time, each undoes the other every iteration and the man sits in the seat
// with both keep-outs reporting that they fired. Measured before the fix: the
// drawn shoulder 197 mm inside the pan, on a chain whose keep-out was live.
//
// So the faces are scored ONCE for the whole station: face f costs the DEEPEST
// push any inside probe needs through it, and the particle takes the cheapest
// face. A probe that is already outside the solid costs nothing anywhere and
// cannot veto a face.
glm::vec3 seat_escape_station(const Seat& s, const glm::vec3* q,
                              const glm::vec3* r, const bool* live, int n_q,
                              const float* allow) {
    float need[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    bool any = false;
    for (int k = 0; k < n_q; ++k) {
        if (!live[k]) continue;
        float d[6];
        seat_depths(s, q[k], r[k].x, r[k].y, r[k].z, d);
        // ★★★ THE ALLOWANCE -- HE IS SITTING IN IT.
        //
        // render/body_chain.h has said so since the table was measured: "THE
        // BIND POSE IS NOT LEGAL AGAINST THIS KEEP-OUT, AND THAT IS NOT A
        // DEFECT: he is SITTING in it" -- pelvis 52.5 mm inside, thigh 38.6.
        // The keep-out exists to protect SUPERMAN, the state the chain is
        // actually live in, so its radii were deliberately not capped to make
        // sitting legal.
        //
        // What nobody reconciled was the arming rung making that same seated
        // pose the permanent target of the return-to-pose spring. Spring pulls
        // him into the seat, keep-out throws him out, Verlet reads the
        // ejection as speed, forever: measured at 0.49 m in ONE substep and a
        // peak of 1.21 m, which is what Chad saw as legs "going all over".
        //
        // The allowance is the reconciliation, and it is the CHEAP one. Per
        // face, it is how deep THE POSE ITSELF already sits. So at the pose the
        // push is exactly zero and the spring has nothing to fight, and going
        // deeper than the pose still pushes back -- to the pose, not out of the
        // seat. Nobody is relocated. The alternative, projecting the pose out
        // of the solid, moves his pelvis 115 mm and his worst leg station 73 mm
        // and hands Chad a man sitting above his own machine.
        //
        // Null = no allowance, BIT-IDENTICAL to the pre-ruling keep-out (the
        // scarf, and every caller that has no pose).
        if (allow != nullptr)
            for (int f = 0; f < 6; ++f) d[f] -= allow[f];
        bool inside = true;
        for (int f = 0; f < 6; ++f)
            if (!(d[f] > 0.0f)) inside = false;
        if (!inside) continue;
        any = true;
        for (int f = 0; f < 6; ++f)
            if (d[f] > need[f]) need[f] = d[f];
    }
    if (!any) return glm::vec3(0.0f);
    int face = 0;
    for (int f = 1; f < 6; ++f)
        if (need[f] < need[face]) face = f;
    switch (face) {
        case 0:
            return glm::vec3(0.0f, need[0], 0.0f);
        case 1:
            return glm::vec3(0.0f, -need[1], 0.0f);
        case 2:
            return glm::vec3(need[2], 0.0f, 0.0f);
        case 3:
            return glm::vec3(-need[3], 0.0f, 0.0f);
        case 4:
            return glm::vec3(0.0f, 0.0f, need[4]);
        default:
            return glm::vec3(0.0f, 0.0f, -need[5]);
    }
}

// ★★★ THE RULING, IN ONE PLACE. See TrailChainParams::probe_per_limb.
//
// Combined: one face for the whole station, each face priced at the deepest
// push any inside probe needs through it -- correct when the two probes are two
// halves of one body, wrong when they are two limbs straddling the solid,
// because then each limb prices the other's exit at the full width of the seat.
//
// Per-limb: each limb picks its OWN cheapest face, and the two pushes are then
// reconciled COMPONENTWISE:
//
//   * the two want OPPOSITE directions on an axis -> 0. Neither can be given
//     what it asks for; one translation cannot move two limbs apart. THIS is
//     the straddle, and it is the whole reason the ruling exists: the bone
//     between his legs was never the thing inside the seat.
//   * otherwise -> the LARGER of the two. Not the sum (two limbs equally deep
//     would be ejected twice as far as either needs) and NOT THE MEAN, which
//     was the first cut and cost 40 mm of drawn limb: with one limb inside and
//     one clear, the mean gives half the push the inside limb needs, and the
//     drawn leg sinks into the machine by the half that was thrown away.
//     Measured on case 5 -- 4.4 mm of drawn sink became 44.4 mm.
//
// What no station push can express is the case where both limbs need the same
// axis by DIFFERENT amounts. That remainder belongs to the per-side
// reconstruction (render/body_blend.cpp), the only place the two limbs exist as
// separate geometry.
glm::vec3 station_push(const Seat& s, const glm::vec3* q, const glm::vec3* r,
                       const bool* live, bool per_limb, const float* allow) {
    if (!per_limb) return seat_escape_station(s, q, r, live, 2, allow);
    glm::vec3 push[2] = {glm::vec3(0.0f), glm::vec3(0.0f)};
    for (int k = 0; k < 2; ++k) {
        if (!live[k]) continue;
        push[k] = seat_escape_station(s, &q[k], &r[k], &live[k], 1, allow);
    }
    glm::vec3 out(0.0f);
    for (int c = 0; c < 3; ++c) {
        const float a = push[0][c], b = push[1][c];
        if (a * b < 0.0f) continue;  // opposite demands: neither is deliverable
        out[c] = std::fabs(a) >= std::fabs(b) ? a : b;
    }
    return out;
}

// `iterations` passes of {distance root->tail, head sphere, back half-space,
// seat solid}. p[0] is never touched -- the anchor is infinitely heavy.
void apply_constraints(TrailChainState& st, const TrailChainParams& pr, int n,
                       const TrailChainInput& in, const KeepOut& k,
                       const Seat& seat) {
    // ★ THE REST PUSH (2026-09-06, "scarf still bouncing when parked").
    // Verlet reads a bare `p += d` as velocity d/dt NEXT step -- every
    // keep-out projection was a pogo kick with restitution ~1, and at a
    // standstill (parked, or a held crouch) the loop gravity->push->fly->
    // fall->push never dies (measured: tail 10-48 mm/frame, forever; the
    // class accounting named the back-plane pushes as the only injector).
    // The standard inelastic contact: after the projection, cancel the
    // OUTWARD implied normal velocity only -- the approach velocity a
    // resting contact must absorb stays absorbed (a fully velocity-neutral
    // push was tried and measured: it un-floors the contact and gravity
    // creep-slides the chain 2.6 mm/frame at park). Positions -- the drape
    // the eye sees -- are bit-identical either way.
    const auto rest_push = [&](int i, const glm::vec3& d) {
        st.p[i] += d;
        const float dl2 = glm::dot(d, d);
        if (dl2 > kNormalEps) {
            const glm::vec3 u = d / std::sqrt(dl2);
            const float vn = glm::dot(st.p[i] - st.p_prev[i], u);
            if (vn > 0.0f) st.p_prev[i] += u * vn;
        }
    };
    for (int it = 0; it < pr.iterations; ++it) {
        for (int i = 1; i <= n; ++i) {
            const glm::vec3 d = st.p[i] - st.p[i - 1];
            const float l = glm::length(d);
            if (l > kLenEps) st.p[i] = st.p[i - 1] + d * (link_len(pr, i) / l);
        }
        if (k.head_r > 0.0f) {
            for (int i = 1; i <= n; ++i) {
                const glm::vec3 d = st.p[i] - in.head_center;
                const float l = glm::length(d);
                if (l < k.head_r) {
                    // A particle exactly at the centre has no push direction;
                    // "backward" is the one direction that is never into the
                    // body, so use it (or +Y when there is no torso plane).
                    const glm::vec3 dir =
                        l > kLenEps
                            ? d / l
                            : (k.has_back ? k.back_n
                                          : glm::vec3(0.0f, 1.0f, 0.0f));
                    st.p[i] = in.head_center + dir * k.head_r;
                }
            }
        }
        if (k.has_back) {
            // ★ SCARF-DRAPE (2026-09-03): THE DRAWN FABRIC AGAINST THE PLANES,
            // NOT THE CENTRELINE. surf_vtx measured the pods -0.064 m inside
            // the coat at full forward lean while surf_clr (the centreline)
            // read +0.015 the whole time -- the pods are rigid boxes hung off
            // the bone frames, and a frame that pitches puts a pod corner
            // through a surface its own particle clears. That is the exact
            // defect class the R4a seat probes closed, so the same
            // TrailChainProbe table is honoured here: when a station carries a
            // live probe, the plane must clear the BOX (support = hx|X.n| +
            // hy|Y.n| + hz|Z.n| about the box's own centre), and the box
            // contains the centreline so the old test is subsumed. No probe =
            // the centreline test, bit-identical -- and the body chain, which
            // has probes, disables the back planes entirely, so nothing
            // shipped moves until a caller sets BOTH.
            const bool back_probes =
                pr.n_probe_stations > 0 && in.n_back_planes > 0;
            glm::vec3 bx[kTrailChainMaxSegments + 1];
            glm::vec3 by[kTrailChainMaxSegments + 1];
            glm::vec3 bz[kTrailChainMaxSegments + 1];
            if (back_probes) chain_axes(st, n, in.width_axis, bx, by, bz);
            // the slab's lateral axis (see bp_lat_half's banner). Zero when
            // degenerate -- the gate below then never fires and the band is
            // the old half-space.
            glm::vec3 lat_ax(0.0f);
            if (back_probes) {
                const glm::vec3 lx =
                    glm::cross(in.torso_axis, in.back_normal);
                if (glm::length(lx) > 1.0e-4f) lat_ax = glm::normalize(lx);
            }
            for (int i = 1; i <= n; ++i) {
                if (in.n_back_planes > 0) {
                    // banded drawn-back planes: pick the band by the torso
                    // parameter, project out with the SAME anchor-pin rule.
                    const float t =
                        glm::dot(st.p[i] - in.torso_origin, in.torso_axis);
                    int b = 0;
                    while (b + 1 < in.n_back_planes && t > in.bp_t[b + 1]) ++b;
                    const glm::vec3& n = in.bp_normal[b];
                    float d = pr.back_keepout_m;
                    const float s_anch =
                        glm::dot(in.anchor - in.bp_origin[b], n);
                    if (s_anch < d) d = s_anch;
                    // ★★★ THE PIN MAY RELAX THE KEEP-OUT.  IT MAY NOT LICENSE
                    // FABRIC INSIDE THE DRAWN SURFACE.  Chad, 2026-08-24, on
                    // the fifth time of asking: "its still sinking into the
                    // coat."
                    //
                    // s_anch is the ANCHOR's depth measured against THIS
                    // POINT'S band -- not the anchor's own band.  The anchor
                    // sits up at the neck; against a band further DOWN the
                    // back, where the coat stands further out, it reads deeply
                    // negative.  d then went negative and every point in that
                    // band was permitted to sit that far INSIDE the coat.
                    // Measured: surf_clr -0.065 m with the anchor itself a
                    // healthy +0.024 m outside.
                    //
                    // The pin exists so a root authored inside the surface is
                    // not yanked out and kinked. That intent is a FLOOR of
                    // zero, not an unbounded negative: relax the keep-out
                    // toward the surface, never through it.
                    if (d < pr.back_keepout_min_m) d = pr.back_keepout_min_m;
                    if (back_probes && i < pr.n_probe_stations &&
                        (pr.probe[i][0].hx_m > 0.0f ||
                         pr.probe[i][0].hy_m > 0.0f ||
                         pr.probe[i][0].hz_m > 0.0f)) {
                        // The pod's own box against this band's plane AND its
                        // two neighbours: a pod is up to a segment long, so
                        // its corners live in the bands next door, where the
                        // coat can stand tens of millimetres further out than
                        // the particle's own band (the plane->surface pushes
                        // measured 41..83 mm apart). A hunched back is convex
                        // outward, so the neighbour test is the convex-hull
                        // test, not an over-constraint.
                        const TrailChainProbe& pb = pr.probe[i][0];
                        glm::vec3 c = st.p[i] + bx[i] * pb.u_m +
                                      by[i] * pb.v_m + bz[i] * pb.w_m;
                        // ★ THE SLAB ENDS AT THE HEM AND THE COLLAR
                        // (bp_front's banner): a box wholly beyond the
                        // measured t extent (one band's grace either side)
                        // is past the coat and free. Gated on a live lateral
                        // bound so a caller without slab data keeps the old
                        // clamped-band behaviour bit-identical.
                        if (in.bp_lat_half[b] > 0.0f &&
                            in.n_back_planes >= 2) {
                            const float bw_t =
                                (in.bp_t[in.n_back_planes] - in.bp_t[0]) /
                                static_cast<float>(in.n_back_planes);
                            const float sup_t =
                                pb.hx_m * std::fabs(glm::dot(
                                              bx[i], in.torso_axis)) +
                                pb.hy_m * std::fabs(glm::dot(
                                              by[i], in.torso_axis)) +
                                pb.hz_m * std::fabs(glm::dot(
                                              bz[i], in.torso_axis));
                            const float t_c = glm::dot(
                                c - in.torso_origin, in.torso_axis);
                            if (bw_t > 1.0e-4f &&
                                (t_c - sup_t > in.bp_t[in.n_back_planes] +
                                                   bw_t ||
                                 t_c + sup_t < in.bp_t[0] - bw_t))
                                continue;
                        }
                        const int b_lo = b > 0 ? b - 1 : 0;
                        const int b_hi = b + 1 < in.n_back_planes ? b + 1 : b;
                        for (int bb = b_lo; bb <= b_hi; ++bb) {
                            const glm::vec3& nn = in.bp_normal[bb];
                            float dd = pr.back_keepout_m;
                            const float sa =
                                glm::dot(in.anchor - in.bp_origin[bb], nn);
                            if (sa < dd) dd = sa;
                            if (dd < pr.back_keepout_min_m)
                                dd = pr.back_keepout_min_m;
                            const float sup = pb.hx_m * std::fabs(glm::dot(bx[i], nn)) +
                                              pb.hy_m * std::fabs(glm::dot(by[i], nn)) +
                                              pb.hz_m * std::fabs(glm::dot(bz[i], nn));
                            const float sc =
                                glm::dot(c - in.bp_origin[bb], nn) - sup;
                            if (sc < dd) {
                                // ★ THE SLAB GATE (bp_lat_half's banner): a
                                // box beside the torso is not in the coat --
                                // no push; one inside leaves by the CHEAPER
                                // of the back face and the near side face.
                                const float W = in.bp_lat_half[bb];
                                if (W > 0.0f &&
                                    glm::dot(lat_ax, lat_ax) > 0.5f) {
                                    const float sup_l =
                                        pb.hx_m *
                                            std::fabs(glm::dot(bx[i],
                                                               lat_ax)) +
                                        pb.hy_m *
                                            std::fabs(glm::dot(by[i],
                                                               lat_ax)) +
                                        pb.hz_m *
                                            std::fabs(glm::dot(bz[i],
                                                               lat_ax));
                                    const float lat_c = glm::dot(
                                        c - in.back_origin, lat_ax);
                                    const float L =
                                        W + pr.back_keepout_m + sup_l;
                                    const float a2 = std::fabs(lat_c);
                                    if (a2 >= L) continue;  // beside, legal
                                    // the FRONT face (bp_front's banner):
                                    // wholly in front of the chest = free;
                                    // inside = eject via the cheapest of
                                    // back, near side, front.
                                    const float F = in.bp_front[bb];
                                    const float sc_max =
                                        glm::dot(c - in.bp_origin[bb], nn) +
                                        sup;
                                    if (F < 0.0f &&
                                        sc_max <= F - pr.back_keepout_m)
                                        continue;  // hanging clear in front
                                    const float cost_lat = L - a2;
                                    const float cost_back = dd - sc;
                                    const float cost_front =
                                        F < 0.0f ? sc_max -
                                                       (F - pr.back_keepout_m)
                                                 : 1.0e9f;
                                    if (cost_front < cost_back &&
                                        cost_front < cost_lat) {
                                        const glm::vec3 df =
                                            nn * (-cost_front);
                                        rest_push(i, df);
                                        c += df;
                                        continue;
                                    }
                                    // the side exit only when DECISIVELY
                                    // cheaper: at speed a whipping tail near
                                    // the side face dithered between tiny
                                    // lateral ejects and tunnelled to -31 mm
                                    // before self-limiting (measured on the
                                    // 0.6-throttle ramp-up).
                                    if (2.0f * cost_lat < cost_back) {
                                        const glm::vec3 dl =
                                            lat_ax *
                                            ((lat_c >= 0.0f ? 1.0f : -1.0f) *
                                             cost_lat);
                                        rest_push(i, dl);
                                        c += dl;
                                        continue;
                                    }
                                }
                                // The box centre rides the particle: move
                                // both, or the next band's test reads a stale
                                // centre and double-pushes.
                                const glm::vec3 dp = nn * (dd - sc);
                                rest_push(i, dp);
                                c += dp;
                            }
                        }
                        // ★ POD 0 IS STEERED FROM STATION 1 (2026-09-04,
                        // Chad: "at the very end (back) it rolls up into the
                        // coat"). Pod 0 hangs off the PINNED root: p[0]
                        // cannot move, and the anchor stand-off converges a
                        // frame late, so during a continuous pull-back the
                        // knot-side pod dipped -21 mm (measured on a lean
                        // ramp, bone scarf_01 -- exactly the driving
                        // eyeline). But pod 0's ORIENTATION follows segment
                        // 0 = p[1] - p[0], and p[1] is ours to move: lifting
                        // p[1] along the plane normal rotates pod 0 out. The
                        // lever (|c0 - p0| / |p1 - p0|) is under 1 for a pod
                        // on the segment, so pushing by need/0.6 slightly
                        // OVERSHOOTS -- proud is the safe failure -- and the
                        // fixed iteration count rebalances station 1's own
                        // box afterwards.
                        if (i == 1 && (pr.probe[0][0].hx_m > 0.0f ||
                                       pr.probe[0][0].hy_m > 0.0f ||
                                       pr.probe[0][0].hz_m > 0.0f)) {
                            const TrailChainProbe& p0 = pr.probe[0][0];
                            glm::vec3 c0 = st.p[0] + bx[0] * p0.u_m +
                                           by[0] * p0.v_m + bz[0] * p0.w_m;
                            const float t0 = glm::dot(
                                st.p[0] - in.torso_origin, in.torso_axis);
                            int b0 = 0;
                            while (b0 + 1 < in.n_back_planes &&
                                   t0 > in.bp_t[b0 + 1])
                                ++b0;
                            const int c_lo = b0 > 0 ? b0 - 1 : 0;
                            const int c_hi = b0 + 1 < in.n_back_planes
                                                 ? b0 + 1
                                                 : b0;
                            for (int bb = c_lo; bb <= c_hi; ++bb) {
                                const glm::vec3& nn = in.bp_normal[bb];
                                float dd = pr.back_keepout_m;
                                const float sa = glm::dot(
                                    in.anchor - in.bp_origin[bb], nn);
                                if (sa < dd) dd = sa;
                                if (dd < pr.back_keepout_min_m)
                                    dd = pr.back_keepout_min_m;
                                const float sup =
                                    p0.hx_m *
                                        std::fabs(glm::dot(bx[0], nn)) +
                                    p0.hy_m *
                                        std::fabs(glm::dot(by[0], nn)) +
                                    p0.hz_m *
                                        std::fabs(glm::dot(bz[0], nn));
                                const float sc0 =
                                    glm::dot(c0 - in.bp_origin[bb], nn) -
                                    sup;
                                // 3 mm deadband: the anchor stand-off parks
                                // pod 0 EXACTLY at the keep-out, so a
                                // zero-deadband trigger chatters on float
                                // noise there and the /0.6 overshoot floats
                                // station 1 proud (caught by gate case 23's
                                // upper bound). The dips this steering
                                // exists for are 20 mm.
                                if (sc0 >= dd - 0.003f) continue;
                                // the slab gates, same as everywhere.
                                const float W = in.bp_lat_half[bb];
                                if (W > 0.0f &&
                                    glm::dot(lat_ax, lat_ax) > 0.5f) {
                                    const float sup_l =
                                        p0.hx_m * std::fabs(glm::dot(
                                                      bx[0], lat_ax)) +
                                        p0.hy_m * std::fabs(glm::dot(
                                                      by[0], lat_ax)) +
                                        p0.hz_m * std::fabs(glm::dot(
                                                      bz[0], lat_ax));
                                    if (std::fabs(glm::dot(
                                            c0 - in.back_origin, lat_ax)) >=
                                        W + pr.back_keepout_m + sup_l)
                                        continue;
                                    const float F = in.bp_front[bb];
                                    if (F < 0.0f &&
                                        glm::dot(c0 - in.bp_origin[bb], nn) +
                                                sup <=
                                            F - pr.back_keepout_m)
                                        continue;
                                }
                                const glm::vec3 dp1 =
                                    nn * ((dd - sc0) / 0.6f);
                                // rest_push, not a bare +=: the /0.6
                                // overshoot is a >1 gain, and as a pogo kick
                                // it fired in 66 mm bursts at park (the
                                // relaxation oscillator the rest_push banner
                                // describes, at its loudest).
                                rest_push(1, dp1);
                                c0 += dp1 * 0.6f;
                            }
                        }
                        continue;
                    }
                    const float s = glm::dot(st.p[i] - in.bp_origin[b], n);
                    if (s < d) rest_push(i, n * (d - s));
                } else {
                    const float s =
                        glm::dot(st.p[i] - in.back_origin, k.back_n);
                    if (s < k.back_d)
                        rest_push(i, k.back_n * (k.back_d - s));
                }
            }
        }
        if (seat.on) {
            // ★ R4a: WHICH SURFACE IS GRADED. With no probe table this is the
            // CENTRELINE, exactly as it shipped -- the scarf, and every test
            // written before the body chain. With one, the centreline is NOT
            // graded at all: the particle is a bone, the DRAWN limbs are the
            // two probes hung off it, and a bone is allowed to pass wherever
            // its own geometry is not. That is the whole point (see
            // TrailChainProbe).
            if (pr.n_probe_stations > 0) {
                glm::vec3 ax[kTrailChainMaxSegments + 1];
                glm::vec3 ay[kTrailChainMaxSegments + 1];
                glm::vec3 az[kTrailChainMaxSegments + 1];
                chain_axes(st, n, in.width_axis, ax, ay, az);
                for (int i = 1; i <= n; ++i) {
                    if (i >= pr.n_probe_stations) continue;
                    glm::vec3 q[2], r[2];
                    bool live[2];
                    for (int s = 0; s < 2; ++s) {
                        const TrailChainProbe& pb = pr.probe[i][s];
                        live[s] =
                            pb.hx_m > 0.0f || pb.hy_m > 0.0f || pb.hz_m > 0.0f;
                        q[s] = st.p[i] + ax[i] * pb.u_m + ay[i] * pb.v_m +
                               az[i] * pb.w_m;
                        r[s] =
                            glm::vec3(box_support(pb, ax[i], ay[i], az[i], 0),
                                      box_support(pb, ax[i], ay[i], az[i], 1),
                                      box_support(pb, ax[i], ay[i], az[i], 2));
                    }
                    // The probes ride RIGIDLY on the particle, so whatever
                    // moves the drawn limbs out moves the bone with them.
                    st.p[i] += station_push(
                        seat, q, r, live, pr.probe_per_limb[i],
                        pr.n_seat_allow > i ? pr.seat_allow[i] : nullptr);
                }
            } else {
                const glm::vec3 zero(0.0f);
                const bool live = true;
                for (int i = 1; i <= n; ++i)
                    st.p[i] += seat_escape_station(
                        seat, &st.p[i], &zero, &live, 1,
                        pr.n_seat_allow > i ? pr.seat_allow[i] : nullptr);
            }
        }
    }
}

}  // namespace

void trail_chain_reset(TrailChainState& st, const TrailChainParams& pr,
                       const glm::vec3& anchor, const glm::vec3& dir) {
    st.n = clamp_segments(pr.segments);
    const glm::vec3 d = safe_dir(dir, glm::vec3(0.0f, -1.0f, 0.0f));
    st.p[0] = anchor;
    for (int i = 1; i <= st.n; ++i) st.p[i] = st.p[i - 1] + d * link_len(pr, i);
    for (int i = 0; i <= st.n; ++i) st.p_prev[i] = st.p[i];
    st.primed = true;
    st.needs_solve =
        true;  // the next step legalises it -- see trail_chain_step
}

// ★ CONSTANT WORK, BY CONSTRUCTION (SCARF_SPEC §5 item 11, which is why there
// is no timing test): every loop bound below is either a compile-time constant
// or a params field (`segments`, `iterations`). NO bound depends on the state,
// the wind, the dt or the geometry; there is NO convergence test, NO tolerance
// loop and NO early exit anywhere in this function. The cost of a step is the
// same on the worst frame of the worst crash as it is at rest. That is the
// §7.6 promise ("fixed segment count, fixed iterations, fixed dt") and it is
// what makes this safe to re-anchor onto the whole body in R4.
void trail_chain_step(TrailChainState& st, const TrailChainParams& pr,
                      const TrailChainInput& in) {
    const int n = clamp_segments(pr.segments);
    // First sight, a segment-count change, or a teleport: re-prime straight
    // along gravity rather than integrate a 1 m jump into the tail.
    if (!st.primed || st.n != n ||
        glm::length(in.anchor - st.p[0]) > kTrailChainTeleportM) {
        trail_chain_reset(st, pr, in.anchor, in.gravity_dir);
    }
    st.p[0] = in.anchor;  // pin; the anchor is infinitely heavy
    st.p_prev[0] = in.anchor;
    const KeepOut keep = effective_keepouts(pr, in);
    const Seat seat = effective_seat(pr, in);

    // ★ A RE-PRIME COSTS ONE SUB-STEP, AND IT MUST (measured, 2026-08-19).
    // A straight-down prime can start DEEP inside the torso half-space -- with
    // the §3b plane the tail began 248 mm inside it -- and a Verlet integrator
    // reads a 248 mm positional correction as 30 m/s of velocity. The chain
    // then rocketed over the top and parked, dead still, straight UP the spine:
    // the plane's other fixed point, 145 deg of lift, and it never came down
    // (perfectly balanced inverted pendulum). So a prime is SOLVED and then
    // frozen (p_prev = p, zero velocity) instead of integrated: the chain
    // starts legal and at rest, and gravity takes it from there. Costs one
    // sub-step at bind and at a teleport, which nothing can see.
    if (st.needs_solve) {
        st.needs_solve = false;
        apply_constraints(st, pr, n, in, keep, seat);
        for (int i = 0; i <= n; ++i) st.p_prev[i] = st.p[i];
        return;
    }
    const float dt = pr.dt_s;
    if (!(dt > 0.0f)) return;  // a 0-tick frame poses, it does not advance

    // -- Verlet predict: quadratic drag toward the relative wind, gravity, damp
    const glm::vec3 g_step = in.gravity_dir * (pr.gravity_mps2 * dt);
    for (int i = 1; i <= n; ++i) {
        glm::vec3 v = (st.p[i] - st.p_prev[i]) / dt;
        const glm::vec3 w = in.wind_mps - v;
        const float wl = glm::length(w);
        float f = pr.drag_k_per_m * wl * dt;
        if (f > 1.0f) f = 1.0f;  // never overshoot past the wind => stable
        v += w * f;
        v += g_step;
        // ★★★ R4a: THE FOUR PSEUDO-FORCE TERMS THIS FRAME IS NOT INERTIAL BY.
        // See the TrailChainInput block. Applied UNCONDITIONALLY -- no branch,
        // no data-dependent bound, so §7.6's constant-work promise and the
        // banner above this function stay true verbatim; and with the default
        // zero field every cross product is exactly (0,0,0), so `v += 0 * dt`
        // leaves v bit-identical and the shipped scarf does not move.
        //
        // ORDER MATTERS AND IS THE RIG'S: drag, then the uniform field, THEN
        // these -- so the Coriolis term sees the same `v` the measurement rig
        // fed it (tools/sled_probe.cpp, `step4`). Reordering would not be
        // wrong physics, but it would stop the 31-tape measurement describing
        // the code that shipped.
        const glm::vec3 r = st.p[i] - in.frame_origin;
        const glm::vec3 a_frame =
            -in.frame_accel_mps2 - glm::cross(in.frame_alpha_rps2, r) -
            glm::cross(in.frame_omega_rps, glm::cross(in.frame_omega_rps, r)) -
            2.0f * glm::cross(in.frame_omega_rps, v);
        v += a_frame * dt;
        // ★★★ R4a THE RETURN-TO-POSE SPRING (Chad's "hold his shape, trail
        // from it", 2026-08-27). Critically damped about the target: with
        // w = 2*pi*f the acceleration is  w^2*(target - p) - 2*w*v, which has
        // exactly zero overshoot, so the term that removes a flail can never
        // add a wobble of its own.
        //
        // ★ GUARDED SO ZERO IS BIT-IDENTICAL, and the guard is on the DIAL
        // and the COUNT, not on a distance: a distance test would make the
        // work data-dependent, and §7.6's constant-work promise is the reason
        // this file is allowed to exist at all.
        //
        // ⚠ STABILITY IS EXPLICIT, NOT HOPED FOR. An explicit critically-
        // damped spring integrated at dt is stable while 2*w*dt < 2, i.e.
        // f < 1/(2*pi*dt) -- 19.1 Hz at the sim's 1/120 s. The stiffness is
        // CLAMPED to half of that, so no dial Chad can turn (nor a caller
        // stepping at a coarser dt) can blow the chain up. A dial that can
        // explode is not a dial.
        if (pr.pose_stiff_hz > 0.0f && i < in.n_pose) {
            float f_hz = pr.pose_stiff_hz;
            const float f_max = 0.25f / (3.14159265f * dt);
            if (f_hz > f_max) f_hz = f_max;
            const float w = 6.2831853f * f_hz;
            v += (w * w * (in.pose_p[i] - st.p[i]) - 2.0f * w * v) * dt;
        }
        v *= pr.damping;
        st.p_prev[i] = st.p[i];
        st.p[i] += v * dt;
    }
    apply_constraints(st, pr, n, in, keep, seat);
}

TrailFrameField trail_frame_field(TrailFrameTracker& tr,
                                  const glm::vec3& vel_frame,
                                  const glm::vec3& omega_frame, float dt_s) {
    TrailFrameField f;
    f.omega_rps = omega_frame;
    // No previous sample, or a 0-tick frame: report the field with NO
    // derivative rather than inventing one. A first frame that differentiates
    // against a default-constructed zero reads a cruising sled as hundreds of
    // g.
    if (tr.primed && dt_s > 0.0f) {
        const float inv = 1.0f / dt_s;
        f.accel_mps2 = (vel_frame - tr.vel_prev) * inv;
        f.alpha_rps2 = (omega_frame - tr.omega_prev) * inv;
    }
    tr.vel_prev = vel_frame;
    tr.omega_prev = omega_frame;
    tr.primed = true;
    return f;
}

void trail_chain_frames(TrailChainState& st, const glm::vec3& width_axis,
                        glm::mat4* out) {
    if (out == nullptr || st.n <= 0) return;
    // ★ ONE frame construction, shared with the R4a seat keep-out's
    // drawn-surface probes -- see `chain_axes` for why they must be the same
    // function. (Unifying them did NOT move any measured number; it removes a
    // divergence that was free to open later, not one that had.)
    glm::vec3 ax[kTrailChainMaxSegments + 1];
    glm::vec3 ay[kTrailChainMaxSegments + 1];
    glm::vec3 az[kTrailChainMaxSegments + 1];
    chain_axes(st, st.n, width_axis, ax, ay, az);
    for (int i = 0; i < st.n; ++i) {
        glm::mat4 m(1.0f);
        m[0] = glm::vec4(ax[i], 0.0f);
        m[1] = glm::vec4(ay[i], 0.0f);
        m[2] = glm::vec4(az[i], 0.0f);
        m[3] = glm::vec4(st.p[i], 1.0f);
        out[i] = m;
    }
    st.last_x = ax[st.n - 1];  // the fallback the NEXT frame starts from
}

// ★ SCARF-DRAPE: THE ANCHOR DEPTH POD 0 NEEDS. Bone 0's pod hangs off the
// PINNED root, which the constraint loop never grades (i starts at 1 and p[0]
// cannot move) -- the anchor pin is the only thing that places it. This
// computes the depth the anchor must be stood off to, along ITS OWN band's
// normal, so that pod 0's box clears every band the box overlaps in t. Shared
// by both call sites and BOTH chains (the red-team's finding: the first cut
// built it inline for the long chain only, and the short tail -- which is
// exactly "distal to the knots" -- kept the defect). Axes come from the
// caller's current chain state (last frame's solve; it converges in a frame
// or two, and the safe failure is proud). Returns pr.back_keepout_m when
// there is nothing to add (no planes, no probe, unprimed chain).
float trail_chain_anchor_need(const TrailChainState& st,
                              const TrailChainParams& pr,
                              const TrailChainInput& in) {
    float keep_a = pr.back_keepout_m;
    if (in.n_back_planes <= 0 || pr.n_probe_stations <= 0 || !st.primed ||
        st.n <= 0)
        return keep_a;
    const TrailChainProbe& p0 = pr.probe[0][0];
    if (p0.hx_m <= 0.0f && p0.hy_m <= 0.0f && p0.hz_m <= 0.0f) return keep_a;
    glm::vec3 bx[kTrailChainMaxSegments + 1];
    glm::vec3 by[kTrailChainMaxSegments + 1];
    glm::vec3 bz[kTrailChainMaxSegments + 1];
    chain_axes(st, st.n, in.width_axis, bx, by, bz);
    const float ta = glm::dot(in.anchor - in.torso_origin, in.torso_axis);
    int ba = 0;
    while (ba + 1 < in.n_back_planes && ta > in.bp_t[ba + 1]) ++ba;
    const glm::vec3& n_ba = in.bp_normal[ba];
    const float s_anch = glm::dot(in.anchor - in.bp_origin[ba], n_ba);
    const glm::vec3 c0 =
        in.anchor + bx[0] * p0.u_m + by[0] * p0.v_m + bz[0] * p0.w_m;
    const float t_c0 = glm::dot(c0 - in.torso_origin, in.torso_axis);
    const float t_ext =
        p0.hx_m * std::fabs(glm::dot(bx[0], in.torso_axis)) +
        p0.hy_m * std::fabs(glm::dot(by[0], in.torso_axis)) +
        p0.hz_m * std::fabs(glm::dot(bz[0], in.torso_axis));
    for (int bb = 0; bb < in.n_back_planes; ++bb) {
        // band edges are clamped at the ends, same as the solver's band pick
        // (and bp_t[n_back_planes] may not be written yet at the call site).
        const float t_bot = bb > 0 ? in.bp_t[bb] : -1.0e9f;
        const float t_top =
            bb + 1 < in.n_back_planes ? in.bp_t[bb + 1] : 1.0e9f;
        if (t_c0 + t_ext < t_bot || t_c0 - t_ext > t_top) continue;
        const glm::vec3& nb = in.bp_normal[bb];
        // A push along the anchor band's normal buys depth against band bb at
        // rate dot(nb, n_ba). Near-orthogonal or opposed, the push cannot buy
        // it -- SKIP rather than divide by a floor: the floored division
        // amplified a violation the push could never fix into a 5x stand-off
        // (red-team finding 3), a bob with no benefit.
        const float g = glm::dot(nb, n_ba);
        if (g < 0.2f) continue;
        // the slab gate: a pod-0 box laterally beside band bb's coat is not
        // in it, and must not inflate the stand-off (bp_lat_half's banner).
        if (in.bp_lat_half[bb] > 0.0f) {
            const glm::vec3 lx = glm::cross(in.torso_axis, in.back_normal);
            if (glm::length(lx) > 1.0e-4f) {
                const glm::vec3 la = glm::normalize(lx);
                const float sup_l = p0.hx_m * std::fabs(glm::dot(bx[0], la)) +
                                    p0.hy_m * std::fabs(glm::dot(by[0], la)) +
                                    p0.hz_m * std::fabs(glm::dot(bz[0], la));
                if (std::fabs(glm::dot(c0 - in.back_origin, la)) >=
                    in.bp_lat_half[bb] + pr.back_keepout_m + sup_l)
                    continue;
            }
        }
        const float supb = p0.hx_m * std::fabs(glm::dot(bx[0], nb)) +
                           p0.hy_m * std::fabs(glm::dot(by[0], nb)) +
                           p0.hz_m * std::fabs(glm::dot(bz[0], nb));
        const float sb = glm::dot(c0 - in.bp_origin[bb], nb) - supb;
        const float viol = pr.back_keepout_m - sb;
        if (viol <= 0.0f) continue;
        const float need = s_anch + viol / g;
        if (need > keep_a) keep_a = need;
    }
    return keep_a;
}

// ★ SCARF-DRAPE: THE PRESENTATION WAVE GETS THE SAME LAW AS THE SOLVER.
// The flutter wave displaces a COPY of the solved chain and the physics never
// sees it -- by design. But a displacement the constraint pass never sees is
// licensed to put fabric anywhere, and at 11.7 m/s it put the tail pod
// -0.027 m inside the coat (measured, SEADS_SCARF_DEBUG=2). This clamp
// re-applies ONLY the banded-plane pod projection to the displaced copy --
// the same band pick, the same anchor-pin relax with floor, the same box
// support, the same neighbour-band rule as the constraint loop above. IF THAT
// BLOCK CHANGES, THIS CHANGES WITH IT. It handles only the banded+probe case
// because only a caller with pods and planes has a wave to clamp; every other
// caller is bit-untouched by construction (the wrapper returns immediately).
void trail_chain_present_clamp(TrailChainState& st,
                               const TrailChainParams& pr,
                               const TrailChainInput& in) {
    const int n = st.n;
    if (n <= 0 || in.n_back_planes <= 0 || pr.n_probe_stations <= 0) return;
    glm::vec3 bx[kTrailChainMaxSegments + 1];
    glm::vec3 by[kTrailChainMaxSegments + 1];
    glm::vec3 bz[kTrailChainMaxSegments + 1];
    glm::vec3 lat_ax(0.0f);
    {
        const glm::vec3 lx = glm::cross(in.torso_axis, in.back_normal);
        if (glm::length(lx) > 1.0e-4f) lat_ax = glm::normalize(lx);
    }
    // TWO passes, fixed (constant work): one pass left the whipping tail's
    // box -7..-11 mm under-resolved against the neighbour bands (measured on
    // the 0.6-throttle drive) -- each push can re-violate the band next door,
    // and the axes swing as the particles move. Two converges it.
    for (int pass = 0; pass < 2; ++pass) {
    chain_axes(st, n, in.width_axis, bx, by, bz);
    for (int i = 1; i <= n; ++i) {
        if (i >= pr.n_probe_stations) continue;
        const TrailChainProbe& pb = pr.probe[i][0];
        if (pb.hx_m <= 0.0f && pb.hy_m <= 0.0f && pb.hz_m <= 0.0f) continue;
        const float t = glm::dot(st.p[i] - in.torso_origin, in.torso_axis);
        int b = 0;
        while (b + 1 < in.n_back_planes && t > in.bp_t[b + 1]) ++b;
        glm::vec3 c =
            st.p[i] + bx[i] * pb.u_m + by[i] * pb.v_m + bz[i] * pb.w_m;
        // the slab's t extent, same as the constraint loop's.
        if (in.bp_lat_half[b] > 0.0f && in.n_back_planes >= 2) {
            const float bw_t =
                (in.bp_t[in.n_back_planes] - in.bp_t[0]) /
                static_cast<float>(in.n_back_planes);
            const float sup_t =
                pb.hx_m * std::fabs(glm::dot(bx[i], in.torso_axis)) +
                pb.hy_m * std::fabs(glm::dot(by[i], in.torso_axis)) +
                pb.hz_m * std::fabs(glm::dot(bz[i], in.torso_axis));
            const float t_c =
                glm::dot(c - in.torso_origin, in.torso_axis);
            if (bw_t > 1.0e-4f &&
                (t_c - sup_t > in.bp_t[in.n_back_planes] + bw_t ||
                 t_c + sup_t < in.bp_t[0] - bw_t))
                continue;
        }
        const int b_lo = b > 0 ? b - 1 : 0;
        const int b_hi = b + 1 < in.n_back_planes ? b + 1 : b;
        for (int bb = b_lo; bb <= b_hi; ++bb) {
            const glm::vec3& nn = in.bp_normal[bb];
            float dd = pr.back_keepout_m;
            const float sa = glm::dot(in.anchor - in.bp_origin[bb], nn);
            if (sa < dd) dd = sa;
            if (dd < pr.back_keepout_min_m) dd = pr.back_keepout_min_m;
            const float sup = pb.hx_m * std::fabs(glm::dot(bx[i], nn)) +
                              pb.hy_m * std::fabs(glm::dot(by[i], nn)) +
                              pb.hz_m * std::fabs(glm::dot(bz[i], nn));
            const float sc = glm::dot(c - in.bp_origin[bb], nn) - sup;
            if (sc < dd) {
                // the slab gate, same as the constraint loop's.
                const float W = in.bp_lat_half[bb];
                if (W > 0.0f && glm::dot(lat_ax, lat_ax) > 0.5f) {
                    const float sup_l =
                        pb.hx_m * std::fabs(glm::dot(bx[i], lat_ax)) +
                        pb.hy_m * std::fabs(glm::dot(by[i], lat_ax)) +
                        pb.hz_m * std::fabs(glm::dot(bz[i], lat_ax));
                    const float lat_c = glm::dot(c - in.back_origin, lat_ax);
                    const float L = W + pr.back_keepout_m + sup_l;
                    const float a2 = std::fabs(lat_c);
                    if (a2 >= L) continue;
                    const float F = in.bp_front[bb];
                    const float sc_max =
                        glm::dot(c - in.bp_origin[bb], nn) + sup;
                    if (F < 0.0f && sc_max <= F - pr.back_keepout_m)
                        continue;
                    // The DRAWN copy always leaves by the back face: the
                    // gates above already free everything genuinely beside,
                    // in front of, or past the coat, and a presentation
                    // clamp that dithers sideways can tunnel with the wave
                    // (measured -31 mm on the throttle ramp). The physics
                    // keeps its cheapest-face exits; the picture takes the
                    // guaranteed one.
                }
                const glm::vec3 dp = nn * (dd - sc);
                st.p[i] += dp;
                c += dp;
            }
        }
    }
    }  // pass
}

float trail_chain_lift_rad(const TrailChainState& st,
                           const glm::vec3& gravity_dir) {
    if (st.n <= 0) return 0.0f;
    const glm::vec3 chord = st.p[st.n] - st.p[0];
    const float cl = glm::length(chord);
    const float gl = glm::length(gravity_dir);
    if (cl <= kLenEps || gl <= kLenEps) return 0.0f;
    float c = glm::dot(chord / cl, gravity_dir / gl);
    if (c > 1.0f) c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    return std::acos(c);
}

// ★★★ THE SEAT SOLID, EXPOSED. See the header. Nothing in the shipped chain
// calls this -- it is additive, and the solver's own path is byte-untouched.
glm::vec3 trail_seat_escape(const TrailChainParams& pr,
                            const TrailChainInput& in, const glm::vec3& p,
                            const glm::vec3& half) {
    const Seat seat = effective_seat(pr, in);
    const bool live = true;
    return seat_escape_station(seat, &p, &half, &live, 1, nullptr);
}

bool trail_seat_depths(const TrailChainParams& pr, const TrailChainInput& in,
                       const glm::vec3& p, const glm::vec3& half, float* d6) {
    const Seat seat = effective_seat(pr, in);
    if (!seat.on) return false;
    seat_depths(seat, p, half.x, half.y, half.z, d6);
    return true;
}

glm::vec3 trail_seat_station_push(const TrailChainParams& pr,
                                  const TrailChainInput& in,
                                  const glm::vec3 q[2], const glm::vec3 r[2],
                                  const bool live[2], bool per_limb) {
    return station_push(effective_seat(pr, in), q, r, live, per_limb, nullptr);
}

}  // namespace render
