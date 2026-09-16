#pragma once

// Controller gains + thresholds (SPEC §9, §13, Appendix). Data, not code:
// the single source is config/controller.toml — no bare numeric gain ever
// lives in control/ or sim/ (CLAUDE.md Conventions). Angles/rates are stored
// in RADIANS here; the toml carries degrees at the config boundary and the
// loader converts once (SPEC: "degrees only at config/UI boundary").
//
// Fields default to zero on purpose (same discipline as AircraftParams): an
// unloaded struct must fail loudly, never fly plausibly. The loader
// cross-checks against the airframe (AoA_max <= plant stall alpha; the
// critical-damping rule K_w >= 4*I*K_theta; the ZOH ceiling K_w*dt/I <= 0.5)
// so a table that would oscillate by construction cannot load.

namespace control {

struct ControllerParams {
    // Outer-loop proportional gains (braking law linear regime, SPEC §9.3).
    double K_theta = 0.0;  // [1/s] pitch+yaw pointing (shared, per SOLUTION)
    double K_phi = 0.0;    // [1/s] roll (bank-to-turn and wings-hold)

    // Braking law margin (SPEC §9.3): alpha_brake = k_b * alpha_max(q), with
    // alpha_max the per-axis peak angular accel DERIVED from the shared
    // aircraft params (sim::ang_accel_max_derived) — measured == derived is
    // AT-18a, so the braking law reads the airframe's true authority.
    double k_b = 0.0;

    // Rate clamps (SPEC §9.3, Appendix). Pitch is clamped by the G-limits
    // below, not a fixed rate; yaw/roll carry explicit ceilings.
    double yaw_max = 0.0;    // [rad/s]
    double p_max = 0.0;      // [rad/s] roll rate ceiling
    double omega_bal = 0.0;  // [rad/s] BALLISTIC attitude-hold rate clamp

    // Inner-loop rate-PI (SPEC §9.4), per axis (x=pitch, y=yaw, z=roll).
    double K_w_pitch = 0.0, K_w_yaw = 0.0, K_w_roll = 0.0;  // [N m s]
    double K_wi_pitch = 0.0, K_wi_yaw = 0.0, K_wi_roll = 0.0;
    double integ_cap = 0.0;  // [rad s] anti-windup clamp on the error integral
    // S-dampff (SPEC §0, 2026-07-11): damping feedforward — completes the
    // plant inversion. tau_cmd gains + damp_ff * damp_axis * q_eff * omega_des
    // so the integrator no longer sources the plant's damping torque during a
    // sustained rotation (the wound integral was carrying the nose ~1 deg
    // PAST the aim at combat V, draining at tau = K_w/K_wi). Per axis, 0 =
    // structurally OFF (bit-identical legacy — the term is gated, never
    // multiplied by 0), 1 = full inversion. The default 0.0 keeps raw
    // aggregate construction in old tests on the legacy path (strict
    // superset, the 4c discipline).
    double damp_ff_pitch = 0.0, damp_ff_yaw = 0.0, damp_ff_roll = 0.0;

    // S-aimff (v4 rung 1): aim-rate feedforward — omega_des(pitch, yaw) +=
    // aim_ff_gain * (the aim's own mouse-induced angular velocity, body
    // frame), so the nose LEADS a moving aim instead of chasing its error.
    // Acts only while the aim moves; steady state untouched (feedforward adds
    // no error-loop gain). 0 = structurally OFF (the term is gated, never
    // multiplied by 0 — bit-identical legacy, the S-dampff discipline).
    // aim_ff_tau low-passes the DERIVED rate at fixed sim_dt (0 = exact
    // pass-through); the raw mouse->aim path is untouched either way (§9.1).
    double aim_ff_gain = 0.0;
    double aim_ff_tau = 0.0;  // [s]

    // S-rimshot v3 "POOL BALL" (v4 rung 2, UNIVERSAL): the FULL-TRAVERSE
    // REBOUND capture — Chad's spec ("goes THROUGH the centre aim, past the
    // centre only so much as it glances the centre of the inside of the
    // rim... then like a dead blow hammer snaps straight to the middle —
    // immediate, no ease in. Direct. Predictably the same every time") +
    // the 2026-07-17 universality correction (ANY deflection, ALWAYS,
    // mid-track included; no arm threshold, no park gate) + the four v3
    // rulings (Q1 full speed through center; Q2 the glance lands DEAD ON
    // the wall; Q3 gentle arrivals DIRECT-SEEK, briskly — the lateral
    // "one attempt then regroup" is dead; Q4 the return is uncapped).
    // ENGAGE fires whenever the nose is genuinely ARRIVING (the seek taper
    // binds AND the pointed demand dips below engage_frac x the net
    // closing rate), then forks on the projected glance depth
    // w_rel^2/(2*alpha_u) vs glance_frac x circle: glance-worthy -> CARRY
    // holds the incoming net rate at full speed through the crossing until
    // the PREDICTIVE BRAKE SURFACE (remaining-to-rim <= the live stopping
    // plan) hands to RETURN, whose rim-targeted servo-fed alpha_req brake
    // lands the apex ON the wall (rim_frac x circle); sub-glance ->
    // DIRECT-SEEK enters RETURN at once. RETURN home: the sqrt(2*alpha*d)
    // ceiling (return_w effectively uncapped) + the mirrored alpha_req
    // brake into center, completing on the predicted-landing dead-blow
    // surface (|d - servo coast| inside the park band) — no free-coast
    // contamination, no linear floor under the sqrt (a rate ~ d branch is
    // exactly the taper creep the event exists to kill). Roll untouched;
    // the AoA/G clamp sequence and the shared yaw_max ceiling bound every
    // event demand (protection never bypassed). One bounce per ARRIVAL
    // (refractory latch on the completion exits only); a re-flick
    // mid-event hands back to the normal law that tick (error-domain
    // hand-back pre-crossing, apex-allowance yank exits after — Chad's
    // ruling: "abandon the bounce, chase instantly"); apex-death and the
    // geometry-break abandon DIRECT-SEEK instead of regrouping.
    // carry = 0 is the STRUCTURAL OFF: the machine never engages and every
    // touched expression keeps the bit-identical legacy tree (the S-hrz
    // rate = 0 pattern).
    double capture_carry = 0.0;     // [-] fraction of the incoming net rate
                                    //     held through the crossing (0 = OFF)
    double capture_rim_frac = 0.0;  // [0..1] far-rim detect at this fraction
                                    //     of the circle ("nearly touches")
    double capture_circle = 0.0;    // [rad] on-screen aim-circle angular
                                    //     radius at 1x zoom (tune data — see
                                    //     controller.toml for the derivation)
    double capture_return_w = 0.0;  // [rad/s] phase-2 return rate CAP (the
                                    //     arrest itself is the full-authority
                                    //     sqrt(2*alpha*d) into center)
    double capture_engage_frac = 0.0;    // [-] ENGAGE when pointed < this x
                                         //     w_rel (in (0,1); the closed-loop
                                         //     decay ratio floor is ~0.86 pitch
                                         //     / 0.69 yaw — below those the
                                         //     trigger is stillborn)
    double capture_handback_frac = 0.0;  // [-] pre-crossing hand-back when
                                         //     |demand| grows past this x
                                         //     |demand at engage| (> 1)
    double capture_break_frac = 0.0;     // [-] yank exits when |demand| exceeds
                                      //     this x the event's apex allowance
                                      //     (> 1: at/below 1 the exit kills
                                      //     the event at its own honest apex)
    double capture_w_eps = 0.0;  // [rad/s] net-closing-rate floor to engage
                                 //     (above the coordination
                                 //     demand-vs-achieved noise ~0.27 deg/s)
    // S-rimshot v3 (POOL BALL): arrivals whose crossing flourish
    // w_rel^2/(2*alpha_u) — the projected glance depth past center at full
    // trackable authority — is under glance_frac x circle skip the glance
    // and DIRECT-SEEK center (RETURN entered at ENGAGE: an active
    // full-authority sqrt capture, never the legacy taper creep — Chad's Q3
    // "brisk" ruling, universal for lateral/banked arrivals too).
    double capture_glance_frac = 0.0;  // [-] (0,1] of the circle
    // Kernel v5 rung A, S-truedepth (docs/v5_kernel_handoff.md): the glance
    // depth an event EARNS is its own stopping distance under full
    // trackable braking, min'd with the wall — rim_live = (depth_frac > 0)
    // ? min(rim_frac*circle, depth_frac*glance) : rim_frac*circle. 1.0 =
    // pure physics (a slow ball does not reach the far cushion); <= 0 =
    // v4 fixed-rim OFF (the fly fallback, always rim_frac*circle); > 1 =
    // deeper glance per unit momentum, still wall-capped.
    double capture_depth_frac = 0.0;
    // Rung A2: the sub-wall curve exponent on the saturation s = min(1,
    // depth_frac*glance/rim) — rim_live = rim * s^pow. 1.0 = rung A
    // bit-identically (the knob-off arm); > 1 presses SMALL events
    // quadratically shallower while s = 1 (wall-earning arrivals) is a
    // fixed point at every pow — the committed glance never moves.
    double capture_depth_pow = 0.0;

    // G-limit clamps (SPEC §9.3; feel proxy, calibrated by AT-6). Pitch-rate
    // budget = (n - cosPhiTheta)*g/V with V floored by v_min.
    double n_max = 0.0;
    double n_min = 0.0;

    // AoA protection (SPEC §9.3b). aoa_max <= plant stall alpha (asserted at
    // load). The filtered AoA (sole smoothing exception) uses aoa_filter_tau.
    double aoa_max = 0.0;         // [rad] positive-AoA limit
    double aoa_max_neg = 0.0;     // [rad] inverted-stall limit (magnitude)
    double K_aoa = 0.0;           // [1/s] pushback gain
    double aoa_filter_tau = 0.0;  // [s] low-pass time constant

    // Deadzone (holds trim, SPEC §9.3), hysteretic. Also the reticle CIRCLE
    // (§7): inside it the pointing is zeroed (no direction change on-target);
    // the seek law below snaps the nose to its edge and parks there.
    double deadzone_lo = 0.0;  // [rad] enter (pointing term -> 0)
    double deadzone_hi = 0.0;  // [rad] re-arm
    // Aim-motion gate (rudder-flick Fly 6, SPEC §9.3 as amended 2026-07-10):
    // the deadzone may LATCH only after the hand has been at rest for this
    // dwell, and an aim-moved tick UNLATCHES it instantly — slow zoomed
    // tracking becomes continuous (the deadzone relaxation-cycle "stairs"
    // are unrepresentable while the hand moves) while the at-rest trim hold
    // is untouched. 0 = LEGACY: aim_moved ignored entirely, latch on err
    // alone — the bit-identity knob-off.
    double deadzone_rest_dwell = 0.0;  // [s] hand-at-rest dwell to latch

    // Nose seek shaping (§7 Chad 2026-07-06): the pitch pursuit uses seek_law
    // instead of the plain braking law so the nose SNAPS to the reticle instead
    // of creeping in on the vanishing linear gain. pursuit_step is a rate FLOOR
    // (the step up out of the circle); pursuit_expo is the parabolic rise
    // (gain grows with deflection). Both 0 recovers the plain sqrt_law.
    double pursuit_step =
        0.0;  // [rad/s] near-zero pursuit rate floor (the snap)
    double pursuit_expo = 0.0;  // [1/rad] parabolic gain rise with deflection

    // Smooth wings auto-level at rest (§7 Item-2 Part B): in the deadzone
    // (mouse at rest) and upright, the held-bank wings-hold setpoint DECAYS
    // toward 0 at this exponential rate and the wings follow it to level.
    // Starts from the captured bank (no spawn lurch, AT-13); suspended by
    // override, off inverted.
    double auto_level_rate = 0.0;  // [1/s] exponential decay of held_bank -> 0
    // Inverted auto-righting (MB-right, Chad 2026-07-07: "roll over on bank
    // after about 2 s no gross inputs if belly up... slow roll off ailerons").
    // Scoped amendment of S7-loop-invert: an inverted REST still stays
    // inverted, but only for inverted_delay — then a SLOW aileron roll
    // (rate-clamped to inverted_rate) rights the plane, handing off to the
    // normal wings-leveling at the knife-edge band. inverted_rate = 0
    // disables the mechanism STRUCTURALLY (the latch never arms — knob-off
    // strict-superset arm).
    double inverted_delay = 0.0;  // [s] rest-while-belly-up time before arming
    double inverted_rate = 0.0;   // [rad/s] righting roll clamp (0 = OFF)
    // MB-lean (Chad 2026-07-08): in FINE the held_bank setpoint decays toward
    // clamp(lean_gain * de-rolled_lateral_azimuth, +/-lean_max) instead of
    // level — the nose is pulled onto the aim by a shallow proportional bank
    // (the "magnet"), and a moderate deflection holds a shallow bank instead
    // of the all-or-nothing ~90 deg commit. Centered aim => target 0 (level
    // at rest unchanged). lean_gain = 0 is the bit-identical knob-off arm.
    double lean_gain = 0.0;  // [rad bank per rad lateral error] 0 = off
    double lean_max = 0.0;   // [rad] lean cap (Chad ruling ~30 deg)
    // S-leanlead (feel/yaw-bank-balance 2026-09-10, Chad: the turn entry
    // should be "a near even balance of yaw and bank" from the first
    // mouse deflection -- measured: the FINE bank arrives through TWO
    // series lags (held_bank chases the lean at auto_level_rate, then
    // K_phi chases held_bank) while the rudder pointing is one lag, so the
    // nose crabs first and the bank trails). The wings-hold roll limb
    // chases held_bank + lean_lead*d, d = the live lean's SAME-SIGN excess
    // over held_bank (continuous, no threshold; controller.cpp) -- the
    // first lag is bypassed on the way IN only. The release
    // (lean_target -> 0), the rest auto-level, the banked-spawn capture
    // (AT-13) and the MB-right righting all see lean_target*d <= 0 and
    // keep the flown decay bit-identically. 0 = structurally OFF (the
    // bit-identical legacy tree); 1 = the roll limb targets the live lean.
    double lean_lead = 0.0;  // [0..1] 0 = off
    // S-leanlead-lateral (2026-09-12, the walk-back's structural fix). The
    // lead above reads lean_target = lean_gain * az_lat, and az_lat is the
    // aim's offset along the HORIZON's right axis while the mouse deflects in
    // the CARRIED (screen) frame. For an aim offset of eps purely along body
    // up -- a pure PITCH input -- the de-roll numerator collapses to
    // sin(eps)*sin(phi), i.e. az_lat ~= eps*sin(phi): nonzero whenever the
    // wings are off level, SAME sign as phi nose-up (a divergent feedback --
    // wings-level is unstable for eps > 1/lean_gain) and OPPOSITE nose-down
    // (a ringing over-corrector). Chad flew v14 and reported exactly that
    // pair -- "nosing down often makes my wings bank when unwanted... pulling
    // a straight loop my wings are completely unwantedly banking over 180
    // degrees". The contamination is MB-lean's own and PRE-DATES the lead;
    // what lean_lead did was bypass the auto_level_rate lag that had been
    // low-passing it, raising that path's loop gain from ~0 (two-pole
    // rolloff) to K_phi*lean_lead*lean_gain.
    //
    // The fix scales the LEAD (never lean_target itself -- the flown MB-lean
    // magnet is untouched) by the aim offset's HORIZON-LATERAL SHARE:
    //     g = smoothstep(lat_lo, lat_hi, |az_lat| / err)
    // share == |sin(phi)| for a pure-pitch aim (bank-dependent, small at the
    // shallow banks where the runaway seeds) and == 1 for a genuinely
    // sideways aim at ANY bank -- so it discriminates the turn entry the dial
    // was built for from the loop/dive contamination. |az_lat| ALONE does not
    // discriminate: it is nonzero for pure pitch too, which is the bug.
    // lean_lead_lateral = false is the structural OFF arm (the landed v14
    // expression tree, bit-identical); lean_lead = 0 is bit-identical
    // regardless of either edge.
    bool lean_lead_lateral = false;     // false = landed-v14 ungated lead
    double lean_lead_lat_lo = 0.0;      // [0..1] share where the lead starts
    double lean_lead_lat_hi = 1.0;      // [0..1] share where it is full

    // Regime blend/latch (SPEC §9.3), hysteretic. blend is smoothstep(lo,hi,e).
    double blend_lo = 0.0;  // [rad] FINE entry / blend start
    double blend_hi = 0.0;  // [rad] MANEUVER exit / blend end
    // Bank-to-turn align power (S7-turn2): the up-elevator is scaled by
    // cos(bankErr)^bank_align_power, so a higher power holds the pull until the
    // roll aligns (roll leads, less climb / pitch-chase in a lateral turn).
    double bank_align_power = 1.0;
    // Kernel v5 rung C2b (S-holdline sag servo, docs/v5_kernel_handoff.md
    // "RUNG C"): the align gate above may fade the pull while the nose is
    // still rolling into alignment; once gravity sags the nose BELOW the
    // aim's world-elevation line, a dedicated proportional servo on the sag
    // (through the same sqrt_law braking-law family, K_theta/aB_pitch/
    // w_max_pitch) keeps fighting regardless of alignment, zero at zero sag
    // (continuous -- no floor-slingshot momentum). pull_floor scales the
    // servo demand: 1.0 = the full K_theta line-hold, 0.0 = the bit-
    // identical OFF arm (the fly fallback).
    double pull_floor = 0.0;  // [frac of the sag-servo demand], 0 = off
    // S-righthand (feel/lean-lead-walkback, 2026-09-12) -- MB-right must not
    // right the aeroplane while the pilot's hand is still flying it.
    //
    // MEASURED FROM CHAD'S OWN TAPE (10134 ticks; his loop apex at t=18.02):
    // the nose passes vertical, cos_phi_theta crosses zero, and MB-right arms
    // -- its "at rest" test is err < blend_lo and his tracking err there is
    // 4.6 deg, inside the circle. inverted_delay is 0.0 (ruled 2026-08-06), so
    // it arms on the FIRST such tick and commands EXACTLY inverted_rate: the
    // tape shows wdz = -180.0 deg/s held through the apex. He is not resting
    // inverted; he is mid-loop, nose 86 deg up, mouse still moving.
    //
    // Chad, verbatim: "the loop is sustained by me sustaining the motion, if I
    // change the motion it should change the behavior." So the righting
    // AUTHORITY is scaled by a HAND-REST ramp: zero while the hand is moving
    // the aim, rising to full over right_hand_rest seconds of rest.
    // CONTINUOUS (a smoothstep of a time integral), not a veto latch -- no new
    // threshold to chatter, and a hand that pauses mid-loop gets a
    // proportional amount of righting rather than a step.
    //
    // The 2026-08-06 ruling ("inverted righting carries NO added delay -- as
    // soon as the rest condition is met") is PRESERVED: hands off at the apex
    // still rights, after this short hand-rest; inverted_delay and
    // inverted_rate are UNTOUCHED. The rest signal is the same CQ2-gated
    // aim_moved the deadzone latch uses, so freelook (mouse on the camera)
    // correctly reads as hand-off-the-aim.
    //
    // Chad, flying it: "I can do the vertical loops and immelmans without a
    // hitch." 0.0 = the STRUCTURAL OFF arm (bit-identical legacy tree).
    // DEBT, measured, deferred (red-team P2 -- the TREMOR case, same class as
    // the P3-a note above). "The hand is live" is any nonzero aim motion, so a
    // +/-1-count-per-frame mouse tremor while belly-up keeps resetting the
    // clock and caps the gate: integrated righting 1.76 deg against 117.75
    // with a still hand. That is the 2026-08-06 resting case with a drifting
    // hand, and it reads as "it will not right me". The cure is a WINDOWED NET
    // aim displacement (a tremor nets to ~0, a real sweep does not) rather
    // than an any-motion test -- a mechanism change, not a dial, so it is
    // deferred rather than bodged with a deadband here.
    double right_hand_rest = 0.0;  // [s] hand-at-rest before MB-right has
                                   //     full authority; 0 = off

    // Wings-leveling fade band (S7-loop-invert): the wings-leveling roll fades
    // over smoothstep(-band, +band, cos_phi_theta) — 1 upright (bit-identical
    // golden), 0 inverted, smooth across the ~90 deg knife-edge (no chatter).
    double wings_level_band = 0.0;  // [cos units]
    // S-lapguard 2026-09-12: aim-laps-the-nose guard, measured inert on both of Chad's tapes (0 ticks), cost 272 m on a 300 deg/s lateral sweep; removed like S-aimclamp/S-retclamp.
    // (The loop-rollover defect it targeted is real; the CURE was wrong --
    // it never fired on his hand. The apex roll he actually felt is
    // right-hand dial below.)
    // Blend-band roll TARGET continuity (the 5-10 deg roll slam, 2026-07-30 —
    // coverage-completion of MB-lean): inside the blend band the MANEUVER
    // roll limb chases the mixed bank target blend*phi_commit +
    // (1-blend)*lean_target instead of the raw bank_error commit, so the two
    // roll limbs AGREE in target as blend -> 0 and the boundary tug-of-war
    // (frozen ~30 deg lean vs 60-90 deg commit, full-aileron reversals at
    // err ~ 5 deg) collapses. 0 = the structural bit-identical legacy
    // two-target blend (the fly kill-switch); 1 = the full mix.
    double roll_target_mix = 0.0;
    // S-straightline (2026-07-30, docs/straightline_thread.md — Chad's spec:
    // "the elevator increase should smoothly coorelate to banking increase"):
    // AXIS-CORRECTION pitch FEEDFORWARD. The attributed dip mechanism is the
    // crab's vertical component (yaw about the BANKED body-up digs the nose
    // at sin(phi) of the sweep); the FF cancels the emitted yaw's PARASITIC
    // vertical share — motion AWAY from the aim's elevation line only:
    // w_axis = -(w_dn*min(yv,0) + w_up*max(yv,0))/cosPhiTheta with
    // yv = emitted_yaw*sin(e.phi) (the MB-lean frame-true pair) and one-
    // sided sag-band fades w_dn/w_up — gated blend * fwd_gate * knife_fade,
    // keyed on the EMITTED yaw (never a faded/blended-away demand). The sag
    // servo above stays as the residual-error backstop
    // (feedback-plus-feedforward). Scales the FF: 1.0 = the exact kinematic
    // complement, 0.0 = STRUCTURAL OFF (bit-identical v11 tree — the fly
    // kill-switch and golden baseline arm).
    double line_hold_ff = 0.0;  // [frac of the axis correction], 0 = off

    // Push-vs-roll gate (SPEC §9.3, S7-push), every leg hysteretic. Decided on
    // GEOMETRY (target fore/aft), not the -G budget: push (nose down) while the
    // below-nose target is ahead of / just past the wing-line, roll (loop) once
    // it goes behind. z = target_body.z threshold = -cos(down-angle); z_enter <
    // z_exit gives the hysteresis band just past straight-down (~100-110 deg).
    double push_gate_bank = 0.0;     // [rad] enter above (|bankErr|)
    double push_gate_bank_lo = 0.0;  // [rad] exit below
    double push_down_z_enter = 0.0;  // enter push at/below this target_body.z
    double push_down_z_exit = 0.0;  // exit push (roll) above this target_body.z
    // World-horizon gate (§7 Item 2): push (nose-down chase) ONLY when the aim
    // is genuinely below the WORLD horizon (dot(aim, local_up) below enter), so
    // a LATERAL turn-reversal (aim near the horizon, appearing body-below only
    // because the plane is banked) rolls to the aim instead of pushing its
    // belly at it. Bank-INDEPENDENT, unlike push_down_z (body frame). enter <
    // exit (hysteresis). Chad's rule: "never chase by elevator-down unless the
    // mouse calls for nose-down".
    double push_horizon_enter = 0.0;  // enter push below this aim world-elev
    double push_horizon_exit = 0.0;   // exit push above this aim world-elev
    // Sideways cone (§7 Item 2 refinement): how far the aim is OUT of the
    // plane's VERTICAL plane (spanned by nose + local_up) — |sin| of the
    // out-of-plane angle. Push (pure pitch-down, no rollover) engages ONLY when
    // the aim is within a narrow cone of local-down (small sideways); a
    // down-AND-to-the-side flick ("left and a bit down") sits OUTSIDE the cone
    // and ROLLS over to track instead. Reference is LOCAL-up, not the banked
    // body. enter < exit (hysteresis).
    double push_side_enter = 0.0;  // enter push below this |aim sideways|
    double push_side_exit = 0.0;   // exit push above this |aim sideways|
    // Rung F "THE SACRED MIDDLE" (v5 kernel-v5-reconcile, Chad 2026-07-24):
    // rung E's 45-deg horizon_enter blankets EVERYTHING below the horizon, so
    // a shallow straight-ahead dive (20-40 deg down, IN-PLANE) could no
    // longer pure-pitch -- bank-to-turn rolled him over. His ruling, both
    // halves preserved: "when I nose straight down I need a wider knife
    // edge... pitch straight down without tipping over... maintain my
    // horizon because I will slowly pitch up from the dive in that same
    // direction" (this arm) AND the standing rung-E ruling that a
    // lateral/bank-over nose-down still needs 45+ deg down (untouched --
    // push_horizon_enter/exit above). This is a SECOND, narrower entry arm,
    // ORed with the horizon leg: tightly in-plane (within side_pure_enter of
    // the vertical plane) AND below the horizon at ANY depth. enter < exit
    // (hysteresis) and side_pure_enter < push_side_enter (the sacred middle
    // is a subset of the wider side cone that still gates every entry).
    double push_side_pure_enter = 0.0;  // enter (sacred middle) below this
                                        // |aim sideways|, any depth<0
    double push_side_pure_exit = 0.0;   // exit (sacred middle) above this

    // Direction latches (SPEC §9.3), hysteretic.
    double roll_latch_on = 0.0;   // [rad] latch roll dir above |bankErr|
    double roll_latch_off = 0.0;  // [rad] unlatch below
    double astern_on = 0.0;       // [rad] latch elev sign above |e|
    double astern_off = 0.0;      // [rad] unlatch below (hysteresis)

    // Speed floors (SPEC §9.6, Appendix). v_dir_eps is an AIRCRAFT param
    // (the plant guards its own v-hat) — the controller reads it from there.
    double v_min = 0.0;             // [m/s] every G-clamp V-division floor
    double v_ballistic = 0.0;       // [m/s] BALLISTIC entry
    double v_ballistic_exit = 0.0;  // [m/s] BALLISTIC exit (hysteretic, ~1.3x)

    // Yaw coordination (SPEC §9.3), lives outside the pointing gate.
    double K_coord = 0.0;  // [1/s] null-sideslip gain
    // S-yaw-magnet (SPEC §0, Chad ruling 2026-07-10): the anti-crab
    // coordination FADES near center — scale = center_frac at err = 0 rising
    // (smoothstep) to 1 by center_band — so the rudder's own pointing
    // finishes into the aim circle instead of parking at the
    // pointing-vs-coordination equilibrium (~0.076°, just outside the
    // deadzone; yaw_scale and global K_coord are both AT-16-walled). The
    // sustained banked turn (err >> band) keeps FULL coordination — AT-16
    // untouched by construction. center_frac = 1 is the knob-off
    // bit-identity (band unread). Yaw oscillation damping is K_w_yaw, not
    // this term — the relief removes a steady standoff, not the damper.
    double coord_center_frac = 1.0;  // [0..1] coordination floor at err = 0
    double coord_center_band =
        0.0;  // [rad] err where full coordination returns
    // Rudder pointing GAIN scale (MB-rud: applied INSIDE sqrt_law's gain slot,
    // K_theta*yaw_scale, so yaw_max is a true shared ceiling with the keyboard
    // override — requirement "highest deflection ALWAYS keyboard" holds by
    // construction on the yaw pointing path).
    double yaw_scale = 0.0;
    // Bank-aligned rudder gate (MB-rud; supersedes S7-yaw's blend fade
    // yaw_maneuver_frac). Yaw pointing in the bank-to-turn branch is scaled by
    //   (1-blend) + blend*yaw_gate,   yaw_gate = yaw_min_frac +
    //   (1-yaw_min_frac)*smoothstep(-yaw_align_band, 0, cos(bankErr))
    // Full rudder while |bankErr| <= 90 deg — the nose CRABS onto the aim
    // immediately AND through the whole roll-in (Chad's Q1b ruling) — fading
    // to yaw_min_frac only past knife-edge, which is where the split-S
    // corkscrew lives (AT-15). cos is EVEN in bankErr, so the gate is immune
    // to roll_latch sign flips and the +/-180 wrap; the blend composition
    // (mirroring the pitch align gate) keeps it continuous across the
    // FINE->MANEUVER boundary (plan red-team P0-1).
    double yaw_align_band = 0.0;  // [cos units] fade band below knife-edge
    double yaw_min_frac = 0.0;    // [0..1] rudder floor past knife-edge

    // Keyboard override engage ramp (SPEC §9.5 / §9.4): a held axis ramps to
    // full deflection over this time (WT-style short ramp — a hard step feels
    // twitchier than the reference). In-core state (internal.ovr_ramp), so the
    // 80 ms transition ports with the pure core and is covered by AT-7.
    double ovr_ramp_time = 0.0;  // [s]

    // Freelook ease-back (SPEC §9.5 / §16 CQ2): after a freelook release,
    // mouse->aim is suspended for this long so a smoothed easing-camera basis
    // can never leak into the aim (CQ2, banned by §6). Caller-side state only
    // (input::Freelook) — the pure controller never sees it — but it lives
    // here so it is tune data, not a bare number in code. Loader caps it at
    // 0.30 s (CQ2's "<=300 ms").
    double freelook_easeback_time = 0.0;  // [s]

    // Orient double-tap window (S-orient, docs/comfort program Q3): the ORIENT
    // verb fires when a SECOND freelook press-edge lands within this window of
    // the first. Caller-side like freelook_easeback_time (the pure controller
    // never sees it — the orient event composes aim snap + S7-hrz capture +
    // camera cut, all caller-side) — here as tune data, not a bare number in
    // code. 0 DISABLES the mechanism structurally (input::OrientTap never
    // fires), so default-off is a bit-identical strict superset.
    double orient_double_tap_s = 0.0;  // [s] max gap between the two taps

    // Release-orient (S-relorient, Chad 2026-07-28 "should be automatic upon
    // release of the spacebar"): EVERY freelook release fires the ORIENT verb
    // (aim := guarded velocity + camera hard-cut), not just the double-tap.
    // Caller-side like orient_double_tap_s. Read OPTIONAL by the loader with
    // this default, so an untouched toml is bit-identical legacy.
    bool freelook_release_orient = false;

    // S-relorient ADDENDUM (Chad 2026-07-28, flying the sealed v6: "anytime my
    // finger isn't pressing freelook, I am in chase camera directly behind and
    // using mouse aim — even if still turning and pressing hard keys for
    // control surfaces"). RETIRES the D9 exception: with this true, an override
    // key held at the release instant no longer suppresses the orient verb, the
    // S7-hrz up-debt capture, or the double-tap. The finger leaving freelook is
    // the whole trigger. Optional-with-default-false like the field above, so a
    // toml without the key reproduces the sealed-v6 kernel bit-identically —
    // that default IS the one-line walk-back.
    bool freelook_release_orient_with_keys = false;

    // Horizon recovery (S7-hrz, docs/horizon_recovery_plan.md D3/D6): the
    // open-loop fixed-angle roll that rights the carried aim/camera frame's
    // horizon on a freelook release. Caller-side like the freelook fields —
    // the pure controller never sees them (the roll is a gauge move about the
    // aim direction; control::step cannot observe it). rate = 0 DISABLES the
    // mechanism structurally (no capture, no quaternion math — the knob-off
    // strict-superset proof).
    double horizon_recovery_rate = 0.0;    // [rad/s] uniform roll rate; 0 = off
    double horizon_recovery_settle = 0.0;  // [1/s] terminal ease capture rate
    // v13g ease-in (pilot ruling 2026-08-06: "the motion should ease in and
    // out rather than being jarring"): the REST-EDGE roll's speed ramps from
    // 0 at this acceleration instead of launching at the full rate on the
    // capture tick; the settle ease-out is unchanged. The freelook-RELEASE
    // roll stays instant (whole-debt in the release tick, Chad's v9 ruling).
    // 0 = legacy instant launch.
    double horizon_recovery_ease_in = 0.0;  // [rad/s^2] rest-edge ramp-in accel

    // v13 REST-EDGE camera horizon recovery (pilot ruling 2026-08-06: "after a
    // maneuver ending inverted the CAMERA also rights itself at rest — horizon
    // level, planet below — without a freelook release"). The hand-at-rest
    // dwell the caller requires before it CAPTURES the standing up-debt once
    // (edge-triggered) and rolls it out at horizon_recovery_rate/settle. Keyed
    // ONLY on aim motion (the same ci.aim_moved seam the deadzone's rest_dwell
    // uses) — never on key state, never on body attitude: the camera reads the
    // mouse and the flight path, nothing of the aircraft's functions. 0 = the
    // recovery arms on the first still tick; the mechanism itself is disabled
    // by horizon_recovery_rate = 0 with everything else it gates.
    double horizon_recovery_rest_dwell = 0.0;  // [s] hand-at-rest arm dwell

    // v13c arm gates (pilot fly-ruling 2026-08-06: "the rotation occurs too
    // easily — even a little off angle of the horizon and I get camera
    // movement... affecting the relative position of my mouse aim on the
    // screen"). The roll rotates the whole picture about the view axis, so an
    // off-center reticle MUST sweep with it — the honest fix is to arm only
    // when the roll cannot disturb the pilot: (1) the debt is inversion-class
    // (>= arm_min — micro-tilts stay the pilot's, retired on freelook release
    // as always), and (2) the aim is resolved on the FLIGHT PATH (angle
    // aim-vs-velocity <= path_band — the reticle sits ~centered so the roll
    // displaces it imperceptibly; a held-off carve structurally cannot fire).
    // Velocity is the legal camera read (the flight path); body attitude and
    // key state stay unread. Sub-1 m/s (undefined path) never arms.
    double horizon_recovery_arm_min = 0.0;    // [rad] min debt to capture
    double horizon_recovery_path_band = 0.0;  // [rad] max aim-vs-path angle

    // v13d (Chad fly-3 2026-08-06: "engage more often but wait until I fly
    // straight... the condition of mouse and nose are resolved"): the capture
    // additionally requires the flight path to be STRAIGHT — the velocity
    // direction's own rotation rate at or below this. Level great-circle
    // flight curves at V/R (~0.6 deg/s at 150 m/s) by construction, so the
    // dial must sit above that; any real turn or loop (>= ~15 deg/s) blocks.
    // With the path straight, the nose rides the path to within AoA — so the
    // path_band resolution check IS the mouse-and-nose-resolved condition
    // through legal reads only (the path, never the body attitude).
    double horizon_recovery_straight_max = 0.0;  // [rad/s] max path turn rate

    // Mouse sensitivities (SPEC §9.1/§9.2 UI boundary): radians of aim (or
    // camera-orbit) rotation per unit mouse delta. Caller-side, like
    // freelook_easeback_time — the pure controller never sees them — but they
    // live here as tune data, not a bare number in app code. Stored radians;
    // the toml carries deg/px and the loader converts once.
    double aim_sensitivity = 0.0;             // [rad per mouse unit]
    double freelook_orbit_sensitivity = 0.0;  // [rad per mouse unit]
    // MB-aim rate-keyed mouse acceleration curve (input/aim_curve.h) —
    // caller-side like aim_sensitivity (the pure controller never sees the
    // mouse). gain_max = 1.0 is the structural OFF arm (pure linear,
    // bit-identical), which is also the default so a hand-built params
    // object stays linear. Rates are DEVICE px/s (DPI-dependent by design —
    // Chad's mouse is the tuning target).
    double aim_curve_knee = 0.0;      // [px/s] below: pure linear (precision)
    double aim_curve_rate_hi = 1.0;   // [px/s] gain reaches gain_max here
    double aim_curve_gain_max = 1.0;  // [x] flick ceiling; 1.0 = curve OFF
    double aim_curve_expo = 1.0;      // [-] ramp shape between knee..rate_hi
    double aim_curve_quant_px = 0.0;  // [px] deltas <= this never curve (P1-3)
    // Freelook camera-orbit swing limits (caller-side/cosmetic, SPEC §9.2). The
    // orbit moves the camera EYE only, never the aim. yaw <= pi (fully
    // forward). pitch_max is the EYE-BELOW (belly-view) cap; the OVERHEAD side
    // is capped tighter by render::freelook_overhead_pitch_cap (the camera-up
    // pole sits at ~pi/2 - atan(height/distance), well inside pi/2 — §6
    // red-team P1).
    double freelook_orbit_yaw_max = 0.0;    // [rad] max |orbit.yaw|
    double freelook_orbit_pitch_max = 0.0;  // [rad] max eye-below |orbit.pitch|

    // S-globelook (v4 rung 3): freelook globe INERTIA — while freelook is
    // held the orbit carries an angular velocity (render::OrbitInertia):
    // instant grab while the mouse moves, exponential coast (tau) while
    // held-and-still, capped at inertia_cap so a flick can never spin the
    // view into vection. Caller-side/cosmetic like the orbit knobs above (the
    // pure controller never sees them; the orbit never feeds mouse->aim).
    // tau = 0 DISABLES structurally — the orbit block stays the bit-identical
    // legacy tree (the S7-hrz rate=0 pattern), which is also the default so a
    // hand-built params object stays legacy.
    double freelook_inertia_tau = 0.0;  // [s] coast decay; 0 = OFF
    double freelook_inertia_cap = 0.0;  // [rad/s] coast/seed velocity cap
    // Rung-3 red-team P1: stillness required before the coast ENGAGES (the
    // S-dz-motion rest_dwell pattern) — still frames inside the dwell stay
    // PEND-classified, so an integer-mouse SLOW drag's 0-count gap frames can
    // never reseed-and-coast (the staircase amplification). 0 = the pre-dwell
    // coast-immediately arm (bit-identical; re-opens the staircase).
    double freelook_inertia_dwell = 0.0;  // [s] stillness before coast; 0 =
                                          //     coast-immediately

    // Decoupled lagging chase camera (SPEC §9.2, S7-cam). Caller-side/cosmetic
    // like the sensitivities above (the pure core never sees them) — here as
    // tune data. The camera rests behind velocity and eases toward the aim at
    // rate = cam_lag_base + cam_lag_gain*deflection; cam_lead blends the rest
    // target from velocity (0) toward the aim (1). The aim never reads these —
    // camera is downstream, so no RA9 rubber-band.
    double cam_lead = 0.0;      // [0..1] velocity->aim lean of the rest target
    double cam_lag_base = 0.0;  // [1/s] base follow rate
    double cam_lag_gain = 0.0;  // [1/(s*rad)] follow rate per rad of deflection
    // (S-keyprec / v8 removed cam_key_anchor_rate: the override keys reach the
    // TRAJECTORY and never the camera, by ruling — there is no key-flown anchor
    // and so no rate for one. The dial was deleted rather than defaulted to 0
    // because the ruling is categorical. See render/camera.h.)
    // (S7-cam3 removed cam_level_rate: camera-up is now the CARRIED aim-frame
    // up — no horizon-lock ease — so there is no re-level rate. See
    // render/camera.h aim_chase_camera and app/main.cpp `cam_up =
    // loop.aim.up()`.)

    // S-cues (comfort program, [comfort] in controller.toml): peripheral
    // orientation cues, pure HUD, DEFAULT OFF (alpha 0 skips the draw — strict
    // superset). Caller-side/cosmetic like the sensitivities above; the pure
    // core never reads them. See render/orient_cues.h.
    double cue_horizon_alpha = 0.0;     // [0..1] ghost-horizon opacity; 0 = OFF
    double cue_horizon_gap_frac = 0.3;  // [0..1) center exclusion disk radius
    double cue_bank_arc_alpha = 0.0;    // [0..1] bank-arc opacity; 0 = OFF

    // S-carets (comfort program, REC-2): screen-edge threat indicators for
    // OFF-SCREEN drones/bandits — a peripheral caret pointing where to turn to
    // face a threat, so multi-bogey awareness needs no freelook excursion. Pure
    // HUD, DEFAULT OFF (alpha 0 skips the draw — strict superset). Caller-side/
    // cosmetic like the cues above; the pure core never reads it.
    double cue_caret_alpha = 0.0;  // [0..1] edge-caret opacity; 0 = OFF

    // REC-6 (comfort program): the STYLIZED COCKPIT FRAME — a subtle,
    // screen-anchored peripheral canopy interior (the steady-state REST FRAME,
    // docs/comfort_research.md §3.3). Pure HUD, cosmetic; the pure core never
    // reads it. alpha 0 => the draw is SKIPPED (strict superset). Chad wants to
    // SEE it, so the shipped default is 0.35 (not 0 like the other cues).
    double cue_cockpit_alpha = 0.35;  // [0..1] cockpit-frame opacity; 0 = OFF
};

}  // namespace control
