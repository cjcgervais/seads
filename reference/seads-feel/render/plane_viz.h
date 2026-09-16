#pragma once

// Feature A: Hero-plane VISIBILITY CYCLE (key 'N').
// A pure, raylib-free header of tunable constexpr constants and POD params
// for the mirror-finish plane's visual mode. Mirrors the vortex.h / smoke.h
// pattern exactly: no clock, no state, just a mode enum + a lookup.
//
// The Mirror row REPRODUCES the old shader output bit-for-bit:
//   body_floor=0.35 => body = u_planeColor*(0.35 + 0.65*ndl)  (1-0.35=0.65)
//   emissive=0, white_mix=0, rim_gain=0, refl_scale=1 => all new terms 0.
// Every other mode is a cosmetic deviation from that baseline.

namespace render {

enum class PlaneViz {
    Mirror,      // classic stealth-reflective finish (original look)
    NeonRim,     // hot neon rim + brighter body (Chad's default pick)
    Searchlight, // near-white self-glow, de-saturated body
    Glow,        // warm diffuse glow, moderate rim
    Halo,        // NeonRim + a rainbow light-ring (glory) around the plane
    Count        // sentinel — keep last
};

// Per-mode cosmetic parameters (render-side tunable constants, exactly like
// vortex.h's kVortex* constants). All terms are additive over the Mirror
// baseline: with Mirror values the output equals the old shader exactly.
struct VizParams {
    float body_floor;  // ambient brightness floor of the body chroma
    float refl_scale;  // multiplies the config reflectivity [0,1]
    float emissive;    // self-glow strength added to the body
    float white_mix;   // how white the emissive term is [0,1] (searchlight=hot)
    float rim_gain;    // ADDITIVE Fresnel-rim strength (silhouette blaze)
    float rim_white;   // rim color: plane chroma -> white [0,1] (outline)
    float desat;       // desaturate the body toward white [0,1]
    float halo_gain;   // rainbow light-ring alpha [0,1] (0 = no ring drawn)
};

inline VizParams viz_params(PlaneViz m) {
    switch (m) {
        //       floor  refl  emis  white  rim  rimWht desat  halo
        case PlaneViz::Mirror:
            return {0.35f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.0f};
        case PlaneViz::NeonRim:  // cooler: white-hot outline + slight desat
            return {0.62f, 0.45f, 0.16f, 0.00f, 2.20f, 0.80f, 0.18f, 0.0f};
        case PlaneViz::Searchlight:  // de-saturated white blow-out
            return {0.88f, 0.20f, 0.70f, 0.90f, 0.80f, 0.85f, 0.65f, 0.0f};
        case PlaneViz::Glow:  // soft even glow, gentle white rim
            return {0.72f, 0.30f, 0.42f, 0.20f, 0.55f, 0.35f, 0.12f, 0.0f};
        case PlaneViz::Halo:  // NeonRim look + the rainbow light-ring
            return {0.62f, 0.45f, 0.16f, 0.00f, 2.20f, 0.80f, 0.18f, 0.9f};
        default:
            return {0.35f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.0f};
    }
}

inline const char* viz_name(PlaneViz m) {
    switch (m) {
        case PlaneViz::Mirror:      return "MIRROR";
        case PlaneViz::NeonRim:     return "NEON-RIM";
        case PlaneViz::Searchlight: return "SEARCHLIGHT";
        case PlaneViz::Glow:        return "GLOW";
        case PlaneViz::Halo:        return "HALO";
        default:                    return "MIRROR";
    }
}

}  // namespace render
