# HANDOFF — SW-2 IS BUILT AND SIGNED. THE OPEN RUNG IS THE SCARF'S RIG.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then §0
and §1 of this file. SW-2 is DONE and Chad has signed it — do not re-open the
shoulder. §5 is the ask, and it is a RIG question, not a geometry one. The gate
to believe is `gate_live.py` run INSIDE BLENDER, and **every coverage number
must be measured in the POSE, not in bind** — see §6, trap 1, which is the
mistake that cost this session its last hour."*

> **STATE, end of 2026-08-24. NOTHING IS SAVED AND NOTHING IS COMMITTED.**
> The `.blend` was clean when this session started and is clean now — closing
> Blender without saving reverts every live change. The `.py` edits ARE on disk
> in `assets/character/sudburian_src/`, unstaged, with backups (§2).
> The live `sudburian_proxy` currently carries: the SW-2 body, the 8/8 weld,
> and the refitted coat.

`docs/SESSION_HANDOFF_20260824_shoulder_shape.md` is what this session launched
from and its §3 measurements still stand. This file supersedes its §2 ask.

---

## 0. WHAT CHAD SAID, VERBATIM, IN ORDER

1. *"fill in the shoulder valleys, and the collar bones are too low and
   protruding too far out. first collar bones then fill in shoulder gaps"* —
   asked which way "out" meant, he answered **BOTH** (forward AND outboard).
2. *"to me it looks like the best weld for the shape of the arms reveals itself
   if I were to imagine flipping them upside down on their own and then setting
   them each on the opposite side of the torso. But currently they look like
   they are welded upside down and on the wrong side ... maybe the previous
   changes were made to that end but fixed inverted."*
3. *"yep arms definately look upside down, remember dont move the hands"*
4. **"I rule yes it is correct"** — on the built arm shape.
5. Asked bone-or-hollows: **"two hollows"**. Then **"looks great"**.
6. *"can we make it better? ? or can we throw a garment overtop? it does look
   good from the back but a little smoothing patching ... then ok"*
7. **"refit"**, then **"3"** (scarf owns the inner band, coat collar the outer).
8. *"the scarf band ring needs to get visibly taller to cover the neck, it can
   double or 2.5x in height no need to fatten it, but it does have to stop
   being sunkent into to coat"*
9. *"the real problem is its vertical height (the ring), NEEDS TO COVER MORE
   NECK but the actaul problem of sinking CONTINUES TO THE tails at the back
   sinking into the coat / back"*

★ **He was right every time, including twice about defects I had introduced.**
Do not open by arguing with him.

---

## 1. WHAT IS BUILT, SIGNED AND GREEN — DO NOT RE-OPEN

**Body gate 14/14. Weld gate 8/8 in Blender's own 3.13.**

| dial | was | now | why |
|---|---|---|---|
| `CLAV_T` | 0.60 | **0.880** | collar line to +16.9 mm over the acromion |
| `CLAV_H` | 0.0050 | **0.0000** | ruling "two hollows" — no bone, no ridge |
| `CLAV_LOBE` | 0.62 | **0.45** | ridges pulled in off the arms |
| `FOSSA_D` | 0.0050 | **0.0075** | the hollow now IS the collar line |
| `FOSSA_DT` / `FOSSA_SIG` | — | **0.120 / 0.120** | NEW; fossa tied ABOVE `CLAV_T` |
| `DELTOID_LIFT` | 0.0 | **0.25** | Chad's "flip", at 25% strength |
| `DELTOID_DROP_COMP` | 1.0 | **0.75** | with the above, drop scales to 0.75 |
| `SHOULDER_BEAD` | 0.0360 | **0.0420** | re-swept against the moved arm |
| `BEAD_W` | 0.1450 | **0.1150** | ditto; keeps the bead off the neck |
| `TAUBIN_REACH`/`ITERS` | 5 / 16 | **6 / 4** | re-swept against the moved arm |
| `TORSO_FAIR_ITERS` | — | **6** | NEW; see §4 |
| `HOODIE_ROWS` hx & hyf | table-fitted | **body-fitted** | the coat refit |

`DELTOID_FILL` stays at **1.6** — Chad's ruling, and it was never the fault.

**Results:** the arm's centreline is monotone (it used to travel UPWARD +6.6 mm
over t 0.17–0.34); the arm's top went 1.5141 → 1.5259, from 10.4 mm *under* the
torso's skin to 1.6 mm under it and 13 mm above the acromion — dead centre of
the +10..+16 mm band three independent sources measured; the join crease went
86.0/102.1 → 77.4/98.4; the weld's join went −12.5 → −10.0 mm; mirror symmetry
exact; **shoulder, elbow and wrist are untouched constants, so nothing reached
the hands.**

---

## 2. BACKUPS, AND HOW TO PUT IT BACK

```
body_geom_preFLIP_20260824.py.bak       before the arms + collar
body_geom_preHOLLOWS_20260824.py.bak    before "two hollows"
weld_geom_preSW2_20260824.py.bak        before the weld re-sweep
costume_geom_preREFIT_20260824.py.bak   before the coat refit
costume_geom_preCOLLAR_20260824.py.bak  refit present, collar absent  <- CURRENT
costume_geom_COLLARTALL_20260824.py.bak the tall collar experiment (REVERTED, see §5)
```

To apply into the live GUI session, in this order, each of which **refuses to
run headless and never saves**:

```
exec(open(r"...\gate_live.py").read())          # believe THIS, not the shell
exec(open(r"...\apply_weld_live.py").read())    # body + weld
exec(open(r"...\apply_costume_live.py").read()) # coat, pants, boots
```

`apply_body_live.py` and `apply_weld_live.py` both REFUSE while any body ruling
is red. That is correct and must not be worked around.

---

## 3. THE CONSULT ARTEFACTS — READ BEFORE RE-DERIVING ANYTHING

`assets/character/sudburian_src/ref_shoulder/` holds `CONSULT_PACKET.md`,
`SPEC_F/G/H/J.md`, `SOURCES_*.md`, `REDTEAM.md` and ~37 cited images from five
context-free lanes and two red teams. **Findings that are measured and NOT yet
built:**

* the shoulder has a real **SHELF** at 73–93% of the neck→acromion run (lane G
  right, lane H refuted by the red team on four instruments);
* the **jugular-notch landmark is ~20 mm wrong** — the model has it +5.1 mm
  above the acromion, every source that measures a body says 10–20 mm BELOW;
* **"a snowsuit erases the clavicle" is REFUTED** — piping is standard on 1970s
  snowmobile suits and piping over padding makes a positive welt;
* lane G's millimetre columns are all ~12% too large (multiply by 0.878).

---

## 4. OPEN DEBTS THIS RUNG DID NOT PAY

* ★★★ **`L10` — "no sharp angle changes on the body", ceiling 15° — ITERATES
  OVER THE TORSO AND NOTHING ELSE.** One `if nm == "torso"` at
  `body_geom.py:3068`. Extended honestly it reads **upper arm 53.8°**. The arm
  has never been under the clause that would have failed it.
* **`TORSO_FAIR_ITERS` is only wired into the `snap_loops == "curve"` path** —
  the one that ships. The `"stitch"` branch does not have it.
* **`costume_geom`'s "MESH: no bare body, anywhere" was ALREADY RED before this
  session** (6565 probes) and is now 3194. It is a *containment* test, and
  **containment cannot certify a scarf**, which is a tube: the neck sits in the
  tube's hole, so 946 probes read bare while fully wrapped. Do not "fix" the
  clause by teaching it about the scarf — 1153 stay bare either way. See §6.
* The coat's hyb was **held, never shrunk** — the solve said it could come in a
  few mm and Chad said the back reads good.

---

## 5. ★★★ THE ASK: THE SCARF'S RIG

**Chad's two complaints (§0 items 8 and 9) are ONE problem and it is not
geometry.**

MEASURED, live, this session:

```
  scarf verts inside the coat, BIND    :   0 of 908
  scarf verts inside the coat, POSED   : 438 of 908, worst 121.3 mm deep
  body verts left uncovered, BIND      : 165
  body verts left uncovered, POSED     : 567  (532 spine_03, 29 neck_01)
```

```
  564 of 908 scarf verts are weighted to  neck_01
  neck_01 in the ride pose ................ +36.0 deg   (the chin tuck)
  spine_03, which the coat's torso follows   -7.4 deg
```

The scarf swings 36° forward and down into a coat that stayed put. **That is
the sinking, at the ring AND at the tails, and no reshaping of the ring touches
it.**

Tested READ-ONLY (no weights were modified): re-weighting `neck_01` →
`spine_03` gives **438 → 279** buried, and leaves the worst at **121.3 mm** —
the deepest are FRONT tail verts at z 1.22–1.26 swung inward to y +0.05 on the
`scarf_01..06` bones. **So the re-weight is necessary and NOT sufficient.**

### ★ THE HEIGHT CANNOT BE BOUGHT UPWARD

```
  the wrap runs        z 1.5167 .. 1.6038   (87.0 mm tall, 34.2 mm thick)
  CHIN_Z 1.6087 - CHIN_CLEAR 0.006 =  1.6027
  -> the wrap is ALREADY 1.1 mm past its own ceiling
  2.0x would need the top at 1.6908   (88.1 mm past the chin)
  2.5x would need the top at 1.7343  (131.6 mm past it)
```

So 2–2.5× must come **downward**, from 1.5167 to 1.4297 (2×) or 1.3862 (2.5×).
And downward it must **also flare outward**, or it buries itself in the coat
lower down — the same complaint one level further on. At z 1.43 the coat's own
surface is at r 0.191 (front) to 0.261 (side), while the wrap is at r
0.106..0.140. **That shape is a COWL, not a taller ring.** Chad said "no need
to fatten it" — a cowl flares, it does not thicken, so that ruling is intact.

### THE QUESTION THAT MUST BE PUT TO CHAD FIRST

`[[costume-rung-t2]]` already flagged this and it is still unanswered:
**re-weighting the scarf off `neck_01` is unsafe as an ASSET-ONLY change,
because the runtime recomputes the scarf's anchor off the neck.** Either
re-weight and book the runtime work, or fix the runtime anchor first. **A taller
ring built before this is settled will simply sink 36° further forward with
everything else.**

---

## 6. TRAPS THIS SESSION HIT — ALL OF THEM MINE

1. ★★★ **I MEASURED COVERAGE IN BIND ALL SESSION WHILE CHAD LOOKED AT THE
   POSE.** 165 uncovered in bind, 567 posed; 0 scarf verts buried in bind, 438
   posed. Every number I quoted before §5 was a true report of a question
   nobody asked — this file's signature failure, made while quoting it.
2. **A table whose `UP` and `OUTBD` columns were the same axis resolved twice.**
   `OUTBD ≡ INBD` was a tautology and I reported it as a finding. The red team
   refuted it and found the real mechanism (the centreline hump).
3. **A dial solved in a frame another dial in the same commit was moving.** I
   solved `CLAV_T` at the ridge's OLD azimuth while `CLAV_LOBE` moved it
   inboard onto a lower part of the yoke — the collar came out LOWER on a
   ruling that said too low.
4. **A prototype that omitted `girdle_z`'s own azimuth weighting**, overstating
   every fossa depth by 11% — exactly enough to flip the feature on and off.
5. **I buried Chad's scarf with my own collar** (coat top 1.5710 → 1.6036,
   above the scarf's 1.6027) and he caught it. Reverted.
6. **Solving a whole row for containment** to fit the coat — `limb()` already
   names that error. It wanted hx 300 mm, 20% wider than the body against the
   ~8% real outerwear measures. Holding hx and solving only hyf gives 5.5%.
7. **A hypothesis I tested and refuted rather than shipping**: that `taubin`'s
   graph walk truncating at `reach` (weight 0.074, and the sigma scales WITH
   reach so the boundary weight is 0.074 at EVERY reach) caused W2. Identical
   numbers at halo reach×2 and ×3 — the arm side is pinned out of that pass.

### AND TWO THE FILE HAD

* **`L2d`'s 2026-08-23 re-aim was written into the comment and never into the
  code** — it went on testing the acromion BONE for a whole rung while its own
  comment said skin. Now genuinely re-aimed at the torso's own skin at/outboard
  of the acromion's x, and **mutation-verified**: passes at −1.6 mm, FAILS
  `SHOULDER_DROP=0` (+18.4), `DELTOID_LIFT` 1.0 (+18.4) and 0.60 (+7.7).
* **The fossa was hardcoded at t 0.66** from when `CLAV_T` was hardcoded at
  0.30. Raising `CLAV_T` to 0.880 put the hollow-above-the-clavicle 0.220
  BELOW it. All three front features are now offsets FROM `CLAV_T`, so the
  inversion is unrepresentable.

---

## 7. THE ONE LESSON WORTH CARRYING

★★★ **ANY DIAL MEASURED OFF A SURFACE MUST BE RE-SWEPT WHEN THAT SURFACE
MOVES.** `DELTOID_LIFT` raised the arm 12 mm and broke, in order:
`SHOULDER_BEAD`, `BEAD_W`, `TAUBIN_REACH/ITERS`, and the entire coat — which
had been fitted to contain `body.TORSO_ROWS`, the body's *radius table*, while
the built torso stood 36.8 mm outside it because the shoulder bead is a FIELD
and a field has no outline.

★★ **And smoothing the BODY tightened the WELD's bar**: `W2`'s ceiling is the
body's own worst turn, and Chad's "two hollows" ruling dropped it 43.1 → 33.7°.
Two of the weld's clauses went red because the body got *better*.
