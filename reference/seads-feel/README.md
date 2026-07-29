# reference/seads-feel/ — PRIMARY reference: the active kernel

These files are **copies**, re-snapshotted **2026-07-28**, from `D:\flight_sim2\seads-feel`,
branch **`feel/kernel-v5`**, HEAD **`cfe1bd7fe`** (tree clean at snapshot time). This
snapshot postdates the `flight-kernel-v5` seal (`149a99c40`) and the v4→v5 reconciliation
(`main` in the game trees = `game-kernel-v5` @ `36ee936e9`), and includes the full
Chad-approved 2026-07-28 session: the rudder trim (`yaw_scale = 2.0`), S-relorient
(release fires the orient verb, `release_orient` knob), the auto-right quickening
(`inverted_delay = 0.5`), and the felt-flight recorder (`test/harness/recorder.h`, new to
this snapshot — the grafted, authoritative version the proposal copies under
`harness/seads_recorder_proposal/` were synced against). Previous snapshot: `89447aba5`,
2026-07-23 (pre-seal).

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
- `app/instructor_tick.h` — where the pieces above wire together for the live app loop.
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
