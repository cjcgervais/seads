#include "input/raw_input.h"

#include <algorithm>

#include "raylib.h"

namespace input {

namespace {

constexpr double kThrottleRate = 0.5;  // [1/s] held Shift/Ctrl sweep
constexpr float kStickReach = 0.35f;   // mouse offset (fraction of the
                                       // smaller screen dimension) for full
                                       // deflection

float key_axis(int positive, int negative) {
    float v = 0.0f;
    if (IsKeyDown(positive)) v += 1.0f;
    if (IsKeyDown(negative)) v -= 1.0f;
    return v;
}

}  // namespace

sim::Inputs poll_raw(RawDeviceState& device, double frame_dt) {
    // Sign map (sim/state.h): pitch +1 = pitch UP, roll +1 = roll LEFT,
    // yaw +1 = nose LEFT. Stick ergonomics live here, on purpose.
    sim::Inputs in;
    in.pitch = key_axis(KEY_S, KEY_W);  // S = pull
    in.roll = key_axis(KEY_A, KEY_D);   // A = roll left
    in.yaw = key_axis(KEY_Q, KEY_E);    // Q = nose left

    // Virtual stick while RMB held: absolute offset from screen centre.
    // Right = roll right (negative input), pulled toward the pilot
    // (screen-down, +y) = pitch up.
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const float reach =
            kStickReach *
            static_cast<float>(std::min(GetScreenWidth(), GetScreenHeight()));
        const Vector2 m = GetMousePosition();
        const float dx =
            (m.x - 0.5f * static_cast<float>(GetScreenWidth())) / reach;
        const float dy =
            (m.y - 0.5f * static_cast<float>(GetScreenHeight())) / reach;
        in.pitch += dy;
        in.roll -= dx;
    }

    in.pitch = std::clamp(in.pitch, -1.0f, 1.0f);
    in.roll = std::clamp(in.roll, -1.0f, 1.0f);
    in.yaw = std::clamp(in.yaw, -1.0f, 1.0f);

    if (IsKeyDown(KEY_LEFT_SHIFT))
        device.throttle_target += kThrottleRate * frame_dt;
    if (IsKeyDown(KEY_LEFT_CONTROL))
        device.throttle_target -= kThrottleRate * frame_dt;
    device.throttle_target = std::clamp(device.throttle_target, 0.0, 1.0);
    in.throttle = static_cast<float>(device.throttle_target);

    // MB-flaps latches (parity with poll_live). The caller maps flap_pos ->
    // in.flap_cmd with the config combat fraction (input/ is config-free).
    if (IsKeyPressed(KEY_F)) device.flap_pos = (device.flap_pos + 1) % 3;
    if (IsKeyPressed(KEY_G)) device.gear_down = !device.gear_down;
    return in;
}

}  // namespace input
