# S3 SPEC — the sled kernel v1

**Rung:** S3, `sandbox/winter-S3` off `sandbox/winter-S2` @ `b06eb0a3e`.
**Ends in:** FIRST DRIVE.
**Law:** WINTER_LAW §3.1, §3.2, §3.3, §3.5a, §2.4c.1, §2.2a, §6b.2. INV-1, INV-3, INV-6, INV-7.

The one-line statement of this rung: **`sim/sled.*` is a second sealed kernel that reads the
same ground the aircraft reads and touches the aircraft kernel not at all**, and every feel in
§3 falls out of applying real forces at three patch positions on a rigid body — never out of a
state machine.

---

## 0. What is FIXED before a line is written

- **Depth is the fixed input** (§2.2a, SIGNED). Median bush 0.77 m, p5 0.37, p95 1.14,
  p99 1.75, trail 0.12 m. **Lowering `[snowpack] base_m` to make the sled plane is forbidden.**
  Ride feel at S3 is a `sim/sled.*` problem. The acceptance suite asserts the shipped
  `[snowpack]` values are untouched by this branch's diff.
- **Per-ski contact patches from day one** (§2.4c.1, RULED). Left and right skis sample
  `drive_surface` at their **own** positions. Suspension travel is real state.
- **Rollover is emergent** — CG height vs stance vs lateral accel vs ramp angle. No
  `if (angle > X)`. Until S8 a rollover ends with the sled at rest; **no fake dismount.**
- **"Still plowing some"** — planing is never total. A binary plow/plane flag satisfies the
  words and misses the ruling.
- **One number, two consumers** (§3.5a): `track_slip × available_snow`. The escape thrust and
  the S5 roost must read the *same* product. It is computed once and stored in the state.

---

## 1. Files

| File | Content |
|---|---|
| `sim/sled.h` | `SledInputs`, `SledParams`, `SledState`, `Patch`, `step_sled()`. Pure glm+std+world. |
| `sim/sled.cpp` | The kernel. |
| `config/sled.toml` | Every dial, strict-validated. |
| `config/load_sled.{h,cpp}` | The TOML boundary. Strict: unknown key or out-of-range ⇒ throw. |
| `test/unit/test_sled.cpp` | The acceptance legs (§6). |
| `test/golden/golden_sled.h` | The sled's own goldens (INV-7). |
| `SLED_SEAL` | One line, the sled kernel generation. Mirrors `KERNEL_SEAL`'s job. |
| `app/main.cpp` | Mode switch, sled tick, dash HUD wiring. |
| `render/readout.h`, `render/draw.cpp` | The dash plate (no airspeed, no flaps). |

`sim/step.cpp`, `sim/aero.h`, `sim/ground.h`, `sim/state.h`, `config/aircraft.toml`,
`config/controller.toml` are **NOT TOUCHED**. That is what makes the flight-golden leg a
statement rather than a hope.

## 2. Frames and conventions

Body frame follows the house table (SPEC §7): **+X right, +Y up, −Z forward**. World is
planet-centred double precision, as `SimState`. Attitude is a `dquat` body→world, integrated
and renormalized every substep exactly as the aircraft kernel does.

## 3. The model

### 3.1 Three patches, and their geometry

```
Patch 0  SkiLeft    mount ( -stance/2, 0, -ski_fwd_m )
Patch 1  SkiRight   mount ( +stance/2, 0, -ski_fwd_m )
Patch 2  Track      mount (         0, 0, +track_aft_m )
```

Mounts are body-frame offsets from the CG, at the **suspension attachment**, and each patch
hangs below its mount along body **−Y**. Contact area: skis `ski_len_m × ski_width_m`, track
`track_len_m × track_width_m`.

Every patch resolves independently against `world::SnowpackField::drive_radius_at(dir)` at its
**own** sub-point (INV-1: that is `HeightField::radius_at` + `depth_at`, never a second
surface). A bank under one ski and not the other **is** the roll moment — nothing computes
"tilt".

### 3.2 Suspension — real state, massless patch

Per patch, state carried in `SledState`:

- `susp_x[i]` — compression, m, in `[0, susp_travel_m]`
- `susp_v[i]` — compression rate, m/s

Travel is **geometric from the physical configuration**: the mount's radius against the radius
the patch would ride at (`drive_radius_at − sink[i]`). Force `N = k·x + c·ẋ`, clamped to
`N ≥ 0` (a patch in the air pushes nothing) and to a bump stop at full travel.

**Stated honestly:** this is a *massless-patch* model — no unsprung mass, so no second stiff
oscillator, so the substep budget is spent on contact rather than on wheel hop. Travel is a
physical quantity carried in state (the renderer, the HUD and the tests all read the same
number); it is not integrated from its own mass. This is a deliberate v1 simplification and is
recorded here rather than discovered later. It satisfies §2.4c.1 — the visual reads the
physical compression, and the bank-side ski compresses first because its ground is higher, not
because a clip says so.

### 3.3 Sinkage — the rate process that makes dwell the state variable (§3.5a)

Per patch, `sink_m[i]` is **state**. Bekker pressure–sinkage gives the *equilibrium* depth for
the current bearing pressure:

```
p_i     = N_i / A_i
z_eq_i  = ( p_i / (k_c/b_i + k_phi) ) ^ (1/n)          clamped to [0, depth_at]
```

and sinkage relaxes toward it while the patch **dwells**, and is shed by **travel** onto
uncompacted snow:

```
dz/dt = (z_eq - z)/tau_sink  -  z * |v_along| / refresh_len_m
```

- At rest the first term wins: the machine settles into the pack. *Dwell buries you.*
- Moving, the second term dominates: the load has moved on before the pack can respond. This
  is the load-duration statement of §3.5 written as a rate equation, and it is the reason
  §3.5's ice rule and §3.5a's deep-snow rule are **one code path**.

Sinkage is what couples to drag (§3.5) and to the shear budget (§3.4): a buried track cannot
develop slip, so the thrust that would save you is gone exactly when it is needed. That
consequence is left to fall out of the equations; **no `stuck` state machine is added at S3.**

### 3.4 Thrust — capped by snow shear, not by the engine (§3.2, §3.5a)

Mohr–Coulomb on the track patch:

```
tau_max   = c_snow + (N_track / A_track) * tan(phi_snow)
T_shear   = A_track * tau_max
```

Track surface speed is commanded by throttle, `v_track = throttle * track_speed_max`. Slip is
the classical definition, and the shear–displacement curve gives thrust:

```
slip        = (v_track - v_fwd) / max(v_track, v_eps)          in [-1, 1]
avail_snow  = clamp(depth_at / roost_ref_depth_m, 0, 1) * (1 - bury_frac)
roost_flux  = |slip| * avail_snow            <-- ★ THE ONE NUMBER
T           = T_shear * (1 - exp(-|slip| * track_len_m / shear_K_m)) * sign(slip)
T           = min(T, engine_power_w / max(v_fwd, v_eps))
```

`roost_flux` is stored in `SledState`. **S5's roost must scale off this field and nothing
else**, and the escape thrust already reads it through `avail_snow` — one product, two
consumers, per §3.5a. `bury_frac` is `clamp(sink_track / track_clearance_m, 0, 1)`: a buried
track has no free cleat to throw snow with, which is simultaneously why the roost dies and why
the escape fails.

Thrust is applied **at the track patch position**, not at the CG. Weight transfer is therefore
emergent: throttle pitches the nose up and unloads the skis, braking loads them. Nothing named
"weight transfer" appears in the code.

### 3.5 Planing lift — the central mechanism (§3.1)

Per patch, a flat-plate planing law at the patch's own attack angle:

```
L_i = 0.5 * rho_eff * v_perp^2 * A_i * sin(alpha_i) * plane_gain
```

`rho_eff` is the effective snow density the patch is skimming; `alpha_i` is the patch's pitch
relative to its local surface tangent (so a nose-up attitude planes and a nose-down one
digs — emergent, not a mode). Lift is applied at the patch, so it also pitches the machine.

Reported, never branched on: `plane_frac = sum(L_i) / (m*g)`, continuous in `[0, 1+]`.

**Displacement drag never vanishes** — that is the "still plowing some" ruling as an equation:

```
A_sub  = ski_width * max(0, min(sink_i + susp-derived immersion, depth_at))     per patch
D_plow = 0.5 * rho_eff * v^2 * A_sub * plow_cd
```

As lift rises, `N` falls, so `z_eq` falls, so `A_sub` falls — but `A_sub > 0` whenever
`depth_at > 0`. There is no speed at which the plow term is zero in snow.

### 3.6 Steering — ski lateral bite (§3.2)

Steering input sets the ski yaw angle `delta = steer * steer_max_rad`. Each ski develops a
lateral force from its **own** normal load:

```
slip_angle_i = atan2(v_lat_i, |v_fwd_i|) - delta
F_lat_i      = -N_i * mu_lat * sat(slip_angle_i / slip_ref_rad)
```

`sat` is a smooth saturation (`tanh`), so bite grows with slip angle and then washes out —
that is the loss of steering the law asks for, and on ice `mu_lat` falls with the surface class
so ice takes your steering away *by data*. Applied at the ski patch positions, so the yaw
moment and the roll moment come from the same force. The track gets a much larger lateral
resistance (a track slides sideways poorly) at its own patch.

**Rollover** is then a consequence and is asserted as one: the roll moment about the
downhill-ski contact line from lateral acceleration at `cg_height_m` against `stance_m/2`.
No threshold exists in the code.

### 3.7 Substepping — MEASURE, do not guess (the card's named unknown)

The kernel takes `dt` and an integer `substeps`. `offline_tool/measure_sled_substep.py`
(headless, drives the kernel through the harness) sweeps substeps 1…16 over the worst case
(bank-crossing at speed, stiff contact) and reports the smallest count at which the terminal
state converges to within a stated tolerance. **The shipped `[sled] substeps` is that measured
number, and the measurement output is quoted in the commit message.** A guessed 4 is exactly
the class of number this project has been burned by.

## 4. Surface coupling

`surface_at()` drives the per-class dials — `mu_lat`, `c_snow`, `phi_snow`, `rho_eff` — through
one table indexed by `world::Surface`. **No parallel surface enum.** Lake ice is
low-`mu_lat`/no-sinkage; road and rock are bare (sparks at S5); groomed trail is thin, hard,
fast — which is why §2.4's "the trail earns its use" needs no wall (INV-4).

### 4.1 ★★ THE SHORELINE CROSSING — AND A P0 THE RED TEAM CAUGHT BEFORE A LINE WAS WRITTEN

§6b.2 flagged the shoreline as "the single most likely place for a contact discontinuity, test
it on the very first drive." Red-teaming this spec against the actual render found the defect
it was pointing at, and it is **not** a discontinuity — it is a **constant 0.15 m offset over
every lake in the world**:

- The DEM bake **flattens each lake to a constant level** (`sudbury_bake.py` step 3), so
  `HeightField::radius_at` over a lake returns the flattened **lakebed**.
- The visible ice is a **separate mirror mesh at `lakebed + [water] surface_lift_m = 0.40 m`** —
  a lift that exists to clear z-fighting against the coincident lakebed and to cover ~0.17 m of
  grid facet sag.
- `SnowpackField::depth_at` currently lerps depth to `ice_snow_m = 0.25` over water, so the
  drive surface is `lakebed + 0.25`.

**⇒ the sled would ride 0.15 m below the ice the player can see, on every lake, and would
sometimes ride below the sag-dipped mesh by more.** A jolt test at the shore would have passed
— the feather makes the transition perfectly continuous — while the machine drove buried in
the ice for the whole crossing. This is the "measure the localized quantity, and measure the
right one" lesson arriving one rung early.

**Fix, in the house style — one config key, two consumers.** `SnowParams` gains `ice_lift_m`,
and `config/load_world.cpp` sets it from **the same `[water] surface_lift_m` key** it already
reads for the mirror mesh, with a loader check that the two are equal after load. `world/`
stays pure (it never learns what a render mesh is); the number is single-sourced at the TOML
boundary, so a future edit of the lift moves the ice and the drive surface together. Over full
water the drive surface becomes `lakebed + surface_lift_m + ice_snow_m` — the ice, plus the
wind-swept snow lying on it — and it lerps to ambient across the INV-8 feather like every
other water term.

The crossing leg stays, because a step is still worth excluding: it sweeps a synthetic shore
measuring the **localized vertical jolt** `dv_z` the way S0/D1's `taper_seam` leg does — the
statistic that moved 11 495× under its own mutation — **never** a band-vs-band distribution,
which is the statistic that passed while surviving its killing mutation (RT-3). It is joined
by leg 16, which asserts the *offset*: the quantity that was actually wrong.

## 5. App integration

- One key toggles **mode**: fly / drive. The sled spawns at the aircraft's ground track; the
  aircraft state is untouched while driving and vice versa. No shared state, no shared tick.
- **Dash HUD: no airspeed, no flaps.** Ground speed, engine/track state, snow depth under the
  machine, surface class, `plane_frac` as a bar, per-ski suspension compression. The plate is
  read-only over the sled state.
- Camera: chase, low, behind the machine.

## 6. Acceptance legs — each named, each with its killing mutation

| # | Leg | Asserts | Killed by |
|---|---|---|---|
| 1 | `sled_flight_goldens_unmoved` | every existing flight golden bit-identical | any edit to `sim/step.cpp` |
| 2 | `sled_seal_isolation` | `sim/sled.*` include closure excludes `sim/step.h`, `sim/aero.h`, `sim/ground.h` | adding the include |
| 3 | `sled_per_ski_asymmetry` | a bank under ONE ski ⇒ `susp_x[0] != susp_x[1]` and a nonzero roll rate | averaging the two skis into one patch |
| 4 | `sled_plane_frac_monotone` | `plane_frac` rises monotonically with v at fixed depth | dropping the v² term |
| 5 | `sled_deeper_snow_raises_planing_speed` | v at `plane_frac = 0.5` is strictly greater at 1.14 m than at 0.77 m | making lift depth-independent |
| 6 | `sled_planes_in_signed_depth` | at 0.77 m the machine reaches `plane_frac >= 0.5` below `track_speed_max` | the whole plow→plane mechanism |
| 7 | `sled_still_plows_at_speed` | `D_plow > 0` at top speed in 0.77 m | a binary plane flag zeroing it |
| 8 | `sled_thrust_shear_capped` | raising `engine_power_w` 10× does not raise steady-state thrust in bush | using engine power as the limit |
| 9 | `sled_dwell_buries_travel_sheds` | sinkage at rest > sinkage at speed, same load | dropping the refresh term |
| 10 | `sled_roost_flux_single_source` | the thrust path and the reported `roost_flux` read the same product | a parallel constant in either |
| 11 | `sled_shoreline_crossing_jolt` | `dv_z` across a lake shore under the S0 comfort bound | a discontinuous ice surface |
| 12 | `sled_rollover_emergent` | rollover occurs at a *lower* lateral accel with a higher CG, and no threshold constant exists | a scripted angle test (fails the sweep's monotonicity) |
| 13 | `sled_snowpack_dials_unmoved` | `[snowpack]` values equal the S2-signed set | lowering `base_m` to make it plane |
| 14 | `sled_substeps_converged` | the shipped substep count reproduces the 16-substep terminal state within tolerance | shipping a guessed count |
| 15 | `sled_no_second_surface` | the kernel's only ground query is `SnowpackField::drive_radius_at` | any direct `radius_at` in `sled.cpp` |
| 16 | `sled_ice_rides_on_the_mirror_mesh` | drive surface over full water == `lakebed + surface_lift_m + ice_snow_m`, and the loader keeps the lift equal to `[water] surface_lift_m` | dropping `ice_lift_m` (the shipped-today 0.15 m burial) |

Plus the standing structural gates: full `ctest` count reported, **no non-ASCII in any
`TEST_CASE` name** (grep, 4 prior incidents), `winter_plan_query.py check` exit 0,
`graphify.py --check-only` layer check OK.

## 6b. RED TEAM OF THIS SPEC — folded before building

**P0-1 — the ice offset.** Folded into §4.1 and leg 16. Caught by reading the render instead of
reasoning about the law.

**P0-2 — leg 12 was untestable as written.** "No threshold constant exists" is an assertion
about absence, which no test can make. Replaced with a **prediction**: measured rollover onset
lateral acceleration must track the static tipping formula `a_tip = g·(stance/2)/cg_height`
across a CG-height sweep, within tolerance. A scripted `if (angle > X)` cannot follow that
curve — it is flat in `cg_height` — so the sweep is its killing mutation. This is a stronger
leg than the one it replaces, because it asserts the *physics*, not the *source text*.

**P0-3 — legs 4 and 9 confound each other.** Sinkage falls with speed for **two** independent
reasons: the rate/refresh term (§3.3) and planing lift unloading the patch (§3.5). Both are
real, but a single free-running measurement cannot attribute the result to either, and one dial
can then hide the other's absence — the exact defect the S2 curvature leg had, which passed
with the term's sign flipped. **Each leg switches the other term off**: leg 4 measures lift at
frozen sinkage, leg 9 measures sinkage at zero lift. Only then does the inequality say what it
claims to say.

**P1-1 — leg 1 is not a Catch2 test.** "Flight goldens bit-identical" is already covered by the
existing suite; the real assertion is that *the aircraft kernel was not edited*. It moves to
the gate as a `git diff --name-only` check against `sim/step.cpp`, `sim/aero.h`, `sim/ground.h`,
`sim/state.h`, `config/aircraft.toml`, `config/controller.toml`, and it is reported by name.

**P1-2 — the S3 mode key is a DEV TELEPORT, and must be labelled one.** §1 rules full
embodiment — land, walk, remount — and §2.4c.1 explicitly forbids faking the dismount early.
A fly/drive toggle key is therefore **not** the shipped mechanism; it is scaffolding so the
first drive can happen before S8 exists. It is named `SEADS_SLED_DEBUG_MODE` in the code and
commented as scaffolding at both ends, so nobody later mistakes it for the ruled design and
builds the menu §1 rejects onto it. The aircraft is simply not ticked while driving.

**P1-3 — the corridor query is in the hot path now.** `depth_at` runs a `LineNetwork::nearest`
search over `corridor_search_m` 90 m, and S3 calls it 3 patches × N substeps × 60 Hz. The S2
HUD comment already anticipated this and deliberately did *not* inflate the physics radius for
a cosmetic reason. v1 calls it straight and **measures** — `[SLED_PROF]` µs/step reported in
the commit — because caching the hit per tick is an approximation and this project does not buy
approximations before it has the number that justifies them.

**P1-4 — sled goldens must not depend on the bake.** A golden recorded against
`assets/sudbury_dem.png` re-reds on every legitimate re-bake, which is how a gate gets bypassed.
`test/golden/golden_sled.h` drives an **analytic synthetic** height field and an injected
`SnowParams`, so the sled seal means "the kernel moved", never "the world moved".

**P2-1 — sinkage clamps at `depth_at`, and that is what makes deep contact rigid.** At
`sink == depth` the patch's support radius is the terrain radius exactly, so the spring keeps
compressing against bedrock rather than sinking forever. Worth stating: the clamp is not a
safety rail, it is the bottom of the snowpack.

## 7. Deliberately NOT in S3

Deformation window and track ribbon (S4), roost and sparks and audio (S5), vegetation
collision (S6), thin ice punch-through (S7), the involuntary dismount and the dig-out (S8).
A rollover or a burial ends with the machine at rest and the run over — **that is the ruled
behaviour, not a placeholder.**
