# BUBBLE LOCATION RUNG — scout (2026-07-22 overnight; read-only recon, nothing built)

Chad's queue word (round 16): "then we can move on to the bubble location." This is the
recon so the rung starts moving, not guessing. Canon: MASTER_PLAN §2.5 (the 2026-07-21
re-redefined map) + docs/tunnel_handoff.md "THE BUBBLE LOCATION RUNG".

## What exists on `sandbox/fields-forge` (read @ cc61dbe8a)

- `sim/fields.h::AtmosphereField` — deck (deck_agl_m/deck_soft_m, go-anywhere floor) +
  `std::vector<Bubble>` domes: {center_dir (unit), ground_radius_m (great-circle),
  ceiling_m, edge_soft_m, ceil_soft_m}. Sampler = `sim::atm_frac_at` (sim/aero.h), spatial
  fraction MULTIPLIES the vertical taper (bubbles only REMOVE air). Pure data; read iff
  `Environment.atm != nullptr`.
- `config/game.toml [atmosphere]` — `enabled=false` startup (B key toggles live), ONE test
  bubble via scalars (`bubble_radius_m` 6000 / `bubble_ceiling_m` 4000 / `bubble_edge_soft_m`
  1200 / `bubble_ceil_soft_m` 600), centered ON THE SPAWN point (code-side). The struct
  already supports many bubbles; the CONFIG + loader do not yet.

## The rung's shape (one fly)

1. **The merge first** (the Coordination note's stated direction): `sandbox/tunnel-forge`
   merges INTO `sandbox/fields-forge` — the tunnel is additive (new modules + env field +
   render); Chad's round-15 through-flight ruling satisfied the "on Chad's greybox fly
   ruling" gate. Expect trivial resolution but treat it as a deliberate rung with its own
   gate run, not housekeeping (both branches carry independent app/main.cpp edits).
2. **Config**: `[[atmosphere.bubble]]` array-of-tables (center as a unit dir or the town
   name resolved at load; radius/ceiling/softs per bubble), replacing the spawn-centered
   test scalars. Loader + a rejects-malformed leg (the load_world celestial pattern).
3. **The two campaign bubbles** (canon §2.5):
   - CHELMSFORD COALITION (west): covers Errington + Chelmsford + Dowling +
     Levack/Onaping Falls; its proximal EAST edge sits AT the Errington Mine —
     constraint: `arc(center, world::kTunnelMouthErrington) == ground_radius_m` (the mouth
     ON the edge). Center/radius solve from that constraint + covering the named towns.
   - SUDBURY FACTION (east): covers Azilda + Sudbury City; proximally bounds MURRAY Mine
     (just east of Azilda): `arc(center, world::kTunnelMouthMurray) == ground_radius_m`.
   - The thin-air GAP between the two bubbles along the tunnel's surface line IS the
     thing the tunnel bypasses — pin it: a sampler sweep along the great-circle
     Errington->Murray shows atm_frac dipping to the deck-only floor between the edges.
   - Town center dirs: resolve at implementation from the GIS bake (label/building-batch
     anchors in render/sudbury_gis.gen.h) or MASTER_PLAN coords — do NOT hand-type
     lat/lons that fork the bake.
4. **Tests**: mouth-on-edge pins (arc distance within edge_soft of the radius), the gap
   sweep, loader rejects, and the existing R6 suite untouched (the single-test-bubble arm
   should remain constructible for the R6 goldens/tests — keep the scalar path or port
   its tests to the array form deliberately).
5. **Fly**: bubble edges are FELT surfaces — Chad flies the Errington edge out of
   Chelmsford air and the Murray edge out of Azilda air; edge_soft is his stick dial.

## Hazards named now

- The merge is the cross-agent reconciliation the memory ledger flags — do it as its own
  commit with explicit-path review of app/main.cpp and config/game.toml collisions;
  NEVER `git add -A` (parallel agents share the repo class).
- Escape-sky transition zone (choking engine at E=0) is R7 — do NOT fold it in here.
- `enabled=false` stays the startup default; activation is Chad's.
