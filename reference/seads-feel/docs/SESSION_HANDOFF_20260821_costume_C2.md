> ⚠ **SUPERSEDED for the garment surface, 2026-08-22.** Chad rejected the
> approach in this file and in its §11. The live rung is now
> **`docs/SESSION_HANDOFF_20260822_sweater.md`** — the body has no shoulder
> girdle, and that is why. §3 (measurements), §5 (the open scarf-parent
> question) and §9 (traps) here are still true and still worth reading.

# HANDOFF — costume rung: C1 (body) + C2 (garment T2) BUILT, NO HOOD, AWAITING CHAD'S EYE

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`,
then §0–§3 of this file. The body has been re-authored (rung C1) and the garment
re-cut onto it (T2). Both are applied in the LIVE Blender session and NOT saved.
Nothing is committed. Start at §5."*

**Supersedes `docs/SESSION_HANDOFF_20260821_costume_C1.md`.** That file's §3
(measurements) and §9 (traps) are still true and still worth reading; its §6
plan is now executed for C1 and C2.

---

## 0. STATE IN ONE LINE

The grey box-and-prism body is gone — replaced by a shaped, lofted figure with
a **butt and a gut** (`body_geom.py`) — and the sweater / snowpants / boots are
re-cut onto it (`costume_geom.py`, T2), with **the sweater now covering the
whole torso** and **NO HOOD** (ruling E, after four rejected attempts: §3).
Every generator gate is green, the live containment measurement says **0 of 908
scarf verts inside any garment shell** and **nothing is bare but the head
(helmet) and the neck (inside the scarf ring)**, and nothing new touches the
machine. ⚠ ONE BIG THING IS OPEN AND IT IS NOT THE COSTUME — see §5, the scarf
is parented to the wrong bone.

---

## 1. CHAD'S RULINGS THIS SESSION (verbatim, in order)

> **D1** — "Yea shape to body parts nicer so the cothes hang natural lookin,
> make it look realistic, get the hood right same ask as the other failed
> attempts, I want someting that looks of believeable quality. Make it simple
> and well done, how I asked for the scarf to the letter. scarf know covered by
> hood at the back and tails coming out of the bottom of the hood and flying out
> from there."

> **D2** — "joints should be occluded., no open space betweeen limbs"

> **D3** — "hood is upside down and I see extra se of arms under the good arms"

> **D4** — "the hood is open at the bottom. A hood is like a pocket that your
> head can fit into not a sleve, so please make it a closed bottom pocket"

> **D5** (answering the conflict in §3) — the pocket with a scarf notch.
> *Built, and it did not work — see §3.*

> **D6** — "still you closed the wrong end. Gotta close the other end and cant
> leave any hole in the bottom … A hood is like a pocket that your head can fit
> into not a sleve"

> **D7**, with a photo of parka hoods from behind — "it covers over the knot but
> the tails could just come out between like fabric layers , we diont need a big
> space for a tail of a scatf dfo we?"

> **★ E — THE RULING THAT CLOSED IT** — "just go with no hood and please make
> the swater longer to cover the whole torso give the sudburian a butt and a
> gut."

> **F** — "in blender it (the scarf tails) are going into the sweater back not
> resting on top of the barrier."   → §5, and it is a RIG defect.

Still in force from before: TWO-PIECE hoodie + snowpants; GREY hoodie, BLACK
snowpants, kComplementBlue stripe on the outer arm and outer leg; quality
target T2 = authored mesh (+ UVs and textures at C4); the scarf is SIGNED and
is not touched.

---

## 2. WHAT WAS BUILT

### C1 — the body. `assets/character/sudburian_src/body_geom.py` (NEW)
Replaces the three boxes and the eleven constant-radius prisms with one lofted
torso and shaped limbs. **1788 verts / 3536 faces**, up from 264 / 162. The
file's own banner carries the five laws it obeys; the two that matter most:

* **Shape is added by REMOVING.** Every section is INSIDE the old volume it
  replaces (torso ≤ the old boxes, forearm 0.0705 vs 0.07496, thigh 0.1190 vs
  0.11937, calf 0.0950 vs 0.09501, neck 0.0770 vs 0.0800). Shrinking cannot
  introduce a new penetration, so the seat fit, the scarf keep-out and the
  helmet fit survive **by construction** rather than by re-measurement.
* **The ONE growth is the deltoid, and it is a TUBE.** The old body had a
  MEASURED 116 mm hole at the armpit (z 1.30) and only a 2 mm bridge at z 1.40:
  the chest box stopped at |x| 0.219 and the arm prism did not start until
  0.264. The sleeve root now carries r 0.099. It must be a tube, never a ball —
  a ball of r 0.098 at the shoulder joint comes to cylindrical radius 0.112 at
  z 1.5167, inside the scarf band's wall. **A ring perpendicular to the arm
  axis cannot reach the band at ANY radius** (proof in the file's L3, and it is
  radius-independent — that is the licence for a 0.112 sleeve where T0 capped
  itself at 0.090).

Sections are **superellipses, power 2.30**, and the garment uses the same
power, the same sample count and the same phase — so "the garment contains the
body" stops being a sampled test and becomes an identity: hx_g ≥ hx_b and
hy_g ≥ hy_b, checked at the union of both tables' breakpoints. Both curves are
piecewise linear, so non-negative at every breakpoint is non-negative
everywhere. **That is the whole unlock of C1**, and it is why the garment can
now taper (a real wrist, a real ankle) where T1 could not.

### C1 apply — `apply_body_live.py` (NEW)
Patches the live mesh; never regenerates the man. **The old claim that the live
body "carries hand tuning no generator reproduces" was TESTED, not assumed:**
the first 3082 live vertices against `sudburian_proxy.geometry_only()` differ in
**897 of them, worst 108 mm, with 1240 faces in a different order.** The claim
is true. So the script strips grey faces whose vertex group is in the replaced
set, appends the new shells, and asserts bit-for-bit that the head box, both
foot boxes, every mitt, scarf and costume vertex and the material slot order
are untouched. It also asserts the five bones it builds against have not moved.
**The head and feet are deliberately NOT regenerated** — the helmet family and
the boot were fit to them by hand.

### C2 — the garment. `costume_geom.py` rewritten as T2
**5936 verts / 11824 tris / 13 pieces** (T1 was 5724 / 11376 / 20, and the
four-attempt hood alone was up to 1792 / 3584 of that). Census green per shell,
all ten rulings green, three of them MESH clauses. T0 and T1 are preserved beside it as
`costume_geom_T0_rejected.py` and `costume_geom_T1_rejected.py`.

* **NO HOOD.** Ruling E. §3 records the four attempts and why it stopped, so
  that nobody rebuilds one from ruling A without reading the history. What
  ruling A asked the hood to do about the scarf is moot; what ruling B asked
  the COLLAR to do still stands and the collar still does it.
* **THE SWEATER COVERS THE WHOLE TORSO.** Hem at **z 0.9100**, below the body's
  own bottom cap at 0.9200, so one garment runs hip to shoulder and the
  snowpants become the under-layer they always should have been. The clause
  `"the sweater covers the WHOLE torso"` proves it over z 0.9200..1.5150.
* **A BUTT AND A GUT**, which is a change of SECTION, not of dials: torso rows
  are now `(z, hx, hy_front, hy_back)` and `sellipse_ring_fb()` draws a
  superellipse with a different half-depth front and back. A gut is a belly
  that comes forward while the back stays put; a symmetric section cannot say
  that. The belly stands **+28 mm** proud of the chest at z 1.180 and the seat
  **+37 mm** at z 0.980.
* **The containment identity survives the asymmetry** — three inequalities
  instead of two (hx, hy_front, hy_back), still checked at the union of
  breakpoints, still a proof rather than a sample.
* **No armpit gusset.** There was one for exactly one round; D3 killed it, and
  he was right — a 210 mm tube of r 0.12 from the ribs to the arm IS a second
  arm. It was also an over-reading: "spaces between limb parts" means the parts
  OF a limb, the gaps where three capped cylinders butted on their joint
  planes. Those are closed by OVERLAP (every distal shell starts above its
  joint, every proximal one ends past it), not by bridging the armpit.
* **The boot is the only rounded RECTANGLE in the file**, because it is the
  only garment that wraps a BOX. See §4, trap 10.

**MEASURED against the machine, seated, after the butt and the gut landed:**
154 rider verts inside a machine part, against 142 before — the extra 12 are
the longer hem sinking into the seat cushion, exactly as the thighs already
did. Nothing new touches the console, the cowl, the shroud or the tunnel;
`rear_storage` went from 151 mm of clearance to 131 mm.

---

---

## 3. THE HOOD — FOUR ATTEMPTS, THEN CUT.  READ THIS BEFORE BUILDING A FIFTH.

**THE HOOD IS GONE — ruling E, and it was the right call.** Four were built in
one session and all four were rejected on silhouette (D3 "upside down", D4
"open at the bottom ... like a sleve", D6 "still you closed the wrong end",
and in between "are we entering whale a mole territory?" — we were). Every
rejection was a silhouette reading and every one of them was correct. This
section is kept so that nobody re-derives a hood from ruling A without knowing
what it costs.

### What was wrong, and it was one thing the whole time

All three rejected hoods were **swept at a constant radius about the NECK
AXIS**, sized to clear the scarf. The scarf reaches r 0.277, so the hood's
inner face stood at ~0.29 — 60 to 110 mm off his back — and the annular gap
between the hood and his back was the "open bottom". Closing it with a floor
was impossible: the tails pass through exactly that annulus, so the floor's
notch swallowed 98 deg of a 172 deg bag (0.032 m² open vs 0.0084 m² floored).
That option should never have been offered; the numbers to rule it out were
already measured when it was.

### The measurement that fixed it

Chad sent a photo of parka hoods from behind and asked *"the tails could just
come out between like fabric layers , we diont need a big space for a tail of a
scatf dfo we?"* — so the tails were measured **at the hem plane**, not as a
radius:

```
  z 1.42 +-12 mm, live:  scarf occupies x -0.190..+0.070 at y -0.2204..-0.1902
                         the coat's own back there is       y -0.1720
  -> the whole scarf at that height is a layer 30 mm THICK,
     sitting 18..48 mm behind the coat.
```

A 30 mm layer. Three hoods had reserved a 60 mm cavity for it all the way
round. **The construction was wrong, not the dials.**

### The construction that works

* the hood's **INNER face follows the COAT** — `coat_r(th, z)`, the hoodie
  torso's own superellipse, plus an 8 mm liner gap — and is pushed outward
  **only** at the (azimuth, height) cells where the measured scarf is in the
  way;
* the **OUTER face** is lofted off the COAT too (`HOOD_BULGE` 52 mm), not off
  the scarf: cloth over a lump shows the lump, it does not inflate to swallow
  it;
* the two faces meet in a rounded **fold** at the hem and again at the
  neckline. **That is what closes the bottom** — no floor, no notch, no rim.
  The gate clause *"the pocket is CLOSED at the bottom"* measures exactly this:
  the widest hem section is 14.0 mm against 14.0 mm of cloth.
* where the scarf passes, the hem lifts off the coat by the scarf's own
  thickness (worst 102 mm at th 248, over 101 deg of the bag) and **the tails
  come out between the layers**, which is what he asked for.

### WHY IT WAS CUT RATHER THAN FIXED — the arithmetic that ends the argument

Even the last construction left the hem lifted off the coat over **101 deg of
the back, worst 102 mm**, because the scarf genuinely stands 50-100 mm off the
coat there and the cloth has to clear it. The floor-with-a-notch version before
it was worse and should never have been offered: the notch swallowed **98 deg
of a 172 deg bag — 0.032 m² still open against 0.0084 m² floored** — and the
numbers to see that were already measured when it was proposed. A hood, this
scarf, and a closed bottom are three things and this character can have two.

### Still true, and still the reason the hood WAS asymmetric

```
  long tails   th 250..280   r 0.216..0.238   z 1.04..1.50
  short tail   th 220..240   r 0.270..0.277   z 1.38..1.53   (scarf_s01/s02)
  the knot     th 220..290   r up to  0.265   z 1.50..1.58
  the band     all azimuths  r 0.106..0.1486  z 1.5167..1.6038
```
The short tail hangs off his LEFT and reaches 100 mm off the coat, so the hood
is fuller there. That is the scarf's shape showing through cloth. The scarf is
signed and was not touched.

## 4. TRAPS — the C1 handoff's nine, plus three more paid for this session

10. **A SUPERELLIPSE DOES NOT CONTAIN THE BOX IT ROUNDS — again.** The boot was
    built as a superellipse at (0.079, 0.172) around a foot box of
    (0.0675, 0.160): *both half-extents larger*, and the coverage scan still
    reported all 16 foot vertices BARE, because the box's corners sit outside
    the curve. This is T0's trap 1 wearing a different hat, and it bit on the
    one piece in the file that still wraps a box. `rrect_ring()` and
    `rrect_contains_box()` exist for exactly that piece.
11. **A CONTAINMENT GATE THAT ONLY CHECKS THE TABLE'S OWN ROWS DOES NOT CHECK
    THE SHAPE.** The first boot clause tested only rows with z ≤ the foot box's
    top and so never tested z = 0.100 itself — which is precisely where it
    failed (4 of 8 foot verts bare, the box's top corners). Both containment
    clauses now evaluate at the **UNION** of both tables' breakpoints plus the
    span's own ends. Same shape as T1's trap 8.
12. **ONE ENVELOPE, TWO QUESTIONS, TWO DILATIONS.** `scarf_env()` guards a
    CLEARANCE and is deliberately pessimistic (±2 azimuth bins). Using it to
    answer *"is there any scarf at this azimuth at all"* — the floor question —
    declared scarf 20° away from any scarf vertex and would have eaten the
    whole floor. `scarf_env_tight()` (±1 bin) answers that one. A gate is only
    as honest as the question it asks.

Also worth keeping: the hood's silhouette was rejected twice in one session
(D3 "upside down", D4 "open at the bottom / like a sleeve"). Both were
SILHOUETTE readings, not measurements, and neither was reachable from the
numbers. The cost each time was one apply-and-look cycle, which is cheap — the
expensive thing would have been reasoning about it instead of showing him.

---

## 5. WHERE IT STANDS, AND WHAT IS NEXT

### Measured, on the APPLIED mesh (`verify_costume_live.py`, the authority)
```
  1. ring wall to the chin        PASS  worst margin +4.9 mm at th 75
  2/3. hood vs scarf              n/a   no hood (ruling E); the verifier now
                                        says so and carries on to test 4
  4. tails rest atop, not through PASS  0 of 908 inside a garment shell
                                        0 of 908 inside a body shell
  5. still bare                         head 8 (helmet), neck_01 81 (inside the
                                        scarf ring). Everything else 0.
generator-side, the clauses that exist because of this session:
  the sweater covers the WHOLE torso    0 rows short over z 0.9200..1.5150
  the sweater covers the snowpants      0 rows where black shows through grey
  L2/L6/L7 growths named and bounded    gut 0.1780 (+28 mm proud of the chest),
                                        butt 0.1900 (+37 mm), back at the
                                        tails 0.1593 against a 0.1630 cap
  the torso does not eat the tails      deepest garment back over the tails'
                                        own z band 0.1745 at z 1.0579 (cap
                                        0.1750, tails at 0.1894); below 1.0579,
                                        where there is no scarf at all, 0.1990
```
The 71 neck vertices are the neck between the collar's top (1.5660, the highest
legal value under the band's LOWEST top 1.5715) and the chin (1.6087). They sit
INSIDE the scarf ring's own wall and are hidden by it. T1 had the same finding
at 12 of 24; the count grew because the neck is finer and thinner now, not
because more of it shows.

### ★★★ RULING G — the five crease defects, and why they were all arithmetic

CHAD, 2026-08-22, on the continuous-shell build:

> "I dont like the sharp undercut of the sweater at the base, I dont like how
> the sweater seam at the shoulder has a big cleavage furrow, also the side
> profile shows an out of place shape ring above the legs at the mid lower back
> that should not exist. I dont like th shelflike plateau of the top torso
> (weird lip ring above the nipple lin eof the chest) adjacent to the shoulder
> attacjments, THe sweater should bridge cover any joints and eliminate flat
> parts of anatomy in the inersticies of the joints."

**FOUR OF THE FIVE WERE SLOPE DISCONTINUITIES IN MY OWN TABLES**, and they were
found by measuring the profile rather than by looking harder:

```
  hx CONSTANT z 0.910->0.955 then falling at -0.314   -> the ring at the
                                                         mid lower back
  the shoulder lift ramping over z 1.475..1.515:
      47 mm of rise in 30 mm of z                     -> the shelf / lip ring
  the hem roll: 2 mm of z against 39 mm of radius     -> the sharp undercut
```

A lofted surface is only as smooth as the SLOPE of its table. Anywhere two
authored rows disagree about the slope the shell gets a crease, and a crease
that runs all the way round reads as a RING — which is the word he used twice.
Hand-authoring rows that happen to be C1 is not something anyone does reliably,
so:

* **`body_geom.smooth_rows()`** densifies the table to 64 rows and smooths it.
  Worst change-of-slope went from **0.50 at z 0.955** to 0.38 up in the
  shoulder dome, where it is curvature and not a crease.
* **The smoothed profile is THE table** — `hoodie_profile()` / `pants_profile()`
  are read by the loft AND by every containment clause. Smoothing at loft time
  only would leave the clauses measuring a shape that was never built.
* **It is a FUNCTION, not a module-level constant.** A derived constant goes
  stale the instant anything edits its source: two mutations silently became
  no-ops and the harness printed "9 of 11 caught" when it meant "9 of 11 did
  anything at all". Second time that shape has bitten in this rung.
* The hem roll now travels **21 mm in z against 54 mm in radius**, and leaves
  the side wall tangentially.
* The shoulder lift ramps from **z 1.430**, and the yoke's width turns over as
  the height comes on, so the surface rolls instead of stepping.

**THE FIFTH — the shoulder furrow — is not arithmetic, and it needed a new
pass.** Two surfaces that cross always crease; only tangency removes it, and
two independent primitives are never tangent along a whole curve. Widening the
yoke out to meet the sleeve's ridge (hx 0.186 -> 0.2380) shrank the angle and
the furrow was still there. So the crease is **FILLED**:

> `_push()` measures, for every vertex of shell A, how far it lies OUTSIDE
> shell B, and where that distance is small — i.e. near the intersection, which
> is exactly where the V is — pushes the vertex outward along A's own outward
> direction. Both shells bulge toward each other near the seam and the V
> becomes a fillet.

It is the cheap analytic cousin of a smooth union: no marching cubes, no new
topology, **no triangles added**, and the displacement is OUTWARD ONLY so it
cannot break containment — the body can only end up further inside. It runs on
torso↔sleeve and hip↔leg, both ways, which is what "bridge cover any joints"
asks for. It is guarded against the scarf band by construction.

⚠ A fillet is a MUTUAL bulge: filleting the pants hip toward the leg without
doing the same to the sweater over it pushed 2 probe points of black out
through the grey. Doing it on one side of a pair is just a bulge.

### ★★★ RULING F — "this does not look like a sweater ... instead of bodypaint"

CHAD, 2026-08-22, on the first butt-and-gut build. Five complaints, and they
turned out to be **one root cause**, which is why they are recorded together:

> "the sweater at the top and bottow have a 90 degrees cut that looks really
> unnatural ... THe shoulders look like this attach at a very fine point after
> a sheer cut plataeau atop the arms. this does not look like a sweater. Why
> are there spaces at the knees. [Why do] the shoulders look totally
> disconneced, why dosent the sweater look like a sweater instead of
> bodypaint?"

plus: *"I dont like the shape of the rump and belly ... It looks like you
reverted back to the block shape and you make the butt curve too high."*

**THE ROOT CAUSE: the garment was a SET OF SEPARATE CAPPED PRIMITIVES that
happened to overlap.** Every symptom follows from it:

| what he saw | what it was |
|---|---|
| "90 degrees cut" at the hem and the top | a loft closed with a flat disc cap is a SLICED PIPE |
| "sheer cut plataeau atop the arms", shoulders "disconneced" | a tube poking through a flat top is not a shoulder |
| "spaces at the knees" | two capped tubes butted on the joint plane — a seam, and a BROKEN BLUE STRIPE, which on a black leg reads as a hole |
| "bodypaint" | every surface at a constant offset from a primitive reads AS that primitive |

**So the construction changed, not the dials:**

* **Sleeves and legs are now ONE CONTINUOUS SHELL across two bones**
  (`body_geom.chain_path` + `chain_limb`), with the corner at the joint rounded
  by a Bezier and the ring frames PARALLEL-TRANSPORTED (rotating a fixed "up"
  into each tangent independently makes the ring spin, and a spinning ring on a
  bent tube is a visible twist). One shell still skins rigidly across the
  joint: `_by_u()` gives each vertex the bone whose half of the chain it lies
  on — the mesh does not have to be cut where the skeleton is. The torso has
  always done this by z; the limbs do it by chain position now.
* **The stripes are one continuous strip each**, across the joint, same trick.
* **Every FREE edge is ROLLED, not capped** — hem, cuffs, trouser cuff. Buried
  ends are NOT rolled: a sleeve's shoulder end is inside the torso, needs no
  hem, and rolling it cost real clearance (the 0.62-scale ring reached r 0.1424
  at z 1.5195, inside the scarf band's wall).
* **The sweater's top RISES over the shoulders**, gated on the vertex's own
  RADIUS rather than its azimuth — so the lift fades to nothing before it
  reaches the band's annulus and the constraint cannot be violated by a later
  edit to the tables.
* **The neckline DIVES under the band floor** as it comes in, and closes inside
  the ring's bore. It is a SEPARATE list (`HOODIE_NECK`) and not more rows,
  because its z descends and `table()` — which every containment clause uses —
  assumes a sorted key. Folding it in would not raise anything; it would just
  make every clause quietly wrong.

**AND THE RUMP AND BELLY ARE AN EGG, NOT A BLOCK.** The first attempt chased
the gut with WIDTH (hx 0.2150 at the belly against 0.2120 at the chest and
0.1950 at the hip) which made the torso one width from hip to shoulder — a
block — and put the seat's crown at z 0.980, up on the small of his back.
Now the belly is the WIDEST station (0.2050) and the chest is NARROWER
(0.1960), the section power dropped 2.30 → 2.05 (very nearly a true ellipse;
2.30 flattens the sides, which is what a superellipse is FOR), and the seat's
crown is at z 0.955. A new clause requires **belly girth > chest girth** —
1.149 m against 1.094 m — because "+28 mm proud of the chest" was true of the
first attempt while it was 114 mm SMALLER in girth.

**Gate: 15 clauses green, and the mutation harness is 11 of 11.** ⚠ Four of the
original eight mutations had silently become no-ops when the limbs became
chains — the harness reported "4 of 8 caught" when the truth was "4 of 8
mutations did nothing". A mutation harness whose mutations are no-ops is worse
than none: it prints a number that looks like a measurement.

### ★ THE RED-TEAM ROUND (context-free, on Chad's standing invitation)

Two fresh-context consults were run while he slept, both told to re-derive from
the artefact and not to trust a comment. Both earned their keep.

**THE GEOMETRY RED TEAM FOUND A SYSTEMIC HOLE AND THREE LIVE DEFECTS.**

> *Every clause in both files tested a TABLE VALUE. Nothing tested a SHELL, and
> nothing tested a shell against another shell.*

`contains_rows` / `contains_radii` compare half-extents through `table()`,
which CLAMPS outside its range — so neither ever asked whether the outer shell
**exists** at that z or t. Six mutations that strip the garment off the man all
passed **GREEN**: a sweater truncated at z 1.42 (65 bare), a sleeve stopping
half way down the forearm (130 bare), no shoulder (64 bare), a bare shin (176
bare), a boot with no shaft, a thigh at ×1.6. Five span constants —
`SLEEVE_UP_T`, `SLEEVE_LO_T`, `PANTS_THIGH_T`, `PANTS_CUFF_Z`, the `BOOT_ROWS`
z-span — were tested by **nothing**. *"the sweater covers the WHOLE torso"* was
a name that outran its reach: it checked WIDTH, never SPAN.

And three real defects the eye would have caught before any gate did:

| | measured |
|---|---|
| the black snowpant thigh burst through the grey sweater at the hem | **+35.8 mm**, both sides, 26 probe points outside every sweater shell |
| all six blue stripes floated clear of the garment | **0 of 38 verts** of every stripe inside its host, hovering up to **6.8 mm** off, with a see-through slot under the end caps |
| the snowpant cuff hung behind the boot shaft | **37 mm** of black in mid-air, 23 of 65 probe points |

Plus: the calf stripe's bottom 26% was buried inside the boot; the band-wall
clause was a VERTEX test that a collar driven straight across the wall passed;
`L5` guarded `-y` over a band stopping at z 1.46 while its own data runs to
1.58; `UPPERARM_R` had no ceiling at all (tripling the deltoid to a 320 mm arm
stayed green); and the elbow, hip and knee still had the coplanar end caps the
shoulder had already been fixed for.

**AND THE "GUT" WAS NOT A GUT.** The banner said *+28 mm proud of the chest* —
true, and the wrong measurement. `hx` at the belly was 0.1730, the
second-narrowest point in the whole torso, so the MEASURED girth was 1.064 m at
the belly against 1.178 m at the chest: from the front — the camera the game
actually uses — he still had a clean athletic V-taper. The gate's metric was
not the thing the eye reads. **Belly girth is now 1.184 m against a chest of
1.137 m**, so the widest part of the man is his middle.

**WHAT WAS DONE ABOUT IT.** All of it is fixed, and the hole is closed by a new
file rather than by more table clauses:

* **`coverage.py` (NEW)** — ray-parity containment with no Blender: per shell
  (never a combined tree), with the projection rotated by two fixed irrational
  angles because the body and the garment share a phase ON PURPOSE and would
  otherwise land every query on a triangle edge. Three new MESH clauses in
  `rulings()`: no bare body anywhere, no black through the grey, and the
  stripes are sunk rather than floating. Runs in **0.6 s**.
* `body_geom.kept_probe_points()` — the head and foot boxes are KEPT LIVE and
  so were invisible to the offline gate; deleting the entire boot shaft passed
  green because there was no foot in the probe set to leave bare.
* The boot now has a **5 mm sole** below the foot box: a bottom ring exactly
  coplanar with the thing it covers left 62 sole probes reading BARE, because
  ray parity has no answer on the plane itself.

**MUTATION-VERIFIED, 8 of 8.** `scratchpad/mutverify.py` re-runs every mutation
above plus the stripe sink and the collar-across-the-wall case; baseline green,
all eight red. A measurement nothing can make fail is not a gate.

**One judgment call left for Chad, with numbers:** the snowpant legs
interpenetrate from the crotch to **z 0.265**, where the body's own legs part
at **z 0.605**. With 12 mm of cloth on each leg and hip axes 190 mm apart they
cannot part much higher — the body's own gap does not exceed two cloth
thicknesses until z ≈ 0.25. In the seated pose his knees are spread, so this
may never show; in rest it reads as a skirt. Flagged, not silently accepted.

### OPEN — needs Chad
* **His eye on the figure.** Nothing here is signed.
* **§8 of the C1 handoff is still open and is now more visible.** In the SEATED
  pose the authored rest tails hang straight down while the torso leans 33°
  forward, so in the `.blend` the long tails are BURIED in his back and the
  "flying out from there" cannot be seen. In BIND it is correct (measured
  above: 0 of 908 inside anything), and the GAME drives those bones every frame
  from `render/trail_chain`. The screenshots that show the hood working are
  therefore REST-pose shots. Now that the `.blend` is the source of truth, that
  discrepancy wants a ruling.
* **The hood is asymmetric** — fuller at the bottom on his LEFT — because the
  short tail reaches r 0.277 there and the cloth has to clear it. That is the
  scarf's shape showing through. If he wants it to taper cleanly all the way
  round, the knot has to come in, and the scarf is signed.

### ★★★ THE BIGGEST FINDING OF THE SESSION — A RIG DEFECT, NOT A COSTUME ONE

CHAD, 2026-08-21: *"in blender it (the scarf tails) are going into the sweater
back not resting on top of the barrier."*  He is right, it is bad, and it is
**not the scarf's geometry and not the costume**:

```
  scarf_01..06 and scarf_s01/s02 are parented to  neck_01
  564 of the scarf's 908 verts (the RING and the KNOT) are weighted to neck_01
  neck_01 carries 36 deg of pose pitch -- the riding chin-tuck
  every scarf bone carries 0 deg of its own
```

So the whole scarf — ring, knot and both tails — swings with his SKULL instead
of sitting on his SHOULDERS.  Ray parity on the EVALUATED mesh:

```
  BIND                                            0 of 908 inside a garment
  POSED, as built (scarf rides neck_01)         544 of 908 inside a garment
  POSED, if the scarf rode spine_03             ★ 1 of 908
  POSED, if the scarf rode spine_02              74 of 908
```

**544 → 1.** The fix is to move the scarf off the head and onto the shoulders:
re-weight the 564 `neck_01` scarf verts to `spine_03`, and re-parent
`scarf_01` / `scarf_s01` to `spine_03`.  A scarf sits on the shoulders; the
neck moves through it.

**NOT DONE, AND IT IS NOT A ONE-LINER.** A fresh-context consult read the
engine and the verdict is **UNSAFE as an asset-only change; SAFE WITH
CONDITIONS if one runtime hunk lands in the same commit.** I re-checked its
four load-bearing claims against the source myself; all four are exact.

**WHY THE OBVIOUS FIX SILENTLY FAILS.** The runtime does not pose the scarf
bones through the hierarchy at all — it writes absolute matrices and suppresses
composition (`render/sled_model.cpp:3208-3211` sets `world` and
`world_override`; `:1712` skips overridden nodes). So the parent is irrelevant
to the driven bones themselves. But the chain's ROOT is recomputed every frame
from the neck:

```
  render/sled_model.cpp:3005-3006
      const glm::mat4 neck_w   = sm.nodes[sm.n_neck].world;
      const glm::mat4 anchor_w = neck_w * sm.scarf_anchor_local;
  render/sled_model.cpp:1021-1022
      sm.scarf_anchor_local = compose(s0.t, s0.r, s0.s);   // scarf_01's LOCAL
```

Re-parent to `spine_03` and the exported local becomes
`spine03_bind⁻¹ · scarf01_bind`, while the code still multiplies it by the
NECK's posed world — so the 36 deg chin-tuck gets applied **a second time**.
The 564 re-weighted ring/knot verts would correctly ride the shoulders while
the tail's solver root still rides the head: **the knot and the tail come
apart, and the tail keeps the exact swing the fix exists to remove.** Green
tests, wrong picture.

**AND THERE IS NO GATE.** `test_trail_chain.cpp:381` is named "trail chain
frames reproduce the GLB rest bone frames" and reads no GLB — it hard-codes the
anchor and calls the pure solver, so re-parenting cannot fail it. (A test whose
name outruns its reach: the same shape already on record from the scarf rung.)
`test_rider_rig.cpp:233` checks parentage only for the 21 catalogue joints and
`render/rider_rig.cpp` has no scarf entry, so scarf parentage is unasserted
anywhere. Nothing asserts the posed anchor.

**IT ALSO CONTRADICTS A SHIPPED SPEC.** `docs/SCARF_SPEC.md:244` and `:284`
specify the neck anchor. Standing law 1 says a conflict with the spec becomes
ONE question to Chad, never a silent re-derivation. **This is that question.**

**THE CHANGE SET, if he rules for it:**
* `render/sled_model.cpp` — bind the ACTUAL parent node of `scarf_01` and of
  `scarf_s01` at load, next to `:1021`, derived from the file and never a name
  literal; use that parent's posed world in place of `neck_w` at `:3006` and
  `:3089`. Keep `neck_p` for `back_origin` / `back_normal` (`:3014-3022`) and
  the pelvis->neck torso axis (`:3050-3051`) — those are torso geometry and
  must stay on the neck.
* the asset, in the LIVE GUI session — re-weight the 564 verts, re-parent the
  two chain roots, re-export. The consult flagged `patch_scarf.py:231-241`
  (which builds the short tail's locals against `bind_neck`) as needing the
  same move — **it does not: that path is RETIRED.** `docs/SCARF_SPEC.md:684`
  records that the scarf is now IN the exported source since the .blend
  re-export rung, and the script already refuses to double-patch. The change
  belongs in the `.blend` and nowhere else.
* tests — an asset test asserting the parentage (none exists), and **the
  missing gate: a POSED ray-parity leg.** Bind was already 0 of 908; a
  bind-pose test proves nothing here.
* `docs/SCARF_SPEC.md` §4.1/§4.2 and `render/trail_chain.h:22` amended, and
  `generated/graph/` regenerated in the same commit.

**One caution for Chad:** `spine_03` also carries the clavicles and the
torso-lean channel, so the ring and knot would follow the ~33 deg forward lean
instead of the head. That is presumably the intent, but it is a visible change
to a signed look, and the 544 -> 1 was measured on the STATIC seated pose, not
across the lean and weight-shift range.

Two things this cost, worth recording:
* **Every test on this ladder reads the BIND mesh, so none of them could see
  it.** `verify_costume_live.py` reported "0 of 908 inside a garment shell" and
  was telling the truth about a question nobody had asked — the fourth instance
  of that exact shape in this rung alone.  A posed-mesh containment leg belongs
  in the verifier.
* `pose_scarf_live.py` (NEW, uncommitted) was written to drape the chain with
  pose rotations and got 544 → 250, but only by zig-zagging the chain to dodge
  the coat. It is kept because its DIAGNOSIS is the finding above and its
  `count_buried()` is the measurement; its `drape()` is not a fix and should
  not be run for effect. Pose-only, never saves, `undo()` clears it.

### NEXT RUNGS (unchanged from the C1 handoff)
**C3** skinning (weighted, multi-bone at elbow/knee/waist/shoulder — everything
is still rigid one-bone-per-vertex, which is why a stripe cannot cross an
elbow). **C0** the engine's texture path (the shipping GLB has `images: 0`, no
`TEXCOORD_0`, and `render/sled_model.cpp` never samples one) — blocking for C4
only. **C4** UVs + 2048² textures. **C5** LOD + draw cost.

**BUDGET:** body 3680 + costume 11824 = 15504 tris of new geometry, against T1's
324 + 11376 = 11700. Net **+3804 tris** over T1 — a shaped body with a butt and a gut,
a longer sweater, and it is LIGHTER than T1, because cutting the hood paid for
all of it for a shaped body and a better
garment. The proxy is 8638 verts / 17054 polys total.

---

## 6. STATE ON DISK AND IN THE SESSION

**Nothing is committed. The tree is dirty.** Branch `main` in
`D:\flight_sim2\seads-recon`.

| artefact | state |
|---|---|
| `assets/character/sudburian_src/body_geom.py` | **NEW.** The shaped body + the primitives both layers share. `python body_geom.py` is the census + rulings, exit 1 on any defect |
| `assets/character/sudburian_src/apply_body_live.py` | **NEW.** Patches the live body in place, with the untouched-proof and the L1 bone assertions. Never saves |
| `assets/character/sudburian_src/costume_geom.py` | **REWRITTEN = T2.** `python costume_geom.py` is the census + 13 rulings, exit 1 on any defect |
| `assets/character/sudburian_src/costume_geom_T1_rejected.py` | **NEW.** T1 preserved verbatim, referenced by nothing |
| `assets/character/sudburian_src/costume_geom_T0_rejected.py` | unchanged, T0 preserved |
| `assets/character/sudburian_src/coverage.py` | **NEW.** Ray-parity containment, pure stdlib. The gate's only mesh-level measurement |
| `apply_costume_live.py`, `measure_neck_ceiling.py` | unchanged and still correct |
| `verify_costume_live.py` | EDITED: tests 2/3 (hood vs scarf) now report "no hood" and fall through instead of asserting, so test 4 — the one that has caught every real defect — still runs |
| `pose_scarf_live.py` | **NEW.** Its DIAGNOSIS is §5's finding and its `count_buried()` is the measurement. Its `drape()` is NOT a fix and should not be run for effect: it got 544 to 250 only by zig-zagging the chain to dodge the coat. Pose-only, never saves, `undo()` clears it |
| `scarf_geom.py`, `export_live_v13.py` | the T0 edits, unchanged. **KEEP both** |
| `assets/sled/indy650.glb` | **STILL OVERWRITTEN with the T0 splice** from two sessions ago. Nothing this session exported. `git checkout assets/sled/indy650.glb` reverts it, and it should be reverted before anything is committed |
| **the live Blender session** (`indy650.blend`) | **HOLDS C1 + T2, NOT SAVED.** Proxy 8638 verts / 6 material slots. Rig 44 bones, correct rider. The armature was put in REST for the screenshots and **restored to POSE**; the viewport was moved and **restored** to Chad's own transform |
| the gate | **NOT RUN.** `test_rider_winding` has never seen these shells — and it cannot until something is exported, which nothing has been |

To rebuild from scratch in a fresh session, in this order:
`apply_body_live.py` then `apply_costume_live.py` then `verify_costume_live.py`.
Both apply scripts are idempotent and both prove what they did not touch.
To remove everything: reload `indy650.blend` (it was never saved).

The viewport was moved for screenshots and RESTORED; the armature was put in
REST and restored to POSE; 137 machine objects were eye-hidden for a clean
silhouette shot and all 137 were un-hidden (the 5 that remain hidden --
`helmet_dent_1..4` and `rider_rig` -- were already hidden and were not touched).

---

## 11. SURFACE RUNG S1 — the rings and the joint furrows, 2026-08-22

**CHAD:** *"I just want to have a more natural form, eliminate the cleave
furrows at the joints and get rid of rings and make the cloth of the sweater
look flowy not jaggedy or with concentric rings ruining the look."*

BUILT and APPLIED in the live session, **not saved, not committed**. Snapshots
either side in `assets/character/sudburian_src/blend_snapshots/`:
`live_20260822_pre_smin.blend` and `live_20260822_post_smin.blend`.

### What was wrong

1. **Rings were fought with a box filter.** `body.smooth_rows()` is
   `[0.25, 0.5, 0.25]` a few times: it buys **C1**, continuous SLOPE. The eye
   reads **curvature**. A box filter does not remove a curvature jump, it
   squeezes it narrower and TALLER — which on a body of revolution is a ring.
   Each round of "fixing" added another authored row, so each round added
   rings. MEASURED: max `|d2r/dz2|` **43.9**, against the torso's own
   circumferential curvature of `1/0.23 = 4.3`.
2. **The joint furrow was filled, not removed.** The old `_push()` bulged
   vertices near the seam, but skipped every vertex INSIDE the neighbouring
   shell — exactly the ones in the trough — so the trough stayed and grew a lip
   on each side. MEASURED: 512 hoodie edges over 25 deg, worst 130.6.
3. **The limbs were never faired at all.** `SLEEVE_R`, `PANTS_R` are
   piecewise-LINEAR: max `|d2r/du2|` 12.8 and 13.1. The sweater got fixed and
   the sleeves kept their bands.

### What replaced it

* `body.fair_rows()` / `body.fair_table()` — a smoothing spline
  (`min ||c-y||^2 + W||D2 c||^2`, pentadiagonal LDL, one dial `fair_m` in
  metres). Curvature: torso 43.9 -> **1.85**, sleeve 12.8 -> **0.16**,
  leg 13.1 -> **0.21**.
* `_blend()` — the analytic cubic-smin displacement
  `delta(f) = (k/6) h^3, h = max(0, 1-|f|/k)`, along each shell's own field
  gradient. Continuous through `f = 0`, so no step at the seam; zero at
  `|f| = k`, so untouched away from it; bounded by `k/6 = 10 mm`.
* Garment ring counts **doubled** (48 / 32) — an INTEGER multiple of the
  body's, so note 1's containment identity survives vertex for vertex.
* `_drape()` — the first folds this garment has ever had on purpose. Fast in
  theta, slow in z: the opposite of a ring.

### Two traps this rung walked into, both recorded in the source

* **Projecting along the radial direction.** On the shoulder dome the radial
  direction lies IN the surface, so "outward" drags vertices sideways into
  their neighbours. 825 creased edges and 178.9-degree folds.
* **Bailing out of a displacement field.** Attempt 2 left un-projectable
  vertices where they were. Two neighbours, one moved 10 mm and one not, IS a
  10 mm step — the SAME defect as the `_push()` this rung set out to remove,
  rediscovered from the other direction.

### Gates

14 clauses green, including four new ones — `no RINGS in the sweater /
snowpants profile / sleeve / trouser leg radius` — all four
**mutation-verified**: restoring the box filter and the raw limb tables turns
them red at 43.86, 4.08 and 3.06. `verify_costume_live` (the authority) is
green: 0 of 908 scarf verts inside any garment or body shell, nothing bare but
the head and the neck.

### Open

* `SHOULDER_LIFT` still costs ~40 creased edges and the neckline's dive under
  the scarf band is a hard 90-degree turn. Both are UNDER the scarf ring.
  Measured, not fixed, because Chad did not name them.
* The scarf's own tails are still visibly faceted. Different rung.
* Nothing is saved. §5's scarf-parenting question is still open and untouched.
