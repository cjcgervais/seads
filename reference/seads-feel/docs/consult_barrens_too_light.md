# Consult packet — "the barrens read too light" (2026-08-11)

**For:** Fable 5, adversarial review.
**From:** the barrens-layer session (Opus 5), `sandbox/barrens-layer` @ `14c8bd3d6`.

Two things are asked, and the second matters as much as the first:

1. **The fly finding.** Chad flew the layer and reported it reads *too light* —
   "the black rocks exposed should be more evident because of contrast with the
   snow." Diagnose properly and rule between the fix options.
2. **Red-team this session's work** (the merge, the PNG-emission fold, the water
   re-clear, the certification). Assume it is wrong somewhere. Details in §6.

---

## 1. What the layer is

The 1970 industrial barrens around Sudbury: ~19,500 ha of soil-denuded,
smelter-blackened bedrock, from the City of Sudbury Regreening open data. It is
a **hero layer** — the single most recognisable ground feature on this map.

The design claim under test is that the barrens read from **inverted polarity**
(crests dark where `PAL_HIGHROCK` 206 makes them bright everywhere else) plus
the total absence of trees — *not* from absolute darkness. Chad's fly is the
first time that claim has met the air, and it did not survive contact.

The game is permanently winter and the whole globe is ridable.

## 2. The mechanism, exactly

`render/planet.cpp` fragment shader:

```glsl
float snowFlat  = smoothstep(uSnowSlopeLo, 1.0, dot(shN, normalize(fragDir)));
float snowCover = uSeasonSnow * (1.0 - water) * snowFlat;
snowCover      *= (1.0 - uBarrenSnowShed * barren);   // <-- the barrens term
albedo          = mix(albedo, vec3(uSnowAlbedo), snowCover);
```

Constants (`config/world.toml`, `render/planet.cpp`):

| | |
|---|---|
| `uSeasonSnow` (`winter_snow_cover`) | 0.85 |
| `uSnowAlbedo` (`snow_albedo`) | 0.92 |
| `uSnowSlopeLo` (`snow_slope_lo`) | 0.55 |
| `uBarrenSnowShed` | 0.75 (hard-coded, `planet.cpp:863`) |
| `PAL_BARREN` | (42, 40, 37) → luminance **0.156** |
| `ice_albedo` | 0.78 |

`barren` is the **G channel of `assets/sudbury_cones.png`** (R = shock fabric).

## 3. The measurement that explains the complaint

Barren field on LAND texels, distribution of the **nonzero** ones
(5,550,208 texels, measured on the shipped 4 m asset):

| percentile | value |
|---|---|
| p50 | **0.212** |
| p75 | 0.318 |
| p90 | 0.541 |
| p95 | 0.675 |
| p99 | 0.859 |
| max | 0.984 |
| mean | 0.259 |

Rendered albedo on flat ground (`snowFlat = 1`, `water = 0`):

| barren `b` | snowCover | **albedo** |
|---|---|---|
| 0.212 (median) | 0.715 | **0.702** |
| 0.300 | 0.659 | 0.659 |
| 0.456 (crest class mean) | 0.559 | 0.583 |
| 0.600 | 0.468 | 0.513 |
| 0.800 | 0.340 | 0.415 |
| 1.000 (no texel) | 0.212 | 0.318 |

Neighbours, fully snowed: `PAL_HIGHROCK` → **0.903**; mid forest/land → **0.850**.

**So the typical barren texel renders 0.70 against neighbours at 0.85–0.90 —
roughly 18% darker.** That is grey-on-white. Chad's report is not a taste
disagreement; it is the arithmetic.

Note also the ceiling: even at `b = 1.0`, shed 0.75 leaves `snowCover = 0.212`,
so albedo bottoms out at **0.318**. The rock can never fully show, at any
value, under the current term.

## 4. The structural reading (challenge this)

The field appears to conflate two different quantities:

- **coverage** — what fraction of this texel is barren ground;
- **shedding** — how completely barren ground refuses to hold snow.

A linear multiply makes a 21%-barren texel shed 16% of its snow. That is
*correct as coverage* and *wrong for the read*: the hero signal should come from
the mapped core zones **saturating** toward bare rock, with the feathered
margins doing the soft work.

Supporting number: the mapped 1970 barren zone is 19,491 ha, but the field
exceeds 0.5 over only 10,899 ha (~56%). The layer's own docs defend this ratio
as "the continuous field, not a decal, working" — that defence may be right for
the *albedo lerp* and wrong for the *snow shed*.

## 5. Fix options, none yet chosen

- **A. Raise `uBarrenSnowShed` → 1.0.** One constant. At `b = 1` gives full rock
  (0.156), but the median texel only moves 0.702 → 0.664. Probably insufficient
  alone.
- **B. Response curve before the shed**, e.g. `b' = smoothstep(lo, hi, b)` with
  something like (0.10, 0.45). Saturates cores, keeps feathered margins soft.
  Needs a principled choice of lo/hi, not a taste dial.
- **C. Re-scale the field itself** so mapped-zone cores sit near 1.0 and
  dose/topo modulate less aggressively. **Widest blast radius — see §5.1.**
- **D. Argue it is not too light at all** and that Chad is responding to
  something else (see §5.2). Say so if you believe it.

### 5.1 ★ The constraint that makes this hard

`barren` is **ONE field with TWO consumers** — this is invariant INV-9, and the
whole reason the G channel exists:

- **render** (this bug): `snowCover *= (1 - 0.75*barren)`;
- **sim** (winter S2): `depth *= (1 - k_barren*barren)`, `k_barren = 0.75`, in
  `world/snowpack.cpp` — it moves the **drive surface** the snowmachine rides.

Option C changes the ground under the sled. Option B applied in the shader only
would **fork the two** — which is precisely what INV-9 forbids.

**A finding this consult should weigh independently:** the winter side's
handoff states "crests ~1.0 shed ~75%, valley refugia ~0.30 shed ~22%". Against
the measured field that is wrong — the per-texel median is 0.212, so the actual
typical depth reduction is **~16%, not 75%**. Their figure appears to assume
crest barren ≈ 1.0. If so, the sim absorb is also far weaker than its authors
believed, and the fix may be shared rather than render-only.

### 5.2 Standing rulings — do not casually reopen

- **`PAL_BARREN` (42,40,37) is discharged and Chad ruled it not be re-tuned.**
  The reasoning: in winter, frozen lakes are bright ice (0.78) against land snow
  (0.92), so the summer dark-rock-vs-dark-water hazard inverts and dissolves,
  leaving a *larger* contrast budget. **If you believe the fix is in
  `PAL_BARREN` after all, say so explicitly and argue it** — but note the rock
  colour is barely visible through the snow term, so darkening it is likely to
  do almost nothing.
- `BARREN_CONE_FRAC = 0.85` (cones on the barrens) is Chad's option C. Not a bug.
- **INV-2**: the layer reads `elev`, never writes it. No bake-side height term.
- The render stub is **provisional** and retires only when the planet fragment
  consumes an interpolated depth attribute from
  `world::SnowpackField::ambient_depth_at`. It does not today (verified: zero
  hits in `render/planet.cpp`). So a fix here is a fix to a stub with a known
  end-of-life — **weigh how much investment that justifies.**

### 5.3 One more possibility worth testing

`snowFlat = smoothstep(0.55, 1.0, dot(N, up))` gates snow by flatness. The
barrens are *ridge crests* — relatively flat on top. If crests are reading fully
snowed while only side slopes shed, the flatness term may be fighting the
barrens term. Is the interaction of `snowFlat` and `barren` right, or should
barren ground shed **independently of slope**?

## 6. Red-team this session's work

Branch `sandbox/barrens-layer`; the relevant commits:

- `960ed4ec0` merge of winter S2+S2b; kept the provisional stub. The delete
  licence had been written as a rung name ("retires at S2b"), a commit titled
  S2b landed that was something else, and the licence was **not** cashed —
  re-verified against the tip instead. Was that the right call?
- `86cfef85c` folded a duplicate `sudbury_cones.png` decode created by the merge.
- `ff3d49da9` folded the PNG emission into `sudbury_bake.build()` (was a
  standalone script writing manifest-less, `bake_id`-less assets).
- `e9c2b6560` certification: 4 m re-bake, ctest 1064/1064, accept_map 16/16,
  accept_barrens 10/10, `projection.lock` carried LOCKED, §4 byte-identical gate
  passed (`dem`/`landmask`/`normal`/`buildings` unmoved; `color` and
  `treedensity` also unmoved though permitted to move).

**Specific things to attack:**

1. **The water re-clear.** `_emit_barren_assets._prefilter_resample` applies
   `pf[landmask] = 0.0` *after* the 48 m sphere blur, because the blur bleeds
   barren back over water after `build_barren_field` zeroed it. Justified by the
   tree density's precedent (hard discrete constraints re-applied post-blur;
   continuous gradients not). Result: full-water contamination 398,442 texels
   → 0. **Is the precedent actually analogous, or is this a category error?**
   The remaining shore-feather signal (53,080 texels, mean 5.9% depth cut) was
   judged *correct* — partial-coverage land share, with the ice lerp owning the
   water share. **Is that right, or is it rationalised leftover?**
2. **Was the pre-existing water contamination real?** Old shipped assets had
   398,442 full-water texels at up to 241/255 barren. I claimed full water was
   inert because `snowpack.cpp` lerps `depth → ice_snow_m` at `wfrac = 1`
   *after* the shed, so only the shore feather mattered. **Verify that
   ordering claim.**
3. **The emission ordering** (`barren` blurred first, `cone` built after) is
   load-bearing for memory — the other order OOM-killed a 4 m bake. Is there a
   correctness consequence to that order I have not noticed?
4. **Is emitting at `barren`'s last use the right seam at all**, or should the
   emission live at step 6 with the other resamples?

## 7. How to verify anything here

```sh
# worktree
D:/seads_sandboxes/barrens        # branch sandbox/barrens-layer

# python (numpy/rasterio/scipy); `python` on PATH is a venv WITHOUT numpy
D:/flight_sim2/seads/offline_tool/.venv/Scripts/python.exe

# gates
ctest --test-dir build -C Debug           # 1064/1064
python offline_tool/accept_map.py         # 16 legs, ~30 min
python offline_tool/accept_barrens.py     # 10 legs, ~2 min

# ★ validate at 16 m before ANY 4 m bake (~10 min vs ~100)
SEADS_SOURCE_RES=16 SEADS_SKIP_ACCEPT=1 python offline_tool/build_sudbury.py
git checkout -- assets render/sudbury_gis.gen.h   # every bake output is tracked
```

Key files: `render/planet.cpp` (the shader + `snow_shed`),
`offline_tool/sudbury_barrens.py` (the field), `offline_tool/sudbury_bake.py`
(`_emit_barren_assets`), `world/snowpack.cpp` (the sim absorb),
`docs/barrens_handoff.md` (state + standing rulings).

## 8. What a useful answer looks like

- A ruling on §5 A/B/C/D **with the number you would set and why that number**.
- An explicit position on whether the fix belongs to the **render only** or to
  the **shared field** — and if shared, what it does to the drive surface, given
  depth 0.77 m is a signed ruling Chad has already flown.
- Whether the winter side's "crests shed ~75%" claim is wrong as I read it.
- Any of §6 you can break. Please try hardest there; the fly finding is at least
  visible, whereas a bad asset-pipeline decision is not.
