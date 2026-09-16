# HANDOFF — THE SCARF RIG, THE TAIL AND THE COAT ARE SIGNED AND PUSHED.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then §0
and §1 of this file. The scarf rung is CLOSED — Chad signed it "perfect" and it
is on main and pushed. Do not re-open it. §5 is the next ask. The instrument to
believe is **`surf_clr`**, printed by the game under `SEADS_SCARF_DEBUG=1`;
`back_clr` is the number that hid this defect for five rounds, and §6 trap 1 is
why."*

> **STATE, end of 2026-08-24. EVERYTHING IS SAVED, MERGED AND PUSHED.**
> `main` = `7bae38718` = `origin/main`, 0 ahead / 0 behind, working tree clean.
> Branch `sandbox/sw2-scarf` = `62f103df7`, also pushed.
> `indy650.blend` is SAVED. `assets/sled/indy650.glb` is RE-EXPORTED.
> `build-play/seads.exe` is rebuilt and carries it.
> Gate on the merged tree: **1538 tests, 5 reds, ALL PRE-EXISTING** (see §2).

`docs/SESSION_HANDOFF_20260824_scarf_rig.md` is what this session launched from.
This file supersedes its §5 ask, which is now answered and shipped.

---

## 0. WHAT CHAD SAID, VERBATIM, IN ORDER

1. *"save and commit the work as it stands, then scarf fix"*
2. *"okay no, its still sinking into the coat, this is a problem I have asked to
   get fixed maybe 5x now and still nothing"*
3. *"Your wrong here, also about the scarf height, there is lots of room to go up
   that is why I want it to be taller (the loop around the neck). Also THe scarf
   sits in the back /coat in the blender viewport and in the game the scarf sinks
   down. Be careful correcting the scarf its important animation in the game but
   needs to rest on top oof the sweater / coat"*
4. *"scarf is all jagged it cant be like that"*
5. *"it looks good now lets move on to the scarf resting on the coat not in the
   coat"* — and, on the tails: **"The tails are the priority"**
6. *"why is it still sinking into the sudburian;s back in the blender viewport?
   ... im judging how something will look in game from the viewport, it should
   reflect what it will look like in game so I dont consider it fixed until the
   viewport is also fixed"*
7. *"no rotation won t fix it I can see that now, and I didnt ask for a rotation.
   I want you to reposition the resting state on top of the coat so that the
   fabric does not fall into the coat, that simple"*
8. *"Also dont break the animation mechanism that is very important"*
9. *"yes save the .blend it looks good, the last bone or two come off the coat
   ... there is a strange artifact under the end of the scarf that looks like a
   resting place (depression) shaped like the end of the scarf. The coat should
   [not] have that unnatural looking depression nor the ridge ... it should
   follow a smooth contour of the back of the coat"*
10. *"can we measure a little more accurately, it lifting off a bit halfway
    down"*
11. **"perfect save the .blend and the game runtime and commit this to main"**

★ **He was right every single time, including the three times he overruled a
measurement I had just presented.** Do not open by arguing with him.

---

## 1. WHAT IS BUILT, SIGNED AND PUSHED — DO NOT RE-OPEN

| thing | where | state |
|---|---|---|
| SW-2 the shoulder | `body_geom.py`, `weld_geom.py` | body 14/14, weld 8/8 |
| the ring (neck loop) | `scarf_geom.loop_z_hi` | 87.0 mm smooth, "it looks good now" |
| the wrap's rig | `reweight_scarf_live.py` | `neck_01` 0.40 / `spine_03` 0.60, GRADED |
| the tail's path | `scarf_geom.TAIL_PATH` | uniform 20 mm perpendicular, "perfect" |
| the coat's tail cap | `costume_geom` drape | C1 soft-min + faded z window |
| the back-plane banding | `render/sled_model.cpp` | equal-width t bands |
| the anchor pin | `render/trail_chain.cpp` | floored at `back_keepout_min_m` |
| the GLB | `assets/sled/indy650.glb` | re-spliced, 59 microns, verified |

**MEASURED RESULTS, all reproducible:**

```
in the GAME     surf_clr        -0.086  ->  +0.022 m
in BLENDER      scarf verts inside the coat   392/908 -> 113/908
                tail worst depth              -167.2  ->  -14.9 mm
                chain gap to the coat   4/0/32/32/31/29  ->  4/20/20/20/20/20/20 mm
                ribbon FACE off the cloth                   +5 / +6 mm
```

---

## 2. HOW TO GET BACK TO IT

```
main 7bae38718 == origin/main        sandbox/sw2-scarf 62f103df7
```

**To rebuild the character in a live Blender session, in this order:**

```
exec(open(r"...\reaim_scarf_bones_live.py").read())   # rest bones from TAIL_PATH
exec(open(r"...\apply_costume_live.py").read())       # coat, pants, boots
exec(open(r"...\apply_scarf_live.py").read())         # ring + tails
exec(open(r"...\reweight_scarf_live.py").read())      # the graded wrap blend
```

**To re-export to the game:**

```
REPO = r"D:\flight_sim2\seads-recon"                  # ★ or it exports to a SANDBOX
exec(compile(open(REPO + r"\assets\character\sudburian_src\export_live_v13.py").read(), ..., "exec"))
python assets/character/sudburian_src/splice_sudburian.py scratch/sudburian_seated.glb --ref scratch/sudburian_ref.json
cmake --build build-play --target seads                # ★ --target seads, NOT the default
```

**GATE:** `1538 tests, 5 reds, all pre-existing and A/B-proven** — GI4 sled debts
799/800/838/841 and probe P-F 54. Prove it the same way if you doubt it: revert
your change, rebuild, re-run those five, compare the set. Do not assert it.

**Snapshots kept** in `blend_snapshots/`: `indy650_preSW2`, `preSCARF`,
`preTAILAIM`, `preCONTOUR` (.blend) and `indy650_glb_pre_reexport`,
`glb_preCONTOUR` (.glb).

---

## 3. THE INSTRUMENTS — AND WHICH ONE LIES

★★★ **GRADE THE SCARF ON `surf_clr`, NEVER ON `back_clr`.**

```
SEADS_SLED_DEBUG_MODE=1 SEADS_SCARF_DEBUG=1 .\build-play\seads.exe
```

* `back_clr` — the chain against the SINGLE chord plane through the neck. It is
  not what constrains the chain and not where the coat is. **It read +0.095,
  +0.115, +0.122 through five rounds while the fabric sat 86 mm inside the coat.**
* `surf_clr` — the chain against the BANDED SURFACE planes, in each point's own
  band. Negative = fabric inside the coat. This one can fail.
* `plane->surface push mm` — how far each band's plane had to move to reach the
  cloth. If these are large again, the centroid planes have drifted inboard.
* `drawn-back capture -- N verts in M rings` — **if M is 1, the banded planes are
  OFF** and everything downstream is the broken single-plane fallback.

In Blender, the honest measure is a **BVH signed distance in the PROXY'S OBJECT
SPACE** (§6 trap 3). `--smoke` flies the AEROPLANE; the sled needs
`SEADS_SLED_DEBUG_MODE=1`.

---

## 4. OPEN DEBTS

* **The coat's hem-roll tangency.** The side wall pulls in at slope 0.356 and
  `HEM_ROLL` leaves it at 0.044 — a real convex corner, same class as Chad's
  earlier "the sharp undercut of the sweater at the base". **It was never
  confirmed to be the ridge he saw**, and the faired back profile is already
  smooth (max curvature 1.7 against the body's own roundness of 4.3), so do not
  re-fair `hyb` — that is not where it is.
* **The ring is parked at 87 mm.** Chad asked for 2–2.5×. It cannot go up: the
  helmet's own occipital rim already rests on it across the whole back half
  (0–5 mm), and only the front has room. `LOOP_TOP_LIFT` exists and is solved by
  `solve_loop_lift_live.py` against HIS baseline (the 24-vert overlap he already
  accepted) — it currently solves to 0.
* **`L10` iterates over the torso and nothing else** (`body_geom.py:3068`).
  Extended honestly the upper arm reads 53.8° against a 15° ceiling.
* **`TAIL_PATH` is a dial measured off a surface.** ★ RE-SOLVE IT WHENEVER THE
  COAT MOVES. That is this repo's own standing lesson and it is why the tail had
  to be re-solved twice in one session.
* **NOT FLOWN.** Chad has seen it in the viewport and in stills. He has not
  ridden it.

---

## 5. THE NEXT ASK

Chad's own priority order has been: the ring (closed), the tails (closed), the
coat's ridge/depression (the depression is closed; **the ridge is unconfirmed**).
So the first question to put to him is a 5-second one:

> *"Is the ridge still there, and if so roughly how far up the back — level with
> the belt, with the tail's end, or just above the hem?"*

Then either fix the hem-roll tangency (§4) or go where he points. **Do not guess
at it** — the faired profile is already smooth, so a wrong guess re-fairs
something that is not the defect, and that is a sixth miss on a rung he has
already had to ask about six times.

---

## 6. TRAPS THIS SESSION HIT — ALL OF THEM MINE

1. ★★★ **FIVE ROUNDS WERE GRADED ON A NUMBER THAT COULD NOT SEE THE DEFECT.**
   `back_clr` stayed positive the entire time. The fix was to build the
   instrument FIRST (`surf_clr`) and only then touch the geometry. **A report is
   only as wide as its questions, and Chad's eye was wider than my instrument
   five times running.**
2. ★★★ **THE DEFECT WAS NOT WHERE THE MECHANISM WAS.** The tail was authored off
   `BACK_Y −0.155`, the BARE SUIT's back — with a bulky coat over it, the whole
   tail sat in the gap between the man and his own jacket. It was never sinking
   THROUGH a surface; it was never outside one. No amount of solver work could
   have fixed that.
3. ★★★ **A FRAME BUG THAT PRODUCED A CONFIDENT FALSE FINDING.**
   `sudburian_proxy.matrix_world` is NOT identity (180° Z flip + translation) and
   is not the rig's. A BVH from the evaluated mesh is in OBJECT space; bone
   positions are WORLD. Mixing them gave "the root floats 145 mm off the coat" —
   the truth is 4.1 mm and it always sat on the coat. **Two measurements of the
   same thing that disagree mean one is lying; I saw that signal twice before
   chasing it.**
4. **A FIXED-AXIS GAP IS NOT A DISTANCE TO A TILTED SURFACE.** The solver held
   "33 mm" along a fixed backward axis while the true perpendicular ran 0 → 32 mm
   down the chain. That IS Chad's "lifting off a bit halfway down". Solve on the
   offset surface (nearest point + normal), not along an axis.
5. **A SWEEP WHOSE KNOB DOES NOTHING IS NOT A SWEEP.** CLEAR 0.030 / 0.038 /
   0.046 all returned exactly 185 buried, because a "pull in until it touches"
   step cancelled the clearance. Three identical results are a bug report.
6. **A HARD `continue` IN A DISPLACEMENT LOOP IS A DISCONTINUITY — THE THIRD
   TIME.** It carved a flat scarf-shaped panel with a ridged rim into the coat.
   The lesson was already written in this rung's own files, twice.
7. **A FLAT BLEND TORE THE FABRIC IN TWO.** The wrap reweight swept the bridges
   onto `spine_03` while the tail pods kept `neck_01`'s full tuck — a 61 mm gap,
   and every clause in the file was green through it. It is a clause now.
8. **`align_roll` TAKES THE BONE'S Z, NOT ITS X.** Passing the negated Z flipped
   every bone's width axis by 180°, which is one of the four things the runtime
   reads.
9. **I RAN TWO CTEST GATES AGAINST ONE BUILD DIR AT ONCE.** CLAUDE.md forbids it
   explicitly. Both results discarded.
10. **I FROZE CHAD'S BLENDER** by building a containment shell INSIDE a
    per-vertex loop — 564 helmet shells per pose. Hoisted, the same measurement
    runs in 0.8 s. Same O(n×m) class as the 2026-08-22 freeze.
11. **A 214 MB PUSH.** GitHub silently drops an oversized POST (a known trap in
    `[[audio-contributor-lane]]`). Pushed in six chunks, **verifying `ls-remote`
    after each** rather than trusting the exit code.

---

## 7. THE ONE LESSON WORTH CARRYING

★★★ **BUILD THE INSTRUMENT THAT CAN FAIL, BEFORE TOUCHING THE GEOMETRY.**

Every wasted round on this rung — all five of them, plus the two inside this
session — was a true report of a question nobody asked: `back_clr` measuring the
wrong plane, a fixed axis measuring the wrong direction, a world point measured
against an object-space mesh, a sweep whose knob was cancelled downstream. The
geometry work was never the hard part. **The hard part was noticing that the
number saying "it's fixed" could not have said anything else.**

★★ And the corollary Chad enforced himself: **the viewport must reflect the game,
because that is what he judges from.** A fix that is only true in the runtime is
not yet a fix.
