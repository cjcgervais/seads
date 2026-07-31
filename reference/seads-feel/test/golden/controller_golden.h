#pragma once

// Section-4b controller golden (HARNESS §3 T2 / §5) — full-state + key
// internal checkpoints of the closed-loop instructor flight in
// test/harness/instructor.h (ctrl_golden_*): trim, a 25 deg bank-to-turn, a
// 20 deg pull, back to level. Flown through the REAL spherical sim::step()
// (never a flat stub, HARNESS §3), recorded via `seads_harness ctrl_golden`.
// A moved golden HALTS the loop: confirm the move is intentional (cascade,
// aircraft.toml, or controller.toml change), re-record, and say so in the
// commit.
//
// Recorded: 2026-07-04, Section 4b cascade core, config/aircraft.toml +
// config/controller.toml as committed. Recorded ONLY after AT-0 and the 4b
// fixes landed (HARNESS §5 — a golden of wrong code freezes the bug in).
// RE-RECORDED: 2026-07-05 — §7 feel tunes (Cl_max 1.4->1.8; K_coord 2.0->3.0).
// Both are config-knob moves, NOT cascade-code changes: same controller.cpp,
// new gain/lift table, so the scripted flight traces a different (tighter-
// coordinated) path — e.g. tick-600 held_bank 1.56 -> 1.14. Intentional.

// RE-RECORDED 2026-07-23 (kernel v5 rung C, HOLD THE LINE -- Chad's ruling):
// C2 sag servo (pull_floor 1.0: the elevator servos the aim's world
// elevation whenever the nose falls below the line -- MANEUVER-limb,
// forward-hemisphere, w_push-bounded) + C1 K_aoa 5 -> 10 (the proportional
// pushback droop halved: alpha rides ~17.7 not ~15.8 at full pull).
// SIGNATURE VERIFIED before this re-record: first divergence tick 251 with
// err 22 deg (MANEUVER), sag +0.0004, tb.z -0.93 -- the servo firing exactly
// its intended case in the scripted flight's bank-to-turn segment; FINE
// checkpoints diverge only by trajectory carry. Same cascade otherwise.

// RE-RECORDED again 2026-07-23 (v5 RUNG D, same session as the rung-C
// re-record below): plant dials k_induced 0.05 -> 0.015 / T_max 9000 ->
// 18000 + [g_limits] n_max 16 -> 32 + the D4 servo fade edges (0.3,0.7) ->
// (0.85,0.95). All Chad-ruled the same day ("give me the power -- the
// airframe is being underserved").

namespace golden {

struct CtrlCheckpoint {
    int tick;
    double px, py, pz;
    double vx, vy, vz;
    double qw, qx, qy, qz;
    double wx, wy, wz;
    double integ_x, integ_y, integ_z, held_bank;
    int regime;  // 0 = FINE, 1 = MANEUVER
};

// RE-RECORDED 2026-07-06 (§7-turn2: bank_align_power 1->3->6 — hold the
// up-elevator until the roll aligns so a lateral aim FLIPS the bank first, no
// climb; blend_hi 12->9 — roll a bit sooner. On top of the responsiveness pass:
// c_pitch 11 / c_yaw 16 / c_roll 24 / p_max 260 / K_coord 1.0 / K_theta 3.2 /
// n_max 16 / n_min -8 / camera lag 6/16 / auto_level 3. All config + one
// cascade line. Intentional.
// RE-RECORDED 2026-07-06 (§7 nose-seek: pitch pursuit gets a PARABOLIC gain
// rise — K*e*(1+expo*e), expo=2.0 — so a bigger deflection pulls harder, a
// Chad feel change. seek_law replaces sqrt_law on the pitch pursuit only; the
// step floor is 0 (rings AT-2 if raised). Same cascade structure, new pointing
// magnitude, so the scripted 20 deg pull / 25 deg bank trace a slightly firmer
// path. Intentional.
// RE-RECORDED 2026-07-07 (MB-rud, SPEC §0 + docs/mission_b_instructor_plan.md
// D1/D2, Chad's rulings: yaw_max 90->65 — the ONE shared rudder ceiling, the
// keyboard "little nerfing"; yaw_scale moved INSIDE sqrt_law's gain slot; the
// yaw_maneuver_frac blend fade replaced by the bank-aligned gate
// yaw_align_band/yaw_min_frac — full rudder while the bank is within 90 deg of
// aligned, floor past knife-edge. The scripted 25 deg bank-to-turn now carries
// MORE rudder into the aligned turn (gate 1.0 vs old fade 0.8) under a LOWER
// ceiling, so the trace shifts. Intentional; AT-15/AT-16 green at the new
// table.)
// RE-RECORDED 2026-07-07 (Mission B fly-verdict dials, one commit: yaw_max
// 65->55 — "top end of the yaw is still a bit too much"; yaw_scale 1.2->1.5 —
// "more yaw on small adjustments... the nose has to catch my mouse";
// c_pitch 11->13 — "raise the pitch just a little". All TOML knobs, no
// cascade code; step-yaw at 1.5 shows 0 reversals/0 overshoot (no hunt),
// step-pitch settles 2.32->1.40 s at 0.27 deg overshoot (AT-2 pass).
// Intentional.)
// RE-RECORDED 2026-07-08 (MB elevator package, Chad "chickadee dips":
// K_wi_pitch 20k->60k (step-1, constitution-capped — see the toml comment) +
// pursuit_expo 2->3 (moderate-deflection pull). Controller-side only; the
// scripted pull tracks tighter so the trace shifts. Intentional.)
// RE-RECORDED 2026-07-08 (MB envelope: T_max 9000 + compression knee 140/245
// — plant knobs; the scripted instructor flight flies the same commands over
// a faster, livelier plant). Intentional.
// RE-RECORDED 2026-07-08 (MB-4 EXECUTED: yaw_scale 1.5->2.0, TOML-only —
// Chad: "more rudder pull at low amounts of deflection at centre screen".
// One step past the plan's 1.6->2.0 ladder by ruling (no 1.8 data point;
// 1.8 is the fallback if 2.0 feels hot). yaw_max stays 55. The scripted
// bank-to-turn carries a stronger linear-region rudder so the trace shifts.
// AT-15 Leg 2 / AT-16 / MB-rud config-oracle legs all green at 2.0 (the
// corkscrew/|beta| walls did NOT trip). Instruments (step yaw 4, before ->
// after): steady residual 0.647 -> see flight-log; dwell probe (step yaw
// 0.2 at the deadzone band) logged there too. Intentional.)
// RE-RECORDED 2026-07-08 (MB deadzone dial lo/hi 0.10/0.25 -> 0.05/0.12,
// Chad: "right to the centre" — TOML only, MB-lean lands next commit. ONLY
// the tick-1200 checkpoint moved (ticks 300-900 bit-identical — the tighter
// circle bites only where the scripted flight PARKS). The trim-hold leg
// re-shaped to fraction + episode-tail (the halved band cycles ~2x faster;
// parking at the re-arm boundary with taps IS the deadzone working, 4b).
// Intentional.)
// RE-RECORDED 2026-07-08 (MB-lean NEW MECHANISM: FINE held_bank decays
// toward clamp(lean_gain * de-rolled_az, +/-lean_max) instead of level —
// Chad: "attracted like a magnet... hold a shallower angle of bank with use
// of rudder at moments of moderate deflection". [auto_level] lean_gain 8 /
// lean_max 30 deg. The scripted flight's settled phases now LEAN into the
// residual lateral error (held_bank ~0.2 rad at tick 600 vs ~0.0008 —
// exactly the mechanism working), so the whole trace past tick 300 shifts.
// Instruments: closed-loop magnet steady mean err 0.093 deg (was ~0.5 —
// the rudder/coordination standoff); moderate-deflection carrot holds
// mean phi 29.9 deg, 0 wing-wag flips. knob-off lean_gain=0 pinned
// bit-identical to the old decay-to-0 arithmetic. Intentional.)
// RE-RECORDED 2026-07-08 (MB-lean diff red-team P1-1 fix: the de-roll axis
// is now (cosPhiTheta, sin(e.phi)) == cross(nose_b, up_b) — exact at EVERY
// attitude; the committed phi_full cos/sin pair was exact only at zero
// pitch (a pitch-45/bank-40 vertical aim leaked ~7.2 deg of phantom lean,
// pinned by the new PITCHED companion leg). The scripted flight's pitched
// FINE segments shift slightly (the numerator scales by cos(pitch)).
// Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 1: yaw_scale 2.0 -> 2.2,
// TOML-only, feel/rudder-flick branch. Chad: "rudder strong at the bottom
// end... lil flicks that do go far are just rudder" — +10% linear-region
// rudder pointing gain. The drafted 2.4 BREACHED the AT-16 beta wall
// (peak |beta| 5.02 deg > 5.0 in the sustained coordinated turn) and was
// NOT recorded; 2.2 re-certified AT-15/AT-16/MB-rud live. Instruments:
// step-yaw-2 settle 0.333 -> 0.292 s (demand 12.8 -> 14.08 deg/s), dwell
// probe 0 reversals both sides, step-pitch-30 bit-identical. The scripted
// flight's yaw-pointing segments shift accordingly. Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 2: [auto_level] rate 3.0 -> 4.5,
// TOML-only, feel/rudder-flick branch. Chad's fly-1 report: post-flick the
// nose "stays offset... quickly settle back into the centre" — the MB-lean
// magnet follower arrives ~1.5x sooner (tau 0.33 -> 0.22 s). lean_gain
// stays 8 (the drafted 6 DROPPED by the same report) so this is the full
// 1.5x lean-loop-gain raise — slow-flight wallow is Chad's fly sentinel.
// AT-13 + the auto-level cascade bounds were config-derived at this flip
// (the fixed 5 deg/s / 30 deg/s constants were calibrated to rate 3.0 —
// the config-relative-bounds lesson). The scripted flight's FINE lean
// segments settle faster; the trace past tick 300 shifts. Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 3: [auto_level] lean_gain 8 -> 10,
// TOML-only, feel/rudder-flick branch. Chad: "make the yaw magnet just a
// little bit stronger — it's still staying outside of the circle." The
// magnet's STRENGTH dial: the post-flick parked offset (pointing-vs-
// coordination equilibrium) shrinks ~20%; dwell-probe steady residual
// 0.101 -> 0.057 deg, step-yaw-4 steady 0.299 -> 0.269. Cumulative lean-
// loop gain = 1.875x the flown-good MB-lean point (rate 4.5 x gain 10) —
// wallow/roll-busy-ness are Chad's fly sentinels, fallback 9. The scripted
// flight's FINE lean segments deepen; the trace past tick 300 shifts.
// Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 4: [deadzone] hi 0.12 -> 0.08,
// TOML-only. Chad: zoomed slow mouse-up gave "little micro boosts...
// ascending jitter" — the re-arm threshold IS the boost amplitude of the
// deadzone relaxation cycle (~9 -> ~6 px at 4x zoom); lo stays 0.05 (the
// approved resting circle). ONLY the tick-1200 checkpoint moved (one late
// FINE re-arm forks the tail); 300/600/900 bit-identical. step-yaw-4 /
// step-pitch-30 bit-identical; pitch dwell 1 reversal / 0.01 deg rebound.
// Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 9 rung 2: [auto_level] lean_gain
// 10 -> 12, TOML-only. Chad: "it needs to go right into the middle, it
// hangs — this matters for lining up my shots." Rung 1 (K_coord 0.8, and
// its 0.9 fallback) was REJECTED PRE-FLY — both breached the AT-16 beta
// wall (5.87 / 5.33 deg > 5.0) — so the magnet closes what the rudder is
// walled from closing. step-yaw-4 steady 0.269 -> 0.243 deg; pitch + dwell
// bit-identical. Cumulative lean-loop gain = 2.25x the flown-good MB-lean
// point — wallow/roll-busy-ness sentinels at their most loaded, fallback
// 11. The scripted flight's FINE lean segments deepen further; the trace
// past tick 300 shifts. Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 10: [auto_level] rate 4.5 -> 5.5
// + lean_gain 12 -> 16, TOML-only, one felt ask ("quicker and stronger
// magnet... smoothly and quickly find center"). WHY 16: the magnet
// equilibrium e_eq ~ 0.75deg/gain — 12 parked at ~0.063 deg, OUTSIDE the
// 0.05 circle ("still hangs"); 16 -> ~0.047, inside, the deadzone latches.
// step-yaw-4 steady 0.243 -> 0.200 deg; pitch bit-identical. Lean loop =
// 3.7x the flown-good point: the V=70 harness wallow probe is DEAD FLAT
// (bank pinned at the cap, 0 slope flips over 8 s; entry overshoots the cap
// to 41 deg transiently — the punchy roll-in to judge). Chad's slow-flight
// leg is the mandatory sentinel; fallbacks 14 -> rate 5.0 -> 12/4.5.
// The scripted flight's FINE lean segments deepen further. Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 11: REVERT lean_gain 16 -> 10 +
// rate 5.5 -> 4.5 — the Fly-10 escalation was FLOWN AND REJECTED by Chad:
// "more ready to bank than it does allow me to do rudder flicking, the
// magnet for yaw isn't strong enough to pull it into the middle." The lean
// closes via BANK; past ~10 the bank-eagerness costs more than the closing
// buys. PROOF OF FULL REVERT: this re-record is BIT-IDENTICAL to the Fly-4
// commit's checkpoints (226070046) — the checkpoints below ARE that record.
// The near-center hang is now owned by the S-yaw-magnet mechanism
// ([coordination] center_frac). Intentional.)
// RE-RECORDED 2026-07-10 (S-yaw-magnet, SPEC §0 — Chad's ruling: "the magnet
// for yaw isn't strong enough to pull it into the middle." NEW MECHANISM:
// [coordination] center_frac 0.25 / center_band 0.5 deg — the anti-crab
// coordination fades near center (smoothstep, no latch) so the RUDDER
// finishes into the aim circle; err >> band keeps full coordination, AT-16
// untouched by construction (it runs live in this same gate). ONLY the
// tick-1200 checkpoint moved — the scripted flight is near-center only in
// its late FINE segment; 300/600/900 bit-identical. Closed-loop: the
// post-flick park now CROSSES lo and latches (e_min 0.0499 deg vs the 0.076
// hang; the S-yaw-magnet cascade legs pin it). Intentional.)
// RE-RECORDED 2026-07-10 (rudder-flick Fly 13: lean_gain 10 -> 6 +
// [coordination] center_band 0.5 -> 1.5 deg + center_frac 0.25 -> 0.15,
// TOML-only — the ORIGINAL plan's shallow-bank ask, finally landable with
// S-yaw-magnet owning centering. Chad: "still too much bank gain early...
// I want more rudder close to centre and the banking to come on more
// slowly and smoothly as I move away — crabbing? yes!" frac 0.25 -> 0.15
// because the shallow lean + crabbier approach retain more sideslip and
// the park missed the circle by a hair at 0.25 (0.0527 > 0.05 — the
// closed-loop leg caught it PRE-FLY); 0.15 restores the crossing. At gain
// 6 the lean_max cap is unreachable inside FINE (knee = blend_lo) —
// vestigial by design; the MB-lean cap leg self-skips, the moderate-
// deflection oracle is the config-recomputed linear target (27 deg).
// The whole trace shifts (shallower FINE leans + crabbier approaches).
// Intentional.)
// RE-RECORDED 2026-07-11 (rudder-flick Rung B1: [auto_level] rate 4.5 -> 5.5
// ALONE, TOML-only — Chad's next-pass ask "bank activated quicker... near-
// center small aims too", Fly-13 shallow character KEPT (lean_gain 6,
// center_frac/band untouched). This ISOLATES the rate Fly 10 confounded
// with lean_gain 16 (that rejection was the gain: lean loop gain here is
// 33 vs the approved 27 vs the rejected 88; lean tau 0.22 -> 0.18 s, series
// lag to the leaned bank 0.42 -> 0.38 s — K_phi's 0.2 s wings-chase is now
// the binding stage). Tick 300 bit-identical but the integrator tail
// (e-18); 600/900/1200 shift as the FINE lean segments arrive quicker.
// Fly sentinel: near-center centering must still feel like RUDDER.
// Intentional.)
// RE-RECORDED 2026-07-11 (rudder-flick Rung Y1: [coordination] center_frac
// 0.15 -> 0.05, TOML-only — Chad's fly report on B1: tiny deflection centers,
// "a little bit more and it goes back to the outside of the circle... I want
// it to quickly go where I need it especially at the last little bit". The
// park equilibrium scales with the flick's residual crab: the new harness
// PARK readout (step-yaw closest-approach + latch ticks) reproduced it — a
// 4 deg flick at 0.15 hangs OUTSIDE lo (0.0545 @V140, 0 latch); 0.10/0.08
// cross by a razor hair; 0.05 crosses with margin at V=80/140/220 and parks
// latched MORE (448 vs 0 ticks). Dwell probes show NO new boundary chatter
// (0 reversals both fracs). 300/600 bit-identical (the relief bites only the
// near-center late FINE segments); 900/1200 shift. Intentional.)
// RE-RECORDED 2026-07-11 (rudder-flick Rung Y2: [coordination] center_frac
// 0.05 -> 0.02, TOML-only — Chad flew Y1: "after a big lateral jut the nose
// comes back short of the circle staying cocked... about 5mm outside... top
// corner of whichever side I jutted to". The PARK-direction readout (new:
// lateral/vertical decomposition + tail beta/phi) attributed it: a big jut
// leaves beta 7.5-9.4 deg of crab and the park rides frac-scaled just outside
// lo for the whole window (V=80 worst: 0.0718, never crosses in 12 s). At
// 0.02 every probed jut (2-20 deg, V=80/140) CROSSES and LATCHES; the tail
// ride left is the VERTICAL trim-band cycle (the accepted at-rest park).
// beta-gating the relief would re-create the ~1.3 deg hang — rejected. The
// trade at its extreme: near-center beta-nulling 1/50th — the lingering
// sideways attitude is the fly sentinel, walk-back 0.05. 300/600
// bit-identical again; 900/1200 shift. Intentional.)
// RE-RECORDED 2026-07-11 (rudder-flick Rung M1, Chad's RULING on the Y2 fly:
// "okay forget the crabbing, I just want that nose to snap and stay right in
// the middle of the mouse aim" — TWO dials, one felt ask (the batch
// precedent): [coordination] center_frac 0.02 -> 0.0 (lateral standoff fully
// closed — rest-park lateral 0.0000 at every probed V; crab now has no
// near-center decay channel, accepted by the ruling) + [deadzone] lo/hi
// 0.05/0.08 -> 0.03/0.05 (rest offset mean 0.06 -> 0.034 deg against the
// FIXED 9 px reticle ring). The circle's honest floor moved: Fly-5's "do not
// re-pin" premise (magnet equilibrium 0.076 above the circle) is dead at
// frac 0; the NEW floor is the V-scaled VERTICAL ff-deficit equilibrium
// (0.028 @V140, 0.050 @V250 — the PARK instrument's discovery; 0.02/0.035
// was TRIED and stopped honestly: AT-14a ff_n=0, the latch went vestigial at
// cruise). Premise recalibrations, each documented in place: trim-hold
// occupancy -> vacuousness guard (27%/0.33 s episodes measured), override
// integrator fixture aim -> 2 deg lateral (an on-nose aim's yaw demand is
// exactly 0 at frac 0), loader pins. 300/600 bit-identical; 900/1200 shift.
// Intentional.)
// RE-RECORDED 2026-07-11 (S-wvane, SPEC §0 — NEW PLANT FORCE, Chad's ruling
// "forget the crabbing, I don't want the cocking... snap to the centre of
// the mouse aim every time": fuselage side-force from sideslip Cy_beta 2.5 +
// drag price Cd_beta 0.6 in sim/step.cpp; the controller is UNTOUCHED — the
// seam holds, forces are not inverted. The scripted instructor flight's
// every crabbed segment now bleeds flat (tau_beta ~1.07 s @V140) and pays
// sideslip drag, so the WHOLE trace shifts from tick 300 on (this is a
// plant change, not a table edit). Recalibrations landed with it, each
// investigated against the tau physics NOT blessed (plan-audit P1-5):
// AT-12 mirror + Cd_beta (the fork detector tripped at rel 0.055, folded,
// back to 4.8e-14 — the workless side force needs NO mirror term, by the
// perpendicular-to-pre-tick-v construction); drone turn bound 0.20 -> 0.15
// (the P-only autopilot turns uncoordinated and the side force honestly
// gentles it ~14%); MB-lean moderate bound 0.5 -> 0.3 of target (the side
// force turns part-FLAT, less bank for the same tracking); the S-yaw-magnet
// centering leg re-scoped to the CRAB-BLEED WINDOW (S-wvane kills the hang
// at the ROOT — the relief-off arm now centers itself; the relief's
// remaining job is keeping the nose planted during the ~1 s bleed).
// Intentional.)
// RE-RECORDED 2026-07-11 (S-dampff, SPEC §0 — PITCH DAMPING FEEDFORWARD, the
// pitch-thread attribution: tau_cmd += damp_ff*damp*q_eff*omega_des_total on
// the ff'd axis; [inner] damp_ff_pitch 1.0, yaw/roll 0.0 = OFF (queued
// rungs). The integrator no longer sources the pitch damping torque during
// sustained rotation, so the wound-integral CARRY-PAST dies (20-deg pitch
// step at V=250: settle >10 s -> 1.08 s, park-past-aim 0.86 -> 0.00 deg —
// Chad's "sitting high after a deflection") and the keypress dip actually
// REACHES its n_min budget (peak rate 32.6 -> 41.1 deg/s at V=140). integ_x
// collapses toward ~0 in every settled segment (its sustained job moved into
// the ff). KNOB-OFF PROOF: the gated term at damp_ff=0 passed the PRE-flip
// golden bit-identically, 258/258, before this record. Same session, same
// commit train: n_min -8 -> -9 (pitch-thread Rung P1, TOML-only, landed
// BEFORE the flip — its gate was green on the old golden). The whole trace
// shifts (pitch tracking is tighter everywhere). Intentional.)
// RE-RECORDED 2026-07-12 (ff-radius CORRECTNESS FIX, auto/feel-research twin —
// autoresearch attribution of the vertical ff-deficit "nose parks a few
// hundredths high in cruise", NOT a gain retune. controller.cpp curvature
// feedforward divisor ap.R -> glm::length(s.position): the aim's parallel-
// transport and the level-flight path both curve at V/|position| = V/(R+alt),
// not V/R, so dividing by the baked planet radius over-commanded pitch-down by
// V*h/(R*(R+h)) and the pointing loop cancelled it with a steady nose-UP
// demand, parking the aim ABOVE the nose by V*h/(R*(R+h)*K_theta). PROVEN by
// the `seads_harness step` altitude sweep: the normalized standoff collapses to
// 1/K_theta (~18 deg) across V AND altitude, and vanishes toward the deadzone
// floor as h->0; post-fix the nose CROSSES lo and latches at every altitude
// (was: hung OUTSIDE, standoff 0.030->0.081 deg over h=500->8000). The
// ctrl_golden spawns at 3000 m so the ff magnitude drops 16.67% there; the
// scripted flight traces a monotone accumulating shift (t300 0.014 m ->
// t1200 0.77 m of position, no blow-up, still airborne). At h=0 the two
// divisors are bit-identical (the honest knob-off arm). The AT-11/AT-6/AT-14a
// test-side ff oracles were re-derived to |position| in the same commit (they
// hardcoded /kAp.R). PROPOSAL for Chad, NOT a merge — the fix is a FELT change
// (nose stops parking high, V-scaled), so shipping to the kernel is his stick.
// Intentional.)
// RE-RECORDED 2026-07-12 (post-seal Rung R1: [inner] damp_ff_roll 0.0 -> 1.0,
// TOML-only — the roll half of the S-dampff plant inversion. Pre-checked in
// the autoresearch twin (A/B on `seads_harness step roll 45 {140,250}`):
// reversals/settle/overshoot UNCHANGED, roll's own park FIXED (V250 standoff
// 0.0436 hanging-OUTSIDE -> 0.0070 CROSSED+latched), peak roll rate 187->244
// deg/s @V140 (the sagged axis now delivers its p_max budget). Diff signature
// verified before re-record: t300 mid-bank wz/integ_z move materially (faster
// roll transient; the integrator no longer sources damp*Q*omega), later
// checkpoints shift slightly, integ_z magnitudes DROP on the ff'd axis
// ("integ = trim"). AT-8/AT-13/AT-16 all green pre-re-record; only this
// golden moved. NOTE the same-day twin A/B ruled damp_ff_yaw=1.0 BLOCKED at
// current gains (47-49 reversals — raise K_w_yaw + re-derive the yaw ladder
// first, its own session). Chad flies R1. Intentional.)
// RE-RECORDED 2026-07-12 (Rung Y3, the yaw ladder session: [inner]
// damp_ff_yaw 0.0 -> 1.0 FLIP-ONLY, K_w_yaw UNCHANGED at 200k — the same-day
// "BLOCKED" ruling above is SUPERSEDED: the 47-49 reversals were 100%
// instrument artifact (deadzone park taps in the 20 s counter; deadzone-kill
// arm 47 -> 2; no ring at any amplitude; the plan-audit's corrected model
// zeta = sqrt((K_w+dQ)/(4*I*K_theta*yaw_scale)) = 0.91-1.15 at 200k, and the
// K_w ladder {200k..300k} probed FLAT). Twin-measured buys: peak yaw rate
// +35%@V140 / +90%@V250, V250 carry-past 0.21 -> 0.02 deg, integ_y idle.
// Diff signature verified before re-record: t300 mid-maneuver moves
// materially (wy -0.030 -> +0.016; integ_y magnitude 0.075 -> 0.025; roll
// phase wz 0.44 -> 1.97 — yaw arriving faster re-choreographs the roll-in);
// settled-checkpoint integ_y magnitudes DROP ~3x (0.065/0.048/0.035 ->
// 0.020/0.016/0.012, sign flip = trajectory-history carry through the tau=8s
// drain, NOT wiring — the wiring pin is test_cascade Sample C, green).
// Position drift meter-scale over 10 s. AT-12's welded n_min_sus>2.0 premise
// re-parameterized same commit (the wind-up->relax breathing trough exists in
// BOTH arms: 2.38@t9.7s ff=0 vs 1.70@t8.0s ff=1 — bound was phase-calibrated
// to the sagged plant; now floor>1.5 anti-glide + NEW n_mean>4.0 guard).
// Chad flies Y3. Intentional.)
// RE-RECORDED 2026-07-17 (S-rimshot v2 UNIVERSAL — the flick-misconception
// fix, docs/v4_rimshot_MISCONCEPTION_handoff.md, Chad's ruling "any
// deflection at all... Always, even mid-track": the ARM gate
// (snap_on/snap_off/park-edge) is DELETED, so the scripted 25/20 deg steps —
// previously below snap_on by design, keeping this golden pinned — now
// legitimately fire the carry-through rebound. Recorded TWICE this session:
// the first record predated the diff red-team's P0-2 (blend-band engage on
// the faded banked demand — multi-engage churn on the lateral step) and was
// superseded the same day by this table, at the FINE-endgame engage gate +
// the re-flick/geometry-break exit discriminator. Diff signature verified
// before THIS record: t300 (mid-MANEUVER, 60 ticks after the tick-240
// lateral step) is BIT-IDENTICAL to the pre-universal table — the endgame
// gate keeps the whole MANEUVER approach legacy; only the settle
// checkpoints (t600+) move, meter-scale. The carry=0 arm remains
// bit-identical to the pre-diff arithmetic (test_capture structural-off
// legs), so the whole move is the event and nothing else. Instrument rows:
// 45deg@V140 engages=1 reversals=1 rim=1.50 arrive=92ms; 2deg@V220
// engages=1 rim=0.85 approach_ratio=0.995; yaw-20@V140 engages=1 (one
// clean attempt, the banked creep endgame regroups — the honesty wall).
// Chad flies the universal rimshot. Intentional.)
// RE-RECORDED 2026-07-17 (S-rimshot v3 "POOL BALL" — Chad's four rulings:
// Q1 FULL SPEED through center, no pre-braking outside/before the circle;
// Q2 the glance lands DEAD ON the inside wall — rim_frac 0.85 -> 1.0 and
// the rim DETECT replaced by the PREDICTIVE BRAKE SURFACE (CARRY -> RETURN
// fires when remaining-to-rim <= w^2/(2*(alpha_u+damp_assist)) + 0.5*w*dt;
// RETURN's outbound leg then lands the apex ON the wall with the
// rim-targeted servo-fed alpha_req law — measured rim 0.99-1.00 across the
// V x step grid vs 1.48-1.59 before: "predictably the same every time");
// Q3 slow/gentle arrivals DIRECT-SEEK (the glance_frac classifier at
// ENGAGE enters RETURN directly — an active full-authority capture, never
// the lazy taper; apex-death and the geometry-break abandon mid-CARRY also
// direct-seek now, so the "one attempt then regroup to legacy" lateral
// behavior is DEAD — yaw-20@V140 captures to the deadzone circle in
// ~0.5 s, drift 0.04 deg); Q4 return_w 60 -> 400 deg/s (effectively
// uncapped — the sqrt IS the return law everywhere). Terminal: the v2
// free-coast tau-arrest is SUPERSEDED by the predicted-landing dead-blow
// (complete when |demand - coast| <= deadzone_hi with coast <=
// 2*deadzone_hi, both vectorized in the pointing plane) + the inbound
// brake law + the unwind-only event integrator (a full freeze re-created
// the documented through-the-return-swing blow-through). Instrument rows:
// 45@140 / 45@220 / 90@140 / 20@180 all engages=1 reversals=1
// rim=0.99-1.00 drift 0.02-0.05; 2@220 DIRECT-SEEKs (rim n/a) drift 0.003.
// Divergence verified BEFORE this record: tick-300 checkpoint
// BIT-IDENTICAL (pre-event flight untouched); only the settle checkpoints
// (600/900/1200 — the scripted maneuvers' terminal captures) move,
// meter-scale. Intentional.
// RE-RECORDED 2026-07-17 (S-rimshot v3 YAW EXACTNESS — the red-team's two
// yaw gaps closed: P1-1 the event now plans AND drives in ERROR space
// (wny_ev = the physical error rate; the emission compensates yaw_coord
// inside the yaw_max clamp, the captured axis co-rotates with the measured
// roll, the outbound glance keeps the carried direction-of-travel axis, and
// the legacy-perp channel + transverse rate-arrest keep the ball THROUGH
// the middle — pre-fix the coordination bleed killed the yaw glance at
// 0.29-0.44 rim); P1-2 cap_w_hold is clamped to the wall-reachable rate
// sqrt(2*alpha_u*rim) at ENGAGE and re-checked live through CARRY (Chad's
// ruling: never-past-the-rim WINS — yaw 10@140 was crossing at 34.6 dps
// with a 0.75 deg stopping distance into a 0.55 deg ring, rim 1.26). The
// brake surface also drops the damping-assist credit (a late fire cannot
// be un-overrun; the rim-targeted alpha_req law stretches an early one).
// Grid after: yaw rim_e 0.98-1.08 across {5..60}@140 + 20@220 (the NEW
// frame-honest norm readout — the body-yaw projection under-reads banked
// arrivals by |u_y|); pitch grid 0.97-0.99 (was 0.95-1.00), w_cap
// unchanged (the clamp never binds pitch). Divergence verified BEFORE this
// record: tick-300 BIT-IDENTICAL (pre-event flight untouched); 600/900/1200
// move sub-meter (the scripted terminal captures fly the compensated
// emission). Intentional.

// RE-RECORDED 2026-07-28 (RUDDER-TRIM Fly A): [coordination] yaw_scale
// 2.2 -> 2.0 (Chad: "a little too much rudder bias... balance it out a bit").
// A single TOML yaw-knob move, NOT a cascade-code change: yaw_scale's only
// consumers are the yaw pointing slots in controller.cpp, so the divergence
// enters through the yaw channel and the checkpoints carry the different
// (softer-ruddered) trajectory. step-yaw instrument clean at V140/V250
// (settle 0.642/0.758 s, 1 capture reversal, overshoot ~unchanged).
// Walk-back = 2.2. Intentional.

// RE-RECORDED 2026-07-30 (POOL-BALL RETIREMENT): [capture] carry 1.0 -> 0.0.
// Chad's ruling: "park the cue ball behavior as retired for now" — the
// capture machine was ATTRIBUTED (docs/jitter_attribution.md: 26x flap
// density below blend_lo, dead-blow amplitude match, stick-confirmed A/B) as
// the owner of the small-deflection rudder/elevator flapping, and carry = 0
// is the DESIGNED structural off (the machine never engages; every expression
// tree is bit-identical legacy). A single TOML dial, NOT a cascade-code
// change. KNOB-ISOLATION PROVEN FIRST (the repo's discipline): re-recording
// with the committed table temporarily back at carry = 1.0 reproduced the
// PREVIOUS values BIT-EXACT, so nothing but this dial moved.
// Divergence signature, capture-shaped: tick 300 is BIT-IDENTICAL (the
// scripted flight's first arrival is the 25 deg lateral at tick 240, whose
// FINE endgame — ENGAGE is gated to err < blend_lo — had not been reached by
// 300); the FIRST DIVERGENT checkpoint is tick 600, i.e. the machine's one
// and only effect enters between 300 and 600, exactly where the lateral
// arrival closes. 600/900/1200 then carry the different (no-rebound) arrival
// shape, ~1-3 m — the 20 deg pull at tick 600 adds its own endgame. Because
// carry = 0 makes the machine structurally unreachable, the mere existence of
// a divergence PROVES the event fired in the carry = 1 arm.
// Walk-back = carry 1.0 (and nothing else). Intentional.

// RE-RECORDED 2026-07-30 (BLEND-BAND ROLL TARGET CONTINUITY — the 5-10 deg
// roll slam, [regime] roll_target_mix 0.0 -> 1.0): inside the blend band the
// MANEUVER roll limb now chases the mixed bank target blend*phi_commit +
// (1-blend)*lean_target instead of the raw bank_error commit (coverage-
// completion of MB-lean; the pitch align / yaw_gate McRuer treatment applied
// to the roll TARGET). KNOB-OFF ARM PROVEN FIRST: with roll_target_mix = 0.0
// the golden reproduced BIT-IDENTICALLY (structural off — the reshaping block
// is skipped entirely), so nothing but this mechanism moved. Divergence
// signature VERIFIED before this record (per-tick knob-0-vs-knob-1 probe):
// first divergent tick 289 — inside the band on the scripted 25 deg lateral
// capture (blend 0.9899, err 8.76 deg, held_bank ~0), first movers
// omega_des.z (-0.5057 -> -0.4732, the roll demand easing toward the mixed
// target) and integ_z; every tick through 288 (the whole FINE prefix + the
// blend == 1 head of the capture) BIT-IDENTICAL — the blend_hi guard holding
// exactly. Checkpoints 300+ carry the softer band-descent trajectory.
// Walk-back = roll_target_mix 0.0 (the Golden-#4 baseline arm). Intentional.
inline constexpr CtrlCheckpoint kCtrlFlight[] = {
    {300, 17996.319414462843, -2.3300407522744488, -349.69290483107801,
     -3.1928936557311891, -14.572827314723662, -138.82626520195942,
     0.28437161602964967, -0.059029124507201894, -0.16455305279785559,
     -0.94264024912556366, 0.48887056648994487, 0.0039332030498266026,
     2.1019044474873136, 0.020528113593921194, 0.024726417976115495,
     0.13635643130654987, -3.0290096447182725e-17, 1},
    {600, 17990.364511347845, -111.27587944291528, -677.81880799867747,
     -5.4267540366182914, -54.892875178323024, -127.0201212714458,
     0.69168219000702713, -0.15550160445491615, -0.1502480121060214,
     -0.68907222690892034, 0.10827900819595637, 0.0038060047068464824,
     0.0026890410114256457, -0.0015462468366456267, 0.01989782984525058,
     0.054084228552243936, 0.0057595455720957445, 1},
    {900, 18053.652954011366, -247.69735397738128, -980.41297003238697,
     36.413731758976532, -53.190978476948352, -115.55239102149771,
     0.70910690197679538, -0.041446829258130924, -0.26012723506646812,
     -0.65405151440081033, -0.0059218715384370714, -6.1786312324379894e-05,
     0.0011315774210023425, -0.029989626668447494, 0.015698727793334095,
     0.042985889317844965, 0.0020419595428375738, 0},
    {1200, 18140.151925323549, -377.53888390797982, -1261.1746403801346,
     32.399629011639846, -50.659166222879442, -109.27980593089455,
     0.708939967253019, -0.047799786999113331, -0.2542198992841001,
     -0.65611854569285699, -0.0060184718714197099, -2.1485942083914428e-05,
     9.2126621078286069e-05, -0.013512923880252423, 0.012411476731430695,
     0.033951505739038018, 0.0014321192162765342, 0},
    // final: alt 3187.86 m, speed 124.732 m/s
};

}  // namespace golden
