# STEREOSCOPE SUDBURY — the long-view world buildout (Fable-consulted)

*Executed via the dedicated `stereoscope-sudbury` skill (built per `docs/stereoscope_skill_brief.md`).
The current tree is a transitional V2 checkpoint (B&W palette, roads-as-lines, relief 350, datum-matched
blend, cubemap 4096, the `SEADS_OBLIQUE` debug cam) still on the OLD enlarged projection — **S0's first job
is to replace it with true 1:1 + the wilderness cap.** Raw DEM cached → S0 re-bake is fast.*

## Context

Chad flew the V2 Sudbury bake and rejected it on fundamentals: the azimuthal-equidistant projection
**enlarged the 28 km basin 1.68× to force-fill the sphere**, producing "watermelon striations" at the
antipodal pinch + a stretched, low-res far field. The deeper redirect: he wants the world to feel like
**flying low through Chelmsford (his hometown) as if through an old black-&-white STEREOSCOPE** — hyperreal
depth, 1940s silver-gelatin monochrome, a world you fly *through* (terrain, **trees**, town **buildings**,
**roads + snowmobile trails**), good-looking *now* on a staged path to *exquisite*. Blender is in hand;
long-term he'll build 1940s-Sudbury-photo period buildings. Planes stay the only color.

This plan is the product of **deep web research** (6-section cited technique report, saved at
`soft-cuddling-cat-agent-af38feb78bb85ebe2.md` → move to `docs/world_stereoscope_research.md` on approval)
and a **Fable consult** (`soft-cuddling-cat-agent-ac40c33aad1c2975c.md`) that ruled the hard decisions with
the math and grounded them against the real tree. Key enabling fact (Explore-confirmed): the runtime is
**projection-agnostic** (samples the cubemap by `fragDir`, DEM via fixed `equirect_uv`), so the projection
fix is **offline-only**; `render/` doesn't change for it.

## Rulings (Fable, with the math)

- **A · Projection = azimuthal-equidistant at TRUE 1:1** (`GROUND_R := R_PLANET = 15000`, no enlargement).
  The 28 km region maps to a **107° cap (~65% of the sphere)**; the other 35% is **procedural
  sphere-native wilderness** (3D-fBm by `fragDir`) — no projection exists there, so **the pinch is
  unrepresentable**. Radial distances exact; edge distortion a mild `k_t=0.51` (vs V2's →0 collapse).
  Stereographic rejected (covers only 47%, shrinks towns); Lambert rejected (7.8:1 edge slivers).
- **CENTER = the Sudbury–Chelmsford midpoint (~46.556, −81.11)** [Chad]. Both the city core and Chelmsford
  sit at ρ≈7.5 km → `k_t=0.96`, squeeze invisible on both street grids. Far lakes (Wanapitei, Vermilion)
  fall into the wilderness blend — accepted (blobs survive squeeze; street grids don't). Capture stays 28 km.
- **B · Depth = a real rlgl depth-TEXTURE FBO** (raylib shadowmap example is the ~20-line template; GL 3.3
  core). Precision sub-meter through the focus band. **FXAA as post pass 0** (a RenderTexture loses MSAA
  anyway; soft edges read "photograph"). Fallback: aerial-perspective (cue 1) computes per-fragment from eye
  distance and ships even without the FBO; only DoF + clarity need the depth texture.
- **C · Resolution = keep 8192 equirect (11.5 m texel).** 1:1 gives back the hero texel but that's fine —
  everything that needed texels is LEAVING the albedo (roads→ribbons, buildings→meshes, trees→instances).
  Crispness = the **M2 normal map** (1 m HRDEM slope as shading) + a **512² tiling Shield-rock detail layer**
  (blend by land class, ~15 lines) + geometry silhouettes. 16384 rejected (4× memory, bake-OOM risk).

## The stages (beauty-per-effort × dependency; every fly-gate is flown OVER CHELMSFORD)

### S0 — Projection re-bake + wilderness cap  ← BUILD THIS WEEK
Foundation; kills the striation. `sudbury_config.py`: `GROUND_R := R_PLANET`, recenter to the midpoint,
`R_MAX` stays 28000 (now decoupled from sphere-fill), `RELIEF_SCALE_M` loses its ×ENLARGE justification →
pure fly-dial (keep 350, re-fly). `sudbury_geo.py`: `theta = rho/R` (drop GROUND_R enlargement). New bake
module: **wilderness fBm cap** (3D-noise-by-direction) with a **content-blend annulus over ρ∈[24,28] km**,
seam statistics matched to the HRDEM ring. Extend the geo self-test with `k_r/k_t` bounds. Re-emit PNGs +
`sudbury_gis.gen.h` (raw DEM cached → fast). **Gate:** Chad flies Chelmsford→downtown at deck (true-size
towns, no striation) + the outer ring. **P0: nothing that stores a sphere direction (trees/ribbons/building
footprints) may bake before S0 lands** — a projection change re-bakes every stored dir.

### S1 — LOCK THE LOOK: the B&W stereoscope post stack + depth FBO
Highest beauty-per-effort — upgrades everything at once; **assets can't be judged until tone is locked.**
New full-screen `RenderTexture` chain in `app/main.cpp`/`render/draw.cpp` + the depth FBO. Pass order:
**FXAA → filmic S-curve (ACES) → silver split-tone → aerial perspective → warm halation → film grain →
vignette**; **DoF + depth-gated clarity behind flags** (enabled in S6). *Gemini writes the verbose GLSL;
Fable red-teams colorspace + order.* Wire the loaded-but-unused `[tone]` block as the post pass's data.
- **P0: DO NOT full-frame Rec.709 desaturate** — the scene is authored-mono and the planes are the only
  chroma; desaturating kills the vision. Saturation-GATE the split-tone: `mix(splitTone(lum), rgb, sat)`.
- **P0: no double-tone** — the post pass is the ONE owner of contrast; the `[tone]` per-material stub must
  not also apply it. **Trap: HUD/reticle draw RAW, AFTER the post pass** (SPEC §9.2 — grain on the reticle
  injects noise into the control loop).
- **Gate:** Chad locks the tone once, over Chelmsford; never re-litigate per-asset.

### S2 — Boreal trees (the "populated world" leap)
`world/props.*` (data) + `render/props.*` (instanced draw). **Cross-quads, NOT billboards** (billboards
tumble under sphere-roll). Density raster baked in S0; **hash-Poisson per cube-face cell** (integer
`(face,i,j)` + seed, never a float dir); **per-CHUNK cull** (~96 cells, one `DrawMeshInstanced` each, ~10-20
visible < the 24-call ceiling); `pos = dir·radius_at(dir)`, `up = dir`. Sudbury moonscape read: sparse
spruce-spikes + jack pine on blackened rock + pale birch, a **density GRADIENT** (denser valley pockets).
*Blender authors the cross-quad tree kit → glTF validator gates it.*
- **P0: `DrawMeshInstanced` count==1 / attrib-divisor / default-shader bugs ship GREEN** — runtime assert
  `instances>1` + `--smoke` + source validator (the repo's own recurring trap class).
- **P1:** trees must read the SAME HeightField as the mesh or they float; **P1:** uniform density kills it.

### S3 — Linework ribbons (roads · trails · rivers)
Geometry **ribbons draped on `radius_at` + ~1 m radial lift** (not decals/baked — only ribbons give
arc-length UVs for dashes). Miter joins + bevel fallback >~40°; **casing** (wide dark bed under narrow light);
FS dashes via `fract(cumulative_arclength/(dash+gap))`. Roads = matte-black bed + light dashed centerline;
**snowmobile trails = thin LIGHT-GREEN dashed** (the sanctioned color accent, OSM `route=snowmobile`, 35
elements); rivers = tapered. Remove the road edge-lines from the albedo when this lands.
- **P1: compute arc-length on the FINAL sphere polyline** (`R·Δangle`), not ground metres, or dashes stretch
  under the tangential compression.

### S4 — Building massing (pass 1)
Bake-time OSM footprint **extrusion** (`levels×3 m` + a type-keyed default table — ~97% are untagged), 3 roof
archetypes, **hybrid batching** (instance an authored kit + merge the unique-prism tail per tile). Emit a
catalog of `(dir, BAKED tangent frame, dims)` — frames from the projection bearing **at bake time**, never a
runtime world axis (seam-safe, street-aligned). *Small extruder in `offline_tool/` reading the same GIS.*
**Gate:** Chelmsford reads as blocks and streets from the air.

### S5 — Hero silhouettes (makes it read as THIS place)
*Blender / Chad via the existing addon + glTF validator:* a mine **HEADFRAME** (the defining Sudbury icon),
a steepled **French-Canadian parish church** (Ste-Anne-des-Pins archetype — the *earlier* steepled church,
not the 1995 rebuild), a **smelter stack + slag** (Superstack only if "timeless Sudbury" beats strict-1940s).
Compose **Kurosawa 3-plane** sightlines: near tree / mid town / far headframe-on-ridge. **Gate:** the "that's
Sudbury" dusk-silhouette moment.

### S6 — Polish → exquisite
Enable + tune the full **depth trio** (DoF + clarity — the stereoscope payoff), tree wind (GPU-Gems port),
an **ink-outline** experiment, a **"printed card" 1-bit dither MODE** (off by default — hard dither fights
smooth silver), density staging, halation dial-in. **Chad's period 1940s-photo Blender buildings slot in
here and forever after** (photogrammetry hero-only, long-term).

## What makes it EXQUISITE vs merely good (guard these)
1. **The depth trio** — the stereoscope IS hyperreal depth; grayscale alone is just a filter (why S1 locks depth).
2. **Density gradients + staged sightlines** — uniform scatter is the fastest route to "asset-store demo."
3. **A silver PRINT, not a filter** — saturation-gated split-tone + warm halation off the lake glints.

## Top 3 effort-wasting traps
1. **Baking any placement before S0 locks the projection** (every stored dir re-bakes on a projection change).
2. **Post-chain colorspace / double-tone** (desaturating the planes, or stacking `[tone]`+post) — poisons
   every gate after S1 and gets misblamed on the assets.
3. **Green-gate blindness on instancing/post** (count==1, undefined FS inputs, pass-order) — smoke + validator
   + the `instances>1` assert are stage DELIVERABLES, not afterthoughts.

## Critical files
- **Offline:** `offline_tool/sudbury_{config,geo,fetch,bake,header}.py`, `build_sudbury.py` (S0 projection +
  wilderness cap; S3/S4 linework/footprint export; S2 density raster).
- **Runtime NEW:** `render/props.{h,cpp}` + `world/props.{h,cpp}` (S2 trees, S4/S5 buildings), a post-pass
  module + depth FBO in `render/` (S1), a ribbon builder (S3).
- **Runtime touched:** `app/main.cpp` / `render/draw.cpp` (post chain + HUD-after-post), `render/planet.cpp`
  (detail layer, M2 normal map), `config/world.toml` (`[tone]`, `[scatter]`, new blocks).
- **Assets:** `assets/sudbury_{dem,color,landmask,normal}.png`, Blender glTF kits (trees, buildings, heroes).
- **Gate:** `test/unit/test_load_world.cpp`, `test_sudbury_gis.cpp`, `test_asset_validator.cpp` (extend per
  ingested mesh + the `instances>1` assert).

## Process (per stage — the /orchestrate discipline)
plan → *(Fable before-consult at ★ math: projection self-test, post colorspace, ribbon arc-length)* →
Gemini for verbose assets (GLSL, mesh kits, terrain) / Blender for hero models / Opus for the seam+integration
→ gate stack (asset validator → build → shaders → seam grep → `--smoke` + `instances>1` assert → flight
`ctest`, LAST, **zero moved goldens**) → **Fable after red-team** → **Chad flies over Chelmsford** → tag →
`/clear` → next. The flight kernel (`sim/`+`control/`) is frozen; the world thread never touches it.

## Verification
1. **S0:** geo self-test (round-trip <1e-6, `k_r/k_t` bounds, chirality); `--smoke SEADS_OBLIQUE=1` over the
   midpoint + the outer ring/wilderness seam; Chad flies.
2. **Each stage:** build + GLSL 330 compile + asset validator + seam grep + flight `ctest` (zero moved
   goldens) + a `--smoke` shot (the green gate is blind to the binary) + one scoped Fable red-team + Chad's fly.
