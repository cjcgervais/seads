# SEADS — World & Art Direction: the legible little world

*Written 2026-07-07 from a brainstorm with Chad. This is the WHY + the direction for
the world/art (the `sandbox/planet-art` worktree executes it). It is also the fix for
a real problem — read §1.*

---

## 1. The problem this solves (why it's not just decoration)

Flying to the **far side / southern hemisphere feels wrong** — disorienting enough to
make Chad a little motion-sick. The critical diagnosis (the `dogfight-systems`
attribution discipline): **the physics is position-invariant** — the plane flies
*identically* on the far side as at spawn (SPEC §6: no privileged point, no fixed
down axis). So this is **100% a PRESENTATION problem**, owned by the world render, not
the flight model. **Controls do not change and must not** — the fix is in what you see.

Two world-fixed anchors currently betray a global "up":
- **The world is real Earth** (`assets/earth_color_5400.jpg` + `earth_dem_2048.png`).
  Earth is the *worst possible* world for a small sphere: everyone has its orientation
  memorized, so the antipode always reads "upside-down continents." **No camera or
  lighting trick beats recognition** — the texture itself is the anchor.
- **A fixed world-space sun** (`render/draw.cpp:42`) rakes from one direction, so the
  far hemisphere is backlit / lit "from below."

`💡` The fix is **content/legibility, not camera.** Give the world recognizable,
known-size, radially-oriented features and every spot reads as *locally upright* —
because the human eye reads orientation, altitude, and speed from **familiar objects**,
which a smooth texture cannot provide. This is the "felt curvature / legibility" the
`docs/TEACHING.md` addendum flagged as the missing piece for the sphere premise to land.

## 2. The vision — a 1940s black-and-white little world, with vivid planes

- A whimsical **little world**: keep R = 15 km (the tactical curvature is the bet),
  but populate it with **big, exaggerated, familiar features** — 1940s small-town
  Canada. Redwoods, houses, a church steeple, a water tower, a stadium / baseball
  diamond, a town square. Known real-world silhouettes at ~2–5× scale, so they read
  from combat altitude and speed. Chad picks a real small town and "arts it up."
- The world is rendered in **1940s BLACK & WHITE** — filmic newsreel: high contrast,
  light grain / vignette, maybe a flicker. **Readability beats mood** where they
  fight: never crush the blacks so far you lose the ground/orientation reference.
- The **planes are the only saturated color**: shiny painted metal, specular
  highlights + a hard rim light so they *glint* — intimidating, fast, punchy. Each
  plane one color; **teams are complementary PAIRS** — orange+blue, purple+yellow,
  green+red — each squadron a matched complementary duo. You read *teams* by their
  pairing (like painted squadron liveries).

## 3. Why it works (the design logic)

- **Monochrome world + saturated planes = the planes are the only chroma.** The eye
  locks onto them instantly: friend/foe by team pair, position, orientation-by-motion.
  One art move solves **orientation + tactics + identity** at once (the Sin City /
  red-coat-in-Schindler's-List trick, pointed at a dogfight).
- **Known-size 3D features are the orientation instrument.** Trees standing radially
  "up", a town below, familiar sizes → the pilot always knows up / how high / how fast.
  This is what kills the far-side ambiguity *and* the motion sickness (a strong visual
  orientation reference resolves the vestibular mismatch).
- **Dropping Earth removes the global anchor.** A made-up world has no "correct up," so
  every location just reads as *a place, right-side up*.
- The planes should **fly as intimidating as they look** — the shiny-fast look is the
  visual half of the flight-feel mission ("fast, punchy"); the two reinforce each other.

## 4. Scope & constraints (respect the constitution)

- **PRESENTATION / CONTENT only.** Physics stays position-invariant (SPEC §6): no fixed
  down axis, no privileged point, nothing cached. Features are **radial props** — each
  oriented so its up = its own `normalize(position)`. That is the ONE legitimate
  per-feature "up": static content recomputed from position, never a global/cached axis.
- **Visual-first: no collision in v1** (you "fly around, over, and above"). This keeps
  the scope fence's *spirit* — a clean feel signal (SPEC §? scope: no terrain gameplay).
  Collision / consequence is a later, deliberate, logged ruling.
- **Speed × scale is one coupled dial.** Faster planes need bigger, better-spaced
  features to stay readable — tune them together, in the flight log.
- A rigid "spin the globe so you're always on top" re-base is a **visual no-op** for
  what you see relative to yourself — don't chase it; the lever is content + which cues
  are world-fixed vs player-fixed.

## 5. Build order (first cut → expand)

1. **Drop Earth** for an orientation-neutral base (the single biggest win — one asset
   swap; keep the DEM for gentle relief if wanted).
2. **B&W world shader + saturated-plane pass** (a material flag or a second shader:
   desaturate the world, render planes in full color).
3. **ONE hero town / arena** (the real small 1940s Canadian town Chad picks) + a few
   scattered giants. Prove felt-orientation + tactical fun in one place first.
4. **Plane liveries**: one color per plane, complementary team pairs, shiny-metal
   spec + rim light.
5. **Scatter features across the globe**; instancing + LOD + horizon culling for perf.

## 6. Where it lives & open questions

- Home: the `sandbox/planet-art` worktree (`D:/seads_sandboxes/planet-art`); assets in
  `assets/`. This doc is the direction; that effort executes.
- Open (Chad's calls): which town; how gritty vs clean the B&W; whether collision ever
  enters (a later ruling); how far to scatter features globally vs one dense arena.

*The kernel proves the plane obeys. This is how the pilot finally* sees *the sphere —
and stops feeling sick on the far side of it.*
