#pragma once

#include <glm/glm.hpp>

#include "control/extract.h"
#include "sim/aero.h"
#include "sim/params.h"
#include "sim/state.h"
#include "sim/world.h"

// The HUD's flight readouts (SPEC §12), computed in the CORRECT FRAME:
// velocity-relative AoA and local_up-aware bank/G — never a fixed-axis or
// body-pitch proxy (HARNESS §1 row 5 red-team focus). It reads through the ONE
// shared control::extract (so "which up" is the same local_up the controller
// and harness use, recomputed here) and sim::load_factor (the single-source
// true n, shared bit-identically with the controller telemetry). A re-derived
// AoA/bank/G anywhere else would be the "flat instrument certifies flat
// controller" fork (CLAUDE.md S3). PURE / raylib-free so the gate grades it.
//
// This is a passive DISPLAY read of SimState.last_vhat: the §9.6 seam forbids
// the CONTROLLER sharing the sim's held v-hat, but the HUD is an instrument on
// the plant's own state, not a second control layer — it shows what the plant
// sees. It never feeds a control decision.

namespace render {

struct FlightReadout {
    double speed = 0.0;        // [m/s]
    double altitude = 0.0;     // [m]
    double aoa = 0.0;          // [rad] velocity-relative, + = nose above vel
    double load_factor = 0.0;  // true n = q*S*Cl/(m*g), signed
    double bank = 0.0;  // [rad] phi, + = right wing down (local_up-aware)
    // MB HUD attitude reads: ATTITUDE, not velocity — these hold their value
    // in steady flight (the AoA delta zeros out; Chad's ask, 2026-07-08).
    double pitch_attitude = 0.0;  // [rad] nose above the local horizon
    double bank_full = 0.0;       // [rad] FULL-RANGE bank, +/-pi (phi FOLDS
                                  // past 90 deg — never feed a roll gauge phi)
};

// Commanded flap detent -> HUD label (pure, pinned by test_hud). Out-of-range
// falls back to CLEAN rather than indexing anything.
inline const char* flap_mode_label(int mode) {
    return mode == 1 ? "COMBAT" : mode == 2 ? "LANDING" : "CLEAN";
}

inline FlightReadout flight_readout(const sim::SimState& s,
                                    const sim::AircraftParams& p) {
    const control::Extracted e = control::extract(s, s.last_vhat, p.v_dir_eps);
    FlightReadout r;
    r.speed = e.speed;
    r.altitude = sim::altitude(s.position, p);
    r.aoa = e.alpha;
    r.load_factor = sim::load_factor(e.alpha, e.speed, r.altitude, s.flap, p);
    r.bank = e.phi;
    // Attitude pitch: angle of the NOSE above the local horizon plane —
    // velocity-independent (a vhat here would read flight-path angle gamma,
    // the exact "moves instead of holds" defect this field replaces).
    r.pitch_attitude =
        std::asin(std::clamp(glm::dot(e.nose, e.local_up), -1.0, 1.0));
    // Full-range bank: atan2(sin(phi_true)*cos(theta), cos(phi_true)*
    // cos(theta)) — cos(theta) >= 0 cancels, giving the true +/-pi roll with
    // the SPEC 7 sign (+ = right wing down). The asin phi FOLDS at 90 deg
    // (bank 100 reads 80); the SIGNED cos_phi_theta (never clamped >= 0)
    // unfolds it. At nose-vertical both args -> 0 and the roll is gauge; the
    // 12->6 o'clock snap through a loop apex is REAL (you are inverted) — do
    // NOT smooth this (a display ease here is the 9.1-adjacent trap).
    r.bank_full =
        std::atan2(-glm::dot(e.body_right, e.local_up), e.cos_phi_theta);
    return r;
}

}  // namespace render
