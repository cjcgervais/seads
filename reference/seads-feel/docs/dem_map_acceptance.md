# The map's acceptance gate — the ten red-team legs, and what kills each one

Companion to `docs/dem_map_red_team.md`, which is the FINDINGS. This is the
INSTRUMENT: what now exists, where it runs, and the mutation each leg was watched
to die to.

The problem this closes, in one line: `measure_map_conditioning.py` measured and
printed, and contained no threshold and no assertion, so **nothing in the tree
could fail**. Two numbers were wrong for a whole session with every test green —
a "~2x DEM residual" that never existed, and a lake table quoted from the two
favourable rows of four.

## Where the legs live

| | runs | needs |
|---|---|---|
| `offline_tool/accept_map.py` | at the END of every bake (`build_sudbury.py` calls it and reports RED), and by hand | the venv + `offline_tool/source/` cache |
| `test/unit/test_bake_manifest.cpp` | the C++ gate, EVERY build | nothing — stdlib + the committed assets |

The split is forced by the environment: the gate's `python` has no numpy, and the
OSM/CDEM source cache is gitignored. So the expensive legs run where the data is
(at bake time, mandatory, not optional), and the one leg that must never be
skipped — did these artifacts come from ONE bake — runs in the gate on every
build with a self-contained SHA-256.

`accept_map.py` exits **0** all green, **1** a leg failed, **3** legs were skipped.
3 is deliberately not 0: a skipped leg must never read as a pass.

## The legs

Run them: `.venv/Scripts/python.exe accept_map.py`
Watch them die: `.venv/Scripts/python.exe accept_map.py --mutants`

Green on the shipped bake (`bake_id=20260809T200807`), with every mutation
confirmed to kill its leg:

```
dem_amplitude_ring    hero 0.994/r1.000  r10km 0.997/r1.000  r20km 0.994/r0.998
                      r27km 0.968/r0.995  wanapitei 1.040/r0.902     (shipped/predicted)
normal_map_amplitude  hero 0.965  r10km 0.984  r20km 0.989  r27km 0.983
                      wanapitei 0.974  r38km 0.965   (all r >= 0.997)
lake_outline_fidelity 76 lakes, tol/texel max 0.50, chord/span mean 0.083
lake_flatness         flat 0.000 m under every lake; p05 freeboard +2.2 / +1.1 / +0.3
landmask_is_coverage  1.89% partial-coverage texels (was 0.33%)
remap_range_pinned    207.0..447.6 m -> 0..350 m, gain 1.455
bake_manifest         7 artifacts hash-clean under one bake_id
```

| leg | asserts | killed by |
|---|---|---|
| `ring_footprint_sanity` | every acceptance patch is on real data, on the disk, k spread < 1.15 | restoring the 4000 SPHERE-metre half-width |
| `dem_amplitude_ring` | shipped DEM vs a replay of the bake's own conditioning: gain 1.30–1.65, r > 0.95, at every ring | `RELIEF_TAPER_EXP` 0.0 / 2.0 / `k_t` instead of `k_area_mean` |
| `normal_map_amplitude` | shipped normal map matches its TAPERED prediction within ±20% | baking `emit_normal_map` from the untapered elev (**the leg that would have caught P0-1**) |
| `lake_flatness_and_freeboard` | flown DEM flat under each lake to < 0.05 m; no land below its own water in the 100–600 m collar, near shore or far | restoring the per-lake CONSTANT taper |
| `lake_shape_reads_the_asset` | the shipped landmask agrees with the outline the shape numbers were measured on | a 50%-scaled / stale / absent landmask |
| `landmask_is_coverage` | > 1% of texels are partial coverage | restoring the `> 0.5` binarise |
| `shore_bank_cap_is_sphere_metric` | the band and ramp scale as 1/k outside 22 km and are a literal no-op inside | removing the `/k_cap` |
| `bake_manifest` | SHA-256 of every artifact + one `bake_id` shared by lock, header and manifest | copying ONE png from the previous commit |
| `radial_selftest` | monotone at 1 m through the blend window, array path bit-identical to scalar at the start radius, `theta(table max) < pi` | any edit to the integrator step, blend window, or table extent |
| `lake_outline_fidelity` | no lake's flown outline is straightened past its own SOURCE outline | restoring the flat 40 m Douglas-Peucker tolerance |
| `remap_range_pinned` | the theatrical range is pinned and the shipped assets were baked at that pin | reverting emin/emax to the live percentiles |

That is eleven: the red team's ten, plus `lake_outline_fidelity` for what Chad
flew on 2026-08-09.

## Two design notes worth keeping

**Patches are sized in GROUND metres and shrink where k moves fast.** The original
instrument used a half-width of 4000 SPHERE metres, which at k = 0.41 is 9.9 km of
ground: its far patches spanned a 2.6:1 range of k and were a third procedural
fBm. Two instruments then agreed with each other because they shared that one
defect, and the branch nearly re-dialled a correct taper on the strength of it.
Patches are now 3 km of ground, and `patch_half_m()` shrinks the window through
the 22–32 km blend window (to 1.5 km at 27 km) so no patch's own samples disagree
about the scale being measured. `ring_footprint_sanity` exists to stop that defect
being re-acquired, and reports the window that was used.

**A leg that fires where the defect cannot exist is measuring the terrain.** The
first `normal_map_amplitude` compared the normal map to the DEM and required that
ratio to be flat with rho. It failed at 10 km — inside 22 km, where
`relief_taper` is EXACTLY 1.0 and the mutation under test changes nothing. The two
assets carry different prefilters over terrain whose spectrum changes from urban
Sudbury to open Shield, so their ratio is not an invariant. It now compares the
shipped asset against a REPLAY of the pipeline that produced it, which cancels
terrain entirely.

**Never sample the asset finer than the asset.** `patch_n()` sizes the sample count
so the step is at least 1.5 equirect texels ON THE SPHERE. Below one texel the
shipped DEM is only bilinear interpolation between texels while anything compared
against it still carries the 4 m source detail, so the regressor holds variance the
response cannot: the slope is biased DOWN and r falls. It looks exactly like a
too-flat map. Measured before the rule existed, the 27 km patch stepped 0.66 of a
texel and read gain 1.289 / r 0.799, against 1.45 / 1.00 at the three rings that
happened to step 1.7-2.0 texels.

Two things about that are worth keeping. First, the DIAGNOSTIC: excluding
near-water samples moved the gain (1.289 -> 1.361) but never moved r (~0.85
throughout). Contamination moves both; a resolution mismatch moves the gain and
pins r below 1. That is what ruled out the lakes. Second, the PROVENANCE: this
defect was created by the fix for the previous one — `patch_half_m` shrinks the
window through the blend zone to hold k steady, and `PATCH_N` did not shrink with
it. Two instrument bugs in a row, from the same file, both of which made a correct
map look wrong. Both rules are now asserted by `ring_footprint_sanity`, because an
instrument is a thing that has to be gated too.

**Measure the RING, not one place.** Each ring is sampled at six bearings and
pooled, with bearings qualifying on sample COUNT and on having enough source
relief to measure a ratio at all — never on the answer they give. Selecting
bearings by their result would be the cherry-picking this branch was already caught
doing. Both source legs also ASSERT that at least one ring past 22 km was measured,
because inside 22 km `relief_taper` is exactly 1.0 and the taper mutations change
nothing at all there.

That assertion exists because of the worst moment in this session:
**`normal_map_amplitude` passed its acceptance run and then SURVIVED its own
killing mutation.** With one bearing per ring, the 33.5 km patch sat on Lake
Wanapitei and dropped out and 38.5 km did not qualify, so the leg was measuring
three rings where the defect cannot exist plus one where it had a 0.84 baseline —
enough headroom for a 1.27x error to pass inside +/-20%. A green leg that cannot
see P0-1 is worse than a red one: it is false assurance about the finding the red
team called "the one that matters most". The baseline was 0.84 because the bake
forces normals radial on a DILATED water mask near shores, which the replay does
not model; the same 600 m water exclusion the DEM leg uses fixed it.

## The score, and what it cost to get honest

The first full run of these legs was 7 pass / 4 fail. Every one of the four was a
defect in the INSTRUMENT, not in the map:

| leg | what it reported | what was actually wrong |
|---|---|---|
| `lake_outline_fidelity` | "header has 77177 verts, the law rebuilds to 77595 — re-bake" | the leg re-derived the header's lake selection and ranked by bounding-box centre where the header uses `representative_point()`, so it graded a different lake set |
| `lake_shape_reads_the_asset` | "Wanapitei: mask says water at only 50% inside" | sampled 150 m inside the shore, inside the deliberate 130-260 m mask erosion |
| `lake_flatness_and_freeboard` | "not flat (42.390 m); land 17.1 m BELOW its own water" | the -120 m ring ran on real shore terrain; and `min` over a 400-point ring reports a neighbouring lower pond, whose rim reads as land under the eroded mask |
| `dem_amplitude_ring` | "r27km gain 1.29 (want 1.30-1.65) r 0.799" | sampled the DEM at 0.66 of a texel — regression dilution |

The map was right all four times. That is the same score the original red team
posted against this branch's own instruments, and it is the argument for the
mutation harness: a leg that has never been watched fail tells you nothing about
whether a green result means anything.

## The mutations are reproducible, not a claim

`--mutants` re-creates each defect and asserts the leg goes RED; a leg that
SURVIVES is reported as a failure of the leg. Two mutations are applied for real
(a PNG copied from the previous commit, a pin that disagrees with the assets); the
rest are applied to the asset in memory with the exact transform the code change
would have produced, because a re-bake is 40 minutes each. Those are marked SIM in
`accept_map.MUTATIONS` and in the output.
