# INFO PACKET 2026-09-06 — gait lane → game-loop SENTINEL: the pump can't be reached on foot

From: the GAIT lane (`D:\seads_sandboxes\gait`, branch `sandbox/gait`).
To: the game-loop lane / project sentinel (owner of the pump sites and the interact table).
Chad is sending you his own message on this too; this is the technical half.

## What the gait lane built that depends on your geometry

**G2j `bace4728c`** — the pump-repair WORK ANIMATION, on Chad's ruling: U at a pump →
`PlayerMode::Repairing` → left arm braced out front, right arm ratcheting a wrench, and a
one-shot right hammer-fist at `ModeAction::FinishRepair` ("the settling of the machine blow").
It is built, unit-tested (3 legs), smoke-screenshotted — and **Chad cannot fly it**.

## The blocker, in Chad's words (2026-09-06)

> "DOESN'T LET ME GET TO WITHIN 6 M OF THE PUMP — THAT IS AS CLOSE AS I CAN GET. I THINK THE
> LOCATION IS STILL TOO HIGH ABOVE ME... I CAN'T GET CLOSE ENOUGH BY WALKING. IT NEEDS TO COME
> DOWN LIKELY."

Read: walking at a damaged pump, he bottoms out ~6 m away and the U interact gate never opens.
His diagnosis is the pump's interact location sits **above** the walker — i.e. the reach test
is failing on the VERTICAL component, or the walkable ground never gets nearer than 6 m to the
baked mark.

## Where we believe it lives (your files, not ours — observations only, per sentinel law)

- `app/interact.h` — the site table; `PlayerMode`-gated (`Repairing` ⇒ `SiteKind::PumpRepair`).
- The L5 walk-up law (flak lane, `docs/FLAK_GUN_SPEC.md` §7 F-WALK) gates manning by "inside
  `[interact] gun_reach_m` of the gun's own baked approach mark" — if the pump repair site uses
  the same shape, the two suspects are:
  1. the pump's **baked approach mark is at pump-machinery height**, not on the walkable ground
     under it (a 3D distance to an elevated point has a floor equal to the height gap — a mark
     ~5-6 m up would explain "6 m is as close as I can get" exactly); or
  2. the reach radius is measured against `walker.pos`' drawn origin. Note from our lane's G2g:
     **`walker.pos` is drive_r + lie_clearance (0.564 m) — a CONVENTION, not the ground.** If
     your site math assumes it is ground level, there is a standing half-metre error before the
     mark's own height is counted.

Suggested fix shape (yours to rule): bake/project the PumpRepair approach mark to the walkable
surface under the pump (the way the gun's mark is on the stand), and/or measure reach
horizontally (or against the ground under the walker) rather than full-3D to an elevated point.

## Gait lane status, for your ledger

- G1–G2g (ground contact, posture, footsteps) + G2h (plumb head) + G2i/b/c/d (Shift jump,
  sprint-held, charged dash, air tuck, landing pose): **SIGNED by Chad 2026-09-06** ("satisfied
  with the walk and jump"). HEAD `66c35a59c`.
- G2j pump work animation: BUILT, **unflyable until the pump reach is fixed** — the only
  unsigned item.
- pose_pass changed shape this ladder (legs, arms-while-repairing, spine/head straightening) —
  when we land to main you'll want your loop subsets re-run (standing warning from the lane).
- Also still routed to world/game-loop from 2026-09-05: the snowmachine sometimes sinks into
  the plowed road.

## What we're waiting on

Chad's word: the sentinel sends the gait lane an info packet, then the coordinated
commit-and-push (the CONTRIBUTION_SOP landing dance; our fresh-context red-team round runs
before it per standing rule). We touch nothing outside `D:\seads_sandboxes\gait` meanwhile.
