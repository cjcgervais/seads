# HANDOFF — SW-1e/1f: THE SHOULDER WELD AND THE GROIN (Chad asleep, awaiting his eye)

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then §0
and §1 of this file. `body_geom.py` gates 14/14 and `weld_geom.py` 6/6, both
exit 0 — run them BEFORE touching anything. Connect to the RUNNING Blender
session and re-apply with `apply_weld_live.py`; nothing is saved and nothing is
committed. The immediate item is CHAD'S EYE on the welded shoulder and on the
groin. **Read ADDENDUM 6 at the end FIRST — the deltoid, on
Chad's ruling, and the P0 that says the SHELL GATE DOES NOT CERTIFY WHAT HE
SEES: run gate_live.py inside Blender. ADDENDUM 5 is the weld it sits on;
1-4 are the road, and their measurements stand though their geometry does not.** ADDENDUM 1 and 2 are the road to it and
their measurements stand; their geometry does not. Run `python
mutverify_weld.py` as well (6 mutations, ~25 s). Open: W7's join, W8's crown,
and §5 the pair-of-pants."*

> **STATE:** the shoulder is WELDED (torso + both upper arms are ONE shell, with
> diffused skin weights), the shoulder is 9.6 mm narrower per side and its top
> ridge is 20% smoother, and the groin is BUILT as a front-to-back arc that
> replaces the flat plate. All applied live. **Nothing saved, nothing committed.**

Supersedes §15 of `docs/SESSION_HANDOFF_20260822_sweater.md` for the shoulder;
that file's §15.3 (the two pre-existing bugs) and §15.7 (traps) still stand.

Backups, newest last: `body_geom_preSW1e_20260823.py.bak`,
`body_geom_preGROIN_20260823.py.bak`.

---

## 0. WHAT CHAD ASKED FOR, AND WHAT EACH ASK TURNED OUT TO BE

| his words | what it actually was |
|---|---|
| "smooth out the top shoulder ridge" | the shoulder BEAD's own edge — `CLAV_H` and the yoke fillet do not move it at all (swept, 11.7° at every one of 9 combinations) |
| "bring in the width a little" | the bead again. The table asks for a 21.5 mm flare from belly to shoulder; the BUILD delivered 65 mm. The extra 42 mm was the bead, an unsanctioned fourth growth on a body whose own L2 allows three |
| "the shoulders are wider than the torso that is a problem I can see" | MEASURED and true: belly hx 0.1919, shoulder hx 0.2570 — the widest part of the man was his shoulders |
| "make sure the arms are aligned to the torso" | they are, now exactly: L14 reads 0.0000 mm. Getting there found a *third* instance of the mirror bug |
| "the weld has to be nice to pass" | it does, and the first weld that certified green was NOT nice — see §2 |
| "build out the groin ... continuous front to back" | the trunk simply STOPPED in a flat 300 × 250 mm disc at z 0.90 |
| "No gap — merge them" (his ruling on where the room comes from) | see §5: buildable in every respect except the last stitch |

## 1. DO THIS FIRST, IN THIS ORDER

1. `CLAUDE.md`'s three standing laws, then `docs/RIDER_AUTHORITY.md`.
2. `cd assets/character/sudburian_src`
   * `python body_geom.py` → **14 PASS, exit 0**
   * `python weld_geom.py` → **7 PASS, exit 0** (~3 s)
   If either is not green, STOP and find out why before touching anything.
3. Connect to the RUNNING Blender session, read its inventory, and
   `exec(open(".../apply_weld_live.py").read())`. Nothing is ever saved, so that
   is the whole recovery. `apply_body_live.py` still works and puts down the
   UNWELDED twelve shells — the two scripts strip each other's output.
4. Then §2 (what the weld is), §3 (the width), §5 (what is open).

## 2. THE WELD — WHAT IT IS AND THE TWO GATES THAT NEARLY SHIPPED WRONG

`weld_geom.py`, promoted out of `weld_prototype/weld.py` on Chad's ruling. The
prototype dir stays as the record of the four traps in its README; read it
before writing stitching code.

Torso and both upper arms are cut against each other, the boundary loops
bridged by a minimal-area DP, the seam Taubin-faired, slivers flipped out and
the last folds relaxed. **7 clauses, W1–W7, and THREE of them exist because a
green gate was wrong** (W7 is the addendum's — see the end of this file):

* **W2's bar was measured off a CAP.** The first version took the sharpest
  dihedral anywhere on the unwelded body — 92.4°, and every one of those worst
  values is a cap RIM (the disc at z 0.90, the elbow, the knee, the arm's own
  proximal end, which the weld cuts away anyway). A cap is allowed to be sharp.
  Certified against it, the seam measured **86.5°, passed green, and the crease
  was plainly visible in Chad's viewport.** The bar is now the worst turn on the
  SMOOTH surface, cap fans excluded on both sides: **40.1°**, the girdle
  handover at the base of the neck. The weld now sits AT that bar and the worst
  edge on the welded body is that same pre-existing feature, not the seam.
* **W6 did not exist, and W1–W5 were all green while the LIVE POSED body folded
  179.7°.** A welded shoulder spans two bones, so it needs blended weights; two
  attempts to compute them from geometry both failed, the first because
  `arm_surface_dist`'s `t` is an INFINITE cylinder (the whole lower back reads
  t > 0.34, so arm weights landed on the middle of his back — the weld README
  warns about exactly this and I walked into it anyway), the second because a
  blend defined only inside the seam band has a step at the band's EDGE.
  The weights now DIFFUSE over the mesh graph from the rigid rule, pinned
  outside 8 rings of a boundary. They live in `weld_geom.weights()`, not in the
  apply script, so the gate can measure them — and W6 measures them IN THE POSE,
  across the arm's whole 74.63° travel. **A gate that measures one pose is this
  programme's signature failure and this file made it again.**

★ The diffusion covers EVERY rigid boundary, not just the seam: the first
version was pinned to the seam alone and W6 stayed red at 179.4° — at z 1.07,
the PELVIS/spine_02 line, nowhere near a shoulder. That is a pre-existing defect
of the body (one lofted trunk across three bones split by a hard z threshold,
and the ride pose rotates the pelvis 31°) that nothing had ever measured. The
welded trunk now reads **2.7× better than the unwelded one in the same poses.**

## 3. THE WIDTH — AND WHY "A LITTLE" IS ALL THERE WAS

`SHOULDER_BEAD` 0.0450 → **0.0360**. Width 0.2570 → 0.2473 (−9.6 mm/side), ridge
excess 14.8° → 11.9°. Every clause green.

**The width and the arm join are the same mass.** Swept (post-`ob_sign` fix):

```
bead   0.045  0.042  0.040  0.037  0.036  0.035  0.025  0.000
width  0.2569 0.2537 0.2516 0.2484 0.2473 0.2462 0.2355 0.2089
ridge   14.8   13.4   12.7   12.1   11.9   11.6   10.2   10.0
join    80.4   80.5   81.0   81.2   81.2   82.1   84.3  109.9   (L12 ceiling 82.0)
```

0.0360 is the last green value. **Three other routes to more width were measured
and all hit the same wall** — they are written into `body_geom.py` so nobody
spends a round re-deriving them: narrowing the shoulder rows (−5 mm costs 1.7°
of join, red by −10 mm); sliding the bead outboard (`BEAD_T0` 0.18/0.24 buys 6
and 14 mm, red at 0.24); burying the arm's root deeper (does not move the join
AT ALL, because L12 measures the VISIBLE crease and buried mass is by definition
not visible). **If Chad wants more width, the honest price is ~3° on the arm
join — still better than the 88.2 he rejected a rung ago — and that is his call,
not a dial to turn quietly.**

## 4. THE GROIN — SW-1f

MEASURED, what was there: the trunk stopped at z 0.90 in a flat horizontal disc
300 × 250 mm across with a hard rim. Hidden inside the legs in bind; **fully
exposed in the ride pose**, because the hip flexes 95.345° (measured off the live
rig, both sides) and swings the thighs out from under it.

It is now a front-to-back ARC — an elliptical lens (43 × 89 mm, area-preserving
`ka*kb = 1.0`) swept along a smooth path from inside the buttocks, under him, to
inside the belly. **How low it hangs is measured, not chosen:** a midline point
stays inside a thigh across the hip's whole travel only down to about z 0.90, so
the perineum is placed at 0.9003. Two things fell out of building it that are
worth keeping:

* `CROTCH_LOW` is a path CONTROL POINT and the arc does not reach it — the bend
  cuts the corner, so the built underside sits 10 mm above it. **Solved
  numerically, not chosen** (0.945/0.930/0.920/0.910/0.900 → 0.9201/0.9115/
  0.9058/0.9002/0.8947). Re-solve if `CROTCH_BEND` moves.
* `chain_path` lays points out in the PARAMETER, and at this bend that runs
  2.7 mm / 27 mm / 1.1 mm along the arc — a 25× swing. A 1.1 mm row on an 89 mm
  ring is a needle (418 of them) and the step from 27 mm to 1.1 mm reads 168.8°.
  The path is now resampled to uniform arc length. **Both were sampling, not
  shape.**

**L15** is the clause: it walks the midline underside in 37 raycast stations from
the buttocks to the belly and asks for no gap AND no step. ★ The step is the
discriminator, not the gap — the flat plate has 0 gaps too (a disc is perfectly
continuous); what it has is a 49.5 mm cliff at its rim. A clause that only asked
"is there surface under him" would have passed the very thing Chad pointed at.
Now: 0 gaps, worst step 5.8 mm, ceiling 20. Mutation-verified (collapse the
groin → L15 red at 75.7 mm).

## 5. ⚠ THE ONE THING THAT IS NOT DONE: THE PAIR OF PANTS

Chad asked for the weld on the groin too. **It is the one junction on this body
the two-loop bridge cannot make, and that is topology, not tuning.**

MEASURED: the groin runs the whole length of the trunk's underside and dips
below it in the middle, so cutting the trunk against it splits the trunk's
bottom clean in two — a BACK apron (y −0.124..+0.008) and a FRONT apron
(y +0.015..+0.109). The junction is then **THREE boundary loops**, two trunk and
one groin. `bridge_dp` makes a CYLINDER, which consumes exactly two.

The repair is standard and the analysis is already done and kept in the file:
`splice_loops()` joins the two apron loops into one with the SHORTEST chord
(measured, **7 mm** — so the pinch is smaller than one triangle; a carelessly
chosen chord would be 230 mm and would fold the crotch in half). What it needs
is a `bridge_dp` that tolerates a loop **visiting a vertex twice**, which a slit
requires by construction and which today's DP turns into degenerate triangles.
MEASURED on the attempt: **198 unbalanced edges, 267 needles, folds at 178°, all
six clauses red.** That is the next session's job and it is real work.

**Until then the groin INTERPENETRATES**, exactly as the neck, both forearms and
all four leg segments still do — and it is built so that this costs nothing to
look at: the trunk is tucked to 140 × 120 mm by z 0.925, which is INSIDE the
groin's own lens, so the flat plate is buried inside the groin rather than
welded away. From outside it reads the same. What it is not is one shell, and W1
counts it honestly as its own.

**The legs are not welded either**, and the reason is Chad's own ruling plus a
measurement: the thighs are TANGENT AT THE MIDLINE (inboard reach |x| 0.000–0.011
on a hip offset of 0.095 and a radius of 0.116 — deliberate, it is how the hip
has no visible join), so there is no room between his legs for a separate crotch
surface. He ruled "no gap — merge them", which is exactly the pair of pants.
**Solve §5 and the legs come with it.**

## 6. THE THIRD MIRROR BUG, AND THE CLAUSE THAT NOW GUARDS IT

`arm_surface_dist` never got the `ob_sign` fix that `limb_rings` got, so on one
side the bead was measuring its distance to an arm whose biceps sits BEHIND the
bone — a surface nobody lofted. Distance field 6.4 mm out of mirror, the bead it
feeds 1.4 mm, the built torso 0.72 mm.

**L14** (body) and **W4** (weld) are the guards, and they are EXACT, not
toleranced: both sides come off the same code, so a correct mirror is exact to
float round-off. W4 then caught two more that no eye would have:

* **a fixed quad diagonal is not mirror-invariant.** 8940 of 9068 faces had no
  mirror twin, and because Taubin averages over each vertex's ADJACENCY it
  smoothed the two shoulders **23.6 mm apart** on a body whose vertices are
  identical to 0.0000 mm. `grid_faces` now cuts the SHORTER diagonal, which is a
  property of the quad and so mirror-invariant (the tie-break is |x| sums, never
  a signed x or an index parity).
  ★ AND THE FIRST DIAGNOSIS WAS WRONG: the obvious suspect was `bridge_dp`
  picking two different correspondences. It was not. With the diagonal fixed,
  stitching each side INDEPENDENTLY measures 0 asymmetric faces and 0.0 mm — a
  mutation of the function proves it. The seam is still built once and mirrored,
  but only because that is exact by construction and costs half the DP.
* **`flip_slivers` is greedy over a dict** and its winner depends on hash order.
  It moves no vertex, but the topology it leaves feeds `unfold`, which does:
  46 of 5518 vertices lost their twin. Every accepted flip now carries its
  mirror twin in the same breath.

## 7. TWO MORE FIXES WORTH NOT REDISCOVERING

* **`flip_slivers` guarded only the flipped edge.** Flipping a diagonal changes
  both triangles' NORMALS, so it changes the dihedral of all four SIDE edges
  too — and that is where the fold appeared: Taubin left the shoulder at 40.1°
  and this pass drove it to **107.9**. The guard now takes the worst over all
  five affected edges, before and after, with the body's own measured bar as the
  floor rather than a number typed in.
* **`unfold` judged itself on the GLOBAL worst turn.** It can only move band
  vertices, so one stubborn edge anywhere pins the metric, the first round
  "fails to improve", rolls back, and the pass ends having done nothing. The
  shoulder went 40.1 → 93.4 with nothing at the shoulder having changed.

## 8. WHAT IS OPEN

* **CHAD'S EYE** on the welded shoulder, the narrower shoulder, and the groin.
  That is the immediate item.
* **§5, the pair of pants** — and with it the leg weld and his "merge them"
  ruling, in full.
* the width question in §3: another ~12 mm is available for ~3° of arm join.
* the pelvis/spine_02 fold at z 1.07 is fixed *inside the welded shell* by the
  diffused weights; the pieces still outside the weld (both forearms, all four
  leg segments, the neck) keep the old rigid one-bone-per-vertex skinning.
* `L3b` sits at 54.5 of 54.8 mm. The scarf band is at its limit again; §15.8 of
  the previous handoff already ruled not to spend a question on it.
* the forearm still owes ~9% of its radius.
* `costume_geom.py` still exits 1 on purpose. Do not patch it.

## 9. ⚠ TRAPS THIS SESSION HIT

1. **A screenshot that never changes is not proof of nothing changing** — but it
   was telling the truth here. Three identical shots of the shoulder were real:
   the seam was genuinely still creased at 86.5° while the gate said green,
   because the BAR was wrong (§2). **Chad's viewport found it before the clause
   did, for the second rung running.**
2. **The apply script leaked 248 loose vertices per run** (9306 → 9656 → 9904).
   The cut leaves vertices that belonged only to removed quads; they render as
   nothing, so they are invisible — and they are also invisible to the STRIP,
   which works through faces. Now dropped before appending.
3. **A `★` in a printed clause detail crashes the gate on this box** —
   `cp1252` cannot encode it. Non-ASCII belongs in comments, never in a string
   that reaches stdout. (Same family as the Catch2 lesson in `CLAUDE.md`.)
4. **`bpy.context` over MCP is not the GUI's context.** `visible_get()` reported
   the proxy hidden while it was plainly on screen. Do not act on a visibility
   read from a script; and if you hide anything to take a shot, record the names
   and put them back in the same call.
5. Blender rules unchanged: agents open it themselves, GUI never headless, ONE
   writer, tell the rider apart by BONE COUNT (19 = legacy and frozen), and
   every line sent into his session must be O(n) with a small constant.

---

# ADDENDUM — SW-1g: CHAD'S ARM DEFECT, HALF FIXED AND HALF DIAGNOSED

**His words, 2026-08-23, on the welded shoulder:**
> "shoulders / upper arms are squished, big dip and visible ring distal of the dip please fix"
> "right at the connection from top down there is a huge inside angle like a right angle, too thin there and then deformed arms distal to that weld zone"

**All seven clauses were green when he said it.** W5 read "the weld moved
NOTHING outside its own seam band" — true, and the damage was entirely INSIDE
the band. That is the fourth true-report-of-a-question-nobody-asked in this
programme, and the first thing this addendum does is close it: **W7** now
measures the arm's radial change about its own bone axis, weld vs the shells
the weld was cut from.

## What it measured

Arm radial change, one-strip weld at the old 120 Taubin iterations:

```
t     0.00   0.05   0.10   0.15   0.20   0.25   0.30   0.35
mm   +23.8   -9.5  -16.7  -13.8   -8.7   -5.7   -6.1    step
```

A bulge, then a **14–17 mm waist**, then a step where the band ends. That is his
dip, his ring, and his "too thin at the connection", exactly.

## FIXED: the distal half

**`TAUBIN_ITERS` 120 → 8, and the 120 was an artefact.** It had been swept
against a `flip_slivers` that was *folding* the surface (§7 of the main
handoff — it guarded only the flipped edge, not the four side edges whose
normals also change). The smoother was being asked to clean up after a pass
that was making the mess. With that guard fixed, **8 iterations reach the same
bar with zero slivers**:

```
t     0.00   0.05   0.10   0.15   0.20   0.25   0.30   0.35
mm   +23.8   -9.5  -14.0   -5.6   -7.0   -6.5   -2.6   +0.6
```

Distal deformation **-13.8 → -5.6 mm at t 0.15** and to nothing by t 0.35. The
"ring distal of the dip" and the "deformed arms distal to the weld zone" are
gone. W7 is a **RATCHET at 7.5 mm** that says so in its own text — the number it
*should* be is 2.5 mm (`UPPERARM_R`'s deltoid V-insertion waist, the smallest
feature the arm is supposed to have). **Do not raise it to make a change fit.**
Mutation-verified: the old 120-iteration build now goes RED on W7.

## NOT FIXED: the connection itself — and it needs a real fillet

**+23.8 mm at t 0.00 and −14.0 mm at t 0.10 are unchanged.** That is his "huge
inside angle like a right angle, too thin there", and it is STRUCTURAL:

> `bridge_dp` stitches the two loops with a strip **one triangle wide**, built
> only from the loops' existing vertices. Such a collar has no interior, so it
> has nothing of its own to fair — the only way to smooth the junction is to
> move the loops, and the arm's ~85 mm cut ring is being averaged against the
> torso's ~500 mm one.

Three routes were measured and all fail, so **do not re-derive them**:

* **less smoothing** — 4/8/16/24/120 iterations take 11.6/14.0/14.7/14.6/16.7 mm
  out of the arm. Even a nearly-off smoother eats 12 mm.
* **restore the radius afterwards** (`restore_limb`, written, **not wired in**) —
  at 120 iterations it was worth 10 mm; at 8 it measures as a **no-op** (−7.0 vs
  −6.8 at t 0.20). Its own mutation is green, which is how that was found. A
  short fade folds the surface at 153.6°; a long one cannot reach the pinch.
* **pin the arm during the smoothing** (`taubin(pin=...)`, wired and available) —
  gives the arm back perfectly (**+0.0 mm**) and folds the surface at **80.5°**.
  The strip has no freedom left to absorb the difference.

**The fix is `collar()`** — a fillet with rings of its own between the two loops,
so the arm and torso keep body_geom's authored surface and the collar fairs in
its own interior. It is written and **NOT WIRED IN**; two correspondences were
tried and both are recorded in its docstring:

1. nearest-point projection onto the torso loop — many vertices project to the
   same place, the rings collapse, **179.5° and 20 needles**;
2. the DP's own matching (each arm vertex → centroid of the torso vertices it
   was joined to) — the rings are sound, but the seam then contains only the
   NEW vertices, so the smoothing cannot touch the loops and the raw **178°**
   crease survives at both ends of the fillet.

**What (2) still needs:** the fairing band must include a ring or two of the
shells either side of the collar *while pinning the arm's own silhouette* — i.e.
`taubin(pin=...)` over a band that spans the collar, not the collar alone. That
is one experiment, not a rewrite, and (2) is the right starting point.

★ And note **`COLLAR_RINGS` binds at import** into `collar()`'s default — setting
`W.COLLAR_RINGS` from a sweep script changes nothing (the "defaults bind at
import" trap, §15.7 item 5 of the previous handoff, hit again). Pass it.

## Mutation coverage that LAPSED, and it should be said

`MUT flips unmirrored` used to turn W4 red. At 8 iterations the smoothing is so
light that the asymmetric flips no longer move a vertex far enough to break the
mirror, so that mutation is now green. **W4 is still correct and still guards the
class; it simply has no live mutation at this iteration count.** If anyone
raises `TAUBIN_ITERS`, re-run it.

---

# ADDENDUM 2 — SW-1g CLOSED: THE CONNECTION. It was never the fillet, it was the CUT.

**STATE:** `collar()` is WIRED IN, the cut is GENEROUS, and the defect Chad
named is measured out. Applied live in his running session; **nothing saved,
nothing committed.** Backup of the pre-session file:
`weld_geom_preCOLLAR_20260823.py.bak`.

## What he said, and what it reads now

> "right at the connection from top down there is a huge inside angle like a
> right angle, too thin there and then deformed arms distal to that weld zone"

Arm radial change about its own bone axis, weld vs the shells it was cut from:

```
             t 0.00  0.05  0.10  0.15  0.20  0.25  0.30  0.35
was (SW-1g)   +23.8  -9.5 -14.0  -5.6  -7.0  -6.5  -2.6  +0.6
now            (join +3.1 at t 0.13)  -2.9  -2.8  +3.0  +3.5  -2.0  -1.3
```

**Worst anywhere on the arm, the join included: 3.5 mm** (was 23.8). W7's
ratchet comes down 7.5 → **4.0 mm** and the clause now covers the join — the
exemption existed because a one-triangle collar structurally could not fair
it, and that is no longer true. The target is still 2.5 mm.

Every clause green, and the ones that are not pass/fail moved too: the seam is
at the body's own bar (40.1°, 0 edges over), 0 needles, and W6's posed worst
went 80.9° → 57.0°, a 2.2× improvement on the unwelded trunk to 3.1×.

## THE FINDING: three rings in a gap narrower than one quad is not a fillet

The previous session's diagnosis was right — a one-triangle-wide collar has no
interior, so the only thing it can fair with is the arm — and its proposed
experiment (**pin the arm, fair a band spanning the collar**) was **measured
and it FAILS**: 162.9–176.1° and 6–10 needles at `free_rings` 0 / 1 / 3 / 5, on
both cuts, at every ring count. It is wired as `PIN_RINGS` and
`mutverify_weld.py` runs it, so the dead end stays measured.

What was actually missing was **room**, and the number nobody had taken is the
gap between the two boundary loops:

```
                       min    mean    max
STRICT cut (all four)  3.2    17.4    32.5 mm
GENEROUS cut (any)     6.4    34.6    62.6 mm
```

**The strict cut was chosen to serve the one-triangle bridge** — the file says
so, and warns that a generous cut leaves "long thin triangles that wrinkle the
moment they are faired". That warning is TRUE OF A ONE-TRIANGLE STRIP and
false of a collar with rings: the rings fill the span with quads and the thin
triangles never arise. So the trade is taken the other way round. MEASURED at
`COLLAR_RINGS = 2`: strict 130.7° / 8 needles, generous **40.1° / 0 needles**.

★ The lesson to carry: **the constraint that blocked the fix was a choice made
for the thing being replaced.** Both sessions swept the fillet's own dials
(rings, iterations, reach, restore fades, pins) and the answer was in a
parameter that belonged to the *cut* — the fillet was fine, it had nowhere to
live.

## The three dials that moved, all swept, all mutation-verified

| dial | was | now | why that value |
|---|---|---|---|
| `STRICT_CUT` | `True` | **`False`** | doubles the gap; the only change that makes rings possible |
| `COLLAR_RINGS` | 0 (unwired) | **2** | 0→+11.5 mm join, 1→6.7, 2→3.5, 3→3.4, 4 and 5 RED |
| `TAUBIN_ITERS` | 8 | **4** | plateau 2–6 all green (2.4/2.9/3.5/4.1/5.2 mm); 4 is its middle, 0 leaves needles, 8 is back to 6.6 |

`TAUBIN_REACH` stays 5. Swept 3/4/5/7 — everything green, and the posed worst
(W6) wanders 42.9–79.6° with no trend across it, so it is not a dial to tune
on: **it is the front-of-shoulder edge at z 1.50 and it is rugged.** Do not
pick the lucky minimum there.

## A P0 the winding caught

The collar's own ring quads are **wound to match the shells and it is not
guessable**: the other order reads perfectly plausible, shades almost
normally, and leaves 164 unbalanced directed edges — a collar facing inward
against a torso facing out. W1 caught it; no screenshot would have.

## `mutverify_weld.py` — new, and it mutates the SHIPPED SOURCE

Five mutations, all confirmed to turn their clause red: rings→0 (W7), the
strict cut (W2+W3), iters→8 (W7), the rings wound inward (W1), and the pinned
arm (W2+W3). It applies each as a text substitution to `weld_geom.py` itself
and runs the gate on the result — a stand-in copied into the test file drifts
the moment the real one changes. It fails loudly if a mutation's target text
has gone.

## What is open

* **CHAD'S EYE** on the connection, still the immediate item. A before/after
  pair was taken at an identical camera in his session (the before was
  re-applied from the `.bak` and then reverted): the ring around the junction
  and the pinch are gone and the arm flows out of the shoulder.
  ⚠ The **armpit underside** still reads lumpy in that view — that is the
  forearm/torso interpenetration, NOT the weld, and it is untouched by this.
* **§5, the pair of pants**, unchanged and still the real work.
* the W7 ratchet at 4.0 mm; the honest target is 2.5.
* `restore_limb` and `_closest_on_polyline` stay unwired, both now labelled in
  their own docstrings as the records of rejected routes.
* the width question in §3, unchanged.

---

# ADDENDUM 3 — SW-1h: "THE UPPER SHOULDERS ARE CRUSHED". He was right; ADDENDUM 2 is REVERTED.

**STATE: the weld is back to the strict cut and the one-triangle bridge, and it
is BIT-IDENTICAL to the pre-SW-1g build** (verified against the `.bak`: same
vertices, same face set, same welded output to 1e-15). Applied live. Nothing
saved, nothing committed. What survives from ADDENDUM 2 is the **measurement**
and one new clause; what does not survive is the geometry.

## What he saw, and what it was

I certified the collared weld from ONE camera on the arm. He looked from above:

```
 top of the shoulder, height in mm, y -0.02      x: 196  208  220  232  244  256  268
 AUTHORED (body_geom's own shells)                  1527 1522 1515 1510 1511 1511 1511
 the weld that ships                                1527 1519 1515 1514 1513 1513 1512
 ADDENDUM 2's collared weld                         1528 1524 1521 1519 1517 1516 1514
```

The authored shoulder has a **saddle at x 0.232 and then the deltoid rise**.
The collared weld replaces it with a **monotonic ramp** — up to **+16.9 mm**
above the authored surface, against **−9.8 mm** for the weld it replaced.
★ It is not LOWER. It is HIGHER, and it still reads crushed, **because the eye
reads curvature.** (Third time this programme has learned that sentence.)

## The chain, measured end to end

1. **`inside_arm` is not the arm.** It models the upper arm as a circle of
   `UPPERARM_R(t)` about the bone; the arm body_geom lofts has a shaped
   section and a rounded cap. MEASURED over the shoulder grid, the model
   stands **8 to 27 mm PROUD of the mesh**, worst at the cap.
2. The cut deletes every quad "inside the arm", so it **deletes real,
   visible shoulder** — and a generous cut deletes twice as much.
3. Whatever bridges the hole must then **invent** a replacement. MEASURED at
   the worst probe: the surface up there is `COLLAR-NEW`, the collar's own
   rings, **16.9 mm above where either shell ever was.**

★ So SW-1g's finding was right about the fillet needing room and **wrong about
where the room comes from.** It came out of the man.

## THE CLAUSE THAT DID NOT EXIST — W8

Every one of W1–W7 watches the seam or the ARM. W7 measures radius about the
**arm's** bone axis, and the collar's vertices **are not even in the shells it
compares against**, so the torso side of the junction and the entire crown had
**nothing measuring them at all.** The collared weld was green on all seven.

**W8** casts rays down onto the shoulder and compares the first surface they
meet with body_geom's own shells: 105 probes, ratchet **11.0 mm** on the
measured state, **target 4.0**. The collared build reads +16.9 and goes red —
that is the mutation.

## MEASURED AND REJECTED (do not re-derive)

| route | result |
|---|---|
| collar tangency (`COLLAR_TANGENT`, Hermite off both surfaces) | 16.9 → 16.8 mm. The error is not how the curve LEAVES the loops, it is that the curve is somewhere the surface never was |
| asymmetric cut (torso strict, arm generous) | crown still 12.5 mm wrong, W7 red at −8.4. Moving which side the span comes from does not shrink the span |
| `snap_to_union` — walk each collar vertex onto the shells' union | correct code, wrong oracle: it snapped to the ANALYTIC union and made it +24.0 mm. This is what exposed item 1 above |
| `MESH_CUT` — ask the mesh, not the model | **FIXES the crown**: W8 −9.8 → about 3 mm, and stays there across reach 5–10 (−3.2/+2.6/+2.6/+2.6/+2.6/−3.1). Still OFF — see below |

## MESH_CUT is off, and the reason is a law, not a doubt

It keeps more arm, so the fairing eats more of it: W7 −6.8 to −9.8 mm, over
its ratchet nearly everywhere. Of 16 cells of reach × iterations, **exactly one
is green** (reach 6, iters 8, W7 −7.4 against a 7.5 ceiling) — and reach 5 is
red, 7 through 10 are red. **One green cell in sixteen is a lucky cell, not a
value.** `Solid` is built, mirror-exact and fast (0.02 s); it is the tool the
next rung needs.

★ A P0 found building it: a parity ray fired through a point lying exactly on
the grid line `y = 0` is undefined — it hits both faces sharing the edge, or
neither. MEASURED: **56 faces of 10580 answered differently on the two sides of
a mirror-exact body.** Fixed with a fixed sub-micron nudge applied to every
query, so it cannot break the symmetry it protects. It is a mutation.

## THE NEXT RUNG, and both defects now have a clause

**Cut both shells ON their intersection curve** instead of by whole quads.
That is the one change that gives the crown its shape (W8) *and* leaves the
fillet room (W7's join), and everything measured this session points at it.
Then `collar()` — which does close the join, 23.8 → 3.1 mm — can be turned back
on honestly.

Until then, **two open defects, and neither is allowed to hide the other**:

* **W7's join: +23.8 mm** — Chad's "huge inside angle like a right angle".
  Printed by the clause, not gated by it, and the text says so.
* **W8: −9.8 mm** — the crown, ratcheted so it cannot get worse unseen.

---

# ADDENDUM 4 — SW-1i: THE SHARED SEAM. Both defects reach target; two local flaws hold it back.

**STATE: what ships is unchanged and still bit-identical to the pre-SW-1g weld**
(re-verified after every edit). The rung below is BUILT, MEASURED and OFF
behind `SNAP_LOOPS = "off"`. Gate 8/8 green, `mutverify_weld.py` 8/8 bite.

## The number that matters

`SNAP_LOOPS = "stitch"` with `MESH_CUT = True`, one command away:

| | ships | stitch | target |
|---|---|---|---|
| **W7 the arm's join** (Chad's right angle) | **+23.8 mm** | **−3.9 mm** | 2.5 |
| **W8 the shoulder's crown** (Chad's crushed) | **−9.8 mm** | **+6.6 mm** | 4.0 |
| W2 the seam's worst turn | 40.1 (at bar) | **40.1 (at bar)** | ≤40.1 |
| W4 mirror | 0 | **0** | 0 |
| W1 unbalanced edges | 0 | **8** | 0 |
| W3 needles | 5.36 | **3.02** | ≥5.0 |

**Both of the defects he has named are at or near target at the same time, for
the first time.** What blocks it is two small local flaws, not the approach.

## What it does

Cut, then walk BOTH boundaries onto the curve where the two shells actually
cross (`on_intersection` — alternate projection onto one shell then the other;
it converges to a point on both). Then **give both shells the same seam**:
every boundary vertex is replaced by its point on the curve, and every boundary
EDGE is split by the other shell's points that fall inside it, by fanning the
triangle behind it. After that the shells share the seam outright — **no strip,
no correspondence, no invented surface.** The fairing is left with the one job
it should have: rounding the crease.

★ The far shoulder is **derived by mirroring, not recomputed.** Re-running the
walk on mirrored loops re-makes its own dedupe decisions and one borderline
pair goes the other way: MEASURED, 434 of 5600 vertices with no mirror twin on
a body exact to 0.0000 mm.

## The two flaws, and every repair that failed

**W1 — 8 unbalanced edges: one 11 mm triangle per shoulder**, three consecutive
seam vertices with a directed edge used twice. Four repairs measured, all worse
or no-ops — **do not re-derive them**:

* one direction round the seam for all fans → **454** unbalanced. The torso's
  rim and the arm's ring bound a HOLE and a TUBE END; they run opposite ways.
* a per-loop vote → 198–406. It needs the loops, and the walk that finds them
  is undefined at the pinch points, which is exactly where the trouble is.
* drop faces whose three corners sit adjacent on the seam → 8 **→ 24**. It
  takes real shell faces with it.
* choose the chain by arc length instead of vertex count → **no change at
  all**, which is what proves the ear is not a wrong-way chain. That diagnosis
  was mine and it was wrong.

**W3 — needles at 3.02°**, in the fans beside that same ear.

Both are local re-triangulation of one fan against its neighbour. That is the
next session and it is small.

## Also measured this rung

* **`zip_loops` is not enough alone.** Two loops on the same curve have zero
  width, so bridging them makes needles by construction (0.86°, 1.2 mm edges
  against 20 mm); merging the coincident VERTICES without matching EDGES
  leaves pinch points — 136 to 172 unbalanced. That is what sent the work to
  edge-splitting, which is what the stitch does.
* **`SNAP_LOOPS = "arm"` is dead**, at every ring count, reach, and projected
  or not: it puts the arm's boundary on the torso while the torso's rim is
  still outside the arm, so the strip between them covers ground that is
  INSIDE the arm and folds through it — 141 to 177°.
* `Solid` now answers `closest` as well as `inside`, both mirror-exact.

---

# ADDENDUM 5 — SW-1j: THE CURVE SEAM. Both defects FIXED, applied live, gate 8/8, mutations 7/7.

**LAUNCH LINE for this state:** *"`cd assets/character/sudburian_src`; `python
body_geom.py` (14 PASS), `python weld_geom.py` (8 PASS), `python
mutverify_weld.py` (7 mutations, ~45 s). Then connect to the RUNNING Blender
and `apply_weld_live.py`. Nothing is saved and nothing is committed."*

## The two numbers Chad named

| | he saw | now | ceiling |
|---|---|---|---|
| **the join** — "a huge inside angle like a right angle, too thin there" | **+23.8 mm** | **−9.3 mm** | 11.0, **gated for the first time** |
| **the crown** — "the upper shoulders are crushed" | **−9.8 / +16.9 mm** | **+4.1 mm** | 5.0 (was 11.0) |

and nothing else got worse: W2 sits at the body's own bar (40.1°, 0 edges
over), W3 is **5.25° — the shells' own worst**, so the weld introduces no
needle at all; W4 exact; W6's posed worst 80.9° → 66.4°; the shoulder's widest
point moves +0.44 mm.

## What it does — the seam is a curve, not a bridge

1. **The cut asks the MESH** (`MESH_CUT`, `Solid`): `inside_arm` stands 8–27 mm
   proud of the arm it describes, and every quad inside that error was being
   deleted as "inside the arm" while it was real shoulder.
2. **Torso strict, arm generous.** The torso keeps its crown; the arm is cut
   back until its ring is OUTSIDE the torso. ★ That asymmetry is what killed
   the fold: with a strict arm, the arm's ring sits *inside* the torso and its
   strip runs back in under the surface and meets the torso's strip as a
   **fin** — 146 to 166°.
3. **The seam is the curve where the two shells actually cross**
   (`on_intersection`: project onto one shell, then the other, repeat — it
   converges to a point on both), sampled at **the torso rim's own vertices**.
4. **Both shells bridge to it**, each across about one quad. Neither shell's
   own vertices move at all.

★ **`CURVE_SEAM_N = 0` — the rim's own points — is the whole of W3.** Uniform
resamplings at 30/46/48/56/60/64/72/76/88/92 points all leave the worst
triangle between 2.9° and 4.9° against a 5.0 floor, because both strips then
have counts to reconcile. Taking the rim's own points makes the torso's strip
1:1 with nothing to guess, and the worst triangle is the shells' own 5.25.

## Wrong turns, measured, so nobody re-derives them

* **`SNAP_LOOPS="stitch"`** (both boundaries onto the curve, edges split so the
  shells SHARE the seam) reaches the same targets and **shades as a saw-tooth**
  — seam edges from 1.7 to 48.9 mm side by side, 40° turns alternating, visible
  in the viewport while W2 passed at the bar. Kept in the file; it is the more
  general machinery and it is what taught this rung that the seam must be
  *evenly sampled*, not merely *correct*.
* ★ **The fold was NOT a winding problem.** The rim runs −6.28 about the arm's
  axis and the arm's ring +6.28, which is the obvious suspect; handing
  `bridge_dp` the seam reversed is a **measured no-op** and its mutation stays
  green. `bridge_dp` searches rotations and gets it right by itself.
* zero-area faces must be dropped **before** the fans, not after: a shell face
  with all three corners on the boundary becomes a triangle on a 1-D curve, it
  supplies the reverse of both its neighbours' boundary edges, and then neither
  looks like a boundary and the fans never fire. That was the "ear".
* `zip_loops` alone: merging coincident VERTICES without matching EDGES leaves
  pinch points, 136–172 unbalanced.

## What is still open

* **the join at −9.3 mm.** It is gated now and ratcheted, and it is still the
  largest thing wrong with the shoulder. The honest target is 2.5.
* **§5, the pair of pants** — untouched, still the real work.
* `restore_limb`, `collar()`, `PIN_RINGS`, `COLLAR_TANGENT` and
  `snap_to_union` all stay OFF, each labelled in its own docstring with the
  measurement that rejected it.

---

# ADDENDUM 6 — SW-1k: THE DELTOID, on Chad's ruling. And a P0 about the gate itself.

**LAUNCH LINE:** *"`exec(open(r'.../gate_live.py').read())` INSIDE BLENDER —
that, not the shell, is the gate that means anything; read §P0 below before you
believe any number. Then `apply_weld_live.py`. Nothing saved, nothing
committed."*

## Chad's diagnosis, confirmed by measurement

> "it's the shape of the shoulders BEFORE the weld that makes them half the
> thickness of the arm just distal to the weld — there is a significant deficit
> of material at the join. Do you agree?"

Yes, and it is worse than half on top. Distance from the **arm's own bone
axis** to the skin, on body_geom's shells with no weld involved:

```
                    at the joint (t 0)     just distal (t 0.30)
 up-and-outboard     torso  10.3 mm             arm  68.8 mm
 fore/aft            torso  47.6 mm             arm  98.5 mm
 UPPERARM_R declares       100.0 mm                  90.8 mm
```

The joint sits **12 mm under the skin**. The cause is one rung up: **SW-1
answered "the shoulders are too bulbous" by squashing the arm's rings to 42%
and dropping them 28 mm over t < 0.34** — exactly the span the weld sits in.
The ball went and took the deltoid with it. ★ So four rungs of weld work were
chasing a shape defect: the surface has to turn from a 10 mm shoulder to a
100 mm arm, and a "right angle" is the *correct* rendering of that.

## What was built, and the ruling behind it

`DELTOID_FILL` returns k_up toward 1 and gives the resulting rise straight back
as drop, so the mass goes **down and outboard** — where a deltoid is — and the
top surface stays under the acromion, which is the constraint that made SW-1
necessary. Chad was given the measured trade and ruled **"close it fully"**:

```
thickness about the arm's axis   t 0.10  0.15  0.20  0.25   0.30
  SW-1                              115   129   134   140    162 mm
  DELTOID_FILL 1.6                  145   164   175   172    164
```

The shoulder stops being 80% of the arm just distal of it and becomes the
fullest part of the upper arm — which `UPPERARM_R` has declared all along
(0.1050 at the deltoid against 0.0908 at t 0.30) and the build never delivered.
The shoulder also gets **narrower** across (0.2475 → 0.2454), so his width
ruling is untouched.

Two ceilings moved, and only because he ruled with the numbers in hand:
`CREASE_MED_MAX` 82 → 85, `CREASE_P90_MAX` 100 → 102, and
`UPPERARM_BUILT_MAX` 0.1195 → 0.1320. ★ That last one was **measuring the right
thing in the wrong place**: it is distance from the BONE LINE, and at the
shoulder that line starts at the acromion, so every millimetre of deltoid reads
as the limb "growing" while the shoulder actually got narrower.

The weld was re-tuned to the new shells (`ZIP_THRESH` 0.012, `QUAD_FLIP`,
`TAUBIN_ITERS` 10) and the crown ratchet moved 5.0 → 6.0: **+5.3 to +6.1 mm
across fairing reach 3,4,5,6,7 and every zip and iteration tried — a floor of
the fuller body, not a tuning miss.**

## ★★★ P0 — THE SHELL GATE WAS NOT CERTIFYING THE MESH CHAD SEES

`python weld_geom.py` runs whatever CPython is on PATH; Blender runs its own.
MEASURED, same files, same body, 3.11 against Blender's 3.13:

```
 body_geom   max |delta|   4.4e-16 m    one ULP -- the authored body agrees
 the WELD    max |delta|   6.6e-03 m    and a DIFFERENT FACE SET
 W6 worst posed turn       65.4 deg on 3.11, 105.2 deg on 3.13
```

**One ULP in, six and a half millimetres out.** The amplifier is the weld's
discrete decisions — `flip_slivers`' greedy flips, `unfold`'s give-up test,
`bridge_dp`'s minimal-area ties, the parity cut, the zip threshold: every one
is a comparison, and a comparison sitting 1e-16 from its boundary goes either
way. The shell gate was green while the live build read 105.2 on a clause it
happened to clear.

★ A first attempt to blame this on set-iteration order in the neighbour sums
was **wrong and is recorded as wrong**: sorting them changed nothing. So was
the hash comparison that started it — `%.4f` prints `-0.0000` for a signed
zero, so the two builds' hashes differed at every precision while the geometry
agreed to 4e-16. **Two bad instruments in a row before the right one.**

`gate_live.py` is the answer for now: it runs both gates inside Blender and
says which interpreter it is. **Reducing the amplification is its own rung and
it is not done** — the honest fix is to make each of those comparisons decide
on a quantity that is not within noise of its own threshold.

## Open

* the amplification above — the real remaining work on this file.
* the join at −10.5 mm (ceiling 11.0), still Chad's right angle, still the
  biggest thing wrong with the shoulder.
* W6 reads 105.2 at full swing in the interpreter that matters, against 66.4
  before the deltoid. It passes like-for-like, and it is worse. Say so.
* §5, the pair of pants.
