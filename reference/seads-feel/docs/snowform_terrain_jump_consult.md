# CONSULT — QUALITY ADDED TERRAIN + THE BIG JUMP (2026-08-26)

> **Provenance.** Chad's deferred overnight consult, run 2026-08-26 as an orchestrated 9-agent workflow:
> four independent lanes (jump geometry · terrain vocabulary · visibility · ride), each adversarially
> red-teamed against the standard that killed two prior revisions of the snowform plan, then synthesised.
> 1.13 M subagent tokens, 326 tool calls. Every lane read this tree; numbers carry `file:line`.
>
> **⛔ THE JUMP HALF IS HISTORY (2026-08-26).** Chad ruled on all four questions, the jumps were built, driven and
> **REVERTED** (`590968241`) -- see `docs/snow_build_handoff.md` §1. Do not resurrect any of it.
> **What is STILL LIVE in this document:** the terrain half, the red team's kill list, and above all the shader
> finding (snow heroes draw through the mono BUILDING shader: +2.0 DN at high sun, -140.1 DN at night). That one
> explains Chad's "grey in color" and it shapes the snow build's first rung.
>
> **Status: ANALYSED, NOT RULED.** Nothing here is built. The "FOR CHAD TO RULE" section at the end is the
> gate — item 1 blocks all of (b) and nothing can be estimated until he picks.
>
> Packet inputs: `docs/steer_lean_hud_handoff.md`, `docs/snowform_measurements.md` (M1..M12),
> `C:\Users\Chad\.claude\plans\declarative-jumping-conway.md` rev 3, `config/world.toml [snowhill]`,
> `world/snowhill.*`, `tools/snowhill_mesh.cpp`, `sim/sled.h`, `test/unit/test_snowhill*.cpp`.

---

# THE ANSWER

**(b) The big jump is buildable now, and it is data, not mechanism.** Everything Chad's ask needs is already expressible in the shipped `snowhill_add` vocabulary — `SnowhillPeak{dx, dy, h, sx, sy, rot}` (world/snowhill.h:31-34), summed, windowed to exact zero at `r_cut_m` (world/snowhill.h:46-56), drawn offline from the *same* function by `tools/snowhill_mesh` (tools/snowhill_mesh.cpp:50-61). The measured law is that a big send comes from a **tall lip, not a sharp one**: launch grade is `0.6065·h/σ` and the pitch rate the machine must follow is `~v·h/σ²`, so raising `h` and `σ` *together* buys air at constant ramp angle while making the lip *gentler*. That is the correct reading of the shipped soft-lip finding (config/world.toml:504-506) — it never said "flatten the lip," it said "stop being sharp." The rung is one config block plus a regenerated `assets/heroes/*.glb` in the **same commit** (config/world.toml:498-500), preceded by one tools-only rung that puts the measuring instrument in the repo instead of a scratchpad binary. The honest ceiling: `kPeaks = 3` (world/snowhill.h:69) and `h_m <= 8.0` (config/load_world.cpp:1186-1188) are hard rails, and there is exactly **one** snowhill in the engine (`SnowpackField::hill`, a single member), so "where does the jump go" is a ruling Chad has to make before a line of it is written.

**(a) Quality dynamic terrain is less than was hoped, and the shortfall has a name: *dynamic*.** Four lanes produced a defensible five-class form vocabulary, a siting rule measured off Chad's own two tapes, and a density budget costed against the 320k-tree one-draw-call precedent — but every one of those classes is a static sum of gaussians baked once, offline, into a table and a glb. Nothing in any proposal responds to time of day, to `world::cold::hardness()`, to weather, or to use. "Visual" is also weaker than the plan assumed, and for a reason nobody had named: **the shipped St Charles hill is drawn through `kBuildingFS`, the mono building shader** (`vWs == -2` snow sentinel, render/buildings.cpp:106-107), whose whole light model is `uAmbient + uDiffuse·max(dot(N,-uSunDir),0)` — no `uGroundDayGain`, no `uNightFill`, no `uWinterNightGlow`, no aerial. Measured against the ground 100 m away in the same frame: **+2.0 DN at high sun (invisible) and −140.1 DN at night (a grey hole in a white field)**. So the terrain lane's real first rung is a *lit sibling shader*, not more shapes, and "dynamic" should be put to Chad as an explicit deferral rather than quietly dropped.

## WHAT THE MEASUREMENTS ALREADY SETTLE

These are not design decisions. They are closed.

1. **A sharp lip topples; it does not launch.** config/world.toml:504-506, MEASURED: *"kicker line (x=-11 local) composite 3.86 m, 23.7 deg max, SOFT LIP (lip rotation topples the machine off a sharp small kicker at speed -- measured; sy 5.0 is the fix)."* The mechanism is now understood: crest curvature is `-h/σ²` and lip pitch rate scales with `v·κ`. Consequence, measured on the scratchpad reproduction: at fixed `h = 3`, lengthening σ from 5 to 13 takes air from **1.95 s to 0.05 s**. *Longer alone destroys the jump; longer AND taller at fixed `h/σ` is the lever.*

2. **The landing dominates the launch, and cross-slope landings roll the machine.** config/world.toml:507-509, MEASURED: *"The steep axis is N-S so the straight jump run lands ON the fall line (probe shape v2, rot -45, rolled on a cross-slope landing)."* Reproduced this session at h6/sx4/sy8, sweeping `rot` 0/20/45/70/90: landing vertical −1.19 / −10.74 / −5.25 / −15.09 / −17.08 m/s, lip rate 87 → 171 deg/s. **The biggest air (3.20 s, 44.9 m, rot 70) was also the worst landing.** Rotating the takeoff axis off the run line is not a lever, it is a self-inflicted wound.

3. **A col on the run line bucks the machine before the crest.** config/world.toml:509-511, MEASURED: *"the run line is measured MONOTONE from crest to yard (probe shape v3, shoulder at y=9.5, made a col that bucked and rolled the machine at 9 m/s BEFORE the crest)."* This binds any multi-hump proposal in **both** directions of travel.

4. **M12: there is no steered approach.** docs/snowform_measurements.md §M12.2 — every steer from 0.02 to 0.26 rolls past 140 deg over a 12 s hold, at constant speed and at WOT, while zero steer is stable to 44.9 m/s. Therefore the committed approach **cannot contain a steering input**, which is exactly Chad's own "hit straight on." Caveat that must travel with it: `probe_steersweep` hard-wires `road_field` (tools/sled_probe.cpp:1284) — M12 is a **Road** measurement. Snow may differ; nobody has run the same window on snow.

5. **Off-centre and cross-corridor approaches are measured rollovers.** Off-centre hits at x0 = 2 m and 8 m produced the two worst rolled latches in the whole sweep (4.96 s and 7.46 s) because a symmetric mound lands you on its lateral flank. M9.1: a 25 cm lateral wander across a plowed-corridor edge at v0 = 24 became −482.7 deg of yaw; `class_blend_m = 0.0` still (config/world.toml:471). A run-in that grazes a bank is a rollover before the lip.

6. **Entry speed is pinned by the surface, not by the jump.** Bush WOT asymptotes at ~17.3-17.7 m/s over a 300 m run-in; asking for v_hold 16/18/20/24 returns an identical 15.56 m/s. **Size the jump for ~17.3 m/s on any Bush approach.** Road reaches 44.9 m/s at zero steer (M12) but a Road approach means crossing a bank (finding 5).

7. **The 75-deg cone is stale for jumps. Do not touch it.** sim/sled.cpp:253-254 requires the attitude AND `s.air_s <= p.comfort.rolled_grace_s`; sim/sled.cpp:1757-1758 resets `air_s` only when patch load > 1 N **or** `(r_after - floor_r) < 1.2 * p.cg_height_m` = 0.677 m (sim/sled.h:396, cg_height_m 0.564). sim/sled.h:193-194 ships `rolled_persist_s = 0.30`, `rolled_grace_s = 0.20`. Verified on the shipped binary: both printed lines report the first attitude trip as **"AFTER touchdown"** (tools/sled_probe.cpp:1127-1132 prints this column already). **Chad's flips survive. No dial is proposed here.** Narrowed per the red team: this holds for entries that actually launch (≥ ~14.4 m/s); below ~13 m/s the shipped docstring records a genuine contact-phase loop-out on the upper face (tools/sled_probe.cpp:1143-1146), which is a different failure and is SK-2's, not this rung's.

8. **`pack_cap_m 0.30` caps the *skin*, not the *ring*.** world/snowpack.cpp:293-294: `hill_reported_depth` returns `base + hill_m` **unchanged** whenever `hill_m <= class_min_m`. So the whole outer footprint band from hill = 0 to hill = 0.30 m is uncapped loose depth stacked on ambient. Any egress argument that quotes the ambient p99 line (6.11 m/s at 1.75 m) does **not** transfer to that ring.

9. **The exact-zero window is real and is the containment guarantee.** world/snowhill.h:46-52 — the peak sum times a C1 smoothstep reaching 0.0 *exactly* at `r_cut`, so depth outside the footprint is bit-unmoved and the localization test is `==`. Any authored form lives or dies inside its own disk.

10. **The shipped baselines, reproduced digit-for-digit this session** (`build/seads_sled_probe.exe snowhill`): SUMMIT exit 15.82 m/s, air **2.554 s**, landing **−10.62 m/s**, rolled 0.99 s, 41.53 m. KICKER exit 13.48 m/s, air **1.754 s**, −10.85 m/s, rolled 0.72 s, 24.27 m. **The tree's own prose is the stale thing** — tools/sled_probe.cpp:1146-1147 and test/unit/test_snowhill_drive.cpp:243-244 both still say "~2.3 s / ~−14.5 m/s," and tools/sled_probe.cpp:1148 says the kicker is "x=-10" while :1169 drives x = −11.0.

## (b) THE JUMP — THE RUNG TO BUILD FIRST

### Rung zero (tools only, nothing to rule): make the instrument shipped

Every number in the jump lane came out of a scratchpad binary linked against `build-play/libseads_sim.a` + `libseads_world.a`. It reproduces the shipped KICKER line to the tick, but **nothing in the repo would catch it drifting.** Fold it into the existing mode.

`tools/sled_probe.cpp` only:
- `probe_snowhill` currently ignores its own argument — tools/sled_probe.cpp:1150-1151, `void probe_snowhill(double hill_throttle) { (void)hill_throttle; }` — while `run_hill_line(hf, f, x0, hold_ms, throttle, target_x, target_y)` at :1016 already takes everything needed. Give it argv the way `steersweep` has argv: `snowhill <x0> <v_hold> <y_start> [h sx sy rot] [land_dy land_h land_sx land_sy]`.
- Add three columns that genuinely do not exist: **separation pitch rate about body X (deg/s)**, **max airborne tilt**, **apex clearance**. Add a fourth that closes the recommendation: **normal-closing speed at touchdown**, `dot(velocity, normal_at(dir))` at `air_end` — the radial component is not the impact number on a sloped landing.
- Do **not** add a "phase of first rolled tick" column — tools/sled_probe.cpp:1127-1132 already prints `first attitude trip ... (%s touchdown)` with the BEFORE/AFTER selector at :1131-1132.
- Fix the lying docstrings while there: :1146-1147 and :1148, per settled fact 10.

Cost: ~60 lines in one offline tool. No `sim/`, no `world/`, no config, no bake, no golden.

### The takeoff geometry

Recommended, and inside every shipped rail: **`h = 6.0`, along-run σ (`sy`) = 7.0, cross-run σ (`sx`) = 8.0, `rot = 0`** so the steep axis lies **along** the run line.

- Measured at 17.33 m/s entry: separation **21.6 deg**, apex **9.7 m** above the surface, **2.0 s** of air, **31.7 m** gap. (Scratchpad instrument — UNPINNED until rung zero ships.)
- **Width is free.** Sweeping `sx` from 4 to 20 m moved the gap by 0.18 m (31.85 → 31.67). Spend width on silhouette and on off-centre forgiveness, which is measured graceful (x0 offset 0/2/4/6/8/10 m → 2.76/2.41/2.34/2.08/2.56/1.35 s, no cliff).
- **The rails, named up front, not discovered:** `h_m` ∈ (0, 8] and σ ≥ 2 m (config/load_world.cpp:1186-1191); `h_max` ∈ [5.6, 6.4] and `grad_max <= 1.0` (test/unit/test_snowhill.cpp:221-223); `north_slope` ∈ [0.45, 0.65], `sse_slope >= 0.70` (:244-246). **Any scaling table above h = 8 is not a proposal, it is a request to move a rail** — the h 9 / 10 / 12 rows circulated in two lanes will not load.

### The landing — present as an A/B, not as a conclusion

Chad's verbatim is *"Sudburian is tough and can handle the fall."* That is a **permission**, and no consultant gets to invert it into a requirement. Both rows, measured on the same takeoff at 17.33 m/s, for him to rule:

| | air | landing vertical | gap |
|---|---|---|---|
| **Flat landing** (takeoff peak only) | 2.008 s | **−10.94 m/s** | 31.68 m |
| **Landing knuckle** (2nd peak, dy −24, h 5.0, sx 10, sy 8) | **2.763 s** | **−0.18 m/s** | 35.61 m |

The knuckle wins on both axes in these numbers, which makes it an easy ruling to *ask* for and a needless one to *assume*. **But it is not clean**, and two things must be measured before it ships:
- **The undershoot / knuckle case.** The landing hump's near face rises 13-15 deg from y = −12 to y = −18. A short hit lands on rising ground — the worst possible landing. The saddle bottom is a gentle 3.00 m bowl at 1.9 deg so a *very* short hit is soft; the **middle distance (18-24 m) is the one that hurts**.
- **The northbound run.** The composite deliberately places a ~2 m col on the run line, which is the exact geometry config/world.toml:509-511 already measured bucking the machine at 9 m/s *before* the crest. Nobody drives a playground mound in one direction only. **Run the line south-to-north with the rung-zero probe. If it bucks, the knuckle needs an asymmetric southern tail or it does not ship.**

### Siting rules (zero code, and they are gates)

1. **Straight run-in, ≥ 250 m**, no steering input anywhere in the committed approach (settled fact 4).
2. **The run-in must not graze a plowed-road corridor edge** (settled fact 5, `class_blend_m` still 0.0).
3. **The landing must be on the fall line**; the takeoff steep axis lies along the run (settled fact 2).

### Is it an SF1 sibling? Yes in *shape*; the *site* is an open ruling

Shape: nothing here needs anything `snowhill_add` cannot express. Peak budget: **p1 = takeoff, p2 = landing knuckle, p3 = the approach shoulder** — and p3 is not optional, because a shoulder that leaves a col before the crest is the shape-v3 failure. That is all three. `kPeaks = 3` (world/snowhill.h:69) with `{peak1, peak2, peak3}` hard-coded in the loader is the **real ceiling on this feature**, not a shape limit.

Site: **there is exactly one snowhill in this engine.** One `SnowhillParams hill` member, one config block, one baked anchor (`world/snowhill_geo.gen.h`, projection-lock hash checked in `config/load_world.cpp`), one hero glb placed in `render/buildings.cpp`. So:

- **Option A — extend `[snowhill]`.** Cheapest. But peak3 is `[-11.0, -4.0, 2.4, 4.5, 5.0, 0.0]`, the kid-built **KICKER** — the repo's own landable certification jump, driven at x = −11.0 by tools/sled_probe.cpp:1169 and hard-coded in test/unit/test_snowhill_drive.cpp. Spending it re-derives five files of pinned assertions on the hill the brief calls *"the SHIPPED St Charles hill Chad already likes."*
- **Option B — a second site.** Keeps the shipped hill intact, but costs `SnowhillParams` becoming an array, a **second anchor baked through the locked projection** via `offline_tool/sf1_snowhill_place.py` (anchors are generated, never hand-authored — that lock hash exists precisely so a re-bake cannot float the hill), a second glb, loader plumbing, and a second hero placement. **This is a new mechanism.** Anyone costing it as "no new file, no new mechanism, no new draw path" is understating it.

### The verification that gates it

Follow the shipped precedent exactly (test/unit/test_snowhill_drive.cpp:242-252 deliberately refuses to assert the landing, because the S3 skid cannot absorb it and the fix is owed suspension work): **ASSERT the launch** — `air_start >= 0`, no BEFORE-touchdown attitude trip, `min_climb_v > 2.0` — and **PRINT the landing**. Plus: the geometry ships in the *same commit* as the regenerated glb, gated by `snowhill_mesh_conforms_to_the_function` (test/unit/test_snowhill.cpp:470, `worst_err <= 0.10` at :524, `max_gap <= 2.5` at :536). Note the double hump sits **near**, not comfortably inside, that mesh budget: 64 sectors is 2.36 m of arc at r = 24 m against a 2.5 m gap tolerance.

## (a) THE TERRAIN — THE VOCABULARY AND THE STAGING

### The classes, each with the verb it earns

1. **WHALEBACK** — one peak, sx ~6 / sy ~40, h 1.5-2.5, aligned **with** the corridor. Verb: *commit and plane*. Earns it because M12 says the only committable line is a straight one, and the tapes show `plane_frac` p50 = 0.000 — Chad essentially never planes. A long shallow 4-8 deg face is where planing lift could actually engage. **UNMEASURED whether it does.**
2. **ROLLER FIELD** — 3 peaks, h 0.6-1.2, σ 3-5, spaced 12-18 m. Verb: *pump, unweight, chatter, roost*. Earns it because 35 of the 62 air events on tape 1 are under the 0.20 s grace window — chatter is already the dominant air; this makes it deliberate.
3. **KICKER** — the (b) form above. Class minimum `sy >= 5.0` per the soft-lip law, **plus a landing clause with the same force**: landing zone inside the same `r_cut`, run-in on the local fall line, bounded landing cross-slope, no col on the run line in **either** direction. A kicker class that specifies only the lip re-runs the v2 topple at every site.
4. **GRAZE-BANK** — a corridor-parallel one-sided ridge. **Ship it explicitly labelled as a probe, not as an earned class.** Its structural defence (C1 by construction, hill wins the class at world/snowpack.cpp:424, no Voronoi handoff) answers the *step* defect but not the *tip-over* fence: a bank exists to be turned on, and turning while committed is what M11/M12 measured as a rollover, attributed to grip 0.953 g vs tip onset 0.363 g — *"Kernel — and snow cannot amend it."*
5. **HORSESHOE RIM** — `snowhill_add` is `>= 0` always and "can only ADD relief, never cut it" (world/snowhill.h:71-72), so a bowl is not expressible. Build the **rim only**, and **the rim must be OPEN** — a closed rim is a gravity trap even when every wall is individually climbable. Law 4's no-climbability-bound does not license that. **Egress on a bare rim-enclosed floor is UNMEASURED and must be taken before this class ships.**

Underlying all five, and the single most useful design consequence in the consult: **a line on this machine is not a steered line.** `align_m` is gated on an already-existing yaw rate (`mag = clamp((|wy| - 0.02)/0.05, 0, 1)`, sim/sled.cpp:482-487), so lean cannot *initiate* a turn, only amplify rotation the machine already has; `plane_lat_lean_gain` ships 0.0. Lean's path authority is proportional to the slope you are on. **The forms ARE the steering.** A field with no sustained cross-slope faces gives the rider nothing to drive with.

### The siting rule and its source

**Every Tier-A site anchor lies in the band [corridor edge + 9.0 m, 85 m] perpendicular from a road/trail centreline, and its `r_cut` disk may not cross the inner bound.**

- Inner bound: the plow-bank envelope is `bank_rise_m 3.00` + `bank_fall_m 6.00` = edge + 9.0 m (config/world.toml:473-475), and `depth_geometry_at` **sums** the corridor term and the hill (world/snowpack.cpp:314, verbatim: `return corridor_eval(dir).geometry_depth_term + snowhill_add(hf, hill, dir);`). A form on a bank literally stacks 1.30 m of bank onto its own height — Chad's own complaint about doubled banks.
- Outer bound and the whole justification: measured off his two most recent tapes. Nearest-corridor distance p50 **19 m** / p90 44 m / max 85 m (tape 1); p50 24 m / max 50 m (tape 2). Each whole drive fits in a 494 m / 966 m bubble, and **neither ever came within 1,506 m of the one authored form in the game.** *The flatscape complaint is a siting problem, not a density problem.*
- Data sources, all already baked: corridor geometry from `render/sudbury_gis.gen.h`; water from `assets/sudbury_landmask.png`; rock from `sudbury_barrens.png`; trees from `sudbury_treedensity.png` + the props footprint mask; buildings from `sudbury_buildings.bin`; SF1 and CutDisk/mine exclusions from their own generated headers.
- **Non-overlapping `r_cut` disks are mandatory** — overlapping sites *sum* (world/snowpack.cpp:314) and stacked amplitude is exactly what law 4 forbids. Clusters are expressed as multi-peak **sites**, never as overlapping sites.

### Density and its computed cost

Measured drivable corridor: 443.80 km road_major + 1,645.46 km road_minor + 124.44 km trail = **2,213.7 km**. At S = 500 m spacing that is **4,427 Tier-A sites**. At the median point of Chad's actual drive, 2.40 km of corridor lies inside the 277.3 m geometric horizon and 6.98 km inside 500 m — so S = 500 m puts ~5 forms inside the horizon and ~14 inside the legible band at all times.

**You cannot buy the world-read with height; you buy it with count.** Eye at 2.564 m on R = 15 km gives a 277.3 m horizon; legible range (≥8 px vertical) saturates: 3 m → 336 m, 6.2 m → 511 m, 12 m → 701 m, 20 m → 887 m. Doubling height buys 1.37×, not 2×.

Cost, against the measured 320,488-tree / one-draw-call precedent: instance buffer 4,427 × 64 B = **283 KB (1.4% of the trees' 20.5 MB)**; ~20 sites inside a 700 m draw radius at 2,049 verts / **4,032 tris** each ≈ 82k tris = **4.3% of the tree scatter**. **Draw calls are the only real ceiling** (budget ≤ 24): the vocabulary must be ~5 shape meshes plus one shared skirt, with all variants carried by the instance transform. **M4 (frame-time + draw-call baseline) is still pending, so every one of these is arithmetic, not a measured frame.**

### Two live O(N) defects that must be fixed before the count grows

- **`snowhill_add` is evaluated twice per ground sample.** world/snowpack.cpp:359 computes `hill_m`, then `classify()` re-evaluates it at :424 with the same dir and params. At 3 patches × 12 substeps × 120 Hz that is **4,320 redundant evaluations per second** on the hot path. Hoist it into `classify()` exactly as the pre-computed `LineHit` already is.
- **`world/props.cpp:151` calls `snowhill_add` per tree candidate**, so the scatter mask is O(9.8M × N).
- A spatial index over the `r_cut` disks is therefore mandatory before rung 4, not optional.

### The staged order — **PROPOSAL, staging is Chad's call**

0. **Rung zero**: the probe argv + columns (jump section). Tools only.
1. **The lit sibling shader** (`snowform_fs`) — because a form drawn through `kBuildingFS` measures +2 DN at noon. See the go/no-go.
2. **One authored jump**, on the site Chad rules, one commit with its glb, launch asserted / landing printed.
3. The `snowhill_add` double-eval hoist + the props O(N) fix.
4. Struct-to-array + spatial index + the offline selector + instanced draw.
5. Tier-B landmark forms (12-20 m) — feel and survivability **UNMEASURED**.

**"Dynamic" is deferred at every rung above.** Nothing here responds to cold, weather, time, or use. That may be the right call, but it is one of the three words in the brief and Chad should say so, not have it dropped.

## THE GO/NO-GO: CAN IT BE SEEN

**M2 is no longer blocked as a procedure, but it is not answered either, and the go/no-go fails somewhere the plan never looked.**

**What is now closed.** The instrument existed all along: `SEADS_OBLIQUE` (app/main.cpp:5146-5170, pan/tilt steerable) composes with the 4th `--smoke` argument, the celestial offset, giving a deterministic frame of any named ground point at any point in the 300 s day. Validated against the binary at day/night resolution: sky-patch blue is black outside the predicted 41-199 s daylight window and blue inside, at all 8 sampled offsets. Register this as the standing SF3 visibility rig; it is zero code and cannot move a golden.

**What is NOT closed, and the numbers that must not be pasted into the doc as-is.** The elevation-vs-clipping table circulated in this consult is **contaminated**: its "hill" box mixes hero pixels with planet-ground pixels, proven by the fact that the box's clip fraction moves (19.23% → 11.94%) across a `ground_day_gain` sweep at a *fixed* celestial cell, while the clean crest box is **bit-identical** across all four gains. So the "clipping onset at 41-53 deg" bracket belongs to neither class, and the plan's 44.8 deg arithmetic — separately checkable and correct: `(1/0.85 − 0.05)/1.60 = 0.7040 → asin = 44.76 deg` — is still **unconfirmed against any frame**. Re-cut on three separately-labelled boxes (hero-only, verified open-snow-only, mixture) and state the abscissa as **celestial offset seconds**, not degrees, until the axis is wired.

**The real finding.** The shipped hill is a hero GLB drawn through `kBuildingFS` (render/buildings.cpp:236 sets the `-2.0` snow sentinel; :106-107 is `if (vWs < -1.5) base = vBj;` then `float lit = uAmbient + uDiffuse * max(dot(N, -uSunDir), 0.0);`). No `uGroundDayGain`, no `uNightFill`, no `uMoonFill`, no `uWinterNightGlow`, no sparkle, no `sky_aerial()`. Measured, hero crest vs planet ground 100 m away in the same frame: **high sun +2.0 DN; mid +(−11.4); low +86.0; night −140.1 DN.** SF3-2's own three-cell gate ("noon, 58 deg, moonless night") **would fail today at two of its three cells on the shipped mechanism**. The rung is a *lit sibling shader*, a third consumer of the concatenation idiom render/sky.cpp:191-192 already runs twice — not a mesh change, and not the plan's predicted "glowing lozenge": it inverts **dark**, not bright.

**The probe_day wiring should still land, as the numeric channel beside the visual rig.** Exact one-liner, current against this tree: app/main.cpp:2456 is `double cel_time_offset = smoke_cel_offset;` → `probe_mode ? probe_day * cel.day_period_s : smoke_cel_offset`, with the non-probe path bit-identical. (docs/snowform_measurements.md still cites the stale :2447.) Second half: `render/probe.h`'s `PatchStats` has max/min luminance but **no clipped counter** — add `n_clip` in `add()` when all three channels ≥ 254 and expose `clipped_frac()`.

**Two things that make this harder than the value budget suggests.** (i) **This engine has no cast-shadow system** — `grep -rn shadow render/` finds only albedo self-shadow language. At high sun there is not even a shadow to carry a silhouette. (ii) The budget must be spent **downward**: at high sun the ground sits at p50 251 DN and the hero at 253 — 2 DN of headroom — so any class defined by being *whiter* is invisible for roughly half of daylight, while downward has the whole 0 → 0.69 range at every cell. Give value to **at most two DARK classes** (the industrial pump apron; one wind-scoured/gravel class carrying the shipped `kBankFS` subtractive oreo idiom, `c = uBase * (1 - cover*uDark*(1-skirt))`). Everything else carries on silhouette and area.

**And the biggest miss in the whole visual lane: not one of the 26 evidence frames is at rider eye.** All are a planet-centred oblique ~200 m up looking down. Chad's ask (b) is aiming a straight run at a lip from a seat ~1.5 m off the deck at grazing incidence — and the shipped soft-lip fix deliberately makes the lip **low-frequency**, exactly the form that vanishes first in white-on-white at grazing angle. Until `SEADS_OBL_UP=2..5` frames exist along the N-S steep axis at offsets 150/110/0, **"can it be seen" is answered only for a map view.** Same for the landing slope, which nobody has checked reads at all.

## WHAT THE RED TEAM KILLED

*Paste-ready for the plan's "What earlier revisions got wrong."*

**FATAL**

1. **"Bake the shape mesh hill-only and instance one shared pad+burial skirt annulus worldwide."** The pad is **not a constant** — `pad = 0.862` (tools/snowhill_mesh.cpp:80) is *site-local ambient*, measured by `offline_tool/sf1_pad_probe.py` at the St Charles anchor, and `ambient_depth_at` varies with slope, elevation, wind aspect, curvature and drainage at every site. One shared annulus forks drawn-from-driven **by translation** at every site whose ambient is not 0.862. That is rev 2's "the rim carried ambient ⇒ a 1.22 m wall around every site" reintroduced in a different mesh — the bookkeeping moved, the defect did not. **Do not re-propose it.** If a skirt is wanted, either generate it per site from that site's own probed ambient, or drive its vertical extent from a per-instance ambient scalar and **pin a measured bound** on the drawn-minus-driven residual.
2. **Any jump peak with `h_m > 8.0`.** `config/load_world.cpp:1186-1188` refuses to load: *"snowhill peak h_m in (0, 8] -- Chad's 20 ft mountain, not a cliff."* The h = 9 recommendation and the h = 10 / h = 12 rows of the "only lever that buys air" scaling table are all unreachable, and `test_snowhill.cpp:222`'s `h_max <= 6.4` bites earlier still. The message ties the bound to Chad's own words, so raising it is a ruling he has not made.
3. **Costing a second jump site as "one `[snowform]` site entry + one offline mesh run — no new file, no new mechanism, no new draw path."** There is no `[snowform]` site system: `grep snowform` over config/ world/ tools/ returns comments and zero code. A second site needs a site array, loader schema + validation, a new anchor baked through `offline_tool/sf1_snowhill_place.py` under the **locked** projection, a generalized mesh tool, and a second hero placement. **This is the rev-1 failure mode: costing the mechanism and forgetting everything around it.**

**MAJOR**

4. **Hand-authoring per-site anchor frames (`up`/`east`/`north` dvec3) in `world.toml`.** Anchors come from the generated `world/snowhill_geo.gen.h` and are copied at load with a projection-lock hash check — the guard exists so *"a projection re-bake that forgets to re-run this script fails the build, never floats the hill."* Config carries shape dials and a site index; **never a frame**.
5. **The landing-knuckle composite as proposed, un-measured northbound.** It builds a ~2 m col on the run line — the exact geometry config/world.toml:509-511 already measured bucking the machine at 9 m/s *before* the crest. The proposal cited that finding in a different section and did not apply it to its own shape.
6. **Presenting the knuckle as "the direct answer to *Sudburian is tough and can handle the fall*."** Chad's sentence is a **permission**, offered as the reason a big send is acceptable. Inverting it into a requirement to eliminate the fall is a ruling, not an inference. **Present A/B, always.**
7. **Costing any peak change as "one config block + one mesh + one drive leg."** It omits five files of pinned re-derivation: test_snowhill.cpp:221-223 (`h_max` ∈ [5.6,6.4], `grad_max <= 1.0`), :244-246 (bearing-specific `north_slope` / `sse_slope`), :343-346, :419; plus test_snowhill_drive.cpp's hard-coded peaks[2] and the probe's two hard-wired lines.
8. **"The hill is under 1 cm past r = 26 m, so size `r_cut` DOWN to the form."** 26 m is a **due-south** number quoted as an omnidirectional property. Max-over-bearing hill at r = 26 is **0.3125 m** — above `class_min_m`, so the class override is still firing — and first falls under 1 cm only at r = 40. The true dead-flat margin is ~4 m, not 18. Worse, with `feather_m = 15`, an `r_cut` of 40 starts the C1 window biting at r = 25 where the gaussian is still ~0.36 m, **clipping live authored amplitude**.
9. **"Bush-class snow inside a footprint can never exceed 0.77 + 0.30 = 1.07 m."** Conditional on flat ambient. `hill_reported_depth` returns `base + hill_m` uncapped below `class_min_m`, and `base` is the *local* ambient — over a p99 drainage line (1.75 m) that is 2.05 m. This was the load-bearing safety argument against stacked amplitude and it is stated as absolute.
10. **`class_min_m` / `pack_cap_m` as a blanket egress answer, and any closed HORSESHOE rim.** Per settled fact 8, the uncapped 0-0.30 m ring gets no cap and a bare rim-enclosed floor gets none by design. Law 3's 2 m/s floor is *asserted*, never scoped or instrumented, and "egress" appears nowhere in the unmeasured lists.
11. **GRAZE-BANK presented as an earned class.** It requires steering while committed, which the same lane's own reading of M12 calls a rollover. Structural C1-ness answers the step defect, not the tip-over fence, which is a kernel grip/CG property no authored shape can amend.
12. **A KICKER class that specifies the lip and says nothing about the landing.** Reproduces the measured v2 topple at every site. And at h 2.5-4.5 it merely *relocates* today's air (the shipped kicker is 3.86 m / 23.7 deg) — it does not answer "quite a bit of air."
13. **Binding the hill class ring to the corridor's `class_blend_m` dial.** That dial was swept and tabled only against the corridor edge; the repo's own law is that a feel dial is ruled on the stick. Binding both means the moment Chad rules 1.0 for road shoulders, he ships an unmeasured blend at ~599 km of form ring. Give the ring its own dial, or hold it behind a measured bankgraze-on-snowhill table.
14. **The lane brief's third word.** Nothing proposed is **dynamic**. Either add a dynamic rung or put the deferral to Chad explicitly.

**MINOR (but they are how revisions die)**

15. "The shipped hill already gives 2.0-3.2 s of air and a 31-41 m gap" — it gives **2.554 s / 41.53 m** (summit) and **1.754 s / 24.27 m** (kicker). That range silently blends shipped measurements with scratchpad *candidate* forms.
16. "NOT ONE rolled latch occurred during the ballistic window" across entries 8-17.3 m/s — the shipped docstring records a contact-phase trip on the upper face below ~13 m/s. **Narrow the claim to entries that launch**; the conclusion (do not touch the gate) survives.
17. Proposing to add a "phase of first rolled tick" column that tools/sled_probe.cpp:1127-1132 **already prints** — a read-the-tree failure in the one rung whose justification is "put the instrument in the repo."
18. `h/σ <= 1.61` as the admission rule when the same proposal derives `0.918/0.6065 = 1.51`; 1.61 admits grade 0.977, past the bound it exists to enforce. And grade ≤ 0.918 is a *frictionless capability ceiling* used as an admission threshold — zero margin in the optimistic direction.
19. Ballistic range computed as `v·t` instead of `v·cos θ·t` (53.1 m, not 58 m), and a departure angle taken from the config header's **max grade** rather than the lip.
20. `k_air_shift` as a landing-attitude corrector: sim/sled.h sizes it at "~0.25 rad/s of transient pitch" (~14 deg/s) against a measured lip rate of 80-170 deg/s over 2 s; measured inert at k = 0/1/3 (gap moved < 0.15 m). **Not a corrector.**
21. Triangle count: 4,032, not 4,096 (64 fan + 64·2·31). The header comment at tools/snowhill_mesh.cpp:101-105 says "2113 verts" and is **stale against its own code**.
22. Tape-token citations that do not state their indexing base. The map is correct **0-indexed**; a reader reproducing it 1-indexed reads `engine_rpm` as `plane_frac`.

**AND THE STANDING RULE THIS CONSULT ADDS:** *the rotation is not authorable.* Measured lip pitch rate 80-170 deg/s over 2-3 s means the machine arrives at the landing at an essentially arbitrary attitude. **Any future rung that promises "lands wheels-down" should be killed on sight.** The honest design is Chad's own sentence — catch the fall, or let him take it, but do not claim to fight the rotation.

## UNMEASURED — AND THE INSTRUMENT THAT WOULD CLOSE EACH

| Question | Why it matters | The exact probe / command |
|---|---|---|
| Does the northbound run over a double-hump composite buck? | config/world.toml:509-511 already measured a col rolling the machine at 9 m/s before the crest. If it bucks, the knuckle needs an asymmetric south tail or does not ship. | Rung-zero probe with the line run south→north: `seads_sled_probe snowhill <x0> <v_hold> <y_start=-40> ...` and read the BEFORE/AFTER-touchdown selector at tools/sled_probe.cpp:1131. |
| Normal-closing speed at touchdown | −0.18 m/s **radial** on a 17-deg face is not the impact number, and it is what the whole knuckle recommendation rests on. | One line in rung zero: `dot(velocity, f.normal_at(dir))` at `air_end`. |
| Does an arc on **snow** survive the 12 s speed-held window that killed the Road band? | `probe_steersweep` hard-wires `road_field` (tools/sled_probe.cpp:1284) and prints "on ROAD". Until this runs, nothing may claim the machine can hold a line through a form field. | Give `probe_steersweep` a surface argument; run on `bush_field` and `snowhill_field`, throttle held, secs 12. |
| Does the form **class ring** produce a bank-strike? | `classify()` flips to TrailMain at a hard step (world/snowpack.cpp:424) and M10's blend is branch-gated on a **corridor** hit (:375) — structurally uncoverable. ~113 m of ring per site. | Re-point `probe_bankgraze`'s geometry at `snowhill_field`: straight, zero steer, tangent passes at offsets around the 0.30 m contour, v0 6/10/16/24. (New function, ~100 lines — not a re-point.) |
| Egress from the uncapped 0-0.30 m ring, and from a bare rim-enclosed floor | Law 3's 2 m/s floor is asserted, never scoped; `pack_cap_m` does not reach either place (world/snowpack.cpp:293-294). | `field_at_depth(hf, 1.07)` + `drive(p, f, 1.0, 14.0)` against the pinned 18.12 (0.77 m) and 6.11 (1.75 m) terminals; and a settle-inside-a-closed-rim + WOT egress case. |
| Uphill egress from rest on a form grade | Arithmetic says the shipped hill's steepest point clears 2 m/s in ~3.1 s with 9.4% thrust margin, ignoring plow drag, compaction drag and the traction ceiling. | `settle_snowhill(p, f, x, y)` (tools/sled_probe.cpp:991) + dwell N s + WOT, peak speed, swept over the grade rose. |
| Does the rolled readout latch mid-flip over **rising** ground? | The air gate is a **radial** clearance (`(r_after − floor_r) < 0.677 m`, sim/sled.cpp:1757-1758). A landing ramp rising to meet the machine closes it early — the last of a barrel roll could latch and cut throttle. | Two extra trace columns in `HillRun` (`air_s`, `rolled_hold_s`) + one send-onto-a-ramp case. No shipped instrument logs either today. |
| Can the lip be seen **from the seat**? | All 26 M2 evidence frames are a 200 m oblique. The soft-lip fix deliberately makes the lip low-frequency — the first thing to vanish at grazing angle in white-on-white. Aiming is the whole of "hit straight on." | `SEADS_OBLIQUE=1 SEADS_OBL_UP=2..5 SEADS_OBL_BACK=150/400/800`, pan/tilt on the N-S steep axis, `--smoke 20 out.png <offset>` at offsets 150 / 110 / 0. |
| Clean clipping onset for **open snow** (M2's actual question) | Every circulated table is a hero/ground mixture; the clean crest box behaves oppositely to the mixture under a gain sweep. | Re-shoot with three labelled boxes (hero crest, verified tree/building-free open snow, mixture) + `SEADS_NO_HERO` A/B difference to isolate form pixels exactly. |
| Sun elevation in **degrees** | Every elevation number comes from the celestial core evaluated offline, validated against the binary only at day/night resolution. | Wire `probe_day` into `cel_time_offset` at app/main.cpp:2456 (one line, non-probe path bit-identical) + `n_clip` in `render/probe.h`'s `PatchStats`. |
| M4: frame time and draw-call baseline | Every density cost here is arithmetic against the 320k-tree precedent. If the current count is already near 24, even 6 more calls is a regression. | `seads.exe --smoke 400` with 20 warm-up frames and the four provenance fields, before and after. |
| Does the mesh mould resolve a double hump? | 64 sectors is 2.36 m of arc at r = 24 m against `max_gap <= 2.5` (test_snowhill.cpp:536) and `worst_err <= 0.10` (:524). Near a shipped tolerance, not inside it. | `tools/snowhill_mesh` on the new peaks + re-run test_snowhill.cpp:470. |
| Dispersion of the send | Every jump row is one deterministic run, and rotation is visibly chaotic in entry speed on the *same* shape (max airborne tilt 26.6 / 118.9 / 43.3 deg at 12 / 14 / 15.6 m/s). | Sweep entry in 0.25 m/s steps over 10-18 m/s; report the fraction of runs whose post-touchdown rolled time exceeds 1 s. |
| Does a 250-300 m straight run-in exist at any candidate site? | The entry speed the jump is sized for requires it, and nobody has checked the terrain has it. | Corridor/landmask query over the candidate anchor's northern apron. |
| Does a whaleback actually engage planing lift? | The WHALEBACK class's entire justification is `plane_frac` p50 = 0.000. | New probe mode driving a synthetic whaleback, reporting `plane_frac` vs face angle. |

**Two standing conditions on all of the above.** (i) Every jump number in this consult is on the **analytic fixture** — flat synthetic `HeightField`, ambient 0.85, all modifiers zeroed, no LineNetwork, no cold, default `SledParams`. That is deliberate repo culture ("a measurement that moves when the world is re-baked is not a measurement"), so a real-anchor run is a *second leg*, never a replacement. (ii) **The base is RED** — four sled legs fail by prior design (docs/snowform_measurements.md §M3). No rung here can claim a green baseline, and any commit must name which four.

## FOR CHAD TO RULE

1. **Where does the jump go?** (A) Extend `[snowhill]` — cheapest, no new mechanism, but it **spends peak3, the kid-built kicker**, and re-derives five files of pinned tests on the hill you already like. (B) A **second site** — keeps St Charles intact, but costs a site array, a new anchor baked through the locked projection, a second glb, loader plumbing and a second hero placement. This blocks everything in (b) and nothing can be estimated until you pick.

2. **Flat landing or landing knuckle?** Both measured on the same takeoff at 17.3 m/s: flat = 2.008 s air / **−10.94 m/s**; knuckle = 2.763 s air / **−0.18 m/s**. The knuckle wins on both, but it is *your* "tough and can handle the fall" that is being spent, and the knuckle brings a real cost: a rising near-face that punishes a short hit, and a col on the reverse approach that must be measured before it ships.

3. **Is "dynamic" in scope for this build, or deferred?** Every form proposed is static, baked once, offline. Nothing responds to cold, weather, time of day, or use. Deferring is defensible; it should be your word, not our omission.

4. **Does the lit sibling shader come before the first authored form?** The shipped hill measures **+2.0 DN at high sun** and **−140.1 DN at night** against the ground beside it, because it is drawn through the mono building shader. Building shapes before fixing that means authoring forms nobody can see at two of the three cells your own gate names — but it also means an art-law change (the building set is deliberately mono; a ground-lit sibling gains chroma), which is your call to make.