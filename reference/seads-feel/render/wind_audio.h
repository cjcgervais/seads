#pragma once

#include <algorithm>
#include <cmath>

// MB-7c (i): wind audio ∝ airspeed (Chad: "esp the wind audio" — the energy
// bleed becomes AUDIBLE: a dive roars, a zoom apex goes quiet, thin air up
// near the ceiling hushes the whole channel so the soft ceiling is audible
// too). This header is the PURE mapping speed -> (volume, pitch); the raylib
// AudioStream plumbing (noise generation, stream feeding) is caller glue in
// app/main.cpp. READ-ONLY off SimState — firewalled like the S8 gunsight:
// nothing here feeds input/control/sim, so it can never move a golden.
//
// Tunables are code constants by the S9-zoom/camera precedent (render-side
// cosmetic knobs Chad ruled may live in code): kWindVQuiet..kWindVRoar spans
// the felt envelope — silent below stall-ish speeds, full roar near redline.

namespace render {

inline constexpr double kWindVQuiet = 35.0;   // [m/s] below: silent
inline constexpr double kWindVRoar = 230.0;   // [m/s] full volume (~redline)
inline constexpr double kWindVolExpo = 1.6;   // perceptual ramp shape
inline constexpr double kWindPitchLo = 0.55;  // stream pitch at V_quiet
inline constexpr double kWindPitchHi = 1.75;  // stream pitch at V_roar
inline constexpr double kWindMaster = 0.65;   // master gain [0,1]
// Thin air hushes the wind: at atm_frac f the volume scales by
// kWindAtmFloor + (1-kWindAtmFloor)*f — audible thinning, never fully mute
// (the airframe still moves through SOME air wherever it can fly).
inline constexpr double kWindAtmFloor = 0.35;

struct WindLevel {
    double volume = 0.0;  // [0,1] stream volume
    double pitch = 1.0;   // stream pitch multiplier
};

// speed [m/s], atm = sim::atm_frac at the current altitude (1 in the fight
// band). Monotone in speed, continuous everywhere, clamped.
inline WindLevel wind_level(double speed, double atm) {
    WindLevel w;
    const double t = std::clamp(
        (speed - kWindVQuiet) / (kWindVRoar - kWindVQuiet), 0.0, 1.0);
    const double a = std::clamp(atm, 0.0, 1.0);
    w.volume = kWindMaster * std::pow(t, kWindVolExpo) *
               (kWindAtmFloor + (1.0 - kWindAtmFloor) * a);
    w.pitch = kWindPitchLo + (kWindPitchHi - kWindPitchLo) * t;
    return w;
}

}  // namespace render
