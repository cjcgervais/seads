# SF2-BANKS SPEC — OREO SNOWBANKS (visible plowed banks)

Law: `WINTER_LAW.md §3.6c` (RULED Chad 2026-08-12, from the first street drive: the banks are
force-only invisible — BLOCK-VP1). Chad, verbatim in substance: *put gravel in the snowbanks —
that's what snowbanks are, mixed with sand, and the plows build the banks and it looks like oreo
ice cream.* Rung SF2-BANKS, branch `sandbox/winter-SF2` from SF1 tip `d2ad2ed28`.

THE PATTERN (SF1 generalized): ONE function, two consumers, zero fork. The bank's driven shape is
already `SnowpackField::depth_at` (feather + `bank_profile()` + junction gaps, composed in
`depth_base_at`). The drawn bank is a ribbon-sibling strip whose every vertex radial is
**`facet_radius_at(dir) + snow.depth_at(dir)`** — the rendered terrain facet (the ribbons' R4d
anti-float discipline) plus THE REAL FIELD, sampled, never re-derived. The mesh cannot fork from
the physics because it never re-implements it. This also closes BLOCK-VP1 LOCALLY: a band of the
true driven snow surface becomes visible beside every plowed road (you SEE the snow you felt).
BLOCK-VP1 itself stays open and Chad's for the global field.

## 1. Geometry — `render/bank_mesh.h/.cpp` (seads_render_core, raylib-free, testable)

- Injectable core (the test seam): `build_bank_strips_for_path(centerline dirs + per-station
  half_w + arc s, const HeightField&, subdiv, tiles, const world::SnowpackField&, params)` →
  CPU arrays (`pos` world-absolute floats, `uv`, `u16 idx`). Public
  `build_bank_strips(hf, subdiv, tiles, snow, params)` iterates the baked
  `kSudburyRibbonPaths`, **kinds 0/1 (plowed roads) ONLY** — trails are groomed, not plowed
  (§2.4c), rivers are water. Centerline + half-width recovered from the drawn L/R vertex pairs
  (INV-6, same recovery as `world/linework`).
- ★RED-TEAM FOLDS (F1-F6, all pre-build). (F1, P0) The three road paths are CONCATENATED
  BATCHES of thousands of OSM runs; a run boundary is arc-length `s` NON-INCREASING (the same
  rule world/linework's add_path splits on). Strips BREAK there — without it the generator
  slings a bank ridge across town at every join. (F2) Cross-section rings = the sorted union of
  the BANK knots and the FEATHER knots, derived from config: with rise 3 / corridor_edge 4 /
  fall 6 that is `e = {0, 1.5, 3, 3.5, 4, 6.5, 9}` — the true crest of the composed
  feather+bank curve sits near e = 3.5, NOT at rise, and a rise-only ring set clips 0.16 m off
  the crest. (F3) The skirt ring is a BURIAL: radial = bare facet − 0.5 m with NO lift, over
  `skirt_m = 6` (a 3 m skirt ending at ground level is a floating 0.85 m terrace edge, not the
  hill's trick). (F4) Stations inside a T24 CutDisk are SKIPPED (ribbon precedent — otherwise
  bank ridges bridge the tunnel-mouth excavations the ribbons correctly vacate). (F5) Default
  `station_m = 16` and the build LOGS milliseconds + vertices + mesh count (measured budget:
  ~2.9 M verts / ~2.4 M depth_at calls at 16 m; 8 m doubles it into 10-25 s of startup).
  (F6) u16 chunking is the MAIN PATH, not a contingency: split at station boundaries every
  ~8000 stations, DUPLICATE the boundary station into both chunks (else a crack), one mesh per
  side x chunk.
- TWO strips per run (left/right), inner ring (e = 0) at the ribbon edge so the strip meets the
  drawn road. Per-STATION width recovery (linework's rule — never the ribbons' per-path median
  corduroy shortcut; red-team F9).
- Junction gaps come FREE: `depth_at` already fades the bank at junctions (`bank_gap_m`), and we
  sample it — the drawn gap and the driven gap are the same number by construction. NOTE for the
  drive checklist (F10): every OSM run endpoint is a junction, so real streets show periodic
  bank NOTCHES at way-splits — drawn == driven, correct by law, pre-named so it does not read
  as a bug.

## 2. Material — the OREO (small dedicated shader, ribbons-family)

Unlit like the ribbons (design decision inherited). Base = snowbank white, slightly darker/bluer
than wild pack (the §S1 trail lesson: white-on-white is illegible; banks read by value + TEXTURE).
The gravel: a deterministic hash speckle in the FS (world-pos/uv-hashed, analytic-fade like the
corduroy's fwidth discipline so it cannot moire at altitude), density and darkness as named
config dials; plus a `crest_smudge` band — the plow's dirtiest throw is the crest line, so the
speckle densifies toward `v ≈ crest` (that IS the oreo read: white cream, dark crumb). Nothing
here is snow-physics: it is paint on the bank strip only.

## 3. Wiring

- `[bank_mesh]` in world.toml: `enabled`, `station_m`, `skirt_m`, `speckle_density`,
  `speckle_dark`, `crest_smudge`. Loaded strict (`require()`) + rails. ★NO `lift_m` dial —
  the lift is SINGLE-SOURCED from `[ribbons] lift_m` (red-team F8: an independent lift leaves
  the bank edge stepped against the asphalt; the parallel-constant pattern INV-6 kills).
- App: banks build beside the ribbon surfaces (draw.cpp), AFTER the snowpack sources bind; the
  snow field pointer arrives via a `set_bank_snow_sources(&snow_field)` setter from main.cpp
  (the `set_tree_snowhill` pattern). Draw after ribbons, before props — with the IDENTICAL
  eye-relative model matrix + glPolygonOffset + cull-off block the ribbon pass uses (red-team
  F7: world-absolute floats without the rebase jitter at speed; without the offset the buried
  skirt z-fights).
- The SF1 snowhill footprint: `depth_at` includes the hill, so a road inside r_cut would grow a
  bank up the hill — today no road is within r_cut (~130 m clear), and the hill-wins class rule
  covers physics; geometry-wise accept what `depth_at` says (single source beats a special case).

## 4. Gate legs — `test/unit/test_bank_mesh.cpp` (ASCII, headless, render_core)

1. **Conformance (the anti-fork law)**: synthetic straight plowed road on a flat synthetic
   field; every generated vertex's radial == `facet_radius_at + depth_at(dir)` exactly (skirt
   ring excepted, == bare facet). Kills any future re-derivation of the profile.
2. **The crest is the bank**: on a synthetic straight road LONGER than 2x bank_gap_m + margin
   (endpoints are junctions — F10), the MAX ring height at a mid-strip station reaches
   `ambient + bank_height_m` (−15 cm tolerance, the chord loss) above the drawn terrain, while
   the inner ring reads the road's plowed depth. Rings must include the feather-complete knot
   (F2) or this fails honestly.
3. **Junction gap**: with a junction in the synthetic net, the crest amplitude at the junction
   station fades below 10% of `bank_height_m` (the drawn gap is the driven gap).
4. **Plowed roads only**: a trail-kind path generates ZERO strips (groomed trails have no
   plow bank — §2.4c), rivers likewise.
4b. **Run breaks (the F1 P0 leg)**: a synthetic path holding TWO disjoint runs (s resets)
   yields two disjoint strips — no geometry between the end of one and the start of the next.
4c. **Cut clip**: a station inside a synthetic CutDisk generates no ring (the ribbon T24
   mirror).
5. **INV-7**: `sim/` untouched; flight goldens bit-unmoved (structural — banks read the field,
   they never write anything).
6. Full ctest counts; `winter_plan_query.py check`; `graphify --stale`; ASCII grep; explicit-path
   commit; §3.6c prose already folded (this spec + JSON status in the same commit).

## 5. Out of scope (named so nobody smuggles)

- No global render-vs-drive fix (BLOCK-VP1 is Chad's).
- No physics change of any kind — the banks were already driven; this rung only draws them.
- No trail-side banks (groomed ≠ plowed); no bank collision changes (already emergent via depth).
- Sparks/albedo canon untouched (slag orange stays the one chroma; the speckle is neutral
  dark gravel greys, not a new color family).
