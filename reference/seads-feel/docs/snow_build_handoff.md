# HANDOFF — THE SNOW BUILD (the task) · the jump build is REVERTED

> **LAUNCH LINE (paste this into a fresh session):**
> Read `docs/snow_build_handoff.md`. **The task is THE SNOW BUILD.** The SF3 jump build was driven, rejected and
> **fully reverted** (§1) — do not resurrect any of it, and read §1 before proposing anything, because the way it
> failed is the trap this thread keeps falling into. **§3 is the ladder.** **§4 is the one measured fact that
> should shape the first rung, and it is not in the plan.** The tree is CLEAN and PUSHED.

## FRESH SESSION START HERE

| | |
|---|---|
| **Branch** | `sandbox/snow` — tracking `origin/sandbox/snow` |
| **Worktree** | `D:\seads_sandboxes\sandbox_snow` |
| **HEAD** | `590968241` — the SF3 revert |
| **Dirty** | **NOTHING. Tree clean, pushed.** |
| **Gate** | `snowhill\|map\|hud\|props\|tree` **49/49**. The full `[sled]` sweep still shows the **4 known-red GI4 baseline legs** (`docs/snowform_measurements.md` §M3) and nothing else |
| **Measurements** | `docs/snowform_measurements.md` — M1..**M12** (M13/M14 went with the revert) |
| **Plan** | `C:\Users\Chad\.claude\plans\declarative-jumping-conway.md` revision 3 — its RENDER LANE is §3 below |
| **Consult** | `docs/snowform_terrain_jump_consult.md` — kept as a record; its terrain half and its shader finding still apply, its jump half is history |

⚠ **NO ctest runs `seads.exe`.** Chad's drive IS the gate. A green suite proves nothing about anything he looks at.

---

## §1 ⛔ THE JUMP BUILD IS REVERTED — AND HOW IT FAILED IS THE LESSON

Chad, 2026-08-26, after driving it, verbatim:

> *"no its not on the map, just scrap this whole jump idea, revert any of the work for this, I didnt even ask for a
> snow hill initially but a jump would have been nice but we are getting off task... it was on a hill, half buried,
> grey in color and flashing due to placement or mesh problems, not worth the trouble shoot."*

Reverted in `590968241` (b503a19f3, 5ee4f208c, 7aeded072, 06c484ecb, f828ad479). The tree is byte-identical to
`d339001b1`. **Do not re-land any of it** — not the site array, not the jump probe mode, not the map marker.

**The probe said the jump was clean: 2.26 s of air, 36.8° of airborne tilt, stable across entry 10–17 m/s. He drove
it and got a half-buried grey mound on a hillside, flashing.** Every word of that is a defect the measurements could
not see, and the reasons are the trap:

1. **Half buried, on a hill.** Every shape number was taken on the **flat analytic fixture** (`flat_field()`,
   ambient 0.85, no LineNetwork, no cold). The SITE was chosen from real DEM; the SHAPE was **never once driven on
   the real ground it would sit on.** The commit itself flagged "the real class and grade owe a second leg" and then
   shipped without it.
2. **Grey.** Predicted, measured, and shipped anyway. See §4 — this is the one that matters for the snow build.
3. **Flashing.** **Not predicted, not measured, never explained.** Most likely z-fighting between the drawn mesh and
   the driven surface — which means the mesh **conformance leg passing did not mean the drawn form sits right on the
   real DEM.** That is a hole in the test, and it is still open. Any future authored form inherits it.
4. **No map marker.** One was added and he still saw nothing. It was committed with an explicit "NOT visually
   verified — no smoke path opens the M-key chart" caveat, and that caveat was the whole story.

**THE RULE THIS THREAD KEEPS RE-LEARNING:** a green probe on a synthetic fixture is not a measurement of the thing
Chad will look at. Before any rung claims done, ask what the fixture **cannot represent** — real slope, real ambient,
real class, real shading, the actual draw — and either measure it there or say plainly that it is unmeasured.

---

## §2 WHAT SURVIVED, AND IS STILL TRUE

- **SK-1d** (`c2b7bfeeb`): steer and lean are fully independent — **mouse = lean (300 px), A/D = steer** (analog,
  2.0 /s, self-centring at 3.0 /s), Q/E = lean trim, C = recentre. The weight-transfer HUD is pasted to the rider's
  **posed sweater frame** (it had been on his helmet — `cg_h` was counted twice). Handlebars deleted from both
  presentations. Palette reads `render::team_colors()`: grid = ally blue, ball = slag orange, glowing.
  **Still awaiting his verdict on the feel** — he has driven it but ruled only on the HUD colours and the steer rate.
- **M12**: there is **no carveable steer angle on Road at any speed**. Held 12 s, every steer from 0.02 to 0.26 rolls
  past 140°, at constant speed as well as WOT, while **zero steer is stable to 44.9 m/s**. The old "carve band" was a
  4-second observation-window artifact. Any terrain design must assume **the player cannot hold a steered line.**
- **The consult** (`docs/snowform_terrain_jump_consult.md`): its red-team kill list still binds — the global snow
  skin, per-site sibling meshes with an ambient rim, and a `d_escape` scalar are all dead.

---

## §3 THE LADDER — `declarative-jumping-conway.md` RENDER LANE

Chad's ruling 2026-08-26: **"3. NOTHING DYNAMIC IN THAT"** — every form is static, baked offline. Nothing responds
to cold, weather, time of day or use. That is his word, not an omission.

| rung | what | gate |
|---|---|---|
| **SF3-0** | MEASURE: clipped-snow-pixel % across the day cycle; census every `radius_at` consumer (rev 1 named 2 of ~17); frame-time + draw-call baseline; pump-bowl census; track-ribbon visibility | no game code |
| **SF3-1** | **WINTER EXPOSURE** — drop the daytime ground gain so snow lands ≈0.978 instead of clipping. **Nothing downstream is visible without it.** | Chad's fly |
| **SF3-2** | ONE HERO SITE: offline shape mesh + baked per-vertex AO + the lit `snowform_fs` + one class texture | he rides past it at noon, 58°, and moonless night |
| **SF3-3** | SELECT, BAKE, INSTANCE — scoring, ~5,300 sites, baked table, shape library, instanced draw, `depth_at` composition. **One indivisible rung** (no depth term ships without its geometry) | the drive |
| **SF3-4** | THE PUMPS — collar apron + spoil rim at 3–5 m | "I know which pump this is from 500 m" |
| **SF3-5** | DEFORMATION + TRACK RIBBON | WINTER_LAW S4 |

---

## §4 ★ THE FACT THAT SHOULD SHAPE THE FIRST RUNG (measured, and NOT in the plan)

**Snow heroes are drawn through `kBuildingFS`, the mono BUILDING shader.** `render/buildings.cpp:236` sets the
`-2.0` snow sentinel and `:106-107` is the whole light model: `uAmbient + uDiffuse * max(dot(N, -uSunDir), 0.0)`.
No `uGroundDayGain`, no `uNightFill`, no `uMoonFill`, no `uWinterNightGlow`, no `sky_aerial()`.

Measured against the planet ground 100 m away in the same frame: **+2.0 DN at high sun (invisible)** and
**−140.1 DN at night (a dark hole in a white field)**. It inverts **DARK**, not bright — the opposite of what the
plan predicted. **This is what Chad meant by "grey in color."** SF3-2's own three-cell gate would fail at two of its
three cells today, on the shipped mechanism, before a single new form exists.

**Chad has ruled the fix IN** (2026-08-26): *"4. YES ON LIT SHADER INTRODUCING SOME CHROMA OK."* The rung is a **lit
sibling of `planet_fs()` sharing the ground-lighting body as a common GLSL string** — the `kSkyGLSL`/`kConeGLSL`
concatenation idiom `render/sky.cpp:191-192` already runs twice — so the two cannot fork. It is a shader rung, not a
mesh rung, and it re-prices all three snow heroes together.

### ⚠ AND A CORRECTION TO THE PLAN, CHECKED AGAINST THIS TREE
SF3-1 is written as *"one config value: winter `ground_day_gain` 1.60 → ~1.05–1.15"*, citing a
`[seasons.winter] exposure / ground_gain` knob. **That per-season knob does not exist here.** `config/world.toml:30`
has a single **global** `ground_day_gain = 1.60`; `[seasons]` (`:176`) carries season selection and draw weights
only, and there is no `exposure` key anywhere in the file. So SF3-1 is **either** a global change that moves *every*
season's daytime frame, **or** it needs the per-season plumbing built first. Decide which, and say which, before
calling it one value.

---

## §5 CLOSED — DO NOT RE-PROPOSE

- **The whole SF3 jump build** (§1). Sites, jump probe mode, map marker, spawn override.
- **The global snow skin**, **per-site sibling meshes with an ambient rim**, **a `d_escape` scalar** — all killed by
  red teams across two plan revisions; post-mortems in the plan's "What earlier revisions got wrong".
- **Re-coupling lean to steer** — Chad ruled it out on feel after driving it.
- **A second palette, or typed hex, in the sled HUD** — `render::team_colors()` is the source.
- **Lowering `steer_max_rad`**, **`plane_lat_load_frac`**, **anything that only re-shapes a lateral force**,
  **moving `stance_m`/`cg_height_m`**, **`traction_mu` for the bank-strike** — each measured and closed.

---

## §6 ⭐ OPEN — AWAITING CHAD, NOT AN AGENT

1. **SK-1d's feel verdict** — is 2.0 /s steer right, does driving by lean feel like it did before SK-1c took the
   mouse, does the blue grid read on his back at speed.
2. **SF3-1's scope** (§4) — global exposure change, or build the per-season knob first.
3. **The grip/tip inversion.** M12 makes it worse than believed: it blocks the carve at *every* steer angle. Nobody
   may move measured machine geometry to close it, so every buildable lever is already in §5.
4. **`class_blend_m`'s value.** Numbers say 1.0; a feel dial is ruled on the stick. Cost: the blend leaks inward, so
   the metre of road nearest the shoulder is looser, and he drives **37.7 % Road**.
5. **The 4 red baseline legs** — pre-existing, escalated, unruled.
6. **`generated/graph/graph.json` is stale on the base** — belongs in its own commit on `main`.

---

## §7 COMMANDS

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/seads_sandboxes/winter-gi/build/_deps/raylib-src \
  -DFETCHCONTENT_SOURCE_DIR_GLM=D:/seads_sandboxes/winter-gi/build/_deps/glm-src \
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=D:/seads_sandboxes/winter-gi/build/_deps/catch2-src \
  -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=D:/seads_sandboxes/winter-gi/build/_deps/tomlplusplus-src
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure     # ~33 min; expect the 4 known-red sled legs
```

**THE DRIVE — end every report with this block, it is what Chad asks for every time:**
```
cd D:\seads_sandboxes\sandbox_snow
cmake --build build-play --target seads
.\build-play\seads.exe
```
**NEVER `build\seads.exe`** — `-O0`, 45× on the hot path; every feel call made on it is made on a lie.
Keys: **J** mount the sled · **H** swap HUD · **C** recentre · mouse = lean · A/D = steer · Q/E = lean trim.

⚠ **`--smoke` needs a frame count**: `seads.exe --smoke 3 shot.png`. Bare `--smoke` runs open-ended, looks like a
hang, and holds a lock on the exe so the next link fails `Permission denied` (`rm` the exe and confirm the relink).
Screenshots are written to the **cwd** by basename, so clean them up — they land in the repo root.

A/B the bank-strike blend with no rebuild: `config/world.toml` → `[snowpack] class_blend_m` (0.0 = step, 1.0 = fix).
