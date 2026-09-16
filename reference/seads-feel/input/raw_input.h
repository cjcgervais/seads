#pragma once

#include "input/thumb_binds.h"
#include "sim/state.h"

// Raw mode (SPEC §5 / §11): device -> Inputs DIRECTLY, no instructor.
// Sections 1-3 fly on this path; the sim never sees a keycode — that
// translation ends here. Nothing in this layer smooths, slews, or filters:
// keys are +/-1 deflections, the mouse virtual stick is an ABSOLUTE offset
// from screen centre (frame-rate independent by construction, unlike a
// per-frame delta). AimIntent + bindings.toml arrive with the instructor
// in Section 4.

namespace input {

// Device-side persistent state. throttle_target is the pilot's commanded
// setting (SPEC §9.8 passthrough — the PLANT owns the slew toward it; this
// rate is just key ergonomics for a held button).
struct RawDeviceState {
    double throttle_target = 1.0;  // spawn at full power
    // MB-flaps latches (F cycles, G toggles — same keys as the live path).
    // poll_raw updates them; the CALLER maps position -> Inputs.flap_cmd
    // with the config combat fraction (input/ stays config-free).
    int flap_pos = 0;  // 0 clean / 1 combat / 2 landing
    bool gear_down = false;
};

// Poll raylib devices once per frame (SPEC §10: sampled per frame, consumed
// by every fixed tick inside the accumulator). frame_dt drives only the
// held-key throttle rate. `binds` = config-sourced thumb codes (R4-FLY-7).
sim::Inputs poll_raw(RawDeviceState& device, double frame_dt,
                     const ThumbBinds& binds = {});

}  // namespace input
