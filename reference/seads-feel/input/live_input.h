#pragma once

// Instructor-mode device polling (SPEC §9.5 / §10): the LIVE path device ->
// (mouse delta, override mask/sign, freelook, throttle, debug toggle). This is
// the mouse-aim counterpart to raw_input.h; raw_input stays the raw-mode path,
// untouched. It returns RAW device readings only — NO aim math (that is the
// pure input::AimFrame) and NO instructor state (that is the pure control::step
// + the shared input::Freelook). Keeping it dumb is what lets the aim/camera/
// freelook logic stay in the tested pure modules the harness also drives.
//
// Sampled once per frame (SPEC §10); the fixed-tick loop consumes it.

namespace input {

// Persistent device state across frames (the held-button throttle integrator,
// same as raw_input's — the pilot's commanded setting; the PLANT owns the
// slew).
struct LiveDeviceState {
    double throttle_target = 1.0;  // spawn at full power (SPEC §6.3)
    // MB-flaps: latched device settings (edge-triggered keys). flap_pos is
    // the 3-position selector 0 = clean / 1 = combat / 2 = landing; the
    // fraction mapping (0 / combat_frac / 1) is config, applied by the
    // caller. Reset to clean/up on respawn (spawn state is clean).
    int flap_pos = 0;
    bool gear_down = false;
};

// One frame's device readings. Override arrays are indexed [pitch, yaw, roll]
// to match control::Input; sign is in the sim::Inputs convention (SPEC §7:
// pitch +1 = up, yaw +1 = nose-left, roll +1 = roll-left) so the caller can
// hand them straight to control::Input without a second sign map.
struct LiveInput {
    double mouse_dx = 0.0;  // GetMouseDelta this frame [px], + = right
    double mouse_dy = 0.0;  // + = down (screen y)
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
    bool freelook_held = false;  // Space held (SPEC §9.5)
    bool zoom_held = false;      // RMB held -> gunsight zoom (instructor only;
                                 // raw mode keeps RMB = virtual stick)
    float throttle = 0.0f;       // [0,1] passthrough command
    // (MB-flaps state lives on LiveDeviceState — a LATCH, not a held key: it
    // survives focus loss like throttle_target; the caller reads the device.)
};

// Poll raylib once (needs a focused window). frame_dt drives only the held-key
// throttle rate. A net-zero axis (both keys, or neither) leaves that override
// off — control::step asserts |sign| == 1 when the mask is set.
LiveInput poll_live(LiveDeviceState& device, double frame_dt);

}  // namespace input
