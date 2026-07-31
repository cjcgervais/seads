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

## Next actions

1. Consult packet to the mandalark kernel docs agent (prior rulings on bank_align_power,
   S7-turn2, rung-C sag servo, pitch-pull shaping; scar tissue on gate-shape changes).
2. Plan mode: candidate shape = replace/augment the cos^6 fade with bank-coordinated pitch
   FEEDFORWARD (hold the path-elevation line proactively during roll-in), sag servo kept as
   the backstop. Clarify with Chad: should the line hold EXACTLY level (zero dip target) or
   is a stated small bound acceptable; and does "rudder first" mean more lead than today's
   crab gives.
