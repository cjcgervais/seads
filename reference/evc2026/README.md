# reference/evc2026/ — point-in-time snapshot

These files are **copies**, taken 2026-07-23, of the kernel-relevant sources in the live
`D:\EvC2026` Roblox/Luau repository:

- `BirdController.client.luau` — client-side flight/input/camera controller. Contains the
  mouse-aim "instructor cascade" (`computeMouseAim`), the no-snap chase camera
  (`updateCamera`), and all reticle/render logic.
- `GameConfig.luau` — single source of truth for tunable constants, including
  `GameConfig.Controls` (the aim law's knobs) and `GameConfig.Flight` / `GameConfig.Profiles`
  (the flight envelope).
- `TouchAimAdapter.luau` — pure delta-source adapter that lets a touch drag reproduce the
  exact `GetMouseDelta()` shape the aim law consumes. Inert (`Controls.touchAim = false`)
  as of this snapshot.
- `kernel.spec.luau` — headless invariant tests for the flight kernel (no-NaN, bounded state).

**D:\EvC2026 is READ-ONLY from this repo and is the authoritative live tree.** A separate,
live Claude session is actively tuning it as "v5 / rung E" — meaning the rung-E changes
described in `docs/cascade/push-gate-knife-edge.md` (the 45° push/split-S commitment gate,
the red off-screen aim arrow) are **design decisions that may not yet be reflected in this
snapshot's code**. Where that's the case, the cascade docs say so explicitly
("per rung-E session log, not yet reflected in snapshot").

Do not hand-edit these files to "fix" them to match rung-E — re-snapshot from the live tree
when the extraction plan (see repo root `CLAUDE.md`) actually moves the kernel here.

**Correction (2026-07-23, mid-build):** the rung-E push-gate/knife-edge work is NOT actually
happening in this EvC2026 tree — it's landed on `D:\flight_sim2\seads-feel`, branch
`feel/kernel-v5`. See `reference/seads-feel/README.md`, which is now this repo's PRIMARY
reference for current flight-feel tuning. This EvC2026 snapshot remains useful as an earlier
draft of the same mouse-aim-instructor idea (see `docs/cascade/mouse-aim-instructor-cascade.md`'s
"Lineage" section) but is secondary.
