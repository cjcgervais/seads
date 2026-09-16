> **SUPERSEDED 2026-08-21 by `docs/SESSION_HANDOFF_20260821_costume_C1.md`.**
> A second attempt (T1) was built and rejected; Chad ruled that the grey
> box-and-prism BODY must be re-authored first. **Launch from the C1 file.**
> This file's SS2 (measurements) and SS5 (traps) remain true and are still
> worth reading; its SS4 plan ordering is superseded.

# HANDOFF — R2c-9 "COVER HIM UP": the costume rung. **STOPPED AT BLOCKOUT GRADE, BY CHAD'S CALL**

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws, `docs/RIDER_AUTHORITY.md`,
then §1–§3 of this file. A T0 blockout costume EXISTS and is measured-correct
against Chad's topology ruling, but he judged the QUALITY insufficient and
RULED the target: **T2 — authored mesh + textures, the ladder's own §R2 target.**
Do not polish the primitives. Start at §4, THE PLAN, rung C0."*

---

## 0. STATE IN ONE LINE

Chad's 2026-08-21 ruling was BUILT and MEASURED GREEN on every clause — and
then he stopped the rung: *"not the quality level I was hoping for."* The
geometry is lofted primitives on a box-and-prism blockout, and that is the
ceiling of the approach, not a tuning problem. **The four topology rules he gave
are worth keeping; the geometry that satisfies them is not.**

## 1. WHAT CHAD RULED (verbatim, 2026-08-21)

> "The scarf needs to be wider vertically around the neck the wall of the ring
> needs to go up to the chin and the hoodie fits under it, but the hood part is
> at the back and covering the back of the scarf ring and going over the knot of
> the scarf. But the hood goes within the scarf at the front (covered by it) and
> scarf is covered by hood at the back the rest just covers the rest of the body
> arnms and legs snowpants, and the rest of the hoodie. Cover him up and make
> the scarf free ends rest atop rather than going through"

Two rulings taken by question, because they supersede signed decisions:

| ruling | supersedes |
|---|---|
| **TWO-PIECE: hoodie + snowpants** | ladder §4 ★★ 2026-08-17 "Suit: Grey, ONE-PIECE Klim Ripsa" — **now SUPERSEDED.** Bonus: the two-piece wins back the waist break §3 rule 3 lost to the one-piece |
| **GREY hoodie, BLACK snowpants**, kComplementBlue stripe on the outer arm and the outer leg (the stripe IS the side zip, §4) | nothing; §4 never named the pants |
| **QUALITY = T2** (2026-08-21, after seeing T0): authored garment mesh + UVs + 2048² textures, i.e. the ladder's own §R2 target | the T0 blockout built this session |

## 2. THE MEASUREMENTS — KEEP THESE, THEY COST THE SESSION

Every one is off the live session or the applied mesh, none inferred. A T2
re-author needs all of them and should not re-derive them.

| thing | MEASURED value |
|---|---|
| **THE CHIN** — what "up to the chin" means | the **Sudburian's own head box bottom, bind z 1.6087**. NOT a helmet chin bar: the v13 helmet's front centre is **OPEN below z 1.8927** (the face port). Anyone who reads "chin" as the helmet will build the ring 250 mm too tall |
| the ceiling over the ring, per azimuth | `measure_neck_ceiling.py` (committed, re-runnable). 24 bins; helmet rim as low as **1.5822** at the sides (x ±0.137), 1.6087 chin at the front, 1.7056 at the back |
| the ring's top edge | a **PROFILE, not a flat cut** — a flat top cannot both reach the chin (1.6087) and duck the helmet rim (1.5822): they are 26 mm apart. `scarf_geom.loop_z_hi()` |
| **what the hood must clear** | the scarf's REAL radius per azimuth, `costume_geom.SCARF_R` — up to **0.2704 at th 225**, because the tails are 132 mm-wide FLAT STRIPS and a strip's *width* sweeps radially. The spec's stated 0.215 was a true statement about the CENTRELINE and the wrong answer to this question |
| body / rig bind frame | full dump in `costume_geom.py`'s banner (bones, torso boxes, girth radii, foot box) |
| the mitt's join | the mitt cuff meets the forearm at **t/L 0.91, radius 0.0225** about the forearm axis — a sleeve ending at 0.93 hides the join |
| helmet family | `helmet_*` incl. the four baked dent variants; measure against ALL of them, never one prim |

## 3. WHAT IS ON DISK AND IN THE SESSION RIGHT NOW

**Committed to nothing — the tree is dirty and NOT committed.**

| artefact | state |
|---|---|
| `assets/character/sudburian_src/costume_geom.py` | NEW. T0 generator: 19 closed outward shells, 2004 verts / 3936 tris — hoodie torso+sleeves+hood, snowpants hip+thigh+calf, boots, 6 stripes. Pure stdlib, per-piece census (`python costume_geom.py` = the census, exit 1 on any defect) |
| `assets/character/sudburian_src/apply_costume_live.py` | NEW. Appends it to the live proxy, idempotent by material slot, with the body-untouched proof. Never saves |
| `assets/character/sudburian_src/verify_costume_live.py` | NEW. **The valuable one.** Measures Chad's four clauses off the APPLIED mesh by ray-parity containment. Keep it for T2 — the rules do not change, only the geometry |
| `assets/character/sudburian_src/measure_neck_ceiling.py` | NEW. The ceiling measurement |
| `assets/character/sudburian_src/scarf_geom.py` | EDITED: flat `LOOP_Z_HI 1.570` → the measured `loop_z_hi(th)` profile. **This edit is worth keeping at T2** |
| `assets/character/sudburian_src/export_live_v13.py` | EDITED: `REPO` is now caller-overridable (it was hardcoded to `D:\seads_sandboxes\winter-gi` and would have exported into the wrong tree) |
| `assets/sled/indy650.glb` | **OVERWRITTEN** with the T0 costume spliced in (splice verified, read-back OK, machine bytes byte-identical). Revert with `git checkout assets/sled/indy650.glb`; a copy of the pre-splice file is at `%TEMP%\claude\D--flight-sim2\…\scratchpad\indy650.glb.bak` |
| **the live Blender session** (pid 26600, `indy650.blend`) | **HOLDS THE T0 COSTUME AND IS NOT SAVED.** The proxy is 5994 verts / 11470 polys / 6 material slots. Nothing is lost if it closes: the three scripts rebuild it deterministically |
| the gate | **NOT RUN.** Chad stopped the build. `test_rider_winding` has never seen these shells — expect it to pass (all 19 census green) but that is a prediction, not a result |

### The verified result of the T0 build (so nobody re-litigates it)

- ring top **1.6038**, 4.9 mm under the chin, ≥ 9.0 mm under the helmet at every azimuth
- hood **inside** the scarf at the front by 7–8 mm; **outside** it at the back by 10–83 mm, over the knot
- **0 of 908** scarf verts inside any garment shell or body shell, in BIND pose
- body coverage: every pelvis / spine_02 / thigh / calf / foot vertex contained. Still bare: head (helmet), neck (inside the ring), lowerarm (mitt zone), 7 upperarm cap verts and 4 chest-top corners — all grey-on-grey at overlapping seams

### ⚠ THE ONE FINDING THAT SURVIVES INTO T2

**In the SEATED POSE, 497 of 908 scarf verts are inside the BODY** — the
authored rest tails hang straight down while the posed torso leans 33° forward,
so in the `.blend` the tails are *buried*, not merely occluded. This is
**pre-existing and unrelated to the costume** (the body and the tails are both
untouched by this rung), and the GAME never draws that pose — `render/trail_chain`
drives those bones every frame off the measured back keep-out. But the 8h
answer on record ("nothing is buried, the tails are occluded by the torso") is
**wrong for the seated pose**, and now that the `.blend` is the source of truth
that discrepancy is worth a ruling from Chad. Re-measure with the posed-state
block at the end of §2 of `verify_costume_live.py`'s sibling check.

## 4. THE PLAN — T2, in rungs, each one Chad-judged

**The honest framing:** the ceiling is the BODY, not the clothes. `sudburian_proxy.py`
builds a man from 3 boxes and 12-gon prisms; ladder §R2 already specifies what
he should be (*"~14k tris LOD0, one material, 2048² albedo"*). T2 is that rung,
finally taken. Do not start it by writing geometry.

### C0 — THE ENGINE PREREQUISITE (blocking, do this first, it is small)
**MEASURED: this engine cannot show a texture today.** The shipping GLB has
`images: 0, textures: 0`, no primitive carries `TEXCOORD_0`, and
`render/sled_model.cpp` never samples one — the only `MATERIAL_MAP_DIFFUSE`
line is the beam's flat colour. So T2 needs, before any art:
1. ingest `TEXCOORD_0` in the GLB loader and hand it to the raylib mesh,
2. load `images`/`textures` out of the GLB (embedded, not side files — the
   asset ships as one .glb) and bind baseColor,
3. decide normal/roughness: raylib's default shader takes them, but this tree
   authors its own VS/FS (`render/` shader block) — **check what the fragment
   shader actually reads before promising a normal map.**
Gate it with a smoke shot of a checker-mapped test prim. **Red-team this rung
before it lands** — it is the first change to the material path since the
helmet, and the standing rule (`red-team-major-work`) applies.

### C1 — THE BODY (the rung that sets the ceiling)
Re-author the Sudburian as one continuous clothed figure at §3's measured
proportions: real shoulder/deltoid mass, chest taper, elbow and knee volumes,
ankle break. Keep the 44-bone rig and the bind frame EXACTLY — every measurement
in §2 above, the seat fit, the grip weld and the scarf anchors are all expressed
in it. Chad judges silhouette in grey, per §9 ("agents build the blockout, Chad
judges the form"), before a single seam is authored.

### C2 — THE GARMENTS AS GARMENTS
Hoodie and snowpants as authored panels: shoulder yoke, sleeve seams, a real
hem with a drawcord, cuff ribbing, knee articulation panels, the side zip as
geometry (it is already ruled to be the blue stripe's line), and a **hood with
an actual opening** — a shell with an inner and outer face and a rim, not the
swept ring T0 uses. The four topology rules from §1 still bind, and
`verify_costume_live.py` still measures them.

### C3 — SKINNING
Weighted, multi-bone, at elbow / knee / waist / shoulder. **Everything today is
rigid one-bone-per-vertex** — that is why the T0 torso reads as a rigid box in
the rear view and why a stripe cannot cross the elbow. This is the rung that
makes him move like cloth rather than armour. Keep the mitts rigid (ladder §2.2
ruled that, and it is correct for a fixed grip).

### C4 — UVs + TEXTURES
2048² albedo + roughness (+ normal if C0 delivered it): fabric weave, quilting
stitch lines, zip teeth, the Klim-ish panel logic from §4.1's references, honest
wear at the knees and cuffs. Colours stay derived, never retyped: grey from the
suit's own baseColorFactor, black from `indy_black_satin`, blue from
`kComplementBlue` parsed out of `render/team_color.h`.

### C5 — LOD + COST
§R2's budget is ~14k tris LOD0. Measure the draw cost the way R0's DRAW COST
section did before declaring it free.

## 5. TRAPS THIS SESSION PAID FOR (all of them recurrences)

1. **A superellipse does not contain the box it rounds.** Power-4 at (box + 8 mm)
   left all 8 corners of every torso box OUTSIDE the garment. Fixed with a
   corner-aware rounded rectangle and a stated containment budget
   (offset ≥ 0.37 · corner radius).
2. **Sampling that misses the feature it exists to reproduce.** The first
   rounded rect was perimeter-parameterised: with 20 samples on a 1.7 m
   perimeter the whole corner arc fell *between* samples and the polygon cut
   the corner off again. Same class as a check whose name outruns its reach.
3. **A measurement with a filter in it is a measurement OF the filter.** The
   ceiling script's first cut had a z floor at 1.60 and reported 1.6003 where
   the real helmet rim is 1.5822.
4. **Interpolating a ceiling upward between bins invents headroom** that the
   real ceiling does not have — it put the band 10.5 mm inside the helmet.
   A ceiling read off a coarse grid is only trustworthy as a *lower envelope*.
5. **A clearance that gets blended is not a clearance** — and the fix for it,
   applied ungated, promptly pushed the front collar OUTSIDE the scarf it was
   supposed to hide under. Both caught by re-measuring the applied mesh.
6. **The per-shell census earned its keep twice**: it caught 4 inverted shell
   families (domes and stripes) before a single triangle reached the mesh.
   Per shell, never a grand total (SCARF_SPEC §8k).
7. **A combined BVH over overlapping shells makes parity lie** — inside two
   shells is an even crossing count, i.e. "outside". Test containment PER SHELL.

## 6. IF CHAD WANTS THE T0 GONE

`git checkout assets/sled/indy650.glb`, and in the live session re-run
`apply_costume_live.py` after emptying `costume_geom.geometry()`'s piece list —
or simply delete the three costume material slots' faces. The `.blend` was never
saved, so a plain reload of `indy650.blend` also removes it. The `scarf_geom.py`
ring-profile edit should SURVIVE either way: it is his ruling, measured, and
independent of the costume.
