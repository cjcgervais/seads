// Procedural low-poly WWII fighter (see aircraft_mesh.h). Everything here is convex pieces built
// from two primitives: octagonal ring LOFTS (fuselage/cowl skins + end fans) and 8-corner SLABS
// (wings, stabilizers, fin, canopy, propeller blades). Winding is derived, not hand-authored:
// every face is wound so its normal agrees with an outward hint (radial for lofts, centroid-out
// for slabs), which the headless client test verifies triangle-by-triangle.
#include "aircraft_mesh.h"

#include <array>
#include <cmath>
#include <utility>

namespace seads {
namespace client {
namespace {

struct V3 {
    float x, y, z;
};
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 crossf(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dotf(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 normf(V3 a) {
    float n = std::sqrt(dotf(a, a));
    return n > 0 ? V3{a.x / n, a.y / n, a.z / n} : a;
}

// Fixed body-frame key light, baked per vertex (deterministic; no runtime lighting shader).
float shade_of(V3 n) {
    const V3 L = normf(V3{0.35f, 0.85f, 0.30f});
    float d = dotf(n, L);
    if (d < 0) d = 0;
    return 0.62f + 0.38f * d;
}

void tri(MeshPart& p, V3 a, V3 b, V3 c) {
    V3 n = normf(crossf(sub(b, a), sub(c, a)));
    const V3 v[3] = {a, b, c};
    for (const V3& q : v) {
        p.vertices.push_back(q.x);
        p.vertices.push_back(q.y);
        p.vertices.push_back(q.z);
        p.normals.push_back(n.x);
        p.normals.push_back(n.y);
        p.normals.push_back(n.z);
        p.shade.push_back(shade_of(n));
    }
}

// Triangle wound so its normal points along `hint` (outward for the piece being skinned).
void tri_out(MeshPart& p, V3 a, V3 b, V3 c, V3 hint) {
    if (dotf(crossf(sub(b, a), sub(c, a)), hint) < 0.0f) std::swap(b, c);
    tri(p, a, b, c);
}
void quad_out(MeshPart& p, V3 a, V3 b, V3 c, V3 d, V3 hint) {
    tri_out(p, a, b, c, hint);
    tri_out(p, a, c, d, hint);
}

// Octagonal fuselage cross-section at station x: radius r, hull centreline height y0.
using Ring = std::array<V3, 8>;
Ring ring(float x, float r, float y0) {
    Ring out;
    for (int k = 0; k < 8; ++k) {
        float a = 2.0f * 3.14159265f * static_cast<float>(k) / 8.0f;
        out[k] = V3{x, y0 + r * std::cos(a), r * std::sin(a)};
    }
    return out;
}

// Skin between two rings; each face wound outward from the local hull axis.
void loft(MeshPart& p, const Ring& A, const Ring& B, float y0a, float y0b) {
    for (int k = 0; k < 8; ++k) {
        int j = (k + 1) % 8;
        V3 c{(A[k].x + A[j].x + B[k].x + B[j].x) * 0.25f,
             (A[k].y + A[j].y + B[k].y + B[j].y) * 0.25f,
             (A[k].z + A[j].z + B[k].z + B[j].z) * 0.25f};
        quad_out(p, A[k], A[j], B[j], B[k], sub(c, V3{c.x, (y0a + y0b) * 0.5f, 0.0f}));
    }
}

// Close a ring to an apex point (spinner cone / tail cap).
void fan(MeshPart& p, const Ring& A, V3 apex, V3 hint) {
    for (int k = 0; k < 8; ++k) tri_out(p, A[k], A[(k + 1) % 8], apex, hint);
}

// Convex 8-corner solid from two matching perimeter quads (t = one face, b = the opposite);
// all 6 faces wound outward from the solid centroid. Handles every plate on the aircraft.
void slab(MeshPart& p, const V3 t[4], const V3 b[4]) {
    V3 c{0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        c.x += (t[i].x + b[i].x) * 0.125f;
        c.y += (t[i].y + b[i].y) * 0.125f;
        c.z += (t[i].z + b[i].z) * 0.125f;
    }
    auto face = [&](V3 a, V3 bb, V3 cc, V3 d) {
        V3 fc{(a.x + bb.x + cc.x + d.x) * 0.25f, (a.y + bb.y + cc.y + d.y) * 0.25f,
              (a.z + bb.z + cc.z + d.z) * 0.25f};
        quad_out(p, a, bb, cc, d, sub(fc, c));
    };
    face(t[0], t[1], t[2], t[3]);
    face(b[0], b[1], b[2], b[3]);
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        face(t[i], t[j], b[j], b[i]);
    }
}

// One tapered lifting plate (wing / horizontal stabilizer), mirrored by s = +1 / -1. Corners run
// root-trailing -> root-leading -> tip-leading -> tip-trailing; th = half thickness.
void wing_plate(MeshPart& p, float s, float x_te_r, float x_le_r, float z_r, float x_te_t,
                float x_le_t, float z_t, float y_r, float y_t, float th) {
    V3 t[4] = {{x_te_r, y_r + th, s * z_r},
               {x_le_r, y_r + th, s * z_r},
               {x_le_t, y_t + th, s * z_t},
               {x_te_t, y_t + th, s * z_t}};
    V3 b[4] = {{x_te_r, y_r - th, s * z_r},
               {x_le_r, y_r - th, s * z_r},
               {x_le_t, y_t - th, s * z_t},
               {x_te_t, y_t - th, s * z_t}};
    slab(p, t, b);
}

}  // namespace

FighterMesh build_fighter_mesh() {
    FighterMesh fm;
    // Fuselage stations nose to tail: a tapered octagonal hull, centreline rising toward the tail.
    const Ring r_tail1 = ring(-0.52f, 0.012f, 0.020f);
    const Ring r_tail2 = ring(-0.34f, 0.035f, 0.015f);
    const Ring r_join = ring(-0.15f, 0.055f, 0.0f);
    const Ring r_mid = ring(0.06f, 0.065f, 0.0f);
    const Ring r_cowl = ring(0.28f, 0.062f, 0.0f);
    const Ring r_nose = ring(0.46f, 0.048f, 0.0f);

    // TAIL: rear hull + cap + horizontal stabilizers + vertical fin.
    loft(fm.tail, r_tail1, r_tail2, 0.020f, 0.015f);
    loft(fm.tail, r_tail2, r_join, 0.015f, 0.0f);
    fan(fm.tail, r_tail1, V3{-0.53f, 0.020f, 0.0f}, V3{-1.0f, 0.0f, 0.0f});
    wing_plate(fm.tail, 1.0f, -0.50f, -0.38f, 0.030f, -0.47f, -0.40f, 0.170f, 0.020f, 0.020f, 0.005f);
    wing_plate(fm.tail, -1.0f, -0.50f, -0.38f, 0.030f, -0.47f, -0.40f, 0.170f, 0.020f, 0.020f, 0.005f);
    {
        V3 t[4] = {{-0.51f, 0.030f, 0.008f},
                   {-0.36f, 0.030f, 0.008f},
                   {-0.41f, 0.170f, 0.008f},
                   {-0.47f, 0.170f, 0.008f}};
        V3 b[4] = {{-0.51f, 0.030f, -0.008f},
                   {-0.36f, 0.030f, -0.008f},
                   {-0.41f, 0.170f, -0.008f},
                   {-0.47f, 0.170f, -0.008f}};
        slab(fm.tail, t, b);
    }

    // BODY: central hull + canopy (a low tapered wedge on the spine).
    loft(fm.body, r_join, r_mid, 0.0f, 0.0f);
    loft(fm.body, r_mid, r_cowl, 0.0f, 0.0f);
    {
        V3 t[4] = {{0.04f, 0.105f, 0.024f},
                   {0.13f, 0.105f, 0.024f},
                   {0.13f, 0.105f, -0.024f},
                   {0.04f, 0.105f, -0.024f}};
        V3 b[4] = {{0.00f, 0.050f, 0.038f},
                   {0.18f, 0.050f, 0.038f},
                   {0.18f, 0.050f, -0.038f},
                   {0.00f, 0.050f, -0.038f}};
        slab(fm.body, t, b);
    }

    // ENGINE: cowl + spinner cone + two crossed propeller blades (thin slabs through the spinner).
    loft(fm.engine, r_cowl, r_nose, 0.0f, 0.0f);
    fan(fm.engine, r_nose, V3{0.56f, 0.0f, 0.0f}, V3{1.0f, 0.0f, 0.0f});
    {
        V3 t[4] = {{0.506f, -0.19f, -0.016f},
                   {0.506f, -0.19f, 0.016f},
                   {0.506f, 0.19f, 0.016f},
                   {0.506f, 0.19f, -0.016f}};
        V3 b[4] = {{0.498f, -0.19f, -0.016f},
                   {0.498f, -0.19f, 0.016f},
                   {0.498f, 0.19f, 0.016f},
                   {0.498f, 0.19f, -0.016f}};
        slab(fm.engine, t, b);
        V3 t2[4] = {{0.506f, -0.016f, -0.19f},
                    {0.506f, 0.016f, -0.19f},
                    {0.506f, 0.016f, 0.19f},
                    {0.506f, -0.016f, 0.19f}};
        V3 b2[4] = {{0.498f, -0.016f, -0.19f},
                    {0.498f, 0.016f, -0.19f},
                    {0.498f, 0.016f, 0.19f},
                    {0.498f, -0.016f, 0.19f}};
        slab(fm.engine, t2, b2);
    }

    // WING: the two main planes (low-mounted, tapered, slight dihedral).
    wing_plate(fm.wing, 1.0f, -0.04f, 0.20f, 0.055f, 0.00f, 0.11f, 0.45f, -0.020f, 0.020f, 0.008f);
    wing_plate(fm.wing, -1.0f, -0.04f, 0.20f, 0.055f, 0.00f, 0.11f, 0.45f, -0.020f, 0.020f, 0.008f);
    return fm;
}

}  // namespace client
}  // namespace seads
