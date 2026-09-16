# STEREOSCOPE SUDBURY — live handoff + evidence ledger

**Driven by the `/stereoscope-sudbury` skill** (`.claude/skills/stereoscope-sudbury/SKILL.md`).
This doc is the SINGLE source of live status. The skill is stable; read THIS for "what stage now."
The plan is `docs/stereoscope_sudbury_plan.md`; the math evidence is
`docs/stereoscope_fable_staged_plan.md` + `docs/world_stereoscope_research.md`.

---

## ▶▶ FRESH-SESSION START HERE (next agent)

### ★ POPULATE THE WHOLE MAP — buildings everywhere (2026-07-14, NEWEST) → AWAITING-FLY
Chad picked "populate the whole map" as the next goal + asked for a Fable master-plan compatibility
audit (the future game — Errington Tunnel, solid ground, snowmachines; he recalled a "16-bit bake").
Fable ruled the 16-bit bake is DEM-only + ORTHOGONAL (buildings drape at runtime, store dir+relative-h),
render-only binary asset is compatible, + three conditions (keep ribbons compiled-in for the millwright
AI; spatial-tile + cull; carry a sim-neutral collision index). Delivered in stages, each gate 413/413,
0 goldens, kernel + cross-agent files untouched.
- **✅ Stage 1 — buildings to a binary asset** (`world-buildings-bin-stage1`, VISUAL NO-OP): ~505k verts
  moved out of the 75 MB constexpr `render/sudbury_gis.gen.h` (→ 24.7 MB) into a sectioned/versioned
  `assets/sudbury_buildings.bin` (magic SBLD, u64 lock hash, section table). Pure shared parser
  `render/building_asset.{h,cpp}` (app + test, no fork), runtime lock-validate + inert fallback. Fable P0
  (OOB from trusting file offsets) folded. `offline_tool/{building_bin,header_to_bin}.py`.
- **✅ Stage 2 — whole-disk fill + tiling + cull** (`world-buildings-wholemap-stage2`): 3 towns → the
  WHOLE disk = **87,584 buildings** (+57,915 filled), 1.26M verts. Spatially tiled (aeqd cells,
  `BUILDING_TILE_M=3000`, 400 batches, median bound 0.078 rad) + a runtime horizon/distance CULL (mirrors
  the tree chunk cull) — drew 110/401 at 7.8 km, 58/401 at 2.7 km (far hemisphere skipped). BAT record
  16→48 B (cone bound), version stays 1. `fetch_ms_buildings(frame,None)` whole-disk (`MS_WHOLE_DISK`);
  `BUILDING_MAX_COUNT` 45k→130k; buildings-only re-bake `offline_tool/bake_buildings.py` (writes ONLY the
  .bin — no terrain re-bake). Fable-AFTER SOUND, P2 folded (relief-connected h_top). Evidence
  `shots/stage2_wholemap_*.png`.
- **✅ Stage 3 — sim-neutral collision index** (`world-buildings-collision-stage3`): a COLL section in the
  .bin, one record per kept footprint (86,347: center dir + equivalent-area radius + eave height), for a
  FUTURE solid-ground/targeting feature (MASTER_PLAN Phase 1). No runtime consumer yet; validated in the
  gate. Fable-AFTER SOUND (radius is an extent PROXY not a cover — documented for the future consumer).
- **NEXT ACTION = Chad flies the populated map** (night, from altitude — dense grids across the disk, not
  just the 3 towns). Fly-dials: `[buildings]` look; `BUILDING_TILE_M`/`BUILDING_MAX_COUNT` (re-bake via
  `bake_buildings.py`). The MASTER_PLAN is `D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md`.

### ★ CC7 — SLOW BRAIDED SLAG POUR + church setback (2026-07-14) → AWAITING-FLY
Chad flew CC6 and asked for (a) the slag pour to look like a BRAIDED river down the black hill, (b)
the flow to OOZE SLOWLY like real earth-time (it "raced down in ~1s"), and (c) the Chelmsford
St-Joseph church pushed BACK off Errington Ave. Procedural (the pour is a shader+mesh, NOT a Blender
asset — Blender can't drive flow that stays glued to the terraced face; Chad OK'd procedural). Fable
BEFORE (SOUND-WITH-FIXES) + AFTER (SOUND), all folded. gate 413/413, 0 goldens, kernel + cross-agent
files untouched.
- **✅ Church setback** (`f418db6e8`, `world-church-setback`): `setback_m` 22→45 in
  `sudbury_hero_place.py`, regenerated `render/sudbury_hero.gen.h` (lock 0x99061E1583D34607). Church
  shifts back along −fwd into its lot; clear front-lawn buffer before Errington. Fly-dial.
- **✅ CC7 braided pour** (`93ad60f5a`, `world-cc7-braided-pour`): **timing** — the lava FS front now
  advances a CONVEX map `pow(vD,1.6)` (fast off the lip, crawling at the toe) over phase [0.06,0.72] =
  0.66 of the cycle (was 0.35) → ~2× the on-face descent; brief fed plateau then exp cool (τ 0.055),
  wrap-safe (extinction 0.973<1). **braid** — `build_lava_mesh` reworked from parallel strips to an
  anastomosing braid: threads weave (braid_amp·sin, per-thread freq jitter + alt fan so no lockstep),
  cross (overlapping ranges), and the width PULSES → pinch (split/dark crust bar) then swell (merge);
  per-thread lift ε·rv kills crossing z-fight; every vertex samples the SHARED face profile (no float);
  `texcoord.x=vD` unchanged so the single-clock FS is untouched. `[slag] rivers` 5→8; **cadence** —
  `[train] car_gap_m` 24→36 lengthens each pot's pour so the slow ooze reads (train speed unchanged;
  train.cpp warns if a car_gap crank pushes the pour window past the spur).
- **NEXT ACTION = Chad flies both.** Pour: night, Copper Cliff (`SEADS_CEL ~9459` region puts the train
  at the dump; the standalone continuous pour is `SEADS_NO_TRAIN=1`, ~50 s cycle, always active — good
  for judging the braid). Evidence `shots/cc7_braided_pour_night.png` (braid on the black ridge, front
  mid-descent) + `shots/cc7_braided_pour_crest.png`. Church: fly Chelmsford, confirm the front-lawn
  buffer. **Fly-dials:** pour SPEED = `[train] car_gap_m` (bigger = slower) / `[train] rate_hz`; braid
  look = `[slag] rivers`/`river_halfwidth_m`/`glow`/`night_boost`; church buffer = `setback_m` in
  `sudbury_hero_place.py` (re-run to regen the header). **Honest note:** the pour front SPEED is still
  capped by the per-pot cadence (single-clock design); the shader reshape ~doubled the descent time,
  car_gap 36 lengthens it further — if Chad wants it slower still, raise car_gap_m or lower rate_hz.

### ★ COPPER CLIFF DUMP ACCURACY PASS — CC6 (2026-07-13) → all AWAITING-FLY
Chad: *"make it accurate and correct to reference. Use fable for accuracy and build consult with blender"*
+ reposition steer (dump faces NW, open area N of the stack S of Regional Rd 15, track from the smelter,
research the exact geography). Four staged commits, each gate **413/413**, 0 goldens, Fable-vetted,
kernel + cross-agent files untouched. Plan `docs/copper_cliff_plan.md` `# CC6`, ledger `copper_cliff_ledger.tsv`.
- **✅ CC6-3 reposition** (`f1630d368`, `world-cc6-reposition`): researched real INCO geography (slag departs
  the smelter EAST end; Superstack 46.4801,-81.0565); OSM-openness scan → anchor on the building/road-free
  slag flats **46.494,-81.0455** (N of stack, off roads). Shared `render/copper_cliff_geo.h` +
  `offline_tool/cc_track.py` (killed the slag.cpp/train.cpp dir dup). Axis 43° NE, face 313° NW, track from smelter.
- **✅ CC6-4 terraced MESA** (`9aac92258`, `world-cc6-terraces`): flat-topped BENCHED black slag mesa via ONE
  single-source `face_profile` (knots) feeding the mesh + lava + `ridge_lift_at` drape (Fable 3-consumer P0);
  `[slag] benches` dial [1..5]; crest widened to 36 m, slag darkened.
- **✅ CC6-2 RAILS** (`6baaad045`, `world-cc6-rails`): static draped 2-rail + tie track via the SAME `point_at`
  as the cars (climbs the crest); `point_at` hoisted above `build_train_renderer`; ~8.3k verts.
- **✅ CC6-5 MACHINERY** (`3de3c4720`, `world-cc6-machinery`): Blender glTF **steeplecab loco + giant tapered
  trunnion slag pots** (`assets/coppercliff/{loco,slag_pot}.glb`), loaded via the fleet path w/ procedural
  fallback; pots render as round ladles (2/car) tipping to feed the pour.
- **NEXT ACTION = Chad flies the whole dump** (placement / terraces / rails / loco+pots / night pour). The
  dump is now N of the stack in open ground (the oblique-cam center moved: **PAN 20.3 TILT 25.8**; the OLD
  pinned PAN 25.95 TILT 30.44 pointed at the OLD ENE spot). A good **night-pour** offset (train AT the dump)
  = `SEADS_CEL ~9459`. Fly-dials: `offline_tool/cc_track.py` coords (re-run → paste into copper_cliff_geo.h),
  `[slag] benches`/`crest_width_m`/`mound_color`, `[train]` pour cadence. **Honest notes:** the terraces read
  SUBTLY at noon (near-black slag) — they show best in raking light + are traced by the night lava; framing the
  elevated crest with the oblique cam is fiddly (the train reads best near-top-down or fly low). **⚠ NOT
  Chad-flown yet — this whole pass is AWAITING his stick.**

### ★ COPPER CLIFF DUMP EXPANSION — CC4 slag RIDGE + CC5 dump TRAIN → AWAITING-FLY (2026-07-13, prior — CC6 above supersedes)
Chad (/agent-builder): "complete the Copper Cliff slag-pour dump true to historic reference — model a
train and the proper length of the hill ridge the track are atop of, and the giant steel pots they use
to dump." Un-deferred the two `DEFERRED-*` Copper Cliff rows; built as CC4+CC5 (the `program.md`
Living-Copper-Cliff loop, Fable BEFORE+AFTER each). Both AWAITING-FLY, gate build+ctest **413/413**, 0
goldens, kernel + cross-agent files untouched. Plan: `docs/copper_cliff_plan.md` `# CC-EXP`.
- **✅ CC4 — the slag RIDGE (tag `world-cc4-ridge`, `91ad279c7`).** The CC3 radial cone MOUND is now a
  **~700 m LINEAR slag ridge**: a flat crest (carries the CC2 track) + one steep **35° pour face aimed NW**
  (toward the highway) + tapered ends. `render/slag.cpp` `make_slag_ridge`/`build_ridge_mesh`; single-source
  `ridge_crest_h`/`ridge_y0` shared by the mesh, the lava, AND the track drape (`ridge_lift_at`, so the
  rails ride the crest — Fable F3). New `[slag]` dials **`ridge_length_m`** (the "proper length"),
  `crest_width_m`, `face_angle_deg`. Fable-AFTER folded a run-taper single-source fork. Evidence
  `shots/cc4_ridge_*.png` (linear range + orange pour on the NW face).
- **✅ CC5 — the DUMP TRAIN (tag `world-cc5-dumptrain`, `a83ebf484`).** A steeplecab trolley **loco +
  pantograph** hauling **giant tapered cast-steel POTS on trunnion cradles (2/car)**; the spur DUMP LEG is
  **rerouted atop the ridge crest** (spur 3321→3951 m); each pot **TIPS ~120° over the dump edge** as it
  passes; and the CC3 lava is **single-clocked off the TRAIN HEAD** (`train_dump_phase`) so **the pots FEED
  the pour** (CC3's static rim pot drops when the train feeds it). New `[train]` dials `car_gap_m` (=**pour
  cadence**, F8) / `tip_span_m`. Fable-AFTER folded an `s_dump` segment-refine so the pots pour OVER the
  lava. Evidence `shots/cc5_pots_feed_pour.png` (the car string on the crest, orange pour cascading right
  below it), `cc5_pour_night.png`.
- **NEXT ACTION = Chad flies Copper Cliff at NIGHT** (the Superstack, SE of the map midpoint; scrub `]` to
  night — spawn is daylight): the black slag ridge NE of the smelter, the loco+pots crawling the crest,
  each steel pot tipping over the NW edge, molten orange slag cascading down. **⚠ #1 fly-note / open
  decision — POUR CADENCE:** the pour is now GATED to the train pass, so with the realistic slow haul
  (`[train] rate_hz=0.0011`) the loop is ~15 min and the pour only runs ~44 s per lap — Chad may fly there
  and see a dark ridge between passes. Two levers: (a) raise `[train] rate_hz` (faster loop = more frequent
  pours, but a faster train), or (b) set `[train] enabled=0` / `SEADS_NO_TRAIN=1` to get CC3's **standalone
  continuous ~50 s pour** back (with its own static tipping pot). Other dials: `[slag]` `ridge_length_m`/
  `mound_height_m`/`crest_width_m`/`face_angle_deg`/`glow`/`night_boost`/`rivers`; `[train]` `car_gap_m`/
  `tip_span_m`/`color`. **Honest notes:** the pots read small from altitude (fly low to see them tip); the
  loco/pot/cradle geometry is procedural low-poly (a glTF swap is a later option); the deep-night lava can
  bloom white at distance (dial `glow`/`night_boost` down). Ledger `docs/copper_cliff_ledger.tsv`.

### ★ STREET LAMPS — perceptibility + coverage (Chad fly-note 2026-07-13) → AWAITING-FLY
Chad flew: lamps "nearly imperceptible from up close... flying overhead and looking down can barely
perceive them. From far they are perfect." Also "some parts of Sudbury have no street lamps."
- **Perceptibility (committed `15683e243`, config-only, no re-bake):** overhead at ~2 km the lamps sit in
  the `min_px` floor regime (a small dot) and don't pop against the lit grid, whereas far they aggregate
  into a legible glow. Bumped `[streetlamps]` `min_px` 2.5→4.0 + `core_bright` 1.0→1.7. Smoke A/B
  (oblique night, offset 240): overhead now outlines the street grid with lamp rows; the far aggregate
  stays a legible glow, not a bloom. Fly-dials live in `[streetlamps]`.
- **Coverage (re-baked, this commit):** lamps only go on `minor` roads, and non-residential ones
  (tertiary/collector) needed a mapped building within 95 m — but MS home-fill only covered Chelmsford/
  Azilda/Dowling, so other communities' un-buildinged streets were dark. Fix: `sudbury_lights.py` now also
  lights a collector street within `LAMP_NEAR_RESIDENTIAL_M=300 m` of a RESIDENTIAL street (uses town
  streets as the "in-town" anchor → rural roads far from any residential street stay dark, NO new
  buildings). Lamps 33362→35463; sparse communities up (Lively +40%, Val Caron +35%, Falconbridge/
  Coniston up). Terrain PNGs byte-identical (deterministic), lock hash unchanged, gate 413/413, 0 goldens.
- **NEXT ACTION = Chad flies a dusk/night pass** looking straight down over the town (perceptibility) +
  over the outlying communities (coverage). Both are dial/reach-tunable if he wants more.



### ★ LAKE WANAPITEI STRAIGHT-EDGE REPAIR — DONE, gate green → AWAITING-FLY (2026-07-12, NEWEST)
Chad flew the world and reported Lake Wanapitei "has a straight section across its width." **Root cause
(diagnosed against the real bake pipeline):** Wanapitei is a huge lake (aeqd rho 27→42.4 km); the
real→procedural WILDERNESS FADE annulus `[36.5, 41] km` cut across its outer third. The existing
"keep lakes real" exemption only protected lake texels where the fade weight `w<0.5` (rho<38.75 km), so
everything past that constant-rho arc dropped to wilderness land = the straight cut. Separately the
dedicated mirror-surface mesh REJECTED the whole 132 km² lake because a 0.17% tip pokes past R_MAX.
- **Fix (3 offline-bake changes, projection LOCK UNTOUCHED — hash identical, no downstream cascade):**
  ① `sudbury_bake.py` — keep masked lake texels real across the full IN-DISK extent (`rho<=R_MAX`, not
  `w<0.5`), with a ~500 m **DEM shore feather** (distance transform, seam-wrapped) so the far shore stays
  real-on-real (no mirror-over-lower-land plinth). ② `sudbury_water.py` — **CLIP** the lake polygon to the
  disk instead of rejecting it wholesale (Wanapitei now gets a 5491-tri mirror; Lake Nipissing at rho~84
  km clips to empty). ③ `build_sudbury.py` `_assert_in_disk` — check the lake EDGE (max rho), not the
  centroid (the fixture-no-op that let this ship), using the largest-area name match.
- **Fable BEFORE (SOUND-WITH-FIXES: shore feather, GeometryCollection-safe clip, edge assert) + AFTER
  (SOUND-WITH-FIXES, 3×P1 folded: EDT seam-wrap, largest-area hit, grain-assert capband excludes lakes).**
- **Re-baked at 16 m** (matches the committed baseline — `sudbury_color.png` came out BYTE-IDENTICAL,
  confirming the res + that the fix doesn't touch albedo). Changed: dem/landmask/normal/treedensity PNGs +
  `sudbury_gis.gen.h` + the lock date-stamp (hash unchanged). Landmask changed 0.63% (all far-ring: the
  restored Wanapitei arm + other far lakes now kept real — consistent with "every named lake real").
- **BONUS: deleted `.claude/gate_waiver`** — the clip fix makes the previously-WAIVED "dedicated water
  surfaces are well-formed" ctest GREEN (Nipissing no longer emits inward-wound far-side triangles).
- **Gate:** build clean; **ctest 413/413** (incl. the un-waived water test); **0 goldens moved**; seam
  grep n/a (offline tool). Evidence: `shots/wan_fixed_landmask.png` (the restored natural SW arm vs the
  old straight cut). **NEXT ACTION = Chad flies to Lake Wanapitei** (NE of the map, ~133° arc from
  center) — the straight cut should be gone and the lake reads as a full mirror to its shore.



### ★ S6 "PRINTED CARD" HALFTONE / DITHER MODE — DONE (Chad flew: "looks nice", 2026-07-12)
**FLOWN + APPROVED.** Chad's verdict: "looks nice." Same fly he noted the W3 snowflakes read too big /
in-your-face → toned `snow_size_m` 0.28→0.18 m (`b816e314c`, flown "okay"). The dials below are live if
he revisits (`[halftone]` + `SEADS_HALFTONE=0|1|2`; default OFF). Evidence ledger retained:
Chad's pick after the weather ladder ("next most logical"). The LAST signature element of the 1940s
B&W stereoscope look: a screen-locked newsprint print MODE, **OFF by default**. When on, the mono
silver world remaps into a print screen; the **aircraft stay smooth color** (the same split-tone `sat`
gate). Self-contained post-FS layer — kernel untouched, no `sim/`/`control/`.
- **What landed:** `render/post_glsl.cpp` gets a halftone block after vignette / before grain — **mode 1
  = AM halftone DOTS** (rotated screen lattice, `sqrt`-area-linear tone, analytic ~1px AA, no `fwidth`
  seam-grid), **mode 2 = ordered 4×4 Bayer DITHER** (pixel-locked, threshold-quantum-scaled soft). paper
  = the normalized warm-silver highlight RATIO, ink = `ink·split_shadow` cool near-black; `col =
  mix(print, col, sat)` so planes pass untouched. Grain attenuated on the mono print, FULL on planes
  (`mix(grain_mul,1,sat)`). Pure fn of `gl_FragCoord` — screen-locked "paper", no clock, cosmetic.
- **Threading:** 6 new uniforms on the validator allowlist (`test_asset_validator`); `[halftone]` config
  section (`enabled`/`style`/`scale_px`/`angle_deg`/`soft`/`ink`/`grain_mul`) + `WorldParams` + strict
  loader (style∈{1,2}, scale_px≥2 NaN/subsample floor) + 3 `test_load_world` legs (value + 2 reject);
  `PostParams` + `post.cpp` upload; `app/main.cpp` glue + **`SEADS_HALFTONE=0|1|2` launch override**.
- **Fable:** BEFORE SOUND-WITH-FIXES (folded: area-linear `T=mix(-aa,1+aa,sqrt(1-Yh))`; analytic AA not
  `fwidth`; normalize-not-clamp paper; `smoothstep(a,a,x)` UB floors; grain-on-print rule). AFTER
  SOUND-WITH-FIXES, **1×P1 folded** — mode-2 `soft` was raw-luma (±1.0) vs the 1/16 Bayer spacing →
  the dither collapsed to a flat grey ramp + lifted blacks; scaled `s = max(soft,1e-2)·0.03125` (soft=1
  = ±½-step = CRISP dither, exact endpoints) — plus P2 hoisted the Bayer matrix to a file-scope const.
- **Gate:** build clean; validator EXACT-allowlist green; seam grep clean (no clock in `post_glsl.cpp`/
  `post.cpp`); **ctest 413/413** (was 410 → +3 halftone load legs), **0 goldens moved** (`git diff
  --stat test/golden` empty). Off-by-default = bit-exact identity (the `if(uHalftoneMode>0)` block is
  skipped; `gmul=1.0`).
- **Smoke (read back), pinned oblique cam, dusk offset 75** — `shots/s6_halftone_{off,dots,dither}.png`:
  OFF = full-detail dusk basin (town street grid, lake sun-glint, silver limb; file 3.4 MB). DOTS =
  terrain STIPPLED, the sun-glint isolated as a white blob, the limb dotted (file 2.1 MB = fewer unique
  values, the print signature). DITHER = reads as an authentic printed card — the mono terrain remaps to
  the paper/ink range, the **town grid stays legible**, the **space-first sky stays dark**, the **HUD
  draws RAW green on top** (post untouched). No aircraft in the oblique review frame, so the
  planes-stay-color leg is by construction (identical `sat` gate as the vetted split-tone), not pixels.
  (Offset-0/75/250 are all dusk/night at this recentered viewpoint — the celestial cycle is short; a
  brighter midday frame wasn't chased per the anti-rabbit-hole rule. Chad flies real daylight anyway.)
- **NEXT ACTION = Chad flies it.** Set `[halftone] enabled=1` in `config/world.toml` (or launch
  `SEADS_HALFTONE=1` dots / `=2` dither), fly LOW over Chelmsford in DAYLIGHT (scrub `]`/`[` to midday):
  does the town read as a 1940s printed card? Planes still smooth color? Dial `[halftone]` `scale_px`
  (coarser/finer dots), `angle_deg`, `soft` (hard↔soft), `ink` (black↔grey ink), `grain_mul`. The plan
  warns hard dither can fight the smooth silver — if so, prefer mode 1, raise `soft`, or leave OFF.
  **A live toggle key (like `K` for season) is a natural follow-up if Chad wants to A/B on the stick.**

### ★ WEATHER + SEASONS THREAD STARTED (2026-07-12, newest — the /agent-builder loop)
Chad's next big world lever. **Autonomous build loop is LIVE:** `weather_seasons_program.md` (repo root,
sibling of `program.md`) drives it off the Fable-vetted spec `docs/weather_seasons_plan.md`; ledger
`docs/weather_seasons_ledger.tsv`. Fable is CONSULT-ONLY (before+after audits). Staged ladder:
**W1 seasons framing → W2a eye-scalar localized weather → W2b per-fragment field → W3 precip → W4 winter
ground.** Chad's ask: LOCALIZED microsystems (not global overcast), NO wind (cells pulse in place / precip
falls straight down), snow=winter + rain=spring, season RANDOM per spawn with a STATIC override for the
future winter-only loop.
- **✅ W1 DONE → AWAITING-FLY (tag `world-w1-seasons`, `0c51497a8`).** `render::Season`
  {Winter,Spring,Summer,Autumn} chosen APP-SIDE at spawn (never render/, never t_cel/clock): precedence env
  `SEADS_SEASON` > `[seasons] static_season` > deterministic smoke default > weighted `mt19937` random
  draw, re-rolled each respawn. Read-only into `FrameInfo`; the HUD bezel tags the active season. Fable
  before (SOUND-WITH-FIXES, 3×P0/5×P1 folded into the plan) + after (SOUND, 3×P2 folded). Gate build+ctest
  **391/391**, 0 goldens moved. Smoke A/B: `SEADS_SEASON=winter` vs `=summer` differ ONLY in a 274px
  bottom-left tag. **Fly:** confirm the HUD season tag; try `static_season="winter"` in `config/world.toml`
  (or `SEADS_SEASON=winter`) for the winter-loop build.
- **✅ W2a DONE → AWAITING-FLY (tag `world-w2a-eyecell`).** LOCALIZED weather: the global `weather_haze`
  scalar is now the storm BUDGET, and `render::weather_cell(dir,t_cel)` (pure, in `render/weather.{h,cpp}`)
  localizes it to a FIXED 24-cell Fibonacci lattice on the sphere — each cell ACTIVATED (derived-from, not
  multiplied-by) the budget via an ordered per-cell threshold, combined by soft-OR `1−Π(1−env·falloff)`.
  W2a feeds it at the EYE's ground-track (`normalize(pose.eye)`) into the ONE `uHazeDensity` scalar every
  consumer already reads (**zero fork**). budget=0 ⇒ field 0 EVERYWHERE (inherits ~70%-clear + spawn-clear);
  NO WIND (centers fixed, field is a pure fn of (dir,budget)). Config `[weather_cell]` (cell_count/inner_deg/
  outer_deg/thresh_lo/thresh_hi/env_width) + strict loader + `test_load_world` value/lock/reject legs;
  `test_weather.cpp` pins clear-⇒-0, bounds, LOCALIZED (patchwork not global), and NO-WIND. Fable-after
  SOUND-WITH-FIXES, **1×P1 folded** (the golden-ratio threshold sequence resonated with the spiral azimuth —
  replaced with a bit-mixed hash-RANK permutation: decorrelated + guaranteed min-gap). Gate build+ctest
  **400/400**, 0 goldens moved. **Smoke** (`--smoke 8 shot.png OFFSET`): offset 1090 (budget=0.948) → eye
  field=0.165 → a light silver horizon haze band, while a GLOBAL impl would paint ~0.85 (near-white) — the
  localization signature; offset 0 (clear) → field=0, pure dark-starry. **Fly:** scrub `]`/`[` — the sky
  should haze as you cross a microsystem and clear between cells (whole-sky uniform overcast is GONE). Dials
  in `[weather_cell]`: `cell_count` (patchwork density), `inner_deg`/`outer_deg` (squall size), `thresh_lo`/
  `thresh_hi` (how much budget a cell needs). NOTE (Fable P2, not folded): `thresh_hi=0.95` means the top ~2
  cells never fully activate at peak budget (~0.9) — lower `thresh_hi`→~0.75 if all 24 should peak.
- **✅ W3 DONE → AWAITING-FLY (tag `world-w3-precip`, `82e3e5e74`).** Season-gated PRECIPITATION: Winter =>
  snow (round soft flecks), Spring => rain (thin streaks along local_up), Summer/Autumn => dry. Drawn ONLY
  where a W2 weather cell is active at the eye (fades as you leave). A WORLD-ANCHORED jittered lattice —
  flake bases are fixed in world space so flying through them STREAMS them past (not a camera snow-globe);
  the world grid is invisible bookkeeping. The ONLY motion is the fall along local_up = normalize(eye) —
  NO WIND (no horizontal advection). Box follows the eye with a radial boundary fade. Pure `precip_sample`
  (`render/precip.{h,cpp}`) gate-pinned (no-wind / world-anchor / fade / determinism); raylib billboard
  renderer (`render/precip_draw.{h,cpp}`) in the translucent tier. New `[precip]` config. `SEADS_NO_PRECIP=1`
  A/B. Fable-after SOUND-WITH-FIXES, **2 folded** (P0 the iterated block under-covered the fade sphere =>
  grow H >= box_half/cell+1.5 + derive the fade radius so coverage holds by construction; P1 foreshorten the
  rain streak near-radial). Gate **409/409**, 0 goldens. **Smoke** (`--smoke 8 shot.png OFFSET` at a forced
  W2 cell, e.g. 1090): winter=>snow flecks, spring=>rain streaks, summer=>dry, winter+clear=>dry. **Fly:**
  set `SEADS_SEASON=winter` (or `spring`), scrub `]`/`[` into a weather cell — snow/rain appears under the
  squall + clears between cells. Dials in `[precip]`: cell_size_m (density), snow/rain size+rate+opacity.
- **✅ W4 DONE → AWAITING-FLY (tag `world-w4-snowcover`, `93622ac89`).** Winter snow-cover tint on the
  planet LAND albedo (a shader lift, no terrain re-bake — toggles instantly with the static-season override
  for the winter-only loop). Whitens flat ground toward snow, MASKED off water (lakes stay dark mirror),
  slope-reduced (rock faces keep grey); applied to albedo BEFORE the lighting products so terminator / moon
  fill / aerial are preserved. `uSeasonSnow=0` outside Winter => bit-exact identity. New `[ground]` dials
  `winter_snow_cover` / `snow_albedo` / `snow_slope_lo`. Fable-after SOUND (no P0/P1). Gate **410/410**, 0
  goldens. **Smoke** (`SEADS_OBLIQUE=1 SEADS_SEASON=winter --smoke 6 shot.png 75`): the land whitens, lakes
  stay DARK mirror, roads etch through; summer = identity. **Fly:** `SEADS_SEASON=winter` (or
  `static_season="winter"` in `config/world.toml` for the winter loop) — terrain reads snow from altitude in
  daylight; winter NIGHTS brighten under snow (expected). Dials: `winter_snow_cover` (amount; 1.0 whiteouts
  cubemap detail, 0.85 keeps texture), `snow_slope_lo` (how steep sheds snow).
- **⏸ W2b DEFERRED (`sky.cpp`/`sky.h` cross-agent dirty).** W2b (per-fragment weather field — the distant
  haze-bank look, so a cell hazes a REGION of the sky not the whole dome) must thread per-fragment haze
  through `set_atmosphere_uniforms` in the OFF-LIMITS `render/sky.cpp`+`sky.h`; `sky.h` is currently dirty
  from the parallel celestial agent, so the `git apply --cached` hunk can't isolate cleanly without racing.
  W2a already delivers localized weather at the eye. **RESUME W2b when `sky.*` is clean/committed** — host
  `weather_local(dir)` + the ray-perigee sample in a NEW `render/weather_glsl.*`, localize all SIX
  `uHazeDensity` consumers (grep-kill every raw read), upload via the one `set_atmosphere_uniforms`. The
  pure `render::weather_cell` already exists (from W2a) as the CPU truth to mirror.
- **★ WEATHER+SEASONS LADDER COMPLETE (2026-07-12 autonomous):** W1 seasons · W2a eye-scalar localized
  weather · W3 precip · W4 winter ground = AWAITING-FLY; W2b = DEFERRED. Kernel firewall intact throughout;
  no cross-agent files touched. All five stages logged in `docs/weather_seasons_ledger.tsv`. **NEXT ACTION =
  Chad flies the four AWAITING-FLY stages** (a winter-loop `static_season="winter"` shows all of W1+W2a+W3+W4
  at once), then W2b when `sky.*` frees up.
- **⌨ FLY AID — the `K` key cycles the season LIVE (`7e977ae9a`):** press `K` in-flight to step
  Winter→Spring→Summer→Autumn; the HUD tag (W1), precip type (W3), and ground snow (W4) all update in the
  same frame. Interactive-only (never touches smoke/probe), cosmetic, app-owned. To SEE precip you must be
  under an active weather cell — scrub `]`/`[` into a squall (or `\` = force overcast); W4 snow shows on the
  terrain in daylight regardless of weather. Existing debug keys: `F1` raw/instructor, `]`/`[` weather-time
  scrub, `\` force-overcast, `K` season-cycle.

### ★ HANDOFF SNAPSHOT (2026-07-12, newest — READ THIS FIRST)
**Where the world is right now:**
- **✅ S3 RIVERS = MIRROR WATER — DONE, Chad flew it: "rivers look good" (2026-07-12).** Commits
  `a409da3ab` (mirror + reflected stars + run-into-lakes + tiny-creeks-dropped) + `c11642e01` (meander-
  smooth). See the detail block just below. This closes the rivers thread.
- **AWAITING-FLY queue (built + gated green, pending Chad's stick):** AURORA MONO (`45f50dbda`), CC3 slag-
  pour refine (`6dc0fca37`), + the older rows still in the STAGE STATUS table (S4 lamp refinements 1/2/
  spacing, S5a heroes). None are blocking.
- **DEFERRED WORKLIST (Chad, for later):** ① lengthen the slag hill into a ridge · ② rail+trains that dump
  pots to feed the pour · ③ the **Big Nickel** hero (Dynamic Earth ~46.4693,−81.0207) · ④ Science North
  snowflake glTF (Blender) · ⑤ Chelmsford French Catholic church · ⑥ **Onaping Falls** (~46.65,−81.36,
  later Blender animation). Full block below.
- **Next logical world steps (candidates — ask Chad):** weather/winter dynamics (the biggest remaining
  experiential lever; the `t_cel→haze` gate exists) · rivers→ **a Big Nickel / church procedural hero** off
  the worklist · S6 stereoscope polish (halftone/dither). The S-stage ladder (S0–S6) + Copper Cliff (CC1–3)
  are all landed. **Kernel is FROZEN — never touch `sim/`/`control/` from this thread.**
- **Tree state:** clean on `sandbox/world-sudbury`, gate green (ctest 380/380, 0 goldens). Cross-agent
  files (`render/sky.h`, `generated/gen_log.jsonl`, `bf109_preview/`, `docs/ballistics_*`, `offline_tool/
  ballistic_score.py`, `.claude/skills/ballistics-forge/`) are OTHER agents' — never stage them.

---

### ★ SESSION 2026-07-12 (detail) — CC3 pour REFINED + AURORA MONO + S3 RIVERS(→MIRROR, DONE) + WORKLIST
- **✅ S3 RIVERS = MIRROR WATER — DONE (Chad flew: "rivers look good"). Commits `a409da3ab` + `c11642e01`.**
  Chad flew the first bright-silver cut ("look like white lines, not rivers") and ruled the LAKE mirror
  finish (a BLACK mirror reflecting the sky + STARS), running INTO the lakes, tiny creeks dropped; then flew
  the mirror and asked for meander-smoothing. All landed + flown:
  - **Mirror** — rivers draw through the PLANET water branch (`render/river_surfaces.{h,cpp}`, `uForceWater=1`,
    same as the lake surfaces) — draped strips from the baked kind-3 geometry, radial normal, single-sourced
    (no fork, H1). The ribbon FS no longer draws kind-3 (the bright-silver branch + `[ribbons] river_color/
    river_sheen` were REMOVED).
  - **Reflected STARS** (procedural sparkle) — a night-gated `water_stars()` in the SHARED water branch (so
    lakes AND rivers get it), a per-location ripple scatters the sample so a starfield reads on the water (a
    flat mirror alone samples ~one cell). `[water] star_reflect`/`star_density` dials.
  - **Run into lakes** — lake-clip REMOVED (silver-over-silver overlap invisible); road-clip stays (bridges).
    **Tiny creeks GONE** — a union-find CHAIN filter (`sudbury_header`, keying every vertex, river/canal
    anchors) keeps only stream systems ≥ `RIBBON_STREAM_MIN_CHAIN_M=4000m`.
  - **Meander-smooth** — coarse OSM waterway corners Chaikin corner-cut (`sudbury_ribbon._chaikin`,
    `RIBBON_RIVER_SMOOTH_ITERS=3`) after a COARSER river DP (`RIBBON_RIVER_SIMPLIFY_M=30m`) — waterways only.
  - Fable BEFORE (3×P1 mirror-plan + 3×P1 ribbon-plan) + AFTER (SOUND) folded. Gate green throughout, gen.h
    ribbon-additive, lock UNCHANGED, terrain reverted. Fly-dials (if Chad revisits): `[water] star_reflect`/
    `star_density`/`reflectivity`; `RIBBON_STREAM_MIN_CHAIN_M` / `RIBBON_RIVER_SMOOTH_ITERS` /
    `RIBBON_RIVER_SIMPLIFY_M` (re-bake). Superseded first cut = the bright-silver ribbon kind (`42b639c50`).
- **✅ AURORA MONO → AWAITING-FLY (commit `45f50dbda`) — closes the SCENE-MONO GAP.** Chad picked "aurora
  mono (close the gap)" as the next world step. The night aurora's green/red chroma was passing the S1
  split-tone sat gate as a 2nd chroma (the night sky was not truly B&W). FIX (data-only, single-sourced):
  `[aurora]` `tint_low → [0.80,0.80,0.80]`, `tint_high → [0.42,0.42,0.42]` — NEUTRAL GREY keeping the old
  green-base/red-top LUMINANCE structure (bright base, dimmer tops) so the two-band curtain + vertical
  falloff still read, now in silver. `C/mx = 0 < sat_c0 (0.06)` → the post fully silvers it like the town;
  the planet-pass ground glow reuses `uAurTintLo` so it silvers too (no fork). SUPERSEDES the 2026-07-09
  green/teal ruling. Curtain math (`render/aurora_glsl.*`) unchanged + already Fable-vetted → no new
  mechanism. Config-only (`render/sky.h` — the cross-agent celestial file — untouched); ctest 380/380, 0
  goldens moved. **Verify: Chad flies TOWARD THE POLE at night** (its 3 prior commits were pole-fly-verified;
  no smoke shots exist — framing the oval headless needs the pole geometry). Fly-dials `[aurora]`
  intensity/oval/curtain_scale; if the silver reads too flat, nudge the tint_low/tint_high luma split.
- **✅ CC3 slag pour REFINED → AWAITING-FLY (commit `6dc0fca37`).** Chad fly-note: the pour "looked like 3
  bar rectangles" and "cycles of pouring move too fast." Fixes (cosmetic render-layer, gate blind to the
  app binary — needs his fly): `render/slag.cpp build_lava_mesh` now makes a **natural alluvial fan** —
  rivers NARROW + close-packed at the source, FAN OUT + WIDEN toward the base, a **leading center vertex**
  (vD bulged forward) so the front reads ROUND and "overtakes itself", a rounded toe, meandering edges,
  `steps 10→16`; `vD` stays in `[0,1]` so the single-clock front/cool shader is untouched. `[slag]`
  `pour_rate_hz 0.05→0.02` (~one pour every 50 s, was 20 s), `rivers 3→5`, `river_halfwidth_m 7.0→5.0`
  (source width) → a fuller braided fan. **Fly-dials if it still reads wrong:** the fan width (`fan` /
  `src_spread` in `slag.cpp`) and the pour period (`pour_rate_hz`).

- **★ DEFERRED WORKLIST — the Copper Cliff EXPANSION + remaining heroes (Chad, 2026-07-12, "defer … then
  start the next logical step").** Chad flew the pour, liked the theme, and DEFERRED the big expansion to
  keep the map moving. Recorded here + in `docs/copper_cliff_ledger.tsv` so it is not lost:
  1. **LENGTHEN THE SLAG HILL** — extend the CC3 mound (a single radial-lobed cone today,
     `render/slag.cpp build_mound_mesh`) into a longer slag **RIDGE / mountain** along the dump edge; the
     real INCO dump is a man-made range, not one cone. Keep the pour face aimed NW.
  2. **RAIL + TRAINS THAT DUMP THE POTS** — extend CC2 (`render/train.{h,cpp}`, today just loops the closed
     spur) so the loco + pots run to the dump edge and **TIP the pots** (like CC3's one static tipping pot)
     to **FEED the pour**, synced to the pour phase — the train delivers + dumps, not just circulates.
  3. **THE BIG NICKEL** (NEW hero) — the giant 9 m nickel-coin monument at Dynamic Earth
     (~**46.4693, −81.0207**, Big Nickel Rd SW of downtown). Procedural hero like the Superstack (a thin
     vertical disc/coin on a plinth), mono steel; add to `offline_tool/sudbury_hero.py` (reuse
     `render/buildings.cpp`, no new render path). See the S5 hero seam in `docs/copper_cliff_plan.md`.
  4. **SCIENCE NORTH snowflake glTF** (S5b) — still DEFERRED (needs Blender 5.1 + `tools/blender/addon.py`).
  5. **CHELMSFORD French Catholic church, Errington Ave** (S5c) — DEFERRED (procedural steeple like the
     Superstack, or a glTF).
  6. **ONAPING FALLS** (High Falls / A.Y. Jackson Lookout on the Onaping River, ~**46.65, −81.36**, NW of
     the city — a real, beautiful Sudbury landmark; Chad 2026-07-12) — for LATER Blender animation work
     (a falls with moving water). Deferred with the other Blender items.

### ★ SESSION 2026-07-12 SUMMARY (READ THIS FIRST) + THE NEXT BIG REQUEST
**This session landed, all AWAITING-FLY, gate green throughout, terrain baseline never moved, kernel
firewall intact, cross-agent files (`sky.h`/`gen_log.jsonl`/`bf109_preview/`) never touched:**
- **S6 far-field-only DEPTH OF FIELD** (tag `world-s6-dof`) — Chad picked DoF + far-only; **Chad flew it:
  "the DOF looks great" (2026-07-12)** (effectively approved; dials `[dof]` if he wants to tune).
- **Lamp fix round 1** (tag `world-s4-lampfix`) — light ALL residential streets (13,493→44,873; the
  20-40 km outlying ring 788→16,925) + the RMB-zoom floor fix (min-px floor referenced to the RESTING fov
  so lamps magnify under gunsight zoom instead of washing out).
- **Lamp fix round 2** (tag `world-s4-lampfix2`) — two-part glow (soft halo + tight hot CORE) so lamps
  read as lit bulbs up close w/o oversaturation; lamps moved to the ROADSIDE (alternating), not the
  centerline. Dials `[streetlamps]` core_bright/core_sharp.
- **Lamp SPACING** (tag `world-s4-lampspace`) — Chad flew it: **"too many lights on roads."**
  Researched real spacing (residential = **30-45 m**, ~3-5x the ~6 m pole height); set `LAMP_SPACING_M`
  **34→45 m** (upper end, sparser 1940s feel). Re-bake; it's a bake constant (re-bake to change), the dial.
- **★ CELESTIAL WHEEL DIRECTION REVERSED** (tag `world-celestial-wheel-reverse`, `2129b8b31`) — Chad flew
  the world (2026-07-12): "it flies very good and looks good too. Only thing is that the sun and moon go
  the wrong way its opposite." Sun/moon/stars all wheel via ONE lever (`sky_wheel = R(+â, omega_day·t)`),
  so a single sign change on `omega_day` in `render/celestial.cpp make_celestial` reverses all three
  consistently. NOT a naive wheel-angle negation (that double-counts `omega_year` → the solar day goes
  ~1.05 rad/day short, the trap the solar-day test pins); recomputed so `|net| = 2π/day` stays exact but
  retrograde: `omega_day = omega_year − 2π/day` (was `+`). Gate: build PASS, ctest **380/380**, goldens
  unmoved. AWAITING Chad fly (does the sun now cross the correct way?). **⚠ This CHANGES where the sun sits
  at any given `t_cel`, so the deep-night `--smoke` offset MOVED AGAIN — the old numbers are STALE.**

**⚠ THE NIGHT SMOKE CELL DRIFTS (celestial agent owns sun timing AND the wheel direction was just reversed
2026-07-12 — old offsets are STALE): SCAN for it (render lamps-on vs `SEADS_NO_LAMPS=1` across a few
offsets, pick the big-diff one; a 0-diff offset = daytime, lamps gated off).**

### ★ LIVING COPPER CLIFF is UNDERWAY via an AUTONOMOUS LOOP (2026-07-12 overnight, /agent-builder)
Chad said "/autoresearch" heading to bed. Built an autoresearch-style **program-loop agent** (`program.md`
+ the Fable-vetted spec `docs/copper_cliff_plan.md` + ledger `docs/copper_cliff_ledger.tsv`) that advances
the three Copper Cliff elements one at a time — build → gate → Fable-red-team → commit AWAITING-FLY — then
STOPS when all three are done/deferred. To resume: *"look at program.md and continue the Living Copper
Cliff build."*
- **✅ CC1 — Superstack smoke: BUILT + gated + Fable-vetted → AWAITING-FLY** (tag `world-cc1-smoke`,
  `09fdfae4e`). A mono drifting plume off the 381 m stack, 28 camera-facing alpha billboard puffs whose
  positions are a PURE wrapped phase the app derives from `t_cel` (render/ never sees raw `t_cel`).
  `render/smoke.{h,cpp}` mirrors the lamp billboard seam; `[smoke]` dials in world.toml (opacity/color/
  drift/rise are the fly-dials); `SEADS_NO_SMOKE=1` A/B. Smoke shot `shots/cc1_smoke_{on,off}.png` — the
  plume column rises off the Superstack and vanishes under the A/B (proves it's the smoke). Fable BEFORE
  (design, 2×P0 folded) + AFTER (landed, 1×P1 folded: hash1 unsigned). Gate 380/380, 0 goldens moved.
  **NOTE the plume reads BRIGHT at night (alpha grey over near-black) — by day it's a darker plume vs the
  silver sky; if Chad wants it softer, dial `[smoke] opacity`/`color`.**
- **✅ CC2 — slag-pot trains: BUILT + gated + Fable-vetted → AWAITING-FLY** (tag `world-cc2-train`,
  `8429d2007`). Loco + 8 open slag pots on a hand-authored CLOSED spur (smelter→dump→return, ~3.8 km)
  draped on the height field; arc-length pose from a wrapped `t_cel` phase; mono steel via the buildings'
  dFdx facet shader. `render/train.{h,cpp}`, `[train]` dials (color/ambient/rate/pots/car_gap), spur dirs
  hardcoded from the locked projection; `SEADS_NO_TRAIN=1` A/B. Smoke shot `shots/cc2_train_{on,off}.png`
  — the car chain appears on the spur and vanishes under the A/B. Fable AFTER SOUND, 1×P1 folded (exact-L
  OOB). Gate 380/380, 0 goldens moved. **⚠ DAY PERIOD = 300 s (5-min day) — the `--smoke` offset wraps
  every 300 s; offset 0/1800 are NIGHT, ~120 is a lit dusk cell (what CC2's shot used). The train reads
  small/dim at altitude (steel is sun-lit); `[train]` color/ambient + a close low pass are the fly-dials.**
- **✅ CC3 — slag pour: BUILT + gated + Fable-vetted → AWAITING-FLY** (tag `world-cc3-slag`, `8d6993e94`).
  THE MARQUEE: a dark procedural slag-heap mound at the Copper Cliff dump + a slag pot that TIPS at the rim
  + 3 molten lava rivers down the pour face (aimed back toward the Superstack). The lava is the ONE
  sanctioned WARM-CHROMA exception — a single-sourced `heatColor()` (hot→cooling-red→crust) on a SINGLE
  CLOCK (front advances + supply plateau then cools, wrap-safe), OPAQUE emissive so the high chroma
  survives the split-tone gate (VERIFIED: sat-mask reads white on the lava, like the aircraft), night-
  brightest. `render/slag.{h,cpp}`, `[slag]` dials (glow/rivers/mound/pour_rate); `SEADS_NO_SLAG=1` A/B.
  Smoke shots `shots/cc3_slag_{on,off,sat}.png` — three orange rivers glow on the dark mound at night,
  vanish under the A/B, read white in the sat-mask. Fable AFTER SOUND, 1×P1 folded (supply plateau so the
  rim stays fed while the pot pours) + a self-caught wrap-pop fix. Gate 380/380, 0 goldens moved.

**★★ LIVING COPPER CLIFF is COMPLETE (all 3 stages AWAITING-FLY) — the program.md loop hit its stop clause.**
Chad's morning fly list: (1) **CC1 smoke** — fly the Superstack plume day + night (`[smoke]` opacity/color/
drift/rise); (2) **CC2 train** — a LOW close pass over the Copper Cliff spur to see the loco + pots
(`[train]` color/ambient/rate; it reads small/dim from altitude — steel is sun-lit); (3) **CC3 slag pour**
— fly Copper Cliff at NIGHT for the lava rivers (`[slag]` glow/rivers/pour_rate/night_boost; the mound is
dark by design so the glow is the hero). **⚠ DAY = 300 s (5-min cycle); offset 0/~8 is NIGHT (lava glows),
~120 is a lit dusk cell. The night `--smoke` cell moved with the celestial wheel reversal — scan.** All
three NEVER touched sim/control; cross-agent files untouched; every stage Fable BEFORE+AFTER, gate
380/380, 0 goldens moved. Ledger: `docs/copper_cliff_ledger.tsv`. Next options if Chad wants MORE: the
additive glow halo around the lava (deferred — the opaque body reads well), smoke inheriting the moving-sun
wind, or the S5b/S5c heroes (Science North glTF, Chelmsford church) still deferred on Blender.

**★ Also this session: the CELESTIAL WHEEL DIRECTION was REVERSED** (tag `world-celestial-wheel-reverse`) —
sun/moon/stars were crossing the sky the wrong way; see the SESSION SUMMARY above. This moved the day/night
`--smoke` offsets: offset 0 is NIGHT (aurora), ~1800 is a usable Copper-Cliff daylight-ish cell (what CC1's
shot used). Re-scan per feature.

### ★★ NEXT BIG REQUEST — "LIVING COPPER CLIFF": the INCO smelter comes alive (Chad, 2026-07-12)
Chad's marquee next want (after flying the night town). THREE linked animated elements at the Copper Cliff
smelter/slag complex (the **Superstack** is already the procedural hero at **46.4747, −81.0539**,
`sudbury_hero.py`):
1. **Animated SMOKE** drifting off the INCO Superstack (the 381 m stack).
2. **TRAINS on the track** ("on the hull" = the slag/ore rail line) — a trolley locomotive hauling a train
   of slag pots, moving.
3. **Animated SLAG POUR / DUMP** — the pots **TURN OVER (tip)** off the railcars and **hot molten slag
   SLIDES/CASCADES down the slag heap** as fiery rivers of lava. The iconic Sudbury night spectacle.
   **"Find photos for historical reference to produce an accurate and beautifully visual event" (Chad).**

**Historical reference (researched 2026-07-12):**
- The pour ran **every ~30 min, 24/7**. An electric **trolley locomotive** hauled **~22 slag pots** (each
  ~16-17 tonnes, **~1350 °C**) **~1.5 km** from the Copper Cliff smelter to the slag dump. >10,000 t/day.
- Pots were **tipped/rotated (dumped electrically)** over the dump edge (a pail of water thrown in to snap
  the solidified "skull"). Molten slag poured down the **black man-made slag mountain** as **long fiery
  rivers of lava — a brilliant ORANGE/VERMILION/RED glow** lighting the night sky, cooling to a dark crust.
  Families parked at the base to watch ("Is that Hell?").
- Dump in use since **1929** (115M+ t). Faces: **No.2 → NORTH toward Sudbury, No.3 → EAST (West End),
  No.4 → toward Hwy 144, No.5 highest.** Chad recalls it **viewable from the highway to the NORTHWEST of
  the dump** — aim a pour face NW toward the highway so a low pass catches it. Ended ~2002 (later Vale-automated).
- Photo/video refs: Flickr "Sudbury – Pouring Slag at Night" (Bruno, /photos/64062788@N05/5835843295),
  Getty "Copper Cliff Ontario" + "molten slag", Sudbury.com Memory Lane slag-pour articles (paywalled but
  quoted), turnstone.ca/rom192ss.htm, mindat.org/loc-225836.html.

**Design notes for the next agent (tri-model; Gemini-heavy asset+animation build; NOT the frozen kernel):**
- **CHROMA EXCEPTION (confirm w/ Chad but it's the whole point):** the hot-slag glow is a SANCTIONED warm
  ORANGE/VERMILION chroma in the B&W world (like the aircraft + aurora), NOT silvered by the S1 post —
  single-source it so the split-tone chroma gate passes a high-sat warm emissive. The stack SMOKE stays
  MONO/silver. The pour is NIGHT-brightest (night-gate the glow like lamps/windows).
- **No clock (§9 seam):** every animation phase (smoke drift, train position, pot tip, slag flow) is a PURE
  fn of the app-owned frame counter / `t_cel`, NEVER a wall clock. render/ reads state, writes nothing.
- **Smoke** = drifting plume (billboard/particle or scrolling-noise shader) off the Superstack top, wind-drifted.
- **Trains** = a small rig (loco + pots) moving along a baked track polyline (OSM rail near Copper Cliff, or a
  hand-authored smelter→dump spur); pose = pure `t_cel` phase.
- **Slag pour** = the marquee: pot tip + a glowing cascade down a NEW procedural slag-heap mound near the dump;
  emissive gradient hot-bright→cooling-red→black-crust, animated flow. The "that's Sudbury" dusk/night hero.
- Scope = BIG (new meshes + 3 animations + a chroma exception). A staged S5/S6 hero sub-thread. Use
  `/stereoscope-sudbury` or `/orchestrate`; Fable-vet the phase math (pure→pose, no clock) + the emissive single-source.

### ★ CURRENT STATE (2026-07-11, end of the night-lighting session)
**S0 · WATER · S1 · S2 · S3 all DONE. S4 town is BUILT + fully fly-refined + a NIGHT-LIT TOWN Chad
confirmed "fantastic". S5a heroes landed.** This session (all committed, gate green throughout, terrain
baseline never moved, kernel firewall intact) added, in order:
- **S4 massing** (`5134756c9`) + **dark-roof tone** (`7edb01a43`) + **triangular gable roofs**
  (`93343b68f`) — houses read as houses.
- **Home-fill** (`world-s4-homefill`) — Chelmsford/Azilda/Dowling were near-empty in OSM; +6,084 Microsoft
  ML footprints → 35,073 buildings, all three now dense town grids.
- **S5a heroes** (`world-s5a-heroes`) — Copper Cliff Superstack (381 m spike) + Laurentian towers, real coords.
- **Night lighting** — window lights (`world-s4-windows`) + **street lamps** (`world-s4-lamps`) tuned
  through Chad's fly (`world-s4-lamps`→`bf4a36e81`→`dc4dc79ce`): thinned windows, lamp glows that grow up
  close + a min-px floor from afar. **Chad flew it to night and ruled "fantastic" (2026-07-11).**

**⚠ NIGHT-LIGHTING UX (the #1 gotcha): all night lighting is NIGHT-GATED and the game SPAWNS IN DAYLIGHT
(cel offset 0).** So a fresh fly shows NO lamps/windows until you reach night — press+hold **`]`** (scrubs
celestial time ~75 s/real-s; `[` rewinds) or fly ~2–3 min (5-min day cycle). **⚠ THE NIGHT SMOKE CELL
DRIFTS** — the parallel celestial agent edits the sun timing, so the deep-night `--smoke` offset MOVES.
As of 2026-07-12 it is **offset ~60** (offset 240 is now DAYTIME — lamps OFF). Don't trust a pinned number:
SCAN for it (render lamps-on vs `SEADS_NO_LAMPS=1` at a few offsets, pick the one whose diff is large — a
0-diff offset = daytime, lamps gated off). Windows AND lamps share the sun-elevation gate, so "no lights"
almost always = "it's daytime," not a bug.

**✅ LAMP REFINEMENTS → AWAITING-FLY (2026-07-11, tag `world-s4-lampfix`).** Two Chad fly-notes fixed:
(1) **"populate ALL residential streets; some parts of town have no lights"** — lamps required a building
within 95 m, which killed residential streets wherever OSM building coverage is sparse (everywhere outside
the 3 home-filled communities). FIX: trust the OSM tag — `highway=residential`/`living_street` are town
streets BY DEFINITION, so they light unconditionally; the near-building gate stays only for ambiguous
`tertiary`/`unclassified` (can be rural). Lamps **13,493 → 44,873**; the 20-40 km ring (outlying communities:
Sudbury proper / Copper Cliff / Lively / Garson / Val Caron) went **788 → 16,925**. Offline re-bake (cached
OSM), `gen.h` lamp-only diff, terrain PNGs reverted byte-identical, lock hash unchanged. (2) **distant lamps
DIM/vanish under RMB gunsight zoom** — the `min_px` size floor was computed from the LIVE (zoomed) fov, so a
floored far lamp stayed a constant screen size while the town magnified 2.8x around it → washed out. FIX
(`render/lights.cpp`): reference the floor to the RESTING fov (`kChaseFovyDeg`) — identical at rest (far
lamps stay altitude-legible) but now MAGNIFIES with the zoom like the world-metres term. Analytically
verified (screen_px = cot(½fov_live)·tan(½fov_rest)·min_px = min_px at rest, ~3x under 2.8x zoom); needs
Chad's RMB-zoom fly to confirm (smoke can't zoom). Gate: build PASS, validator 18/18, ctest 367/367
goldens-0. `[streetlamps]` brightness may want a downward nudge now there are 3.3x more lamps (Chad's fly).

**✅ LAMP REFINEMENTS ROUND 2 → AWAITING-FLY (2026-07-12, tag `world-s4-lampfix2`).** Two more Chad
fly-notes: **(3) lights dim/vague up close** — the glow was a single soft gaussian (peak 0.55) that reads as
a haze, not a lit lamp, once it grows to world-size up close. FIX (`render/lights.cpp` kLampFS): a TWO-PART
glow — the soft HALO (aura) PLUS a tight hot CORE (the bulb: `exp(-r²·core_sharp)`) so a lamp reads as an
ACTUAL illuminated point up close; the core is kept tight (`core_sharp=34`) so a dense residential street
doesn't blow to a white bar (Chad: illuminate WITHOUT oversaturation). New `[streetlamps]` dials
`core_bright=1.0`/`core_sharp=34`; halo `brightness` nudged 0.55→0.40 for the 3.3x-denser field. Verified
up-close max luma 245 (bright, NOT clipped), town-from-above 0 pure-white px (no blowout). **(4) lamps in
the MIDDLE of the road** — the bake sampled the road centerline. FIX (`sudbury_lights.py` +
`LAMP_ROADSIDE_M=5.0`): offset each lamp PERPENDICULAR to the road to the roadside, ALTERNATING sides so a
street reads as lamps down both sides from the air. Offline re-bake (44,873→44,875), gen.h lamp-only diff,
terrain reverted byte-identical, lock hash unchanged. Gate: build PASS, validator 18/18, ctest 367/367
goldens-0. `[streetlamps]` core_bright/core_sharp/brightness are the fly-dials for the up-close look.

**✅ S6 — FAR-FIELD-ONLY DEPTH OF FIELD landed → AWAITING-FLY (2026-07-11, tag `world-s6-dof`).** Chad picked
DoF as the S6 flagship + the FAR-ONLY flavor (near/mid crisp for combat, only the distant limb/background
softens). Pure additive change to the S1 post pass (kPostFS) — the depth-texture FBO was built for exactly
this. Fable BEFORE+AFTER (AFTER caught a center-tap P1, folded). **★ finding baked into the defaults:** on
R=15 km the horizon from ~2 km up is only ~8 km away, so terrain never gets far — `focus_end` lives in the
single-digit-km range (defaults `focus_start_m=2500 focus_end_m=9000 radius_px=3.0 sky_coc=0.40`), else only
the sky blurs. `sky_coc` caps the far-sky blur so the space-first stars survive. Dials `[dof]`; `SEADS_NO_DOF=1`
+ `SEADS_POST_DEBUG=3` (CoC viz). Gate green, ctest 367/367 goldens-0, validator 18/18. Evidence block below.

**NEXT ACTIONS (pick per Chad):** (0) **Chad flies S6 DoF** — near crisp while flying/fighting? distant limb
softens into the stereoscope-depth feel? stars still read against space? then dial `[dof]` radius/focus.
(1) **S5b — Science North snowflake glTF** — Chad's Hybrid pick, **DEFERRED (Chad 2026-07-11)** — the Blender
MCP bridge is dead until Chad launches Blender 5.1 + `tools/blender/addon.py`; (2) **the big Chelmsford FRENCH
CHURCH on Errington Ave** — a named hero, **DEFERRED (Chad 2026-07-11)** — likely a procedural steeple like the
Superstack, or a glTF; (3) **S6 polish remaining** (weather-dynamics winter groomer lines, dither/halftone
mode, density staging); (4) more S4/S5 fly-dials. The S4/S5a "AWAITING-FLY" stage-table rows can flip to DONE —
Chad has been flying + approving each refinement live. **Fresh dials to remember:** `[dof]`
focus_start_m/focus_end_m/radius_px/sky_coc, `[streetlamps]` size_m/min_px/brightness/color, `[buildings]`
window_* + wall/roof/ambient/diffuse + height_scale. Detailed notes follow; the EVIDENCE LEDGER is further down.

---

**★ S1 TONE IS LOCKED — Chad flew 2026-07-10: "the tone is perfect."** The whole S2–S6 ladder is now
UNBLOCKED. The locked look = the CURRENT committed `config/world.toml [tone]` values (contrast 1.35,
lift 0.02, grain 0.02, cool-silver shadows / warm-silver hi, halation 0.15, vignette 0.32) — Chad
approved them AS-IS, no dial changes were needed. S0 DONE, WATER DONE, S1 DONE.

**S0 · WATER · S1 · S2 · S3 DONE. S4 (building massing) BUILT + gated GREEN → AWAITING-FLY.** The ACTIVE
stage is now Chad's S4 fly, then S5.
**✅ NIGHT WINDOW LIGHTS (2026-07-11, tag `world-s4-windows`):** most houses show lit windows on their
walls when the sun is down (the town twinkles from the air). The HORIZONTAL window-grid coord is the
BAKED wall-perimeter arc-length in `GisBuildingVertex.ws` / `texcoord.y` — **Fable-AFTER caught a P0**: a
per-fragment `dot(worldPos, cross(up,N))` is IDENTICALLY 0 (up==normalize(worldPos)), so the first cut was
a silent no-op (the "twinkle" I saw was the post's grain — the warm-pixel metric was confounded; a
pure-RED signature test at the real night cell now proves it, 7.4k→29.5k px scaling 4× with `lit_frac`).
Vertical coord = radial height. `[buildings]` dials `window_color`/`window_bright`/`window_lit_frac`;
default NEAR-WHITE (low-sat so the S1 post keeps it silver — a warm tint is a 2nd chroma, Chad's call).
Laurentian towers get lit windows too; the Superstack (ws<0) stays dark. **NOTE: the real deep-night
smoke cell is offset ~240** (offset 175/176 is DAY/dusk — a prior mislabel). Gate green, ctest 365/365,
lock unchanged, terrain reverted.
**✅ STREET LAMPS + THINNED WINDOWS (2026-07-11, tag `world-s4-lamps`):** Chad — "can't see the lights
from high up; the point is to see the town from above at night" + "way too many windows." So: (1) wall
windows THINNED to ~6.5 m bays + discrete panes (a few per wall, not a blanket); (2) NEW **street lamps**
as additive billboard point-glows lining the town streets — the point lights that read from altitude.
`sudbury_lights.build_street_lamps` samples MINOR roads every 34 m, keeps lamps within 95 m of a building
(town streets, not highways/rural) → **13,493 lamps** in `GisLightPoint[]`; `render/lights.{h,cpp}` draws
them as constant-screen-px glows (Fable-confirmed billboard math), night-gated, additive. From high up the
town's street grid GLOWS (`shots/lamp_high.png`). Fable-AFTER SOUND-WITH-FIXES: **P1-1** draw-order — lamps
must draw AFTER all opaque depth-writers (planet/buildings/trees/aircraft) or later opaques paint over the
additive glow → moved after the fleet pass; **P1-2** lamp color desaturated to near-white (stays silver
per the mono rule). `[streetlamps]` dials; `SEADS_NO_LAMPS=1` A/B. Gate green, ctest 366/366, lock
unchanged, terrain reverted. **Deep-night smoke cell = offset 240.**
**DEFERRED (Chad 2026-07-11): Science North snowflake glTF (needs Blender) + the big Chelmsford French
church on Errington Ave** — both to revisit later.
**✅ HOME-FILL (2026-07-11, tag `world-s4-homefill`):** OSM building coverage was near-empty in the
outlying communities (Azilda 33 / Dowling 83 / Chelmsford 483 mapped, vs thousands of real homes). Added
**Microsoft GlobalMLBuildingFootprints** (`fetch_ms_buildings` + `merge_buildings` in `sudbury_fetch.py`,
deduped vs OSM by centroid-in-OSM) for the THREE communities Chad named (`HOME_FILL_REGIONS` circles):
**+6,084 homes → 35,073 buildings total** (655k tris). Chelmsford/Azilda/Dowling now read as dense town
grids (`shots/hf_{chelmsford,azilda,dowling}_crop.png`). MS footprints are untyped → residential default
6 m + the gable/hip hash-split (they read as houses); reuse the S4 render path. Best-effort (a network
failure = OSM-only, logged). A WHOLE-MAP fill is available but ~+60k buildings (~1.3M verts) → would want
the building geometry moved to a binary asset first (deferred). S3 linework ribbons
was flown + signed off (Chad, 2026-07-11: "perfect"). S2 boreal
trees are signed off (Chad flew 2026-07-10: look "beautiful", then "its good" after two perf passes —
persistent VRAM instance buffer + world-absolute cached per-chunk buffers with 2-chunks/frame amortized
placement; the CPU stutter is gone). S2 lives in `world/props.*` (PURE placement, gate-tested) +
`render/props.*` (raylib instanced draw) + `assets/sudbury_treedensity.png` (baked density) + `[trees]`
dials in `config/world.toml`; `SEADS_NO_TREES=1` = A/B. Evidence block below. **S2.1 (Blender tree kit)
is a deferred polish swap** — not requested.

**✅ S3 — linework ribbons: DONE (Chad flew 2026-07-11: "perfect"; tag `world-s3-ribbons` + fly-tune
commits through `fc658a287`).** Roads (major/minor) + **176 snowmobile trails** (`route=snowmobile`)
render as DRAPED GEOMETRY ribbons on the planet's own height field, replacing the 11.5 m rasterized
albedo road-lines (removed). Offline tessellation `offline_tool/sudbury_ribbon.py` (aeqd-plane miter
offset → project; great-circle arc-length `s`; transverse `v`; CCW winding; **trails CLIPPED off road
corridors** so they never overlap) → per-KIND ushort batches in `render/sudbury_gis.gen.h` (4 batches,
~101.8k verts) → `render/ribbons.{h,cpp}` builds one draped mesh per batch at `radius_at(dir)+2 m` (the
SAME height field the terrain mesh + trees use — anti-float) with a procedural FS. **FINAL FLOWN LOOK**
(after several fly-tune passes, all captured in the S3 evidence block's `iter` lines): roads = a **DARK,
CONTINUOUS asphalt bed** with a light fade + smooth low-freq mottle (never erasure/flicker — the
high-freq mottle term was removed) + a **thin dashed light centerline**; snowmobile trails = a **subtle
DARK-BROWN earth path** (`trail_color=[0.37,0.27,0.19]`), ~25% narrower than the first pass, clipped at
road crossings. Winter groomer/corduroy lines are DEFERRED to the weather-dynamics pass (removed shader
in history at `65d64df09`). `[ribbons]` dials in `config/world.toml` (read at startup); `SEADS_NO_RIBBONS=1`
= A/B. Fable BEFORE (SOUND-WITH-FIXES, all P0/P1 folded) + AFTER (SOUND). Gate: build PASS, ctest
**365/365** goldens-0, asset-validator 18/18 (ribbon leg + lock-hash unchanged). **Rivers LANDED as the
4th kind 2026-07-12 (commit `42b639c50`, AWAITING-FLY — see the top session block).** **NEVER touch
`sim/`/`control/`.**

**✅ S4 — building massing: BUILT + gated GREEN → AWAITING-FLY (2026-07-11; tag `world-s4-buildings`).**
Real OSM footprints (29032 kept) → extruded flat/gable/hip prisms (Chad's roof ruling), baked as 7
static draped ushort batches (415k verts / 540k tris) in `render/sudbury_gis.gen.h`; MONO sun-lit,
rendered CULL-OFF with an FS flat-facet normal from `dFdx` of the eye-relative worldPos. Mirrors the S3
ribbon seam: offline `sudbury_building.py` (extrusion, ear-clip cap, min-rect gable/hip roofs) +
`sudbury_fetch.fetch_buildings` + header batches; runtime `render/buildings.{h,cpp}` + `[buildings]`
config end-to-end; `SEADS_NO_BUILDINGS=1` A/B. Fable BEFORE (SOUND-WITH-FIXES) + AFTER (UNSOUND→fixed:
the **P0** was the FS lighting frame — vPos was world-absolute so roofs shaded as dark walls; fixed to
`matModel·vertex` eye-relative; P1-1/2/3 folded). Gate: build PASS, asset-validator 24/24 (new prism +
extrusion + cap/roof tripwire legs), ctest 365/365 goldens-0, lock hash UNCHANGED, terrain PNGs reverted
(gen.h pure-additive vs HEAD). Evidence block below. **NEXT ACTION = Chad flies S4** over Chelmsford
(reads as blocks + streets? then dial `[buildings]` wall/roof/ambient/diffuse/height_scale — the tone is
a sane default, not tuned to his stick). See `docs/stereoscope_s4_plan.md` for the design + both Fable
rounds. **✅ S5a — HERO LANDMARKS (procedural half) LANDED → AWAITING-FLY (2026-07-11, tag
`world-s5a-heroes`):** Chad named the icons he wants — the **Copper Cliff Superstack**, **Science
North**, and the **tall Laurentian buildings** — and picked a HYBRID build: procedural for the Superstack
+ Laurentian, a sourced glTF for Science North's snowflake. The procedural pair is IN: `sudbury_hero.py`
emits a 381 m tapered-cylinder Superstack (46.4747,−81.0539) + a cluster of Laurentian slab towers
(46.4596,−80.9690) at real coords, appended to the building batches (reuse `render/buildings.cpp` — no
new render path). Superstack reads as the iconic spike over Copper Cliff (`shots/s5_superstack_crop.png`).
Fable-AFTER SOUND-WITH-FIXES (P1: the widened h-bound was disarming the town runaway-height tripwire →
`kSudburyBuildingHeroBatchCount` so the validator bounds town batches at 80 m, the hero batch at 400 m).
Gate green, ctest 365/365, lock hash unchanged, terrain PNGs reverted. **NEXT ACTION = Chad flies S4 (dark-
roof town) + S5a (Superstack + Laurentian).** **S5b — Science North snowflake glTF is DEFERRED (Chad's call
2026-07-11 "defer it"):** the sourced-glTF route needs Blender running with the SEADS addon (`C:\Program
Files\Blender Foundation\Blender 5.1` + `tools/blender/addon.py` — the MCP bridge reported NOT connected);
when revisited, generate/download the snowflake, ingest (rig-D-style glTF loader), mono-shade + drape at
Ramsey Lake (~46.466,−80.985, PAN=42.4 TILT=35.3). Plan's generic S5 heroes (mine headframe / parish
church / smelter) also remain available after Chad's named three.

**⚠ CROSS-AGENT HAZARD (live as of 2026-07-11):** TWO other agents commit/edit on this branch — the
bf109/rig-D + celestial agent (leaves `render/sky.h` + `generated/gen_log.jsonl` dirty) AND a
flight-AUDIO agent (leaves `app/main.cpp` + `render/*_synth.h`/`audio_dsp.h` + `assets/audio/` dirty).
Consequences + the proven defense (used for every S3 commit): (a) stage stereoscope files by EXPLICIT
path, NEVER `-A`; (b) when you must edit a file another agent has uncommitted work in (e.g. `app/main.cpp`
for a config-setter line), ISOLATE your hunk into the index with `git apply --cached` (extract just your
`@@` hunk from `git diff <file>`, `--check` then apply, verify `git diff --cached <file>` = only your
lines) — see `docs/lessons.md`; (c) the branch tip may not COMPILE FROM A CLEAN CHECKOUT (committed
`render/sky.cpp` uses `moon_tex`, but `render/sky.h`'s decl is left uncommitted by the celestial agent);
the MAIN checkout builds because sky.h is dirty. Do NOT commit their files for them.

**Candidate next actions:**
1. **★ START S4 — building massing** (S0–S3 all DONE; see the S3 block above for the plan + the
   S3-machinery-to-mirror pointer). `docs/stereoscope_sudbury_plan.md` §S4: OSM footprint extrusion →
   prisms + 3 roof archetypes, hybrid batching, BAKED street-aligned tangent frames. S2.1 (Blender tree
   kit) + rivers (a 4th ribbon kind) + winter trail-groomer lines (with weather dynamics) are deferred
   polish swaps.
2. **(historical) S1 tone-lock fly — DONE** ("the tone is perfect", 2026-07-10). Open sub-item still
   available if Chad wants it: author the aurora MONO to complete the B&W world (scene-authoring =
   celestial thread, not the post pass).
3. **Non-blocked data polish — SURFACE-COVERAGE EXPANSION LANDED + FLOWN 2026-07-11 (`587f1f00f`, ✅ Chad
   "all is good on the fly test" — new lakes + re-baked land both read right; refreshed PNGs KEPT).**
   Chad opted into "more lakes." A probe found the map was emitting only ~17 dedicated mirror surfaces
   despite a 48 cap: the `cand` filter (landable OR span>=900) never checked in-disk, so **26 of the
   top-48 slots were folded far-side dupes** (ρ>R_MAX → `build_lake_surface` returns 0 tris → wasted
   slots), and the cap clipped legit in-disk lakes incl. hero **Nepahwin (1126 m)** + landable **Simon
   (1009 m)**. FIX (`sudbury_header.emit`): filter cand to IN-DISK *before* the cap + sort landable-first
   then span. `WATER_SURFACE_MAX_LAKES` 48→80. Result: **66 surfaces** (10.6k verts / 13.8k tris, static),
   lock hash UNCHANGED, gate 363/363. ⚠ The 4 m/24000² CDEM bake OOM'd this 28.8 GB box, so
   `sudbury_bake.build` got BIT-IDENTICAL memory reductions (per-channel palette lerps + `del` of finished
   (N,N) scratch) — verified output-identical: re-baked albedo/DEM are ~98% pixel-identical to HEAD on
   LAND; the 2–6% residual is at water/shore (CDEM lake-flatten re-pull), NOT the rewrite. **OPEN for the
   flying agent:** (a) Chad flies the new lakes (Nepahwin/Simon + the 49 added) + confirms LAND terrain
   still reads right after the re-bake; (b) his call whether to keep the refreshed terrain PNGs or pursue a
   terrain-preserving path. Prior LAKE-DEDUP fix (2026-07-10) is subsumed. Still open (Chad opt-in):
   lower `WATER_SURFACE_MIN_SPAN_M` below 900 for even smaller ponds (75 in-disk clear 900 m today).
- **Projection lock:** `0x99061E1583D34607` PROVISIONAL (S0-REV2 cap). Chad has NOT re-flown S0-REV2's
  full map to flip it LOCKED — but he HAS approved the lakes on it. Leave PROVISIONAL until a clean S0
  re-fly, or ask.
- **Commit discipline:** a PARALLEL agent (rig-D/celestial) commits on this branch with `git add -A` —
  stage stereoscope files by EXPLICIT path, never `-A`. `render/sky.h`, `generated/gen_log.jsonl`,
  `bf109_preview/` in the tree are theirs, not ours.

---

## CURRENT STATUS
- **✅ LAKE SURFACE-COVERAGE EXPANSION 17→66 — Chad APPROVED 2026-07-11 ("all is good on the fly
  test").** Covers BOTH open items: (a) the new lakes (Nepahwin/Simon + the 49 added) read right, and
  (b) the re-baked LAND terrain reads right after the bit-identical bake memory reductions — so the
  refreshed terrain PNGs are KEPT (no terrain-preserving path needed). Commits `587f1f00f` (expansion)
  + `4f3d3325f` (record). Still open (Chad opt-in only): lower `WATER_SURFACE_MIN_SPAN_M` below 900 for
  even smaller ponds (75 in-disk clear 900 m today).
- **✅ LAKES DONE — Chad approved 2026-07-10 ("okay that is really good now").** TWO fixes landed:
  (1) the **OSM multipolygon ring-assembly fix** (the REAL root cause — commit `92d9d2062`) and (2) the
  **dedicated flat mirror surfaces** (commit `5878dcd81`). Root cause: big lakes are `natural=water`
  RELATIONS whose boundary is split across multiple `way` segments that must be STITCHED into one closed
  ring; the fetch closed each `way` alone, CHORDING a straight line across the gap (Whitewater = 6 open
  outer segments + 9 islands → a 3901 m straight edge — Chad's "far side straight all the length"). This
  corrupted the **landmask itself** → straight cuts on every relation-lake, and the dedicated surfaces
  (built from the same broken polygons) only partly hid it. **FIX = proper multipolygon assembly**
  (`sudbury_fetch.py` `_stitch_rings`/`_relation_to_polys`/`_explode`): stitch outer segments into rings
  + subtract island holes. Whitewater chord **3901 m → 253 m**, 9 islands cut; **8895 islands** removed
  globally, **0** rings force-closed. Corrects the shape of **all 1293+ water bodies in the landmask**
  (coverage 6.6%→7.1%), not just the surfaces. Before/after: `shots/whitewater_polygon_fix.png` (red
  chorded vs blue stitched+islands). Fable BEFORE+AFTER both SOUND-WITH-FIXES, all P0/P1 folded. Gate:
  build PASS, ctest **350/350** goldens-0. KNOWN residue (accepted): a few FAR non-hero lakes (Bigwood,
  Ministic) show ~1 km straight segments that are GENUINE upstream OSM edges (single closed ways), not
  the bug; and the Kelly near-duplicate (candidate action #3 above).
- **[HISTORY] DEDICATED FLAT WATER SURFACE = BUILT + gated GREEN (2026-07-10, folded into the DONE above).** Real OSM lake
  outlines (Chad's call) → offline tessellation (`offline_tool/sudbury_water.py`: boundary-densified +
  interior-grid Delaunay, 250 m grid, facet sag 1.04 m < 2 m lift) → unit sphere dirs + triangles in
  `render/sudbury_gis.gen.h` → per-lake raylib meshes at radius `elev_m + [water] surface_lift_m`
  (`render/water_surface.*`), drawn right after the planet with the planet's OWN program
  (`uForceWater=1`) so the mirror look is single-sourced with the limb (no fork). **14 lakes** get
  surfaces (Wanapitei/Whitewater/Vermilion/Ramsey/Long + 9 more); lakes whose outline folds past the
  aeqd disk edge (ρ>R_MAX=42 km, e.g. Lake Nipissing at 84 km, and far duplicate-named lakes) are
  rejected. Smoke A/B (`SEADS_NO_WATER`) over Whitewater PROVES the fix: the bright faceted shore
  "cover strip" (lake-region brightness 66) is replaced by clean dark mirror water (18), edges smooth,
  roads intact, no z-fight. Gate: build PASS, ctest **350/350** goldens-0, asset-validator + new water
  leg (unit dirs / index-range / **outward winding**) green. Fable BEFORE+AFTER both SOUND-WITH-FIXES,
  all P0/P1 folded (evidence block below; plan `docs/stereoscope_water_plan.md`). `SEADS_NO_WATER=1` =
  A/B toggle; `[water] surface_lift_m` + `sudbury_config.WATER_*` are the dials. `projection.lock` stays
  PROVISIONAL (params unchanged, hash `0x99061E1583D34607`). **NEXT = Chad flies** the named lakes low
  (do the big landable ones read as clean water now? faceting gone?). **KNOWN follow-ups (data, not the
  mechanism):** (1) **Kelly Lake** (a landable target) resolves via largest-area name-dedup to a *far
  duplicate* Kelly at ρ=60 km (folded) → gated out; its near-Sudbury polygon lost the dedup. Same
  ambiguity may mis-resolve other duplicate lake names. Fix = disambiguate named targets by
  nearest-to-center, not largest-area (a bake data-curation pass). (2) multipart lakes render only the
  largest ring.
- **⭐ ORIGINAL RULING / DIAGNOSIS (Chad 2026-07-10) — retained for context.** Chad flew
  S0-REV2: pinch = "major success," Wanapitei good — BUT the major landable lakes (Wanapitei, Vermilion,
  Whitewater, Ramsey, Long, Kelly) have "straight-edged cover strips over most of the lake" so they don't
  read as water; small water bodies cut at right angles too. **DIAGNOSIS CONFIRMED (full-res crops):** the
  baked data is CLEAN — the landmask has natural curved lake outlines, the albedo is clean, the DEM has
  the lake ~flat. The straight edges are the **coarse ~118 m terrain MESH** (subdiv 200): a flat lake
  plateau meeting higher shore terrain is a CLIFF, and at low grazing (landing) angles the coarse mesh
  renders that cliff as big straight-edged triangle facets across the lake. **FIX = render each major lake
  as its own smooth flat MIRROR surface at the water altitude, decoupled from the terrain mesh** (real
  lake outline → flat fan-mesh at radius = R + water_height, drawn with the mirror-water look after the
  terrain; the shore cliff no longer facets the water; the flight kernel gets a clean flat landing
  surface later). The shore-erosion (`SHORE_ERODE_M`) becomes moot for these lakes once the surface
  overlays them — revisit on integration. Chad OK leaving straight lines in small/less-visible spots +
  legit straight beaches. Open design choice = real polygon outlines (better) vs simple circular discs
  (simpler). Emit the outlines from the bake (OSM `poly` already fetched) → gen.h → a new render pass.
  **FRESH-SESSION START HERE.** Model plan (tri-model pipeline): **OPUS builds it** (architect + bake
  emission + the new render pass + shader + draw integration + gate) with a **Fable red-team** at the ★
  math BEFORE and AFTER — Fable ALONE is wrong (it's the isolated math sniper, not integration). ★ math
  for the Fable-BEFORE: (1) OSM outline (lon/lat) → sphere dirs at radius R+water_height + the
  flat-vs-spherical-cap approximation over a 1–12° lake; (2) fan-triangulation winding + outward normal;
  (3) SINGLE-SOURCE the mirror look against the planet's `sky_color()` so the lake can't fork from the
  limb (the H1 trap); (4) z-fighting / draw-after-terrain + depth. Default to **real outlines** unless
  Chad says discs. Reuse the existing planet-FS water branch (reflect + sparkle, radial normal) — likely
  NO Gemini needed. Verify each named lake up close via `SEADS_OBL_PAN/TILT`; gate per the skill;
  `projection.lock` re-baked stays PROVISIONAL until Chad re-flies.
- **⭐ S0-REV2 = WILDERNESS CAP (large radius) + PINCH FIX + LAKE SHORE FIX (2026-07-10) → BUILT + gated
  GREEN, FLOWN — pinch "major success", lakes need the water-surface (above). SUPERSEDES the fill version.** Chad flew the fill build and rejected the
  antipodal PINCH ("crunched up, jagged") + reported "land ON the water / straight cutoffs" on the big
  lakes. Fixes: (1) **PINCH KILLED** — the cap now runs at a LARGE radius (`CAP_DEG=160.4`, `R_MAX=41993`,
  coverage 97.1%, so nearly all terrain stays REAL) and the antipodal cone is procedural fBm; the root
  cause was the M2 NORMAL MAP baking from the edge-clamped elevation past the disk → a radial shading
  starburst — fixed by blending N→radial across the cap band (`sudbury_bake.py emit_normal_map`);
  verified GONE (`shots/anti_fix.png` = smooth bush, no starburst). (2) **LAKE SHORE FIX** — the coarse
  ~120 m mesh vs the ~11 m water mask let the fine mask paint water UP the shore ramp; now the water
  RENDER mask is eroded ~1 mesh cell (`SHORE_ERODE_M=130`, small/thin lakes protected) so water sits only
  on the flat interior (`sudbury_bake.py` water_render). Far named lakes STAY REAL under the large cap
  (Wanapitei reads as a flat lake, `shots/wan_fix.png`) — so the "dedicated water disc" plan Chad picked
  may now be UNNECESSARY (the fade is far milder than feared); re-confirm on his fly. `projection.lock`
  PROVISIONAL again (new hash `0x99061E1583D34607`; R_MAX changed). Bake still 16 m CDEM (env override).
  Debug: `SEADS_OBL_PAN/TILT` (deg) pan the smoke cam to any feature. Gate: build PASS, ctest 349/349
  goldens-0, asset-validator 16/16 (lock↔header consistent). **NEXT = Chad flies** (pinch gone? big-lake
  shores clean? far lakes OK, or still want dedicated discs?).
- **S0-REV = FILL THE SPHERE with REAL data (Chad ruling, 2026-07-09) → SUPERSEDED by S0-REV2 (pinch).** Chad flew it 2026-07-10 ("it beautiful") but on closer flight rejected the antipodal pinch → S0-REV2.
  Chad chose "R_MAX everywhere": the projection now fills the sphere with REAL Greater Sudbury edge to
  Chad chose "R_MAX everywhere": the projection now fills the sphere with REAL Greater Sudbury edge to
  edge (`R_MAX = π·R = 47124`, `θ_edge = 180°`, coverage 100%) at TRUE 1:1 (`GROUND_R=15000`) about the
  midpoint. The wilderness fBm cap is RETIRED (`FILL_SPHERE=True`); the far lakes are now REAL floatable
  water (**Wanapitei 85.1 m, Long 24.7 m, Kelly 146.8 m** — all LANDABLE). The antipodal "watermelon"
  pinch RETURNS at the single point opposite the midpoint — **accepted by Chad** as the price of real-
  everywhere (it's on the hidden far side of the planet). `assets/projection.lock` PROVISIONAL hash
  `0xB3BD05484DA34397`; validator leg #25/#26 green. This SUPERSEDES the cap version (committed
  `b86a5bb8b`, kept in history + selectable via `FILL_SPHERE=False`). Evidence block below.
- **✅ S0 DONE — Chad flew the filled-sphere map 2026-07-10 and ruled "it beautiful"; lock now LOCKED.**
  The mechanical follow-through is COMPLETE (2026-07-10): (1) `projection.lock` `status`
  PROVISIONAL→**LOCKED** (`date=2026-07-10 locked_commit=59ee44fe1`; hash-excluded, unchanged
  `0xB3BD05484DA34397`); (2) re-gate GREEN — `cmake --build` PASS + full `ctest` **348/348** +
  `ctest -R asset-validator` **15/15** (the lock-hash leg passing confirms the flip did NOT move the
  hash → downstream stays green).
- **✅ S1 POST STACK BUILT + gated GREEN → AWAITING-FLY (2026-07-10).** The B&W silver-gelatin
  "stereoscope" post pass landed: a scene FBO (RGBA16F color + a real DEPTH24 **texture**, alpha
  reserved for a future S6 transmittance grade) resolved through `render/post_glsl.cpp` —
  `FXAA → display-space contrast sigmoid (NO ACES) → silver split-tone (saturation-gated: mono→silver,
  planes pass through) → warm halation → vignette → film grain`, all display-space, HUD drawn RAW after
  (SPEC §9.2). New `render/post.{h,cpp}` + `render/post_glsl.{h,cpp}`; the loaded-but-unused `[tone]`
  block is now the post pass's data (the ONE tone owner) with the new silver-print dials.
  `SEADS_NO_POST=1` = A/B bypass + degraded fallback; `SEADS_POST_DEBUG=1/2` = depth-viz / sat-mask.
  Fable BEFORE (design) + AFTER (landed) both SOUND-WITH-FIXES, all P0/P1 folded (see the evidence
  block + `docs/stereoscope_s1_plan.md`). Gate: build PASS, ctest **349/349** goldens-0, asset-validator
  **16/16** (new post-FS uniform-allowlist backdoor guard). **NEXT ACTION = Chad flies** (lock the tone
  once over Chelmsford, then dial `config/world.toml [tone]`). NOTE: **S1-M2 (object-space normal map)
  already landed** (`world-s1-m2-normalmap`, AWAITING-FLY). **✅ SCENE-MONO GAP CLOSED (2026-07-12, commit
  `45f50dbda`, AWAITING-FLY):** the aurora tints are now neutral grey so the split-tone silvers the curtain
  — the scene is fully mono (planes + CC3 hot slag stay the only chroma). See the top session block.
- **SIDE-LANDED 2026-07-10 (camera feel, orthogonal to the stereoscope stages — noted because S1's smoke
  framing touches the camera):** freelook mousewheel **dolly-out** + two chase tweaks, all render-only,
  flown + committed (`69351abe6`, prior `37affc02c`). Hold Space + wheel = zoom out to a wide
  "behold-the-planet" view (geometric steps, resets to default each freelook entry, freelook-only so
  combat is untouched); RMB gunsight zoom 4×→2.8×; resting chase 34→45 m. `SEADS_DOLLY_DEBUG=1` = an
  env-gated title-bar readout. Kernel firewall intact (nothing in `sim/`/`control/` touched).
- **DEM-source caveat + the EXACT re-bake command (Chad's call, 2026-07-09):** S0-REV bakes from **16 m
  national CDEM**, whole-grid, via
  `cd offline_tool && SEADS_SOURCE_RES=16 SEADS_CDEM_ONLY=1 ./.venv/Scripts/python.exe -u build_sudbury.py`
  — the 1 m HRDEM `/vsicurl` COG read throttled to ~160 KB/s and stalled, and the FILL window is 48 km.
  Per Ruling C the final asset is 8192/11.5 m texel regardless; the lock hash is DEM-res-independent.
  **⚠ Do NOT just re-run at 4 m on the filled sphere — a 48 km window at 4 m is a 24000² grid ≈ 23 GB
  peak (OOM).** The crisp upgrade is the **LiDAR-CORE** approach: a small high-res HRDEM window around
  Chelmsford (fast transfer) baked at ~4 m and blended into the 16 m fill / M2 normal map — DEFERRED,
  network-gated. Committed config stays `SOURCE_RES_M=4` (the intent); the bake uses the env overrides.
- **★ GO-ANYWHERE design constraint (Chad, 2026-07-09):** the whole planet surface must be
  traversable/landable EVERYWHERE — airstrips + lakes are named spots, not gates (land a field, a road,
  float a lake later; snowmobiles maybe later). The LANDING INTERACTION is Chad's flight-kernel work; the
  **world thread's job is a go-anywhere-coherent map** — S0's full-sphere cap already delivers a seamless
  traversable surface; **S3 roads must drape as real drivable/landable ribbons (not floating decals),
  S2/S4 props sit ON the surface (no impassable walls), relief stays gently landable.** The lake tension
  is RESOLVED by S0-REV (fill the sphere) — every named lake is now REAL floatable water (Wanapitei/
  Vermilion/Kelly/Long included); the whole planet is real Sudbury edge to edge.

## STAGE STATUS (one row per stage; terminal state is AWAITING-FLY until Chad's pasted word flips it)
| S | State | Chad's DONE quote (dated) |
|---|---|---|
| S0 projection re-bake + wilderness cap | DONE (Chad 2026-07-10); lock LOCKED, re-gate green 348/348 + 15/15 | "it beautiful" (2026-07-10) |
| S1 lock the look (post stack + depth FBO) | DONE (Chad 2026-07-10); tone locked AS-IS, M2 normal map rode along in the "perfect" look | "the tone is perfect" (2026-07-10) |
| S2 trees | DONE (Chad 2026-07-10); look "beautiful" + perf fixed (persistent VRAM + world-abs cache) | "its good" (2026-07-10) |
| S3 linework ribbons | DONE (Chad 2026-07-11); gate 365/365, tag `world-s3-ribbons` + fly-tune to `fc658a287`; Fable BEFORE+AFTER | "perfect" (2026-07-11) |
| S3 rivers → MIRROR water | DONE (Chad flew 2026-07-12: "rivers look good"); commits `a409da3ab` (mirror + reflected stars + run-into-lakes + tiny-creeks-dropped) + `c11642e01` (meander-smooth); planet water branch (render/river_surfaces), [water] star_reflect/star_density dials; Fable BEFORE+AFTER; gate 380/380 goldens-0 | "rivers look good" (2026-07-12) |
| S4 building massing | DONE-in-effect (Chad flew every refinement live 2026-07-11); tags `world-s4-buildings`/`-gableroofs`/`-homefill`; dark roofs + triangular gables + 35k buildings (home-fill) | flown live (roofs, home-fill) |
| S4 night lighting (window lights + street lamps) | DONE — Chad flew to night 2026-07-11: "fantastic"; tags `world-s4-windows`/`-lamps` (+ `bf4a36e81`/`dc4dc79ce` fly-tunes); Fable-AFTER on both | "fantastic" (2026-07-11) |
| S4 lamp refinements (residential coverage + RMB-zoom floor) | AWAITING-FLY (2026-07-11); tag `world-s4-lampfix`; lamps 13493→44873 (ALL residential streets, 20-40 km ring 788→16925); the min-px floor now referenced to the RESTING fov so lamps magnify under gunsight zoom instead of washing out | — |
| S4 lamp refinements 2 (bright-up-close core + roadside) | AWAITING-FLY (2026-07-12); tag `world-s4-lampfix2`; two-part glow (soft halo + tight hot CORE) reads as a lit bulb up close w/o oversaturation; lamps offset to the ROADSIDE (alternating), not the centerline; dials core_bright/core_sharp | — |
| S4 lamp SPACING (too many → 45 m) | AWAITING-FLY (2026-07-12); tag `world-s4-lampspace`; Chad flew: "too many lights on roads"; researched residential 30-45 m; `LAMP_SPACING_M` 34→45 m (a bake constant — re-bake to change) | "too many lights on roads" (2026-07-12) |
| S5a hero landmarks (Superstack + Laurentian, procedural) | AWAITING-FLY (2026-07-11); tag `world-s5a-heroes`; Fable-AFTER (hero-batch bound P1 folded); Superstack reads great in smoke | — |
| S5b Science North (snowflake glTF) | DEFERRED (Chad "defer it", re-confirmed 2026-07-12) — sourced-glTF needs Blender running (addon not connected); on the DEFERRED WORKLIST | — |
| S5c Chelmsford French Catholic church, Errington Ave | AWAITING-FLY (2026-07-13); tag `world-s5c-church`; Blender-built glTF via NEW S5b hero-loader (`blender-hero-forge` skill); Fable BEFORE+AFTER both SOUND; gate 413/413, 0 goldens moved; evidence block below | — |
| S5d Big Nickel (Dynamic Earth, ~46.4693,−81.0207) | DEFERRED-WORKLIST (Chad NEW 2026-07-12) — procedural hero like the Superstack (vertical coin/disc on a plinth), mono steel, add to sudbury_hero.py | — |
| CC-ext lengthen the slag hill (ridge) | DEFERRED-WORKLIST (Chad 2026-07-12) — grow build_mound_mesh into a slag ridge/mountain along the dump edge, pour face NW | — |
| CC-ext rail + trains that DUMP the pots | DEFERRED-WORKLIST (Chad 2026-07-12) — CC2 loco+pots run to the dump edge + TIP to feed the pour, synced to the pour phase | — |
| S6 DoF (far-field only) | Chad flew it 2026-07-12: "the DOF looks great" (effectively approved; dials `[dof]`); tag `world-s6-dof`; Fable BEFORE+AFTER | "the DOF looks great" (2026-07-12) |
| S6 polish (remaining) | AVAILABLE — weather-dynamics (winter groomer lines), dither/halftone mode, density staging | — |

## THE PINNED CHELMSFORD SMOKE VIEWPOINT (canonical — so per-stage shots are comparable)
Every stage's `--smoke` certification uses the SAME viewpoint so two shots differ only by the stage's
change. It is the env-gated `SEADS_OBLIQUE` aerial review cam over the basin center (which becomes the
Sudbury–Chelmsford midpoint after S0 recenters). **Run via the Bash tool** (env-prefix syntax; on the
PowerShell-primary box use `$env:SEADS_OBLIQUE=1; …` instead). Substitute `{N}`/`{mechanism}` per
stage; the fixed VALUES (UP/BACK/frames/offset) are what's reused verbatim:

```
SEADS_OBLIQUE=1 SEADS_OBL_UP=2500 SEADS_OBL_BACK=4000 \
  ./build/seads.exe --smoke 8 shots/s{N}_{mechanism}.png 0
```
(4th `--smoke` arg = celestial-time offset in s; `0` = the pinned midday cell. Use a dusk offset only
for S5's silhouette gate, recorded in that evidence block. Until S0 finalizes it, this pin is PROVISIONAL.)

**S0 FINALIZED this pin (2026-07-09):** after the recenter, `SEADS_OBL_UP=2500 SEADS_OBL_BACK=4000`
was confirmed to frame the midpoint basin (the oblique cam targets `+Z` = the new midpoint, so it
auto-follows the recenter) — `shots/s0_projection.png` shows the town street grid + a lake + the clean
far limb. **These are the FINAL values; every later stage reuses them verbatim.** (offset 0 renders a
DIM cell — fine for S0's geometry gate; use a brighter/dusk offset for S5's silhouette gate.)
Reminder (lessons.md): a default smoke frame may not FRAME a new element — confirm the mechanism is
on-screen, then Read the PNG back and describe the pixels.

---

## PER-STAGE EVIDENCE BLOCK — copy this template verbatim into a stage's entry below.
A stage DOES NOT EXIST until every field holds a pasted artifact. An empty field = that gate did not
run; do not advance. You may NOT write DONE — terminal state is AWAITING-FLY until Chad's dated words.

```
### S{N} — {name}   [state: IN-PROGRESS | AWAITING-FLY | DONE]
- commit:            <hash of the atomic stage commit>
- projection.lock:   <the PARAM hash embedded in this stage's placement artifacts; PROVISIONAL/LOCKED; N/A if the stage stores no dir>
- build:             <PASS/FAIL — built BEFORE the validator runs (no stale exe)>
- asset validator:   <ctest -R name + pasted PASS tail; incl. lock-hash leg + instances>1 leg (fires when an instance-buffer artifact exists)>
- shaders GLSL330:   <PASS/FAIL + any warnings>
- seam grep:         <clean / the offending line>
- smoke shot:        shots/s{N}_{mechanism}.png — <1–2 sentences describing what the pixels SHOW
                     (read the PNG back; do not just assert "looks fine")>
- flight ctest:      <count e.g. 346/346>  · goldens moved: 0 <evidence via `git diff --stat` on the golden dir, not asserted>
- Fable red-team:    <verdict + P0/P1 list + how each was folded, or "none">
- fallback/flags:    <e.g. depth-FBO used vs aerial-perspective fallback — the logged decision, if any>
- Chad fly (dated):  <his pasted words, or "AWAITING">
```

## EVIDENCE LEDGER (newest last)

### S1-M2 — object-space NORMAL MAP prototype (crisp slope shading)   [state: AWAITING-FLY]
Brought forward from S1 at Chad's go-ahead (crisp terrain lever). Proves the mechanism on the on-disk
16 m data; the LiDAR (1 m) core is now a BAKE-ONLY swap (no code) once the throttled COG read clears.
- commit:            (this atomic commit; tag `world-s1-m2-normalmap`)
- what:              `assets/sudbury_normal.png` (8192, object-space planet-local normals from the
                     UNBLURRED elevation) → a 2nd cubemap sampled by `fragDir`; the planet FS replaces
                     the ~100 m mesh normal in the LAND diffuse (`ndl`/`ndlMoon`) with it; water keeps its
                     radial mirror normal. `uHasNormalMap` → Earth falls back to the mesh normal.
- Fable BEFORE:      SOUND-WITH-FIXES, folded — sphere-side finite difference through the SHARED inverse
                     map (P0: no grid→sphere azimuth rotation; EPSG:3979 LCC is ~12° off true north);
                     per-texel min-|dot| tangent frame (non-degenerate at the hero center; N is
                     frame-invariant); drop R/r; REPLACE not combine; N=d on the dilated water mask; dither.
- Fable AFTER:       SOUND-WITH-FIXES, folded — P1-1 `glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS)` (both
                     cubemaps); P1-2 FD step = the OUTPUT texel arc (~11.5 m, not source res → correct at
                     any source incl. the LiDAR swap) + prefilter finer sources + dilation sized to the step.
- bake:              8192×4096, sphere-side FD 11.5 m texel step / 16 m source; slope median 6.0° p95 29.2°.
- build/ctest:       build PASS; flight ctest 348/348, goldens moved 0. render/ FS compiles+renders (the
                     smoke shot IS the shader-linkage check — the green gate is blind to the app binary).
- smoke + A/B:       `shots/m2_final176.png` (raking sun, offset 176, low oblique cam) — lit terrain reads
                     with fine slope tooth, no seam/artifact. A/B via `SEADS_NO_M2=1`
                     (`shots/m2_off176.png`): M2-off is visibly SMOOTHER → M2 adds crisp slope shading (NOT
                     a no-op). Effect is SUBTLE at 16 m on gentle Shield; LiDAR is where it pops.
- flags:             `SEADS_NO_M2=1` = mesh-normal A/B toggle (smoke/debug, seam-safe env gate).
- Chad fly (dated):  ACCEPTED (rode along) — Chad flew the raking-sun cell during the S1 tone lock
                     (2026-07-10) and ruled the whole look "perfect", M2 active. No dedicated isolated
                     `SEADS_NO_M2` A/B was called out; the 16 m normal map stands as-is. The LiDAR-core
                     (1 m) swap remains a deferred bake-only upgrade (network-gated) — not re-opened here.


### S0-REV — FILL THE SPHERE (real everywhere; supersedes the cap version)   [state: DONE]
- commit:            `68fdf4521` (bake) + `59ee44fe1` (approval); lock→LOCKED follow-through this commit.
                     SUPERSEDES the cap version `b86a5bb8b` / tag `world-s0-projection-rebake` (in history).
- projection.lock:   `0xB3BD05484DA34397` **LOCKED** (2026-07-10; hash unchanged by the status flip —
                     re-gate 348/348 + asset-validator 15/15 green proves it) · params = `proj=aeqd datum=sphere_R6371000
                     axis=+Z basis=E+X_N+Y center_lat=46.556000 center_lon=-81.110000 GROUND_R=15000
                     R_MAX=47124` (derived cap_deg=180 / k_t_edge=0 / coverage=100% UNHASHED). Fill the
                     sphere: real data to the antipode, pinch accepted (Chad). `FILL_SPHERE=True`.
- build:             PASS — seads.exe + seads_tests.exe linked (MinGW GCC + Ninja, Debug, assert-live)
- asset validator:   PASS 18/18 (`ctest -R asset-validator`). NEW #25 "projection.lock fingerprint
                     matches the baked header" (FNV-1a-64 of the lock `params=` line == kSudburyProjectionLockHash
                     == header self-hash == lock `hash=` field) + #26 "projection probe is a unit dir,
                     east of center" (bake-derived downtown dir, chirality). instances>1 leg deferred to
                     S2's instance buffer. gis 3/3 PASS.
- shaders GLSL330:   N/A — S0 is offline-bake only; `render/` untouched (projection-agnostic seam holds)
- seam grep:         clean — `grep -rE "GROUND_R|aeqd|R_MAX|CAPTURE_RADIUS" render/` = only
                     `render/sudbury_gis.gen.h` (generated DATA, exempt); no projection math in `render/`
- smoke shot:        `shots/s0fill_projection.png` (SEADS_OBLIQUE=1 UP=2500 BACK=4000, `--smoke 8`, offset 0)
                     — the little-planet limb curves against near-black space (aurora above); below,
                     Greater Sudbury reads as a legible STREET GRID + urban blocks at TRUE scale (roads
                     paint out to 40 km) with a near-black lake right-of-frame; the far field is now REAL
                     terrain fading cleanly to the limb. The antipodal PINCH is on the hidden far side (not
                     in frame from the hero viewpoint) — accepted. (`shots/s0_projection.png` = the prior
                     cap version for comparison.) Scene dim (offset-0 cell; B&W tone is S1's job).
- flight ctest:      348/348 · **goldens moved: 0** (`git diff --stat test/` = only the 2 test files:
                     test_asset_validator +80, test_sudbury_gis +6/−1; no golden data touched)
- Fable red-team:    The cap version was Fable BEFORE+AFTER vetted (SOUND-WITH-FIXES, all P0/P1 folded:
                     lock hashes proj/datum/axis/basis, end-to-end k_r + chirality selftest, CRLF-proof FNV,
                     bake-derived probe — ALL retained here). S0-REV is a Chad-directed CONFIG FLIP on that
                     already-vetted aeqd math: R_MAX→π·R, `FILL_SPHERE` skips the cap, the striation-death
                     guards are INTENTIONALLY voided (θ_edge=π, k_t_edge=0 asserted instead). No new complex
                     mechanism → no fresh Fable round (it would only confirm "filling pinches, by design").
                     The cap code (histogram-match/seam-stat, Fable-vetted) is inert but retained for the
                     `FILL_SPHERE=False` fallback. DEFERRED: `GisLake in_cap` flag — moot now (all lakes real).
- fallback/flags:    **DEM = 16 m CDEM-only** (`SEADS_SOURCE_RES=16 SEADS_CDEM_ONLY=1`) — the fill window
                     grew to 48 km (`SOURCE_HALF_M`) to reach the antipode, and the throttled 1 m HRDEM COG
                     read stalls, so the whole grid comes from the 16 m national CDEM (uniform provisional;
                     the CDEM read completed fast where HRDEM stalled). Final asset stays 8192/11.5 m
                     (Ruling C). The LiDAR-core + 4 m upgrade is a deferred transparent swap (re-run without
                     the env flags once the COG read is healthy — lock hash is DEM-res-independent).
                     Committed config = 4 m / HRDEM-blend / `FILL_SPHERE=True`.
- Chad fly (dated):  **APPROVED 2026-07-10 — Chad flew the filled-sphere map and ruled "it beautiful."**
                     S0 is signed off. Next agent's first step: flip `projection.lock` PROVISIONAL→LOCKED,
                     re-gate, mark S0 DONE, then start S1. (Fly criteria that passed: Chelmsford/downtown
                     at true size + the far REAL lakes; whole planet is real Sudbury edge to edge.)


### S1 — LOCK THE LOOK: B&W stereoscope post stack + depth FBO   [state: AWAITING-FLY]
- commit:            (this atomic commit; tag `world-s1-poststack`). Design + both Fable rounds =
                     `docs/stereoscope_s1_plan.md`.
- projection.lock:   N/A — the post pass stores NO baked sphere direction (it consumes pure fragTexCoord
                     + the depth FBO); projection-agnostic, so the lock hash does not gate it.
- build:             PASS — seads.exe + seads_tests.exe linked fresh (rm'd first; MinGW GCC + Ninja,
                     Debug, assert-live). New sources: render/post.cpp (app target) + render/post_glsl.cpp
                     (seads_render_core, so the headless validator gates it).
- asset validator:   PASS 16/16 (`ctest -R asset-validator`). NEW leg "post-process shader uniforms are
                     exactly the allowlist" — the standalone post FS declares 20 uniforms; exact-match +
                     no time/clock token = the grain-seed backdoor guard (uFrameCount is the ONLY time-
                     like input, an app-owned ordinal). instances>1 leg still inert (no instance buffer).
- shaders GLSL330:   PASS — kPostFS compiles + links (the smoke shots ARE the linkage check; the green
                     gate is blind to the app binary). Pairs with raylib's default fullscreen VS (uses
                     only fragTexCoord/fragColor it emits — no undefined-FS-input trap).
- seam grep:         clean — no clock read (`GetTime|GetFrameTime|std::chrono|clock(|time(`) in
                     render/post.*; no lat/lon/heading/fixed-axis; grain seed = the app-owned frame
                     counter passed as a param (render/ never reads a clock). n/f single-sourced from
                     rlGetCullDistanceNear/Far (the same clip planes the scene projection uses).
- smoke shots:       (pinned Chelmsford oblique cam, UP=2500 BACK=4000, --smoke 8; NOTE raylib
                     TakeScreenshot strips the dir → shots land at repo root, moved into shots/)
                     · `shots/s1_poststack.png` (offset 0, dim cell) — the silver-print planet: street
                       grid + lake read mono, near-black space, HUD crisp/RAW on top (not grained →
                       §9.2 holds). AURORA shows GREEN/MAGENTA chroma = the scene-mono gap (below).
                     · `shots/s1_lit_on.png` vs `shots/s1_lit_off.png` (offset 176, raking sun) — the
                       A/B: post-ON has deeper blacks, a punchier silver street grid, warm-silver
                       highlights, and the vignette "print frame"; post-OFF is flat + brighter + no
                       vignette. The pass does real, visible work (NOT a no-op).
                     · `shots/s1_depth.png` (SEADS_POST_DEBUG=1) — near terrain BLACK, sky WHITE (d=far),
                       smooth limb: PROVES the depth-texture FBO is alive + correct (locks depth for S6).
- flight ctest:      349/349 · **goldens moved: 0** (`git diff --stat` = only source/config/test/docs;
                     no golden data dir touched). (348 prior + 1 new validator TEST_CASE = 349.)
- Fable red-team:    BEFORE (design) + AFTER (landed) both SOUND-WITH-FIXES, no P0. BEFORE folded: no
                     ACES (display-space sigmoid), drop post-haze (double-count), invProj-radial for S6,
                     RGBA16F, real depth texture, animated grain, order FXAA→S-curve→split→halation→
                     vignette→grain. AFTER folded: P1-1 FBO-failure lifecycle (per-size latch, no
                     per-frame retry/leak, deleted dead post_enabled); P2-1 clamp silver; P2-2 contrast
                     cap 8; P2-3 grain seed %1024; P2-4 hoist depth-viz; P2-6 unload_post at shutdown.
                     P2-5 (halation radius→dial) DEFERRED (Fable: borderline/structural). Re-gated green.
- fallback/flags:    `SEADS_NO_POST=1` = A/B bypass; if the RGBA16F FBO can't build, the pass DISABLES
                     itself (logged TraceLog, latched — never silent, never per-frame) and the scene
                     renders raw to the backbuffer. `SEADS_POST_DEBUG=1` depth viz / `=2` sat mask.
                     Depth-FBO path used (real texture); NO aerial-perspective fallback taken (dropped by
                     design — haze stays single-sourced in the scene).
- Chad fly (dated):  **DONE — Chad flew 2026-07-10 (raking cell over Chelmsford, clean tone-lock
                     worktree build): "the tone is perfect."** Tone LOCKED at the current committed
                     `config/world.toml [tone]` values AS-IS — no dial changes requested. This UNBLOCKS
                     the S2–S6 ladder. Still-open sub-item (Chad opt-in, not blocking): author the AURORA
                     MONO to complete the B&W world (scene-authoring = celestial thread, not the post).


### WATER — lake shapes: ring-assembly fix + dedicated mirror surfaces   [state: DONE — Chad 2026-07-10]
- commit:            (this atomic commit). Plan + both Fable rounds = `docs/stereoscope_water_plan.md`.
- projection.lock:   `0x99061E1583D34607` PROVISIONAL (params UNCHANGED — the water dirs embed + are
                     gated by this same hash; the bake refreshed only bake_commit). asset-validator
                     lock-hash leg green ⇒ the emitted sphere dirs are lock-consistent.
- build:             PASS — seads.exe + seads_tests.exe linked fresh (rm'd first; MinGW GCC + Ninja).
                     New sources: render/water_surface.cpp (app target); offline_tool/sudbury_water.py.
- asset validator:   PASS — NEW leg "dedicated water surfaces are well-formed meshes" (test_sudbury_gis):
                     per-lake unit dirs + LOCAL index-in-range + **outward winding** (dot(cross,centroid)
                     >0, the Fable ★2 check) + ushort cap + a named hero present; fires because lakes
                     exist (inert if the bake emits none). 28338 assertions pass. Full asset-validator
                     green (lock-hash leg unaffected — params unchanged).
- shaders GLSL330:   PASS — the planet FS gained `uniform float uForceWater` + `water = uHasWater *
                     max(texel.a, uForceWater)`; the water pass reuses the SAME program (the smoke shots
                     ARE the linkage check — the green gate is blind to the app binary).
- seam grep:         clean — render/water_surface.* has no clock read (GetTime|GetFrameTime|chrono|
                     clock(|time(), no lat/lon/heading, no sim/ write. Projection math stays in the
                     offline bake (sudbury_water.py); the runtime consumes pure baked dirs.
- smoke shots:       (pinned Chelmsford oblique cam framing Whitewater Lake, `--smoke 8`, offset 176)
                     · `shots/sW_whitewater_on.png` vs `_off.png` (UP=2500 BACK=4000) — the lake reads
                       as clean dark mirror water; its lower shoreline is SMOOTHER with water ON (the
                       coarse-mesh facets covered). A/B changed 9.8k px localized to the lake.
                     · `shots/crop_final_on.png` vs `crop_final_off.png` (grazing UP=1500 BACK=5200,
                       magnified) — the KEY proof: OFF has a BRIGHT faceted "cover strip" along the far
                       shore (Chad's complaint); ON replaces it with clean dark mirror water to the
                       shore. Lake-region mean brightness 66 (OFF) → 18 (ON); roads intact; no z-fight
                       speckle, no water poking above land, no sliver gaps.
- flight ctest:      **350/350** · **goldens moved: 0** (`git status` = only source/config/test/docs/
                     gen.h/lock/shots; no golden data dir touched). (349 prior + 1 new water TEST_CASE.)
- Fable red-team:    BEFORE (design) + AFTER (landed) both SOUND-WITH-FIXES. BEFORE folded: circumcenter
                     sag g²/(4R) → grid 250 m / lift 2 m + bake assert; constrained-ish triangulation
                     (densified rings + centroid-in-poly); REUSE the planet program (uForceWater) instead
                     of a separate shader (kills the aurora+sky_aerial+uniform fork); slope-scaled
                     glPolygonOffset. AFTER folded P1: `has_water<0.5` guard so a degraded landmask can't
                     render lakes-as-land; getenv cached. NOT folded (would regress): tighten gate to
                     WILD_FADE_IN — drops Wanapitei (Chad's target, ρ=37.8 km in the fade). Extra fix
                     during build: reject ρ>R_MAX lakes (aeqd fold past the disk → flipped winding, e.g.
                     Nipissing); winding decided by the 3D outward dot + sliver drop |dot|<1e-7 + 9-dec
                     dirs so the validator's strict >0 can't trip on truncation.
- fallback/flags:    `SEADS_NO_WATER=1` = A/B bypass (read once). No water over a planet without the
                     Sudbury landmask (`has_water<0.5` → no draw). Dials: `[water] surface_lift_m` +
                     `offline_tool/sudbury_config.WATER_{SURFACE_MIN_SPAN_M,MAX_LAKES,INTERIOR_GRID_M,
                     OUTLINE_SIMPLIFY_M}`.
- Chad fly (dated):  **DONE — Chad approved 2026-07-10: "okay that is really good now."** Covers BOTH the
                     dedicated surfaces (`5878dcd81`) AND the ring-assembly root-cause fix (`92d9d2062`,
                     the decisive one — his "far side of Whitewater is straight all the length" clue).
                     Deferred (not blocking): more-lakes coverage / mirror darkness (`[water] reflectivity`)
                     / Kelly near-duplicate disambiguation — all candidate follow-ups, none requested.


### WATER-DEDUP — named-lake dedup prefers the in-disk instance (folded-duplicate fix)   [state: AWAITING-FLY]
- commit:            (this atomic commit). Data-curation follow-up to the WATER DONE above.
- what:              `sudbury_header.py` dedup for duplicate lake NAMES changed from largest-area-only to
                     `sort key (rho_rep <= R_MAX, area) desc` — prefer an IN-DISK (renderable) instance,
                     then largest. The old key resolved a name to a FAR duplicate that folds past the aeqd
                     disk (ρ>R_MAX → garbage off-map label + no surface, since water rejects ρ>R_MAX).
- data probe:       cached `source/water_raw.json`, 16182 polys. Kelly was ALREADY correct (near 12.9 km,
                     3.38 M m² > far 0.63 M m²). Real defect = 8 names to a folded duplicate; all 8 now
                     resolve near-Sudbury: Hannah 54.4→13.7 km, Crooked 40.4→16.0, Ella 42.4→25.6,
                     Pine 44.3→21.0, Bell 46.4→41.6, + Beaver/McLaren/Spanish. Hero/landable lakes: NONE
                     changed (no regression). Bonus: the `lakes_out[:250]` cap had been dropping near hero
                     lakes Minnow/Robinson/Bethel (out-massed by far lakes) — in-disk-first restores them.
- projection.lock:  `0x99061E1583D34607` PROVISIONAL — UNCHANGED (params identical; only `bake_commit`
                     line bumped). asset-validator lock-hash leg #27 green ⇒ downstream stays consistent.
- build:            PASS — seads.exe + seads_tests.exe relinked fresh (rm'd first; MinGW GCC + Ninja).
- asset validator:  PASS 20/20 (`ctest -R asset-validator`) incl. #27 projection.lock fingerprint +
                    the water-mesh leg (unit dirs / local index range / outward winding, 4 new surfaces).
- shaders GLSL330:  N/A — no render/shader code touched (water_surface.cpp + planet FS byte-identical to
                    the approved WATER DONE state; only gen.h DATA + offline python changed).
- seam grep:        clean — `sudbury_header.py` adds only `_rho_rep` (representative_point + hypot) and the
                    sort key; no clock/lat/lon/heading; projection math stays in the offline bake.
- smoke shot:       `shots/kelly_dedup_check.png` (pinned Chelmsford oblique, UP=2500 BACK=4000, offset 0)
                    — limb vs near-black space, aurora above, Sudbury street grid legible below, large
                    near-black lake right-of-frame reads as clean dark water, no z-fight/artifact. Matches
                    the approved S0/water look (the 4 new surfaces are near-Sudbury lakes off this fixed
                    frame; the check is that nothing regressed).
- PNGs:             bit-identical (deterministic 16 m re-bake) — `git status` shows only sudbury_header.py,
                    render/sudbury_gis.gen.h, assets/projection.lock (+ the parallel agent's own files).
- water surfaces:   14→17 (per bake print; committed gen.h had 13). +4 (Armstrong 37 / Nelson 19 /
                    Wavy 29 / White Oak 30 km); 0 removed; all 13 prior retained.
- flight ctest:     350/350 · **goldens moved: 0** (`git diff --stat -- test/` = empty; no golden/test
                    file changed).
- Fable red-team:   none — NOT a ★ math boundary (reuses `representative_point` + the existing R_MAX bound
                    and the already-Fable-vetted water-mesh winding math; this is a data-selection ORDER
                    fix, not a new mechanism, so no fresh round per the skill).
- Chad fly (dated): **AWAITING** — optional; low-risk data polish. Fly near Hannah/Crooked/Pine or the new
                    surfaces (Nelson 19 km, Wavy 29 km) if you want to confirm labels/surfaces read right.


### S2 — boreal trees: hash-Poisson instanced cross-quad scatter   [state: DONE — Chad 2026-07-10]
The "populated world" leap. Deterministic hash-Poisson scatter (world/props.* PURE + render/props.*
raylib draw) against a new baked density raster; instanced mono cross-quads sitting on the planet's own
height field. Fable-BEFORE + AFTER both SOUND-WITH-FIXES.
- commit:            (this atomic commit; tag `world-s2-trees`).
- projection.lock:   `0x99061E1583D34607` PROVISIONAL — UNCHANGED (params identical; the density raster
                     rode this same deterministic bake, only `bake_commit` bumped). The raster is a
                     projection-baked placement INPUT consumed by equirect_uv(dir) (like albedo/normal),
                     so the existing lock-hash leg gates it; asset-validator green ⇒ lock-consistent.
- build:             PASS — seads.exe + seads_tests.exe linked fresh (rm'd first; MinGW GCC + Ninja,
                     Debug, assert-live). NEW: world/props.cpp (→ seads_render_core, so the headless
                     validator pins the PLACEMENT math), render/props.cpp (→ seads app target, the draw).
- asset validator:   PASS 18/18 (`ctest -R asset-validator`). TWO new legs, keyed off the density raster:
                     (1) "tree-density raster is present with the header dims" — reads the PNG IHDR,
                     asserts 8192×4096 == kSudburyTreeDensityW/H (the instance-buffer-artifact-present
                     trigger); (2) "tree placement fires (>1), gates on density, never floats" — calls
                     world::place_chunk on a synthetic DENSE field → REQUIRE size>1 (the count==1
                     tripwire) + every instance on radius_at(dir) with up==dir (anti-float) + a ZERO
                     field → 0 (not a no-op) + determinism (two calls identical) + the cull returns a
                     bounded subset. 999+ assertions.
- shaders GLSL330:   PASS — the instanced props VS/FS compile+link (`GLSL 3.30 NVIDIA`; the --smoke run IS
                     the linkage check, the green gate is blind to the app binary). Uses raylib's
                     DrawMeshInstanced attrib binding (instanceTransform + mvp); scene MONO so the S1 post
                     tones it silver (planes stay the only chroma).
- seam grep:         clean — no clock read (GetTime|GetFrameTime|chrono|clock(|time() in world/render
                     props, no lat/lon/heading coordinate use (the grep hits are equirect "wrap
                     longitude"/"clamp latitude" comments only), no sim/ write. Placement is a PURE fn of
                     (cell id, fixed seed) — no camera/clock → zero shimmer. render draw is eye-relative
                     (the planet's double→float seam). Projection math stays in the offline bake.
- smoke shots:       (pinned Chelmsford oblique cam, UP=2500 BACK=4000, `--smoke 8`, raking offset 176)
                     · `shots/s2_trees.png` vs `shots/s2_trees_off.png` (A/B via SEADS_NO_TREES=1) — ON
                       shows a dense stippled boreal canopy across the land, with the town STREET GRID and
                       the lake cleanly PUNCHED OUT (exclusion works) and the canopy following the terrain
                       relief (anti-float); OFF is the bare grey terrain. The pass does real, visible work
                       (NOT a no-op). Log: `PROPS: 320488 trees over 14 visible chunks` — the instanced
                       mechanism fired (320k instances in ONE draw call, under the ≤24-call budget).
- flight ctest:      **354/354** · **goldens moved: 0** (`git status` = only source/config/test/docs/
                     gen.h/lock/toml + the new png; `git diff --stat -- test/` = only
                     test_asset_validator.cpp +85; no golden data dir touched). (350 prior + water-dedup
                     didn't add a case; +2 new asset-validator TEST_CASEs + 2 discovered = 354.)
- Fable red-team:    BEFORE (design) + AFTER (landed) both SOUND-WITH-FIXES, no P0. BEFORE folded (P1):
                     triple32 nested hash + decorrelated stream seeds; Jacobian (1+s²)(1+t²)/(1+s²+t²)^1.5
                     acceptance correction; per-chunk bounding-sphere/proximity+horizon cull with reach =
                     min(range, R·acos(R/(R+a)) + R·√(2·h_top/R)) — the √ horizon term, not h/R; ONE merged
                     instance buffer (K=256 cells/chunk, chunks = cache/cull units), species per-instance;
                     full-cell jitter; VS scale-fade. AFTER (landed, numerically verified) folded: P1-1
                     precompute the rotation·scale columns once per set-change (per-frame path = only the
                     translation, kills a 15–30 ms/frame rebuild at 320k); P1-2 evict-not-in-visible cache
                     bound (GO-ANYWHERE memory). P2s folded: drop the local warp() fork (use render::warp);
                     ceil-divide chunks_per_face (no treeless seam); fade_frac==0 smoothstep guard; gain>1
                     face-bias warning; normal comment-guard. Not folded (P2, Chad-eyes): backlit trees
                     read pure-ambient — revisit if patchy.
- fallback/flags:    `SEADS_NO_TREES=1` = A/B bypass (read once). `[trees] enabled=0` = treeless (pre-S2).
                     Density raster MISSING → trees disabled + a logged TraceLog WARNING (degraded, never
                     a silent crash). Dials (`config/world.toml [trees]`): density_gain (the moonscape
                     fly-dial), min/max_scale, height_m, base_width_m, render_range_m, ambient, diffuse,
                     fade_frac; structural: cells_per_face 1280 (~18 m), chunk_cells 256 (G=5). Bake dials:
                     `offline_tool/sudbury_config.TREE_*`.
- PERF FIX (2026-07-10, Chad flew "beautiful but CPU struggling / not using VRAM"): the draw path was
                     re-uploading the WHOLE instance buffer every frame (raylib DrawMeshInstanced re-creates
                     + re-streams the VBO per call — ~20 MB/frame at 320k) + rebuilding 320k matrices CPU-
                     side each frame to rebase to the moving eye → CPU-bound, GPU starved. Replaced with a
                     PERSISTENT VRAM instance buffer (rlLoadVertexBuffer once/area, rlUpdateVertexBuffer on
                     area change) + the per-frame camera rebase moved to the VERTEX SHADER via a single
                     `uEyeRel` uniform (transforms are anchor-relative so magnitudes stay small = sub-mm
                     float precision). Per frame is now ~5 uniforms + one instanced draw; the buffer sits in
                     VRAM. Pixel-identical output (`shots/s2_vram.png` byte-identical to `s2_trees.png`),
                     gate green 354/354. Manual rlgl draw (rlDrawVertexArrayElementsInstanced) since
                     DrawMeshInstanced can't keep a persistent buffer.
                     PERF FIX 2 (Chad re-flew: "better but skipped a few times"): the FIRST fix still
                     rebuilt+re-uploaded ALL 320k transforms on every visible-set change (anchor moved each
                     time → recompute) — a per-boundary-crossing spike = the skips. Fixed: cache each
                     chunk's WORLD-ABSOLUTE float16 buffer, built ONCE when the chunk first comes into view
                     and reused forever (camera motion never recomputes it — the VS rebase is now the full
                     world `uEye`); a boundary crossing only places the FEW NEW chunks + concatenates cached
                     bytes. Plus AMORTIZE: place at most `kMaxChunkBuildsPerFrame=2` new chunks/frame so a
                     multi-chunk crossing streams over a couple frames (trees fade in → invisible). Precision
                     trades anchor-relative sub-mm for world-abs ~4 mm (float32 at 15 km) — invisible on a
                     16 m tree. Gate green 354/354, full canopy renders. AWAITING Chad's re-fly.
- Chad fly (dated):  **DONE — Chad flew 2026-07-10: "its good"** (look ruled "beautiful" earlier the same
                     day; the two perf passes cleared the CPU stutter). S2 signed off at the committed
                     dials (density_gain 0.85, ambient 0.16). Deferred (NOT requested, next-agent opt-in):
                     S2.1 Blender tree kit (swap the procedural cross-quad for authored spruce/pine/birch
                     glTF, validator-gated); a background-thread placement build IF a fast boundary crossing
                     ever hitches again (world-abs cache + 2-chunks/frame amortize was enough for Chad).


### S3 — draped LINEWORK ribbons: roads + snowmobile trails   [state: AWAITING-FLY]
The "roads you fly along" leap. OSM polylines → draped triangle-strip ribbons on the planet's own height
field, replacing the rasterized albedo road-lines. Plan + both Fable rounds: `docs/stereoscope_s3_plan.md`.
- commit:            (this atomic commit; tag `world-s3-ribbons`).
- projection.lock:   `0x99061E1583D34607` PROVISIONAL — UNCHANGED (params identical; the ribbon dirs
                     embed + are gated by this same hash; the bake bumped only bake_commit). asset-validator
                     lock-hash leg #38 green ⇒ the emitted ribbon dirs are lock-consistent.
- what:              `offline_tool/sudbury_ribbon.py` tessellates each polyline: aeqd-plane miter offset
                     (constant-width, MITER_LIMIT 2.0 + fold-back clamp) → project each edge via
                     `aeqd_to_dir`; cumulative GREAT-CIRCLE arc-length `s` (chord form, double) for the FS
                     dash; transverse `v`=−1(L)/+1(R). `sudbury_header._build_ribbon_batches` bins ways
                     per-KIND into <60000-vert ushort batches → `GisRibbonVertex{dir,s,v}` /
                     `GisRibbonPath{kind,ranges}` in gen.h (4 batches: 1 major / 2 minor / 1 trail; 101970
                     verts / 88878 tris). `render/ribbons.{h,cpp}` builds one draped mesh per batch at
                     `radius_at(dir)+2 m` (the SAME `g_planet.height` the terrain mesh + trees drape on),
                     texcoords=(s,v); custom `kRibbonVS/FS` — dark bed + duty-cycle dashed centerline
                     (roads, mono), light-green dashes (trail, the sanctioned chroma). Drawn after
                     `draw_water`, before `draw_trees`. Roads REMOVED from the albedo bake (no double roads).
- build:             PASS — seads.exe + seads_tests.exe linked fresh (rm'd first; MinGW GCC + Ninja, Debug,
                     assert-live). NEW source: render/ribbons.cpp (seads app target — uses raylib; the
                     validator gates the gen.h DATA, not the draw).
- asset validator:   PASS 18/18 (`ctest -R asset-validator`) incl. #38 projection.lock fingerprint
                     (UNCHANGED hash ⇒ downstream lock-consistent). NEW leg (test_sudbury_gis) "draped
                     ribbon paths are well-formed strips": per-path kind∈{0,1,2}, LOCAL ushort index range,
                     unit dirs, v=±1, s≥0 finite, OUTWARD winding (dot(cross,centroid)>0 — the Fable ★
                     check on every one of the 88878 tris), arc-length ACCUMULATES (smax>0, <π·R — the
                     fixture-no-op guard from Fable-AFTER P2), and a trail (kind 2) is present.
- shaders GLSL330:   PASS — kRibbonVS/FS compile+link (the smoke shots ARE the linkage check; the green
                     gate is blind to the app binary). Uses raylib's auto-set `mvp` + vertexTexCoord attrib.
- seam grep:         clean — render/ribbons.* has no clock read (GetTime|GetFrameTime|chrono|clock(|time(),
                     no lat/lon/heading, no sim/ write. Projection math stays in the offline bake; the
                     runtime consumes pure baked dirs + arc-length. Eye-relative draw (MatrixTranslate(-eye),
                     the planet's double→float seam).
- smoke shots:       (pinned Chelmsford oblique cam, `--smoke 8`; re-rendered on the COMMITTED terrain)
                     · `shots/s3_ribbons_low.png` (UP=700 BACK=2600, offset 176, low grazing) — the road
                       network reads as a legible STREET GRID draped on the relief: urban blocks, a diagonal
                       arterial, roads tracing the lake shore; roads follow the terrain (not floating), no
                       z-fight speckle. The KEY proof the mechanism works at low flight.
                     · `shots/s3_ribbons.png` vs `shots/s3_ribbons_off.png` (UP=2500 BACK=4000, offset 0,
                       A/B via SEADS_NO_RIBBONS) — ON shows the grid road-lines; OFF has only the urban
                       albedo patches + trees, NO grid (the roads come ENTIRELY from the new geometry — the
                       albedo road-removal + ribbon replacement both landed). `shots/s3_ribbons_raking.png`
                       (offset 176) = the grid at raking light.
                     · Green trail verified by a differential pixel probe (terrain region, excl. aurora):
                       ON shots carry markedly more + stronger green-dominant px (2580/2033, max G−R 114/117)
                       than OFF (1184, G−R 82) — the light-green snowmobile accent renders. It is thin +
                       peripheral to the town core, so not dominant in a town-centered frame → Chad confirms
                       by flying out to a trail.
- flight ctest:      **365/365** · **goldens moved: 0** (`git diff --stat -- test/` = only
                     test_sudbury_gis.cpp +≈70, my ribbon leg; no golden data dir touched).
- assets:            TERRAIN PRESERVED — `sudbury_{dem,landmask,normal,treedensity}.png` restored
                     BYTE-IDENTICAL to HEAD (587f1f00f, Chad-approved) so S3 is a clean, attributable
                     roads→ribbons change (a fresh full bake drifts the terrain by imperceptible LSB
                     resampling noise — dem meanΔ 1.39/255, normal 0.63/255; kept out). Only
                     `sudbury_color.png` changed (roads removed from the albedo + the same-pipeline
                     shore/water value refresh Chad blessed in 587) + gen.h (ribbons) + projection.lock
                     (bake_commit bump, hash unchanged).
- Fable red-team:    BEFORE (design) SOUND-WITH-FIXES — all folded: P0-1 hairpin NaN guard (m=n_in,
                     k=LIMIT); P1-2 lift 2 m + glPolygonOffset(-2,-4) + station ≤80 m + drape L/R
                     independently; P1-3 duty-cycle dash; P2-1 chord-form arc-length; P2-3 MITER_LIMIT 2.0 +
                     fold-back clamp; winding confirmed outward (no swap). AFTER (landed) SOUND — all six
                     folds verified present + correct, no P0/P1; folded the one cheap P2 (the arc-length-
                     accumulates test guard). Both fresh-context, author≠red-teamer.
- fallback/flags:    `SEADS_NO_RIBBONS=1` = A/B bypass (read once). `[ribbons] enabled=0` = no ribbons.
                     Trail fetch is best-effort: an Overpass failure logs + bakes roads-only (degraded,
                     never silent) — this run fetched 176 trails clean. No paths baked → the renderer is
                     inert (logged). Dials: `[ribbons]` road_bed/road_line/trail_color, road/trail
                     dash+gap, road_center_frac, lift_m.
- look iterations:   (2026-07-11, on the AWAITING-FLY stage, Chad's art direction — config/shader only,
                     geometry unchanged) (1) centerline thin + short (`road_center_frac` 0.28→0.10,
                     `road_dash_m` 10→7). (2) WEATHERED ASPHALT: roads gained a "less perfect" look — a
                     global light fade + lighter mottled patches over ~25% of the length. (3) Chad flew (2):
                     roads "overall very good" but some sections read WASHED OUT + the snowmobile trail
                     should be a SUBTLE CLAY PATH, not the interim icy corduroy. Landed: roads TONED DOWN
                     (`road_mottle` 0.35→0.22, `road_fade` 0.25→0.18, lighten capped 0.7, target worn-grey
                     0.42 not mid-0.5 so a stacked patch can't blow out); the trail is now a **subtle smooth
                     CLAY path** (light brown/grey/yellow, `trail_color`=[0.60,0.55,0.45] + a whisper of
                     `trail_mottle`) — the **groomer/corduroy lines + sparkle are REMOVED** (they return
                     later with the WEATHER-DYNAMICS pass, as winter grooming). All variation is a
                     deterministic hash of (s,v) — no clock. Smoke `shots/s3_clay.png`: roads read subtle
                     (no washout), trail no longer blue/green (icy-blue px 8740→~0), reads as a faint clay
                     line. Dials: `[ribbons]` road_mottle/road_fade/road_mottle_frac, trail_color/trail_mottle.
- iter (2026-07-11b): Chad flew the clay pass: trail too YELLOW + a few roads still washed out + narrow the
                     trail more. Landed: (a) trail color → darker/browner/greyer `[0.40,0.34,0.30]` (less
                     yellow); (b) roads toned further — `road_mottle` 0.22→0.16 + the FS lightening cap
                     0.7→0.5 toward worn-grey 0.40 (the remaining washed-out flashes gone); (c) trail width
                     **~25% narrower** (bake: half-width 3.5→2.6 m); (d) **trail CLIPPED off road crossings**
                     — each road buffered (half-width + 4 m) + unioned, subtracted from every trail so a
                     trail never overlaps a road (cut at the crossing, resumes past it). (c)/(d) needed a
                     re-bake (gen.h only: 101970→101756 verts; terrain PNGs restored byte-identical; albedo
                     bit-identical this time). `shots/s3_iter2.png`: roads clean (no washout), trail a faint
                     narrower clay line. Fly-dials unchanged (`[ribbons]`).
- iter (2026-07-11c): Chad flew iter-2: roads STILL washed out in many sections + FLICKERING ("light
                     fading/mottling, not full erasure — keep roads continuous"); trail now visible → make
                     it BROWN; the dashed centerline still not reading thinner. Runtime-only fixes (no
                     rebake): (a) road FS reworked — the DARK asphalt stays continuous, only a light fade +
                     SMOOTH ~28 m mottle, fading toward a DARK worn grey 0.30 (was 0.40) so a peak patch
                     can't lighten into the lit terrain; the HIGH-FREQ per-texel mottle term is REMOVED (it
                     aliased → the flicker). `road_mottle` 0.16→0.14, `road_fade` 0.18→0.10. (b)
                     `trail_color` → brown earth `[0.46,0.34,0.24]`. (c) `road_center_frac` 0.10→0.06
                     (thinner — and it finally reads now the bed is dark, not washed). `shots/s3_iter3.png`:
                     roads dark + continuous (no washout), grid legible.
- FUTURE (weather):  when the weather-dynamics pass lands, RE-ADD winter groomer/corduroy lines to the trail
                     (fine black longitudinal lines across the width + optional icy sparkle) — gated on a
                     season/snow signal. The removed shader (icy-blue body + `fract(tv*grooves)` corduroy +
                     hashed sparkle) is in git history at commit `65d64df09` for reference.
- Chad fly (dated):  **DONE — Chad flew 2026-07-11: "perfect."** Signed off after the fly-tune passes
                     above (iter a/b/c + the trail darken `fc658a287`). FINAL committed `[ribbons]`: dark
                     continuous asphalt + thin dashed centerline (`road_center_frac` 0.06, `road_mottle`
                     0.14, `road_fade` 0.10); dark-brown earth trail (`trail_color` [0.37,0.27,0.19]),
                     ~25% narrower + clipped off road crossings. Winter groomer lines deferred to weather.


### S4 — building massing: extruded OSM footprints (flat/gable/hip)   [state: AWAITING-FLY]
The "town reads as blocks" leap. Real OSM footprints -> extruded flat/gabled/hipped prisms, baked as
static draped meshes; MONO sun-lit (the S1 post silvers them; planes stay the only chroma). Mirrors the
S3 ribbon machinery. Design + BOTH Fable rounds = `docs/stereoscope_s4_plan.md`.
- commit:            (this atomic commit; tag `world-s4-buildings`).
- projection.lock:   `0x99061E1583D34607` PROVISIONAL — UNCHANGED (params identical; the building dirs
                     embed + are gated by this same hash; the re-bake refreshed only bake provenance,
                     REVERTED). asset-validator lock-fingerprint leg #39 green ⇒ the emitted dirs are
                     lock-consistent (buildings are projection-baked placements).
- build:             PASS — seads.exe + seads_tests.exe linked fresh (rm'd first; MinGW GCC + Ninja,
                     Debug, assert-live). NEW: render/buildings.cpp (seads app target — raylib draw only,
                     geometry is baked); offline_tool/sudbury_building.py.
- asset validator:   PASS 24/24 (`ctest -R asset-validator`). NEW leg "building massing batches are
                     well-formed prisms" (test_sudbury_gis): per-batch unit dirs + LOCAL index range +
                     h/bj bounds + the EXTRUSION tripwire (hmax>0 AND hmin<0) + the cap/roof tripwire (≥1
                     tri with ALL verts h>0 — Fable-AFTER P1-3, pins the ear-clip/roof ran). Fires off
                     kSudburyBuildingBatchCount>0 (inert in S0–S3). lock-fingerprint leg #39 unaffected.
- shaders GLSL330:   PASS — kBuildingVS/FS compile+link (the smoke shots ARE the linkage check; the green
                     gate is blind to the app binary). FS derives a FLAT facet normal from dFdx/dFdy of
                     the EYE-RELATIVE worldPos (matModel-transformed, Fable-AFTER P0) + eye-flip; MONO,
                     sun-lit; roof/wall value split off dot(N,up). Cull-OFF (winding not load-bearing).
- seam grep:         clean — render/buildings.* has no clock (GetTime|GetFrameTime|chrono|clock(|time(),
                     no lat/lon/heading (the one hit was `MatrixTrans`lat`e`), no sim/ write. Projection
                     math stays in the offline bake (sudbury_building.py); runtime consumes baked dirs.
- smoke shots:       (pinned Chelmsford oblique cam) — `shots/s4_buildings_pinned.png` (UP=2500 BACK=4000,
                     offset 0) = the town from the basin overview: buildings sub-pixel at that range, the
                     grid reads as roads. A/B `shots/s4_low_on.png`(→`s4_fixed.png`) vs
                     `shots/s4_low_off.png` (UP=400 BACK=900, offset 176) PROVES buildings draw (16k
                     localized Δpx). `shots/s4_fixed_crop.png` (magnified town, AFTER the P0 lighting fix)
                     = the read-back: Chelmsford reads as SILVER 3D MASSING BLOCKS — sunlit roofs (light)
                     + shaded sides (darker) between the dark street grid; building luminance mean 136
                     (matches ground), range p10=49→p90=211 (real light/shade, not flat). Before the P0
                     fix the buildings shaded as dark flat blobs (roofs lit as walls) — that shot caught
                     the bug Fable-AFTER named.
- flight ctest:      365/365 (see below) · goldens moved: 0 (`git status` = no golden/test-data touched;
                     only source/config/gen.h/docs).
- Fable red-team:    BEFORE (design) SOUND-WITH-FIXES + AFTER (landed) UNSOUND→fixed. BEFORE folded: real
                     ear-clip cap (not unconstrained Delaunay), min-rect w forced left-perp, rectangularity
                     gate→flat, metric hip pyramid ε, base sunk to kill z-fight, cull-off design. AFTER
                     folded (ALL P0/P1): **P0** the VS used world-absolute vPos so toEye/up referenced the
                     planet CENTER not the eye → roofs shaded as dark walls, sun-independent (the dark/flat
                     look) → `vPos = matModel·vertex` (eye-relative, the planet.cpp/rig.cpp convention);
                     **P1-1** stitch relation outer rings (no per-way chord on multi-way hero footprints);
                     **P1-2** roof-rect underside quad (no slit-to-sky in the overhang on a slope);
                     **P1-3** ear-clip bail counted+logged + the cap/roof validator tripwire; **P2** skip
                     `building=no`. Re-baked + re-gated green after folding.
- fallback/flags:    `SEADS_NO_BUILDINGS=1` = A/B bypass (read once). Bake best-effort: an Overpass failure
                     logs + bakes with no buildings (degraded, LOGGED, never silent), the runtime is inert
                     (dummy count-0 batch). Dials: `[buildings]` wall_val/roof_val/ambient/diffuse/
                     height_scale (all FELT; geometry is baked). height_scale = the vertical-exaggeration
                     depth-read dial. DEFERRED (Fable-BEFORE P1-1): per-building anchor radius (rigid level
                     roofs on slopes) — the eave cap + roof underside are the watertight safety nets; revisit
                     if Chad's fly shows eave slits/tilt on the Shield gradient.
- bake stats:        29032 buildings kept of 29443 candidates (411 degenerate, 0 dropped to the cap, 0
                     cap_bails) → 7 ushort batches, 415387 verts, 540120 tris. 16 m CDEM bake; the terrain
                     PNGs the re-bake incidentally touched were REVERTED to HEAD (S4 must not move Chad's
                     approved terrain baseline — gen.h is pure-additive vs HEAD, 0 removed data lines).
- Chad fly (dated):  **AWAITING** — fly low over Chelmsford: does the town read as blocks + streets? Then
                     dial `[buildings]` (wall/roof value contrast, height_scale for more/less vertical pop).
                     The tone is a SANE DEFAULT post-lighting-fix, not tuned to his stick yet.


### S6 — far-field-only DEPTH OF FIELD (the stereoscope depth payoff)   [state: AWAITING-FLY]
The "fly through an old stereoscope viewer" leap: distance-graded blur off the S1 depth-texture FBO.
Chad's call (2026-07-11 menu): **DoF first**, flavor **FAR-FIELD ONLY** — everything he flies/fights
THROUGH stays perfectly crisp; only the distant limb/background softly blurs (readability first, no
rack-focus, no near blur). Pure additive change to `render/post_glsl.cpp` (kPostFS) — the depth texture
was already sampled there (S1 built the DEPTH24 FBO for exactly this). NO Gemini (integration-tight edit
into the existing single post pass, not a fresh verbose asset); Opus authored + Fable BEFORE+AFTER.
- commit:            (this atomic commit; tag `world-s6-dof`).
- projection.lock:   N/A — the DoF stores NO baked sphere direction (consumes pure fragTexCoord + the
                     depth FBO + view-space clip planes); projection-agnostic, the lock hash does not gate it.
- what:              kPostFS gained a far-only circle-of-confusion `coc = smoothstep(uFocusStart,
                     uFocusEnd, linearZ(depth))` (view-space m), a 16-tap golden-angle gather on the
                     display-referred scene color weighted by `min(tapCoc, coc)`, blended over the FXAA'd
                     sharp path by `smoothstep(0,0.15,coc)`. Inserted AFTER FXAA, BEFORE the display
                     sigmoid; everything downstream (split-tone/halation/vignette/grain) unchanged. The
                     far SKY (cleared depth d==1.0) CoC is capped at `uSkyCoc` so the flown space-first
                     starfield isn't blurred away. `linearZ` is now single-sourced by the depth viz AND
                     the CoC. `[dof]` dials in `config/world.toml`; `SEADS_NO_DOF=1`/`enabled=0` force
                     radius 0 (the shader off-gate + A/B). New debug `SEADS_POST_DEBUG=3` = CoC viz.
- ★ tiny-planet geometry (a real finding, baked into the defaults): on R=15 km the HORIZON from ~2 km
                     up is only ~8 km away, so visible TERRAIN rarely exceeds ~8–10 km. The first-cut
                     defaults (focus_end 25 km) blurred ONLY the sky — invisible on terrain (proved by the
                     A/B + CoC viz). Retuned to `focus_start_m=2500 focus_end_m=9000 radius_px=3.0
                     sky_coc=0.40` so the distant limb/background clearly softens while near/combat range
                     (<2.5 km) stays crisp.
- build:             PASS — seads.exe + seads_tests.exe linked fresh (rm'd first; MinGW GCC + Ninja,
                     Debug, assert-live). Shader lives in render/post_glsl.cpp (seads_render_core, so the
                     headless validator gates its uniform surface).
- asset validator:   PASS 18/18 (`ctest -R asset-validator`). The post-FS uniform-allowlist leg now
                     covers the 4 new DoF uniforms (uFocusStart/uFocusEnd/uDofRadius/uSkyCoc) — exact-match
                     + no time/clock token (the grain-seed backdoor guard still the only time-like input).
- shaders GLSL330:   PASS — kPostFS compiles+links (the smoke shots ARE the linkage check; the green gate
                     is blind to the app binary). textureLod for every in-branch fetch (non-uniform flow;
                     implicit-LOD UB avoided). const int N=16 literal-bound loop unrolls.
- seam grep:         clean — no clock read (GetTime|GetFrameTime|chrono|clock(|time() in render/post.*;
                     no lat/lon/heading/fixed-axis; the IGN rotation is seeded by gl_FragCoord ONLY (no
                     clock → no temporal shimmer); near/far single-sourced from rlGetCullDistance*.
- config validation: `[dof]` guards (load_world.cpp): focus_start ∈ [0, focus_end) STRICT (a collapsed
                     smoothstep edge is undefined — the silent-disarm class), focus_end ≤ 60 km (far clip),
                     radius ∈ [0,16], sky_coc ∈ [0,1]. Mirrors the [tone] ordered-gate guards.
- smoke shots:       (pinned Chelmsford oblique cam, UP=2500 BACK=4000, `--smoke 8`, raking offset 176)
                     · `shots/s6_coc.png` (SEADS_POST_DEBUG=3, CoC viz) — near foreground DARK (crisp,
                       coc≈0) ramping LIGHTER toward the limb (rising blur) to near-WHITE far terrain, with
                       the sky held at a uniform mid-grey ≈0.40 = the `sky_coc` cap. PROVES the far-only
                       ramp + the sky protection.
                     · `shots/s6_dof_on.png` vs `shots/s6_dof_off.png` (A/B via SEADS_NO_DOF=1) — ON: the
                       near town street-grid + trees read crisp while the distant terrain toward the limb
                       softens and the limb edge goes gently soft; OFF: crisp throughout. Region diff
                       (luma |Δ|≥6): far/limb+mid terrain = 40k+52k changed px, but the BLACK SPACE up top =
                       only 261 px — the blur is far-field terrain, NOT the sky-as-mush. Real, visible work,
                       not a no-op; near stays sharp.
- flight ctest:      367/367 · **goldens moved: 0** (`git diff --stat -- test/` = only test_asset_validator
                     .cpp +3/−1, the allowlist line; no golden data dir touched). Shader/post is invisible
                     to the kernel goldens by construction.
- Fable red-team:    BEFORE (design) SOUND-WITH-FIXES + AFTER (landed) SOUND-WITH-ONE-P1. BEFORE folded:
                     P1-a weight `min(tapCoc,coc)` (stop far-sky over-weighting a mid ridge into a bright
                     fringe); P1-b sky CoC cap (protect the space-first stars); P2-a textureLod in the
                     non-uniform branch (implicit-LOD UB); P2-b all-taps-rejected fallback → the FXAA'd
                     center not a raw tap; P2-c per-pixel IGN spiral rotation (a raised radius dial scatters
                     ghosting into grain-masked noise); P2-e depth texture pinned NEAREST. AFTER verified
                     all 7 folds present+correct and caught a NEW **P1**: the gather omitted the CENTER tap
                     → a low-survivor far sliver (through canopy/struts) copied one random neighbour =
                     speckle + a discontinuity vs the fallback. FOLDED: seed `acc = aa*coc; wsum = coc;`
                     (center at its own weight, FXAA'd, so the degenerate case converges continuously to the
                     sharp fallback). AFTER's soft P2 (mix ramp saturates by coc=0.15) left for Chad's fly —
                     likely absorbed by the P1 seed; the pixel-gated ramp `smoothstep(0,1.5,coc*uDofRadius)`
                     is the known follow-up if far ridgelines shimmer.
- fallback/flags:    `SEADS_NO_DOF=1` = A/B bypass; `[dof] enabled=0` = off (both force radius 0, the shader
                     off-gate). `SEADS_POST_DEBUG=3` = CoC viz. Rides the existing S1 post FBO — if that FBO
                     can't build the whole post pass (incl. DoF) disables itself (S1's logged, latched
                     degraded path; never silent).
- fly-dials:         `[dof]` focus_start_m (crisp-out distance) · focus_end_m (full-blur distance) ·
                     radius_px (strength) · sky_coc (far-sky blur cap). Remember the tiny-planet geometry:
                     terrain sits within ~8–10 km, so focus_end lives in the single-digit-km range.
- Chad fly (dated):  **AWAITING** — fly low + at altitude: near stays crisp while flying/fighting? the
                     distant limb/background softens into the stereoscope-depth feel? stars still read
                     against space (sky_coc)? Then dial radius_px (subtle↔strong) + focus_start_m/focus_end_m
                     for where the blur begins. NOTE the game SPAWNS IN DAYLIGHT — the effect reads any time
                     of day (it's depth-based, not night-gated).

### S5c — Chelmsford St-Joseph French Catholic church (Blender-built glTF hero)   [state: AWAITING-FLY]
The first `blender-hero-forge` skill hero + the FIRST S5b-style glTF ingest path stood up (Chad's route
ruling 2026-07-13: Blender-built glTF, not procedural). Reference-true massing built + Chad-approved in
Blender, exported `assets/heroes/church.glb`, draped at Chelmsford through the town mono shader (no fork).
- commit:            (this atomic commit; tag `world-s5c-church`)
- projection.lock:   0x99061E1583D34607 (PROVISIONAL) — `render/sudbury_hero.gen.h` embeds it;
                     `buildings.cpp` static_asserts kSudburyHeroProjLockHash == kSudburyProjectionLockHash
                     (a projection re-bake that forgets to re-run sudbury_hero_place.py fails the build).
- build:             PASS (seads.exe links; static_assert lock guard compiles)
- asset:             `assets/heroes/church.glb` 11.5 KB, 129 verts/106 faces, +Y up; source
                     `generated/church_build.py` (parametric, re-runnable). glb loads: raylib
                     "Model basic data (glb) loaded successfully" + "HERO: 'heroes/church.glb' placed (1 mesh)".
- shaders:           reuses kBuildingVS/FS (no new shader) — mono flat-facet, ws=-1 => church stays dark
                     stone at night (no windows).
- seam grep:         clean — sim/ + control/ untouched (render-only); no new render path (rides b.meshes).
- smoke shots:       `shots/s5c_church.png` (day, offset 120) + a dusk (170) cut. The church reads at the
                     Chelmsford town center as a dark-fieldstone (tone 0.72) massing with a tall central
                     spire — the tallest structure in town, upright, correct 50 m footprint. Location +
                     orientation + scale proved via a nadir + beacon(tone 5.0) locator pass (which also
                     caught a SMOKE-cam bug: SEADS_OBL_TILT sign — +tilt aims SOUTH; Chelmsford is N, so
                     PAN=-27.64 TILT=-10.80 frames it).
- flight ctest:      413/413 · goldens moved: 0 (buildings.cpp is app-target-only, NOT compiled into
                     seads_tests — the flight goldens cannot move from this change).
- Fable BEFORE:      SOUND-WITH-FIXES — folded: (P1a) orthonormal tangent basis
                     north_t=norm(REF_N-(REF_N·up)up), east_t=cross(north_t,up); (P1b) ground on raw
                     radius_at + explicit 0.7 m sink (no BUILDING_BASE_SINK_M double-count). Rigid
                     placement confirmed fine (curvature float ~2.4 cm on 50 m/R=15 km).
- Fable AFTER:       SOUND (0 P0/P1) — verified vs raylib rmodels.c: memory lifecycle leak/double-free-free,
                     u32->u16 index conversion safe at 129 verts, node-transform baked at load, fork
                     structurally impossible (same Mesh layout + b.meshes + DrawMesh loop). P2 folded:
                     height_scale note + all-degenerate WARN; P2 noted: keep forge glbs < 65535 verts.
- flags:             SEADS_NO_HERO=1 A/B-bypasses the hero set (like SEADS_NO_BUILDINGS).
- Chad fly (dated):  **AWAITING** — fly over Chelmsford (town center, ~46.582,-81.20) in daylight: does the
                     church read as the parish landmark (dark stone, tall spire, tallest in town)? Dial
                     `tone` (fieldstone darkness) + `heading_deg` (facade azimuth vs the street grid — psi=0
                     now, tune by eye) in offline_tool/sudbury_hero_place.py, and `sink_m` if the base
                     floats/buries. Then flip AWAITING-FLY -> DONE and lock the projection.

**S5c UPDATE v2 (Chad feel, 2026-07-13):** BIGGER + very TALL (spire tip 48->63.5 m), moved to the
REAL spot (3594 Errington Ave N, 46.58283/-81.19757 — researched: Eglise St-Joseph, blessed 1913, arch.
Alphonse Venne, "most imposing in the Valley"), GREY-and-WHITE two-tone (per-vertex COLOR_0 baked in
Blender -> the ingest maps it to per-vertex bj: grey 0.6 fieldstone walls / white 0.96 spire+belfry+trim;
was "very dark"), and an OPEN belfry stage with a big dark BELL. gate 413/413, 0 goldens moved. OPEN for
Chad's fly: the FACADE HEADING toward Errington St N (heading_deg=0 provisional in sudbury_hero_place.py
— give the bearing) + the two-tone `tone` multiplier (1.6) + `sink_m`.

**S5c CHURCH — FINAL STATE (2026-07-13, AWAITING-FLY).** All Chad's fixes landed & committed (tip:
`975aa8ba9`): the Blender-built glTF Eglise St-Joseph at 3594 Errington Ave N (46.58283,-81.19757),
BIGGER/TALLER (63.5 m), grey+white two-tone (per-vertex COLOR_0 → bj), open belfry + big bell, facade
faces EAST onto Errington, SET BACK off the road (`setback_m=22`) with a front lawn, and a WIDE front
STAIR up to a dark door elevated 2.4 m. Reusable via the `blender-hero-forge` skill. Fly-dials in
`offline_tool/sudbury_hero_place.py` (re-run + rebuild): `setback_m` / `heading_deg` / `tone` / `sink_m`.
One un-Fable'd bit: the per-vertex-color read added to `append_hero_meshes` after the last red-team
(null-guarded, gate-green) — worth a quick Fable pass before DONE. NEXT WORLD ITEM: CC6-2 rails + CC6-3
reposition (see `docs/copper_cliff_plan.md` CC6 block).
