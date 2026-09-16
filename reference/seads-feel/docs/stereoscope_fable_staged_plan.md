# STEREOSCOPE SUDBURY — staged world/art buildout (Fable consult, 2026-07-09)

## Rulings first (A/B/C), then the stages.

---

## RULING A — PROJECTION: aeqd at TRUE 1:1 (GROUND_R = R_PLANET = 15000), edge at θ≈107°, procedural wilderness cap for the remaining 35%. Lambert is OUT. Stereographic is the fallback only if Chad's fly says "shapes wrong."

**The honest math.** No flat 28 km disk maps onto a positively-curved sphere without distortion; the only choice is WHERE and WHAT KIND. For any azimuthal map θ(ρ): radial scale k_r = R·θ'(ρ), tangential k_t = R·sinθ/ρ. The three canonical picks:

| ρ (ground) | aeqd 1:1 (θ=ρ/R): k_r / k_t | stereographic (θ=2atan(ρ/2R)): iso k | Lambert (θ=2asin(ρ/2R)): k_r / k_t |
|---|---|---|---|
| 5 km  | 1.00 / 0.98 | 0.97 | 1.01 / 0.99 |
| 10 km | 1.00 / 0.93 | 0.90 | 1.06 / 0.94 |
| 15 km (Chelmsford from current center) | 1.00 / 0.84 | 0.80 | 1.15 / 0.87 |
| 20 km | 1.00 / 0.73 | 0.69 | 1.29 / 0.78 |
| 28 km (edge) | 1.00 / 0.51 | 0.53 | 2.79 / 0.36 |

- **Lambert equal-area**: edge aspect ratio 7.8:1 — watermelon slivers return at the rim. Rejected.
- **Stereographic**: shape-perfect (conformal) but the 28 km disk only reaches θ=86° → covers **47%** of the sphere, leaving 53% wilderness — against "all towns hero + 300 lakes"; and towns shrink isotropically (Chelmsford at 0.80x reads small and crosses fast). Fallback only.
- **aeqd 1:1**: radial distances EXACT (travel time center↔edge honest), disk edge at θ = 28/15 rad = **106.95°** → the map covers (1−cos θ_edge)/2 = **64.6%** of the sphere; wilderness cap = 35% (a 73° cap about the antipode). Worst tangential squeeze at the map edge is a finite, mild 0.51 — the V2 striation was k_t→0 AT the pinch (the whole 2π·28 km rim converging to a point); at 1:1 the map *terminates* at k_t=0.51 and the cap is generated sphere-native (3D fBm by fragDir, no projection at all). **The pinch is unrepresentable. Striation confirmed dead.**
- The equirect container poles (±Y) land at θ=90° — mid-map oversampling, harmless (denser texels, no singularity in content).

**The one open sub-decision (ask Chad):** center. Today's center (downtown/Ramsey) puts Chelmsford at ρ≈15 km → 16% tangential squeeze on the hometown street grid — borderline visible from overhead. Recenter to the Sudbury–Chelmsford midpoint (≈46.556, −81.11) and BOTH sit at ρ≈7.5 km (k_t=0.96, invisible) — but Wanapitei Lake (already ρ≈30+ km, edge-case today) falls to the wilderness blend or forces CAPTURE_RADIUS_M→~32 km (edge k_t drops to 0.40). **Recommendation: recenter to the midpoint; Wanapitei is a blob and blobs survive tangential squeeze, street grids don't.** Chad's call.

**Implementation is a two-constant diff + one new module** (this is why it's the week-1 item): `sudbury_config.py` GROUND_R := R_PLANET (kill the /π), R_MAX stays 28000 (now decoupled from "fill the sphere"); `sudbury_geo.py` forward/inverse unchanged (θ=ρ/GROUND_R already); NEW: wilderness generator in the bake — for θ > θ_fade blend DEM+albedo from real data to sphere-native fBm over ρ∈[24,28] km (match mean elevation + amplitude to the HRDEM ring statistics at the seam; drop non-hero OSM water beyond 24 km). RELIEF_SCALE_M: the V2 ×ENLARGE justification (C1-P1-2) vanishes at 1:1 — it's now a pure theatrical fly-dial; keep 350, re-fly.

## RULING B — DEPTH: custom rlgl depth-TEXTURE FBO. One pass. No linear-depth pre-pass, no MRT. MSAA is dead anyway → FXAA as post pass 0.

- raylib already exposes the whole path: `rlLoadFramebuffer()` + `rlLoadTextureDepth(w,h,useRenderBuffer=false)` + `rlFramebufferAttach(...RL_ATTACHMENT_DEPTH...)` — the raylib shadowmap example is the exact template. ~20 lines for `LoadRenderTextureDepthTex()`. GL_DEPTH_COMPONENT24 textures are core GL 3.3. **Low risk.**
- **Precision math (why no pre-pass):** with n=0.5 m, f≈2.5e6 m, 24-bit hyperbolic depth quantizes at dz ≈ z²/(n·2²⁴): **0.03 m @ 500 m, 0.48 m @ 2 km, 107 m @ 30 km**. The three cues need: DoF focus band 50 m–2 km (sub-meter: fine), near-clarity <1.5 km (fine), aerial-perspective ramp 0.5–25 km (107 m steps on an exponential tail, under grain: invisible). A second linear-depth pass would double draw calls against the 8 ms budget for zero visible gain. Rejected.
- **MSAA:** any RenderTexture loses FLAG_MSAA_4X_HINT regardless. Ruling: accept it; FXAA as the first post pass + film grain. On-brand — jaggies read as "video," soft edges read as "photograph."
- **Fallback (needs no depth in post):** cue (1) aerial perspective computes per-fragment in the terrain/prop shaders (they know world pos → eye distance). It ships even if the FBO fights. Only DoF + clarity require the depth texture.

## RULING C — RESOLUTION: KEEP 8192 equirect / 2048-per-90° (11.5 m texel). 16384 rejected. Non-uniform/virtual texturing rejected. Detail moves to the normal map + a tiling detail layer + GEOMETRY.

- Honest cost of Ruling A: 1:1 drops the hero ground texel from 6.8 m (V2's 1.68x zoom) back to **11.5 m**. That's fine BECAUSE the things that needed texels are leaving the albedo: roads → geometry ribbons (S3), buildings → meshes (S4), trees → instances (S2). What remains is the land-cover VALUE ladder + rock/lake masks — 11.5 m is ample for value fields.
- Close-up crispness comes from: (a) the **M2 object-space normal map** (already planned, same 8192) baked from the 1 m HRDEM — sub-texel slope detail as SHADING; (b) a **512² tileable Shield-rock/bush detail layer** blended by land class at high frequency in the terrain FS (~15 lines, classic, cheap); (c) geometry silhouettes.
- 16384 = 4x memory (~400 MB class), the bake's own config notes OOM risk at source 2 m, and the payoff is one octave on a layer that's about to stop carrying detail. The stereoscope look is CONTRAST + DEPTH CUES + silhouettes, not texel count — 1940s prints are soft and grainy. Over-engineering; rejected.

---

## THE STAGES (beauty-per-effort × dependency order; every Chad-fly gate is flown OVER CHELMSFORD — the vertical slice is the standing gate, per the research's lock-look-early rule)

### S0 — V3 PROJECTION RE-BAKE + WILDERNESS CAP  ← BUILD THIS WEEK
- **Goal:** kill the rejected striations; true 1:1; everything downstream bakes against a STABLE geo→dir.
- **Build:** GROUND_R=15000; wilderness fBm cap + seam blend (ρ 24→28 km); re-emit PNGs + `render/sudbury_gis.gen.h`; extend `sudbury_geo._selftest` with k_r/k_t bound checks + the θ_edge invariant. Runtime untouched (projection-agnostic seam holds).
- **Roles:** Opus drives the bake; Fable checks the seam-statistics match + selftest bounds; no Gemini/Blender.
- **Gate:** Chad flies Chelmsford→downtown at deck height (scale feel — towns now TRUE size, smaller than V2's 1.68x) + the outer ring (striations gone?). Rules the CENTER question (midpoint vs downtown).
- **Traps:** P0 — nothing that stores a sphere dir (trees, ribbons, footprints) may bake before this lands; a projection change invalidates every placement. P1 — u_offset / chirality re-verify with a landmark (the memory ruling).

### S1 — THE LOOK LOCK: post chain + depth FBO (highest beauty-per-effort in the plan — upgrades everything already built at once, and trees/buildings can't be judged until the tone is locked)
- **Build:** depth-texture FBO helper (Ruling B); post chain in order: **FXAA → filmic S-curve (ACES) → silver split-tone → aerial perspective (depth) → halation (warm) → grain (animated, luminance-gated) → vignette**; DoF + near-clarity implemented but behind config flags (tuned in S6). Aerial perspective ALSO as the in-material fallback.
- **Roles:** Gemini generates the verbose post GLSL (its lane); Opus integrates the FBO + pass plumbing; Fable red-teams colorspace/order + the linearization.
- **Gate:** Chad flies the SAME S0 terrain under the new tone and LOCKS it. Everything after is judged under this look — never re-litigate per-asset.
- **Traps:** **P0 — full-frame Rec.709 desaturate KILLS the planes-are-the-only-chroma vision.** The scene is already authored-mono; DROP the research's grayscale step, gate the split-tone by saturation (`mix(splitTone(lum), rgb, sat)`) so mono pixels get silver toning and the planes/scatter/aurora pass through. **P0 — double-tone:** the loaded-but-unused per-material `[tone]` stub and the post pass must not BOTH apply contrast; the post pass becomes the ONE owner, bypass the stub. P1 — pass order (grain AFTER tonemap, halation BEFORE grain) or it bakes mud that gets blamed on assets. P1 — the green gate is blind to all of this: `--smoke` shot + a source validator for the pass chain, per mechanism.

### S2 — TREES (the single biggest "populated world" jump)
- **Build:** cross-quads (2–3 intersecting quads — billboards tumble under sphere-roll, already ruled); density raster from OSM landuse folded into the S0 bake; placement = deterministic hash-Poisson per cube-face cell (seed = `[scatter]` 1337, INTEGER cell hash, never clock); species silhouettes (black-spruce spike / jack pine / pale birch) as atlas variants; per-CHUNK horizon cull (~96 cells, one DrawMeshInstanced per visible chunk, ~10–20 visible — inside max_draw_calls=24 with room); position = dir·radius_at(dir), up = dir.
- **Roles:** Gemini: instancing GLSL + the placement catalog code; Blender/Chad optional quad-atlas art; Fable: Poisson determinism + chunk-cull margin math; Opus integrates.
- **Gate:** Chad flies the Chelmsford treeline at deck height — the moonscape GRADIENT (sparse conifers on blackened rock near the smelters, thickening away) must read.
- **Traps:** **P0 — DrawMeshInstanced count==1 / attrib-divisor bugs ship GREEN**: runtime assert instances>1 in the props draw + smoke shot + source validator. P1 — trees must sample the SAME HeightField the mesh uses or they float/sink. P1 — UNIFORM density is the killer of the look (research §5): the density raster must carry gradients + clearings, never a constant.

### S3 — LINEWORK: draped geometry ribbons (roads / snowmobile trails / rivers)
- **Build:** OSM polylines → draped ribbon meshes (radius_at + ~1 m radial lift against z-fighting); per-vertex cumulative arc-length UVs; casing (wide dark under narrow light) for hierarchy; FS dash via fract(s/(dash+gap)); trails = thin light-green dash (the sanctioned accent); rivers tapered. **Remove road edge-lines from the albedo bake when this lands** (no double roads).
- **Roles:** Gemini: ribbon tessellation + miter/bevel code; Fable: arc-length + join math; Opus integrates.
- **Gate:** Chad follows a road low from Chelmsford toward town; a snowmobile trail into the bush.
- **Traps:** P1 — compute cumulative arc-length on the FINAL sphere polyline (R·Δangle), not in ground meters, or dashes stretch across the tangential compression. P1 — miter spikes at hairpins: bevel fallback above ~40°.

### S4 — BUILDINGS pass 1 (mass, not heroes)
- **Build:** bake-time OSM footprint extrusion (levels×3 m + type-keyed defaults for the ~97% untagged); 3 roof archetypes; hybrid batching — INSTANCE the authored kit for stock, MERGE the unique-prism long tail per tile; compiled catalog of (dir, baked tangent frame, dims) — **frames baked from the projection bearing, never derived from a runtime world axis** (seam-safe, and buildings stay street-aligned).
- **Gate:** Chad flies over Chelmsford and the town READS as blocks, streets, a place.
- **Traps:** P1 — the count==1 trap again (separate validator leg). P1 — over-detailing: prisms + roofs ONLY; detail is S5's job and the post chain's job.

### S5 — HERO SILHOUETTES (what makes it THIS place)
- **Build:** Blender (Chad authors, existing addon + the headless glTF validator gates): MINE HEADFRAME, French-Canadian parish church (Ste-Anne archetype — Chelmsford's own St-Joseph works too), smelter stack + roast-yard/slag (Superstack only if Chad rules "timeless" over strict-1940s). Placed via gen.h landmarks at `[scatter]` furniture_scale, staged on ridgelines/sightlines (Kurosawa 3-plane: near tree / mid town / far headframe-on-ridge).
- **Gate:** Chad's "that's Sudbury" moment — the horizon silhouette test at dusk.

### S6 — POLISH → EXQUISITE
- **Build:** enable + tune DoF and near-clarity (the full stereoscope trio); GPU-Gems wind on the tree quads; the ink-outline experiment; the 1-bit "printed card" dither MODE (off by default — it fights smooth silver); a density-staging pass (focal clusters, deliberate emptiness); halation/split-tone final dial-in. Period buildings from Chad's 1940s photos slot in HERE and forever after — the pipeline is already proven by S5.

**Every stage:** full gate (build+ctest, zero moved goldens) + `--smoke` shot + source-level validator + ONE scoped Fable red-team round, then Chad flies. Kernel firewall absolute throughout.

---

## What makes it EXQUISITE vs merely good
1. **The depth trio** (aerial perspective + DoF + near-clarity) — the stereoscope IS hyperreal depth; grayscale alone is just a filter. This is why S1 locks before any asset lands.
2. **Density gradients + staged sightlines** — sparse moonscape → treeline → town cluster → headframe on the ridge. Uniform scatter is the fastest way to "asset-store demo."
3. **The silver print, not a grayscale filter** — split-tone (cool shadows / warm highlights) + warm halation blooming off lake glints and the sun rim. That's the silver-gelatin signature.

## Top 3 effort-wasting traps (ranked by blast radius)
1. **Baking ANY placement data before S0 locks the projection** — every tree/ribbon/footprint stores a sphere dir; a projection change re-bakes the world. S0 first, nothing dir-storing in parallel.
2. **Post-chain colorspace/order + double-tone** — full-frame desaturate kills the chroma planes; `[tone]` stub + post both applying contrast bakes mud attributed to assets and poisons every gate after S1. One tone owner.
3. **The green-gate blindness on instancing/post** (the repo's own recurring trap) — count==1, undefined FS inputs, and pass-order bugs all ship green. Per-stage smoke + source validator + the instances>1 assert are non-negotiable stage deliverables, not afterthoughts.
