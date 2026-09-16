# GOLDEN FELT FLIGHT #5 — S-straightline / sealed kernel v12

**Named by Chad, 2026-07-30 (~22:23): "the second flight that I just recorded is the new
golden flight. I was in thin air for my first attempt."**

## Identity (from the tape's own data)

- Canonical tape: `test/golden/felt/golden_5_straightline_v12.seadsrec`
  (preserved byte-identical from `build-play/felt_flight_20.seadsrec`, 22:23,
  2,436,038 bytes).
- `recverify`: **sig_ok=1**, ticks=6696 (~55.8 s at sim_dt 1/120),
  **telem_ticks=6696** — the FIRST golden in history with the boundary state
  (`telem_blend`/`telem_held_bank`) natively on every tick of the tape (the
  recorder-v2 dividend).
- Build: `tag=sandbox/kernel-v5-reconcile@game-kernel-v5-46-g2116f6ea3` — the v12 graft
  commit exactly; `line_hold_ff = 1.0` confirmed in the flown config.
- The rejected first attempt (`felt_flight_19.seadsrec`, 22:22, 4528 ticks, sig_ok=1) is
  the "thin air" take — left in build-play/, NOT canonical.

## ⚠ Kernel-stamp correction (hand-recorded per KERNEL_SEAL's own protocol)

The tape header reads `kernel=flight-kernel-v10-2026-07-30` — **STALE**: `KERNEL_SEAL`
was not updated at the v11 (01b28231a) or v12 (2116f6ea3) grafts (its banner requires
same-commit updates; both missed it — the hand-maintained-constant class its banner
predicts). **The kernel actually flown is `flight-kernel-v12-2026-07-30`** (proven by the
`tag=` field naming the v12 graft commit, which is signature-sealed into the tape).
`KERNEL_SEAL` is corrected to v12 in the same commit that lands this file; every future
tape stamps correctly. The v10 stamp on this tape is a known misprint, superseded by this
identity note — do not re-record the golden to fix a header.

## Context

Flown the same night S-straightline was flown-approved and sealed as v12 (Chad verbatim:
"yes I really like it. This is now the baseline for a quality flight kernel"), with
COMS-1's truth-check cleared on the stick. Candidate new predicate for the mandalark
pipeline: dip depth from the nose-elevation pins — "the line is straight" as a number
future kernels regress against.
