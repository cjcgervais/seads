#pragma once

#include <algorithm>
#include <cmath>

#include "render/sled_audio.h"

// Snowmachine engine DRIVER -- the state behind the three layers.
//
// Turns a throttle scalar into revs, sustain, and onset accents.
//
// ⚠ THIS IS DRIVEN AT FRAME RATE, NEVER AT BUFFER RATE. An earlier version had
// render/sled_synth.h call update() once per audio buffer -- 46 ms at 1024
// frames / 22050 Hz. A red-team showed that silently DROPS real taps: a 40 ms
// keypress that opens and closes entirely between two buffers is never seen,
// and two quick stabs inside one buffer collapse to one. Chad's whole ask is
// "tapping keys gives the brap brap", so the onset detector must run where the
// input does. The synth now consumes a LATCH this fills, and the app pushes
// drivers per frame -- the same "drivers set before the drain" idiom the wind
// and gun channels already use.
//
// Cosmetic, render-side only. Firewall: only <algorithm>, <cmath>,
// render/sled_audio.h. No raylib, no <random>, no wall-clock, no allocation.

namespace render {

struct SledDrive {
    double revs = 0.0;        // 0 = idle, 1 = pinned
    double sustain = 0.0;     // [s] throttle continuously held
    double accent_age = -1.0; // [s] since last onset; negative = none
    double since_accent = 1e9;// [s] since last onset, for the refractory
    bool open = false;        // hysteretic throttle gate state

    // Onset latch. Set here at frame rate, consumed by the synth at buffer
    // rate, so an onset can never fall between two render() calls.
    int pending_accents = 0;

    // Returns true on the tick an accent is triggered.
    bool update(double throttle, double dt) {
        const double t = std::clamp(throttle, 0.0, 1.0);

        // HYSTERETIC gate -- two thresholds, so a throttle dwelling at the edge
        // cannot chatter the accent (CLAUDE.md: every gate leg is hysteretic).
        const bool was_open = open;
        if (open) {
            if (t < kSledOnsetOff) open = false;
        } else {
            if (t > kSledOnsetOn) open = true;
        }

        revs = sled_rev_step(revs, t, dt);

        if (open && dt > 0.0) {
            sustain += dt;
        } else if (!open) {
            sustain = 0.0;
        }

        if (dt > 0.0) {
            since_accent += dt;
            if (accent_age >= 0.0) {
                accent_age += dt;
                if (accent_age >= kSledAccentLife) accent_age = -1.0;
            }
        }

        // Rising edge of the hysteretic gate, subject to a refractory so the
        // pipe cannot be re-barked faster than it could physically re-load.
        const bool onset = open && !was_open && since_accent >= kSledAccentRefractory;
        if (onset) {
            accent_age = 0.0;
            since_accent = 0.0;
            ++pending_accents;  // latched for the synth
        }
        return onset;
    }

    // Consume the latch. Returns how many accents fired since the last call,
    // so a burst inside one buffer is not lost -- it is capped at a small
    // number because retriggering the same one-shot more than that within one
    // buffer is inaudible anyway.
    int take_accents() {
        const int n = std::min(pending_accents, 4);
        pending_accents = 0;
        return n;
    }

    double idle_level() const { return sled_idle_level(revs); }
    double run_level() const { return sled_run_level(revs); }
    double accent_level() const { return sled_accent_level(accent_age); }
    double tone_level() const { return sled_tone_level(revs, sustain); }

    // Playback rate for the idle loop: it stiffens slightly as revs come up
    // rather than sitting at a fixed pitch while the tone climbs past it.
    //
    // The kSledPitchPedestal factor is Chad's first-ride ruling ("it need to
    // come down one or two octaves it really high pitched") -- see the long
    // note in sled_audio.h for the measurements that set it and why it is
    // exactly one octave. It scales the WHOLE expression, not the offset, so
    // the 1.35x sweep across the rev range survives intact and only the floor
    // moves. Chase pitch THERE, never here.
    double idle_ratio() const {
        return kSledPitchPedestal * (1.0 + 0.35 * std::clamp(revs, 0.0, 1.0));
    }

    // Playback rate for the riding bed, pitched by revs. This is the layer that
    // carries the sense of speed, so it gets the wider range. Same pedestal,
    // same reason -- and this is the layer that was worst: 517 Hz of source
    // content played at 1.5x reached 776 Hz at full throttle.
    double run_ratio() const {
        return kSledPitchPedestal * (0.80 + 0.70 * std::clamp(revs, 0.0, 1.0));
    }

    // The engine's PHYSICAL firing rate -- reference only, nothing plays at it.
    double fire_hz() const { return sled_fire_hz(revs); }
    // What the tone layer actually sings: the firing rate transposed down by
    // kSledVoiceTranspose (Chad's ear, twice). The synth must use THIS one --
    // feeding it fire_hz() would leave the tone an octave above the sampled
    // beds and pull the machine apart into two engines.
    double voice_hz() const { return sled_voice_hz(revs); }
    double tone_cutoff() const { return sled_tone_cutoff(revs); }
};

}  // namespace render
