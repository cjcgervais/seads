# The Sudbury Industrial Barrens — hero map layer (B1)

Branch `sandbox/barrens-layer`, worktree `D:\seads_sandboxes\barrens`, forked from
`sandbox/world-dem` @ 96a685489 so it inherits the hybrid conformal radial law
untouched. The DEM repair/bake/red-team is live in `D:\seads_sandboxes\world-dem`
and **is not touched by this branch**; this layer rebases onto the repaired DEM
when that lands.

## What the layer is

The blackened, smelter-killed bare bedrock around Sudbury — the "moonscape" —
at its **1970 pre-Superstack maximum**. One equirect raster,
`assets/sudbury_barrens.png` (L8, 8192x4096, 0..255 == barren intensity 0..1),
baked exactly like `sudbury_treedensity.png` and consumed at runtime by the same
`equirect_uv(fragDir)` — so it is projection-agnostic and rides the projection
lock like every other channel.

## Primary source — VERIFIED LIVE, no login

**City of Greater Sudbury, Regreening Program.** Authored by the people who ran
the reclamation; this is not a proxy or an index, it is *the* mapped barren
boundary.

| layer | endpoint | content |
|---|---|---|
| `Barren_Rings` | `services.arcgis.com/q3mIlR87lZlZsds3/arcgis/rest/services/Barren_Rings/FeatureServer/0` | 4 polygons: 3x `1970 Barren Limit` (19,525 ha), 1x `1970 Semi-Barren Limit` (83,725 ha) |
| `Lime_Treatment_Site` | `.../Lime_Treatment_Site/FeatureServer/1` | 813 polygons, `TreatmentDate` 1978-2025, the reclamation-by-year footprint |

Native CRS EPSG:26917 (NAD83 / UTM 17N); requested as GeoJSON in EPSG:4326 and
pushed through the existing `SudburyFrame` — no new projection math anywhere.

Measured in the aeqd frame about (46.5560, -81.1100), the three barren lobes are
the three smelters, and every one is inside the disk:

| lobe | area | centroid rho | bearing | max rho | reads as |
|---|---|---|---|---|---|
| A | 7,571 ha | 9.59 km | 149.3 deg | 15.11 km | **Copper Cliff** |
| B | 4,841 ha | 22.85 km | 85.1 deg | 26.42 km | **Falconbridge** |
| C | 7,079 ha | 22.57 km | 109.8 deg | 26.62 km | **Coniston** |
| semi-barren | 83,576 ha | 15.58 km | 107.5 deg | 34.15 km | the whole envelope |

Projection note, stated rather than discovered later: lobe A is entirely inside
`RHO_CONFORMAL_START_M` = 22 km, i.e. bit-exact 1:1. Lobes B and C straddle the
[22, 32] km conformal blend, so they shrink *evenly* (k_r == k_t) and stay round
— the blend is shape-preserving, which is the whole point of Chad's Option B.
Nothing about this layer needs the law changed.

Licence: City of Greater Sudbury open data hub, CC-BY-SA. Attribute the City.
Landsat (below) is US public domain, unrestricted.

## The law: a continuous field, not a decal

A hard fill of the ring polygon would read as a sticker from the air. The
literature gives the shape of the real gradient, and we have a DEM, so the
raster is a product of three terms, each in 0..1:

    barren = zone(x) * dose(x) * topo(x)

**zone** — from the polygons. 1.0 inside a Barren limit, `SEMI_WEIGHT` (0.45)
inside Semi-Barren, 0 outside, with a signed-distance feather (`ZONE_FEATHER_M`)
across each boundary so no edge is a cut line.

**dose** — radial decay from each of the three real smelter sources, combined as
a max (three overlapping halos, not one centred blob):
  - Copper Cliff smelter / Superstack  46.4731 N, -81.0694 W
  - Coniston smelter                   46.4867 N, -80.8497 W
  - Falconbridge smelter               46.5810 N, -80.8000 W
`dose = max_i clamp(1 - (d_i / DOSE_FALLOFF_M)^DOSE_EXP)`, floored at
`DOSE_FLOOR` so the zone polygons still carry their own edges.

**topo** — the confirmed Freedman & Hutchinson 1980 rule, and the reason this
layer *matches the relief* instead of floating over it: on the SSE transect there
was no forest inside 3 km; from 3-8 km the surviving forest was confined to
**valley bottoms and sheltered slopes** while the **hilltops were bare, soil-
denuded and blackened** out to 15 km. So:

    tpi   = elev - gaussian_blur(elev, TOPO_RELIEF_M)   # local relief position
    topo  = TOPO_FLOOR + (1-TOPO_FLOOR) * smoothstep(-TOPO_SPAN, +TOPO_SPAN, tpi)

Ridge crests -> 1.0 (killed). Valley floors -> `TOPO_FLOOR` (0.30, refugia).
`TOPO_RELIEF_M` = 400 m, above the 59 m mesh cell and below the basin scale.
The elevation used is the same `elev` array the DEM bake already holds, so the
barrens can never drift from the terrain they sit on — single source, no re-read.

Water is hard-zeroed. The wilderness annulus past `WILD_FADE_IN_M` fades to 0
like every other real-data channel.

## The three consumers

1. **Albedo.** `PAL_BARREN = (42, 40, 37)` — charcoal, not pure black (the
   literature is explicit that the rock reads dark charcoal-grey with the
   underlying norite showing, not ink). Lerped in by `barren` over whatever the
   value ladder would otherwise paint. This is the hero read: in a B&W newsreel
   ladder where bare rock is the *brightest* class (`PAL_HIGHROCK` 206), the
   barrens invert it to the darkest land value on the map. Ridge crests go black
   while the valleys between them stay grey — the relief draws itself.

   **Corrected from (30,30,33) by the red team, and the reason matters for
   gameplay.** Every rung of the existing ladder steps >= 15 dL*. At value 30 the
   barrens sit 5.1 dL* from `PAL_WATER` (20) — about a third of the ladder's own
   spacing. In daylight a big lake is bright silver (`[water] reflectivity 0.90`)
   so there is no confusion, but only ~66-80 lakes get a dedicated mirror mesh
   (`WATER_SURFACE_MIN_SPAN_M` 900); everything smaller rides the cubemap alpha
   and mip-filters to fractional coverage, landing near value 26 — versus a
   barren ridge at 30. That is a **landable-pond misread in a float-plane sim**,
   at night and under overcast especially. 42 restores dL* ~11.1, with a
   deliberate WARM bias (+5 R over B) against `PAL_WATER`'s cool bias — a second,
   independent cue, and truer to rust-black smelter rock than neutral ink.
   The moonscape does not read from absolute darkness anyway; it reads from the
   **inverted polarity** (crests dark where crests are bright everywhere else)
   and from the **total absence of trees**, which no palette value can imitate.
2. **Tree density.** `dens *= (1 - barren)`. This replaces the existing
   `TREE_HIGHROCK_FRAC` guess — a heuristic "blackened high rock" band keyed on
   elevation fraction — with the real 1970 footprint. The guess stays as the
   fallback outside the mapped envelope.
3. **The shatter-cone detail mask** (see below) — the same channel, gating a
   runtime shader. Costs nothing extra to bake.

## Optional corroboration: Landsat 5 TM, 1993

Not required for the layer, used as an **acceptance check** that the 1970
polygons still land on ground that reads barren. Verified available anonymously:
Microsoft Planetary Computer STAC `landsat-c2-l2`, scene
`LT05_L2SP_019028_19930901_02_T1` (1 Sep 1993, 1% cloud) — WRS-2 path 019 / row
028 covers the whole 43 km disk in one scene. NDVI + the TM 5/7 SWIR ratio
separate blackened bedrock from boreal forest. Needs `pystac-client` +
`planetary-computer` (not currently in the bake venv), so this leg is gated
behind `--landsat` and is never on the critical path.

## Dials

All in `sudbury_barrens.py`, all fly-tunable, all bit-exact-off at their
zero values:

    BARRENS_YEAR        1970    # 1970 = the mapped peak; >=1978 subtracts the
                                # cumulative Lime_Treatment_Site union for a
                                # reconstructed later-epoch footprint
    SEMI_WEIGHT         0.45
    ZONE_FEATHER_M      600.0
    DOSE_FALLOFF_M      14000.0
    DOSE_EXP            1.35
    DOSE_FLOOR          0.35
    TOPO_RELIEF_M       400.0
    TOPO_SPAN_M         18.0
    TOPO_FLOOR          0.30
    BARREN_GAIN         1.0     # 0.0 == layer OFF, bit-identical

## B2 — the integration patch, with line numbers verified in this worktree

Not yet applied. `sudbury_bake.py` is being actively edited by the DEM repair on
`sandbox/world-dem`; applying this now guarantees a conflict on a file where a
bad merge silently moves the whole world. It lands after that rebase.

**Albedo — insert at `sudbury_bake.py:645`**, after both `_paint` calls (642-643)
and before the shore-erosion block (646). VERIFIED: `_paint` is a **hard
assignment** (`base[m] = color`), not a lerp. Anywhere earlier and the Copper
Cliff industrial polygon wipes the barrens off lobe A wholesale — the single most
important barren area on the map, and invisible in a preview thumbnail. Water is
then protected for free by the hard assignment at 696.

Suppress barren where the town paint landed (`b_eff = barren * (1 - painted_soft)`,
~200 m blur of `urban | indus`): slag, roofs and yards are not blackened bedrock,
and at full strength the barrens take `PAL_INDUSTRIAL` 86 down to 42, killing the
"there is a works here" cue exactly at the hero and floating the Superstack and
the building prisms over near-black ground.

**Tree density — insert immediately after `sudbury_bake.py:728`**, i.e. between
`dens = np.clip(...)` and `del elev_shape, ...`:

    dens = (dens * (1.0 - barren)).astype("float32")

VERIFIED ordering, and the reason is the same lesson the DEM keeps teaching:
this must be **before** `_sphere_metric_blur` at 741. Multiplying an
already-anti-aliased field by an un-anti-aliased mask re-injects the mask's high
frequencies straight into the resample — far-ring aliasing, on the hero layer.
Before the blur, the *product* is filtered as one field.

It must **not** get the post-blur re-clear that `landmask`/`excl` get at 743-744.
Those are hard discrete correctness constraints (a tree in a lake is an artifact).
`barren` is continuous, and blur bleed across its boundary **is** the soft
gradient the design is for. Re-clamping would re-sharpen it into the decal edge
the whole layer exists to avoid. The symmetry with 743 will tempt the next
reader — the code says so in a comment.

`TREE_HIGHROCK_FRAC` **superposes rather than being replaced**, which the earlier
draft of this spec got wrong: `elev_shape` stays fully active inside the envelope,
so mapped barren ground is zeroed twice and the footprint ends up with *less*
contrast than intended. Worse, the two disagree on physics — `elev_shape` keys on
absolute elevation fraction, `topo` on local TPI, so a low ridge crest inside a
lobe and a high valley floor inside a lobe are both handled for the wrong reason.
Gate the heuristic out where the real data governs:

    elev_shape = elev_shape * (1 - env) + env      # env = the feathered zone

**Registration.** New PNG must be wired in three places or nothing checks it:
`sudbury_header.py` (~643, `kSudburyBarrensW/H` beside the tree-density dims),
a new `TEST_CASE` in `test_asset_validator.cpp` against the PNG IHDR, and the
committed asset. **The test name must be pure ASCII** — a non-ASCII Catch2 name
silently never runs under ctest, which has bitten this repo four times and is now
a `gate.sh` tripwire. A barrens test that never executes is worse than none.

**What must NOT move.** At gain 1, `sudbury_dem.png`, `sudbury_landmask.png`,
`sudbury_normal.png` and `sudbury_gis.gen.h` stay byte-identical; only `color`,
`treedensity` and the new `barrens` may differ. The layer only ever *reads*
`elev` (`np.nan_to_num` returns a copy — checked), so `emin`/`emax` cannot move
and `remap_range.lock` needs no re-pin. That partition is the strongest single
gate available and B2 is gated on it.

**Runtime cost is not free.** `planet.cpp` packs albedo RGB + water coverage in
A — there is no spare channel. Consumer (3) needs a second cubemap: a new
sampler, a second `bake_equirect_cubemap` call, ~25 MB VRAM. Budgeted, not
hidden. Also: `planet.cpp:156` mixes snow over albedo, which would erase the
barrens in winter — blackened rock sheds snow early in reality, so this wants
`snowCover *= (1 - barren)` or an explicit "summer layer" ruling.

## Shatter cones — feasibility

Answered in full in `docs/shatter_cone_feasibility.md`. Short version: the
*rendering* is feasible and cheap; the *geology* is licence, and Chad should know
which is which before he rules.
