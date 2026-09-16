# SF1 SPEC — the St. Charles snow mountain

Law: `WINTER_LAW.md §3.6b` (ruled 2026-08-12). Handoff: `Game_loop_idea/vehicle_program/SNOWHILL_HANDOFF.md`.
Branch `sandbox/winter-SF1` cut from SC1 tip `86c8dc5c2`. Worktree `D:/seads_sandboxes/winter-sf1`.

THE SETTLED ONTOLOGY (do not reopen): the mountain is DEPTH, never DEM height (INV-2 safe — a plow
pile IS snow, the §2.4c snowbank scaled up). ONE analytic shape fn is THE authority; TWO consumers:
(a) the snowpack adds it as a localized depth override → driven; (b) the hero mesh is generated FROM
the same fn → drawn. Function authoritative, mesh dressing. This is why SF1 does not wait on
BLOCK-VP1: the hill sees itself drawn because it brings its own mesh.

## 1. Anchor + frame (offline, one projection)

`offline_tool/sf1_snowhill_place.py` — mirrors `sudbury_hero_place.py` byte-for-byte in method:
`SudburyFrame.geo_to_dir` + orthonormal tangent basis (north projected, east = cross), emits
`world/snowhill_geo.gen.h`: `kSnowhillDir/East/North` (double[3] each) + the projection lock hash.
The gen header is consumed ONLY in `config/load_world.cpp`, which copies the frame into
`SnowhillParams{up, east, north}` and static_asserts the lock hash against `sudbury_gis.gen.h`
(buildings.cpp precedent — a projection re-bake without regen FAILS THE BUILD). The frame is DATA in
the params so the probe and every test leg inject a synthetic anchor on a flat analytic planet (the
sled_probe culture: a measurement that moves when the world is re-baked is not a measurement).

PLACEMENT (Chad's eye, a dial not a gate): school way 758270227 centroid 46.58213, −81.18947; yard
169 (E-W) × 129 (N-S) m; school fronts WEST on Charlotte ⇒ behind = EAST. "Right-centre in the
playground" ⇒ HILL = **lat 46.58204, lon −81.18875** (~55 m east, ~10 m south of centroid). Keeps
the NE-corner rail-trail mouth (46.58321, −81.18903, `snowmobile=designated`) clear as the approach
corridor. Top-down check image goes in front of Chad with the drive checklist.

## 2. The function — `world/snowhill.h` / `.cpp` (world-pure: glm + std + world only)

```
struct SnowhillPeak  { double dx_m, dy_m, h_m, sx_m, sy_m, rot_deg; };
struct SnowhillParams {
    bool   enabled      = true;
    glm::dvec3 up, east, north;   // anchor frame — DATA, injectable (red-team F4)
    double r_cut_m      = 60.0;   // exact-zero cutoff (localization is BIT-exact)
    double feather_m    = 15.0;   // C1 window width inside r_cut
    double class_min_m  = 0.30;   // hill_add above this ⇒ Surface::TrailMain
    double pack_cap_m   = 0.30;   // ★ the COMPACTED-pile skin (red-team F6, below)
    SnowhillPeak peaks[3];        // authored in config/world.toml [snowhill]
};
double snowhill_add(const HeightField* hf, const SnowhillParams& p, glm::dvec3 dir);
```

Local tangent coords (gnomonic, exact to mm over ≤ 100 m): `q = dir/dot(dir,up) − up`,
`x = hf->R · dot(q, east)`, `y = hf->R · dot(q, north)`. Peaks are anisotropic gaussians
`h·exp(−½(u²/sx² + v²/sy²))`, (u,v) = (x−dx, y−dy) rotated by `rot_deg`; SUMMED, then multiplied by
the window `w = smoothstep01((r_cut − r)/feather)` so the field is EXACTLY 0.0 beyond `r_cut`.
EARLY-OUT first, on CHORD² (red-team F9 — avoids the near-1 cancellation of cos(r_cut/R)):
`glm::length2(dir − up) > (r_cut/R)²` ⇒ return 0.0 exactly. Cost on the global path: two early-outs
per `sample_at` (depth_at + surface_at both call snowhill_add — the same pre-existing doubling as
`lines->nearest`; red-team F11).

Initial authored shape (tuned numerically by the mesh tool until the legs in §5 pass; constants live
in `config/world.toml [snowhill]`, loaded in `config/load_world.cpp` with `require()` + sanity
checks h ∈ (0,8], s ≥ 2.0):

| peak | (dx, dy) m | h m | (sx, sy) m | rot° | role |
|---|---|---|---|---|---|
| P1 | (0, 0)   | 5.4 | (4.0, 6.8) | −45 | main crest — narrow axis NW-SE gives the steep face |
| P2 | (+8, +8) | 3.0 | (6.0, 6.0) | 0   | broad NE shoulder — fills the backside into an ascent |
| P3 | (−8, +5) | 1.8 | (3.8, 4.4) | 0   | outlier hump — asymmetry, the third assemblage |

MEASURED (scratchpad tune_hill.py, 1401² grid): max h **6.13 m** at (1.2, 1.4); global max grade
**0.880** (41.4°); ascent lane bearing **0° (due N, toward the trail mouth)** max slope 0.573
(29.8° — the handoff's honest 25–30°); gentle crawl NE 30–60° at ~21°; jump face bearing
**150° (SSE, into the open yard)** max slope 0.869 (41.0°). These are the leg numbers in §5.1.

## 3. Composition (the likeliest silent bug, made structural)

`SnowpackField` gains `SnowhillParams hill;` (value, default-disabled when config absent).
★ P0 (red-team F1): `depth_at()` has FOUR return paths (no-linework, no-corridor-hit, inside-
corridor, final clamp) and the no-corridor-hit path is THE normal case at the hill site. Restructure
to a SINGLE EXIT: every existing path assigns `d` using the byte-identical existing expressions
(constraint F8 — the restructure must not reorder any pre-existing arithmetic, the localization leg
is `==`), then the one `return d + snowhill_add(...)`. The hill is thus added after the corridor
override and after every clamp on every path — plowing and `depth_max_m` can never zero or cap it.
`ambient_depth_at()` does NOT include the hill (render per-vertex ambient and black-rock exposure
keep their meaning; the hill brings its own mesh).

`surface_at()`: the hill check goes BEFORE the corridor branch (red-team F7 — hill WINS over any
corridor that ever creeps inside r_cut; today the trail mouth is ~130 m out, but the rule is
structural, not geographic): after LakeIce, `if (snowhill_add(...) > hill.class_min_m) return
Surface::TrailMain;` — compacted pile, TrailMain hardness family.

★ THE COMPACTED-PILE SKIN (red-team F6 — the finding that saves the feel). `pack_modulus` softens
with reported depth and `roost_flux` scales with it: TrailMain class + 6 m of reported `depth_m` is
MUSH with a saturated roost — the opposite of Chad's "carry speed up it". A pushed pile is
compacted through its body; only a skin is loose. So where `hill_add > class_min_m`, `depth_at`'s
REPORTED depth (and therefore `sample_at.depth_m` / `depth_under_m`) is `min(depth, pack_cap_m)`
while `drive_radius_at`/`drive_r` keeps `radius_at + FULL depth` — the geometry is the whole pile,
the sinkable snow is its skin. This is a DECLARED split, one field two readings, gated by leg §5.9
and named in the drive checklist for Chad's sign-off (it is the §3.6b compaction ontology made
concrete, not a new mechanic).

## 4. The mesh — generated FROM the function (conformance by construction)

`tools/snowhill_mesh.cpp` (headless, links world/): radial grid (rings ≤ 2.5 m spacing × 64
sectors, ≪ 65535 verts) sampling `mesh_h(x,y) = snowhill_add_local(x,y) + pad(r)` where
`pad(r) = ambient_pad_m · smoothstep01((r_pad − r)/pad_feather) − skirt(r)`: ★ ambient_pad_m is
COMPUTED at mesh-bake time as `ambient_depth_at(kSnowhillDir)` (red-team F2 — 0.78 was an
unverified assumption; the tool prints the sampled value and stamps it in the glb name comment),
falling to a skirt −0.6 m below drawn ground at the rim (no floating pancake edge). ★ `sink_m = 0`
(red-team F2: drawn crest = radius − sink + hill + pad; driven crest = radius + ambient + hill;
conformance requires pad − sink = ambient, so sink MUST be 0 with pad = ambient — the skirt alone
buries the rim). Writes `assets/heroes/snowhill_stcharles.glb` (minimal GLB writer: POSITION +
COLOR_0 + indices; per-vertex grayscale = snow tone 0.90–1.0, height-shaded so the cols between
peaks read). Axis mapping PINNED (red-team F10): **glb.X = y_north, glb.Z = x_east, glb.Y = h** —
runtime with `heading_deg=90` maps glb +X → north, +Z → east, +Y → up; the mesh tool writes
local-north into glb.X. Placement row appended in `sudbury_hero_place.py`'s HEROES list:
`heading_deg=90`, `sink_m=0.0`, `setback_m=0`, `tone=1.05`.
The runtime ingest path (`render/buildings.cpp append_hero_meshes`) is UNTOUCHED — the mountain is
just another hero. Night readability: silhouette + the steep SW face's lee shadow carry the read;
verify on the dusk smoke shot.

BLOCK-VP1 honesty: the mesh is draped at bare-DEM ground (hf.radius_at − sink), while the driven
yard is DEM + ~0.78 m. The hill core conforms to the DRIVEN surface (that is the pad's job); the
apron blends into the global render-vs-drive gap that BLOCK-VP1 owns. Stated, not smuggled.

## 5. Gate legs — `test/unit/test_snowhill.cpp` (ASCII names, all mutation-honest)

1. **Shape**: sampled global max of the summed field ∈ [5.6, 6.4] m (Chad's 20 ft); max |grad| ≤ 1.0
   (45°) everywhere; along the ascent bearing the max slope ∈ [0.45, 0.65] (24–33°); along the jump
   bearing max slope ≥ 0.70.
2. **Localization (bit-exact)**: with the hill enabled vs disabled, `depth_at` is `==` identical for
   every sampled dir with r > r_cut — including on a synthetic corridor, a bank, and an ice fixture.
   Protects the signed 0.77 field.
3. **Plow-composition**: a synthetic TrailMain corridor straight through the hill centre:
   `drive_radius_at(crest) − radius_at ≥ snowhill_add(crest)` (plowing cannot delete the mountain —
   stated on the GEOMETRY, since the F6 skin cap deliberately makes the REPORTED depth ≈ pack_cap
   at the crest; the original `depth_at ≥ snowhill_add` phrasing contradicted §5.9 and was fixed at
   implementation), while the same corridor 100 m beyond the footprint still reads `trail_pack_m`.
4. **Surface class**: crest reads TrailMain; outside r_cut the class is bit-unchanged vs disabled.
5. **Conformance (both directions)**: parse the shipped glb (our own subset), map verts glb-local →
   hill-local (documented ψ=90 mapping), assert max |vert_h − (snowhill_add + pad)| ≤ 0.10 m over
   the core (where the pad window == 1); direction two = grid density: max ring spacing ≤ 2.5 m so
   no feature at min σ 3.6 m can hide between verts.
6. **Drive probe** (`tools/sled_probe.cpp` new `snowhill` scenario, + a ctest leg with numeric
   asserts): flat synthetic hf + hill at origin; full-throttle run from 80 m out along the ascent
   bearing ⇒ summits (speed > 2 m/s throughout, `rolled == false`); crest exit over the jump face at
   speed ⇒ REAL AIRBORNE PHASE, asserted on OBSERVABLE state (SledState reports no per-patch
   normal): all three `susp_x` ≈ 0 (droop at zero load, emergent §2.4c.1) AND `sink_m` == 0 AND CG
   radius > drive_radius_at + rest clearance, held ≥ 0.25 s. ★ AMENDED AT IMPLEMENTATION
   (measured): "then lands upright" is NOT asserted — a full send lands at ~−14.6 m/s and the S3
   skid (7.5% sag, ζ 0.877, Packet B §7.3) topples in the runout, exactly as that packet predicted
   pre-suspension-rework. Landing survivability = S4 §9f + the owed skid rework; filed as an SF1
   drive finding in the graph, NOT hidden by hill retuning. Also measured and filed: the slow-climb
   LOOP-OUT (≤ ~13 m/s entry pitches past 75° on the upper face; cliff between 13.0 and 14.4 m/s)
   and the `rolled` 75°-cone readout FLASHING on lip rotations and bounces (S4/S8 must not trigger
   a dismount on a flash). The probe also prints a KICKER line (x=−11 over P3) as checklist data.
   Jumps are contact-patch physics over the resolved ramp — no scripted launch anywhere.
7. **INV-7**: flight goldens bit-unmoved (gate); `sim/` untouched entirely this rung.
8. Full `ctest` counts reported; `winter_plan_query.py check` exit 0; `graphify.py --stale`; ASCII
   TEST_CASE grep; commit by explicit path; law §3.6b prose + `winter_plan.json` SF1 fold in the
   same commit.
9. **Compacted-pile skin** (red-team F6): at the crest, `sample_at.depth_m ≤ pack_cap_m` while
   `drive_r == radius_at + full depth sum` (both asserted); outside r_cut the reported depth is
   bit-unchanged. Kills the mutation `pack_cap applied to drive_r` and the mutation `cap removed`.

## 6. Out of scope (stated so nobody smuggles them)

- No kernel edit (sim/ frozen this rung; the S4 §9f landing budget consumes the hill later).
- No BLOCK-VP1 generalization; no change to ambient f(), corridors, banks, ice, barren.
- No scripted jump/launch/ramp logic anywhere.
- Whoops/small forms: dead until damping is fixed (Packet B §7.4) — the mountain is BIG on purpose.
