# TUNNEL FLY CARDS — round 18 (T26 SEAL PASS: THE CERTIFICATION FLY, 2026-07-23)

## ★ ROUND-18 VERDICT (Chad, 2026-07-23): CERTIFIED — "yep great work on the seal...
## that is really all I found." ONE new finding seeds the T27 census: "a ceiling of
## invisible collidable dirt coming out of murray — I see a puff of smoke and lose like
## 70-90 m/s in a matter of a second or two" (collides-solid-but-renders-nothing, the
## INVERSE census class; scrape/soft-band suspect; exit corridor over the Murray mouth).

## ROUND-18 BANNER — this is a CERTIFICATION fly, not a search
The whole tunnel was censused offline (2,498 eyes / 14,897 sightlines, every viewpoint
class): 3 holes found, 3 sealed, census now reads ZERO leaks and stays in the gate forever
as the regression tripwire. Every round-17 finding is fixed. Kernel/collision untouched;
red-team returned no P0/P1. You fly ONCE to certify — anything new you find seeds the next
census, not a whack-a-mole round.

## CARD 1 — THE CHAMBER JUNCTION GAPS (your finding 3 — BOTH mouths)
The rectangular see-through below each tunnel mouth where it meets the chamber wall was
real at BOTH junctions (census: 2 leak clusters, the breach collar tore azimuthally where
the oblique bore grazes the wall). The collar now carries 4x the azimuth chords + a wider
3-band margin, re-landed on the chamber wall (over-cover only, never overhanging).
- **Grade:** enter the egg from EACH side, turn back and look at the mouth you came
  through, especially the band BELOW it down to the wall. Any sky/see-through left?
- Your "one more hole from the Errington side": the census swept the whole Errington
  hemisphere interior AND exterior — clean. It was the Errington junction gap above.
  If you still see one, note roughly where; a census eye goes there next.

## CARD 2 — THE ERRINGTON ENTRANCE (your finding 1 + the extra black line)
- **The "terrain cover sheet" overhang is GONE** — it was never terrain: it was the
  portal frame's LINTEL BEAM + CANOPY HOOD, placed rigidly off the bore mouth with no
  terrain awareness, hanging over the trench-side drop. Both removed (visual-only, no
  collision). The posts + wing walls + all the ghost-ruins stay; your approved RIGHT-side
  cut wall is verified pixel-unchanged. The beacons that sat on the lintel went with it.
- **The second black line** was a RIVER (mirror-water, a separate draw the trail clip
  didn't cover) — it now clips at the same excavation cuts as everything else and ends in
  the forest before the pit.
- Also: coarse terrain facets could hover whole over any cut (a latent bug, fixed with
  subdividing trim) — the crater edges may read slightly crisper.
- **Grade:** fly the trench approach in daylight. Overhang gone? Entrance still reads
  right without the lintel beam (or do you want a properly-grounded beam back — that is
  a build-on-request)? Right-side wall still cool?

## CARD 3 — THE MURRAY MOUTH (your finding 2 + one census sliver)
- **The half-buried lights:** the beacon glows (48 m radius) sat ~20 m off the rock, so
  the wall sliced every billboard. The rings now clamp clear of the wall — full round
  discs. Trade: the ring is tighter/more inset than before (52x32 m vs 88x72). Same fix
  applies at Errington (identical unreported clipping).
- The census also caught a 1-ray sliver UNDER the pit-floor edge (a grazing sightline
  into rock below the cone terminus) — sealed with a short sub-floor curb, invisible
  from normal flight.
- **Grade:** the mouth at dusk/night — lamps read as full discs, clear of rock? Ring
  size still read right, or too tight?

## CARD 4 — THE THROUGH-RUNS + THE STILL-OWED [PROF] LINES
- Both through-runs (Errington->Murray, Murray->Errington) + a chamber wall-crawl along
  the egg. The census says sealed everywhere — this fly certifies it.
- Launch with `SEADS_PROF=1` and send back any [PROF] spike lines — the stutter
  instrument from round 17 is still armed and still owed data. The stutter fix keys on
  what those lines name.
- The disk-roof-over-throat look call (old round-17 CARD 4) remains open if you want it.

## KNOWN NOTES (named trades, not bugs)
- The junction collar densification is fly-verified + red-teamed but not separately
  census-pinned (the census pins the margin + curb); a hairline crack at a pit-rim
  terrain seam is theoretically possible (T-junction class) — if you see one, point.

---

# PRIOR — round 17 (T23+T24+T25: THE JUNCTION SEE-THROUGH + THE FLOATING LINES + THE STUTTER NET, 2026-07-22 overnight)

## ROUND-17 BANNER
Your three round-16 tail findings, worked overnight. Collision and the kernel are untouched;
the tunnel volume is untouched — everything here is render cover, ribbon clipping, one look
notch, and an instrument.

## CARD 1 — THE MURRAY BOTTOM SEE-THROUGH (your "maybe you fixed it" — NOW it's fixed)
"I can see through the bottom where the murray pit meets murray tunnel" was real and
measured: 67 straight-down sightlines saw through an uncovered ring around the slot at the
pit-floor junction (the trim that opens the slot dropped whole ~40 m facets, and the T21
adapter tuck had pulled the old cover out from behind the overshoot). The trim now
subdivides at the boundary and drops only fully-swallowed slivers — same discipline as the
T14b gash trim: voids impossible, over-cover allowed. All 67 leaks are 0 now, and a
permanent vertical-dive audit (one true-90° ray per point of a dense annulus over the whole
pit floor) guards it.
- **Grade:** dive your mine straight down again, eyes on the pit-floor/slot junction. Any
  see-through left? Any stop you can't attribute to a visible surface?
- The junction edges now carry small grey rock slivers hugging the slot boundary (the
  honest cover). If they read ragged/ugly from the dive, say so — the lever is the
  subdivision depth (one constant).

## CARD 2 — THE ERRINGTON RIGHT FLANK (your pointer, both diagnoses)
- **The floating lines:** the snowmachine trail + the road ribbon were drawn on the
  heightfield across the CUT terrain — hovering over the excavation void ("almost
  overhang"). They now end exactly where the ground does (the ribbons yield to the same
  cut disks the terrain drops triangles for — single source, test-pinned).
- **The stacked pale layers:** the same-brightness surfaces at different depths right of
  the arch now separate a notch in value — the pit sleeve went DARKER (x0.8), the portal
  frame/wings went BRIGHTER (0.75 -> 0.82). The mouth itself is NOT re-darkened (your T20
  ruling stands: contrast IS the Errington read). The ruins are untouched.
- **Grade:** fly your right-flank line in daylight. Does the overhang/illusory read
  resolve — lines grounded, layers separating in parallax? If a layer still reads wrong,
  point at it; each is now its own value dial.

## CARD 3 — THE STUTTER NET (measure first — your fly IS the measurement)
The round-16 stutter ("in the tunnel and also while shooting in the sky") is NOT fixed —
it is instrumented, per the T16 lesson (attribute before touching). Launch with the env var
`SEADS_PROF=1` and fly normally: whenever a frame spikes past 2x your recent median, ONE
console line prints naming which pass owned it (planet / tunnel walls / lamps / aircraft /
fx+hud / swap / app tick...).
- **Ask:** fly the tunnel + a sky gunfight with SEADS_PROF=1, then send me the [PROF]
  lines (or just leave the console up and read the top word). That attributes the stutter
  in one fly; the fix is next session's first rung.

## CARD 4 — regression
- The through-run both ways, the strip re-entry from round 16, the escape from round 15,
  the T toggle, drone combat.
- Known + diagnosed, NOT changed tonight (your look call first): the dark strip's FAR half
  is roofed by the pit-floor disk drawn over open throat air — flying down through the far
  strip passes through a drawn floor (it has been this way since T17; your dives near the
  mouth ring never crossed it). Opening it would cut a long visible gash across the pit
  floor — say if you want that, or if the roof should stay and the throat's far reach
  should shrink instead.

**Kill switches:** `[tunnel] enabled = false`; ribbons revert = `SEADS_NO_RIBBONS=1`;
profiler off = just don't set SEADS_PROF.

**After this round:** the BUBBLE LOCATION rung (Errington on the Chelmsford edge, Murray by
Azilda for Sudbury — the campaign map placement), then the 07-21 mine-entrance ART rulings
(Murray proper open-pit look, Errington portal frame w/o the T13 headframe).

---

# TUNNEL FLY CARDS — round 16 (T19: THE 1931 RUINS + THE MURRAY BLEND, 2026-07-21 night) — SUPERSEDED BY T23/T24

## ROUND-16 BANNER
Cosmetics only this round — collision, kernel, and the tunnel itself are untouched (your
round-15 through-flight verdict stands).
- **Errington is now a 1931 ghost mine:** a raking ruined gallows headframe (one leg snapped,
  fallen timbers at its base), a roofless rockhouse shell, an ore-car trestle running to a
  waste-rock dump fan — clustered on the pit flanks in weathered-timber grey, the pale
  concrete portal still framing the adit. True to the researched Treadwell-era look.
- **The Murray opening blends in:** the pit surround now fades into the dark slot (no more
  glaring pale ring), and the leftover flaps are darkened + halved again.

**Kill switch:** `[tunnel] enabled = false`.

## CARD 0c — THE FLY-BACK-IN DEATH (your latest finding)
Your "right down the pipe, hit something invisible" was real: the dark opening in the Murray
pit floor is longer than the tunnel slot — its front half sits over a rising rock apron that
had collision but NO drawn face, so a steep drop into that half stopped you mid-air. The
apron is now DRAWN (a dark rising floor with the throat lamps sitting on it), so what stops
you is visible — and the honest way in is unchanged: drop into the strip, level early, ride
the apron down into the bore.
- **Grade:** fly the re-entry that killed you. Does the strip now read as a ramp INTO the
  tunnel rather than a void? Any stop you can't attribute to a visible surface = say where.
- Errington "unclean and obstructed": I shrank the channel's leftover flaps another notch —
  but tell me WHERE the obstruction reads (the terraced walls above the arch? the portal
  wings? something in the channel?) and I'll hit that exact thing next.

## CARD 0b — THE INVISIBLE ENTRY DEATH + THE ARMS (your latest two findings)
- **Errington entry deaths:** my T18 collision change had narrowed the pit's open air into a
  cone tighter than the crater you see — sagging below the trench-floor line while lining up
  the arch crossed into invisible rock. Reverted: the pit's air is the full drawn crater
  again (its wall IS rendered — the sleeve), and a new inbound-fan test pins 27 descending
  entry lines per mouth so an entry regression can't ship silently again.
- **Murray stretchy arms:** the pale spokes around the mouth were a hidden stitch ribbon
  lying in the floor plane; it's now tucked 6 m under the floor disk. Long spokes gone.
- **Grade:** fly the Errington entry the way you died — trench line, sag low, into the lower
  half of the arch. And orbit the Murray mouth — arms gone?

## CARD 0 — THE IMPREGNABLE FIX (fly your first-word finding again)
Your "errington is impregnable!" was the low-approach read: the old wide canopy hood roofed
the arch from trench-line height, and my darkening pass (which you only asked for at Murray)
had washed out the adit's contrast under it. The canopy is now a short brow and Errington
keeps full contrast.
- **Grade:** run the trench line at deck height — does the beacon-ringed arch read as THE
  way in from the moment the pit comes into view?

## CARD 1 — the Errington ruins read
Orbit the Errington mouth at 300-700 m, then run the trench line in.
- **Grade:** does it read as a DEAD 1930s mine — the leaning headframe on the skyline, ruin
  glimpses in the trees — while the adit stays obvious and flyable?
- Named trade: the forest runs right to the rim, so the rockhouse/trestle/dump read as
  glimpses among the trees (forest-reclaimed). If you want the mill BOLD, say so — the lever
  is a tree-exclusion margin around the ruins (a world-thread change).

## CARD 2 — the Murray blend
Orbit + dive the pit.
- **Grade:** does the opening now belong to the scene — the surround fading into the slot,
  no out-of-place pale geometry? Throat lights still read on the way in?

## CARD 3 — regression
- The through-run both ways, the escape you flew in round 15, drone combat, the T toggle.

**After this round:** the BUBBLE LOCATION rung (placing the atmosphere bubbles per your
campaign map — Errington on the Chelmsford edge, Murray by Azilda for Sudbury). Say the word
and I write the handoff doc for the next agent.

---
# TUNNEL FLY CARDS — round 12 (T14b+T15: SEAL THE SEE-THROUGH + LIGHT THE MURRAY PIT, 2026-07-21) — SUPERSEDED BY T16

## ROUND-12 BANNER — your two round-11 findings, both fixed

- **"I can see through the ground to the surface" (west of the Murray tunnel): SEALED.**
  The openings in the chamber wall are cut wider than the tunnel itself (they have to be, so
  the wall grid never clips the opening) — and the "rock" between the chamber wall and the
  surface was never real geometry, so through that margin you saw straight out of the planet.
  Each opening now has a COLLAR: a rock funnel from the tunnel's mouth ring out to the hole
  rim, exactly like the surface pit collars. A ray test confirms it: 46 see-through sightlines
  before, ZERO after. The cut was also trimmed to hug the tunnel's slanted silhouette.
- **"A blind entrance... black and I can only see one light going down": LIT.** The Murray pit
  had no lights above the tunnel-mouth ring 300 m down. It's now a beacon FUNNEL: a ring of 12
  around the pit rim at the surface (also your climb-out reference — the "exiting is a little
  blind" cue), a ring of 8 at mid-depth on the pit wall, then the existing mouth ring at the
  bottom. Three rungs of light going down.

**Kill switch:** `[tunnel] enabled = false` (bit-identical to the frozen kernel).

## CARD 1 — the west approach to the Murray tunnel (the see-through)
Fly the through-route E→arena→Murray and watch the wall around the Murray opening as you
approach from the west.
- **Grade:** is the see-through-to-the-surface GONE? The opening should read as a lit hole
  with a solid rock throat behind it — wall everywhere else. Same check on the Errington side.

## CARD 2 — the Murray pit dive (the blind entrance)
Enter Murray from the surface. From approach the pit should read as a ring of lights on the
ground; diving in you pass the rim ring, the mid ring, then the mouth ring at the floor.
- **Grade:** can you line up and fly in by sight now — no more black hole with one light? If
  the funnel needs MORE light, the dials are lamp counts (`kBowlRimBeaconCount` 12 /
  `kBowlMidBeaconCount` 8 in `render/tunnel_mesh.h`) — say brighter/denser and I move them.
- Climbing OUT: the rim ring above you is the exit reference (your "exiting is a little
  blind" note).

## CARD 3 — regression
The chamber openings/beacons (round 11's fix), the trench entry, and the bores are otherwise
untouched.
- **Grade:** anything that worked in round 11 that broke?

**Still queued next (your 07-21 art rulings):** Murray's proper open-pit look and the
Errington portal frame replacing the headframe.

---

# TUNNEL FLY CARDS — round 11 (T14: THE TRUE BREACH LOCATOR, 2026-07-21) — SUPERSEDED BY T15

## ROUND-11 BANNER — the blind spots were ONE bug, and it's dead

Your round-10 verdicts — "blind spots entering AND exiting the central chamber" and "cannot
fly into or out of Murray from that side" — were ONE bug. When the room was shrunk (round 10's
centering), the code that decides WHERE to cut the two openings in the room's wall was never
updated: it cut them ~750–900 m from where the tunnels actually pierce the wall, into solid
rock. So the REAL openings stayed painted-over wall (your blind spots), and the beacon rings
marked the FAKE holes — flying at a lit ring was flying into rock, which is exactly why Murray
felt impassable. Collision was always right; only the picture lied. Now the openings, the
beacon rings, the ember skips, and the tube break all derive from the ONE true crossing point
(computed on the same math the collision flies), and the opening is cut ELONGATED to match the
tunnel's slanted angle through the wall.

**Kill switch:** `[tunnel] enabled = false` (bit-identical to the frozen kernel).

## CARD 1 — the chamber openings READ now (E→room→M)
Dive Errington → down the bore → into the arena → across → out the Murray side.
- **Grade:** approaching the room, does the breach read as a LIT OPENING ahead (the light at
  the end of the tunnel), and once inside, do BOTH exits read as beacon-ringed holes in the
  wall — no more flying on faith?
- The beacon rings now hug the REAL lips. If a ring still reads off-center or the hole looks
  clipped at its edges, say WHICH side and WHERE — the hole size dial is `kBreachHoleFactor`
  (2.5× bore width) in `render/tunnel_mesh.h`.
- **WATCH ITEM (named trade):** the hole is cut generously oblong to match the tunnel's slant
  (~1400×550 m see-through vs a ~500×220 m physical opening — the same 2.5× margin policy the
  round holes always had, applied to the slant). Near the LONG ends of a lit ring there is
  see-through-but-solid rock: aiming at a ring-end lamp instead of the dark centre can clip.
  If that bites you on the stick, say so — the ready lever is deriving the long axis from the
  physical opening + the same absolute margin (≈396 m instead of 707 m semi).

## CARD 2 — the Murray side (the round-10 impossible)
Enter at the Murray bowl, dive the bore into the arena, then turn around and fly OUT the same
way.
- **Grade:** can you now fly into AND out of Murray by sight? This exact run was your "cannot
  fly into or out of Murray" — it should just be a tunnel now.

## CARD 3 — entry + bore regression
The Errington trench entry, the descent bores, and the arena look/feel are UNTOUCHED this
round (the fix moved only where openings/beacons/tube-breaks sit).
- **Grade:** anything that worked in round 10 that broke? (Entry ease, lamp lines, the
  centered-room feel.)

**Heads-up, next rung (your 07-21 rulings, NOT in this round):** both mine entrances still
read as "black ant mounds" — Murray gets the proper open-pit look and Errington loses the
headframe for a framed portal. Coming after this fly.

---

# TUNNEL FLY CARDS — round 10 (T13: CENTER THE CHAMBER + EASE THE ENTRY + THE HEADFRAME, 2026-07-19) — SUPERSEDED BY T14

## ROUND-10 BANNER — the chamber is CENTERED, the entry is EASY, and Errington has a HEADFRAME

Four of your asks are BUILT this round:
- **THE CHAMBER SITS IN THE MIDDLE.** You wanted "two proper tunnels meeting a central room,"
  and the room had swallowed most of both bores. Shrunk the room (arena width 7350 → 4200):
  the two bore legs and the room crossing are now comparable thirds (measured ~4034 / 4055 /
  3954 m). Fly E→room→M — the room should sit in the MIDDLE now, not right at each mouth.
- **FLYING IN IS EASY.** The Errington mouth is sunk behind the pit rim, so a straight
  bore-tangent approach used to clip rock behind the crater (5 crashes on the test approach).
  A stepped open-cut APPROACH TRENCH now descends up-tangent into the pit — the approach is
  in open cut all the way in (0 crashes). Line it up from ~600 m out at grade and glide down.
- **NO BLIND SIDE-HOLES.** The two pump side-chambers were dead ends (no pump game-loop yet),
  so they're gated OFF — the bores are a clear passage with no false side-routes. They come
  back when the pumps land.
- **★ THE HEADFRAME.** Your round-10 call — a surface pit-head structure over the Errington
  mouth "just like a real mine would." Built: a steel-lattice SINKING HEADFRAME (the Errington
  Mine No.3 silhouette — a tapering four-leg lattice tower with a sheave head house on top and
  an inclined back-brace pair leaning over the pit), a low bulkhead collar ring at the pit rim
  (broken where the trench enters), and two RED aviation beacons atop the head house.

**NAMED TRADE — the headframe is FLY-THROUGH (no collision):** it's a findability LANDMARK,
not a wall — you can fly straight through the steelwork. That's deliberate: putting an SDF on
it would drop a wall right into the approach corridor the trench just opened. The beacons + the
mouth-ring reuse the same bright lamp tier, so it's findable day and night. If you want it
solid, say so and I add collision (and re-open the entry-ease question).

**Kill switch:** `[tunnel] enabled = false` (bit-identical to the frozen kernel).

---

## CARD 1 — the Errington entry (line up the trench, glide into the bore)
Approach the Errington mouth from ~600 m out AT GRADE (a shallow descent, not a steep plunge).
The HEADFRAME + its two red beacons are your landmark — aim for the pit under the tower. A
stepped open-cut trench descends up-tangent into the crater; glide ~20–25° down the cut and
into the bore.
- **Grade:** does entry feel FAIRLY EASY now? (No clipping rock behind the pit on the way in,
  no "I just die at the mouth.") The trench is `[tunnel] trench_len_m` (450; 0 = OFF, reverts to
  the hard sunk entry); `trench_rim_m` (150) is its width.
- **Grade the headframe:** does it read as a real pit-head structure over the mouth — the
  tower + head house + the beacons a findable landmark from the approach? It's `headframe_on`
  (true; false = gone), height `headframe_h_m` (80). Remember it's FLY-THROUGH by design.

## CARD 2 — the centered chamber (fly E→room→M and back)
Dive Errington → down the bore → break into the arena → out the far bore to Murray, then
reverse (Murray → arena → Errington).
- **Grade:** does the chamber sit in the MIDDLE — a central room fed by two proper tunnels of
  comparable length, not a room that starts right at each mouth? (The dial is `[tunnel]
  arena_a_m` = 4200; 7350 reverts to the room-swallows-the-tunnels version.)
- The arena is the same shallow oblate dogfight room as round 9, floor SEALED (lit by embers +
  the core-key). If it reads too dark or the shape's wrong, that's the round-9 dials.

## CARD 3 — clear passage (no blind side-holes)
Fly both bores and look for side-openings.
- **Grade:** does anything still read as a passage that ISN'T one? The pump side-chambers are
  gated off this round (`[tunnel] chambers_on = false`), so the bore should be a single clean
  corridor with no dead-end side-holes. If you see a side-opening that looks flyable but dead,
  say WHERE. (`chambers_on = true` brings the side rooms back — they return with the pumps.)

## CARD 4 — the Murray dive (regression check)
Unchanged this round — fly the Murray bowl dive as before.
- **Grade:** does the Murray open-pit bowl still read and dive clean (the round-4+ behavior)?
  This is a regression check — nothing here changed, so if it feels different, say so.
- Revert dials: `[tunnel] bowl_radius_m` (450) / `bowl_depth_m` (300).

---

# TUNNEL FLY CARDS — round 9 (post-T11 THE CORE WINDOW, 2026-07-19) — SUPERSEDED BY T12

## ROUND-9 BANNER — THE CORE WINDOW: a shallow arena + a window down to the core

Your T10 verdict ("way too vast... I wanted to see the core not from every possible angle
you can't escape from there... you dive a little, it takes ages to climb back... I said
100x not close to 800x the egg") is BUILT. The concentric shell is GONE. The new inner body
is ONE giant SHALLOW ARENA — an oblate ellipsoid just below the terrain (fight band ~1.5-2.5
km deep) — with a WIDE CORE-WINDOW SHAFT dropping from its floor to the glowing core. You
dogfight SHALLOW, you SEE the core as a glowing pool through the floor window, and diving to
it is OPT-IN (you no longer fall into an inescapable pit). Size ≈ **100× the egg** now (the
number you asked for), not 800×.

**FLY IT:**
- Dive a mouth (the open-cut crater). Down the SHORT bore ≈ **3.1 km at ~24°** (~15 s at
  200) — strata bands on the walls, crown + floor lamp lines leading in, a bright glow
  growing ahead (the breach into the arena).
- Break into the ARENA: a wide, oblate, shallow dogfight room. It reads lit-from-below —
  the CORE glows as a **~28° pool** through a floor WINDOW (the shaft opening), embers ring
  the ceiling, beacon rings mark the two breaches. You fight up here in the 1.5-2.5 km band;
  escape to the surface is **45-90 s**, and a 500-1000 m dive costs only **25-50 s** to
  climb back (the "climb ages" complaint is gone — the arena is shallow and oblate).
- The core is a huge bright disc through the window: FULL-disc from within **±3.6 km** of
  the shaft axis. Off to the side you still see it, just clipped by the window rim (a 47°
  window). If you want it wider I turn `shaft_radius_m` up (2500 buys ±4.6 km; costs a
  little arena width to hold 100×).
- **OPT-IN DIVE:** drop down the shaft to the core — the arena floor is **2.5-3 min** out,
  a full core round-trip **5-7 min** (the named opt-in price). The INVERTED-LIFT regime
  lives ONLY in the shaft, below r ≈ V²/g: **past halfway down the shaft at speed, lift
  points AT the core — the core defends itself.** Up in the arena you fly normally.
- The core (2500 m) is a solid crash wall at the bottom of the shaft — don't fly into it.
  GUNS FIRE UNDERGROUND (tracers live in the arena + the shaft).

**GRADE:** does the arena read SHALLOW + escapable (no "climb ages")? Does the core read as
a huge glowing POOL through the floor window from the fight band? Is the size ~100× (not
800×)? Do the bores read clean (~24°, no grey curtain)?

**THE SIZE DIALS:** `[tunnel] arena_a_m` (width), `arena_c_m` (height/oblateness),
`shaft_radius_m` (window size). Say "wider / shallower / bigger window" and I move one.
**Kill switch:** `[tunnel] enabled = false` (bit-identical to the frozen kernel).

---

# TUNNEL FLY CARDS — round 8 (post-T10 THE HOLLOW CORE, 2026-07-19) — SUPERSEDED BY T11

## ROUND-8 BANNER — THE HOLLOW CORE: the egg became the planet's whole inner space

Your ruling ("make it a huge inner space where a bunch of planes could dogfight... the
core to the 15km sphere that lights the cavern... the different layers of the planet, and
well lit by the core") is BUILT. The buried dirt egg is gone. The whole INTERIOR of the
15 km sphere is now hollow: a solid glowing CORE at the centre lights a huge flyable
cavern (core 2500 m → ceiling 10500 m ≈ 8 km of radial airspace, ~21 km across — ~725×
the old egg; you asked for 100× as the floor, and the short-tunnel + core-lit spec pushed
it here). The two SHORT bores (Errington + Murray, ~4.6 km each) breach through the crust
+ mantle into the shell — the layers band on the bore walls as you descend, and the core
light spills up the last stretch (the light at the end of the tunnel).

**FLY IT:**
- Dive a mouth (the open-cut crater). Down the SHORT bore: strata bands on the walls
  (crust → mantle), crown + floor lamp lines leading in, and a BRIGHT GLOW growing ahead
  — that is the core light spilling up through the breach (the angle-of-entry cue).
- Break through the breach into the shell: the core lights EVERYTHING. A bunch of planes
  could dogfight in here. Chase someone around the core.
- **T10.1 CAVERN READABILITY (2026-07-19):** the two EXITS are now RINGED IN LIGHT — a ring
  of bright beacons around each breach lip, findable from across the cavern (the "two ways
  in two ways out" cue). And a dim EMBER FIELD is scattered on the ceiling so the shell is
  no longer pure black — the embers give you depth, parallax, and a sense of speed/scale as
  you fly (the reference the black egg lacked); the core light also throws a gradient across
  the near ceiling. Grade whether the space now READS (references exist, you can tell where
  you are and how fast you're moving) and whether the exits are findable.
- The weightless band is r = V²/g — ~6.4 km out at 250, ~2.0 km out at 140. Inside it,
  "level" flight needs the nose pulling toward the core (lift AT the centre, ~−1 g). The
  core (2500 m) is a solid crash wall — don't fly into it.
- GUNS FIRE UNDERGROUND now — fire in the cavern, the tracers live.

**NAMED TRADES (pre-agreed):** ≤few-frame camera pop if you graze a wall fatally; an
eye-in-rock flicker if you hug a cavern wall; the bandits have NO cavern AI yet (this rung
is the space + the light + live guns — a cavern dogfight AI is its own rung).

**THE SIZE DIAL:** `[tunnel] cavern_ceiling_m`. LOWER it = smaller cavern + LONGER
tunnels. Say "smaller/bigger" and I move it. **Kill switch:** `[tunnel] enabled = false`
(bit-identical to the frozen kernel).

---

# TUNNEL FLY CARDS — round 7 (post-T9, 2026-07-19)

## ROUND-7 BANNER — the GRAND EGG: bigger, lit, and the camera stays ON your plane

Your round-6 report ("when I fly into the egg I get a ZOOMED-OUT view of the egg and
I can't see my plane… the egg can be way way bigger and well lit and I can see my
plane inside of the egg") is found and fixed — TWO mechanisms:

- **The zoomed-out view was a CAMERA bug (T9a CAVECAM), not the egg.** Inside the
  tunnel the plane is legally below the bare-sphere surface, and the chase camera's
  "never enter the planet" clamp was hoisting the eye ~2 km straight up to the surface
  the instant you pulled the nose UP inside the egg — a zoomed-out exterior shot with
  the plane invisible. (Nose-down/level looked fine, which is why only the pull-up
  broke it.) FIXED: inside the tunnel the camera skips that clamp and stays on your
  plane. **The one honest trade:** if you clip a wall by less than ~50 m the camera can
  pop for a few frames just before the crash fires — but that's already a dying pass
  (every real tunnel boundary sits ≥ ~75 m above the clamp line, so normal flying never
  triggers it).
- **The GRAND egg (T9b):** the Black Stope is now **2600 m tall × 2200 m wide** (~4×
  the old volume) and **WELL LIT** — the walls read a lit mid-grey cavern instead of
  near-black, with several bright lamp rings down the long axis and finer facets so it
  reads round, not faceted. Verified in day frames: lit walls, lamp rings, cavern shape,
  and the bright egg reads as the DESTINATION at the end of the dark bore.

## CARD 7 — the grand egg + the cave camera
- **Fly into the egg and PULL UP into a climb** — the camera must stay on your plane
  the whole way (the old zoom-out-to-exterior is gone). If you EVER lose your plane to
  a zoomed-out egg view on a normal pass, say so — that jumps the queue.
- Judge the SIZE: is 2600 × 2200 the arena you want for an energy fight? (dials:
  `[tunnel] egg_up_m` / `egg_down_m` / `egg_horiz_m`.)
- Judge the LIGHT: is the egg well-lit — walls read, lamp rings read, plane visible
  inside? Too bright / too dark = one dial (`kEggAmbient` in render/tunnel.cpp) or the
  lamp size/spacing (`kEggRingSpacing`/bright lamp size in render/tunnel_mesh.h /
  tunnel.cpp) — say the felt problem, I move the constant.
- Named trade (repeat): a ≤few-frame camera pop only on a sub-50 m wall graze (already
  crashing). If you see a pop on OPEN air, that's a bug — say WHERE.

# (prior round-6 banner + round-5/round-4 cards follow — all still current)

## ROUND-6 BANNER — the GREY WALL CURTAIN is dead (it was paint, not rock)

Your round-5 report ("an opaque grey wall curtain after a rise in the ramp — can't
see where to go, it opens afterward, made me crash") is found and fixed:

- **What it was:** at the ramp→plateau sag (~4 km down the bore — further in than it
  feels), the flattening floor rises head-on into your view, and the shader's
  floor-brightening key painted that facet as a solid BRIGHT GREY dome — right on top
  of the lamp lines that show the continuation. Open geometry, opaque paint. Your
  "rise in the ramp" and "opens afterward" were both exactly right.
- **The fix:** the floor's brightening key is dropped 0.55 → 0.15 — the crest now
  reads as dark tunnel with the floor-edge and crown lamp lines converging past it
  (verified in close-approach day frames at 100–250 m from the crest). Geometry
  untouched; no goldens moved.
- **The standard grew its third axis:** the gate now verifies OPEN (corridor discs) +
  SURVIVABLE (live crash predicate) + **VISIBLE** — a sightline verifier asserts
  ≥800 m of visible centerline at every station, both directions, every build.
- **One honest wall, your ruling if needed:** 2850 m of depth over ~5 km of arc IS a
  ~31° descent; no re-profiling can push tangent floor-sight past ~450 m (every
  geometry lever was probed and capped). If the sag turns out hard to FLY (not see),
  the lever is a gentler/shallower tunnel — `[tunnel] depth_m` — a design trade
  that's yours.

# (prior round-5 banner + round-4 cards follow — all still current)

## ROUND-5 BANNER — the two unfair deaths are DEAD; the tunnel is now MACHINE-FLOWN

Your round-4 verdicts ("die mid-entrance without a wall"; "the entrance worked but
there's a WALL inside — I think it's still got HILLS in there") were both real, both
found, both fixed, and the fix class is now permanently gated:

- **The entrance death:** the pit's collision volume was a narrowing CONE while the
  visual crater is a straight CYLINDER — the ring of visibly-open air between them
  read as solid rock, and the deep-penetration clause killed you there at ~50 m
  depth. The collision pit is now the cylinder (survivable volume == removed-terrain
  volume by construction), both portals. Walked entry pins (center + 3 off-axis
  descents through the LIVE predicate) are in the gate.
- **The wall/hills inside:** the bore's own SDF read the UPPER HALF of the tunnel as
  SOLID at every spine joint (flat segment endcaps meeting in a kink on the
  descending, bending spine — 917 phantom-rock samples across the flyable space).
  That was your wall, and your hills. Rewritten as a continuous polyline SDF:
  917 → 0.
- **"Don't tell me to fly until it's verified" — adopted as the standard:** a T7
  FLYTHROUGH VERIFIER now lives in the gate — the entire route (approach → pit →
  bore → egg → junction → Murray, BOTH directions, ~1600 stations ≤10 m apart) is
  flown through the LIVE sim crash predicate with a ~45 m clear-corridor disc at
  every station, on every build. A fresh red-team then swept ~19 M samples for the
  opposite crime (false-open rock you could glide through unkilled) — zero. Route
  day-frames reviewed by the foreman: pit open, bore stations 300–5000 are clean
  lamp corridors, the egg opens ahead.

Fly the same run as the round-4 cards below — the three T6 verdicts (recess
entrance, wider bore, monotone descent) still want your stick ruling, now on a
tunnel the machine has flown first.

# Round 4 cards (all still current) — round 4 (post-T6, 2026-07-18)

**T6 reshaped the HOME END and the DESCENT in response to your round-3 verdicts.**
Run `D:\flight_sim2\seads-tunnel\build\seads.exe`. Kill-switch: `[tunnel] enabled
= false` in `config/game.toml`.

The run: spawn over Errington (nose already at Murray, ~12 km) → dive the Murray
BOWL → the tube → the Black Stope → out Errington, home.

## WHAT CHANGED SINCE ROUND 3 (fly these first)

Three round-3 verdicts drove T6. Each is now fixed — judge each on the stick:

1. **The Errington entrance ("opaque positive structure, not a hole" — REJECTED).**
   The home mouth is now a **recessed open-cut PIT**: the bore is sunk 130 m
   (bore-CENTER; the crown sits ~40 m below grade), the trees are cleared over
   the mouth, and an open crater descends to the dark bore opening. Nothing pokes
   up above the treeline — it reads as a DARK CRATER you dive INTO, not a lump.
   Screenshot-verified in day light (T6a TUNCAM instrument).
   - **Verify:** read the entrance from **800 m** on the approach — does it read
     as an opening (a hole/crater), not a positive bump?
   - **Verify:** on a **low approach** — does the recess still read? (Named flag
     below: the shallow 250 m low-angle view is DIM.)

2. **The descent ("it looks like it goes back to the surface and I just die" —
   REJECTED).** The bore now descends **MONOTONE** — it NEVER gains height on the
   way down to the egg. Constant grade, lamp lines converging downhill the whole
   way. No mid-tunnel "climb back toward the surface" that read as a dead end.
   - **Verify:** at about **s ≈ 1000 m into the tube**, does it still read as
     "going DOWN"? The lamp corridor should keep converging downhill — no point
     where it looks like it climbs back up.

3. **The bore is now WIDER × TALLER.** 220 m wide × 180 m tall (`[tunnel]
   tube_width_m = 110` / `tube_height_m = 90`, both SEMI-axes; the floor sits
   20 m up, leaving ~160 m of clearance above it).
   - **Verify:** does the tube feel roomy to fly now, or still tight? (`tube_width_m`
     / `tube_height_m` are single dials.)

**T5 carries over UNCHANGED:** the flat floor, the floor-edge/crown lamp layout,
the destructible gas lamps, the terrain-hugging collar. The floor / lamp cards
below are the same as round 3.
   - **Verify the destructible lamps still work:** shoot out a stretch of lamps
     in the lower ramp, then re-fly it — dark where the dead lamps are.

### Named honest flags (T6 — report if you feel them)
- **The 250 m low-angle approach view is DIM.** The recess is real, so from a
  shallow low angle the pit floor/bore is in shadow and hard to read — it reads
  best from ABOVE. Your stick rules on approach legibility: if the low approach
  needs to read better, that's a lamp/rim-light dial, say so.
- **Walls stay VOID-BLACK by design** — the lamp corridor carries the read, not
  wall shading. If the walls need value, that's a separate ruling.

### Walk-back order (T6 dials, one per fly)
1. `[tunnel] enabled = false` — bit-identical baseline.
2. `tube_width_m` / `tube_height_m` back toward **70** (narrower/shorter) if the
   bore is too big.
3. `mouth_sink_m` (130) — shallower/deeper entrance pit.
4. `floor_height_m = 0` — floor OFF (see CARD 2 walk-back).

## CARD 1 — the Murray bowl (your "nice big opening")
A 900 m-wide open pit tapering to the tube over 300 m of depth.
- Judge: does the opening read from approach, and is the dive-in natural now (no
  more "right angle" hunting)?
- Judge: bowl size/steepness — opening radius and depth are single dials
  (`bowl_radius_m` 450, `bowl_depth_m` 300).
- Note: past the bowl floor the tube plunges steeply (~49°) then rounds out —
  it's a pull-up after the bowl descent. The tube is a SLOW corridor by design:
  at 140 m/s the worst bend is a 2.9 g pull; at 280 m/s it would need ~12 g.
  Judge whether the corridor speed feel is right or the bends want gentling.

## CARD 2 — the lit tube run + the floor

### Lighting
Gas-lamp PAIRS line both floor edges every ~60 m along the whole spine, including
the bottom ramp — a runway read. A CROWN LINE runs along the arch every ~120 m.
The egg rings and chamber lamps are unchanged.
- Judge: can you FLY it now — do the floor-edge pairs give you the runway in
  motion, and does the crown arc give you the vault?  Spacing/brightness are
  single dials (`kFloorLampSpacing`/`kCrownLampSpacing` in
  `render/tunnel_mesh.h`; lamp brightness in `render/tunnel.cpp`).
- The lamps are DESTRUCTIBLE: shoot one (guns) → it dies dark, the round is
  consumed, the dark spot persists across respawns. Verify this:
  **shoot out a stretch of lamps in the lower ramp, then re-fly the section
  — it should be dark where the dead lamps are.**
- Named trade: the short connector tubes into the side chambers carry only their
  end lamps and may read dark.

### The floor
`[tunnel] floor_height_m = 20` (in `config/game.toml`) raises a flat ~98 m-wide
floor through the spine tube, floor-to-crown ~120 m, edge-lit by the floor-edge
lamp pairs.
- Fly it LOW through the flat mid-section corridor. Does it read as a floor you
  can strafe down?
- **NAMED TRADE (report honestly if you feel it):** the crash boundary sits
  BELOW the visible floor surface — the SDF collision is the chord-truncation
  half-space, which undercuts the rendered mesh slightly:
  - In the flat mid-corridor: ~2–5 m of "soft skin" (you can fly a little
    below the visual floor before the crash fires).
  - On the steep ramp near the egg junction: up to ~32 m gap (the ramp geometry
    magnifies the chord/SDF mismatch).
  - You crash slightly LATER than the visuals suggest, NEVER earlier.
  - If the ramp floor must be hard (no soft skin tolerated), say so — the fix
    is per-vertex local-station SDF truncation, a follow-up ruling.
- Kill-switch: set `floor_height_m = 0` to revert to the circular-section tube
  (the lamp layout falls back to low-wall ±60° pairs so the tube still reads).

## CARD 3 — the Black Stope (the energy-fight chamber)
1800 m tall, FAT END DOWN, 1200 m wide, walls LIT, two bright gaslamp rings
(one at mid-height, one 500 m down in the fat bowl). The tunnels enter LOW —
you emerge into the bottom of the chamber.
- Judge: THE ruling — does the vertical egg give you the energy fight you asked
  for (dive into the fat bottom, zoom the thin top, hammerhead back down)?
- Judge: the lighting — does the chamber read as a fightable arena ("the energy
  fight has a lit chamber")? Wall value + ring brightness are dials.
- Judge: entering low — does "the tunnels connect the bottom" feel right, or
  should the junction sit lower/higher (one dial: the 0.85 junction fraction)?
- The two pump chambers hang off either SIDE at mid-height through short
  connectors. Poke into one; the pump-room skeleton is in but the hero dressing
  (machinery, Blender assets) waits on your reference picture.

## CARD 4 — walls stay honest (including the floor)
Same law as round 1: death AT the visible wall, no teleport-to-surface. The
skirt at Errington and the bowl wall at Murray are solid, sun-bright ground;
only the bore and the pit read as openings.
- Flying into the floor = crash, same as a wall. The soft-skin trade above
  (CARD 2) is a named variance — the floor is still a kill surface, just with
  a slightly late trigger on the ramp.
- If anything kills you on visibly-open air or lets you through visible rock,
  say WHERE — that's a single-source violation and it jumps the queue.

## CARD 5 — portal strobe check
After a crash inside the tunnel (respawn over Errington), hold neutral ~5 s and
watch the portal rim below:
- The cut edge at the Errington mouth must stay SOLID — no flashing in and out.
- The Murray bowl rim must be equally stable.
- T5d's terrain-hugging collar band fixed the strobe by draping the annulus at
  terrain-cell density (~59 m cells, 2× azimuth); if it recurs, say which mouth.

## CARD 6 — out Errington + the whole shape
- Judge: the home mouth after the raid — findable? (Now the recessed OPEN-CUT
  PIT, T6c — a dark crater, no longer the small adit. Does home read on the way
  back in?)
- Whole-run verdict: leg lengths, depth feel, the egg placement, the floor —
  is this the raid you want to fight a war over?

## Walk-back order
1. `[tunnel] enabled = false` — bit-identical baseline.
2. `floor_height_m = 0` — floor OFF, circular-section tube, lamp layout
   reverts to low-wall fallback pairs.
3. One dial per fly in `[tunnel]`: T10 = `cavern_ceiling_m` (the SIZE dial: down
   = smaller cavern + longer tunnels), `cavern_core_m`, `breach_margin_m`,
   `chamber_breach_offset_m`; still live = bowl_radius_m / bowl_depth_m,
   tube_width_m / tube_height_m, mouth_sink_m.
4. Look = code constants in `render/tunnel_mesh.h`
   (`kFloorLampSpacing`/`kCrownLampSpacing`/`kCavernLong`/`kCavernLat`/
   `kBreachHoleFactor`) + `render/tunnel.cpp` (`kCavernVal`/`kCavernAmbient`/
   `kCoreVal`/`kCoreKeyGain`/`kCoreKeyFall`/`kChamberVal`/`kSkirtVal`) — say the
   felt problem (ceiling too dark, core too small, breach hole too tight) and I
   move the constant.
