# Milestone — The camera was the bug, not the mouse

*A learning doc, in the `docs/TEACHING.md` annotated style, about the single most
expensive class of error in the feel systems: **mis-attributing a feel symptom to
the wrong subsystem.** Written 2026-07-07 after the S7-cam3 fix landed. Callouts:
`💡 INSIGHT` · `⚠ TRAP` · `🔬 HOW WE KNOW` · `🧭 FOR AGENTS`.*

---

## 1. The class of error

Three subsystems cooperate to make "mouse-aim dogfight feel":

| Subsystem | Owns (the feel it is responsible for) | Rule |
|---|---|---|
| **Raw mouse → aim frame** (`input/aim_frame.h`) | WHERE you point — screen-space intent, "up is up" | **Never** smoothed/eased/re-referenced (RA9 / SPEC §9.1). |
| **Chase camera** (`render/camera.cpp`, `app/main.cpp`) | WHAT you see — horizon, reticle recentering, framing, readability | **May** smooth/lag/ease — it is downstream of the aim. |
| **Instructor cascade** (`control/controller.cpp`) | HOW the plane gets there — coordinated turn, authority, envelope | Coefficients/gains only; plant-inverts the sim (SPEC §9). |

`⚠ TRAP` — **A feel symptom names a PERCEPTION, not a subsystem.** "The mouse
inverts after a split-S" is a statement about a *relationship you see on screen*, and
that relationship emerges from **two** subsystems (the mouse's rotation basis and the
camera's screen basis) plus a third that flies the plane. Reading the symptom
literally — *"the mouse feels wrong → change the mouse"* — is how you spend a whole
session patching the wrong subsystem.

---

## 2. The exemplar (two sessions, one bug)

**Symptom:** flying a loop / split-S, the mouse "goes all opposite" — up/down and
left/right feel flipped until you roll back upright; the camera flips at the top.

**Wrong attribution (prior session): the MOUSE.** Four fixes, each easing or
re-referencing the mouse basis, each violating the raw-mouse rule, each flown and
reverted:
1. **F1 screen-relative** — rotate the aim about the rendered camera basis. Stalled
   at the zenith (the horizon-locked camera can't define screen-right up the radial)
   and is a smoothed basis feeding mouse→aim (RA9 ban).
2. **`level_up_to`** — re-level the mouse frame's up to the world horizon. Flipped
   the pitch axis at the zenith.
3. **S7-mouselevel `roll_toward_local_up`** — ease the mouse frame's up toward
   local_up. Three gate revisions, each a new failure; it *curls an active sweep*.
4. **Pole-free hybrid** — switch the mouse basis near the zenith. Jumped at the
   switch, broke the split-S.

Every one moved the symptom around while breaking Chad's inviolable rule ("the mouse
is raw, no force acts on it"). That is the signature of mis-attribution.

**Right attribution (this session): the CAMERA.** The shipped mouse
(`apply_mouse`, rotating the aim about the frame's *own* carried axes) was **already
pure raw** and never inverts *in its own frame*. What inverted was the **camera**:
S7-cam2 made camera-up **horizon-locked** (`ease_level_up`→`local_up`) while the mouse
frame is **carried**. Through a loop the two frames rotate apart, so past the top
"mouse-up" no longer maps to "up on the screen you see." Fix (S7-cam3): camera-up =
the carried aim-frame up (`cam_up = loop.aim.up()`). One change. **The mouse is
byte-for-byte untouched.**

`💡 INSIGHT` — The pilot's own words were the correct diagnosis, heard right:
*"the mouse is not raw input, it's getting influenced everywhere."* The mouse *was*
raw — but the horizon-locked camera made it **look** influenced. Making the camera
show the world from the aim frame reveals the raw mouse as it always was. The observer
cannot tell which frame moved; only the *relationship* is visible.

---

## 3. The diagnostic that finds it in one step

Ask not *"is the mouse raw?"* (it was) but: **"do the two frames that must agree
actually share a basis, and does the fix belong in the one I'm allowed to change?"**

- "Mouse-up == screen-up" is a **relationship** between the mouse's rotation basis
  and the camera's screen basis. If they are *different frames*, the relationship
  breaks exactly when the frames diverge — i.e. after a maneuver. Level flight hides
  it (the frames coincide); a loop exposes it.
- **The constraint tells you where the fix must go.** RA9 forbids touching the
  mouse→aim path but *explicitly allows* smoothing/moving the camera. So a
  "mouse-vs-screen" divergence can only legitimately be fixed on the **camera** side.
  When one of two disagreeing subsystems is frozen by a rule, the fix is in the other
  one — full stop.

`🔬 HOW WE KNOW` — The executable pin
(`test_instructor_tick.cpp` "mouse-up stays screen-up at any attitude"): project a
mouse-up nudge of the aim through the camera basis at every roll of the carried frame.
With camera-up = aim.up() the reticle moves **UP by a constant +0.0173 at every
attitude**; with the horizon-lock choice (camera-up = local_up) the same nudge decays
through 0 at 90° and **flips to −0.0173 past inverted** — the bug, made a number.

---

## 4. Why it generalizes (the same family, already in the tree)

This is not a one-off; it is the feel-systems form of lessons the repo already teaches:

- **"A flat instrument certifies a flat controller"** (TEACHING §4.2). The instrument
  and the bug share a frame, so the bug is invisible. Here the *pilot's eye* is the
  instrument: a horizon-locked camera + a carried mouse share no frame, so the mouse
  "reads" wrong. Same class, moved from the harness to the screen.
- **"The bases must SEPARATE"** (TEACHING §3d / §4.3). You cannot see a frame bug
  until the frames diverge. Every level-flight test passed because body_up == local_up
  == aim.up() there; only a rolled/inverted attitude splits them. The mutation-verify
  (roll, not pitch) is what exposes it — and the loop is what exposes it in the hand.
- **"Route the live path through the tested code"** (TEACHING §4.7). The mouse was
  provably raw *in code*; the failure was in an *untested main.cpp camera line*
  (`cam_up = …`). Feel bugs love the untested glue seam.

---

## 5. The checklist (pin this above the stick)

Before touching a feel system because "it feels wrong":

1. **Name the perception, then attribute it.** Which *relationship* is off (mouse↔screen,
   pilot↔plane, aim↔nose)? Which subsystems form it?
2. **Prove the accused is actually guilty.** Is the named subsystem provably correct
   in isolation (the mouse was raw)? If yes, the bug is in a *partner* subsystem or
   the *relationship*, not the accused.
3. **Let the rules pick the fix site.** If a rule freezes one partner (raw mouse), the
   fix is in the other (camera). Don't fight the rule to fix the wrong subsystem.
4. **Watch for the stacked-patch smell.** If fix N re-breaks what fix N−1 fixed, or
   each fix needs a new gate to survive, STOP — you are patching a symptom on the wrong
   layer. Re-attribute from scratch (a fresh context helps).
5. **Instrument the relationship, not the symptom.** Pin the thing you can see
   (project mouse-up through the camera; check the sign), and mutation-verify it with
   the *contrast* (the wrong choice must fail), not just a happy pass.
6. **One dial, plan-mode first, the pilot flies it.** Mis-attribution is cheap to
   start and expensive to unwind; the discipline exists to make you attribute *before*
   you code. The plan-mode question that cracked this one — *"what should the horizon
   do through a loop?"* — is what surfaced that the **camera**, not the mouse, was the
   degree of freedom in play.

`🧭 FOR AGENTS` — The fastest path through a feel bug is almost never the first
subsystem the symptom names. Spend the first move on attribution. The cheapest tool is
a sentence: *"the mouse feels X because the camera does Y"* — if you can't fill in Y,
you don't yet know which subsystem to touch.
