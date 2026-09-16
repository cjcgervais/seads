#include "render/air_field_glsl.h"

namespace render {

// The AirField uniform block (spec §1.1). Array size 4 == render::
// kMaxAirBubbles (air_field.h) — kept a literal here because GLSL uniform
// array declarations need a compile-time constant; test_air_field.cpp's
// source-validator leg pins the literal against the C++ constant so a future
// bump can't silently desync the two.
//
// Bundled in here too (spec §2's config keys, all render-tuning, no sim
// mirror to fork against): the optical-depth march controls (tau/march/steps)
// AND the haze/weather/aerial/mie-day dials — one new uniform block + one new
// setter (render::set_air_field_uniforms) called BESIDE set_atmosphere_
// uniforms, never widening its existing 16-arg signature (spec §1.1).
const char* const kAirFieldUniformsGLSL = R"(
uniform float uAirEnabled;
uniform float uAirDeckAglM;
uniform float uAirDeckSoftM;
uniform float uAirPlanetR;
uniform int uAirBubbleCount;
uniform vec3 uAirBubbleCenterDir[4];
uniform vec3 uAirBubbleMajorAxis[4];
uniform float uAirBubbleA[4];
uniform float uAirBubbleB[4];
uniform float uAirBubbleCeilingM[4];
uniform float uAirBubbleEdgeSoftM[4];
uniform float uAirBubbleCeilSoftM[4];
// S-domeround (docs/airdome_round_spec.md §1/§4): the superellipse
// roundness exponent, ONE value shared by every bubble (Chad's fly-dial).
// uAirBubbleCeilingM[] now CARRIES H (the volume-preserving dome centre
// height) rather than the literal config ceiling — folded into the
// existing uniform per the spec, no new per-bubble array.
uniform float uAirDomeExp;
uniform float uAirTauScaleM;
uniform float uAirMarchMaxM;
uniform int uAirStepsSky;
uniform int uAirStepsGround;
uniform float uAirHazeDensity;
uniform float uAirWeatherGain;
uniform float uAirAerialGain;
// The haze_overcast_density CEILING on the weather thickening term (quality
// fix pass item 7 — supersedes the loaded-but-orphaned key the initial
// S-airdome landing left behind; see render/air_field.h's field comment).
uniform float uAirHazeCeiling;
uniform vec3 uMieTintDay;
)";

// air_at/air_falloff: a line-by-line float transliteration of render/
// air_field.h (the spec's H1 fence #2 — the two are pinned side by side by
// test_air_field.cpp's structural source-validator leg; the numeric
// cross-check that CAN run with no GL context pins render::air_at itself
// against the live sim::atm_frac_at). air_optical_depth is the uniform-step
// trapezoid march (spec §1.2) — the mechanism that makes the dome VISIBLE
// rather than just a spatial gate at the eye. air_eye_pos/air_sky_max_dist
// are the two small helpers every consumer (sky/planet/star FS main()) needs
// to build the march's eyePos + maxDist.
const char* const kAirFieldGLSL = R"(
float air_falloff(float d, float soft) {
    if (d <= 0.0) return 1.0;
    if (d >= soft || soft <= 0.0) return 0.0;
    float t = d / soft;
    return 1.0 - t * t * (3.0 - 2.0 * t);
}
// The direction-dependent ellipse radius (sim/aero.h atm_frac_at's inline
// branch; render/air_field.h::air_ellipse_radius). `up`/`c`/`arc` are the
// caller's already-computed query direction / dot / arc distance (air_at
// below is the one caller, so no duplicate acos/dot here).
float air_ellipse_radius(vec3 centerDir, vec3 majorAxis, float a, float b,
                         vec3 up, float c, float arc) {
    if (b <= 0.0) return a;
    if (arc < 1e-6) return b;
    vec3 centerN = normalize(centerDir);
    vec3 m = majorAxis - centerN * dot(majorAxis, centerN);
    float mLen = length(m);
    if (mLen < 1e-9) return a;
    m /= mLen;
    vec3 t = up - centerN * c;
    float tLen = length(t);
    if (tLen < 1e-12) return b;
    t /= tLen;
    float cosTh = clamp(dot(t, m), -1.0, 1.0);
    float sinTh = sqrt(max(0.0, 1.0 - cosTh * cosTh));
    float denom = sqrt((b * cosTh) * (b * cosTh) + (a * sinTh) * (a * sinTh));
    return denom > 1e-9 ? (a * b / denom) : b;
}
// The spatial (deck UNION bubbles) air fraction at a world position `pos`
// (planet-center-relative, metres) — sim::atm_frac_at WITHOUT the altitude
// taper and WITHOUT the tunnel term (spec §1.1). uAirEnabled < 0.5 => 1.0
// everywhere (the null-env spatial factor is exactly 1 — full air, spatially
// uniform, never a phantom vacuum).
float air_at(vec3 pos) {
    if (uAirEnabled < 0.5) return 1.0;
    float r = length(pos);
    float alt = r - uAirPlanetR;
    float oneMinus = 1.0 - air_falloff(alt - uAirDeckAglM, uAirDeckSoftM);
    if (r > 0.0) {
        vec3 up = pos / r;
        for (int i = 0; i < 4; ++i) {
            if (i >= uAirBubbleCount) break;
            vec3 centerN = normalize(uAirBubbleCenterDir[i]);
            float c = clamp(dot(up, centerN), -1.0, 1.0);
            float arc = uAirPlanetR * acos(c);
            float radiusH = air_ellipse_radius(
                uAirBubbleCenterDir[i], uAirBubbleMajorAxis[i],
                uAirBubbleA[i], uAirBubbleB[i], up, c, arc);
            // S-domeround (docs/airdome_round_spec.md §1) -- the SAME
            // superellipse-of-revolution law as render/air_field.h::air_at
            // (this is a line-by-line float transliteration of it).
            float altP = max(alt, 0.0);
            float H = uAirBubbleCeilingM[i];
            float u = 0.0;
            if (radiusH > 0.0 && H > 0.0) {
                float rho = sqrt(arc * arc + altP * altP);
                if (rho <= 0.0) {
                    u = 1.0;
                } else {
                    float n = uAirDomeExp > 0.0 ? uAirDomeExp : 3.0;
                    float s = pow(pow(arc / radiusH, n) + pow(altP / H, n),
                                 1.0 / n);
                    float d = rho * (s - 1.0) / s;
                    float w = pow(altP / H, n) / pow(s, n);
                    float soft = uAirBubbleEdgeSoftM[i] * (1.0 - w) +
                                uAirBubbleCeilSoftM[i] * w;
                    u = air_falloff(d, soft);
                }
            }
            oneMinus *= (1.0 - u);
        }
    }
    return 1.0 - oneMinus;
}
// The eye's world position (planet-center-relative): every consuming FS
// declares uUp (normalize(eye)) and uEyeAlt (altitude) with these exact
// names before concatenating this block (sky.cpp / planet.cpp / star_glsl.h
// kStarFsDecls all do).
vec3 air_eye_pos() {
    return uUp * (uAirPlanetR + uEyeAlt);
}
// The sky-pass march distance (spec §1.2): the eye->planet-sphere hit along
// `dir` if the ray hits (an above-horizon ray never does — the planet mesh
// occludes below-horizon sky anyway), else uAirMarchMaxM.
float air_sky_max_dist(vec3 eyePos, vec3 dir) {
    float b = dot(eyePos, dir);
    float c = dot(eyePos, eyePos) - uAirPlanetR * uAirPlanetR;
    float disc = b * b - c;
    if (disc < 0.0) return uAirMarchMaxM;
    float t = -b - sqrt(disc);
    return (t > 0.0) ? t : uAirMarchMaxM;
}
// S-marchbound: ray/sphere entry-exit, and the conservative bracket of the
// segment that can contain air — a line-by-line float transliteration of
// render/air_field.h::air_sphere_interval / air_march_range (see that file
// for the bound's derivation and for why the stride, not the dither, was
// Chad's world-anchored boundary banding). t1 <= t0 means "no air on this
// ray".
vec2 air_sphere_interval(vec3 o, vec3 dir, vec3 c, float rad) {
    vec3 oc = o - c;
    float b = dot(oc, dir);
    float cc = dot(oc, oc) - rad * rad;
    float disc = b * b - cc;
    if (disc < 0.0) return vec2(1.0, -1.0);  // miss
    float sq = sqrt(disc);
    return vec2(-b - sq, -b + sq);
}
vec2 air_march_range(vec3 eyePos, vec3 dir, float maxDist) {
    if (uAirEnabled < 0.5) return vec2(0.0, maxDist);
    float t0 = 1e30;
    float t1 = -1e30;
    float rDeck = uAirPlanetR + uAirDeckAglM + uAirDeckSoftM;
    vec2 deck = air_sphere_interval(eyePos, dir, vec3(0.0), rDeck);
    if (deck.y > deck.x) {
        t0 = min(t0, deck.x);
        t1 = max(t1, deck.y);
    }
    for (int i = 0; i < 4; ++i) {
        if (i >= uAirBubbleCount) break;
        float softMax = max(uAirBubbleEdgeSoftM[i], uAirBubbleCeilSoftM[i]);
        float aMax = max(uAirBubbleA[i], uAirBubbleB[i]) + softMax;
        float hMax = uAirBubbleCeilingM[i] + softMax;
        if (aMax <= 0.0 || hMax <= 0.0) continue;
        float rad = 1.02 * sqrt(hMax * hMax +
                                aMax * aMax * (1.0 + hMax / uAirPlanetR));
        vec3 c = normalize(uAirBubbleCenterDir[i]) * uAirPlanetR;
        vec2 iv = air_sphere_interval(eyePos, dir, c, rad);
        if (iv.y > iv.x) {
            t0 = min(t0, iv.x);
            t1 = max(t1, iv.y);
        }
    }
    t0 = max(t0, 0.0);
    t1 = min(t1, maxDist);
    if (t1 <= t0) return vec2(0.0, -1.0);  // empty
    return vec2(t0, t1);
}
// A well-conditioned per-pixel hash (interleaved gradient noise). The former
// fract(sin(dot(fragCoord, ...)) * <big constant>) form loses float precision
// at large gl_FragCoord (its sine argument reaches ~1e5 at 1080p) and
// degenerates into structured diagonal streaks rather than noise — a real
// second-order defect on top of the stride, fixed here so the residual dither
// is honest noise. Small dot, no trig, no large-argument cancellation. The
// absence of the old form is pinned in test_air_field.cpp.
float air_ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}
// Uniform-step trapezoid march of air_at() along the ray (spec §1.2): the
// mechanism that makes the dome VISIBLE (a blue shell you can see from
// outside and fly into), not just a spatial gate sampled at the eye.
// Returns a dimensionless optical depth, uAirTauScaleM the reference path
// length at which unit air reads as one optical depth.
//
// S-marchbound: the `steps` are spent inside air_march_range's bracket, not
// across a blind maxDist, so the stride resolves the ~1.2 km soft edges
// instead of banding across them. An empty bracket returns EXACTLY 0 — a
// vacuum ray now short-circuits rather than marching 20 samples of nothing.
// The residual per-pixel phase jitter (one step, air_ign) stays: it converts
// whatever quantization survives the finer stride into fine noise, and it
// costs nothing.
float air_optical_depth(vec3 eyePos, vec3 dir, float maxDist, int steps) {
    vec2 range = air_march_range(eyePos, dir, maxDist);
    if (range.y <= range.x) return 0.0;
    int n = steps < 2 ? 2 : steps;
    float ds = (range.y - range.x) / float(n);
    float jitter = air_ign(gl_FragCoord.xy) * ds;
    float tau = 0.0;
    float fPrev = air_at(eyePos + dir * (range.x + jitter));
    for (int i = 1; i <= n; ++i) {
        vec3 p = eyePos + dir * (range.x + jitter + ds * float(i));
        float fCur = air_at(p);
        tau += 0.5 * (fPrev + fCur) * ds;
        fPrev = fCur;
    }
    return tau / uAirTauScaleM;
}
)";

}  // namespace render
