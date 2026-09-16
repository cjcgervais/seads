# LIVING COPPER CLIFF — staged plan (the spec `program.md` reads)

**Thread:** WORLD (stereoscope Sudbury). **NEVER touch `sim/` or `control/`** (frozen kernel firewall).
**Goal:** turn the INCO Copper Cliff smelter/slag complex into a living night spectacle — three linked
animated elements — each BUILT + gated green + Fable-red-teamed + committed **AWAITING-FLY** so Chad flies
it. Judged by feel, not fidelity; do not gold-plate.

Chad's rulings (2026-07-12): **plan all three staged**; the **hot slag glows warm orange** — a SANCTIONED
warm-chroma exception (like the aircraft + aurora), NOT silvered by the S1 post; the **Superstack smoke
stays mono/silver**. Fable audit BEFORE and AFTER the plan.

Historical reference (researched, in `docs/stereoscope_handoff.md` "LIVING COPPER CLIFF" block): electric
trolley loco hauling ~22 slag pots (~16 t, ~1350 °C) ~1.5 km smelter→dump every ~30 min, 24/7; pots
tipped over the dump edge; molten slag poured down the black man-made slag mountain as fiery
orange/vermilion rivers; viewable from the highway NW of the dump; ran 1929–~2002.

## The seam this rides (from the recon — cite these when building)
- **t_cel (no-clock phase source):** `app/main.cpp:1005-1007` `t_cel = tick_count*sim_dt + cel_time_offset`
  (tick-derived, AT-9-pinned; render/ never reads a clock — SPEC §9). Every animation phase below is a
  PURE fn of `t_cel`. Also an app-owned cosmetic `frame_count` (`app/main.cpp:1283`).
- **Hero mesh emission (offline):** `offline_tool/sudbury_hero.py` `superstack()`/`build_heroes()` →
  `frame.aeqd_to_dir(X,Y)` (the ONE projection) → building batches in `render/sudbury_gis.gen.h` →
  `render/buildings.cpp:112` `build_batch_mesh()` drapes at `radius_at(d)+h*height_scale`. Hero-height
  validator ceiling `HERO_H_MAX=400` (`sudbury_config.py:258`); Superstack at **46.4747,−81.0539**.
- **Draped polyline (track):** `render/ribbons.{h,cpp}` + `offline_tool/sudbury_ribbon.py`; drape via
  `render/sphere_param.cpp:221` `HeightField::radius_at(dir)` (SAME field as terrain/trees — anti-float).
  No OSM `railway` fetch exists → **hand-author the spur** (control points in lon/lat), don't add a
  network-gated fetch overnight.
- **Additive billboard glow (smoke puffs, slag glow):** `render/lights.{h,cpp}` — camera-facing quad,
  `off = max(uSizeM, uMinPx*(-viewPos.z))`, night-gated `sunEl=dot(-uSunDir,vUp)`, additive, depth-test
  ON / depth-write OFF, drawn AFTER all opaque depth-writers (`render/draw.cpp:933`).
- **Chroma exception (the slag):** `render/post_glsl.cpp:174-187` split-tone sat-gate:
  `C=max-min; sat=smoothstep(uSatC0,uSatC1, C/max(mx,uSatDark)); col=mix(silver,toned,sat)`. A RAW
  high-chroma orange emissive in the scene FBO → `sat→1` → passes UNsilvered (exactly how planes/aurora
  keep color). Verify with `SEADS_POST_DEBUG=2` (sat mask white on the slag) + a pure-orange signature.
- **Draw order (`render/draw.cpp:855-977`):** sky/stars → planet → water → ribbons → buildings → trees →
  aircraft → **lamps (additive)** → translucent props → post → HUD. New OPAQUE meshes (train, mound) slot
  in the terrain-feature tier (after buildings); new ADDITIVE glows (smoke, slag) slot in the lamp tier;
  translucent grey smoke may instead ride the props/alpha tier.

## Pure-phase math (what Fable audits — all `frac(t_cel*rate)`, deterministic, no clock)

### CC1 — Superstack smoke (mono/silver plume)
Anchor `top = dir_stack*(radius_at(dir_stack)+STACK_H)`, `up=dir_stack`, wind tangent
`w = normalize(t_wind - dot(t_wind,up)·up)`. N puffs; puff i age `a_i = frac(t_cel*rate + i/N) ∈ [0,1)`.
- `pos_i = top + up·(a_i·RISE) + w·(pow(a_i,0.8)·DRIFT) + jitter(i)` (jitter = fixed hash of i, not time).
- `size_i = mix(R0,R1,a_i)` (expands rising).
- `opacity_i = smoothstep(0,0.12,a_i)·(1 - smoothstep(0.70,1.0,a_i))`.
  **★ Loop-continuity invariant (Fable): opacity→0 at BOTH a=0 and a=1**, so a puff dies at the top and
  respawns at the base with zero alpha — no pop at the `frac` wrap. Fable: confirm no seam.
- Mono grey (low sat → silvered by post; NO chroma exception). Alpha-blended translucent quads.

### CC2 — slag-pot trains (pure arc-length pose on a baked LOOP spur)
Hand-authored **closed** spur (smelter→dump→return), baked to unit dirs + a cumulative arc-length table
`s[0..M], L=s[M]`. Head `s_head = frac(t_cel*train_rate)·L`; car k at `s_k = mod(s_head - k·CAR_GAP, L)`.
- `point(s)`: binary-search the cum-length table, lerp the segment; drape `radius_at(dir)+CAR_LIFT`.
- `tangent(s) = normalize(point(s+ε)-point(s-ε))`; orient car = frame(tangent, up).
  **★ Continuity (Fable): the spur is CLOSED (`point(L)=point(0)`, tangents match) so a car wrapping L→0
  does not jump**; and every segment length > 0 (tangent defined). Loco + K pots, mono steel.
- Rebuilt per frame from `t_cel` (few cars — cheap); OPAQUE tier after buildings.

### CC3 — slag pour (marquee; warm-chroma exception, night-brightest)
Procedural slag-heap **mound** (radial-lobed cone, dark matte) at the dump; pour face aimed NW. Cycle
period `T_pour`, phase `p = frac(t_cel/T_pour)`:
- Pot tip `θ(p)`: `smoothstep(0,0.15,p)·θmax` up, hold, `(1-smoothstep(0.60,0.75,p))` back to 0 → returns
  to rest each cycle (no pop). A designated pot at the mound rim tips.
- Lava cascade: flow quads down the face; **heat gradient single-sourced** `heat(u)` (u = down-slope +
  age): hot `(1.0,0.55,0.12)` → cooling red `(0.7,0.10,0.02)` → black crust `(0.04,0.02,0.02)`. Flow
  scroll `frac(t_cel*flow_rate)`; a fresh tongue appears when `p∈[0.15,0.60)`, else the face cools to crust.
  **★ Emissive single-source (Fable): `heat()` is ONE definition** (a fork with any limb/scatter/`sky_color`
  is the H1 trap). **★ Chroma-gate (Fable): the RAW emissive `C=max-min` must exceed `uSatC1`** so
  `sat→1` and it survives the post as orange (verify sat-mask + signature). **★ Night-gate the intensity**
  (additive orange onto a BRIGHT day sky washes toward white → low chroma → silvered; onto dark night it
  stays vivid — matches the real spectacle). **★ No-clock:** every phase is `frac(t_cel*rate)`.

## Stage ladder (one row per loop iteration; terminal = AWAITING-FLY or DEFERRED-with-reason)
| Stage | Element | Deliverable | Chroma |
|---|---|---|---|
| CC1 | Superstack smoke | mono plume off the 381 m stack, wind-drifted, pure-phase | mono/silver |
| CC2 | slag-pot trains | loco + pots moving on the baked loop spur | mono steel |
| CC3 | slag pour | mound + pot tip + molten lava cascade, night-brightest | **WARM ORANGE** |

## Per-stage gate (the loop's success signal — ALL must hold to advance)
1. `cmake --build build --config Debug` PASS (no new warnings-as-errors).
2. `ctest --test-dir build -C Debug` fully green; **0 goldens moved** (`git diff --stat` on the golden dir).
3. Asset validator green if the stage bakes an artifact (new tripwire leg for new geometry/emissive).
4. **Smoke shot** at the pinned Copper Cliff viewpoint + a night offset — Read the PNG back and confirm
   the mechanism is ON-SCREEN and reads right (the green gate is blind to the app binary — SPEC lesson).
5. **Fable-AFTER red-team** (fresh context, `model=fable`) on the landed mechanism; verdict SOUND /
   SOUND-WITH-FIXES with every P0/P1 folded (iterate only on P0/P1 — token-conscious).
6. Commit by EXPLICIT path (never `-A`; cross-agent files `render/sky.h`/`generated/gen_log.jsonl`/
   `bf109_preview/` + audio files are NOT ours), tag `world-cc{N}-<name>`, update the handoff evidence block
   + `docs/copper_cliff_ledger.tsv`.

## Copper Cliff coordinates (approximate — fly-dials, refine on Chad's fly)
- Superstack / smelter: **46.4747, −81.0539** (the built hero).
- Slag dump mound: ~**46.482, −81.030** (the black slag mountains NE of the stack; pour face aimed NW
  toward the Hwy 17 bypass — Chad's "viewable from the highway NW of the dump").
- Spur: smelter (46.474,−81.054) → dump (46.482,−81.030) → return (a closed loop).

## Fable-BEFORE folds (2026-07-12 — AUTHORITATIVE corrections; build with these)
Verdict SOUND-WITH-FIXES (2×P0 + P1 hygiene). CC1 wrap claim + CC3 pot-tip C1-continuity confirmed correct.
- **★ CROSS-CUTTING P1 (all stages) — never upload raw `t_cel` as a float32 uniform.** At t≈10 h, float
  resolution is ~4 ms → visible scroll stutter. Compute EVERY `frac(t_cel*rate)` in DOUBLE on the CPU and
  upload the wrapped phase ∈[0,1) per feature. (t_cel stays double end-to-end until the wrap.)
- **★ CROSS-CUTTING P1 — billboards camera-facing from the VIEW matrix's screen right/up**, NOT
  `cross(world_up, view)` — the latter degenerates flying straight down the plume axis over the stack.
- **CC1 P1:** assert `w` non-degenerate at init (`t_wind ∦ up`), else fallback `w=normalize(cross(up,x))`.
  Alpha puffs: depth-test ON / write OFF, **sort back-to-front by view-depth per frame** (presentation, not
  phase). P2: reseed `jitter` per cycle via `hash(i, floor(t_cel*rate + i/N))` (the floor flips exactly at
  the alpha=0 wrap → still zero-pop) so the plume doesn't replay identically.
- **CC2 P0 — negative mod.** `s_k = fmod(s_head - k*CAR_GAP, L); if (s_k < 0) s_k += L;` (CPU `fmod` is
  truncated, not floored). The tangent stencil `point(s±eps)` must wrap its arg through the SAME floored
  mod. **P1:** the polyline is C0-not-C1 at EVERY vertex (heading snaps by the corner angle) → corner-round
  at bake (Chaikin ×2) AND/OR low-pass the tangent over a car-length window `w` (`point(s+w)-point(s-w)`).
  **P1:** `normalize` the lerped dir before `radius_at`/scaling (lerp of unit dirs is sub-unit). **P2:**
  Gram-Schmidt the car frame (`fwd=normalize(t - dot(t,up)·up)`); eps ≈ car length (heightmap tangent noise).
- **CC3 P0 — ONE clock per feature.** Do NOT free-run `frac(t_cel*flow_rate)` for lava age (its wrap pops
  hot←crust mid-pour). Derive age from the pour phase itself and model the tongue as an ADVANCING FRONT,
  not a repaint: `front(p)=clamp((p-0.15)/τ,0,1)*face_len`, `heat = heatf(front(p) - downslope)` masked for
  negative arg → fresh lava advances down the face (extent 0 at emission), cools to crust by p→1⁻ which
  equals the p=0 state (wrap-safe). **P1:** draw the cascade BODY as opaque/alpha (dst replaced → chroma
  survives day AND night, gate-robust); keep ADDITIVE only for the night-gated glow HALO. **P1:** verify
  `uSatDark` sits ABOVE crust luma (≥~0.1) or crust (C=0.02,mx=0.04) may pass the gate as un-silvered chroma
  — check the LIVE post values + the scene FBO format (UNORM clamps hard, HDR float degrades gently) before
  tuning satC0/satC1. Hot/cooling-red pass cleanly (confirmed).

## Fable-AFTER (per landed stage): red-team the landed mechanism in a fresh context. See the ledger.

---

# CC-EXP — the DUMP EXPANSION (CC4 ridge + CC5 dump train). Chad, 2026-07-13 (/agent-builder)

Chad un-deferred the two `DEFERRED-*` ledger rows and asked to **"complete the Copper Cliff slag-pour
dump true to historic reference — model a train and the proper length of the hill ridge the track are atop
of, and the giant steel pots they use to dump."** Two new ladder stages, same loop (build → gate → smoke →
Fable-after → commit AWAITING-FLY). Fable audits BEFORE (this section) and AFTER (each landed diff).

**Historic reference (researched 2026-07-13, sources in the ledger `note`s):** the INCO Copper Cliff slag
dump is a ~200 ha man-made **RIDGE / range** of black slag hills (in use 1929–~2002, 115M+ t). The rail
line runs **ATOP the crest**; pots are tipped over the edge and molten slag sheets down a **steep planar
face at the angle of repose (~35°)** to a rubble toe. Dump ~10,000 t/24 h; the highest bank ("No.5") reads
tens of metres tall. An **electric trolley loco** (steeplecab / boxcab, roof pantograph, dark steel; slag
trains were double-headed) hauled **~12 open cast-steel pots** (2/car ×6 cars), each **~17 t at ~1350 °C**,
**~2.5–3 m dia × ~2.5–3 m tall**, a **thick-walled tapered cup wider at the rim**, nested in a **trunnion
cradle** on a flat car, **rotated ~120° on its side trunnions** to pour. Colours: matte black-charcoal
ridge (rust weathering, emissive pour streaks); dark-grey steel loco/pots (rust). We COMPRESS the linear
scale for the 15 km sphere (ridge ~700 m not 1.5 km) — judged by feel, not fidelity.

## The seams these ride (already built — CITE, don't reinvent)
- **CC3 `render/slag.{h,cpp}`** — mound frame `anchor=kMoundDir` (= `kSpur[3]`, the dump edge), `up=anchor`
  (radial), `face`=toward `kSuperstackDir` in the tangent plane, `side=cross(face,up)`. `build_mound_mesh`
  (radial lobed cone), `build_lava_mesh` (fan down +face, `texcoord.x=vD` downslope frac), `build_pot_mesh`
  (open cone), a static tipping pot at the rim keyed on `phase`, single-clock lava FS keyed on `vD`+`uPhase`.
- **CC2 `render/train.{h,cpp}`** — CLOSED spur `kSpur[6]` (offline dirs through the LOCKED aeqd), Chaikin×2,
  cumulative DRAPED arc-length `cum[]`/`L`; `point_at(s)` (floored `fmod`, renormalized lerp, `radius_at`
  drape); per-car frame Gram-Schmidt `fwd = t − dot(t,up)·up`; loco/pot = `add_box`/`add_cone`; mono FS.
- **Phase wiring (`app/main.cpp:1176-1182`)** — `info.train_phase = frac(t_cel*train_rate)` (rate 0.0011,
  loop ~909 s, ~4.2 m/s), `info.pour_phase = frac(t_cel*pour_rate)` (rate 0.02, ~50 s/pour). `draw.cpp`
  passes them to `draw_train`/`draw_slag`. Draw order: buildings → **train → slag** (opaque tier).
- **No new uniforms on the post/validator allowlist** — train/slag use their own inline `LoadShaderFromMemory`
  shaders (CC2/CC3 added `uPhase` etc. with the validator staying green). Keep new uniforms on THOSE shaders.

## CC4 — LENGTHEN THE HILL INTO A SLAG RIDGE (reshape the CC3 mound; render-only, no bake)
Replace the radial cone with a **linear ridge** whose flat crest carries the (CC2) track and whose single
steep face is the pour face. `render/slag.cpp` only; `[slag]` gains ridge dials; A/B `SEADS_NO_SLAG` intact.

**Ridge frame (build_slag_renderer, from the LOCKED dirs — no new offline data):**
- `up = anchor` (radial at `kMoundDir`), as today.
- **`axis`** = the crest tangent = `normalize(g)` where `g = (kSpur[4]−kSpur[2]) − dot(·,up)·up` (the spur's
  dump-leg through-direction at the dump edge, Gram-Schmidt onto the tangent plane) → the crest runs ALONG
  the track. Assert `|g| > eps` (the two control dirs are ~2 km apart, non-degenerate).
- **`face`** = horizontal pour direction ⟂ the crest = `±normalize(cross(up, axis))`, sign chosen so
  `dot(face, kSuperstackDir−anchor) > 0` (pour face aims NW toward the Superstack/highway — Chad's "viewable
  from the highway NW of the dump"). `face ⟂ up` and `face ⟂ axis` exactly (both are ⟂ up; cross gives ⟂).
- Local mesh coords **X=face, Y=up, Z=axis** (right-handed: `cross(face_asX?, …)` — verify handedness so the
  FS facet-normal-toward-eye keeps the crest lit; culling is off so winding is free either way).

**Ridge mesh `build_ridge_mesh(L)` (replaces build_mound_mesh; extrude a cross-section along Z=axis):**
- Params (new `[slag]` dials): `ridge_length_m≈700`, `crest_width_m≈24`, `mound_height_m` (reuse, ~45),
  `face_run = height/tan(35°) ≈ height·1.428` (front pour face), `back_run ≈ height·2.4` (gentler back).
- Stations `k=0..K` along Z from `−Lr/2 → +Lr/2` (K≈40). At each station a per-station height
  `h_k = height · endTaper(zk) · (1 + 0.12·noise1(k))` where **`endTaper`** ramps h→0 over the last ~90 m at
  BOTH ends (smoothstep) so the ridge has SLOPED ends, not vertical end-walls; `noise1` gives lifts/terraces.
- Cross-section vertices per station (X across): back-toe `(−cw/2 − back_run, 0)`, back-crest `(−cw/2, h_k)`,
  front-crest `(+cw/2, h_k)`, front-toe `(+cw/2 + face_run·(h_k/height), 0)`. Triangle-strip between adjacent
  stations. The **front (+X) face is the pour face** (steep 35°); the flat crest strip `[−cw/2,+cw/2]` at
  `y=h_k` carries the track. Base `y=0` sits ON the terrain (the frame origin `anchor_pos` is draped);
  sink the toes ~2 m so no terrain gap.
  **★ Fable check:** the crest-Z at the dump point (Z=0) stays the RIDGE MAX height so the pour face is a
  clean 35° drop from there; ends taper so the silhouette reads as a natural range from altitude.

**Lava on the ridge (`build_lava_mesh` reworked for the linear face):** rivers no longer fan radially — each
river `rv` sources at a crest-Z offset `z_rv` (clustered near the active dump point Z≈0 for CC4) and flows
DOWN +X: source `(+cw/2, h, z_rv)` → toe `(+cw/2+face_run, 0, z_rv + meander)`. `vD = downslope frac (0
crest → 1 toe)` unchanged → **the single-clock lava FS is untouched** (still keys on `vD`+`uPhase`, the
Fable-vetted advancing-front/supply-plateau). Keep the narrow-source→widen, leading-center-vertex, rounded-
toe, meander refinements from `CC3-refine`. **★ Fable check:** `vD∈[0,1]` preserved (no shader edit); the
front `p_arr=0.15+vD·0.35` / tail still wrap-safe.

**Static tipping pot (CC4 interim):** re-seat CC3's rim pot onto the ridge crest at the dump point
(`anchor_pos + up·h + face·(cw/2·0.9)`, tipping about the `axis` trunnion toward +face) so CC4 alone reads
as "a long ridge with a pot pouring at one point." **CC5 removes this** (the train's pots take over the tip).

## CC5 — THE DUMP TRAIN: proper loco + giant steel pots, running the crest, tipping to FEED the pour
`render/train.{h,cpp}` (+ a small `render/slag` coupling hook + the `app/main.cpp` phase line). Historic-
reference meshes; the dump leg routed along the ridge crest; pots tip over the edge in a **dump zone**;
the CC3 lava driven by the **train head** so the pour is fed by the pots (ONE clock, no two-clock fight).

**(a) Proper meshes (reference-driven `add_box`/`add_cone`, mono steel — silvered by the post):**
- **Steeplecab loco:** low chassis box (`~12 m×3 m×3 m`) + a **center-peaked cab** (a taller narrow box at
  mid-length with two short end-hoods = three stacked boxes) + a **roof pantograph** (a thin A-frame of 2–3
  slim boxes rising ~1.8 m to a horizontal contact bar). Double-ended (symmetric). Optionally lead with TWO
  locos (double-headed — reference) via a `locos` dial; default 1 to stay cheap.
- **Giant steel pot car:** flat 4-wheel deck box + a **trunnion cradle** (two short upright yoke boxes on
  ±Z sides) + the **pot** = a thick tapered cup via `add_cone` (rb≈1.3, rt≈1.6, h≈2.8 — wider at the rim)
  with a **double wall** (a second slightly-smaller inner cone for wall thickness at the rim) and a small
  pour-lip notch. **2 pots per car** side-by-side along Z (the reference 2/car). `pots` dial = pot-CARS.

**(b) Route the dump leg along the crest:** the spur's dump-edge vertex is `kSpur[3]=kMoundDir`. Insert two
crest control points either side of it along the ridge `axis` (`kMoundDir ± axis·(0.42·ridge_length)` re-
normalized) BEFORE the Chaikin pass, so the smoothed track runs the crest length atop the ridge, then curves
back on the return leg. The `lift_m` rail-head drape now sits on the ridge crest `h` (the ridge mesh raised
the local terrain by `h` at the crest — so bump `lift_m` by the crest height at the dump, OR drape the track
on a small `crest_height(s)` offset for the dump-leg arc). **★ Fable check:** the spur stays CLOSED and the
inserted points keep every segment length > 0 (no zero-length tangent); re-Chaikin so C0→smooth as CC2.

**(c) Pots tip in the DUMP ZONE (pure fn of each car's arc-length — no clock, no new phase source):** define
the dump-zone center `s_dump` = the draped arc-length of `kMoundDir` along the spur (found once at build).
Each pot car at arc-length `s_c` has `d = wrapdist(s_c − s_dump, L)` (signed shortest arc). **Tip angle**
`θ_c = θmax · bump(d / tip_span)` where `bump(x)=max(0, 1 − x²)` (C1 at the edge, 0 outside `|d|>tip_span`),
`θmax≈2.1 rad` (~120°). The pot rotates about the car's **fwd (trunnion) axis** toward +face (the pour side).
So as the continuous train crawls the crest, **each pot tips over the edge in turn** — a procession of
pouring pots. Pure in `s_c` (pure in `train_phase`), deterministic, wrap-safe (bump→0 at the zone edges).
  **★ Fable check:** `wrapdist` uses the SAME floored-mod convention as `point_at`; `bump` reaches exactly 0
  at the zone edge so a pot entering/leaving the zone has no tip discontinuity; the rotation axis is the
  car's Gram-Schmidt `fwd` (NOT world axis) so it tips sideways over the face at any track heading.

**(d) Couple the lava to the TRAIN (single clock — the pots FEED the pour):** replace the independent
`info.pour_phase` fed to `draw_slag` with a phase derived from the train head, so the lava PULSES with each
arriving pot: `dump_phase = frac((s_head − s_dump)/car_gap + φ0)` (period = one car_gap of head travel = the
pot-arrival interval; `φ0` aligns `dump_phase=0.15` (front emission) with a pot at peak tip `d≈0`). Feed
`dump_phase` to `draw_slag` as its `phase`. Now the lava's advancing front (`p_arr=0.15+vD·0.35`) starts as
a pot reaches the edge and cools before the next — **the pour is literally supplied by the tipping pots**,
one clock (`s_head`), no fight. `pour_rate_hz` becomes vestigial (keep for back-compat / the standalone
static demo; document it). CC3's own static rim pot is REMOVED (the train supplies the tipping pot).
  **★ Fable check:** `dump_phase` is computed in DOUBLE and wrapped to [0,1) before the float32 uniform
  (the cross-cutting P1). `φ0` is a constant, not time. The lava FS is UNCHANGED — it still sees a [0,1)
  phase; only its SOURCE moved from `pour_rate` to the train head. `car_gap` and `tip_span` chosen so at
  least one pot is mid-tip whenever `dump_phase∈[0.15,0.60)` (lava supplied) — else the face shows a dark
  gap under visible lava. Verify: `tip_span ≥ 0.5·car_gap` (a pot is always within a zone as the next
  arrives) OR accept a brief between-pots lull (reads as the real intermittent pour — Chad's call on the fly).

## CC-EXP stage ladder (one row per loop iteration; terminal = AWAITING-FLY or DEFERRED)
| Stage | Element | Deliverable | Chroma |
|---|---|---|---|
| CC4 | slag ridge | reshape the mound → a ~700 m linear ridge (flat crest + 35° pour face + tapered ends), lava re-fit to the face | mono ridge / **warm lava** |
| CC5 | dump train | reference loco + giant steel pots, track routed atop the crest, pots tip in the dump zone, lava fed by the train head (single clock) | mono steel / **warm lava** |

## CC-EXP coordinates (reuse the locked dirs; no new offline data, lock UNTOUCHED)
- Dump edge / ridge center: `kMoundDir = kSpur[3] = (0.37728,−0.50662,0.77524)`. Crest axis from `kSpur[2]↔[4]`.
- Superstack (pour-face aim + smelter end of the spur): `kSuperstackDir = (0.26562,−0.55886,0.78557)`.

## Fable-BEFORE folds (CC-EXP, 2026-07-13 — AUTHORITATIVE; build with THESE, they supersede the body above)
Verdict SOUND-WITH-FIXES (2×P0 + 7×P1). Confirmed correct (don't touch): the ridge frame ⟂-ness + sign
robustness (~0.86 margin); **35° invariance** (`face_run·h_k/height = h_k/tan35°` → slope `tan35°` for ALL
h_k; h_k→0 gives zero-area tris, no inversion); the lava FS re-fit (geometry-agnostic, keys only on `vD`+
`uPhase`); the coupling ALIGNMENT (`φ0=0.15` puts emission at peak tip; cars-trail-head sign consistent).

- **F1 (P0) — the free `frac((s_head−s_dump)/car_gap)` pours ~424 ghost cycles/lap with no train at the
  dump.** GATE it to the window where the string is actually AT the dump (still ONE clock = `s_head`, all
  double): `w = wrapdist(s_head−s_dump, L)` (signed, [−L/2,L/2)); `dump_phase = (w ≥ 0.85·gap && w ≤
  (pots+0.85)·gap) ? frac(w/gap + 0.15) : 0.0`. `0.0` reads as FS-dark (`p<p_arr` → discard). Continuity is
  EXACT at both edges (`frac(1.0)=0` at `w=0.85·gap`; `frac(pots+1)=0` at the far edge) — no hysteresis. The
  loco slot (w≈0 < 0.85·gap) stays dark. Extinction (p=0.98) lands at `w=(pots+0.83)·gap`, inside the window.
- **F2 (P0) — inserting crest control points as `normalize(kMoundDir ± axis·(0.42·Lr))` adds ~294 m to a
  UNIT dir → a point on the far side of the planet.** Make it ANGULAR: `δ = 0.42·Lr / radius_at(kMoundDir)`;
  `P± = normalize(kMoundDir·cos δ ± axis·sin δ)`. The **−axis point goes BEFORE `kSpur[3]`** in the polyline
  (axis = [4]−[2] = travel dir). P−/kSpur[3]/P+ are collinear → Chaikin keeps the crest segment dead
  straight through the dump (good). Consider pulling P± to **0.35·Lr** so the Chaikin corner-cut near P±
  doesn't swing the track off the 24 m crest while still elevated (F13; verify on the smoke shot).
- **F3 (P1) — the FLAT local frame lifts ~4 m off the sphere at the ridge ends (`z²/2Ra`) + ignores terrain
  drift over 700 m.** Per-station base offset `y0_k = (radius_at(dir_k) − Ra) − z_k²/(2·Ra)`, applied to the
  whole cross-section, with `dir_k = normalize(anchor·cos(z_k/Ra) + axis·sin(z_k/Ra))`, `Ra=radius_at(anchor)`.
  **SINGLE-SOURCE `h(z)` (endTaper·noise) AND `y0(z)`** — the track's crest lift for the dump leg MUST sample
  the IDENTICAL `crest_height(s)=h(z(s))+y0(z(s))` (H1 fork trap: a constant `lift_m` bump floats the train
  45 m over the rest of the loop and the ±0.9 m noise buries/floats the rails). Reject the "bump lift_m" branch.
- **F4 (P1) — the BACK toe isn't tapered** → a full-depth apron sticks out the back at the tapered ends.
  Scale it too: back-toe X `= −cw/2 − back_run·(h_k/height)`.
- **F5 (P1) — `bump(x)=max(0,1−x²)` is C0 not C1 at x=1** (slope −2→0 kink; θ reaches 0 so no positional pop,
  but the tip RATE kinks). Use `bump(x) = (max(0,1−x²))²` — true C1 (derivative 0 at x=1 AND x=0), peak 1.
- **F6 (P1) — the pot tip must be a RIGID rotation + a SEPARATE mesh.** `pot_up=cosθ·up+sinθ·face` only
  rotates about `fwd` when `face⟂fwd` (false where the track curves in the zone → sheared pot). Rotate in the
  car's OWN frame about `fwd`: `t = sign(dot(right,face))·right; pot_up = cosθ·up + sinθ·t; pot_side =
  cosθ·t − sinθ·up`. The **pot must be its own template mesh** (deck+cradle+wheels a second static mesh) or
  the per-pot rotation is unrepresentable. Pivot at the **trunnion point** (cradle height): compose
  `M_car · T(pivot) · R(fwd,θ) · T(−pivot)` per pot, or the pot sweeps through the deck at 120°.
- **F7 (P1) — `s_dump` is not a table vertex** (Chaikin doesn't pass through control points). Compute once at
  build: `s_dump = argmax over the smoothed table of dot(dir(s), kMoundDir)` (fine at table resolution).
- **F8 (P1) — the coupling rescales the pour period to `car_gap/v_train`** (with gap 9 m, v≈4.2 m/s → ~2.1 s
  = strobey). RAISE the CC5 default `car_gap_m` to **~24 m** so each pot's pour lasts ~6 s and the pots read
  as separated on the crest; the string then pours its `pots` pots one-by-one over ~`pots·gap/v ≈ 46 s` as it
  passes, then dark till the next lap (~909 s) — faithful to the real intermittent pour. **`car_gap_m`,
  `train.rate_hz`, `pots` are now the POUR-CADENCE fly-dials** (document them; expect Chad to tune duty/speed).
- **F9 (P1) — the lava must sample `h(z)`+`y0(z)`**, not nominal `height`: source `y = h(z_rv)+y0(z_rv)`, toe
  X `= +cw/2 + face_run·(h(z_rv)/height)` (else up to ~0.9 m source mismatch vs the 1.5 m lift).
- Hygiene (fold if cheap): F10 the `kSpur[2]↔[4]` chord is ~240 m not 2 km (hairpin apex; through-dir still
  correct — fix the comment); F11 spell `wrapdist` = `d=fmod(a,L); if(d<0)d+=L; if(d>0.5L)d-=L;`; F12 pin the
  Z=0 station noise to the local max if "crest peaks at the dump" must hold (moot once F9 samples h(z)).

---

# CC6 — DUMP ACCURACY PASS (Chad, 2026-07-13). Status: COMPLETE — all AWAITING-FLY.

Chad: *"make it accurate and correct to reference. Use fable for accuracy and build consult with
blender."* + the reposition steer (dump faces NW, open area N of the stack S of Regional Rd 15, track
from the smelter, research the exact geography). Delivered as FOUR staged commits (each gate 413/413,
0 goldens, Fable-vetted, kernel untouched), on top of the earlier CC6-1 ramp:
- **CC6-3 reposition** (`f1630d368`, `world-cc6-reposition`): researched real INCO geography, OSM-
  openness-scanned the anchor to the slag flats at 46.494,-81.0455 (N of stack, off roads); shared
  `render/copper_cliff_geo.h` + `offline_tool/cc_track.py` (killed the dir duplication); axis NE / face NW.
- **CC6-4 terraced mesa** (`9aac92258`, `world-cc6-terraces`): benched black slag MESA via ONE
  single-source `face_profile` (mesh + lava + drape); `[slag] benches` dial; widened/darkened.
- **CC6-2 rails** (`6baaad045`, `world-cc6-rails`): static draped 2-rail + tie track via the shared
  `point_at` (climbs the crest); `point_at` hoisted above `build_train_renderer`.
- **CC6-5 machinery** (`3de3c4720`, `world-cc6-machinery`): Blender glTF steeplecab loco + giant
  tapered trunnion slag pots (`assets/coppercliff/*.glb`), loaded via the fleet path w/ fallback.
NEXT = Chad flies the whole dump (placement / terraces / rails / loco+pots / night pour). Fly-dials:
`cc_track.py` coords, `[slag] benches`/`crest_width_m`/`mound_color`, `[train]` cadence. A good night
pour offset (train at the dump) = SEADS_CEL ~9459 at oblique PAN 20.3 TILT 25.8.

--- original CC6 fix notes (CC6-1 ramp landed earlier; CC6-2/3 above supersede the queued rows) ---
Chad flew the CC4 ridge + CC5 train and gave three fixes. **NEVER touch sim/ or control/.**

**✅ CC6-1 — CLIMBABLE RAMP (DONE, commit `9641a81a6`, AWAITING-FLY).** "Trains full of slag can't
climb a steep hill — extend the length, long ramp up, track lengthwise along the top, down a shallow
hill." Reshaped `ridge_endtaper` in `render/slag.cpp` (the SINGLE height source the mesh + lava + track
drape all sample) into: long gradual ramp up → flat crest (`crest_flat_half=0.22*half_len`) → long shallow
descent. `ridge_length_m` 700→1000 (world.toml), grade ~6.6°. Spur dump-leg routed END-TO-END in
`train.cpp` (`delta = 0.98*half_len/Ra`) so the track enters at the ramp bottom, climbs, and descends.
Reads as a long ridge with sloped ends from above. gate 413/413, 0 goldens.

**⬜ CC6-2 — VISIBLE RAILS (NEXT). Chad: "a track to move on."** There is NO rail geometry — the loco/pots
(`train.cpp` `build_loco`/`build_potcar`/`build_slag_pot`, drawn in `draw_train_renderer`) ride an
INVISIBLE spur. Add a static `build_rails(r, ridge)` mesh: sample the spur every ~4 m via `point_at`
(which drapes on the crest, so the rails climb the ramp too), emit **2 rail ribbons** (thin boxes offset
±gauge/2 ≈ ±0.8 m along `right`, raised ~0.2 m) + **cross ties** every ~6 m; store `Mesh rails` in
`TrainRenderer`, draw it (static, no phase) with the existing `kTrainFS` mono shader. ⚠ `point_at` is in
the SECOND anon namespace (below `build_train_renderer`) — MOVE it (and `wrapdist`) above, or build rails
in a deferred first-draw step, so `build_train_renderer` can call it. Keep vert count < 65535 (ushort idx).

**⬜ CC6-3 — REPOSITION OFF THE ROAD (NEXT). Chad: "it's on the road... move to the MIDDLE of the open
area, keep the spot + orientation."** The dump anchor `kMoundDir` is DUPLICATED: `slag.cpp:18` (`kMoundDir`
+ `kSpur2`/`kSpur4` for the axis) AND `train.cpp:24` (`kSpur[3]`). Nudge the WHOLE complex by ONE small
tangent offset (same delta on `kMoundDir`, `kSpur[2..4]`) off the adjacent road into the open middle,
keeping the axis/orientation. Get a clean daylight recon render first to pick the nudge dir/dist (the
Copper Cliff area is dim at offset 120 — sweep offsets; frame with `SEADS_OBL_PAN=25.95 TILT=30.44`). This
is fly-tunable — ask Chad "which way off the road" for the exact steer. Consider hoisting these dirs to a
single shared constant so the nudge is one edit, not three.

---

# CC7 — SLOW BRAIDED SLAG POUR + church setback (Chad, 2026-07-14). Status: COMPLETE — AWAITING-FLY.

Chad flew CC6 and asked: make the slag pour look like a **BRAIDED river** down the black hill, and make
it **OOZE slowly** like real earth-time (it "raced down in ~1s… slag is slower to move down a hill… make
it look more like real earth time"). Plus a separate ask: push the **Chelmsford St-Joseph church BACK off
Errington** into its lot. The pour is PROCEDURAL (a shader + mesh in `render/slag.cpp`, draped on the
shared terraced-face profile) — NOT a Blender asset; Blender can't drive flow that stays glued to the
terraced face, so this is code/shader, done procedurally with Chad's OK. Fable BEFORE (SOUND-WITH-FIXES)
+ AFTER (SOUND), all folded; gate 413/413, 0 goldens, kernel + cross-agent files untouched.

- **✅ Church setback** (`f418db6e8`, `world-church-setback`): `setback_m` 22→45 in
  `offline_tool/sudbury_hero_place.py`; regen `render/sudbury_hero.gen.h` (lock 0x99061E1583D34607). The
  church shifts back along −fwd into its lot → a clear front-lawn buffer before Errington Ave N. Fly-dial.

- **✅ CC7 braided pour** (`93ad60f5a`, `world-cc7-braided-pour`):
  - **Timing (lava FS, `kLavaFS`):** the front advances a CONVEX map `pow(vD,1.6)` (fast off the lip,
    crawling at the toe — real slag thickens/cools as it spreads) over phase `[0.06,0.72]` = **0.66 of the
    cycle** (was 0.35) → ~2× the on-face descent time for a fixed cadence. Brief fed plateau (`p_tail =
    p_arr + 0.06`) then exp cool (`τ = 0.055`). WRAP-SAFE (Fable): latest extinction `p = 0.973 < 1`,
    every vD discarded at `p=0` → face fully dark on `(0.973,1) ∪ [0,0.06)`, no pop at the frac wrap.
  - **Braid (`build_lava_mesh`):** parallel meandering strips → an anastomosing braid. Each thread WEAVES
    laterally (`braid_amp·sin`, per-thread FREQ JITTER via a fixed `hash(rv)` + alternating `fan_dir` so
    threads don't move in lockstep), adjacent threads share overlapping lateral ranges so they CROSS, and
    the width PULSES down the face (`0.55 + 0.45·sin`) → pinch (split → dark crust bar) then swell
    (merge). Per-thread lift `ε·rv` (≤0.14 m) kills coplanar z-fight at crossings. Every vertex samples
    the SHARED single-source face profile (`ridge_crest_h`/`ridge_y0`/`face_profile_at`) so all threads
    stay glued to the terraced face (no float); `texcoord.x = vD` unchanged so the single-clock FS is
    untouched. `rivers` clamped [1,64] (ushort indices, Fable-AFTER P2). `[slag] rivers` 5→8.
  - **Cadence:** `[train] car_gap_m` 24→36 lengthens each pot's pour so the slow ooze reads (train speed
    unchanged); `train.cpp` warns if a `car_gap` crank pushes the pour window past the spur (ghost pours).

**NEXT = Chad flies both.** Pour SPEED is still capped by the per-pot cadence (single-clock design); if
he wants it slower still, raise `[train] car_gap_m` or lower `[train] rate_hz`. The standalone continuous
pour (`SEADS_NO_TRAIN=1`, ~50 s cycle, always active) is the cleanest way to judge the braid. Evidence:
`shots/cc7_braided_pour_night.png` + `shots/cc7_braided_pour_crest.png`.
