#include "render/post_glsl.h"

// See post_glsl.h for the design + the Fable-BEFORE ruling this implements.
// Uniform names are the validator allowlist (test_asset_validator) — keep them
// in sync. No uniform name may contain "time"/"clock" (the backdoor guard); the
// grain seed is uFrameCount, an app-owned cosmetic counter (app owns the fixed-
// dt accumulator; render/ never reads a clock).

namespace render {

const char* const kPostFS = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;    // scene color (RGBA16F, display-referred; alpha=transmittance reserved)
uniform sampler2D uDepth;      // scene depth (DEPTH24 texture) — sampled only by the debug viz
uniform vec2  uResolution;     // framebuffer size (px)
uniform float uContrast;       // display sigmoid steepness c
uniform float uLift;           // black lift on input
uniform float uGrain;          // grain amount k_g
uniform vec3  uSplitShadow;    // cool silver shadow tint (luminance multiplier)
uniform vec3  uSplitHi;        // warm silver highlight tint (luminance multiplier)
uniform float uSatC0;          // split-tone chroma-gate deadband
uniform float uSatC1;          // split-tone chroma-gate knee
uniform float uSatDark;        // near-black chroma floor (m_dark)
uniform float uHalation;       // warm bloom strength
uniform float uHalThresh;      // bloom luma threshold
uniform vec3  uHalTint;        // warm halation tint
uniform float uVignette;       // corner exposure falloff
uniform float uFxaa;           // FXAA blend amount [0,1]
uniform int   uFrameCount;     // app-owned COSMETIC grain seed (not a feel/aim clock)
uniform int   uDebugMode;      // 0 normal · 1 linearized-depth viz · 2 sat mask · 3 DoF CoC viz
uniform float uNear;           // clip near (depth linearization; single-sourced from rlSetClipPlanes)
uniform float uFar;            // clip far
uniform float uFocusStart;     // S6 DoF: crisp out to this view-space distance (m)
uniform float uFocusEnd;       // S6 DoF: full blur reached at this distance (m)
uniform float uDofRadius;      // S6 DoF: blur disc radius at full CoC (screen px); 0 = off
uniform float uSkyCoc;         // S6 DoF: far-sky CoC cap (protect the space-first starfield)
uniform int   uHalftoneMode;   // S6 print: 0 off (default) · 1 AM halftone dots · 2 ordered dither
uniform float uHalftoneScale;  // S6 print: dot-cell size (screen px); loader floors >= 2 (NaN/subsample guard)
uniform float uHalftoneAngle;  // S6 print: screen-grid rotation (rad); mode 2 ignores it (pixel-locked Bayer)
uniform float uHalftoneSoft;   // S6 print: mode1 = px AA-width multiplier (floored 0.5); mode2 = tonal crossfade
uniform float uHalftoneInk;    // S6 print: ink darkness level (× uSplitShadow -> cool near-black ink)
uniform float uHalftoneGrainMul; // S6 print: grain scale on MONO print pixels (planes keep full grain)

const vec3 LUMA = vec3(0.299, 0.587, 0.114);
float luma(vec3 c) { return dot(c, LUMA); }

// Compact FXAA (Timothy Lottes, reduced console variant). Valid at head
// placement because the scene RT is already display-referred/perceptual
// (Fable Q3). uFxaa blends AA<-original so 0 = passthrough.
vec3 applyFxaa(vec2 uv, vec2 rcp, float amount) {
    vec3 rgbM  = texture(texture0, uv).rgb;
    if (amount <= 0.0) return rgbM;
    vec3 rgbNW = texture(texture0, uv + vec2(-1.0, -1.0) * rcp).rgb;
    vec3 rgbNE = texture(texture0, uv + vec2( 1.0, -1.0) * rcp).rgb;
    vec3 rgbSW = texture(texture0, uv + vec2(-1.0,  1.0) * rcp).rgb;
    vec3 rgbSE = texture(texture0, uv + vec2( 1.0,  1.0) * rcp).rgb;
    float lNW = luma(rgbNW), lNE = luma(rgbNE);
    float lSW = luma(rgbSW), lSE = luma(rgbSE), lM = luma(rgbM);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    vec2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));
    float reduce = max((lNW + lNE + lSW + lSE) * 0.25 * (1.0 / 8.0), 1.0 / 128.0);
    float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpMin, vec2(-8.0), vec2(8.0)) * rcp;
    vec3 rgbA = 0.5 * (texture(texture0, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(texture0, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(texture0, uv + dir * -0.5).rgb +
                                     texture(texture0, uv + dir *  0.5).rgb);
    float lB = luma(rgbB);
    vec3 aa = (lB < lMin || lB > lMax) ? rgbA : rgbB;
    return mix(rgbM, aa, amount);
}

// Display-space per-channel contrast sigmoid (Fable Q1: NO ACES — the scene is
// already display-referred; a scene-linear operator would double-tone AND
// drain plane chroma). s(x)=x^c/(x^c+(1-x)^c); c=1 -> identity.
float scurve1(float x, float c) {
    x = clamp(x, 0.0, 1.0);
    float a = pow(x, c);
    float b = pow(1.0 - x, c);
    return a / (a + b + 1e-6);
}
vec3 scurve(vec3 v) {
    v = uLift + (1.0 - uLift) * v;   // black lift on input
    return vec3(scurve1(v.r, uContrast),
                scurve1(v.g, uContrast),
                scurve1(v.b, uContrast));
}

// Hash for the grain (any cheap hash suffices at k_g~0.02). Frame-seeded so the
// pattern moves (static grain reads as a dirty monitor — Fable Q6).
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// S6 "printed card": the canonical 4x4 dispersed-dot ordered-dither (Bayer)
// index matrix. A DEFINITIONAL math object (the recursive index matrix), same
// class as the golden angle — structural, not a look dial (Fable-BEFORE Q7).
// Thresholds are (k+0.5)/16 so a pure-white (Yh=1) / pure-black (Yh=0) pixel
// quantizes cleanly to paper/ink. p is the pixel coord & 3 (0..3).
const int BAYER[16] = int[16](0,  8,  2, 10,
                              12, 4, 14,  6,
                              3, 11,  1,  9,
                              15, 7, 13,  5);
float bayer4x4(ivec2 p) {
    return (float(BAYER[(p.y & 3) * 4 + (p.x & 3)]) + 0.5) / 16.0;
}

// View-space depth (m) from a non-reversed 24-bit depth sample. d=0 -> uNear,
// d=1 -> uFar exactly (denom = 2*uNear); single-sourced by the depth viz AND the
// S6 DoF CoC so the two can't fork (Fable-BEFORE §1).
float linearZ(float d) {
    return (2.0 * uNear * uFar) /
           (uFar + uNear - (2.0 * d - 1.0) * (uFar - uNear));
}

// S6 far-field-only circle of confusion, normalized [0,1]. Near/mid (zv below
// uFocusStart) -> 0 = crisp (everything the pilot flies/fights through). The far
// sky (cleared depth d==1.0, the sky pass writes none) is capped at uSkyCoc so
// the flown space-first starfield isn't blurred away (Fable-BEFORE P1-b).
float cocAt(vec2 p) {
    float d = textureLod(uDepth, p, 0.0).r;
    float c = smoothstep(uFocusStart, uFocusEnd, linearZ(d));
    if (d >= 1.0) c = min(c, uSkyCoc);
    return c;
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 rcp = 1.0 / uResolution;

    // --- debug: prove the depth-texture FBO is alive in S1 (DoF proper = S6).
    // Placed ABOVE FXAA so the viz costs no wasted neighbor taps. ---
    if (uDebugMode == 1) {
        float g = clamp(linearZ(texture(uDepth, uv).r) / uFar, 0.0, 1.0);
        finalColor = vec4(vec3(g), 1.0);
        return;
    }

    // --- FXAA (pass 0; display-referred RT) ---
    vec3 aa = applyFxaa(uv, rcp, uFxaa);

    // --- S6 far-field-only DEPTH OF FIELD (Fable-BEFORE vetted) ---
    // Blur ONLY the distant background/limb; near/mid (coc==0) stays FXAA-sharp
    // so combat readability is untouched (Chad: far-only). Single-pass golden-
    // angle gather on the display-referred scene color (same space FXAA averages).
    // Each tap is weighted by min(tapCoc, coc): the tapCoc factor stops a sharp
    // NEAR neighbour bleeding into a blurred FAR pixel; the coc cap stops the far
    // sky OVER-weighting a mid-distance ridgeline into a bright fringe (P1-a).
    vec3 scene = aa;
    float coc = cocAt(uv);
    if (uDofRadius > 0.0 && coc > 0.0) {
        float r = coc * uDofRadius;
        // per-pixel spiral rotation (interleaved gradient noise) so a raised
        // radius dial scatters point-highlight ghosting into grain-masked noise
        // instead of a fixed 16-dot stamp (P2-c).
        float ign = fract(52.9829189 * fract(dot(gl_FragCoord.xy,
                                                 vec2(0.06711056, 0.00583715))));
        float rot = ign * 6.2831853;
        // seed the accumulator with the CENTER tap at its own weight (min(coc,coc)
        // = coc per P1-a), using the FXAA'd aa: a low-survivor disc (a far sliver
        // seen through canopy/struts) then degrades CONTINUOUSLY to the sharp
        // fallback instead of copying one random neighbour = speckle (Fable-AFTER).
        vec3 acc = aa * coc;
        float wsum = coc;
        const int N = 16;
        for (int i = 0; i < N; ++i) {
            float t = (float(i) + 0.5) / float(N);
            float ang = float(i) * 2.399963267 + rot;   // golden angle + jitter
            vec2 off = vec2(cos(ang), sin(ang)) * sqrt(t) * r * rcp;
            float w = min(cocAt(uv + off), coc);
            acc += textureLod(texture0, uv + off, 0.0).rgb * w;
            wsum += w;
        }
        // all taps rejected (sky through a gap in the airframe) -> the FXAA'd
        // sharp center, never raw/AA-less (P2-b). textureLod everywhere: the
        // branch is non-uniform flow, implicit-LOD derivatives are UB (P2-a).
        vec3 blurred = (wsum > 1e-4) ? acc / wsum : aa;
        scene = mix(aa, blurred, smoothstep(0.0, 0.15, coc));
    }
    if (uDebugMode == 3) { finalColor = vec4(vec3(coc), 1.0); return; }

    // --- display-space contrast sigmoid ---
    vec3 toned = scurve(scene);

    // --- silver split-tone, saturation-GATED (Fable Q5) ---
    // sat measured on the PRE-split-tone (post-S-curve) rgb; relative chroma so a
    // dark saturated plane still passes; m_dark floor kills the near-black halo.
    float mx = max(toned.r, max(toned.g, toned.b));
    float mn = min(toned.r, min(toned.g, toned.b));
    float C = mx - mn;
    float sat = smoothstep(uSatC0, uSatC1, C / max(mx, uSatDark));
    if (uDebugMode == 2) { finalColor = vec4(vec3(sat), 1.0); return; }
    float Y = clamp(luma(toned), 0.0, 1.0);
    vec3 tint = mix(uSplitShadow, uSplitHi, Y);   // near-neutral -> silver, not sepia
    // clamp so a >1 highlight tint can't drive col>1 and flip the halation
    // screen blend's (1-col) negative (Fable-AFTER P2-1)
    vec3 silver = clamp(Y * tint, 0.0, 1.0);
    vec3 col = mix(silver, toned, sat);           // mono->silver, planes->color

    // --- warm halation off bright glints (screen blend; Fable Q2) ---
    if (uHalation > 0.0) {
        const int N = 8;
        vec2 offs[8] = vec2[8](
            vec2( 1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0,  1.0), vec2(0.0, -1.0),
            vec2( 0.7, 0.7), vec2(-0.7, 0.7), vec2(0.7, -0.7), vec2(-0.7, -0.7));
        float radius = 4.0;   // px
        float h = 0.0;
        for (int i = 0; i < N; ++i) {
            // re-apply the cheap S-curve per tap (Fable's fused single-pass note)
            vec3 s = scurve(texture(texture0, uv + offs[i] * radius * rcp).rgb);
            h += max(luma(s) - uHalThresh, 0.0);
        }
        h *= uHalation / float(N);
        col = col + uHalTint * h * (1.0 - col);
    }

    // --- vignette (before grain so grain amplitude is uniform; Fable P2) ---
    vec2 dd = uv - 0.5;
    float vig = 1.0 - uVignette * dot(dd, dd) * 3.0;
    col *= clamp(vig, 0.0, 1.0);

    // --- S6 "printed card" halftone / ordered-dither MODE (OFF by default;
    // uHalftoneMode==0 -> this whole block is skipped, bit-exact identity). The
    // mono silver world becomes a SCREEN-LOCKED print screen (the paper is fixed,
    // the world scrolls under it — pure fn of gl_FragCoord, no clock, cosmetic);
    // the aircraft (chroma) pass through untouched via the same sat gate as the
    // split-tone. Fable-BEFORE SOUND-WITH-FIXES: area-linear tone, analytic AA. ---
    if (uHalftoneMode > 0) {
        float Yh = clamp(luma(col), 0.0, 1.0);   // local silver luminance
        float ink;                               // 1 = ink (dark), 0 = paper
        if (uHalftoneMode == 1) {
            // rotated screen lattice (R(+angle), column-major); mode-2 is pixel-
            // locked so it ignores scale/angle.
            float ca = cos(uHalftoneAngle), sa = sin(uHalftoneAngle);
            vec2 sp = mat2(ca, sa, -sa, ca) * (gl_FragCoord.xy / uHalftoneScale);
            float d = length(fract(sp) - 0.5) * 1.41421356;   // 0 center .. 1 corner
            // sp is affine w/ an orthonormal rotation, so |grad_px d| = sqrt2/scale
            // EXACTLY everywhere — analytic ~1px AA, no fwidth (its cell-seam spike
            // etches a grey grid) and no smoothstep(a,a,x) UB at soft=0 (P1 no.2/no.4).
            float aa = (1.41421356 / uHalftoneScale) * max(uHalftoneSoft, 0.5);
            // sqrt() makes dot AREA ~linear in darkness; the [-aa,1+aa] padding
            // lets a dot fully OPEN at Yh=1 and fully CLOSE at Yh=0 (P1 no.1).
            float T = mix(-aa, 1.0 + aa, sqrt(clamp(1.0 - Yh, 0.0, 1.0)));
            ink = 1.0 - smoothstep(T - aa, T + aa, d);
        } else {
            // ordered 4x4 Bayer: ink where the tone is below the cell threshold.
            // soft is a purely TONAL crossfade here (no spatial edge), SCALED to the
            // Bayer quantum (1/16): soft=1 -> +/-1/32 = half a step = crisp dither
            // with exact white/black endpoints; raw luma units would span 16 steps
            // and collapse the dither to a flat grey ramp (Fable-AFTER P1). Floored
            // off the smoothstep(a,a,x) UB.
            float bt = bayer4x4(ivec2(gl_FragCoord.xy));
            float s = max(uHalftoneSoft, 1e-2) * 0.03125;
            ink = 1.0 - smoothstep(bt - s, bt + s, Yh);
        }
        // paper = the warm silver highlight RATIO (normalize, don't clamp — a clamp
        // would collapse R==G and yellow the paper; P1 no.3). ink = cool near-black.
        vec3 paper = uSplitHi / max(max(uSplitHi.r, uSplitHi.g),
                                    max(uSplitHi.b, 1e-4));
        vec3 inkc = clamp(uHalftoneInk * uSplitShadow, 0.0, 1.0);
        col = mix(mix(paper, inkc, ink), col, sat);   // mono -> print, planes -> untouched
    }

    // --- film grain (terminal op = the single 8-bit-quantize dither; luminance-
    // only equal-channel add preserves the planes-only-chroma law; Fable Q6) ---
    // wrap the frame seed so the hash input can't drift into float-ulp
    // quantization on a multi-hour session (Fable-AFTER P2-3)
    float gr = hash12(gl_FragCoord.xy + float(uFrameCount % 1024) * 1.7) * 2.0 - 1.0;
    float Yc = clamp(luma(col), 0.0, 1.0);
    float w = 0.15 + 0.85 * 4.0 * Yc * (1.0 - Yc);   // silver grain lives in the mids
    // Under the print MODE the mono paper/ink is bi-level with nothing to de-band,
    // so full grain reads as speckle boiling on flat ink — attenuate it on the
    // print, but keep FULL grain on the smooth-shaded planes (sat gate; P1 no.5).
    float gmul = (uHalftoneMode > 0) ? mix(uHalftoneGrainMul, 1.0, sat) : 1.0;
    col += uGrain * gmul * gr * w;

    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)GLSL";

}  // namespace render
