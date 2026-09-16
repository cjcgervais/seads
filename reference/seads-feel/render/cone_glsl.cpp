#include "cone_glsl.h"

// Uniforms. Deliberately few: every extra dial is another place the look can
// fork from the doc. uConeDetail == 0 is an exact bypass (planet.cpp skips the
// whole block), so the feature ships OFF-able with no shader recompile.
const char* kConeUniformsGLSL =
    "uniform samplerCube coneCube;\n"    // r = shock-fabric mask 0..1
    "uniform float uConeDetail;\n"       // master strength; 0 = off
    "uniform float uConeScale;\n"        // lattice cells per metre (1/cell size)
    "uniform float uConeFadeNear;\n"     // full detail closer than this (m)
    "uniform float uConeFadeFar;\n"      // gone past this (m)
    "uniform float uConeSlopeLo;\n"      // slope below which no cones (0=flat,1=vertical)
    "uniform vec3  uConeAxis;\n"         // planet-local unit dir toward the basin centre
    "uniform float uHasFabric;\n"        // 1 when the shock-fabric cubemap loaded
    "uniform float uBarrenSnowShed;\n"   // how much snow the barrens shed [0,1]
    // ★ The shed RESPONSE curve, single-sourced from world::SnowpackParams so
    // the shader and world/snowpack.cpp cannot drift (INV-9). `barren` is a
    // COVERAGE field, not a 0/1 mask, so a linear shed left the mapped cores
    // grey -- Chad's "reads too light", 2026-08-11.
    "uniform float uBarrenShedLo;\n"     // below this, ground keeps its snow
    "uniform float uBarrenShedHi;\n"     // above this, full shed (bare rock)
    // ★ THE SLOPE GATE (Chad's fly ruling, 2026-08-11): flat barrens hold
    // snow, sloped faces are the blackest. Mirrors
    // world::SnowpackField::barren_slope_gate from the SAME constants
    // (INV-9); runs on 1-cosSlope so both consumers can produce it exactly.
    "uniform float uBarrenSlopeXLo;\n"   // 1-cos(barren_slope_lo_deg)
    "uniform float uBarrenSlopeXHi;\n"   // 1-cos(barren_slope_hi_deg)
    "uniform float uBarrenFlatShed;\n"   // gate FLOOR on dead-flat ground
    // ★ Fly 3 (2026-08-11): the macro normal is a mip-smoothed normalCube
    // sample (kills the triangle-facet transition the mesh normal had), and
    // the steepest barren faces darken the albedo itself past full shed.
    "uniform float uBarrenNormalLod;\n"   // mip level ~= a 90 m footprint
    "uniform float uBarrenFaceDark;\n"    // extra albedo kill at full ramp
    "uniform float uBarrenFaceDarkXHi;\n"   // 1-cos(face_dark_hi_deg)
    // ★ Fly 5: the baked mottle's bright patches resist face-dark, so the
    // steepest faces stay MOTTLED instead of crushing to uniform black.
    "uniform float uBarrenFaceMottle;\n";  // bright-mottle resistance [0,1]

const char* kConeGLSL =
    "vec2 c_hash22(vec2 p){\n"
    "    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));\n"
    "    return fract(sin(p) * 43758.5453);\n"
    "}\n"
    // One cone per lattice cell, evaluated analytically. Returns the 2D height
    // GRADIENT; the height itself is never needed.
    //   h(r,th) = -tanHalf*r + A(r)*sin(N(r)*th + ph)
    // with A(r) a bump that vanishes at the apex (striae converge to a point)
    // and at the nappe edge, and N(r) rising with r (the horsetail fork).
    "vec2 cone_grad(vec2 uv, float ori){\n"
    "    vec2 cell = floor(uv);\n"
    "    vec2 h2 = c_hash22(cell);\n"
    "    vec2 apex = cell + 0.5 + (h2 - 0.5) * 0.55;\n"
    "    vec2 d = uv - apex;\n"
    "    float r = length(d);\n"
    "    if (r > 0.70 || r < 1e-4) return vec2(0.0);\n"   // outside this nappe
    "    float th = atan(d.y, d.x);\n"
    // Apices point up-range, toward the basin: the striation phase is measured
    // from the projected basin direction, not from an arbitrary axis, so the
    // fans across a face agree instead of scattering. h2.x keeps them from
    // being identical.
    "    float ph = ori + h2.x * 1.2;\n"
    // Sudbury apical angle 75-90 deg => half-angle ~38-45 deg; jitter +/-15 deg
    // about that, per the field range.
    "    float tanHalf = 0.84 + 0.34 * h2.y;\n"
    "    float N = 9.0 + 26.0 * r;\n"                     // striae fork outward
    "    float dN = 26.0;\n"
    "    float e = (r - 0.24) / 0.19;\n"
    "    float A = 0.115 * exp(-e * e);\n"                // vanishes at apex + rim
    "    float dA = A * (-2.0 * e / 0.19);\n"
    "    float arg = N * th + ph;\n"
    "    float s = sin(arg), c = cos(arg);\n"
    "    float dh_dr = -tanHalf + dA * s + A * c * dN * th;\n"
    "    float dh_dth = A * c * N;\n"
    "    float ct = cos(th), st = sin(th);\n"
    "    return vec2(dh_dr * ct - dh_dth * st / r,\n"
    "                dh_dr * st + dh_dth * ct / r);\n"
    "}\n"
    // Triplanar assembly -> a planet-local gradient, projected into the base
    // surface's tangent plane (surface-gradient blend) -> perturbed normal.
    "vec3 cone_detail_normal(vec3 p, vec3 n, float w){\n"
    "    vec3 bl = abs(n); bl = bl * bl; bl = bl * bl;\n"  // pow(|n|,4)
    "    bl /= max(bl.x + bl.y + bl.z, 1e-5);\n"
    "    float sc = uConeScale;\n"
    // Per-plane orientation reference: the basin direction projected into that
    // plane, as an angle. Keeps the fans coherent across a face.
    "    float oX = atan(uConeAxis.z, uConeAxis.y);\n"
    "    float oY = atan(uConeAxis.x, uConeAxis.z);\n"
    "    float oZ = atan(uConeAxis.y, uConeAxis.x);\n"
    // TWO OCTAVES. Real shatter cones are a self-similar hierarchy: small
    // "parasitic" cones nest on the flanks of larger ones. One octave is enough
    // when the nearest viewer is a cockpit; it is not enough now the whole globe
    // is ridable and a snowmachine passes rock at 5-20 m, where a single 2.5 m
    // lattice reads as a few big lumps. The second octave at 1/4 the cell (~0.6 m)
    // puts real structure at arm's length, and its weight is halved so the
    // metre-scale forms still govern the silhouette.
    "    float s2 = sc * 4.0, w2 = 0.5;\n"
    "    vec2 gx = cone_grad(p.yz * sc, oX) + w2 * cone_grad(p.yz * s2, oX);\n"
    "    vec2 gy = cone_grad(p.zx * sc, oY) + w2 * cone_grad(p.zx * s2, oY);\n"
    "    vec2 gz = cone_grad(p.xy * sc, oZ) + w2 * cone_grad(p.xy * s2, oZ);\n"
    "    vec3 g = vec3(bl.y * gy.y + bl.z * gz.x,\n"
    "                  bl.x * gx.x + bl.z * gz.y,\n"
    "                  bl.x * gx.y + bl.y * gy.x);\n"
    "    g -= n * dot(g, n);\n"                            // tangent-plane part
    "    return normalize(n - w * g);\n"
    "}\n"
    // Scalar HEIGHT for one plane's lattice -- the same h(r,th) that
    // cone_grad differentiates (see the file-header formula), returned
    // directly instead of as a gradient. Textually mirrors cone_grad's setup
    // (hash, apex, r, th, ph, tanHalf, N, A) so the two are provably the SAME
    // lattice/striation math; cone_grad itself is untouched (its output is
    // pinned) rather than refactored to share this, so there is zero risk of
    // perturbing the normal path.
    "float cone_h(vec2 uv, float ori){\n"
    "    vec2 cell = floor(uv);\n"
    "    vec2 h2 = c_hash22(cell);\n"
    "    vec2 apex = cell + 0.5 + (h2 - 0.5) * 0.55;\n"
    "    vec2 d = uv - apex;\n"
    "    float r = length(d);\n"
    "    if (r > 0.70 || r < 1e-4) return 0.0;\n"   // outside this nappe: no
                                                     // relief (matches
                                                     // cone_grad's vec2(0.0))
    "    float th = atan(d.y, d.x);\n"
    "    float ph = ori + h2.x * 1.2;\n"
    "    float tanHalf = 0.84 + 0.34 * h2.y;\n"
    "    float N = 9.0 + 26.0 * r;\n"
    "    float e = (r - 0.24) / 0.19;\n"
    "    float A = 0.115 * exp(-e * e);\n"
    "    return -tanHalf * r + A * sin(N * th + ph);\n"
    "}\n"
    // cone_value: the scalar cone-relief HEIGHT in [0,1] -- the VALUE
    // channel that makes the shatter-cone lattice READ on near-black barren
    // albedo (cone_detail_normal only perturbs the NORMAL, which is
    // invisible at ~0.02 albedo). Same triplanar assembly, hash, and
    // two-octave lattice as cone_detail_normal (SAME sc/s2/w2, SAME |n|^4
    // blend weights, SAME per-plane basin-direction orientation), but blends
    // cone_h (a scalar) instead of cone_grad (a 2D gradient).
    //
    // CONVENTION: 0.5 == no relief (neutral). h is <= 0 everywhere inside a
    // nappe (it falls away from the apex, where h == 0) and is exactly 0.0
    // outside every nappe (cone_h's own sentinel) -- so "no cone here" and
    // "standing at an apex" are the same h and land on the same 0.5 baseline
    // by construction, and the striations (the sin term riding the taper)
    // read as symmetric ridges/grooves around it once clamped.
    "float cone_value(vec3 p, vec3 n){\n"
    "    vec3 bl = abs(n); bl = bl * bl; bl = bl * bl;\n"  // pow(|n|,4)
    "    bl /= max(bl.x + bl.y + bl.z, 1e-5);\n"
    "    float sc = uConeScale;\n"
    "    float oX = atan(uConeAxis.z, uConeAxis.y);\n"
    "    float oY = atan(uConeAxis.x, uConeAxis.z);\n"
    "    float oZ = atan(uConeAxis.y, uConeAxis.x);\n"
    "    float s2 = sc * 4.0, w2 = 0.5;\n"
    "    float hx = cone_h(p.yz * sc, oX) + w2 * cone_h(p.yz * s2, oX);\n"
    "    float hy = cone_h(p.zx * sc, oY) + w2 * cone_h(p.zx * s2, oY);\n"
    "    float hz = cone_h(p.xy * sc, oZ) + w2 * cone_h(p.xy * s2, oZ);\n"
    "    float h = bl.x * hx + bl.y * hy + bl.z * hz;\n"
    // Normalize by the worst-case combined depth: single-octave |h| peaks at
    // the nappe rim (r->0.70, sin term negligible there) at tanHalf_max*0.70
    // = 1.18*0.70 ~= 0.826; the second octave adds up to w2 * that same
    // bound = ~0.413; combined worst case ~= 1.24. h==0 (apex or no-relief)
    // maps to exactly 0.5; clamp absorbs the rare deeper-than-typical tail.
    "    const float kDepth = 1.24;\n"
    "    return clamp(0.5 + h / kDepth, 0.0, 1.0);\n"
    "}\n";
