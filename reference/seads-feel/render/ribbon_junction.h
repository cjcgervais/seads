#pragma once
// ★ ROAD-REPAIR / F2 -- THE JUNCTION CUT.
//
// Chad, 2026-09-12: "alot of eyesores in the intersections and corners of
// roads, there is alot of weird cut angles and verticies, Also z flashing is
// happening all over."
//
// E1 (docs/road_repair/onaping_eyesores.md §3.1) answered the structural
// question and the answer was: THERE IS NO JUNCTION CUT ANYWHERE IN THE ROAD
// STACK. render/ribbons.cpp drapes each baked way as its own independent quad
// strip and nothing clips one deck against another. Measured on the baked
// drape: 14,076 overlapping drawn segment pairs, 4,646 road-road intersection
// sites, 9,368 junction leg pairs with deck-on-deck overlap p50 11.1 m / p90
// 20.3 m out from the node. So the square-cut END of one way's deck stands
// across its neighbour's pavement (the "weird cut angles and verticies"), and
// two near-coplanar decks share one glPolygonOffset(-2,-4) (the "z flashing").
// Rung 8's subdivision did not draw that -- it dropped the deck off its 53 m
// floating chord and stopped HIDING it.
//
// THE CUT, and it is one idea: at a node where N >= 3 drawn ways END, trim
// every leg's deck back to a junction boundary and emit ONE cap polygon over
// the node. The crossing becomes a SINGLE surface instead of N overlapping
// ones. Two decks that no longer share area cannot z-fight, and a deck end
// that stops at the cap boundary is not standing on anybody's pavement.
//
// ★ THE SEAM IS THE WHOLE POINT. A cut that trades z-fighting for CRACKS is
// worse than the defect -- terrain showing through a road is a bigger eyesore
// than a flicker. So the cap's boundary vertices are not "near" the trimmed
// leg ends, they are computed by THE SAME FUNCTION at THE SAME parameter and
// placed by THE SAME radius_fn + lift: `ribbon_rung_at` below is called by the
// drape builder (render/ribbon_subdiv.h) and by the cap builder, and the only
// float that exists is the one both of them produce. The seam gap is
// identically zero by construction, not small by tuning -- and a ctest leg
// grades exactly that.
//
// ★ AND THE CAP IS SUBDIVIDED, for the reason rung 8 exists. A junction cap
// is by nature a BIGGER flat polygon than a subdivided road quad, so it is
// precisely the shape that re-opens the Onaping sink (a 53 m flat chord floated
// up to +7.2 m above drive_r). The cap is therefore a RADIALLY RINGED fan:
// `max_seg_m` rings out from the node and `max_tr_m` splits around the corner
// spans -- the deck's own two dials, so the cap can never be coarser than the
// road that runs into it.
//
// PURE: glm + std + the baked GIS table + render/ribbon_subdiv.h.
// ZERO raylib, ZERO app, ZERO world/.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "render/ribbon_subdiv.h"
#include "render/sudbury_gis.gen.h"

namespace render {

// How near two way-endpoints must be to be the SAME node (m). E1 clustered at
// 3 m; 5 m is used here because the bake rounds the shared vertex and a 2 m
// miss must not split one crossing into two half-cut ones. It is NOT a dial:
// it is a property of the bake, and it is pinned by a ctest leg.
inline constexpr double kJunctionNodeTolM = 5.0;

// The most of its own drawn length one way may lose to the caps at its two
// ends, PER END. A short connector between two junctions must not be eaten
// whole -- that would open a HOLE between two caps, which is the crack this
// rung exists to avoid. At 0.40 per end a way always keeps >= 20 % of itself
// and the two ends can never cross (along-length is monotone in t). Legs
// clamped by this are counted in the ledger and reported as residual.
inline constexpr double kJunctionMaxConsumeFrac = 0.40;

// ★ The shared rung evaluator `ribbon_rung_at` and the per-quad `RibbonQuadTrim`
// live in render/ribbon_subdiv.h: the DRAPE builder has to call the same
// function this file's cap builder calls, and a header can only be included one
// way round.

// How many of its two triangles each baked quad keeps in `kept`. Computed ONCE
// per path: the per-quad question is asked thousands of times (once per
// junction leg) and answering it by rescanning a 154,170-index list each time
// is a billion-op build.
inline std::vector<unsigned char> ribbon_quad_tri_counts(
    const std::vector<unsigned short>& kept, int nrungs) {
    std::vector<unsigned char> n(
        static_cast<std::size_t>(nrungs > 1 ? nrungs - 1 : 1), 0);
    for (std::size_t k = 0; k + 2 < kept.size(); k += 3) {
        const int p0 = kept[k] / 2, p1 = kept[k + 1] / 2, p2 = kept[k + 2] / 2;
        const int lo = std::min(p0, std::min(p1, p2));
        const int hi = std::max(p0, std::max(p1, p2));
        if (hi != lo + 1 || lo < 0 ||
            static_cast<std::size_t>(lo) >= n.size())
            continue;
        if (n[static_cast<std::size_t>(lo)] < 255)
            ++n[static_cast<std::size_t>(lo)];
    }
    return n;
}

// The column count build_ribbon_batches will give quad `m`. A quad that lost a
// triangle to the T24 excavation clip owns only half its area along a diagonal
// and the batch builder forces it to ONE span for exactly that reason -- the
// cap edge meeting it has to agree, or the seam is off by a column.
inline int ribbon_quad_columns(const GisRibbonPath& P,
                               const std::vector<unsigned char>& tri, int m,
                               double R, double max_tr_m) {
    if (static_cast<std::size_t>(m) >= tri.size() ||
        tri[static_cast<std::size_t>(m)] < 2)
        return 1;
    const GisRibbonVertex& VL0 = kSudburyRibbonVerts[P.vtx_off + 2 * m];
    const GisRibbonVertex& VR0 = kSudburyRibbonVerts[P.vtx_off + 2 * m + 1];
    const GisRibbonVertex& VL1 = kSudburyRibbonVerts[P.vtx_off + 2 * m + 2];
    const GisRibbonVertex& VR1 = kSudburyRibbonVerts[P.vtx_off + 2 * m + 3];
    const glm::dvec3 dL0(VL0.dir[0], VL0.dir[1], VL0.dir[2]);
    const glm::dvec3 dR0(VR0.dir[0], VR0.dir[1], VR0.dir[2]);
    const glm::dvec3 dL1(VL1.dir[0], VL1.dir[1], VL1.dir[2]);
    const glm::dvec3 dR1(VR1.dir[0], VR1.dir[1], VR1.dir[2]);
    return std::max(ribbon_tr_splits(dL0, dR0, R, max_tr_m),
                    ribbon_tr_splits(dL1, dR1, R, max_tr_m));
}

// ---------------------------------------------------------------------------
// THE PLAN
// ---------------------------------------------------------------------------

// One junction cap, as DIRECTIONS -- the radius_fn/lift placement happens in
// the batch builder, so the cap is draped by exactly the drape's own rule.
struct JunctionCapCPU {
    glm::dvec3 centre{0.0};        // the node
    std::vector<glm::dvec3> ring;  // CCW boundary directions
    std::vector<float> ring_s;     // the arc-length each boundary point
                                   // inherits from the leg it meets (the
                                   // asphalt mottle is a function of s alone,
                                   // so an s-discontinuity at the seam would
                                   // draw a tone step exactly where the crack
                                   // would have been)
    // TRUE where a boundary point is a LEG's own column vertex -- the seam
    // proper. FALSE on the corner-span points between two legs, which belong
    // to no leg by construction (they span the corner lobe of the crossing)
    // and must not be graded as a seam: their nearest leg vertex is metres
    // away and that is the shape of an intersection, not a crack.
    std::vector<unsigned char> ring_on_leg;
    float centre_s = 0.0f;
    int rings = 1;  // radial subdivision (the sag defence)
    double radius_m = 0.0;
    int legs = 0;
};

// Everything F2 decides, plus the ledger that makes the decision gradeable.
struct JunctionPlan {
    // [path index][quad index] -- an empty inner vector means "this path is
    // untouched", which is the common case.
    std::vector<std::vector<RibbonQuadTrim> > trim;
    std::vector<JunctionCapCPU> caps;
    // --- the ledger (the reason the dial is CHOSEN, not guessed) ---
    int ways = 0, endpoints = 0;
    int nodes_total = 0;          // endpoint clusters of any degree
    int nodes_deg1 = 0;           // dead ends -- never cut
    int nodes_deg2 = 0;           // colinear continuations -- never cut
    int nodes_deg3plus = 0;       // the F2 population
    int nodes_cut = 0;
    int nodes_skipped_trail = 0;  // a node with a snowmobile-trail leg
    int nodes_skipped_short = 0;  // a leg whose way never reaches the boundary
    int legs_cut = 0;
    int legs_clamped = 0;  // the 40 %-of-length clamp bit
    int quads_dropped = 0;
    double cap_area_m2 = 0.0;
    // ★ RULING 2's instrument (F1 interaction). The part of the cap footprint
    // that lies outside EVERY leg's own corridor half-width -- i.e. the corner
    // lobes of the crossing, the only new asphalt F2 paints where no leg
    // corridor runs. F1 (`[bank_mesh] deck_yield_m`) demotes a bank station
    // inside a drawn deck using world::LineNetwork half-widths, and the
    // LineNetwork is built from the BAKED vertices, which F2 does not touch --
    // so F1's reach is unchanged and this number is exactly the area where a
    // bank could still be drawn on top of a junction cap.
    double cap_area_uncovered_m2 = 0.0;
    double cut_radius_min_m = 0.0, cut_radius_max_m = 0.0;
    double road_len_removed_m = 0.0;
    const std::vector<RibbonQuadTrim>* trim_for(std::size_t pi) const {
        if (pi >= trim.size() || trim[pi].empty()) return nullptr;
        return &trim[pi];
    }
};

namespace junction_detail {

inline int uf_find(std::vector<int>& p, int a) {
    while (p[a] != a) {
        p[a] = p[p[a]];
        a = p[a];
    }
    return a;
}
inline void uf_union(std::vector<int>& p, int a, int b) {
    const int ra = uf_find(p, a), rb = uf_find(p, b);
    if (ra != rb) p[rb] = ra;
}

struct WayRun {
    int path;
    int m0;
    int m1;
    double len_m;
    double half_w_m;
};
struct EndPt {
    int way;
    int end;         // 0 = the m0 side, 1 = the m1+1 side
    glm::dvec3 dir;  // its drawn centreline direction
    glm::dvec3 out;  // unit tangent INTO the way (away from the node)
};
struct LegCut {
    int ep;
    int quad;
    double t;
    int nt;
    bool low_end;
};

inline double arc_between(const glm::dvec3& a, const glm::dvec3& b, double R) {
    return R * std::acos(std::min(1.0, std::max(-1.0, glm::dot(a, b))));
}

}  // namespace junction_detail

// Build the junction plan.
//
// `kept` is parallel to kSudburyRibbonPaths: the T24-clipped LOCAL index list
// of each path, or an empty vector for a path that draws nothing (rivers,
// fully-excavated paths). `junction_cut_m <= 0` is THE IDENTITY BY BRANCH --
// it returns an empty plan, nothing is measured and nothing is allocated.
inline JunctionPlan build_junction_plan(
    const std::vector<std::vector<unsigned short> >& kept, double R,
    double junction_cut_m, double max_seg_m, double max_tr_m,
    double node_tol_m = kJunctionNodeTolM,
    double max_consume_frac = kJunctionMaxConsumeFrac) {
    using namespace junction_detail;
    JunctionPlan plan;
    if (!(junction_cut_m > 0.0)) return plan;  // ★ THE IDENTITY BRANCH
    if (!(R > 0.0)) return plan;

    const std::size_t npaths = std::min(kept.size(), kSudburyRibbonPathCount);
    std::vector<std::vector<unsigned char> > tri(npaths);

    // --- 1. the drawn WAYS: maximal runs of consecutive drawn quads ---------
    std::vector<WayRun> ways;
    std::vector<EndPt> eps;
    for (std::size_t pi = 0; pi < npaths; ++pi) {
        if (kept[pi].empty()) continue;
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.kind == 3) continue;  // waterways are not roads
        const int nrungs = P.vtx_count / 2;
        if (nrungs < 2) continue;
        tri[pi] = ribbon_quad_tri_counts(kept[pi], nrungs);
        const std::vector<int> segs = ribbon_drawn_segments(kept[pi]);
        if (segs.empty()) continue;
        std::size_t a = 0;
        while (a < segs.size()) {
            std::size_t b = a;
            while (b + 1 < segs.size() && segs[b + 1] == segs[b] + 1) ++b;
            const int m0 = segs[a], m1 = segs[b];
            double len = 0.0;
            std::vector<double> hw;
            glm::dvec3 prev(0.0);
            for (int r = m0; r <= m1 + 1; ++r) {
                const int qm = (r <= m1) ? r : m1;
                const double qt = (r <= m1) ? 0.0 : 1.0;
                glm::dvec3 dL, dR;
                float s0, s1, v0, v1;
                ribbon_rung_at(P, qm, qt, &dL, &dR, &s0, &s1, &v0, &v1);
                const glm::dvec3 c = glm::normalize(dL + dR);
                if (r > m0) len += arc_between(prev, c, R);
                prev = c;
                hw.push_back(
                    R * std::asin(std::min(1.0, 0.5 * glm::length(dL - dR))));
            }
            std::nth_element(hw.begin(), hw.begin() + hw.size() / 2, hw.end());
            const int wi = static_cast<int>(ways.size());
            ways.push_back(
                WayRun{static_cast<int>(pi), m0, m1, len, hw[hw.size() / 2]});
            for (int end = 0; end < 2; ++end) {
                const int rm = (end == 0) ? m0 : m1;
                const double rt = (end == 0) ? 0.0 : 1.0;
                const glm::dvec3 c0 = ribbon_centre_at(P, rm, rt);
                const glm::dvec3 c1 =
                    ribbon_centre_at(P, rm, end == 0 ? 1.0 : 0.0);
                glm::dvec3 t = c1 - c0 * glm::dot(c0, c1);
                const double tl = glm::length(t);
                t = (tl > 1e-12) ? t / tl : glm::dvec3(0.0);
                eps.push_back(EndPt{wi, end, c0, t});
            }
            a = b + 1;
        }
    }
    plan.ways = static_cast<int>(ways.size());
    plan.endpoints = static_cast<int>(eps.size());
    if (eps.empty()) return plan;

    // --- 2. cluster the endpoints into NODES (a hash grid + union-find) -----
    const double cell = node_tol_m / R;
    const double cos_tol = std::cos(node_tol_m / R);
    auto key_off = [&](const glm::dvec3& d, int dx, int dy, int dz) {
        const long long i = static_cast<long long>(std::floor(d.x / cell)) + dx;
        const long long j = static_cast<long long>(std::floor(d.y / cell)) + dy;
        const long long k = static_cast<long long>(std::floor(d.z / cell)) + dz;
        return static_cast<std::size_t>((i * 73856093LL) ^ (j * 19349663LL) ^
                                        (k * 83492791LL));
    };
    std::map<std::size_t, std::vector<int> > grid;
    for (int i = 0; i < static_cast<int>(eps.size()); ++i)
        grid[key_off(eps[i].dir, 0, 0, 0)].push_back(i);
    std::vector<int> par(eps.size());
    for (std::size_t i = 0; i < par.size(); ++i) par[i] = static_cast<int>(i);
    for (int i = 0; i < static_cast<int>(eps.size()); ++i)
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dz = -1; dz <= 1; ++dz) {
                    std::map<std::size_t, std::vector<int> >::const_iterator
                        it = grid.find(key_off(eps[i].dir, dx, dy, dz));
                    if (it == grid.end()) continue;
                    for (int j : it->second)
                        if (j > i &&
                            glm::dot(eps[i].dir, eps[j].dir) >= cos_tol)
                            uf_union(par, i, j);
                }
    std::map<int, std::vector<int> > nodes;
    for (int i = 0; i < static_cast<int>(eps.size()); ++i)
        nodes[uf_find(par, i)].push_back(i);
    plan.nodes_total = static_cast<int>(nodes.size());

    // --- 3. trim + cap, node by node ---------------------------------------
    plan.trim.assign(kSudburyRibbonPathCount, std::vector<RibbonQuadTrim>());
    auto trims_for = [&](int pi) -> std::vector<RibbonQuadTrim>& {
        std::vector<RibbonQuadTrim>& t =
            plan.trim[static_cast<std::size_t>(pi)];
        if (t.empty()) {
            const int nr = kSudburyRibbonPaths[pi].vtx_count / 2;
            t.assign(static_cast<std::size_t>(nr > 1 ? nr - 1 : 1),
                     RibbonQuadTrim());
        }
        return t;
    };

    for (std::map<int, std::vector<int> >::const_iterator nit = nodes.begin();
         nit != nodes.end(); ++nit) {
        const std::vector<int>& leg_eps = nit->second;
        const int deg = static_cast<int>(leg_eps.size());
        if (deg == 1) {
            ++plan.nodes_deg1;
            continue;
        }
        if (deg == 2) {
            ++plan.nodes_deg2;  // ★ a colinear continuation is NOT a junction
            continue;
        }
        ++plan.nodes_deg3plus;
        bool has_trail = false;
        for (int e : leg_eps)
            if (kSudburyRibbonPaths[ways[eps[e].way].path].kind == 2)
                has_trail = true;
        if (has_trail) {
            ++plan.nodes_skipped_trail;
            continue;
        }

        glm::dvec3 ctr(0.0);
        double max_hw = 0.0;
        for (int e : leg_eps) {
            ctr += eps[e].dir;
            max_hw = std::max(max_hw, ways[eps[e].way].half_w_m);
        }
        if (glm::length(ctr) < 1e-9) continue;
        ctr = glm::normalize(ctr);
        // A cut INSIDE the widest deck's own half-width cannot separate the
        // legs, so the dial is FLOORED there -- it is a radius, not an offset.
        const double Rc = std::max(junction_cut_m, max_hw);

        // --- walk each leg out to Rc (or to its clamp) ---------------------
        std::vector<LegCut> cuts;
        bool ok = true;
        for (int e : leg_eps) {
            const EndPt& E = eps[e];
            const WayRun& W = ways[E.way];
            const GisRibbonPath& P = kSudburyRibbonPaths[W.path];
            const double allow = max_consume_frac * W.len_m;
            double along = 0.0;
            bool found = false;
            LegCut lc{e, 0, 0.0, 1, E.end == 0};
            glm::dvec3 prev = E.dir;
            const int step = (E.end == 0) ? +1 : -1;
            for (int q = (E.end == 0) ? W.m0 : W.m1;
                 q >= W.m0 && q <= W.m1 && !found; q += step) {
                const double ta = (E.end == 0) ? 0.0 : 1.0;
                const double tb = (E.end == 0) ? 1.0 : 0.0;
                const glm::dvec3 far_c = ribbon_centre_at(P, q, tb);
                const double d_far = arc_between(ctr, far_c, R);
                const double along_far = along + arc_between(prev, far_c, R);
                if (d_far < Rc && along_far < allow) {
                    along = along_far;
                    prev = far_c;
                    continue;  // the whole quad is inside the junction
                }
                double lo = ta, hi = tb;
                for (int it = 0; it < 30; ++it) {
                    const double mid = 0.5 * (lo + hi);
                    const glm::dvec3 c = ribbon_centre_at(P, q, mid);
                    if (arc_between(ctr, c, R) >= Rc ||
                        along + arc_between(prev, c, R) >= allow)
                        hi = mid;
                    else
                        lo = mid;
                }
                const glm::dvec3 c = ribbon_centre_at(P, q, hi);
                if (along + arc_between(prev, c, R) >= allow - 1e-6 &&
                    arc_between(ctr, c, R) < Rc - 1e-6)
                    ++plan.legs_clamped;
                lc.quad = q;
                lc.t = hi;
                lc.nt = ribbon_quad_columns(P, tri[static_cast<std::size_t>(
                                                   W.path)],
                                            q, R, max_tr_m);
                found = true;
            }
            if (!found) {
                ok = false;  // the way ends before the boundary: leave it alone
                break;
            }
            cuts.push_back(lc);
        }
        if (!ok || static_cast<int>(cuts.size()) != deg) {
            ++plan.nodes_skipped_short;
            continue;
        }

        // A way whose two ends land in the SAME quad would fold on itself. The
        // 0.40 clamp makes that unreachable (along-length is monotone in t), so
        // this is a belt-and-braces refusal, not a case that is expected.
        bool clash = false;
        for (std::size_t i = 0; i < cuts.size() && !clash; ++i)
            for (std::size_t j = i + 1; j < cuts.size(); ++j)
                if (eps[cuts[i].ep].way == eps[cuts[j].ep].way &&
                    cuts[i].quad == cuts[j].quad)
                    clash = true;
        if (clash) continue;

        // --- the cap boundary, legs in CCW bearing order -------------------
        // (e1, e2) span the tangent plane at the node, so a leg's bearing is an
        // atan2 in that plane and "left of the leg" is cross(ctr, out).
        glm::dvec3 e1 = glm::dvec3(0.0, 0.0, 1.0) -
                        ctr * glm::dot(ctr, glm::dvec3(0.0, 0.0, 1.0));
        if (glm::length(e1) < 1e-6)
            e1 = glm::dvec3(1.0, 0.0, 0.0) -
                 ctr * glm::dot(ctr, glm::dvec3(1.0, 0.0, 0.0));
        e1 = glm::normalize(e1);
        const glm::dvec3 e2 = glm::cross(ctr, e1);
        std::vector<std::pair<double, std::size_t> > order;
        for (std::size_t i = 0; i < cuts.size(); ++i) {
            const glm::dvec3& t = eps[cuts[i].ep].out;
            order.push_back(std::make_pair(
                std::atan2(glm::dot(t, e2), glm::dot(t, e1)), i));
        }
        std::sort(order.begin(), order.end());

        JunctionCapCPU cap;
        cap.centre = ctr;
        cap.radius_m = Rc;
        cap.legs = deg;
        cap.rings = (max_seg_m > 0.0)
                        ? std::max(1, std::min(kRibbonSplitCap,
                                               static_cast<int>(
                                                   std::ceil(Rc / max_seg_m))))
                        : 1;
        double s_sum = 0.0;
        std::vector<int> leg_nt;
        for (std::size_t oi = 0; oi < order.size(); ++oi) {
            const LegCut& lc = cuts[order[oi].second];
            const EndPt& E = eps[lc.ep];
            const GisRibbonPath& P = kSudburyRibbonPaths[ways[E.way].path];
            glm::dvec3 dL, dR;
            float sL, sR, vL, vR;
            ribbon_rung_at(P, lc.quad, lc.t, &dL, &dR, &sL, &sR, &vL, &vR);
            const glm::dvec3 left = glm::cross(ctr, E.out);
            const bool dL_is_left = glm::dot(dL - dR, left) > 0.0;
            // Walk the leg's OWN column vertices from its right edge to its
            // left edge: the cap boundary crosses the leg end exactly where
            // the leg's terminal rung is, vertex for vertex.
            for (int k = 0; k <= lc.nt; ++k) {
                const int col = dL_is_left ? (lc.nt - k) : k;
                const double u = static_cast<double>(col) / lc.nt;
                cap.ring.push_back(ribbon_lat_dir(dL, dR, u));
                cap.ring_on_leg.push_back(1);
                const float sk = sL + (sR - sL) * static_cast<float>(u);
                cap.ring_s.push_back(sk);
                s_sum += sk;
            }
            leg_nt.push_back(lc.nt);
        }
        // The CORNER spans between consecutive legs, split by max_tr_m: a 24 m
        // chord across the corner of a wide crossing must not float either.
        {
            std::vector<glm::dvec3> ring2;
            std::vector<float> s2;
            std::vector<unsigned char> on2;
            const std::size_t n = cap.ring.size();
            std::size_t at = 0;
            for (std::size_t oi = 0; oi < leg_nt.size(); ++oi) {
                for (int k = 0; k <= leg_nt[oi]; ++k) {
                    ring2.push_back(cap.ring[at]);
                    s2.push_back(cap.ring_s[at]);
                    on2.push_back(1);
                    ++at;
                }
                const std::size_t nxt = at % n;
                const glm::dvec3 A = cap.ring[at - 1], B = cap.ring[nxt];
                int ns = 1;
                if (max_tr_m > 0.0) {
                    const double arc = arc_between(A, B, R);
                    ns = std::min(
                        kRibbonSplitCap,
                        std::max(1, static_cast<int>(std::ceil(arc / max_tr_m))));
                }
                for (int k = 1; k < ns; ++k) {
                    const double f = static_cast<double>(k) / ns;
                    const glm::dvec3 M = A + (B - A) * f;
                    if (glm::length(M) < 1e-9) continue;
                    ring2.push_back(glm::normalize(M));
                    on2.push_back(0);
                    s2.push_back(cap.ring_s[at - 1] +
                                 (cap.ring_s[nxt] - cap.ring_s[at - 1]) *
                                     static_cast<float>(f));
                }
            }
            cap.ring.swap(ring2);
            cap.ring_s.swap(s2);
            cap.ring_on_leg.swap(on2);
        }
        if (cap.ring.size() < 3) continue;
        cap.centre_s = static_cast<float>(
            s_sum / static_cast<double>(std::max<std::size_t>(1, deg)));

        // --- commit: the trims, then the cap ------------------------------
        for (std::size_t i = 0; i < cuts.size(); ++i) {
            const LegCut& lc = cuts[i];
            const WayRun& W = ways[eps[lc.ep].way];
            std::vector<RibbonQuadTrim>& T = trims_for(W.path);
            if (lc.low_end) {
                for (int q = W.m0; q < lc.quad; ++q)
                    if (!T[static_cast<std::size_t>(q)].drop) {
                        T[static_cast<std::size_t>(q)].drop = true;
                        ++plan.quads_dropped;
                    }
                RibbonQuadTrim& t = T[static_cast<std::size_t>(lc.quad)];
                t.t_lo = std::max(t.t_lo, lc.t);
                if (t.t_lo >= t.t_hi && !t.drop) {
                    t.drop = true;
                    ++plan.quads_dropped;
                }
            } else {
                for (int q = W.m1; q > lc.quad; --q)
                    if (!T[static_cast<std::size_t>(q)].drop) {
                        T[static_cast<std::size_t>(q)].drop = true;
                        ++plan.quads_dropped;
                    }
                RibbonQuadTrim& t = T[static_cast<std::size_t>(lc.quad)];
                t.t_hi = std::min(t.t_hi, lc.t);
                if (t.t_lo >= t.t_hi && !t.drop) {
                    t.drop = true;
                    ++plan.quads_dropped;
                }
            }
            ++plan.legs_cut;
        }
        double area = 0.0;
        for (std::size_t i = 0; i < cap.ring.size(); ++i) {
            const glm::dvec3 A = cap.ring[i];
            const glm::dvec3 B = cap.ring[(i + 1) % cap.ring.size()];
            const glm::dvec2 a(glm::dot(A - ctr, e1) * R,
                               glm::dot(A - ctr, e2) * R);
            const glm::dvec2 b(glm::dot(B - ctr, e1) * R,
                               glm::dot(B - ctr, e2) * R);
            area += 0.5 * std::fabs(a.x * b.y - a.y * b.x);
        }
        plan.cap_area_m2 += area;
        // --- ruling 2's instrument: how much of the cap no leg corridor covers
        {
            std::vector<glm::dvec2> lu;
            std::vector<double> lw;
            for (std::size_t i = 0; i < cuts.size(); ++i) {
                const glm::dvec3& t = eps[cuts[i].ep].out;
                glm::dvec2 u(glm::dot(t, e1), glm::dot(t, e2));
                const double ul = glm::length(u);
                if (ul < 1e-9) continue;
                lu.push_back(u / ul);
                lw.push_back(ways[eps[cuts[i].ep].way].half_w_m);
            }
            for (std::size_t i = 0; i < cap.ring.size(); ++i) {
                const glm::dvec3 A = cap.ring[i];
                const glm::dvec3 B = cap.ring[(i + 1) % cap.ring.size()];
                const glm::dvec2 a(glm::dot(A - ctr, e1) * R,
                                   glm::dot(A - ctr, e2) * R);
                const glm::dvec2 b(glm::dot(B - ctr, e1) * R,
                                   glm::dot(B - ctr, e2) * R);
                const double tri = 0.5 * std::fabs(a.x * b.y - a.y * b.x);
                if (tri <= 0.0) continue;
                int hit = 0, tot = 0;
                for (int p = 1; p <= 4; ++p)
                    for (int qq = 1; qq + p <= 5; ++qq) {
                        const double fa = p / 6.0, fb = qq / 6.0;
                        const glm::dvec2 s2 = a * fa + b * fb;
                        ++tot;
                        for (std::size_t k = 0; k < lu.size(); ++k) {
                            const double tt = glm::dot(s2, lu[k]);
                            if (tt < 0.0) continue;
                            if (glm::length(s2 - lu[k] * tt) <= lw[k]) {
                                ++hit;
                                break;
                            }
                        }
                    }
                if (tot > 0)
                    plan.cap_area_uncovered_m2 +=
                        tri * static_cast<double>(tot - hit) /
                        static_cast<double>(tot);
            }
        }
        plan.road_len_removed_m += static_cast<double>(deg) * Rc;
        if (plan.nodes_cut == 0) {
            plan.cut_radius_min_m = plan.cut_radius_max_m = Rc;
        } else {
            plan.cut_radius_min_m = std::min(plan.cut_radius_min_m, Rc);
            plan.cut_radius_max_m = std::max(plan.cut_radius_max_m, Rc);
        }
        ++plan.nodes_cut;
        plan.caps.push_back(cap);
    }
    return plan;
}

// ---------------------------------------------------------------------------
// THE CAP MESH
// ---------------------------------------------------------------------------
// A radially ringed fan from the node out to the boundary, placed by the SAME
// radius_fn + lift the drape uses. Ring `rings` is the boundary itself, taken
// VERBATIM (never renormalized) so its float positions equal the trimmed leg
// ends' bit for bit -- that is the zero-gap seam.
//
// v is +1 across the whole cap: an intersection is plain asphalt and the
// centreline dash stops at the stop line, which is both what a road does and
// what keeps a dash from converging to a point at the node. s is inherited from
// the leg each boundary point meets and interpolated inward, so the asphalt
// MOTTLE (a function of s alone) is continuous across the seam.
// `vert_cap` (optional, parallel to the emitted batches' vertices, one entry
// per vertex per batch) records which cap each vertex came from, so the caller
// can measure a cap's SAG and attribute it to a place on the planet -- the sink
// re-check this rung owes.
inline std::vector<RibbonBatchCPU> build_junction_cap_batches(
    const std::vector<JunctionCapCPU>& caps,
    const std::function<double(const glm::dvec3&)>& radius_fn, double lift,
    std::vector<std::vector<int> >* vert_cap = nullptr) {
    std::vector<RibbonBatchCPU> out;
    if (caps.empty()) return out;
    out.emplace_back();
    if (vert_cap != nullptr) vert_cap->assign(1, std::vector<int>());
    for (std::size_t cap_i = 0; cap_i < caps.size(); ++cap_i) {
        const JunctionCapCPU& cap = caps[cap_i];
        const std::size_t nb = cap.ring.size();
        if (nb < 3 || cap.rings < 1 || cap.ring_s.size() != nb) continue;
        const std::size_t need = nb * static_cast<std::size_t>(cap.rings) + 1;
        if (need > 65535u) continue;  // unreachable for any sane node
        RibbonBatchCPU* b = &out.back();
        if (b->pos.size() / 3 + need > 65535u) {
            out.emplace_back();
            b = &out.back();
            if (vert_cap != nullptr) vert_cap->emplace_back();
        }
        auto push = [&](const glm::dvec3& d, float s, float v) {
            const std::size_t vi = b->pos.size() / 3;
            if (vert_cap != nullptr)
                vert_cap->back().push_back(static_cast<int>(cap_i));
            const double r = radius_fn(d) + lift;
            b->pos.push_back(static_cast<float>(d.x * r));
            b->pos.push_back(static_cast<float>(d.y * r));
            b->pos.push_back(static_cast<float>(d.z * r));
            b->uv.push_back(s);
            b->uv.push_back(v);
            return static_cast<unsigned short>(vi);
        };
        const unsigned short c0 = push(cap.centre, cap.centre_s, 1.0f);
        std::vector<unsigned short> prev, cur;
        for (int r = 1; r <= cap.rings; ++r) {
            cur.clear();
            const double f = static_cast<double>(r) / cap.rings;
            for (std::size_t i = 0; i < nb; ++i) {
                // ★ the boundary ring is the BOUNDARY, verbatim.
                const glm::dvec3 d =
                    (r == cap.rings)
                        ? cap.ring[i]
                        : glm::normalize(cap.centre +
                                         (cap.ring[i] - cap.centre) * f);
                const float s =
                    cap.centre_s +
                    (cap.ring_s[i] - cap.centre_s) * static_cast<float>(f);
                cur.push_back(push(d, s, 1.0f));
            }
            if (r == 1) {
                for (std::size_t i = 0; i < nb; ++i) {
                    b->idx.push_back(c0);
                    b->idx.push_back(cur[i]);
                    b->idx.push_back(cur[(i + 1) % nb]);
                }
            } else {
                for (std::size_t i = 0; i < nb; ++i) {
                    const std::size_t j = (i + 1) % nb;
                    b->idx.push_back(prev[i]);
                    b->idx.push_back(cur[i]);
                    b->idx.push_back(cur[j]);
                    b->idx.push_back(prev[i]);
                    b->idx.push_back(cur[j]);
                    b->idx.push_back(prev[j]);
                }
            }
            prev = cur;
        }
    }
    while (!out.empty() && out.back().idx.empty()) {
        out.pop_back();
        if (vert_cap != nullptr && !vert_cap->empty()) vert_cap->pop_back();
    }
    return out;
}

}  // namespace render
