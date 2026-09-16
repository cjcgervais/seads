# THE ERRINGTON TUNNEL — staging brief (Phase 3 "clearing the tunnel")

**Branch `sandbox/tunnel-forge` (off fields-forge 458b8ce65), worktree
`D:\flight_sim2\seads-tunnel`. Driver: the Fable session that drove v4, v4-style —
subagent coders to Chad's felt spec, one spec-fidelity red-team per rung, instrument
acceptance = definition of done. Merges into `sandbox/fields-forge` ONLY on Chad's fly
ruling. `main` is the kernel line — the tunnel never targets it directly.**

Canon: `D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md` (§2.5, §2·GL, §3) — where this
brief conflicts with it, MASTER_PLAN's game-loop section wins. Chad's rulings in this
brief (2026-07-17) are canon-additions.

## Chad's staging rulings (2026-07-17, verbatim intent)

1. **Driver:** this session, v4-style (subagents + spec red-team).
2. **Milestone 1 = flyable greybox end-to-end, WITH the pump architecture roomed-in:**
   "Make the black stope a **combat egg shape** with **separate chambers on either side**
   that house **each faction's underground pump**. Also the **gaslamps**. I chose option
   one [greybox] but want to make sure you make room for the 2 opposing team pumps as
   part of the game loop logic."
3. **Reference picture:** not yet — do NOT block on it. Hero dressing (Errington ruins +
   stope dressing via blender-hero-forge) is its own later round; when Chad drops the
   picture it goes in `assets/reference/` and the hero round points at it.
4. **Player faction: the Chelmsford coalition** (Azilda/Chelmsford Central/Dowling/
   Onaping/Levack). **Errington Mine is HOME; the dive into Murray pit is the RAID.**
   (Coalition bubble topology — one large vs linked cluster — stays OPEN for the bubble
   round.)

## The geometry (canon + rulings, consolidated)

- Mouths: **Errington Mine #3** 46.54611, -81.22083 (home, SW of Chelmsford) ↔
  **Murray pit** 46.5144, -81.0657 (the enemy's egg open-pit — the raid dive).
- **Depth 2000 m** below terrain at the deepest (≈ Creighton). Tube legs ~1 km-scale
  from each mouth down to the chamber.
- **THE BLACK STOPE: one chamber, a COMBAT EGG** — 600–800 m across the long axis (the
  Fable Bf-109 turn-radius scale ruling: authentically *styled*, gameplay-*sized*),
  egg-oriented so the fat end/thin end read in flight. Off **either side/end: two
  SEPARATE smaller side-chambers**, one per faction, each housing that faction's
  **underground siphon pump** (multiple passes to kill — sustained exposure = time +
  risk, per §2·GL). The side-chambers are defensible pockets off the main egg, not
  corners of it.
- **Gaslamps**: period mining gas lamps light the tube and stope (the world-sudbury
  street-lamp additive point-glow tech is the proven pattern — constant-screen-px
  glows; render-only).
- **Tunnel atmosphere: ALWAYS full density** (a contained mine) — the strategic back
  door that works regardless of the bubble war.

## The architecture (already ruled in MASTER_PLAN — do not re-litigate)

- **The cubesphere heightfield cannot hole** (one radius per direction) → **portal
  surgery at mesh build** (cut the mouth openings in the visual terrain mesh) + a
  **swept-tube tunnel with its own spline-space collision**. Physics never touched the
  visual mesh anyway.
- **The `Environment` tunnel net** is the designed home for tunnel volume/air/collision
  queries (kernel v4's env threading exists for exactly this). Null tunnel net ⇒
  bit-identical to today — the same structural-off discipline as GravityField/
  AtmosphereField (R5/R6).
- **Sim never includes render headers.** The tunnel's volume/spline data is a neutral
  module both sides consume.

## Seam audit (what exists vs what's missing)

| Seam | State |
|---|---|
| `Environment` threaded through `sim::step` + `control::step` | ✅ (R3/R6 + v4 merge) |
| Spatial air (`atm_frac_at(position, env)`) | ✅ R6 — but shapes are deck + bubble DOMES only; the tunnel needs a TUNNEL term (full density inside the net, overriding the deck/depth) |
| Capture/cascade reads local density | ✅ (v4×R6 merge — the pool ball plans against local air; inside the tunnel = full authority) |
| Terrain crash predicate | ⚠ THE seam: `altitude <= 0 = crash` / solid-ground logic must YIELD inside the tunnel volume — flying 2000 m below terrain is the whole point. The tunnel net owns inside/outside; collision inside is spline-space (tube walls + chamber shell), not the sphere. |
| Visual terrain mesh | ⚠ needs portal surgery at the two mouths |
| Tube/egg/chamber meshes | ✖ new (greybox: swept tube + egg + two side-chambers, ellipsoid/capsule-union collision rep) |
| Gaslamps | pattern exists (world-sudbury lamps); new placement along the spline |
| Pumps | ✖ new entities; milestone 1 rooms them in (side-chambers + a pump placeholder each + the game-loop hook points), full kill-loop wiring is a later rung |

## Rung ladder (one rung per fly, Chad's stick judges; each rung gated + red-teamed)

- **T1 — the tunnel net (pure/sim seam):** spline path Errington↔Murray via the chamber
  (depth profile to −2000 m), a signed inside/outside query (tube radius along the
  spline + the egg + side-chamber volumes), full-density override into `atm_frac_at`,
  and the crash-predicate yield inside the net. Null ⇒ bit-identical (the R5/R6
  discipline); ATs for the seam + the density override + the yield.
- **T2 — portal surgery + greybox meshes (render):** mouth openings cut into the
  terrain mesh; swept-tube + combat-egg + side-chamber greybox geometry; interior
  depth/fog so the tube reads. Render-only, firewall intact.
- **T3 — spline-space collision:** tube walls + chamber shell kill you honestly
  (crash → respawn per the existing rule); the greybox becomes consequence-real.
  **After T3 Chad flies the first end-to-end: dive Murray pit → tube → the egg →
  out Errington.**
- **T4 — gaslamps + read pass:** lamp glows along the spline + chamber rim; whatever
  the T3 fly said needs to read better.
- **T5 — pump room-in:** the two side-chamber pump placeholders + the game-loop hook
  points (pump hp / multi-pass exposure seam per §2·GL), wired far enough that the
  kill-loop rung (world agent's cadence or a later round here) can land on it without
  reshaping the geometry.
- **LATER (blocked on Chad's reference picture + Blender bridge):** the hero round —
  Errington Mine ruins (buildings + conveyors) + Black Stope dressing via
  `/blender-hero-forge`.

## Rung status (the honest ledger)

- **T1 LANDED 2026-07-17** (7b8798af6 + fold c75d920e3, gate **667/667**, goldens
  unmoved). The net: densified slerp spine Errington↔Murray (mouths AT the local
  surface, −2000 m below LOCAL terrain mid — depth ramps over 0.3 of the path),
  the asymmetric combat egg at the midpoint (two-half oriented ellipsoid,
  700 m long axis: fat 400 toward home / thin 300 toward Murray, 280 lat /
  240 vert semis), two side-chamber pockets on opposite sides+ends (one per
  faction, 560 m lateral / 200 m long offsets) with connector tubes; min-union
  SDF (sign-exact everywhere, metric-accurate near the surface — T3 gets the
  sharper wall metric). Full-air = a UNION TERM in `atm_frac_at` (exact 1.0
  restored inside, exact no-op beyond `soft_m`; null-atm arm already exactly 1
  at depth — "always" holds in both arms). Crash yield in BOTH branches
  (sim ground_contact gate + the app-tick bare-sphere branch). Opus-coded,
  Opus red-teamed (SOUND-WITH-FIXES): **P0-1 folded** — `on_ground` latch
  (entering the net grounded, e.g. taxiing over the Murray pit rim, latched
  the grounded regime forever; now liftoff-cleared with the strike transients)
  + grounded-entry/near-boundary-firewall/chamber-distinctness legs. 7 mutants
  killed (m1–m7). Baked mouth dirs: `world/tunnel_geo.h` (pyproj aeqd, exact
  pipeline match). Config `[tunnel]` ships enabled=true on this branch;
  enabled=false = the kill-switch (bit-identical).
  **Words-level note for Chad:** the two chambers sit on opposite sides AND
  opposite ends (diagonal) — grade "either side" on the T3 fly; a placement
  re-rule is two config dials.
- **T2 LANDED 2026-07-18** (c48b2b077 + fold 9cb416d2f, gate **678/678**, goldens
  unmoved, render-only, firewall intact). Portal surgery: optional CutDisk list in
  the PURE `fill_face` (empty ⇒ bit-identical; any-vert-inside drop, cut radius
  2·tube_radius at each mouth). Greybox: `render/tunnel_mesh.*` (pure,
  seads_render_core) generates tube rings (parallel-transported, no twist) + the
  two-half egg + chambers + connectors + mouth collars FROM `world::TunnelNet` —
  **the single-source pin is the rung's law: every wall vertex proven on/inside
  the T1 SDF** (visual == query, so T3's collision kills at the visible wall).
  App side `render/tunnel.*`: mono depth-cued two-sided shader, drawn after slag.
  Opus red-team (SOUND-WITH-FIXES): **P1 folded** — the collar/hole gap (any-vert
  cut leaves a jagged edge up to a cell-diagonal past the cut radius; probed
  54–70 m of see-through void) → collar reach now DERIVED from the terrain grid
  (`collar_reach(cut, subdiv, tiles, R)` ≈ 326 m, config-relative) + 1.5 m
  sub-terrain tuck (z-fight) + a 32-azimuth-bin no-gap regression leg; **P2
  folded** — pocket walls pinned TWO-SIDED against the SDF (≥70 % |sd|<2 m,
  collapsed-wall mutant killed). 6 mutants killed this rung (m1–m4 + m8/m9).
  Queued notes: cut metric uses base R not radius_at (~3 m at 140 m, P3-1);
  `[ground] enabled=false + [tunnel] enabled=true` mismatches net-at-R vs
  collar-on-relief (unshipped combo, no guard, P3-2). **FLY-CARD NOTE: the deep
  tube/egg is near-black by design until T4 gaslamps** — the T3 card asks Chad
  whether the walls read in motion or it's "flying in void".
- **T3 LANDED 2026-07-18** (2885bf28f + fold 0d0ffbe84, gate **684/684**, goldens
  unmoved). The probe-first evidence: the suspected TELEPORT-LANDING hole was REAL —
  a wings-level wall graze 2 km down hit ground_contact's landing acceptance and
  snapped the plane +1999.5 m up to the surface, alive and grounded. Fix: the
  DEEP-PENETRATION clause in `ground_contact` (`r_s − r > deep_penetration_m` ⇒
  crashed, before any acceptance; `[ground] deep_penetration_m = 50`, default 0 =
  off = bit-identical; range-checked unreachable by legitimate landings). Wall
  grazes at egg/chamber/tube now crash AT the wall (±1 m sweep pinned), legitimate
  landings pinned untouched. Red-team round 2 (SOUND-WITH-FIXES) confirmed the
  foreman's MOUTH suspicion but the foreman OVERRULED the funnel-SDF fix after
  re-deriving the geometry: the collar is a flat surface-level SKIRT (not a deep
  funnel), landing on it is honest ~1 m contact — the real bug was the READ (the
  skirt drew through the near-black tunnel shader ⇒ the whole ~326 m mouth read as
  one giant hole when only the 140 m-diameter tube is real). Folded: collar pieces
  tagged + drawn at kSkirtVal 0.45 (solid earth vs 0.11 walls), mouth-aperture pins
  (60 m in / 100 m out), skirt inner-ring |sd|<2 pin, clause comment softened.
  Mutants m10–m14 killed (7 T1 + 6 T2 + 5 T3 = 18 total this program). NAMED
  RESIDUAL (err-open ruling: never die on visible open air): the Murray-side skirt
  outer edge overlaps the open tube volume (sd ≈ −62 at those verts) — a visible
  sliver you can pass through from inside; forgivable direction, on the fly card.
  **THE LESSON OF THE RUNG: for a death surface, BRIGHTNESS is part of collision
  honesty — a near-black solid surface at a mouth reads as VOID, and the fix for a
  visual-vs-physics mismatch may be the READ, not the SDF.**
- **FLY 1 VERDICTS (Chad, 2026-07-18):** "tunnel needs lights its hard to fly
  blind lol" + "I needed to approach from the right angle" (the mouth) + the
  re-rulings: egg 1800 m tall / fat DOWN ("so that an energy fight is possible")
  / way bigger / tunnels connect the BOTTOM / the energy fight has a LIT chamber
  / Murray = "an open egg... a bowl mesh that has a nice big opening to fly
  into" / spawn over Errington.
- **SPAWN LANDED 2026-07-18** (c6255c2a4): born over Errington at 2500 m, nose
  toward Murray; crash/death respawns come home (LoopState spawn spec, tick
  env-free; defaults = the legacy +X spawn BIT-identically, pinned).
- **T4a LANDED 2026-07-18** (72dd25d92): the VERTICAL Black Stope — 1800 m tall
  (thin 800 up / FAT 1000 down, horiz semi 600), egg top under an exact 1200 m
  of rock (relief-independent — u_long = local up makes the cover depth_m −
  egg_up_m by construction), spine junction at depth 2850 INSIDE the fat bottom
  (the "tunnels connect the bottom" pin); chambers ±850 m purely lateral (the
  diagonal question is DEAD — the long axis is vertical now); the MURRAY BOWL:
  450 m opening tapering to the 70 m tube at 300 m depth, terrain cut 450, rim
  reach 637 ≥ the 617 jagged edge (no see-through), spine ends at the bowl
  floor. Mutants m15–m18 killed (incl. a real bowl-SDF sign bug caught in dev).
- **T4b LANDED 2026-07-18** (57d444ad3 + fold 64251a6a5, gate **699/699**):
  125 GASLAMPS — 83 down the tube (~120 m, alternating walls, 45° up), 2×16
  bright rings in the Stope (center + 500 m down into the fat half), 10 in
  chambers/connectors — via a parallel entry into the street-lamp glow tech (no
  shader fork; night-gate bypassed, sunEl −0.92 at both mouths); the egg walls
  draw LIT (kEggVal 0.32, depth-cue exempted; chambers 0.22). Red-team round 3
  (SOUND-WITH-FIXES, no P0/P1): P2-1 folded — lamp glows hoisted AFTER all
  opaque depth-writers (the world-sudbury draw-order lesson recurred); P3-1
  folded — the Murray rim app-path leg. Flyability numbers on record: worst
  sustained turn 2.9 g @ V140; the Murray dive tube plunges ~49° (a pull-up
  after the bowl descent); at V280 the deep bend needs 11.6 g — the tube is a
  slow corridor by design, fly-card note. Connectors may read dark (2 lamps
  dropped) — fly-card note. Mutants m19–m22 killed.
- **T5a LANDED 2026-07-18** (33546dd09, gate **706/706**): FLAT FLOOR —
  `[tunnel] floor_height_m = 20` (0 = OFF = bit-identical). A chord-truncated
  flat floor raised 20 m above the spine tube's lowest radial point, perpendicular
  to LOCAL up at each station. Floor SDF: plain tube-union computed first (nearest
  capsule tracked), then intersected once against the floor half-space at the
  nearest station — one clean truncation, no per-capsule leak. Rendered as a
  separate kFloor strip piece stitched from per-station chord edges.
  Test legs: floor=0 SDF + mesh byte-identical; above=inside/below=SOLID at a
  flat corridor station; junction open (egg union survives below the floor plane);
  fly-into-floor crashes. Mutants m23–m26 killed (sign + chord-truncation class).
- **T5b LANDED 2026-07-18** (8755c3eb6, gate **707/707**): LAMP LAYOUT — replaces
  the tube's ±45° alternating lamps with FLOOR-EDGE PAIRS (kFloorLampSpacing = 60 m
  along the whole spine incl. the bottom ramp; one lamp per side at the
  floor-meets-wall junction, raised kFloorLampRaise + inset from wall; floor OFF →
  fallback ±60° low-wall positions) and a CROWN ARC LINE (kCrownLampSpacing = 120 m
  along the top of the arch). Egg-ring/chamber/connector lamps unchanged.
  Test legs: floor-edge both-sides + crown line (config-derived counts, floor ON
  AND floor-OFF fallback); both-sides + crown-placement mutations verified.
  Mutants m27–m30 killed.
- **T5c LANDED 2026-07-18** (6cc2ca5ad, gate **713/713**): DESTRUCTIBLE LAMPS —
  `render/tunnel_lamp_hits.h` (PURE, seads_render_core, firewalled): TunnelLampWorld
  (positions + per-lamp alive[] + dirty), swept-segment-vs-sphere hit pass
  (kLampHitRadius = 8 m; catches the tunneling case), one-round-one-lamp retire.
  `app::tick` / `step_frame`: new `lw` pointer (default nullptr → bit-identical).
  lamp_hits runs AFTER combat_tick (drones win). Deaths PERSIST across respawn
  (world state — respawn branch never touches lw). Renderer rebuilds glow tiers
  from alive lamps on `dirty`. Test legs (test_tunnel_lamps.cpp): direct hit; near-
  miss+boundary; tunneling; dead-excluded rebuild; one-round-one-lamp; inactive
  ignored. Radius + swept-segment mutations verified. Mutants m31–m35 killed.
- **T5d LANDED 2026-07-18** (710651481, gate **714/714**): TERRAIN-HUGGING COLLAR —
  kills the portal strobe. build_collar / build_bowl_wall now lay the terrain-overlap
  annulus as a MULTI-RING band (rings inward at ~terrain-cell arc density, each vertex
  on radius_at(dir) − 1.5 m tuck, 2× azimuth segments kCollarBandSegments), reducing
  2:1 onto the exact tube-end seam ring. Murray rim hugs terrain to the true bowl rim.
  Null-ground / no-grid path falls back to the pre-T5d single band bit-identically.
  DEM KILL TEST (test_tunnel_mesh, headless via stb_image): loads real Sudbury DEM +
  real app config, asserts every collar/bowl-rim triangle interior sits strictly below
  facet_radius_at across the overlap annulus. Measured post-fix: Errington max poke
  −1.22 m; Murray rim max poke −0.78 m (both safely buried). Mutants m36–m39 killed.
- **T5 RED-TEAM VERDICT: SOUND-WITH-FIXES** — P1-1 NAMED ON CARDS (soft-skin trade:
  crash boundary sits 2–5 m below the visible floor in the flat corridor, up to ~32 m
  on the steep ramp near the egg, because the SDF chord-truncation undercuts the
  rendered mesh; fly NEVER dies earlier than visuals suggest; hard-floor fix is
  per-vertex SDF truncation, a follow-up ruling); P1-2 FOLDED HERE: differential
  firewall leg added to test/unit/test_tunnel_lamps.cpp ("T5c lw=nullptr is bit-
  identical to lw-populated, no firing (firewall)") — 100-tick step_frame with lw
  populated vs lw=nullptr, fire_held=false, BIT-IDENTICAL LoopState curr+prev
  (memcmp == 0), alive[] all true, dirty false. Gate **715/715**.
- **T6a LANDED 2026-07-18** (5be25f996): TUNCAM — mouth/bore screenshot presets, the
  rendered-frame verification instrument for the visual mouth mechanisms (the process
  lesson row below made this mandatory). No plant/gate change.
- **T6b/c/d LANDED 2026-07-18** (a4b4cd0db): (T6b) ELLIPTICAL BORE — the spine tube is
  now an elliptical cross-section, semi-axes tube_width_m 110 × tube_height_m 90 (220 m
  wide × 180 m tall), SDF + mesh single-source the (horiz = cross(tan,up), vert = up)
  frame; (T6c) THE ERRINGTON ENTRY PIT — the home mouth is a recessed open-cut crater
  (bore CENTER sunk mouth_sink_m 130; crown ~40 m below grade), same open-cone Bowl
  shape as the Murray raid pit, terrain cut spans the pit rim (single-source
  errington_cut_radius); (T6d) THE MONOTONE DESCENT — absolute-radius profile,
  constant-grade descent to a deep plateau, cover cap keeps min_cover_m rock overhead
  en route, cumulative-min from the home side makes the Errington leg monotone by
  construction (the bore never reads as climbing back to the surface).
- **T6 scatter fold LANDED 2026-07-18** (c1133331d): tree scatter excluded inside the
  portal cut disks (no trees standing in the open pit / bowl).
- **T6 RED-TEAM VERDICT: SOUND-WITH-FIXES** — folds landed HERE (this commit):
  - **P1-1 (real monotonicity coverage):** the T6d cumulative-min was NAMED-but-UNPINNED
    (the prior author's claimed kill did not exist — commenting the cumulative-min out
    survived the whole gate, because the canon profile is already monotone on the uniform
    fixture AND the real DEM: the cover cap is fully inert at canon depth, dt =
    depth+0.85·egg_down ≫ min_cover+tube_height, so the safety net never fires). Added
    `test_tunnel.cpp` "T6d monotone descent": a synthetic ridge-valley HeightField (a
    mid-path terrain feature along the Errington→Murray great circle, two dips separated
    by a RISE) built against a deliberately-shallow test tunnel where the cover cap BINDS
    — so the terrain rise reaches the profile and, without the cumulative-min, lifts the
    bore back up. **MUTANT VERIFIED RED:** commenting the cumulative-min assignment fails
    the leg at node 28 (|pos| 17441.18 > prev 17440.13 + 1e-9, ~1 m rise between the
    dips; 17 violating nodes over the leg, up to 7 m). Also pins crown cover ≥ min_cover
    on the same leg.
  - **P3-3 FIXED (not just documented):** the cover cap ran BEFORE the smoothing pass, so
    smoothing lifted the crown back ABOVE the cap near a dip (~8 m poke, a shallow-tunnel
    cover violation the P1-1 fixture exposed). REORDERED to smooth → cover cap →
    cumulative-min (cap last, then monotone). BIT-IDENTICAL on the shipped deep tunnel
    (the cap is inert at canon depth, so order cannot matter there — no golden moves,
    test_tunnel_mesh's depth_m=2000 fixture unchanged). The reorder is what makes the new
    cover pin pass at the exact min_cover boundary.
  - **P2-1 (single-source pit rim):** the pit rim factor was welded in TWO places
    (main.cpp `2.0*tube_width` feeding the CutDisk; build_tunnel_net `max(2.0·w, 1.6·w)`
    with a dead max). Hoisted `world::kPitRimFactor` + `errington_pit_rim()` in
    tunnel_net.h; both sites read it; the dead max is gone. Pinned in the T6c cut-radii
    test (net.pit.bowl_r == errington_pit_rim(tube_width)).
  - **P3-1 (lying comments):** tunnel_net.h + game.toml said mouth_sink_m sinks the bore
    CROWN; the code sinks the bore CENTER (crown = mouth_sink − tube_height). Comments
    corrected to bore-CENTER (matching load_game.h which was already right).
  - **P3-2 (floor validator):** load_game.cpp allowed floor_height_m < 2·tube_height
    (a floor above the bore center, contradicting its own "keeps crown clearance"
    comment). Tightened to < tube_height (floor stays below the bore center).
  - Gate **716/716** (+1 T6d monotone leg).
- **PROCESS LESSON (T6):** T5's mouth shipped on GEOMETRY-ONLY verification (SDF/mesh
  pins) and the entrance read WRONG in the rendered frame ("opaque positive structure").
  T6a (TUNCAM) makes RENDERED-FRAME verification mandatory for VISUAL mechanisms — a
  greybox that passes its geometry gate can still fail the pilot's eye; screenshot it.
- **T6-P0 (32b6dda26, gate 718/718): the mid-entrance unfair death.** Chad round-4:
  "die every time without even hitting a wall" dead-center at Errington. Probe-proven:
  the pit/bowl SDF was a NARROWING CONE while the terrain CutDisk removes a
  depth-independent CYLINDER — the visually-open shell between cone wall and crater
  edge read SOLID; the T3 deep-penetration clause (gated on `!contains()`) killed at
  ~50 m depth in clear air. Dead-center-to-floor happened to survive; every off-axis
  descending line died — hence "the middle" still dying. Fix: `bowl_sd` = rim-radius
  cylinder + flat floor (survivable volume == removed-terrain volume by construction);
  `floor_r` retained for the visual funnel mesh only. Pins: walked center +
  0.3/0.6/0.9×rim entry descents through the LIVE `sim::step` predicate, BOTH portals;
  cone-revert mutant RED.
- **T7 (edcabed3b, gate 720/720): "a WALL inside… still got HILLS in there" + the
  MACHINE-FLOWN standard.** Root cause was NONE of the plausible suspects: the tube
  SDF's min-over-per-segment elliptical capsules with FLAT endcaps left convex-kink
  SOLID WEDGES at every interior spine node on the ~37° descending bending spine —
  **917 false-solid samples inside the flyable cross-section, the upper half of the
  bore reading solid at joints** (Chad's wall AND his hills). Fix: true
  point-to-polyline nearest (one segment frame, C0 across joins, endcap penalty only
  at the two true terminals) → 917 → 0. **T7 FLYTHROUGH VERIFIER now in the gate:**
  the whole route both directions (~1600 stations ≤10 m, live `sim::step` predicate +
  ~45 m clear-corridor discs, ~25.6k assertions, 1.1 s); mutants (interior-cap
  reintroduction; injected 50 m mid-bore blob) verified RED. Fresh red-team verdict
  **SOUND**: independent ~19 M-sample false-OPEN sweep = 0 (the rewrite's dual
  failure mode), terminals/floor/bend-wedge attacks clean, wall-honesty hug intact
  (worst |sd| 0.043 m), goldens untouched. P3 nits logged: the "blob caught" test
  name over-claims (the flythrough leg owns that kill); the 8-spoke corridor disc
  can miss a small off-center blob between spokes (inherent to discrete discs); the
  "approach above the rim" comment overstates the path start.
- **T8 (gate 722/722): "an opaque grey WALL CURTAIN ~1 km in, after a RISE in the
  ramp — I can't see where to go, it made me crash."** Chad round-5, into HONEST
  geometry: the T7 flythrough passed there and the corridor CENTRELINE stays open
  ≥1200 m ahead the whole route (pure-geometry fact) — so this was VISIBILITY, not
  containment. ATTRIBUTED (TUNCAM_BORE sag series s=3800..5200 + an offline
  centreline/tangent sight-distance dump): at the Errington ramp→plateau SAG knuckle
  (arc ~3.8–5.0 km) the pilot descends the steep bore then the floor flattens UP into
  the sightline; the floor facet there faces the descending camera near head-on, so
  the shader's local-up floor key `max(-dot(N,up))≈1` drew the floor as a BRIGHT GREY
  MASS (kFloorVal 0.16 × ~0.6 key) that painted OVER the crown/floor-edge lamp lines
  — the vanishing-point cue that carries the continuation — reading as an opaque wall.
  **PHYSICS WALL, honestly stated:** the tangent-ray floor sight cannot be pushed past
  ~450 m by GEOMETRY at any reasonable depth (2850 m over ~5 km arc is inherently a
  ~0.6 grade; ramp_frac/depth sweeps + a curvature-limiter all cap ≤455 m; deeper
  sag-curves overshoot the plateau / move the deep-junction goldens). So the fix is
  the ONE contrast dial T8 authorizes: **kTunnelUpKey 0.55 → 0.15** (render/tunnel.cpp)
  — the grazing floor darkens so the LAMP LINES read through it and the continuation is
  visible; the floor keeps a subtle up-key sheen at normal grazing (the tube shape still
  reads at s=1000). GEOMETRY UNTOUCHED (goldens unmoved). **T8 SIGHTLINE VERIFIER now
  in the gate:** `world::centerline_sight_distance` (pure) + a leg asserting the forward
  centreline line-of-sight ≥ 800 m at every interior station BOTH directions (222
  assertions, ~7 s), the mouth neighbourhoods exempt (the open cut caps sight there).
  Mutant verified RED: kProfileSmoothPasses 60→0 (a real sharp knuckle) collapses the
  interior sight to 640 m @ arc 4000; the in-gate leg injects the equivalent fold and
  REQUIREs the collapse. before/after PNGs in verify_shots (sag_4500 grey dome →
  t8_bore_4500 readable). sight_min is a TEST-OWNED constant (no build code consumes it
  — a game.toml dial would be a dead value the loader owns but nothing reads).
- **PROCESS LESSON (T8):** "the volume is open" (T7) and "the way ahead is VISIBLE"
  (T8) are DIFFERENT instruments. T7's flythrough/corridor-disc is a CONTAINMENT test
  (is the cross-section open); it passed at the curtain because the geometry WAS open.
  The curtain was a render READABILITY failure — a bright floor painting over the
  vanishing-point lamp cue — invisible to every containment/wall pin. For "can the
  pilot fly it," pin line-of-SIGHT (a straight chord staying inside), not just
  containment. And when a felt "wall" survives an honest geometry audit, suspect the
  SHADER (what's drawing over the cue), not only the mesh.
- **PROCESS LESSON (T7):** an interior-VOLUME sweep is a different instrument from
  wall-vert pins — every mesh vert sat honestly ON the SDF (|sd|<2 green) while 917
  solid wedges floated in the middle of the flyable space, invisible to every
  boundary test. For any flyable volume: pin the VOLUME (flythrough + corridor
  discs + interior grid), not just the surface. And re-run a subagent's claimed
  mutant kill — a second false kill claim was caught this rung (the blob test).
- **T9 (gate 727/727): "when I fly into the egg I get a ZOOMED-OUT view of the egg
  and I cant see my plane… The egg can be way way bigger and well lit and I can see
  my plane inside of the egg."** Chad round-6. TWO mechanisms, TWO commits.
  - **T9a CAVECAM (1eac00913):** the zoomed-out view was a CAMERA bug, ATTRIBUTED in
    render/camera.cpp. Inside the tunnel net the eye is always below R+2, so on a
    nose-UP pose the chase eye behind is radially LOWER than the plane → the SPEC §9.2
    surface clamp's f0<0 branch returns the higher endpoint (target) → the degenerate
    reseat lifts the eye ~2 km straight up to bare-sphere radius = the zoomed-out
    exterior egg view, plane invisible. (Nose-down/level the eye behind is higher →
    returned unchanged, why the descent always looked fine.) FIX: a defaulted
    `underground` flag on chase_camera + aim_chase_camera that SKIPS the clamp + reseat
    (eye = raw offset) so the camera stays on the plane inside the cavern; computed once
    per frame from the draw-state position via the single-source predicate
    `app::inside_tunnel()` shared with the crash-yield (bit-identical refactor there).
    Default false ⇒ surface flight bit-identical. Pins: pure-arm both directions
    (underground ⇒ eye == raw offset below the target; surface ⇒ eye hoisted to bare
    radius exactly), no-op-at-cruise bit-identity, the forwarding pin through
    app::instructor_camera, the shared-predicate leg. Mutants killed: m1 (drop the flag
    forwarding → the forwarding leg's ug/sf poses collapse equal, 0>100 fails), m2
    (invert the flag in aim_chase_camera → underground pose hoisted, surface pose
    dropped — both arm legs fail). NAMED TRADE: a sub-50 m wall graze
    (deep_penetration_m=50) can drop out of contains() for a few frames before the crash
    fires → a brief camera pop on an already-dying trajectory. Statelessness is safe —
    DEM-probed boundary numbers: Errington mouth 95.3 m above bare R, Murray 146.0 m,
    corridor terrain min 81.3 m, min within 1.55 km of either rim 74.8 m; every
    air-crossable contains() boundary sits ≥ ~75 m above the R+2 clamp threshold, so the
    gate boundary and the clamp-binding region are DISJOINT (no hysteresis added —
    audited as gold-plating).
  - **T9b GRAND EGG + WELL LIT (6a706be68):** egg grown to **2600 m tall × 2200 m wide**
    (~4× volume): egg_up 800→1100, egg_down 1000→1500, egg_horiz 600→1100; depth
    2000→1600 so the corridor plateau is shallower and the egg-top rock cover
    depth_m−egg_up_m = 500 stays ≥ the loader floor 400; chamber_lateral_offset 850→1500
    (a 260 m gap the connectors auto-span). Junction deep_target = 1600 + 0.85·1500 =
    2875 (~25 m deeper than the old 2850 — immaterial to the T8 sag wall). **WELL LIT:**
    a per-piece ambient floor — the lit egg gets `kEggAmbient 0.75` (walls read
    ~0.24–0.29 mid-grey) while every other piece keeps `kTunnelAmbient 0.05`; the
    tube/floor key (incl. the T8 grey-curtain fix) is untouched (the egg is the
    DESTINATION at the end of the bore, not the sag-knuckle floor). **LAMPS:** the two
    welded egg rings → DERIVED rings, one every `kEggRingSpacing 350` along the long axis
    spanning `0.6·a_pos` down to `0.8·a_neg`, count scaled from the span (the 2600 m egg
    gets ~4 rings here / more on the shipped egg); `kEggRingCount 16→20`; bright lamp
    size 22→32; `kEggRingDrop` removed (no zombie constant). **TESSELLATION:** kPocketLong
    24→48, kPocketLat 16→24 so the 2.2 km cavern reads round (under the u16 index cap).
    - **P0-1 FOLD:** test_tp() updated to the new canon so the T7 flythrough, T8
      sightline, T6 monotone, and T1-egg legs machine-fly the NEW geometry — all pass.
      **REAL FINDING investigated + fixed honestly (NOT shaved):** the GRAND egg swallows
      the entire flat descent-bottom (the only flat region of the V-spine), so the T5a
      floor-truncation legs — which test the egg-INDEPENDENT floor SDF and need a
      flat-clear corridor — now use a dedicated COMPACT-egg fixture (test_tp_floor) where
      a flat plateau clears the egg; the grand egg's own coverage is the T1/T7/T8 legs.
      This is a DECOUPLING (the shipped floor code + floor dial run), not a shave. The
      flat_corridor_station clearance was re-derived config-relative (the egg's own
      anisotropic two-half metric > tube_height) replacing a welded 800 m.
    - **P0-2 FOLD:** the welded 2-ring/32-lamp test rewritten config-relative against the
      SAME span/spacing/egg_ring_radius formula; mutant killed (nrings:=1 → 40 lamps vs
      the derived 80 → fails).
  - **DAY-SCREENSHOT VERDICTS (shots_t9/, all Read):** EGG interior (dz0/az0, dz+700/az90,
    dz−1000/az180) = lit mid-grey walls, multiple lamp rings, cavern shape reads,
    floor/tube silhouette visible. BORE junction approach (s=4200..6000) = the bright lit
    egg reads as the DESTINATION through the dark bore, crown + floor-edge lamp lines
    CONTINUOUS leading in. T8 regression (sag s=3800/4600/5200) = NO bright grey floor
    curtain — floor stays dark, lamp lines read through, the bright dome ahead is the LIT
    EGG destination (correct). No dial iteration needed (kEggAmbient 0.75, tessellation
    48×24 both read right first pass). New TUNCAM_EGG preset:
    `SEADS_TUNCAM_EGG="dz_m,az_deg[,r_frac]"`.
### T10 — THE HOLLOW CORE (2026-07-19, LANDED — ⚠ SUPERSEDED BY T11, kept as record)

> **SUPERSEDED BY T11 (THE CORE WINDOW), 2026-07-19.** Chad flew T10 and RULED it out:
> "way too vast... I wanted to see the core not from every possible angle you can't escape
> from there... you dive a little bit it takes ages to climb back... I said 100x not close
> to 800x the egg." The concentric shell (725× the egg) is REPLACED by one shallow ARENA
> ellipsoid + a core-window SHAFT (~100× the egg). See the T11 section below. The T10
> record stands for the design history.


Chad's ruling: "I got to the egg but had no visual reference, its dirt so it has to be
huge and I can't crash right when I get into it — make it a huge inner space where a
bunch of planes could dogfight... the core to the 15km sphere that lights the cavern...
we should see the different layers of the planet, core / outer core / mantle / crust, and
well lit by the core. two ways in two ways out — the egg should be about 100x the size."

The buried egg is GONE. The sphere INTERIOR is now a CONCENTRIC HOLLOW SHELL — the design
was Fable-consulted (SOUND-WITH-FIXES); the folded P0s landed:
- **P0-1 (net):** DELETE the egg Pocket + its build. `world::TunnelNet` gains
  `core_r/cavern_r/cavern_on` and ONE hollow-shell SDF term
  `shell_sd = max(|p|-cavern_r, core_r-|p|)`, min-unioned. The core interior AND the rock
  above the ceiling read OUTSIDE the net, so the FROZEN crash predicate (ground_contact +
  the deep-penetration wall strike) makes the core and every rock face an honest crash
  wall with ZERO kernel changes. The chambers RE-HANG off the bores (`chamber_breach_offset_m`
  ≈ 800 m above each breach), the game-loop pump hook preserved.
- **P0-2 (mesh + render):** a new inward-viewed cavern sphere at cavern_r (336×168, u16
  cap clears at 56953 verts) SKIPPING facets within `breach_hole_arc = 2.5·tube_width` of
  each breach axis (the bores read THROUGH the ceiling); the bore is SUPPRESSED below the
  breach (no wall across open cavern); a solid glowing CORE sphere at core_r
  (PieceKind::kCore, emissive self-glow + an additive glow billboard 1.5·core_r). The FS
  gains a radial CORE KEY (bright toward the centre, 1/(1+(r/6000)²) falloff) on the
  cavern ceiling, radius STRATA bands on the bore walls (crust r>13k / mantle 10.4-13k +
  a boundary line), and BREACH SPILL (the bore's last ~600 m glow with core light — the
  angle-of-entry cue). The egg lamp rings are gone (the core lights the cavern).
- **P0-3 (guns):** `weapon::retire_round` threads the net so a round at/under R that is
  INSIDE the tunnel/cavern keeps flying (guns fire underground); only rock/core retires
  it. Bandit return-fire gets the same predicate. Null net => the legacy retire,
  bit-identical.
- **P1-1 (profile):** the deep plateau is now the ABSOLUTE radius
  `cavern_ceiling_m - breach_margin_m` (10200), so the bores are SHORT (~4.6 km at ~50°)
  and BREACH through the ceiling. `deep_target` retired.

**THE 100× RULING (Chad: "100x the size... we will start with that number"):** 100× the
flown egg = the FLOOR. The spec's OWN items (short tunnels, core-lit layers) force the
ceiling to 10500 ≈ 725× the flown egg (shell airspace ~8 km radial / ~21 km across).
`cavern_ceiling_m` is THE size dial — LOWER it = smaller cavern + LONGER tunnels. Honest.

**FELT NUMBERS (name them on the card):** the weightless band is r = V²/g — 6.4 km @ V250,
2.0 km @ V140. Below it, level flight needs lift pointing AT the core (~−1.1 g to hold
r = 3 km @ V250). The core (r=2500) floors |position| clear of the r→0 singularity
(local_up / the S-ffrad curvature ff both divide by |p|).

**NAMED TRADES:** ≤few-frame camera pop on a fatal wall graze; eye-in-rock transients near
the cavern walls; and — this rung is SPACE + LIGHT + LIVE GUNS; the bandits have NO cavern
AI yet (a cavern dogfight AI is its own rung).

**Config:** `[tunnel]` egg_* keys DELETED; `cavern_core_m=2500`, `cavern_ceiling_m=10500`,
`breach_margin_m=300`, `chamber_breach_offset_m=800` added. Loader checks: core ≥ 1500,
shell airspace ≥ 2500, rock mantle ≥ 2500, breach_margin ≥ tube_height + 100.

**Verifiers:** T7 flythrough + T8 sightline carry (route = the spine through the cavern).
NEW legs: the shell 3-state sweep (cavern open / core solid / rock-above-ceiling solid),
the shell SDF sign, the cavern-breached + tube-suppressed mesh pins, the chamber re-hang,
and the weapons underground-fire trio. FOUR mutants re-run + observed dead: shell SDF sign
(max→min), breach-hole skip dropped, tube-suppression dropped, retire-predicate inversion.

**DAY-SCREENSHOT VERDICTS (shots_t10/, all Read):** CAVERN core (r=9500 + r=6000) = the
glowing near-white core reads DRAMATICALLY across the dark cavern (bigger/brighter as you
close). CEILING (r=10000) = the mantle underside reads as a dim grey field lit from below
by the core (after a one-dial fix: the strata mantle 0.07 was crushing the ceiling to
black — split so strata is BORE-WALLS-ONLY, the cavern keeps kCavernVal + the core key).
BREACH (r=9500) = the bore stub + a breach opening read against the grey ceiling with
faint bore lamps below. BORE (s=500..4200 descending) = clean tunnel with CONTINUOUS
crown/floor lamp lines + a BRIGHT breach glow ahead (the light at the end of the tunnel =
the angle-of-entry cue). MOUTH = the recessed open-cut PIT reads as a dark crater with the
pit-lamp glow inside. T8 REGRESSION (s=2500/3500, the ~50° knuckle) = NO grey curtain,
lamp lines read through. New TUNCAM_CAVERN preset
`SEADS_TUNCAM_CAVERN="r_m,az_deg[,look=core|breach|ceiling]"`.

**T10.1 — CAVERN READABILITY (2026-07-19, LANDED, gate 733/733, render-only).** The DRIVER
Read shots_t10/ and found the felt-spec failures: at r=9500 the core read but EVERYTHING else
was pure black (no depth cues); the r=10000 ceiling was a UNIFORM featureless grey (the "no
visual reference, its dirt" failure at 10× scale); the breach read as a dark angular blob
(the exits would be unfindable across the cavern). The bore was GOOD — untouched. The fix (the
repo's flown "to see a big space you need POINT LIGHTS, not wall shading" lesson — the
world-sudbury street-lamps-from-altitude precedent — applied at km scale):
- **EMBER FIELD** (render/tunnel_mesh.cpp place_tunnel_lamps): `kCavernEmberCount = 600` dim
  warm embers scattered DETERMINISTICALLY (a Fibonacci-sphere lattice, no clock/random) on
  the ceiling sphere at cavern_r, skipping breach holes (single-sourced with the ceiling
  mesh's `breach_axes()` + hole predicate). Their own render tier (`ember_lamp_look`, size_m
  90, min_px 2, brightness 0.42, night_bypass ON) — a km-readable glow (an 8 m tube-lamp glow
  is invisible at 5-15 km). Gives the black shell depth/parallax.
- **BREACH BEACON RINGS**: `kBreachBeaconCount = 12` bright lamps around each breach lip (arc
  radius = breach_hole_arc + `kBreachBeaconArcMargin_m` 140, on the ceiling, centered on each
  breach axis). Bright tier size_m 48, min_px 5 so each beacon stays a legible dot at ~19 km.
  Each exit now reads as a RING OF LIGHT.
- **NIGHT-GATE BYPASS UNIFORM** (render/lights.*): the ceiling ember field spans a FULL
  sphere, so no single sun dir reads night for every lamp; added `uNightBypass` (LampLook
  `night_bypass`, default 0 = street lamps bit-identical) forcing the whole tunnel lamp set
  always-lit (a contained mine).
- **CORE KEY RESHAPE** (render/tunnel.cpp FS): `kCoreKeyGain` 0.60→1.10, `kCoreKeyFall`
  6000→9000 so the near ceiling gets a visible GRADIENT (orientation cue) + the far wall reads
  dim-not-black. Plus a LOW-FREQUENCY ceiling value-noise (±15% around base, three sine
  octaves of the world direction — deterministic) so the mantle underside is not a
  mathematically uniform wash. Strata (bore-walls-only) + the T8 bore floor-key UNTOUCHED.
- **FINAL DIALS:** kCoreKeyGain 1.10, kCoreKeyFall 9000, kCavernEmberCount 600, ember
  size_m 90 / min_px 2 / bright 0.42, kBreachBeaconCount 12 / margin 140 / bright-tier
  size_m 48 / min_px 5, ceiling mottle ±15%.
- **TESTS** (test_tunnel_mesh, +2 legs, +~2080 assertions): ember/beacon COUNT (config-derived
  + breach-hole-skip oracle, with a WIDENED-BORE leg driving the skip LIVE — the canon hole
  arc is sub-facet, so the canon leg is blind), all-on-ceiling, no-ember-in-hole, both rings
  encircle distinct axes, and a cavern-OFF structural-off leg. Two pre-existing lamp
  strict-inside legs updated to skip the on-boundary ceiling families. MUTANTS RE-RUN + observed
  RED: drop the ember breach-hole skip (600 vs oracle 593 at the wide bore) FAILS; drop a beacon
  ring (12 vs 24) FAILS.
- **DAY-SCREENSHOT VERDICTS (shots_t10_1/, all Read):** core_9500 = ember field scattered
  across the dark shell + the glowing core (depth/parallax now exist); core_6000 = a richer
  near ember field; ceiling_10000 = a directional gradient + subtle mottling (no longer a
  uniform wash); breach_9500 = the near breach reads as a RING OF LIGHT (beacons encircle the
  bore stub) + the far breach's ring visible top-of-frame across the cavern; breach_farside
  (r=9500 az=180, ~19 km) = the ember field reads deep, though the target breach itself is
  OCCLUDED BY THE CORE from directly opposite (a geometry fact, not a beacon failure — the
  cross-cavern ring read is confirmed in breach_9500). Bore spot-checks (s=2500/4200)
  UNCHANGED (the FS ceiling hash + core-key are gated on cavern pieces only).

- **★ NOW: CHAD FLIES ROUND 8 — `docs/tunnel_fly_cards.md`** (dive a mouth → short ~4.6 km
  bore, strata banding, breach glow ahead → break into the shell, the core lights
  everything, chase the weightless band, guns live). Dial line: `cavern_ceiling_m` DOWN =
  smaller cavern + longer tunnels. Kill-switch: `[tunnel] enabled = false`. Hero dressing
  + a cavern bandit AI wait on the fly.

- **PRIOR: CHAD FLIES ROUND 6** (round-6 card: fly into the grand egg, pull up into a
  climb — the camera stays on your plane; the cavern is 2600×2200 and lit). SUPERSEDED by
  T10 (the egg is gone). Named trade: the ≤few-frame camera pop on a sub-50 m wall graze.

### T11 — THE CORE WINDOW (2026-07-19, LANDED, gate 736/736, AWAITING CHAD'S FLY)

Chad's ruling (flying T10): "way too vast... I wanted to see the core not from every
possible angle you can't escape from there... you dive a little bit it takes ages to climb
back... I said 100x not close to 800x the egg." T10's concentric shell is GONE. The design
was Fable-consulted (SOUND-WITH-FIXES); the folded ruling landed:

**THE SHAPE.** One giant shallow ARENA ellipsoid replaces the shell; a CORE-WINDOW SHAFT
drops from its floor to the glowing core. The pilot dogfights SHALLOW (fight band 1.5-2.5
km deep), SEES the core as a ~28° glowing disc through the floor window, and diving to it
is OPT-IN.

- **Arena** (oblate, the escape ruling): horizontal semi `arena_a_m` 7350 × vertical semi
  `arena_c_m` 2600. Center radius `r_c = terr_mid − arena_depth_m − arena_c_m` ≈ 10975,
  apex ≈ 13575 (`arena_depth_m` 1500 below mid terrain), floor ≈ 8375. A third Pocket via
  the LIVE shipped `ellipsoid_sd` (u_long = the mid-path dir, a_pos = a_neg = arena_c along
  it, b = c = arena_a across it). Oblate = no more "dive a little, climb ages."
- **Shaft**: `capsule_sd(p, û_mid·(0.5·core_r), û_mid·(floor+500), shaft_radius_m)`
  (`shaft_radius_m` 2000) — both endpoints buried, no exposed caps.
- **Global core intersection** (P1-5): the FINAL SDF line `sd = max(sd, core_r − |p|)`
  keeps the |position| floor for EVERY primitive and makes the shaft floor exactly the
  curved core surface — a min-union of open primitives + ONE global max cannot manufacture
  false-solid wedges inside air.
- **The concentric family DIES, not zombies** (P1-6): `cavern_ceiling_m`, `cavern_r`,
  `cavern_on`, the shell SDF term, the shell loader checks, the T10 shell test legs, the
  concentric ceiling/hole `build_cavern` all DELETED. RE-KEYED to the arena apex:
  tube-suppression breach detection, the beacon/ember placement, the mesh breach detection.
  CARRIED untouched: guns net-awareness, CAVECAM, strata (bore-walls-only), pit/bowl/floor
  machinery, the chamber re-hang (now off the arena breach).

**Config** `[tunnel]`: `cavern_ceiling_m` DELETED; ADDED `arena_a_m=7350`, `arena_c_m=2600`,
`arena_depth_m=1500`, `shaft_radius_m=2000`. `cavern_core_m=2500`, `breach_margin_m=300`
carry. Loader checks: arena oblate (a≥c), arena_c≥1000, (arena floor)−core ≥ 1500, breach ≥
tube_height+100, shaft ∈ [500, arena_a/3], + the SHOULDER TRIPWIRE (P1-3): the shallowest
point of the oblate arena is the SHOULDER, not the apex — closed form ρ_max over θ (interior
max at cosθ*=c·r_c/(a²−c²)), require ρ_max ≤ R − min_cover − 100. At the shipped set ρ_max ≈
13846 → min cover ≈ 1.23 km below real terrain (terr_mid ≈ 15075; the bare-R form the
loader checks is conservative — the red-team's reconciliation: the earlier "1.05 km"
subtracted from R=15000 while the arena is placed relative to terrain) — passes.

**Mesh/render:** the arena interior is the pocket tessellator at arena dims (336×168, u16
cap clears at 56953 verts) with breach holes where the bores enter AND a floor opening
where the shaft meets it; a wide shaft tube (floor ring → core); embers RE-PLACED on the
arena ceiling (`kCavernEmberCount` 600→150 — density-parity said 78, readability wins),
skip breach holes + the shaft opening; beacon rings carry at the arena breaches. The radial
core key + strata carry. New TUNCAM: `SEADS_TUNCAM_CAVERN` re-aimed to arena scale +
`SEADS_TUNCAM_WINDOW="r_m,az_deg"` (the money shot — the core disc through the shaft).

**Verifiers/tests:** T7 fly_route + T8 sightline carry (the T8 mutant-kill now folds a
NARROW DESCENT-BORE node — the mid-route passes the OPEN arena, so a fold there no longer
curtains); NEW: the 5-STATE RADIAL SWEEP (rock-above-apex / arena / rock-below-floor-off-
axis / shaft-on-axis / core, PLUS a generic off-footprint surface→core all-solid — the
pilot's "not from every possible angle" made executable), the shaft crash leg, BOTH
JUNCTION TORI volume sweeps (shaft∩floor, shaft∩core — the T7 lesson: pin the VOLUME), the
arena/shaft/core mesh pins, the arena ember/beacon re-derivation. MUTANTS re-run + observed
dead: global core-intersection dropped, shaft capsule dropped, shoulder tripwire → apex-
only, ember shaft-opening skip dropped. GATE 736/736.

**FELT NUMBERS (on the card):** the core disc is ~28° through a 47° window, full-disc from
±3.6 km of the shaft axis (dial: shaft_radius_m 2500 buys ±4.6 km, costs arena_a → ~7050 to
hold 100×). The inverted-lift regime lives ONLY in the shaft below r ≈ V²/g ("past halfway
down the shaft at speed, lift points at the core — the core defends itself"). Size ≈ 100×
the egg (dials: arena_a_m / arena_c_m / shaft_radius_m). Bores ≈3.1 km at ~24° (~15 s at
200); fight band 1.5-2.5 km deep; escape 45-90 s; a 500-1000 m dive costs 25-50 s; the
arena floor is 2.5-3 min out; a core round-trip 5-7 min — the named opt-in price.

**DAY-SCREENSHOT VERDICTS (shots_t11/, all Read):** window_money (TUNCAM_WINDOW r=10975)
= THE MONEY SHOT LANDS — the core reads as a huge bright near-white POOL centered in the
frame through the dark shaft window (the ~28° glowing pool through the floor). cavern_wide
(TUNCAM_CAVERN ceiling) = the ember field scattered across the dark arena ceiling gives
depth/parallax + a gradient (readability, the reference the black egg lacked). cavern_core
(TUNCAM_CAVERN r=9000) = the core lights the space DRAMATICALLY (a bright crescent + light
rays across the arena — the inverted-lift zone). cavern_breach (r=11500, look=breach) = the
dark bore stub breaches into the arena against the ember field (the beacon ring reads
subtly — a fly-tunable if Chad wants the exits brighter). bore_s600/s1500 = clean descent,
crown/floor lamp lines receding + a bright breach glow ahead, NO grey curtain (gentler
~24° than T10's ~50°). mouth = the bore interior looking down, lamp lines leading in.
Render dials UNCHANGED (kCoreKeyGain 1.10 / kCoreKeyFall 9000 carried from T10.1 read
well at arena scale — no retune needed).

**SUPERSEDED BY T12 (THE SEALED CORE), 2026-07-19** — Chad flew round 9 and ruled the
core window OUT (see T12 below). The core-window shaft is GONE; the arena floor is sealed.

### T12 — SEAL THE CORE + MEND THE MESH (2026-07-19, LANDED, AWAITING CHAD'S FLY)

**CHAD'S ROUND-9 FLY VERDICTS (verbatim):** "Okay lets cover up the core, the chamber
seems well lit enough without the core plus there are holes going to it and I can see
through the earth. The mesh going from errington mine into the tunnel has a blocker that
blocks half the tunnel. Comin back up through the errington tunnel the other way, I can
see outside of the top of th etunnel to the earth surface before the actual exit hole. The
entrance to errington mine is no very visible, the black hole is but the tunnel shaft id
still hard to find. Lets make sure that the chamber is filled over the core (we dont need
to go that deep, but lighting needs to be adequate. Look for holes in the mesh. The tunnel
has weird ramps and cut offs of the mesh we should mend of of that properly."

**PART A — THE SEALED CORE (the subtraction).** The T11 core-window SHAFT + the emissive
core render are GONE; the arena floor is SOLID rock "filled over the core":
- **Net (`world/tunnel_net.*`):** the `shaft` capsule SDF member + `shaft_radius_m` param
  DELETED; the arena floor closes (no primitive opens below it). KEPT: `cavern_core_m` +
  the global core intersection `sd = max(sd, core_r − |p|)` — re-commented as the
  BURIED-CORE SAFETY FLOOR (the cheap |position| guard; local_up / S-ffrad curvature ff
  both divide by |p|). It never bites inside the flyable arena.
- **Mesh (`render/tunnel_mesh.*`):** `build_shaft` + `build_core` + the arena FLOOR-SHAFT
  OPENING skip DELETED (the floor is now a sealed solid ellipsoid cap); `PieceKind::kCore`
  + `kCoreLong/kCoreLat` deleted (no zombies). Piece count 10 → 8.
- **Render (`render/tunnel.cpp`):** the kCore sphere draw, the core glow billboard, the
  `uEmissive` shader branch + uniform, `kCoreVal`, and the `emissive`/`core_glow` state
  DELETED. KEPT the radial core-KEY shading on kCavern pieces EXACTLY as flown (Chad: "the
  chamber seems well lit enough" — the key + embers ARE the light; the key is paint keyed
  toward the origin, it needs no visible core).
- **Config/loader:** `[tunnel] shaft_radius_m` DELETED (game.toml + load_game.{h,cpp} +
  main.cpp tparams + the mirrors); the shaft-radius band loader check + its throw test
  deleted; the "real air ... for the shaft" check re-worded to "real ROCK ... T12 covers
  the core"; TUNCAM_WINDOW preset deleted.
- **Lighting-adequacy call (judged on the floor-ward TUNCAM PNGs):** the CEILING reads
  adequately (embers + radial core-key gradient + ceiling mottle — the reference the black
  egg lacked; Chad's "well lit enough without the core" holds there). But the floor-ward
  view was a near-BLACK VOID (the sealed floor self-shades and the core that used to glow
  below it is gone). Per the task's floor-void clause, ADDED `kFloorEmberCount` 100 floor-
  half embers (the same deterministic Fibonacci lattice, own count) so the sealed floor has
  depth/parallax. The radial core-key + ceiling embers are otherwise untouched.

**PART B — MEND THE MESH (root causes, file:line).** The manifold verifier + geometry
probes were the instruments (the dark night-lit interior does not diagnose well by
screenshot — the T7/T8 lesson: pin the geometry, not the pixels).
- **The half-tunnel blocker AND the see-through crown are ONE fault** (root cause,
  `world/tunnel_net.h:245` kPitRimFactor): at 2.0× the terrain cut is a 220 m crater that
  spans the first TWO bore nodes (arc 0 + 149.5 m), but the bore descends only ~67 m per
  150 m arc, so at node 1 the bore CROWN (r 15192) pokes 112 m ABOVE the crater floor (r
  15080) into OPEN cut air — looking up through the crown you see the surface (the
  see-through), and the tube's upper wall poking into the open crater reads as the wall
  blocking half the descent (the blocker). FIX: `kPitRimFactor` 2.0 → 1.3 (a 143 m crater)
  so the cut reaches only the mouth node — node 1 (arc 149.5 > 143) keeps its rock cover,
  the descending bore is under solid rock immediately past the mouth (verified: node ≥1
  `terrain_cut_above=0`). Still an open crater (43% wider than the bore); the T6-P0
  survivable-cylinder invariant holds (rim 143 > tube_width 110). FEEL trade on the card.
- **Weird ramps/cutoffs + "holes in the mesh":** the MESH-INTEGRITY VERIFIER
  (`test_tunnel_mesh.cpp` "T12 mesh integrity") found NO stray boundary edges or
  non-manifold edges in any opaque piece on the SEALED canon geometry (179k assertions).
  The "holes going to it / see through the earth" were the T11 core-window shaft + the
  arena floor-shaft opening — Part A sealed them. **Honest note: the verifier DOES fail on
  the pre-fix (T11) geometry** — the T11 floor-shaft opening rim is a large boundary loop
  NOT at a breach hole (my chase down of the arena floor strays confirmed the mechanism);
  sealing the floor removes it.
- **THE MESH-INTEGRITY VERIFIER (the structural pin of "look for holes in the mesh"):**
  per opaque piece, an edge→triangle-count map keyed on rounded vertex POSITION (merging
  coincident seam floats), degenerate (<0.5 m²) sliver triangles skipped (greybox pole
  caps). (1) NO edge shared by >2 triangles (non-manifold). (2) Every boundary edge
  (count==1) lies on a DESIGNED opening enumerated per kind: the two mouth end-rings + the
  breach-suppression gap + pocket seams (kWall); the collar rims (kCollar); the two breach
  holes + the degenerate ±u_long pole caps (kCavern); none (closed kChamber). Mutation
  ("T12 mesh integrity mutant"): a poked-out mid-tube triangle leaves a stray boundary edge
  in the tube body — CAUGHT (observed).
- **Errington findability — MOUTH BEACON RINGS** (`render/tunnel_mesh.*`, kMouthBeaconCount
  12): a bright ring of lamps around the actual bore opening at each mouth face (spine
  front + back), on the bore ellipse at kMouthBeaconFrac 0.8 of the semis, bright tier, so
  the shaft reads as a ring of light inside the dark crater. Both mouths (Murray untested
  but symmetric — the same fix). The Errington pit places a FULL ring; the Murray bowl-floor
  mouth truncates the ellipse against the bowl SDF so a few cull (a substantial ring still
  reads). Test leg + config-derived count, mutation-verified (drop the loop => 0).

**SEALED-CORE TESTS (Part A):** the T11 5-state sweep becomes the SEALED form ("T12 sealed
arena radial sweep": rock above / arena open / rock SOLID ALL THE WAY DOWN on the mid axis
to the core — the executable "filled over the core" pin); the shaft flythrough + BOTH
junction-tori legs DELETED, replaced by "T12 sealed floor" (below-floor solid crashes,
above-floor survives); the arena mesh floor-opening leg INVERTS (in_opening > 0 — the floor
is filled); the tagging/count legs drop shaft+core (cavern==1, piece count 8); off-footprint
all-solid carries. MUTANTS observed: re-adding the shaft would re-open the sealed column
(the sealed sweep FAILS); the poked-tube mutant is CAUGHT by the verifier.

**FINAL DIALS:** `kPitRimFactor` 1.3 (was 2.0); `kMouthBeaconCount` 12 / `kMouthBeaconFrac`
0.8; `kFloorEmberCount` 100 (NEW — the sealed-floor readability add); arena/core/depth dials
UNCHANGED (arena_a 7350, arena_c 2600, arena_depth 1500, cavern_core 2500 — now the buried
safety floor). Render dials UNCHANGED (kCoreKeyGain 1.10, kCoreKeyFall 9000, kCavernEmber
150).

**DIFF RED-TEAM FOLDS (SOUND-WITH-FIXES, 2026-07-19):** two folds landed (gate 738→741, 3
new legs). **P1 — pin the RELATIONSHIP, not the constant** (`test_tunnel.cpp` "T12 P1"): 1.3
works because the crater rim CYLINDER (`errington_pit_rim = kPitRimFactor·tube_width`) is
narrower than node 1's lateral distance from the mouth axis, but that is calibrated-to-today
(the S8-drone trap) and the T6d cover pin SKIPS the near-mouth pit region by construction. The
new leg derives everything from the BUILT net + params (config-relative — a retune of
kPitRimFactor/spacing_m/mouth_sink/arena_depth/breach_margin/ramp_frac moves the leg's inputs
with it, never re-welds 143/149.5): (a) node 1's lateral offset from the mouth axis must exceed
the pit rim (it sits outside the open cut), and (b) for every near-mouth node inside the rim its
bore crown (`|pos|+tube_height`) must stay ≤ the pit-floor radius (no poke into the open cut) —
canon has zero non-mouth nodes inside the rim, so (a) is the live pin. Mutation `kPitRimFactor`
1.3→2.0 (the original bug) FAILS it (observed: node-1 lateral 149.5 < rim 220). **P2 — run the
verifier on the SHIPPED floor-ON geometry** (`test_tunnel_mesh.cpp` "T12 P2"): the existing
manifold leg runs `floor_height_m=0.0` while game.toml ships 20.0, so the kFloor strip's chord
truncation + its added strip piece were unseen. HONEST RESULT: the mesh PASSES floor-ON — the
tube's own boundary-edge count is UNCHANGED (96) by the truncation (below-floor ring verts slide
UP to the floor plane and merge into degenerate slivers the area filter drops; no new tube-wall
strays), and the only new piece is the closed kFloor ribbon (92 boundary edges = 88 chord RAILS
+ 4 TRANSVERSE caps, all at the two mouths + two breach breaks). The kFloor case is NOT
blanket-allowed (the never-shave rule): a boundary edge is designed only if it is a chord rail
(off-axis) OR a transverse cap AT a mouth/breach break — an interior transverse edge is a runway
hole. Mutation (poked-out interior floor quad) FAILS it (observed: interior transverse edge
appears). Both mutants restored; gate 741/741.

### T13 — CENTER THE CHAMBER + EASE THE ENTRY + THE HEADFRAME (2026-07-19, LANDED, AWAITING CHAD'S FLY)

**AUDIT TRIGGER (Chad's round-9/10 asks + measurement):** (1) the "chamber must lie in the
MIDDLE between the two entrances — two proper tunnels meeting a central room" — the room
swallowed most of both bores; (2) a felt short-Errington / long-Murray leg asymmetry; (3)
flying INTO the tunnel was hard (a bore-tangent entry line clipped rock behind the sunk pit);
(4) blind side-passages read as real routes; (5) the round-10 headframe callin — a surface
pit-head structure over the Errington mouth "just like a real mine would."

**AUDIT NUMBERS (uniform 300 m field):** at `arena_a_m = 7350` the wide oblate ellipsoid
reached so far up-bore that the first arena-interior node fired only **3176 m (E) / 2958 m
(M)** from the mouths while the breach-to-breach crossing spanned **5910 m** (the crossing =
1.86× the mean leg — the room dwarfed the tunnels). Shrunk to `arena_a_m = 4200`: the thirds
become **4034 (E-bore) / 4055 (crossing) / 3954 (M-bore)** — comparable, a central room fed by
two proper tunnels. Entry: the bore-tangent approach chord (LIVE plant) crashed **5 (E) → 0**
with the trench; Murray **0** (the easy reference). Real-DEM skew (the felt E/M asymmetry): a
**1.4%** leg-length difference — instrument, not geometry. The chambers are **confirmed dead
ends** (no pump game-loop consumes them yet), so they are gated off.

**THE FOUR MECHANISMS:**
- **A1 — CHAMBER CENTERING (S2):** `arena_a_m` 7350 → 4200 (the horizontal semi; the oblate
  short axis + apex depth unchanged). The room now sits in the middle, thirds comparable. A
  **T13 chamber-centering verifier** measures the bore/crossing/bore thirds off the built net.
- **A2 — CHAMBERS GATED OFF (S1):** `chambers_on = false` ⇒ the two pump Pockets + their
  connector Capsules are structurally absent (b == 0 / r == 0) so they never bite the SDF, are
  never meshed, place no lamps — a clear passage with no blind side-holes. `true` restores the
  T4-era side rooms (the pumps return when the game-loop lands). Piece list: tube + 2 collars +
  arena = 4 (was 8: + 2 chambers + 2 connectors). Verifier is arm-aware (no chamber-seam
  boundary edge allowed off-arm).
- **A3 — THE ENTRY APPROACH TRENCH (S3):** `world::errington_trench_steps` — the single-source
  step list (3 open-cut Bowls marching UP-TANGENT from the pit rim, opposite the underground
  route, depths ramping 60 m → ~pit depth). BOTH the SDF Bowls AND the terrain CutDisks +
  tree-clear footprint (main.cpp) derive from it, so the excavation and the survivable volume
  agree. Entirely on the approach side, over NO tunnel (the T12 crown-see-through class cannot
  re-open). `[tunnel] trench_len_m=450` (0 = OFF, bit-identical), `trench_rim_m=150`. A **T13
  entry-approach verifier** (LIVE `sim::step` predicate) pins the crash-count drop.
- **B1 — THE HEADFRAME + BULKHEAD COLLAR + BEACONS** (`render/tunnel_mesh.*`, kHeadframe): a
  steel-lattice SINKING HEADFRAME (Errington Mine No.3 silhouette — a tapering four-leg lattice
  tower with a sheave head house on top + an inclined back-brace pair leaning up-tangent over
  the pit) straddling the entry pit, a low bulkhead collar ring at the rim (broken at the trench
  opening), + 2 red aviation beacons atop the head house (reusing the T12 mouth-beacon bright
  tier ⇒ night findability free). **VISUAL-ONLY — NO SDF: planes fly THROUGH it** (a deliberate
  scoped trade; the headframe is a landmark, not a wall — the SDF stays the pit/trench volume).
  Every member is a CLOSED manifold box (`push_box`: 8 verts, 12 tris), so the T12 manifold
  verifier passes trivially (no boundary edges). Positioned ENTIRELY from the net (the pit frame
  + `net.pit_up_tangent`) — the collar gap + the brace lean derive from ONE frame, no magic
  azimuth. Piece: 31 boxes / 248 verts / 372 tris. `[tunnel] headframe_h_m=80.0,
  headframe_on=true` (false ⇒ piece absent, bit-identical mesh).

**B1 TESTS (`test_tunnel_mesh.cpp`, 4 legs + 2 mutation-verified):** (a) ON ⇒ one kHeadframe
piece, verts>0, cue==1.0, every vert within a DERIVED mouth bounding sphere (H + trench_len +
brace_reach + margin); (b) OFF ⇒ zero headframe pieces AND non-headframe pieces BIT-IDENTICAL
to the on-arm (structural-off); (c) the kHeadframe piece is edge-manifold with ZERO boundary
edges (closed boxes); (d) the collar GAP faces the trench (no collar box center in the gap
arc). Mutations: drop the headframe_on gate ⇒ (b) FAILS (kHeadframe in the off build);
flip the gap azimuth sign ⇒ (d) FAILS (2 collar centers in the trench arc). Both observed.

**DIALS (with one-line reverts):** `arena_a_m = 4200.0` (revert **7350.0**); `chambers_on =
false` (revert **true**); `trench_len_m = 450.0` / `trench_rim_m = 150.0` (revert **0.0** = OFF);
`headframe_h_m = 80.0` / `headframe_on = true` (revert **false** = piece absent, bit-identical
mesh). Silhouette dials (leg spread, member section, brace reach, collar gap arc) are code
constants in `render/tunnel_mesh.h`.

**NAMED TRADES:** the headframe is **FLY-THROUGH (no SDF)** — a deliberate scoped choice (a
landmark, not a collision wall; adding an SDF would put a wall in the approach corridor A3 just
opened). The chambers RETURN (`chambers_on = true`) when the pump game-loop lands. Gate 752/752.

**ROUND-10 FLY VERDICTS (Chad, 2026-07-20):** blind spots entering AND exiting the central
chamber (flyable on faith, opaque both ways); cannot fly into or out of Murray from that side.
Root-caused by the scout pass (`docs/tunnel_scout_2026-07-20.md`, commit 9395fcbfa) — ONE bug,
fixed by T14 below.

### T14 — THE TRUE BREACH LOCATOR (2026-07-21; the round-10 blind-spot fix)

**ROOT CAUSE (scout-confirmed):** T13-A1 shrank the arena (`arena_a_m` 7350 → 4200) but
`breach_axes()` still located breaches as "the spine node nearest `arena_apex_r` BY RADIUS" — a
depth rule. Holes/beacons landed 748–897 m from where the bore actually pierces the shrunk
ellipsoid: 275 m holes, NO overlap ⇒ both REAL openings stayed un-cut opaque wall (the blind
spots), the beacon rings encircled fake holes over solid rock (lethal, and why Murray read
impassable), and the tube-ring suppression (same radius rule) left ~410 m of wall-less tube per
side in solid rock. The SDF was always CORRECT — that is why faith-flying worked.

**THE MECHANISM (`render/tunnel_mesh.{h,cpp}`):** `breach_axes()` DELETED. New single source
`render::breach_points(net)` → two `Breach{pos, into, node_in, stretch}`: split the spine at the
deepest node, scan each leg for the first node INSIDE the arena, then BISECT
`world::ellipsoid_sd` (now public in `world/tunnel_net.h` — the SAME function the SDF flies, no
re-derived copy) along the crossing segment for the exact centreline↔wall intersection. `into` =
bore tangent into the arena; `stretch` = 1/cos(bore vs numeric ellipsoid surface normal), capped
`kBreachMaxStretch = 4` — the bore meets the dome obliquely, so the true opening is ELONGATED
along the tangent and a round hole would clip it. Shared predicate
`render::breach_hole_hit(breaches, hole_arc, wdir, pad)`: an ELLIPSE in planet-centre angle
space per breach (semi-minor `hole_arc`, semi-major `stretch·hole_arc`, at the breach's OWN
radius — not the surface radius). EVERY consumer routes through the pair: the arena facet cut,
the ember skip, the beacon rings, the tube suppression (now the inclusive `[node_in_E,
node_in_M]` index range = exactly the inside-arena nodes), the breach spill glow, the manifold
verifiers' "designed opening" allowance, and the `SEADS_TUNCAM_CAVERN` breach look-target
(main.cpp). Beacons: ring ellipse (cut + `kBreachBeaconArcMargin_m`) RAY-CAST onto the arena
ceiling surface from the planet centre — the old `ell_point` frame-parameter projection distorts
by ~km on the oblate shoulder.

**PROBE NUMBERS (bare-sphere canon, arena_a_m=4200):** true crossings at arc **4034.5 / 8089.4**
(nodes 26/56, radii 13605.9/13595.1) vs the old holes at 3267/8761; legs E 4034.5 / M 3954.2,
crossing 4054.9 ⇒ centering skew 2.0%, crossing/mean-leg 1.015 (T13's centering holds under the
honest locator).

**TESTS (753/753 green):** NEW **T14 leg** at the SHIPPED arena_a_m=4200 (the fixture canon is
7350, where the old rule accidentally near-worked): locator sits ON the wall (|sd| < 1), within
one segment of its node, and — THE regression pin — the bore's actual tube-wall lines are walked
through the ellipsoid (bisection on the same sd) and EVERY true-opening rim direction must be
inside the cut hole (old locator: every rim fails; dropped stretch: oblique rim ends fail). The
T11 suppression leg rewritten two-sided: (a) no tube vert materially inside the arena (wall
across open air) AND (b) an emitted vert within ~one node spacing of EACH breach (the wall-less
tube class). T11.1 beacons: every arena beacon's DIRECTION on its ring ellipse around the TRUE
axis (angular bound — config-relative, oblique-projection-proof) + full ring per breach. All
three manifold verifiers + both poke-mutant legs re-keyed from the radius rule to
breach-position distance + the shared padded predicate.

**SMOKES re-shot (2026-07-21):** `bore_3600.png` = from inside the Errington bore the arena
reads THROUGH the breach — ember-lit room, and the MURRAY bore's lamp lines visible clear across
through the FAR hole (round 10: opaque wall). `cavern_breach.png` = the breach look-target now
aims at the true crossing. `mouth_errington.png` / `bore_1000.png` regression-clean.
`docs/tunnel_visual_audit_report.md` (untracked, claimed all-clean pre-fix) DELETED — it was
contradicted by the fly + the scout probe.

**RED-TEAM (fresh context, 2026-07-21): SOUND-WITH-FIXES, all folded.** P1: the "stretch:=1
fails the rim walk" mutation claim was FALSE at 4200 (max rim ~255 m < the 275 m round hole —
the false-mutation-banner class again) — folded as a SECOND rim-walk arm at the 7350 fixture,
where stretch:=1 measurably fails (3/24 E, 6/24 M rim directions outside, rims 288/309 m).
P2 folds: mouth-inside-arena suppression assert (a suppressed node 0/n-1 would hand the collar
builders an empty end ring), beacon ray-scan start now config-relative
(`max(R_local, arena_apex_r) + 200`), near-radial beacon ring rounds to match the round-hole
predicate fallback. P2 WATCH (fly card 1): the oblique cut is ~2.9x the physical opening
(~1400x550 m gash; the canon 2.5x margin policy applied to the slant) — ~460 m of
see-through-but-solid rock at each long end inside the lit ring; ready lever named on the card.
Red-team confirmed clean: two-half seam (a_pos==a_neg identically), single contiguous
suppression run (worst suppressed node 21.8 m INSIDE), no ellipsoid_sd fork/ODR, predicate
math ~0.1% error, cut-vs-lamp consistency at canon. Post-fold gate 754/754.

**CARRY (Chad's 2026-07-21 art rulings, NEXT RUNG — not in T14):** both entrances read as
"black ant mounds": MURRAY = a proper Blender open-pit look, then the tunnel at the pit floor;
ERRINGTON = DELETE the T13 sinking headframe, replace with a portal frame (framed adit) + tunnel,
not all black. Campaign-map canon (Azilda → Sudbury faction etc.) lives in MASTER_PLAN §2.5.

**ROUND-11 FLY VERDICTS (Chad, 2026-07-21): "flew all the way through... a good experience"
with two findings:** (1) "before I enter the murray tunnel from the west, I can see through the
ground to the surface"; (2) "a blind entrance to get into the murray tunnel as its black and I
can only see one light going down, caused me to crash" (+ "exiting is a little blind" minor).
Both fixed same-day by T14b + T15 below.

### T14b + T15 — SEAL THE SEE-THROUGH + LIGHT THE MURRAY PIT (2026-07-21, fly-11 folds)

**T14b — GASH TRIM:** the cut semi-axes now live IN the Breach (`hole_minor_m`/`hole_major_m`,
baked by `breach_points`; `breach_hole_hit` drops its hole_arc param). Major WAS
`stretch·hole_arc` (~707 m, 2.9× the physical opening); now the PHYSICAL oblique extent + the
canon absolute margin: `max(minor, stretch·tube_height + (hole_arc − tube_width))` (~399 m at
ship; degrades exactly to the round hole as stretch→1). Beacon rings follow the baked axes.

**T15 — THE BREACH COLLAR (the real see-through kill):** ROOT CAUSE of finding (1): the cut
hole is wider than the tube's silhouette BY DESIGN (margin + facet granularity), and the rock
between the arena wall and the terrain is NOT geometry — terrain reads backface-invisible from
below, so every sightline through the hole margin escaped to the surface (trees, sky, mouth
glows), and underground lamp glows bled outward the same way (probe: street-lamps-off A/B
identical ⇒ not a lamp-pass bug; ray probe: **46 leaking sightlines** per the old geometry).
Trimming can shrink but never seal a margin that must exist. THE SEAL: an annulus funnel per
breach — inner ring = the tube's breach-mouth ring EXACT floats (new `TubeEnds.ring_bE/ring_bM`;
the mouth-collar no-crack discipline underground), outer ring = the cut rim + `kBreachCollarPad_m`
(160, covers the ragged any-corner facet edge) ray-cast onto the arena wall via the SHARED
`arena_surface_point` (extracted; the beacon rings use the same — no projection fork), nudged
5 m INTO the arena (no z-fight; keeps the T2 on/inside-the-net invariant — an outward nudge
failed T2 at +5 and was flipped). Appended to the ARENA piece (kCavern shading = the wall
continues into a lit throat; no piece-count change; inner-ring azimuth matched about the ring's
own CENTROID — the ring sits up to a node spacing off the breach axis, so axis-azimuths would
fold onto a lobe). **T14b-MURRAY BOWL FUNNEL:** finding (2) — the pit had NO lamps above the
bore mouth ring 300 m down. Two beacon rings now light it: RIM (12, at the surface — approach
outline + the climb-out cue) + MID-DEPTH (8, on the funnel wall), own bright-family tier
`kBowlBeaconIntensity` 2.5 (distinct from breach/mouth 3.0 so every exact-tier test stays
honest); `bowl_depth ≤ 0` ⇒ structurally absent. Smoke `murray_bowl.png`: the pit reads as a
beacon-ringed funnel.

**TESTS (756/756):** NEW **T15 leak kill-test** (Möller–Trumbore over the tube+cavern tris,
13×9 ray grid per breach spanning hole+rim from an in-arena eye): a ray reaching 700 m above
the crossing radius with no hit AND outside the net volume = a LEAK; REQUIRE 0.
Mutation-verified live: collar disabled ⇒ **46 leaks caught**, restored ⇒ 0. NEW T14b bowl-lamp
leg (count 12+8 on the own tier, every lamp inside the bowl cylinder between grade and floor,
tier distinct, `bowl_depth=0` ⇒ 0 lamps). T11.1 ring bound reads the baked `hole_major_m`.
`SEADS_TUNCAM_MOUTH` gained a 4th token `murray`; `SEADS_TUNCAM_CAVERN` look `breach2` targets
the Murray crossing.

**ROUND-12 FLY VERDICTS (Chad, 2026-07-21): "flew all the way through" with two findings:**
(1) "an opaque wall I flew through going into the chamber"; (2) "a hole near Errington where I
can see through to the other side of the earth" (+ "similar inside the chamber near the Murray
tunnel exit"). Both fixed by T16 below (alongside the stage-1 render culling + SDF broad-phase
and the stage-2 Errington portal / Murray benched pit / daylight exterior ambient / exit throat
rings that landed the same round).

### T16 — THE FLY-THROUGH WALL + THE ERRINGTON SEE-THROUGH (2026-07-21, fly-12 folds)

**DEFECT 1 — THE FLY-THROUGH WALL (render cut from collision truth).** ROOT CAUSE: the arena
RENDER wall (a lat/long ellipsoid grid, facets dropped inside an independently-computed angular
ellipse `breach_hole_hit`) and the COLLISION opening (where the swept bore actually pierces the
hole-free arena ellipsoid) were TWO representations — so a rendered facet/collar could stand
over collision-free air (fly-through) and the T15 collar, a mouth-ring→padded-rim funnel, hung
across open bore air on the OBTUSE side of the oblique crossing. FIX (render-side, single
source): `world::TunnelNet::tube_signed_distance()` exposes the swept-tube SDF (a pure refactor
of the tube term out of `signed_distance_scan` — `signed_distance` is BIT-IDENTICAL, goldens
unmoved). `build_arena` now drops a facet iff a corner reads `tube_signed_distance < 2 m` (the
true collision opening, bounded to a 2500 m breach neighbourhood so the far ceiling pays one
length check). `build_breach_collar` REBUILT as a two-phase seal that never overhangs open air:
PHASE 1 marches the tube's breach-mouth ring along the bore `into` the arena, CLAMPING each
vertex that crosses into open air onto the wall (so it follows the real tube wall in rock and
bends onto the arena wall AT the opening); PHASE 2 pushes that rim outward by
`kBreachCollarPad_m` to cover the ragged any-corner facet edge. Collision (world/tunnel_net.cpp)
was NOT changed except the pure tube-SDF extraction.

**DEFECT 2 — THE ERRINGTON SEE-THROUGH (trench walls).** ROOT CAUSE: the T13 approach-trench
open cuts drop terrain triangles but had NO covering geometry, so a sightline through the
backface-invisible cut floor escaped to space; the "similar Murray-exit chamber hole" is the
breach class fixed by DEFECT 1's collar. FIX: `build_trench_step` drapes each live trench Bowl
— a terrain-hugging rim band out to `collar_reach` (new single-source `collar_reach_from_cell`),
a vertical wall to the flat floor, and a floor disk — on the bowl_sd cylinder (visible == the
survivable volume). Its OWN piece + OWN `PieceKind::kTrench` (daylight `kSkirtVal`, EXTERIOR,
draw-gated like a collar) so the "two mouth collars" / mouth-seam / band-hug structural pins
stay exact; appended before the portal so the portal stays last.

**THE WIDENED PROBE (the round's proof).** The round-11 kill-test cast from ONE eye per breach
so it provably never covered the Errington surface / trench / general viewpoints — round 12
found holes after its "46→0" banner. NEW `T16 leak audit` casts dense ray fans from ~30 eyes
(surface above both mouths + the trench at 300/1500 m + 2 oblique; interior arena-centre, each
breach + 2 offsets, in-tube-before-Murray, in-Errington-pit) and defines a LEAK by collision
truth: a sightline through SOLID ROCK (underground AND `!net.contains`) reaching open sky, with
an analytic front-facing-terrain occluder (uniform sphere MINUS the cut disks) so a grazing
surface ray that would hit real ground is not a false leak, and legit open-cut exits (up the
pit/mouth/tube, never touching solid rock) excluded by construction. **Result: 0 leaks over the
final geometry.** MUTATION-VERIFIED live: drop the trench walls ⇒ surface eyes leak (`>0`);
drop the breach collars ⇒ interior breach eyes leak (`>0`); restored ⇒ 0. NEW
`T16 render-vs-collision`: from up-tube flight eyes + arena-interior view eyes, the render hit
never sits out in open rock (`sd < 3`), head-on flight render/collision agree `< 8 m`, and NO
flight ray hits a render tri with open bore air `> 10 m` behind it; MUTATION-VERIFIED (a collar
tri shoved 60 m into open air ⇒ the fly-through count goes positive). Möller–Trumbore reused
with a bounding-sphere broad-phase; `kCavern` grid-vs-collar tris split by the grid vertex
count. **Gate 763/763** (from 761; +2 probe tests; goldens/SDF tests unmoved — the tube-SDF
extraction is bit-identical). Smokes (scratchpad `t16_*.png`, night, Read-verified): Errington
pit+trench+portal read as solid excavated earth (no stars through the ground); the chamber
head-on at the Murray breach is a sealed ember-lit shell with a beacon throat (no
backface/starfield); the bore→arena sightline is a solid lamp-lined corridor.

**★ NOW: CHAD FLIES ROUND 13 — `docs/tunnel_fly_cards.md`** (card 1: the Errington portal +
sealed cut; card 2: the full run + the fly-through wall + the exits; card 3: perf / the
round-12 choppiness; card 4: regression). Kill-switch: `[tunnel] enabled = false`.

# T17 — BOTH ENTRANCES OPENED (fly-13 fold, 2026-07-21 late)

**THE FINDING (Chad, round 13): "both entrances are occluded I could not fly into either."**
Certified by fresh TUNCAM smokes before touching code: Errington read as a pale SLAB over the
whole approach; the Murray floor mouth was plugged by a black dome — and the dome is plainly
visible, unremarked, in the committed round-12 certification smoke (murray_bowl_t16.png). A
smoke certifies only what its caption names.

**ROOT CAUSES (three, all render-side; collision untouched).**
1. **The T16 trench drapes roofed their own channel.** The three trench steps are heavily
   overlapping cuts (150 m rims, 100 m spacing, abutting the pit); each T16 drape emitted a
   FULL 360-degree wall + floor disk, so every step's drape rendered across its neighbours'
   open volume — the slab.
2. **The Murray bore crown stood above the pit floor.** The bore leaves the floor inclined;
   its crown for the first ~2 node spacings sits INSIDE the bowl's open collision cylinder
   (T6-P0) — rendered rock on flyable air, the dome's dark core.
3. **The funnel tented onto the mouth ring (since T4a).** build_bowl_wall stitched its last
   HORIZONTAL circle 1:1 by azimuth index onto the near-VERTICAL tube end ring — index-matched
   azimuths point in wildly different 3D directions, so the band TWISTED: it tented over the
   arch at every azimuth and swept across the bore interior below the floor. Rounds 11/12 flew
   THROUGH this visual-only surface blind (the "blind entrance/exit" reports); T16's daylight
   benches exposed it.

**THE FIX (all in render/tunnel_mesh.*, one world-side export).**
- `world::bowl_sd` exported (H1: the render trim reads the SAME open-cut volume collision
  flies; precedent `ellipsoid_sd`).
- **THE CUT-SWALLOWED TRIM** (`drop_cut_swallowed_tris`, flag `cut_swallow_trim` on
  `build_tunnel_mesh`, TRUE shipped / false = the test-only kill lever): a facet of a piece is
  dropped when swallowed by an open-cut volume in that piece's EXPLICIT cut list. Per-piece
  scoping is load-bearing (audit-measured): tube/floor yield ONLY to the Murray bowl (the
  Errington bore is a DESIGNED recess adit — yielding to the pit gutted the first bore segment,
  79 pit-eye leaks); each mouth collar yields to everything EXCEPT its own cut (its
  funnel/benches live inside their own cylinder by design; its rim band floated over the
  trench); the trench yields to every cut. Border facets are SUBDIVIDED (depth 2, mixed leaves
  KEPT) — any-vertex dropping opened rock-void slivers at every cut junction (5–9 leaks per
  trench eye), fully-inside-only dropping left the lies standing; mixed-leaf-keep can over-cover
  by a leaf but can never open a void.
- **THE BORE-SWALLOW TRIM** (`drop_tube_swallowed_tris`, mouth pieces): no mouth facet may sit
  inside the TUBE volume (`net.tube_signed_distance`, the T16 single source). Samples verts +
  edge midpoints + centroid: the bore is convex, so on-surface-cornered facets can still CHORD
  through it (probe-measured on the swing-in ray).
- **THE MOUTH ADAPTER RING** (`emit_inner_via_adapter`, both bowl arms): the funnel's final
  band now routes through an adapter — the tube ring's verts mapped to their own FOOTPRINT
  azimuths about the bowl axis on the last analytic circle (recessed 0.75 m rock-side:
  coincident circles weld a 3-surface junction non-manifold). prev→adapter cannot twist
  (same circle); adapter→inner runs radially (apron below the floor, side walls, back drape —
  nothing over the opening). The tube seam keeps EXACT floats.
- **THE MURRAY FLOOR DISK** (benched arm): flat rings at bowl_depth+0.5 close the floor around
  the half-buried bore; the bore-swallow trim cuts the OPEN-CUT SLOT through it — open pit,
  then a slot, then tunnel. The trench floor fan was likewise refactored into rings (a full-disk
  fan shares its centre vert with every triangle — one swallowed centre would erase the whole
  rock-backed disk).
- **THE PIT CUT SLEEVE** (`build_pit_sleeve`, Errington): the pit's T6-P0 collision cylinder
  had NO rendered boundary (the cone floats inside it), so opening the trench-over-pit roof
  exposed the shell between cone and cylinder straight to rock void (the last 5 audit leaks).
  The sleeve renders the cut's own wall + floor annulus once, behind the cone; the bore-swallow
  trim opens the adit through it.

**THE DUAL PIN (the audit's blind spot, closed).** The T16 leak audit rewards SEALING; nothing
rewarded OPENING — zero leaks was achieved in T16 partly by walling off the two designed
entrances. NEW always-on `T17 entrances open` kill test: the Murray dive fan (ring-frame
targets) + the swing into the bore + the Errington per-step channel sightlines + the
bore-tangent entry line must all be clear of greybox; the SAME paths against a
`cut_swallow_trim=false` mesh must be BLOCKED (dive fan by the tent, crown dives by the dome,
channel by the drape roof, entry line by the slab) — the open-pins can never go vacuous.
Audit WARNs are now pass-tagged (`audit mode N`) — during this round the un-tagged mutation-arm
WARNs (no-trench: 58–71 leaks/eye, BY DESIGN) read as main-pass leaks and cost a long ghost
chase.

**Gate 764/764** (763 + the T17 entrance test; goldens unmoved; collision bit-identical —
`bowl_sd` was hoisted to world scope unchanged). Structural pins updated honestly: the seam
pins LOCATE the exact-float ring instead of assuming tail position (the trims append
subdivided leaf verts); the benched ring roster is 2·Nb stairs + adapter + seam + 4 disk rings
+ 1 centre vert; T5d's band scan starts at rim+2 (the sleeve legitimately descends AT the rim
arc); T3's per-ring modulo allows the one disk-centre vert. Smokes (err_t17_az0/az180,
mur_t17_az0/overhead, Read-verified): the Errington trench ends at a LIT ARCH under the portal
canopy; the Murray pit floor shows an open slot with the throat-ring ladder visible inside.

**â NOW: CHAD FLIES ROUND 14 — `docs/tunnel_fly_cards.md`** (card 1: the Errington
entry; card 2: the Murray dive + the named flap residue; card 3: both directions; card 4:
regression). Kill-switch: `[tunnel] enabled = false`.

# T18 — THE PIT IS A REAL SPACE (fly-14 fold, 2026-07-21 late)

**THE FINDINGS (Chad, round 14):** tunnels good; "leftover artifacts where the tunnel enters";
"the murray mine pit has to be a real space. I collided into nothing escaping the pit. It
still thinks there is ground there"; "stretches of mesh across the murray entrance and the
errington entrance"; plus the Errington history/Blender commission (brief researched in
parallel: docs/errington_entrance_art_brief.md — Treadwell Yukon copper-zinc, 1924-31, NOT
Mond nickel; ranked photo ledger; blockout spec; 6 open questions).

**ROOT CAUSES (probe-measured, in discovery order).**
1. **The bore's air DEAD-ENDED at the Murray mouth ring.** The terrain heightfield keeps full
   surface height over the pits, so below grade a pilot lives exactly where net.contains says
   — and in front of the inclined arch, below the pit-floor plane, nothing contained. An
   at-or-below-centreline exit crossed the ring plane into instant ground contact.
2. **The pit collision was the T6-P0 CYLINDER; the drawn pit is a 46-degree benched funnel.**
   Everything under the staircase was invisible open collision: any climb-out shallower than
   the benches (i.e. every realistic one) rode under the drawn wall and died at the unrendered
   cylinder boundary. The T6-P0 rationale (cylinder == removed-terrain volume) predates the
   T16 benches + T17 sleeve actually RENDERING the walls.
3. **The funnel band over each arch (the round-14 "stretches of mesh")** — the T17 adapter
   band's above-centre facets wrapped the arch like a sail.

**THE FIX.**
- **THE MOUTH THROAT** (world, `TunnelNet::throat_*` + `throat_signed_distance`): one virtual
  segment of the bore's own swept ellipse carried past the Murray ring along the exit tangent
  (same eval the tube flies), with a RAMP floor (bore-floor at the ring rising to the pit
  floor) and a HARD CAP 2 m above the pit-floor plane — the throat is the BELOW-FLOOR WEDGE
  only; above it the bowl cone owns the slot column. Uncapped it poked out through the drawn
  staircase (83 m of open collision behind a drawn bench, pin-measured). throat_on=false =
  structurally absent (the T18 kill lever). Consumed by collision, atm, and the floor-disk
  slot trim — one shape.
- **THE VISIBLE-CONE COLLISION** (world, `Bowl.open_floor_r`): bowl_sd's side boundary tapers
  bowl_r -> open_floor_r (Murray = the staircase floor edge via kMurrayBenches — CANON IN
  WORLD, render's kBowlBenches now consumes it; Errington = its smooth-cone floor). The cone
  runs exactly between the staircase's inner and outer corner lines: collision within one
  bench tooth of the drawn wall, never through it. open_floor_r=0 keeps the cylinder (trench
  steps — their drapes render the full cylinder).
- **THE ARCH-BAND TRIM** (render, `drop_arch_band_tris`): mouth-piece facets touching a ring
  vert above the ring-CENTRE shell drop at both mouths (Murray: the slot's tube walls seal
  behind; Errington: the T17 sleeve does). Floor-keying it at Errington dropped the crater's
  cone-to-floor wrap too (20 audit leaks under the sleeve's floor annulus) — the ring-centre
  key is the audit-derived correction. Border-subdivision depth 2 -> 3 (halved leaf flaps).
**THE PINS.** NEW `T18 escape survivable`: exit-path fans at BOTH mouths (above/below
centreline, 5-45 degrees, azimuth spread) — no path may die below terrain outside the net
unless a RENDERED surface sits within 30 m (dying at a drawn wall is honest; "collided into
nothing" is the bug); plus the below-floor exit corridor volume-pinned OPEN with the throat
and MOSTLY-ROCK with it off (the lever). Premise updates, re-derived not weakened: the T6-P0
corridor walks define "visibly open" by the RENDERED cone (was: cylinder); the T16 benched
pin now bounds every referenced wall vert within one bench tooth of sd=0 BOTH WAYS (was:
inside-only — the inversion is the point: walls are rock now). Gate **765/765** (+1); goldens
unmoved; the kernel untouched (world change = the net's own volume, exactly the seam the
tunnel feature owns). Smokes: mur_t18.png (open slot, throat lights, flaps halved),
err_t18.png (clean dark arch under the portal, sail gone).

**★ NOW: CHAD FLIES ROUND 15 — `docs/tunnel_fly_cards.md`** (card 1: the escape that killed
him; card 2: artifact check; card 3: the Errington history DECISIONS from the art brief;
card 4: regression). Then the Blender hero entrance build on his card-3 rulings.

# T19 — THE 1931 RUINS + THE MURRAY BLEND (fly-15 fold, 2026-07-21 night)

**THE RULINGS (Chad, round 15):** through-flight approved BOTH ways (T18 signed on the
stick); entrance art = "make it look like it was in 1930 ruins" (the ghost-ruin state — the
art brief's lead question answered); "the murray bowl is good just needs a clean up. Make the
opening blend in to the scene"; NEXT = the bubble location; a handoff doc when he asks.

**BUILT (an Opus builder agent under Fable review — Chad's token ruling).**
- **THE ERRINGTON 1931 RUINS** (`build_errington_ruins`, folded into the kPortal piece,
  same headframe_on gate, portal stays the LAST piece): a raking ruined gallows headframe at
  headframe_h with one leg snapped at 55% + fallen half-sunk members, a roofless staggered-
  height rockhouse shell, an ore-car trestle (2 posts collapsed) running to a 4-mound
  waste-rock dump fan — 36 closed boxes, ~432 tris, placed in the net.pit/pit_up_tangent
  frame (no magic coords). Weathered read via per-vertex CUE (kRuinVal 0.42 ~ silvered
  timber vs kPortalVal 0.75 concrete — no FS change).
- **THE MURRAY BLEND** (`darken_cue_near`): mouth-piece cue scales toward 0.35 within
  2.2 tube-widths of the Murray mouth (1.6 at the Errington adit) — the surround fades into
  the slot instead of butting pale-on-black; border-subdivision depth 4 at the two MOUTH
  collars only (flaps halved again; mixed leaves still never dropped — coverage holds).
Pins moved honestly: the portal cue check is now two-family (frame 1.0 + ruin 0.56, both
non-vacuous) with a config-relative ruin reach bound; the T5d vertex-budget sanity 6000 ->
12000 (depth-4 leaf duplication; u16 assert still owns the hard cap). Gate **765/765**
(Fable re-ran independently); T16 audit / T17 open / T18 escape UNCHANGED; smokes
re-taken + Read-verified by the parent: the headframe silhouettes the skyline over the lit
adit; Murray's opening blends. Known read (for a later world-thread call): the ~40-50 m tree
scatter runs to the pit rims, so the rockhouse/trestle read as glimpses among trees — apt
for a reclaimed ghost mine; a tree-exclusion margin around the ruins is the lever if Chad
wants the mill bolder.

**T20 HOTFIX (fly-16 first word, Chad: "errington is impregnable!").** Two T19-adjacent
reads sealed the LOW trench-line approach (el ~10): the portal CANOPY at 1.3-rw was a ~143 m
slab that roofed the arch from shallow angles, and the T19 darkening — which Chad only asked
for at MURRAY — had erased the Errington adit's contrast under it: together the entrance
read as a closed building. FIX: kPortalCanopyReach 1.3 -> 0.45 (a short brow; posts + lintel
stay the frame) and the Errington-side darken_cue_near call REMOVED (contrast IS the
Errington read; Murray's blend untouched). Low-approach smoke re-taken + Read-verified: the
beacon-ringed arch dominates from trench-line height. Gate 765/765 (the canopy bounds pin is
config-relative and followed).

**★ NOW: CHAD FLIES ROUND 16 — then THE BUBBLE LOCATION (the R6 AtmosphereField campaign
placement: Errington at the Chelmsford bubble's proximal east edge, Murray at the Sudbury
faction's proximal bound by Azilda), handoff doc on request.**

# T21 — THE ENTRY-LINE REVERT + THE STRETCHY ARMS (fly-16 fold, 2026-07-22)

**THE FINDINGS (Chad, round 16):** "I died a few times going into the errington tunnel when
I shouldnt have, but something invisible is there" + "weird artefacts like stretchy arms at
mouth of murray."

**ROOT CAUSES + FIXES.**
1. **The Errington deaths were the T18 pit CONE — reverted to the CYLINDER.** The cone was
   uniformity, not a measured fix: it strangled the descending ENTRY line (a pilot sagging
   below the trench-floor plane while lining up the arch's lower half crossed the cone
   boundary — radius ~120 at those depths vs the 143 rim — into invisible rock). Unlike
   Murray, the Errington cylinder IS rendered (the T17 sleeve draws its wall + floor
   annulus), so cylinder collision dies at a DRAWN surface. `net.pit.open_floor_r = 0`;
   Murray keeps its measured visible-cone. COVERAGE GAP CLOSED: the T18 fans only flew
   OUTBOUND — the T21 INBOUND fans (27 descending entry lines per mouth, mouth-specific
   honest starts: the trench channel at Errington, above-the-rim dives at Murray) now pin
   entry the same way (lethal-on-nothing == 0 both mouths).
2. **The stretchy arms were the adapter's phase-shear ribbon at the floor plane.** The
   uniform berm circle stitches to the footprint-WARPED adapter circle; where the warp is
   large the stitch quads stretch tangentially — at 0.75 m recess they lay in the floor
   plane and read as pale spokes radiating around the mouth. The adapter now sits 6 m DOWN
   and 4 m IN: the whole ribbon (and the pi-band roots) tucks under the floor disk.
   Close-up smoke re-taken + Read-verified: the long spokes are gone (small dark slivers
   remain at the beacon line, an order smaller).

Gate **765/765**; goldens unmoved; Murray collision untouched; the leak audit / T17
entrances / T18 escape all green (T18 now includes the inbound fans).

# T22 — THE THROAT RAMP FACE (fly-16 fold cont., 2026-07-22)

**THE FINDINGS (Chad):** "I died seemingly unfairly in the murray bowl trying to fly back in
I was far from anything and coming down right down the pipe hit something invisible" +
Errington "the way in is fine its just a bit unclean and obstructed."

**ROOT CAUSE (Murray).** The floor disk is trimmed over the THROAT's footprint as well as
the slot, so the visible dark opening in the pit floor spans BOTH sides of the mouth ring —
but the front half's lower boundary is the throat's collision RAMP (T18), which had NO drawn
face: a steep descent into that half stopped 40-60 m above the visible tunnel floor, on
nothing. FIX: `build_throat_ramp` draws the ramp — kThroatRampRows x kThroatRampAcross rows
exactly 0.4 m proud of the throat's interpolated floor shells, as wide as the throat's floor
chord per station (the T5a chord law), appended to the bowl piece before the darkening (it
inherits the mouth blend) and before the trims (on-boundary, nothing eats it). The T18 test
gained STEEP-DIVE rows into the front half of the strip (9 near-vertical lines); the bowl
count pins derive the +rows*across delta from the new header constants. Errington: the
trench drapes get the depth-4 flap shrink ("a bit unclean"); the card asks Chad to POINT at
the remaining obstruction. Gate **765/765**; down-the-strip smoke Read-verified (the throat
lamps now sit on a visible rising floor).

## Coordination

- The world agent runs **R7** (escape telemetry + bait predicate) on
  `sandbox/fields-forge` in parallel — different files; this branch merges into
  fields-forge on Chad's greybox fly ruling, resolving trivially (the tunnel is
  additive: new modules + env field + render).
- Commit by explicit path (the shared-repo discipline); never touch the ballistics/sky
  loose files.
- Budget discipline: subagent coders, ONE spec-fidelity red-team per rung, instruments
  over debate (the v4 lesson set — see `chad-spec-backward-no-walls`).

# T23 — THE JUNCTION SEE-THROUGH (round-16 tail #2, 2026-07-22 overnight)

**THE FINDING (Chad):** "I went straight down my mine and died from something invisible.
I can see through the bottom where the murray pit meets murray tunnel" — CONFIRMED still
present post-T22.

**ROOT CAUSE (probe-measured, not the suspected adapter sliver alone).** The bore/throat
trim (`drop_tube_swallowed_tris`) dropped WHOLE facets — at the floor disk's ~40 m ring x
azimuth facets the slot cut overshot by up to a facet, and since T21 tucked the adapter
bands 6 m down + 4 m in, NOTHING rendered behind the overshoot beside the slot walls. Only
a near-VERTICAL sightline threads the gap (the T18 fans flew 70-80 degrees; his dive was
90): a dedicated one-vertical-ray-per-point annulus probe over the pit floor measured **67
see-through leaks** on the shipped mesh.

**THE FIX (single source):** the tube/throat trim now SUBDIVIDES mixed/chording facets
(depth `kMouthSplitDepth`, the same discipline as the T14b cut trim) and drops only
fully-swallowed leaves — voids impossible, over-cover allowed. 67 -> **0**.

**KILL TESTS.** (1) "T23 Murray floor junction": the dense vertical fan REQUIREs 0 on the
shipped mesh; the load-bearing arm rebuilds with `tube_trim_split_depth=0` (the legacy
whole-facet drop, a test-only lever on build_tunnel_mesh) and REQUIREs leaks — that arm IS
the round-16 defect. (2) T18 gained PURE-VERTICAL dive rows (junction annulus + strip
stations, each down its own local up); their honest mutation is the whole MURRAY MOUTH
PIECE undrawn (11 deaths) — two sharper arms were MEASURED non-lethal and rejected (the
whole-facet mesh: every junction death sits within 30 m of some drawn surface, its defect
is see-through and belongs to the leak probe; throat-off: the T22 ramp face keeps those
stops honest). (3) The T16/T3/T5d count pins were reformed against a new
`TunnelPiece::trim_base_verts` mark (roster pinned exactly; the leaf tail in unshared
triples; T5d's size budget bumped 12000 -> 30000, measured + named).

**HONESTY LEDGER.** Two traps hit and fixed IN the tests: (a) the T18 vertical rows first
ran from grade+300 — the harness's "escaped" check (grade+30) returned false on the FIRST
sample and every arm passed vacuously (the fixture-no-op class; rows now start at
grade+25); (b) tube_signed_distance is DISCONTINUOUS across the bore's end-cap plane
(adjacent leaf verts 2.5 m apart read +1.9 vs -87.4), so the per-vert |sd| bound is wrong
for leaves — the leaf pin is now the trim's actual contract (no fully-swallowed kept leaf,
float-roundtrip eps slack).

**DIAGNOSED, NOT CHANGED (Chad's look call):** the pit-floor disk ROOFS the throat's far
strip (drawn floor over collision-open air, t_hit == disk at stations 50-150 m out) — a
PRE-EXISTING T17-era quirk, identical on the legacy mesh, invisible to entries near the
mouth ring. Opening it (a smaller trim eps for the disk-vs-throat-cap relation) would cut
a long visible gash across the approved pit look — round-17 CARD 4 asks.

Gate green; goldens unmoved; collision untouched.

# T24 — THE FLOATING RIBBONS + THE PALE STACK (round-16 tail #3, 2026-07-22 overnight)

**THE FINDING (Chad, the right flank):** "there is the snowmachine trail and another line.
There is almost overhang there... several same-brightness pale surfaces."

**(a) THE RIBBON CLIP.** The draped road/trail ribbons are built on the heightfield but the
terrain over the Errington pit + trench is CUT — the lines hovered across the void. Fix at
the single source: `ribbon_indices_outside_cuts` (render/ribbon_clip.h, header-only +
raylib-free so ctest pins the shipped function) drops every triangle with ANY vertex inside
ANY cut disk — the IDENTICAL `dir_in_any_cut` metric and any-vertex rule `fill_face` drops
terrain triangles with (now exported from sphere_param, one definition). The app threads
the SAME `planet_cuts` list the terrain + tree scatter consume (RibbonBuildParams.cuts).
KILL TEST "T24 ribbon clip": empty cuts == the baked list verbatim per path; with the canon
tunnel cuts every kept tri is fully outside AND a nonzero count drops (a never-firing clip
is theater). Smoke-read: the black trail now ENDS at the excavation edge.
NOTE (pre-existing fork, out of scope): world/props.cpp's tree-scatter cut check is its own
copy of the arc<radius rule (world/ cannot include render/) — same math, layering debt.

**(b) THE VALUE SEPARATION.** The stacked pale layers right of the arch (trench drape, pit
sleeve, collar band, portal wing — all at kSkirtVal 0.58 except the portal) now separate:
the SLEEVE is a notch darker (cue x0.8 at build — per-vertex cue multiplies the FS value,
so one piece can carry two layers), the PORTAL frame/wings a notch brighter (kPortalVal
0.75 -> 0.82). The mouth is NOT re-darkened (T20). Daylight read = Chad's round-17 card.

# T25 — THE STUTTER ATTRIBUTION NET (round-16 tail #1, 2026-07-22 overnight)

**THE FINDING (Chad):** "I had stuttering a couple of times in the tunnel and also while
shooting in the sky." Per the T16 lesson: MEASURE FIRST — nothing was "fixed" blind.

**THE INSTRUMENT.** `SEADS_PROF=1` arms a per-pass frame profiler: the app (which owns the
clock — render/ reads none) laps every named draw pass via a new
`FrameInfo::prof_mark` callback (nullptr = zero overhead, the default), plus one
"app_tick" lap for input + sim + combat + audio. A frame whose total exceeds
max(2x rolling 120-frame median, 20 ms) prints ONE `[PROF] ... SPIKE` line with the worst
laps. Round-17 CARD 3 asks Chad to fly with it armed — his fly IS the measurement; the fix
is the next session's first rung, keyed to whichever bucket owns the spikes (the handoff's
suspects: mouth-piece raster, additive lamp glows, or the combat pipeline — the sky-shooting
half likely belongs to the ballistics thread).

## T23/T24/T25 red-team ledger (fresh-context adversarial review, 2026-07-22)

Verdict: **no P0/P1**; the split_depth<=0 bit-identity claim verified TRUE against HEAD
(same 7 probes, same eps, identical drop set; 67-leak mutation arm re-measured live), T17
entrance margins measured huge vs the possible leaf protrusion (entry-line nearest hit
2576 m vs the <=5m+leaf bound), T24's edge-chord hole bounded at ~2 m (ribbon densify
<=80 m vs 370-450 m cut radii) and IDENTICAL to the terrain's accepted chord hole.
FOLDED SAME SESSION:
- **P2-1**: `drop_arch_band_tris` now runs BEFORE the subdividing trims at both mouths —
  it matches hot ring verts by EXACT FLOAT identity, and subdivision midpoint leaves are
  new unshared verts that can never match (sub-facet sail fragments off the arch, the
  "stretchy arms" class at 1/2-facet scale). Order was irrelevant under whole-facet
  trims; it is load-bearing under subdivision. Smoke re-read: the dark inter-lamp
  fragments at the Murray mouth thinned visibly.
- **P2-2**: the T23 fan's outer radius was a welded 210 — now `1.3 * floor_edge_r`
  derived by the benched wall's own law (the AT-15 silent-disarm class).
- **P3-1**: the trench piece's `trim_base_verts` mark is set (the header contract said
  "0 == no leaf-appending trim" and the trench received one unmarked).
NOTED, NOT CHANGED: P3-2 (a theoretical T16 leaf-pin flake window where a leaf vert
straddles the end-cap sd discontinuity within mm — exclude cap-plane verts if it ever
flakes), P3-3 (u16 headroom 33%/21% assert-guarded only under NDEBUG — ship is Debug),
P3-4 (SEADS_PROF arms on presence, not value — repo convention).
