# The mouse-aim instructor cascade

The core control mechanism of the flight kernel: a "War Thunder instructor"-style mouse-aim
system where a freely-moveable cursor represents desired flight direction, and the bird's
nose chases it.

---

## Feel (Chad's own words)

> The cascade follows only the mouse-aimer input. The mouse is independent. Camera lag is
> illusion only. The nose/velocity must reach dead centre of the aimer — as the cue-ball
> goes into a pocket, hits the back edge of the hole then direct down to centre with no lag.
> Arcade-responsive, well-powered, energy-retaining, snappy to centre with minimal wobble.
> Stall/mush at low energy is legitimate physics — even bait-able gameplay.

Unpacked, in Chad's terms:

- **Only the mouse-aimer drives the cascade.** Keyboard adds on top; free-look suspends it;
  nothing else is allowed to leak into the aim law.
- **The mouse is independent.** The cursor is a world-anchored direction you set and it stays
  there — it does not get dragged around by the camera or by the bird's own motion.
- **Camera lag is illusion only.** The camera visually trails the aim/heading for a smooth,
  anti-nausea read, but it must never be mistaken for — or allowed to feed back into — the
  actual control law. The nose's approach to the cursor is real; the camera's lag is theater.
- **The pocket-shot arrival.** The nose should not crawl toward the cursor and hover near it —
  it should approach, catch the "back edge," then drop straight to dead centre with no
  residual lag or float, the way a cue ball on a good line falls the last inch into the
  pocket instead of teetering on the rim.
- **Arcade-responsive, well-powered, energy-retaining, snappy to centre, minimal wobble.**
  The bird answers quickly, holds its energy through maneuvers, and settles onto the cursor
  crisply rather than porpoising or oscillating around it.
- **Stall/mush is legitimate, not a bug.** Running out of energy and going mushy at low
  speed is honest physics — and it's allowed to be gameplay (bait an overconfident opponent
  into a stall).

---

## Principle (instructor voice)

The mechanism is two on-screen markers and one control law:

- A **cursor** (green ring) — a world-anchored direction you steer with the mouse. It is
  your *command*: where you want the nose to go. It does not chase anything; you place it
  and it stays until you move it again.
- A **nose cross** (amber) — the bird's actual boresight, projected out in front of it. This
  is the *dependent* marker: it always approaches the cursor. When they coincide, the beak
  is pointed exactly where the cursor sits.

Because the cursor is anchored in the **world** (not the screen), the nose can genuinely
*catch* it — command error can fall all the way to zero. This is the deliberate improvement
over a screen-anchored cursor, which a chase camera keeps re-centring around the nose so it
can never truly be reached.

Holding the cursor pinned at the edge of its reachable "cone" is a **sustained-turn
command** — the gap can't close, so the bird just keeps turning/looping at its own
plant-limited rate. This is what makes uncapped loops and sustained hard turns possible
(as opposed to an older attitude-command model that capped out).

The instructor is a coordinated-turn controller, not three independent axis servos:
horizontal cursor offset drives bank + rudder together (roll the lift vector onto the
target, yaw coordinated with it); vertical offset drives the elevator (pull the nose through
the target). A bank-proportional pitch feedforward pays the "altitude cost" of the current
bank *before* the sag becomes cursor error, which is the coordinated-turn analogue of a real
aircraft's nose-up trim in a turn. A heading-rate damping term (not just roll-rate damping)
is what kills the "sail past the cursor and porpoise back" failure mode — it is the missing
feedback on the bank→turn→heading double integrator.

Deadzone + expo shaping near dead-centre gives fine aim without being twitchy; full linear
(expo≈1) authority at larger offsets keeps big movements (loops) snappy rather than mushy.
A stall-band protection on the nose-up pull only eases off within a few degrees of the
actual stall angle — so a committed loop still finishes — and is bypassed entirely while
powered (flapping holds lift past stall), which is the mechanical reason a *powered* loop
always commits and an *unpowered* one can legitimately mush out (Chad's "stall is legitimate
physics" point above).

The camera's job is separate and downstream: it eases toward the aim/heading with an
exponential lag so a fast cursor throw visibly "hangs" on screen before the view catches up
(the felt lag Chad describes), while the actual nose-to-cursor closing math never reads the
camera at all.

---

## Math

Per-frame, in `computeMouseAim(dt)`:

1. **Cursor swing.** Mouse delta rotates the world-anchored `aimTargetDir` about the
   camera's up/right axes: `dYaw = -delta.X * sens`, `dPitch = ±delta.Y * sens`, where
   `sens = aimMouseSensitivity * aimAnglePerPixel`.
2. **Reachable cone / screen-circle clamp.** `aimTargetDir` is bounded either by a
   nose-relative cone (`aimLeadScreenFrac`, `aimMaxLeadDeg`) or — in the shipped
   free-cursor mode (`aimFreeCursor = true`) — is unclamped, roaming the whole screen.
3. **Body-frame error.** `localTarget = cf:VectorToObjectSpace(aimTargetDir)`; `rgt =
   localTarget.X` (horizontal error), `upc = localTarget.Y` (vertical error).
4. **Turn-rate lead** (currently shipped at `aimLeadTime = 0`, i.e. off): predicts the
   horizontal error one lead-time ahead using the bird's actual heading rate
   `headRate = pitchVel*sinB − yawVel*cosB`, scaled by `cosB·cosΔψ`, washed out to zero
   while the cursor is rim-pinned (sustained-turn command) or a keyboard axis is held.
5. **Deadzone + expo shaping** (`shapeAxis`): `hCmd = shapeAxis(hPred, aimBankDeadzone,
   aimBankExpo)`, `vCmd = shapeAxis(upc, aimPitchDeadzone, aimPitchExpo)`.
6. **Stall-band gate.** `aoaHead` rides at 1.0 until `|AoA|` is within `aimStallBandDeg` of
   `stallAngleDeg`, then eases to 0 — except while powered (`flapThrottle > 0.05`), where
   it's forced to 1 the whole time.
7. **Elevator (PD + feedforward):**
   `pitch = vCmd*aimPitchGain*speedScale − aimPitchDamp*pitchRate01`, plus a
   bank-proportional feedforward `(sec(bank) − 1) * aimBankFeedforward`, faded to zero past
   `aimBankFFTaperDeg` (the "knife-edge taper"); then `pitch *= aoaHead` if pulling nose-up.
8. **Bank + rudder (coordinated):**
   `rollP = -hCmd * aimRollGain`; `rollLevel = levelAssist − aimRollDamp*rollRate01 +
   aimHeadDamp*headRate01`; `roll = clamp(rollP + rollLevel, -1, 1)`;
   `yaw = clamp(-hCmd * aimYawGain, -1, 1)`.
9. An inversion-envelope fades only the bank-*deepening* part of `rollP` as `|bank|`
   approaches `aimRollCeilingDeg` (85°), so a far-corner cursor can't roll the bird through
   vertical and rest inverted, while recovery authority stays intact.

`pitch, roll, yaw` are then eased into `aimApplied.*` at `aimResponse` (13.0/s) in
`onFlightStep`, summed with keyboard, and fed to the flight engine.

---

## Code

Snapshot: `reference/evc2026/BirdController.client.luau`, `reference/evc2026/GameConfig.luau`.

- `computeMouseAim(dt)` — `BirdController.client.luau` — the whole instructor: cursor swing,
  clamps, error, lead, shaping, PD+feedforward elevator, coordinated roll/rudder.
- `shapeAxis(x, dead, expo)` — `BirdController.client.luau` — deadzone+expo shaper.
- `onFlightStep(dt)` — `BirdController.client.luau` — calls `computeMouseAim`, eases the
  result into `aimApplied` at `GameConfig.Controls.aimResponse`, sums keyboard on top.
- `updateCamera(dt)` — `BirdController.client.luau` — the separate, downstream camera lag
  (`aimHeadingLag`), never read by `computeMouseAim`.
- `GameConfig.Controls` (`GameConfig.luau`) — the tunable constants:
  `aimMouseSensitivity`, `aimAnglePerPixel = 0.0024`, `aimFreeCursor = true`,
  `aimRollCeilingDeg = 85`, `aimPitchGain = 8.2`, `aimRollGain = 7.5`, `aimYawGain = 1.55`,
  `aimLevelGain = 0.9`, `aimPitchDamp = 0.64`, `aimRollDamp = 0.58`, `aimHeadDamp = 0.45`,
  `aimBankFeedforward = 0.35`, `aimBankFFTaperDeg = 55`, `aimStallBandDeg = 7`,
  `aimLowSpeedGainFloor = 0.35`, `aimLeadTime = 0` (off), `aimResponse = 13.0`,
  `aimPitchDeadzone = 0.006`, `aimBankDeadzone = 0.006`, `aimPitchExpo = 1.0`,
  `aimBankExpo = 1.0`.
- `AEROBATIC_MIN_SPEED = 60` (`GameConfig.Flight`) — the airspeed floor above which loops
  unlock (Energy-Maneuverability: a slow bird mushes instead of looping).

No push/split-S commitment gate and no red off-screen aim arrow exist in this file as
snapshotted — see `push-gate-knife-edge.md` for that rung-E work, which is landed on the
current-authority C++ kernel (below) and not yet reflected here.

---

## Lineage: this is the testbed; `feel/kernel-v5` is current authority

The EvC2026 Roblox/Luau `computeMouseAim` documented above is a **prior-generation
testbed** implementation of the same mechanism. The kernel Chad is actually flight-testing
now — the one the rung-E push-gate story (`push-gate-knife-edge.md`) belongs to — is the C++
kernel at `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5` (HEAD `89447aba5` at
snapshot time, **unpushed**, diverging from `main`/v4). Snapshot:
`reference/seads-feel/control/controller.cpp`, `control/controller.h`,
`input/aim_state.h`.

The same core ideas reappear there under different names:

- A **world-anchored aim direction** (not screen-anchored), exactly like `aimTargetDir` in
  the Luau kernel — see the `Freelook` struct's doc comment in `input/aim_state.h`: "freelook
  is entirely (a) HOLDING the world-frame aim — the caller only parallel-transports it each
  tick... and (b) sending the mouse to the CAMERA instead."
- The same **mouse-suspended-during-free-look** rule (Space/freelook in EvC2026; the
  `Freelook` state machine in `aim_state.h`), including a documented easeback window (CQ2,
  ≤300 ms) during which "mouse→aim is SUSPENDED so no easing-frame (smoothed camera) basis
  ever feeds the aim — a smoothed basis in the mouse→aim loop is banned." This is the same
  camera-independence discipline as EvC2026's `updateCamera`/`computeMouseAim` split (and see
  `docs/DECISIONS.md`'s standing camera-independence constraint).
- A servo/gain vocabulary (`sqrt_law`, `K_theta`, `bank_align_power`, `w_push`,
  `w_max_pitch`) in `control/controller.cpp` that plays the same role as EvC2026's PD gains
  (`aimPitchGain`, `aimRollGain`, `aimPitchDamp`) — a coordinated pointing law with rate
  feedback, tuned through many more measured "rungs" (A, A2, C, D, E — see
  `reference/seads-feel/docs/v5_kernel_handoff.md`) than the Luau kernel's history shows.

This doc's Math/Code sections describe the Luau testbed faithfully; they are **not** a
description of `feel/kernel-v5`'s actual servo law, which is materially more evolved (see
`tuning/evc2026-v5-rungE.md` for the rung ladder). Treat the EvC2026 mechanism as "the same
idea, an earlier draft," not as current spec.
