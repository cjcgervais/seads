# mandalark-kernel

This is the future home for your flight kernel — the "instructor cascade" that makes flying
feel the way you want it to feel (mouse-aim, the nose chasing the cursor, the camera lag
that's just for show).

Right now your actual, flyable kernel still lives in three other places:

- `D:\EvC2026` — the Roblox eagle/crow game (an earlier draft of the same ideas).
- `D:\SEADS_2026` — the spherical-earth dogfighting physics.
- `D:\flight_sim2\seads-feel` (branch `feel/kernel-v5`) — **the one you're actually flying
  right now**. The v5 kernel is sealed and reconciled into the main game (2026-07-24);
  new feel work (the rudder trim, release-orient) still lands on this branch first.

This repo doesn't touch any of those — it only takes dated copies of the relevant files
(`reference/`) and writes them up in plain language at four levels: how it should **feel**,
the **principle** behind it, the **math**, and exactly where in the code it lives. That's
`docs/cascade/`. There's also:

- `docs/DECISIONS.md` — the standing calls that have been made and why.
- `FEEL_LOG.md` — your flight-test journal, with tonight's checklist ready to fly.
- `tuning/` — a running record of every tuning number anyone can find, and what it means.
- `CLAUDE.md` — instructions for whichever AI session picks this repo up next.

Nothing here is live. Editing a number in `tuning/` doesn't change what you fly — it's a
paper trail, so the numbers and the reasoning behind them don't get lost between sessions.
When the kernel eventually does move here for good, this same write-up structure is what
survives the move.
