#pragma once

// Section-2 golden trajectory (HARNESS §3 T2) — full-state checkpoints of
// the scenario in test/harness/scenarios.h, recorded via `seads_harness
// golden` and committed. Single C++ runtime, compared with float tolerance.
// A moved golden HALTS the loop: confirm the move is intentional (plant or
// config change), re-record, and say so in the commit message.
//
// Recorded: 2026-07-03, Section 2 plant (incl. throttle slew per red-team
// P2-1), config/aircraft.toml as committed.
// RE-RECORDED: 2026-07-06 — §7 responsiveness: Cl_max 1.8, c_pitch 7->11, c_yaw
// 4->16, c_roll 15->24 (more pitch/yaw/roll authority). The raw scripted flight
// moves from tick 300 on (the higher authority bites everywhere). Intentional;
// same plant CODE, only aircraft.toml knobs moved.
// RE-RECORDED: 2026-07-07 — Mission B fly-verdict dial: c_pitch 11->13 (Chad:
// "raise the pitch (elevator authority) just a little bit higher for gross
// keyboard input and instructor"). Plant knob only; the raw scripted flight's
// pitch legs bite harder. Intentional.

// RE-RECORDED 2026-07-23 (v5 RUNG D, Chad's arcade energy ruling): PLANT
// dials only -- k_induced 0.05 -> 0.015, T_max 9000 -> 18000 (T/W 0.31 ->
// 0.61). Same sim code; the scripted raw flight flies the same inputs
// through a stronger, cleaner airframe (divergence thrust-driven from the
// first ticks). n_max is a controller dial and cannot touch this golden.

namespace golden {

struct Checkpoint {
    int tick;
    double px, py, pz;
    double vx, vy, vz;
    double qw, qx, qy, qz;
    double wx, wy, wz;
};

// RE-RECORDED 2026-07-08 (MB envelope: T_max 5800->9000 + compression knee
// v_full 111->140 / v_redline 208->245 — Chad's speed + Q3b compression
// rulings; the raw scripted flight accelerates and deflects harder
// everywhere). Intentional.
// RE-RECORDED 2026-07-11 (S-wvane, SPEC §0 — NEW PLANT FORCE, Chad's ruling
// "forget the crabbing, I don't want the cocking... snap to the centre of
// the mouse aim every time": fuselage side-force from sideslip Cy_beta 2.5 +
// its drag price Cd_beta 0.6, sim/step.cpp. The raw script's 2 s yaw=0.3
// skid leg now BLEEDS its crab flat (tau ~1 s at combat V) and pays sideslip
// drag — every checkpoint from 600 on shifts; tick 300 moves only in the
// e-15 tail (pre-skid). Knob-off Cy=Cd=0 is pinned bit-identical in
// test_aero "S-wvane" legs. Intentional.)
inline constexpr Checkpoint kFlight[] = {
{300,
     16992.423322834253, 1.4267355341770402e-15, -361.36608114491554,
     11.839303509297169, 2.2138225454742246e-15, -149.48938734065828,
     0.69564211107966856, 0.12681503575137351, -0.12681503575137351, -0.69564211107966845,
     0.99272429869999257, 0, 0},
    {600,
     17240.071788212266, -2.0637910817698701, -559.45395224467495,
     127.57501251598514, -6.7440688560152102, 20.652216309595044,
     -0.32918716790457253, 0.7735530932937047, 0.40047002425617756, -0.36452047955494121,
     0.28193990671225472, 0, -2.998546728176378},
    {900,
     17519.628666108729, -15.004597662269292, -438.70603077012441,
     99.93669215536859, 25.503008112458641, 51.442202067864585,
     -0.59568453269743638, -0.64570437872415942, 0.46518054803744674, 0.10877890663664753,
     0.00084543537645029223, 0.59964266313092418, -0.00038972275256457897},
    {1200,
     17702.984573015601, 143.98568827612763, -357.59321979476175,
     49.22436273871908, 86.834565042754889, 16.335874003404964,
     -0.68425191292564613, -0.65380103456976579, 0.32080001009290882, -0.037826979491995164,
     5.2575121894875216e-07, 0.00040727905849003077, -4.4261429025094537e-09}
};

}  // namespace golden
