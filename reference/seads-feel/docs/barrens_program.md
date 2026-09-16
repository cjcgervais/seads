# The barrens layer — program + workflow

Worktree `D:\seads_sandboxes\barrens`, branch `sandbox/barrens-layer`, forked
from `sandbox/world-dem` @ 96a685489. **The live DEM repair in
`D:\seads_sandboxes\world-dem` is never touched by this branch.** When the
repaired DEM lands, this branch rebases onto it; the layer consumes `elev` as an
argument and holds no copy, so a DEM change flows through with no edit here.

## The workflow (how this gets built, repeatably)

Four roles, matching the tunnel `seal-pass` house pattern:

| role | who | does |
|---|---|---|
| research | Sonnet subagents, parallel, one narrow brief each | source discovery, literature, imagery mechanics — never code |
| verification | this session | every endpoint the agents claim is re-queried here before a line is written. Two of the four agents shipped at least one unverifiable claim; the endpoints in the spec are the ones I hit myself |
| design + build | this session (Opus) | the law, the module, the acceptance leg |
| red team | Opus consult, fresh context, review-only | the spec BEFORE implementation, then the diff |

Rule carried over from `seal-pass`: subagents never commit and never touch
`sim/`, `control/`, or goldens. Commits are by explicit path from this session.

## Rungs

- [x] **B0 research** — 4 parallel Sonnet briefs (gov catalogues / Landsat /
      literature / shatter cones + rendering). Findings folded into the spec.
- [x] **B1a source verified** — `Barren_Rings` + `Lime_Treatment_Site` re-queried
      live from this session. 4 + 813 polygons, EPSG:26917, no login.
- [x] **B1b the law** — `offline_tool/sudbury_barrens.py`, `zone * dose * topo`.
      Selftest green: 3 lobes / 19,491 ha / all inside `R_MAX`, all three
      smelters inside their own mapped lobe, 813 lime sites 1978-2025.
- [x] **B1c acceptance** — `offline_tool/accept_barrens.py`, 11 measured checks
      against the real 16 m DEM, all PASS. Previews:
      `preview_barrens.png`, `preview_barrens_overlay.png`.
- [x] **B1d red team** — Opus consult on the spec (sphere vs ground metric, the
      `rho_grid` overstatement, tree-density ordering, the projection lock, the
      albedo composite).
- [ ] **B2 bake integration** — wire into `sudbury_bake.build()`: albedo lerp,
      `dens *= (1 - barren)`, emit `assets/sudbury_barrens.png`, register in the
      manifest/lock, extend `sudbury_header.py`.
- [ ] **B3 runtime** — sample the channel in the terrain shader; the barrens are
      an albedo term first, no new draw calls.
- [ ] **B4 fly** — Chad flies Copper Cliff -> Coniston -> Falconbridge low.
- [ ] **B5 shatter cones** — gated on Chad's A/B/C ruling
      (`docs/shatter_cone_feasibility.md`). Triplanar detail-normal pass,
      RNM blend, `slope x barren x distance` gate. Mask already baked.

## What is deliberately NOT done

- **Landsat.** `LT05_L2SP_019028_19930901_02_T1` (1 Sep 1993, 1% cloud, WRS-2
  019/028, one scene covers the whole disk) is verified reachable anonymously
  via the Planetary Computer STAC. It is a **corroboration** leg, not a source:
  the City polygons are the authored 1970 boundary and an NDVI classification
  would be a noisier proxy for the same thing at a later date. Kept behind a
  `--landsat` flag; needs `pystac-client` + `planetary-computer`, which are not
  in the bake venv.
- **Re-pinning `remap_range.lock`.** Adding a channel must not move the DEM
  remap range. If a bake wants to re-pin, that is a separate decision.

## Sources + licence

- City of Greater Sudbury Regreening Program, `Barren_Rings` /
  `Lime_Treatment_Site` (ArcGIS Hub, CC-BY-SA) — **attribute the City**.
- Freedman & Hutchinson 1980, *Can. J. Bot.* 58 — the ridge/valley rule.
- Winterhalder 1996, *Environmental Reviews* 4(3) — regreening context.
- Landsat (if the corroboration leg is ever run) — US public domain.

## Known corrections to project memory

Memory records the barrens as "17,000 ha barren, 81,000 ha semi-barren". The
**measured** figures from the City's own 1970 polygons are **19,525 ha barren**
and **83,725 ha semi-barren** (19,491 / 83,576 ha as re-measured in our aeqd
frame). Published literature figures differ again by method (Winterhalder 1996
gives 10,000 / 36,000). Use the City numbers — they are the ones the layer is
built from and the ones the acceptance leg asserts.
