# PACKET → r4a lane (sudburian-grip-law-r4a), from the game-loop lane — 2026-09-01

Full plan: `docs/PLAN_20260901_game_loop_millwright.md` on `sandbox/game-loop`
(worktree `D:\seads_sandboxes\game-loop`). Read §3 (architecture) and §4.1 (L1) — those are the
only parts that touch your substrate.

## What we took from you, verbatim, and built against
- Frozen surface at **adef92479**: `step_walker`, `walker_throw`, `walker_remount`,
  `WalkerInputs{forward,turn}`, `WalkerState::pos/heading`, `mode` read-only. We merged that exact
  commit into our sandbox (merge e5c2e9472; only `generated/graph/*` conflicted and was
  regenerated). We never pin `WalkerMode` values or `WalkerState` layout.
- The outer `PlayerMode {Pilot, Sled, Afoot, Repairing, OnGun}` is app-side and never decides
  whether he is upright — `WalkerState::mode` stays the sole on-foot authority.
- `player_mount_request()` is the ONE seam; its body is yours to replace at R4e. It lands in the
  same place as the KEY_R re-attach so grip and man are re-attached together (your stranded-man
  bug is the test's mutation case).
- No new code calls `sample_at` inside the tape's tap window; the walker is stepped inside the
  sim tick as R4c does it.

## What we are stubbing that is yours
- `app/walker_place.h::walker_place_afoot(WalkerState&, pos, heading)` — he appears standing at
  the dismount point (0.9 m off the left running board), asked for by enumerator name. This is the
  placement entry you said belongs to R4e. It lives in `app/`, not `sim/walker.*`, so your files
  are untouched; delete it the day the real one lands. The question of sequencing R4e sooner goes
  to Chad in our report, not to you.

## What changes on your side later (not now)
- Once he can move the machine (repair, righting, gun) the tape has to grow — your observation,
  recorded in the plan §2. We add a `PumpRepaired` event to the CONQUEST tape only; the sled tape
  is not touched by this lane.
- The wrench as a world object is queued as the next rung after this one.

## What we will not do
- Touch `sim/walker.*`, `sim/rider_grip.*`, `render/sled_model.*`, `render/rider_*`, or your
  R4a/R4c docs. Build a second remount. Work in seads-recon.

Reply to session flight-sim2-21 (or leave a note in this file's directory as
`docs/PACKET_TO_LOOP_from_r4a_*.md`) if any of the above is wrong.
