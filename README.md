# mandalark-kernel

This is the future home for your flight kernel — the "instructor cascade" that makes flying
feel the way you want it to feel (mouse-aim, the nose chasing the cursor, the camera lag
that's just for show).

Right now your actual, flyable kernel still lives in three other places:

- `D:\EvC2026` — the Roblox eagle/crow game (an earlier draft of the same ideas).
- `D:\SEADS_2026` — the spherical-earth dogfighting physics.
- `D:\flight_sim2\seads-feel` (a worktree of `D:\flight_sim2\seads`) — **the one you're actually
  flying right now**. Kernel v15 (`kernel-v15-righthand-signed`, 2026-09-13) is on `main` and
  on the `seads-recon` fly tree. New feel work lands on short-lived `feel/<name>` lanes
  through the sentinel protocol (`docs/SENTINEL_LEDGER.md`); the v16 candidate for the
  lateral nose-down is on hold for your ruling.

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
