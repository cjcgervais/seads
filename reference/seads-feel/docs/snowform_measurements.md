# SNOWFORM — SF3-0 MEASUREMENTS

**Rung:** SF3-0 (MEASURE). No game code. **Branch** `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow`,
**base** `main @ 7bae38718` (verified: `sandbox/gi4-ride` and `sandbox/world-sudbury` are both ancestors; `git diff
HEAD main` over the whole SF3 file surface is empty).
**Why this rung exists:** CLAUDE.md's NO-GUESSING law — *"Every number that describes a real thing … comes from a
MEASUREMENT … If you cannot measure it, ASK."* Two earlier revisions of the snowform plan were killed by numbers
that turned out to be wrong. Nothing downstream is built on an estimate.

| item | status |
|---|---|
| M1 — `radius_at` consumer census | ✅ **DONE** (below) |
| M2 — clipped-snow-pixel % across the day cycle | ⚠ **BLOCKED — no shipped instrument can sweep it** (§M2) |
| M3 — test-count + failing-leg baseline | ✅ **DONE — ★ THE BASE IS RED, AND IT IS THE SLED** (§M3) |
| M4 — frame-time + draw-call baseline | ⏳ pending |
| M5 — pump-bowl census (~300 m apron) | ⏳ pending |
| M6 — track-ribbon visibility at speed | ⏳ pending |
| M7 — SK-3 corners: long dwell, from-rest-on-a-grade | ⏳ pending (needs a new probe mode) |

---

## M1 — EVERY `radius_at` CONSUMER

**Why it matters.** Snowform revision 1 proposed moving the drawn ground surface and named **2** consumers. The real
number is **31 call sites across 18 files**, plus two whole subsystems that desync by a *different* mechanism (§M1.6).
Missing them is what made revision 1 delete the lakes and bury 320k trees.

**Method.** `python tools/graph/graph_query.py impact world/heightfield.h` for the module-level truth (Standing Law 2),
then call-site enumeration by text search — the graph gives dependency, not call-site counts, and this is exactly the
fallback the law allows. Excluded: `facet_radius_at` / `drive_radius_at` (different functions), the definition in
`world/heightfield.*`, comment-only lines.

**Graph result:** touching `world/heightfield.h` rebuilds **113 dependents (32 non-test TUs)** and retests **46 test TUs**.

### M1.1 — The drawn mesh itself (4 sites)
| site | role |
|---|---|
| `render/sphere_param.cpp:312` | `fill_face` vertex radius |
| `render/sphere_param.cpp:302` | `surf` lambda — the 4-probe analytic normal |
| `render/sphere_param.cpp:250` | `facet_radius_at` corner (`dir * hf.radius_at(dir)`) |
| `render/sphere_param.cpp:263` | degenerate-facet fallback |

★ Note the multiplier: `fill_face` evaluates the radius **once for position + four times for the normal probe**, and
`facet_radius_at` evaluates **three corners** per query. Revision 1 costed this as one evaluation each.

### M1.2 — Draw-side placement: floats or buries if the drawn ground moves (13 sites)
| site | what moves |
|---|---|
| `world/props.cpp:171` (+ `props.h:43`) | **~320k tree/prop bases** — comment reads *"anti-float: the mesh's own height field"* |
| `render/buildings.cpp:153` | baked building prisms |
| `render/buildings.cpp:203` | **hero GLBs** — `church.glb` and `snowhill_stcharles.glb` (`sink_m` applied here) |
| `render/lights.cpp:101` | lamp / streetlight columns |
| `render/train.cpp:235`, `:337`, `:393` | rail crest, span midpoint, run endpoint |
| `render/slag.cpp:165`, `:471` (+ `slag.h:47` `Ra`) | slag ridge surface + anchor base radius |
| `render/smoke.cpp:80` (+ `smoke.h:37`) | Superstack plume anchor top |
| `render/tunnel_mesh.cpp:25` | portal rings, trench rims, bowl rims |
| `world/buildings.cpp:115` | building base radius (physics prisms) |

### M1.3 — Physics, frozen kernel (2 sites)
| site | role |
|---|---|
| `sim/ground.h:61` | aircraft ground contact — `radius_at(up) + contact_height_m` |
| `sim/ground.h:327` | wingtip strike test |

★ **INV-1 exposure.** These read the **bare DEM** and are inside the frozen kernel. Any drawn-surface change leaves
aircraft contact behind. Not an agent's to accept — Open Item 4 in the plan.

### M1.4 — The snowpack function's own probes (4 sites)
| site | role |
|---|---|
| `world/snowpack.cpp:62` | `curvature_at` centre sample `h0` |
| `world/snowpack.cpp:65` | `curvature_at` 4-probe loop |
| `world/snowpack.cpp:102` | elevation term (`radius_at − R`) |
| `world/snowpack.cpp:326` | `ground_radius_base` — the analytic branch |

★ **Confirmed live defect.** `ambient_depth_at` calls `hf->normal_at(dir, p.curv_probe_m)` twice with **identical
arguments** — once via `slope_at`, once for the aspect/lee term. `normal_at` is 4 `radius_at` each, so one
`ambient_depth_at` costs **14 `radius_at` where 10 suffice — ~29% off a hot path.** Free win, independent of snowform.

### M1.5 — Queries and gameplay (6 sites)
`app/main.cpp:2215` (aircraft spawn `+10 m`) · `drone/drone.h:1842`, `:2272` (drone AGL) ·
`render/draw.cpp:4430` (`fear_local_up` AI ground reference) · `world/tunnel_net.cpp:222` (mouth cover `cap`),
`:520` (surface radius)

### M1.6 — ⚠ Desync by a DIFFERENT mechanism — absent from the census, and the one that bit revision 1
`render/water_surface.cpp` and `render/river_surfaces.cpp` **never call `radius_at` at all.** The lake mirror is built
from a **baked per-lake elevation**: `render/water_surface.cpp:16`, `const double radius = L.elev_m + lift_m`, with
`[water] surface_lift_m = 0.4`.

Meanwhile `ambient_depth_at` lerps the *depth* to `ice_lift_m + ice_snow_m = 0.65` over water
(`world/snowpack.cpp:160-163`). So any change that adds ambient depth to the drawn terrain raises the lakebed to
`elev_m + 0.65` while the ice mirror stays at `elev_m + 0.40` — **the lakebed pokes 0.25 m through the ice on every
lake.** A `radius_at` grep alone will never find this. Any future ground-reference work must treat baked-elevation
surfaces as a first-class category.

### M1.7 — Verdict for the current plan
Revision 3 draws forms as **instances placed relative to the terrain**, and deliberately leaves the global ambient
offset alone (it is 0.77 m across a 59 m cell — a **0.75° slope change**, invisible by construction). So M1.2's 13
sites are **not** globally at risk. They matter **only inside a site footprint**, where they become the mandatory
exclusion set for site selection (plan rung SF3-3):

> landmask/water · `CutDisk`s · linework corridors · buildings · the SF1 hill's `r_cut` · high-`barren` rock ·
> `slope_at(dir) < s_max` · **and every M1.2 consumer within the site radius.**

---

## M3 — ★ BASELINE GATE: THE BASE IS RED, AND FOUR OF THE FIVE FAILURES ARE THE SLED

**Environment.** `main @ 7bae38718`, Debug/Ninja, deps staged from `winter-gi/build/_deps` (raylib 5.5, glm, Catch2,
toml++). Build **308/308 clean, exit 0**. `graph_query.py check` → `layer check OK`.

**Result.** `ctest --test-dir build -C Debug` → **exit 8** · **99% tests passed, 5 failed out of 1538** ·
total 2000.26 s.

★ **The live test count is 1538.** CLAUDE.md's "1028" and `gi_measurements.md`'s "1163" are both stale snapshots.
★ **`graphify.py --stale` is RED on the base** — tracked `generated/graph/graph.json` differs from a fresh scan.
Verified pre-existing (re-run with this document removed; unchanged). **Not regenerated here:** the law says
regenerate in the same commit as a structural change, and folding a main-side discrepancy into this branch would
disguise it as ours.

### The five
| # | test | assertion | expansion |
|---|---|---|---|
| 54 | `probe P-F: the relentless raider keeps the pump and shoots back` | — | enemy-AI probe; unrelated to snow |
| **799** | **`sled_slides_before_it_tips_on_flat_snow`** (`test_sled.cpp:1103`) | `max_roll < 0.35` | **0.4226** |
| **800** | **`sled_grip_ceiling_stays_below_the_tip_threshold`** (`:1312`) | `peak / onset <= 0.90` | **2.6226** |
| 838 | `sled_assist_reference_plane_is_load_weighted` (`:3123`) | `abs(nm_weighted) < 150.0` | 341.38 |
| 841 | `sled_debug_sink_is_write_only` (`:3456`) | `saw_release_under_one` | **false** |

### ★ M3.1 — #800 is Chad's superspin, and it is a RED LEG ON MAIN

Test #800's own diagnostic line, printed by the leg:

> `h_lat (Bush 0.30, load-weighted, frac=1) = 0.7416 m`
> `tip fence (Bush 0.30): peak = 9.3414 m/s^2 (0.953 g), onset = 3.5619 m/s^2 (0.363 g), ratio = 2.623`

The leg exists to guarantee **the machine runs out of lateral grip BEFORE it starts to tip** — grip ceiling *below*
tip onset, `peak/onset <= 0.90`. Measured **2.623**: lateral grip peaks at **0.953 g** while tipping begins at
**0.363 g**. The ordering is **inverted by 2.9×**. #799 is the same defect from the other side: roll reaches 0.4226
where the leg allows 0.35.

**Consequence, and it matches Chad's report exactly.** Chad, 2026-08-24: *"touching the ski to the bank just a
little and getting pulled in and spinning like 10 to 20 x, its a bit glitchy feeling."* With grip ceiling 2.6× above
tip onset, **any** lateral bite tips and rotates the machine instead of letting it slide out. A ski catching a bank
cannot break away, because breakaway is now far above the tipping point.

### ★ M3.1b — NOT A REGRESSION. A DELIBERATE, DOCUMENTED, ESCALATED OPEN ITEM.

No bisect was needed; the history carries the whole trace. Both legs were born in `01a3f2276` (S3 sled kernel
rebuild) and went red in the **GI4 RIDE** work, committed WIP-by-design with the failure count in the subject line
(`dccc24e9f` "GI4 RIDE (WIP, 1170/1175)"). Verbatim from that commit:

> *"GI4 breaks `sled_grip_ceiling_stays_below_the_tip_threshold` on Bush (0.567 -> 2.99). That rule — slide before
> you tip — is **WHY the machine cannot carve**, and it already failed on TrailMain/Road/RockOutcrop (1.6-10x),
> demoted in-file as 'an escalation for Chad'. **Carve harder and tip less cannot both come from grip, because grip
> IS the tipping moment.** Raising the tip threshold instead means moving cg_height_m/stance_m, which are MEASURED
> off the real machine and not mine to move."*

**What GI4 bought.** The planing lateral force fixed "can't hold a turn": Bush WOT full lock went **370 m → a dead
flat 33.2/33.5/33.7/33.9 m** at v0 8/12/16/20. Root cause of the old behaviour: lateral bite reads the Bekker normal
only, and planing lift had already taken the load out from under the skis.

**"Raise the tip threshold" was already tried and SIZED — geometry cannot close it** (`b971d14a9`): across the real
Indy's plausible range (stance 0.927→1.070 m, cg 0.564→0.450 m) tip onset moves **0.316 → 0.553 g, ~1.75×, where the
fence needs 3.3×**. Best cell ratio 1.236, still over 0.90. No geometry was moved — correctly.

**The proposed fix was built and FALSIFIED by its own measurement** (`ef7a2d682`, "Recording the kill rather than
the hope"):

| configuration | peak | onset | ratio |
|---|---|---|---|
| plate OFF | 0.588 g | 1.035 g | 0.568 |
| plate ON, no mean-norm | 0.953 g | 0.318 g | 2.991 |
| plate ON, mean-normalised | 0.952 g | 0.334 g | 2.847 |

Tip onset recovered **5% of what it must get back**. The stated attribution was wrong and the commit says so.

**The real mechanism was then traced** (`0d2fbea5e`). At the settled attractor the machine is **standing on its
hull** — 62 N of 4106 N on the running surfaces — held there by **planing lift** (2845 N), not the plate. The law:

> *"every restoring roll term is scaled by contact load (suspension normal, mu bite, and the assist via w_contact)
> and every destabilising one is not (planing lift and the GI4 plate, both ~ v²)"* → *"speed → lift → contact down
> → drag AND restoring down → speed"*

★ **That positive feedback loop is Chad's superspin.** Bite the bank at speed, and the terms that would arrest the
rotation are scaled by a contact load that speed itself has removed. Also still open from `dccc24e9f`: *"the Road
powered-turn rollover (8 of 8 cells at WOT with steer … and **Road is 37.7% of his drive**)"* — and Chad's report is
literally *"going down the road, touching the ski to the bank."*

**Consequence for SK-1.** The plan's two discontinuity findings stand and explain the *character* of the event — the
per-patch class step at `bank_class_min_m = 0.065` (×12 `pack_k_scale`, ×7.5 `c_snow_pa`) gives the glitchy onset,
and the nearest-line handoff step of up to `bank_height_m = 1.30 m` gives the launches at cuts. But the **magnitude**
— 10–20 rotations — is the load-scaled-restoring-vs-unscaled-destabilising loop. SK-1 is therefore **not a new
design problem**: it is an open GI4 escalation with a traced mechanism and a named next target (the planing-lift
runaway). It needs Chad's ruling on the grip-versus-tip trade, not a fresh investigation.

⚠ **Do not tune to make these green.** CLAUDE.md: *"A gate turned green by tweaking constants until it passes is
still a guess; find the root cause or stop and ask."* And these are feel legs — Chad drives.

### M3.2 — #841 is a determinism leg, and it is worth its own look
`sled_debug_sink_is_write_only` proves that attaching the debug sink does **not** change kernel behaviour
(the null-vs-filled bit-identity discipline). Failing means the instrument may be perturbing the thing it measures —
which would contaminate any measurement taken through it, including several SF3-0 items. Treat every debug-sink-derived
number on this base as suspect until this is understood.

### M3.3 — practical note
The Debug gate takes **~33 minutes** wall-clock, dominated by enemy-AI probe tests at ~60 s each. Budget for it;
do not run two ctest gates against one build dir.

---

## M2 — ⚠ BLOCKED: THE SNOW-CLIPPING QUESTION CANNOT BE MEASURED WITH THE SHIPPED TOOLS

**The question.** Snowform rev 2 asserts open snow clips to white above ~44.8° sun elevation
(`albedo 0.85 × (0.05 + 1.60·ndl) ≥ 1`, hard-clamped in `scurve1`, `render/post_glsl.cpp`), and that
therefore **no shading can be seen in daylight** and the winter exposure must change first. That claim is
the go/no-go for rungs SF3-2..SF3-4. It is currently **derived arithmetic, not a measurement.**

**Two instruments tried. Neither answers it.**

1. **`--smoke N shot.png CEL_OFFSET_S`** — the sun *does* wheel: diffing shots at offsets 0 and 150
   (half of `day_period_s = 300`) gives mean 4.20 DN, p99 76, max 153, **11.1% of pixels differing by
   >2 DN**. So the mechanism works. But the smoke viewpoint is the **aircraft at `SEADS_SPAWN_ALT=40`
   looking at the horizon**: the frame is dominated by sky and sun disc, with terrain as a dark
   silhouette. Sampling the lower 45% measured *shadowed terrain*, not sunlit snow. Whole-frame stats
   across the sweep: p50 49→56, p95 204→209, **max 246–249, clipped(≥254) = 0.00% at every offset.**
   Honest reading: **nothing clipped in any frame captured — but no frame captured the case in question.**
2. **`--probe ALT RANGE csv season day`** — has a background-patch luminance (`lum_bg`) over ~1212
   samples, which is the right quantity. But **`day_frac` is inert.** `app/main.cpp`, verbatim:
   *"season_frac/day_frac are the API surface for the celestial stages (Stage 1+); **inert now, logged
   honestly**."* Ten runs at `day` = 0.0…0.5 returned the **identical** `lum_bg = 0.4565`, `lum_plane =
   0.4055`. The parameter is logged and discarded.

**The one honest datum.** At the probe's fixed (unswept) celestial time, winter, alt 1500 m, range 800 m:
**`lum_bg = 0.4565`** — mid-range, **not clipped**. One point, at one unknown sun elevation. It does not
confirm or refute the claim.

**What M2 needs — and it is small.** Wire `probe_day` into `cel_time_offset`, the way `smoke_cel_offset`
already is (`app/main.cpp:2447`). The API surface exists and is documented as a placeholder for exactly
this stage; the sun mechanism is proven working by (1). Then sweep `day` and read `lum_bg` plus a
clipped-fraction over the background patch. **Until then no number in SF3-2..SF3-4 may cite a clipping
percentage**, and the exposure ruling rests on arithmetic rather than measurement.

⚠ This is the same shape as SK-3's missing probe modes: the plan needs measurements the shipped
instruments cannot take. Two rungs now depend on small, honest instrument work landing first.

---

## M4–M7 — remaining

Recorded here as they land. No downstream rung may quote a number that is not in this file.

---

## M8 — GI4 §9.7 ITEM 2 (TRACTION-LIMIT THE THRUST): BUILT, MEASURED, **AND IT IS NOT CHAD'S BUG**

**Chad's ruling, 2026-08-24, verbatim:** *"the planing lift is important for traversing atop the snow so that is
important it stays, and at slower speeds I think it is good too because [I'd] like to jump the snowbanks, but at
high speeds, getting pulled in and flipping out like 15x is not desirable so yes take care of that for this sandbox."*

**What was built.** `sim::SledParams::traction_mu` — a CONTACT ceiling on track thrust, applied after the two
existing ENGINE ceilings (`max_thrust_n`, `engine_power_w/v`). Rationale: `shear`'s cohesion term (`area*c_eff`) and
`roost_thrust` are both load-INDEPENDENT, so a track carrying 55 N still pulls `max_thrust_n`. `<= 0.0` = OFF as a
BRANCH (structurally bit-identical), registered in `SLEDTAPE_PARAMS_D` with an OFF-by-absence entry.

**OFF leg verified.** `[sled]` = 79 cases, 75 passed, **4 failed — the same four GI4 baseline lines, one assertion
each.** Nothing new broken, nothing newly green.

### M8.1 On the term it targets, it works — and at mu >= 3.0 it is FREE
`probe attractor 16 1.0 12 0` (Bush, WOT, full lock, the §9 runaway):

| traction_mu | outcome |
|---|---|
| OFF (baseline) | `t(|roll|>20) = 6.22 s`, **v_end 24.8 m/s and rising**, thrust pegged 2272 N every row |
| 0.6 / 1.0 / 2.0 / 3.0 / 4.0 | `t = 1.33 s`, **v_end 0.1 m/s** — the runaway does not develop |

Bush straight-line WOT traverse (`bush v0=2/6/12 stand+back`), Chad's "traversing atop the snow":

| traction_mu | v_end (m/s) | verdict |
|---|---|---|
| OFF (baseline) | 14.1 / 14.5 / 15.3 | — |
| 1.0 | 9.6 / 9.6 / 9.6 | **−35%. Unacceptable — this is the property Chad ruled must stay.** |
| 2.0 | 13.6 / 13.7 / 13.8 | still short |
| **3.0** | **14.1 / 14.5 / 15.3** | **IDENTICAL to baseline — free** |
| 5.0 | 14.1 / 14.5 / 15.3 | identical |

So a window exists at `traction_mu >= 3.0`: the Bush planing runaway is stopped and the traverse is untouched.
Planing lift itself is never modified, per the ruling.

### M8.2 ★ BUT IT DOES NOT FIX WHAT CHAD REPORTED, AND THE REASON IS STRUCTURAL
Chad's words are **"going down the road, touching the ski to the bank"**. `probe carve`, ROAD rows:

| | baseline (OFF) | mu = 1.0 | mu = 3.0 |
|---|---|---|---|
| road neutral v0=16 | ROLLED, max −179.5 | ROLLED, max −180.0 | ROLLED, max +180.0 |
| road lean-in v0=16 | ROLLED, max −179.8 | ROLLED, max −179.6 | ROLLED, max −179.9 |
| road neutral v0=20 | ROLLED, max −180.0 | ROLLED, max +179.4 | ROLLED, max −179.9 |
| road lean-in v0=20 | ROLLED, max −179.0 | ROLLED, max −115.7 | ROLLED, max −128.2 |

**Every Road carve row rolls the machine right over, at every value of the dial.** The cap is *inert on Road by
construction*: Road has `rho_eff = 0`, so there is no planing, the machine stays in full contact, and
`traction_mu * normal` never falls below the engine ceiling. Proof: Road straight-line `v_end` is **25.9 / 27.1 /
30.8 at every mu tested, byte-identical to baseline.**

### M8.3 Conclusion — recording the kill, not the hope
§9.7 item 2 addresses the **Bush planing-lift attractor**. **Chad's superspin is the ROAD bank-strike, and they are
different mechanisms** — there is no planing on Road, so the whole speed→lift→contact-down→speed loop that items 1–3
target cannot be what he is hitting. The GI4 handoff already flagged this separately as **STILL OPEN**: *"the Road
powered-turn rollover (8 of 8 cells at WOT with steer … and Road is 37.7% of his drive)"*.

**SHIPS AT 0.0 pending Chad's ruling.** It is a real fix for a real, traced, open defect, and the mu >= 3.0 window
costs nothing measured — but it is not the thing he asked for, and shipping it as though it were would be exactly
the "plausible-but-wrong change" this repo's process exists to prevent.

**The live candidate for the Road bank-strike** is the per-patch class step (plan §SK-1a): `world/snowpack.cpp`
flips a patch to `TrailMain` the instant `bank_profile(e)*gap > bank_class_min_m = 0.065` — **6.5 cm** — and
`sim/sled.cpp` looks the dials up per contact patch, so one ski grazing a bank steps that patch alone by
**x12 `pack_k_scale`, x7.5 `c_snow_pa`, x1.27 `mu_lat`** in a single tick. On Road, a bank is the only snow the
machine touches. That is the next thing to measure.

---

## M9 — ★ SK-1a: CHAD'S BANK-STRIKE REPRODUCED, AND IT IS A CLASSIFICATION STEP

**New instrument:** `seads_sled_probe bankgraze <v0> [half_w]`. A REAL plowed (`RoadMajor`) corridor via the existing
`road_field()`, so `classify()` and `bank_profile()` run for real — no synthetic `sample_override`. The machine is
placed at a lateral OFFSET from the centreline and driven **straight, throttle open, NO steer input**, 3 s.
Surface enum: `0 Bush · 1 TrailMain · 2 TrailTributary · 3 Road`.

### M9.1 The reproduction (half_w 6.0 m, so the corridor edge is at 6.0 m)

| lat [m] | L,R,trk | v0=6 yaw / roll | v0=10 yaw / roll | v0=16 yaw / roll | v0=24 yaw / roll |
|---|---|---|---|---|---|
| 5.50 | 3,3,3 | −0.0 / 2.4 | −0.0 / 2.4 | −0.0 / 2.4 | −0.0 / 2.4 |
| **5.75** | **0**,3,3 | 0.1 / 2.7 | 0.0 / 2.7 | **−89.0 / 89.5** | **−482.7 / 166.6 ROLLED** |
| 6.00 | 1,3,0 | 0.3 / 7.3 | 0.2 / 7.2 | −38.1 / 38.6 | **−597.8 / 171.1 ROLLED** |
| 6.75 | 1,0,1 | — | — | **137.6 / 112.1 ROLLED** | — |
| 7.00 | 1,1,1 | — | — | **586.7 / 163.5 ROLLED** | — |

**Read the 5.50 → 5.75 step.** A **25 cm** lateral move — one ski crossing the corridor edge — takes the machine
from **yaw 0.0°, roll 2.4°** (perfectly stable) to **89° of roll at 16 m/s and 1.34 ROTATIONS at 24 m/s**, with the
steering dead centre. −597.8° at 6.00 m is 1.66 rotations and 171° of roll: fully inverted.

★ **The speed split is exactly Chad's ruling.** *"at slower speeds I think it is good too … but at high speeds,
getting pulled in and flipping out like 15x is not desirable."* At the identical offset, 6 and 10 m/s are benign
(roll 2.7°); 16 and 24 m/s flip. The threshold sits between 10 and 16 m/s.

### M9.2 The mechanism: TWO discontinuous class steps, per patch, in one tick
`world/snowpack.cpp` `classify()` returns a surface CLASS, and `sim/sled.cpp:492` looks the dials up **per contact
patch**. Crossing the corridor edge steps ONE ski, alone:

| dial | Road (3) | Bush (0) | TrailMain (1) |
|---|---|---|---|
| `mu_kin` | 0.220 | 0.045 | 0.032 |
| `mu_lat` | 0.60 | 0.55 | 0.70 |
| `c_snow_pa` | **25000** | **1200** | 9000 |
| **`rho_eff`** | **0.0** | **260.0** | **380.0** |
| **`sinkable`** | **false** | **true** | **true** |
| `pack_k_scale` | 1.0 | 1.0 | **12.0** |

`rho_eff` **0 → 260** is the violent one: on Road the planing lateral plate and the roost are *identically zero*, and
the instant one ski crosses the edge that patch alone develops the FULL plate — a lateral force with no counterpart
on the other side, applied at a patch position offset from the CG, i.e. a pure yaw+roll couple that appears as a
STEP. `sinkable` false → true switches sinkage, planing lift and plow drag on at the same instant, and
`c_snow_pa` drops by **21x**.

This is a second instance of the class the plan's SK-1b already names (the nearest-line handoff): **`bank_profile`
is C1 by construction, but the SURFACE CLASS it feeds is a step function, and the dials behind the class are not
continuous with it.** The C1 guarantee stops at the classifier.

### M9.3 Why item 2 could never have fixed this
`traction_mu` is inert on Road (`rho_eff = 0` → no planing → full contact), confirmed in §M8.2 by Road straight-line
`v_end` being byte-identical at every mu. The GI4 §9.7 loop is the Bush planing attractor; **this is a different
defect on a different surface**, and it is the one Chad reported.

### M9.4 The candidate fix (NOT yet built, NOT yet measured)
Blend the per-patch dials across the class transition instead of switching — the same smoothstep `bank_profile`
already uses — so `bank_class_min_m` becomes the start of a ramp rather than a cliff, and the Road→Bush corridor
edge gets the same treatment. It touches no groomed-trail behaviour away from an edge, and OFF must be
bit-identical. **The blend width is a MEASUREMENT, not a guess:** sweep it on this probe against the v0 6/10/16/24
rows, and put the table in front of Chad.

---

## M10 — SK-1a FIX BUILT: THE CLASS BLEND, AND ITS SWEEP

**What was built.** `world::SnowParams::class_blend_m` — the lateral width over which a patch's dials RAMP between
the corridor class and the off-corridor class instead of stepping.
- `GroundSample` gains `surf_b` (the class being blended toward) + `surf_mix` in [0,1]. **Tape-safe:** the ground log
  writes only `drive_r`/`depth_m`/`surf`, so a taped sample reconstructs `surf_mix = 0` — the pre-SK-1a step, i.e.
  the kernel that actually drove the tape.
- `classify()` gained `force_outside` so the SAME function answers "what is the class just outside this edge" —
  **no second implementation to fork.**
- `sim/sled.cpp` lerps the seven continuous dials via `blend_dials()`. `sinkable` is a bool and takes the DOMINANT
  class, so it flips at mix 0.5 where the continuous terms are already half-way, rather than at the boundary where
  they all stepped together.
- **`class_blend_m = 0.0` ships, and is a BRANCH, not a 0-weight lerp — bit-identical.**

### M10.1 The sweep (probe `bankgraze`, straight, throttle open, NO steer, 3 s)

**v0 = 24 m/s, lat 5.75 m (one ski ~21 cm past the corridor edge):**

| `class_blend_m` | yaw total | max roll | v_end | verdict |
|---|---|---|---|---|
| **0.0 (shipped)** | **−482.7°** | **166.6°** | 20.2 | **ROLLED** |
| 0.5 | −38.4° | 68.7° | 27.5 | — |
| **1.0** | **−11.6°** | **52.7°** | 30.5 | — |
| 2.0 | −30.2° | 46.6° | 30.5 | — |

**No rollover at any blend width.** At 1.0 m the yaw excursion falls **40x**.

### M10.2 The full speed table at `class_blend_m = 1.0` — Chad's ruling, checked

| v0 | lat | yaw OFF → ON | max roll OFF → ON | verdict |
|---|---|---|---|---|
| 6 | 5.75 | 0.1° → 0.1° | 2.7° → 2.8° | **unchanged** ✅ |
| 10 | 5.75 | 0.0° → 0.2° | 2.7° → 2.8° | **unchanged** ✅ |
| 16 | 5.75 | −89.0° → **−27.7°** | 89.5° → **30.0°** | 3x better |
| 16 | 6.00 | −38.1° → **−0.2°** | 38.6° → 18.9° | ~gone |
| 24 | 5.75 | −482.7° → **−11.6°** | 166.6° → **52.7°** | **rollover gone** |
| 24 | 6.00 | −597.8° → **3.5°** | 171.1° → 33.7° | **rollover gone** |

★ This is exactly the split Chad ruled: *"at slower speeds I think it is good too … but at high speeds, getting
pulled in and flipping out like 15x is not desirable."* 6 and 10 m/s are untouched to within noise; 16 and 24 stop
flipping.

### M10.3 Honest costs
1. **The blend leaks INWARD by `class_blend_m`.** Rows 0.5 m inside the edge move slightly (v_end 34.4 → 35.0 at
   v0 24). Driving near a road edge is marginally faster/looser than before. Arguably correct — a plowed edge is not
   a knife edge — but it IS a change to on-road feel near the shoulder, and Chad drives 37.7% Road.
2. **Low-speed straight-line speed rises ~3-4%** on the edge rows (v0 6: 18.8 → 19.6; v0 10: 22.5 → 23.2), same
   cause.
3. **2.0 m is worse than 1.0 m on yaw** (−30.2 vs −11.6) while leaking twice as far inward. 1.0 m is the measured
   best of the swept set — but the set is coarse (0 / 0.5 / 1.0 / 2.0) and the optimum is not resolved.
4. `sinkable`'s bool flip at mix 0.5 is still a step. It is a much smaller one (every continuous term is half-way
   there), and the measurements say it no longer produces a rollover — but it is not continuous, and that is a
   named residual, not a solved problem.

### M10.4 Status
**SHIPS AT 0.0 PENDING CHAD'S RULING AND HIS DRIVE.** The numbers say 1.0 m; the repo's law says a feel dial is
ruled on the stick, not on a probe table (*"the numbers explain a score, they never replace the stick"*).

---

## M11 — SK-1b: "IT WON'T CARVE ON WSAD" — THE INPUT IS NOT THE CONSTRAINT, THE KERNEL IS

**Chad, 2026-08-25:** *"I am not enough able to hold a steady turn as I am using wsad and it wont carve so well is
this something perhaps adding this snow could ammend or is it better to go straight to kernel"*

### M11.1 The input finding (real, but NOT the cause)
`app/main.cpp:3272` — `sin_.steer = live.override_mask[2] ? override_sign[2] : 0.0f`. With A/D the steer command is
**binary +/-1**, and `steer_rate_per_s = 2.0` reaches full lock in **0.5 s**. A held key is ALWAYS full lock; a tap
decays straight back to 0. **There is no way to HOLD a partial steering angle on the keyboard.** That is a genuine
gap — but it is not what stops the carve.

### M11.2 The measurement that settles it — new probe `steersweep`
ROAD, WOT, 4 s, steady-state yaw over the last 2 s:

| steer | v0=8 R / maxroll | v0=12 R / maxroll | v0=16 R / maxroll |
|---|---|---|---|
| 0.10 | 624 m / 6.2° | 692 m / 6.3° | 1042 m / 4.8° |
| **0.20** | **7.6 m / 150.5° ROLLED** | **5.2 m / 149.9° ROLLED** | **4.5 m / 169.1° ROLLED** |
| 0.30–1.00 | ROLLED | ROLLED | ROLLED (2.5–5 m, 149–178°) |

★ **There is no carveable steer angle on Road at ANY speed tested.** The machine goes from *barely turning*
(R 624–1042 m at steer 0.1) to *inverted* (roll 150–169°) in one 0.1 step of command. **A perfect analog steering
axis would have nothing to hold** — the stable cornering regime does not exist to be found.

### M11.3 Attribution
This IS the known-red fence, `sled_grip_ceiling_stays_below_the_tip_threshold` (§M3.1): peak lateral **0.953 g** vs
tip onset **0.363 g**, ratio **2.623** where the leg requires <= 0.90. The machine reaches its tipping moment long
before it runs out of grip, so **any** genuine steering input rolls it. On Road specifically there is no relief,
because GI4's carve fix (`plane_lat_gain`, which took Bush from 370 m to a dead-flat 33 m) is **structurally inert
on Road**: `rho_eff = 0`, so the planing lateral plate is identically zero there.

### M11.4 Answer to Chad's question
**Kernel — and snow cannot amend it.** Road is `road_bare_m = 0.02`, not sinkable, `rho_eff = 0`: there is no snow
on a plowed road for any snowform work to act through. The carve is limited by the grip/tip ordering, which is
machine geometry + dials, not ground cover.

Ordering that follows: the **grip/tip inversion is the blocker for BOTH** Chad's felt items — the bank-strike
magnitude (SK-1a's blend reduces the trigger but the machine still tips easily) **and** the missing carve. It is
already an escalation awaiting his ruling (`dccc24e9f`: *"Carve harder and tip less cannot both come from grip,
because grip IS the tipping moment"*; `b971d14a9`: geometry alone moves tip onset 1.75x where the fence needs 3.3x).
The keyboard steer gap (§M11.1) is worth closing **as well**, but closing it alone would change nothing.

---

## M12 — ★ THE §3 "CARVE BAND" IS AN OBSERVATION-WINDOW ARTIFACT. IT DOES NOT EXIST.

**Instrument:** `seads_sled_probe steersweep <v0> <lo> <step> [steer_max] [lean_follow] [secs] [throttle]`
(the last three are new; `throttle < 0` engages a P speed-hold at `v0`). Road, `road_field(0.85, 6.0)`.
**Cause of the audit:** the handoff's §3 reported a non-rolling turn at steer 0.11 (R 98.7 m, roll 34.6°) —
a band 1% of the axis wide. That number was taken in a **4-second** window. It is a rollover, caught early.

### M12.1 The same row, one instrument, window length swept (v0=16, WOT, lean 0)

| secs | R [m] | roll_max | roll_end | d_roll/s | verdict |
|---|---|---|---|---|---|
| 3 | 21 997 | 2.4° | 2.0° | +0.0 | settled |
| **4** | **98.7** | **34.6°** | **34.6°** | **+32.6** | **STILL TIPPING** |
| 5 | 18.8 | 123.8° | 116.1° | +81.6 | ROLLED |
| 6–16 | — | 149.0° | — | — | ROLLED |

**The 4-second row reproduces §3's numbers exactly — and is climbing at 32.6°/s when the window closes.**
One more second and it is past 120°. §3 did not measure a carve; it measured the first second of a rollover.
`max_roll` alone cannot tell the two apart, which is why the sweep now also reports `roll_end` and `d_roll/s`.

### M12.2 Speed was ruled out as the confound
WOT for 12 s accelerates v0=16 to ~45 m/s, so a late roll could have been speed rather than steer. Two controls:

- **Speed-held (P controller on `v0`), 12 s, v0 = 12 and 16, coupled:** every steer from **0.02 to 0.26** rolls
  past 140°. Constant speed does not save it.
- **Zero steer, 12 s:** `roll_max 1.4°, settled` speed-held; `2.4°, settled` at WOT reaching **44.9 m/s**.
  The machine is rock-stable at 45 m/s with no steer. **The roll is caused by STEER, at any nonzero value.**

### M12.3 The SK-1c lean coupling was measured, and it does not widen anything
`lean_follow` now drives `in.lean_lat = follow * steer`, so the sweep measures the machine SK-1c actually ships.
At the 4 s window the coupling **shifts** the apparent band down one step (v0=16: the apparent non-roll row moves
0.11 → 0.10) and leaves its width unchanged. At 12 s both coupled and uncoupled roll everywhere. The coupling is
neither the cause nor a cure; §3's numbers were simply describing a machine that also no longer ships.

### M12.4 What this confirms and what it kills
**Confirms M11.2 as originally written** — *"there is no carveable steer angle on Road at ANY speed tested"* —
and attributes it to the same known-red fence (M11.3, grip 0.953 g vs tip onset 0.363 g). The finer sweep that
appeared to find a survivor between the 0.1 steps did not find one.
**Kills:** the §3 band, the "1% of the axis" framing, and with it **the planned SK-1c band marker — there is no
band to mark.** Painting 0.10–0.11 on the HUD would have shipped a lie into the instrument.
**Does not change:** SK-1c's input work stands on its own (an analog axis is still strictly better than a binary
one), and the grip/tip inversion remains Chad's ruling, untouched.

