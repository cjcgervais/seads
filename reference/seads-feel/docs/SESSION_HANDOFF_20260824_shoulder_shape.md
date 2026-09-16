# HANDOFF — THE SHOULDER'S SHAPE (SW-2). Chad has ruled; the shape is not there yet.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then §0
and §1 of this file. **Do not start by building.** §2 is the ask: run a team of
CONTEXT-FREE consult agents who RESEARCH REFERENCE IMAGES of real shoulders and
come back with a shape spec, then converge them. The gate to believe is
`gate_live.py` run INSIDE BLENDER — see §6: the shell gate does not certify
what Chad sees."*

> **STATE 2026-08-23, end of session:** `body_geom.py` 14/14 and
> `weld_geom.py` 8/8, both green, in Blender's own Python. **Nothing is saved
> and nothing is committed.** The live proxy holds the *previous* weld — Chad
> stopped the last apply on purpose, so his viewport is one step behind the
> tree.

`docs/SESSION_HANDOFF_20260823_weld_groin.md` (six addenda) is the WELD's
history and its measurements stand. This file is the SHAPE.

---

## 0. WHAT CHAD SAID, IN HIS OWN WORDS, IN ORDER

These four are the spec. Everything else here is measurement.

1. *"it's the shape of the shoulders BEFORE the weld that makes them half the
   thickness of the arm just distal to the weld — there is a significant
   deficit of material at the join. Do you agree?"*
2. *"my collar bone is aligned with the top of my shoulders, sudburian's collar
   bone is much lower than that. I noticed because you filling in the channel
   creates a collision with the collar bone, and the collar bone should be a
   little above the shoulder actually."*
3. *"bring the collar right up, then the part of the shoulder that is the
   channel needs a POSITIVE BUILD OUT to be a viable shoulder."*
4. *"not the width of the arms — get rid of the ridges and build up the top of
   the arm, which IS the top of the shoulder, so there isn't a valley in his
   arm followed by a big ring ridge down the arm. That all needs to be smoothed
   out and shaped."*

★ He was right every time, and each one was confirmed by measurement before it
was acted on. **His eye has beaten the clause on this file for four rungs
running.** Do not open by arguing with him.

## 1. DO THIS FIRST

1. `CLAUDE.md`'s three laws, then `docs/RIDER_AUTHORITY.md`.
2. `cd assets/character/sudburian_src`
3. **In Blender**, not on a shell:
   `exec(open(r"...\gate_live.py").read())` → body 14/14, weld 8/8.
4. Read §6 before you trust any number you produce.

## 2. ★★★ THE ASK: A TEAM OF CONTEXT-FREE CONSULTS WHO RESEARCH IMAGES

Chad's instruction, 2026-08-23: *"include context-free consults from a team of
agents to solve this so it looks good — use reference images"*, and then:
***"I need them to research reference images."***

So: **do not go straight to dials.** Spawn a team. Each agent starts with NO
context from that session and must re-derive from the artefact — the standing
rule in memory (`red-team-major-work`): a consult that reads the previous
agent's conclusions is not a consult.

Give each agent only:

* the four quotes in §0,
* the measured tables in §3 — **numbers, no diagnosis**,
* `assets/character/sudburian_src/ref_sweater/` as an example of what is
  already held, plus its `SOURCES.md`, which is the house format for recording
  where an image came from,
* the instruction to **research NEW reference imagery on the web and cite it**.

Suggested lanes, one agent each, all context-free:

| lane | what it must come back with |
|---|---|
| **anatomy** | where the deltoid's mass actually sits relative to the acromion and the clavicle, from anatomical plates. `ref_sweater/D_gray*.png` is a start and is NOT enough |
| **the clothed shoulder** | photographs of a man in a SNOWSUIT / parka, from above and from the front. The garment is what Chad sees, not skin. How does the sleeve head read from above? |
| **sleeve construction** | set-in vs raglan vs drop shoulder (`ref_sweater/C_*` has two). Which one is the Sudburian's suit, and what does its shoulder line DO? This decides whether any ridge belongs there at all |
| **the collar line** | where a collar bone reads on a clothed man, against the shoulder top and the base of the neck. Chad: *"aligned with the top of my shoulders"*, *"right at the base of my neck"* |
| **red team** | given the other lanes' specs, attack them: which is contradicted by a photograph? Re-derive, do not summarise |

Converge them, THEN write the spec, THEN touch a dial. Every image gets a line
in `SOURCES.md`.

## 3. THE MEASUREMENTS — hand these over WITHOUT the diagnosis

**The top of the man, walked straight down onto the arm's own bone line**
(authored shells, no weld), and which shell is on top:

```
  t       torso top   arm top    on top    (t = fraction of the 344 mm bone)
 0.000     1.5245     1.5087     torso     the arm is 15.8 mm BELOW the torso
 0.050     1.5170     1.5110     torso
 0.075     1.5118     1.5111     torso
 0.100     1.5045     1.5111     ARM
 0.125     1.4906     1.5112     ARM
 0.250       --       1.5114     ARM       <- flat from 0.025 to 0.25
 0.300       --       1.5077     ARM
 0.500       --       1.4967     ARM
```

**Thickness about the arm's own axis**, before and after the deltoid Chad ruled
in:

```
   t        0.10  0.15  0.20  0.25  0.30
 before      115   129   134   140   162 mm
 now         145   164   175   172   164
```

**Distance from the arm's bone axis to the skin, at the joint vs just distal:**

```
                    at the joint (t 0)     just distal (t 0.30)
 up-and-outboard     torso  10.3 mm             arm  68.8 mm
 fore/aft            torso  47.6 mm             arm  98.5 mm
 UPPERARM_R declares       100.0 mm                  90.8 mm
```

**Landmarks:** acromion 1.51286 · jugular notch (ANSUR) 1.5180 · clavicle ridge
1.5264 (at `CLAV_T` 0.60) · top of the front yoke 1.5465 · neck join 1.5941 ·
the arm's own highest ring 1.5162 at t −0.02.

**The front yoke is only 39 mm tall** — 1.5077 at its outer edge to 1.5465 where
it meets the neck. That is why the collar bone cannot be moved two inches by
sliding it along the yoke. See §5.

## 4. WHAT IS BUILT AND ON

* **`DELTOID_FILL = 1.6`** — Chad's ruling *"close it fully"*. Returns the arm's
  ring squash toward round and gives the resulting rise straight back as drop,
  so the mass goes DOWN and OUTBOARD and the top surface stays put. The
  thickness table above is what it bought.
* **`CLAV_T = 0.60`** — new dial. The clavicle ridge's position along the yoke
  had been a hardcoded `0.30`. Moving it in `t` raises the ridge WITHOUT
  touching `CLAV_H`, the amplitude Chad ruled down in SW-1b for protruding
  forward. **It buys +7.2 mm and he has asked for 50.8.**
* the weld's own rung — mesh-exact cut, curve seam, `unshave`. Other handoff.

Ceilings moved during that session, **every one on his explicit ruling with the
number in front of him**: `CREASE_MED_MAX` 82→85→87, `CREASE_P90_MAX`
100→102→103, `UPPERARM_BUILT_MAX` 0.1195→0.1320, `SHOULDER_TOP_MAX`
11.0→5.0→6.0→9.0.

★ **Four ceilings in one night is a DEBT, not a licence.** The crease they are
paid out of is Chad's own complaint from an earlier rung. The next rung should
be buying it back, not spending more.

## 5. WHAT IS BUILT AND OFF, AND EXACTLY WHAT BLOCKS EACH

* **`DELTOID_LIFT`** — gives back `SHOULDER_DROP` so the arm's top becomes the
  shoulder's top, which is item 4 of the spec. At 0.4 the arm's top goes
  1.5115 → 1.5222 and meets the torso's 1.5211. **The width does not move**
  (0.2454 → 0.2454), so his width ruling is untouched. OFF because it trips
  **L2d**.
* **`CHANNEL_BUILD`** — extra radial mass at the crease, weighted OFF the side
  (zero at y≈0 where the width is measured, ~1 at |y| 0.10 where the channel
  is) so it can build the front and back without touching the width. Item 3.
  Written, untested; the spec moved under it.

### ★★★ L2d IS THE BLOCKER, AND RE-AIMING IT IS THE RUNG

L2d pins the arm's top 0.2 mm under the **acromion**. Its own note reads: *"the
acromion IS the top of the shoulder, so skin over the deltoid reaches it and
does not clear it."* The first half is anatomy. The second does not follow —
**the torso's own skin at that same station stands 11.6 mm above that bone**
(trapezius, and a snowsuit over it). The clause holds the ARM to the bone and
the TORSO to nothing, so the arm can never become the top of the shoulder,
which is exactly what Chad asked for.

★ **Three re-aims were tried and none is clean. Do not repeat them:**

* the shoulder POINT as reference catches the yoke climbing to the neck and
  reads 24 mm of headroom that is not there;
* the skin directly above the arm's highest vertex finds **no torso within
  20 mm at all**, because by then the arm's top is outboard of where the torso
  has any;
* a station-by-station comparison is unfair where the torso is curving down to
  END.

The clause was left as it was rather than shipped half-defined. **This is what
the consult team should settle first, because it is a question about what a
shoulder IS — which is what reference images answer.**

### The collar bone needs the NECK, not the ridge

Two inches puts the clavicle at 1.577, which is **above the entire yoke
surface** (top 1.5465). What is in the way is that the front of the neck
junction sits 77 mm above the front of the chest (`NECK_FRONT_DROP`), turning
the whole front of the shoulder into a long ramp with the collar bone low on
it. Moving it means moving the neck join down or raising the chest — and
raising the chest will fight `L3b`, already at 54.6 of its 54.8 mm limit.

★ The file pins the jugular notch to ANSUR at 1.5180, which is 5 mm ABOVE the
acromion, so **the anthropometric data and Chad's eye disagree here.** On this
file his eye has won four times. Bring photographs, not tables.

## 6. ★★★ P0 — THE SHELL GATE DOES NOT CERTIFY WHAT CHAD SEES

`python weld_geom.py` runs whatever CPython is on PATH; Blender runs its own.
MEASURED, same files, same body, 3.11 against Blender's 3.13:

```
 body_geom   max |delta|   4.4e-16 m    one ULP -- the authored body AGREES
 the WELD    max |delta|   6.6e-03 m    and a DIFFERENT FACE SET
 W6 worst posed turn       65.4 deg on 3.11, 105.2 deg on 3.13
```

**One ULP in, six and a half millimetres out.** The amplifier is the weld's
discrete decisions — `flip_slivers`' greedy flips, `unfold`'s give-up test,
`bridge_dp`'s minimal-area ties, the parity cut, the zip threshold: every one
is a comparison, and a comparison sitting 1e-16 from its own boundary goes
either way. **Use `gate_live.py` inside Blender.** Reducing the amplification is
its own rung: quantise each of those comparisons to a tolerance well clear of
float noise, so the same file builds the same mesh everywhere.

## 7. TRAPS THAT SESSION HIT — all of them mine

1. **W8's probe window stopped at |y| 0.08 and Chad's channel is at |y| 0.10.**
   The clause read +5.4 mm while a 13 mm trench sat two centimetres outside it.
   A clause is a WINDOW as much as a threshold.
2. **A sha1 of `%.4f` coordinates differed at every precision because of
   SIGNED ZERO** (`-0.0000` vs `0.0000`) while the geometry agreed to 4e-16.
   Two bad instruments before the right one, which was `max |delta|`.
3. **Blaming set-iteration order for that divergence was wrong** — sorting
   every neighbour sum changed nothing. It is written into `taubin`'s docstring
   AS WRONG, because a plausible diagnosis that was never tested is a guess.
4. **A one-sided smoother is a runaway.** Clamping the fairing so it may fill
   but not shave removed the only restoring force and inflated the shell to
   +395 km at 40 iterations. The constraint has to be against a SURFACE that
   does not move (`unshave()`), not against the motion.
5. **Defaults bind at import.** `collar()`'s `tang` was never passed on, so a
   whole tangency sweep printed six identical rows. This programme's own
   handoff warned about it and the function did it anyway.
6. Blender rules unchanged: agents open it themselves, GUI never headless, ONE
   writer, and the rider is told apart by BONE COUNT (19 = legacy and frozen).
