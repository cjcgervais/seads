# SESSION HANDOFF 2026-09-03 — STING RPAS ST-1..ST-3 BUILT

Lane `sting` (LANES.toml `[lanes.sting]`), worktree `D:\seads_sandboxes\sting-rpas`,
branch `sandbox/sting-rpas` off main `7b0ecf82a`. Design plan (research, architecture,
rung ladder, Chad's rulings): `D:\flight_sim2\Game_loop_idea\PLAN_STING_RPAS.md`.

## 0. Chad's ruling that opened the lane (2026-09-03, verbatim)
> We wont do the side hang / pull we now just use stand to help right the tipped over
> sled and we will leave it at that, we can defer any pull. ... plan looks good let us build

So: P is the Sting's key (the 2026-08-21 P-for-PULL side-hang ruling is RETIRED, his
word); the plan's defaults are approved.

## 1. What is built (ST-1 flight, ST-2 deploy flow, ST-3 kill — one session)

| Piece | Where | Shape |
|---|---|---|
| The drone kernel | `app/sting.h` (pure, raylib-free) | ★★★**v2 after Chad's first fly ("uncontrollable... I should be able to use mouse aim circle and it have a cascade")**: the Sting now flies the REAL instructor — its own `input::AimFrame` (transported + mouse at `cp.aim_sensitivity`, aim-rate FF bracketed) + its own `control::Internal` through `control::step` → `sim::step`, the plane's exact mouse-aim experience. Autothrottle = the bandit kCruiseTrim law at a W/S-resolved band, raised to **80/150/200 m/s** on his "faster" ruling. The v1 bank/gamma PD is DELETED, not an arm. Turn counter now reads the cascade's own `telem.load_factor`. |
| Deploy flow | `app/main.cpp` | P shoulders (Afoot + upright + left>0), mouse aims an over-shoulder view, LMB launches along the aim (30 m/s off the stock), P stows; in flight P = detonate. Mode moves ONLY through `player_mode_transition` (`DroneKey` → `DeployDrone`/`EndDrone`) |
| Mode machine | `app/player_mode.h` (loop lane's, additive, announced) | `PlayerMode::Drone`, `ModeEvent::DroneKey`, `ModeContext::drone_ready`; J/U/O refuse while flying; `autoright_legal` already refuses by construction |
| Tick threading | `app/instructor_tick.h` | `StingWorld*` defaulted-null through `tick()`/`step_frame()` (the FlakWorld pattern); flight step before the combat block; ram sweep = **(6a-sting)** after (6a-flak) |
| The kill | `combat_tick` VERBATIM | the drone's tick segment as a ONE-ROUND pool (`sting_ram_round`): swept hit, ke_damage (2000 ref @ v_ref 20 = one-shot), FX, player kill credit through `killed_spawn_indices`/`cw.kills`, maverick inert law — zero new collision code. `clear_kills=false` (third sweep of the same player: append, never erase — the flak arm's lesson) |
| Life limits | `app/sting.h` | battery 105 s; TWO LARGE TURNS = hysteretic events on the repo's true-n (`sim::load_factor`), >2.5 g held >0.7 s each, release <1.5 g, then a 3 s warble → boom; ground check app-side off `snow_field.drive_radius_at`; 3 per match (`st.left`) |
| Cameras | `app/main.cpp` | shoulder = over-shoulder aim view; flight (v2) = chase anchored on the CARRIED AIM, camera-up = the aim frame's up — the plane's "camera rotation = raw aim, 1:1" law; `SEADS_STING_CAM="dist,high,ahead"` (7.0, 2.2, 14.0) |
| Draw + HUD | `render/draw.{h,cpp}` | primitive Sting (bullet body + back DOME + 4 arms), ALLY BLUE; HUD = crosshair + `STING x3` shouldered, `BATT/TURNS/LEFT` (or `DESTRUCT`) flying; **v2: the Sting draws the plane's own neon aim ring + red nose crosshair pair (`info.sting_aim_dir`) and the AIRCRAFT's pair stands down while the drone flies** (`!info.sting_flying` gates, default-false = bit-identical) |
| Tests | `test/unit/test_sting.cpp` | mode gates + J-refusal, launch counter, battery end (flown through the real plant), one-shot detonate consumption, the turn latch driven synthetically (one long turn = ONE turn; hysteresis; brief spike = none; negative g by magnitude), ram one-shot through combat_tick, aim clamp |

## 2. Deliberate scope cuts (the plan's rungs, not oversights)
- **ST-5 art/sound**: GLB model, launcher shoulder pose (flak_gunner mechanism),
  launch/boom one-shots — NOT built. The drone is primitives; the man just stands.
- **Lead pip** on the aimed enemy: not built; the crosshair + tracer eyeballing is v1.
- **Config-file dials**: StingParams are in-code defaults; a `[sting]` scenario.toml
  block is a later slice.
- The sting is **anti-air only** (it sweeps `dw->drones` and nothing else) and is
  **not hittable** by enemy rounds.
- `st.left` refills only on app restart — a per-match reset hook is owed when a
  "new match" seam exists to hang it on.

## 3. Traps honoured (do not re-trip)
- Every mode change through the table; forced modes tear the sting down app-side
  (guard in main: active && mode != Drone → silent death).
- The walker key gate names its FOURTH consumer (Drone + shouldered) beside OnGun.
- Sting touches NOTHING of sled/walker/tape state; the man simply stands.
- combat_tick order: sting sweep AFTER the player-gun and flak sweeps, append-only.
- DrawBillboard is dead; the drone is real geometry.
- `sim/`/`control/` untouched (frozen kernel); the "different cascade" is app-side.

## 4. Next
1. Gate (`.claude/hooks/gate.sh` detached; red set == baseline BY NAME via
   `tools/gate/gate_baseline.py check`).
2. Chad flies (checklist in the session reply; exe
   `D:\seads_sandboxes\sting-rpas\build-play\seads.exe`).
3. His verdicts → dials (speed band, turn g/dwell, battery, cam triple via
   `SEADS_STING_CAM`), then ST-5 art/sound, then land per SOP.
