# Lake rendering — build plan (stereoscope-sudbury, post-S0-REV2)

## ⭐ ROOT-CAUSE CORRECTION (Chad's 2nd fly, 2026-07-10) — the REAL fix
Chad flew the dedicated water surfaces (below) and reported: lakes STILL have the "cover" / straight
cuts — "the far side of Whitewater Lake is straight all the length" — even Whitewater, which HAD a
dedicated surface. That last clause was the tell: the straight edges were NOT (mainly) the coarse
terrain mesh faceting the original diagnosis blamed. **The real bug is in the OSM ingest: big lakes are
`natural=water` RELATIONS whose boundary is split across several `way` members that must be STITCHED
into one closed ring — the fetch closed each way independently, CHORDING a straight line across the
gap.** Whitewater = 6 outer segments (all open) + 9 inner rings (islands); the largest segment closed
alone gave a **3901 m straight edge**. This corrupts the **landmask itself** (which rasterizes these
polygons) ⇒ straight cuts on every relation-lake, in the albedo AND the dedicated surfaces (built from
the same broken polygons). The dedicated-surface mesh work "helped a bit" only because it partly
covered the mess.

**FIX = proper OSM multipolygon assembly** (`offline_tool/sudbury_fetch.py`: `_stitch_rings` /
`_relation_to_polys` / `_explode`): stitch outer way-segments into closed rings (endpoint-node match,
4 join cases), subtract inner rings as island HOLES. Result: Whitewater's chord 3901 m → **253 m**, 9
islands cut; across all water relations **8895 islands** removed, **0** rings needed force-closing
(every ring stitched cleanly). This corrects the shape of **all 1293+ water bodies in the landmask**,
not just the 14 surfaces — directly answering Chad's "all 300+ lakes with the correct shape." The
dedicated surfaces (13 now, rebuilt from correct outlines) remain as secondary shore-faceting polish.
Before/after: `shots/whitewater_polygon_fix.png`. Fable-AFTER on the assembler = SOUND-WITH-FIXES (two
P1 latent-data-loss paths folded: force-close now DROPS a broken ring instead of chording; `_explode`
returns all parts so a real lake arm / a way's buffer(0)-split can't be silently amputated or crash the
tessellation's `.exterior`).

---

# Dedicated flat lake-water surface — original build plan (the SECONDARY polish)

**Chad ruled 2026-07-10:** the major landable lakes read as "straight-edged cover strips" because
the coarse ~120 m terrain MESH facets the shore cliff across the flat lake at low grazing angles.
The baked DATA is clean (curved outlines, flat DEM, clean albedo) — the defect is the mesh geometry.
**Fix = render each major lake as its OWN smooth flat mirror surface at the water altitude, decoupled
from the terrain mesh.** Chad confirmed **real OSM lake outlines** (not simple discs).

This is NOT a stage-ladder stage; it's a targeted render feature on the S0-REV2 base. It stores
sphere directions ⇒ it embeds + is gated by the projection lock (`0x99061E1583D34607`, PROVISIONAL).
The projection PARAMS do not change ⇒ the lock hash does not change ⇒ downstream stays green.

## Model split (tri-model pipeline)
- **Opus builds it** — bake emission + the new render pass + shader + draw integration + the gate stack.
- **Fable red-teams the ★ math** BEFORE (this doc) and AFTER (landed). Fable is the isolated math
  sniper, NOT integration — it does not wire the raylib/GL feature.
- **Gemini: not needed** — the mirror-water look already exists in the planet FS; we reuse it.

## Design

### Which lakes get a surface
The named lakes with `span_m >= WATER_SURFACE_MIN_SPAN_M` OR `landable`, capped at
`WATER_SURFACE_MAX_LAKES`. Targets Chad named: Wanapitei, Vermilion, Whitewater, Ramsey, Long, Kelly
(all within the cap: farthest is Kelly at θ=130.6°, inside WILD_FADE_IN=139°). Small/less-visible
lakes keep the baked mask look (Chad OK'd leaving right-angle cuts there).

### Bake side (projection math lives ONLY here — house law)
For each qualifying lake `l` (has `l["poly"]` lon/lat + `l["poly_m"]` aeqd shapely):
1. **Boundary**: `poly_m.exterior` simplified (Douglas-Peucker, `WATER_OUTLINE_SIMPLIFY_M`) to keep
   the curved shape while dropping OSM micro-vertices. MultiPolygon → largest ring. Holes (islands)
   handled by the centroid-in-polygon triangle filter (below).
2. **Interior grid**: sample aeqd points on a grid at `WATER_INTERIOR_GRID_M` spacing, keep those
   inside `poly_m`. This is the key to the flat-vs-cap fidelity (★1): every triangle edge subtends a
   small angle so the piecewise surface hugs the spherical cap (sagitta bound below).
3. **Triangulate** (boundary ∪ interior) with `shapely.ops.triangulate` (Delaunay), keep triangles
   whose centroid ∈ `poly_m` (respects holes). Enforce winding so aeqd signed area > 0 (★2:
   (East,North,Up) is right-handed ⇒ CCW-in-aeqd = front-face-from-outside under raylib's
   CCW/cull-back with the radial-out normal).
4. **Project** every aeqd vertex → sphere dir via a new `SudburyFrame.aeqd_to_dir(X,Y)` (the tail of
   `geo_to_dir`, refactored so `geo_to_dir` calls it — ONE projection formula, single source).
5. **Emit** into `render/sudbury_gis.gen.h`: shared `kSudburyWaterVerts[]` (unit dirs),
   `kSudburyWaterIndices[]` (uint), and `kSudburyWaterLakes[]` = `{name, elev_m, vtx_off, vtx_count,
   idx_off, idx_count}`. `elev_m` = the lake's theatrical water radius (`elev_fn(dir_center)`, the
   same value already emitted for the label/landing target — single source). The header already
   embeds `kSudburyProjectionLockHash`; the validator's lock-hash leg gates these dirs.

### Runtime side (`render/water_surface.{h,cpp}`, projection-agnostic — consumes pure dirs)
- Build once (like `ensure_planet`): one raylib `Mesh` per lake. Vertex position =
  `dir * (elev_m + water_surface_lift_m)`; normal = `dir` (radial). uint→ushort indices per lake
  (each lake << 65536 verts).
- Draw AFTER `draw_planet` (line 676), before the Fleet Rig, eye-relative via
  `MatrixTranslate(-eye)` (identical double→float seam). Depth test ON (occluded by rising shore
  terrain; covers the dipping cliff facets — ★4).
- Shader `water_surface_fs()` compiles the SAME `kSkyUniformsGLSL + kScatterGLSL + kSkyGLSL` as
  `planet_fs`, and both call a NEW shared `kWaterGLSL` `water_look(...)` (reflect + sun/moon sparkle,
  LOD-faded) so the lake look CANNOT fork from the planet limb (★3, H1). `planet_fs`'s water branch
  is refactored to call `water_look`; A/B smoke confirms the planet is visually unchanged.
- `SEADS_NO_WATER=1` = A/B bypass (seam-safe env gate). Config `[water] surface_lift_m`.

## The 4 ★ math points (Fable-BEFORE brief)

**★1 — outline → sphere dirs + flat-vs-cap over a big lake.** Vertex world pos = `dir*elev_m`;
all vertices at radius `elev_m` ⇒ the surface is a tessellated spherical CAP (not a Euclidean plane —
a plane would cut the terrain; the radial-normal mirror wants the cap, matching the planet water
branch). Interior grid keeps facet sagitta small: for a max dip `s_max`, half-edge angle
`φ ≤ acos(1 − s_max/elev_m)`; s_max=2 m, elev_m≈15080 ⇒ φ≈0.0163 rad ⇒ edge ≈ 490 m ⇒ grid ≤ ~450 m.
*Vet: the on-cap (not planar) choice, the sagitta→spacing derivation, degeneracy for the biggest
lakes (θ up to ~0.76 rad).*

**★2 — triangulation winding + outward normal.** Outward normal = radial = `normalize(vertex)`, not
the geometric face normal. Winding: emit aeqd-signed-area > 0 so CCW-from-outside (raylib
CCW-front/cull-back). *Vet: the (E,N,U) right-handed ⇒ CCW-in-aeqd = front-from-radial-out claim
holds at large θ; whether to also disable culling as belt-and-suspenders; radial normal correct for
the mirror.*

**★3 — single-source the mirror look vs `sky_color` (H1/limb fork).** Both `planet_fs` and
`water_surface_fs` compile the identical `kSkyGLSL` and call `water_look`/`sky_color` with the same
args (viewDir = eye-relative `fragRel/dist`, same `uEyeAlt`, same `wN`). *Vet: is the shared-function
refactor the right H1 discipline; any uniform/precision difference (viewDir sign, eye_alt, haze) that
could still fork lake vs limb at the horizon.*

**★4 — z-fight / draw-after-terrain + lift.** Water at `elev_m + lift` overlays the flattened
interior (also ≈`elev_m`). Constant radial `lift` (≈1 m) sits water proud of the flattened lakebed
but below the rising shore terrain (which clips it via depth ⇒ water shows only on the flat
interior); it also covers the cliff facets that dip below `elev_m`. *Vet: constant world lift vs
polygon-offset depth bias; the lift bound (< min shore rise per mesh cell, > depth precision at
~15 km eye range given n/f from rlGetCullDistanceNear/Far); is ~1 m enough to beat z-fighting.*

## Fable-BEFORE outcome (SOUND-WITH-FIXES — folded)
Fresh-context Fable-5 verdict; all P0/P1 folded. It corrected the geometry math AND redirected the
architecture:
- **★1 P0 — sag length scale was wrong.** Max facet dip is at the CIRCUMCENTER, not the edge
  midpoint: for a square-grid Delaunay (right triangles) `s ≈ g²/(4·R_w)`. At g=450 the mid-facet dip
  is **3.36 m > 1 m lift** ⇒ lakebed pokes through. FOLD: **grid = 250 m** (sag ≈ 1.0 m),
  **lift = 2.0 m**, and the bake ASSERTS `lift > g²/(4·elev_min)`.
- **★1 P1 — `shapely.ops.triangulate` is UNCONSTRAINED** (convex-hull Delaunay; concave shores +
  island holes not respected). FOLD: **densify ALL rings (exterior + interior holes) to ≤ grid**, add
  the interior grid, `scipy.spatial.Delaunay`, then keep triangles whose centroid ∈ `poly_m`
  (shapely `contains`, holes respected). Residual shore sliver ≤ the densify step, hidden by lift+erosion.
- **★2 SOUND.** aeqd Jacobian det = sinθ/θ > 0 ∀ θ<π ⇒ signed-area>0 ⇒ CCW-front-from-outside at
  EVERY lake. Radial-normal shading is correct + required. Keep back-face culling ON (a winding bug =
  a vanished lake = a good tripwire); validator checks the sign.
- **★3 P1 — a separate water shader WOULD fork.** The planet does more than reflect+sparkle to a water
  pixel: **aurora ground glow** then **`sky_aerial`** (distance in-scatter), plus a dozen uniforms each
  program would own privately. FOLD (architecture change): **do NOT author a separate shader.** The
  water pass **reuses the planet's own program + material**, adding ONE uniform `uForceWater` that sets
  `water = uHasWater * max(texel.a, uForceWater)`. The water mesh's `fragDir` samples the SAME lake
  albedo + radial normal from the SAME cubemaps; aurora, `sky_aerial`, and every sky/scatter/moon
  uniform are IDENTICAL by construction (same program, bound the same frame by `draw_planet_mesh`).
  Zero fork surface — strictly better H1 than a shared `kWaterGLSL`.
- **★4 P1 — lift alone loses to depth LSB.** With clip n=2/f=60000 and 24-bit depth, 1 LSB ≈
  z²/3.36e7 m (0.75 m @5 km, 6.7 m @15 km). FOLD: lift = 2 m PLUS **slope-scaled
  `glPolygonOffset(-1.0, -2.0)`** on the water pass (the factor term is what saves the grazing landing
  view). Both surfaces shade near-identically so any residual far tie is color-invisible anyway.

Net architecture (revised): the runtime is a thin mesh-builder + a draw that flips `uForceWater`/
polygon-offset around `DrawMesh(planet.mat)`. No new shader, no shared-look refactor.

## Fable-AFTER outcome (SOUND-WITH-FIXES — landed code red-team)
Fresh-context Fable-5 red-team of the landed implementation. Every probed mechanism verified correct
(uniform single-source holds — GL program-object state, `draw_water` is the next statement after
`draw_planet_mesh`; polygon-offset lifecycle clean; ushort triply-guarded; sliver floor 1e-7 is ~30×
the emission noise + ~100× under the smallest legit tri; R_MAX exterior-only check suffices since ρ is
convex). Findings:
- **P1 FOLDED — `uHasWater` nullifies `uForceWater` in the degraded-landmask path.** If the landmask
  fails to load, the planet still builds `ok=true` with `has_water=0`, so `water = uHasWater *
  max(texel.a, uForceWater) = 0` ⇒ the 14 lake meshes would shade as flat LAND discs floating at the
  lift, polygon-offset in front — worse than the faceting. FOLD: `draw_water_surfaces` returns early if
  `p.has_water < 0.5` (water surfaces only over a real landmask). Re-gated green, water still renders.
- **P2-2 NOT folded (would regress).** Fable suggested tightening the reject gate to `WILD_FADE_IN_M`
  (36.5 km) so no mirror sits over the wilderness fade. But **Wanapitei (Chad's #1 named target) is at
  ρ=37.8 km — inside the fade annulus but a real lake Chad wants.** R_MAX (42 km) is the correct
  threshold; tightening would drop Wanapitei. Kept R_MAX; Wanapitei intentionally rendered in the fade.
- **P2s noted/accepted:** water `elev_m` is an 8-bit-DEM resample not `remap(level_m)` (safe: 11 m texel
  ≪ 900 m min span); multipart-lake secondary arms fall back to the baked-mask look (per ruling);
  island-bridging is depth-self-healing; getenv cached in a `static const bool` (folded). None block.

## Gate stack (per the skill, in order)
Fable-BEFORE → build → asset validator (new legs: unit dirs, winding sign, index-in-range, lock-hash
embed, `water lakes > 0`) → shaders compile → seam grep → smoke shots (pinned Chelmsford +
`SEADS_OBL_PAN/TILT` to Wanapitei/Long, read pixels back) → flight ctest goldens-0 → Fable-AFTER →
evidence block → atomic commit → Chad flies.
