#pragma once
// ===========================================================================
// THE FEEL TAPE'S COLUMN LIST -- ONE definition, used by BOTH the writer
// (app/main.cpp's feel_tape_hook) and every reader (test/harness/feel_tape.h).
//
// WHY THIS FILE EXISTS (2026-09-13). The tape had TWO independent
// emitter/reader divergences, and both produced confident, wrong numbers:
//
//   1. The row writer grew to 74 columns (v4's full per-tick state, then v5's
//      aim quaternion + control::Internal latches) but the HEADER string was
//      never extended past 52. Everything after col 52 was written to disk
//      and NAMED NOWHERE, so a name-indexed reader could not see it.
//   2. test_loop_rollover.cpp's reader is POSITIONAL with v2-era indices
//      (alt = v[29]); against the grown row that index is hand_rest.
//
//      The cost: a probe seeding from names got 0.0 for angular_vel, the aim
//      quaternion and every latch, so "Chad's 3 worst dives" replayed with
//      zero body rates and a reset controller. Both arms of the counterfactual
//      followed the same non-dive and the dial read ON == OFF. The verdict was
//      void -- not a result about the dial at all.
//
// THE LAW THIS ENCODES: the writer never spells a column name and the reader
// never spells an index. Both derive from kFeelTapeColumns, so a new column is
// one edit and a divergence is a compile/parse error, not a silent zero.
// test_tape_roundtrip.cpp asserts end-to-end that the header this list prints
// has exactly as many fields as the row the writer emits.
// ===========================================================================

#include "app/feel_tape_fields.h"

namespace app {

// Order IS the wire format: index i here is field i of every data row.
// `int_*` = a control::Internal latch (provenance in the name, because three
// of these duplicate an earlier telemetry column of the same concept).
inline constexpr const char* kFeelTapeColumns[] = {
    // --- v1: the mouse and the cascade's own view of the tick ---
    "tick", "t", "frame_ticks", "aim_dx", "aim_dy",
    "arx", "ary", "arz",                      // in.aim_rate_ff
    "aimx", "aimy", "aimz",                   // st.aim.forward()
    "tbx", "tby", "tbz",                      // aim in BODY frame
    "err", "blend", "phi", "theta", "cos_pt",
    "wdx", "wdy", "wdz",                      // telem.omega_des
    "held_bank",
    // --- v2: the state the replay grades against + the roll channel limb
    //         by limb, so an attribution is READ, never inferred ---
    "alt", "speed", "righting", "push_mode", "deadzoned",
    "roll_latch", "hand_rest", "hand_gate",
    "roll_hold", "roll_maneuver", "roll_right",
    // --- TARGET 2: the nose/aim WORLD elevations and their gap.
    //     SIGN: elev_gap = nose - aim, so NEGATIVE means the nose is BELOW
    //     the aim -- Chad's "it noses down crashing me" condition. (The
    //     first lateral gate was built against the opposite reading.) ---
    "aim_world_elev", "nose_world_elev", "elev_gap",
    // --- v4: the FULL per-tick sim seed. v2's single t=0 seed made only the
    //     opening window replayable; with these, ANY tick is a seed. ---
    "px", "py", "pz",
    "vx", "vy", "vz",
    "qw", "qx", "qy", "qz",
    "vhx", "vhy", "vhz",
    "load_factor",
    // Duplicates col "push_mode" above; kept at this POSITION so tapes cut by
    // the v4 exe keep their field alignment. Readers want the first one.
    "push_mode2",
    // --- v5: the seed fields whose absence voided the dive battery ---
    "wx", "wy", "wz",                          // st.prev.angular_vel
    "aqw", "aqx", "aqy", "aqz",                // st.aim.q -- apply_mouse
                                               // rotates about the frame's OWN
                                               // axes, so forward() alone
                                               // steers a mid-tape seed down a
                                               // different path
    "int_roll_latch",                          // a SIGN latch: seeding it 0
                                               // mid-manoeuvre picks the other
                                               // way round
    "int_elev_latch",
    "int_integx", "int_integy", "int_integz",
    "int_aoa_filtered",                        // the AoA pushback is what
                                               // binds the pitch channel
    "int_capture", "int_ballistic", "int_deadzoned", "int_pursuit",
    "int_rest_time", "int_inv_rest", "int_righting",
    // --- the pitch clamps, so a tape answers "was the channel bounded?" by
    //     reading (measured: the AoA ceiling binds 112-333 of 720 ticks on
    //     his lateral dives, against 0-3 for the G ceiling) ---
    "aoa_ceil", "w_max_p",
    // S-yawbudget: the factor the digging yaw was scaled by (1 = untouched).
    "yaw_budget_scale",
    // S-tremor: the windowed NET aim rate [rad/s] and the continuous liveness
    // it produced (0 = the motion netted to nothing, 1 = fully live). Read
    // them BESIDE hand_rest/hand_gate -- a tremor shows a live hand_dx with a
    // near-zero hand_net_rate, which is the whole signature.
    "hand_net_rate", "hand_live_frac",
// --- THE FULL REPLAY STATE, generated from SEADS_TAPE_STATE_FIELDS so the
//     names can never drift from the members. Everything above this line is
//     DIAGNOSTIC (what an analysis reads); everything below is what a SEED
//     restores. sim::SimState::throttle/flap/gear live here -- their absence
//     was the 4.7e-02 m/s first-tick dV.
#define SEADS_TAPE_NAME_ONE(name, expr) #name,
    SEADS_TAPE_STATE_FIELDS(SEADS_TAPE_NAME_ONE, SEADS_TAPE_NAME_ONE,
                            SEADS_TAPE_NAME_ONE, SEADS_TAPE_NAME_ONE)
#undef SEADS_TAPE_NAME_ONE
};

inline constexpr int kFeelTapeColumnCount =
    int(sizeof(kFeelTapeColumns) / sizeof(kFeelTapeColumns[0]));

}  // namespace app
