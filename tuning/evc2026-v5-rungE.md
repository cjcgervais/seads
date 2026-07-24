# Tuning capture: EvC2026 constants + the seads-feel "v5 rung ladder"

**Read this split first.** This file has two unrelated sources of truth:

1. **§1 — EvC2026 constants**, pulled from `GameConfig.luau` / `BirdController.client.luau`
   in the Roblox/Luau prior-generation testbed kernel (`reference/evc2026/`).
2. **§2 — The seads-feel "v5 rung ladder"**, the REAL tuning history behind the rung-E story
   (push-gate 45° knife edge, etc.). This lives entirely in the C++ kernel at
   `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5` (`reference/seads-feel/`) — it does
   **not** exist in EvC2026's `GameConfig.luau`. An earlier draft of this file conflated the
   two; §2 below is corrected against the actual source (`git log`,
   `docs/v5_kernel_handoff.md`, `config/*.toml`) as of 2026-07-23.

---

## §1 — EvC2026 (`GameConfig.luau`) constants

All values as snapshotted 2026-07-23 in `reference/evc2026/GameConfig.luau`.

### `GameConfig.Flight` (global environment)

| Constant | Value | Meaning |
|---|---|---|
| `METERS_PER_STUD` | 0.28 | Roblox scale constant |
| `REAL_G` | 9.81 | m/s² |
| `GRAVITY_G` | 1.3 | fraction of real gravity — the sweep knob |
| `LEGACY_GRAVITY` | 196.2 | the gravity the v2 feel was calibrated at |
| `AIR_DENSITY` | 0.044 × GRAV_SCALE | abstract lift/drag density scalar |
| `CONTROL_AUTHORITY_MIN_SPEED` | 20 | studs/s where control authority ≈ 0 |
| `CONTROL_AUTHORITY_FULL_SPEED` | 90 | studs/s where control authority is full |
| `AEROBATIC_MIN_SPEED` | 60 | studs/s above which loops/rolls unlock |
| `TERMINAL_VELOCITY` | 420 | flap-achievable top speed (studs/s) |
| `STABILITY_RATE` | 0 | weathervane strength — zeroed (no auto-level) |
| `GRIP_RATE` | 2.2 | rad/s base, velocity-follows-nose alignment assist |
| `PHUGOID_DAMPING` | 2.1 | vertical-motion (porpoise) damping |
| `SERVICE_CEILING` | meters(2000) ≈ 7143 studs | envelope top at climbCeilingBonus=1.0 |
| `CEILING_BAND` | meters(170) ≈ 607 studs | band below ceiling where flap power fades to 0 |
| `FLAP_AUTHORITY_FLOOR` | 0.6 | min control authority while flapping |
| `FLAP_LIFT_FLOOR` | 0.55 | lift fraction retained past stall while flapping |
| `FLAP_DIVE_FADE_BAND` | 0.4 | top fraction of diveSpeedCap where dive flap thrust fades |
| `FLAP_DIVE_FLOOR` | 0.1 | min dive flap thrust fraction at the very top end |
| `AEROBATIC_SPEED_BAND` | 40 | studs/s ramp band above AEROBATIC_MIN_SPEED |
| `CRASH_SPEED` | 45 | studs/s closing speed at/above which terrain impact is lethal |

### `GameConfig.Profiles.Eagle` (energy fighter)

| Constant | Value | Meaning |
|---|---|---|
| `mass` | 16 | momentum / energy-retention |
| `wingLoading` | 1.0 | high = wide turns, strong glide/dive |
| `cl0` | 0.30 | lift at zero AoA |
| `liftSlopePerDeg` | 0.11 | lift-curve slope |
| `clMax` | 1.95 | peak lift coefficient |
| `postStallCl` | 0.70 | lift retained past stall |
| `stallPadDeg` | 6 | degrees over which lift rolls off past stall |
| `parasiticDrag` | 0.020 | low → good glide/dive |
| `inducedDragK` | 0.58 | Cd_i = k·CL² |
| `stallAngleDeg` | 27 | critical AoA |
| `flapThrust` | 900 × GRAV_SCALE | forward force per flap |
| `flapClimbForce` | 500 × GRAV_SCALE | climb-rate knob |
| `flapDragRetention` | 0.6 | drag-bleed cancelled while flapping |
| `energyRetention` | 0.82 | fraction of climb energy gravity would otherwise take |
| `glideBleed` | 0.15 | per-sec exponential decay of speed above cruise while gliding |
| `glideSpeed` | 130 | cruise |
| `maxLevelSpeed` | 170 | level-flight cap |
| `diveThrustBonus` | 1.6 | thrust multiplier while diving |
| `diveSpeedCap` | 390 | pure-dive top speed |
| `rollRate` | 3.2 | rad/s |
| `pitchRate` | 2.0 | rad/s |
| `yawRate` | 1.1 | rad/s |
| `sustainedTurnRate` | 1.0 | rad/s sustained level-turn ceiling |
| `rollInertia` | 0.55 | 0..1 control sluggishness |
| `maxStamina` | 320 | flap budget |
| `staminaFlapCost` | 5 | per sec at full up-throttle |
| `staminaDiveCost` | 4 | per sec of powered dive |
| `staminaRegen` | 16 | per sec while gliding |
| `climbCeilingBonus` | 1.0 | relative climb ceiling |
| `health` | 99 | 3 hits at eagleHits=3 |
| `beakDamage` | 26 | |
| `talonDamage` | 34 | |
| `diveAttackMultiplier` | 3.46 | stoop lethality multiplier |
| `bodyRadius` | 4.5 | studs |

### `GameConfig.Profiles.Crow` (angles fighter — for contrast)

`mass=3.0`, `wingLoading=0.55`, `clMax=1.6`, `stallAngleDeg=24`, `flapThrust=185×GRAV_SCALE`,
`energyRetention=0.4`, `glideBleed=0.5`, `glideSpeed=95`, `maxLevelSpeed=140`,
`diveSpeedCap=220`, `rollRate=5.0`, `pitchRate=3.6`, `yawRate=1.8`, `sustainedTurnRate=2.1`,
`rollInertia=0.20`, `maxStamina=70`, `health=25`, `beakDamage=9`, `talonDamage=12`,
`bodyRadius=2.2`.

### `GameConfig.Controls` (the mouse-aim instructor — see `docs/cascade/mouse-aim-instructor-cascade.md`)

| Constant | Value | Meaning |
|---|---|---|
| `mouseAim` | true | mouse-aim is the shipped default control mode |
| `aimMouseSensitivity` | 1.0 | overall cursor-swing gain |
| `aimAnglePerPixel` | 0.0024 | rad the cursor swings per mouse unit |
| `aimLeadScreenFrac` | 0.95 | nose-cone size (load-bearing overshoot stabilizer) |
| `aimMaxLeadDeg` | 70 | hard cap on the lead half-angle |
| `beakReticleDistance` | 90 | studs the beak cross is drawn/measured at |
| `talonReticleDistance` | 70 | studs the talon arc is drawn below the belly |
| `talonArcSpreadDeg` | 60 | half-spread of the talon arc |
| `aimResponse` | 13.0 | per-sec ease of applied input toward target |
| `aimPitchDeadzone` | 0.006 | |
| `aimPitchExpo` | 1.0 | linear |
| `aimBankDeadzone` | 0.006 | |
| `aimBankExpo` | 1.0 | linear |
| `aimCursorAreaFrac` | 0.5 | screen-circle clamp area fraction (inert under `aimFreeCursor`) |
| `aimCursorEdgeMarginPx` | 48 | |
| `aimRollGain` | 7.5 | bank-to-turn strength |
| `aimPitchGain` | 8.2 | elevator pull strength |
| `aimYawGain` | 1.55 | coordinated rudder |
| `aimLevelGain` | 0.9 | wings-level assist |
| `aimPitchDamp` | 0.64 | PD pitch-rate damping |
| `aimRollDamp` | 0.58 | PD roll-rate damping |
| `aimHeadDamp` | 0.45 | heading-rate damping (the sail-past/porpoise fix) |
| `aimBankFeedforward` | 0.35 | bank-proportional nose-up feedforward |
| `aimBankFFTaperDeg` | 55 | knife-edge taper for the feedforward |
| `aimLeadTime` | 0 | turn-rate lead — OFF (shipped); machinery kept, untested at nonzero |
| `aimLeadWashRate` | 6 | per-sec ease of the lead's washout |
| `aimFreeCursor` | true | bypasses both cursor clamps; camera lag re-centres visually |
| `aimRollCeilingDeg` | 85 | inversion-guard soft shoulder |
| `aimStallBandDeg` | 7 | degrees below stall where nose-up pull eases |
| `aimLowSpeedGainFloor` | 0.35 | min pitch-gain multiplier at very low airspeed |
| `aimSpeedGainRef` | 1.0 | multiple of glideSpeed for full gain |
| `flapThrottleRate` | 1.1 | units/sec throttle ramp rate |

### `GameConfig.Camera` (partial — chase/no-snap constants seen)

`chaseTurnRate = 3.6` (rad/s cap on chase-direction slew), `lookAheadFactor = 0.3` (blend
toward velocity vs. nose for the chase direction).

**No `horizon_enter`/`horizon_exit`, `k_induced`, `T_max`, `n_max`, `pull_floor`,
`depth_frac`, or `capture_carry` constants exist anywhere in `GameConfig.luau` or
`BirdController.client.luau`** — confirmed by direct search of the snapshot. Those are §2's
constants, from the actual C++ kernel.

---

## §2 — The seads-feel "v5 rung ladder" (current authority, `feel/kernel-v5`)

Source: `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5`, `git log`; shipped values from
`config/controller.toml` / `config/aircraft.toml`; attribution from
`reference/seads-feel/docs/v5_kernel_handoff.md`. **This branch is unpushed and diverges
from `main` (still v4)** — see the reconciliation watch-item in `docs/DECISIONS.md`.

Each rung: dial, shipped value, the "walk it back" OFF/pre-rung value, and Chad's ruling
where the commit message carries one.

| Rung | Dial | Pre-rung / OFF value | Shipped (current) value | Chad's ruling |
|---|---|---|---|---|
| A — S-truedepth (`dc0b1d01c`) | `capture_depth_frac` | `≤ 0` (structural OFF — the v4 fixed-rim behavior) | `1.5` | (attribution rung — momentum-earned glance depth, no direct quote) |
| A2 — sub-wall curve (`c8e98afa3`) | `capture_depth_pow` | `1.0` (= rung A bit-identically; `≤ 0` rejected by the loader) | `2.0` | "better, just needs a little more" (fly-1 verdict on small/fine adjustments) |
| C — hold-the-line (`f523177e9`) | `K_aoa` | `5.0` (pre-C1) | `10.0` | "It should hold the line of my mouse inputs and try to get to my mouse until full stall — it might sink a bit as I begin the stall but the nose should stay where my mouse is asking." |
| C — hold-the-line (`f523177e9`) | `pull_floor` | `0.0` (structural OFF — bit-identical legacy tree) | `1.0` | (same ruling as above; the S-holdline sag servo this dial arms) |
| D — arcade energy model (`c0625ede1`) | `k_induced` | `0.05` (the PRE-rung-D value you walk back TO — NOT rung D itself) | `0.015` | "give me the power — I had been intuiting all along that the airframe is being underserved" (supersedes Chad's own 2026-07-08 `T_max 9000` ruling) |
| D — arcade energy model (`c0625ede1`) | `T_max` | `9000` (pre-rung-D; T/W 0.31) | `18000` (T/W 0.61) | same ruling as above |
| D — arcade energy model (`c0625ede1`) | `n_max` | `16` (pre-rung-D; capped pitch ~57 dps at 148 m/s) | `32` (the wing-rip G-cap) | same ruling as above |
| E — knife edge (`89447aba5`) | `push_horizon_enter` | `1.0°` below horizon | `45.0°` below horizon | "I think it's the knife edge set too high on the horizon... nose-down with the bank over should need my down input too, past like 45 degrees" |
| E — knife edge (`89447aba5`) | `push_horizon_exit` | `-1.5°` | `40.0°` | (paired with the enter threshold above, 5° hysteresis band) |

**Global kill switch, all rungs' machinery:** `capture_carry = 0.0` is the structural OFF for
the entire v3/v4 capture-arrival mechanism (every capture block skipped; the state machine
stays IDLE forever) — walking `capture_carry`, `pull_floor`, and `capture_depth_frac` all to
0 together walks the whole rung-A-through-D stack back to the pre-rung-A (v4-approved,
`d68de7d91`) baseline, layer by layer, independent of the rung-E push-gate change (which is
a separate mechanism/dial pair).

**Rung ladder order** (full commit list, `git -C D:\flight_sim2\seads-feel log --oneline`,
newest first, `feel/kernel-v5` only): E (`89447aba5`) → D (`c0625ede1`) → C red-team folds
(`c32688986`) → C landed (`f523177e9`) → C spec (`545925ec9`) → A2 (`c8e98afa3`) → lessons
(`cc5584680`) → A red-team folds (`382418939`) → A (`dc0b1d01c`) → baseline v4 approved
(`d68de7d91`).
