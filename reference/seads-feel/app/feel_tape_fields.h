#pragma once
// ===========================================================================
// THE REPLAY STATE, ENUMERATED ONCE.
//
// WHY (2026-09-13, the third instance of one bug): a seeded replay is only as
// honest as the state it restores, and twice now the tape has silently
// restored a DIFFERENT aeroplane than the one Chad flew:
//
//   1. The row writer emitted 74 columns while its header named 52, so every
//      v5 seed field was on disk and named nowhere. A name-indexed probe read
//      0.0 for angular_vel, the aim quaternion and every latch -- and "Chad's
//      3 worst dives" replayed with zero body rates and a reset controller.
//   2. After that was fixed, the seed STILL omitted sim::SimState::throttle,
//      ::flap and ::gear -- plant state the tape never named at all. The
//      throwing reader could not catch it: it guards columns that are
//      named-and-absent, not state nobody ever listed. Symptom: a constant
//      4.7e-02 m/s first-tick dV that no throttle INPUT could correct,
//      because the plant slews its own internal value.
//
// Field-by-field patching is what produced instance 2 while fixing instance
// 1. So the list below IS the contract: the column names, the writer and the
// seeder are all generated from it, and the static_asserts at the foot fail
// the BUILD if a member is added to sim::SimState or control::Internal
// without being added here.
//
// X-macro arguments: (column_name, accessor_expression_on_`S`).
//   XD = double-valued (glm components are listed one per column)
//   XB = bool          XI = int          XE = enum (CaptureState)
// `S` is an app::LoopState lvalue, so one list serves both directions.
// ===========================================================================

#include "app/instructor_tick.h"

// --- sim::SimState (the PLANT). `curr` is the post-tick state. -------------
#define SEADS_TAPE_SIM_FIELDS(XD, XB, XI, XE)                                 \
    XD(s_px, S.curr.position.x)                                               \
    XD(s_py, S.curr.position.y)                                               \
    XD(s_pz, S.curr.position.z)                                               \
    XD(s_vx, S.curr.velocity.x)                                               \
    XD(s_vy, S.curr.velocity.y)                                               \
    XD(s_vz, S.curr.velocity.z)                                               \
    XD(s_qw, S.curr.orientation.w)                                            \
    XD(s_qx, S.curr.orientation.x)                                            \
    XD(s_qy, S.curr.orientation.y)                                            \
    XD(s_qz, S.curr.orientation.z)                                            \
    XD(s_wx, S.curr.angular_vel.x)                                            \
    XD(s_wy, S.curr.angular_vel.y)                                            \
    XD(s_wz, S.curr.angular_vel.z)                                            \
    XD(s_throttle, S.curr.throttle)                                           \
    XD(s_vhx, S.curr.last_vhat.x)                                             \
    XD(s_vhy, S.curr.last_vhat.y)                                             \
    XD(s_vhz, S.curr.last_vhat.z)                                             \
    XD(s_flap, S.curr.flap)                                                   \
    XD(s_gear, S.curr.gear)                                                   \
    XB(s_on_ground, S.curr.on_ground)                                         \
    XB(s_crashed, S.curr.crashed)                                             \
    XI(s_wing_strike, S.curr.wing_strike)                                     \
    XB(s_prop_strike, S.curr.prop_strike)

// --- control::Internal (the CASCADE's memory) ------------------------------
#define SEADS_TAPE_INT_FIELDS(XD, XB, XI, XE)                                 \
    XD(i_integx, S.internal.integ.x)                                          \
    XD(i_integy, S.internal.integ.y)                                          \
    XD(i_integz, S.internal.integ.z)                                          \
    XD(i_vhx, S.internal.last_vhat.x)                                         \
    XD(i_vhy, S.internal.last_vhat.y)                                         \
    XD(i_vhz, S.internal.last_vhat.z)                                         \
    XD(i_aoa_filtered, S.internal.aoa_filtered)                               \
    XD(i_held_bank, S.internal.held_bank)                                     \
    XB(i_deadzoned, S.internal.deadzoned)                                     \
    XB(i_push_mode, S.internal.push_mode)                                     \
    XB(i_ballistic, S.internal.ballistic)                                     \
    XD(i_roll_latch, S.internal.roll_latch)                                   \
    XD(i_elev_latch, S.internal.elev_latch)                                   \
    XD(i_hand_rest, S.internal.hand_rest)                                     \
    XD(i_ovr_rampx, S.internal.ovr_ramp.x)                                    \
    XD(i_ovr_rampy, S.internal.ovr_ramp.y)                                    \
    XD(i_ovr_rampz, S.internal.ovr_ramp.z)                                    \
    XB(i_pursuit, S.internal.pursuit)                                         \
    XB(i_any_override, S.internal.any_override)                               \
    XD(i_inv_rest, S.internal.inv_rest)                                       \
    XB(i_righting, S.internal.righting)                                       \
    XD(i_rest_time, S.internal.rest_time)                                     \
    XD(i_aim_rate_filtx, S.internal.aim_rate_filt.x)                          \
    XD(i_aim_rate_filty, S.internal.aim_rate_filt.y)                          \
    XD(i_aim_rate_filtz, S.internal.aim_rate_filt.z)                          \
    XE(i_capture, S.internal.capture)                                         \
    XD(i_cap_ux, S.internal.cap_ux)                                           \
    XD(i_cap_uy, S.internal.cap_uy)                                           \
    XD(i_cap_w_hold, S.internal.cap_w_hold)                                   \
    XB(i_cap_crossed, S.internal.cap_crossed)                                 \
    XD(i_cap_err0, S.internal.cap_err0)                                       \
    XD(i_cap_d_allow, S.internal.cap_d_allow)                                 \
    XI(i_cap_stall_ticks, S.internal.cap_stall_ticks)                         \
    XB(i_cap_refractory, S.internal.cap_refractory)                           \
    XB(i_cap_inbound, S.internal.cap_inbound)                                 \
    XD(i_cap_rim_t, S.internal.cap_rim_t)

// --- REPLAY STATE A PRE-EXISTING TAPE CANNOT CARRY -------------------------
// Written like every other state field, so a tape recorded from HERE ON seeds
// them exactly -- but NOT REQUIRED by the reader, because the tapes of Chad's
// flights that the gate replays were recorded before these columns existed and
// re-recording them is not possible: they are HIS hand, not a fixture.
//
// This is a narrow, NAMED exception to the throwing reader, and it is only
// legitimate for SHORT-MEMORY state that the recorded INPUT reconstructs:
// aim_net / aim_net_w are a hand_net_window (0.2 s) leaky integral of the aim
// rotation, and aim_dx / aim_dy ARE recorded columns -- so a seed that starts
// them at 0 re-converges within one window, and the only error is that the
// replay's first ~0.2 s scores the hand as stiller than it was. Nothing with a
// LONG memory (the rate-PI integral) or a LATCHED sign (roll_latch) may ever be
// added here: those cannot be reconstructed, a zero seed flies a different
// aeroplane, and that is the whole reason the reader throws.
#define SEADS_TAPE_OPT_FIELDS(XD, XB, XI, XE)                                 \
    XD(i_aim_netx, S.internal.aim_net.x)                                      \
    XD(i_aim_nety, S.internal.aim_net.y)                                      \
    XD(i_aim_netz, S.internal.aim_net.z)                                      \
    XD(i_aim_net_w, S.internal.aim_net_w)

// --- the AIM FRAME + the app-side state that steers the recorded deltas ----
// apply_mouse rotates about the frame's OWN axes, and rest_horizon_tick ROLLS
// that frame -- so without these the recorded mouse steers along a different
// basis and the replay diverges even from a perfect plant seed.
#define SEADS_TAPE_APP_FIELDS(XD, XB, XI, XE)                                 \
    XD(a_qw, S.aim.q.w)                                                       \
    XD(a_qx, S.aim.q.x)                                                       \
    XD(a_qy, S.aim.q.y)                                                       \
    XD(a_qz, S.aim.q.z)                                                       \
    XD(a_aim_rest, S.aim_rest)                                                \
    XB(a_recov_armed, S.recov_armed)                                          \
    XD(a_recov_remaining, S.recov.remaining)                                  \
    XD(a_recov_sign, S.recov.sign)                                            \
    XD(a_recov_w, S.recov.w)                                                  \
    XD(a_prev_pathx, S.prev_path.x)                                           \
    XD(a_prev_pathy, S.prev_path.y)                                           \
    XD(a_prev_pathz, S.prev_path.z)                                           \
    XB(a_fl_override_used, S.fl.override_used)                                \
    XB(a_fl_freelook_prev, S.fl.freelook_prev)                                \
    XD(a_fl_easeback, S.fl.easeback)                                          \
    XD(a_prev_upx, S.prev_up.x)                                               \
    XD(a_prev_upy, S.prev_up.y)                                               \
    XD(a_prev_upz, S.prev_up.z)                                               \
    XB(a_grounded, S.grounded)                                                \
    XD(a_spawn_alt, S.spawn_alt)                                              \
    XD(a_spawn_upx, S.spawn_up.x)                                             \
    XD(a_spawn_upy, S.spawn_up.y)                                             \
    XD(a_spawn_upz, S.spawn_up.z)                                             \
    XD(a_spawn_fwdx, S.spawn_fwd.x)                                           \
    XD(a_spawn_fwdy, S.spawn_fwd.y)                                           \
    XD(a_spawn_fwdz, S.spawn_fwd.z)

// Order IS the wire format. OPT sits between INT and APP; the column names,
// the writer and the seeder all walk THIS list, so the three can never
// disagree about where the new columns are.
#define SEADS_TAPE_STATE_FIELDS(XD, XB, XI, XE)                               \
    SEADS_TAPE_SIM_FIELDS(XD, XB, XI, XE)                                     \
    SEADS_TAPE_INT_FIELDS(XD, XB, XI, XE)                                     \
    SEADS_TAPE_OPT_FIELDS(XD, XB, XI, XE)                                     \
    SEADS_TAPE_APP_FIELDS(XD, XB, XI, XE)

namespace app {

// THE TRIPWIRE. These are not style checks -- each one is the bug that cost a
// night. Add a member to sim::SimState or control::Internal and this fails to
// compile until the member appears in the list above; the compiler error is
// the reminder the two silent 0.0 defaults never gave.
//
// If a size changes for a legitimate reason (padding, a reordered member),
// re-derive the number AND walk the struct against the list before bumping it.
static_assert(sizeof(sim::SimState) == 168,
              "sim::SimState changed -- add/remove the member in "
              "SEADS_TAPE_SIM_FIELDS, then update this size. A tape that does "
              "not carry every plant field seeds a DIFFERENT aeroplane "
              "(throttle/flap/gear cost a night on 2026-09-13).");
// 248 -> 280 (2026-09-16, S-tremor): + glm::dvec3 aim_net and its scalar
// normaliser aim_net_w, the windowed NET aim-displacement accumulator. It is
// REPLAY STATE (a leaky integral with a window of memory), so both are
// WRITTEN like any other internal field and carried in SEADS_TAPE_OPT_FIELDS
// above -- OPT, not INT (red-team P3, 2026-09-16: that distinction is the
// whole exception, see the rulebook comment there) -- because a mid-tape seed
// that started them at zero would score the replay's first window as a still
// hand, and because REQUIRING the columns would refuse the six S-yawbudget
// tapes of Chad's own 2026-09-13 dives, which cannot be re-recorded.
static_assert(sizeof(control::Internal) == 280,
              "control::Internal changed -- add/remove the member in "
              "SEADS_TAPE_INT_FIELDS, then update this size.");
static_assert(sizeof(input::AimFrame) == 32,
              "input::AimFrame changed -- see SEADS_TAPE_APP_FIELDS.");
static_assert(sizeof(input::HorizonRecovery) == 24,
              "input::HorizonRecovery changed -- see SEADS_TAPE_APP_FIELDS.");
static_assert(sizeof(input::Freelook) == 16,
              "input::Freelook changed -- see SEADS_TAPE_APP_FIELDS.");

// Column count of the state block, computed from the list itself.
#define SEADS_TAPE_COUNT_ONE(name, expr) +1
inline constexpr int kFeelTapeStateFieldCount =
    0 SEADS_TAPE_STATE_FIELDS(SEADS_TAPE_COUNT_ONE, SEADS_TAPE_COUNT_ONE,
                              SEADS_TAPE_COUNT_ONE, SEADS_TAPE_COUNT_ONE);

}  // namespace app
