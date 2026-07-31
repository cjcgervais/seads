# FLY CARD — S-straightline (`[regime] line_hold_ff`)

**★ FLOWN-APPROVED 2026-07-30 (Chad, verbatim): "yes I really like it. This is now the
baseline for a quality flight kernel. This v11 is the standard."** (His "v11" reads as the
BUILD in his hands, not the tag — v11 is already sealed as S-rollmix, so this standard
seals as **v12**; numbering flagged in the mandalark ledger 7124fb7, the felt_flight_18 =
Golden #4 class.) COMS-1's truth-check CLEARED on this fly — "your tracers go where you
pointed them" ships true. Sealed as `flight-kernel-v12-2026-07-30`.

**The spec this card is graded against (Chad, verbatim, binding):**
> "the elevator increase should smoothly coorelate to banking increase. they are both
> supposed to align to the final turn angle at the saem time is what I hypothesize... The
> line that my tracers draw should be straight and my gunnery should have a predictable
> path via my control surface operation not accept 'character' that I need to compensate
> for. A straight line is the shortest distance, a dip is a delay."

## The flown build (fill AT FLY TIME — the table-flown-is-a-fact rule)

- Exe: `D:\flight_sim2\seads-feel\build\seads.exe` (this worktree; recon graft only at seal)
- `line_hold_ff = ___` (shipped 1.0) · `roll_target_mix = ___` (shipped 1.0)
- `git rev-parse --short HEAD` = `___`

## What changed (one dial)

The turn-entry tracer dip was ATTRIBUTED (instrument, not theory): the rudder crab sweeps
the nose toward a lateral aim about the BANKED body axis, so sin(bank) of the sweep points
at the ground. `line_hold_ff` adds exactly the elevator that cancels the crab's vertical
motion AWAY from your aim's line — the nose sweeps HORIZONTALLY to a level aim, and the
reversal's upward kink dies too. Vertical motion TOWARD your aim (an up-flick, a dive) is
never fought — it's your commanded arc. Gravity/sag handling is unchanged (sag servo).
Kill-switch:
`line_hold_ff = 0.0` = bit-identical v11. Rejection = revert-to-branch, never a softened
park.

## Pre-stated closures (measured offline BEFORE this fly — do not expect more)

| flick @ V200 | v11 dip | now | residual owner |
|---|---|---|---|
| 15° | 2.23° | 0.92° | plant lag |
| 30° | 5.05° | 1.08° | plant lag |
| 60° | 8.31° | 4.50° | the G/AoA envelope (see HEADLINE) |

## ROWS

### ★ HEADLINE — the G-bite (director condition 3: first-class, not a sentinel)
Full cancellation on a hard flick IS a high-G level pull — that is the point, and it is
the dominant NEW sensation. Hard 45–60° lateral flicks at combat speed: the entry now
BITES — the nose pulls hard to keep the sweep level, riding the AoA envelope.
**Does that G feel RIGHT (the honest airplane) or HARSH?** Verdict: ____________________

### The line (the spec row)
Tracers on (your instrument). 15/30/60° lateral flicks at V200, aim held: the line should
be STRAIGHT — no U, no dip-curl. At 60° a small residual dip (~4°, envelope-bounded) is
the pre-stated limit. Verdict: ____________________

### The reversal kink (the signed half)
Established turn, flick ACROSS to the other side: the old upward kink (nose rises while
the bank reverses) should be gone — the sweep stays level BOTH directions.
Verdict: ____________________

### Anti-climb sentinel (July-6 must not return)
Any "pushed up in a turn" during roll-in? Verdict: ____________________

### Bob / porpoise sentinel (plan-audit P3-2 + red-team P2-2)
Any pitch bobbing or pumping at TURN ENTRY (the one-sided sag-band fade is the new
dynamic), or a slow porpoise in a sustained near-knife hold with the aim just above the
nose? Offline traces at V140/200/250 show none — your hand is the real probe.
Verdict: ____________________

### Dive sentinel
60°-below flicks / split-S: must feel unchanged (the FF is silent there up to a ~1-2°
transient; push-wedge dives keep their v11 behavior by scope).
Verdict: ____________________

### Sustained-turn feel
Long held max-bank turns now ride the envelope harder (more G, more speed bleed — the
honest cost of the level sweep). Right or wrong? Verdict: ____________________

## Documented-known-limits (recorded, NOT blessed character — future threads only on a
felt report)
1. ~1.3° steady droop below the aim in a sustained 60°-bank FINE track.
2. >~75° bank: elevator cannot hold the line (top-rudder territory — MB-rud fade is a
   flown ruling).
3. PUSH branch has no complement (commanded deep dives keep their kink).
4. aim_ff clamp-corner asymmetry on saturated moving sweeps (pre-existing).

## COMS-1
Clears ONLY on your stick: if the entry dip is gone and you cannot feel the residuals
while flying gunnery, "your tracers go where you pointed them" is true as a player will
ever experience it. If you can feel them, say so — the promise waits. At seal: notify the
mandalark docs agent.
