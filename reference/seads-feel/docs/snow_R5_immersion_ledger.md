# R5 — THE IMMERSION RUNG. Chad's list, 2026-08-28.

Branch `sandbox/snow`. Base for this rung: `9fdca369a`.
Authority for MECHANISM stays `docs/snow_session_handoff_20260827.md`.
Authority for the TRACK stays `_20260828_R4.md` + `_R4b.md`.
This file is the authority for the R5 SCOPE and its order.

**Chad's process ruling: ONE ITEM AT A TIME.** Fable 5 builds, I verify, he drives.
Nothing moves to the next row until the previous is verified AND driven.

**Chad's platform ruling: HEADLESS AND PROCEDURAL.** Blender is OUT for this rung
(he has it booked for other work, and the engine could not consume a baked track
anyway -- the cut is authored wherever he drives).

**NOT A DEFECT:** `assets/audio/*` is empty ON PURPOSE. This is a sandbox; the sounds
live in seads-recon and the main game. The `[AUDIO] missing, ...` lines at startup are
EXPECTED NOISE. Do not "fix" them and do not report them as a finding.

---

## THE ORDER, AND WHY

Rows 1-2 are the INSTRUMENT. They ship first because they gate his ability to JUDGE
rows 3-7. This is the R4b lesson applied up front: a rig that cannot reach the state
certifies nothing about it. He could not see the track partly because the camera never
framed it -- so fix the seat before tuning what the seat looks at.

| # | Row | What it is | State |
|---|-----|-----------|-------|
| 1 | VIEW DIAL | Keys 1-4 = four chase presets, live. Keys 1-4 are FREE (verified). | **BUILT + VERIFIED `7341352d7`. AWAITING HIS DRIVE.** |
| 2 | FREELOOK ANGLE | 63 -> 83 deg up, 20 -> 31.5 deg down. Azimuth was already unlimited. | **BUILT + VERIFIED `7341352d7`. AWAITING HIS DRIVE.** |
| 3 | THE TRACK READS | The whitewash defect. See below. | ★ **SIGNED BY CHAD 2026-08-28. `SEADS_TRACK_PACK` ships 0.45.** |
| 4 | ROOST | 20-40 ft snow plume off the belt, speed-keyed. | ★ **SIGNED BY CHAD 2026-08-28 at ship dials.** |
| 5 | EXHAUST | Throttle-keyed grey smoke off the machine. | ★ **SIGNED BY CHAD 2026-08-28 at ship dials.** |
| 6 | RIDER BREATH | Cold-air puff, periodic, at the rig's head. | ★ **SIGNED BY CHAD 2026-08-28 at ship dials.** |
| 7 | SNOW SHADING | General pass -- "looks decent, could be a little better, a little more immersive." | **OPEN -- and probably ABSORBED. See below.** |
| 8 | ★ SUN SPARKLE | **Chad: "1 is the winner."** Glint/glitter on UNDISTURBED snow only. See below -- this is row 3's partner, not a separate cosmetic. | ★ **SIGNED BY CHAD 2026-08-28 at ship 1.0.** |
| 9 | ★ SHADOWS ON SNOW | **Chad: "would go far with #2 and the sudburians and sled's own shadow."** Aircraft + Sudburians + THE SLED ITSELF. See below -- there is currently NO shadow system in this renderer at all. | **BUILT: mechanism + aircraft + sled (measured outline). Enemies deferred.** |

---

## ROW 3 IS THE REAL ONE. THE FINDING:

`world/tracks.h:122` exposes `compaction_at(dir)` -- packed-snow density under the belt.
It is implemented. It is unit-tested (`test/unit/test_tracks.cpp:24`: "compaction_at
reduces reported depth and is never negative").

**AND NOTHING IN THE RENDERER READS IT.** Confirmed by grep across the tree: the only
consumers of TrackField are `render/snow_patch.cpp:199` and `app/main.cpp:4248`, and both
take `deform_at()` -- the GEOMETRY -- only.

So the cut has SHAPE and zero TONAL response. In broad daylight, white-on-white, shape
alone carries no contrast: a high sun casts no shadow into a 0.55 m groove, and the
groove's own normals barely differ from the field's. That is the whole of "its a bit
whitewashed atm". It is the SHADING gap `_R4.md` predicted when Chad accepted the floor.

**Chad also ruled the FORM of the fix (his option 4): the cut should read SMOKIER --
softer and hazier -- NOT as a hard geometric groove.** That points at the physically
honest answer rather than a darkening hack: snow is translucent, deep snow scatters
BLUE, and a compacted groove scatters less and reads cooler and denser than the powder
beside it. So the cut should be tinted and desaturated by COMPACTION, not merely
darkened by an ambient-occlusion term.

⚠ This row must not raise `min_depress_m` (0.55). The floor is Chad-signed and the
handoff is explicit that the remaining gap is shading, not depth.

---

## THE FENCES THIS RUNG MUST NOT BREAK

Carried from R3/R4/R4b. Each cost a defect to learn.

1. `full_depth = 0.30f`, `barren_shed_keep = 1.0f` (`render/planet.h`) -- SIGNED, and 0.30
   is LOW ON PURPOSE. Do not raise it.
2. NO never-fill-in guard in `TrackField::add`. It became a session-wide running max.
3. The rider patch sits on `facet + draw_fold_at`, NOT the bare facet. The fold is NOT
   rim-faded. **Fork detector: drawn-minus-driven stays at `lift_m` = 0.150 m.**
4. Shape comes from the NEAREST distance; amplitude is BLENDED. Two different things.
5. Untracked ground must stay BIT-IDENTICAL -- `deform_at` returns exactly 0.0 off-track
   (`test_tracks.cpp:64`). Any shading term added in row 3 inherits this: zero compaction
   must produce a bit-identical pixel, or the whole world shifts under him.

## THE GATE

1565 cases, 1560 pass / 5 fail, `REAL_EXIT=42` -- the 5 are PRE-EXISTING (`6a3df8865`).
Any NEW failure is this rung's, and no row ships with one.


---

## ROW 8 -- SUN SPARKLE. WHY IT IS ROW 3'S PARTNER, NOT A COSMETIC.

Undisturbed snow is faceted crystals and it GLITTERS. Snow packed under a belt
is not, and does not. So if the virgin field sparkles and the track does not,
**the cut reads by CONTRAST without being darkened at all.**

That matters because the naive fix for row 3 -- darken the groove with an
ambient-occlusion term -- makes a clean white field look grubby, and Chad
already ruled (option 4) that he wants the cut SMOKIER, not dirtier. Sparkle
puts the contrast in the FIELD instead of in the CUT. It makes row 3 easier.

Sparkle must be keyed off the SAME `compaction_at()` signal row 3 wires up:
compaction 0 sparkles, compaction 1 does not. One signal, two consumers.

⚠ Inherits the bit-identical fence (fence 5): zero compaction on untracked
ground must still produce the shipped pixel. A sparkle term that fires
everywhere changes the whole world; it has to be a stable, view- and
sun-angle-dependent function, NOT per-frame noise, or it will crawl and boil
when he drives -- which reads as video artefact, not snow.

## ROW 9 -- SHADOWS ON SNOW. ★ THE FINDING: THERE IS NO SHADOW SYSTEM.

Grepped the whole tree. Nothing in this renderer casts a shadow. The only
"shadow" hits are `render/sled_model.cpp:416` (a comment about a shaded SIDE),
`render/planet.cpp:406` (an albedo term for self-shadowed blast rock), and
`render/post.h:25` `split_shadow` (a colour-grade tint). **No shadow map, no
depth pass, no projection.** The machine currently floats on the snow.

So this row is not a tweak, and it has two honest shapes:

**(a) A real shadow map.** Correct everywhere, costs a depth pass over a
planet-scale scene, touches the whole renderer, and brings cascade/acne/peter-
panning work with it. This is a rung of its own, not a row.

**(b) PROJECTED SHADOWS ONTO THE SNOW ONLY.** The ground here is an ANALYTIC
surface -- planet sphere + snowpack + the R1 fold -- so a caster's silhouette
can be projected straight onto it along the sun vector without a depth buffer.
On a flat white field this is most of the value of (a) for a fraction of the
cost, and it covers exactly the three casters Chad named: aircraft, Sudburians,
sled.

**Recommend (b), and put both to him.** The reason (b) is not a cheat here:
a moving shadow crossing white snow is the most readable threat cue that
exists, and it is the ONLY cue that works for an aircraft BEHIND him -- the
exact case row 2's freelook cannot save him from. The sled's own shadow is
what stops the machine floating, and it gives the eye a depth reference right
beside the groove, which helps row 3 read.

⚠ If (b): the shadow must land on `facet + draw_fold_at`, NOT the bare facet.
This is fence 3 and it has already cost one defect this rung-family -- the
rider patch drew 0.56 m under the world for exactly this reason. A shadow
projected onto the unfolded surface will float or sink by the same amount.


---

## ROW 3 -- THE SCOUT, INCLUDING THE LEAD THAT DIED

**THE CHANNEL.** The planet shader has exactly two vertex texcoord channels:
`.x -> vRim` (snow-patch rim weight, `planet.cpp:96`) and `.y -> vSnowDepth`
(ambient snowpack metres, `planet.cpp:97`). The PLANET mesh writes `(0, depth_m)`
(`planet.cpp:736`); the SNOW PATCH writes `(rim_weight, 0.0)`
(`snow_patch.cpp:276`).

★ **A LEAD THAT LOOKED LIKE A DEFECT AND IS NOT.** That the patch writes depth 0
while the surrounding planet mesh writes the real depth (map p50 0.77 m) looks
exactly like a shading discontinuity on the most-looked-at pixels in the game.
**It is not.** `planet.cpp:1549` deliberately forces `uSnowDepthMix = 0.0f` for
the patch draw, and the comment says why: a zero depth under the ARMED exposure
would paint a bare-ground rectangle on the ground directly under the rider.
Intentional, reasoned, and it must stay. Checked before reporting, per the R4b
lesson (*re-run the experiment, do not defend the reading*).

**And it is what makes row 3 cheap:** because `uSnowDepthMix` is pinned to 0 on
that pass, `texcoord.y` is genuinely INERT on the patch, so it is free to carry
per-vertex COMPACTION with no risk whatever to the planet pass.

**THE LOOK IS ALREADY RULED IN THIS REPO. DO NOT RE-DERIVE IT.**
`render/ribbons.h:37-47` solved this same problem for the groomed trail:

> "deliberately DARKER and BLUER than the wild snowpack (snow_albedo 0.92), not
> brighter. Packed snow really is denser, bluer and slightly darker than fresh
> cover -- and a WHITE trail on a WHITE world is LESS legible than the clay it
> replaces, which would have shipped a regression that looked like the fix
> (S1_SPEC RT-1). The trail reads by value contrast + corduroy TEXTURE, never by
> being bright." -- `trail_winter_color{0.80f, 0.84f, 0.90f}`

Same physics, same white-on-white problem, already signed. The track is the
ungroomed sibling of that trail. ⚠ A later agent's instinct will be to make the
track BRIGHTER so it "stands out". That is the exact regression S1_SPEC RT-1
names.

## ROW 9 -- ★ CHAD ADDED HIS OWN AIRCRAFT, AND IT IS NOT COSMETIC

*"my own airplane shadow would help my landings"* (2026-08-28).

**This upgrades row 9 from immersion to INSTRUMENT.** Closure with your own
shadow is how a pilot judges flare height, and over an unbroken white snowfield
there is almost no other height cue in the last few metres -- no texture
gradient, no parallax, no scale reference. He is describing a real depth-cue
deficit in the landing task, not a look.

Consequence for the option split: it argues for **(b) projection onto the snow**
even harder, because (b)'s weakness is shadows on complex geometry and its
strength is exactly this -- a caster over a broad, smooth, analytic snow
surface. The landing case is (b)'s best case.

★ It also means row 9 needs a MEASUREMENT, not just a look: does the shadow
appear at a useful height, and does its closure rate read in the last 5 m? A
shadow that only resolves below 2 m is too late to flare on. Build that probe
before ruling the row.

**The four casters, in the order they earn their place:**
1. His own AIRCRAFT (a landing instrument -- above).
2. The SLED (stops the machine floating; gives the eye a depth reference right
   beside the groove, which helps row 3 read).
3. ENEMY AIRCRAFT (the only threat cue that works for an attacker BEHIND him --
   the case row 2's freelook cannot save).
4. The SUDBURIANS (world life; cheapest once 1-3 exist).


---

## ROW 3 IS SIGNED. THE SHIP VALUE IS 0.45, AND HE CAME DOWN TO IT.

Chad flew the 0.65 candidate, asked for 0.45, flew that, and ruled:
*"I like it lets move on."*

**The direction of the correction is the lesson.** The candidate was chosen to be
clearly legible and he moved it DOWN. At 0.65 a floor-depth rut runs about two
thirds of the way to the full packed colour and starts reading as a painted
stripe; 0.45 keeps the smoky band and hands more of the total to the sqrt
skirt -- which is the SOFTNESS he asked for when he said "a bit o smoke".

⚠ **Do not raise it back toward 0.65 on the argument that the track would be
"more visible".** More visible is not the goal. This is the third time in this
rung-family that the right answer was the LOWER number (`full_depth` 0.30 over a
red team's push upward; the camera fix being cam_ahead rather than cam_dist;
now this). `ribbons.h` / S1_SPEC RT-1 is the standing warning about the
brighten-it instinct.

⚠ **HONEST CAVEAT ON THE PROVENANCE.** Two `seads.exe` instances were live during
that sweep -- a stale one outlived a `taskkill` -- and he ruled from *"the one
that is open ... I think it is at 0.45"*. He was told before he ruled and ruled
anyway, so the value is his. But the provenance is ONE NOTCH WEAKER than the R3
and R4 rulings, which were single-instance. **If this ever reads wrong, re-sweep
the ladder before suspecting the code.**

★ PROCESS FIX FOR THE REST OF THIS RUNG: kill and verify a single instance
BEFORE handing him an arm. `taskkill //F //IM seads.exe` then confirm the
process list is empty. A sweep whose arm the driver cannot name is the same
failure as an A/B whose arms are indistinguishable.


---

## ROWS 4/5/6 ARE SIGNED, FIRST PASS, AT SHIP DIALS.

Chad drove the ship values (`SEADS_ROOST`/`EXHAUST`/`BREATH` all 1) and ruled:
*"that looks perfect."* No dial sweep was needed -- the first arm landed.

★ **THE REASON IT LANDED FIRST TRY IS WORTH KEEPING.** The roost was not tuned
to look right; its length is `life x speed` by construction (6 m at 11 m/s,
12 m at 22 m/s = his 20-40 ft), and its intensity comes off `roost_flux`, a
kernel field that has existed since S5 was planned. When the number already
means the right thing, the look falls out. Compare row 3, which needed a
seat sweep because the tint strength had no physical anchor.

⚠ **HONEST NOTE ON THE OCCLUSION QUESTION.** The open worry was whether the
roost hides the row-3 track for its 0.55 s life. He was TOLD to watch for it
and then said "that looks perfect", so this is a pass -- but it is a pass by
ABSENCE of complaint, not by him being asked the question and answering it
directly. If the track ever reads worse at speed than parked, this is the
first suspect, and `SEADS_ROOST=0` is the one-command proof arm.


---

## ROW 8 IS SIGNED. *"good I sign the sparkle too"* -- ship 1.0, first arm, no sweep.

Second row this rung to land on the first arm (rows 4/5/6 were the first), and
for the same underlying reason: its amplitude was ANCHORED to something that
already existed -- the shipped NIGHT sparkle -- rather than invented. Rows that
inherit a number that already means something land; rows that invent one need
his seat. That is now three data points and it should shape how the remaining
rows are specced.

## ROW 9 -- THE CONSULT LANDED, AND IT OVERTURNED MY RECOMMENDATION

A fresh-context Fable consult was run against my option (b). **It was right and
I was wrong**, which is the third time in this rung-family the standing lesson
(*re-run the experiment, do not defend the reading*) has paid out.

**MY ERROR: I called the ground ANALYTIC. It is not.** It is a DEM
`HeightField` interpolated through mesh facets (`render/sphere_param.h:186`)
plus the fold, plus track deform, plus a LIFTED patch. Deterministically
queryable is not the same as analytic.

**AND THE CONSEQUENCE IS EXACTLY FENCE 3.** `render/snow_patch.h:78`: the rider
patch floats `lift_m` = **0.150 m ABOVE** the folded planet surface. So a shadow
decal draped correctly on `facet + draw_fold_at` lands **15 cm UNDER the patch
the sled is standing on** -- the SLED'S OWN SHADOW buried precisely where it
matters -- unless the drape logic forks per surface, which is the defect
factory that already cost this rung-family 0.56 m of rider patch.

### ★ THE RULING: OPTION (c), NOT (a) AND NOT (b).

**(c) = receiver-side analytic occluder proxies.** A small uniform array of
caster shapes (capsules/spheres/discs) evaluated PER-FRAGMENT in the receiving
ground shader; each fragment casts a ray at the sun and tests the proxies. No
depth buffer, no decal, no drape.

**Why it wins:** whatever surface actually drew that pixel receives the shadow
at ITS OWN position. Folded planet mesh, lifted patch, tomorrow's geometry --
all correct without knowing about each other. **The 0.56 m-under-the-world
failure class becomes UNREPRESENTABLE rather than merely avoided.** And
VERIFIED: `draw_snow_patch` draws with the PLANET'S OWN SHADER, so one shader
edit covers mesh and patch with no seam at the patch rim.

### ⚠⚠ THE FINDING THAT COULD KILL THE ROW -- AND IT IS NOT A RENDERING PROBLEM

`config/world.toml:1062`: **`day_period_s = 300.0`** -- a FIVE-MINUTE day. The
rider sits essentially on the celestial equator, so **roughly half of every
five-minute cycle is night with no sun shadow at all.** A 60-90 s approach can
begin with a usable shadow and end in the dark.

**No shadow implementation fixes this.** It is a property of the world, and the
levers are Chad's: lengthen `day_period_s` (the config comment already calls it
a fly-dial), or accept an intermittent instrument. **Surface it; do not choose
for him.**

### ★ I CAUGHT AN ERROR IN THE CONSULT. THE SUN IS 4x BIGGER THAN IT ASSUMED.

It computed penumbra from `sun_angular_diameter_deg = 0.35`. The shipped value
is **1.40** (`config/world.toml:1068`), and the comment says *"Chad: 2x again =
4x orig"* -- he deliberately enlarged it. So shadows are **4x SOFTER** than the
consult's numbers, and its claim that the shadow "resolves as a distinct object
from several hundred metres" is **wrong**: at 300 m the penumbra is ~7.3 m
against a ~10 m wingspan, i.e. mostly blur. It sharpens to ~1.2 m at 50 m and
~0.37 m at flare height.

That may actually be GOOD -- a shadow that crisps as you descend IS the altitude
cue -- but the row must not be designed off the wrong number.

### ★ THERE ARE NO SUDBURIANS. CASTER 4 IS CURRENTLY UNBUILDABLE.

Grepped for townspeople / pedestrians / civilians / NPC people: **nothing in the
tree.** The only Sudburian is the rider on the sled. The ledger's earlier
"cheapest once 1-3 exist" is true but the precondition is a caster that does not
exist yet. Tell him rather than quietly dropping it.

### BUILD ORDER, ONCE THE PROBE SAYS GO
1. Mechanism + HIS OWN AIRCRAFT (the justification; proxy dims MEASURED from
   `aircraft_node_specs()` box_dims, per the NO GUESSING law).
2. The SLED (free-rides the shared patch shader; fence 3 just works).
3. ENEMY aircraft (same proxies per drone, CPU-culled to the nearest few).
4. Sudburians -- when they exist.

⚠ Bank strips and ribbons have their OWN shaders: the sled's shadow will BLINK
OFF crossing a groomed trail until the same term is added there. Mechanical, but
real scope -- sequence it right after the sled if the pop reads.

⚠ FENCE FOR THIS ROW: with zero casters in range the shadow factor must be
EXACTLY 1.0 -- an early-out on caster count, not a multiply by ~1.0 -- or every
pixel on the planet moves. Pin it the way row 3 pinned its tint.


---

## ★ CHAD'S TWO ROW-9 RULINGS, 2026-08-28. BOTH ARE CLOSED.

**1. `day_period_s` STAYS AT 300.** *"LEAVE AT 300"*. He was given the exact
number that would buy a longer window (0.419 x day_period_s; 429 s buys a 180 s
window) and declined it. ⚠ **Do not re-propose lengthening the day** as a fix
for anything shadow-related. He is accepting a six-finals-per-window instrument
to keep the world's 5-minute rhythm, and that rhythm is a bigger design fact
than this row.

**2. BUILD ORDER: GO.** Mechanism + his aircraft, then the sled, then enemies.

### WHY THE ROW SURVIVED ITS OWN PROBE -- AND WHY THE FIRST FRAMING WAS WRONG

The consult and I both framed the 5-minute day as possibly FATAL. **It is not,
and the arithmetic is embarrassingly simple:** a 3 deg final from 50 m at
48.9 m/s is **19.5 SECONDS**. The usable (>15 deg) window is **125.8 s**. That
is SIX finals per window. The cue was never marginal -- what he actually loses
is the freedom to *start* an approach at an arbitrary moment.

★ LESSON: "intermittent" was a scary adjective standing in for a number. The
probe cost one agent-hour and turned a maybe-fatal into a comfortable margin.
**Price the fear before you design around it.**

### THE PROBE'S NUMBERS ARE NOW THE DESIGN SPEC

| | |
|---|---|
| sun above 0 deg | 50.0% of all time, 151.0 s windows |
| sun above 15 deg | 41.3%, **125.8 s** longest, 12/yr |
| shadow enters frame | 8.7-50 m at elev >=10 deg, cross-sun or astern |
| closure, last 5 m | 3-26 deg/s (50-913 px/s) |
| penumbra at flare | 0.06-0.58 m vs a 10.10 m span |
| penumbra beyond ~413 m slant | swamps the shadow -- a smudge |

### ★ AIRMANSHIP THAT FELL OUT OF THE GEOMETRY (keep this; it is content)

- **CROSS-SUN approach = widest margin.** Best separation, earliest entry.
- **SUN ASTERN** = visible from 50 m but the shadow sits nearly ON the aircraft.
  Weak separation, weak cue.
- **SUN DEAD AHEAD at <=15 deg = the one geometric failure.** Shadow enters at
  2.1-5.4 m, too late to flare on. **Never land into a low sun.**

A real procedure the game teaches by GEOMETRY rather than by a tooltip. That is
the signature of a mechanism modelling something true instead of faking a look
-- the same signature the roost had when it landed on the first arm.


---

## ROW 7 IS THE ONLY THING LEFT, AND IT MAY NOT EXIST ANY MORE

Chad's original words: *"the snow looks decent could have a little better
shading a little more immersive."* He said that BEFORE rows 3, 8 and 9 all
landed in that same shader.

**My reading: row 7 has been eaten.** The track tint, the sun sparkle and the
shadows are all "better snow shading, more immersive", and they were built
against stated goals. A general shading pass now would be UNANCHORED work
touching four Chad-signed rows for no named defect -- and this rung has three
data points saying unanchored numbers are exactly what needs his seat.

**Do not open row 7 on your own initiative.** Ask him what is still missing and
build to that, or close it as absorbed. It is the one row with no measurement
and no ruling behind it.

## ★ THE DESIGN LESSON OF THE WHOLE RUNG

Three rows landed on the FIRST arm with no sweep (4/5/6 roost, 8 sparkle) and
two needed his seat (3 tint, 9 sled outline). The difference was not luck:

| row | its key number | outcome |
|---|---|---|
| 4/5/6 roost | `roost_flux` -- a kernel field that already meant this | first arm |
| 8 sparkle | amplitude anchored to the shipped NIGHT sparkle | first arm |
| 3 tint | strength INVENTED | needed his sweep, and he came DOWN |
| 9 sled | proxy shape INVENTED (a bounding capsule) | he rejected it on sight |

**A row that inherits a number which already means something lands. A row that
invents one needs Chad.** Spec accordingly: before building, go find whether
the quantity already exists in the kernel or in a sibling feature. Twice this
session it did (`roost_flux`, the night sparkle) and nobody had noticed.


---

## R4a CROSS-BRANCH EXCHANGE (2026-08-29). THREE THINGS CHANGED.

Full text: `docs/PACKET_TO_R4A_from_snow_R5_reply.md` (mine) and R4a's
`docs/PACKET_TO_SNOW_R5_from_r4a.md` on `sandbox/r4a-phase0`.

### 1. MY PACKET WAS SCOPED WRONG, AND R4a CAUGHT IT BY DIFFING

I told them "render/ reads state, writes nothing". TRUE of my rung -- verified,
`git diff 9fdca369a..HEAD -- sim/ control/` is EMPTY. **FALSE as a description of
what lands on them:** `main...HEAD` carries `sim/sled.cpp` +111 / `sim/sled.h`
+113 (the G1 gyro term, from `b8e430415`, an earlier session on this branch).

★ **AN INTER-BRANCH PACKET MUST BE SCOPED TO THE MERGE, NOT TO THE AUTHOR'S OWN
COMMITS.** I wrote about my rung; what crosses into their world is the BRANCH.

### 2. ★ THE RIDER SHADOW'S 0.25 m RADIUS IS MEASURABLY WRONG -- QUEUED, NOT DROPPED

`rider_radius = 0.25` was flagged in-code as the one unmeasured number in the
shadow system. **It did not have to be.** R4a's `render/body_chain.h` is a
17-station table generated by
`assets/character/sudburian_src/measure_body_chain.py`, which reads
`assets/sled/indy650.glb` -- 44 skin joints, 34878 DRAWN vertices, oriented
boxes of the CLOTHED surface. Exactly the quantity.

Their reading: shoulder half-width **0.2460**, spine_03 0.1457, spine_02 0.1403,
spine_01 0.1278, pelvis 0.1477. **So a single 0.25 m capsule is shoulder-width
from neck to hips** -- within 4 mm at the top, ~1.8x too wide at the waist. Under
Chad's "large scale, really noticeable" ruling that is a visible defect and the
shipped shadow has it.

⚠ **DO NOT HARDCODE THOSE NUMBERS FROM THIS LEDGER.** `body_chain.h`'s own law is
"re-run the script and paste; never retype a number" -- and R4a's own first read
was misaligned by FIVE STATIONS (it nearly reported the elbow as the shoulder).
`body_chain.h` and the script **do not exist in this tree**; they arrive with
R4a's rebase.

**The GLB is byte-identical across both trees (`sha256 51082acbdcb47cc6...`), so
the measurement is valid here -- the moment the generator lands, run it.**

Also note the fix is NOT a constant swap: a capsule has ONE radius, so a taper
needs a second stacked capsule (**caster budget 9 -> 10 of 16**, eating room the
enemy aircraft need) or a truncated-cone primitive in the shader. **A design
decision with a cost, on a row Chad has not flown. Put it to him.**

### 3. HALF OF THE RIDER-SWAP WARNING IS RETIRED

R4a's measured `pelvis` station centre is **z = -0.5848**; my legacy-derived rest
pelvis z is **-0.584840**. Same number. **That constant SURVIVES the rider swap**
-- one less re-anchor, and the handoff's warning is narrowed accordingly rather
than left scarier than the evidence supports.

**STILL OPEN: the seated helmet crown (mine reads 1.25 m).** Their body-chain
table is bounded by the seat pan and tops out at the shoulder (y 1.1206), so it
can neither confirm nor refute it. They will measure it off the same GLB on
request -- asked, with provenance, incl. which of `sled_helmet_dent_set`'s four
baked dent variants is canonical. **It is the LAST legacy-derived number in the
rider shadow.**

### 4. THE MERGE, AND A SHARED LESSON WORTH KEEPING

Chad has ruled **snow pushes to main FIRST**; R4a holds and rebases after.
Expected textual conflicts: `rider_pose.h`, `app/main.cpp`, `render/draw.{h,cpp}`,
`render/sled_model.{h,cpp}`, `CMakeLists.txt`, `generated/graph/*`. None semantic
-- their mechanism files (`rider_load`, `body_chain`, `body_drive`, `trail_chain`,
`body_blend`) are untouched by us and ours by them.

⚠ One conflict has a RIGHT resolution: in `render/sled_model.cpp` I DELETED a
duplicate sag0 env-read lambda and pointed it at the shared `sled_sag0_m()`.
**Keep the shared call; do not restore the local lambda.**

★ **GATE ON THE THING THE OTHER LANE CANNOT CHANGE.** R4a nearly keyed their rider
stage machine on velocity/force. They gated on the kernel's CONTACT clock
instead, so this branch's `k_gyro` -- which changes how the machine rolls --
cannot false-fire it. An attitude- or acceleration-keyed trigger would have moved
under our work silently and neither lane would have connected a snow render rung
to a rider stage machine. Contact is a kernel fact; attitude is downstream of
anything that touches torque.

**Baseline note:** `sled_assist_reference_plane_is_load_weighted` and
`sled_debug_sink_is_write_only` are PRE-EXISTING GI4 debts (red on the enemy-AI
branch for weeks), NOT attributable to snow's `roll_tq[]` 8 -> 9. If they move
after the rebase, that is real and worth chasing.

---

## THE MERGE ONTO MAIN — ANALYSED 2026-08-29, NOT YET DONE

R4a landed first: `main` is at **`c8d88d325`**. From `sandbox/snow` we are
**41 ahead, 59 behind**. Dry-run (`git merge-tree`, worktree untouched):

| file | conflict | resolution |
|---|---|---|
| `generated/graph/*` (7 digests + graph.json) | yes | **REGENERATE** — `python tools/graph/graphify.py`, never hand-merge |
| `app/main.cpp` | yes | real, both lanes added to the drive block |
| **`sim/sled.cpp`** | **yes — THE KERNEL** | ★ see below |
| `tools/sled_probe.cpp` | yes | real |

★ **`render/sled_model.cpp` and `render/rider_pose.h` did NOT conflict**, which is
better than R4a predicted. The `sled_sag0_m()` resolution stands unopposed: keep
the shared call, do not restore the local lambda.

### ⚠ THE KERNEL CONFLICT IS THE ONE THAT NEEDS CHAD

**Both lanes edited `sim/sled.cpp`**, which CLAUDE.md fences ("NEVER touch
`sim/`/`control/` from the world thread" — the kernel firewall).

- **main** brings five R4a commits there: the rider-load instrument measured on 27
  tapes, self-right v3 (the PUMP), v4 (standing does the shift), and
  "LEAN IS THE THROW, STAND IS THE RIGHTING".
- **ours** brings `b8e430415` (the G1 gyro / rotor precession) and `27532eded`
  (SK-1a bank-strike), both from EARLIER sessions on this branch, not from R5.

Neither is R5's work and neither is mine to arbitrate at 02:30. **Resolve this one
with Chad, in daylight, and re-run the full gate afterwards** — his standing law is
one dial at a time with him flying each, and this is two lanes' kernel edits meeting
for the first time.

**Do not resolve it by taking one side wholesale.** The gyro is Chad-ruled ARMED;
R4a's self-right is Chad-ruled and driven. They are additive in intent.

---

## THE MERGE IS DONE, AND THE GATE IS CLEAN

`sandbox/snow` merged `main` (R4a's contact ruling + their owed graph regen).
**44 ahead, 0 behind.**

**FULL GATE: 1674 tests, 1669 pass, 5 fail — and the five are EXACTLY R4a's known
baseline reds:**

| # | test |
|---|---|
| 62 | probe P-F: the relentless raider keeps the pump and shoots back |
| 911 | `sled_slides_before_it_tips_on_flat_snow` |
| 912 | `sled_grip_ceiling_stays_below_the_tip_threshold` |
| 950 | `sled_assist_reference_plane_is_load_weighted` |
| 953 | `sled_debug_sink_is_write_only` |

**ZERO new failures.** R4a's pre-merge baseline was 1613/1618 with these same five;
the suite grew by 56 cases across both lanes and not one of them went red. The GI4
sled debts (950, 953) are confirmed pre-existing on the enemy-ai branch and are not
attributable to snow's `roll_tq[]` 8 -> 9.

Shader check (nothing headless compiles the GLSL): **20 programs, zero failures**,
snow fold on, hero GLB wired at 218 nodes / **181 prims** — 180 before the merge, so
R4a's rider work is present in the running binary, not merely in the diff.

### ★★★ THE GATE FOUND WHAT EVERY FILTERED RUN THIS RUNG COULD NOT

The first gate run showed **seven** red. Two were mine, and they were not a merge
regression — they reported **"No test cases matched"**. The row-9 `TEST_CASE` names
contained an **EM-DASH**; `catch_discover_tests` round-trips the name through ctest,
the Windows codepage mangles it, the filter matches nothing, and the case fails as
no-tests-ran.

**This is a documented CLAUDE.md lesson, verbatim, from S2.**

**So the row-9 tests had NEVER RUN UNDER THE GATE.** They passed all session because
they were only ever invoked with a direct filter, which matches the literal string.
Every "114 assertions green" was true and was certifying nothing through ctest.

⚠ **A GREEN FILTERED RUN IS NOT A GREEN GATE.** Any new `TEST_CASE` name must be
ASCII, and a new test is not verified until it has run **through ctest at least
once**. Three confident-but-empty signals appeared in this rung-family within one
day — R4a's loader answering an unasked question, `helmet_top` wearing a MEASURED
label it had not earned, and this. The pattern is not carelessness; it is that
**each signal was checked in the mode that could not see its own gap.**
