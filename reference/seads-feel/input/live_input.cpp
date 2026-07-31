#include "input/live_input.h"

#include <algorithm>

#include "raylib.h"

namespace input {

namespace {

constexpr double kThrottleRate = 0.5;  // [1/s] held Shift/Ctrl sweep

// Map a +/- key pair to an override (mask, sign) in the sim::Inputs convention.
// Net-zero (both or neither held) => not overriding this axis: mask stays false
// so control::step never sees |sign| != 1 (it asserts on that).
void axis_override(int positive, int negative, bool& mask, double& sign) {
    double s = 0.0;
    if (IsKeyDown(positive)) s += 1.0;
    if (IsKeyDown(negative)) s -= 1.0;
    mask = (s != 0.0);
    sign = s;
}

}  // namespace

LiveInput poll_live(LiveDeviceState& device, double frame_dt) {
    LiveInput in;

    const Vector2 d = GetMouseDelta();
    in.mouse_dx = d.x;
    in.mouse_dy = d.y;

    // Override keys -> [pitch, yaw, roll], signs per SPEC §7 (same ergonomics
    // as raw_input: S = pull = pitch-up, A = roll-left, Q = nose-left = +1).
    axis_override(KEY_S, KEY_W, in.override_mask[0], in.override_sign[0]);
    axis_override(KEY_Q, KEY_E, in.override_mask[1], in.override_sign[1]);
    axis_override(KEY_A, KEY_D, in.override_mask[2], in.override_sign[2]);

    in.freelook_held = IsKeyDown(KEY_SPACE);
    in.zoom_held = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

    if (IsKeyDown(KEY_LEFT_SHIFT))
        device.throttle_target += kThrottleRate * frame_dt;
    if (IsKeyDown(KEY_LEFT_CONTROL))
        device.throttle_target -= kThrottleRate * frame_dt;
    device.throttle_target = std::clamp(device.throttle_target, 0.0, 1.0);
    in.throttle = static_cast<float>(device.throttle_target);

    // MB-flaps: F cycles clean -> combat -> landing -> clean; G toggles gear.
    // Edge-triggered latches (device settings, like throttle_target — the
    // PLANT owns the deploy slew toward them).
    if (IsKeyPressed(KEY_F)) device.flap_pos = (device.flap_pos + 1) % 3;
    if (IsKeyPressed(KEY_G)) device.gear_down = !device.gear_down;
    return in;
}

}  // namespace input
