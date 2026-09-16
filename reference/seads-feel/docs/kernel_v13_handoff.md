# Kernel v13 handoff — the automatic-comfort round (CLOSED, signed 2026-08-06)

Chad's closing verdict on fly-6: "very good." Round SIGNED. Branch
`sandbox/kernel-v5-reconcile`, commits `7ed1629e0..8a1f7c1c7`, tag
`kernel-v13f-signed`, all pushed. Gate 958/958, controller/flight goldens
unmoved the entire round, pursuit cascade untouched.

## ★ v13g addendum (same day, evening — fly-7 FLOWN AND SIGNED "that flew well")

Commit `e15c090ae` (this branch; NOT pushed to origin — archive push only).
Gate 958/958, goldens unmoved. Two things, per Chad's fly-7 asks:

- **The rest-edge roll EASES IN** ("the motion should ease in and out rather
  than being jarring"): new `[horizon_recovery] ease_in = 500` deg/s^2 —
  `input::HorizonRecovery::step` grew a per-capture ramp state `w` that climbs
  from 0 at `accel` instead of launching at the full 150 deg/s on the capture
  tick (~0.3 s to full rate). The `settle` ease-out tail is bit-identical once
  ramped (the min tracks the falling cap exactly); the freelook-RELEASE roll
  stays INSTANT (the v9 ruling). `accel` is a defaulted 4th param (0 = legacy
  instant launch) so every existing caller/test is untouched — strict-superset.
  Loader: `ease_in` required, walled >= 0.
- **The whole arm chain lowered** (Chad ruled "all 3" on the AskUserQuestion):
  `rest_dwell` 0.10 -> **0.05** s, `straight_max` 6 -> **9** deg/s,
  `path_band` 10 -> **15** deg.

Dials now: `rate 150 / settle 5 / ease_in 500 / rest_dwell 0.05 / arm_min 0 /
path_band 15 / straight_max 9`. Walk-back additions: start too abrupt ->
`ease_in` down (~300); big debt feels lazy -> `ease_in` up (~800); false fires
mid-carve -> `path_band`/`straight_max` back down independently; refuses to
arm on a truly still hand (1-px sensor jitter margin) -> `rest_dwell` 0.07,
not 0.10.

The loader-rejection anchor-string trap bit a THIRD time (the negative-dwell
leg keys on the exact `rest_dwell = 0.05    # [s] mouse-still` line text —
re-key it on ANY rewording of that TOML line).

## ★ Kernel propagation (same evening, Chad's ask — the reconciliation is DONE)

`sandbox/kernel-v5-reconcile` (recon) is confirmed the AUTHORITATIVE superset
line — it already contained feel's grafted truedepth/straightline/graphify
work, the tunnel history, and all but two fields-forge commits. Merged into:

- **seads-feel** `feel/kernel-v5` @ `d9b882dec` — 26 conflicts, ALL resolved
  toward recon (overlap of feel's own commits with recon's grafts of the same
  work). Gate 958/958.
- **seads-tunnel** `sandbox/graphify` @ `01cfc4422` — kernel files merged
  CLEAN; only glue conflicted (kept the tree's own CLAUDE.md/gate.sh; graph
  regenerated with its 12 modules). Gate 958/958.
- **world** `sandbox/fields-forge` @ `39faace5f` — the one real
  reconciliation: recon had HALF-absorbed `cc61dbe8a` (hero-viz N-cycle yes,
  M reassigned to the bubble map 2026-07-25). Kept recon's N=viz/M=map,
  re-applied only the wingtip-SMOKE half. ⚠ OPEN ITEM: **the smoke toggle is
  keyless now (always-on)** — Chad to pick a key. The audio agent's
  uncommitted files (bagpipe/wind/engine_synth-delete) converged with recon's
  committed versions; originals preserved in stash
  `pre-kernel-merge: live audio work`. Gate 958/958.
- **bfm-r1** — fast-forwarded (recon already contained BFM rung 1).
- **SKIPPED deliberately**: `sandbox/ai-tape` — the ACTIVE flight-recorder
  team (AI players, testing+building) lives on the recon line; their branch is
  ONE commit behind v13g and fast-forwards cleanly when THEY choose. Never
  move their checkout mid-session. Also skipped: ballistics-forge +
  planet-art (old uncommitted feature work — clobber risk; stash-protect the
  same way if Chad asks).
- Archive `mandalark-kernel-seads` synced + PUSHED `d946be5..9ab47a7`.

## What v13 is (final state, all flown)

One theme: the pilot never pays cognitive load for orientation housekeeping.

- `[freelook] easeback_time = 0` — the 0.3 s post-release mouse→aim pause is
  retired. Mouse is live on the release tick. Loader wall relaxed to >= 0; the
  mechanism and its <= 300 ms cap survive for any re-arm.
- `[freelook] orient_double_tap_s = 0.30` — retired as redundant in the
  morning, RESTORED on the stick in fly-4 ("keep the space double tap"): the
  manual "put me back together" echo — instant whole-debt right, no dwell, no
  gates. Every airborne release still fires the verb automatically.
- `[auto_level] inverted_delay = 0` — inverted righting arms on the FIRST rest
  tick (reverses "an inverted rest stays inverted"; `inverted_rate = 0` is the
  way back). Active-hands inverted flight is still never fought.
- REST-EDGE CAMERA HORIZON RECOVERY (new mechanism, `app/instructor_tick.h`,
  reviving `input::HorizonRecovery`): at settled rest the carried aim/camera
  frame rolls its horizon debt out level about its own forward — 150 deg/s,
  settle-5 eased tail, ONE SMOOTH MOTION (a captured roll completes; mouse
  motion never shreds it — the v13b fix for the felt "steps" from resting-hand
  1-px jitter). Settled == ALL of:
    - mouse still `rest_dwell = 0.10` s,
    - debt above `arm_min = 0` deg (fly-4: ANY tilt from flat rights; the
      ~0.57 deg finish epsilon is the only floor; dial + machinery survive
      test-pinned for the walk-back),
    - aim resolved on the FLIGHT PATH within `path_band = 10` deg (this IS
      "mouse and nose resolved" when straight — nose rides the path within
      AoA; the camera never reads body attitude or keys, velocity is the one
      legal read),
    - path STRAIGHT: rotation rate <= `straight_max = 6` deg/s (great-circle
      V/R ~0.6 passes, loader-walled above `v_redline/R`; real turns block).
  Cancels outright: freelook entry, GROUNDED, focus loss.

## Dials (all `[horizon_recovery]`, TOML-only, no rebuild)

`rate 150 / settle 5 / rest_dwell 0.10 / arm_min 0 / path_band 10 /
straight_max 6`. Walk-back order if over-eager: `straight_max` down first
(symptom: fires at turn-exit), `rest_dwell` up second (symptom: fires the
instant the hand pauses).

## Lessons banked this round (full text in the test/TOML comments)

- Cancel-on-any-motion + re-dwell turns resting-hand sensor jitter into felt
  "steps" — a captured open-loop roll should COMPLETE (the release-roll
  precedent). The hand owns the capture, never the shredder.
- The smooth-motion stall counter must read the physical DEBT, not the latch
  (`recov.remaining` is 0 exactly during the mutant's parks) — first draft
  passed its own mutant; fixture-no-op class.
- A yawed-off aim is never crossed by an elevator loop's path sweep — in-plane
  (pitch) offsets or the crossing pin goes vacuous.
- Loader-rejection tests key on exact TOML line text — re-key them whenever a
  dial line is reworded (bit twice in one session).

## Where things live

- Fly tree: this repo, `build-play/seads.exe` (rebuilt at v13d; v13e/f were
  TOML-only).
- Archive: https://github.com/cjcgervais/mandalark-kernel-seads (private) —
  bare-bones SPEC.md + the kernel proper + graphify code graph in place of
  docs; synced through v13g (`9ab47a7`, local clone D:\mandalark-kernel-seads).
  Refresh = recopy from here, rerun `python tools/graph/graphify.py`, keep
  SPEC bare per its own §10.
- Open question Chad raised mid-round (his call, unresolved): whether future
  kernel rounds belong in seads-feel instead of seads-recon.
