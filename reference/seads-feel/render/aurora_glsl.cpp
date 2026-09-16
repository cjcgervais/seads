#include "render/aurora_glsl.h"

namespace render {

// Gemini-generated (recipe tools/gemini/recipes/aurora_glsl.txt, reference-
// grounded to real auroral BANDS — Alaska Geophysical Institute: serpentine arcs
// with wavy borders that snake, fold + drift, NOT a static ring/bloom), reviewed:
// declares no uniform, no clock, no atan; exactly 2*pi-periodic + az-seam-free via
// INTEGER-harmonic angle-addition on phaseSC. TWO parallel bands whose centers
// MEANDER with az and DRIFT with phase (waves propagate along the arc) + sharpened
// vertical rays + green-base/red-top color. The sky-FS wrapper supplies the
// ray-shell geometry (rho/az) + the LAYER limb-brightening + the gates.
const char* const kAuroraGLSL = R"(
vec3 aurora_curtain(float rho, float az, vec2 phaseSC, float ovalCenter,
                    float ovalWidth, float curtainScale, vec3 tintLow, vec3 tintHigh)
{
    // 1. SNAKING FOLD: a serpentine meander of the band center that DRIFTS along az
    // (low INTEGER harmonics via angle-addition => big lazy folds that travel).
    float g2 = sin(2.0*az)*phaseSC.y + cos(2.0*az)*phaseSC.x;
    float g3 = sin(3.0*az)*phaseSC.y + cos(3.0*az)*phaseSC.x;
    float g5 = sin(5.0*az)*phaseSC.y + cos(5.0*az)*phaseSC.x;
    float fold = ovalWidth * (1.1*g2 + 0.6*g3 + 0.3*g5);

    // 2. TWO parallel bands, each snaking on its own; d<0 = bright green BASE,
    // d>0 = soft red TOPS. Accumulate color weighted by each band's envelope.
    float acc = 0.0; vec3 colAcc = vec3(0.0);
    // band 0
    float d0 = (rho - (ovalCenter + fold)) / ovalWidth;
    float env0 = (d0 < 0.0) ? exp(-pow(max(-d0,0.0)*1.6,2.0)) : exp(-max(d0,0.0)*0.8);
    float h0 = clamp(0.5 + 0.4*d0, 0.0, 1.0);
    colAcc += mix(tintLow, tintHigh, h0*h0) * env0; acc += env0;
    // band 1 (offset + an independently-shifted fold)
    float d1 = (rho - (ovalCenter + ovalWidth*1.6 + (0.8*fold - ovalWidth*0.5*g3))) / ovalWidth;
    float env1 = (d1 < 0.0) ? exp(-pow(max(-d1,0.0)*1.6,2.0)) : exp(-max(d1,0.0)*0.8);
    float h1 = clamp(0.5 + 0.4*d1, 0.0, 1.0);
    colAcc += mix(tintLow, tintHigh, h1*h1) * env1; acc += env1;

    // 3. VERTICAL RAYS: sharp thin striations with dark gaps, drifting along az.
    float k0 = max(floor(curtainScale + 0.5), 1.0);
    float w1 = sin(k0*az)*phaseSC.y + cos(k0*az)*phaseSC.x;
    float w2 = sin((2.0*k0+1.0)*az)*phaseSC.y + cos((2.0*k0+1.0)*az)*phaseSC.x;
    float w3 = sin((4.0*k0-1.0)*az)*phaseSC.y + cos((4.0*k0-1.0)*az)*phaseSC.x;
    float rays = pow(clamp(0.5 + 0.5*(0.6*w1+0.3*w2+0.2*w3), 0.0, 1.0), 3.0);
    float grain = 0.85 + 0.15*fract(sin(113.0*az)*437.5854);  // static, no phi (sin(113*pi)=0, seam-safe)

    // 4. COMBINE: the band-averaged color modulated by the ray striations.
    vec3 col = (acc > 1e-4) ? colAcc/acc : vec3(0.0);
    return col * (acc * rays * grain);
}
)";

}  // namespace render
