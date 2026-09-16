# SEADS world map — consult packet: Wanapitei's straight shore + wilderness "folding"

Branch `sandbox/world-dem` (worktree `D:\seads_sandboxes\world-dem`), off `sandbox/fields-forge`
@ `91cae03da`. Offline-bake territory only (`offline_tool/` + `assets/*.png` +
`render/sudbury_gis.gen.h`). Kernel firewall (`sim/`, `control/`) is not in scope.

## 1. Chad's report (2026-08-09, verbatim intent)
> "The big lake Wanapetiae is round. One part of it is really a straight shore. We need to round
> out the lake to better match the actual shape. A lot of folding also could be smoothed in the
> wilderness areas."

Real Lake Wanapitei is a meteorite-impact crater lake: a near-CIRCULAR north basin ~8 km across,
with a southern arm; ~132 km², ~11.5 km span. It should read ROUND from the air.

## 2. What the pipeline is
- `offline_tool/build_sudbury.py` orchestrates: fetch CDEM + OSM (cached in `offline_tool/source/`,
  ~2 GB, network not required) → `sudbury_bake.py` bakes an 8192×4096 equirect
  (`assets/sudbury_dem.png` 16-bit hi/lo in R/G, `sudbury_landmask.png`, `sudbury_color.png`,
  `sudbury_normal.png`, `sudbury_treedensity.png`) → `sudbury_header.py` emits
  `render/sudbury_gis.gen.h` (named lakes, airstrips, dedicated water mirror meshes, ribbons,
  buildings, lamps) → `assets/projection.lock`.
- Projection: azimuthal-equidistant about a HERO point (`LAT0,LON0 = 46.5560,-81.1100`, the
  Sudbury–Chelmsford midpoint), TRUE 1:1 — `theta = rho / R_PLANET`, `R_PLANET = 15000 m`.
- `CAP_DEG = 160.4` → `R_MAX = 41 993 m` (aeqd disk crop). Past a fade annulus
  `WILD_FADE_IN 36 500 m` → `WILD_FADE_OUT 41 000 m` the surface blends to procedural
  sphere-native fBm.
- `RELIEF_SCALE_M = 350` (16-bit DEM 0..65535 → 0..350 m of radial displacement).
- `MESH_BLUR_SIGMA_M = 100` — the source DEM is Gaussian-blurred above the render mesh spacing so
  the coarse mesh doesn't alias into jagged peaks.

## 3. Two prior attempts at "straight lake shores" (both landed, both were real bugs, neither is the cause now)
- `92d9d2062` — OSM multipolygon relation rings were closed per-way, chording straight lines across
  gaps (Whitewater chord 3901 m → 253 m). Fixed by proper ring stitching.
- `3d7670c89` — the wilderness fade cut across Wanapitei's outer third at a constant-rho arc, and the
  dedicated mirror mesh rejected the whole lake for a 0.17 % tip past `R_MAX`. Fixed by keeping
  masked lake texels real to `R_MAX` and CLIPPING (not rejecting) the mirror polygon.

Chad is still reporting a straight shore after both. So the remaining cause is something else.

## 4. Measurements taken this session (read-only, against the SHIPPED assets)

### 4a. The source data is clean — it is NOT an OSM/stitching problem
Assembled Wanapitei polygon from the cached `water_raw.json` through the repo's own
`fetch_water`/`_relation_to_polys`:

| quantity | value |
|---|---|
| exterior vertices | 6 909 |
| area | 132.31 km² |
| aeqd rho range | 27.12 → 42.40 km |
| longest single source segment | **201 m** |
| longest collinear run (<3° turn) | **362 m** |

On an 11.5 km lake, a 362 m straightest run is nothing. The upstream shape is round.

### 4b. The `R_MAX` disk clip contributes a real but SMALL straight edge
`sudbury_water.build_lake_surface` clips the polygon to the `R_MAX` disk:
- area removed 0.155 %
- boundary now riding the disk arc: 938 m total, **longest contiguous run 861 m**
- sagitta of that arc = 2.2 m → reads dead straight

861 m on an 11.5 km lake is ~7 % of the span. Real, worth fixing, but probably not what Chad means
by "one part is *really* a straight shore."

### 4c. The actual cause: the aeqd TANGENTIAL SQUEEZE at large theta
Slope statistics sampled on **true geodesic patches of the R = 15 km sphere** (the metric the pilot
flies), decoded from the shipped 16-bit DEM, land-only, 700×700 samples:

| window | theta | rho | k_t = sin θ/θ | mean slope | p90 | p99 |
|---|---|---|---|---|---|---|
| hero centre | 0° | 0 km | 1.000 | **1.92°** | 4.73° | 9.60° |
| mid ring | 76.4° | 20 km | 0.729 | 3.39° | 7.92° | 18.75° |
| **Wanapitei** | **128.0°** | **33.5 km** | **0.353** | **7.24°** | 15.53° | 46.86° |
| fade band | 147.1° | 38.5 km | 0.212 | 12.65° | 28.30° | 57.34° |
| wilderness | 171.9° | 45 km | 0.047 | 16.50° | 37.18° | 56.19° |

`mean_slope × k_t` ≈ 1.9 / 2.5 / 2.6 / 2.7 — i.e. the roughness growth is **exactly** the
1/k_t tangential compression of the same rolling Shield terrain. Hillshades of those patches
(`sph_*.png`) show it plainly: at rho 38.5 km the terrain and every small lake are smeared into long
radial ribbons. **That is Chad's "folding."**

And the same mechanism is the straight shore: at Wanapitei, k_t = 0.353 means the lake is compressed
**2.8× tangentially**. A round crater rim squeezed 2.8× across becomes a lens with two long,
near-parallel, radially-aligned shores — visible directly in `sph_wanapitei.png`. **The lake is not
cut straight by a data bug; it is squashed into a straight-sided lens by the projection.**

### 4d. The first-principles version
The aeqd disk carries π·42² = **5 540 km²** of real Sudbury ground. The whole planet's surface is
4π·15² = **2 827 km²**. The map is ~2× too big for the planet, so the far ring MUST be compressed —
there is no bake-side filtering that can undo it, only mitigate how it reads.

Sphere-area fractions: k_t < 0.5 (≥2× squeeze) covers **34 %** of the planet; k_t < 0.25 covers
**11 %**.

### 4e. A design constant went stale and nobody re-derived it
`sudbury_geo.py` and `sudbury_config.py` still document "edge tangential squeeze is a finite, mild
**k_t = 0.512**" — that was true for the original S0 cap (`R_MAX = 28 km`, θ_edge = 106.95°). The
S0-REV2 recut to `CAP_DEG = 160.4` (Chad's "keep every named lake real" ruling) pushed the edge to
**k_t = 0.047** — a ~10× worse squeeze — while the "mild" justification was left in place. Wanapitei
sits at k_t = 0.353, well inside the region the old design would have called unacceptable.

## 5. Constraints that any fix must respect
- **GO-ANYWHERE ruling** (Chad): the whole surface stays traversable/landable and map-coherent.
- **"Keep every named lake REAL"** (Chad, S0-REV2): Wanapitei / Kelly / Long / Vermilion /
  Whitewater must be real GIS water, not procedural. Kelly is the farthest at θ = 130.6°.
- **NO antipodal pinch** (Chad flew and rejected `FILL_SPHERE=True`: "crunched up, jagged").
- **NO mountains**: Sudbury is rolling Canadian Shield. `RELIEF_SCALE_M` was already dialled
  1000 → 350 because 1000 flew "too steep + jagged."
- Campaign geography now depends on the map (MASTER_PLAN §2.5): Chelmsford coalition west,
  Sudbury/Azilda east, Errington + Murray mine tunnel mouths on the atmosphere-bubble edges. Moving
  the hero point or the scale moves all of that.
- `assets/projection.lock` hash + a validator ctest leg enforce lock ↔ header ↔ chirality
  agreement; the header and PNGs must be co-generated in one bake run.
- Everything downstream is re-baked from the same run: normal map, treedensity, ribbons, buildings,
  lamps, water mirror meshes, tunnel cut disks.

## 6. The question for the consult
Given §4c/§4d — the folding and the straight shore are ONE mechanism, the aeqd tangential squeeze,
not a data defect — what is the right fix, ranked by (feel gained) / (blast radius)?

Candidate directions, for critique and for anything missed:
1. **Anisotropic pre-filter.** Low-pass the source DEM tangentially by ~1/k_t before the equirect
   resample, so squeezed terrain becomes smooth swells instead of folds. Cheap, bake-only, keeps
   geography. Kills the folding; does NOT un-squeeze the lake outline.
2. **Radial relief taper.** Scale the DEM's relief by k_t (or a floor) with rho, so the far ring
   flattens as it compresses. Trivially cheap. Same limitation as (1); risks a visibly flat far
   world.
3. **Shrink the map / re-cut the cap.** Reduce `CAP_DEG` (back toward k_t ≈ 0.5) and/or introduce a
   sub-1:1 ground scale so the real extent fits the sphere with bounded squeeze. Fixes the geometry
   at the root but breaks 1:1, moves every landmark, and may push named lakes out.
4. **Non-equidistant radial law.** Replace `theta = rho/R` with a law whose k_t stays bounded (e.g.
   an area-preserving / conformal-ish blend past some rho) — the far world compresses RADIALLY
   instead of tangentially, so shapes stay locally round. Bigger change: the runtime's equirect
   inverse and `aeqd_to_dir` are single-sourced, so it is one formula in two places, but the lock
   hash and every baked asset move.
5. **Targeted only-Wanapitei fix** (round the outline, extend past `R_MAX`) — treats the symptom;
   would leave the lake round-in-outline but still living in folded terrain.
6. Something else.

Also wanted: is the 861 m `R_MAX` clip (§4b) worth fixing independently, and if so how, given a lake
tip genuinely reaches 0.4 km past the disk crop?

## 7. Evidence artifacts (session scratchpad)
`sph_centre.png`, `sph_r20km.png`, `sph_wanapitei.png`, `sph_r38_fade.png`, `sph_r45_wild.png`
(geodesic hillshades), `wanapitei_mask.png` (rectified landmask crop), plus the diagnostic scripts
`diag_wanapitei.py`, `diag_clip.py`, `sphere_shade.py`.
