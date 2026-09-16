#include "render/tunnel_mesh.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/geometric.hpp>

#include "world/heightfield.h"
#include "world/tunnel_net.h"

// T2 greybox generation (docs/tunnel_staging.md). PURE (glm + std). Everything
// here derives FROM the T1 net so the visible wall == the T1/T3 query surface
// (the single-source invariant the test pins).

namespace render {

namespace {

// The surface radius at a unit direction: the collar rim source (ground) or the
// bare sphere at `bare_R` (recovered from the net's mouth when ground is null).
double surface_r(const glm::dvec3& dir, const world::HeightField* ground,
                 double bare_R) {
    return ground != nullptr ? ground->radius_at(dir) : bare_R;
}

// Depth below the LOCAL surface at a world point [m] (>= 0 underground). Used
// for the per-vertex brightness cue.
double depth_below_surface(const glm::dvec3& v,
                           const world::HeightField* ground, double bare_R) {
    const double len = glm::length(v);
    if (len <= 0.0) return 0.0;
    const glm::dvec3 dir = v / len;
    return surface_r(dir, ground, bare_R) - len;
}

// The brightness cue in [0,1]: 1 at/above the local surface, fading to
// kDepthFloor by kCueFadeDepth_m deep. Deep tube reads dark; near the mouths it
// brightens (the "the tube reads" depth cueing).
float depth_cue(const glm::dvec3& v, const world::HeightField* ground,
                double bare_R) {
    const double depth = depth_below_surface(v, ground, bare_R);
    if (depth <= 0.0) return 1.0f;
    double f = depth / kCueFadeDepth_m;  // 0 at surface .. 1+ deep
    if (f > 1.0) f = 1.0;
    const double cue = 1.0 - (1.0 - static_cast<double>(kDepthFloor)) * f;
    return static_cast<float>(cue);
}

// Push a world point + its cue into a piece.
void push_v(TunnelPiece& p, const glm::dvec3& v, float cue) {
    p.positions.push_back(static_cast<float>(v.x));
    p.positions.push_back(static_cast<float>(v.y));
    p.positions.push_back(static_cast<float>(v.z));
    p.cue.push_back(cue);
}

// T19 (Deliverable B) — THE MOUTH-DARKENING GRADIENT. Scale a mouth piece's
// per-vertex cue DOWN toward rock-dark near the bore mouth so the pale open-pit
// surround fades into the dark slot instead of butting bright-against-black:
//   cue *= mix(kMouthBlendFloor, 1, clamp(dist(vert, mouth_centre)/R_blend)).
// Applied AFTER the cut trims (so the trim's emitted leaf verts are covered).
void darken_cue_near(TunnelPiece& p, const glm::dvec3& mouth_centre,
                     double R_blend) {
    if (R_blend <= 0.0) return;
    const double floor = static_cast<double>(kMouthBlendFloor);
    for (std::size_t i = 0; i < p.cue.size(); ++i) {
        const glm::dvec3 v{p.positions[3 * i + 0], p.positions[3 * i + 1],
                           p.positions[3 * i + 2]};
        const double t = std::min(1.0, glm::length(v - mouth_centre) / R_blend);
        p.cue[i] *= static_cast<float>(floor + (1.0 - floor) * t);
    }
}

// Stitch two consecutive rings (each `seg` verts, ring0 starting at index
// base0, ring1 at base1) into a quad strip (2 tris/quad), consistent winding.
void stitch_rings(TunnelPiece& p, int seg, unsigned int base0,
                  unsigned int base1) {
    for (int s = 0; s < seg; ++s) {
        const int sn = (s + 1) % seg;
        const unsigned short a = static_cast<unsigned short>(base0 + s);
        const unsigned short b = static_cast<unsigned short>(base0 + sn);
        const unsigned short c = static_cast<unsigned short>(base1 + s);
        const unsigned short d = static_cast<unsigned short>(base1 + sn);
        p.indices.push_back(a);
        p.indices.push_back(c);
        p.indices.push_back(b);
        p.indices.push_back(b);
        p.indices.push_back(c);
        p.indices.push_back(d);
    }
}

// Stitch a FINE ring (seg_f verts, base_f) to a COARSE ring (seg_c verts,
// base_c) where seg_f == 2*seg_c (a 2:1 azimuthal reduction). Each coarse vert
// c owns fine verts 2c and 2c+1: two tris fan the coarse vert to the fine pair,
// plus one tri closes to the next coarse vert. Consistent winding with
// stitch_rings (fine == "ring0"/base0 side). T5d seam: the terrain band's
// innermost ring (kCollarBandSegments) reduces onto the tube-end ring
// (kTubeSegments) with no crack.
void stitch_rings_2to1(TunnelPiece& p, int seg_c, unsigned int base_f,
                       unsigned int base_c) {
    const int seg_f = 2 * seg_c;
    for (int c = 0; c < seg_c; ++c) {
        const int cn = (c + 1) % seg_c;
        const int f0 = (2 * c) % seg_f;
        const int f1 = (2 * c + 1) % seg_f;
        const int f2 = (2 * c + 2) % seg_f;
        const unsigned short C = static_cast<unsigned short>(base_c + c);
        const unsigned short Cn = static_cast<unsigned short>(base_c + cn);
        const unsigned short F0 = static_cast<unsigned short>(base_f + f0);
        const unsigned short F1 = static_cast<unsigned short>(base_f + f1);
        const unsigned short F2 = static_cast<unsigned short>(base_f + f2);
        // Fan C to (F0,F1) and (F1,F2), then close (C,Cn,F2).
        p.indices.push_back(F0);
        p.indices.push_back(C);
        p.indices.push_back(F1);
        p.indices.push_back(F1);
        p.indices.push_back(C);
        p.indices.push_back(F2);
        p.indices.push_back(F2);
        p.indices.push_back(C);
        p.indices.push_back(Cn);
    }
}

// Stitch a COARSE ring (seg_c verts, base_c) to a FINE ring (seg_c*sub verts,
// base_f) where fine vertex c*sub is co-located with coarse vertex c (a general
// 1:sub azimuthal fan; sub==2 reproduces stitch_rings_2to1's winding exactly).
// T26: the exact 24-vert tube-seam ring fans onto the azimuthally-subdivided
// collar ring so the tube<->collar seam stays crack-free while the collar body
// carries `sub`x the azimuth density.
void stitch_rings_coarse_to_fine(TunnelPiece& p, int seg_c, int sub,
                                 unsigned int base_c, unsigned int base_f) {
    const int seg_f = seg_c * sub;
    for (int c = 0; c < seg_c; ++c) {
        const int cn = (c + 1) % seg_c;
        const unsigned short C = static_cast<unsigned short>(base_c + c);
        const unsigned short Cn = static_cast<unsigned short>(base_c + cn);
        for (int m = 0; m < sub; ++m) {
            const int fm = (c * sub + m) % seg_f;
            const int fm1 = (c * sub + m + 1) % seg_f;
            const unsigned short Fm = static_cast<unsigned short>(base_f + fm);
            const unsigned short Fm1 = static_cast<unsigned short>(base_f + fm1);
            p.indices.push_back(Fm);
            p.indices.push_back(C);
            p.indices.push_back(Fm1);
        }
        // Close to the next coarse vert (the fan's trailing tri).
        const unsigned short Fend =
            static_cast<unsigned short>(base_f + ((c + 1) * sub) % seg_f);
        p.indices.push_back(Fend);
        p.indices.push_back(C);
        p.indices.push_back(Cn);
    }
}

// A perpendicular unit vector to `t` seeded from `hint` (Gram-Schmidt); falls
// back to a stable axis if hint is ~parallel to t.
glm::dvec3 perp_seed(const glm::dvec3& t, const glm::dvec3& hint) {
    glm::dvec3 u = hint - glm::dot(hint, t) * t;
    double l = glm::length(u);
    if (l < 1e-9) {
        const glm::dvec3 alt =
            std::fabs(t.x) < 0.9 ? glm::dvec3{1, 0, 0} : glm::dvec3{0, 1, 0};
        u = alt - glm::dot(alt, t) * t;
        l = glm::length(u);
    }
    return u / l;
}

// Node tangent (forward difference at ends, central inside).
glm::dvec3 spine_tangent(const world::TunnelNet& net, std::size_t i) {
    const std::size_t n = net.spine.size();
    glm::dvec3 t;
    if (i == 0)
        t = net.spine[1].pos - net.spine[0].pos;
    else if (i + 1 >= n)
        t = net.spine[n - 1].pos - net.spine[n - 2].pos;
    else
        t = net.spine[i + 1].pos - net.spine[i - 1].pos;
    const double l = glm::length(t);
    return l > 0.0 ? t / l : glm::dvec3{0, 0, -1};
}

// Build the swept tube. Returns the transported (u,w) frame + the world ring
// vertices at BOTH ends so the collars can reuse the EXACT end-ring floats (no
// seam crack). The frame at each node is parallel-transported to avoid twist.
struct TubeEnds {
    std::vector<glm::dvec3> ring0;  // first node's ring verts (world)
    std::vector<glm::dvec3> ring1;  // last node's ring verts (world)
    glm::dvec3 u0, w0, u1, w1;      // end frames (for the collar inner ring)
    double radius = 0.0;
    // T15 — the BREACH-mouth rings (the last emitted ring before each
    // suppression gap: node_in-1 on the Errington leg, node_in+1 on the
    // Murray leg). The breach collars reuse these EXACT floats as their inner
    // seam (the mouth-collar no-crack discipline, applied underground).
    std::vector<glm::dvec3> ring_bE;
    std::vector<glm::dvec3> ring_bM;
};

// T5a: clamp a tube ring vertex UP to the flat floor plane (perpendicular to
// local up at the station center `c`). floor_r <= 0 => OFF (returns v exactly).
// The floor plane sits at radial height h_floor = floor_r - length(c) below the
// center along up = normalize(c); a vertex whose up-component is below that is
// slid straight up to the plane (lateral position preserved — a true chord
// truncation, and the clamped vert lands on the SDF floor boundary). This keeps
// the wall from poking below the visible floor (single-source with the net's
// floor-truncated tube SDF).
glm::dvec3 floor_clamp(const glm::dvec3& v, const glm::dvec3& c,
                       double floor_r) {
    if (floor_r <= 0.0) return v;
    const glm::dvec3 up = glm::normalize(c);
    const double h_floor = floor_r - glm::length(c);  // below center (<0)
    const double h = glm::dot(v - c, up);
    if (h >= h_floor) return v;
    return v - (h - h_floor) * up;  // raise to the floor plane
}

// A per-station record of the flat floor chord (T5a) so the floor strip can be
// stitched between consecutive stations. `on` == the floor is live here.
struct FloorRow {
    glm::dvec3 left{0.0};   // one chord edge (world)
    glm::dvec3 right{0.0};  // the other chord edge (world)
    bool on = false;
};

TubeEnds build_tube(const world::TunnelNet& net,
                    const std::vector<Breach>& breaches,
                    const world::HeightField* g, double bare_R, TunnelPiece& p,
                    std::vector<FloorRow>* floor_rows) {
    const std::size_t n = net.spine.size();
    // T6b: the ELLIPTICAL bore — width (horizontal semi) x height (vertical
    // semi).
    const double rw = net.tube_width;
    const double rh = net.tube_height;
    const int seg = kTubeSegments;

    TubeEnds ends;
    ends.radius = rw;  // (documentary; the collars derive their own frame)
    ends.ring0.reserve(seg);
    ends.ring1.reserve(seg);
    if (floor_rows) floor_rows->assign(n, FloorRow{});

    // T11/T14 — TUBE SUPPRESSION at the TRUE breaches. Suppress exactly the
    // spine nodes INSIDE the arena ellipsoid (the inclusive [node_in_E,
    // node_in_M] index range from the T14 locator): a ring there would paint a
    // wall across open arena air, and a ring OUTSIDE it that the old radius
    // rule dropped left ~410 m of wall-less tube in solid rock. The last
    // emitted ring on each side is the breach mouth (left open into the arena;
    // the arena's T14 hole is cut around the SAME crossing so they align).
    // Only CONSECUTIVE emitted rings are stitched, so no wall spans the gap.
    // No breaches (arena off / bore never enters) => nothing suppressed
    // (bit-identical to the pre-T10 full tube).
    const bool have_breach = breaches.size() == 2;

    unsigned int prev_base = 0;
    bool have_prev = false;
    for (std::size_t i = 0; i < n; ++i) {
        const glm::dvec3 c = net.spine[i].pos;
        const bool suppress =
            have_breach && i >= breaches[0].node_in && i <= breaches[1].node_in;
        if (suppress) {
            have_prev = false;  // break the strip at the breach
            continue;
        }
        const double floor_r = net.spine[i].floor_r;
        const glm::dvec3 t = spine_tangent(net, i);
        // The cross-section frame is LOCAL-UP-ALIGNED (T6b), computed the SAME
        // way the SDF's elliptical_capsule_sd decomposes an offset: horiz =
        // normalize(cross(tangent, up)), vert = normalize(cross(horiz,
        // tangent)) (≈ up when up ⟂ tangent). This makes the ring an ellipse
        // aligned to (width·horiz, height·vert) that lands EXACTLY on the SDF's
        // sd==0 surface — single source. up and tangent both vary smoothly
        // along the spine, so consecutive rings never twist (the
        // transport-no-twist pin).
        const glm::dvec3 up = net.spine[i].up;  // == normalize(c)
        glm::dvec3 horiz = glm::cross(t, up);
        double hl = glm::length(horiz);
        if (hl <= 1e-9) {
            const glm::dvec3 alt = std::fabs(up.x) < 0.9 ? glm::dvec3{1, 0, 0}
                                                         : glm::dvec3{0, 1, 0};
            horiz = alt - glm::dot(alt, up) * up;
            hl = glm::length(horiz);
        }
        horiz /= hl;
        const glm::dvec3 vert = glm::normalize(glm::cross(horiz, t));

        // T10/T14 BREACH SPILL: the bore's last ~600 m before it breaches the
        // cavern ceiling glows with core light (the angle-of-entry cue "the
        // light at the end of the tunnel"). Blend the depth cue toward 1.0 by
        // world distance to THIS leg's TRUE crossing (an unsuppressed node is
        // either before the Errington breach or after the Murray one).
        double spill = 0.0;
        if (have_breach) {
            const glm::dvec3& bpos =
                (i < breaches[0].node_in) ? breaches[0].pos : breaches[1].pos;
            const double d_above = glm::length(c - bpos);
            if (d_above < 600.0)
                spill = std::clamp(1.0 - d_above / 600.0, 0.0, 1.0);
        }
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const double a = 2.0 * 3.14159265358979323846 * s / seg;
            glm::dvec3 v =
                c + rw * std::cos(a) * horiz + rh * std::sin(a) * vert;
            v = floor_clamp(v, c, floor_r);  // T5a: chord-truncate at the floor
            const double cue0 = depth_cue(v, g, bare_R);
            const double cue =
                cue0 + (1.0 - cue0) * spill;  // toward 1 at breach
            push_v(p, v, static_cast<float>(cue));
            if (i == 0) ends.ring0.push_back(v);
            if (i + 1 == n) ends.ring1.push_back(v);
            // T15: capture the breach-mouth rings for the breach collars.
            if (have_breach && i + 1 == breaches[0].node_in)
                ends.ring_bE.push_back(v);
            if (have_breach && i == breaches[1].node_in + 1)
                ends.ring_bM.push_back(v);
        }
        if (have_prev) stitch_rings(p, seg, prev_base, base);
        prev_base = base;
        have_prev = true;

        // T5a floor chord edges at this station: the flat floor is
        // perpendicular to local up; its edges sit where the floor plane cuts
        // the ELLIPSE. The floor plane sits d below the center along up (d =
        // height - floor_height measured off center; here d = length(c) -
        // floor_r). On the ellipse the horizontal half-width at vertical offset
        // -d is rw*sqrt(1-(d/rh)^2).
        if (floor_rows && floor_r > 0.0) {
            const double d = glm::length(c) - floor_r;  // center-above-floor >0
            if (d < rh) {  // the floor cuts the bore (else below the bore)
                const double frac = d / rh;
                const double half =
                    rw * std::sqrt(std::max(0.0, 1.0 - frac * frac));
                glm::dvec3 lat = glm::cross(up, t);  // ⟂ up and tangent
                const double ll = glm::length(lat);
                if (ll > 1e-9) {
                    lat /= ll;
                    // The floor plane center point (drop from c along up by d).
                    const glm::dvec3 fc = c - up * d;
                    FloorRow& row = (*floor_rows)[i];
                    row.left = fc - lat * half;
                    row.right = fc + lat * half;
                    row.on = true;
                }
            }
        }

        if (i == 0) {
            ends.u0 = horiz;
            ends.w0 = vert;
        }
        if (i + 1 == n) {
            ends.u1 = horiz;
            ends.w1 = vert;
        }
    }
    return ends;
}

// T5a: build the flat floor strip from the per-station chord edges. Consecutive
// live rows are stitched into a quad (left0,right0)->(left1,right1). Tagged
// kFloor so the draw uses kFloorVal (and the shader's local-up key brightens it
// — it faces up). Depth-cued like the tube. A row where the floor does not cut
// the tube (off) breaks the strip.
void build_floor(const std::vector<FloorRow>& rows, const world::HeightField* g,
                 double bare_R, TunnelPiece& p) {
    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (!rows[i - 1].on || !rows[i].on) continue;
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        push_v(p, rows[i - 1].left, depth_cue(rows[i - 1].left, g, bare_R));
        push_v(p, rows[i - 1].right, depth_cue(rows[i - 1].right, g, bare_R));
        push_v(p, rows[i].left, depth_cue(rows[i].left, g, bare_R));
        push_v(p, rows[i].right, depth_cue(rows[i].right, g, bare_R));
        // Quad (l0,r0,l1,r1): two tris, consistent winding.
        p.indices.push_back(static_cast<unsigned short>(base + 0));
        p.indices.push_back(static_cast<unsigned short>(base + 2));
        p.indices.push_back(static_cast<unsigned short>(base + 1));
        p.indices.push_back(static_cast<unsigned short>(base + 1));
        p.indices.push_back(static_cast<unsigned short>(base + 2));
        p.indices.push_back(static_cast<unsigned short>(base + 3));
    }
}

// Tessellate a two-half oriented ellipsoid (the egg / a chamber) into a
// lat/long grid in its own frame. a_pos on +u_long, a_neg on -u_long (matches
// the net's SDF two-half convention). When `lit` the per-vertex cue is baked at
// 1.0 (the egg/chamber are LIT interiors — the depth floor never darkens the
// Black Stope; T4b, "the energy fight has a lit chamber").
void build_pocket(const world::TunnelNet::Pocket& e,
                  const world::HeightField* g, double bare_R, bool lit,
                  TunnelPiece& p) {
    const int nlong = kPocketLong;  // divisions around u_long (azimuth 0..2pi)
    const int nlat = kPocketLat;    // divisions from -pole to +pole
    const double pi = 3.14159265358979323846;

    // Rows j=0..nlat (lat angle phi in [-pi/2, pi/2] along u_long); each row a
    // ring of nlong verts around (u_lat, u_up). Poles collapse but we keep full
    // rings (degenerate quads at the caps are harmless for a greybox).
    const unsigned int base = static_cast<unsigned int>(p.cue.size());
    for (int j = 0; j <= nlat; ++j) {
        const double phi = -pi * 0.5 + pi * j / nlat;  // -pi/2 .. pi/2
        const double sp = std::sin(phi), cp = std::cos(phi);
        const double aL = sp >= 0.0 ? e.a_pos : e.a_neg;  // two-half long axis
        for (int s = 0; s < nlong; ++s) {
            const double th = 2.0 * pi * s / nlong;
            const glm::dvec3 local = aL * sp * e.u_long +
                                     e.b * cp * std::cos(th) * e.u_lat +
                                     e.c * cp * std::sin(th) * e.u_up;
            const glm::dvec3 v = e.center + local;
            push_v(p, v, lit ? 1.0f : depth_cue(v, g, bare_R));
        }
    }
    for (int j = 0; j < nlat; ++j) {
        const unsigned int r0 = base + static_cast<unsigned int>(j) * nlong;
        const unsigned int r1 = r0 + nlong;
        stitch_rings(p, nlong, r0, r1);
    }
}

// T11/T12 — THE ARENA INTERIOR (the shallow dogfight room). An inward-viewed
// oriented oblate ellipsoid (the net's arena Pocket), SKIPPING facets whose
// WORLD direction (from origin) lies within a breach hole (so the bores read
// through the arena wall). T12 SEALED THE FLOOR: the floor-shaft opening is
// GONE (the core-window shaft is gone), so the -u_long floor pole is now solid
// rock "filled over the core" — every facet outside a breach hole is kept. A
// lat/long grid in the arena's own frame (u_long = the oblate short axis;
// u_lat/u_up the wide plane). cue baked 1.0 (the core key + strata own its
// shading). A facet is kept only when ALL FOUR corners clear every breach hole.
void build_arena(const world::TunnelNet& net,
                 const std::vector<Breach>& breaches, TunnelPiece& p) {
    const world::TunnelNet::Pocket& e = net.arena;
    const int nlong = kCavernLong;
    const int nlat = kCavernLat;
    const double pi = 3.14159265358979323846;
    // T16 DEFECT-1 — DROP FROM COLLISION TRUTH (supersedes the T14 angular-
    // ellipse in_hole here). A wall vertex is OPEN when the swept bore actually
    // pierces the shell at it (net.tube_signed_distance < kBreachRenderCut_m):
    // the visible hole is then the EXACT collision opening, so a rendered wall
    // can never sit over open bore air (the fly-through wall) nor a hole over
    // rock (the see-through). Bounded to the breach neighbourhoods so the far
    // ceiling pays one length check per vertex, not an ~80-segment tube scan.
    // (The angular breach_hole_hit lives on for the ember/beacon skips — those
    // are lamps, not occluders, so a slightly larger no-light zone is
    // harmless.)
    const unsigned int base = static_cast<unsigned int>(p.cue.size());
    const std::size_t ncol = static_cast<std::size_t>(nlong + 1);
    const std::size_t nrow = static_cast<std::size_t>(nlat + 1);
    std::vector<char> open(nrow * ncol, 0);
    for (int j = 0; j <= nlat; ++j) {
        const double phi = -pi * 0.5 + pi * j / nlat;  // -pi/2 (floor) .. pi/2
        const double sp = std::sin(phi), cp = std::cos(phi);
        const double aL = sp >= 0.0 ? e.a_pos : e.a_neg;
        for (int s = 0; s <= nlong; ++s) {
            const double th = 2.0 * pi * s / nlong;
            const glm::dvec3 local = aL * sp * e.u_long +
                                     e.b * cp * std::cos(th) * e.u_lat +
                                     e.c * cp * std::sin(th) * e.u_up;
            const glm::dvec3 v = e.center + local;
            const std::size_t idx = static_cast<std::size_t>(j) * ncol + s;
            push_v(p, v, 1.0f);
            bool near_breach = false;
            for (const Breach& b : breaches)
                if (glm::length(v - b.pos) < kArenaBreachTestRadius_m) {
                    near_breach = true;
                    break;
                }
            if (near_breach && net.tube_signed_distance(v) < kBreachRenderCut_m)
                open[idx] = 1;
        }
    }
    const auto vid = [&](int j, int s) {
        return base + static_cast<unsigned int>(j) * ncol + s;
    };
    for (int j = 0; j < nlat; ++j) {
        for (int s = 0; s < nlong; ++s) {
            // Skip the quad if ANY corner is OPEN (the bore pierces there) —
            // the hole is the true collision opening (T12: the floor is solid;
            // no shaft opening, so only the two bore breaches cut the shell).
            bool skip = false;
            for (int dj = 0; dj <= 1 && !skip; ++dj)
                for (int ds = 0; ds <= 1 && !skip; ++ds) {
                    const std::size_t idx =
                        static_cast<std::size_t>(j + dj) * ncol + (s + ds);
                    if (open[idx]) skip = true;
                }
            if (skip) continue;
            const unsigned short a = static_cast<unsigned short>(vid(j, s));
            const unsigned short b = static_cast<unsigned short>(vid(j, s + 1));
            const unsigned short c = static_cast<unsigned short>(vid(j + 1, s));
            const unsigned short d =
                static_cast<unsigned short>(vid(j + 1, s + 1));
            p.indices.push_back(a);
            p.indices.push_back(c);
            p.indices.push_back(b);
            p.indices.push_back(b);
            p.indices.push_back(c);
            p.indices.push_back(d);
        }
    }
}

// T14b/T15 — ray-cast a world direction onto the arena CEILING surface: scan
// down from ABOVE the arena apex (config-relative — a bare start constant
// could begin INSIDE the ellipsoid after a deep-crossing retune) for the first
// inside sample, then bisect the sd == 0 crossing (the ceiling-side boundary).
// Shared by the beacon rings AND the T15 breach-collar outer rim (one
// projection, no fork). `near_r` centres the scan window (the breach radius).
bool arena_surface_point(const world::TunnelNet& net, double near_r,
                         const glm::dvec3& wdir, glm::dvec3& out) {
    double r_out = std::max(near_r, net.arena_apex_r) + 200.0;
    double r_in = 0.0;
    bool found = false;
    for (double r = r_out; r >= near_r - 1500.0; r -= 50.0) {
        if (world::ellipsoid_sd(r * wdir, net.arena) < 0.0) {
            r_in = r;
            found = true;
            break;
        }
        r_out = r;
    }
    if (!found) return false;
    for (int it = 0; it < 50; ++it) {
        const double mid = 0.5 * (r_out + r_in);
        if (world::ellipsoid_sd(mid * wdir, net.arena) < 0.0)
            r_in = mid;
        else
            r_out = mid;
    }
    out = (0.5 * (r_out + r_in)) * wdir;
    return true;
}

// T15 — THE BREACH COLLAR (fly-11: "before I enter the murray tunnel from the
// west, I can see through the ground to the surface"). ROOT CAUSE: the cut
// hole is wider than the tube's silhouette (deliberately — margin + facet
// granularity), and the rock between the arena wall and the terrain is NOT
// GEOMETRY: terrain reads backface-invisible from below, so every sightline
// through the hole margin saw clear to the surface (sky, trees, mouth-beacon
// glows), and underground lamp glows bled the other way. The T14b trim shrank
// the gash but cannot seal a margin that must exist. The SEAL: an annulus
// funnel per breach from the tube's breach-mouth ring (EXACT floats — the
// mouth-collar no-crack discipline) out to the cut rim on the arena wall
// (padded past the ragged any-corner facet edge, nudged 5 m into the rock so
// kept facets never z-fight it). Appended to the ARENA piece (kCavern shading:
// the funnel reads as the wall continuing into a throat; the manifold
// verifier's breach allowance already covers its two designed boundary
// loops). Inner-ring azimuth matching is about the ring's own centroid (the
// ring sits off the breach axis by up to a node spacing, so axis-azimuths
// would fold onto a lobe).
void build_breach_collar(const world::TunnelNet& net, const Breach& b,
                         const std::vector<glm::dvec3>& inner, TunnelPiece& p) {
    const int n = static_cast<int>(inner.size());
    if (n < 3) return;
    const double R_local = glm::length(b.pos);
    if (R_local <= 0.0) return;
    const glm::dvec3 into = glm::normalize(b.into);
    const glm::dvec3 ax = b.pos / R_local;
    const double step = std::max(20.0, 0.5 * net.tube_width);

    // Project a point onto the arena wall along its own world direction (the
    // opening-rim landing for a vertex that has entered the open arena).
    const auto to_wall = [&](const glm::dvec3& q) -> glm::dvec3 {
        const double r = glm::length(q);
        glm::dvec3 s;
        if (r > 0.0 && arena_surface_point(net, r, q / r, s)) return s;
        return q;
    };
    // Nudge a wall point 5 m INTO the arena (against the ellipsoid outward
    // gradient) so the funnel draws in FRONT of the kept facets (no z-fight;
    // the ragged any-corner edge hides behind it; the T2 on/inside-net
    // invariant holds).
    const auto nudge_in = [&](glm::dvec3 s) -> glm::dvec3 {
        glm::dvec3 grad{0.0};
        for (int k = 0; k < 3; ++k) {
            glm::dvec3 e{0.0};
            e[k] = 0.5;
            grad[k] = world::ellipsoid_sd(s + e, net.arena) -
                      world::ellipsoid_sd(s - e, net.arena);
        }
        const double gl = glm::length(grad);
        if (gl > 1e-12) s -= (5.0 / gl) * grad;
        return s;
    };

    // T26 — AZIMUTHAL DENSIFICATION. The oblique bore drops a long thin tongue
    // of arena facets where it grazes the wall (many quads have a corner within
    // kBreachRenderCut_m of the bore over a long grazing stretch). The compact
    // 24-vert mouth-ring cross-section, marched onto the wall, left that tongue
    // under-covered on the trailing (-e1) side (the census's junction leaks).
    // Subdivide the mouth ring azimuthally so the throat + margin carry enough
    // chords to seal the tongue; keep the EXACT 24-vert seam ring and fan it
    // onto the dense ring (the tube<->collar no-crack seam is untouched).
    const int sub = std::max(1, kBreachCollarAzSub);
    const int nf = n * sub;
    std::vector<glm::dvec3> inner_f(static_cast<std::size_t>(nf));
    for (int s = 0; s < n; ++s) {
        const glm::dvec3& a0 = inner[static_cast<std::size_t>(s)];
        const glm::dvec3& a1 = inner[static_cast<std::size_t>((s + 1) % n)];
        for (int m = 0; m < sub; ++m) {
            const double t = static_cast<double>(m) / sub;
            inner_f[static_cast<std::size_t>(s * sub + m)] =
                a0 + (a1 - a0) * t;  // on/just inside the mouth ring (chord)
        }
    }

    // PHASE 0 — the exact seam ring (shared tube floats), fanned onto the dense
    // mouth ring (covers the thin chord-sag band; no crack with the tube).
    const unsigned int base_seam = static_cast<unsigned int>(p.cue.size());
    for (const glm::dvec3& v : inner) push_v(p, v, 1.0f);
    const unsigned int base_f0 = static_cast<unsigned int>(p.cue.size());
    for (const glm::dvec3& v : inner_f) push_v(p, v, 1.0f);
    stitch_rings_coarse_to_fine(p, n, sub, base_seam, base_f0);

    // PHASE 1 — THE THROAT. March the dense mouth ring along `into` (the bore
    // tangent into the arena). A vertex that has crossed INTO the open arena
    // (ellipsoid_sd < 0) is CLAMPED onto the arena wall (its own radial
    // landing) and stays there; a vertex still in rock keeps following the
    // tube. So the funnel follows the actual tube wall while it is rock-backed
    // and bends onto the arena wall EXACTLY where the bore pierces — it never
    // spans across open bore air.
    unsigned int prev_base = base_f0;
    std::vector<glm::dvec3> prev = inner_f;
    std::vector<char> landed(static_cast<std::size_t>(nf), 0);
    const int kMaxThroat = 12;
    for (int r = 1; r <= kMaxThroat; ++r) {
        std::vector<glm::dvec3> ring(static_cast<std::size_t>(nf));
        bool all_landed = true;
        for (int s = 0; s < nf; ++s) {
            if (landed[static_cast<std::size_t>(s)]) {
                ring[static_cast<std::size_t>(s)] =
                    prev[static_cast<std::size_t>(s)];
                continue;
            }
            const glm::dvec3 cand = inner_f[static_cast<std::size_t>(s)] +
                                    static_cast<double>(r) * step * into;
            if (world::ellipsoid_sd(cand, net.arena) < 0.0) {
                ring[static_cast<std::size_t>(s)] = nudge_in(to_wall(cand));
                landed[static_cast<std::size_t>(s)] = 1;
            } else {
                ring[static_cast<std::size_t>(s)] = cand;  // still rock-backed
                all_landed = false;
            }
        }
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (const glm::dvec3& v : ring) push_v(p, v, 1.0f);
        stitch_rings(p, nf, prev_base, base);
        prev_base = base;
        prev = ring;
        if (all_landed) break;
    }

    // PHASE 2 — THE MARGIN ANNULUS. From the opening-rim ring (now all on the
    // wall) push each vertex OUTWARD from the breach axis; lay kBreachCollar
    // Bands concentric on-wall bands out to kBreachCollarReach_m so the widened
    // annulus over-covers the oblique grazing tongue with fine triangulation
    // (voids impossible). Every band vertex is re-landed on the wall via the
    // shared arena_surface_point — never overhangs open air.
    const std::vector<glm::dvec3> rim = prev;      // landed rim (nf verts)
    const unsigned int rim_base = prev_base;
    const double reach_ang = kBreachCollarReach_m / R_local;
    const int bands = std::max(1, kBreachCollarBands);
    unsigned int band_prev_base = rim_base;
    for (int k = 1; k <= bands; ++k) {
        const double pad_ang = reach_ang * (static_cast<double>(k) / bands);
        std::vector<glm::dvec3> outer(static_cast<std::size_t>(nf));
        for (int s = 0; s < nf; ++s) {
            const glm::dvec3 w = rim[static_cast<std::size_t>(s)];
            const double wl = glm::length(w);
            const glm::dvec3 wd = wl > 0.0 ? w / wl : ax;
            const double cd = glm::clamp(glm::dot(wd, ax), -1.0, 1.0);
            const double ang = std::acos(cd);
            glm::dvec3 perp = wd - cd * ax;
            const double pl = glm::length(perp);
            if (pl < 1e-9) {
                outer[static_cast<std::size_t>(s)] =
                    nudge_in(to_wall(wd * R_local));
                continue;
            }
            perp /= pl;
            const double ang2 = ang + pad_ang;
            const glm::dvec3 wdir =
                glm::normalize(std::cos(ang2) * ax + std::sin(ang2) * perp);
            outer[static_cast<std::size_t>(s)] =
                nudge_in(to_wall(wdir * R_local));
        }
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (const glm::dvec3& v : outer) push_v(p, v, 1.0f);
        stitch_rings(p, nf, band_prev_base, base);
        band_prev_base = base;
    }
}

// A short straight tube (the connector capsule) between a and b, radius r.
void build_connector(const glm::dvec3& a, const glm::dvec3& b, double r,
                     const world::HeightField* g, double bare_R,
                     TunnelPiece& p) {
    const int seg = kTubeSegments;
    glm::dvec3 axis = b - a;
    double len = glm::length(axis);
    if (len < 1e-6) return;
    axis /= len;
    const glm::dvec3 up = glm::normalize(a);
    glm::dvec3 u = perp_seed(axis, up);
    glm::dvec3 w = glm::normalize(glm::cross(axis, u));

    const int rings = 2;  // straight => two rings suffice
    unsigned int prev_base = 0;
    for (int ri = 0; ri < rings; ++ri) {
        const glm::dvec3 c = a + axis * (len * ri / (rings - 1));
        const unsigned int rbase = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const double th = 2.0 * 3.14159265358979323846 * s / seg;
            const glm::dvec3 v = c + r * (std::cos(th) * u + std::sin(th) * w);
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        if (ri > 0) stitch_rings(p, seg, prev_base, rbase);
        prev_base = rbase;
    }
}

// A mouth collar: an annulus funnel from the terrain hole rim (outer ring)
// down to the tube end ring (inner ring == the EXACT tube end-ring verts —
// shared floats, no crack).
//
// `outer_r` is the outer radius [m] — caller derives it via collar_reach() so
// the collar spans the full jagged terrain edge beyond the cut disk.
//
// T5d TERRAIN-HUGGING BAND: the collar is a MULTI-RING annulus. The coarse
// pre-T5d 2-ring band chorded a single long radial line from the terrain rim
// down to the tube seam, which sliced through the fine (~59 m cell) terrain
// facets by tens of metres over the whole overlap annulus (the "portal
// strobe"). Instead we lay intermediate rings at ~cell-density radial spacing,
// each vertex tucked onto the LOCAL terrain (radius_at(dir) − tuck), so every
// terrain-band chord stays under the facets. `cell_arc_m` is the terrain grid
// cell size (terrain_cell_arc()); <= 0 => the legacy single-band fallback (the
// null-ground / no-grid path, bit-identical to pre-T5d).
//
// Z-fight tuck: 1.5 m below the terrain — meaningful now that the measured
// facet-vs-field undershoot at the mouths is ≤ 0.3 m (T5d probe). The INNERMOST
// ring stays the EXACT tube-end-ring verts (the seam pin is inviolate); the
// final strip from the last terrain ring onto it lives inside the cut hole
// (arc < cut_radius) where the terrain was excavated, so it fights nothing.
void build_collar(const glm::dvec3& mouth_dir, double outer_r,
                  const std::vector<glm::dvec3>& inner_ring,
                  const glm::dvec3& u, const glm::dvec3& w,
                  const world::HeightField* g, double bare_R, double cell_arc_m,
                  TunnelPiece& p) {
    const int seg_tube = kTubeSegments;
    // The terrain band uses more azimuth segments than the tube (finer chords)
    // when we have grid info; the legacy fallback stays at tube-segment count.
    const int seg = cell_arc_m > 0.0 ? kCollarBandSegments : seg_tube;
    constexpr double kTuckOuter_m = 1.5;  // z-fight tuck depth [m]

    const double R = surface_r(mouth_dir, g, bare_R);
    // A twist-free tangent frame phased to the tube end ring's own u axis.
    const glm::dvec3 tu = perp_seed(mouth_dir, u);
    const glm::dvec3 tw = glm::normalize(glm::cross(mouth_dir, tu));
    (void)w;  // (u,w) documented; only u seeds the phase, w kept for symmetry

    // Terrain rings from outer_r inward. The innermost terrain ring sits at
    // ~one cell arc so the final strip onto the exact tube-end ring is short
    // and lands inside the cut hole. Ring COUNT derives from the same terrain
    // cell arc collar_reach() uses — a subdiv/tiles retune moves it
    // automatically. cell_arc_m <= 0 (no grid info) => a SINGLE terrain ring at
    // outer_r (the legacy 2-ring band, bit-identical to pre-T5d — nterr=1, gaps
    // unused).
    const bool hug = cell_arc_m > 0.0;
    const double inner_r = hug ? std::min(cell_arc_m, outer_r) : outer_r;
    const int gaps = hug ? std::max(1, static_cast<int>(std::ceil(
                                           (outer_r - inner_r) / cell_arc_m)))
                         : 1;
    const int nterr = hug ? gaps + 1 : 1;  // terrain rings (outer_r .. inner_r)

    // Emit the terrain rings, each on the local surface minus the tuck.
    unsigned int prev_base = 0;
    for (int ri = 0; ri < nterr; ++ri) {
        const double f = gaps > 0 ? static_cast<double>(ri) / gaps : 0.0;
        const double r_here = outer_r + (inner_r - outer_r) * f;
        const double ang = r_here / R;  // small-angle great-circle radius
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const double a = 2.0 * 3.14159265358979323846 * s / seg;
            const glm::dvec3 dir = glm::normalize(
                std::cos(ang) * mouth_dir +
                std::sin(ang) * (std::cos(a) * tu + std::sin(a) * tw));
            const double sr = surface_r(dir, g, bare_R) - kTuckOuter_m;
            const glm::dvec3 v = dir * sr;
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        if (ri > 0) stitch_rings(p, seg, prev_base, base);
        prev_base = base;
    }

    // Inner ring: the EXACT tube end-ring verts (shared floats — the seam pin).
    // No tuck here — the inner ring must close the tube seam exactly. The band
    // ring reduces 2:1 onto it (kCollarBandSegments -> kTubeSegments) when the
    // band is finer; else a plain 1:1 stitch (the legacy fallback).
    const unsigned int inner_base = static_cast<unsigned int>(p.cue.size());
    for (int s = 0; s < seg_tube; ++s)
        push_v(p, inner_ring[static_cast<std::size_t>(s)],
               depth_cue(inner_ring[static_cast<std::size_t>(s)], g, bare_R));
    if (seg == seg_tube)
        stitch_rings(p, seg_tube, prev_base, inner_base);
    else
        stitch_rings_2to1(p, seg_tube, prev_base, inner_base);
}

// The MURRAY BOWL WALL (T4a): the visible open-pit wall. A multi-ring cone from
// the terrain rim (outer, at bowl_r on the surface) down to the tube end ring
// (inner, at the bowl floor — the EXACT tube end-ring floats, shared, no seam
// crack). Tagged is_collar so it draws sun-solid (an open pit is daylight
// terrain). Every wall vertex sits at |sd| ~ 0 on the bowl SDF (the single-
// source law extends to the pit wall). `outer_r` is the terrain rim radius [m].
//
// T5d TERRAIN-HUGGING RIM: the rim ring used ONE flat surface radius
// (bw.surface_r) for the whole ring, so around the ~59 m-cell terrain it poked
// up to +21 m above the fine facets (the same portal strobe as the Errington
// collar, at the rim). We lay TERRAIN-HUGGING rim rings from outer_r down to
// the actual bowl rim bw.bowl_r at ~cell-density (each vertex on radius_at(dir)
// − tuck), then the analytic open-pit cone continues INSIDE the excavated pit
// (bowl_r .. floor), where there is no terrain to fight. cell_arc_m <= 0 => the
// legacy single-tucked-rim fallback (null-ground / no-grid path).
void build_bowl_wall(const world::TunnelNet::Bowl& bw, double outer_r,
                     const std::vector<glm::dvec3>& inner_ring,
                     const world::HeightField* g, double bare_R,
                     double cell_arc_m, bool benched, TunnelPiece& p) {
    const int seg = kTubeSegments;  // the analytic cone azimuth
    const int seg_band =
        kCollarBandSegments;       // the terrain rim azimuth (finer)
    const int ncone = kBowlRings;  // analytic cone rings (bowl_r rim .. floor)
    constexpr double kTuckOuter_m = 1.5;  // z-fight tuck at the surface rim [m]

    // A tangent frame about the bowl axis, phased to the tube end ring's own
    // frame so the strip does not twist. Seed the phase from the inner ring's
    // first vert direction relative to the axis.
    const glm::dvec3 axis = bw.axis;
    glm::dvec3 seed = inner_ring.empty()
                          ? glm::dvec3{1, 0, 0}
                          : glm::normalize(inner_ring[0] - axis * bw.surface_r);
    glm::dvec3 tu = seed - glm::dot(seed, axis) * axis;
    if (glm::length(tu) < 1e-9) tu = perp_seed(axis, glm::dvec3{1, 0, 0});
    tu = glm::normalize(tu);
    const glm::dvec3 tw = glm::normalize(glm::cross(axis, tu));

    unsigned int prev_base = 0;
    bool first = true;

    // Does the outer collar materially overshoot the bowl rim (the app path)?
    // If so, the terrain-overlap band [bowl_r, outer_r] gets terrain-hugging
    // rings and the analytic cone starts one ring IN (its own flat rim ring
    // is replaced by the terrain rim ring at bowl_r — the join sits on terrain,
    // not the flat surface_r that poked +21 m). If not (the fallback: no grid
    // info, or outer_r<=bowl_r), the analytic cone owns its rim at outer_r and
    // tucks it below terrain (pre-T5d behavior, so P3-1's fallback rim stays at
    // outer_r).
    const bool hug_rim = (cell_arc_m > 0.0 && outer_r > bw.bowl_r + 1.0);

    // (1) TERRAIN-HUGGING RIM RINGS (T5d): from outer_r inward to and INCLUDING
    //     the bowl rim bw.bowl_r, at ~cell-density, each vertex on the LOCAL
    //     terrain − tuck. The innermost of these (at bowl_r) IS the pit rim —
    //     the analytic cone descends from it, so the surface->pit join is on
    //     terrain (no flat-surface_r poke at the cut edge).
    bool band_emitted = false;  // last emitted ring is a fine (seg_band) ring
    if (hug_rim) {
        const int gaps = std::max(
            1, static_cast<int>(std::ceil((outer_r - bw.bowl_r) / cell_arc_m)));
        for (int ri = 0; ri <= gaps; ++ri) {  // outer_r .. bowl_r inclusive
            const double f = static_cast<double>(ri) / gaps;  // 0 .. 1
            const double r_here = outer_r + (bw.bowl_r - outer_r) * f;
            const double ang = r_here / bw.surface_r;
            const unsigned int base = static_cast<unsigned int>(p.cue.size());
            for (int s = 0; s < seg_band; ++s) {
                const double a = 2.0 * 3.14159265358979323846 * s / seg_band;
                const glm::dvec3 dir = glm::normalize(
                    std::cos(ang) * axis +
                    std::sin(ang) * (std::cos(a) * tu + std::sin(a) * tw));
                const double sr = surface_r(dir, g, bare_R) - kTuckOuter_m;
                const glm::dvec3 v = dir * sr;
                push_v(p, v, depth_cue(v, g, bare_R));
            }
            if (!first) stitch_rings(p, seg_band, prev_base, base);
            prev_base = base;
            first = false;
        }
        band_emitted = true;
    }

    // (2) THE OPEN-PIT WALL below the rim, down to the floor. The inner (last)
    //     ring is the EXACT tube end-ring floats (the seam pin). Two shapes:
    //     a SMOOTH analytic cone (the Errington entry pit) or STEPPED BENCHES
    //     (the Murray open pit, T16 ENTRANCE-ART). A ring is emitted by this
    //     lambda: it pushes the ring verts (inner => the shared tube floats)
    //     and stitches to the previous ring (2:1 once, joining a fine terrain
    //     band; plain 1:1 otherwise).
    const double cone_rim_r = hug_rim ? bw.bowl_r : outer_r;
    auto emit_ring = [&](double ring_r, double depth, bool inner,
                         bool rim_tuck) {
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            glm::dvec3 v;
            if (inner) {
                v = inner_ring[static_cast<std::size_t>(s)];  // exact seam
            } else {
                const double a = 2.0 * 3.14159265358979323846 * s / seg;
                const glm::dvec3 radial = std::cos(a) * tu + std::sin(a) * tw;
                const double tuck = rim_tuck ? kTuckOuter_m : 0.0;
                v = axis * (bw.surface_r - depth - tuck) + radial * ring_r;
            }
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        if (!first) {
            if (band_emitted)
                stitch_rings_2to1(p, seg, prev_base, base);
            else
                stitch_rings(p, seg, prev_base, base);
        }
        prev_base = base;
        first = false;
        band_emitted = false;  // only the terrain->wall join is 2:1
    };

    // T17 — THE MOUTH ADAPTER RING (fly-13 root cause #3, probe-measured). The
    // final band used to stitch the last analytic funnel ring (a HORIZONTAL
    // circle in bowl-azimuth order) 1:1 onto the tube end ring — but a bore
    // that leaves the pit floor near-horizontally has a near-VERTICAL end
    // ring, so index-matched azimuths point in wildly different 3D directions
    // and the band TWISTS: its facets tented over the arch from every side
    // (the Murray "black knoll" sealing the mouth; rounds 11/12 flew through
    // it blind) and swept across the bore interior below the floor. THE FIX:
    // route the seam through an ADAPTER ring — the inner ring's verts mapped
    // to their own FOOTPRINT azimuths about the bowl axis, placed on the last
    // analytic circle (same radius/depth). prev -> adapter is a same-circle
    // sliver band (cannot twist); adapter -> inner runs RADIALLY toward each
    // ring vert (an apron below the floor, side walls, a back drape behind
    // the arch crown — and NOTHING over the opening). The inner ring stays
    // the EXACT tube floats (the seam pin); the band stays sealed (no new
    // boundary edges), so the leak audit's coverage carries over.
    const auto emit_inner_via_adapter = [&](double prev_r, double prev_depth) {
        // The inner verts' footprint azimuths about the bowl axis, in the
        // (tu, tw) frame. Degenerate footprint (a vert on the axis) falls
        // back to the seed azimuth 0 — ordering survives, the sliver shrinks.
        struct AV {
            double b;
            int s;
        };
        std::vector<AV> av(static_cast<std::size_t>(seg));
        for (int s = 0; s < seg; ++s) {
            const glm::dvec3& rv = inner_ring[static_cast<std::size_t>(s)];
            const glm::dvec3 lat = rv - glm::dot(rv, axis) * axis;
            const double ll = glm::length(lat);
            const glm::dvec3 dir = ll > 1e-6 ? lat / ll : tu;
            av[static_cast<std::size_t>(s)] = {
                std::atan2(glm::dot(dir, tw), glm::dot(dir, tu)), s};
        }
        std::sort(av.begin(), av.end(),
                  [](const AV& x, const AV& y) { return x.b < y.b; });
        // (1) The adapter circle, emitted in ASCENDING bowl azimuth so the
        //     incoming 1:1 stitch from the same-shaped previous circle cannot
        //     bowtie. pi[] remembers where each inner index landed.
        std::vector<int> pi_of(static_cast<std::size_t>(seg));
        const unsigned int abase = static_cast<unsigned int>(p.cue.size());
        // The adapter sits RECESSED below and INSET inside the last analytic
        // circle. Recessed (T17): coincident circles would weld a 3-surface
        // junction onto one edge ring and read non-manifold. DEEP + inset
        // (T21, fly-16 "weird artefacts like stretchy arms at mouth of
        // murray"): the uniform->footprint-warped stitch band is a phase-
        // SHEARED ribbon of long thin quads — at 0.75 m it lay in the floor
        // plane and read as pale spokes radiating around the mouth; at 6 m
        // down and 4 m in, the whole ribbon (and the pi-band roots) tucks
        // UNDER the floor disk, which covers it from every airborne angle.
        const double a_depth = prev_depth + 6.0;
        const double a_r = std::max(prev_r - 4.0, 1.0);
        for (int j = 0; j < seg; ++j) {
            pi_of[static_cast<std::size_t>(av[static_cast<std::size_t>(j)].s)] =
                j;
            const double b = av[static_cast<std::size_t>(j)].b;
            const glm::dvec3 dir = std::cos(b) * tu + std::sin(b) * tw;
            const glm::dvec3 v = axis * (bw.surface_r - a_depth) + dir * a_r;
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        if (!first) stitch_rings(p, seg, prev_base, abase);
        // (2) The EXACT inner ring, joined radially via the adapter mapping.
        //     pi_of is a monotone circular bijection (possibly reversed — the
        //     footprint of a tilted ring can run the other way), so the quads
        //     tile the annulus exactly once; the FS is two-sided, winding is
        //     not load-bearing.
        const unsigned int ibase = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const glm::dvec3& v = inner_ring[static_cast<std::size_t>(s)];
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        for (int s = 0; s < seg; ++s) {
            const int sn = (s + 1) % seg;
            const unsigned short a0 = static_cast<unsigned short>(
                abase + pi_of[static_cast<std::size_t>(s)]);
            const unsigned short a1 = static_cast<unsigned short>(
                abase + pi_of[static_cast<std::size_t>(sn)]);
            const unsigned short i0 = static_cast<unsigned short>(ibase + s);
            const unsigned short i1 = static_cast<unsigned short>(ibase + sn);
            p.indices.push_back(a0);
            p.indices.push_back(a1);
            p.indices.push_back(i1);
            p.indices.push_back(a0);
            p.indices.push_back(i1);
            p.indices.push_back(i0);
        }
        prev_base = ibase;
        first = false;
        band_emitted = false;
    };

    if (benched) {
        // STEPPED BENCHES: rim -> (vertical FACE + horizontal BERM) x kBowl
        // Benches -> floor. Ring idx 0 = rim (cone_rim_r, depth 0); idx 2k+1 =
        // face-k bottom (radius cone_rim_r - k*dr, depth (k+1)*dd); idx 2k+2 =
        // berm-k (radius cone_rim_r - (k+1)*dr, same depth). The last berm
        // lands at (floor_r, bowl_depth) == the tube end ring. Each face radius
        // is the MAX of the smooth-cone radius over its depth span and each
        // berm radius its MIN, so the whole stepped surface stays at-or-outside
        // the smooth funnel (carving OUTWARD into rock, never inward into the
        // open flight volume — collision, the bowl_r cylinder, is never
        // protruded). When a fine terrain band owns the rim (hug_rim) the
        // staircase starts at the first face (idx 1, 2:1-joined to the band).
        const int Nb = kBowlBenches;
        const double dr = (cone_rim_r - bw.floor_r) / Nb;
        const double dd = bw.bowl_depth / Nb;
        const int nstair = 2 * Nb;  // last ring index (the tube-end seam)
        const int start = hug_rim ? 1 : 0;
        for (int idx = start; idx < nstair; ++idx) {  // T17: inner via adapter
            double rr, dep;
            if (idx == 0) {
                rr = cone_rim_r;
                dep = 0.0;
            } else {
                const int k = (idx - 1) / 2;          // bench index
                const bool is_face = (idx % 2 == 1);  // odd == a vertical face
                dep = (k + 1) * dd;
                rr = is_face ? cone_rim_r - k * dr : cone_rim_r - (k + 1) * dr;
            }
            emit_ring(rr, dep, /*inner=*/false, idx == 0 && !hug_rim);
        }
        // The tube-end seam, via the T17 adapter (the last analytic circle is
        // face-(Nb-1)'s bottom: radius cone_rim_r - (Nb-1)*dr at full depth).
        const double floor_edge_r = cone_rim_r - (Nb - 1) * dr;
        emit_inner_via_adapter(floor_edge_r, bw.bowl_depth);
        // T17 — THE PIT FLOOR DISK (Murray/benched only). The bore leaves the
        // pit floor half-buried: its crown stands above floor depth out to
        // ~2 node spacings behind the arch, so the band facets there CHORD
        // through the bore and the bore-swallow trim drops them — leaving the
        // floor strips BESIDE the half-open bore uncovered (a see-through
        // into below-floor rock). A flat ringed disk at bowl_depth covers the
        // whole inner floor; the bore-swallow trim then cuts the open-cut
        // SLOT through it exactly where the bore is (rings, not one fan, so
        // the cut stays local — the trench-floor T17 lesson). The smooth
        // Errington pit needs none: its cone tapers to the ring and keeps
        // covering the beside-bore shell.
        {
            constexpr int kPitFloorRings = 4;
            // 0.5 m below the collision floor (rock-side): the disk's outer
            // circle must not coincide with the funnel's floor-edge ring (the
            // 3-surface weld above) nor the adapter (0.75 down).
            const glm::dvec3 fc = axis * (bw.surface_r - bw.bowl_depth - 0.5);
            unsigned int prev_floor = 0;
            bool floor_first = true;
            for (int ri = 0; ri <= kPitFloorRings - 1; ++ri) {
                const double rr =
                    floor_edge_r *
                    (1.0 - static_cast<double>(ri) / kPitFloorRings);
                const unsigned int base =
                    static_cast<unsigned int>(p.cue.size());
                for (int s = 0; s < seg; ++s) {
                    const double a2 = 2.0 * 3.14159265358979323846 * s / seg;
                    const glm::dvec3 radial =
                        std::cos(a2) * tu + std::sin(a2) * tw;
                    const glm::dvec3 v = fc + radial * rr;
                    push_v(p, v, depth_cue(v, g, bare_R));
                }
                if (!floor_first) stitch_rings(p, seg, prev_floor, base);
                prev_floor = base;
                floor_first = false;
            }
            const unsigned int cen = static_cast<unsigned int>(p.cue.size());
            push_v(p, fc, depth_cue(fc, g, bare_R));
            for (int s = 0; s < seg; ++s) {
                const unsigned short a0 =
                    static_cast<unsigned short>(prev_floor + s);
                const unsigned short b0 =
                    static_cast<unsigned short>(prev_floor + (s + 1) % seg);
                p.indices.push_back(static_cast<unsigned short>(cen));
                p.indices.push_back(a0);
                p.indices.push_back(b0);
            }
        }
        // T26 — THE SUB-FLOOR EDGE CURB (2 rings). A grazing sightline from
        // inside the descending Murray bore threaded UNDER the cone-bottom /
        // pit-floor-edge terminus into the rock ~26 m below the floor, just
        // outboard of the bore (the census's single Murray-mouth sliver:
        // tube_sd ~120, bowl_sd ~+27, no occluder in front). The pit floor disk
        // seals the floor PLANE but the cone STOPS at its floor edge, leaving no
        // occluder hanging below that terminus. Drape a short skirt DOWN AND
        // slightly OUT from the floor-disk edge so no grazing ray reaches behind
        // it. Kept WITHIN ONE BENCH TOOTH of depth so every curb vert stays
        // within tooth of the net surface (the T16 wall-shape bound); it is
        // appended AFTER the bench rings so the monotone bench-profile checks do
        // not cover it (over-cover allowed, voids impossible).
        {
            const double tooth =
                (bw.bowl_r - bw.floor_r) / static_cast<double>(kBowlBenches);
            const double curb_drop = std::max(30.0, 0.6 * tooth);
            const double curb_flare = 0.5 * tooth;
            const glm::dvec3 top_c = axis * (bw.surface_r - bw.bowl_depth - 0.5);
            const glm::dvec3 bot_c =
                axis * (bw.surface_r - bw.bowl_depth - 0.5 - curb_drop);
            const unsigned int ctop = static_cast<unsigned int>(p.cue.size());
            for (int s = 0; s < seg; ++s) {
                const double a2 = 2.0 * 3.14159265358979323846 * s / seg;
                const glm::dvec3 radial = std::cos(a2) * tu + std::sin(a2) * tw;
                const glm::dvec3 v = top_c + radial * floor_edge_r;
                push_v(p, v, depth_cue(v, g, bare_R));
            }
            const unsigned int cbot = static_cast<unsigned int>(p.cue.size());
            for (int s = 0; s < seg; ++s) {
                const double a2 = 2.0 * 3.14159265358979323846 * s / seg;
                const glm::dvec3 radial = std::cos(a2) * tu + std::sin(a2) * tw;
                const glm::dvec3 v =
                    bot_c + radial * (floor_edge_r + curb_flare);
                push_v(p, v, depth_cue(v, g, bare_R));
            }
            stitch_rings(p, seg, ctop, cbot);
        }
    } else {
        // SMOOTH CONE (the Errington entry pit): ncone rings, rim -> floor.
        const int ri_start = hug_rim ? 1 : 0;  // band owns the rim when hugging
        for (int ri = ri_start; ri < ncone - 1; ++ri) {  // T17: inner adapted
            const double f = static_cast<double>(ri) / (ncone - 1);  // 0..1
            emit_ring(cone_rim_r + (bw.floor_r - cone_rim_r) * f,
                      bw.bowl_depth * f, /*inner=*/false, ri == 0 && !hug_rim);
        }
        const double fp = static_cast<double>(ncone - 2) / (ncone - 1);
        emit_inner_via_adapter(cone_rim_r + (bw.floor_r - cone_rim_r) * fp,
                               bw.bowl_depth * fp);
    }
}

// T16 DEFECT-2 — THE APPROACH-TRENCH WALLS. Each entry-trench step is an
// open-cut Bowl (a cylinder of radius bowl_r down to a flat floor at
// bowl_depth) the terrain mesh drops triangles over — but NOTHING covered the
// cut, so from the approach ramp a sightline through the backface-invisible cut
// floor escaped to space (Chad round-12: "a hole near Errington where I can see
// through to the other side of the earth"). This drapes each step: a terrain-
// hugging RIM band reaching past the ragged cut edge (outer_r = collar_reach),
// a vertical WALL down to the flat floor, and a FLOOR disk — so the open cut
// reads as solid earth and no sightline escapes. The wall/floor sit on the
// bowl_sd cylinder (radius bowl_r, floor at bowl_depth) so the visible surface
// == the survivable volume (the T6-P0 single-source discipline). kCollar
// (daylight rock), appended to the Errington mouth piece (no piece-count
// change). bowl_depth <= 0 => inert (structurally absent).
void build_trench_step(const world::TunnelNet::Bowl& bw, double outer_r,
                       const world::HeightField* g, double bare_R,
                       double cell_arc_m, TunnelPiece& p) {
    if (bw.bowl_depth <= 0.0) return;
    const double pi = 3.14159265358979323846;
    const int seg =
        kCollarBandSegments;  // 48 (fine rim chords over ~59 m cells)
    constexpr double kTuck = 1.5;
    const glm::dvec3 axis = bw.axis;
    glm::dvec3 tu = perp_seed(axis, glm::dvec3{1, 0, 0});
    const glm::dvec3 tw = glm::normalize(glm::cross(axis, tu));

    unsigned int prev_base = 0;
    bool first = true;
    const auto emit_surface_ring = [&](double r_here) {
        const double ang = r_here / bw.surface_r;
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const double a = 2.0 * pi * s / seg;
            const glm::dvec3 dir = glm::normalize(
                std::cos(ang) * axis +
                std::sin(ang) * (std::cos(a) * tu + std::sin(a) * tw));
            const double sr = surface_r(dir, g, bare_R) - kTuck;
            const glm::dvec3 v = dir * sr;
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        if (!first) stitch_rings(p, seg, prev_base, base);
        prev_base = base;
        first = false;
    };
    // (1) TERRAIN-HUGGING RIM BAND: outer_r (past the ragged cut) down to the
    //     cut rim bowl_r, at ~cell density.
    const bool hug = cell_arc_m > 0.0 && outer_r > bw.bowl_r + 1.0;
    if (hug) {
        const int gaps = std::max(
            1, static_cast<int>(std::ceil((outer_r - bw.bowl_r) / cell_arc_m)));
        for (int ri = 0; ri <= gaps; ++ri)
            emit_surface_ring(outer_r + (bw.bowl_r - outer_r) *
                                            static_cast<double>(ri) / gaps);
    } else {
        emit_surface_ring(std::max(outer_r, bw.bowl_r));
    }
    // (2) THE VERTICAL WALL to the flat floor (radius bowl_r at bowl_depth).
    const unsigned int floor_ring = static_cast<unsigned int>(p.cue.size());
    for (int s = 0; s < seg; ++s) {
        const double a = 2.0 * pi * s / seg;
        const glm::dvec3 radial = std::cos(a) * tu + std::sin(a) * tw;
        const glm::dvec3 v =
            axis * (bw.surface_r - bw.bowl_depth) + radial * bw.bowl_r;
        push_v(p, v, depth_cue(v, g, bare_R));
    }
    stitch_rings(p, seg, prev_base, floor_ring);
    // (3) THE FLOOR DISK — seals the downward sightline. CONCENTRIC RINGS +
    // a small centre fan, NOT one big fan (T17): the cut-swallowed trim drops
    // triangles per-vertex, and a full-disk fan shares its centre vertex with
    // every triangle — one swallowed centre (the steps overlap; a shallower
    // step's floor centre sits inside its deeper neighbour's cut) would erase
    // the ENTIRE disk including its rock-backed half and re-open the
    // see-through this drape exists to seal. Rings localize the drop to the
    // genuinely-overlapped quads.
    const glm::dvec3 fc = axis * (bw.surface_r - bw.bowl_depth);
    constexpr int kFloorDiskRings = 4;  // bowl_r -> bowl_r/4, then the fan
    unsigned int prev_floor = floor_ring;
    for (int ri = 1; ri < kFloorDiskRings; ++ri) {
        const double rr =
            bw.bowl_r * (1.0 - static_cast<double>(ri) / kFloorDiskRings);
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const double a = 2.0 * pi * s / seg;
            const glm::dvec3 radial = std::cos(a) * tu + std::sin(a) * tw;
            const glm::dvec3 v = fc + radial * rr;
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        stitch_rings(p, seg, prev_floor, base);
        prev_floor = base;
    }
    const unsigned int cen = static_cast<unsigned int>(p.cue.size());
    push_v(p, fc, depth_cue(fc, g, bare_R));
    for (int s = 0; s < seg; ++s) {
        const unsigned short a = static_cast<unsigned short>(prev_floor + s);
        const unsigned short bb =
            static_cast<unsigned short>(prev_floor + (s + 1) % seg);
        p.indices.push_back(static_cast<unsigned short>(cen));
        p.indices.push_back(a);
        p.indices.push_back(bb);
    }
}

// T17 — THE CUT-SWALLOWED TRIM (contract + defect ledger in tunnel_mesh.h).
// Drop every triangle of `p` that has ANY vertex swallowed by an open-cut Bowl
// volume (bowl_sd < -kCutSwallowEps_m against the Murray bowl, the Errington
// pit, or a trench step) — rendered rock strictly inside collision-open air is
// a wall the pilot cannot fly (the Murray mouth dome / the Errington trench
// lid). ANY-vertex (not centroid): the defect facets are mostly STRADDLERS —
// a mouth-ring crown vert 85 m up in the open pit stitched to a ring vert deep
// in rock averages to a rock-side centroid and would survive a centroid test
// while its spike still plugs the mouth. Vertices ON a facet's own cut
// boundary sit at sd ~ 0 (chord sag < 1 m) — never dropped at the 5 m eps.
// Indices-only surgery: vertices stay, so end-ring float captures and every
// exact-seam consumer are untouched.
// `cuts` (T17) is the EXPLICIT list of open-cut volumes this piece must yield
// to — per-piece scoping is load-bearing, audit-measured both ways:
//  * The TUBE / FLOOR yield ONLY to the Murray bowl. At Murray the
//    above-floor crown is a lie in a wide-open pit (the dome); at ERRINGTON
//    the whole mouth ring sits inside the tight recess (pit rim 143 m around
//    a 110 m bore — mouth_sink DESIGNED the adit to emerge through the open
//    cut), so yielding to the pit gutted the first bore segment and the pit
//    eye saw 79 rock-void leaks through the wall-less stretch.
//  * A mouth collar yields to everything EXCEPT its own cut (its decorative
//    funnel/benches live inside their own T6-P0 cylinder by design, but its
//    terrain band floated straight across the trench channel — the lid).
//  * The trench drapes yield to every cut (their overlaps ARE the defect).
// `cone_cut` (optional) is a cut this piece yields to only within its VISUAL
// CONE — the crater surface its funnel mesh actually renders (bowl_r at the
// surface tapering to floor_r at depth) — NOT its full T6-P0 collision
// cylinder. Audit-measured: trench-floor facets that yielded to the Errington
// pit's CYLINDER opened sightlines into the shell between the rendered cone
// and the cylinder (visually-solid-but-open air with NO rendered boundary),
// straight out into rock void. The cone expression is the render's own
// build_bowl_wall shape — a render-side visual volume, not a collision fork.
double visual_cone_sd(const glm::dvec3& q, const world::TunnelNet::Bowl& bw) {
    if (bw.bowl_depth <= 0.0) return std::numeric_limits<double>::infinity();
    const glm::dvec3 s_surf = bw.axis * bw.surface_r;
    const glm::dvec3 d = q - s_surf;
    const double ax = -glm::dot(d, bw.axis);  // depth below the surface [m]
    const glm::dvec3 radial = d - glm::dot(d, bw.axis) * bw.axis;
    const double rad = glm::length(radial);
    const double f = glm::clamp(ax / bw.bowl_depth, 0.0, 1.0);
    const double r_cone = bw.bowl_r + (bw.floor_r - bw.bowl_r) * f;
    // A conservative metre-scale inside measure: all three negative == inside.
    return std::max({rad - r_cone, -ax, ax - bw.bowl_depth});
}

void drop_cut_swallowed_tris(
    const world::TunnelNet& net,
    std::initializer_list<const world::TunnelNet::Bowl*> cuts, TunnelPiece& p,
    const world::TunnelNet::Bowl* cone_cut = nullptr, int split_depth = 3) {
    (void)net;  // cuts carry their own Bowls; kept for call-site symmetry
    const auto cut_sd = [&](const glm::dvec3& q) {
        double sd = std::numeric_limits<double>::infinity();
        for (const world::TunnelNet::Bowl* bw : cuts)
            // apply_cap=false (T27-REV): the collision-only mouth cap must move
            // no rendered triangle — the trim reads the legacy pre-T27 cut.
            sd = std::min(sd, world::bowl_sd(q, *bw, /*apply_cap=*/false));
        if (cone_cut) sd = std::min(sd, visual_cone_sd(q, *cone_cut));
        return sd;
    };
    const auto inside = [&](const glm::dvec3& q) {
        return cut_sd(q) < -kCutSwallowEps_m;
    };
    // Per-vertex verdict cached once (a vertex is shared by ~6 triangles).
    const std::size_t nv = p.cue.size();
    std::vector<char> swallowed(nv, 0);
    for (std::size_t i = 0; i < nv; ++i) {
        const glm::dvec3 v{p.positions[3 * i + 0], p.positions[3 * i + 1],
                           p.positions[3 * i + 2]};
        swallowed[i] = inside(v) ? 1 : 0;
    }
    // BORDER facets are SUBDIVIDED, not axed (audit-measured: any-vertex
    // dropping opened rock-void slivers at every cut junction — the dropped
    // straddler's rock-backed half lost its cover; the trench surface eyes
    // each read 5-9 see-through leaks). Fully-outside facets keep their
    // original shared-vertex triangle; fully-inside facets drop; a MIXED
    // facet is split at edge midpoints (depth <= 2, leaves ~1/4 the facet
    // span) and only fully-inside leaves drop — a mixed LEAF is kept, so the
    // trim can over-COVER by a leaf but can never open a void.
    struct Leaf {
        glm::dvec3 v[3];
        float c[3];
        int depth;
    };
    std::vector<Leaf> stack;
    std::vector<Leaf> emit;
    std::vector<unsigned short> kept;
    kept.reserve(p.indices.size());
    const int kSplitDepth =
        split_depth;  // T18: depth 2 left ~10 m leaf flaps
                      // at the mouths (fly-14); depth 3
                      // halved them; T19 passes 4 for the
                      // two MOUTH collars (kMouthSplitDepth)
                      // so the pale border flaps halve again
    for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
        const unsigned short i0 = p.indices[t], i1 = p.indices[t + 1],
                             i2 = p.indices[t + 2];
        const int nin = swallowed[i0] + swallowed[i1] + swallowed[i2];
        if (nin == 0) {  // untouched: keep the shared-vertex original
            kept.push_back(i0);
            kept.push_back(i1);
            kept.push_back(i2);
            continue;
        }
        if (nin == 3) continue;  // fully swallowed by an open cut: drop
        const auto vat = [&](unsigned short i) {
            return glm::dvec3{p.positions[3 * i + 0], p.positions[3 * i + 1],
                              p.positions[3 * i + 2]};
        };
        stack.push_back({{vat(i0), vat(i1), vat(i2)},
                         {p.cue[i0], p.cue[i1], p.cue[i2]},
                         0});
        while (!stack.empty()) {
            const Leaf L = stack.back();
            stack.pop_back();
            const int lin = (inside(L.v[0]) ? 1 : 0) +
                            (inside(L.v[1]) ? 1 : 0) + (inside(L.v[2]) ? 1 : 0);
            if (lin == 3) continue;  // swallowed leaf: drop
            if (lin == 0 || L.depth >= kSplitDepth) {
                emit.push_back(L);  // outside or mixed-at-floor
                continue;
            }
            const glm::dvec3 m01 = 0.5 * (L.v[0] + L.v[1]);
            const glm::dvec3 m12 = 0.5 * (L.v[1] + L.v[2]);
            const glm::dvec3 m20 = 0.5 * (L.v[2] + L.v[0]);
            const float c01 = 0.5f * (L.c[0] + L.c[1]);
            const float c12 = 0.5f * (L.c[1] + L.c[2]);
            const float c20 = 0.5f * (L.c[2] + L.c[0]);
            const int d = L.depth + 1;
            stack.push_back({{L.v[0], m01, m20}, {L.c[0], c01, c20}, d});
            stack.push_back({{m01, L.v[1], m12}, {c01, L.c[1], c12}, d});
            stack.push_back({{m20, m12, L.v[2]}, {c20, c12, L.c[2]}, d});
            stack.push_back({{m01, m12, m20}, {c01, c12, c20}, d});
        }
    }
    p.indices = std::move(kept);
    for (const Leaf& L : emit) {
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int k = 0; k < 3; ++k) {
            p.positions.push_back(static_cast<float>(L.v[k].x));
            p.positions.push_back(static_cast<float>(L.v[k].y));
            p.positions.push_back(static_cast<float>(L.v[k].z));
            p.cue.push_back(L.c[k]);
            p.indices.push_back(static_cast<unsigned short>(base + k));
        }
    }
}

// T17 — THE BORE-SWALLOW TRIM for the two MOUTH pieces (probe-measured: with
// the tent/twist gone, the swing into the Murray bore still hit kCollar just
// inside the ring plane at floor height). The funnel's full-depth rings are
// FULL circles about the bowl axis, and behind a near-horizontal arch the
// bore's own interior rises above the pit-floor depth — so the deep funnel
// arcs on the into-hill side have ALWAYS pierced the bore. The benches are
// deliberately inside the BOWL volume (render-only terraces), but NO mouth
// facet legitimately sits inside the TUBE volume: drop every collar facet
// with a vertex strictly inside the bore (net.tube_signed_distance — the T16
// single-sourced pure tube term — under -kCutSwallowEps_m). Seam verts sit ON
// the tube surface (sd ~ 0) and are never dropped. Exposed behind the drops
// is the bore interior, bounded by rendered tube wall — leak-safe by
// construction, re-certified by the T16 audit.
// T23 (fly round-16 tail, Chad: "I went straight down my mine and died from
// something invisible. I can see through the bottom where the murray pit
// meets murray tunnel"): whole-facet dropping against the tube/throat opened
// the slot cut through the FLOOR DISK with up-to-facet-size overshoot (~40 m
// ring x azimuth facets at the disk edge), and since T21 tucked the adapter
// bands 6 m down + 4 m in, NOTHING renders behind the overshoot beside the
// slot walls — a see-through annular sliver at the pit-floor/slot junction
// that only a near-VERTICAL sightline threads (the T18 steep fans at 70-80
// degrees hit the funnel wall first; his dive was 90). The fix is the same
// discipline the cut trim already has: SUBDIVIDE mixed/chording facets and
// drop only fully-swallowed leaves — voids impossible, over-cover allowed.
// split_depth 0 reproduces the legacy whole-facet drop BIT-identically (any
// probe point inside => drop) — the T23 kill test's mutation arm.
void drop_tube_swallowed_tris(const world::TunnelNet& net, TunnelPiece& p,
                              int split_depth = kMouthSplitDepth) {
    // Verts + edge midpoints + centroid: the bore is CONVEX, so a facet
    // whose corners all sit on/outside the surface can still CHORD through
    // the interior (probe-measured: the band facet blocking the bore swing
    // had every vertex within the seam tolerance) — an interior chord always
    // carries an interior midpoint or centroid.
    const auto inside = [&](const glm::dvec3& q) {
        // T18: the Murray mouth THROAT is flyable air too — the floor-disk
        // slot must open over it. throat_signed_distance is already the
        // below-floor wedge (hard-capped at the pit-floor plane in the SDF
        // itself), so collision and this trim read ONE shape.
        return std::min(net.tube_signed_distance(q),
                        net.throat_signed_distance(q)) < -kCutSwallowEps_m;
    };
    // 0 = clean (keep), 1 = touched (mixed/chording), 2 = fully swallowed.
    const auto classify = [&](const glm::dvec3& a, const glm::dvec3& b,
                              const glm::dvec3& c) {
        const int nin =
            (inside(a) ? 1 : 0) + (inside(b) ? 1 : 0) + (inside(c) ? 1 : 0);
        if (nin == 3) return 2;
        const bool chord = nin > 0 || inside(0.5 * (a + b)) ||
                           inside(0.5 * (b + c)) || inside(0.5 * (c + a)) ||
                           inside((a + b + c) / 3.0);
        return chord ? 1 : 0;
    };
    const auto vtx = [&](std::size_t i) {
        return glm::dvec3{p.positions[3 * i + 0], p.positions[3 * i + 1],
                          p.positions[3 * i + 2]};
    };
    struct Leaf {
        glm::dvec3 v[3];
        float c[3];
        int depth;
    };
    std::vector<Leaf> stack;
    std::vector<Leaf> emit;
    std::vector<unsigned short> kept;
    kept.reserve(p.indices.size());
    for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
        const unsigned short i0 = p.indices[t], i1 = p.indices[t + 1],
                             i2 = p.indices[t + 2];
        const int verdict = classify(vtx(i0), vtx(i1), vtx(i2));
        if (verdict == 0) {  // untouched: keep the shared-vertex original
            kept.push_back(i0);
            kept.push_back(i1);
            kept.push_back(i2);
            continue;
        }
        if (verdict == 2 || split_depth <= 0)
            continue;  // fully swallowed — or the legacy whole-facet arm
        stack.push_back({{vtx(i0), vtx(i1), vtx(i2)},
                         {p.cue[i0], p.cue[i1], p.cue[i2]},
                         0});
        while (!stack.empty()) {
            const Leaf L = stack.back();
            stack.pop_back();
            const int lv = classify(L.v[0], L.v[1], L.v[2]);
            if (lv == 2) continue;  // swallowed leaf: drop
            if (lv == 0 || L.depth >= split_depth) {
                emit.push_back(L);  // clean, or mixed-at-floor (over-cover)
                continue;
            }
            const glm::dvec3 m01 = 0.5 * (L.v[0] + L.v[1]);
            const glm::dvec3 m12 = 0.5 * (L.v[1] + L.v[2]);
            const glm::dvec3 m20 = 0.5 * (L.v[2] + L.v[0]);
            const float c01 = 0.5f * (L.c[0] + L.c[1]);
            const float c12 = 0.5f * (L.c[1] + L.c[2]);
            const float c20 = 0.5f * (L.c[2] + L.c[0]);
            const int d = L.depth + 1;
            stack.push_back({{L.v[0], m01, m20}, {L.c[0], c01, c20}, d});
            stack.push_back({{m01, L.v[1], m12}, {c01, L.c[1], c12}, d});
            stack.push_back({{m20, m12, L.v[2]}, {c20, c12, L.c[2]}, d});
            stack.push_back({{m01, m12, m20}, {c01, c12, c20}, d});
        }
    }
    p.indices = std::move(kept);
    for (const Leaf& L : emit) {
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int k = 0; k < 3; ++k) {
            p.positions.push_back(static_cast<float>(L.v[k].x));
            p.positions.push_back(static_cast<float>(L.v[k].y));
            p.positions.push_back(static_cast<float>(L.v[k].z));
            p.cue.push_back(L.c[k]);
            p.indices.push_back(static_cast<unsigned short>(base + k));
        }
    }
}

// T17 — THE PIT CUT SLEEVE (audit-measured: with the trench drapes correctly
// yielding to the deeper pit, sightlines entered the pit's open cylinder and
// exited through its SIDE into rock — the T6-P0 cylinder is collision truth
// but the cone mesh renders only the tapered crater INSIDE it, so the shell
// between cone and cylinder has no rendered boundary). Render the cut's own
// boundary once: a vertical cylinder wall at bowl_r down to the floor plus a
// floor annulus in to floor_r. Sits BEHIND the crater cone (invisible except
// through designed gaps, where it reads as the crater wall); the bore-swallow
// trim afterwards cuts the adit opening through it where the bore crosses.
void build_pit_sleeve(const world::TunnelNet::Bowl& bw,
                      const world::HeightField* g, double bare_R,
                      TunnelPiece& p) {
    if (bw.bowl_depth <= 0.0) return;
    const int seg = kTubeSegments;
    const double pi = 3.14159265358979323846;
    constexpr double kTuck = 1.5;  // below the surface (z-fight vs terrain)
    const glm::dvec3 axis = bw.axis;
    glm::dvec3 tu = perp_seed(axis, glm::dvec3{1, 0, 0});
    const glm::dvec3 tw = glm::normalize(glm::cross(axis, tu));
    unsigned int prev = 0;
    bool first = true;
    const auto ring = [&](double r_here, double depth) {
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int s = 0; s < seg; ++s) {
            const double a = 2.0 * pi * s / seg;
            const glm::dvec3 radial = std::cos(a) * tu + std::sin(a) * tw;
            const glm::dvec3 v =
                axis * (bw.surface_r - depth) + radial * r_here;
            // T24 (fly round-16, Chad's right-flank pointer: "it looks cool
            // but... almost overhang there, hard to tell"): the sleeve sat at
            // the SAME value as the trench drapes / collar band / portal wing
            // stacked behind the arch — depth-ambiguous. One notch DARKER
            // (cue x0.8) so the layer parallax reads; the mouth itself is NOT
            // re-darkened (the T20 lesson — contrast IS the Errington read).
            push_v(p, v, 0.8f * depth_cue(v, g, bare_R));
        }
        if (!first) stitch_rings(p, seg, prev, base);
        prev = base;
        first = false;
    };
    ring(bw.bowl_r, kTuck);                // wall top (tucked)
    ring(bw.bowl_r, 0.5 * bw.bowl_depth);  // wall mid (trim granularity)
    ring(bw.bowl_r, bw.bowl_depth);        // wall foot
    ring(0.5 * (bw.bowl_r + bw.floor_r), bw.bowl_depth);  // floor mid
    ring(bw.floor_r, bw.bowl_depth);                      // floor inner edge
}

// T18 — THE ARCH-BAND TRIM (fly-14, Chad: "there are stretches of mesh across
// the murray entrance and the errington entrance... doesn't look natural").
// The adapter band's above-floor facets — the back drape over the Murray slot
// and the Errington brow wrap — read as sails strung across the openings.
// Drop every mouth-piece facet touching an ABOVE-FLOOR tube-end-ring vertex
// (exact-float match; the seam discipline makes ring verts bit-identifiable):
// the arch stands free, the below-floor apron (rock-backed) survives. SAFE AT
// BOTH MOUTHS NOW: behind the Murray drape is the open slot bounded by tube
// wall; behind the Errington brow the T17 pit cut SLEEVE renders the wall (in
// T17 this exact trim was rejected because the sleeve did not exist yet and
// the drop exposed the un-rendered pit cylinder).
// `hot_shell_r` — ring verts ABOVE this shell radius (along the mouth axis)
// are "hot": the Murray call passes the pit-floor shell (== the ring centre
// there); Errington passes its ring-CENTRE shell so only the upper-half brow
// drops and the crater's cone-to-floor wrap survives (audit lesson).
void drop_arch_band_tris(const std::vector<glm::dvec3>& ring,
                         const glm::dvec3& axis, double hot_shell_r,
                         TunnelPiece& p) {
    std::vector<glm::vec3> hot;
    for (const glm::dvec3& v : ring)
        if (glm::dot(v, axis) - hot_shell_r > 2.0) hot.push_back(glm::vec3(v));
    if (hot.empty()) return;  // a flat-lying mouth ring: nothing tents
    const std::size_t nv = p.cue.size();
    std::vector<char> is_hot(nv, 0);
    for (std::size_t i = 0; i < nv; ++i) {
        const glm::vec3 q{p.positions[3 * i + 0], p.positions[3 * i + 1],
                          p.positions[3 * i + 2]};
        for (const glm::vec3& h : hot)
            if (q.x == h.x && q.y == h.y && q.z == h.z) {
                is_hot[i] = 1;
                break;
            }
    }
    std::vector<unsigned short> kept;
    kept.reserve(p.indices.size());
    for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
        if (is_hot[p.indices[t]] || is_hot[p.indices[t + 1]] ||
            is_hot[p.indices[t + 2]])
            continue;  // a band facet strung on the above-floor arch
        kept.push_back(p.indices[t]);
        kept.push_back(p.indices[t + 1]);
        kept.push_back(p.indices[t + 2]);
    }
    p.indices = std::move(kept);
}

// T22 — THE THROAT RAMP FACE (fly-16, Chad: "I died seemingly unfairly in the
// murray bowl trying to fly back in... coming down right down the pipe hit
// something invisible"). The dark opening in the Murray pit floor spans BOTH
// sides of the mouth ring — the slot behind it AND the throat's footprint in
// front (the floor disk is trimmed over both) — but the front half's lower
// boundary is the throat's collision RAMP, which had NO drawn face: a steep
// descent into that half stopped 40-60 m above the visible tunnel floor, on
// nothing. This draws the ramp: a strip of rows exactly on (0.4 m proud of)
// the throat's interpolated floor shells, as wide as the throat's floor chord
// at each station — the pilot now sees the rising rock apron he can hit.
// Appended to the Murray bowl piece BEFORE the darkening pass (it inherits
// the mouth blend) and before the trims (it sits ON the throat-floor
// boundary, sd ~ +0.4, so no trim eats it).
void build_throat_ramp(const world::TunnelNet& net, const world::HeightField* g,
                       double bare_R, TunnelPiece& p) {
    if (!net.throat_on) return;
    constexpr int kSteps = kThroatRampRows - 1;  // rows = header constant
    constexpr int kAcross = kThroatRampAcross;   // verts per row (header)
    constexpr double kProud_m = 0.4;  // drawn face proud of the collision floor
    unsigned int prev = 0;
    bool first = true;
    for (int i = 0; i <= kSteps; ++i) {
        const double tc = static_cast<double>(i) / kSteps;
        const glm::dvec3 c = net.throat_a + tc * (net.throat_b - net.throat_a);
        const double shell = net.throat_floor_a +
                             tc * (net.throat_floor_b - net.throat_floor_a) +
                             kProud_m;
        const glm::dvec3 up = glm::normalize(c);
        glm::dvec3 h = glm::cross(net.throat_tan, up);
        const double hl = glm::length(h);
        if (hl < 1e-9) continue;
        h /= hl;
        // The throat ellipse's half-width where the floor shell cuts it (the
        // T5a floor-chord law), floored so the strip never pinches to a line.
        const double d = glm::length(c) - shell;  // centre above the floor
        const double frac = std::clamp(d / net.tube_height, 0.0, 1.0);
        const double half = std::max(
            10.0, net.tube_width * std::sqrt(std::max(0.0, 1.0 - frac * frac)));
        const unsigned int base = static_cast<unsigned int>(p.cue.size());
        for (int j = 0; j < kAcross; ++j) {
            const double w =
                half * (2.0 * static_cast<double>(j) / (kAcross - 1) - 1.0);
            const glm::dvec3 v = glm::normalize(c + h * w) * shell;
            push_v(p, v, depth_cue(v, g, bare_R));
        }
        if (!first) {
            for (int j = 0; j + 1 < kAcross; ++j) {
                const unsigned short a0 = static_cast<unsigned short>(prev + j);
                const unsigned short a1 =
                    static_cast<unsigned short>(prev + j + 1);
                const unsigned short b0 = static_cast<unsigned short>(base + j);
                const unsigned short b1 =
                    static_cast<unsigned short>(base + j + 1);
                p.indices.push_back(a0);
                p.indices.push_back(a1);
                p.indices.push_back(b1);
                p.indices.push_back(a0);
                p.indices.push_back(b1);
                p.indices.push_back(b0);
            }
        }
        prev = base;
        first = false;
    }
}

// A CLOSED manifold box (a concrete portal member). Center `c`, half-extents
// (hx,hy,hz) along the orthonormal frame (ex,ey,ez). 8 verts, 12 triangles (6
// quads). Every edge is shared by exactly 2 triangles => no boundary edges (the
// T12 manifold verifier passes trivially). cue baked at 1.0 (a surface
// structure — the exterior daylight ambient + kPortalVal own its shading).
void push_box(TunnelPiece& p, const glm::dvec3& c, const glm::dvec3& ex,
              const glm::dvec3& ey, const glm::dvec3& ez, double hx, double hy,
              double hz, float cue = 1.0f) {
    const unsigned int base = static_cast<unsigned int>(p.cue.size());
    // 8 corners in Gray-friendly order: sign of (x,y,z) half-extents. `cue` is
    // the per-vertex brightness (1 = the pale portal frame; T19 ruin members
    // bake kRuinVal/kPortalVal so the FS draws them darker — silvered timber).
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2) {
                const glm::dvec3 v =
                    c + ex * (sx * hx) + ey * (sy * hy) + ez * (sz * hz);
                push_v(p, v, cue);
            }
    // Corner index: bit2 = (sx>0), bit1 = (sy>0), bit0 = (sz>0).
    auto ci = [&](int sx, int sy, int sz) -> unsigned short {
        const int b = (sx > 0 ? 4 : 0) | (sy > 0 ? 2 : 0) | (sz > 0 ? 1 : 0);
        return static_cast<unsigned short>(base + b);
    };
    auto quad = [&](unsigned short a, unsigned short b, unsigned short d,
                    unsigned short e) {
        p.indices.push_back(a);
        p.indices.push_back(b);
        p.indices.push_back(d);
        p.indices.push_back(a);
        p.indices.push_back(d);
        p.indices.push_back(e);
    };
    // 6 faces (winding is not load-bearing — the tunnel FS is two-sided). Each
    // face's 4 corners in ring order so the two tris share the face diagonal.
    quad(ci(-1, -1, -1), ci(-1, -1, 1), ci(-1, 1, 1), ci(-1, 1, -1));  // -X
    quad(ci(1, -1, -1), ci(1, -1, 1), ci(1, 1, 1), ci(1, 1, -1));      // +X
    quad(ci(-1, -1, -1), ci(-1, -1, 1), ci(1, -1, 1), ci(1, -1, -1));  // -Y
    quad(ci(-1, 1, -1), ci(-1, 1, 1), ci(1, 1, 1), ci(1, 1, -1));      // +Y
    quad(ci(-1, -1, -1), ci(-1, 1, -1), ci(1, 1, -1), ci(1, -1, -1));  // -Z
    quad(ci(-1, -1, 1), ci(-1, 1, 1), ci(1, 1, 1), ci(1, -1, 1));      // +Z
}

// T13 (B1) — a strut box between two world endpoints `a`, `b`, of square
// section (half-thickness `t`). Built as a closed box aligned to the a->b axis
// (a stable perpendicular frame seeded from the pit up). Reuses push_box so the
// member is a closed manifold.
void push_strut(TunnelPiece& p, const glm::dvec3& a, const glm::dvec3& b,
                double t, const glm::dvec3& up_hint, float cue = 1.0f) {
    glm::dvec3 axis = b - a;
    const double len = glm::length(axis);
    if (len < 1e-6) return;
    axis /= len;
    glm::dvec3 ey = up_hint - glm::dot(up_hint, axis) * axis;
    double l = glm::length(ey);
    if (l < 1e-9) {
        const glm::dvec3 alt =
            std::fabs(axis.x) < 0.9 ? glm::dvec3{1, 0, 0} : glm::dvec3{0, 1, 0};
        ey = alt - glm::dot(alt, axis) * axis;
        l = glm::length(ey);
    }
    ey /= l;
    const glm::dvec3 ez = glm::normalize(glm::cross(axis, ey));
    const glm::dvec3 c = 0.5 * (a + b);
    push_box(p, c, axis, ey, ez, 0.5 * len, t, t, cue);
}

// T16 ENTRANCE-ART — THE ERRINGTON MOUTH FRAME. The bore's own orthonormal
// frame at the sunk mouth (spine.front, the bore opening on the pit floor):
// `c` the mouth centre, `up` local radial, `tan` the bore tangent INTO the
// tunnel, `horiz` the wide (lateral) bore axis, `vert` the tall bore axis,
// `out` the approach direction (out of the tunnel). rw/rh the bore semi-axes.
// SINGLE SOURCE for both build_portal AND its lintel beacons.
struct MouthFrame {
    glm::dvec3 c{0.0}, up{0.0}, tan{0.0}, horiz{0.0}, vert{0.0}, out{0.0};
    double rw = 0.0, rh = 0.0;
    bool ok = false;
};

MouthFrame errington_mouth_frame(const world::TunnelNet& net) {
    MouthFrame f;
    if (net.spine.size() < 2) return f;
    f.c = net.spine.front().pos;
    f.up = glm::normalize(f.c);
    f.tan = spine_tangent(net, 0);  // into the tunnel (down-tangent)
    glm::dvec3 horiz = glm::cross(f.tan, f.up);
    double hl = glm::length(horiz);
    if (hl <= 1e-9) {
        const glm::dvec3 alt =
            std::fabs(f.up.x) < 0.9 ? glm::dvec3{1, 0, 0} : glm::dvec3{0, 1, 0};
        horiz = alt - glm::dot(alt, f.up) * f.up;
        hl = glm::length(horiz);
    }
    f.horiz = horiz / hl;
    f.vert = glm::normalize(glm::cross(f.horiz, f.tan));  // ~local up
    f.out = -f.tan;                                       // toward the approach
    f.rw = net.tube_width;
    f.rh = net.tube_height;
    f.ok = true;
    return f;
}

// T19 ENTRANCE-ART — THE ERRINGTON 1931 GHOST-RUIN ENSEMBLE (see the header).
// A leaning timber gallows headframe (one leg snapped short + fallen members
// half-sunk at its base), a roofless rockhouse/mill shell (staggered irregular
// wall tops), and an ore-car trestle running to a waste-rock dump fan —
// clustered on ONE Errington pit FLANK, never over the bore. Folded into the
// kPortal piece (same net.headframe_on gate). Every member is a closed manifold
// box baked at the ruin cue (kRuinVal/kPortalVal => the FS reads it darker than
// the pale frame). Placement is the pit tangent frame (net.pit +
// net.pit_up_tangent), scaled by the pit rim radius — no magic world coords.
void build_errington_ruins(const world::TunnelNet& net,
                           const world::HeightField* g, double bare_R,
                           TunnelPiece& p) {
    const world::TunnelNet::Bowl& pit = net.pit;
    if (pit.bowl_depth <= 0.0) return;  // legacy flat mouth => no pit, no ruins
    const double rim = pit.bowl_r > 0.0
                           ? pit.bowl_r
                           : world::errington_pit_rim(net.tube_width);
    const double surfR = pit.surface_r > 0.0 ? pit.surface_r : bare_R;
    const glm::dvec3 pu = pit.axis;  // local up at the pit
    // pt = the approach-side horizontal tangent (away from Murray); ps = the
    // lateral flank tangent. Fall back to a stable tangent if unset.
    glm::dvec3 pt = net.pit_up_tangent - glm::dot(net.pit_up_tangent, pu) * pu;
    if (glm::length(pt) < 1e-6) pt = perp_seed(pu, glm::dvec3{1, 0, 0});
    pt = glm::normalize(pt);
    const glm::dvec3 ps = glm::normalize(glm::cross(pu, pt));
    const float rc = static_cast<float>(kRuinVal / kPortalVal);  // ruin cue

    // A ground point at tangent-plane offset (a along pt, b along ps), sunk
    // `sink` metres; the direction curves onto the sphere so footings hug the
    // shell. `sky_pt` is `height` metres ABOVE the local surface (beam tops).
    auto ground_pt = [&](double a, double b, double sink) {
        const glm::dvec3 dir = glm::normalize(pu * surfR + pt * a + ps * b);
        return dir * (surface_r(dir, g, bare_R) - sink);
    };
    auto up_at = [&](double a, double b) {
        return glm::normalize(pu * surfR + pt * a + ps * b);
    };
    auto sky_pt = [&](double a, double b, double height) {
        const glm::dvec3 dir = glm::normalize(pu * surfR + pt * a + ps * b);
        return dir * (surface_r(dir, g, bare_R) + height);
    };
    // A vertical (optionally leaning) post rising `h` from its footing, square
    // section (hx along pt x hz along ps). lean_a/lean_b tilt the up-axis by
    // kRuinLeanDeg toward (lean_a*pt + lean_b*ps).
    auto post = [&](double a, double b, double hx, double hz, double h,
                    double lean_a, double lean_b) {
        const glm::dvec3 foot = ground_pt(a, b, kRuinFootSink_m);
        glm::dvec3 axis = up_at(a, b);
        glm::dvec3 tilt = pt * lean_a + ps * lean_b;
        const double tl = glm::length(tilt);
        if (tl > 1e-9) {
            tilt /= tl;
            const double ang = kRuinLeanDeg * 3.14159265358979323846 / 180.0;
            axis = glm::normalize(std::cos(ang) * axis + std::sin(ang) * tilt);
        }
        glm::dvec3 ex = pt - glm::dot(pt, axis) * axis;
        if (glm::length(ex) < 1e-9) ex = perp_seed(axis, ps);
        ex = glm::normalize(ex);
        const glm::dvec3 ez = glm::normalize(glm::cross(axis, ex));
        // Box spans foot .. foot + axis*(h + sink) so the buried base has no
        // floating edge (foot is already sunk kRuinFootSink below grade).
        const double hh = 0.5 * (h + kRuinFootSink_m);
        const glm::dvec3 c = foot + axis * hh;
        push_box(p, c, ex, axis, ez, hx, hh, hz, rc);
    };

    // (A) THE RAKING GALLOWS HEADFRAME (~net.headframe_h tall, one leg snapped
    //     short) — the Errington landmark, on the approach-side flank so it
    //     rises above the treeline against the sky and frames (never blocks)
    //     the lit adit. Four legs CONVERGE toward the head (a real gallows) and
    //     RAKE back toward the approach (+pt, an inclined-shaft silhouette);
    //     a solid sheave head-block caps the intact tops; two raking back-
    //     braces (the skipway) drop to the ground; fallen members lie at the
    //     base. Struts (foot -> top) so the convergence/rake is exact.
    const double H = net.headframe_h > 0.0 ? net.headframe_h : 80.0;
    const double hf_a = 0.40 * rim, hf_b = kRuinHeadframeFlankFrac * rim;
    const double fb = 12.0;        // foot half-footprint [m]
    const double ft = 4.5;         // top half-footprint (legs converge) [m]
    const double rake = 0.18 * H;  // tops raked back toward the approach (+pt)
    struct Leg {
        double fa, fbb, ta, tb, h;
    };
    // Three intact legs + the +pt/+ps corner SNAPPED to 55% (upper section
    // gone).
    const Leg legs[4] = {
        {hf_a - fb, hf_b - fb, hf_a + rake - ft, hf_b - ft, H},
        {hf_a - fb, hf_b + fb, hf_a + rake - ft, hf_b + ft, H},
        {hf_a + fb, hf_b - fb, hf_a + rake + ft, hf_b - ft, H},
        {hf_a + fb, hf_b + fb, hf_a + rake + ft, hf_b + ft, 0.55 * H}};
    glm::dvec3 tops[4];
    for (int i = 0; i < 4; ++i) {
        const glm::dvec3 foot =
            ground_pt(legs[i].fa, legs[i].fbb, kRuinFootSink_m);
        tops[i] = sky_pt(legs[i].ta, legs[i].tb, legs[i].h);
        push_strut(p, foot, tops[i], 2.4, pu, rc);
    }
    // The surviving sheave head-block at the apex of the three intact tops.
    {
        const glm::dvec3 apex =
            (tops[0] + tops[1] + tops[2]) / 3.0 + glm::normalize(tops[0]) * 3.0;
        const glm::dvec3 axis = glm::normalize(apex);
        glm::dvec3 ex = glm::normalize(ps - glm::dot(ps, axis) * axis);
        const glm::dvec3 ez = glm::normalize(glm::cross(axis, ex));
        push_box(p, apex, ex, axis, ez, ft + 3.0, 5.0, ft + 3.0, rc);
    }
    push_strut(p, tops[0], tops[1], 1.6, pu, rc);  // head cross-beam
    push_strut(p, tops[0], tops[2], 1.6, pu, rc);  // head side-beam
    // Two raking BACK-BRACES (the inclined skipway) down to the ground on +pt.
    push_strut(p, tops[0], ground_pt(hf_a + 2.6 * fb, hf_b - fb, 0.4), 2.0, pu,
               rc);
    push_strut(p, tops[1], ground_pt(hf_a + 2.6 * fb, hf_b + fb, 0.4), 2.0, pu,
               rc);
    // Fallen members half-sunk at the base (near-horizontal boxes).
    push_strut(p, ground_pt(hf_a + 1.4 * fb, hf_b - 1.3 * fb, -0.5),
               ground_pt(hf_a + 3.2 * fb, hf_b - 0.3 * fb, 0.6), 1.6, pu, rc);
    push_strut(p, ground_pt(hf_a - fb, hf_b + 1.6 * fb, 0.4),
               ground_pt(hf_a + 0.9 * fb, hf_b + 3.0 * fb, -0.4), 1.4, pu, rc);

    // (B) THE ROOFLESS ROCKHOUSE / MILL SHELL — three wall runs, each a row of
    //     boxes at STAGGERED heights so the top line is irregular (gable ends
    //     broken). Long axis along pt (into the slope), on the rockhouse flank.
    {
        const double rk_a = -0.25 * rim, rk_b = kRuinRockhouseFlankFrac * rim;
        const double th = 1.5;  // wall half-thickness [m]
        // A wall run of `n` boxes from (a0,b0) stepping (da,db) each, heights
        // cycling through an irregular (broken-top) set.
        const double hset[4] = {22.0, 14.0, 27.0, 10.0};
        auto wall = [&](double a0, double b0, double da, double db, int n,
                        double seg, double along_pt, double along_ps) {
            for (int i = 0; i < n; ++i) {
                const double a = a0 + da * i, b = b0 + db * i;
                const double h = hset[i % 4];
                const glm::dvec3 foot = ground_pt(a, b, kRuinFootSink_m);
                const glm::dvec3 axis = up_at(a, b);
                glm::dvec3 ex = pt * along_pt + ps * along_ps;
                ex = glm::normalize(ex - glm::dot(ex, axis) * axis);
                const glm::dvec3 ez = glm::normalize(glm::cross(axis, ex));
                const double hh = 0.5 * (h + kRuinFootSink_m);
                push_box(p, foot + axis * hh, ex, axis, ez, seg, hh, th, rc);
            }
        };
        // Two long side walls (along pt) + one broken gable end (along ps).
        wall(rk_a, rk_b - 9.0, 13.0, 0.0, 3, 6.5, 1.0, 0.0);
        wall(rk_a, rk_b + 9.0, 13.0, 0.0, 3, 6.5, 1.0, 0.0);
        wall(rk_a + 26.0, rk_b - 9.0, 0.0, 9.0, 2, 4.5, 0.0, 1.0);
    }

    // (C) THE ORE-CAR TRESTLE + WASTE-ROCK DUMP FAN. A run of paired posts
    //     marching from the rockhouse out toward the dump, a beam riding the
    //     tops (a couple of bents collapsed => short/absent), ending in a fan
    //     of low wide flattened mounds.
    {
        const double b0 = kRuinRockhouseFlankFrac * rim + 14.0;
        const double b1 = kRuinDumpFlankFrac * rim;
        const int npost = 6;
        glm::dvec3 prev_top;
        bool have_prev = false;
        for (int i = 0; i < npost; ++i) {
            const double f = static_cast<double>(i) / (npost - 1);
            const double b = b0 + (b1 - b0) * f;
            const double a = 0.20 * rim;
            const bool collapsed = (i == 2 || i == 4);          // two down
            const double ph = collapsed ? 3.0 : 9.0 + 4.0 * f;  // rise outward
            // A pair of posts straddling the track (±3.5 m along pt).
            post(a - 3.5, b, 1.1, 1.1, ph, 0.0, 0.3 * (collapsed ? 1.0 : 0.0));
            post(a + 3.5, b, 1.1, 1.1, ph, 0.0, 0.3 * (collapsed ? 1.0 : 0.0));
            const glm::dvec3 top = sky_pt(a, b, ph);
            if (have_prev && !collapsed)  // the beam skips a collapsed bent
                push_strut(p, prev_top, top, 1.1, pu, rc);
            prev_top = top;
            have_prev = !collapsed;
        }
        // Waste-rock dump fan: 4 low wide flattened mounds spilling off the
        // end.
        const double db = kRuinDumpFlankFrac * rim;
        const double mound[4][3] = {// {a, b, half-width}
                                    {0.20 * rim, db + 14.0, 30.0},
                                    {0.20 * rim + 38.0, db + 10.0, 24.0},
                                    {0.20 * rim - 34.0, db + 20.0, 22.0},
                                    {0.20 * rim + 8.0, db + 46.0, 26.0}};
        for (auto& m : mound) {
            const glm::dvec3 foot = ground_pt(m[0], m[1], 2.0);
            const glm::dvec3 axis = up_at(m[0], m[1]);
            glm::dvec3 ex = glm::normalize(pt - glm::dot(pt, axis) * axis);
            const glm::dvec3 ez = glm::normalize(glm::cross(axis, ex));
            const double mh = 8.0;  // low mound
            push_box(p, foot + axis * mh, ex, axis, ez, m[2], mh, 0.8 * m[2],
                     rc);
        }
    }
}

// T16 ENTRANCE-ART — THE ERRINGTON PORTAL FRAME (framed adit; see the header).
// Replaces the deleted T13 sinking headframe per Chad's 2026-07-21 ruling. Two
// thick POSTS + a LINTEL beam framing the elliptical bore, angled WING WALLS
// flaring outward, and a protruding CANOPY hood over the approach — all built
// as closed manifold boxes in the mouth frame, entirely OUTSIDE the bore
// ellipse (semi rw x rh) so the flyable opening is never narrowed. VISUAL-ONLY
// (no SDF). Gated on net.headframe_on (the repurposed surface-structure flag).
// T19: the 1931 ghost-ruin ensemble is folded in on the same gate (same piece).
void build_portal(const world::TunnelNet& net, const world::HeightField* g,
                  double bare_R, TunnelPiece& p) {
    if (!net.headframe_on || net.headframe_h <= 0.0) return;
    const MouthFrame f = errington_mouth_frame(net);
    if (!f.ok) return;

    const double rw = f.rw, rh = f.rh;
    const double clr = kPortalClearance;
    const double post_t = std::max(1.0, kPortalPostThick * rw);  // post half-th
    const double lin_t = std::max(1.0, kPortalLintelThick * rh);  // lintel half
    // Post inner face stands clr*rw beyond the horizontal semi; the post CENTRE
    // is a further half-thickness out, so the whole post clears the bore.
    const double post_lat = rw * (1.0 + clr) + post_t;
    // Lintel CENTRE clr*rh above the vertical semi + its own half-height.
    const double lin_y = rh * (1.0 + clr) + lin_t;
    // Posts span from below the bore bottom (planted on the pit floor) up to
    // the lintel.
    const double post_bot = -rh * (1.0 + kPortalRiseFrac);
    const double post_top = lin_y;
    const double post_cy = 0.5 * (post_top + post_bot);
    const double post_hh = 0.5 * (post_top - post_bot);

    // (1) TWO POSTS (left/right of the bore) — vertical boxes, clear laterally.
    for (int s = -1; s <= 1; s += 2) {
        const glm::dvec3 pc = f.c +
                              f.horiz * (static_cast<double>(s) * post_lat) +
                              f.vert * post_cy;
        push_box(p, pc, f.horiz, f.vert, f.tan, post_t, post_hh, post_t);
    }

    // (2) THE LINTEL beam — REMOVED (round-17 census/fly finding, Chad: "a flat
    //     gray rectangular slab hovers over the excavation" on the Errington
    //     trench side). The lintel spans post_lat+post_t (~155 m half-width)
    //     at a FIXED height/lateral offset from the bore-mouth frame (f.c/
    //     f.vert/f.horiz), with no reference to the actual excavation surface
    //     — unlike the T19 ghost-ruin ensemble below, which conforms every
    //     footing to the real terrain via ground_pt/surface_r. On the
    //     trench-side approach the ground drops away under that rigid frame
    //     assumption, so the far half of the beam hung in open air with a
    //     visible cast shadow. Isolated by a differential render probe: with
    //     BOTH this box and the canopy (below) suppressed the slab vanished
    //     completely at the repro eye; either alone still left a floating
    //     remnant of the other, so both are gated off together. VISUAL-ONLY,
    //     NO SDF — removing it cannot open a leak or move a collision
    //     surface. The posts + wing walls (still built below) keep the
    //     "framed adit" read; a terrain-following re-grounding (mirroring
    //     ground_pt) is the correct long-term fix if the beam is wanted back.

    // (3) WING WALLS: angled slabs flaring outward + back from each post,
    //     widening the entrance mouth (a real adit's splayed retaining walls).
    for (int s = -1; s <= 1; s += 2) {
        const double sd = static_cast<double>(s);
        const glm::dvec3 inner =
            f.c + f.horiz * (sd * post_lat) + f.vert * post_cy;
        const glm::dvec3 outer =
            f.c + f.horiz * (sd * (post_lat + kPortalWingSpread * rw)) +
            f.vert * post_cy + f.out * (0.6 * rw);
        push_strut(p, inner, outer, post_t, f.vert);
    }

    // (4) THE CANOPY HOOD — REMOVED (same round-17 finding as the lintel
    //     above). It protrudes OUT along f.out — precisely toward the
    //     approach/trench — from the same fixed, non-terrain-following mouth
    //     frame, so it is the piece most directly aimed at the excavated
    //     void; see the lintel comment above for the full attribution and the
    //     differential-probe evidence (both boxes gated off together).

    // T19 — fold the 1931 ghost-ruin ensemble into the same kPortal piece.
    build_errington_ruins(net, g, bare_R, p);
}

// T16 ENTRANCE-ART — the aviation-beacon lamp positions across the portal
// lintel (reads at dusk). kPortalLintelBeacons points spread along the lintel
// top, in the mouth frame. Empty when the portal is OFF. The caller draws them
// via the bright-tier lamp path (the T12 mouth-beacon idiom).
std::vector<glm::dvec3> portal_beacon_points(const world::TunnelNet& net) {
    std::vector<glm::dvec3> out;
    // Round-17: the LINTEL (and canopy) box was REMOVED from build_portal (it
    // hung over the trench-side excavation — see the attribution comment
    // there), so the beacons that sat on its top edge would now float in open
    // air with nothing beneath them (the red-team's P2-4). They ride with the
    // lintel: none until a terrain-grounded lintel returns, at which point
    // this function re-derives the spread from that lintel's real top.
    (void)net;
    return out;
}

// Every piece must fit 16-bit indices (they all will — the greybox is small).
void assert_u16(const TunnelPiece& p) {
    assert(p.cue.size() < 65535u &&
           "tunnel greybox piece exceeds 16-bit index cap");
    (void)p;
}

}  // namespace

// T14 — the true breach locator (single source; see the header). Split the
// spine at its deepest node, scan each leg from its mouth for the first node
// INSIDE the arena ellipsoid, then bisect world::ellipsoid_sd (the SDF's own
// wall — never a re-derived copy) along the crossing segment for the exact
// centreline<->wall intersection. The oblique-crossing stretch comes from the
// ellipsoid's numeric surface normal at the crossing vs the bore tangent.
std::vector<Breach> breach_points(const world::TunnelNet& net) {
    std::vector<Breach> out;
    if (!net.arena_on || net.spine.size() < 2 || net.arena.b <= 0.0) return out;
    const auto inside = [&](const glm::dvec3& p) {
        return world::ellipsoid_sd(p, net.arena) < 0.0;
    };
    const std::size_t n = net.spine.size();
    std::size_t deep_i = 0;
    for (std::size_t i = 1; i < n; ++i)
        if (glm::length(net.spine[i].pos) < glm::length(net.spine[deep_i].pos))
            deep_i = i;

    // First inside node on each leg (E scans mouth->deep, M scans back->deep).
    std::size_t in_E = n, in_M = n;
    for (std::size_t i = 0; i <= deep_i; ++i)
        if (inside(net.spine[i].pos)) {
            in_E = i;
            break;
        }
    for (std::size_t i = n; i-- > deep_i;)
        if (inside(net.spine[i].pos)) {
            in_M = i;
            break;
        }
    if (in_E >= n || in_M >= n) return out;  // bore never enters the arena

    const auto make_breach = [&](std::size_t node_in,
                                 std::size_t node_out) -> Breach {
        Breach b;
        b.node_in = node_in;
        const glm::dvec3 pin = net.spine[node_in].pos;
        const glm::dvec3 pout = net.spine[node_out].pos;
        if (node_out == node_in || inside(pout)) {
            // Degenerate (mouth already inside): the node itself is the best
            // available crossing; tangent from the local spine frame.
            b.pos = pin;
            b.into = spine_tangent(net, node_in);
        } else {
            // Bisect sd == 0 on [pout (outside) .. pin (inside)]: 60 halvings
            // land the crossing to ~1e-16 of the segment (deterministic).
            double lo = 0.0, hi = 1.0;
            for (int it = 0; it < 60; ++it) {
                const double mid = 0.5 * (lo + hi);
                if (inside(pout + mid * (pin - pout)))
                    hi = mid;
                else
                    lo = mid;
            }
            b.pos = pout + (0.5 * (lo + hi)) * (pin - pout);
            b.into = glm::normalize(pin - pout);
        }
        // Oblique elongation: 1/cos(theta) between the bore tangent and the
        // ellipsoid's outward surface normal at the crossing (numeric central-
        // difference gradient of the SAME sd the crossing was found on), capped
        // so a grazing crossing cannot cut the whole dome.
        const double h = 0.5;
        glm::dvec3 grad{0.0};
        for (int k = 0; k < 3; ++k) {
            glm::dvec3 e{0.0};
            e[k] = h;
            grad[k] = (world::ellipsoid_sd(b.pos + e, net.arena) -
                       world::ellipsoid_sd(b.pos - e, net.arena)) /
                      (2.0 * h);
        }
        const double gl = glm::length(grad);
        if (gl > 1e-12) {
            const double cos_t =
                std::abs(glm::dot(b.into, grad / gl));  // 1 = head-on
            b.stretch =
                std::min(kBreachMaxStretch, 1.0 / std::max(cos_t, 1e-9));
        }
        // T14b — bake the cut semi-axes (see the header): minor = the canon
        // round hole; major = the PHYSICAL oblique extent (the bore's vertical
        // semi stretched by the slant) + the same absolute margin the round
        // hole carries (hole_arc - tube_width). max() degrades to the round
        // hole for a near-head-on crossing.
        const double hole_arc = breach_hole_arc(net.tube_width);
        b.hole_minor_m = hole_arc;
        b.hole_major_m = std::max(hole_arc, b.stretch * net.tube_height +
                                                (hole_arc - net.tube_width));
        return b;
    };

    out.push_back(make_breach(in_E, in_E > 0 ? in_E - 1 : in_E));
    out.push_back(make_breach(in_M, in_M + 1 < n ? in_M + 1 : in_M));
    return out;
}

// T14 — the shared hole predicate (see the header). An elongated ellipse in
// angle space about each breach axis: semi-axes = the breach's OWN baked
// hole_minor_m/hole_major_m (T14b: major hugs the physical oblique opening +
// the canon margin) at the breach's own radius. pad_m widens both (the
// verifier rim allowance).
bool breach_hole_hit(const std::vector<Breach>& breaches,
                     const glm::dvec3& wdir, double pad_m) {
    for (const Breach& b : breaches) {
        const double R_local = glm::length(b.pos);
        if (R_local <= 0.0) continue;
        const glm::dvec3 ax = b.pos / R_local;
        const double minor = (b.hole_minor_m + pad_m) / R_local;
        const double major = (b.hole_major_m + pad_m) / R_local;
        const double cd = glm::clamp(glm::dot(wdir, ax), -1.0, 1.0);
        const double ang = std::acos(cd);
        if (ang >= major) continue;  // outside the bounding circle
        glm::dvec3 perp = wdir - cd * ax;
        const double pl = glm::length(perp);
        if (pl < 1e-12) return true;  // dead centre of the hole
        perp /= pl;
        // Elongation direction: the bore tangent's component tangential to the
        // sphere at the breach (the direction the oblique opening stretches).
        glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
        const double el = glm::length(e1);
        if (el < 1e-9) {
            // Radial crossing (no tangential component): a round hole.
            if (ang < minor) return true;
            continue;
        }
        e1 /= el;
        const glm::dvec3 e2 = glm::cross(ax, e1);
        const double u = ang * glm::dot(perp, e1);
        const double v = ang * glm::dot(perp, e2);
        if ((u / major) * (u / major) + (v / minor) * (v / minor) < 1.0)
            return true;
    }
    return false;
}

TunnelMeshData build_tunnel_mesh(const world::TunnelNet& net,
                                 const world::HeightField* ground,
                                 double errington_collar_outer_m,
                                 double murray_collar_outer_m,
                                 double terrain_cell_arc_m,
                                 bool cut_swallow_trim,
                                 int tube_trim_split_depth) {
    TunnelMeshData data;
    if (net.spine.size() < 2) return data;

    // Recover the bare-sphere radius for the null-ground collar/cue: the mouth
    // sits AT the local surface, so |spine.front()| is the surface radius at
    // the home mouth. With a live ground this is only the null fallback.
    const double bare_R = glm::length(net.spine.front().pos);

    // Resolve the collar outer radii: the caller passes terrain-grid-derived
    // values (via collar_reach()); a <= 0 sentinel falls back to the legacy
    // kMouthCutFactor * tube_radius (Errington) / the net's bowl radius
    // (Murray) for callers without grid info (tests using the null-ground
    // path).
    const double resolved_err_collar =
        errington_collar_outer_m > 0.0 ? errington_collar_outer_m
        : net.pit.bowl_depth > 0.0     ? net.pit.bowl_r  // T6c: the pit rim
                                       : kMouthCutFactor * net.tube_width;
    const double resolved_mur_collar =
        murray_collar_outer_m > 0.0 ? murray_collar_outer_m : net.bowl.bowl_r;

    // T14 — the true breaches, computed ONCE and threaded to every consumer
    // (tube suppression + spill, the arena hole cut). Empty when the arena is
    // off or the bore never enters it. The suppression range must never eat a
    // MOUTH node (red-team P2): a suppressed node 0 / n-1 would leave the
    // collar builders an EMPTY end ring and they would stitch garbage indices.
    // Unreachable at canon (mouth r ~15170 vs arena max 13800) — assert, don't
    // silently ship it.
    const std::vector<Breach> breaches = breach_points(net);
    assert(breaches.empty() || (breaches[0].node_in > 0 &&
                                breaches[1].node_in + 1 < net.spine.size()));

    // 1) The swept tube (returns end rings for the collars). T5a: the ring
    //    verts are chord-truncated at the flat floor and the per-station floor
    //    chord edges are collected for the floor strip.
    TunnelPiece tube;
    std::vector<FloorRow> floor_rows;
    const TubeEnds ends =
        build_tube(net, breaches, ground, bare_R, tube, &floor_rows);
    // T17: drop tube facets swallowed by the MURRAY BOWL — the inclined
    // bore's crown above the pit floor was a black dome PLUGGING the mouth
    // (collision there is the open bowl cylinder). The bowl ONLY: the
    // Errington bore is a designed recess adit (see drop_cut_swallowed_tris).
    // Trimmed AFTER the end rings are captured; indices-only, so every
    // exact-seam consumer is untouched.
    tube.trim_base_verts = tube.cue.size();  // T23 count-pin mark
    if (cut_swallow_trim) drop_cut_swallowed_tris(net, {&net.bowl}, tube);
    assert_u16(tube);
    data.pieces.push_back(std::move(tube));

    // 1b) The T5a flat floor strip (only when the floor is live for some
    //     stations). Tagged kFloor; a no-floor net produces zero rows => this
    //     piece is skipped => bit-identical to no floor.
    {
        TunnelPiece floor;
        build_floor(floor_rows, ground, bare_R, floor);
        floor.trim_base_verts = floor.cue.size();  // T23 count-pin mark
        if (cut_swallow_trim)
            drop_cut_swallowed_tris(net, {&net.bowl}, floor);  // T17
        // T19 (B): fade the floor slot into the dark bore at the Murray mouth
        // (the "floor disk" surround) so it no longer reads glaring-pale.
        darken_cue_near(floor, net.spine.back().pos,
                        kMouthBlendMurrayFrac * net.tube_width);
        if (!floor.indices.empty()) {
            floor.kind = PieceKind::kFloor;
            assert_u16(floor);
            data.pieces.push_back(std::move(floor));
        }
    }

    // 2) The two side chambers (one piece each — keeps every piece tiny). T10:
    //    the egg is GONE; the chambers re-hang off the bores. LIT interiors:
    //    cue baked 1.0 so the depth floor never darkens them, tagged kChamber
    //    so the draw uses kChamberVal.
    //    T13: an OFF chamber (chambers_on == false) is built with b == 0 — skip
    //    it so no piece exists (the SDF skips it identically; the piece list
    //    must match the volume).
    for (int k = 0; k < 2; ++k) {
        if (net.chamber[k].b <= 0.0) continue;  // T13: chamber gated off
        TunnelPiece ch;
        build_pocket(net.chamber[k], ground, bare_R, /*lit=*/true, ch);
        ch.kind = PieceKind::kChamber;
        assert_u16(ch);
        data.pieces.push_back(std::move(ch));
    }

    // 3) The two connectors. T13: an OFF connector (r == 0) is skipped too.
    for (int k = 0; k < 2; ++k) {
        if (net.connector[k].r <= 0.0) continue;  // T13: connector gated off
        TunnelPiece conn;
        build_connector(net.connector[k].a, net.connector[k].b,
                        net.connector[k].r, ground, bare_R, conn);
        assert_u16(conn);
        data.pieces.push_back(std::move(conn));
    }

    // 4) The mouth pieces (inner ring == the tube end ring, exact floats).
    //    Tagged is_collar=true so the draw path uses kSkirtVal (solid earth /
    //    daylight, not the void-black kWallVal) — the skirt/pit is OUTSIDE the
    //    net (Fix 1).
    //    [6] Errington = the OPEN ENTRY PIT WALL (T6c): the home mouth is a
    //    RECESS, not a positive collar tenting over a poked-up tube. The pit is
    //    the SAME open-cone bowl-wall treatment as Murray, at pit scale — a
    //    crater from the surface rim down to the sunken bore opening. When the
    //    pit is OFF (net.pit.bowl_depth <= 0) it falls back to the legacy flat
    //    skirt.
    {
        TunnelPiece collar;
        if (net.pit.bowl_depth > 0.0) {
            // The Errington entry pit stays a SMOOTH funnel (the portal frames
            // the adit at its floor); the open-pit terraces are Murray's look.
            build_bowl_wall(net.pit, resolved_err_collar, ends.ring0, ground,
                            bare_R, terrain_cell_arc_m, /*benched=*/false,
                            collar);
        } else {
            build_collar(glm::normalize(net.spine.front().pos),
                         resolved_err_collar, ends.ring0, ends.u0, ends.w0,
                         ground, bare_R, terrain_cell_arc_m, collar);
        }
        // T17: render the pit cut's own boundary behind the crater cone (the
        // shell cover — see build_pit_sleeve). Before the trims, so the
        // bore-swallow cut opens the adit through it.
        if (net.pit.bowl_depth > 0.0) {
            const std::size_t sleeve_base = collar.cue.size();
            build_pit_sleeve(net.pit, ground, bare_R, collar);
            // T28 (fly round-18 wrap, Chad: at the Errington opening a residual
            // quarter of the pit-cut sleeve's UPPER wall ring still arcs across
            // the flat-top of the opening, with a top-of-DEM cover jutting off
            // it). The sleeve exists to occlude the LOW grazing see-through
            // rays that thread under the cone terminus (the T26 census sealer);
            // those are near the floor. Its shallow upper band seals nothing —
            // it only stands proud inside the crater on the flat quadrant the
            // cone does not reach, reading as "mesh strung across the opening"
            // (the T18/T26 lintel/canopy class). Drop the sleeve facets whose
            // shallowest vertex is within kSleeveBrowDepthFrac of the pit depth
            // of the surface; the floor + lower-wall seal (below that depth) is
            // untouched, so the census stays 0. Scoped to the sleeve verts
            // only — the bowl_wall's own liked side-cut-wall overhang is not
            // the sleeve and is never considered here.
            const double surf_r = net.pit.surface_r;
            const glm::dvec3 pax = net.pit.axis;
            const double brow_depth = kSleeveBrowDepthFrac * net.pit.bowl_depth;
            std::vector<unsigned short> kept;
            kept.reserve(collar.indices.size());
            for (std::size_t t = 0; t + 2 < collar.indices.size(); t += 3) {
                const unsigned short i0 = collar.indices[t];
                const unsigned short i1 = collar.indices[t + 1];
                const unsigned short i2 = collar.indices[t + 2];
                const bool sleeve_tri = i0 >= sleeve_base &&
                                        i1 >= sleeve_base && i2 >= sleeve_base;
                bool drop = false;
                if (sleeve_tri) {
                    // Shallowest (nearest-surface) vertex depth of the facet.
                    double min_depth = brow_depth + 1.0;
                    for (const unsigned short vi : {i0, i1, i2}) {
                        const glm::dvec3 v{collar.positions[3 * vi + 0],
                                           collar.positions[3 * vi + 1],
                                           collar.positions[3 * vi + 2]};
                        const double d = surf_r - glm::dot(v, pax);
                        if (d < min_depth) min_depth = d;
                    }
                    drop = min_depth < brow_depth;
                }
                if (!drop) {
                    kept.push_back(i0);
                    kept.push_back(i1);
                    kept.push_back(i2);
                }
            }
            collar.indices = std::move(kept);
        }
        collar.trim_base_verts = collar.cue.size();  // T23 count-pin mark
        if (cut_swallow_trim) {  // T17: yield to the bore + the OTHER cuts
            static_assert(world::TunnelNet::kTrenchSteps == 3,
                          "cut lists below enumerate the trench steps");
            // T18: free the Errington arch — the brow band read as "mesh
            // strung across the entrance"; the T17 sleeve renders the wall
            // behind it. Keyed on the RING CENTRE shell (|spine.front|), NOT
            // the pit floor: the Errington ring spans the pit's full depth,
            // and a floor-keyed trim also dropped the crater's cone-to-floor
            // wrap (audit-measured: 20 see-through leaks under the sleeve's
            // floor annulus). Only the ring's UPPER half carries the brow.
            // T23 red-team P2-1: the arch band runs BEFORE the subdividing
            // trims now — it matches hot ring verts by EXACT FLOAT identity,
            // and a subdivided facet's midpoint leaves are new unshared verts
            // that can never match (sub-facet sail fragments off the arch,
            // the "stretchy arms" class). Order was irrelevant under the
            // whole-facet trims; it is load-bearing under subdivision.
            if (net.pit.bowl_depth > 0.0)
                drop_arch_band_tris(ends.ring0, net.pit.axis,
                                    glm::length(net.spine.front().pos), collar);
            drop_tube_swallowed_tris(net, collar, tube_trim_split_depth);
            drop_cut_swallowed_tris(
                net,
                {&net.bowl, &net.trench[0], &net.trench[1], &net.trench[2]},
                collar, /*cone_cut=*/nullptr, /*split_depth=*/kMouthSplitDepth);
        }
        // T20 (fly-16, Chad: "errington is impregnable!"): the T19 darkening
        // is Murray-only. At Errington the adit sits recessed UNDER the
        // portal canopy — fading its surround erased the arch's contrast on
        // the low trench-line approach and the entrance read as a sealed
        // building. Contrast IS the Errington entrance read.
        collar.kind = PieceKind::kCollar;
        collar.is_collar = true;
        assert_u16(collar);
        data.pieces.push_back(std::move(collar));
    }
    // [7] Murray = the OPEN-PIT BOWL WALL (T4a): a multi-ring cone from the
    // terrain rim down to the tube end ring at the bowl floor. The spine's back
    // node already sits at the bowl floor (surface_r - bowl_depth), so the tube
    // end ring is the exact bowl-floor tube opening the wall closes onto.
    {
        TunnelPiece bowl;
        // T16 ENTRANCE-ART: the Murray raid mouth is a proper OPEN-PIT mine —
        // stepped terraces (benched), reading as a real pit from the air.
        build_bowl_wall(net.bowl, resolved_mur_collar, ends.ring1, ground,
                        bare_R, terrain_cell_arc_m, /*benched=*/true, bowl);
        // T22: the throat's rising rock apron gets a DRAWN face (before the
        // trims — it sits proud of the collision floor — and before the
        // darkening, so it inherits the mouth blend).
        build_throat_ramp(net, ground, bare_R, bowl);
        bowl.trim_base_verts = bowl.cue.size();  // T23 count-pin mark
        if (cut_swallow_trim) {  // T17: yield to the bore + the OTHER cuts
            // T18: free the Murray arch — the back drape hung over the open
            // slot ("mesh strung across the entrance"); the slot's tube walls
            // seal behind it. T23 red-team P2-1: BEFORE the subdividing
            // trims (exact-float hot matching — see the Errington note).
            if (net.bowl.bowl_depth > 0.0)
                drop_arch_band_tris(ends.ring1, net.bowl.axis,
                                    net.bowl.surface_r - net.bowl.bowl_depth,
                                    bowl);
            drop_tube_swallowed_tris(net, bowl, tube_trim_split_depth);
            drop_cut_swallowed_tris(
                net, {&net.pit, &net.trench[0], &net.trench[1], &net.trench[2]},
                bowl, /*cone_cut=*/nullptr, /*split_depth=*/kMouthSplitDepth);
        }
        // T19 (B): fade the pale Murray open-pit surround into the black bore
        // slot near the floor mouth (the "out of place" fix, fly-15).
        darken_cue_near(bowl, net.spine.back().pos,
                        kMouthBlendMurrayFrac * net.tube_width);
        bowl.kind = PieceKind::kCollar;
        bowl.is_collar = true;
        assert_u16(bowl);
        data.pieces.push_back(std::move(bowl));
    }

    // T11/T12 — THE SEALED-CORE ARENA: the shallow ARENA interior (an oriented
    // oblate ellipsoid, breached by the two bores). T12 SEALED THE FLOOR: the
    // floor-shaft opening + the core-window shaft tube + the emissive core
    // sphere are ALL GONE (Chad: "cover up the core... make sure the chamber is
    // filled over the core"). Only when the arena is live.
    if (net.arena_on) {
        // T14: the hole cut routes through the SAME breaches the tube
        // suppression used, so the cut opening and the bore mouth align by
        // construction (the round-10 blind spots were exactly this fork).
        TunnelPiece arena;
        build_arena(net, breaches, arena);
        // T15/T16: seal the ragged facet margin — one on-wall annulus per
        // breach, from the TRUE bore-opening rim (collision truth) out by the
        // collar pad. Same piece (kCavern shading; no piece-count change).
        if (breaches.size() == 2) {
            build_breach_collar(net, breaches[0], ends.ring_bE, arena);
            build_breach_collar(net, breaches[1], ends.ring_bM, arena);
        }
        arena.kind = PieceKind::kCavern;
        assert_u16(arena);
        data.pieces.push_back(std::move(arena));
    }

    // T16 DEFECT-2 — THE APPROACH-TRENCH WALLS (own kTrench piece, EXTERIOR).
    // Each entry-trench step's open cut is draped with terrain-hugging rim band
    // + vertical wall + floor disk so the ramp no longer sees through the cut
    // to space. Own piece (not the mouth collar) so the mouth-seam / band-hug /
    // two-mouth-collar invariants stay clean. Present ONLY when the trench is
    // live (some step has bowl_depth > 0) => a trench-OFF net adds no piece
    // (bit-identical). Appended before the portal so the portal stays LAST.
    {
        TunnelPiece trench;
        for (int ti = 0; ti < world::TunnelNet::kTrenchSteps; ++ti) {
            const world::TunnelNet::Bowl& tb = net.trench[ti];
            if (tb.bowl_depth <= 0.0) continue;
            const double t_outer =
                terrain_cell_arc_m > 0.0
                    ? collar_reach_from_cell(tb.bowl_r, terrain_cell_arc_m)
                    : tb.bowl_r;
            build_trench_step(tb, t_outer, ground, bare_R, terrain_cell_arc_m,
                              trench);
        }
        // T17: the three trench steps OVERLAP each other and abut the pit —
        // each step's full-circle wall/floor drape rendered straight across
        // its neighbours' open cuts, roofing the whole Errington approach
        // into a slab (fly-13). Drop every drape facet swallowed by ANOTHER
        // cut; the rock-backed cover (the T16 see-through seal) survives.
        // (A facet on its OWN step's boundary reads sd ~ 0 there — never
        // dropped by its own cut at the 5 m eps.)
        trench.trim_base_verts = trench.cue.size();  // T23 count-pin mark
        if (cut_swallow_trim)
            drop_cut_swallowed_tris(net,
                                    {&net.bowl, &net.pit, &net.trench[0],
                                     &net.trench[1], &net.trench[2]},
                                    trench, /*cone_cut=*/nullptr,
                                    /*split_depth=*/kMouthSplitDepth);  // T22:
        // fly-16 "the way in is fine its just a bit unclean" — the channel's
        // border flaps get the same depth-4 shrink the mouths got in T19.
        if (!trench.indices.empty()) {
            trench.kind = PieceKind::kTrench;  // own kind (not kCollar): the
            trench.is_collar = false;  // "two mouth collars" pins stay exact
            assert_u16(trench);
            data.pieces.push_back(std::move(trench));
        }
    }

    // T16 ENTRANCE-ART — THE ERRINGTON PORTAL FRAME (visual-only, NO SDF).
    // Appended LAST so it perturbs no body piece (and the portal on/off arm
    // test reads it as the last piece).
    {
        TunnelPiece portal;
        build_portal(net, ground, bare_R, portal);  // T19: ground hugs ruins
        if (!portal.indices.empty()) {
            portal.kind = PieceKind::kPortal;
            assert_u16(portal);
            data.pieces.push_back(std::move(portal));
        }
    }

    return data;
}

// ---------------------------------------------------------------------------
// GASLAMPS (T4b). Pure placement — see the header contract. Every lamp is
// verified strictly inside the net (sd < kLampInsideMargin) before it is kept.
// ---------------------------------------------------------------------------
namespace {

constexpr double kPi = 3.14159265358979323846;

// Keep a lamp only if it is strictly inside the open volume.
void keep_if_inside(const world::TunnelNet& net, const glm::dvec3& pos,
                    float intensity, std::vector<TunnelLamp>& out) {
    if (net.signed_distance(pos) < kLampInsideMargin)
        out.push_back(TunnelLamp{pos, intensity});
}

// Round-17 finding ("Murray entrance lights look half buried" — also present,
// unreported, at Errington): the T12/T16 mouth-ring beacons are drawn in the
// BRIGHT lamp tier (48 m glow radius, render/tunnel.cpp bright_lamp_look()),
// but kMouthBeaconFrac alone only clears ~20% of the bore semi-axes off the
// wall (~18-22 m at the shipped tube_width/height) — far short of the 48 m
// glow, so the wall's own depth-tested geometry slices through every
// billboard and each beacon reads as a lamp sunk into the rock. This SHARED
// helper (T12 mouth ring + T16 Murray throat rings both call it) picks the
// SMALLER of the frac-based radius (the original "stay inside the net" pin —
// a flat metre inset alone once put the Murray ring partly outside the net,
// per the T12 note above) and a wall-clearance-derived radius sized off the
// bright tier's own glow, so the ring can never sit closer to the wall than
// its own glow radius, in EITHER semi-axis independently (a small tube axis
// clamps only that axis, not the whole ellipse). Floored so a pathologically
// small tube never yields a degenerate/negative radius.
inline constexpr double kBrightBeaconGlowRadius_m =
    48.0;  // == bright_lamp_look().size_m (render/tunnel.cpp) — single-source
           // COMMENT only (the two files don't share a symbol); keep in sync
           // if the bright glow size ever changes.
inline constexpr double kMouthBeaconWallClearance_m =
    kBrightBeaconGlowRadius_m + 10.0;  // glow radius + a small buffer [m]
inline constexpr double kMouthBeaconMinRadiusFrac =
    0.15;  // never inset past 15% of the semi-axis (keeps a legible ring on
           // a narrow tube even if the clearance target can't be met)

double mouth_beacon_axis_radius(double frac_radius, double axis) {
    const double clearance_radius = axis - kMouthBeaconWallClearance_m;
    const double floor_radius = kMouthBeaconMinRadiusFrac * axis;
    return std::max(std::min(frac_radius, clearance_radius), floor_radius);
}

}  // namespace

std::vector<TunnelLamp> place_tunnel_lamps(const world::TunnelNet& net) {
    std::vector<TunnelLamp> lamps;
    if (net.spine.size() < 2) return lamps;

    // --- FLOOR-EDGE PAIRS (T5b): runway-edge lamps lining BOTH sides of the
    // spine tube where the floor meets the wall, every ~kFloorLampSpacing along
    // the whole spine (incl. the bottom ramp). With the T5a floor ON the pair
    // sits at the floor chord edges, raised a couple metres + inset from the
    // wall; with the floor OFF it falls back to ~±kFloorEdgeDownDeg low-wall
    // positions so the runway read survives. Nodes inside the egg/chambers/bowl
    // are skipped by the strict-inside test (a wall lamp there is buried).
    const double rw = net.tube_width;            // horizontal semi (T6b)
    const double rh = net.tube_height;           // vertical semi
    const double crown_mount = rh - kLampInset;  // crown inset from the ceiling
    const bool floor_on = net.floor_height > 0.0;
    const double cos_edge = std::cos(kFloorEdgeDownDeg * kPi / 180.0);
    const double sin_edge = std::sin(kFloorEdgeDownDeg * kPi / 180.0);
    double since_floor = kFloorLampSpacing;  // place at the first eligible node
    for (std::size_t i = 0; i < net.spine.size(); ++i) {
        if (i > 0)
            since_floor += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
        if (since_floor < kFloorLampSpacing) continue;

        const glm::dvec3 c = net.spine[i].pos;
        const glm::dvec3 tan = spine_tangent(net, i);
        const glm::dvec3 up = glm::normalize(c);
        glm::dvec3 u_side = glm::cross(tan, up);  // horizontal lateral (⟂ up)
        const double sl = glm::length(u_side);
        if (sl < 1e-9) continue;  // degenerate (tube straight up) — skip
        u_side /= sl;

        // The two floor-edge lamp positions (left/right).
        glm::dvec3 left, right;
        if (floor_on && net.spine[i].floor_r > 0.0) {
            // Floor chord on the ELLIPSE: center dropped `d` below the tube
            // center along up; the ellipse half-width there =
            // rw*sqrt(1-(d/rh)^2). Lamps sit inset from the wall + raised above
            // the floor.
            const double d = glm::length(c) - net.spine[i].floor_r;  // >0
            if (d < rh) {
                const double frac = d / rh;
                const double half =
                    rw * std::sqrt(std::max(0.0, 1.0 - frac * frac));
                const double lamp_half = std::max(0.0, half - kLampInset);
                const glm::dvec3 fc = c - up * d + up * kFloorLampRaise;
                left = fc - u_side * lamp_half;
                right = fc + u_side * lamp_half;
            } else {
                continue;  // floor below the bore here (shouldn't happen)
            }
        } else {
            // Floor OFF: low wall positions ~kFloorEdgeDownDeg below
            // horizontal, on the ellipse (horizontal semi rw, vertical semi rh,
            // both inset).
            const double hw = rw - kLampInset;
            const double hv = rh - kLampInset;
            left = c + (-cos_edge * u_side * hw - sin_edge * up * hv);
            right = c + (cos_edge * u_side * hw - sin_edge * up * hv);
        }
        const std::size_t before = lamps.size();
        keep_if_inside(net, left, kTubeLampIntensity, lamps);
        keep_if_inside(net, right, kTubeLampIntensity, lamps);
        // Advance the accumulator only when at least ONE of the pair was kept,
        // so a run of skipped (pocket) nodes doesn't desync the spacing.
        if (lamps.size() != before) since_floor = 0.0;
    }

    // --- CROWN/ARC LINE (T5b): a single line of lamps along the TOP of the
    // arch, every ~kCrownLampSpacing, inset from the ceiling. "The arc of the
    // tunnel" (Chad). Same skip-inside discipline.
    double since_crown = kCrownLampSpacing;
    for (std::size_t i = 0; i < net.spine.size(); ++i) {
        if (i > 0)
            since_crown += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
        if (since_crown < kCrownLampSpacing) continue;
        const glm::dvec3 c = net.spine[i].pos;
        const glm::dvec3 up = glm::normalize(c);
        const glm::dvec3 crown =
            c + up * crown_mount;  // top of the arch, inset
        const std::size_t before = lamps.size();
        keep_if_inside(net, crown, kTubeLampIntensity, lamps);
        if (lamps.size() != before) since_crown = 0.0;
    }

    // T10 — the Black Stope egg is GONE; the CORE now lights the cavern (an
    // emissive mesh + glow billboard, render/tunnel.cpp), not a lamp ring.

    // --- CHAMBER lamps: kChamberLampCount per side chamber, ringed at chamber-
    // center height around its own u_lat/u_up frame at the lateral wall.
    for (int ci = 0; ci < 2; ++ci) {
        const world::TunnelNet::Pocket& ch = net.chamber[ci];
        if (ch.b <= 0.0) continue;  // T13: chamber gated off — no lamps
        const double lamp_r = ch.b - kLampInset;
        if (lamp_r > 0.0) {
            for (int k = 0; k < kChamberLampCount; ++k) {
                const double a = 2.0 * kPi * k / kChamberLampCount;
                const glm::dvec3 radial =
                    std::cos(a) * ch.u_lat + std::sin(a) * ch.u_up;
                keep_if_inside(net, ch.center + radial * lamp_r,
                               kChamberLampIntensity, lamps);
            }
        }
    }

    // --- CONNECTOR lamps: kConnectorLampCount spaced along each connector axis
    // (near the endpoints are inside the fat pockets, so bias to the middle).
    for (int ci = 0; ci < 2; ++ci) {
        const world::TunnelNet::Capsule& conn = net.connector[ci];
        if (conn.r <= 0.0) continue;  // T13: connector gated off — no lamps
        for (int k = 0; k < kConnectorLampCount; ++k) {
            // Evenly interior fractions (e.g. 2 lamps => 1/3, 2/3).
            const double f = static_cast<double>(k + 1) /
                             static_cast<double>(kConnectorLampCount + 1);
            const glm::dvec3 pos = conn.a + (conn.b - conn.a) * f;
            keep_if_inside(net, pos, kChamberLampIntensity, lamps);
        }
    }

    // --- T11 ARENA READABILITY: the ember field + breach beacon rings. Only
    // when the arena is live. These lamps sit ON the arena ellipsoid ceiling
    // surface — so they are NOT inside-net gated (signed_distance ~ 0 there,
    // not < -2; the strict-inside gate would drop them all). Placement is the
    // only cull (breach-hole + floor-shaft skip for the embers).
    if (net.arena_on && net.arena.b > 0.0) {
        const world::TunnelNet::Pocket& ar = net.arena;
        // T14: the SAME breaches + hole predicate the arena mesh cut with —
        // embers skip the true openings, beacons ring them (the round-10 bug
        // was beacons ringing misplaced death-holes ~800 m from the real,
        // unlit openings).
        const std::vector<Breach> breaches = breach_points(net);
        const auto in_hole = [&](const glm::dvec3& wdir) {
            return breach_hole_hit(breaches, wdir);
        };
        // A point directly on the arena ellipsoid CEILING surface, from a
        // FRAME-LOCAL unit direction `fu` (in the arena's u_long/u_lat/u_up
        // frame): pos = center + (a_pos*fu.x)*u_long + b*fu.y*u_lat +
        // c*fu.z*u_up. That lands EXACTLY on the ellipsoid (the arena mesh's
        // own parameterization), no ray-cast needed.
        const auto ell_point = [&](const glm::dvec3& fu) -> glm::dvec3 {
            const double aL = fu.x >= 0.0 ? ar.a_pos : ar.a_neg;
            return ar.center + aL * fu.x * ar.u_long + ar.b * fu.y * ar.u_lat +
                   ar.c * fu.z * ar.u_up;
        };

        // EMBER FIELD: a Fibonacci-HEMISPHERE lattice (deterministic, no
        // random) over the CEILING half (frame-local +u_long side) of the arena
        // ellipsoid. Each lattice unit direction maps onto the ellipsoid
        // ceiling surface. Breach holes skip. T12: the floor-shaft skip is GONE
        // (the floor is sealed; the ceiling embers never approached it anyway).
        const double golden = kPi * (3.0 - std::sqrt(5.0));  // ~2.399963 rad
        for (int i = 0; i < kCavernEmberCount; ++i) {
            // Ceiling hemisphere lattice: x (== the u_long axis) in (0, 1].
            const double x = 1.0 - (static_cast<double>(i) + 0.5) /
                                       kCavernEmberCount;  // (0, 1)
            const double rr = std::sqrt(std::max(0.0, 1.0 - x * x));
            const double th = golden * i;
            const glm::dvec3 fu{x, rr * std::cos(th), rr * std::sin(th)};
            const glm::dvec3 pos = ell_point(fu);
            if (in_hole(glm::normalize(pos))) continue;  // bore reads through
            lamps.push_back(TunnelLamp{pos, kCavernEmberIntensity});
        }

        // T12 FLOOR EMBERS: the SAME Fibonacci lattice on the FLOOR hemisphere
        // (frame-local -u_long side, x in [-1, 0)), so the SEALED floor is not
        // a black void when the pilot looks down (the core that used to glow
        // there is gone). Breach holes skip (defensive — they sit near the
        // apex).
        for (int i = 0; i < kFloorEmberCount; ++i) {
            const double x =
                -(1.0 - (static_cast<double>(i) + 0.5) / kFloorEmberCount);
            const double rr = std::sqrt(std::max(0.0, 1.0 - x * x));
            const double th = golden * i;
            const glm::dvec3 fu{x, rr * std::cos(th), rr * std::sin(th)};
            const glm::dvec3 pos = ell_point(fu);
            if (in_hole(glm::normalize(pos))) continue;
            lamps.push_back(TunnelLamp{pos, kCavernEmberIntensity});
        }

        // BREACH BEACON RINGS (T14/T14b): kBreachBeaconCount bright lamps on
        // the ELLIPSE just outside each cut hole's lip (the breach's OWN baked
        // semi-axes + margin — the ring hugs whatever the cut is), centred on
        // each TRUE crossing. Each ring DIRECTION (the same planet-centre
        // direction space the facet cut keys on) is ray-cast onto the arena
        // ellipsoid's ceiling surface — NOT the frame-parameter ell_point map,
        // which distorts by km on the oblate shoulder. From across the arena
        // each REAL exit reads as a ring of light hugging the actual cut rim.
        for (const Breach& b : breaches) {
            const double R_local = glm::length(b.pos);
            if (R_local <= 0.0) continue;
            const glm::dvec3 ax = b.pos / R_local;
            const double minor =
                (b.hole_minor_m + kBreachBeaconArcMargin_m) / R_local;
            glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
            const double el = glm::length(e1);
            // Near-radial crossing: the cut predicate falls back to a ROUND
            // hole (its e1 is undefined), so the ring must be round too — an
            // elongated ring along an arbitrary perp_seed would outline an
            // ellipse the cut doesn't have (red-team P2 fork).
            const double major =
                el > 1e-9
                    ? (b.hole_major_m + kBreachBeaconArcMargin_m) / R_local
                    : minor;
            e1 = el > 1e-9 ? e1 / el : perp_seed(ax, glm::dvec3{0, 1, 0});
            const glm::dvec3 e2 = glm::normalize(glm::cross(ax, e1));
            // Ray-cast each ring direction onto the arena CEILING surface via
            // the SHARED arena_surface_point (one projection with the T15
            // collar rim — no fork; red-team P2 config-relative scan start).
            const auto surface_point = [&](const glm::dvec3& wdir,
                                           glm::dvec3& out) {
                return arena_surface_point(net, R_local, wdir, out);
            };
            for (int k = 0; k < kBreachBeaconCount; ++k) {
                const double a = 2.0 * kPi * k / kBreachBeaconCount;
                const double u = major * std::cos(a);
                const double v = minor * std::sin(a);
                const double rho = std::sqrt(u * u + v * v);
                if (rho < 1e-12) continue;
                const glm::dvec3 dirp = (u * e1 + v * e2) / rho;
                const glm::dvec3 wdir =
                    glm::normalize(std::cos(rho) * ax + std::sin(rho) * dirp);
                glm::dvec3 pos;
                if (surface_point(wdir, pos))
                    lamps.push_back(TunnelLamp{pos, kBreachBeaconIntensity});
            }
        }
    }

    // --- T12 MOUTH BEACON RINGS (Chad: "the tunnel shaft is still hard to
    // find"). A bright ring of lamps around the actual bore opening at each
    // mouth face (the spine front = Errington, back = Murray), so the shaft
    // reads as a RING OF LIGHT inside the dark entry crater from approach
    // distance. Placed on the bore ellipse at the mouth station, in the
    // bore's own (horiz, vert) frame — the SAME frame build_tube builds the
    // mouth ring in, so the beacons sit exactly on the visible opening.
    // Bright tier (kBreachBeaconIntensity). Both mouths (Round-17: Murray
    // FLOWN and reported "half buried" — see mouth_beacon_axis_radius above,
    // which both mouths now route through). Kept only if strictly inside the
    // net (so a beacon never floats in the open pit outside the tube).
    {
        const double rw = net.tube_width, rh = net.tube_height;
        const std::size_t nsp = net.spine.size();
        const std::size_t mouth_i[2] = {0, nsp - 1};
        for (int mi = 0; mi < 2; ++mi) {
            const std::size_t i = mouth_i[mi];
            const glm::dvec3 c = net.spine[i].pos;
            const glm::dvec3 up = glm::normalize(c);
            const glm::dvec3 tan = spine_tangent(net, i);
            glm::dvec3 horiz = glm::cross(tan, up);
            const double hl = glm::length(horiz);
            if (hl < 1e-9) continue;
            horiz /= hl;
            const glm::dvec3 vert = glm::normalize(glm::cross(horiz, tan));
            // Round-17: clamp each semi-axis clear of the wall beyond the
            // bright beacon glow radius (see mouth_beacon_axis_radius above).
            const double rx = mouth_beacon_axis_radius(kMouthBeaconFrac * rw, rw);
            const double ry = mouth_beacon_axis_radius(kMouthBeaconFrac * rh, rh);
            for (int k = 0; k < kMouthBeaconCount; ++k) {
                const double a = 2.0 * kPi * k / kMouthBeaconCount;
                const glm::dvec3 pos =
                    c + rx * std::cos(a) * horiz + ry * std::sin(a) * vert;
                keep_if_inside(net, pos, kBreachBeaconIntensity, lamps);
            }
        }
    }

    // --- T14b MURRAY BOWL FUNNEL (fly-11: "a blind entrance ... its black and
    // I can only see one light going down, caused me to crash"). The pit had
    // no lamps above the bore mouth ring 300 m down — a black hole with one
    // glow. Two rings light the funnel: the RIM ring at the surface (the pit
    // outline from approach AND on the climb out) and a MID-DEPTH ring on the
    // funnel wall (a depth rung between rim and mouth). Own bright-family tier
    // kBowlBeaconIntensity. Placed just inside the open cut (on the SDF
    // cylinder boundary region), not inside-net-gated for the same reason the
    // arena beacons aren't (they sit ON the cut boundary, sd ~ 0).
    // bowl_depth <= 0 => structurally absent (bit-identical lamp list).
    if (net.bowl.bowl_depth > 0.0 && net.bowl.bowl_r > 0.0) {
        const world::TunnelNet::Bowl& bw = net.bowl;
        const glm::dvec3 u = perp_seed(bw.axis, glm::dvec3{0, 1, 0});
        const glm::dvec3 w = glm::normalize(glm::cross(bw.axis, u));
        // RIM ring: at the surface plane, nested a few metres into the cut.
        const double rim_r = bw.bowl_r - kBowlBeaconInset;
        for (int k = 0; k < kBowlRimBeaconCount; ++k) {
            const double a = 2.0 * kPi * k / kBowlRimBeaconCount;
            const glm::dvec3 radial = std::cos(a) * u + std::sin(a) * w;
            const glm::dvec3 pos =
                bw.axis * (bw.surface_r - 5.0) + radial * rim_r;
            lamps.push_back(TunnelLamp{pos, kBowlBeaconIntensity});
        }
        // MID ring: half-depth, on the visual funnel taper (rim -> floor_r),
        // inset off the wall.
        const double mid_r = 0.5 * (bw.bowl_r + bw.floor_r) - kBowlBeaconInset;
        if (mid_r > 0.0) {
            for (int k = 0; k < kBowlMidBeaconCount; ++k) {
                const double a = 2.0 * kPi * (k + 0.5) / kBowlMidBeaconCount;
                const glm::dvec3 radial = std::cos(a) * u + std::sin(a) * w;
                const glm::dvec3 pos =
                    bw.axis * (bw.surface_r - 0.5 * bw.bowl_depth) +
                    radial * mid_r;
                lamps.push_back(TunnelLamp{pos, kBowlBeaconIntensity});
            }
        }
    }

    // --- T16 ENTRANCE-ART: MURRAY EXIT THROAT RINGS (round-12: "leaving the
    // Murray tunnel into the open pit is still a BLIND flight — nothing marks
    // the final tube stretch from inside"). Three rings of throat beacons in
    // the last ~kMurrayThroatRings*kMurrayThroatSpacing of BORE before the
    // Murray floor mouth (placed by spine arc-length from the Murray mouth,
    // spine.back), on the bore ellipse — a ladder of rings to the exit. Own
    // bright-family tier. Kept only if strictly inside the net.
    {
        const auto& sp = net.spine;
        const std::size_t n = sp.size();
        if (n >= 2) {
            const double rw = net.tube_width, rh = net.tube_height;
            // Round-17: same wall-clearance clamp as the T12 mouth ring.
            const double rx = mouth_beacon_axis_radius(kMouthBeaconFrac * rw, rw);
            const double ry = mouth_beacon_axis_radius(kMouthBeaconFrac * rh, rh);
            for (int r = 0; r < kMurrayThroatRings; ++r) {
                const double target = kMurrayThroatSpacing_m * (r + 1);
                // Walk from the Murray mouth (back) inward, accumulating arc.
                double arc = 0.0;
                glm::dvec3 pos{0.0}, tan{0.0};
                bool found = false;
                for (std::size_t i = n - 1; i > 0; --i) {
                    const glm::dvec3 seg = sp[i - 1].pos - sp[i].pos;
                    const double sl = glm::length(seg);
                    if (sl < 1e-9) continue;
                    if (arc + sl >= target) {
                        pos = sp[i].pos + seg * ((target - arc) / sl);
                        tan = glm::normalize(seg);  // inward from the mouth
                        found = true;
                        break;
                    }
                    arc += sl;
                }
                if (!found) continue;  // spine shorter than this ring's arc
                const glm::dvec3 up = glm::normalize(pos);
                glm::dvec3 horiz = glm::cross(tan, up);
                const double hl = glm::length(horiz);
                if (hl < 1e-9) continue;
                horiz /= hl;
                const glm::dvec3 vert = glm::normalize(glm::cross(horiz, tan));
                for (int k = 0; k < kMurrayThroatPerRing; ++k) {
                    const double a = 2.0 * kPi * k / kMurrayThroatPerRing;
                    const glm::dvec3 lp = pos + rx * std::cos(a) * horiz +
                                          ry * std::sin(a) * vert;
                    keep_if_inside(net, lp, kMurrayThroatIntensity, lamps);
                }
            }
        }
    }

    // --- T16 ENTRANCE-ART: PORTAL LINTEL BEACONS (the framed adit reads at
    // dusk). kPortalLintelBeacons aviation beacons across the lintel top,
    // placed on the portal structure by construction (they sit in the open pit
    // above the net, so they bypass the strict-inside gate — the SAME bright
    // tier as the mouth beacons). OFF when the portal is off (empty list).
    for (const glm::dvec3& bp : portal_beacon_points(net))
        lamps.push_back(TunnelLamp{bp, kBreachBeaconIntensity});

    return lamps;
}

}  // namespace render
