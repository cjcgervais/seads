// Procedural low-poly WWII fighters (see aircraft_mesh.h). Everything here is convex pieces built
// from two primitives: octagonal ring LOFTS (fuselage/cowl skins + end fans) and 8-corner SLABS
// (wings, stabilizers, fin, canopy, propeller blades). Winding is derived, not hand-authored:
// every face is wound so its normal agrees with an outward hint (radial for lofts, centroid-out
// for slabs), which the headless client test verifies triangle-by-triangle.
//
// Per-airframe variants are pure PROPORTIONS: one parameter set per roster type feeds the same
// assembly (radial types get a blunt wide cowl + short nose; inlines a slender pointed one; wing
// plan/span, tail sizes, canopy and overall scale differ per type; the P-51 adds its ventral
// radiator scoop). GENERIC keeps the original one-size fighter as the no-metadata fallback.
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

// Airframe proportions. Defaults ARE the GENERIC fighter (the original one-size model); each
// roster variant overrides the handful of dimensions that carry its silhouette.
struct Proportions {
    // Fuselage stations/radii (+X nose). Radial engines: nose_r near cowl_r (blunt drum) + a
    // short nose; inlines: small nose_r (pointed) + a long one.
    float mid_r = 0.065f;   // max hull radius (r_mid; r_join rides at 0.85x)
    float cowl_x = 0.28f;   // cowl ring station
    float cowl_r = 0.062f;  // radius at the cowl ring
    float nose_x = 0.46f;   // nose ring (front of the cowl)
    float nose_r = 0.048f;  // radius at the nose ring
    float spin_x = 0.56f;   // spinner apex
    // Wing plan: two tapered panels per side, root -> mid -> tip. Chord factors + centreline
    // sweep at mid/tip shape the plan (straight taper, squared tip, or the Spitfire ellipse).
    float wing_xc = 0.08f;     // root chord centreline x
    float wing_chord = 0.24f;  // root chord
    float wing_zm = 0.25f;     // mid spanwise station (root is at the hull side, z=0.055)
    float wing_zt = 0.45f;     // tip (half-span)
    float chord_m = 0.73f;     // chord factor at mid
    float chord_t = 0.46f;     // chord factor at tip
    float sweep_m = -0.012f;   // centreline x shift at mid
    float sweep_t = -0.025f;   // centreline x shift at tip
    float dihedral = 0.040f;   // tip y above the root y (-0.020)
    // Empennage + canopy + prop + extras.
    float stab_zt = 0.170f;  // horizontal-stabilizer half-span
    float fin_top = 0.170f;  // vertical-fin top y
    float can_x0 = 0.04f, can_x1 = 0.13f;  // canopy footprint (razorback/bubble/greenhouse)
    float can_h = 0.105f, can_w = 0.024f;  // canopy height / half-width
    float blade_r = 0.19f;                 // propeller blade reach
    float scoop = 0.0f;                    // ventral radiator scoop depth (P-51; 0 = none)
    float scale = 1.0f;                    // overall size multiplier, applied last
};

Proportions proportions_for(AircraftType t) {
    Proportions p;  // GENERIC
    switch (t) {
        case AircraftType::P47D:  // the Jug: biggest airframe, deep radial drum, razorback
            p.scale = 1.14f; p.mid_r = 0.075f; p.cowl_x = 0.30f; p.cowl_r = 0.075f;
            p.nose_x = 0.43f; p.nose_r = 0.066f; p.spin_x = 0.52f;
            p.wing_chord = 0.26f; p.wing_zt = 0.45f; p.chord_t = 0.42f;
            p.stab_zt = 0.180f; p.fin_top = 0.185f;
            p.can_x0 = 0.02f; p.can_x1 = 0.12f; p.can_h = 0.100f; p.can_w = 0.026f;
            p.blade_r = 0.21f;
            break;
        case AircraftType::BF109F4:  // small inline: pointed spinner, short squarish wing
            p.scale = 0.90f; p.mid_r = 0.058f; p.cowl_r = 0.046f;
            p.nose_x = 0.49f; p.nose_r = 0.028f; p.spin_x = 0.59f;
            p.wing_chord = 0.23f; p.wing_zt = 0.41f; p.chord_t = 0.52f;
            p.stab_zt = 0.150f; p.fin_top = 0.150f;
            p.can_x0 = 0.03f; p.can_x1 = 0.11f; p.can_h = 0.095f; p.can_w = 0.022f;
            p.blade_r = 0.18f;
            break;
        case AircraftType::KI61:  // the roster's one slim liquid-cooled Japanese type: long + slender
            p.scale = 0.97f; p.mid_r = 0.056f; p.cowl_r = 0.044f;
            p.nose_x = 0.50f; p.nose_r = 0.026f; p.spin_x = 0.60f;
            p.wing_chord = 0.22f; p.wing_zt = 0.47f; p.chord_t = 0.40f;
            p.stab_zt = 0.160f; p.fin_top = 0.155f;
            p.blade_r = 0.18f;
            break;
        case AircraftType::A6M2:  // Zero: radial, long rounded wing, greenhouse canopy
            p.mid_r = 0.060f; p.cowl_r = 0.062f;
            p.nose_x = 0.42f; p.nose_r = 0.054f; p.spin_x = 0.50f;
            p.wing_chord = 0.25f; p.wing_zt = 0.49f; p.chord_m = 0.88f; p.chord_t = 0.34f;
            p.stab_zt = 0.170f; p.fin_top = 0.160f;
            p.can_x0 = 0.00f; p.can_x1 = 0.16f; p.can_h = 0.100f; p.can_w = 0.024f;
            p.blade_r = 0.18f;
            break;
        case AircraftType::YAK3:  // smallest airframe on the roster
            p.scale = 0.86f; p.mid_r = 0.056f; p.cowl_r = 0.046f;
            p.nose_x = 0.48f; p.nose_r = 0.030f; p.spin_x = 0.57f;
            p.wing_chord = 0.23f; p.wing_zt = 0.38f; p.chord_t = 0.48f;
            p.stab_zt = 0.145f; p.fin_top = 0.145f;
            p.blade_r = 0.17f;
            break;
        case AircraftType::LA7:  // stubby radial
            p.scale = 0.95f; p.mid_r = 0.064f; p.cowl_r = 0.066f;
            p.nose_x = 0.41f; p.nose_r = 0.058f; p.spin_x = 0.49f;
            p.wing_chord = 0.24f; p.wing_zt = 0.41f; p.chord_t = 0.46f;
            p.stab_zt = 0.155f; p.fin_top = 0.160f;
            p.blade_r = 0.18f;
            break;
        case AircraftType::SPITFIRE_MK5:  // the elliptical wing + tall rounded fin
            p.mid_r = 0.058f; p.cowl_r = 0.046f;
            p.nose_x = 0.49f; p.nose_r = 0.028f; p.spin_x = 0.59f;
            p.wing_chord = 0.26f; p.wing_zm = 0.27f; p.wing_zt = 0.46f;
            p.chord_m = 0.92f; p.chord_t = 0.26f; p.sweep_m = 0.010f; p.sweep_t = -0.010f;
            p.stab_zt = 0.160f; p.fin_top = 0.180f;
            p.blade_r = 0.18f;
            break;
        case AircraftType::P51:  // Mustang: laminar squared wing, bubble canopy, belly scoop
            p.scale = 1.04f; p.mid_r = 0.060f; p.cowl_r = 0.048f;
            p.nose_x = 0.48f; p.nose_r = 0.032f; p.spin_x = 0.58f;
            p.wing_chord = 0.24f; p.wing_zt = 0.44f; p.chord_m = 0.72f; p.chord_t = 0.50f;
            p.stab_zt = 0.165f; p.fin_top = 0.165f;
            p.can_x0 = 0.02f; p.can_x1 = 0.14f; p.can_h = 0.108f; p.can_w = 0.026f;
            p.scoop = 0.030f;
            break;
        case AircraftType::GENERIC:
        default:
            break;
    }
    return p;
}

}  // namespace

AircraftType aircraft_type_from_code(uint32_t code) {
    return code < AIRCRAFT_TYPE_COUNT ? static_cast<AircraftType>(code) : AircraftType::GENERIC;
}

const char* aircraft_type_name(AircraftType t) {
    switch (t) {
        case AircraftType::P47D: return "P-47D";
        case AircraftType::BF109F4: return "Bf 109 F-4";
        case AircraftType::KI61: return "Ki-61";
        case AircraftType::A6M2: return "A6M2";
        case AircraftType::YAK3: return "Yak-3";
        case AircraftType::LA7: return "La-7";
        case AircraftType::SPITFIRE_MK5: return "Spitfire Mk V";
        case AircraftType::P51: return "P-51";
        default: return "fighter";
    }
}

FighterMesh build_fighter_mesh(AircraftType type) {
    const Proportions p = proportions_for(type);
    FighterMesh fm;
    // Fuselage stations nose to tail: a tapered octagonal hull, centreline rising toward the tail.
    const Ring r_tail1 = ring(-0.52f, 0.012f, 0.020f);
    const Ring r_tail2 = ring(-0.34f, 0.035f, 0.015f);
    const Ring r_join = ring(-0.15f, p.mid_r * 0.85f, 0.0f);
    const Ring r_mid = ring(0.06f, p.mid_r, 0.0f);
    const Ring r_cowl = ring(p.cowl_x, p.cowl_r, 0.0f);
    const Ring r_nose = ring(p.nose_x, p.nose_r, 0.0f);

    // TAIL: rear hull + cap + horizontal stabilizers + vertical fin.
    loft(fm.tail, r_tail1, r_tail2, 0.020f, 0.015f);
    loft(fm.tail, r_tail2, r_join, 0.015f, 0.0f);
    fan(fm.tail, r_tail1, V3{-0.53f, 0.020f, 0.0f}, V3{-1.0f, 0.0f, 0.0f});
    for (float s : {1.0f, -1.0f})
        wing_plate(fm.tail, s, -0.50f, -0.38f, 0.030f, -0.47f, -0.40f, p.stab_zt, 0.020f, 0.020f,
                   0.005f);
    {
        V3 t[4] = {{-0.51f, 0.030f, 0.008f},
                   {-0.36f, 0.030f, 0.008f},
                   {-0.41f, p.fin_top, 0.008f},
                   {-0.47f, p.fin_top, 0.008f}};
        V3 b[4] = {{-0.51f, 0.030f, -0.008f},
                   {-0.36f, 0.030f, -0.008f},
                   {-0.41f, p.fin_top, -0.008f},
                   {-0.47f, p.fin_top, -0.008f}};
        slab(fm.tail, t, b);
    }

    // BODY: central hull + canopy (a low tapered wedge on the spine) + optional ventral scoop.
    loft(fm.body, r_join, r_mid, 0.0f, 0.0f);
    loft(fm.body, r_mid, r_cowl, 0.0f, 0.0f);
    {
        V3 t[4] = {{p.can_x0, p.can_h, p.can_w},
                   {p.can_x1, p.can_h, p.can_w},
                   {p.can_x1, p.can_h, -p.can_w},
                   {p.can_x0, p.can_h, -p.can_w}};
        V3 b[4] = {{p.can_x0 - 0.04f, 0.050f, p.can_w + 0.014f},
                   {p.can_x1 + 0.05f, 0.050f, p.can_w + 0.014f},
                   {p.can_x1 + 0.05f, 0.050f, -(p.can_w + 0.014f)},
                   {p.can_x0 - 0.04f, 0.050f, -(p.can_w + 0.014f)}};
        slab(fm.body, t, b);
    }
    if (p.scoop > 0.0f) {  // P-51 belly radiator: a tapered slab under the mid fuselage
        V3 t[4] = {{-0.14f, -0.045f, 0.022f},
                   {0.10f, -0.045f, 0.022f},
                   {0.10f, -0.045f, -0.022f},
                   {-0.14f, -0.045f, -0.022f}};
        V3 b[4] = {{-0.10f, -(p.mid_r + p.scoop), 0.020f},
                   {0.06f, -(p.mid_r + p.scoop), 0.020f},
                   {0.06f, -(p.mid_r + p.scoop), -0.020f},
                   {-0.10f, -(p.mid_r + p.scoop), -0.020f}};
        slab(fm.body, t, b);
    }

    // ENGINE: cowl + spinner cone + two crossed propeller blades (thin slabs through the spinner).
    loft(fm.engine, r_cowl, r_nose, 0.0f, 0.0f);
    fan(fm.engine, r_nose, V3{p.spin_x, 0.0f, 0.0f}, V3{1.0f, 0.0f, 0.0f});
    {
        const float bx = p.nose_x + 0.4f * (p.spin_x - p.nose_x);  // blade station on the spinner
        V3 t[4] = {{bx + 0.004f, -p.blade_r, -0.016f},
                   {bx + 0.004f, -p.blade_r, 0.016f},
                   {bx + 0.004f, p.blade_r, 0.016f},
                   {bx + 0.004f, p.blade_r, -0.016f}};
        V3 b[4] = {{bx - 0.004f, -p.blade_r, -0.016f},
                   {bx - 0.004f, -p.blade_r, 0.016f},
                   {bx - 0.004f, p.blade_r, 0.016f},
                   {bx - 0.004f, p.blade_r, -0.016f}};
        slab(fm.engine, t, b);
        V3 t2[4] = {{bx + 0.004f, -0.016f, -p.blade_r},
                    {bx + 0.004f, 0.016f, -p.blade_r},
                    {bx + 0.004f, 0.016f, p.blade_r},
                    {bx + 0.004f, -0.016f, p.blade_r}};
        V3 b2[4] = {{bx - 0.004f, -0.016f, -p.blade_r},
                    {bx - 0.004f, 0.016f, -p.blade_r},
                    {bx - 0.004f, 0.016f, p.blade_r},
                    {bx - 0.004f, -0.016f, p.blade_r}};
        slab(fm.engine, t2, b2);
    }

    // WING: two tapered panels per side (root -> mid -> tip) so a chord bulge or squared tip can
    // shape the plan (Spitfire ellipse, Bf 109 square cut, A6M2 long rounded taper).
    {
        const float zr = 0.055f, zm = p.wing_zm, zt = p.wing_zt;
        const float yr = -0.020f;
        const float fmid = (zm - zr) / (zt - zr);
        const float ym = yr + fmid * p.dihedral, yt = yr + p.dihedral;
        const float cr = p.wing_chord, cm = cr * p.chord_m, ct = cr * p.chord_t;
        const float xr = p.wing_xc, xm = p.wing_xc + p.sweep_m, xt = p.wing_xc + p.sweep_t;
        for (float s : {1.0f, -1.0f}) {
            wing_plate(fm.wing, s, xr - cr * 0.5f, xr + cr * 0.5f, zr, xm - cm * 0.5f,
                       xm + cm * 0.5f, zm, yr, ym, 0.008f);
            wing_plate(fm.wing, s, xm - cm * 0.5f, xm + cm * 0.5f, zm, xt - ct * 0.5f,
                       xt + ct * 0.5f, zt, ym, yt, 0.008f);
        }
    }

    // Overall size: uniform scale, applied last (normals + baked shade are scale-invariant).
    if (p.scale != 1.0f) {
        MeshPart* parts[4] = {&fm.engine, &fm.wing, &fm.tail, &fm.body};
        for (MeshPart* mp : parts)
            for (float& v : mp->vertices) v *= p.scale;
    }
    return fm;
}

}  // namespace client
}  // namespace seads
