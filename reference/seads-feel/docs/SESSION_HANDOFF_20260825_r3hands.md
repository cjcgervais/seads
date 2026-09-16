# HANDOFF — R3-HANDS IS SIGNED. THE LEVERS MOVE. R4a IS PLANNED AND RED-TEAMED.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then §0
and §1 of this file. R3-HANDS is CLOSED — Chad signed it "great job perfect".
Do not re-open the finger squeeze: §3 proves it is unreachable from the runtime
and it is a MESH question about hands he already signed. §6 is the next ask, and
`docs/SUDBURIAN_LADDER.md` §7 is its spec. The instrument to believe is the
**differential** printed by `SEADS_SLED_CTL_DEBUG=1` — an absolute joint
position cannot see this rung's defects and §5 is why."*

> **STATE, end of 2026-08-25.**
> Branch `sandbox/r3-hands`, off `main` `4be931ce7`.
> Gate: see §2. `build-play/seads.exe` carries everything below.
> `indy650.blend` and `assets/sled/indy650.glb` were **NOT touched** this rung —
> it is runtime-only, which is the whole reason it was safe to iterate fast.

---

## 0. WHAT CHAD SAID, VERBATIM, IN ORDER

1. *"lets start at the best logical place for developing the sudburian further"*
2. *"Ive driven it just now, it checks out and looks good."* — ★ this is the
   drive verdict on `main`, and it **closes the coat-ridge question** that
   `docs/SESSION_HANDOFF_20260824_scarf_coat.md` §5 left open after six asks.
3. *"the thumb should be animated for the throttle with the right elbow going
   slightly down on thumb throttle push, and for the left side the braking
   animation currently the elbow goes down and it looks like it twists the
   handle backwards, when really the braking animation should be the fingers in
   front of the red brake lever should squeeze in and the left elbow should
   articulate up a bit"*
4. *"thumb for throttle looks good, but on the braking side finders are lifting
   up with the elbow, actually fingers shoud be squeezinf in, (swinging their
   tips aft towards the rider themselves the fulcrums being the bases of the
   fingers themselves...) currently they are lifting up to the sky"*
5. *"you went the wrong way in, you went medial ... Squeezing action.. got it?"*
6. *"no there is too much displacement now! ... they are not supposed to detach
   from the hand in an offset downward displacement"*
7. *"there is no way you can't just pivot the fingers themselves without the
   fingers detaching and going down, you know what looked really close was when
   you went to the pelvis ... Am I right?"* — ★ **he was right.**
8. *"throttle is not red"* — ★ also right; see §4.
9. *"fingers are not moving just revert any work and go back to the pelvis
   checkpoint"*
10. ★★★ *"hey great job perfect. That was actually a pretty good one so I thought
    we were close, but This still looks good! Thanks for the fast iterations"*
11. *"lets reinstall their motion, then handoff written and then commit and push"*

★ **He was right every time he overruled a measurement I had just presented.**
Four finger builds were rejected and reverted; the one he pointed back to is the
one that shipped.

---

## 1. WHAT IS BUILT AND SIGNED — DO NOT RE-OPEN

| thing | where | state |
|---|---|---|
| thumb on the throttle | `thumb_01_r`, driven by `throttle` | SIGNED |
| brake elbow RISES | the bar-roll deleted on that side only | SIGNED |
| the "handle twist" | gone — brake wrist roll removed | SIGNED |
| brake fingers | `mittfront_01_l`, the pelvis-aimed curl | SIGNED |
| **the brake lever** | `indy_red` blade, 130.3 mm, vertical pin | NOT YET DRIVEN BY CHAD |
| **the throttle lever** | 264-vert shell split out of its housing | NOT YET DRIVEN BY CHAD |

**MEASURED, all reproducible (see §5 for how):**

```
brake elbow      +2.1 mm  ->  +29.6 mm UP
brake wrist    -35.0 mm   ->    0.0    (the twist, deleted)
throttle arm   -18.0 / +29.1  ->  UNCHANGED, bit-identical
thumb tip                     ->  (-1.0, -7.9, +33.8) mm
mitt tip                      ->  (-33.0, +2.9, -10.3) mm
every channel at zero input   ->  EXACTLY 0.0
```

★ The two arms and the two control bones are **0-OFF exact**: at zero input the
signed rest visual is reproduced bit for bit, which is what let two new driven
bones be added to a pose Chad had already signed.

---

## 2. THE GATE

Run it in `build/`, one ctest per build dir at a time. Before this rung the set
was **1560 tests / 5 reds**, and those five were **PROVEN pre-existing by
revert-and-compare, not asserted**: stash the branch, rebuild, re-run the same
five by name, confirm the identical set. They are

```
54  probe P-F: the relentless raider keeps the pump and shoots back
    sled_slides_before_it_tips_on_flat_snow
    sled_grip_ceiling_stays_below_the_tip_threshold
    sled_assist_reference_plane_is_load_weighted
    sled_debug_sink_is_write_only
```

★★ **`sled_debug_sink_is_write_only` is NOT a firewall failure, and this matters
for R4a.** 14402 of its 14403 assertions pass, including every bit-identity
check on position, orientation, velocity, angular velocity, assist and hull. The
single red is `REQUIRE(saw_release_under_one)` — a **non-vacuity** check that
the fixture still drives the release band below 1.0. The sink really is
write-only; what regressed is the fixture's coverage, which is the same GI4
release-band debt as the other three. R4a's Phase 0 leans on that firewall and
may rely on it.

★ 4 new legs this rung, all `[r3hands]`, **both mutations verified red**: a
hard-coded axis (caught by 2 legs) and a flipped sign (caught by 3).

---

## 3. ★★★ THE FINGER SQUEEZE IS NOT REACHABLE FROM THE RUNTIME

**Do not retry it in code. It is a MESH question, and the mesh is signed art.**

Measured off the shipped skin, via the **inverse-bind matrices** (§5 trap 2):

* `mittfront_01_l` carries **610 verts, ALL at weight exactly 1.0** — zero
  blending. It is a rigid block, not a finger.
* That block sits **34–104 mm FORWARD of the bar**, straddling it vertically,
  with its long axis LATERAL (127 mm — four fingers side by side).
* Its bone axis is `(-0.126, -0.535, +0.836)`: a flat hand pointing forward and
  down, not fingers curled around a bar.

**Mass forward of a pivot swings DOWN when it rotates.** The complete menu of
directions that tip can travel — for ANY pivot axis, since the tip is confined
to the plane perpendicular to the bone — is:

```
medial   down    aft
 0.99    0.00   0.15   <- no sink, but 99% medial   (the SIGNED pelvis build)
 0.64    0.60   0.48
 0.08    0.85   0.53   <- most aft available, and 85% of it is down
```

There is **no direction with aft dominant AND no sink AND no medial**. Chad
rejected both ends of that menu by eye before I had measured it. A real aft
squeeze needs the mitt front reshaped to wrap UNDER the bar — that reopens the
R2c-M2 hands he hand-tuned and signed 2026-08-18, and it was deliberately not
taken. **If it is ever taken, it is an art rung with his eye on it, not a fix.**

---

## 4. THE LEVERS — WHAT THEY ARE, AND THE TRAP

Nothing in this repo had **ever** driven either lever: squeezing the brake left
the red blade dead in the air. That is the mechanism Chad described — *"they
will press on the brake lever and in turn, squeeze it into the handlebar"* — and
it is a machine part, not his signed hand, which is why it was safe to build.

* **Brake** = the `indy_red` blade, 130.3 mm. Its angle is **MEASURED, not
  chosen**: swung about its pin until the tip reaches the bar it takes
  **19.9°**, and the tip travels `(-4.5, -0.5, -47.6)` mm — essentially pure
  aft, 48 mm. ★ It is a dial measured off a surface: **re-solve it if the bar or
  the lever moves.**
* ★★ **Throttle: `indy_red` is the KILL CAP, not the lever.** Chad: *"throttle
  is not red"*. That tab is 22 mm and `thumb_01_r` sits **121 mm** from it. The
  real lever was authored inside the SAME `indy_rubber` primitive as the box it
  hinges on, so neither material nor node separates them — but they are separate
  **shells**. `split_outboard_shell()` welds by position, unions across
  triangles, and peels the lever out: **264 of 350 verts, a thin 98.4 × 54.8 ×
  10.1 mm plate**, the only plate of four shells, reaching furthest outboard,
  and 70 mm from the thumb where every other shell is 122–130 mm. Four
  independent signals, one answer.
* Both pin at their **INBOARD** end in the **aft-face plane** (Chad's own R2c-M2
  ruling). **One positive turn about vertical serves both** — the brake extends
  outboard at +x and must go aft, the throttle outboard at −x and must go
  forward; opposite sides and opposite intents cancel exactly. Derived, not lucky.
* ⚠ **The throttle's 12° is NOT measured** — its lever already sits on the bar
  at rest, so there is no contact angle to solve. Chad rules it.

---

## 5. THE INSTRUMENT, AND THE FOUR TRAPS IT CAUGHT

```
SEADS_SLED_DEBUG_MODE=1 SEADS_SLED_CTL_DEBUG=1 \
SEADS_SLED_RIG_SMOKE="steer,sL,sR,sT,lat,up,fwd,absorb,thr,brk" \
  ./build-play/seads.exe --smoke 110 shot.png
```

Fields 9/10 are throttle/brake. Pinning the suspension makes the pose static, so
the print is a clean A/B. It is **headless, deterministic, and already existed**.

★★★ **THE PRINT IS A DIFFERENTIAL — this frame WITH the control minus this frame
WITHOUT it.** The rider rides the suspension, so a raw elbow height moves ~40 mm
frame to frame at zero input. An absolute position could never have shown a
2.1 mm rise, which is exactly why the defect survived until it was measured.

1. ★★★ **A REPORT IN THE WRONG FRAME.** `d_tip` first differenced the bone's
   `local` alone — the *hand's* frame, arbitrarily rotated — and printed
   "+34 mm forward" for fingers Chad could see going skyward. A delta is only a
   direction if you say which frame it is in.
2. ★★★ **THE SPEC SAYS TO IGNORE A SKINNED MESH'S NODE TRANSFORM.** Reading the
   mitt through `rest_world[sp.node]` put its verts 1.2 m from their own bone
   and gave a shelf of 26.7 mm; through the inverse-bind matrices it is 8.1 mm.
   I built a "fix" on the wrong number and deleted it. **Two measurements of one
   thing that disagree mean one is lying.**
3. ★★ **A DEAD BRANCH MY OWN INSTRUMENT CAUGHT.** A per-side solve for the brake
   elbow's height probed `solve_chain`, which reads `sm.nodes[].world` — NOT
   posed at load — while its inputs came from `rest_world`. It returned
   −0.6612 m, the `rise > 0` guard refused it, and the whole branch silently
   fell back to the shipped angle. **And the fallback was the right answer:**
   the 20° was never the problem, the bar roll was dragging the very chain the
   pole rotates about. Delete the roll and the untouched 20° buys +29.6 mm.
4. ★★ **NAMING A VECTOR "AFT" DOES NOT MAKE IT AFT.** `pelvis − grip_socket` was
   called aft and levelled; the pelvis is ON the centreline and the grip 315 mm
   outboard, so it was 315 mm of *lateral*. Measure a machine axis SAME-SIDE
   (grip → board socket) and the lateral terms cancel by construction.

---

## 6. THE NEXT ASK

**R4a — the grip law and the buck-off loop.** Spec is `docs/SUDBURIAN_LADDER.md`
§7, Chad's own 2026-08-16 ruling, staged R4a–R4e. A full plan exists and was
**red-teamed by three independent fresh contexts** before he read it. His
rulings: derived kernel fields are **NOT taped**; lateral lean deferred;
`grip_capacity_n` dialled on current tapes and **re-solved when his deforming
terrain lands**; `KEY_J` stays the post-throw stopgap until R4c.

★★★ **The two structural findings that must survive into the build:**

1. **Superman is impossible in the frame the solver runs in.** `trail_chain` is
   Verlet in **sled model space** (`trail_chain.h:126`), where the machine's
   linear and angular acceleration are identically zero — only aero drag can
   lift the chain, and the flown `drag_k = 0.153/m` is a *scarf ribbon* number
   (≈0.0053 for an 87.5 kg man, **29× smaller**). Needs frame pseudo-forces plus
   `a_body`/`alpha` plumbed to render through `SledRig`. **Do not** run the
   chain in world space (float at R = 15 km) or re-difference velocity in render.
2. **A point-mass rider deletes the quantity the rung is about.** Nine unknown
   contact scalars against three equations, and it can take **no moment** — but
   "he pivots up over the bars" IS a pitch moment, and a seat-first ordering
   reports **zero grip load on the bump that throws him**. Use a planar sagittal
   rod with contact-set enumeration; `I_yy` comes free from `rider_segments()`.

Also settled by measurement, so do not re-litigate: the drivetrain needs **no
work** (idle rpm, clutch decouple below 3.25 m/s, free-coast, engine brake all
exist; no-creep falls out by arithmetic) — but §7.4's FIRST link is missing,
because throttle→idle on release does not exist (`sim/sled.cpp:241` gates on
`rolled`, not on rider attachment; the spring-return is the W key). And §7.7's
"all taped" would break **all 19 tapes** — the pin roster is positional.

**Smaller, cheap, and open:** the lever throws are Chad's to rule (§4); lateral
lean `lean_l`/`lean_r` is still one rigid pelvis translation
(`sled_model.cpp:1504`).

---

## 7. ★ R4a PHASE 0 IS BUILT AND MEASURED — 2026-08-25, branch `sandbox/r4a-phase0`

**READ `docs/R4A_PHASE0_FINDINGS.md` FIRST.** It is the deliverable. Its working
is `docs/R4A_PHASE0_MODEL.md` (the derivation of the rider-load instrument) and
`docs/R4A_PHASE0_DATA.md` (the full per-tape distribution). All three are
**untracked and uncommitted**, on `sandbox/r4a-phase0` off `8852cf7d7`.

**What exists now that did not before.** A planar sagittal **rod** rider with
contact-set enumeration (seat / boards / hands), rebuilt from the shipped
`indy650.glb`, run as `seads_sled_probe griphold <tape>`. It answers §6.2 of this
file: a point-mass rider deletes the quantity the rung is about, and this one does
not — at rest it reports **grip = 0.00 N**, and on the bump that throws him it
reports the whole load through the hands.

**The measurement.** All **27** tapes in `build-play/` (not 19 — tapes 20–27 were
recorded during and after this rung; §7.7's *"all 19 tapes"* is now *"all 27"*)
replayed **in full and bit-exactly**: `first_div tick = -1` everywhere,
**743,157 ticks = 103.2 minutes** of Chad's own driving. The low-passed grip load
sits at **200–450 N** most of the time and its per-tape peak spans **461 N (a road
cruise) to 32,732 N (a crash)**. On the bush tapes the rider is in **CASE F —
hands the only contact — for 8–34 % of all substeps**. The findings doc proposes a
**range** for `grip_capacity_n` (floor ~1,000 N, knee at 5,000–15,000 N) and
explicitly does not pick a value. **Chad rules it.**

**The firewall held.** `sim/` gained **+17/+19 lines, zero deletions**: two
`double[3]` fields on `SledDebugSubstep` and one write block entirely inside
`if (dbg)`. The sink is null on the shipped path, all 27 tapes are bit-exact, and
the gate is the same 1,560 / 5 pre-existing reds named in §2. No golden, no tape,
no `SledParams`, no `render/`, no asset, no PIN roster.

**Three lenses red-teamed it, and one found a real hole — do not skip it.** The
CASE-F entry test is `V <= 0`, which is not the same statement as "nothing below
carries"; in a small fraction of F substeps the instrument reports **less** than a
statically admissible solution (worst measured under-report 572.5 N). It is
`docs/R4A_PHASE0_FINDINGS.md` §3.1 and it is Phase 1's first item, together with
an **exceedance-EVENT count** (§3.3) — the tape-level counts in the findings doc
cannot tell one honest crash from forty flickers, and the release latch is
one-way.

**Both structural findings of §6 above survive intact.** The `a_body`/`alpha_body`
half of the superman plumbing now exists on the debug record; the frame
pseudo-forces and the `SledRig` route to render do not. And §7.4's first link —
throttle → idle on release — is still missing (`sim/sled.cpp:241` gates on
`rolled`, not on rider attachment).
