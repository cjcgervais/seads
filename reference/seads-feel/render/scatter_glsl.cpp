#include "render/scatter_glsl.h"

namespace render {

// Atmospheric scatter (S-airdome rewrite, spec §1.4 — supersedes the Stage-3
// rim-only "space-first" version below). Chad's 2026-08-09 ruling relaxes
// space-first INSIDE a bubble: the zenith is now blue and daytime Mie forward-
// scatter ("some white scatter") is visible, not just a sunset blaze. The
// WHOLE result is x airAmt (the caller's air_optical_depth amount, spec §1.2)
// so vacuum stays EXACTLY vec3(0) BY CONSTRUCTION — airAmt appears nowhere
// else in this function except the per-channel Rayleigh saturation (which is
// ALSO exactly 0 at tau==0), so the single final multiply is a redundant-but-
// deliberate safety net, not the only thing keeping vacuum dark. viewDir/
// sunDir world unit; sunDir = light-travel (sun->scene), so -sunDir is the
// dir TO the sun. up = normalize(eye) — the sphere-seam local up, never a
// fixed axis. sunElevCos = dot(-sunDir, up): +1 zenith, 0 horizon, <0 below.
// Declares NO uniform — references the [atmosphere]/[air field] blocks
// (kSkyUniformsGLSL + kAirFieldUniformsGLSL, both concatenated before this).
//
// Quality fix pass item 4 (docs/airdome_report.md): `tau` (the raw optical
// depth), not just the saturated `airAmt`, is now threaded in — the uniform
// `airAmt`-only Rayleigh form flattened every long chord (a 30+ km horizon
// path) to the SAME saturated value as a moderate one, reading as a
// featureless grey/cream wash rather than blue. See kScatterGLSL's per-
// channel comment below for the fix.
const char* const kScatterGLSL = R"(
vec3 scatter(vec3 viewDir, vec3 sunDir, vec3 up, float sunElevCos,
             float tau) {
    float airAmt = 1.0 - exp(-tau);
    // Rayleigh: sky-blue via the classic (1+cos^2 theta) phase SHAPE about the
    // sun (theta = angle between the view ray and the sun ray) — NOT
    // rim-weighted (Chad's ruling: the zenith inside a dome IS blue, killing
    // the old space-first horizonW term). The zenith->horizon density
    // gradient that term used to supply now comes STRUCTURALLY from tau
    // itself (the optical-depth march is longer along horizon-grazing
    // chords) — no hand-authored gradient re-added here. Day-only (rayW),
    // zero below the horizon so a bubble at night stays dark and
    // starry-through-thin-air, never blue.
    //
    // PER-CHANNEL saturating in-scatter (quality fix pass item 4): each color
    // channel saturates toward u_rayleigh's OWN hue at its OWN rate (kChanW
    // weights blue highest) instead of one shared `airAmt` scalar tinted
    // uniformly — a long/thick chord now reddens/whitens out TOWARD the
    // configured blue tint rather than blowing past it to flat grey/cream
    // (the failure a uniform-airAmt tint produces once tau gets large: every
    // channel saturates at the SAME rate, so the color ratio — the hue —
    // never shifts, but the ABSOLUTE lift keeps climbing against whatever
    // else is in the sum, reading as washed-out). At tau -> 0 this is exactly
    // 0 by construction (no separate gate needed); at tau -> inf it saturates
    // at u_rayleigh itself, never washing to white. Deliberately
    // un-normalized (not the physical 3/(16pi) prefactor) for "easily
    // perceptible... beautiful blue" (Chad's words, spec §0); the model is
    // already cheap single-scatter, not physical (house convention).
    // S-sungate (Chad's fly, 2026-08-09: "mie scatter only occurs after the
    // sun is much higher above the horizon... about 1/4 up its ascension").
    // ATTRIBUTION: NOT an altitude/ground reference (up == normalize(eye)
    // already, and at 200 m the horizon dip is 0.45 deg — invisible). The
    // cause was THIS gate's edges. sunElevCos is the SINE of sun elevation,
    // so the old smoothstep(-0.05, 0.30, .) only reached full at
    // asin(0.30) = 17.5 deg and sat at 0.057 AT the horizon — measured
    // against his "about 1/4 up" (~22 deg), the dial, not the eye height.
    //
    // Second defect the same edges caused: rayW also multiplies the Mie
    // term, whose duskBoost peaks BELOW 1.7 deg elevation — so the dusk
    // blaze was being crushed to ~5% exactly at dusk. Both close together.
    //
    // The fix single-sources the "is the sun up" question onto the SAME sun-
    // elevation window the sky's own day<->dusk<->night blend already uses
    // (uDuskLo/uDuskHi, config [atmosphere] dusk_lo_rad/dusk_hi_rad, declared
    // in the shared kSkyUniformsGLSL block every consumer concatenates), so
    // scatter and the sky can no longer disagree about when the sun is up —
    // rather than adding a third independent pair of edges to keep in sync.
    // Ships -0.14..+0.10 rad: full scatter from 5.7 deg up, 0.60 AT the
    // horizon (was 0.057), zero by 8 deg below. Fly-dials: dusk_lo_rad /
    // dusk_hi_rad.
    float sunElevRad = asin(clamp(sunElevCos, -1.0, 1.0));
    float rayW = smoothstep(uDuskLo, uDuskHi, sunElevRad);
    float ctSun = clamp(dot(viewDir, -sunDir), -1.0, 1.0);
    float phase = 0.5 * (1.0 + ctSun * ctSun);
    vec3 kChanW = vec3(0.55, 0.85, 1.35);  // R,G,B saturation rate; blue fastest
    vec3 inScatter = u_rayleigh * (vec3(1.0) - exp(-tau * kChanW));
    vec3 rayleigh = inScatter * rayW * phase;

    // Mie ("the white scatter"): uMieTintDay (near-white silver) at a high
    // sun, blending to the existing u_mie (orange) as the sun drops — an
    // ASCENDING smoothstep(0.02, 0.30, .) inverted to 1 at/below a low sun, 0
    // at/above a moderate sun (GLSL requires edge0 < edge1; this reproduces
    // the spec's described low-sun->1 / high-sun->0 curve without relying on
    // undefined descending-edge behaviour). Henyey-Greenstein forward halo
    // about the sun (unchanged core, just re-tinted) + a broad low-view-
    // elevation lift, gated by the SAME day/night window as Rayleigh (rayW)
    // so the forward halo is visible at any high sun, not just low sun near
    // sunset — the midday "white forward-scatter toward the sun" the spec's
    // screenshot table names.
    //
    // Quality fix pass item 5 ("restore Chad's dusk blaze"): mie_g stays at
    // Chad's own 0.66 dial (untouched) — the weak-peach failure was the
    // forward-halo WEIGHT, not the halo's tightness. duskBoost ramps the
    // hg (sun-forward) term's coefficient up to ~3.2x at a low sun while
    // trimming the broad rimV lift down (0.35 -> 0.18) so the warmth
    // concentrates as a real blaze NEAR the sun rather than bleeding orange
    // across the whole dome.
    float mixLow = 1.0 - smoothstep(0.02, 0.30, sunElevCos);
    vec3 mieTint = mix(uMieTintDay, u_mie, mixLow);
    float g = clamp(u_mieG, 0.0, 0.95);        // avoid the g->1 blowup
    float denom = 1.0 + g * g - 2.0 * g * ctSun;
    float hg = (1.0 - g * g) / (4.0 * 3.14159265 * pow(denom, 1.5));  // 1/4pi
    float vUp = dot(viewDir, up);              // seam-safe (local up)
    float rimV = 1.0 - smoothstep(0.0, 0.25, abs(vUp));  // broad low-elev lift
    // S-sunglare (Chad's fly, 2026-08-09: "the suns glare is too intense").
    // ATTRIBUTION: the sun DISC is a clean fwidth core with no halo, and the
    // high-sun Mie tint is now zero — so the glare is this forward-scatter halo
    // at a LOW sun, and it is a DOUBLE-COUNT introduced by the S-sungate fix.
    // The 3.2x duskBoost was tuned when rayW crushed low sun to 0.057; rayW is
    // now ~0.6-1.0 there, so the same boost multiplies out to a ~10-17x
    // brighter blaze than the tuning intended. The boost was compensating for
    // the very defect that got fixed.
    //
    // Both coefficients are now DIALS rather than baked numbers (the house
    // rule), and mie_halo_gain is THE glare knob: the low-sun peak lands ~2x
    // the pre-S-sungate look — the blaze is real again without the flare.
    float duskBoost = 1.0 + uMieDuskBoost * mixLow;
    vec3 mie = mieTint * rayW * (hg * uMieHaloGain * duskBoost +
                                 rimV * uMieRimLift);

    // The final multiply by airAmt: a deliberate belt-and-suspenders (spec
    // §1.4's explicit ask) on top of the per-channel Rayleigh's own tau->0
    // zero and the rayW/mixLow gates already on Mie — vacuum (tau==0,
    // airAmt==0) is EXACTLY vec3(0), never colored regardless of sun/view
    // angle.
    return (rayleigh + mie) * u_scatterStrength * airAmt;
}
)";

}  // namespace render
