# HANDOFF — SK-1d · THE CARVE BAND IS DEAD  ⛔ SUPERSEDED

> **⛔ THIS IS NOT THE CURRENT HANDOFF. Read `docs/snow_build_handoff.md`.**
> The task moved to THE SNOW BUILD on 2026-08-26, and the terrain/jump consult this document defers to was run,
> built, driven and **REVERTED** (`590968241`). Nothing in §6 below should be started.
> What is still live here: **§3 (SK-1d, the shipped controls and HUD)** and **§2 (M12, there is no carve band)**.
> Everything else is history.

# HANDOFF — SK-1d (COMMITTED + PUSHED) · THE CARVE BAND IS DEAD · THE DEFERRED TERRAIN CONSULT

> **LAUNCH LINE (paste this into a fresh session):**
> Read `docs/steer_lean_hud_handoff.md`. **SK-1d is COMMITTED (`c2b7bfeeb`) AND PUSHED, and is AWAITING CHAD'S
> DRIVE** — §3 is what shipped, §4 is how to drive it. The tree is CLEAN.
> **★ READ §2 BEFORE YOU PLAN OR REPORT ANYTHING: there is no carve band, and the section that claimed one was
> falsified by measurement (M12).** Do not re-propose anything in §5. §6 is Chad's DEFERRED terrain/jump consult,
> in his words, not yet started — it is the best next move if he re-raises it. §7 is his to rule, not an agent's
> to build.

## FRESH SESSION START HERE

| | |
|---|---|
| **Branch** | `sandbox/snow` — tracking `origin/sandbox/snow` |
| **Worktree** | `D:\seads_sandboxes\sandbox_snow` |
| **Base** | `main @ 7bae38718` (`sandbox/gi4-ride` and `sandbox/world-sudbury` are BOTH ancestors) |
| **HEAD** | **`c2b7bfeeb`** — SK-1d: decouple steer from lean, paste the HUD to the sweater, M12 |
| **Prior** | `27532eded` — SK-1a blend + GI4 item 2, both shipped OFF, gate-verified |
| **Dirty** | **NOTHING. Tree clean.** |
| **Pushed** | **YES** (2026-08-25) |
| **Gate** | `[sled]` = the **4 known-red GI4 baseline legs**, no new failures. Targeted: `hud` / `rider` / `sled_model` / `instructor_tick` / `aim_frame` = **29/29** |
| **Measurements** | `docs/snowform_measurements.md` (M1..**M12**) — every number with its instrument |

⚠ **The base is RED before you start.** Four sled legs fail on `main` by prior design, escalated and unruled
(`docs/snowform_measurements.md` §M3). Do not "fix" them by tuning. Do not read a 4-failure gate as your bug.

⚠ **NO ctest runs `seads.exe`.** The input mapping and both HUD presentations are structurally invisible to the
green gate. **Chad's drive IS the gate** — a green suite proves nothing about any of §3.

---

## §1 CHAD'S ASKS — VERBATIM

**2026-08-25, the steering + HUD ask (BUILT as SK-1c, then REVISED by him — see §3):**

> *"why nott link the steer to the lean (if that is supposed to be how you r tun so it auto leans tied to the left
> right of the mouse lean dot. And put the dot on the sudburians back, make it a dynamic moving grid and make it
> stylized and easy to read, Able to see the handlebars through sudburians back like like lidar and you can see
> turing deflection visually by making it slag orange and neon emmitting light particles we can pick up through the
> sudburian somehow (not literally but visually like HUD style)."*

**2026-08-25, refining it and DEFERRING the consult:**

> *"Lets run an overnight consult that you can orchestrate and you can then analyse it. COnsult packet is all the
> context yuo aquired in this session and how to establish a plan for quality added terrain that is dynamic and
> visual and fun. And how no make some places have a big jump that if hit straight on gives quite a bit of air.
> Sudburian is tough and can handle the fall. .. Ooohh but defer that I want to make that seering option by mouse
> real first. Grid on sudburians back moving with him, distinct and easy to see and so are the handlebars, either
> across the bottom of the screen awesomeness or through thte sudburian xray style visibility."*

**2026-08-25, AFTER driving the coupled build — THE RULINGS THAT DEFINE SK-1d. These OVERRIDE the first ask.**

> *"okay I dont like the steering via mouse coupled with lean they have to be two independent things, part of what
> is so fun on the road is driving by lean. Do keep the weight transfer graph on the sudburians body but make it
> pasted to his white /grey sweater. Currently it is on his head. make it slightly bigger to fit the whole back"*

> *"steering is too slow the handlebars arent necessary anymore and please make the grid blue and the ball slag
> orange, glowing as the thememd color code graphified into this codebase"*

**The question that started the whole thread:**

> *"yea I felt I could ride onto the shallow side at pretty fast a speed. I do find that I am not enough able to hold
> a steady turn as I am using wsad and it wont carve so well is this something perhaps adding this snow could ammend
> or is it better to go straight to kernel"*

**Standing rulings that bind this work:**

> *"the planing lift is important for traversing atop the snow so that is important it stays, and at slower speeds I
> think it is good too because [I'd] like to jump the snowbanks, but at high speeds, getting pulled in and flipping
> out like 15x is not desirable so yes take care of that for this sandbox"*

> *"I just noticed that the sled is limited by slope of mountains, especially around onaping, it adds a cool dynamic
> to the game, the fact that gravity would pull it down or too steep of terrain it can climb on an angle, dont worry
> about that limit the rule of no getting stuck to the regular snow that isnt in a creek or whatnot, even then lets
> allow 2m/s egress."*

> *"I like the dynamics of flips and all that where I do one or two or I barrel roll and especially off the formation
> hill at st charles elementary. What im talking about is going down the road, touching the ski to the bank just a
> little and getting pulled in and spinning like 10 to 20 x, its a bit glitchy feeling in that respect. That is all
> and place s where the banks are cut and the bank doubles up, I get launched in a crazy way that is a bit too much."*

> *"a creek wont slow you down to a mx of 2m/s but 2m/s will be granted, even if you stop on a creek. But traversing
> a creek dosent do much at speed becasue at speed your ski lift you above that deeper resistance category"*

---

## §2 ★★★ THERE IS NO CARVE BAND — READ THIS BEFORE PLANNING OR REPORTING

The previous handoff's §3 reported a non-rolling turn at steer 0.11 (R 98.7 m, roll 34.6°), a band "1% of the axis
wide", and every plan in this thread was shaped around reaching or widening it. **It was an observation-window
artifact.** Measured, `docs/snowform_measurements.md` **M12** (v0=16, WOT, lean 0 — the original conditions, one
instrument, window length swept):

| secs | R [m] | roll_max | d_roll/s | verdict |
|---|---|---|---|---|
| 3 | 21 997 | 2.4° | +0.0 | settled |
| **4** | **98.7** | **34.6°** | **+32.6** | **STILL TIPPING** |
| 5 | 18.8 | 123.8° | +81.6 | ROLLED |
| 6–16 | — | 149.0° | — | ROLLED |

**The 4-second row reproduces the old numbers exactly — and is climbing at 32.6°/s when the window closes.** It was
the first second of a rollover, not a carve. `max_roll` alone cannot tell those apart, which is why the sweep now
also reports `roll_end` and `d_roll/s`.

**Speed was ruled out as the confound.** WOT for 12 s accelerates v0=16 to ~45 m/s, so a late roll could have been
speed rather than steer. Two controls:

- **Speed-held** (P controller on `v0`), 12 s, v0 = 12 and 16: every steer from **0.02 to 0.26** rolls past 140°.
- **Zero steer**, 12 s: `roll_max 1.4°, settled` speed-held; `2.4°, settled` at WOT reaching **44.9 m/s**.

The machine is rock-stable at 45 m/s with no steer. **The roll is caused by STEER, at any nonzero value.**

This **confirms M11.2 as originally written** — *"there is no carveable steer angle on Road at ANY speed tested"* —
and attributes it to the same known-red fence (M11.3: peak lateral **0.953 g** vs tip onset **0.363 g**, ratio
**2.623** where the leg requires ≤ 0.90). Road gets no relief: GI4's carve fix (`plane_lat_gain`) is structurally
inert there (`rho_eff = 0`), and snow cannot act on a plowed road at all (`road_bare_m = 0.02`, not sinkable).

**Consequences you must respect:**
- **A band marker was planned and deliberately NOT built.** There is nothing to mark; painting 0.10–0.11 on the HUD
  would have shipped a lie into the instrument.
- **Do not describe any input work as fixing the carve.** Only §7.1 can, and that is Chad's ruling.
- **Any future sweep must state its window.** A 4-second WOT number on this machine is not a steady state.

---

## §3 SK-1d — WHAT SHIPPED IN `c2b7bfeeb`

### 3a. Controls — steer and lean are FULLY INDEPENDENT (`app/main.cpp`)
Chad rejected SK-1c's lean auto-follow on feel. `kSteerLeanFollow` is **deleted**, not zeroed.

| input | drives | notes |
|---|---|---|
| **mouse X** | `sled_lean_lat` | back to the original **300 px** full scale — the axis he drove on drive 1 |
| **mouse Y** | `sled_lean_fwd` | untouched |
| **A / D** | `sled_steer_cmd` | analog accumulator, **2.0 /s**, **self-centres at 3.0 /s on release** |
| **Q / E** | lean trim | 2.0 /s |
| **C** | recentre | zeroes steer + both lean axes |

- **The steer rate is matched to the kernel on purpose.** 0.8 /s took 1.25 s to full lock — *slower than
  `steer_rate_per_s = 2.0`* — so the COMMAND was the bottleneck, not the machine. A command faster than the slew
  would only queue up and lie to the HUD.
- **The self-centre is load-bearing.** Binary A/D returned to straight for free; an accumulator does not, and
  without it every corner has to be counter-tapped out. Release is deliberately faster than the push.
- ⚠ **App-local constants** (`px_full 300`, `steer_key_rate 2.0`, the 3.0 return). If he wants them tuned by feel
  they belong in `config/scenario.toml [sled_comfort]` — *"tune data, not code"*.

### 3b. The Sudburian-back HUD (`render/draw.cpp`, `render/draw.h`, `render/sled_model.*`)
`FrameInfo::sled_hud_xray` (default true), **H** toggles. Both presentations now read **weight transfer only**.

- **Pasted to the SWEATER, not the chassis.** It was on his helmet because the old anchor read
  `sp + by*(cg_h + 0.34)` and `sp` is ALREADY the CG — **`cg_h` counted twice**, ~1.47 m up. It now rides the rig's
  own posed torso frame, published as **`render::sled_model_rider_back()`** from the SAME pelvis/neck/back-normal
  the scarf's §3b torso plane already derives — a READ of that frame, never a second derivation — sized to the
  pelvis→neck run and stood off the **measured** suit surface. `valid` is false on any frame the hero rig did not
  pose, and the HUD falls back to the body frame (with the double-count removed).
- **Handlebars DELETED from both presentations.** Deleted, not gated behind a flag — a dead branch is a fork
  waiting to happen. H stays an honest A/B of ONE instrument.
- **Palette reads `render::team_colors()`** — this repo's one cross-layer hue table (map + world + liveries) — not
  typed hex: grid = `ally` (kComplementBlue), weight ball = `enemy` (kSlagOrange), an **exact complement pair by
  construction**. The glow is stacked shells under the additive blend already in force: real falloff, no second
  pass, no sprite, no shader. **Retint the table and this HUD follows.** Do not add a second palette (H1).
- The grid **shears with rider weight** — the displacement is a shape, not a pip in a box.

### 3c. Audit defects fixed (all app/render side)
1. **The motes were frozen and the AT-9 claim was inverted.** They animated on `info.sled_ticks` = ticks consumed
   THIS FRAME (a constant 2 at steady 60/120), so what little motion existed was **frame-rate dependent** — the
   opposite of what the code claimed. Now keyed on the cumulative `sled_tick_no`. ⚠ The scarf rig uses the same
   field legitimately because it *accumulates* it internally — do not "fix" the scarf to match.
2. **`rlDisableDepthTest()` ran BEFORE `BeginBlendMode`**, which flushes the pending rlgl batch — earlier geometry
   got flushed depth-less. Order swapped; the exit already unwound correctly.
3. **The drive-mode seed reseeded the KERNEL but not the app-side input accumulators**, so a dismount at half-lock
   remounted already steering. Harmless while steer was binary; steer now has yaw authority.

### 3d. Probe (`tools/sled_probe.cpp`)
`steersweep <v0> [lo] [step] [steer_max] [lean_follow] [secs] [throttle]` — the last three are new.
`lean_follow` drives `in.lean_lat = follow * steer` so the sweep measures the machine we SHIP (pass 0 for the old
steer-only sweep). **`throttle < 0` engages a P speed-hold at `v0`** — the only setting that attributes a roll to
steer alone. Output gained `roll_end` and `d_roll/s`, and it stops at the ±1 rail instead of printing
kernel-clamped duplicates as distinct rows. Also: `bankgraze <v0> [half_w] [class_blend_m]`, `g4_traction`.

### 3e. Verification
`build-play` + Debug both build clean · `--smoke 3 <png>` clean · `hud|rider|sled_model|instructor_tick|aim_frame`
**29/29** · `[sled]` shows the four known-red baseline legs and **no new failures**.
**The kernel is UNTOUCHED** — `git diff 27532eded..HEAD -- sim control test config` is empty.
⚠ `generated/graph/` was **deliberately NOT regenerated** despite new symbols in `render/sled_model.h`: it is stale
on the base, and regenerating here would fold main-side `world/buildings.*` drift into this thread. It belongs in
its own commit on `main`.

---

## §4 NEXT ACTIONS

1. **★ CHAD DRIVES SK-1d.** `.\build-play\seads.exe` → **J** drive, **H** swap HUD, **C** recentre.
   Mouse = lean, A/D = steer, Q/E = lean trim.
   **What to judge:** is 2.0 /s steer right now, or still slow · does driving by lean feel like it did before
   SK-1c took the mouse · does the blue grid read on his back at speed against a white world · is the slag ball
   legible as the weight read · x-ray vs bottom bar · is the panel the right size for his back.
2. **Then §6, the terrain/jump consult** — the best next move if he re-raises it, and the one that delivers the
   other half of what he wants (air). §7.1 cannot be built by an agent.
3. Anything he wants tuned by feel → move the §3a constants to `config/scenario.toml [sled_comfort]` first.

---

## §5 CLOSED — DO NOT RE-PROPOSE

- **Lowering `steer_max_rad`** to spread the axis — measured negative result: at 0.05 rad the FULL axis gives
  R 1160 m and 2.4° of roll. A steer angle too small to initiate the tip does not turn at all.
- **`plane_lat_load_frac`** — built properly, FALSIFIED by its own measurement (tip onset 0.318 → 0.334 g, ~5% of
  the 1.035 g needed).
- **Anything that only re-shapes a lateral force** — *"§8.3 has four measurements saying that class does not reach
  this."*
- **Moving `stance_m` / `cg_height_m`** — sized at 1.75× against a 3.3× hole, and both are MEASURED off the real Indy.
- **`traction_mu` as a fix for the bank-strike** — inert on Road by construction.
- **A HUD band marker for the carve band** — §2: there is no band.
- **Re-coupling lean to steer** — Chad ruled it out on feel after driving the coupled build.
- **A second palette, or typed hex in the sled HUD** — §3b: `render::team_colors()` is the source.

---

## §6 DEFERRED — THE OVERNIGHT TERRAIN CONSULT (NOT STARTED, his words in §1)

He asked for an orchestrated overnight consult, then deferred it in the same message. Two topics:
**(a) a plan for quality added terrain that is "dynamic and visual and fun"**, and **(b) "some places have a big
jump that if hit straight on gives quite a bit of air. Sudburian is tough and can handle the fall."**

Feed the packet from `docs/snowform_measurements.md` and the snowform plan
(`C:\Users\Chad\.claude\plans\declarative-jumping-conway.md`, revision 3). Constraints it must respect:

- **Snow is DEPTH, never DEM height** (INV-2); `projection.lock` LOCKED, no re-bake.
- Two prior revisions of the snowform plan were **killed by red teams** — the post-mortems are in the plan's "What
  earlier revisions got wrong" and must not be re-proposed (global skin, per-site sibling meshes with an ambient rim,
  a `d_escape` scalar).
- **Visibility is the go/no-go and is UNMEASURED (M2 is BLOCKED):** snow clips to white above ~44.8° sun elevation,
  the sun sits high (pump latitude **8.18°**), and `probe_day` is **inert** — *"inert now, logged honestly"*. Wiring
  `probe_day` into `cel_time_offset` is the small honest fix.
- **(b) is well-aligned with what already exists:** Chad likes the St Charles snowhill send, `snowhill_add` is the
  shipped authored-form mechanism, and its mesh is generated OFFLINE from the same function. A jump is an SF1
  sibling, not a new system. ⚠ But the **75° attitude cone is direction-agnostic (pitch counts) and zeroes throttle**
  — the shipped hill already loops out below ~13 m/s entry, and Chad wants his flips. Any jump work must not cost
  him those.

---

## §6b COMMANDS

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/seads_sandboxes/winter-gi/build/_deps/raylib-src \
  -DFETCHCONTENT_SOURCE_DIR_GLM=D:/seads_sandboxes/winter-gi/build/_deps/glm-src \
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=D:/seads_sandboxes/winter-gi/build/_deps/catch2-src \
  -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=D:/seads_sandboxes/winter-gi/build/_deps/tomlplusplus-src
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure     # ~33 min; expect the 4 known-red sled legs
```
**To drive — NEVER `build\seads.exe`** (`-O0`, 45× on the hot path):
```
cmake --build build-play --target seads    # tests refuse to compile outside Debug, by design
.\build-play\seads.exe
```
⚠ **`--smoke` needs a frame count**: `seads.exe --smoke 3 shot.png`. Bare `--smoke` runs open-ended and will look
like a hang — and it holds a lock on the exe, so the next link fails with `cannot open output file ... Permission
denied` (CLAUDE.md's stale-binary trap wearing a different hat: `rm` the exe and confirm the relink).

A/B the bank-strike blend with no rebuild: `config/world.toml` → `[snowpack] class_blend_m` (0.0 = step, 1.0 = fix).

---

## §7 ⭐ OPEN — AWAITING CHAD, NOT AN AGENT

1. **The grip/tip inversion.** Blocks the carve — and §2 says it blocks it at EVERY steer angle, not just most of
   them. Nobody may move measured machine geometry to close it. Every buildable lever in this space is now in §5,
   so the one legal agent contribution is a decision memo: the traced mechanism (M3.1b's load-scaled-restoring vs
   unscaled-destabilising loop) and the geometry-sizing table (`b971d14a9`: 1.75× available vs 3.3× needed).
2. **SK-1d's verdict** — and its constants, if he wants them tuned or moved to config (§3a).
3. **`class_blend_m`'s value.** Numbers say 1.0; a feel dial is ruled on the stick. Named cost: the blend leaks
   inward, so the metre of road nearest the shoulder is looser, and he drives **37.7 % Road**.
4. **`traction_mu`** — real fix for the Bush planing attractor, free at ≥ 3.0, but NOT his reported bug. Ships 0.0.
5. **The 4 red baseline legs** — pre-existing, escalated, unruled.
6. **§6's consult** — deferred by him, not cancelled.
7. **`generated/graph/graph.json` is stale on the base** — belongs in its own commit on `main` (§3e).

**Nothing in §3 fixes the carve. §2 is why.**
