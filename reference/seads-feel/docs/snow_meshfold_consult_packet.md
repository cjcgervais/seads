# CONSULT PACKET — BLOCK-VP1: fold the snow depth into the RENDERED planet surface

Branch `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow`, base `f92939e2e`.
Engine: C++17, raylib 5.5 + OpenGL 3.3, MinGW GCC + Ninja. Planet radius **R = 15 000 m**
(a "little planet"; the whole surface is real Greater Sudbury from CDEM + OSM).

**You are being consulted context-free. Everything you need is in this document. Do not assume
familiarity with the codebase; every claim below is either a file:line citation or a measurement
taken on this tree during this session.**

---

## 1. THE ASK, IN THE OWNER'S WORDS

Chad (the owner; the only authority on feel) after driving the current build, verbatim:

> "there are no ruts, its depressing the whole square area"

> "yeah the squares gone but I dont see any ruts, and thats not the only point I should see skis
> going into snow, deeper snow should bury me a bit, when I give throttle and plane out I should be
> planing out over this snow and not just making tracks but depressing snow... Snow that is properly
> shaded and felt, not some cheap drawn in illusion..... I though yuo were going to bake me in some
> snow on this whole planet?"

> "Also deeper snow should get more deformation, hards pack snow would be less, the trail less bush
> should show a shallow trench maybe 1/3 a meter behind me, giving a tell of the depth. Also you
> mention ruts, but you didnt mention the track mark, should be two ski tracks and a central track
> mark as broad as the track right"

His ruling on the 1/3 m figure, clarified this session: **it is the trench DEPTH in deep bush**
(bush sinkable depth measures 0.71–0.73 m here, so the trench cuts roughly half of it), collapsing
to a few centimetres on packed trail. The trench depth is meant to be a **readout of local snow
depth**.

A standing ruling from an earlier session also binds: **"NOTHING DYNAMIC IN THAT"** — no form may
respond to cold, weather, time of day, or use. (Deformation by the machine is not "dynamic" in that
sense; it is the machine cutting the surface. Ambient forms responding to weather would be.)

---

## 2. THE DEFECT — BLOCK-VP1, a known, named, escalated, still-open block

`docs/snow_info_packet_winter_to_barrens.md:24-30`, verbatim from the repo:

> **BLOCK-VP1 (render vs drive)** — *"the renderer drapes every mesh on the bare DEM while the sled
> drives on DEM + snow depth — a ~0.77 m gap in bush, everywhere — and terrain cells are ~59 m so no
> metre-scale snowform can exist in the render mesh at all."* Severity TOP, needs=Chad.

`render/bank_mesh.h:22`: *"BLOCK-VP1 itself stays open and Chad's for the global field."*

`render/planet.cpp:316-257`, on the current drawn snow: *"today's snowCover is a render-only
approximation (uSeasonSnow * (1-water) * snowFlat) that predates the depth function, so this is a
stub on a stub, kept only so the layer is visible before S2 exists."*

**So: snow is currently a fragment-shader albedo whitening term. Paint, not surface. The machine
drives ~0.77 m above the drawn world, everywhere, always.**

Chad has now ruled: fold the depth into the rendered surface, **runtime mesh fold** (not the offline
bake route — that touches a locked asset pipeline owned by another workstream and needs their
sign-off; he chose the reversible route first).

---

## 3. THE SURFACES AND WHO READS THEM

### 3.1 The snowpack function (`world/snowpack.{h,cpp}`) — PURE glm+std
Analytic, globe-wide, derived from the ONE height field:

    depth = f(slope, aspect, curvature, elevation, drainage_accumulation)

`world/snowpack.h:22-27` states its design purpose outright: **"IT IS THE HOMOGENIZER"** — depth
loads in concave curvature and scours on convex, so `terrain + depth` is a curvature-weighted
smoothing of the DEM. Key dial: `config/world.toml:385 curv_probe_m = 40.0` — the finite-difference
step for slope and curvature.

Accessors:
- `ambient_depth_at(dir)` — the ambient field alone.
- `depth_at(dir)` — REPORTED **sinkable** depth (bank-capped, hill-capped). What sinkage reads.
- `depth_geometry_at(dir)` — the DRAWN shape channel (uncapped, carries corridor `deck_lift_m`).
- `drive_radius_at(dir)` — **the driven surface**: `ground_radius_base + geometry_depth_term +
  lift_m + snowhill_add`. The one place terrain and depth are summed.
- `ground_radius_base(dir)` — `hf->radius_at(dir)` unless `[snowpack] hf_faceted_ground` (default
  **false**).

Surface classes (`world::Surface`): Bush, TrailMain, TrailTributary, Road, LakeIce, RockOutcrop,
MineWorks.

### 3.2 The rendered planet (`render/planet.cpp`)
- Cubesphere: 6 faces x `tiles^2` sub-meshes; `config/world.toml:600-602` `subdiv = 200`,
  `tiles = 2` → effective grid `tiles*(subdiv-1)+1` verts/edge → **~59 m cells**.
- `render/planet.cpp:722-723`: *"fill_face(p.height,…) == p.height.radius_at at every vertex
  (pinned in test_sphere_param)"*. **The mesh vertex radius IS `radius_at`, and that identity is a
  pinned test.**
- The heightfield `p.height` is filled at `render/planet.cpp:694-700` from the DEM png and is
  retained **POST-BLUR** (`MESH_BLUR_SIGMA_M = 100` in the offline bake) — flagged in-file as
  "the H1 anti-fork". A tool that re-decodes the png does NOT get the same surface.
- Lighting lives ONLY here: `uGroundDayGain`, `uNightFill`, `uMoonFill`, `uWinterNightGlow`,
  `sky_aerial()` — 20 hits in `render/planet.cpp`, **zero** in `render/buildings.cpp`.
- `render/planet.cpp:193`: `vec3 shN = (uHasNormalMap > 0.5) ? surfN : normalize(fragNormal);`
  Sudbury **has** an M2 object-space normal cubemap baked from the DEM, so **vertex normals are
  discarded** for shading. Any geometry added to the mesh will shade flat unless this is addressed.

### 3.3 `facet_radius_at` — the drape reference, and the blast radius
`render/sphere_param.h:170-178`: it reproduces, analytically, the radius the MESH shows between its
vertices (the `fill_face` triangle interpolation), because *"radius_at is the FIELD, but between
mesh vertices the screen shows the facet"*.

**Consumers (every one of these drapes on it and will detach if the mesh moves and this does not):**
- `render/ribbons.cpp:165` — road/trail ribbons (`facet_radius_at + lift`)
- `render/bank_mesh.cpp:177,206` — the oreo plow banks
- `render/river_surfaces.cpp:45` — rivers
- `world/snowpack.h` — the `facet_radius_fn` injection for `hf_faceted_ground`
- `render/snow_patch.{h,cpp}` — this session's work (see §5)

**Also anchored to `radius_at` directly:**
- `world/props.cpp:171` — `d * hf.radius_at(d); // anti-float: the mesh's own height field`
  (trees/props). These will float ~0.77 m if the ground rises and they do not.
- Buildings (`assets/sudbury_buildings.bin`, 86 503 collision prisms), lamps, the lake mirror
  meshes (`render/water_surface.cpp`, drawn with the planet's own program, `uForceWater=1`).

### 3.4 The felt half already exists and is deeply tuned
`sim/sled.h` carries Bekker pressure-sinkage, a `bury` term, `plane_frac`, planing lift, roost flux,
`track_clearance_m`, and a cold-stiffened pack chain. **The physics of sinking and planing is real
and tuned; only the drawn surface is missing.** Machine dimensions, measured (`sim/sled.h:398-405`):

    stance_m       = 0.927   // ski centre-to-centre
    ski_width_m    = 0.135   ski_len_m   = 0.95
    track_width_m  = 0.38    track_len_m = 1.14   // the belt's ground contact

---

## 4. MEASUREMENTS TAKEN THIS SESSION (on this tree, on the real DEM and real linework)

All taken through an in-binary instrument, because the retained heightfield is post-blur and a
standalone tool would measure a different surface.

1. **Terrain relief in the band the owner actually rides** (2 013 transects, 173 118 samples, 9–85 m
   perpendicular from road/trail centrelines — his own drive tapes put him at p50 19–24 m):
   - DEM relief peak-to-peak: p10 0.73 m, **p50 4.37 m**, p90 19.80 m
   - Snowpack depth modulation: p10 0.102 m, **p50 0.682 m**, p90 1.186 m
   - Snow is ~15.6 % of driven relief.
2. **The depth field has no snow-scale detail**: mean |depth change| per 2 m step = **0.0259 m**,
   i.e. a **0.74° local slope**. Consequence of `curv_probe_m = 40.0`. It is smooth by construction
   at 40 m, so a fine mesh shows nothing about it a 59 m mesh could not.
3. **FIELD minus FACET** (how far the true height field rides off the drawn mesh) at the shipped
   `subdiv=200, tiles=2`: p01 −0.013, p50 **+0.091**, p99 +0.274, min −0.086, max +0.292 m.
   (An older in-repo figure of "p99 ≈ 6 m" was measured at `subdiv 200` UNTILED and no longer
   applies.)
4. **`depth_at` cost**: **~1.06 µs/call**; `ambient_depth_at` ~0.77 µs. The cost is the
   finite-difference slope/curvature sampling, not the corridor search — it does not optimise away.
   A 96×96 sample grid is 9.8 ms.
5. **Ground clipping is NOT the blocker** (an earlier plan assumed it was): open-snow pixels at peak
   day are only **2.2 % clipped**, retaining ~70 DN of spread.

---

## 5. WHAT THIS SESSION BUILT, AND WHY IT IS BEING RECONSIDERED

Uncommitted, working, gated 102/102 on the named subset:
- `render/snow_patch.{h,cpp}` — a rider-following ground patch (160×160 nodes @ 0.5 m), refreshed
  incrementally (rows/frame) to bound cost, sharing the planet material so it lights as ground.
- `world/tracks.{h,cpp}` + `test/unit/test_tracks.cpp` (9 cases, 178 assertions) — a `TrackField` of
  path stamps in a uniform hash grid; deformation composed **into `drive_radius_at`** beside
  `snowhill_add`, so the sled feels its own ruts and there is no second representation.
- A shader change: a `uVertexNormalMix` uniform that blends the patch's own geometric normals past
  the M2 normal-map override, weighted by a per-vertex rim weight. Planet pass sets it 0, and
  `mix(x,y,0) == x`, so that pass is bit-identical.

**Why it is being reconsidered.** The patch first drew `drive_radius_at` — "draw exactly what you
drive". That produced a ~0.9 m slab standing proud of a world with no snow in it, and the rim fade
made it a visible SQUARE (the owner's first quote). Reverting the patch to draw only the
deformation removed the square but left the ruts invisible and the world still 0.77 m low. **The
local patch is the wrong altitude of fix; the owner has rejected it as "cheap drawn in illusion".**

---

## 6. WHAT WE PROPOSE, AND WHAT WE WANT YOU TO ATTACK

**Direction:** fold the ambient snow depth into the rendered planet surface globally at mesh build
(`radius_at + ambient_depth_at` at every cubesphere vertex), move `facet_radius_at` and every drape
consumer with it in the same pass, and carry the metre-scale track deformation in a separate
mechanism.

### Questions we need ruled

**Q1 — Is the runtime mesh fold sound at this resolution?** 6 × 2² × 40 000 = 960 000 vertices ×
~0.77 µs ≈ **0.74 s** of added load. But `ambient_depth_at` needs `hf`, which exists before mesh
build — while the FULL `depth_at` needs the LineNetwork/landmask/barren, which are built AFTER the
planet. Is folding only the AMBIENT term correct, given corridors and banks are already drawn by
`ribbons.cpp` / `bank_mesh.cpp` as separate geometry on top? What double-counts?

**Q2 — `facet_radius_at` must move with the mesh or every drape detaches.** It is currently a pure
function of `HeightField`. Folding depth means it must also take the snow params. Is there a
formulation that keeps the anti-fork guarantee (the drawn facet == what the mesh shows) without
turning a cheap pure function into a ~0.77 µs call on paths that use it per-vertex
(`ribbons.cpp`, `bank_mesh.cpp`, `river_surfaces.cpp`)? Caching? A baked snow height texture
sampled the same way the DEM is?

**Q3 — THE BARRENS TRAP, flagged in-repo.** `docs/snow_info_packet_winter_to_barrens.md:38-42`:
depth over `barren=1` is only ~0.15 m, so if ground rises ~0.77 m everywhere EXCEPT rock, the
barrens become **carved-out hollows**. What is the correct joint treatment so both surfaces move in
one pass? Is this an argument for folding depth into the DEM asset instead of the mesh?

**Q4 — Shading.** `render/planet.cpp:193` discards vertex normals wherever the M2 normal cubemap
exists. If the mesh gains 0.68 m of snow relief at 59 m cells, the normal map (baked from the
unblurred DEM, no snow) will shade the OLD surface. Does the M2 map need re-baking with snow, a
runtime perturbation, or should the fold move to the DEM bake so one map covers both? Note the
owner's demand: **"Snow that is properly shaded"**.

**Q5 — Does the fold actually buy the look?** Measurement 2 says the ambient field changes 2.6 cm
per 2 m (0.74°). Folding it moves the surface up 0.77 m but adds almost no *shading* variation, so
it may fix "the machine floats above the world" without making snow read as snow. Is the fold
necessary-but-insufficient, and what is the second half? Candidates: a static high-frequency
micro-relief term (sastrugi / wind ripple / drift lips at 0.3–3 m); a snow-specific BRDF; the
deformation itself.
**Constraint:** any micro-relief that is DRAWN but not DRIVEN re-creates BLOCK-VP1 at a smaller
scale. Any that is DRIVEN changes feel and needs the owner's stick. Which is right?

**Q6 — THE TRACK SIGNATURE.** Required cross-section: **two ski cuts (0.135 m wide, centres
±0.4635 m) plus a central belt mark 0.38 m wide** — overall span ~1.06 m. Depth must be
**proportional to local snow depth**: ~0.33 m trench in bush (where sinkable depth is 0.71–0.73 m,
so ~half), collapsing to a few cm on packed trail; harder pack deforms less. A 0.135 m ski cut
cannot be drawn on the current 0.5 m patch grid — it needs ~0.05 m cells, which no grid covering
useful ground can afford.
**Is a ribbon mesh built ALONG the path (the `bank_mesh.cpp` pattern — vertices only where the track
is) the right vehicle, rather than sampling into a grid?** If so, how should it meet the (now
snow-bearing) planet mesh without a seam, and how should the deformation still reach
`drive_radius_at` so the machine feels its own ruts — the property the current `TrackField`
guarantees and which we do not want to lose?

**Q7 — Ordering and reversibility.** What is the correct rung order such that each rung is
independently drivable and judgeable by the owner? He has rejected two builds for shipping
something that looked plausible but was not what he asked for. What is the smallest first rung that
proves the fold before the blast radius is paid?

### Constraints that bind any answer
- `sim/` and `control/` are a FROZEN kernel; the world thread may not touch them.
- No ctest runs `seads.exe` — a green gate proves nothing about anything the owner looks at. Every
  visual claim needs a screenshot or his drive.
- House law: **no guessing.** Every number describing a real thing comes from a measurement or the
  owner's words.
- One authored form (a 6 m plow mountain, `world/snowhill.*`) ships and stays; the owner ruled it
  stays but that no new authored forms are to be built.
- A prior authored-form build was rejected for being "half buried, grey, and flashing" — the
  failure mode was a drawn mesh disagreeing with the driven surface. Do not propose anything that
  reintroduces a second representation of the same surface.
