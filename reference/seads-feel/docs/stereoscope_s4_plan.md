# STEREOSCOPE SUDBURY — S4 building massing (design + Fable ledger)

Mirrors the S3 ribbon machinery (`sudbury_ribbon.py` + `render/ribbons.*`). Buildings are baked
extruded prisms (real OSM footprints) with **flat / gabled / hipped** roofs keyed by type (Chad's
S4 ruling, 2026-07-11). Sun-lit MONO massing (planes stay the only chroma; the S1 post silvers it).
Fly-gate: Chelmsford reads as blocks + streets from the air.

## Seam (identical to S3 — NO `world/` module, no Gemini; like the water pass)
- **`offline_tool/sudbury_fetch.py`** → `fetch_buildings(frame)`: OSM `building=*` ways + relation
  outers, carrying `building`, `building:levels`, `height` tags. Returns `[{poly (lon/lat), btype,
  levels|None, height_m|None}]`. Mirrors `fetch_landuse` ring-assembly.
- **`offline_tool/sudbury_building.py`** (new, sibling of `sudbury_ribbon.py`): one footprint →
  in-disk aeqd → walls + roof solid → per-vertex `(dir, h)`; outward CCW winding. Projection math
  stays HERE (every dir via `frame.aeqd_to_dir` — the ONE formula, no fork).
- **`offline_tool/sudbury_header.py`** → `_build_building_batches` + emit (mirrors
  `_build_ribbon_batches`): bin buildings into `<BUILDING_BATCH_MAX_VERTS` ushort meshes →
  `GisBuildingVertex[]` / `GisBuildingIndices[]` / `GisBuildingBatch[]` in `render/sudbury_gis.gen.h`.
- **`render/buildings.{h,cpp}`** (sibling of `ribbons.*`): upload batches at `dir*(radius_at(dir)+
  h*height_scale)` (SAME height field the terrain/trees/ribbons drape on — anti-float); sun-lit mono
  FS. `SEADS_NO_BUILDINGS=1` A/B. Drawn after ribbons (opaque, depth-writing), before the translucent
  trees.
- **`config/world.toml [buildings]`** + `config/load_world.{h,cpp}` + `app/main.cpp` wiring +
  `render/draw.{h,cpp}` `BuildingBuildParams` — all mirror `[ribbons]`.

## Height model (metres; the ~97% untagged resolve by the default table)
`height_m = height tag` → else `building:levels × LEVEL_M (3.0)` → else `DEFAULT_H[btype]`. Runtime
multiplies by `[buildings] height_scale` (a vertical-exaggeration fly-dial; no re-bake).
DEFAULT_H (m): house/detached/semidetached/bungalow 6 · residential/apartments/terrace 9 ·
commercial/retail/office/civic/public/school 8 · industrial/warehouse/factory 10 · church/chapel 12 ·
garage/shed/hut 3 · yes/other 6. `height` clamped to `[H_MIN 2.5, H_MAX 60]`.

## Roof archetype keying (by type, footprint area as the override)
- **flat** — commercial/retail/office/industrial/warehouse/civic/public/school/apartments, OR any
  footprint area > `FLAT_AREA_M2` (400) regardless of type (big footprints have flat roofs).
- **gable** — house/detached/semidetached/terrace/church/chapel (church = tall gable; the steeple is
  an S5 hero, not here).
- **hip** — bungalow/cottage, and a per-building hash split of the generic `house`/`yes` residential
  tail so a block isn't uniform (≈35% of small residential → hip, rest gable). Deterministic hash of
  the OSM id (no clock).
Degenerate footprints (min-rect half-length `Lh` or half-width `Wh` < `ROOF_MIN_HALF` 2.0 m, or
< 4 corners after simplify) fall back to **flat**.

## Geometry — everything in the aeqd plane, then `aeqd_to_dir` per vertex
Given a footprint ring simplified (`BUILDING_SIMPLIFY_M` 0.8) + oriented CCW (shapely `orient`), the
closing dup dropped → `N` corners `P_i` (aeqd metres). Eave height `he = height_m`.

1. **In-disk gate:** if ANY `P_i` has `hypot(X,Y) > R_MAX`, DROP the whole building (aeqd folds past
   the disk edge → garbage dirs — the same rule ribbons/water use). All-or-nothing (buildings are
   small; no split).
2. **Walls** (per edge `i→i+1`): base verts `(P_i, h=0)`, top verts `(P_i, h=he)`. Two triangles
   `(b_i, b_{i+1}, t_{i+1})`, `(b_i, t_{i+1}, t_i)`. Winding chosen so the outward horizontal normal
   points AWAY from the footprint centroid (CCW ring ⇒ left normal `(-dy,dx)` is outward). Base and
   top share the aeqd point ⇒ the wall is exactly radial (vertical).
3. **Eave cap** (flat attic floor at `he`): ear-clip triangulate the footprint (shapely
   `triangulate`/`ops` on the polygon), all verts `h=he`, wound so the normal points radially OUT
   (up). ALWAYS emitted (even for gable/hip) so the box is watertight where footprint ≠ min-rect.
4. **Roof solid** (gable/hip only) sits on the **min-rotated rectangle** of the footprint. From
   `poly.minimum_rotated_rectangle` corners → center `c`, unit long axis `u`, unit short axis `w`,
   half-length `Lh` (along `u`), half-width `Wh` (along `w`), `Lh ≥ Wh`. Rise
   `rr = min(ROOF_PITCH·Wh, ROOF_MAX_RISE)` (`ROOF_PITCH` 0.7 ≈ 35°, `ROOF_MAX_RISE` 6 m). Eave rect
   corners at `h=he`: `E±±  = c ± Lh·u ± Wh·w`.
   - **Gable:** ridge endpoints `R± = c ± Lh·u` at `h=he+rr`. Two sloped rectangles
     `(E+-,E++,R+,R-)`-style long faces (eave long-edge → ridge) + two gable-end triangles
     `(E-±... , R-)` / `(E+±..., R+)`. (Exact corner ordering in code; winding = outward.)
   - **Hip:** ridge shortened to `R± = c ± max(Lh−Wh, 0)·u` at `h=he+rr`. If `Lh−Wh ≤ ε` the ridge
     collapses → a **pyramid apex** at `c` (single point) and 4 triangles. Otherwise 2 trapezoids
     (long sides, eave long-edge → ridge segment) + 2 hip triangles (short-edge eave corners → the
     near ridge endpoint).
   The roof base coincides with the eave-cap plane only along the eave lines (measure-zero) — the
   sloped faces leave `h=he` immediately, so no coplanar z-fight with the cap.
5. Every vertex: `dir = frame.aeqd_to_dir(X,Y)`, store `(dir, h)`. Per building, a value jitter
   `bj ∈ [1−BJ, 1+BJ]` (BJ 0.12) from the id hash, stored per vertex → adjacent blocks differ in
   tone so the town reads as distinct massing.

## Normals — derived in the FS, NOT baked (the de-risk)
`N = normalize(cross(dFdx(worldPos), dFdy(worldPos)))` gives an exact **flat per-facet** normal (the
hard-plane stereoscope look) for free. Flip toward the eye so it's outward for the convex exterior:
`if (dot(N, fragToEye) < 0) N = -N;` where `fragToEye = normalize(-vPosEyeRel)` (we render
eye-relative, so `worldPos − eye = vPosEyeRel`; derivatives are eye-invariant). Roof-vs-wall value
break keys off `up = normalize(vPosEyeRel + uEye)`: `roofness = smoothstep(0.35,0.75, dot(N, up))`.
No baked normal field, no tangent Jacobian. Winding still must be outward CCW so backface culling
keeps exterior faces (the flip fixes the derived-normal sign either way, but cull needs it).

## FS (mono, sun-lit; feeds the S1 silver post → planes stay the only chroma)
```
lit   = uAmbient + uDiffuse * max(dot(N, -uSunDir), 0.0);   // N·L, sun = light-travel dir
val   = mix(uWallVal, uRoofVal, roofness) * vBj;            // per-building jitter
finalColor = vec4(vec3(clamp(val*lit, 0.0, 1.0)), 1.0);     // r==g==b (mono)
```
`[buildings]` dials: `wall_val` (~0.42), `roof_val` (~0.62), `ambient` (~0.35), `diffuse` (~0.75),
`height_scale` (1.0), `value_jitter` (0.12). No bare look-constants in GLSL (house law).

## Batching
`_build_building_batches`: accumulate per-building `(verts, idx)` into batches until the next
building would exceed `BUILDING_BATCH_MAX_VERTS` (60000); each batch → one `GisBuildingBatch`
`{vtx_off, vtx_count, idx_off, idx_count}`. Deterministic (input order = fetch order, stable).
Safety valves: skip footprints with area < `BUILDING_MIN_AREA_M2` (18, drops sheds/noise) and cap the
total to `BUILDING_MAX_COUNT` (sorted area-desc first, so the cap keeps the town's real massing).
`log()` the dropped count (no silent truncation — house law).

## Gate additions
- **asset-validator leg** (`test_sudbury_gis`, keyed off `kSudburyBuildingBatchCount>0` so it's inert
  in S0–S3): every vertex `dir` unit; every index local-in-range per batch; **outward winding** —
  for each triangle `dot(cross(pb−pa,pc−pa), (pa+pb+pc) − 3·building_centroid) > 0`... simplified to
  the ribbon/water test `dot(cross, centroid) > 0` on the sphere-dir positions PLUS a per-batch
  "has ≥1 building with he>0 top ring" (the count>1 tripwire, so a no-op bake fails). Plus the
  existing lock-hash leg gates the emitted dirs (buildings are projection-baked placements).
- smoke at the pinned Chelmsford viewpoint → `shots/s4_buildings.png`, read back; A/B
  `SEADS_NO_BUILDINGS`.

## ★ Fable-BEFORE — red-team targets (fresh context)
1. min-rotated-rect axis/half-extent extraction + the gable/hip ridge-endpoint formulas + the
   `Lh≈Wh` hip→pyramid degenerate + thin/tiny/non-convex footprint fallbacks.
2. Outward CCW winding for walls / eave cap / each roof face, so backface cull + the eye-flipped
   dFdx normal are self-consistent (no inside-out faces, no black facets).
3. The dFdx/dFdy face-normal + eye-facing flip + `up`-keyed roof/wall break — correctness &
   precision (eye-relative worldPos reconstruction at ~15 km, float32).
4. In-disk rejection (any vertex ρ>R_MAX ⇒ drop) + radial drape (base per-corner `radius_at(dir)`,
   top `+h`) — does a footprint straddling a terrain-mesh cell edge read acceptably?
5. Batching determinism + the area-desc cap keeping real massing.

## Fable-BEFORE verdict — SOUND-WITH-FIXES (2026-07-11, fresh Fable context)
Folded into `sudbury_building.py` + `sudbury_config.py` before the gate:
- **P0-3** eave cap was `shapely.ops.triangulate` (unconstrained Delaunay → slabs a flat roof over
  an L/U notch) → replaced with a real **ear-clip** of the CCW ring, reusing the eave-ring verts.
- **P0-4** min-rect `w` FORCED to the left-perp of `u` (shapely gives no orientation guarantee; the
  `E = c ± Lh·u ± Wh·w` corner build needs `w ⊥ u` + fixed handedness).
- **P1-2** rectangularity gate `area/minrect.area < 0.65 → flat` (no roof slab bridging a notch).
- **P1-3** hip ridge half-length `< ROOF_MIN_RIDGE` (0.5 m, metric) → pyramid apex.
- **P1-4** defensive OSM `height`/`levels` parse — already in `fetch._building_height`.
- **P0-1 / P0-2 (winding)** made moot by the **cull-OFF** render decision (below): a flipped face
  still lights correctly (FS eye-flipped `dFdx` normal) and depth orders the near face, so winding is
  not visually load-bearing; the validator does structure + a not-a-no-op tripwire, NOT the (invalid
  for non-radial faces) centroid winding test.
- **P1-1 (per-building anchor radius)** DEFERRED — the always-emitted eave cap is the watertight
  safety net (it fills the footprint at `he`, so a per-corner drape mismatch at the roof edge can't
  see through). Revisit if Chad's fly shows eave slits/tilt on the Shield gradient.
- **P2** batch cap tie-breaks on OSM id (determinism); FS normal trick confirmed sound.

NOTE (corrects the geometry section above): outward for a CCW footprint edge is the RIGHT normal
`(dy,−dx)`, not the left. Buildings render **cull-OFF** (like ribbons) so this is validator-only, not
visual. The emitted wall triangle order `(b_i,b_{i+1},t_{i+1})+(b_i,t_{i+1},t_i)` is outward-correct.

## Fable-AFTER verdict — UNSOUND → fixed (2026-07-11, fresh Fable context on the LANDED code)
One real **P0** in the FS lighting frame (caught the actual cause of the dark/flat look), plus P1
fragilities. All P0/P1 folded + re-baked/re-built/re-gated:
- **P0-1** the VS set `vPos = vertexPosition` (world-absolute, |v|≈15000); the `−eye` translate lives
  only in raylib's `mvp`, so `toEye = normalize(−vPos)` pointed at the **planet center**, not the eye
  → roof normals always flipped down → `roofness→0` (roofs shaded as dark walls), roof diffuse ≈ 0,
  sun-independent. FIX: VS declares `uniform mat4 matModel` (raylib auto-sets the −eye transform) and
  `vPos = (matModel·vertex).xyz` — the `planet.cpp`/`rig.cpp` convention; eye-relative also
  conditions the `dFdx` facet normal. (My earlier tone bump had been compensating for this — re-tuned
  after the fix.)
- **P1-1** relation buildings chorded per-way (big industrial/hero footprints are multi-way outer
  rings) → FIX: `_stitch_rings` the outers before polygonizing (the water-relation fix), holes dropped.
- **P1-2** the roof-rect overhang was open-bottomed → a slit to sky on a slope → FIX: emit the
  roof-rect underside quad at `he` (2 tris) so any drape mismatch shows a lit underside, never sky.
- **P1-3** ear-clip bail was silent + the `hmax>0&&hmin<0` tripwire was satisfied by wall-quad eave
  verts → FIX: count `cap_bails` into the bake stats + log; validator now also requires ≥1 triangle
  with ALL verts `h>0` (only a cap/roof qualifies — wall tris always carry a base vert `h<0`).
- **P2** skip `building=no` (explicit non-building). Sound-confirmed: min-rect extraction, hip/pyramid
  closure, ushort bins, double→float ~0.9 mm cast, inert-bake guards.
