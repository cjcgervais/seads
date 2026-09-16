#pragma once
// ★ ROAD-REPAIR / ONAPING SINK -- THE CHORD, NAMED AND THEN CUT.
//
// Chad flew the merged build and reported: "a few spots near Onaping (Valley)
// pump I went into the road on snowmachine ... on a hill to the north of the
// pump in the middle of the road going up slightly inclined pavement."
// The road census says SINK 0 on plowed roads within 2.5 km of Valley, so the
// ruler could not see what the eye saw. The two are not in conflict: they
// measure DIFFERENT SURFACES.
//
//   the census measures a FUNCTION.  `ribbon` is drawn_radius_at(dir) + lift
//                                    evaluated AT a station -- exact, by
//                                    construction, at every station it visits.
//   the eye sees a MESH.             render/ribbons.cpp build_path_mesh drapes
//                                    the BAKED vertices at that same function
//                                    and then fills FLAT TRIANGLES between
//                                    them. Between two baked rungs the drawn
//                                    deck is a straight CHORD in 3D.
//
// Over a concave grade break (the foot of a hill -- exactly what Chad names)
// the chord bridges ABOVE the ground; the machine's drive_r follows the
// terrain, floored to the ribbon function at its OWN point, which is down in
// the dip. The body ends up under the drawn deck: "went into the road". Over a
// convex crest the sign flips and the deck floats under the machine. Neither
// is visible to a ruler that samples the function.
//
// MEASURED (docs/road_repair/onaping_sink.md §1): the baked rungs are 52.67 m
// apart at the median planet-wide and up to 79.91 m, and 218 of the 529 drawn
// plowed segments within 2.5 km of the Valley pump (41.2 %) carry a positive
// centreline chord sag over the census's own 0.10 m SINK threshold -- worst
// +7.224 m.
//
// This header is the pure part of both seeing that and fixing it: which baked
// rungs are actually CONNECTED by drawn triangles, and the subdivided drape.
//
// PURE: glm + std + the baked GIS table. ZERO raylib, ZERO app.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "render/sudbury_gis.gen.h"

namespace render {

// A cost bound on the subdivision, never a resolution choice: at the measured
// max baked rung spacing (79.91 m) it is unreachable for any sane max_seg_m.
inline constexpr int kRibbonSplitCap = 64;

// The baked ribbon strip is a list of RUNGS: vertex 2m is the LEFT edge
// (v = -1) and vertex 2m+1 the RIGHT edge (v = +1) of rung m. A quad between
// rung m and rung m+1 is two triangles, and every triangle of that quad spans
// exactly the two pair-indices m and m+1.
//
// So: rung m is joined to rung m+1 in the DRAWN mesh iff some kept triangle
// has vertices in both pairs. Derived from the index list rather than assumed
// from vertex order, because (a) a path concatenates many disjoint runs whose
// rungs are adjacent in the array but share no triangle, and (b) the T24 cut
// clip (`ribbon_indices_outside_cuts`) drops triangles over excavated void --
// a chord there is not drawn and must not be measured.
//
// Returns the ascending list of m such that the quad (m, m+1) is drawn.
inline std::vector<int> ribbon_drawn_segments(
    const std::vector<unsigned short>& kept) {
    // A rung index can be at most vtx_count/2; mark the joins in a set that
    // scales with the path, not with the triangle count.
    std::vector<char> joined;
    for (std::size_t k = 0; k + 2 < kept.size(); k += 3) {
        const int p0 = kept[k] / 2;
        const int p1 = kept[k + 1] / 2;
        const int p2 = kept[k + 2] / 2;
        int lo = p0 < p1 ? p0 : p1;
        if (p2 < lo) lo = p2;
        int hi = p0 > p1 ? p0 : p1;
        if (p2 > hi) hi = p2;
        if (hi != lo + 1) continue;  // degenerate / not a rung-to-rung quad
        if (static_cast<std::size_t>(lo) >= joined.size())
            joined.resize(static_cast<std::size_t>(lo) + 1, 0);
        joined[static_cast<std::size_t>(lo)] = 1;
    }
    std::vector<int> segs;
    for (std::size_t m = 0; m < joined.size(); ++m)
        if (joined[m]) segs.push_back(static_cast<int>(m));
    return segs;
}

// ---------------------------------------------------------------------------
// THE SECOND CUT: ACROSS the road (the TRANSVERSE term).
//
// Longitudinal subdivision cannot touch it, and it is the bigger half of what
// Chad actually rode.  A baked rung is ONE quad spanning the WHOLE road
// (recovered half-widths reach 7.55 m on a road_major, so 15 m of flat span),
// so the drawn deck at the CENTRELINE -- "the middle of the road" -- is the
// flat chord between the two EDGES.  MEASURED at max_seg_m = 8: 118 of the 133
// remaining >0.10 m plowed segments near Valley were this term alone.
// ---------------------------------------------------------------------------

// How many COLUMNS one rung becomes.  1 == the identity (the baked L..R quad).
// Forced EVEN above 1, so u = 0.5 is a REAL VERTEX ROW: the road's dashed
// centreline lives at v = 0 (|v| < road_center_frac in the FS), and a deck that
// bends across the road has to bend AT the line the machine rides, not beside
// it.  A ribbon narrower than max_tr_m keeps its single span -- a 5 m trail
// gets no centre row, a 15 m road gets four columns.
inline int ribbon_tr_splits(const glm::dvec3& dL, const glm::dvec3& dR, double R,
                            double max_tr_m, int cap = kRibbonSplitCap) {
    if (!(max_tr_m > 0.0)) return 1;
    double c = glm::dot(dL, dR);
    if (c < -1.0) c = -1.0;
    if (c > 1.0) c = 1.0;
    const double w = R * std::acos(c);
    int n = static_cast<int>(std::ceil(w / max_tr_m));
    if (n < 1) n = 1;
    if (n > 1 && (n & 1)) ++n;  // EVEN => u = 0.5 is a vertex
    if (n > cap) n = cap & ~1;
    return n;
}

// The direction at lateral fraction u in [0,1] across a rung.
inline glm::dvec3 ribbon_lat_dir(const glm::dvec3& dL, const glm::dvec3& dR,
                                 double u) {
    if (u <= 0.0) return dL;
    if (u >= 1.0) return dR;
    return glm::normalize(dL + (dR - dL) * u);
}

// The DRAWN deck POSITION at lateral fraction u on a rung split into nt
// columns -- i.e. a point on the mesh's own piecewise-linear rung polyline, not
// on the smooth ground.  This is what the SEADS_RIBBON_SAG ruler samples, so
// the ruler reads the surface that ships and not a model of it.
inline glm::dvec3 ribbon_rung_point(
    const glm::dvec3& dL, const glm::dvec3& dR, int nt, double u,
    const std::function<double(const glm::dvec3&)>& radius_fn, double lift) {
    if (nt < 1) nt = 1;
    auto vert = [&](int c) {
        const glm::dvec3 d =
            ribbon_lat_dir(dL, dR, static_cast<double>(c) / nt);
        return d * (radius_fn(d) + lift);
    };
    double x = u * nt;
    int i = static_cast<int>(std::floor(x));
    if (i < 0) i = 0;
    if (i >= nt) i = nt - 1;
    const double f = x - i;
    if (f <= 0.0) return vert(i);          // u lands ON a column vertex
    if (f >= 1.0) return vert(i + 1);
    const glm::dvec3 A = vert(i), B = vert(i + 1);
    return A + (B - A) * f;
}

// ---------------------------------------------------------------------------
// THE CUT: subdividing the drape at build time.
// ---------------------------------------------------------------------------

// How many SUB-QUADS one baked segment becomes, given the centreline
// directions of its two rungs. Returns 1 (the identity: no insertion) when
// `max_seg_m` is 0 or the chord is already short enough. Capped, because a
// pathological baked rung pair must not be able to allocate the world.
inline int ribbon_seg_splits(const glm::dvec3& dc0, const glm::dvec3& dc1,
                             double R, double max_seg_m,
                             int cap = kRibbonSplitCap) {
    if (!(max_seg_m > 0.0)) return 1;
    double c = glm::dot(dc0, dc1);
    if (c < -1.0) c = -1.0;
    if (c > 1.0) c = 1.0;
    const double seg_len = R * std::acos(c);
    int n = static_cast<int>(std::ceil(seg_len / max_seg_m));
    if (n < 1) n = 1;
    if (n > cap) n = cap;
    return n;
}

// One upload-ready batch of ribbon geometry. `idx` is LOCAL 0-based into this
// batch's own `pos`/`uv`, so a batch maps 1:1 onto a raylib Mesh -- which is
// why there can be several per path: raylib meshes index with UNSIGNED SHORT,
// and a subdivided road path runs well past 65,535 vertices.
struct RibbonBatchCPU {
    std::vector<float> pos;           // 3 per vertex, world-ABSOLUTE
    std::vector<float> uv;            // 2 per vertex: (arc-length s, v)
    std::vector<unsigned short> idx;  // 3 per triangle
};

// THE DRAPE, subdivided.
//
// `radius_fn(dir)` is the DRAWN terrain radius at a direction (the caller
// passes render::drawn_radius_at bound to the live height field + the live
// [planet] subdiv/tiles); `lift` is added here so there is exactly one place
// the drape's clearance enters. `kept` is the T24 cut-clipped LOCAL index
// list. `max_seg_m <= 0` is the IDENTITY: one batch holding every baked vertex
// in baked order with `kept` verbatim -- byte-for-byte what the pre-cut
// build_path_mesh produced.
//
// With the dials on, each DRAWN quad (rung m to rung m+1) becomes a GRID:
// `ribbon_seg_splits` rows ALONG the road and `ribbon_tr_splits` columns
// ACROSS it, every inserted vertex at radius_fn(normalize(mix(...))) + lift
// with linearly interpolated (s, v) -- so the centre column carries v = 0 and
// the dashed centreline still lands on the centreline. Chad's Onaping sink is
// the chord over the ~52 m longitudinal gaps AND the ~15 m transverse span.
//
// ★ THE CLIP RULE, and it is the one thing here that can go quietly wrong:
// the T24 clip runs PER TRIANGLE, not per quad -- a quad with one vertex in a
// cut disk loses one of its two triangles and keeps the other. So this walks
// the KEPT triangles and records, per quad, the (edge, end) ROLE PATTERN of
// each surviving triangle; every sub-quad then emits exactly those patterns.
// A clipped original triangle therefore clips ALL of its children, and a
// surviving one keeps its original winding by construction.
inline std::vector<RibbonBatchCPU> build_ribbon_batches(
    const GisRibbonPath& P, const std::vector<unsigned short>& kept,
    const std::function<double(const glm::dvec3&)>& radius_fn, double lift,
    double R, double max_seg_m, double max_tr_m = 0.0) {
    std::vector<RibbonBatchCPU> out;
    if (kept.empty()) return out;

    auto vdir = [&P](int i) {
        const GisRibbonVertex& V = kSudburyRibbonVerts[P.vtx_off + i];
        return glm::dvec3(V.dir[0], V.dir[1], V.dir[2]);
    };
    auto place = [&](const glm::dvec3& d, float* p) {
        const double r = radius_fn(d) + lift;
        p[0] = static_cast<float>(d.x * r);
        p[1] = static_cast<float>(d.y * r);
        p[2] = static_cast<float>(d.z * r);
    };
    auto whole_path_batch = [&](const std::vector<unsigned short>& ix) {
        out.emplace_back();
        RibbonBatchCPU& b = out.back();
        b.pos.resize(static_cast<std::size_t>(P.vtx_count) * 3);
        b.uv.resize(static_cast<std::size_t>(P.vtx_count) * 2);
        for (int i = 0; i < P.vtx_count; ++i) {
            const GisRibbonVertex& V = kSudburyRibbonVerts[P.vtx_off + i];
            place(glm::dvec3(V.dir[0], V.dir[1], V.dir[2]),
                  &b.pos[static_cast<std::size_t>(i) * 3]);
            b.uv[static_cast<std::size_t>(i) * 2 + 0] = V.s;
            b.uv[static_cast<std::size_t>(i) * 2 + 1] = V.v;
        }
        b.idx = ix;
    };

    // --- the IDENTITY: the pre-cut mesh, vertex for vertex, index for index -
    if (!(max_seg_m > 0.0) && !(max_tr_m > 0.0)) {
        whole_path_batch(kept);
        return out;
    }

    // --- the surviving triangles of each quad, as ROLE PATTERNS -------------
    // pattern byte = end*2 + edge, with end in {0,1} (which rung) and edge in
    // {0,1} (0 = LEFT/v=-1, 1 = RIGHT/v=+1) -- i.e. exactly (i - 2m).
    const int nrungs = P.vtx_count / 2;
    std::vector<std::vector<unsigned char> > pat(
        static_cast<std::size_t>(nrungs > 0 ? nrungs : 0));
    std::vector<unsigned short> odd;  // triangles that are not a rung-to-rung
                                      // quad (none in the shipped bake; carried
                                      // verbatim rather than silently dropped)
    for (std::size_t k = 0; k + 2 < kept.size(); k += 3) {
        const int a = kept[k], b2 = kept[k + 1], c = kept[k + 2];
        const int pa = a / 2, pb = b2 / 2, pc = c / 2;
        const int lo = std::min(pa, std::min(pb, pc));
        const int hi = std::max(pa, std::max(pb, pc));
        if (hi != lo + 1 || lo < 0 || lo + 1 >= nrungs) {
            odd.push_back(kept[k]);
            odd.push_back(kept[k + 1]);
            odd.push_back(kept[k + 2]);
            continue;
        }
        std::vector<unsigned char>& q = pat[static_cast<std::size_t>(lo)];
        q.push_back(static_cast<unsigned char>(a - 2 * lo));
        q.push_back(static_cast<unsigned char>(b2 - 2 * lo));
        q.push_back(static_cast<unsigned char>(c - 2 * lo));
    }

    out.emplace_back();
    int last_quad = -2;  // the quad emitted immediately before ...
    int last_nt = 0;     // ... its column count, and its FINAL rung's row of
    std::vector<unsigned short> last_row;  //     indices, shared into rung 0.
    std::vector<unsigned short> grid;      // (n+1) x (nt+1), row-major
    for (int m = 0; m + 1 < nrungs; ++m) {
        const std::vector<unsigned char>& q = pat[static_cast<std::size_t>(m)];
        if (q.empty()) continue;
        const glm::dvec3 dL0 = vdir(2 * m), dR0 = vdir(2 * m + 1);
        const glm::dvec3 dL1 = vdir(2 * m + 2), dR1 = vdir(2 * m + 3);
        const GisRibbonVertex& VL0 = kSudburyRibbonVerts[P.vtx_off + 2 * m];
        const GisRibbonVertex& VR0 = kSudburyRibbonVerts[P.vtx_off + 2 * m + 1];
        const GisRibbonVertex& VL1 = kSudburyRibbonVerts[P.vtx_off + 2 * m + 2];
        const GisRibbonVertex& VR1 = kSudburyRibbonVerts[P.vtx_off + 2 * m + 3];
        const glm::dvec3 c0 = glm::normalize(dL0 + dR0);
        const glm::dvec3 c1 = glm::normalize(dL1 + dR1);
        const int n = ribbon_seg_splits(c0, c1, R, max_seg_m);
        // ★ THE CLIP RULE, TRANSVERSE HALF. A quad whose two triangles BOTH
        // survive owns its whole area, so it may be split into a grid. A quad
        // that lost one triangle to a cut owns only HALF its area along the
        // R0->L1 diagonal, and a grid cell straddling that diagonal would
        // RESURRECT the clipped half. Such a quad therefore keeps its single
        // full-width span (nt == 1) and is only split along its length, which
        // is exactly the behaviour it had before this cut existed.
        const bool whole_quad = (q.size() == 6);
        const int nt =
            whole_quad
                ? std::max(ribbon_tr_splits(dL0, dR0, R, max_tr_m),
                           ribbon_tr_splits(dL1, dR1, R, max_tr_m))
                : 1;

        RibbonBatchCPU* b = &out.back();
        bool share = (last_quad == m - 1 && last_nt == nt);
        const std::size_t cols = static_cast<std::size_t>(nt) + 1;
        const std::size_t need =
            cols * static_cast<std::size_t>(share ? n : n + 1);
        if (b->pos.size() / 3 + need > 65535u) {
            out.emplace_back();
            b = &out.back();
            share = false;  // the shared rung lives in the batch we just left
            last_quad = -2;
        }
        auto push_vertex = [&](const glm::dvec3& d, float s, float v) {
            const std::size_t vi = b->pos.size() / 3;
            b->pos.resize(b->pos.size() + 3);
            place(d, &b->pos[vi * 3]);
            b->uv.push_back(s);
            b->uv.push_back(v);
            return static_cast<unsigned short>(vi);
        };
        grid.assign(cols * (static_cast<std::size_t>(n) + 1), 0);
        for (int j = 0; j <= n; ++j) {
            const std::size_t row = static_cast<std::size_t>(j) * cols;
            if (j == 0 && share) {
                for (std::size_t i = 0; i < cols; ++i) grid[i] = last_row[i];
                continue;
            }
            // The rung's own edges: EXACT baked values at the two ends, the
            // normalized interpolation between.
            glm::dvec3 dLj = dL0, dRj = dR0;
            float sL = VL0.s, sR = VR0.s, vL = VL0.v, vR = VR0.v;
            if (j == n) {
                dLj = dL1;
                dRj = dR1;
                sL = VL1.s;
                sR = VR1.s;
                vL = VL1.v;
                vR = VR1.v;
            } else if (j != 0) {
                const double t = static_cast<double>(j) / static_cast<double>(n);
                const float tf = static_cast<float>(t);
                dLj = glm::normalize(dL0 + (dL1 - dL0) * t);
                dRj = glm::normalize(dR0 + (dR1 - dR0) * t);
                sL = VL0.s + (VL1.s - VL0.s) * tf;
                sR = VR0.s + (VR1.s - VR0.s) * tf;
                vL = VL0.v + (VL1.v - VL0.v) * tf;
                vR = VR0.v + (VR1.v - VR0.v) * tf;
            }
            for (int i = 0; i <= nt; ++i) {
                const double u = static_cast<double>(i) / nt;
                const float uf = static_cast<float>(u);
                grid[row + static_cast<std::size_t>(i)] =
                    push_vertex(ribbon_lat_dir(dLj, dRj, u), sL + (sR - sL) * uf,
                                vL + (vR - vL) * uf);
            }
        }
        // Every surviving ORIGINAL triangle, re-emitted in every CELL of the
        // grid: its (end, edge) role pattern indexes the cell's own corners, so
        // winding is preserved by construction and a clipped original has no
        // children anywhere.
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < nt; ++i)
                for (std::size_t t3 = 0; t3 + 2 < q.size(); t3 += 3)
                    for (std::size_t e = 0; e < 3; ++e) {
                        const unsigned char code = q[t3 + e];
                        const std::size_t row =
                            static_cast<std::size_t>(j) + (code >> 1);
                        const std::size_t col =
                            static_cast<std::size_t>(i) + (code & 1u);
                        b->idx.push_back(grid[row * cols + col]);
                    }
        last_quad = m;
        last_nt = nt;
        last_row.assign(grid.begin() + static_cast<std::ptrdiff_t>(
                                           static_cast<std::size_t>(n) * cols),
                        grid.end());
    }
    // The (empty in the shipped bake) non-quad remainder, carried verbatim in
    // its own batch so its LOCAL baked indices stay meaningful.
    if (!odd.empty()) whole_path_batch(odd);
    while (!out.empty() && out.back().idx.empty()) out.pop_back();
    return out;
}

}  // namespace render
