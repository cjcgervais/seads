# reference/seads-feel/ — PRIMARY reference: the active kernel

These files are **copies**, re-snapshotted **2026-07-29 (night)**, from
`D:\flight_sim2\seads-feel`, branch **`feel/kernel-v5`**, HEAD **`29787debc`** — the
**`flight-kernel-v9-2026-07-29`** seal (tag verified to resolve to that commit; tree clean
at snapshot time; tag and branch pushed to origin). This is a **FLOWN seal**: Chad flew the
build and approved all three fly-card conditions the same day. This snapshot postdates the
v5 seal (`149a99c40`), the reconciliation (`game-kernel-v5` @ `36ee936e9`), the v6 seal
(`cfe1bd7fe`), the v7 seal (`51eb5b9e3`), and the **v8 seal (`ae7ae8f23`) — which was
deliberately never snapshotted here** (partially rejected on the stick; a pre-fly seal).

**New since the v7 snapshot — the sealed v8+v9 content:**

- **v8 S-keyprec** (flown, KEPT): override keys are camera-inert. S-keychase is retired —
  `render::ease_chase_forward`'s call stays branch-free on key state (a standing review bar;
  no ctest can catch a violation there).
- **v9 S-nosesnap** (flown, APPROVED — the close of the four-round camera arc):
  - Freelook **welds aim := nose unconditionally** (entry, keys or not); the no-keys
    "parked carve" is retired by Chad's ruling. `orient_snap_dir` returns the **nose**, so
    the release is a no-op on the aim by construction.
  - Release is **one instant snap**: camera behind the plane, **upright to the horizon**,
    aim and nose in view — the whole S7-hrz up-debt retired in the release tick, no eased
    roll-in. Chad's ruling of record: "Snap to view upon release of freelook, no eased
    anything."
  - The **CQ2 0.30 s easeback window is KEPT by ruling** with a new cockpit rationale
    ("I need that .4s to observe / orient myself") — it is not dead-rationale debt; do not
    flip it in a cleanup.
- **New in `docs/`:** `v9_fly_cards.md` (Chad's approval conditions), and the measured
  evidence grids `v9_comfort_baseline_v8.txt` / `v9_comfort_after.txt` — the v8→v9
  before/after for the pre-registered decision rule (headline: `nose_at_fire` 18.552→0.578,
  `updebt_after_release` 44.904→0.003, no-keys drift 75.5°→1.46°). The rulings ledger is
  `D:\mandalark-kernel\docs\DECISIONS.md` (four 2026-07-29 entries + the flown verdict);
  the live tree carries a byte-copy in its own `docs/DECISIONS.md`.

Gate at the seal: **388/388**, zero moved goldens. Grafted to seads-recon
`sandbox/kernel-v5-reconcile` @ `6058329d3` the same day (recon gate 901/901, comfort
numbers reproduce bit-identically) — the play build flies v9.

Previous snapshots: `51eb5b9e3` (2026-07-29, v7 seal), `cfe1bd7fe` (2026-07-28, v6 seal),
`89447aba5` (2026-07-23, pre-seal). The v7 snapshot's note about `render/camera.h` /
`test/harness/comfort.h` carrying post-seal docs-only corrections (`7650dc6d0`) is
obsolete: this snapshot takes every file from the seal commit itself, and the v9 red-team
sweep (`a307a8a69`) folded the corrected framing into the sealed docs.

**This is the kernel Chad is actually flying.** It supersedes both other reference
directories in this repo as the primary source of truth for current flight-feel work:

- `reference/evc2026/` (Roblox/Luau) — a prior-generation **testbed**. The same ideas
  (world-anchored mouse aim, camera-independent control) appear there first, but the actual
  mechanism Chad is tuning now is this one.
- `reference/seads/` (`D:\SEADS_2026`) — a different, separate C++ kernel: the pure
  spherical-earth navigation/combat physics (great-circle math, `R=15000` sphere). Still
  useful for the non-euclidean-geometry cascade entry; not where the feel-tuning rung ladder
  lives.

## What's here

- `docs/v5_kernel_handoff.md` — **the crown jewel.** Full BEFORE/AFTER measured metric grids
  for every rung (A, A2, C, D, E) tied to Chad's felt verdicts and rulings, quoted verbatim.
  Read this before touching any dial.
- `test/harness/{harness_main.cpp, telemetry.h, instructor.h, comfort.h, injector.h,
  scenarios.h, recorder.h}` — the flight-test harness (deterministic scripted maneuvers:
  `track`, `nudge`, `lathold`, reversal probes) used to produce every measured grid in the
  handoff doc, plus the felt-flight recorder (the `.seadsrec` format behind `goldens/`).
- `control/{controller.h, controller.cpp, params.h, transport.h, extract.h}` — the instructor
  cascade itself: the push-gate state machine, the capture/arrival servo (rungs A/A2), the
  hold-the-line AoA servo (rung C), and every dial the handoff doc references.
- `sim/{params.h, state.h, step.h, step.cpp, world.h, aero.h, invariants.h}` — the flight
  plant: aerodynamics (`k_induced`, `T_max`, `n_max` — rung D), integration, world state.
- `input/{aim_state.h, aim_curve.h, aim_frame.h}` — the world-anchored aim direction and
  freelook state machine (the C++ analogue of EvC2026's `aimTargetDir`/free-look).
- `render/{camera.h, camera.cpp, orient_cues.h, orient_cues.cpp, draw.h, draw.cpp}` — the
  camera and the rung-E off-screen red aim arrow (`draw.cpp`, search "OFF-SCREEN AIM ARROW").
- `app/instructor_tick.h` — where the pieces above wire together for the live app loop
  (the per-TICK half: freelook rules, the orient verbs, the S7-hrz capture).
- `app/main.cpp` — the per-FRAME caller: device polling, the freelook-orbit camera and its
  release decay, the `orient_fired` hard cut (as of v9: forward AND up, one instant snap),
  the `ease_chase_forward` call site (branch-free on key state — standing review bar),
  focus loss, and the accumulator loop.
  **Read this whenever a camera question isn't answered by `instructor_tick.h`** — the
  orbit and `cam_fwd` glue is here, not there.
- `config/{controller.toml, aircraft.toml, load_controller.h/.cpp, load_aircraft.h/.cpp}` —
  the actual shipped dial values (this is where `push_horizon_enter = 45.0`, `K_aoa = 10.0`,
  `k_induced = 0.015`, etc. actually live, with Chad's rulings quoted inline as comments).

## Constraints

**`D:\flight_sim2\seads-feel` is READ-ONLY from this repo** — no writes, no git commands
there (read-only `git log`/`git status` for research is fine; nothing that touches the
working tree). It is the authoritative live tree; a live session may move past rung E at any
time. Re-snapshot before trusting anything here as still-current — check the live branch
tip against the HEAD named at the top of this file (see the live-branch watch-item in
`docs/DECISIONS.md` and `CLAUDE.md`). New feel work lands on `feel/kernel-v5` before it
reaches the game trees' `main` — baselining on `main` alone lags the feel thread.
