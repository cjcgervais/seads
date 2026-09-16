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

LiveInput poll_live(LiveDeviceState& device, double frame_dt,
                    const ThumbBinds& binds) {
    LiveInput in;

    const Vector2 d = GetMouseDelta();
    in.mouse_dx = d.x;
    in.mouse_dy = d.y;
    in.wheel = GetMouseWheelMove();  // freelook-only dolly-out (caller-gated)

    // Override keys -> [pitch, yaw, roll], signs per SPEC §7 (same ergonomics
    // as raw_input: S = pull = pitch-up, A = roll-left, Q = nose-left = +1).
    axis_override(KEY_S, KEY_W, in.override_mask[0], in.override_sign[0]);
    axis_override(KEY_Q, KEY_E, in.override_mask[1], in.override_sign[1]);
    axis_override(KEY_A, KEY_D, in.override_mask[2], in.override_sign[2]);

    in.freelook_held = IsKeyDown(KEY_SPACE);
    in.zoom_held = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in.fire_held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (IsKeyDown(KEY_LEFT_SHIFT))
        device.throttle_target += kThrottleRate * frame_dt;
    if (IsKeyDown(KEY_LEFT_CONTROL))
        device.throttle_target -= kThrottleRate * frame_dt;
    // R4-FLY-5 mouse-thumb throttle (Chad's ask): the FORWARD thumb button
    // throttles up; the BACK thumb button throttles down, and HELD past zero
    // becomes the wheel brake ("all the way to zero plus more is brake").
    // Keyboard SHIFT/CTRL/B stay live in parallel.
    // R4-FLY-6/7: which raylib codes the physical thumbs fire is DRIVER
    // territory (Windows mice disagree), so the codes come from the caller
    // (config [input], ThumbBinds) and the HUD MB readout diagnoses a
    // mismatch on sight. NOTE (by design, not a bug): holding BOTH thumbs
    // nets throttle 0 with the brake held — that IS the runup; releasing
    // the back thumb with the forward one held spools up.
    const auto held = [](int code) {
        return code >= 0 && IsMouseButtonDown(code);
    };
    const bool thumb_up = held(binds.up_a) || held(binds.up_b);
    const bool thumb_down = held(binds.down_a) || held(binds.down_b);
    if (thumb_up) device.throttle_target += kThrottleRate * frame_dt;
    if (thumb_down) device.throttle_target -= kThrottleRate * frame_dt;
    const bool thumb_brake = thumb_down && device.throttle_target <= 0.0;
    device.throttle_target = std::clamp(device.throttle_target, 0.0, 1.0);
    in.throttle = static_cast<float>(device.throttle_target);

    // MB-flaps: F cycles clean -> combat -> landing -> clean; G toggles gear.
    // Edge-triggered latches (device settings, like throttle_target — the
    // PLANT owns the deploy slew toward them).
    if (IsKeyPressed(KEY_F)) device.flap_pos = (device.flap_pos + 1) % 3;
    if (IsKeyPressed(KEY_G)) device.gear_down = !device.gear_down;
    // R4g wheel brakes: B held OR the back thumb button held past zero
    // throttle (parity with poll_raw).
    in.wheel_brake = (IsKeyDown(KEY_B) || thumb_brake) ? 1.0f : 0.0f;
    return in;
}

}  // namespace input
