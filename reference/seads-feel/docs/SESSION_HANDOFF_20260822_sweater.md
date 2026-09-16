# HANDOFF — THE SWEATER RUNG: §15 IS THE LAUNCH (SW-1d built, awaiting Chad on the ARMS)

**LAUNCH LINE (rewritten 2026-08-22, end of the SW-1d session):**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then
**§15 of this file — it is the handoff and supersedes §13 and §14**. Read §15.3
before anything else: two PRE-EXISTING bugs were found there that no clause could
see, and Chad found both with his eye. Then §14, then §13, then §7 of
`assets/character/sudburian_src/ref_sweater/SW1_MEASUREMENTS.md`. §0–§4 are still
the diagnosis and still true. **The immediate item is CHAD'S EYE ON THE ARMS, as
they stand, BEFORE any weld** — he ruled the weld back himself. Do NOT touch
`costume_geom.py`, it is red on purpose. Connect to the RUNNING Blender session
before you model anything, and never send an unbounded loop into it (§15.7)."*

> **STATE:** SW-1 (the shoulder girdle) is BUILT and applied live, `body_geom.py`
> gates 7/7 green, and Chad's six corrections on it are all built. **Awaiting his
> verdict on the body, alone, no garment.** SW-2 (the scarf) is NOT started —
> he ruled it gets its own deliberate session. Nothing saved, nothing committed.

**Supersedes §11 of `docs/SESSION_HANDOFF_20260821_costume_C2.md`** (the surface
rung S1, 2026-08-22). That work is applied in the live session and is NOT the
thing to defend — see §7 for what survives it and what does not. The rest of the
C2 file (§3 measurements, §5 the open scarf-parent question, §9 traps) still
stands.

---

## 0. STATE IN ONE LINE

The garment has been re-faired, smooth-unioned and draped, every gate is green,
and **Chad looked at it and rejected the approach** — because all four things he
can see are downstream of one fact nobody has fixed: **the body has no shoulder
girdle.** Nothing is saved. Nothing is committed.

---

## 1. CHAD'S WORDS (verbatim, 2026-08-22, in order)

> **F1** — *"this seems like a terrible way to go about making a sweater, the
> sholders are too bulbous, the upper torso still has a badly indended plateau,
> the rings extrudiung out the lower back and the seams at the shoulders
> indented all the way to the torso looks unnatural, look at an image of a
> seater wearing person online? How can we get it to look like that the best way
> without runnning into severe issues or overcomplicating?"*

> **F2** — *"well there are no shoulder blades or collar bones near the neck so
> it just goes in at 90 degrees looking very unnatural, we need a holistic
> approach"*

**F2 is the diagnosis and it is correct.** It is also the whole of §2. He named
the cause in one sentence after three agents spent two days on symptoms.

---

## 2. THE FOUR DEFECTS, MEASURED, AND THE ONE CAUSE

Every number below is measured off the current live/generated mesh, not
asserted. The script fragments that produce them are in §9.

| # | What Chad sees | What it measures as |
|---|---|---|
| 1 | *"the sholders are too bulbous"* | The sleeve at the shoulder is a **125 mm-radius tube** whose axis starts at the SAME HEIGHT as the neck base. Its cap tops out at **z 1.6086**, standing **109 mm proud** of the torso's top. |
| 2 | *"a badly indended plateau"* on the upper torso | The body torso ends in a **flat cap at z 1.500** and the neck leaves at r 0.077. That is a horizontal annulus **113 mm wide at CONSTANT z**. |
| 3 | *"the seams at the shoulders indented all the way to the torso"* | The smooth-union blend radius is **60 mm**. The step it is being asked to bridge is **109 mm**. It cannot. The trench between the ball and the plate is what is left over. |
| 4 | *"the rings extrudiung out the lower back"* | Over z **1.076..1.134** the sweater's back depth oscillates **±4 mm with a one-to-two-row period** — because the blend measures distance to a **31-segment POLYLINE** leg, and adjacent sweater rows snap to different segments. |

### And the one cause, in one number

```
neck base z 1.51286   acromion (rig SHOULDER) z 1.51286
                      -> trapezius drop 0 mm, slope 0.0 deg
                                                   a real one is 18-25 deg
```

**There is no shoulder girdle.** No clavicle running forward and out from the
sternum. No trapezius sloping down and out from the neck to the acromion. No
scapular plane standing proud of the back. The neck is a cylinder rising out of
a flat plate, so the silhouette **turns 90 degrees at the neck** — exactly as
Chad says — and the arm has nowhere to come from except a ball dropped on top.

Defects 1, 2 and 3 are not three problems. They are one absence, seen from three
angles.

---

## 3. ★★★ THE COLLISION THAT MADE IT FLAT — READ THIS BEFORE PROPOSING A FIX

The obvious move is "give the torso a trapezius". **It does not fit**, and this
is why every previous attempt produced a plate:

```
scarf band keep-out:   r 0.1060 .. 0.1486   above z 1.5167  (top 1.5715)

a trapezius sloping  neck base (r 0.077, z 1.580)
                  -> acromion  (r 0.210, z 1.500)
   crosses r 0.1060 at z 1.5626   and   r 0.1486 at z 1.5369
   *** COLLIDES with the band over z 1.5369..1.5626 = 26 mm ***
```

The scarf's ring is modelled as a **horizontal annulus around a cylindrical
neck**. That shape is only valid on a body with **flat shoulders**. It is not a
constraint that the shoulder happens to violate; it is a constraint that was
derived from the flat shoulder and has been forcing it ever since.

So: **the scarf is wrong for the same reason the sweater is wrong**, and this
rung cannot be done without re-seating it. A real scarf rests ON the trapezius
and follows it down; it does not float in a horizontal ring. This is the
holistic part of "we need a holistic approach" and it is the single biggest
piece of work in the rung. It also interacts with the **still-open scarf-parent
question** in `SESSION_HANDOFF_20260821_costume_C2.md` §5 — the scarf is
parented to `neck_01`, the chin-tuck bone, and rides his skull. **Both scarf
problems should be settled in one pass, not two.**

---

## 4. THE ANSWER TO CHAD'S QUESTION

> *"How can we get it to look like that the best way without running into severe
> issues or overcomplicating?"*

### The answer: one body, one surface, one gate.

**A sweater is not a shape that resembles a body. It is the body's own surface,
pushed out and relaxed.** That single sentence is the whole method, and it is
what makes the shoulders correct for free — they are the *body's* shoulders.

Today's architecture does the opposite. `body_geom.py` builds a body out of
lofted rings and tubes; `costume_geom.py` builds **a second, parallel set of the
same primitives**, sized slightly larger, and then runs displacement passes to
hide the places where they cross. That is why there are seams at all. **There is
no seam in a sweater on a body.** Every pass that has ever been added here —
`_push`, `_blend`, `_drape`, `shoulder_lift`, `HEM_ROLL`, the fairing splines —
exists to repair damage caused by having two surfaces instead of one.

### The recommended build, in the order it must happen

**Rung SW-1 — the shoulder girdle, in the BODY.** Nothing above works without
it. Three features, all in `body_geom.py`:
* **trapezius** — the torso surface must keep rising past the current flat cap
  and slope *down and out* from the neck base to the acromion. Target slope
  **18–25°**, i.e. the acromion sits **55–80 mm below** the neck base.
* **clavicle** — a shallow ridge running forward and out from the sternum,
  breaking the front of the yoke so it is not a smooth egg.
* **scapular plane** — the upper back is two flattish plates standing proud of a
  spinal furrow, not a barrel.

⚠ **DO NOT MOVE `SHOULDER = (0.21, 0, 1.51286)`.** It is a rig bone. Moving it
moves the arm's pivot, the IK targets, the grip frame and the pose. The
*acromion skin* drops; the *bone* stays. This is skin, not bone — the same
distinction the existing comment at `TORSO_ROWS[-1]` already makes.

**Rung SW-2 — re-seat the scarf** (§3). The keep-out annulus is replaced by a
keep-out that follows the new shoulder surface. Settle the `neck_01` parenting
question in the same pass.

**Rung SW-3 — derive the garment from the body.** Offset the body's own surface
outward by cloth thickness (12–20 mm), relax, then cut and roll the hem, the
cuffs and the neckline. Sleeves stop being separate tubes: the body already has
arms, and the offset covers them continuously. Vertex groups are inherited from
the nearest body vertex, so there is **no new rigging work at all**.

### Two ways to do SW-3. Prefer the first.

**(a) Blender-native, non-destructive. RECOMMENDED.**
Duplicate the body mesh → **Shrinkwrap**/**Solidify** outward → **Corrective
Smooth** → **Subdivision** → cut the openings → **Data Transfer** for weights.
Chad watches it in the viewport and it is tweakable by eye, which is what "not
overcomplicating" means for a garment.
★ **This is now legal and it was not before.** The `.blend`-re-export rung
(closed 2026-08-21, `main 725e4d95d`) made **the `.blend` the source** and
retired both post-passes. A modifier stack on a mesh IS source now. Check that
law yourself before you rely on this sentence.

**(b) Analytic offset in the generator.** Keep Python; offset the body's
*implicit field* by a constant and mesh it once. Note that this uses the
machinery S1 already wrote (`LoftField`, `ChainField`, `_smin`) — but **once, on
one field**, instead of as a patch-up between shells. It is a deletion, not an
addition.

Whichever is chosen, **write it down as a ruling before building**, because (a)
and (b) put the source of truth in different places.

---

## 5. THE ORDER OF WORK

**Step 0 — OPEN THE REFERENCE PHOTOGRAPHS FIRST.** Chad asked for this
explicitly (*"look at an image of a seater wearing person online?"*) and it did
not happen this session — the attempt was cut short and the numbers in §2 were
gathered instead. It is also the standing art-rung law
(`[[art-rung-hardware-first]]`: *open the reference photos first*). What to look
for, specifically:
  * the **neck-to-shoulder slope** seen from the front, and from behind
  * where the sleeve head actually sits — a real one is a *seam line on the
    slope*, not a ball on a plate
  * how the collar sits **on** the trapezius
  * where a sweater's folds start: at the **armpit** and the **elbow crook**,
    radiating, never as horizontal bands

**Step 1** — SW-1, the shoulder girdle. Show Chad the BODY alone, no garment.
Do not proceed until he rules on the silhouette. Two attempts maximum, no
self-pass (`[[art-ladder-process]]`).

**Step 2** — SW-2, re-seat the scarf. One question to Chad about parenting.

**Step 3** — SW-3, the garment as an offset. Show him the shoulder first.

**Step 4** — folds, and only then. Sweater folds radiate from the armpit and the
elbow crook. They are never horizontal bands. If they read as bands, the fold
field is wrong, not too small.

---

## 6. THE SEVERE ISSUES, NAMED

Chad asked for this explicitly. These are the ways SW-3 goes wrong:

1. **An outward normal offset self-intersects** wherever the body's concave
   radius is smaller than the offset — armpit, crotch, elbow crook. Fix: relax
   after offsetting (Corrective Smooth in (a); a level-set offset in (b) has the
   problem by construction and is the reason to prefer a field over a mesh
   offset). **Check the armpit specifically; it is the tightest concavity.**
2. **The containment identity is lost.** The "same power, same sample count,
   same phase → radial scaling" proof does not survive an offset-and-relax. This
   is acceptable and was already the plan: `coverage.py` ray parity is the
   authority (mutation-verified 8/8) and the C2 handoff says so. But **say it
   out loud in the commit**, do not let a table-based clause keep printing PASS
   about a proof that no longer holds. That is this programme's signature
   failure.
3. **The scarf keep-out must be re-derived, not reused.** §3.
4. **The pants, boots and mitts** currently overlap the sweater by shell. Under
   SW-3 they are regions of one offset surface split by material. Simpler — but
   the hem/cuff/waist transitions all have to be re-cut.
5. **Triangle budget.** The body is 1788 verts; an offset copy is ~1788 more.
   Cheap. The current garment is 15072 verts, so this is likely a large
   REDUCTION.
6. **The pose.** All of this is authored in bind and judged in the pose. The
   scarf-parent finding (C2 §5) is proof that bind-green and pose-green are
   different questions: 0 of 908 in bind, 544 of 908 in the pose.

---

## 7. WHAT DIES, WHAT SURVIVES

**Dies** — the whole parallel-loft architecture: `HOODIE_ROWS`, `SLEEVE_R`,
`PANTS_R`, `shoulder_lift`, `_blend`, `_drape`, and the per-shell census that
goes with them. Do not mourn it. It was a correct treatment of the symptoms of a
wrong architecture, and it is worth exactly one sentence in the commit message.

**Survives and is worth keeping:**
* `body.fair_rows` / `fair_table` — a smoothing spline with one dial in metres.
  Any profile in this project wants it. ★ **The lesson generalises: a box filter
  buys C1, continuous SLOPE, and the eye reads CURVATURE.** Measured 43.9 → 1.85.
* `_smin` / `LoftField` / `ChainField` — reusable for SW-3(b) as ONE field.
* The four `no RINGS in the …` clauses, all mutation-verified. **Nothing
  measured a ring for four rounds; Chad was the ring detector.** Keep them
  pointed at whatever replaces the profiles.
* `coverage.py`, `mutverify_costume.py`, `verify_costume_live.py`. Untouched by
  any of this and still the authority.

---

## 8. WHAT NOT TO DO

* **Do not add another displacement pass.** Three have been written now
  (`_push`, a Newton projection, `_blend`) and each one traded a crease at the
  seam for a crease somewhere else. ★ **Any `continue` in a displacement loop is
  a discontinuity in a displacement field** — two neighbours, one moved 10 mm
  and one not, IS a 10 mm step. That bug was written twice, from opposite
  directions, by two different agents.
* **Do not widen the blend radius to 109 mm** to bridge §2's step. It would
  swallow the whole upper torso and it treats the symptom.
* **Do not model a "shoulder pad".** Chad has already rejected a padded
  shoulder, a shelf, a plateau and a gusset (*"I see extra se of arms under the
  good arms"*). The shoulder is a SLOPE, not an object.
* **Do not re-derive geometry Chad did not name** (`[[chad-execute-ask-exactly]]`).
  The butt, the gut, the sweater length, NO HOOD, the boots and the stripes are
  all SIGNED. This rung is the shoulder girdle, the scarf seat, and the garment
  surface.

---

## 9. THE MEASUREMENTS, REPRODUCIBLE

```python
# the 109 mm step  (run in assets/character/sudburian_src)
import math, body_geom as b, costume_geom as c
sh, el = b.SHOULDER, b.ELBOW
d = [el[i]-sh[i] for i in range(3)]; L = math.hypot(*d); d = [x/L for x in d]
up = [-d[0]*d[2], -d[1]*d[2], 1-d[2]*d[2]]; n = math.hypot(*up)
print(sh[2] + b.table(c.SLEEVE_R_F, 0.0)*up[2]/n)   # 1.6086 vs torso top 1.5000

# the 0.0 degree trapezius
print(math.degrees(math.atan2(b.NECK_Z0-b.SHOULDER[2], b.SHOULDER[0]-0.077)))

# the 26 mm scarf collision -- see section 3
```

Gate state as left: `python costume_geom.py` → **14/14 PASS**;
`verify_costume_live.py` → 0 of 908 scarf verts inside any garment or body
shell, nothing bare but the head (helmet) and the neck (inside the scarf ring).
**Green gates are not the problem. The gates never asked about a shoulder.**

---

## 10. STATE AND PROVENANCE

* **Nothing is saved. Nothing is committed.** The live Blender session holds
  everything.
* Snapshots either side of the S1 surface rung:
  `assets/character/sudburian_src/blend_snapshots/live_20260822_pre_smin.blend`
  and `live_20260822_post_smin.blend`.
* Modified in the working tree and untracked: `body_geom.py`, `costume_geom.py`,
  and the rest of the C2 list. `assets/sled/indy650.glb` still carries a stale
  T0 splice — `git checkout` it before anything lands.
* Blender rules unchanged: **agents open Blender themselves, in the GUI, never
  headless, one writer per session** (`[[blender-work-in-gui]]`,
  `[[blender-single-writer]]`). Tell the rider apart by BONE COUNT — the legacy
  19-bone rig is frozen forever (`[[two-riders-sudburian-only]]`).
* Red-team this plan from the artefact, not from this document, before burning
  an attempt (`[[red-team-major-work]]`).

---

# 11. SW-1 IS BUILT AND APPLIED LIVE — AWAITING CHAD'S RULING ON THE SILHOUETTE
*(2026-08-22, later the same day. Appended by the session that flew §5 step 0 and step 1.)*

## 11.0 State in one line
The shoulder girdle exists, the body gate is 6/6 green with the scarf collision
carried as a printed MEASUREMENT rather than silenced, and **Chad has not looked
at it yet.** Nothing is saved. Nothing is committed. SW-2 has NOT been started —
Chad ruled the scarf gets its own deliberate session.

## 11.1 Step 0 happened: `assets/character/sudburian_src/ref_sweater/`
13 photographs + `SOURCES.md` + `SW1_MEASUREMENTS.md`. **Read §7 of the
measurements file first — it is a red team's corrections and it overrides §1–§6.**

★ **THE PHOTOGRAPHS DID NOT CARRY THE ANGLE AND ARE NO LONGER CITED AS IF THEY
DID.** The first cut derived 28.5° off Gray's posterior plate; a fresh-context red
team could not reproduce it (the pixel frame quoted was 744×1157, the file is
733×1156 — the landmarks were not on that image) and re-placed it at 19–25°.
What settled it was **ANSUR II, n = 4082, computed per subject: slope median
28.9° (p05–p95 21.8–35.3), drop median 77 mm (54–100)**. `TRAP_SLOPE_DEG = 29.0`
is the population median. The photographs' job — and they did it — was the SHAPE:
a slope with a hollow in it, two clavicle ridges with a notch between them, a
LABELLED supraclavicular fossa, and how a scarf actually sits (§11.5).

## 11.2 What is in `body_geom.py` now

* **L8, the girdle.** `girdle_rings()` / `girdle_z(t, a)` — rings above the
  shoulder line whose z is a function of AZIMUTH. That is the whole rung: a
  vertical loft's highest constant-z ring IS a horizontal annulus, so "trapezius"
  is unsayable in a table of (z, hx, hy) no matter what dials are turned. The
  girdle is appended to the TORSO'S OWN loft (`vloft(..., extra_hi=)`), so chest
  and yoke are ONE surface with one winding — SW-3's principle, one rung early,
  where it is free.
  - side: acromion 1.51286 → lateral neck base **1.5888** (76 mm at 29°)
  - front: chest wall 1.504 → jugular notch **1.518** (ANSUR suprasternale)
  - that 76-versus-18 difference IS the girdle. It is why a neck looks long from
    the front and short from the side.
* **The torso now WIDENS toward the shoulder** (0.1990 → 0.2050) instead of
  narrowing to 0.1900. There was nothing out at the acromion for an arm to grow
  from; that is why the arm had to be a ball.
* **The shoulder cap is flattened and dropped** (`SHOULDER_K_UP`,
  `SHOULDER_DROP`, `limb(cap_fn=)`), and `UPPERARM_R` at t=0 is 0.075, was 0.096.
  The arm's own top surface stood at **z 1.5865 — 74 mm above the acromion.**
  That is "the sholders are too bulbous", measured on the BODY with no garment in
  the scene. The cause is structural: the rig's SHOULDER is the acromion, and a
  real deltoid wraps a humeral head 40–50 mm BELOW it, so the SKIN takes an
  offset the rig does not have. **THE BONE DID NOT MOVE.** Both operations only
  ever move a vertex DOWN, which is why L3's proof survives by inspection.
* **`back_dent(a, z)`** — the spinal furrow and the two scapular plates, as a
  radial field on the torso. A table of (z, hx, hy) can say how wide, never how
  wide IN WHICH DIRECTION.

MEASURED RESULT, the union's top surface near y = 0, neck outward:
`1.5888 → 1.5737 → 1.5464 → 1.5266 → 1.5151 (x 0.192) → 1.5173 (arm) → 1.5149 →
1.5098`. Monotone descent from neck to deltoid. **No trench, no ball, and the
highest point of the shoulder region is the neck base, as it must be.**

## 11.3 The gate: 6/6 green, and ONE clause was deliberately split

`python body_geom.py` → exit 0. Two new clauses, both mutation-verified:
* **L8 the yoke never folds back on itself** — 5/5 mutants killed. ★ Its first
  cut derived the allowance from `NOTCH_D + FOSSA_D`, i.e. from the very dials it
  guards, and the "hollow deep enough to be a fold" mutant SURVIVED. That is the
  silent-disarm trap in this repo's own lessons, written again. Absolute now.
* **L8b no girdle ring stands proud of the chest** — written after §11.4's bug 2.

★ **L3 WAS SPLIT, AND IT WENT RED FOR THE RIGHT REASON.**
  - **L3a no ARM vertex in the scarf band** — L3's actual safety argument (a ring
    perpendicular to the bone cannot reach the band; a cap sphere can). Untouched,
    still green, 0 verts. If it ever reddens, a shoulder ball is back.
  - **L3b the trapezius's intrusion is MEASURED** — **52 torso verts stand in the
    scarf's keep-out, rising 35.0 mm above its floor z 1.5167.** The band is a
    horizontal annulus around a cylindrical neck: a shape only valid on FLAT
    SHOULDERS. It is not a constraint the trapezius violates, it is a constraint
    DERIVED from the flat shoulder that has been forcing it. Every rung that
    tried to give this man a shoulder hit this annulus and gave up the shoulder,
    because the annulus was in the gate and the shoulder was not. **35.0 mm is
    the number SW-2 starts from.** It prints on every run; it is not silenced.

## 11.4 Two shape bugs found by LOOKING, that no gate asked about
1. **The clavicle ridge peaked at the front MIDLINE** — which is exactly where
   the jugular NOTCH belongs. It read as a collar, because it was one: a ring of
   raised surface on a flat plate. Now two ridges at |cos a| ≈ 0.62, a notch
   between them, the fossa above them.
2. **The roll-over rings EXTRAPOLATED the t>0 outline law backwards**, making the
   first ring 8.7 mm WIDER than the torso row it hands over from — an outward
   step at the top of the chest, i.e. a LIP, and it read as a raised rim arcing
   across his collarbones. ★ It survived turning the clavicle dial to zero, and
   that A/B is how it was caught: the feature was innocent, so the carrier was
   guilty. Fixed by interpolating to the last torso row; pinned by L8b.

## 11.5 SW-2, the scarf — Chad's words, and what that session inherits
> *"scarf should go over the sweater, but be careful with scarf it is an animated
> asset and needs careful work to put it on top of and resting over the sweater
> as it already does over where the knots are but its tails sink into the torso
> and it should not do that, scarf needs its own deliberate and careful session"*

NOT TOUCHED THIS RUNG. What it inherits:
* the 35.0 mm intrusion above, printed by L3b every run;
* `ref_sweater/E_scarf_drape_mannequin_trapezius.jpg` — the mechanism in one
  photograph: the band wraps the neck, FLARES down and out ONTO the trapezius,
  and the tail hangs from the shoulder shelf, standing OFF the chest below it.
  **"Its tails sink into the torso" is the same fact from below: the tails were
  hanging from a shelf that did not exist.** It exists now.
* the still-open `neck_01` parenting question (C2 handoff §5) — settle both in one
  pass, as §3 of this document already says.

## 11.6 ⚠ WHAT KILLED THE LIVE SESSION, so nobody repeats it
**`raise SystemExit` inside a script `exec()`d over MCP KILLS BLENDER.**
`apply_body_live.py` refuses to apply on a failed ruling by raising SystemExit —
correct behaviour, and over the MCP bridge it exited the whole process. Chad's GUI
session (open since 08-20, holding the S1 surface work unsaved) died with it.
Nothing was written or corrupted: the refusal happens BEFORE any mesh mutation.
**ALWAYS WRAP IT:** `try: exec(...) except SystemExit as e: print("REFUSED:", e)`.

Recovery: Blender was relaunched on `indy650.blend` via
`tools/blender/mcp_startup.py` and SW-1 applied cleanly. ⚠ **That file on disk has
NO COSTUME** (3990 verts, 3 material slots) — the costume only ever existed in the
live session. The current session is body + head + feet + mitts + scarf.
`apply_costume_live.py` puts it back, against a body it no longer fits; SW-3
replaces it anyway, and §5 step 1 says to show Chad the BODY ALONE first.
`blend_snapshots/live_20260822_post_smin.blend` still holds the S1 state, and
`body_geom_preSW1_20260822.py.bak` is the body generator as it was before today.

## 11.7 NEXT
1. **Chad rules on the silhouette.** Body alone, front / three-quarter / back.
   `TRAP_SLOPE_DEG` is the one dial and ANSUR's own p05–p95 is 21.8–35.3°, so
   there is a MEASURED band to move inside. `CLAV_H`, `NOTCH_D`, `FOSSA_D`,
   `SPINE_FURROW`, `SCAP_PROUD` are shape dials, NOT measured, each goes to 0.0.
2. Then SW-2 (the scarf, its own session), then SW-3 (the garment as an offset).

---

# 12. CHAD'S FIVE CORRECTIONS ON THE SW-1 BODY — ALL BUILT AND APPLIED LIVE
*(2026-08-22, same session, after he looked at §11. Gate 7/7 green, exit 0.)*

His words, in order, and what each measured as:

| # | Chad | What it measured as | What it is now |
|---|------|---------------------|----------------|
| 1 | *"the barrel chest looks bad ... shape the chest and give a bit of belly"* | total DEPTH (hy_front + hy_back) ran 0.310 → 0.298 from hip to chest: a **5 % variation over the whole man**. In profile that is a TUBE, and no belly shows on a tube because the front came forward exactly as fast as the back went away | **L9, the rib tuck.** 40 mm of front depth over 100 mm of height, under a pectoral shelf. ★ A gut does not read from how far the belly sticks out — it reads from **what is above it** |
| 2 | *"belly bulge looks a little high on the apex"* | apex at z 1.20, 220 mm above the hip line — a stomach up under the ribs | apex at **z 1.16**, and the fall-off above it is faster than the rise below, because a carried gut sags |
| 3 | *"belly budges too much to the side"* | the belly was also the WIDEST station (hx 0.2010), wider than the chest | hx 0.1930, hy_front keeps every millimetre. **A gut is DEPTH, not breadth.** ★ Exactly the inverse of the mistake L6 already records — chase it with depth alone and it reads as a V-taper; over-correct into width and it reads as a barrel. The GIRTH clause is the only honest statement of it, which is why it is the gate |
| 4 | *"the chest still needs to taper better to the collar bone, there is a bit of a rim there still I dont want to show through the sweater"* + *"the shoulders nned to smooth out the apex ... no sharp angle changes on the body topology"* | the rim was TWO changes in one row: width **+9 mm** while depth **−15.5 mm**. And measured as a turn angle, the torso/girdle junction was **75.1°in a single segment** | **L10, a LAW.** Every profile is faired before it is lofted, the ASSEMBLED chain is faired across the junction the two smooth curves used to meet at, and the worst turn is now **9.5°** against a 15° ceiling — measured by a clause, on the built vertices |
| 5 | *"the arms joins should look more the way the legs are done ... not properly recieved by the side of the upper torso"* | ray out +x at y=0: **166 mm of air at z 1.30, 108 mm at 1.34, 48 mm at 1.38**. The LEG has none — the thigh is buried (173 mm of overlap). ★ And 108 was a regression I had just caused: killing the shoulder BALL by dropping the deltoid from 0.096 to 0.075 re-opened the hole L3 had closed | the deltoid comes back to **0.100 / 0.105**, past where L3 had it, and the arm's top still sits **9 mm** above the acromion instead of 74. The two defects stopped being the same dial when `SHOULDER_K_UP` split HEIGHT from WIDTH: a deltoid is wide and not tall |
| 6 | *"the arms and legs need some anitomical shape so they dont look so cylindrical"* | structural: `limb()`'s ring was `r*cos, r*sin` — a CIRCLE. A circular section swept along a line is a cylinder however carefully its radius is tapered, and every radius table in the file was tapering the radius of a cylinder | **L11.** Sections are elliptical with an off-bone centre, in a frame that knows where the front is (`ref` = +z for arms, +y for legs). Biceps forward, brachioradialis at the elbow end, flat wrist, quadriceps forward, gastrocnemius **behind and high**, flat ankle |

## 12.1 Three traps this pass walked into, all of them old friends
1. **L10 kept reporting 75.1° after the join was faired** — it was walking the tables analytically, i.e. reading the INPUT to the fairing and calling it the output. It now walks the BUILT vertices. Same family as §11.3's L8.
2. **L2 was green while three limbs stood outside their ceilings** (thigh 0.1214 > 0.1194, calf 0.1016 > 0.0950): the clause reads the RADIUS TABLE, and since L11 the table is no longer the limb's extent — an off-centre section puts a vertex further from the bone than any number in it. A `L2 … BUILT` clause was added and the leg radii came down by exactly the measured ratios (×0.983, ×0.935). ★ Raising the ceilings instead would have changed the fit of the seat, the boot and the keep-out **silently**.
3. **The first section normalisation held the section's MAXIMUM to r**, which scales the whole section down by the size of its own bulge: the limbs lost **20 % of their volume** (upperarm 0.00713 → 0.00568 m³) for a change that was meant to be volume-neutral. It holds the AREA now, and the volumes are back to the millilitre.

## 12.2 Where the numbers stand
```
belly girth 1.109 m vs chest 1.103 m       the middle is still the biggest part of him
worst silhouette turn 9.5 deg              ceiling 15.0
arm top z 1.5223                           acromion 1.51286, was 1.5865 before SW-1
arm/torso overlap 22-50 mm at z 1.42-1.50  was 12-31 mm of air
trapezius into the scarf keep-out 43.4 mm  ceiling 54.8 -- SW-2's starting number
body 3492 verts                            was 1884
```
Still true: **nothing saved, nothing committed, the scarf untouched.**

---

# 13. ★ NEXT AGENT STARTS HERE — SUPERSEDES §5 AND §11.7
*(written 2026-08-22 at the end of the SW-1 session, as the handoff)*

## 13.0 The one-line state

**SW-1 (the shoulder girdle) is BUILT, applied live, gate 7/7 green, and Chad has
given six corrections on it which are ALL BUILT (§12). He has not yet given a
verdict on the result.** SW-2 (the scarf) has not been started and he has ruled it
gets its own deliberate session. Nothing is saved. Nothing is committed.

## 13.1 Do this first, in this order

1. Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`.
2. Read **§7 of `assets/character/sudburian_src/ref_sweater/SW1_MEASUREMENTS.md`
   before §1–§6 of it** — §7 is a red team's corrections and it overrides them.
3. Read §11 and §12 of this file. §0–§4 are still true as the diagnosis; §5's
   step 0 and step 1 are DONE; §5 steps 2–4 are the remaining ladder.
4. Connect to the RUNNING Blender session and read its inventory. **The running
   session is the truth.** If none is running, see §13.5.
5. `cd assets/character/sudburian_src && python body_geom.py` → must print
   **exit 0 and 7 PASS clauses**. If it does not, STOP and find out why before
   touching anything.

## 13.2 THE ASK ON THE TABLE

**Chad's eye on the body, alone, no garment.** Show him front, three-quarter,
profile and back. Everything below is a dial he can move without anyone
re-deriving geometry he did not ask about:

| dial | now | what it does | measured? |
|---|---|---|---|
| `TRAP_SLOPE_DEG` | 29.0 | the shoulder slope. **THE** dial | YES — ANSUR II n=4082 median 28.9°, p05–p95 **21.8–35.3** |
| `TRAP_SAG` | 0.015 | how hollow the slope is at mid-span | roughly — 17 ± 9 mm off the statue |
| `NECK_JOIN_R` | 0.068 | where the yoke closes on the neck | no |
| `CLAV_H` / `NOTCH_D` / `FOSSA_D` | 0.008 / 0.010 / 0.005 | clavicle ridges, jugular notch, supraclavicular fossa | **NO — shape only**, each goes to 0.0 |
| `SPINE_FURROW` / `SCAP_PROUD` | 0.012 / 0.005 | spinal furrow, scapular plates | **NO — shape only** |
| `SHOULDER_K_UP` / `SHOULDER_DROP` | tables | how flat and how low the deltoid cap sits | no |
| `ARM_SHAPE` / `FOREARM_SHAPE` / `THIGH_SHAPE` / `CALF_SHAPE` | tables | L11's limb sections | no |
| `JOIN_FAIR_N` / `TORSO_FAIR_N` / `GIRDLE_FAIR_M` | 4.0 / 64 / 0.016 | L10's fairing | no — but L10's **ceiling** (15°) is absolute and must not move |

★ **`ANGLE_BREAK_MAX`, `TRAP_BAND_RISE_MAX` and L8's 15 mm allowance are
CEILINGS, not dials.** Each is deliberately an absolute number and NOT derived
from the thing it guards — deriving it from the fairing or from `NOTCH_D` makes
the clause certify whatever it is given. That mutant SURVIVED the first cut of
L8 in this very file. Do not "fix" a red clause by moving a ceiling.

## 13.3 THE LADDER FROM HERE

**SW-2 — the scarf. Its own session, by Chad's explicit ruling** (§11.5 has his
words verbatim). It inherits:
* the shelf that now exists: **43.4 mm of trapezius standing in the scarf's old
  keep-out** (r 0.1060–0.1486 above z 1.5167), printed by clause L3b on every
  run. That band is a horizontal annulus around a cylindrical neck — a shape
  only valid on FLAT shoulders. **The band is wrong, not the shoulder.**
* Chad's own report of the symptom: *"its tails sink into the torso"* — the same
  fact from below. The tails were hanging off a shelf that did not exist.
* `ref_sweater/E_scarf_drape_mannequin_trapezius.jpg`, which is the whole
  mechanism in one photograph.
* the still-open `neck_01` parenting question (C2 handoff §5) — the scarf is
  parented to the CHIN-TUCK bone and rides his skull; 544 of 908 verts buried in
  the pose, 0 of 908 in bind. **Settle both in one pass.** It is one question for
  Chad, not a decision to make.
* ⚠ it is an ANIMATED asset. Careful work, not a regenerate.

**SW-3 — the garment as an offset of the body** (§4 and §6 of this file still
stand in full). Note what SW-1 changed for it:
* the body is 3492 verts, was 1884. An offset copy is cheap; the current garment
  is 15072.
* **`costume_geom.py` currently exits 1**, and correctly: `the sweater covers the
  WHOLE torso` is 14 rows short and `MESH: no bare body, anywhere` reports 1835
  bare probe points around z 1.517 — the new trapezius stands outside a garment
  cut for a flat plate. That is the parallel-loft architecture dying on schedule,
  not a regression to repair. **Do not patch `costume_geom.py` to chase it.**
* the containment identity is already gone by construction: the torso is faired
  and its girdle rings are not constant-z, so "same power, same sample count,
  same phase → radial scaling" no longer holds. `coverage.py` ray parity is the
  authority (mutation-verified 8/8) — and SAY SO OUT LOUD in the commit, do not
  let a table-based clause keep printing PASS about a proof that no longer holds.

## 13.4 WHAT SW-1 ADDED TO `body_geom.py`, in one list

Laws: **L8** the yoke (girdle rings carry a per-azimuth z), **L9** the rib tuck,
**L10** no sharp angle changes anywhere (Chad's words, made a law), **L11** the
limbs are not cylinders.
Machinery: `girdle_z` / `girdle_rings` / `girdle_profile`, `back_dent`,
`torso_faired`, `fair_series`, `fair_rings`, `table_n`, `limb(cap_fn=, shape_fn=,
ref=)`, `vloft(dent=, extra_hi=, fair_join=)`.
Clauses: **L3a** (arm out of the scarf band — L3's real safety proof, untouched),
**L3b** (the trapezius intrusion, MEASURED not silenced), **L8** (no fold-back,
5/5 mutants killed), **L8b** (no girdle ring proud of the chest), **L10** (worst
silhouette turn, on the BUILT vertices), **L2 … BUILT** (limb extent on the
vertices, not the radius table).

## 13.5 ⚠ TRAPS THIS SESSION HIT — all of them cheap to avoid

1. **`raise SystemExit` inside a script `exec()`d over MCP KILLS BLENDER.**
   `apply_body_live.py` refuses on a failed ruling that way, and it exited the
   whole process, taking Chad's session (open since 08-20, holding the unsaved S1
   work) with it. Nothing was corrupted — the refusal fires before any mesh edit.
   **ALWAYS:** `try: exec(...) except SystemExit as e: print("REFUSED:", e)`.
   Relaunch:
   `Start-Process "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" -ArgumentList "D:\flight_sim2\Game_loop_idea\vehicle_program\blender\indy650.blend","--python","D:\flight_sim2\seads-recon\tools\blender\mcp_startup.py"`
2. **THE MEASUREMENT MUST BE ON THE BUILT THING.** Three times in one session a
   clause measured a TABLE and reported PASS about a shape nobody built: L10 kept
   printing 75.1° after the join was faired (it was reading the fairing's input),
   L2 was green while three limbs stood outside their ceilings, and L6 compared
   girth at hard-coded z 1.2000 after Chad moved the apex to 1.16. All three now
   read `g["verts"]` or a table derived from it.
3. **A ceiling derived from the dial it guards is not a ceiling.** See §13.2.
4. **An A/B that exonerates a feature indicts its carrier.** The rim across the
   chest survived turning the clavicle to zero — which is how the real cause (the
   roll-over rings extrapolating 8.7 mm WIDER than the row below) was found.
5. **Normalising a section's MAXIMUM is not volume-neutral.** It cost the limbs
   20 % of their volume. Hold the AREA.
6. Blender rules unchanged: agents open it themselves, **in the GUI, never
   headless**, ONE writer. Tell the rider apart by BONE COUNT (19 = legacy and
   frozen; 44 = the Sudburian).

## 13.6 STATE AND PROVENANCE

* **Nothing saved, nothing committed.** `main` is at the tip of the C2 work; the
  whole SW-1 rung is untracked working-tree edits to `body_geom.py` plus the new
  `ref_sweater/` folder.
* `indy650.blend` ON DISK has **no costume** (3990 verts, 3 material slots) — the
  costume only ever existed in a live session. The current session is body + head
  + feet + mitts + scarf. `apply_costume_live.py` puts it back, against a body it
  no longer fits.
* `blend_snapshots/live_20260822_post_smin.blend` holds the pre-SW-1 S1 state.
* `body_geom_preSW1_20260822.py.bak` is the body generator as it was this morning.
* `assets/sled/indy650.glb` still carries a stale T0 splice — `git checkout` it
  before anything lands.
* **Red-team this from the artefact, not from this document, before burning an
  attempt.** The one red team run this session moved two numbers and refuted a
  measurement I had already written into the file.

---

# 14. SW-1b -- CHAD'S FOUR CORRECTIONS ON THE SHOULDER AND THE ARMS
*(2026-08-22, later the same day. SUPERSEDES SS13.0 and SS13.2. Everything else
in SS13 still stands.)*

## 14.0 State in one line

**BUILT in `body_geom.py`, applied live to `sudburian_proxy` and to the review
copy `SW1_BODY_ONLY`, gate 11/11 green exit 0, 6/6 mutations killed. Nothing
saved, nothing committed. Awaiting Chad's verdict.**
Backup of the previous generator: `body_geom_preSW1b_20260822.py.bak`.

## 14.1 His words, verbatim

> "there is too much of a cleavage furrow where the shoulders connect to the
> upper torso  Shoulder width is wider than the connection zone at the side of
> upper torso in out mesh but it is not true of human anatomy) (not enough chest
> meat) and torso needs to be shaped better to accept the shoulder joint, see
> legs at the hips for the way the hips accept the arms, but do it the way an
> upper torso should accept arms, and the clavical protrudes forward a bit too
> far. and arms shape is too cylindriCAL."

## 14.2 The measurement that had to come first

**L12.** Every piece in this file is an independent closed shell that
interpenetrates its neighbours, so along the intersection curve the union's
surface turns from one tangent plane to the other. With outward normals n_a and
n_b, `theta = angle(n_a, n_b)` is the crease: 0 tangent, 90 a right-angle valley.
Measured on the BUILT TRIANGLES.

| | median | worst |
|---|---|---|
| the shoulder | **104** | 122 |
| the hip -- **the bar Chad named**, identical code | **52** | 102 |

**Then every dial already in the file was swept against it** -- chest width
+-12 mm, an armpit undercut, arm burial t -0.070 to -0.180, `SHOULDER_DROP` and
`SHOULDER_K_UP` over their whole useful range. **The entire sweep moved the
median by 4 degrees, 100 to 108.** The furrow was not a dial, and finding that
out before spending the attempt is the only reason this rung landed.

## 14.3 Two things worth carrying forward more than the code

**THE HIP'S NUMBER IS NOT REACHABLE, AND THAT IS ANATOMY.** The thigh leaves the
pelvis PARALLEL to the pelvis wall, so two near-parallel surfaces cross and the
crease is shallow by construction. An arm leaves the torso at about 50 degrees
ACROSS it -- on this man and on every other one. What Chad is pointing at with
the hip is not the number, it is the MECHANISM: the pelvis carries the mass and
the thigh is buried in it. Tell him the number is unreachable rather than
quietly redefining the metric until it is reached.

**THE EYE READS THE FILLET RADIUS, NOT THE DIHEDRAL.** A real deltopectoral
groove is about 110 degrees and nobody calls it a defect, because it is a fillet
with muscle either side of it. Ours was the same angle at radius ZERO.

## 14.4 What was built

**1. `shoulder_bead()` -- the torso grows the mass that fills the groove.**
A gaussian in the signed distance to the arm's own analytic surface, times a
gaussian along the arm. Applied radially to the torso rows AND to the girdle
rings -- a field that stopped at the last row would draw a hard line across the
chest at z 1.496, which is the defect L8 and L8b already exist to prevent.
* **One-sided on purpose.** In a body the groove is filled by torso muscle, not
  by arm muscle. It also costs half the machinery.
* **Centred on the arm's SURFACE, not its axis.** An earlier cut used a fixed
  distance from the axis and was 40 mm out, because `SHOULDER_K_UP` squashes the
  ring: the surface is 74 mm from the axis underneath and 101 mm beside.
* `BEAD_T0` **0.06 -> 0.12 bought 5 degrees for free.** Centred on the shoulder
  line the bead grows as much mass inboard up the trapezius as it does at the
  crease, and L3b went red; centred on the deltoid it puts the same mass where
  the crease is.

**2. `SHOULDER_DROP` 20 -> 28 mm.** On the file's own argument: a humeral head
sits 40-50 mm below the acromion and only half of it had been given back. The
arm's top now lands at z 1.5127 against `ACROMION_Z` 1.5129 -- the deltoid is
BURIED rather than perched, which is the hip mechanism. Swept: 20 / 28 / 36 / 44
mm give a crease across the top of the shoulder of 65.4 / 47.8 / 58.0 / 77.3.

**3. Chest meat.** `hx` to 0.2145 against L2's ceiling of 0.2150.
**It stops below z 1.435 and that is not taste:** `CHEST_Z`, the station L6's
girth clause measures, is z 1.400, and widening there adds about 14 mm of girth
against a gut margin of exactly 14 mm. The first cut put +4.5 mm at 1.400 and L6
went red. The meat he is asking for is at the side of the upper torso anyway.

**4. The clavicle.** `CLAV_H` 8 -> 5 mm, and the real fault was underneath it:
the front wall receded only **18 mm** from the pectoral shelf (z 1.400) to the
top of the chest (z 1.500) and then broke at a corner. Anatomy wants 35-45 mm,
gradual. Now 25 mm and monotone, and L10 improved 9.5 -> 8.9 degrees on its own.

**5. The arms.** Eccentricity 0.92-1.12 -> 0.86-1.20 -- the old span was a true
anatomical statement rendered below the threshold at which anyone can read it.
The forearm's fullest station moved from the elbow to the belly at t 0.18, where
a real forearm's is. A deltoid V-insertion waist, riding on the OLD taper
envelope: the first cut put the waist and the belly both ABOVE the old curve and
thickened the middle of the arm by 11-13 mm, which is a fatter tube.
* **The radius paid for the section**, which is L11's own precedent in this file.
  The forearm lost about 9%. ONE scale on the first five rows reverses it.
* **The distal forearm was deliberately left alone.** The mitt gauntlet is a
  FITTED part landing at t 0.91 with an 88 x 78 mm wrist opening, and the wrist
  is inside a mitt and cannot be seen.

## 14.5 The result

Identical code, three builds: **102.0 -> 96.7 (everything but the bead) ->
66.1 median**, worst 116.0 -> 93.8. Around the arm's own axis: top 48, front 63,
back 50-66, and the **axilla 92-104, which is correct** -- an armpit is a fossa,
and the hip's own hidden fold measures 85-90 by the same code.

## 14.6 The four clauses added, and the holes they close

* **L2b** the torso RECEIVES the arm. The bead is a FIELD and L2's torso clause
  reads the hx TABLE, so the built torso stands 20 mm outside the old prism and
  nothing in the file could say so. Bound is EXTERNAL: the torso's built
  half-width must stay inside the arm's, or the arm emerges from a wall again.
* **L2c** the BUILT deltoid may not grow. **A pre-existing hole:** `DELTOID_MAX`
  is checked against the radius table, and L2's BUILT clause skips the upper arm
  BY NAME -- so the built deltoid has stood 13 mm past its own licence since L11
  landed, bounded by nothing. This is a **RATCHET, not a bound**: 1 mm above what
  this build makes, to freeze a silhouette Chad has already ruled on.
* **L2d** the deltoid is buried, not perched. **THIS CLAUSE EXISTS BECAUSE A
  MUTATION SURVIVED.** Zeroing `SHOULDER_DROP` puts the arm's top back over the
  acromion -- the visible ridge -- and L12 missed it, because the top of the
  joint is 6 points out of 200. **A median cannot see 6 points; a metric that is
  right on average is not a guard on a feature.**
* **L12** the crease itself. The axilla is excluded as a **stated absolute 90
  degree wedge** and measured L3b-style -- that is the one place this clause
  could be gamed, so the wedge is fixed rather than widened until it goes green.
  An empty join now FAILS instead of raising IndexError, and does not return
  early (which would have silenced L5 and L4 below it).
* **L8b** was overclaiming and now says so: it measures the OUTLINE TABLE only
  and is blind to the bead by construction.

## 14.7 Dials Chad can move without anyone re-deriving geometry

| dial | now | what it does |
|---|---|---|
| `SHOULDER_BEAD` | 0.0450 | how much mass fills the furrow. **THE** dial. 0.0 = the old jammed-in shoulder |
| `BEAD_W` / `BEAD_T0` / `BEAD_TSIG` | 0.070 / 0.120 / 0.260 | how far it reaches, where it is centred, how far along the arm |
| `SHOULDER_DROP` | 28 mm | how deep the deltoid is buried. Bounded by L2d against the acromion |
| `CLAV_H` | 0.0050 | the clavicle ridges. 0.0 deletes them |
| the last four `TORSO_ROWS` `hy_front` | 0.1570 .. 0.1240 | how far the chest recedes to the collarbone |
| the last four `TORSO_ROWS` `hx` | 0.1975 .. 0.2145 | the chest meat. **0.2150 is L2's ceiling** |
| `ARM_SHAPE` / `FOREARM_SHAPE` | tables | how un-cylindrical the arms are |
| `FOREARM_R` first five rows | -- | scale up together to give the forearm its 9% back; L2 will say what it costs |

## 14.8 What is still open, honestly

* **L3b is what caps the fillet**: 49.8 mm of a 54.8 mm ceiling. The scarf band
  is the binding constraint on the shoulder again, exactly as SS13.3 predicts,
  and SW-2 re-seats it. More fillet is available the moment that band is right.
* **The axilla is 92-104 degrees** and is left that way deliberately.
* **The elbow and knee still show their shell seams.** Chad did not name them;
  the same bead would close them if he wants it.
* `costume_geom.py` still exits 1 on purpose. SS13.3 still applies: do not patch it.

## 14.9 In the live session

`SW1_BODY_ONLY` is the new build. `SW1B_BEFORE`, `TORSO_BEFORE` and
`TORSO_AFTER` are hidden and registered at the SAME transform, so the before and
after can be flipped without moving the camera. The torso-only pair exists
because the arms block the profile view the clavicle has to be judged in.

---

# 15. ★ NEXT AGENT STARTS HERE — SUPERSEDES §13 AND §14
*(2026-08-22, late. §13's ladder and §14's diagnosis are still true as history;
this section is the state and the launch.)*

## 15.0 State in one line

**SW-1d is BUILT in `body_geom.py`, applied live to `sudburian_proxy`, gate
12/12 green exit 0. Nothing saved, nothing committed. Chad is about to look at
the ARMS as they now stand, before any weld.**

Backups, newest last: `body_geom_preSW1_20260822.py.bak`,
`body_geom_preSW1b_20260822.py.bak`, `body_geom_preWELD_20260822.py.bak`.

## 15.1 Do this first, in this order

1. `CLAUDE.md`'s three standing laws, then `docs/RIDER_AUTHORITY.md`.
2. **This section, then §14, then §13.** §13.2's dial table is superseded by 15.4.
3. `cd assets/character/sudburian_src && python body_geom.py` → **12 PASS,
   exit 0**. If not, STOP and find out why before touching anything.
4. Connect to the RUNNING Blender session and read its inventory. If none is
   running, relaunch per §13.5 trap 1 and re-run `apply_body_live.py` — nothing
   is ever saved, so that is the whole recovery.

## 15.2 What Chad asked for, and what each ask turned out to be

He gave four corrections on the SHOULDER AND ARMS (§14.1), then three more
looking at the POSED rider. Every one of them was a real defect and two of them
were pre-existing bugs that nothing in the gate could see.

| his words | what it actually was |
|---|---|
| "a giant nearly 90 degree void ... hands on handlebars" | every number in SW-1b was measured in BIND; the pose reads 85.0 where bind read 66.1 |
| "the fix attempt ... was a bandaid" | the bead was a tube around ONE arm position and the arm swings **74.6°** out of it |
| "looks like you applied a twist to the arms" | the weld's phase alignment compared progress-from-each-loop's-own-start, which is 0 for both, so it aligned nothing |
| "I see traingular holes in the joints" | 143 sliver triangles, worst min-angle 0.38° — a needle shades as a dark slit |
| "its full of holes and hbroken" | 293 edges folded past 100°, 162 past 160° — my edge-flip pass, which improved every angle metric while folding the surface |
| "the right arm is different from the sudburian's left arm" | **`eb = cross(tang, ea)` DOES NOT MIRROR** — see 15.3 |
| "sticks out the back end in a point" | the bead was a genuine ridge; **L10 only ever walked the columns** — see 15.3 |

## 15.3 ★★★ THE TWO PRE-EXISTING BUGS. Read these before anything else.

**THE MIRROR BUG.** `eb`, the arm section's front-back axis, is a CROSS PRODUCT.
Mirroring a limb across x = 0 flips its sign *on top of* mirroring it, so eb came
out +y on the left arm and −y on the right. `ob` is documented as "the biceps
forward of the bone" — and it pushed the left arm's mass FORWARD and the right
arm's BACKWARD. MEASURED before the fix: **160 of 194 left-arm vertices had no
mirror twin, worst mismatch 14.7 mm.** Present since L11 landed. Chad saw it with
his eye before any clause did; there was no symmetry clause, and there still
isn't one — **writing L14 (the body is mirror-symmetric) is cheap and overdue.**
★ THE FIX IS ON `ob`, NOT ON `eb`. Flipping eb makes the frame left-handed, which
reverses the ring's traversal and inverts that shell — the census would go red on
a negative volume. Negating the OFFSET puts the mass at the front on both sides
and leaves the handedness alone. The LEGS were never affected: their tang has no
x-component, so their eb does not flip.

**L10 ONLY EVER WALKED THE COLUMNS.** It measures each azimuth's (radius, z)
profile — the silhouette from the side — and is blind BY CONSTRUCTION to a corner
that runs UP the body. `shoulder_bead` is a localised radial swell, so it bends
the surface in BOTH directions and only one was guarded: the bead sat at an
excess of **30.3°** with L10 green at 14.0. New **L10b** measures the turn AROUND
a constant-z row.
★ AND I READ IT WRONG THE FIRST TIME, WHICH IS WORTH THE SPACE: watching the ring
turn fall with resolution I called it "a sampling fault". Measured properly — on
CONSTANT-Z ROWS ONLY, because the girdle's rings deliberately carry a per-azimuth
z and a 3-D turn there is the yoke doing its job — the excess runs 36.1 / 30.3 /
29.2 at N = 24 / 48 / 72. **It converges. It was a real ridge.** The bead is a
gaussian in distance to the ARM'S SURFACE, and that distance changes fast with
azimuth behind the shoulder. The fix was to WIDEN it, not to resample it.

## 15.4 The dials, superseding §13.2

| dial | now | what it does |
|---|---|---|
| `SHOULDER_BEAD` | 0.0450 | how much mass fills the furrow. **THE** dial; 0.0 = the old jammed-in shoulder |
| `BEAD_W` | 0.1450 | how far it reaches. **This is the anti-ridge dial** — 0.085 gave an excess of 30.3°, 0.145 gives 14.8 against the body's own 10.0 |
| `BEAD_T0` / `BEAD_TSIG` | 0.120 / 0.260 | where along the arm it is centred, and how far it reaches |
| `BEAD_SWEEP_N` / `BEAD_SWEEP_MARGIN` | 7 / 1.15 | the swept envelope. **N = 1 reproduces SW-1b's single-pose tube exactly**, which is how L12's A/B is produced |
| `RIDE_SWING_AXIS` / `RIDE_SWING_DEG` | measured | **RIG DATA, like SHOULDER and ELBOW. If the ride pose changes, RE-MEASURE — it is not derivable** |
| `TORSO_N` | 48 | was 24. Halves the faceting and is the density the weld's cut needs |
| `SHOULDER_DROP` | 28 mm | how deep the deltoid is buried; bounded by L2d against the acromion |
| `CLAV_H` | 0.0050 | the clavicle ridges; 0.0 deletes them |
| `ARM_SHAPE` / `FOREARM_SHAPE` | tables | how un-cylindrical the arms are. `ob` is now sign-corrected per side |
| `FOREARM_R` first five rows | — | scale up together to give the forearm back the ~9% it paid for its section |

★ CEILINGS, NOT DIALS: `ANGLE_BREAK_MAX` 15, `RING_EXCESS_MAX` 20,
`CREASE_MED_MAX` 82, `CREASE_P90_MAX` 100, `UPPERARM_BUILT_MAX` 0.1195 (a
RATCHET on a silhouette Chad already ruled on), L3b's 54.8 mm. Each is stated
against something OUTSIDE the thing it guards. Do not fix a red clause by moving
one.

## 15.5 The clauses SW-1b→1d added, and the hole each closes

* **L2b** the torso RECEIVES the arm — L2's torso clause reads the hx TABLE and
  cannot see a FIELD; the bead stands 42 mm outside the old prism.
* **L2c** the BUILT deltoid may not grow — `DELTOID_MAX` is checked against the
  radius table, and L2's BUILT clause skips the upper arm BY NAME, so the built
  deltoid has been 13 mm past its licence since L11. A ratchet, not a bound.
* **L2d** the deltoid is buried, not perched — **exists because a mutation
  survived**: zeroing `SHOULDER_DROP` puts the arm's top back over the acromion
  and L12 missed it, because the top of the joint is 6 points out of 200 and **a
  median cannot see 6 points**.
* **L10b** no sharp angle changes AROUND the body — see 15.3.
* **L12** the arm is RECEIVED IN EVERY POSE — measured across the arm's travel,
  not at one pose. An empty join FAILS rather than raising IndexError, and does
  not return early (which would silence L5 and L4 below it).
* **L8b** was overclaiming and now says so: it measures the OUTLINE TABLE only.

## 15.6 THE WELD — paused, and the prototype is preserved

`assets/character/sudburian_src/weld_prototype/` with its own README. It reaches
WATERTIGHT (0 unbalanced edges of 13767, slivers 143 → 7) but **12 edges still
fold past 100°**. Chad ruled it back himself — *"that is an existing problem this
weld is trying to cover over"* — and he was right; 15.3 is what was underneath.
**Re-cut it on the corrected body.** The four algorithmic traps that each cost a
round are in that README; read it before writing a line of stitching code.

## 15.7 ⚠ TRAPS THIS SESSION HIT

1. **★★★ I FROZE CHAD'S BLENDER AND HE HAD TO KILL IT.** A diagnostic counted
   boundary edges as O(edges × faces) — ~385 million operations — and Blender
   executes Python on its main thread. **`TaskStop` only abandons YOUR wait, not
   Blender's execution.** Every line you send into his session must be O(n) with
   a small constant. Recovery is cheap (relaunch, re-apply) but it is his
   session and it was open with unsaved work.
2. **A cached `area.spaces[0].region_3d` SILENTLY STOPS APPLYING** after a UI
   change — writes go to a dead pointer and the viewport does not move. I read
   three identical screenshots as "no change". Re-resolve the region every call.
3. **A watertight mesh can still be visually broken.** The edge-use histogram
   proves manifoldness and says nothing about the surface passing through
   itself. I reported "no holes" from it while the shoulder was folded.
4. **MEASURE IN THE POSE**, and take anatomical bins in the TORSO'S OWN FRAME —
   the rider leans ~30°, so world +z is not his up, and binning the axilla
   against it reported 97.5 for a join whose thetas were identical to 0.004°.
5. **Defaults bind at import.** `def f(..., n=TORSO_N)` captures the value when
   the module loads, so setting `B.TORSO_N` later changes nothing. Pass it.
6. Blender rules unchanged: agents open it themselves, GUI never headless, ONE
   writer, tell the rider apart by BONE COUNT (19 = legacy and frozen).

## 15.8 What is open

* **CHAD'S EYE ON THE ARMS**, as they stand, before any weld. That is the
  immediate item and everything else waits behind it.
* **L14, the symmetry clause** — the mirror bug had no guard and still has none.
* the weld, re-cut on the corrected body (15.6).
* the ride pose's join is 78–80° median where bind is 68; the axilla is 97.6 and
  is left that way deliberately (an armpit is a fossa).
* L3b sits at 54.6 of 54.8 mm. The scarf band is at its limit again — but note
  it is NOT what blocks the shoulder: doubling the bead past it moved the ride
  pose 71.7 → 71.9, so do not spend a question on it.
* the forearm still owes ~9% of its radius (§14.4 item 5).
* `costume_geom.py` still exits 1 on purpose. §13.3 stands: do not patch it.
