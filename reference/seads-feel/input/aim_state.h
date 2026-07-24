#pragma once

#include <algorithm>
#include <cmath>

// The freelook half of the SPEC §9.5 input state machine — the caller side of
// "the instructor keeps flying your last commanded turn while you check six."
//
// PURE: no I/O, no clock (dt is passed in), no raylib. The pure controller
// needs NO freelook flag (SOLUTION §5.5, CLAUDE.md build order): freelook is
// entirely (a) HOLDING the world-frame aim — the caller only
// parallel-transports it each tick (SPEC §9.1), never routing the mouse into it
// — and (b) sending the mouse to the CAMERA instead. This struct owns ONLY the
// freelook latches; the caller owns the aim vector, its transport, and the
// mouse->camera routing.
//
// It is the SHARED, tested owner of the three event rules so the harness (the
// caller-of-record today) and the app (when the instructor is wired into the
// live loop) run the IDENTICAL code — never two implementations of a rule that
// then drift (CLAUDE.md 4b/H1: "route the live path THROUGH the tested
// function so the pinned code IS the shipped code"). AT-8 pins this struct.
//
// The three rules (SPEC §9.5):
//   1. Held: aim held (caller only transports it), mouse -> camera. The
//      instructor keeps flying the last commanded turn.
//   2. First override activity of a hold: aim := nose AT THAT INSTANT — the
//      parked cursor you can't see becomes a surprise the moment you seize an
//      axis. Latched, so it fires once per hold (covers a key pressed DURING
//      freelook and a key already held when freelook began).
//   3. Release: aim := nose ONLY IF an override was used during the hold — else
//      nothing changes (no stored target the pilot didn't just command;
//      snap-back is unrepresentable, CLAUDE.md Learned).
//
// Plus CQ2 (SPEC §16): after a release the camera eases back to behind the
// nose; for that <=300 ms window mouse->aim is SUSPENDED so no easing-frame
// (smoothed camera) basis ever feeds the aim — a smoothed basis in the
// mouse->aim loop is banned (§6, RA9).

namespace input {

struct Freelook {
    // An override fired at some point during the CURRENT freelook hold. Doubles
    // as the "already snapped this hold" latch (rule 2 fires once), and drives
    // the conditional reset on release (rule 3). Cleared on release.
    bool override_used = false;
    // freelook held on the PREVIOUS tick — the release edge (rule 3) fires when
    // this is true and freelook is false this tick.
    bool freelook_prev = false;
    // Remaining post-release mouse->aim suspension [s] (CQ2). Counts down each
    // MOUSE-mode tick; re-entering freelook cancels it.
    double easeback = 0.0;

    // What the caller must do with the aim THIS tick.
    struct Step {
        bool snap_to_nose;  // set aim := nose this tick (rules 2 / 3)
        bool
            mouse_aim_live;  // caller may route the mouse into the aim iff true
        bool released = false;  // the release edge fired THIS tick (S7-hrz: the
                                // horizon-recovery capture point, after rule 3)
    };

    // Advance the latches one tick. `freelook` / `any_override` = the keys held
    // THIS tick (any_override = OR of the per-axis override mask). `dt`,
    // `easeback_time` in seconds. Call every tick, in every mode: with freelook
    // never engaged it is a pure no-op on the aim (snap_to_nose stays false and
    // mouse_aim_live stays true), so the pure-instructor flight is untouched —
    // freelook is a strict superset, exactly as the override mask is.
    Step step(bool freelook, bool any_override, double dt,
              double easeback_time) {
        Step r{false, false};
        if (freelook) {
            // Rule 1 + rule 2. Re-entering freelook cancels a pending ease-back
            // (the mouse is back on the camera; there is nothing to ease into).
            easeback = 0.0;
            if (any_override && !override_used) {
                r.snap_to_nose = true;  // rule 2, once per hold
            }
            override_used = override_used || any_override;
            // mouse -> camera; aim held. mouse_aim_live stays false.
        } else {
            if (freelook_prev) {                           // the release edge
                if (override_used) r.snap_to_nose = true;  // rule 3
                override_used = false;
                easeback = easeback_time;  // begin the CQ2 suspension
                r.released = true;         // S7-hrz capture point
            }
            // CQ2: suspend mouse->aim across the ease-back window. Decrementing
            // AFTER arming means the release tick itself is already suspended.
            if (easeback > 0.0) {
                easeback = std::max(0.0, easeback - dt);
                r.mouse_aim_live = false;
            } else {
                r.mouse_aim_live = true;  // normal MOUSE mode
            }
        }
        freelook_prev = freelook;
        return r;
    }

    // Focus loss / crash-reset (SPEC §9.5 robustness): drop every freelook
    // latch. The caller pairs this with aim := nose and control::reset() — an
    // alt-tab mid-freelook must not resume a stale hold, and a respawn must
    // never inherit the previous life's freelook state.
    void reset() { *this = Freelook{}; }
};

// Horizon recovery (S7-hrz, docs/horizon_recovery_plan.md): the OPEN-LOOP
// fixed-angle roll latch that rights the carried aim/camera frame's horizon
// after a freelook release. PURE caller-side state, beside Freelook so the
// same callers own both reset legs.
//
// The contract (red-teamed, plan section 10 — F1 is the P0 this shape kills):
// the roll angle and sign are CAPTURED ONCE at the release edge
// (AimFrame::up_misalignment) and counted down per tick with the D3 profile
// `w = min(rate, settle * remaining)` — a quick uniform roll easing into the
// end. NEVER recompute the target from local_up per tick: a recomputed target
// moves with the mouse-driven forward (the banned control-driving-quaternion
// loop) and flips across the zenith. A pilot who maneuvers mid-roll therefore
// ends NEAR upright, not exactly — accepted; the next freelook tap trims it.
//
// Termination is structural (F2): the last step takes the whole remainder once
// it falls under kFinishEps (~0.57 deg, imperceptible in one tick), so
// `remaining` reaches EXACTLY 0.0 and the inactive path costs no quaternion
// math (F8 — a zero-angle angleAxis through normalize can still perturb LSBs).
struct HorizonRecovery {
    double remaining = 0.0;  // [rad] captured angle left to roll; 0 = inactive
    double sign = 0.0;       // +/-1 while active

    // Terminal snap threshold [rad]: a degeneracy/termination guard (code, not
    // tune data — the exponential D3 tail never reaches 0 on its own).
    static constexpr double kFinishEps = 0.01;

    // Capture at the release edge (the ONLY writer besides reset/step). A
    // sub-threshold misalignment goes (or stays) INACTIVE — an aligned release
    // is a structural no-op, no deadband knob needed (plan D5). The reset() on
    // the small branch is deliberate (diff red-team P3-1): capture() must
    // never leave a STALE prior latch alive — unreachable via today's callers
    // (every release edge is preceded by a canceling held tick), but the
    // invariant is load-bearing for any future caller (§5b nesting).
    void capture(double signed_angle) {
        const double a = std::abs(signed_angle);
        if (a <= kFinishEps) {
            reset();
            return;
        }
        remaining = a;
        sign = signed_angle > 0.0 ? 1.0 : -1.0;
    }

    // One tick of the D3 profile; returns the signed roll delta to apply this
    // tick (0.0 exactly while inactive — the caller skips the frame math).
    double step(double dt, double rate, double settle) {
        if (!(remaining > 0.0)) return 0.0;
        const double w = std::min(rate, settle * remaining);
        double d = w * dt;
        if (remaining - d <= kFinishEps) d = remaining;  // terminal snap
        remaining -= d;
        const double out = sign * d;
        if (!(remaining > 0.0)) reset();  // reaches EXACTLY 0 (F2)
        return out;
    }

    // Cancel (D9 / re-press / focus loss / GROUNDED): drop the latch.
    void reset() { *this = HorizonRecovery{}; }
};

// Orient double-tap (S-orient, docs/comfort program Q3): the pure detector for
// the ORIENT verb — "double-tap Space to put me back together." A DISCRETE,
// player-commanded composed event: on the SECOND freelook press-edge that
// arrives within `window_s` of the previous press-edge, fire ONCE. The caller
// then (a) snaps aim := guarded velocity, (b) captures the S7-hrz up-debt roll,
// and (c) hard-cuts the lagged camera-forward — all sanctioned discrete moves.
//
// Follows input::Freelook's style: PURE, no clock (dt is passed in), dt
// accumulation only. Feed the freelook PRESS-EDGE (a rising edge of the
// freelook-held key) each tick. `window_s = 0` structurally NEVER fires (the
// strict-superset off-switch, the S7-hrz `rate = 0` pattern): the whole
// detector short-circuits before touching any state, so with it off the tick is
// bit-identical.
//
// Semantics (red-team-aware — the "three fast taps fire once" trap): a fire
// CONSUMES the pairing (`since` reset to a large sentinel), so a third rapid
// press after a fire opens a FRESH first tap rather than double-firing. Only an
// alternating press-after-press within window fires.
struct OrientTap {
    // Seconds since the last press-edge that is still eligible to pair. Starts
    // large (no eligible prior tap). Capped so it can't grow without bound.
    double since = 1e9;

    // Advance one tick. `pressed_edge` = the freelook key went DOWN this tick
    // (a rising edge the caller builds from the same device sample freelook
    // reads). `dt`, `window_s` in seconds. Returns true on the tick a valid
    // double-tap completes (fire-once). window_s <= 0 => never fires.
    bool step(bool pressed_edge, double dt, double window_s) {
        if (!(window_s > 0.0)) return false;  // off-switch: structural no-op
        // Age the pending tap; clamp so `since` stays a bounded small number.
        since = std::min(since + dt, 1e9);
        if (!pressed_edge) return false;
        // A press-edge: does it pair with a prior press still inside the
        // window?
        const bool fire = since <= window_s;
        // Consume on fire (so a third fast press opens a fresh first tap, not a
        // second double-fire); otherwise this press BECOMES the new first tap.
        since = fire ? 1e9 : 0.0;
        return fire;
    }

    // Focus loss / crash-reset / GROUNDED: forget any pending tap so an alt-tab
    // or respawn can never complete a stale double-tap.
    void reset() { *this = OrientTap{}; }
};

}  // namespace input
