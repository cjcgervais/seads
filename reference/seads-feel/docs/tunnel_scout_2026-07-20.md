# TUNNEL SCOUT PASS — Fable, 2026-07-20 (pre-token-reset)

**Chad's fly symptoms (round 10):** (1) blind spots entering AND exiting the
central chamber — flyable on faith, but the view is opaque both ways; (2) cannot
fly into or out of Murray mine from that side.

**Verdict: both symptoms are one bug.** T13-A1 shrank the arena
(`arena_a_m` 7350 → 4200) but the breach locator was never updated — every
consumer of `breach_axes()` (`render/tunnel_mesh.cpp`, committed ~line 953)
still finds "the spine node nearest `arena_apex_r` BY RADIUS" (a depth rule)
instead of where the bore actually pierces the shrunk ellipsoid. Collision
(`world/tunnel_net.cpp` SDF) is CORRECT — that is why faith-flying works.

## Probe numbers (bare-sphere net, live config dials; probe compiled against
## the real `world/tunnel_net.cpp` — arcs match the T13-A4 real-DEM legs ±20 m)

| Breach | Hole cut at (arc) | Bore actually crosses wall | Offset | Hole radius |
|---|---|---|---|---|
| Errington side | 3267 m | 3963 m | **748 m** | 275 m — NO overlap |
| Murray side | 8761 m | 7928 m | **897 m** | 275 m — NO overlap |

Consequences, all from the same stale rule:
- Both REAL openings are un-cut cavern wall = **opaque** (the blind spots).
- The misplaced holes ARE cut — ~800 m away, into solid rock — and the
  **breach beacon rings encircle the FAKE holes** (lethal to fly into; SDF is
  rock there). The real openings are dark. This is why Murray reads as
  impassable: from inside the chamber the Murray exit is an unmarked patch of
  dark wall, and the lit ring is 900 m away around a death-hole.
- The tube-ring suppression uses the same radius rule (`|c| < apex_r −
  tube_height`): rings suppressed over arc **3550..8338** while the arena spans
  **3963..7928** → **~410 m of wall-less tube on EACH side** inside solid rock,
  right before each opaque wall.
- Murray collision is CLEAN: all 83 spine centerline nodes inside the net,
  bowl-entry columns (axis + 300 m off-axis) inside, T8 sightline 1500 m (cap)
  everywhere sampled. No physical blocker.

## Worktree state at scout time
- Flown exe = built 2026-07-19 19:29 = committed T13 (`f61727e81`).
- `docs/tunnel_visual_audit_report.md` (untracked) claims the mesh is clean —
  **contradicted by the fly + these numbers; do not trust it.**
- `render/tunnel_mesh.cpp` carries an UNCOMMITTED half-finished fix
  (2026-07-20 15:15) going the RIGHT direction (true ellipsoid-intersection
  breach positions, per-position local hole angle) but **it does not compile**:
  `build_arena` was re-signatured to `(e, breach_positions, hole_arc, p)` while
  the call site (~line 1103) still passes the old 5-arg list, and the
  beacon-ring/ember section (~line 1301) still calls the old `breach_axes()`.
  Never built, never flown.

## Fix plan (next session)
1. Finish the started refactor: SINGLE-SOURCE the true breach positions
   (currently duplicated in `breach_positions()` AND inline in `build_tube` —
   H1 fork risk), fix the `build_arena` call site, and route **beacons +
   ember-skip + suppression** through the same positions.
2. Hole size: the bore meets the wall obliquely, high on the dome — the true
   opening is elongated; the 275 m round hole may still clip it. Compute the
   actual intersection extent (or pad along the bore tangent).
3. Rebuild (the flown exe predates ALL of this), re-shoot the four smoke
   presets (mouth/bore/cavern core/cavern breach), re-run the gate — the
   manifold verifiers key on "designated openings" and need the new breach
   definition. Remember the Catch2 ASCII-name trap.

## CHAD RULINGS FOLDED 2026-07-21 (do these IN the fix session)
- **Campaign bubbles (canon in MASTER_PLAN §2.5):** Azilda SWITCHED to the Sudbury
  faction and is its PROXIMAL town — Murray Mine is just east of Azilda, between
  Azilda and Sudbury. Chelmsford coalition (Errington + Chelmsford + Dowling +
  Levack/Onaping Falls) bubble's proximal EAST edge at Errington Mine; Sudbury
  faction (Azilda + Sudbury City: downtown, Science North, Long Lake) bubble
  proximally bounds Murray Mine. Each tunnel mouth ON its bubble's proximal edge.
- **Both entrances currently read as "black ant mounds" — art rulings:**
  - **MURRAY = a proper OPEN-PIT mine look** (Blender-shaped): the real pit is on
    the side of the highway, normally water-filled — here a dry open pit that
    actually reads as one from the air, THEN the tunnel at the pit floor.
  - **ERRINGTON: DELETE the T13 sinking headframe** (Chad does not want that
    structure). Replace with a **proper tunnel PORTAL FRAME (framed adit) and then
    a tunnel** — and NOT all black.
