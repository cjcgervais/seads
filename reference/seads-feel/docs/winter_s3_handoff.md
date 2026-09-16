# WINTER S3 — the sled kernel: handoff (2026-08-10, after drive 1)

> **Launch line:**
> **"Continue winter S3. Run `python Game_loop_idea/winter_plan_query.py rung S3` and do the
> first thing under `drive_2026_08_10`."**

**★ THE GRAPH IS AUTHORITATIVE FOR STATUS; THIS FILE IS THE VERSIONED SNAPSHOT.** The winter plan
lives in `Game_loop_idea/winter_plan.json`, which is **not in any git repo** — so a handoff that
existed only there would be one `rm` from gone, and would not travel with the branch. Query the
graph for live state. Read this if the graph and the branch have parted company.

---

## 1. State

| | |
|---|---|
| Branch | `sandbox/winter-S3` @ `b67425118` |
| Worktree | `D:/seads_sandboxes/winter-s0` |
| Base | `sandbox/winter-S2` @ `b06eb0a3e` |
| Pushed | **No.** Tree clean. |
| Gate | **1080/1080** (S2 was 1063), graphify layer check OK, no non-ASCII `TEST_CASE` |
| Spec | `Game_loop_idea/S3_SPEC.md` (unversioned — copy at `docs/winter_s3_spec.md`) |

```
b67425118  the two drive-1 fixes (gear key, teleport mount)
a551b4fca  smoke arming hook (SEADS_SLED_DEBUG_MODE)
11c673f0a  the sled kernel
```

**INV-7 holds BY DIFF, and must keep holding.** These must stay out of every S3 diff:

```sh
git diff --name-only sandbox/winter-S2..HEAD -- \
  sim/step.cpp sim/aero.h sim/ground.h sim/state.h sim/params.h \
  sim/environment.h config/aircraft.toml config/controller.toml    # must be EMPTY
```

The sled runs **alongside** the aircraft, which is why `app::step_frame` needed no change at all.

## 2. ★ THE ONE OPEN FINDING — "it rolls right away"

Chad drove it 2026-08-10 and deferred the fix to the next session. **His call; do not pre-empt
it, and do not start by turning dials.**

**Leading hypothesis — the INPUT, not the physics.** `A`/`D` arrive through
`input::LiveInput::override_sign[2]`, which is exactly `-1` or `+1`. There is **no ramp and no
rate limit**, so every tap is an instant full-lock handlebar. The aircraft never feels this
because its axis passes through the control cascade first; the sled takes the raw value straight
into `steer_max_rad = 0.42`. The rollover probe already showed the machine letting go at steer
**0.75** at 16 m/s — and the keyboard hands it **1.00** on the first frame.

**Instrument before dialing.** That exact case:

```sh
build/seads_sled_probe.exe trace 0.77 0.6 1.0 8     # depth throttle steer seconds
```

Secondary suspects, **in this order**:

1. Steering authority probably should FALL with speed — real ski bite does. That is a *physical
   term*, not a clamp. Do not reach for it until the input is ruled out.
2. `mass_kg` 275 vs the real Indy 650 ~307 (227 dry + rider). A light machine tips sooner.
3. `cg_height_m` 0.58 may be high for an Indy; the reference photos can settle it.

**★ DO NOT CONCLUDE THE ROLLOVER MODEL IS WRONG.** It is emergent and mutation-tested, and
§2.4c.1 **rules** that too steep a side approach at speed must roll the machine. The question is
whether the rider can ask for that much steering *by accident* — not whether the physics should
answer when he does.

## 3. What drive 1 already fixed (`b67425118`)

- **The mode key was `G`, which is the LANDING GEAR** — `input/live_input.cpp:74` and
  `input/raw_input.cpp:75` both toggle `gear_down` on it. One press dropped the gear *and* jumped
  to the sled. Now **`J`**. `V` stays **RESERVED** for Chad's cockpit view.
- **The mount was a TELEPORT** — his word, and §1's own word for the failure. Mounting now
  requires the aircraft **stopped on the ground**; a refusal prints `LAND AND STOP THE AIRCRAFT
  FIRST`; the sled seeds 5 m to the side instead of inside the fuselage. This is still not
  embodiment — S8 is — but it removes the part the law rejects.

## 4. Controls (so they are not re-derived)

| | |
|---|---|
| LEFT SHIFT / LEFT CTRL | throttle up / down |
| A / D | steer left / right |
| B | brake |
| J | mount / dismount |

## 5. Standing constraints

- **DO NOT RETUNE `[snowpack]`.** Depth **0.77 m is SIGNED** (§2.2a). Plow→plane is the free
  parameter and it lives in `sim/sled.h`. Lowering `base_m` to fix ride feel re-opens a signed
  ruling by editing a value nobody would think to re-fly.
- **Dials are code-owned in v1**, deliberately. A half-wired `[sled]` TOML block splits feel dials
  between config and code — the fork pattern. Config-drive the **whole** set at S4, once the drive
  has said which dials matter.
- **No sled golden yet.** The 17 legs pin behaviour; freezing numbers Chad is about to move only
  reds the gate for bookkeeping.
- **Build Debug** (SPEC §6.1 assert-live). Full `ctest` is ~10 min — run it in the background.

## 6. The measurements, and how to reproduce them

`build/seads_sled_probe.exe [substep|plane|dwell|roll|perf|trace]` — analytic ground, no DEM, no
assets, so the numbers survive a re-bake.

- **substeps = 8**, measured. A straight-line launch converges at 3 (1.1e-3); the **stiff** case —
  a 2 m drop at 20 m/s, which is what a §2.4c snowbank produces — still reads 1.7e-3 at 6 and only
  clears at 8 (6.4e-4). **Measuring the easy case alone would have shipped 3.**
- **Plow → plane in the signed 0.77 m:** 0.316 m sunk at rest → 0.090 m under way, `plane_frac`
  0.09 → 0.61, terminal 14.0 m/s, pitch settles ~2°. Chad's "on top and plane out, though still
  plowing some" *is* 0.61 with 0.09 m still in the snow.
- **Terminal by depth:** 0.12 → 17.9, 0.37 → 15.4, 0.77 → 14.0, 1.14 → 7.5, **1.75 → 6.4 and it
  bogs** at 1.47× the free cleat clearance. §2.2a asked for that hazard to be protected; it has a
  leg.
- **Rollover, emergent:** steer-at-roll by CG height — 0.45 m **never rolls**, then 0.75 / 0.45 /
  0.35 / 0.05.
- **`[SLED_PROF]` 90.6 µs/step** — Debug, and **measured while a 14.5 GB bake was running**, so
  treat it as pessimistic and re-measure clean.

## 7. Things that cost real time — do not rediscover them

- **A P0 caught by red-teaming the spec, and NOT the one §6b.2 predicted.** The bake **flattens**
  lakes, so `radius_at` over water is the **lakebed** while the ice is the mirror mesh at
  `lakebed + [water] surface_lift_m`. S2's `depth_at` lerped to `ice_snow_m`, so the sled would
  have driven **0.15 m below visible ice on every lake in the world**. That is a **constant
  offset, not a seam** — the shoreline *jolt* test §6b.2 asks for would have **passed** while the
  machine drove buried. Fixed by `SnowParams::ice_lift_m`, single-sourced from `[water]
  surface_lift_m` at the config boundary with a loader equality check. **Lesson: "measure the
  localized quantity" is necessary, not sufficient — measure the right one.**
- **Suspension and snow are in SERIES.** Two independent springs (the suspension producing force,
  sinkage relaxing toward a Bekker equilibrium computed *from* that force) is positive feedback
  with loop gain > 1, and it porpoises forever.
- **Sinkage needs TWO timescales.** One relaxation left the machine millimetres down at every
  speed, so **0.77 m and 1.75 m drove identically** — the signed input of the whole rung with no
  effect on the ride.
- **The patch mounts sit BELOW the CG.** At CG height every patch force passes through the CG,
  `cg_height_m` does nothing, and the machine cannot tip. **§2.4c.1 is one line of geometry.**
- **Thrust needs a peak drawbar cap.** The traction limit scales with the *instantaneous* normal
  load, so a landing spike was cashed as thrust and backflipped the machine.
- **The per-tick TRACE found all four.** Terminal numbers hid them as "noise". Reach for
  `seads_sled_probe trace` before reasoning.
- **★ A MUTATION SURVIVED.** The rollover leg passed a scripted `if(bank > 60°)` — because that
  mutation still routes the roll through the dynamics, so a high CG still reaches 60° at a lower
  steer. The antipattern §2.4c.1 actually forbids is a threshold on the **INPUT**;
  `rolled = (|steer| > 0.6 && v > 10)` kills the leg instantly. **Choosing the mutation is as hard
  as writing the leg.**

## 8. Cross-agent

The **black-rock agent** was mid-bake on the evening of 2026-08-10 (`build_sudbury.py`, ~11 GB
resident), writing **only** to `D:/seads_sandboxes/barrens/assets` — `sudbury_barrens.png` and
`sudbury_cones.png`, their PNG-emission fold. **Do not touch their worktree.**

`winter-s0/assets` was untouched throughout, and **`sudbury_cones.png` is absent from this tree**,
so the barren shed is off **by data** (no `#ifdef` anywhere — the S2 design working). When the
branches eventually meet, that raster multiplies `depth`, which is now the surface the sled rides:
expect barren ground to drive **firmer and faster** than the bush, and do not read that as a
regression.

## 9. Parked for when Chad is ready (NOT before the drive is ruled)

**`OPEN-HERO-SLED` — the hero sled is a POLARIS INDY 650.** Chad's reference photos are in
`Game_loop_idea/reference_pics/` (`indy_650.jpg` … `indy_650_5.jpg`): early-90s black and red,
single headlight in the hood, "650 INDY" flank graphic, red coil-overs on the front A-arms,
tubular ski loops, red idler wheels, black seat with red piping. The S3 red rectangular prism is
an explicit stand-in — but its **pose** is real, and each runner is drawn at its own live
suspension extension.

**★ The reference also pins the KERNEL geometry**, which is the easy thing to miss: a real machine
has real dimensions, so the Blender mesh and `sim::SledParams` must **single-source** them (one
number, two consumers). The guessed dials already agree closely — stance 1.06 m vs ~1.08 m
(42.5 in IFS), track width **0.380 m vs 15 in exact**, contact ~1.22 m off a 121 in belt — with
**mass the outlier** (275 vs ~307 kg). And the running board is badged **"INDEPENDENT FRONT
SUSPENSION"**: the reference machine has the per-ski independent front end §2.4c.1 ruled
structural before S3 was built.
