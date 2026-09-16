# HANDOFF — 2026-08-27: BLOCK-VP1 CLOSED (R1), TRACKS, ROOST SIZED, GYRO G1+G2, R3 DEPTH CHANNEL

> ✅ **SUPERSEDED FOR THE WORK QUEUE by `docs/snow_session_handoff_20260827b_NEXT.md`.**
> Chad drove and SIGNED the R3 look rung; everything here is committed (`e5d89e3f9`).
> **This file remains the authority for MECHANISM and MEASUREMENT** — §3 and §7 are still
> the record of how each thing works and what it measured. Read the NEXT file for what to do.

> **LAUNCH LINE (paste into a fresh session):**
> Read `docs/snow_session_handoff_20260827.md`. **BLOCK-VP1 is closed at R1** — the drawn world now
> carries the snowpack and the machine no longer floats 0.77 m above it, with the drive provably
> bit-identical. **§4 is what Chad must rule on the stick; nothing in §3 is approved until he
> drives it.** `docs/snow_build_handoff.md` is SUPERSEDED by this file for everything it covers.
> **R3 (§3.7) is RULED IN AND NOW SHIPS ARMED (`depth_mix = 1.0f`) — see §0 below for
> Chad's drive verdict, which IS the acceptance test for it.** The two gyro dials
> (`k_gyro`, `k_gyro_react`) remain **0.0 and unruled**.

---

## 0. ★★★ CHAD'S DRIVE VERDICT — 2026-08-27. READ THIS BEFORE §3.

He drove the armed build (`SEADS_SNOWDEPTH=1`) and ruled, verbatim:

> *"I saw the shatter cones, the black of the barren rocks are still there, good. I dont
> really see any track marks or snow depression unless I am in the deep stuff. I can kind
> tell there are crests and it feels like snow, but the track dosent sink down when I
> accelerate in regular snow the whole machine sinks in deep snow.... Herd to say I didnt
> see much difference until I was in the deep snow then I sunk half my machine but there is
> no intermediate"*

**PASSED, and it is the only thing R3 could fail:** the black-rock fence held under armed
exposure. His eye agrees with the measurement (sloped rock 0.199 vs the 0.230 he had already
approved). **This also DISCHARGES §5's owed shatter-cone re-probe** — arming exposure changes
what covers the cones, and he found them.

**On his say-so R3 now ships armed.** `render/planet.h` `SnowParams::depth_mix` 0.0f -> 1.0f.
The A/B survives the flip: `SEADS_SNOWDEPTH=0` turns it off with no rebuild, `=0.5` sweeps it.

### 0.05 ★★★ SECOND RIDE, SAME DAY — THE INTERMEDIATE IS FOUND, AND THE FENCE IS SPLIT

He swept the saturation depth live (PgUp/PgDn, value on screen) and ruled:

> *"0.10 for the win I see intermediate and the whiter tone is better"*

then, after the fence fix, *"i like it"* — and on the ride after that:

> *"i like it but the rocks cut kinda straight edged best go to 0.3"*

**SHIPPED: `full_depth = 0.30f`, `barren_shed_keep = 1.0f`** (both `render/planet.h`).

★ **WHY 0.10 CUT STRAIGHT-EDGED — the same mechanism as the win, and you need both
halves.** Coverage is `smoothstep(0, full_depth, depth)`, so the SMALLER the value, the
narrower the depth band over which snow fades to rock. At 0.10 m that band is thinner
than the field's own variation across a rock edge, so the transition collapses to
near-binary and reads as a straight line drawn on the terrain — hard-alpha. 0.30 widens
it ~3x and the edge follows the rock again, **without undoing the win**: 0.30 still
saturates far below the census p50 of 0.77 m, so exposure stays near-flat across ridden
ground and the fold's relief still does the work. **0.30 also measures BETTER on the
fence than 0.10** (sloped rock 0.145 vs 0.168) at a negligible tone cost (open ground
0.985 vs 0.987).

⚠ **WHY 0.10 WORKS IS NOT WHAT THE SWEEP WAS AIMED AT. DO NOT "CORRECT" IT BACK UP.**
§0.3 cause 2 said the shading SATURATED too low and implied the fix was to raise it. He went the
other way, and the result is coherent: at 0.10 essentially all snow saturates, so depth-keyed
exposure stops varying — **and that is the point.** The varying exposure was FIGHTING the R1 fold's
own relief, washing out the hollows and crests the geometry already carries. Flat exposure lets the
SHAPE do the work. **The intermediate he sees is GEOMETRIC, not tonal.** The saturation finding is
what put the dial on his stick; the answer it produced was the opposite end of that dial.

⚠ **AN A/B WHOSE ARMS ARE INDISTINGUISHABLE IS NOT A MEASUREMENT.** His first sweep was
unattributable — *"I didnt see what the ladder was at"* — because the value lived only in a console
line he could not read while driving. The rung that followed put it ON SCREEN and on PgUp/PgDn, and
only then did the verdict mean anything. **Any dial he is asked to rule on must be visible and
steppable from the seat.**

### 0.06 THE FENCE WAS WELDED TO THE TONE DIAL — NOW IT IS TWO DIALS

0.10 alone **broke his black-rock fence**: at that saturation the depth field can no longer express
the shed, so sloped rock measured **0.230 → 0.431**, nearly double the coverage he had approved.
The shed had been faded out entirely by `depth_mix` on the argument that `vSnowDepth` already
carries it — **true only while `full_depth` is deep enough for the depth field to express it.**

Put to him as options rather than silently resolved; he chose **B, the root fix**: give the shed its
own dial. The FS line is now
`snowCover *= (1.0 - max(1.0 - uSnowDepthMix, uBarrenShedKeep) * uBarrenSnowShed * bshed * bgate)`.
`max()`, not a product or sum: `keep = 0` is EXACTLY the old expression, `keep = 1` holds the fence
at any mix, and max can never shed harder than one full application — so the double-shed the old
comment feared is unreachable at any dial value.

**MEASURED AT THE SHIPPED VALUES (459,717 land samples, `SEADS_R3_PROBE=1`):**

| surface | legacy (approved) | 0.10, shed welded | 0.10 + keep 1.0 | **0.30 + keep 1.0 (SHIPPED)** |
|---|---|---|---|---|
| open ground | 0.992 | 0.989 | 0.987 | **0.985** — his white tone |
| mapped rock, flat | 0.910 | 0.998 ✗ | 0.910 | **0.909** — exact |
| **rock, SLOPED ≥12°** | **0.230** | 0.431 ✗ | 0.168 | **0.145** — *blacker than approved* |
| rock, STEEP ≥20° | 0.094 | 0.195 ✗ | 0.062 | **0.056** — *blacker* |

Both of his rulings are satisfied at once, which was impossible while they shared a dial. The old
fade-only form is pinned as an **ANTI-needle** in `test_winter_reskin.cpp` — restoring it would
silently re-weld the fence, which is exactly the regression that passes a green build. The probe's
DEPTH column models the surviving shed, so the fence stays MEASURABLE rather than argued.

### 0.07 ★ THE LAWN OBSERVATION SETTLED §3.2b — "ACCEPT IT" IS DEAD

Unprompted, he reported: *"on peoples lawns seems to sink a bit less"*. Measured the same session
(`SEADS_VP1_GAP=1`), drawn-above-driven by distance out from a corridor: **20-40 m out +0.397 m**,
40-80 m +0.101 m, beyond 80 m ~0.000 m. Lawns sit inside the 60 m corridor mask, where the drawn
ground floats ~0.4 m above what he drives on — so he sinks into a surface already drawn too high,
and it reads as less sink. §3.2b's option 3 was *"Accept it — 0.42 m in a band with no nearby
reference may simply not be visible."* **He saw it. Option 3 is ruled out**, and R2's annulus strips
(option 1) are justified rather than speculative. **He has NOT yet chosen between options 1 and 2.**

### 0.08 ⚠ OPEN AND UNMEASURABLE: "SOMETIMES MY SKI IS GOING DOWN INTO THE ROAD"

Reported the same ride, **not closed**. The road DECK measures **0.0% under** across 9,321 samples,
and open ground shows drawn-above-driven at 40% of samples but only **6-9 mm** (the known
facet-interpolation jitter, the likeliest thing he is catching at a grazing ski angle). **But the
instrument cannot adjudicate the place he reported.** `SEADS_VP1_GAP` compares the drive against the
MESH, while the 0-9 m band beside a road is drawn by the BANK STRIPS (§3.2b's own probe caveat).
**No instrument in the repo measures the drive against `bank_mesh`.** Build that measurement before
proposing any fix — do not tune the banks against a report no instrument can confirm.

### 0.1 "THERE IS NO INTERMEDIATE" — three causes, none of them this dial

His report is not a defect in R3. It is the ceiling §7 predicted, confirmed by his eye, plus
two debts that were already open. **Do not answer any of it by moving `full_depth`.**

1. **The ambient field cannot carry the signal.** `world/snowpack.h:23` IS the homogenizer:
   2.6 cm per 2 m, a 0.74 deg slope. R3 shades faithfully FROM that field, so faithful
   shading can only produce broad regional gradients — "deep stuff" and "not deep stuff",
   nothing between. §7 bullet 1 wrote this down before he drove. **Metre-scale relief must
   come from deformation (R4) and micro-relief (R5).**
2. **The rut depth is a flat constant.** `world/tracks.h:44` `depress_m = 0.12` — the same
   12 cm groove in 5 cm of trail crust as in 0.9 m of bush pack. The track IS laid every
   physics tick (`app/main.cpp:4104`) and he IS feeling it; it simply cannot deepen with the
   snow, which is exactly "it doesnt sink down when I accelerate". §3.3 already carries the
   fix: one law, `deform = k * depth_at_when_laid`, k ~ 0.45. ⚠ Stamps are bare `glm::dvec3`
   and carry NO depth and NO heading — this needs a per-stamp scalar in the ring, not a dial
   change.
3. ⚠ **NEW, AND A REAL REGRESSION OF THE A/B: arming R3 makes the track HARDER to see.**
   The only surface that draws a rut is `render/snow_patch.cpp`, the 79.5 m rider patch.
   §3.7's bug-2 fix forces `uSnowDepthMix = 0` on that patch's own draw so it cannot paint a
   bare-ground rectangle under the rider. Net: **the one surface showing his ruts is the one
   surface excluded from the new depth-keyed lighting** — the ground around the rut shades by
   depth, the rut keeps flat legacy shading. That was the correct call for a layer §3.4
   retires at R4, but it means "shaded and felt" stops exactly where the track is, and it is
   half of why he saw no difference outside the deep stuff. **R4 removes this by DELETING the
   patch, never by plumbing depth into a doomed layer.**

### 0.2 WHAT IS COMMITTED

⚠ §1's "nothing committed this session" was true when written and is superseded HERE: this rung is
being committed on `sandbox/snow`. **Still NOT approved and NOT ruled:** §3.2b (the R2 shoulder
fork), G1, G2 (both built, tested, **NEVER DRIVEN**), and the roost spec amendment (§4 card 5).

### 0.3 ★★★ WHAT THE RED TEAM FOUND — TWO CAUSES OF "NO INTERMEDIATE" THAT R4 CANNOT FIX

A three-lens Fable red team ran against this rung on 2026-08-27. It **killed one of §0.1's own
claims and found two mechanisms §0.1 missed.** Read this before planning R4.

1. ❌ **§0.1 cause 3 ("arming R3 makes tracks HARDER to see") is a POST-HOC STORY. Do not act on
   it.** The rut's legibility comes from the patch's GEOMETRIC NORMALS, which are fully armed on
   that draw (`vertex_normal_mix = 1.0`, `render/planet.cpp`) and are untouched by the depth-mix
   flip. What forcing mix=0 actually does is give the 79.5 m patch a DIFFERENT BASE EXPOSURE from
   the depth-shaded ground around it — where ambient depth < `full_depth`, the patch reads WHITER,
   i.e. MORE conspicuous. Nothing in the repo measures rut visibility at mix 0 vs 1. The mechanics
   of the claim are confirmed; the DIRECTION was asserted without an instrument.
2. ★ **THE SHADING SATURATES BELOW THE MAP'S MEDIAN DEPTH.** `smoothstep(0, 0.70, vSnowDepth)`
   against a measured census of p5 0.37 / **p50 0.77** / p95 1.14 / p99 1.75 m
   (`config/world.toml:372`). More than HALF of all land is clipped at full coverage — 0.77 m and
   1.75 m shade BIT-IDENTICALLY. The intermediate band he asked for is arithmetically discarded.
   §0.1's header originally forbade touching `full_depth`; **that prohibition is RETRACTED.**
3. ★★ **THE SLED'S OWN PLANING LAW IS BIMODAL, AND NO RENDERING RUNG TOUCHES IT.** `sim/sled.h:551`,
   verbatim: *"Above the hump the planing operating point is also identical across 0.21..0.30
   (sinkage settles at 0.080 m)"*. The machine rides at ~8 cm of sink at EVERY depth that planes,
   then bogs past ~1.3 m (`track_clearance_m = 0.255`, `sled.cpp:1047`) and buries. That IS his
   "no intermediate", it is **designed and Chad-signed** ("median planes, p99 bogs"), and **R4 and
   R5 cannot move it by one centimetre.** The only honest lever is the planing dials, which are
   HIS to reopen. **Expect him back with "the machine still doesn't settle" if R4 ships alone.**
4. The world is **NOT** the problem: the depth field is smooth and unimodal (census above). The
   intermediates exist; they are erased downstream, twice, by 2 and 3.
5. Minor, unflown: `winterSurf = max(snowCover, iceCover)` gates the S1b night glow and sparkle, so
   thin/scoured ground now also loses night whiteness. **His night rulings were made against the
   OLD mask.** A night pass is owed before this dial is considered settled.
6. ✅ **DONE — Chad asked for it the same day.** `SEADS_R3_FULLDEPTH` is now wired into the live
   draw beside the `SEADS_SNOWDEPTH` read, so cause 2 is sweepable from the seat with no rebuild.
   See §4 card 4. **The sweep is the experiment; his eye is the verdict. UNFLOWN as of this line.**

---

## 1. STATE

| | |
|---|---|
| **Branch** | `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow` |
| **Base** | `f92939e2e` — **nothing committed this session** |
| **Gate** | Full ctest **1562 cases, 5 red — all documented baseline** (4 pre-existing GI4 sled debt: 815/816/854/857, + the raider leg 54 that `SESSION_HANDOFF_20260823_enemy_ai.md:95` calls *"ON MAIN ON PURPOSE"*). **Zero new failures.** ⚠ R3 initially broke a 6th, **893** — see below; fixed, not suppressed. Runtime ~30 min. |
| **Drive** | **Provably unchanged.** A 173,118-sample census run with the fold on and off is byte-for-byte identical. Re-runnable: `SEADS_SNOWDUMP=<f>.tsv` with/without `SEADS_NO_SNOWFOLD=1`. |

⚠ **893 is worth knowing about before you touch the planet FS.** `test_winter_reskin.cpp` pins the
shader's shed line by its **exact source text**, so any edit to it fails the gate by design — that is
the tripwire working, not noise. R3 amended the line and the pin was **updated to the new
expression** (and given a leg forbidding the *un-faded* form, which would shed the black rock twice).
Never make that test pass by loosening the needle.

⚠ **Two ctest gates must never share `build/`.** A mutation-verify rebuild mid-run silently voided a
whole 23-minute gate this session — it reported `exit 0` with most cases **"Not Run"**. If a gate
comes back suspiciously clean, check for "Not Run" before believing it.

⚠ **NO ctest runs `seads.exe`.** Chad's drive IS the gate. The `--smoke` path spawns the PLANE at
2500 m looking at space — it proves the shaders compile and the frame renders, and it CANNOT judge
ground look. Do not mistake a green smoke shot for a verdict on the snow. Everything in §3 is built and measured;
none of it is approved.

⚠ **This repo has a heredoc hazard.** `bash <<'EOF'` mangles backslash escapes inside Python source.
It cost a truncated `sim/sled.cpp` this session (restored from git, nothing lost). **Write edit
scripts to a file and run them; never inline a heredoc containing escapes.**

---

## 2. THE THROUGH-LINE, IN CHAD'S WORDS

He rejected two builds before the right altitude of fix was found:

> *"there are no ruts, its depressing the whole square area"*
> *"I should see skis going into snow, deeper snow should bury me a bit ... Snow that is properly
> shaded and felt, not some cheap drawn in illusion ..... I though yuo were going to bake me in some
> snow on this whole planet?"*

**The lesson, and it is the reason this session worked once it did:** a local cosmetic patch was
built twice while the real defect (BLOCK-VP1) sat named and open in the repo's own docs. Put the
local fix AND the root fix to him as options before building either. He said so directly:

> *"let me know about these kinds of options in the future and if yuo are just performative in your
> cheap window dressings or not."*

---

## 3. WHAT LANDED (built + measured, NOT approved)

### 3.1 R1 — THE FOLD. BLOCK-VP1 closed.
The renderer draped everything on the bare DEM while the sled drove on DEM + snow depth — *"a
~0.77 m gap in bush, everywhere"* (`docs/snow_info_packet_winter_to_barrens.md:24`, severity TOP,
needs=Chad). The cubesphere now carries `ambient_depth_at x corridor_mask`.

| see-vs-drive gap | before | after |
|---|---|---|
| open ground p50 | **+0.777 m** | **+0.002 m** |
| mean abs | 0.802 m | 0.163 m |

- **The drive did not move**, by construction: `facet_radius_at` stays the TERRAIN facet and is what
  app injects as the sled's ground; the mesh and every drape use a new `drawn_radius_at`. One shared
  body (`facet_radius_impl`) so the two cannot fork in shape, only in whether the fold is present.
  ⚠ `[snowpack] hf_faceted_ground` is **true** in the shipped config — the sled reads the facet, so
  folding it there would double-count ambient into feel by ~0.7 m. The red team caught this.
- **Chad's barrens fence honoured by construction.** The black-rock shed and its slope gate live
  INSIDE `ambient_depth_at`, so sloped rock sheds automatically. Measured over the mapped disk:
  sloped rock (>=12 deg) takes **p50 0.000 m** of fold; at >=20 deg, mean 0.086 m. Re-check any time:
  `SEADS_BARREN_PROBE=1`.
- **Windy Lake — a bug HE found, now fixed.** Over water `ambient_depth_at` returns
  `ice_lift_m + ice_snow_m = 0.65 m` because the drawn ice is a separate mirror mesh above the
  FLATTENED lakebed. The fold lifted the lakebed 0.25 m ABOVE the ice. Gating on water alone was not
  enough (a folded land vertex lifts the facet out over the lake), so the gate is **dilated 80 m
  inland**. Punch-through 3,930 baseline -> 12,206 broken -> **4,578 fixed**; open water identical to
  baseline. Swept 80/140/200 m — saturates at 80.
- Load cost **+7.4 s** (27.1 -> 34.5 s); `fill_face`'s normal probes evaluate the fold too. The fix,
  if it bites, is a per-vertex table.
- A/B: `SEADS_NO_SNOWFOLD=1`. Mask width: `SEADS_MASK_M`. Water dilation: `SEADS_WATER_DILATE_M`.

### 3.2 R2 — PARTIAL. The shoulder is still wrong.
Corridor mask swept and set to **60 m (one mesh cell)** — the tightest ramp the mesh can carry
without vertices sampling it too coarsely and making roads lumpy.

| mask | float in the 0-30 m band |
|---|---|
| 120 m | +0.798 m |
| **60 m (shipped)** | **+0.636 m** |
| 30 m | +0.475 m |
| 15 m | +0.367 m |

**~0.6 m of float remains in the 0-30 m band — where his own tapes put him (p50 19-24 m off the
centreline).** Narrowing the mask cannot close it; the residual is the corridor's own bank and
feather, which a 59 m mesh cannot represent at any mask width. **The annulus strips are the rest of
R2 and are NOT built.** Beyond 120 m the gap is already 0.013 m.

Also fixed: moving the plow banks onto `drawn_radius_at` double-counted the snowpack (they add their
own depth channel). Reverted to the terrain facet; rule written down in `render/bank_mesh.h`.

### 3.2b R2 — THE FORK, MEASURED. **NEEDS CHAD'S RULING BEFORE BUILDING.**

The obvious shortcut (narrow the mask so the fold is only suppressed where the corridor actually
suppresses the driven depth) was tested and **is wrong**. The mask width is a TRADE, not an optimum:

| mask | shoulder float (20-40 m out) | deck punch-through |
|---|---|---|
| **60 m (shipped)** | mean **0.421 m** | **0.0 % under** |
| 20 m | mean 0.176 m | 29.1 % under |
| 12 m | mean 0.132 m | **45.5 % under** |

Narrowing closes the shoulder but lifts the drawn surface ABOVE the road deck (the facet interpolates
between vertices that carry fold, and a 59 m cell cannot dip into a 6 m road). Deck punch-through
means **skis under the road** on the surface he drives 37.7 % of the time. **The mesh cannot resolve
both.** 60 m is the deck-safe choice and is what ships.

⚠ **Probe caveat, so nobody misreads the table:** the 0-10 m band reads p50 +1.3 m at every mask, but
that band is drawn by the BANK STRIPS, not the mesh — `SEADS_VP1_GAP` compares the drive against
`drawn_radius_at` (the mesh) only. The 0-9 m number is not a defect, it is the probe looking at the
wrong surface there. The real R2 residual is the **10-80 m** band.

**Three candidate approaches, none built:**

1. **Extend the bank strips outward** (the Fable consult's recommendation) from 9 m to the mask
   width, drawn at the true driven height. Cost: the ambient field is smooth at 40 m, so stations can
   be coarse — ~2,213 km / 40 m x 2 sides x ~6 rings ~ **664k verts**, comparable to the shipped
   320k tree scatter. Largest code change, lowest risk to what already works.
2. **Full fold with no mask, and excavate the corridor out of the mesh.** Drawn == driven everywhere
   by construction, and the plow trench becomes real geometry rather than a mask artifact. Needs the
   ribbons to drape on the TERRAIN facet (they add no depth channel, so this is arguably where they
   belonged all along) and needs a hole in the mesh over the corridor. Note `render/ribbon_clip.h`
   already carries a T24 excavation clip, so the precedent exists. Cleanest result, biggest blast
   radius.
3. **Accept it.** 0.42 m of float in a band with no nearby reference to judge it against may simply
   not be visible. He has already driven R1 and did not report the shoulder — only "skis do go under
   sometimes", which is the 8-9 mm jitter beyond 120 m, a different thing entirely.

**Recommendation: drive it first and rule (3) in or out before paying for (1) or (2).** The
measurement says the defect is real; only his eye says whether it is visible.

### 3.3 Tracks — `world/tracks.{h,cpp}` + 9 test cases
Deformation composed into `drive_radius_at` beside `snowhill_add`, so the machine feels its own ruts
and there is no second representation. **Chad's spec for the real signature is NOT built yet** — two
ski cuts (`ski_width_m 0.135`, centres +/-0.4635 m from `stance_m 0.927`) plus a 0.38 m belt mark,
depth **proportional to local snow depth** (~0.33 m in bush against 0.71-0.73 m of pack, a few cm on
trail). Current `TrackField` cannot express it: stamps carry no heading, and `depress_m` is a
constant. Fable's fix is one law instead of two dials — `deform = k * depth_at_when_laid`, k ~ 0.45.

### 3.4 The rider patch — `render/snow_patch.{h,cpp}` — RETIRE IT
Built, then superseded. Its job is gone: ambient is in the mesh (R1) and the track belongs in a
ribbon along the path (R4). It is still wired and still drawing. **Delete it at R4.**

### 3.5 G1 + G2 — ROTOR GYROSCOPICS, both default-off
See `docs/sled_gyro_spec.md`. `k_gyro = 0.0` ships and is bit-identical (pinned). Armed, one second
of airborne flight at 0.6 rad/s of yaw develops **1.02 rad/s of roll**. Five `[gyro]` legs pass
including a value oracle at **0.0000** relative error and pitch immunity at **exactly** zero.
Rotor numbers are sourced, not invented — the kernel's belt is a real Camso 15x121 (its own
`3.0724 m` is Camso's `2.52 in x 48` exactly), and the kernel's 8000 rpm at 46 m/s matches real
gearing to 0.1 %.

---

### 3.6 G2 — THE REACTION WHEEL, default-off. **The bigger of the two.**

`k_gyro_react = 0.0` ships. Armed, a **throttle blip in the air pitches the nose up at +0.594 rad/s
= 34.1 deg/s** — the effect riders describe without naming it (*"too much throttle off the lip
causes your nose to come up"*), and exactly the "track as a reaction wheel" `sim/sled.h` deferred.

Applied as a telescoping momentum DELTA to the rate, never a torque — the `k_air_shift` precedent,
for the reason that comment gives. **The momentum ledger balances exactly:** `borrowed -0.092855`
against `-(L_end - L_start)/I_x = -0.092855`. A torque formulation would pass a peak-and-sign test
and fail this one.

⚠ **Two dials, deliberately independent.** `k_gyro` (precession) and `k_gyro_react` (reaction) scale
their own terms only; the readout `rotor_momentum_kgm2s` is the PHYSICAL truth and is gated by
neither. The first draft coupled them through `k_gyro` and silently zeroed G2 — caught by its own
knob-off leg. One knob judging two effects also breaks one-dial-at-a-time.

⚠ **Honest gap, recorded not smuggled:** the belt is KINEMATIC, so it spins up for free — the
reaction charges the chassis for momentum whose energy was never charged to the engine.

⚠ **The airborne BRAKE-TAP is structurally unreachable.** `sim/sled.cpp:1250` is
`rep_belt = max(dv*v_cmd, max(v_bf,0))`, so brake can never pull the belt below forward speed.
Blip-for-nose-up works; tap-for-nose-down does not. That is G4, gated on Chad asking, and it has the
largest blast radius because other consumers read that belt readout.

Eight `[gyro]` legs pass.

### 3.7 R3 — THE PER-VERTEX DEPTH ATTRIBUTE + DEPTH-KEYED EXPOSURE. **SHIPS ARMED (§0).**

⚠ **This section was written while the dial was default-off. It SHIPPED off, Chad drove it armed
on 2026-08-27, and it now ships ARMED (`depth_mix = 1.0f`) — §0 is the current truth.** Everything
below about the MECHANISM is still accurate; only the default moved. Do NOT "fix" this section by
restoring 0.0f — that would silently revert his ruling.

The mesh now carries the snowpack as a **shading channel**, not just as geometry, and the planet FS
can read coverage off the real field instead of off its own private mask. The crossfade is retained
so the A/B survives: `SEADS_SNOWDEPTH=0` is bit-identical to the pre-R3 render.

- **What it retires.** `render/planet.cpp:316`'s self-described *"PROVISIONAL SNOW STUB"* — a
  render-only `snowCover = uSeasonSnow x (1-water) x snowFlat` plus a **second** barren shed. Both
  predate the depth field. WINTER_LAW 3.2/6c.1 forbids exactly this (*"a layer must NOT compute a
  private exposure mask — or the rock shows where the sled is still sinking"*), and
  `ambient_depth_at` already **is** `f(slope, aspect, curvature, elevation, drainage)` **plus** the
  black-rock shed and its slope gate. The stub was reimplementing two of those, worse.
- **The carrier.** `FaceMesh::depths` → texcoord **.y** → `vSnowDepth`. `.x` stays 0 (it is vRim);
  it would be inert today because the planet pass sets `uVertexNormalMix = 0`, but "currently
  multiplied by zero" is not a contract. **Empty `depths` means NO CHANNEL, never "zero snow"** —
  the upload skips the buffer and the draw clamps the mix to 0, so `SEADS_NO_SNOWFOLD` and the Earth
  cannot paint the planet bare.
- **One body, no fork.** New `SnowpackField::draw_sample_at` returns `{ambient_m, fold_m}`;
  `draw_fold_at` is now a thin call onto it. The mesh takes **both from one call per vertex**, so the
  surface the eye is lit by and the surface it sits on cannot disagree except through the mask.
- ⚠ **The shading channel is the UNMASKED ambient, deliberately.** The 60 m corridor mask answers a
  MESH-RESOLUTION constraint that per-fragment colour does not have. Reusing `draw_fold_at`'s early
  exits would hand the shader a zero across the whole band and **paint a 2,213 km bare-ground stripe
  down every road and trail.** Pinned by a test, and the mutant that reintroduces the short-circuit
  is verified to kill it.
- **The barren shed is faded by the same dial** (`1 - (1-mix) * uBarrenSnowShed * ...`). Arming one
  without the other sheds the black rock **twice**. FACE-DARK is untouched — it is colour, not
  exposure, and it is Chad's ruling.

**THE FENCE, MEASURED (`SEADS_R3_PROBE=1`, 459,717 land samples) — mean coverage:**

| `full_depth` | open ground | sloped rock ≥12° | steep rock ≥20° |
|---|---|---|---|
| *legacy stub* | 0.992 | **0.230** | **0.094** |
| 0.20 m | 0.988 | 0.373 | 0.178 |
| 0.35 m | 0.986 | 0.307 | 0.156 |
| 0.50 m | 0.982 | 0.250 | 0.133 |
| **0.70 m (shipped)** | 0.946 | **0.199** | 0.107 |
| 0.90 m | 0.859 | 0.163 | 0.088 |

**0.70 is the shallowest value that does not REGRESS his fence** — sloped black rock comes out at
0.199, *blacker* than the 0.230 he flew and approved. Below it the mechanism puts MORE snow on the
faces he ruled three times should be the blackest thing on the planet. The open-ground cost
(0.992 → 0.946) is **not** lost snow in open country: it is scoured crests finally reading THIN,
which is the entire point of keying exposure to depth. **It is still a trade and only his eye
settles it** — sweep with `SEADS_R3_FULLDEPTH`.

- **Load cost: ZERO.** Hoisting ambient ahead of the mask was A/B'd on the real DEM — eager 40.9 s
  vs short-circuiting 41.7 s, inside run-to-run noise, because the early exits fire on a negligible
  fraction of vertices. ⚠ Separately: the fold's **total** load cost measures **+15.1 s** here
  (24.4 → 39.5 s), not the **+7.4 s** §3.1 records. That gap is NOT R3 (the lazy arm measures the
  same), so §3.1's figure was taken under different conditions — **re-measure before quoting it.**
- **The drive did not move.** 173,118-sample census with the fold on vs off is **byte-for-byte
  identical** on this tree, re-verified after R3.
- A/B: `SEADS_SNOWDEPTH=0` turns it OFF, `=0.5` sweeps it, unset = the ship value, which is
  **1.0 (ARMED) since Chad's 2026-08-27 ruling — see §0.**

⚠ **TWO BUGS THE GREEN BUILD HID, both found by asking "who else emits a vertex / reads this
uniform?" rather than by a failing test.** Both are invisible while the dial is off, which is exactly
why they had to be hunted rather than waited for:

1. **The T25b cut trim emits NEW vertices** for partially-swallowed facets (positions + normals
   pushed, `render/sphere_param.cpp`). The depth channel did not grow with them, so
   `depths.size() != positions.size()/3`, `upload_face` drops a desynced channel WHOLE, and **every
   face carrying a tunnel mouth would have rendered as bare ground** once armed. Fixed by sampling
   the leaf's own direction; pinned by a test, mutation-verified (289 vs 451 verts).
2. **The rider snow patch writes `texcoord.y = 0`** (`render/snow_patch.cpp:173`) and shares the
   planet program — so armed, it would paint a **bare-ground rectangle directly under the rider**,
   the most-looked-at pixel in the game. `draw_snow_patch` now forces `uSnowDepthMix = 0` for its own
   draw and restores the planet's value after (the lake mirror pass shares the program). Forced
   rather than plumbed **because §3.4 retires this layer at R4** — a doomed layer does not get a new
   field dependency.

⚠ **Incidental fix, worth knowing:** `render/draw.cpp` built `SnowParams` with a **13-element
positional brace init**. Inserting two members shifted every field after them by one slot; it was
caught only because the types happened to disagree (`vec3` into `float`). A same-typed insertion
would have compiled and quietly fed `snow_sparkle` into `barren_face_dark`. **Now assigned by name.**

⚠ **The tree is NOT clang-format clean** — files untouched this session (`render/bank_mesh.cpp` 79
lines, `render/ribbons.cpp` 16) also reformat. R3 was left unformatted rather than bury the diff in
pre-existing churn; run the repo's format command as its own pass, never mixed into a rung.

## 4. ⭐ WHAT CHAD MUST RULE — DRIVE CARDS

```
cd D:\seads_sandboxes\sandbox_snow
cmake --build build-play --target seads
.\build-play\seads.exe
```
**J** mount · **Shift** throttle · **]** / **[** time of day · **H** HUD · **C** recentre ·
mouse = lean · A/D = steer · Q/E = lean trim

1. **R1, the fold.** Does the ground read right where you ride; are the roads still roads; are the
   black rocks still black. A/B: `set SEADS_NO_SNOWFOLD=1` (blank it to re-enable).
   *He has driven this once: roads OK, rock OK, cones visible, "skis do go under sometimes" — that
   is the 8-9 mm facet-interpolation jitter beyond 120 m, not the shoulder float.*
2. **G1, precession.** `k_gyro` is 0.0 in `sim/sled.h`. Set it to 1.0, rebuild, carve at speed and
   jump with yaw on. Does the machine steer into the lean?
3. **G2, the reaction wheel.** `k_gyro_react` is 0.0. Set it to 1.0 — **judge it SEPARATELY from
   G1**, which is why they are two dials. Blip the throttle in the air; the nose should come up,
   ~34 deg/s. The brake tap will do nothing — known, see 3.6, not a bug.
   **Both are KERNEL dials: one at a time, his stick, both stay 0 until he says otherwise.**
4. **R3, the depth-keyed exposure. ✅ RULED IN 2026-08-27 — SHIPS ARMED, no env var needed.**
   (`set SEADS_SNOWDEPTH=0` to drive the OFF arm of the A/B.)
   **This is a LOOK ruling, judged standing still as much as riding:** does the snow read as
   *shaded and felt* rather than a flat white paint — do hollows read deep and crests read scoured,
   and **are your black rocks still black on the slopes?** The measurement says the sloped faces come
   out blacker than what you approved (0.199 vs 0.230), but the measurement is not your eye.
   ★ **SWEEP THE SATURATION DEPTH — THIS IS THE EXPERIMENT ON "NO INTERMEDIATE" (§0.3 cause 2).**
   `set SEADS_R3_FULLDEPTH=1.2` then relaunch; no rebuild. The ship value 0.70 m saturates BELOW the
   map's median depth (census p50 **0.77 m**), so today more than half of all land is clipped at full
   coverage and 0.77 m shades identically to 1.75 m. **Raising it is also the fence-safe direction**
   — sloped rock goes 0.199 (at 0.70) -> 0.163 (at 0.90), i.e. BLACKER. What it costs is uniform
   white on open ground (0.946 -> 0.859), which is the trade only his eye settles. Try 0.9, 1.2, 1.5.
   The console prints the override on start, so a stale shell variable cannot silently explain away
   a later verdict. Values <= 0 are refused and fall back to the ship value.
   ⚠ It was **probe-only until 2026-08-27** — an earlier version of this card told him to sweep it
   in-game when the live draw had no override at all. `SEADS_R3_PROBE=1` prints the offline table.
5. **The roost trade** (§5, Q1 of `docs/roost_consult_packet.md`): the spec says S5's roost must
   scale off `roost_flux` *"and nothing else"*, which read literally forbids a volumetric roost. The
   kernel itself does not obey that reading. **Needs his amendment before R1 of roost is built.**

---

## 5. NEXT, IN ORDER

★ **THE ORDER CHANGED ON CHAD'S 2026-08-27 DRIVE (§0). R4 IS NOW THE HEAD OF THE QUEUE** — he
drove armed exposure and the thing he asked for ("skis going into snow", an *intermediate*) is
provably not reachable from the ambient field. Do not spend another rung on exposure.

**Snow:** **R4 — the track signature + RETIRE THE RIDER PATCH.** This now answers three of his
reports at once: the missing drawn rut, the missing intermediate (via
`deform = k * depth_at_when_laid`, §0.1 cause 2), and the patch's exclusion from depth shading
(§0.1 cause 3, which only deleting the patch fixes). -> R5 micro-relief/BRDF (**he said yes**;
put the drawn-vs-driven trade to him explicitly first) — this is what makes UNTOUCHED snow read
as snow (§0.1 cause 1). -> R2 annulus strips (the 0.6 m shoulder float, **still gated on his
§3.2b ruling**; he has now driven twice without reporting the shoulder, which trends toward
option 3 "accept it" but is NOT yet his word). -> R3 leftover: **M2 softened under snow** (the
normal map is baked from bare DEM, so snow still shades with rock's micro-roughness; `vSnowDepth`
is available to the FS, which is exactly what that needs).

✅ **DISCHARGED — do not re-run:** "re-probe the shatter cones after arming exposure" was owed
here. Chad's armed drive found them (§0). His eye closed it.

⚠ **R3's remaining two items are NOT cosmetic leftovers.** `vSnowDepth` is now available to the FS,
which is exactly what a depth-aware M2 softening needs — do that next, before R4, while the channel
is fresh. And the cone re-probe is a FENCE check, the same class as `SEADS_BARREN_PROBE`: arming
exposure changes what covers the cones.

**Roost** (`docs/roost_consult_packet.md`, Fable-consulted): R1 spray tracking the physics -> R2
volumetric variability + bury death -> R3 material honesty -> R4 the bog made legible -> R5 optional.
⚠ **P0: do not reuse `combat/kill.h`'s `FxPool` drag** — 10 %/s is effectively vacuum and would throw
snow 94 m instead of 30-50 ft. Derived drag `tau ~ 0.5 s` from Chad's own 30-50 ft.
⚠ **P0: `DrawInfo` lacks `belt_speed_ms` / `track_slip` / `engine_rpm`** — plumb them as pure reads
or the renderer will recompute `v_track = throttle x 46` and be wrong on closed throttle.

**Gyro:** G1 + G2 BUILT and pinned, both default-off. Remaining: G3 crank/clutch momentum off
`engine_rpm` (⚠ a CVT pins engine rpm, so it is roughly CONSTANT with road speed — do NOT refer it
by ratio squared) → G4 the airborne brake-tap (largest blast radius; gated on him asking).

---

## 6. INSTRUMENTS BUILT THIS SESSION (all re-runnable)

| command | what it measures |
|---|---|
| `SEADS_SNOWDUMP=<f>.tsv` | terrain/depth/driven census on the real DEM + linework |
| `SEADS_VP1_GAP=1` | the see-vs-drive gap, bucketed by distance from a corridor |
| `SEADS_BARREN_PROBE=1` | fold added over mapped black rock, by slope — **Chad's fence** |
| `SEADS_R3_PROBE=1` | R3 exposure: legacy stub vs the depth field, bucketed by the fence slope |
| `SEADS_R3_SHEDKEEP=<0..1>` | the black-rock fence dial in the R3 PROBE's DEPTH column (ship 1.0) |
| `SEADS_R3_FULLDEPTH=<m>` | ✅ **LIVE AT THE DRAW** (and pins the offline probe's sweep). Saturation depth in metres; `<= 0` refused |
| `SEADS_SNOWDEPTH=<0..1>` | the R3 A/B at the DRAW, no rebuild. **Now DISARMS (`=0`); armed is the ship default** |
| `SEADS_ICE_PROBE=1` | mirror-vs-ground clearance; catches lake punch-through |
| `SEADS_SNOWBENCH=96` | `depth_at` cost (~1.06 us/call) + field-vs-facet |
| `SEADS_TRACK_DEMO=1` | lays a deterministic star of track at the sled |
| `seads_sled_probe roost` | roost flux vs depth x throttle x duration, + bog legibility |
| `seads_sled_probe gyro 1.0` | the G1 A/B, k_gyro 0 vs 1 |

---

## 7. THE MEASURED FACTS WORTH NOT RE-DERIVING

- Snow depth field changes **2.6 cm per 2 m** — a **0.74 deg** slope. `curv_probe_m = 40.0` and
  `world/snowpack.h:23` calls the function **"THE HOMOGENIZER"**. It has no snow-scale detail, so
  drawing it faithfully cannot make snow look like snow. Metre-scale relief must come from
  deformation, not from the ambient field.
- DEM relief in the ridden band: p50 **4.37 m**. Snow modulation p50 **0.682 m** (15.6 % of it).
- `depth_at` ~**1.06 us/call**; `ambient_depth_at` ~0.77 us. The cost is the finite-difference
  slope/curvature sampling — it does not optimise away. +25 % once ~4,000 track stamps are laid.
- Ground clipping is **NOT** the blocker: 2.2 % of open-snow pixels clipped at peak day, ~70 DN of
  spread retained. An earlier plan assumed otherwise.
- FIELD minus FACET at the shipped `subdiv 200 / tiles 2`: p50 **+0.091 m**, p99 +0.274, min −0.086.
  The old in-repo "p99 ~6 m floating-road" figure was measured **untiled** and no longer applies.
- `roost_flux` IS alive and richly variable (bush WOT 6 s: **0.373**; peak 0.419 at 0.60 m depth) —
  an in-repo note recording 0.000 was one pinned bog case.
- In the bog, **throttle is not disconnected**: belt 11.5 -> 46.0 m/s, slip 0.46 -> 0.87, rpm
  3,275 -> 8,000 across throttle. Nothing consumes those channels — that is the defect, not the
  kernel.

---

## 8. CORRECTIONS MADE THIS SESSION (so they are not re-made)

1. Claimed the near-field ground was a "featureless grey field, spread 12 DN" — that measurement
   used a 0.8 m chase camera, which puts the eye INSIDE the machine. Much of what was measured was
   the sled's own bodywork.
2. Claimed the bog leaves throttle "disconnected from every observable". False — see §7.
3. Spec said momentum refers by ratio squared through a CVT. It does not; inertia does. Would have
   made the gyro vanish at low speed where the crank dominates.
4. Spec (and the SHIPPED kernel comment at `sim/sled.h`) cited a test that does not exist. Both
   corrected. **A comment that names a test is a claim about the tree.**
5. A `[sled]` suite result was accepted from a **stale binary** (silent link failure). Discarded and
   re-run on a proven-fresh relink. `rm` the exe and confirm the relink — the repo's own lesson.
