# S3 — Linework ribbons (roads · snowmobile trails) — plan + Fable ledger

Executes `docs/stereoscope_sudbury_plan.md` §S3 via the `/stereoscope-sudbury` skill.
Live status is `docs/stereoscope_handoff.md`; this file is the design + the Fable rounds.

## Goal (fly-gate)
Chad follows a road low from Chelmsford toward town; a **light-green snowmobile trail into the
bush**. Roads read as **draped geometry ribbons** (dark bed + light dashed centerline), NOT the
11.5 m rasterized albedo edge-lines they replace. Trails are the ONE sanctioned world-chroma accent.

## Scope (v1)
- **Roads** (already fetched — `sudbury_fetch.fetch_roads`, cached `source/roads_raw.json`): major
  (motorway/trunk/primary/secondary) + minor (tertiary/residential/unclassified).
- **Snowmobile trails** — NEW fetch `route=snowmobile` (best-effort: if Overpass is unreachable the
  bake logs a WARNING and emits roads-only; trails come when the network is healthy — a degraded,
  logged decision, never a silent skip).
- **Rivers = DEFERRED** (the machinery is kind-generic so `waterway` slots in later; the fly-gate is
  roads + trail, so v1 does not add the river fetch/taper). Logged in the handoff.
- **Remove the road edge-lines from the albedo bake** when this lands (no double roads) —
  `sudbury_bake.py` road_bed/road_edge paint removed; urban/industrial patches KEPT.

## Architecture — mirrors the WATER + S2 patterns (house law)
- **Projection math stays offline** (`offline_tool/sudbury_ribbon.py`); the runtime consumes pure
  baked sphere dirs + arc-length. Every dir goes through `frame.aeqd_to_dir` (single source, no fork).
- **Bake:** `sudbury_ribbon.build_ribbon(frame, polyline_lonlat, half_width_m, ...)` → per-vertex
  (unit dir, arc-length `s`, transverse `v`) + local triangle indices. `sudbury_header.emit` bins
  ribbons per-KIND into ≤65000-vertex batches (raylib ushort meshes) → `GisRibbonVertex[]` /
  `kSudburyRibbonIndices[]` / `GisRibbonPath[]` in `render/sudbury_gis.gen.h`, embedding the same
  projection-lock hash (gated by the existing asset-validator lock leg — projection UNCHANGED so the
  hash stays `0x99061E1583D34607`).
- **Runtime:** `render/ribbons.{h,cpp}` builds one mesh per batch, each vertex DRAPED at
  `dir * (g_planet.height.radius_at(dir) + lift)` (the SAME height field the terrain mesh + trees use
  — anti-float), texcoords = `(s, v)`. Custom `kRibbonVS/FS`: eye-relative `uEye` rebase (like props);
  FS draws the casing bed + a proper duty-cycle dashed centerline from `(s, v)`; trail kind emits
  light-green (passes the S1 saturation-gated post as the sanctioned chroma). `draw_ribbons` runs
  right after `draw_water`, before `draw_trees`. `SEADS_NO_RIBBONS=1` = A/B. `[ribbons]` dials in
  `config/world.toml` (no bare look-constants in GLSL — house law).

## Fable-BEFORE (2026-07-11) — verdict SOUND-WITH-FIXES; every P0/P1 folded
- **P0-1 hairpin NaN** — when `dot(t_in,t_out) < -0.99` (turn > ~172°), `normalize(n_in+n_out)` dies
  BEFORE the k-clamp. Guard: `m = n_in; k = MITER_LIMIT`. Also dedupe zero-length segments before
  `normalize`. → FOLDED in `sudbury_ribbon._miter`.
- **P1-2 lift/z-fight** — 1 m is insufficient (a near-tangent draped strip is worse than the water,
  which needed 2 m + slope offset; also ribbon-vs-terrain interp mismatch alone can bury >1 m between
  stations). FOLD: **lift = 2 m**, **`glPolygonOffset(-2,-4)`** (stronger `factor` than water's -1 for
  grazing incidence), **station spacing ≤ terrain grid** (densify ≤ 80 m < ~100 m mesh), **L and R
  edges draped independently** through the identical height field (a 24 m ribbon on a 30% side-slope
  differs ~7 m edge-to-edge). Cap lift ≤ 3 m (roads hover at ridge crests past that).
- **P1-3 dash duty cycle** — `step(0.5, fract(...))` hard-codes 50/50. Correct:
  `lit = fract(s/(dash+gap)) < dash/(dash+gap)`. → FOLDED in `kRibbonFS`.
- **P1-1 k_t width note** — plane-then-project ACCEPTED (everything on the map squeezes by the same
  k_t; a 1/k_t-compensated road would read too wide vs its surroundings). No compensation. Roads live
  at ρ<20 km (k_t>0.73; ≤7% shrink inside 10 km). Documented, not "fixed".
- **P2-1/P2-2 arc-length** — use the well-conditioned CHORD form `θ = 2·asin(½‖c_i−c_{i-1}‖)` in
  double (acos ill-conditions in float32 near dot≈1). Claim verified: great-circle `s` keeps dashes
  uniform world-metres. → FOLDED.
- **P2-3 miter clamp** — `MITER_LIMIT = 2.0` (turn 120°); also clamp the offset length to
  `0.5·min(adjacent segment lengths)` to kill inner-edge fold-back on sub-50 m hairpin segments. →
  FOLDED.
- **Winding CONFIRMED outward, do NOT swap:** tri order `(L_i, R_i, L_{i+1})` + `(R_i, R_{i+1},
  L_{i+1})` with left = +90° normal `(-ty,tx)` is CCW-from-outside; the aeqd Jacobian sinθ/θ>0 over the
  whole disk (θ_max 160.4°<π) makes it outward everywhere. The validator still runs the
  `dot(cross(e1,e2), centroid)>0` check per triangle (defense).

## Gate stack (per the skill, in order)
Fable-BEFORE ✓ → run bake (16 m, `SEADS_SOURCE_RES=16 SEADS_CDEM_ONLY=1`) → rm test exe + build →
asset-validator (NEW ribbon leg: unit dirs / LOCAL index range / outward winding / arc-length
monotone-per-path + the lock-hash leg) → shaders GLSL330 → seam grep → smoke shot at the pinned
Chelmsford viewpoint (read back) → full ctest goldens-0 → Fable-AFTER → evidence block → atomic commit
(explicit paths) + tag `world-s3-ribbons` → Chad flies.
