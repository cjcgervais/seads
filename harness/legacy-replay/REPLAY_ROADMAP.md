# Replay roadmap

Two kernels, two very different replay stories.

## SEADS C++ kernel — TRACTABLE (see `seads_recorder_proposal/`)

The SEADS kernel is already built for headless, deterministic replay:

- `app::tick` / `app::step_frame` (`app/instructor_tick.h`) are pure functions of
  `(LoopState, TickInput, params)` — no globals, header-only, driven headlessly
  all over `test/unit/`.
- The fixed-dt accumulator (`app/loop.h`) gives a clean tick seam, and
  `test_at9.cpp` already proves identical tick-indexed input ⇒ bit-identical
  trajectory.

So true input-replay needs **no extraction** — just a tap at the existing seam.
That tap is the `seads_recorder_proposal/` (recorder.h + INTEGRATION.md + a
differential test). Hand it to the seads-feel session; the roadmap there is:

1. Add the read-only `on_tick` tap inside `step_frame` (or drive the loop from
   `main.cpp` for a zero-touch firewall diff).
2. Add `test_recorder_firewall.cpp` to the ctest gate.
3. Record a flight (`--record out.seadsrec`), replay it in a `test_replay.cpp`
   leg, emit `CtrlCsvTelemetry` columns, diff with `replay_diff.py`.

## Roblox "Eagles vs Crows" kernel — ENTANGLED (extraction needed)

The EvC flight logic is **not** cleanly replayable headlessly today. `FlightPhysics`
itself IS pure (`FlightPhysics.new(profile, pos)` + `engine:Update(dt, input)`,
no Roblox instances — see its header and `tests/kernel.spec.luau`, which already
drives it headlessly under lune). The problem is the **instructor cascade**, which
lives inside `BirdController.client.luau` and is deeply tied to Roblox runtime
state. To feed recorded aimer input through the kernel headlessly, these would
need extracting from `BirdController` into a pure module (say
`src/shared/MouseAimInstructor.luau`), taking plain values in and returning input
out — no `Instance`, `Camera`, `UserInputService`, or `_G` reads:

- **`computeMouseAim`** — the core: turns the per-frame `GetMouseDelta()` into
  `aimTargetDir` (world cursor) and the applied pitch/roll/yaw. It currently reads
  `UserInputService:GetMouseDelta()`, the camera basis, and several module locals.
  Extract as `f(state, mouseDx, mouseDy, birdCF, camBasis, dt, cfg) -> aimApplied`.
  (`TouchAimAdapter.luau` already isolates the *delta source* above this read —
  the same seam a replay would inject recorded deltas at.)
- **`computeMouseSteer`** — the virtual-stick spring/return (`steer`), currently a
  closure over module locals. Extract as a pure step over an explicit `steer` state.
- **The push_gate / horizon-enter-exit hysteresis + split-S commitment logic** —
  the `horizon_enter 45.0 / exit 40.0` band and the commit ruling (grep `push`,
  `horizon`, `commit` in `BirdController`). These read the bird's live nose/velocity
  and camera; extract as `f(nose, vel, aim, gateState, cfg) -> gateState'`.
- **`onFlightStep`** — the per-frame orchestrator that sums keyboard + mouse-aim
  into the `inputState` fed to `FlightPhysics:Update`. Its *pure core* (the summing
  + ramps) is what a headless replay would call; the Instance reads (finding the
  bird, drawing the reticle, camera) stay in `BirdController`.

Until that extraction happens, the EvC side does **capture-and-diff replay** (this
harness) — record the trajectory + inputs, diff after a change — but not
**bit-exact input-replay through the kernel**. That's the honest gap. The capture
harness is deliberately built so that when the extraction lands, the recorded
`mouseDX/mouseDY` (already captured per frame) + the fed `aim`/gate state can drive
the extracted pure functions directly.

### Why not force it now
`BirdController.client.luau` is ~4200 lines, sits hard against Luau's 200
module-local cap (its own comments document bricking the client by adding two
locals), and is owned by another live session on an unpushed branch. Extraction is
a real refactor with a compile-cap budget — it must be that session's deliberate
move, not a drive-by from the harness.
