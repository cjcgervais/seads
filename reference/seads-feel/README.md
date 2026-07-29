# reference/seads-feel/ — PRIMARY reference: the active kernel

These files are **copies**, re-snapshotted **2026-07-29**, from `D:\flight_sim2\seads-feel`,
branch **`feel/kernel-v5`**, HEAD **`51eb5b9e3`** — the **`flight-kernel-v7-2026-07-29`**
seal (tree clean at snapshot time). This snapshot postdates the v5 seal (`149a99c40`), the
v4→v5 reconciliation (`game-kernel-v5` @ `36ee936e9`), and the v6 seal (`cfe1bd7fe`).

**New since the v6 snapshot — the sealed v7 content** (Chad, 2026-07-29: "now it is
precisely perfect"): the **S-relorient ADDENDUM** (the D9 exception retired — a freelook
release with override keys still held now fires the full orient verb;
`[freelook] release_orient_with_keys`) and **S-keychase** (while keys fly and freelook is
not held, the chase camera's rest target becomes the flight path instead of the parked aim;
`[camera] key_anchor_rate = 6.0`). Both are optional-with-default-off knobs, so v6
behaviour is reachable one line at a time.

**`app/main.cpp` is new to this snapshot** (816 lines). It was added because two separate
sessions needed the freelook-orbit decay, the `orient_fired` camera cut, and the
`ease_chase_forward` / `chase_anchor` call site — none of which live in
`app/instructor_tick.h` — and had to read them out of the live tree instead. The cascade
docs cite symbols in it; it belongs here.

**Two files carry a post-seal, docs-only correction** (`7650dc6d0`, same day):
`render/camera.h` and `test/harness/comfort.h` are taken from that commit rather than from
the seal, because the sealed versions carry a **superseded framing** — they described
S-keychase as a comfort fix and the parked aim as a target the pilot had "left." Both are
wrong: the parked aim is the pilot's deliberate plan for the release, and the
behind-velocity anchor is a **gunnery** reference (it makes the nose-versus-velocity gun line
legible). Verified: **every non-comment line in both files is byte-identical to the seal** —
`7650dc6d0` changed comments and docs only, zero executable code, gate unmoved at 388/388.
So this snapshot is the v7 seal's *code* exactly, with only the corrected commentary. See
`docs/cascade/camera-anchor-mode-duality.md`.

Previous snapshots: `cfe1bd7fe` (2026-07-28, v6 seal), `89447aba5` (2026-07-23, pre-seal).

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
  release decay, the `orient_fired` hard cut of `cam_fwd`, the `chase_anchor`/
  `ease_chase_forward` call site (S-keychase), focus loss, and the accumulator loop.
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
