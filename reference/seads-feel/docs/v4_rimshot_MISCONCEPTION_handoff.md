# RIMSHOT CAPTURE — THE FLICK MISCONCEPTION (handoff to the next Fable session)

> ✅ **RESOLVED 2026-07-17 (same day): S-rimshot v2 UNIVERSAL landed on `auto/kernel-v4`.**
> The redesign this document mandates is DONE — plan mode + Chad's AskUserQuestion rulings
> (re-flick = abandon-and-chase; one bounce per arrival, lockstep is the only quiet state),
> two plan-stage consult rounds, two diff red-team rounds, 13 mutants, golden re-recorded
> with signature verification. Evidence: `docs/v4_ledger.tsv` row 2.5; the fly card is
> CARD 2 in `docs/v4_fly_cards.md`. This document is HISTORICAL — kept as the record of
> the misconception and Chad's verbatim rulings.

**Read this BEFORE touching the capture mechanism (`[capture]` / S-rimshot / control/controller.cpp
CaptureState). Chad flagged a structural misconception in the rung-2 build, 2026-07-17. The
mechanism as shipped is architecturally wrong — not a tuning miss, a wrong FRAME.**

Branch `auto/kernel-v4`, worktree `D:\flight_sim2\seads-v4`. Tree is CLEAN at `f738b5a04`
(gate 342/342). The rimshot code IS committed there but its trigger must be redesigned.

---

## THE MISCONCEPTION (in one sentence)

**The rimshot was built as a big-FLICK-gated EVENT (`snap_on = 30°` arm threshold). Chad never
once said "flick." He asked for the carry-through-rebound-to-dead-center as the UNIVERSAL way
the nose settles onto the aim — for ANY deflection at all. The flick framing was an assumption
the authoring session invented, and Chad traces it to a MISREADING of his earlier kernel asks.**

Chad's exact words this session:
- "it slows down outside the circle — the nose should go direct to the opposite side of the
  circle from which it approaches, hit the rim and reflect immediate to dead centre."
- "**fable got this all wrong** — I mean **any deflection at all** the mechanism should be there
  to snap to centre as I asked. **I didn't only ask this for flicks. Fable made a wrong
  assumption here.**"

Go back and re-read his ORIGINAL asks (program.md §Goal ask 2; the commit messages on
`999bf0199`, `562788c5b`, `8041b8afb`): "the nose crosshair should respond fully across the
mouse-aim circle and overshoot such that it nearly touches the opposite end… rebound settling
into the centre as a basketball hits the round edge of the rim." That is a description of how
the nose SETTLES onto the aim — the basketball dropping through the rim — **for every
approach**, not a special move for large sweeps. The word "flick" appears nowhere in any of
Chad's asks. It was introduced by the implementer as a gating concept and then hardened into
`snap_on`/`snap_off` thresholds and a whole ARM/ENGAGE/CARRY/RETURN event machine keyed on
"a big deflection that then parks." That gate is the misconception.

## CHAD'S RULING ON THE TRIGGER (2026-07-17, verbatim decision)

Asked point-blank "when should the rebound trigger — on settle only, or always including
mid-track?", Chad chose: **"Always, even mid-track."** The nose should be carrying-through and
rebounding toward wherever he points **at all times** — it is the pointing law's approach
behavior, not an event that arms on a condition. There is NO flick gate, NO size threshold, and
NOT EVEN a park/settle gate. Whenever the nose is closing on the aim, it must not slow in — it
carries through to the far rim of the reticle circle and rebounds to dead center.

## WHAT THIS MEANS ARCHITECTURALLY (guidance, not a spec — you design it, plan-mode + ask Chad)

- **Kill the flick gate.** `snap_on`/`snap_off` as an ARM threshold must go. (An uncommitted
  experiment lowering `snap_on 30→5` was made and DISCARDED — it was still the wrong frame and it
  broke the dead-blow arrest test, because the re-arm logic on line ~226 `err > snap_on → re-arm`
  is COUPLED to the threshold and corrupts CARRY once `snap_on < engage_e ≈ 5.6°`. Do not just
  re-lower a number; the whole gate concept is wrong.)
- **Kill the park-edge trigger too.** `park_edge = cap_prev_moved && !in.aim_moved` makes it a
  "fire when the hand stops" event. Chad ruled "always, even mid-track" — the mechanism runs
  whether or not the mouse is moving.
- **This is very likely a reshape of the near-center POINTING LAW, not a discrete event.** The
  attribution (ledger 2.0) already proved the slow-in is the K_theta linear/parabolic taper
  below ~6-8° — the `seek_law` near-center branch that tapers rate → 0 as e → 0. That taper is
  what Chad feels as "slows down outside the circle." The rimshot should REPLACE that taper for
  every approach: instead of rate ∝ error (exponential creep into center), carry the closing
  rate through the aim, overshoot to the far rim of the circle, and rebound to dead center with
  the full-authority τ-surface arrest (that arrest law — `min(return_w, sqrt(2·α·d))` + release
  when `d ≤ w·τ`, τ = I/(K_w+damp·q_eff) — is CORRECT and hard-won; KEEP it, it lands the nose
  dead at center with no creep. See ledger 2.3 "the SURPRISE.")
- Carry-through needs STATE (a memoryless law can't "carry" — it needs to remember it's mid-
  rebound), so a small always-on state element keyed on **"the nose is converging on the aim"**
  (closing rate > 0 with a gap to close) is the right shape — NOT keyed on "a flick parked."
- **Compose with rung 1 (S-aimff).** During active tracking the aim-rate feedforward already
  LEADS the moving aim; the rimshot handles the SETTLE into center. Both always on = the "always,
  even mid-track" feel. Think about their interaction (the FF supplies lead rate; the rimshot
  shapes the terminal approach). This is the crux of getting "always" right without it fighting
  the track.

## THE HONEST WALL — say it to Chad, do NOT silently gate it away

"Always, even mid-track" is exactly the regime the research memo (docs/v4_research.md trap #10)
warns rebounds can turn into WOBBLE during smooth continuous tracking. **Chad has RULED he wants
it anyway.** Build it, let his STICK judge. If it wobbles:
- Do NOT solve it by re-introducing a flick/size/park gate. That is the original sin. Chad will
  read any "only fires when X is big enough" as the same wrong assumption again.
- The legitimate solution space (all preserve "any deflection, always"): make the overshoot
  amplitude scale so it is imperceptible on a smoothly-tracked small gap but crisp on a genuine
  settle; engage the carry only when the nose's closing rate exceeds what the FF is already
  supplying (so a well-led smooth track has nothing to "carry through"); shape the rebound so a
  continuously-moving center never produces a full reversal. Explore WITH Chad, one dial, he flies.

## PROCESS (CLAUDE.md — this is the expensive-mistake zone)

This is a frozen-kernel feel change. **ENTER PLAN MODE. ASK Chad explicit clarifying questions
BEFORE coding.** Confirm the FELT behavior in his words — he thinks in feel (basketball, rim,
snap, dead center), never in thresholds or state machines. Every candidate is dial-gated
(carry=0 = bit-identical off), one dial at a time, Chad flies each. The rimshot MOTION he already
validated conceptually ("hit the rim and reflect to dead centre") — the arrest law is right —
only the TRIGGER/scope is wrong. Fix the scope, keep the motion.

## What is still correct in the current code (don't throw it out)

- The τ-surface arrest (dead-blow landing at center, no creep) — ledger 2.3, KEEP.
- The full-authority `sqrt(2·α·d)` return ceiling + no-linear-floor — KEEP.
- The fresh-axis re-capture at CARRY→RETURN (off-plane fix) — KEEP.
- The carry = 0 structural-off / bit-identical discipline — KEEP (goldens must stay unmoved).
- circle_deg = 0.55 (the real 9px reticle radius) — KEEP.
Only the ARM/trigger scope (snap_on/snap_off/park_edge) is the misconception.

## Provenance
Written by the Opus 4.8 session 2026-07-17 that Chad switched to after the Fable rung-2 build,
when he caught the flick misconception. The uncommitted snap=5 experiment was reverted; tree
is clean/green at f738b5a04. Live status of the rest of v4: docs/v4_handoff.md (rungs 1 & 3 —
S-aimff and S-globelook — are NOT affected by this; they stand as landed & await Chad's fly).
