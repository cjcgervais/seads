# Opus consult report — Wanapitei lens + wilderness folding (2026-08-09)

Independent fresh-context consult against `docs/dem_map_consult_packet.md`, the four offline
modules, the runtime projection/heightfield side, the plan/handoff docs, and the five geodesic
hillshades. Consultant ran its own measurements. Verdict below is theirs; corrections to the packet
are theirs and are accepted.

## (A) Attribution verdict — CONFIRMED, with 3 corrections and 2 additional independent causes

### A1. Direct shape measurement (stronger than the packet's slope inference)
PCA on every `sudbury_landmask.png` texel within 12 km of Wanapitei's real centre (46.75, −80.75),
in both the aeqd ground frame and the sphere tangent frame:

| frame | principal sigma | aspect |
|---|---|---|
| aeqd ground plane (= real Sudbury) | 4185 x 3580 m | **1.17** |
| sphere, as flown | 3853 x 1414 m | **2.73** |

Aspect inflation from the projection alone: **2.33x**. The lake is round in the data and a lens on
the planet. The shore is not cut — it is squashed.

### A2. CORRECTION — edge k_t is 0.120, not 0.047
`R_MAX = 41 993 m` → theta_edge = 160.40° → k_t = 0.1198, which is what `assets/projection.lock`
already records. The stale-constant regression is **0.512 → 0.120, i.e. 4.3x**, not 10x.

### A3. CORRECTION — the rho = 45 km row is not squeezed GIS terrain
rho 45 km is theta 171.9°, **past R_MAX**. That patch is the procedural fBm cap. Its 16.5° mean
slope is not evidence of squeeze; it is evidence that the cap is *calibrated against* the squeeze —
`sudbury_bake.py` step 6b histogram-matches the cap to the fade ring and asserts the gradient ratio
stays in (0.30, 3.2). The ring is folded, so the cap is built to fold identically.
**The cap is a follower, not a cause** — fix the ring and it smooths on the next bake. Corollary: a
relief taper applied only inside the disk opens a seam unless the cap tapers with it.

### A4. CORRECTION — the "Kelly Lake at theta 130.6°" justification for CAP_DEG is false
From the baked header, actual theta per named lake: Whitewater 10.9°, **Kelly 49.1°**, Ramsey 50.6°,
Long 81.3°, Vermilion 93.3°, **Wanapitei 128.5°**. Only Wanapitei is out there. The cap conclusion
survives (Wanapitei's tip at rho 42.4 km genuinely forces it) but **exactly one feature holds the
cap open**, which makes a projection change far more tractable than the packet assumed.

### A5. ADDITIONAL CAUSE #1 — the anti-alias prefilters are specified in GROUND metres while every
consumer samples in SPHERE metres. This is a real invariant violation, not an accepted distortion.

`MESH_BLUR_SIGMA_M = 100` exists so the DEM is blurred above the render mesh spacing. It is applied
isotropically in the EPSG:3979 ground grid; on the sphere tangentially it is only `100 * k_t`. The
render mesh (subdiv 200 x tiles 2 → 399 verts/face-edge) is **59.1 m of sphere arc**, so the design
invariant fails from rho ~25 km outward:

| rho | k_t | DEM sigma tangential on sphere | sigma / mesh cell | normal-map FD step (ground m, tangential) | its prefilter | alias factor |
|---|---|---|---|---|---|---|
| 0 | 1.000 | 100 m | 1.69 | 11.5 | 5.8 | 1.0 |
| 20 km | 0.729 | 72.9 | 1.23 | 15.8 | 5.8 | 1.4 |
| 33.5 km (Wanapitei) | 0.353 | 35.3 | **0.60** | 32.6 | 5.8 | **2.8** |
| 38.5 km | 0.212 | 21.2 | **0.36** | 54.3 | 5.8 | **4.7** |
| 42.0 km (edge) | 0.120 | 12.0 | **0.20** | 96.0 | 5.8 | **8.3** |

The normal map is the worse offender and is what you actually see (the mesh is smooth at 59 m; the
shading carries the detail). `emit_normal_map` steps finite differences by one equirect texel of
**sphere** arc (11.5 m) but prefilters `elev` to `hp = 0.5*arc = 5.75 m` in **ground** metres —
point-sampling 4 m LiDAR at 32.6 m ground spacing at Wanapitei, 96 m at the edge. And `ga`/`gb` are
divided by the sphere arc, so encoded slope is additionally amplified by 1/k_t tangentially only.
Aliased + direction-dependent + 1/k_t-amplified, coherent along the radial axis = the radial ribbons.

Bake-only, needs no ruling. But **not sufficient**: smoothing removes the jag, not the 1/k_t
*amplitude* steepening of the surviving swells.

### A6. ADDITIONAL CAUSE #2 — the landmask path manufactures literal straight shore segments
1. `mask_eq = resample(water_render, 0)` — **nearest neighbour**. Point-samples a 4 m raster at
   32.6 m of ground tangentially at Wanapitei → shore staircase with full-texel steps, elongated
   radially. Also contradicts the runtime's own contract (alpha documented as a mip-filterable
   coverage channel).
2. `binary_erosion(landmask, iterations=33)` with SciPy's default connectivity-1 cross = erosion by
   an **L1 diamond** of radius 132 m, which chamfers curved boundaries toward 45° facets; and being
   in ground metres it retreats 130 m radially but only 41 m tangentially at Wanapitei.

Neither causes the lens. Both put literal 30–130 m straight/staircased segments on the shore.
Both are one-line fixes.

### A7. Checked and cleared
Mesh drape (`fill_face`/`facet_radius_at`/`radius_at` are projection-agnostic and sphere-isotropic);
`SHORE_ERODE_M` magnitude (1.1% of the lake); water mirror simplify (40 m on 11.5 km);
**goldens** (`test/golden/` is controller/kernel only — nothing there loads a HeightField or the DEM,
so a re-bake cannot move them; the only asset-gating legs are the projection-lock consistency,
tree-density dims, and the chirality probe, all of which pass on any co-generated bake).
Confirmed the packet's §4b R_MAX clip: 861 m contiguous, sagitta 2.2 m, reads dead straight.

### A8. Formula duplication = true blast radius
The runtime is **projection-agnostic** (shader samples by `fragDir`, height field by fixed
`equirect_uv`). `equirect_uv`, `sample01`, `dem16_unpack` are duplicated offline/runtime but are
**unaffected by a radial-law change**. The radial law `theta = rho/GROUND_R` exists in
**three Python functions in one file** (`sudbury_geo.aeqd_to_dir`, `.dir_to_geo`, `.dirs_to_lonlat`)
and has **no C++ copy**. `render/bubble_map.h`'s aeqd is a display projection about a different
centre and is independent.

## (B) Ranked options

| # | option | fixes folding | fixes lens | blast | ruling | verdict |
|---|---|---|---|---|---|---|
| 1 | Sphere-metric conditioning (anisotropic prefilter, DEM + normal map) | **yes, at root** | no | bake-only, 2 fns, lock unchanged, zero C++ | none | **BUILD** |
| 2 | Landmask fix (order-1 coverage resample + disk/sphere-metric erosion) | — | literal straight/staircase segments only | 2 lines | none | **BUILD, same pass** |
| 3 | R_MAX 41 993 → ~43 000 m | — | removes the 861 m disk chord | one constant, lock params re-emit | none | **BUILD, same pass** |
| 4 | Radial relief taper behind a dial | residual amplitude steepening | no | ~15 lines, must taper the cap too | Chad's dial | **BUILD at conservative exponent** |
| 5 | **Hybrid radial law** (1:1 to rho_0, then k_r → k_t so the far field is locally conformal) | yes | **yes — the only option that does** | 3 Python fns + full re-bake + new lock params; hand-baked dirs **bit-identical if rho_0 >= 22 km**; zero C++ | **YES** | **PUT TO CHAD** |
| 6 | Uniform sub-1:1 scale | yes | yes, most completely | every landmark + campaign distance moves | **YES** | present, not recommended |
| 7 | Shrink CAP_DEG under pure aeqd | partial | partial | as #5 | yes | reject — violates "every named lake real" |
| 8 | Wanapitei-only outline round-out | no | cosmetically | small | no | reject — round mirror over a lens shoreline = water visibly over land |
| 9 | Re-tune the fBm cap directly | superficially | no | small | no | reject — it is a follower (A3) |

## (C) Recommended pass — "sphere-metric conditioning" (1+2+3+4 as one bake-only commit)

- **Stage A — DEM prefilter.** Keep the fixed 100 m ground-side blur; add a new **step 6c** on
  `dem_eq` in the equirect (8192x4096, cheap): 4-level Gaussian pyramid at sigma = {0, 8.7, 17, 34,
  68} texels, blended by rho so total sigma **on the sphere** = 100/k_t(rho). Isotropic on the
  sphere, not "tangential only" — the latter produces a corduroy look.
- **Stage B — normal map.** `hp` becomes local: `hp_local(rho) = 0.5*arc/k_t(rho)` in ground metres
  (5.8 m at hero → 96 m at edge), via a 5-level pyramid selected by rho. Reduce the ±8.0 gradient
  clip to a rho-dependent bound tied to the taper so shading and geometry cannot disagree.
- **Stage C — relief taper, behind `RELIEF_TAPER_EXP` (default 0.5).**
  `t(rho) = clamp(k_t, 0.15, 1.0) ** EXP`; `pivot = gaussian(dem_eq, ~2 km)`;
  `dem_eq = pivot + (dem_eq - pivot) * t`. **Re-stamp lake flatness afterward** against `mask_eq`
  (a pivot varying across a 15 km lake would tilt it). Apply the same `t` inside the cap branch.
  At exp 0.5 and Wanapitei, tangential steepening 3.2x → 1.8x while radial flattens to 0.56x of real.
  `RELIEF_SCALE_M` stays 350 and `config/world.toml` is untouched — the taper lives inside the DEM
  values, and header elevations follow automatically (step 7 re-samples the final u16).
- **Stage D — landmask.** `order=1` coverage resample; replace `binary_erosion` with a
  distance-transform erosion at a sphere-metric threshold (or at minimum a true disk element).
- **Stage E — R_MAX 41 993 → 43 000 m.** Costs 3.9° more edge in a band that is already 100% fBm
  (invisible), and buys: no mirror-mesh clip on Wanapitei (861 m chord gone), and `keep_lake` covers
  the real tip so the shore stays real DEM out to the end.

### What this does NOT fix — state plainly
**A tangential low-pass cannot make a squeezed lake round again.** Wanapitei stays a ~2.7:1 lens.
Do not spend a fly on this pass in isolation — the report will be "the straight shore is still there."

### Lock / header / downstream
Without Stage E the lock **params line and hash are unchanged**. `render/sudbury_gis.gen.h`
regenerates (positions identical, elevations re-sampled). No C++ changes, no goldens change.
With Stage E the params string changes → hash re-emits; both validator legs still pass because they
are co-generated. Practical prerequisite: the ~2 GB source cache is in
`D:\flight_sim2\seads\offline_tool\source`, not in this worktree.

### Option 5's blast radius is much smaller than assumed
Every hand-written unit direction in `world/`/`render/` sits **inside rho = 21.5 km**:
tunnel mouths Murray 5.7 km / Errington 8.5 km, Copper Cliff track+Superstack+slag+train <= 10.1 km,
`kValleyCenterDir` 16.1 km, `kSudburyCenterDir` 17.8 km, both pump anchors 21.2/21.4 km.
(The 104.65° hit is `kSudburyMajorAxis`, a tangent axis, not a position.) Choose **rho_0 = 22 km**
and the hybrid law is identically `theta = rho/R` there — those constants are **bit-identical,
provable by assertion**. Tunnel mouths, cut disks, the tunnel net, Copper Cliff, bubble centres and
pump anchors do not move at all.

## (C6) Failure modes and how each is caught BEFORE a fly (all offline/headless)

| failure mode | detection |
|---|---|
| Over-smoothed far world reads as putty | Re-run `sphere_shade.py` at the same 5 rings before/after. Acceptance: mean slope becomes rho-independent (~1.9–5.0° at every ring) and hillshades show structured swells. Render the exponent bracket {0.0, 0.5, 1.0} as three triplets. |
| k_t sign/inverse slip | New assertion in `sudbury_geo._selftest`: for all rho, `sigma_sphere(rho) >= mesh_cell` derived from `config/world.toml` subdiv/tiles — so the invariant cannot go stale the way k_t=0.512 did. Mutation lever: anisotropy exponent → 0 must fail it. |
| Long-term asset regression | New headless ctest leg reading the shipped `sudbury_dem.png`: geodesic patches at rho 20/33.5/38.5 km, mean slope under a bound. **This is the permanent regression that ends the whack-a-mole.** |
| Lake tilts after the taper | Assert max abs delta-h across each `WATER_SURFACE_*` lake's masked texels in the final u16 < runtime `[water] surface_lift_m`. Mutation lever: skip the re-stamp. |
| Cap/ring seam from a one-sided taper | Existing grain-ratio assert (0.30, 3.2) — **tighten to (0.6, 1.6)** now the ring is unfolded, and add the same check on slope, not just abs-grad. |
| Header elevations fork from `radius_at` | Structurally safe already; add a spot assertion that the Superstack base and each landable lake level round-trip through `HeightField::radius_at` within 0.1 m. |
| Normal map vs mesh disagree after the taper | Compare `ga/gb` against finite differences of the final `dem_eq` at 200 sampled dirs per ring, tolerance tapering with rho. |
| Order-1 mask bleeds water at thin lakes | Existing "restore any lake the erosion mostly ate" guard, plus a cos-lat-weighted coverage-conservation check within 1%. |
| Non-ASCII Catch2 names | Already a structural `gate.sh` tripwire (4th bite). Keep new test names ASCII. |
| — | Fresh-context red team on the two conditioning functions and the taper pivot, with the mutation levers named in the brief, after the diff exists and before the fly. |

## (D) The ruling for Chad

The map carries 5 540 km2 of real Sudbury on a 2 827 km2 planet. Today the entire cost is paid
sideways: the far ring squeezes ~3x across and not at all along. Folding can be fixed with no ruling.
**The lens cannot be fixed without changing how the far ring shrinks — i.e. giving up exact 1:1
somewhere.**

- **Option A — keep exact 1:1 everywhere.** Wanapitei flies as 11.5 x 3.6 km, up to 4.3:1. Folding
  fixed, shape not. Nothing moves.
- **Option B (recommended if he wants the lake) — exact 1:1 out to 22 km, then the far ring shrinks
  evenly instead of sideways.**

| | today | Option B (rho_0 22 km, rho_1 32 km) |
|---|---|---|
| Wanapitei shape | 4.3:1 wedge | **~1.4:1** |
| squeeze at Wanapitei | 3.2x sideways only | 1.9x **evenly (round)** |
| hero → Wanapitei distance | 34.9 km | 30.7 km (−12%) |
| Wanapitei size | 11.5 x 3.6 km | 6.4 x 3.1 km |
| theta at disk edge | 160.4° | 126.2° |
| planet that is real Sudbury | 97.1% | **79.5%** |
| C++ changed | — | **none** |
| hand-baked world constants changed | — | **none** (all inside 22 km) |

  Costs to say out loud: far-field flights ~10–12% shorter; Wanapitei becomes a smaller round lake
  rather than a big wedge; procedural wilderness grows 3% → 20% of the planet, so the fBm quality
  starts to matter (budget a `WILD_BASE_FREQ`/amplitude retune, gated on the tightened grain assert).

- **Option C — shrink the whole map uniformly.** Cleanest geometry, biggest campaign cost:

| ground scale | theta_edge | k_t @ Wanapitei | aspect | real % of planet | Chelmsford→Sudbury (15 km real) flies as |
|---|---|---|---|---|---|
| 1.000 (today) | 160.4° | 0.313 | 4.3:1 | 97.1% | 15.0 km |
| 0.800 | 128.3° | 0.515 | 1.9:1 | 81.0% | 12.0 km |
| 0.667 | 107.0° | 0.644 | 1.55:1 | 64.6% | 10.0 km |
| 0.500 | 80.2° | 0.789 | 1.27:1 | 41.5% | 7.5 km |

  Not recommended — Option B buys most of the shape win while leaving the hometown core untouched.

Deliverable to decide with, before any fly: the five-ring hillshade set rendered three times (today
/ A-conditioned / B-conditioned) plus a Wanapitei plan-view triptych. Hours of offline compute, zero
flying time. Then **one** certification fly on whichever he picks.

## (E) Risks / open uncertainties (consultant's own)

1. **Sequencing is the real time risk.** Shipping the conditioning pass alone and asking for a fly
   will produce "still a straight shore." Get the A/B/C ruling first, build accordingly, fly once.
2. **Not proven which dominates the folding percept** — 1/k_t amplitude steepening vs tangential
   aliasing. Inference favours the normal map's 2.8–8.3x Nyquist violation (the mesh is smooth at
   59 m; shading carries the read), but the packet's slope table came from the DEM, not the shipped
   normal map. **Cheap to settle first:** decode `sudbury_normal.png` on the same five geodesic
   rings and compare against the DEM's distribution. If the normal map is much heavier, Stage B is
   the whole game and Stage C's exponent can stay conservative.
3. **The packet's slope table has a sampling artifact.** `sphere_shade.py` samples with
   nearest-neighbour integer uv at a 17 m step over an 11.5 m texel grid — that moires and inflates
   p90/p99 (and likely contributes the fine corduroy in `sph_centre.png`, where k_t = 1 and no
   squeeze exists). Means are robust, trend is real; **do not quote p99 = 46.86°.**
4. **Option B does not make Wanapitei perfectly round.** Conformality gives *local* isotropy, but the
   lake spans rho 27–42 km across which the scale factor still falls 0.54 → 0.29 — a 1.9x gradient
   along the lake. Result is a round-shored but slightly **tapered** lake (a pear, not a lens),
   aspect ~1.4 not 1.0. Only Option C makes the scale roughly constant across a 15 km lake.
5. The Wanapitei PCA window was a 12 km ground disk around the real centre and catches some
   neighbouring water; the 2.33x inflation figure is sound, absolute sigmas are the blob's.
6. Taper interaction with `SHORE_BANK_RAMP = 0.03` and `WATER_INTERIOR_GRID_M = 100` untested — both
   should get *safer* under a flattening taper, but read `build_sudbury`'s sag assertion and the
   shore-bank print on the first bake rather than assuming.
7. The 20% procedural wilderness under Option B is an unsized quality risk — `sph_r45_wild.png` shows
   the cap value-clipping into hard black blobs today, and that will matter at 8x the area.
8. **Did not read in full:** `sudbury_header.py`, `sudbury_ribbon.py`, `sudbury_building.py`,
   `build_sudbury.py`. The claim that no other module re-derives `theta = rho/GROUND_R` outside the
   three `sudbury_geo` functions is grep-based and is **load-bearing for Option B's blast radius** —
   verify with a second pair of eyes before implementing.
