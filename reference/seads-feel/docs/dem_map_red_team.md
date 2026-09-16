# Red-team verdict — world-dem (fresh context, 2026-08-09) + CORRECTION OF THE RECORD

> **STATUS 2026-08-09 (later the same day).** Every finding below is now either
> FIXED IN CODE or has a LEG THAT WOULD CATCH IT — the instrument half is in
> `offline_tool/accept_map.py` and `test/unit/test_bake_manifest.cpp`, and is
> written up in `docs/dem_map_acceptance.md`. Read that for what runs where and
> what kills each leg; read THIS file for why any of it was needed.
>
> - P0-1 (untapered normal map), P1-2 (per-lake constant taper), P1-4
>   (ground-metric shore band) — fixed in `d44685d63` / `c20b609e6` /
>   `e5c34f788`, before this instrument existed; each now has a regression leg.
> - P1-3 (landmask re-binarised) — the binarise was removed, but that was only
>   half of it: order-1 `map_coordinates` is bilinear POINT sampling, which on a
>   0/1 field returns a fraction only within one 4 m source texel of the shore.
>   Measured on the shipped asset: **0.33%** of texels partial, i.e. the staircase
>   survived the fix meant to remove it. The mask is now area-averaged at half a
>   destination texel of sphere arc BEFORE the resample.
> - P1-5 (instrument ignores the asset), P1-6 (no partial-bake detection),
>   P1-7 (core drifts with the far field) — closed by `lake_shape_reads_the_asset`,
>   the bake manifest, and the pinned remap range respectively.
> - The "REQUIRED TEST LEGS" list at the bottom is BUILT, plus an eleventh for
>   Chad's 2026-08-09 fly report (small lakes with unnatural straight edges — the
>   flat 40 m Douglas-Peucker tolerance).

Fresh-context Opus red team against commits 2fcdc94cb / 5607671d6 / 3d9ab1ee3.
Gate at the time: build clean, ctest 968/968. The gate is structurally blind to every
finding below — none of them are compile or unit-test failures.

## THE HEADLINE REVERSAL

**There is NO ~2x DEM residual.** Commit 3d9ab1ee3's "OPEN RESIDUAL" section is WRONG and
its closing line ("RELIEF_TAPER_EXP is the dial") is the most expensive sentence in this
branch's record — acting on it would have broken a currently-CORRECT DEM.

Why my instruments lied: `measure_map_conditioning.rings()` uses a half-width of 4000
SPHERE metres, which at k=0.41 is 9.9 km of GROUND. The far-ring patches therefore span a
2.6:1 range of k and are 35-57% PROCEDURAL fBm, with 9-31% outside the disk entirely.
Instruments (b) and (c) agreed because they share that one defect, not because they were
independently right.

The red team's pointwise regression (shipped DEM vs a replay of `_sphere_metric_blur` on
the source, fixed 3 km GROUND windows) gives slope/k = 1.47 +/- 0.15 with r ~ 1.00 from the
hero point to 33.5 km. Untapered it would read 2.4-3.6 and rising. **The DEM taper is
exactly right. Do not re-dial RELIEF_TAPER_EXP.**

## P0 — the real remaining "folding"

**P0-1: the object-space NORMAL MAP is never tapered.** `build()` passes the raw,
un-flattened, un-tapered `elev` to `emit_normal_map`, which applies only the frozen
theatrical remap. Encoded slope is therefore 1/k too steep: 1.6x at 27 km, 1.8x at 30 km,
2.5x at Wanapitei, 3.0x at 38.5 km, 3.6x at the rim. Calibrated to 1% at the hero point;
from 24 km out the measurement tracks the UNTAPERED prediction.

This is the one that matters most: `emit_normal_map` REPLACES the mesh normal, so the far
field's entire lit appearance is the normal map. The geometry now reads at real steepness
and the shading reads at up to 3.6x real, coherent along the radial axis — i.e. Chad's
"folding" complaint survives the fix that was supposed to kill it. The inline comment
claiming the amplitude matches the mesh is now false.

Fix: compute pivot/taper once in `build()` and pass `pivot + (elev - pivot) * taper` (the
PER-PIXEL taper; water is forced radial in the normal map so the per-lake constant is
irrelevant there).

## P1 — the one that would have been flown as a regression

**P1-2: the per-lake CONSTANT taper puts water above land on Wanapitei's hero-facing shore.**
k runs 0.627 -> 0.279 across that lake, mean 0.379, so a single per-lake constant
manufactures a land-vs-lake offset of -8.2 theatrical m at the 28 km shore (land BELOW the
mirror) swinging to +3.1 m at 42 km. Runtime `surface_lift_m` is ~0.4 m. Measured on the
shipped asset: inner collar (27-31 km) mean land-lake +11.3 m but min -4.0 m with **4.1% of
the collar below water**; outer collar +25.2 m — the ~14 m asymmetry is the predicted
signature. The shore-bank cap runs after and only ever LOWERS land, so it cannot repair it.

This is exactly the "land ON the water / straight cutoffs" class Chad already flew and
rejected once (S0-REV2) — on the exact lake he complained about, on the hero-facing side.

Fix is structural, not a dial: pivot the taper on the WATER LEVEL near each lake (global P
blended to level_L over a ~1-2 km collar), then use the per-pixel t everywhere. Lake pixels
give level_L + (level_L - level_L)*t = level_L — flat for ANY t — and the shoreline stays
continuous. That buys flatness, continuity and per-pixel amplitude at once, which is what
my four correction rounds were circling without reaching.

## Other P1s

- **P1-3** the landmask is re-binarised at `>0.5` fourteen lines after the order-1 change,
  so the shipped alpha is still hard 0/1 and the "coverage channel" claim is not delivered;
  and bilinear on a 4 m grid sampled at 11.5-96 m of ground is still point sampling, so the
  full-texel shore staircase is not removed.
- **P1-4** `SHORE_BANK_BAND_M`/`SHORE_BANK_RAMP` and the tree-density blurs are STILL
  ground-metric, in a commit whose thesis is that ground-metric prefilters are the bug. The
  protective shore band is 61 m of sphere at Wanapitei (one mesh cell, not the ~2.5 it was
  tuned for) and the ramp reads 7.4% instead of 3%. The albedo resample has no prefilter at
  all.
- **P1-5** `lake_shapes()` accepts `mask` and `u_off` and USES NEITHER — it measures the
  radial law, not the bake, and would print identical numbers against a stale, partial, or
  absent bake. The file contains no threshold and no assertion; it only prints. It cannot
  fail.
- **P1-6** nothing anywhere can detect a mixed asset set, and `projection.lock` already
  records `bake_commit=5607671d6` while the assets came from 3d9ab1ee3.
- **P1-7** `k_area_mean < 1` throughout the ruled-1:1 core, so core relief IS reduced ~4% at
  10 km and ~18% at 20-22 km. Option B guaranteed 1:1 for the LAW, and the bit-identity
  assertions cover DIRECTIONS only, not ground heights. Terrain under Copper Cliff, both
  bubble centres and both pump anchors moves.

## CORRECTION OF THE RECORD (commit messages are durable; these were overclaims)

1. **The lake numbers were cherry-picked.** 5607671d6 and 3d9ab1ee3 both quote "Wanapitei
   1.23 vs real 1.19; Whitewater 2.34 vs real 2.34". The instrument prints FOUR lakes by
   default and I had run it: **Vermilion 2.99 -> 4.72 (error 1.73)** and **Long Lake
   9.49 -> 5.91 (error 3.57)**. Both are hero lakes, both sit at ~21 km in the blend zone,
   and both are the LARGEST shape errors on the map. Quoting only the favourable pair,
   twice, in a record whose subject is lake shape, is the most serious item in this file.
2. **"The map now reads at REAL steepness"** — only the DEM does (see P0-1).
3. **"Every anti-alias prefilter was sized in GROUND metres"** — correct diagnosis, but two
   of four were converted and it is written up as total.
4. **Wanapitei is round in its SECOND MOMENTS, not in shape.** Conformality is local; the
   lake spans k 0.627->0.279, so it is compressed 2.2x more at its far end. Measured
   near-half/far-half tangential width ratio: real 0.70, flown 0.99 — a 1.41x WEDGE. PCA
   aspect (1.19 -> 1.23) is near-blind to this, which is exactly why the number looked
   perfect, and the acceptance instrument cannot see it.
5. **"core look preserved and invariant to any taper retune"** — the remap RANGE is frozen;
   the core RELIEF is not (P1-7).
6. **"sqrt(k_r*k_t) is the honest amplitude correction"** — it is the least-bad SCALAR. No
   scalar corrects an anisotropic amplification; the blend zone is left ~0.82x too flat
   radially and ~1.21x too steep tangentially at 21.4 km, and THAT residual anisotropy is
   what produces the Vermilion and Long Lake errors. An accepted cost of Option B, not a
   solved problem.
7. **"_rho_grid verified equal to 1.3e-5 m"** — float32 accumulation at magnitudes ~2.3e9
   has a ulp of ~256, so the true rho error is ~2 mm. Physically irrelevant (k error ~1e-7)
   but not what the record says.
8. **"co-generated in a single bake, so the invariant holds"** — the invariant is ASSERTED,
   not ENFORCED, in a commit that documents a mixed asset set having actually happened.

## VERIFIED SOUND (attacked, held)

Bit-identity inside RHO_CONFORMAL_START_M (including rho == 22000 exactly and the
vectorized path); monotonicity at 1 m spacing; RK4 vs 10x refined 1.4e-7 m; conformality
past FULL 2e-16; table saturation past 140.87 deg is safe (fully fBm-replaced); NO fifth
fork of the radial law anywhere in offline_tool/render/world/sim/test (the surviving
`acos*R` sites are sphere-arc between unit dirs, and `render/bubble_map.h` is an independent
aeqd OF THE SPHERE, not a fork); R_MAX 41993->43000 has no stale arc-angle consumer; the
frozen remap cannot increase clipping; header elevations are sampled from the tapered u16
so landing heights match `radius_at`; and `_sphere_metric_blur`'s weights do sum to exactly
1 (removing wsum was safe).

## REQUIRED TEST LEGS, each with its killing mutation

1. `test_dem_amplitude_ring` — pointwise regression on fixed 3 km GROUND windows; require
   slope/k in [1.30, 1.65] and r > 0.95. Killed by: RELIEF_TAPER_EXP 0.0; 2.0; k_t instead
   of k_area_mean.
2. `test_normal_map_amplitude` — match the TAPERED prediction within +/-20%. Killed by:
   reverting emit_normal_map's input to the untapered elev. **This is the leg that would
   have caught P0-1.**
3. `test_lake_flatness_and_freeboard` — flown DEM flat under each lake to <0.05 m; min land
   in a 100-600 m collar >= lake level in BOTH inner and outer halves; halves differ <5 m.
   Killed by: restoring the per-lake constant taper (fails at -8.2 m on Wanapitei).
4. `test_ring_footprint_sanity` — every acceptance patch <2% past WILD_FADE_IN, 0% past
   R_MAX, k_max/k_min <1.15. Killed by: restoring the 4000 sphere-metre half-width. **This
   exists to stop the instrument re-acquiring the defect that produced the false residual.**
5. `test_lake_shape_reads_the_asset` — killed by: a 50%-scaled landmask (today's version is
   invariant because it ignores the asset).
6. `test_landmask_is_coverage` — >1% of texels strictly between 8 and 247. Killed by:
   restoring the >0.5 binarise. (Fails as shipped today — that is the point.)
7. `test_shore_bank_cap_is_sphere_metric` — killed by: removing the /k_t.
8. `test_bake_manifest` — SHA-256 of every asset + a per-run bake_id shared by lock, header
   and .bin. Killed by: copying one PNG from the previous commit. **The only thing that
   could catch a partial bake.**
9. Extend `sudbury_radial._selftest` — monotonicity at 1 m; array-path bit-identity at the
   start radius; theta(table max) < pi.
10. Then ONE certification fly, asking by name for: Wanapitei's hero-facing shore (water on
    the land or above it), the wilderness at 25-35 km (is the combing gone in the SHADING),
    and the ground under Copper Cliff and the pump anchors (anything visibly moved).
