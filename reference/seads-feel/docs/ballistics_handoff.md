# Ballistics leg — handoff

> **SUPERSEDED (2026-07-12) → resume from [`ballistics_forge_handoff.md`](ballistics_forge_handoff.md).**
> The drag model, world-frame pipper, tracers, and the `/ballistics-forge` loop have all LANDED on
> `sandbox/ballistics-forge` (worktree `D:\seads_sandboxes\ballistics-forge`, uncommitted). The
> "muzzle-speed reconciliation" and "make it visible" NEXT items below are DONE. Kept for the
> model+scaffold+firing-mechanism history only.

The rig-D **guns leg**: arm the Bf 109 F-4/R1 and make the battery fire. Chad's scope split this
into a **model-only + scaffold** pass (DONE), a **firing MECHANISM** pass (DONE — this commit), and
the remaining **render + sound + kill-loop** passes (NEXT). The whole leg is WORLD-thread: it NEVER
touches `sim/` or `control/` (frozen kernel) — the projectile module reads `sim::SimState` and
nothing here feeds a control/sim tick.

## DONE this commit (firing MECHANISM — pure physics + input + tick, gated, Fable-vetted)
Chad's scope: mechanism first, NO render/main.cpp/sound (avoids the live audio-agent collision on
`app/main.cpp`; each half stays gate-verifiable). What landed:
- **Input** — `input::LiveInput.fire_held` = `IsMouseButtonDown(MOUSE_BUTTON_LEFT)` (instructor
  only; raw mode keeps LMB free), forwarded frame→tick like `zoom_held`. Read now, consumed by the
  tick; the render pass reads it live off the frame.
- **Pure driver** — `weapon::GunWorld` (projectile pool + per-gun cooldown) + `weapon::fire_tick`
  (`weapon/ballistics.cpp`): advances every active round under per-round sphere-coherent gravity
  (`g·gravity_dir(pos)`, recomputed at the round each tick — Fable P1-5), retires past
  `kMaxFlightTime` OR at/under the crash sphere (`|pos| ≤ ap.R`); on `firing`, each gun off cooldown
  spawns one round and recharges by `1/rof_hz` (accumulating `+=`, exact long-run rate); a released
  trigger resets cooldowns to 0 (no spin-up). Pool reuses retired slots (bounded ≤ ~745 rounds).
- **Tick wiring** — `TickInput`/`FrameInput.fire_held` (defaulted false → strict superset); `tick()`
  and `step_frame()` take `weapon::GunWorld* gw = nullptr` (additive, nullptr-gated → the pre-guns
  path is BIT-IDENTICAL, mirror tests `test_at9`/`test_instructor_tick` unmoved). `fire_tick` runs
  ONCE per player tick beside the drone/meter advance (frame-rate independent, AT-9). Firing gated
  `fire_held && !raw_mode && !st.grounded && !grounded_tick` — inert on BOTH the crash and the
  GROUNDED reset tick (Fable C8 fold); rounds in flight keep advancing through a mode toggle /
  respawn (dead trigger on the rest of a crash frame). Reads only the const shooter — firewall clean.
- **Tests** — `test/unit/test_guns.cpp` +4 firing legs: cyclic cadence (≈T·rof+1, catches a
  per-tick or never-fire mutant), trigger-release re-arm (no spin-up), ground-retire (two-sided at
  the crash sphere), pool reuse (no unbounded growth). Gate **364/364, zero goldens moved.**
- **Fable-5 hard-math consult** (fresh context, `docs/ballistics_fable_consult.md`): **no P0s**;
  integrator/cadence/pool-bound/NaN-unreachability/firewall all CONFIRMED. C8 P2 folded (above).
  Remaining P2s (reported, not folded — cosmetic/future): terrain shoot-through (retire vs R+h);
  trigger-tapping can beat the cyclic rate; no loader guard for `rof < 1/sim_dt = 120 Hz`; a battery
  resize resets cooldowns. See the consult doc for the derivations + numbers.

## Battery (confirmed by Chad) — 5 guns, one converging trigger
Historical F-4/R1 fit, all firing on one trigger, toed-in to a harmonisation range:
- **1× hub 20mm** (Motorkanone) — fires through the prop hub.
- **2× cowl 7.92mm MG** — upper cowl deck, above & behind the hub cannon.
- **2× wing 20mm** — underwing **gondola pods** (Chad's look call).

## DONE this pass (model + scaffold)
- **Geometry** welded into `generated/bf109/build_all.py` as disjoint closed primitives (the
  `build_exhaust` precedent, no boolean union): `add_gondola` (wing_l/r), `add_cowl_mgs`
  (engine_cowl), `add_hub_muzzle` (spinner). A `_assert_no_fusion` builder tripwire (Fable P1-7)
  guards `remove_doubles` from fusing a gun vert into the host skin. Re-exported 4 GLBs
  (`wing_l, wing_r, engine_cowl, spinner`); `box_dims` reconciled in `render/rig.cpp` (wing
  `{4.6,0.58,2.40}`, engine_cowl `{1.06,1.12,1.60}`, spinner `{0.55,0.55,0.77}`). Asset validator +
  full gate green (360/360, zero goldens moved). Verified in Blender (`bf109_preview/guns_*.png`) +
  in-engine (`bf109_preview/guns_engine_*.png`).
- **Pure module** `weapon/ballistics.{h,cpp}` (namespace `weapon`, glm+std, raylib-free, in
  `seads_render_core`): `Projectile` (dvec3), `GunSpec`/`GunBattery`, `muzzle_world_dir` / `spawn`
  (velocity-inheriting) / `advance` (semi-implicit Euler, caller passes per-tick sphere-coherent
  `grav`) / `retired` (single-sources `render::kBallisticTMax`). Pinned by `test/unit/test_guns.cpp`
  (convergence, generic pose, inheritance, discrete drop, retire bounds, **fire-down-the-pipper**).
- **Config** `config/world.toml [guns]` + `cfg::WorldParams.guns` (`config/load_world.*`), with the
  strict `convergence_range_m >= 50` floor (Fable P1-3). Loaded + validated now; **consumed next
  session**.

## The 5 muzzle points (ASSEMBLY body coords: +X right / +Y up / −Z nose)
Measured from the build (`build_all.py print_muzzles`), stored in `[guns]`:
| gun     | muzzle_body (m)          | round     | speed m/s |
|---------|--------------------------|-----------|-----------|
| hub     | ( 0.000,  0.000, −2.870) | 20mm      | 800 |
| cowl_l  | (−0.160,  0.400, −2.400) | 7.92mm MG | 760 |
| cowl_r  | ( 0.160,  0.400, −2.400) | 7.92mm MG | 760 |
| wing_l  | (−2.182, −0.389, −0.250) | 20mm      | 800 |
| wing_r  | ( 2.182, −0.389, −0.250) | 20mm      | 800 |

Convergence range 300 m. World muzzle = `shooter.position + shooter.orientation * muzzle_body`;
world fire dir = `weapon::muzzle_world_dir(shooter, muzzle_body, convergence_range)`.

## NEXT — RENDER pass (steps 1–2 above are DONE as the mechanism; this is what's left to SEE it)
The mechanism fires into the pool invisibly. The render pass makes it visible + touches
`app/main.cpp` — **re-check the parallel audio agent's `main.cpp` edits and stage explicitly before
committing** (a `git add -A` from a parallel agent will otherwise sweep files). Wiring order:
1. **Build + own the battery** — assemble a `weapon::GunBattery` from `cfg.guns` (5 `GunSpec`s +
   convergence) and hold a `weapon::GunWorld` in the app; read `in.fire_held` off the live
   `LiveInput` into `FrameInput.fire_held`; pass `&gw` to `step_frame(...)`. (The tick already
   consumes both.)
2. **Tracers** — `DrawLine3D` eye-relative (the vortex-trail pattern, `render/draw.cpp` ~848) fading
   over `[guns].tracer_lifetime_s`; thread the active projectile positions into `render::FrameInfo`.
   NOTE Fable C3: a spawn-frozen grav would bow a long tracer by ~114 m over 10 s — the per-round
   recompute already in `fire_tick` is why the tracer arc is honest; don't re-derive a frozen copy
   in render.
3. **Muzzle flash** — additive, the prop-disc additive pass (`draw_prop`), at the 5 muzzle_world
   points while a gun is on cooldown-just-fired.
4. **Smoke shot** — the gate is BLIND to the app binary: `--smoke N shot.png` + eyeball tracers +
   flash. Then Chad flies.

### DO IN THE RENDER PASS — muzzle-speed reconciliation (Fable P1, now quantified)
The pipper (`scenario.toml [gunsight].muzzle_speed = 850`) solves faster than the rounds fly (20 mm
800, 7.92 mm 760). Fable's number: on a 150 m/s crossing target at 500 m the lead trails by **5.5 m
(20 mm) / 10.5 m (7.92 mm)** — a full fighter-width miss on exactly the deflection shots the pipper
is for. Fix = set `[gunsight].muzzle_speed = 800` (the `load_scenario.cpp:116` tripwire needs
≥ 2·v_redline + g·tmax = 588.1, so 800 passes; even 760 would), or solve the pipper per gun class.
Do it in the render pass so it's visible + so any gunsight golden move is caught then. Harmless until
firing is visible (the mechanism rounds already fly correctly at their own speed).

## THEN — sound
`render/gun_audio.h` pure map (the flight-audio pattern) + app glue; read-only off the firing state.
Use the `/flight-audio` skill — COORDINATE with the parallel audio agent (shared `app/main.cpp` +
synth headers).

## THEN — kill-loop pass
Add drone HP + projectile-vs-drone hit detection (`drone::DroneState` has NO HP today — respawns on
crash only) + destroy/respawn on kill. New gameplay state; mutate drone state only, never the kernel.

## Fable pre-consult (this plan) — SOUND-WITH-FIXES, all folded
0 P0; 8 P1 folded: single-source `kBallisticTMax`; `dvec3` projectile; per-tick sphere-coherent grav;
`convergence_range >= 50` floor; assembly-frame muzzle offsets; builder fusion tripwire; test dodges
the fixture-no-op trap (wing-gun convergence, generic pose, discrete-drop pin) + the fire-down-the-
pipper absolute cross-check; muzzle-speed reconciliation named above.
