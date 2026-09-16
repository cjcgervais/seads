#pragma once

namespace input {

// R4-FLY-7 (Chad: "won't let me take off or taxi") — which raylib mouse-button
// codes drive the thumb throttle. Windows drivers DISAGREE on which code a
// physical thumb button fires (3 SIDE / 4 EXTRA / 5 FORWARD / 6 BACK), and a
// wrong guess reads as a stuck brake or a dead throttle — so the codes are
// config keys ([input] in game.toml), passed in by the CALLER (input/ stays
// config-free). The HUD's MB readout shows the code of any held button, so a
// mismatch is diagnosable on sight. A negative code = unbound slot.
struct ThumbBinds {
    int up_a = 4;     // MOUSE_BUTTON_EXTRA
    int up_b = 5;     // MOUSE_BUTTON_FORWARD
    int down_a = 3;   // MOUSE_BUTTON_SIDE
    int down_b = -1;  // default unbound: a speculative extra down-bind risks
                      // colliding with a driver's forward code (R4-FLY-7 —
                      // the suspect for "throttle up applies the brake")
};

}  // namespace input
