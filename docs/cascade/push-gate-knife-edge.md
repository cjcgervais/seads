# The push/split-S commitment gate — rung-E's 45° knife edge

> **Status: LANDED on `feel/kernel-v5` (the active kernel), NOT reflected in the
> `reference/evc2026/` Roblox snapshot.** The canonical implementation lives in
> `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5`, commit `89447aba5` (2026-07-23,
> **unpushed** at snapshot time). This is the real, shipped rung-E mechanism — see
> `reference/seads-feel/control/controller.cpp` and `reference/seads-feel/render/draw.cpp`,
> snapshotted 2026-07-23. It does **not** exist in `reference/evc2026/BirdController.client.luau`
> — the Roblox/Luau kernel is a prior-generation testbed; `feel/kernel-v5` is current
> authority. `feel/kernel-v5` has not merged to `main` (still v4) — see the reconciliation
> watch-item in `docs/DECISIONS.md` and `CLAUDE.md`.

---

## Feel

You're in a hard turn with the mouse held out to one side. Somewhere in that drag your aim
dipped slightly below the horizon — not a deliberate dive command, just noise in a long
lateral hold. Suddenly the bird commits to a nose-down push, or worse, rolls into a
split-S you never asked for. That's the "mystery dive" bug: a small, accidental dip low
gets read as "go straight down," and the airplane obeys.

What it should feel like instead: pure lateral holds — however long, however hard — never
push the nose down on their own. Nose-down only happens when you actually put the cursor
meaningfully below the horizon, not when a sideways drag happens to graze it. And when you
*do* want a committed dive or a split-S, a big flick — hard left/right AND well down — should
commit cleanly and let you roll all the way through, past inverted, out the bottom.

## Principle

The commitment gate ("push mode") is a state machine (`ns.push_mode` in `controller.cpp`)
gated by several conjuncts, two of which are the ones rung-E touched:

- `aim_elev` — the aim direction's **world elevation** (`dot(aim_world, local_up)`),
  bank-independent by construction, so a hard lateral flick that only *looks* body-low
  because the plane is banked is not misread as "nose down."
- A **side cone** (`aim_side = |dot(aim_world, vplane_n)|`, where `vplane_n =
  cross(nose, local_up)`) that confines push (pure pitch-down, no roll) to aims near the
  plane's own vertical plane — a pure dive sits inside the cone and pitches; a
  down-and-to-the-side flick sits outside it and rolls to track instead.

Both gates existed before rung E. The bug: `push_horizon_enter` was **1.0°** below the
horizon (`push_horizon_exit` at **-1.5°**). At that tolerance, the side cone **degenerates**
near-astern (the vertical plane itself contains the astern direction), so a big lateral
deflection whose aim had drifted only a hair low — invisibly, off-screen, along the
aim-frame's own arc, not from any deliberate down input — became push-eligible exactly when
the bank was far over. That is the mystery dive: Chad's report was "past a certain
deflection it still noses down... I think it's the knife edge set too high on the horizon,"
and the attribution (`v5_kernel_handoff.md`, RUNG E) traced it precisely to this 1° gate plus
the near-astern cone degeneracy.

**The fix (commit `89447aba5`):** `push_horizon_enter` moves 1.0° → **45.0°**;
`push_horizon_exit` moves -1.5° → **40.0°** (a 5° hysteresis band so the gate doesn't
chatter at the boundary). Commitment now requires the aim to be genuinely **45°+ below the
horizon**, not a one-degree graze. Measured sanity check (`latflick 148/70`): a pure lateral
hold produces `push == 0` on every row; adding a real down input (`down=50`, i.e. aim ~50°
below horizon) rolls the aircraft through past inverted — the split-S, commanded by real
down input, preserved exactly (bankErr 118° stays under the 120° bank guard, so the
roll-through owns the maneuver).

**The companion display problem:** once push-eligibility genuinely requires the aim to reach
45°+ below horizon, a player has to be able to tell when their aim has drifted there,
especially off-screen (a long lateral drag curves below the horizon along the aim-frame's
arc without the player ever seeing it happen). The fix is a **red off-screen aim arrow**,
drawn at the screen edge whenever the true aim direction is off-screen, pointing toward it.
It is a pure display addition computed from the same shared camera-screen projection basis
the reticle itself uses (never a re-derived projection — the "projection-basis-fork"
lesson) — the reticle itself is **never** clamped or substituted (the "S-retclamp" lesson: a
pinned edge marker replacing the reticle under-reports the true error). The arrow makes the
dip visible; the 45° gate makes it harmless even when unnoticed.

## Math

- **Commitment gate**, both directions, hysteresis-banded:
  - Enter push mode when: `have_bank && |bank_eff| > push_gate_bank && elev < 0 &&
    target_body.z <= push_down_z_enter && aim_elev < push_horizon_enter && aim_side <
    push_side_enter`.
  - Exit push mode when any of: `elev >= 0 || !have_bank || |bank_eff| <=
    push_gate_bank_lo || target_body.z > push_down_z_exit || aim_elev > push_horizon_exit ||
    aim_side > push_side_exit`.
  - Rung E changed exactly two of these constants: `push_horizon_enter: 1.0° → 45.0°`,
    `push_horizon_exit: -1.5° → 40.0°` (both measured as degrees below the world horizon; a
    5° band between the two prevents chatter at the boundary). All other conjuncts
    (`push_gate_bank`, the side-cone `aim_side` bounds, `target_body.z` bounds) are
    unchanged by this rung.
  - `aim_side` itself: `vplane_n = normalize(cross(nose, local_up))`; `aim_side =
    |dot(normalize(aim_world), vplane_n)|` — zero for an aim exactly in the plane's own
    vertical plane (a pure dive), growing as the aim swings out of it (a lateral flick).
    Degenerate (treated as in-plane) at vertical flight, where `nose ∥ local_up`.
- **Not in push mode** (the ordinary pointing law), a separate `align` term keeps the pull
  from climbing during a partial roll-in: `align = max(0, cos(bank_eff))^bank_align_power`,
  and `pitch = max(w_push * ((1-blend) + blend*align), w_min_pitch)` — unrelated to rung E's
  change but the branch rung E's gate falls back to when not committed.
- **Off-screen arrow geometry:** off-screen test = `!ret.in_front || |ret.x| > 1 ||
  |ret.y - lens_shift_ndc| > 1` (NDC space). Direction: project the true (unclamped) aim onto
  the shared screen basis `(sb.r, sb.u)`, normalize to a unit screen-space direction, then
  find the ray/screen-rect intersection inset by a fixed pixel margin (26 px) so the arrowhead
  sits just inside the frame edge, oriented outward toward the true aim.

## Code

Snapshot: `reference/seads-feel/control/controller.cpp`, `reference/seads-feel/render/draw.cpp`.

- `ns.push_mode` (state) and the enter/exit conjuncts above — `control/controller.cpp`,
  around the "World-horizon gate (§7 Item 2...)" comment block.
- `cp.push_horizon_enter = 45.0`, `cp.push_horizon_exit = 40.0` — declared in
  `control/params.h` (as `push_horizon_enter`/`push_horizon_exit`, defaulted to 0.0 in the
  struct) and given their shipped values in `config/controller.toml` (search
  `horizon_enter`/`horizon_exit`).
- `aim_side`, `vplane_n = cross(e.nose, e.local_up)` — `control/controller.cpp`, the
  "Sideways cone (§7 Item 2 refinement...)" block, immediately above the push-mode gate.
- `align = pow(max(0, cos(bank_eff)), cp.bank_align_power)` — `control/controller.cpp`, the
  non-push (`else`) branch immediately following the gate.
- The off-screen red arrow — `render/draw.cpp`, the "v5 OFF-SCREEN AIM ARROW" block
  (references `camera_screen_basis`, `info.reticle_dir`, `info.lens_shift_ndc`).
- `render/orient_cues.h/.cpp`, `render/camera.h/.cpp` — the shared camera/screen-projection
  machinery the arrow and reticle both draw from (snapshotted alongside `draw.cpp` for
  context; not modified by rung E).
- Full attribution + measured before/after grids: `reference/seads-feel/docs/v5_kernel_handoff.md`,
  section "RUNG E — THE KNIFE EDGE + THE RED ARROW."

No equivalent gate exists in the EvC2026 Roblox snapshot (`reference/evc2026/`) — see that
directory's README for why (a separate, prior-generation testbed kernel).
