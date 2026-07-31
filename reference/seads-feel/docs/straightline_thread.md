# THREAD STUB — S-straightline: the tracer-line dip (turn-entry pitch coordination)

**Opened 2026-07-30 by Chad's ruling (verbatim, the spec):**
> "the dip is a flaw, whether pre existing or not. I feel that a normal flight would apply,
> rudder first to lead the bank a little before the elevator but I feel the elevator increase
> should smoothly coorelate to banking increase. they are both supposed to align to the final
> turn angle at the saem time is what I hypothesize... The line that my tracers draw should be
> straight and my gunnery should have a predictable path via my control surface operation not
> accept 'character' that I need to compensate for. A straight line is the shortest distance,
> a dip is a delay... Curver are nice, but I can decide what and when that looks like via
> inputs rather that adjusting to quirks."

**Ruled: BUG, not character.** Status: STUB — plan mode + kernel-base consult packet before
any code. NOT folded into S-rollmix (its own thread, its own card, per the docs agent's line).

## The attributed mechanism (measured, ledger row 291dc5314)

The dip is pre-existing v10 behavior (A/B: identical at roll_target_mix 0/1 to 3 decimals;
depth 2.23/5.05/8.29° for 15/30/60° flicks at V200), newly visible through Chad's tracer
instrument. It is the composition of two flown-in mechanisms:
1. `[regime] bank_align_power = 6` (S7-turn2, Chad 2026-07-06 "roll first, don't get pushed
   up in a turn"): pitch pull scaled by `cos(bank_eff)^6` — near-ZERO through most of the
   roll-in, then opens late. The lull.
2. The rung-C sag servo (`pull_floor = 1.0`, "hold the line of my mouse inputs"): REACTIVE —
   fires proportionally only after the nose falls below the aim's world-elevation line. The
   upswing. It bounds the dip; it cannot prevent it.

## The felt spec, translated (verify with Chad in plan mode before coding)

- Rudder LEADS the bank slightly (note: the MB-rud crab already does this — verify, don't
  duplicate).
- Elevator feeds in CONTINUOUSLY with the achieved bank — the textbook coordinated turn
  entry: as lift tilts, vertical lift share ~cos(bank) drops, so back-pressure grows with
  bank to hold the path line. Chad's "both align to the final turn angle at the same time."
- Success instrument: the TRACER LINE — a lateral flick under held aim draws a STRAIGHT
  line (no U). Offline oracle: the dip-probe rig (nose-elevation trace through a flick,
  ledger row 291dc5314 has the baseline numbers 2.23/5.05/8.29°).

## Reframed by Chad's same-night correction (mandalark ledger e0168ad + SPEC §0 P-helm)

The 2026-07-06 roll-first ruling is the FOURTH flown instance of the compensation-decay
law — Chad's own history: it was tuned against the underpowered / pole-locked plant and
"may have also been a compensatory measure without the proper solution." Consequence for
this thread's design conversation: the mechanism is a CLEAN-SHEET build for the plant as
it flies NOW (T/W 0.61, all three axes inverted), with the simultaneous-arrival
hypothesis as the spec — NOT a softened negotiation with the old gate. The gate's cos^p
law cannot express simultaneous arrival at any power (power-down only interpolates
between climb and dip — the docs agent's read, agreed). The intent principle behind the
whole thread is now constitutional: SPEC §2.5 "Predictable to myself, unpredictable to
them" (P-helm). The retiring gate SHAPE may re-arm per-vessel (A6M2, low authority) as a
deliberate character dial — parked, like the capture machine, never deleted.

## Constraints already known

- The July-6 anti-climb intent STANDS: the new shape must not resurrect "pushed up in a
  turn" (climb during roll-in). Both rulings are simultaneously satisfiable — supersede the
  gate's SHAPE, not its intent. Walk-back preserved: whatever ships must have a knob-off arm
  bit-identical to the S-rollmix kernel.
- AT-15's boundary-tied family keys on the push/roll tables and blend premises — check the
  calibration manifest before touching any [regime] shape.
- The pitch align gate's `(1-blend)+blend*align` composition is the McRuer-continuity
  precedent — any new gate must keep the FINE boundary continuous.
- One dial per fly; Chad flies everything; the S-rollmix discipline chain (consult → plan
  audit → instrument-first if a new instrument is needed → red-team → card) applies.
- Tracer-fly with `roll_target_mix = 1.0` (the S-rollmix table) is the baseline feel.

## Player-facing stake (mandalark docs/KERNEL_COMS.md, 2ed1d66)

COMS-1 ("Control is king... Your nose flies a straight line to where you aim. Your
tracers go where you pointed them") is wired to the coms doc's truth rule: nothing goes
in it that the kernel doesn't actually do. THIS THREAD is what makes that sentence
literally true — until S-straightline is flown-approved, the promise is pending on the
measured 2.2-8.3 deg turn-entry dip. The thread's completion is therefore also a coms
event: notify the mandalark docs agent at seal so COMS-1's truth-check clears.

## EXECUTED 2026-07-30 — THE ATTRIBUTION FLIPPED IN FLIGHT (read before citing the stub above)

The consult + plan + audit chain ran (kernel-base prior-advice received; plan-stage fresh
red-team SOUND-WITH-FIXES; Chad's three plan-mode rulings: roll-in is the win / crab is
enough / keep gate + add FF). Then the PRE-REGISTERED INSTRUMENT (commit 1: lathold/latflick
DIP phase-split) overturned the mechanism §16-17 above attributes:

**The dip is NOT the align-gate lull + gravity sag. It is the CRAB.** Gravity can source
~0.2° of the measured 5° (the deficit column proves it live); the dip is the MB-rud rudder
sweeping the nose toward the lateral aim about the BANKED body-up axis — sin(bank) of that
sweep points at the ground (~20°/s nose-drop at 50° bank). The gravity-deficit FF (the
approved plan shape) measured 2-4% closure; killed before a fly was spent on it. Phase
split: 100% of the dip accrues in the roll window (c > 0.2), none past knife-edge — the
knife-edge worry was empirically dead too.

**The shipped mechanism (director-approved with 4 conditions, compressed re-audit
SOUND-WITH-FIXES, identity + all four sign quadrants independently verified; the parasitic
discriminator added after the additivity premise sweep caught the v2 all-component cancel
driving elevated-aim-while-banked geometries into the −G floor — an uncommanded push):**
`[regime] line_hold_ff` — AXIS-CORRECTION pitch feedforward. Exact nose-elevation
kinematics (the MB-lean frame-true pair): d(elev)/dt = pitch·cosΦθ + yaw·sin(e.phi). The FF
cancels ONLY the emitted yaw's PARASITIC vertical component — the part moving the nose AWAY
from the aim's elevation line: w_axis = −(w_dn·min(yv,0) + w_up·max(yv,0))/cosΦθ with
yv = emitted_yaw·sin(e.phi), each side fading over a ±0.087 (~5°) sag band at the line. At
the line both cancel fully (the flick's dig AND the reversal's kink); past the band the
yaw's motion TOWARD the aim is the pointing arc itself and is never fought (elevated and
below-line aims keep their commanded arcs). Keyed on the EMITTED yaw (post yaw_gate/blend),
gated blend · fwd_gate · knife_fade, AoA/G envelope = the hard wall. Gravity sag stays the
sag servo's job (feedback-plus-feedforward, no double-pay — additivity leg + zero-sag leg).
0.0 = structural off, bit-identical v11.

**PRE-STATED CLOSURES (measured before any fly, the honesty rule; final parasitic law):**
V200 world-held flicks 15/30/60° — baseline dip 2.23/5.05/8.31° → 0.92/1.08/4.50°
(closure 59/79/46%). The 15/30°
residual ~1° is plant lag (inner loop + lift build). The 60° residual is the AoA/G ENVELOPE
riding (alpha_p95 15.7° vs aoa_max 20° — MEASURED ON A ~6–10 s WINDOW, red-team P2-1: the
default 30 s lathold window dilutes the ~1 s entry transient to alpha_p95 ≈ 2.8; always
state the window with this stat) — full cancellation of a 55°/s crab at 60°+ bank is more G
than the airframe has; the entry is now an honest high-G level pull. Chad flies knowing
these numbers; the card must not promise more. V-SWEEP (red-team, offline backing beyond
V200): V140 30/60° dips 1.99/6.18°, V250 → 0.44/2.99° — monotone V-scaling, no ring, no
chatter; the reversal trace at 86° established bank holds elevation flat within ~0.1°
through the whole bank reversal (the upward kink measurably dead). A steady ~2.3°-below
park in a HELD-err 86°-bank turn (partial knife fade) joins the known-limits family
(measured MANEUVER-side companion of limit (a)).

**Documented-known-limits (Chad's ruling — recorded, NOT blessed character; each a future
thread only on a felt report):** (a) ~1.3° steady droop below the aim in a sustained
60°-bank FINE track (blend=0 there — structural scope); (b) the >~75°-bank window where
elevator cannot hold the line (top-rudder territory, MB-rud fade is a flown ruling);
(c) the PUSH branch has no complement (push-wedge down-and-lateral crabs keep their kink —
push is a commanded deep dive); (d) aim_ff's yaw_max clamp-corner asymmetry on saturated
moving sweeps (pre-existing, unowned by this thread).

Fly card: `docs/straightline_fly_card.md` (G-bite = HEADLINE row). COMS-1 clears on Chad's
stick, not the math.
