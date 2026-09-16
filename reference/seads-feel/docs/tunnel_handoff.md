# TUNNEL-FORGE HANDOFF (updated 2026-07-23: T26 SEAL PASS CERTIFIED by Chad + T27 landed)

## ★ ROUND 18 FLOWN (Chad, 2026-07-23): THE SEAL IS CERTIFIED — "yep great work on the
## seal... that is really all I found." T26 pushed (1eae41ae8). His ONE new finding is
## FIXED as T27 (landed same day): "a ceiling of invisible collidable dirt coming out of
## murray — a puff of smoke and lose 70-90 m/s." ATTRIBUTION: the terrain heightfield
## keeps full grade over the drawn-open mouths and the open-cut SDF was capped exactly AT
## grade, so sim::ground_contact's r_s = radius_at + contact_height_m (2.45) shell stayed
## "ground" over the open mouth — a climb-out was graded a GENTLE LANDING (radial velocity
## ZEROED = the 70-90 m/s) + wing_strike smoke (sim/ground.h; the kernel was NOT touched —
## the fix is the tunnel's own volume). FIX: Bowl::lip_cap_m = 6 m raises the open-cut
## ceiling above grade over every drawn-open cut (Murray bowl, Errington pit, trench),
## collision-only, mesh-bit-identity GUARDED in the T27 leg. INSTRUMENT: the T27 inverse
## census (exit-corridor grab-shell sweep, both mouths, same predicate the app consumes:
## !net.contains && |p| < r_s) — 12 holes before (9 Murray ~300 m from the mouth + 3
## Errington trench) -> 0 after; cap-off mutation fires 243. Gate 771/771. HONEST NOTE:
## lip_cap is flat above surface_r; real-DEM relief > ~3.5 m across an opening could
## leave a thin residual (census runs uniform-field like every sibling audit) — if Chad
## ever feels a graze again, the lever is lip_cap_m or a radius_at-derived cap.
## ★ NEXT: Chad's next ordinary flight just confirms the Murray climb-out is clean (no
## card needed); the [PROF] stutter lines are STILL OWED; then the BUBBLE-LOCATION rung.

**START HERE.** You are continuing the Scarce Skies tunnel thread in the worktree
`D:\flight_sim2\seads-tunnel`, branch `sandbox/tunnel-forge`. Everything below is committed
locally (T14..T26), gate green, goldens unmoved. The flight kernel is FROZEN — this thread
touches `world/tunnel_net.*` (the tunnel's own volume only), `render/tunnel_mesh.*`,
`render/tunnel.cpp`, `render/ribbon*`/`river_surfaces*`, `render/sphere_param.*` (T26
terrain trim), tests, and docs. Read `CLAUDE.md` (Learned section especially), then
`docs/tunnel_staging.md`, then this.

## ★ T26 — THE SEAL PASS EXECUTED (2026-07-23; /seal-pass, all-subagent, zero Chad flights)

**ALL FOUR round-17 findings + one extra (Chad's mid-run report: a second black line over
the entrance) are FIXED; gate 770/770; census 0 leaks; red-team no P0/P1. NEXT ACTION =
CHAD FLIES `docs/tunnel_fly_cards.md` ROUND 18 — one CERTIFICATION fly.** The ledger:

1. **The T26 LEAK CENSUS** (Phase A, the forever tripwire): a full-coverage offline sweep
   in `test_tunnel_mesh.cpp` — 2,498 eyes / 14,897 rays (junction-interior below-mouth
   rakes, chamber panoramic, exterior->in over the Errington hemisphere, bore arclength,
   mouth vertical/oblique fans), greedy-clustered into holes with world coords + suspect
   piece + TUNCAM repro strings, honest CLEAN negatives, a drop-breach-collars mutation
   arm (86), and `REQUIRE(leak_rays==0)`. Baseline found EXACTLY 3 holes; finding 4 was
   NOT a separate hole (chamber+exterior swept clean — it was the Errington junction gap).
2. **Findings 3+4 (both junction gaps) SEALED:** the breach collar tore azimuthally where
   the oblique bore grazes the arena wall — 4x azimuthal densification (exact 24-vert
   tube seam preserved via a coarse->fine fan) + 3-band margin to 320 m, all re-landed on
   the wall via `arena_surface_point`. Plus a census-only Murray sub-floor edge curb
   (1-ray sliver under the pit-floor cone terminus).
3. **Finding 1 (the "terrain cover sheet") — attribution was the story:** NOT terrain and
   NOT the collar (both falsified by differential render probes; single-piece removal is
   confounded by overlapping geometry — see lessons). It was the T16 portal frame's
   LINTEL + CANOPY boxes, placed rigidly off the bore-mouth frame with no terrain
   conformance, hanging over the trench-side drop. Both REMOVED (visual-only, no SDF);
   posts/wings/ruins stay; the approved right-side cut wall verified pixel-unchanged;
   the lintel-top beacons ride with the lintel (red-team P2-4; test contract flipped to
   pin the ABSENCE). A terrain-grounded lintel re-add is a build-on-request.
   ALSO fixed en route: `fill_face`'s any-vertex cut trim let coarse terrain facets hover
   whole over any cut (latent, mutation-verified kill test; `kPlanetCutSplitDepth=4`,
   cuts-empty byte-identical).
4. **Finding 2 (half-buried Murray lights):** the 48 m bright-glow billboards sat ~20 m
   off the wall — `mouth_beacon_axis_radius` clamps ring semi-axes clear of the wall
   (52x32 vs 88x72; trade on the card). Fixes Errington's identical unreported clipping.
5. **The extra black line** (Chad, mid-run): a RIVER (kind 3, mirror-water pass) — the
   one ribbon class T24's clip skipped; `build_river_surfaces` now takes the same cuts +
   `ribbon_indices_outside_cuts` (T24-river test leg).

**Red-team residue (P2/P3, documented not fixed):** the census pins the collar margin +
curb but NOT the azimuthal densification specifically (a sub=1 revert stays green — fly
+ red-team verified only); only junction+bore regions have PROVEN non-vacuity (chamber/
exterior/mouth arms would need their own mutation levers); the kCavern piece is at ~60k
of 65535 ushort indices (Debug assert only — mind headroom on any collar growth);
`curb_drop`'s 30 m floor exceeds a bench tooth if `kBowlBenches`/depth retunes (T16
catches it loudly); possible hairline T-junction crack at the pit-rim terrain seam
(cosmetic). Round-17's stutter [PROF] lines are STILL OWED from the fly.

**Process note (the /seal-pass skill worked):** census -> supervised batch fix (Opus/
Sonnet in isolated worktrees, Fable spec+review+gate+commit) -> one red-team -> ONE fly.
Worktree gotchas for future runs: agent worktrees may be created on the WRONG BASE
(verify HEAD == the tunnel tip, reset if not); FetchContent deadlocks under
`.claude/worktrees` (use `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-v4/
deps_src/raylib-src`); agents must build ONLY in their worktree (a stray main-tree smoke
locked the gate's test exe).

## ★ THE SEAL PASS (Chad's ruling, 2026-07-22): end the whack-a-mole; Claude-only

**SKILL-IFIED: invoke `/seal-pass` (`.claude/skills/seal-pass/SKILL.md`) — it is the
binding harness for this completion: model tiering (Fable specs/reviews/commits,
Opus for the census instrument + subtle geometry, Sonnet as workhorse), isolated
worktrees, the supervision firewall, and the phase specs. The summary below is the
short form; the skill is the procedure.**

(A Hermes/third-party offload was explored and CANCELLED — Anthropic policy routes
third-party apps to paid extra usage, and cheaper unsupervised models are a code
hazard. Chad's ruling: Claude only, cheaper Claude models under Fable supervision.)
The old loop — Chad flies, finds one hole, we fix it, he flies again — spends his fly
time AND Fable tokens on DISCOVERY. The Seal Pass inverts it: discover everything
offline in one census, fix in one supervised batch, certify in ONE fly.

- **Phase A — LEAK CENSUS (one rung):** extend the existing audit infra into a
  full-coverage census — chamber-interior panoramic eyes (both junction mouths from
  below/side — finding 3), an Errington-hemisphere dense fan (finding 4), exterior
  orbit + vertical fans over both mouths and the full bore. Output = EVERY leaking
  sightline clustered into holes with coordinates + suspect cover surface. Spec by
  Fable; implementation by SONNET subagents in isolated worktrees; Fable reviews the
  diff, runs the gate. The census leg stays forever as the regression tripwire.
- **Phase B — BATCH FIX:** every census hole + finding 1 (subdividing trim; KEEP the
  approved right-side cut wall) + finding 2 (raise the lamps) fixed with the known
  trim/collar pattern (T14b/T15/T23) — Sonnet implements per-hole, Fable reviews each
  diff, gate + census rerun until census = 0 leaks. Chad flies ZERO times in here.
- **Phase C — ONE certification fly:** through-run both ways + chamber wall-crawl +
  the strip re-entry + `SEADS_PROF=1` (send the [PROF] lines — still owed) + the
  CARD-4 throat look call. If the census did its job, this is the closing fly.
- Supervision protocol (binding): Sonnet subagents NEVER commit and never touch
  `sim/`/`control/`/goldens; every diff is Fable-reviewed before gate; worktree
  isolation for anything beyond trivial; commits only by the supervising session by
  explicit paths.

AFTER the tunnel is sealed: the bubble-location rung (`docs/bubble_location_scout.md`;
footprint reference image `assets/opposing_atm_bubbles.png` — west egg
Levack/Onaping/Dowling/Chelmsford, east egg Murray Mine/Copper Cliff→Sudbury→Garson,
Azilda in the thin-air seam; NOTE the drawn west line passes ~through Azilda vs §2.5
canon Azilda-in-east — Chad's call at solve time).

## ROUND 17 CARD (flown — kept for reference; docs/tunnel_fly_cards.md)

The three round-16 tail findings were worked overnight (2026-07-22): **T23** the Murray
junction see-through is FIXED (67 vertical leaks -> 0, subdividing bore trim + a permanent
vertical-dive audit), **T24** the Errington floating ribbons are CLIPPED at the excavation
cuts (single-source with the terrain) + the pale-layer values separated a notch
(sleeve darker / portal brighter), **T25** the stutter is INSTRUMENTED, not guessed at —
Chad flies with `SEADS_PROF=1` and the spike lines name the guilty pass. Two queued items
after his fly: the stutter fix (keyed to what [PROF] names) and the disk-roof-over-throat
look call (round-17 CARD 4; details in staging T23). The sections below predate the run —
their findings list is now HISTORY; the mechanism notes and the bubble-rung queue stand.

## THE STATE IN ONE PARAGRAPH

The Errington↔Murray tunnel is FLOWN AND APPROVED end to end (Chad, round 15: "I could fly
through both ways"). Both entrances are open, collision matches the drawn excavations
(Murray = the benched visible cone + a below-floor mouth THROAT with a drawn ramp face;
Errington = the sleeve-rendered cylinder pit), the Errington surface is a researched 1931
Treadwell-era ghost-ruin ensemble, and the Murray opening blends into the scene. The
always-on kill tests that guard all of this: the T16 multi-eye leak audit (0 leaks), T17
entrances-open (entry paths clear of greybox; trim-off arm blocked), T18 escape-survivable
(outbound + inbound + steep-dive fans at both mouths: no death below terrain without a
rendered surface within 30 m; below-floor exit corridor volume-pinned with the throat-off
lever).

## OPEN FINDINGS — ROUND-16 TAIL (Chad's exact words; the next rung, T23)

1. **"I had stuttering a couple of times in the tunnel and also while shooting in the
   sky."** MEASURE FIRST — the T16 lesson (the round-12 "choppiness" was the always-on SDF
   scan, ~28us/call fixed by a broad-phase, NOT render). Instruments: the `--smoke` runs
   print per-frame timing; suspects in rough order: (a) the mouth collar pieces grew (T19
   depth-4 border leaves: Errington ~8.6k verts) — but they draw as one mesh, so upload,
   not raster, would be the cost; (b) the bright-lamp tiers at the mouths (many additive
   glows); (c) shooting-specific = the combat pipeline (tracers/audio allocations — that
   one afflicts the SKY, so it is likely NOT tunnel geometry at all and may belong to the
   ballistics thread). Do not guess: profile, attribute, then fix ONE thing.
2. **"I went straight down my mine and died from something invisible. I can see through
   the bottom where the murray pit meets murray tunnel."** A SEE-THROUGH HOLE plus an
   invisible stop at the pit-floor/slot junction on a VERTICAL dive. Prime suspect: the
   T21 adapter tuck (6 m down + 4 m IN) — the pi-band roots moved to r~162.5 while the
   funnel floor edge/disk sit at 166.5: a ~4 m annular sliver near the SLOT edges may now
   be uncovered (see-through), and the vertical dive then stops on honest-but-unmarked
   rock behind the hole. VERIFY with the leak audit plus a dedicated eye straight above
   the Murray mouth (WARNs are pass-tagged `audit mode N` — trust only mode 0; the
   mutation arms leak BY DESIGN, a prior session lost hours to that misread). Fix shape:
   cover the sliver (outset the pi-band TOP row back to the floor edge while keeping the
   recessed interior, or widen the disk's inner overlap), and add PURE-VERTICAL rows to
   the T18 inbound fan (straight-down through the strip at several stations — the current
   steep dives are ~70-80 degrees, his death was 90). CONFIRMED STILL PRESENT by Chad
   post-T22 ("I can see through the bottom of murray mine, maybe you fixed it" — it was
   NOT fixed, only diagnosed; T22 drew the ramp face, which is a different surface than
   the suspected slot-edge sliver).
3. **Errington, CHAD'S POINTER LANDED (his words): "on the right side there seems stuff
   in the way or make it look less illusory, it looks cool but also there is the
   snowmachine trail and another line. There is almost overhang there its hard to tell."**
   Two diagnoses to verify on the RIGHT flank of the approach:
   (a) **The floating ribbons.** The snowmachine trail + a second line (road ribbon — the
   black + orange lines visible in every entrance smoke) are drawn ON the heightfield, but
   the terrain there is CUT (pit + trench) — the ribbons hover across the excavation void,
   the likeliest "illusory / almost overhang" read. Fix shape: give the ribbon draw the
   SAME CutDisk drops the terrain triangles get (errington_pit_rim + the trench steps —
   single-source, they already exist); that code lives app/render-side (the sudbury_gis
   ribbon pass), not in tunnel_mesh.
   (b) **The stacked pale layers.** Right of the arch several unrelated pale surfaces
   stack at different depths (trench step wall, pit sleeve, collar band, portal wing) with
   near-identical value — depth-ambiguous ("illusory"). Cheap lever: separate their VALUES
   a notch (the sleeve darker than the drape, the wing brighter) so parallax reads; do NOT
   re-darken the mouth (the T20 lesson — contrast IS the Errington read).
   Chad also says the ruins "look cool" — do not rework them.

## THEN: THE BUBBLE LOCATION RUNG (Chad: "then we can move on to the bubble location")

The R6 AtmosphereField campaign placement per Chad's 2026-07-21 map redefinition (canon in
`Game_loop_idea/MASTER_PLAN.md` §2.5 + the memory ledger): Chelmsford coalition bubble
(west: Errington + Chelmsford + Dowling + Levack/Onaping Falls) with its proximal EAST edge
AT the Errington Mine; Sudbury faction bubble (east: Azilda + Sudbury City) proximally
bounding MURRAY Mine just east of Azilda. Each tunnel mouth sits ON its bubble's proximal
edge; the thin-air gap between bubbles is what the tunnel bypasses. The AtmosphereField
mechanism lives on `sandbox/fields-forge` (R6, tag `game-R6-atmospherefield`) — this rung
likely needs a merge/reconciliation with fields-forge; the tunnel is additive, R6's
`atm_frac` field is spatial, and `sim/fields.h` is the single source. Escape-sky transition
zone (choking engine) is a separate queued R7 item — do not fold it in here.

## THE ERRINGTON ART THREAD (paused, decisions pending)

`docs/errington_entrance_art_brief.md` = the researched history (Treadwell Yukon
copper-zinc-lead-silver, 1924-31, killed by the metal-price crash — NOT Mond nickel), a
ranked photo ledger, a Blender blockout spec, and 6 open questions. Chad ruled the STATE
(1931 ghost-ruin — built as the T19 procedural ensemble) but has NOT answered the rest
(lead question: keep real copper-zinc history or bend to the game's nickel fiction). The
true Blender hero asset is still open beyond the greybox ruins; the rig-D headless Blender
pipeline in the main seads repo is the precedent.

## WAYS OF WORKING (the ones that bit this thread)

- The loop: fly finding -> root-cause with a PROBE/instrument before coding -> fix at the
  single source -> kill test with a mutation lever -> gate (`.claude/hooks/gate.sh`, must
  exit 0) -> smoke + READ the png yourself -> docs (staging + fly cards + lessons) ->
  commit by EXPLICIT paths (never `git add -A`; a parallel agent shares this repo class)
  -> memory. Chad flies every rung; put the fly checklist INLINE in your reply.
- Smokes: `SEADS_TUNCAM_MOUTH="dist,az,el,errington|murray" ./build/seads.exe --smoke 60
  name.png` (az 0 = toward the other mouth; el above horizon; the png lands in the CWD —
  raylib drops directory prefixes). Also SEADS_TUNCAM_BORE="s_m[,back]" and
  SEADS_TUNCAM_CAVERN. A smoke certifies ONLY what its caption names — enumerate what must
  read OPEN, not just what got fixed.
- Audits reward sealing; the entrance-open/escape fans are their mandatory dual. Any new
  trim/drop needs a backer analysis (what renders behind the dropped facets?) and
  subdivision keeps mixed leaves (voids impossible, over-cover allowed).
- Test names pure ASCII (4 recurrences; gate.sh has a tripwire). Mutation-arm WARNs are
  pass-tagged — read only `mode 0` as truth.
- Chad's scope words bind exactly: "make the opening blend in" meant MURRAY ONLY (the T20
  regression came from applying it to Errington); "1930 ruins" answered only the state
  question. When he says "use opus for the easier parts", spawn Opus agents for
  well-specified mechanical work and keep review/gate/commit yourself.
- Cost discipline: instruments over debate; one scoped red-team per landed mechanism;
  budget rungs small.

## LAUNCH

"Continue the Scarce Skies tunnel thread — read docs/tunnel_handoff.md." First action:
reproduce finding 2 (the Murray junction see-through) with a straight-down TUNCAM smoke +
the audit, before touching anything.
