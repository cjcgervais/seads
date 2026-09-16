# HANDOFF — START HERE. R4a: THE STAGE SELECTOR IS BUILT, MEASURED, AND ARMED.

> Launch line for the next session:
> **"Read `docs/SESSION_HANDOFF_20260827_r4a_arming.md`; do §7."**

Prior handoffs, in order, all still true where this one does not supersede
them: `docs/SESSION_HANDOFF_20260826_r4a_selfright.md` →
`docs/SESSION_HANDOFF_20260827_r4a_superman.md` →
`docs/SESSION_HANDOFF_20260827_r4a_plumbing.md` →
`docs/SESSION_HANDOFF_20260827_r4a_bodychain.md`. **Its §7 set this rung**, and
this is items 1, 2 and the physics half of 3.

---

## 0. THE STATE

| branch | state |
|---|---|
| `sandbox/r4a-phase0` | this rung. **COMMITTED** `2228182b3` + `109e51fd2`, NOT pushed (see §6). |
| `main` `fa2cfe3b9` | unchanged. |

**Nothing the game draws has changed.** The stage machine and the body chain
are computed every frame; the drawn rider is NOT yet posed from the chain. What
is new and visible is a DEBUG DRAW, off by default, behind
`SEADS_BODY_CHAIN=1` — and that is deliberate (§3).

---

## 1. ★★★ THE MEASUREMENT THAT SHAPED THE RUNG, AND IT IS A NEGATIVE RESULT

The handoff told this rung to get `seat_load_frac` / `board_load_frac`
render-side and build the stage machine on them. Before wiring anything I ran
the selector over the replayable tape corpus. **It never fires.**

```
seads_sled_probe stagesel sled_tape_2.sledtape        (870 ticks, settle excluded)
  seat_frac   n=750  min=0.6697 p50=0.6710 max=0.6799
  board_frac  n=750  min=0.3273 p50=0.3282 max=0.3287
  arm         n=750  min=0.0000 p50=0.0000 max=0.0000
  arm >= 0.05 :  K 0.00 %   R 0.00 %          (every level, every tape)
```

Every tape, settle excluded: **CASE S, 100 %.** `seat_frac` only ever goes
**UP** (0.67 → 0.99 on the bush tapes — landings, not unweightings). The
`griphold` case mix's non-S substeps are all inside the first 12 ticks of a
tape: the spawn settle, and nothing else.

★ **THE ATTRIBUTION, and it is not a defect in the selector.** The corpus has
essentially **no airtime** — `griphold`'s own PEAK context reads `air_s 0.000`
on the violent tapes. A rider is unweighted by the machine dropping away from
him, and on Chad's recorded driving the machine never does.

Two consequences, and both are load-bearing:

1. **It cannot false-arm on anything he has driven.** 0.00 % over the whole
   corpus is exactly SUDBURIAN_LADDER §7.9's "release must be rare, it takes a
   big bump" — measured rather than hoped.
2. ⚠ **AND THEREFORE THE CORPUS CANNOT PROVE IT FIRES.** A table of zeros is
   what a DEAD selector and an unexercised one both look like. That is the same
   disease this program has paid for four times (`back_clr`, `surf_clr`, the
   wrap's back plane, the body-chain penetration metric): a number that cannot
   fail. So liveness is proved on the ONE axis that unweights a rider, and the
   probe prints the ladder before it touches a tape:

```
-- LIVENESS LADDER (synthetic: a_body.y swept, no rotation, no lean) --
    a_y/g   seat_frac  board_frac    arm   case
    -1.0      0.0000      0.0000  1.0000   A      <- FREE FALL
    -0.8      0.1338      0.0662  0.6400   S
    -0.6      0.2675      0.1325  0.3600   S
    -0.4      0.4013      0.1987  0.1600   S
    -0.2      0.5350      0.2650  0.0400   S
    +0.0      0.6688      0.3312  0.0000   S      <- SEATED
    +1.0      1.3376      0.6624  0.0000   S      <- a landing
```

---

## 2. WHAT LANDED

### 2.1 `render/rider_load.{h,cpp}` — the rod model, MOVED not rewritten

The R4a Phase-0 planar sagittal rod (four closed-form cases, the admissibility
postcondition, `grip_load_lp`'s house filter) lived **private inside
`tools/sled_probe.cpp`**. It is now a pure render-core TU that BOTH the probe
and the shipped game read.

★ The reason is the house law, not tidiness: a stage machine in render that
re-derived these loads would be a SECOND implementation of a measured model,
and every published number in `docs/SESSION_HANDOFF_20260825_r4a_phase0.md`
describes THIS code. If the game ran a copy, those numbers would stop
describing the game the moment either copy moved and nothing would go red.

**The move is proved BIT-IDENTICAL**, not argued: `griphold` and `superman`
were captured on six tapes plus the synthetic self-check leg, the probe was
rebuilt from `git show HEAD:` sources, captured again, and `diff -r` is empty.
Re-verified a second time after the new `stagesel` mode landed.

### 2.2 `stagesel` — the new probe mode, and the differential it exists for

The kernel's `a_body`/`alpha_body` are only in `SledDebugSubstep` under
`if (dbg)`, and §7 item 1 forbids wiring a sink into the shipped kernel to
reach them (tape-visible). So render finite-differences the already-shipped
`velocity` / `angular_vel` — **and that is an approximation.** `stagesel`
measures it, replaying a tape and solving the same model twice per tick:

* **arm K** — the kernel's own triple (griphold's arm, the truth);
* **arm R** — the derivation the game runs.

★★★ **AND IT CARRIES THE TERM A BARE DIFFERENCE THROWS AWAY.**
`trail_frame_field` differences the COMPONENTS of a vector in a ROTATING frame.
The true inertial acceleration is `a = d(v_body)/dt + omega x v_body`, and the
second term is exactly what a component-wise backward difference loses. With it
in, over the corpus:

| | p50 | max |
|---|---|---|
| `|d seat_frac|` R vs K | **0.0002** | **0.0235** |
| `|d board_frac|` | 0.0002 | 0.0423 |
| `|d arm|` | 0.0000 | 0.0000 |
| `|d a|` m/s² | 0.31 | 2.26 |

The game's selector and the instrument agree to under 2.4 % of a load fraction
on Chad's real driving, with no debug sink anywhere near the kernel.

★★★ **AND THEN I MEASURED THE TERM ITSELF, AND IT BUYS NOTHING HERE.** A third
arm — R0, the same derivation with `omega x v` REMOVED — reads:

| | R (with the term) | R0 (without) |
|---|---|---|
| `|d seat_frac|` p50 / max, tape 4 | 0.0002 / 0.0190 | 0.0002 / 0.0183 |
| `|d seat_frac|` p50 / max, tape 33 | 0.0028 / 0.0235 | 0.0026 / 0.0269 |
| `|d a|` m/s² p50 / max, tape 4 | 0.31 / 2.26 | 0.32 / 2.32 |

A wash — sometimes microscopically *better* without it. The reason is the same
blind spot as §1: **`omega` is small in ordinary riding**, so `omega x v` is
below the frame-differencing error, and the corpus contains no state where it
would not be. The term is KEPT because it is the correct expression and because
the states this chain exists for — a rollover, a tumble, a hard yaw — are
exactly where `omega` is not small. But it is not the difference-maker, and
saying so is the point: a correction nobody has weighed is indistinguishable
from a correction nobody needs.

⚠ **CONSEQUENCE FOR THE SCARF, and it corrects what I first wrote.** The
scarf's `frame_accel_mps2` is the same bare difference and is therefore short
by `omega x v` too — but on this evidence that omission is **immaterial in
ordinary riding**, not a defect worth touching a look you drove and signed on
2026-08-27 ("didnt notice because I was immersed and the scarf helped that").
It is left alone, and it is a question only if a high-`omega` state ever reads
wrong.

### 2.3 ★★★ The arming weight — and there is no threshold in it

```
u_seat  = 1 - min(1, seat_load_frac  / seat_frac_rest)     stage 1 UNWEIGHTED
u_board = 1 - min(1, board_load_frac / board_frac_rest)    stage 2 BOARDS FREE
arm     = u_seat * u_board                                 stage 3 SUPERMAN
```

The two references are the model's **OWN at-rest split**, computed from the
same solve at load time and **never typed**. THE LAW, paid for seven times: a
constant that describes the shipped table stops describing it the moment the
table moves. So there is no constant — re-export the rider and both references
follow him.

The product is **1 exactly at CASE F** (nothing below carries, which IS "the
body extends behind the anchor") and **0 whenever either support still carries
its resting share**. Continuous and reversible, which is §7.3's own requirement
for stages 0–3. Nothing in between is a dial anybody chose.

★ **IT IS A PRODUCT AND THAT IS THE RULING.** One support still carrying is not
superman however free the other one is. A `min`, a `max`, a mean or either
factor alone all differ, and the gate's case 3 is what each of those dies on.

### 2.4 The pin — and the obvious reading of the priming rule is a trap

`render/body_chain.h` §1.2: *"PRIME THE BODY CHAIN WHEN IT ARMS, IN THE POSE IT
ARMS IN."* The obvious implementation — prime once at the arming edge — primes
it **SEATED**, because the moment the weight first leaves zero he is still on
the machine (it only reaches 1 in free fall). That is a prime in exactly the
pose the rule forbids, and the least-penetration exit would take his pelvis out
through the nearest face.

So the chain is not primed at an edge at all. **While the weight is zero the
chain is PINNED TO THE DRAWN MAN every frame, and it is RELEASED when the stage
arms.** Then "the pose it arms in" is exact and automatic, there is no edge to
detect, the keep-out only starts biting once he is already leaving the seat,
and it is reversible — the weight falls back to zero and the chain re-pins.

The pin is a bake, not a guess: each of the 17 measured stations is projected
onto the rest-pose polyline of the rider's own joints (lateral pairs collapsed
to their mean, the same collapse the load model makes) and stored as
`(segment, t)` **plus its residual offset in that segment's own frame**; at
runtime the SAME `(segment, t)` and the SAME frame are evaluated on the POSED
polyline. A station therefore follows the drawn man through every pose the R3
ladder can strike.

★ **THE OFFSET IS NOT TIDINESS — the game's own log is what found it.** With
projection alone the load-time print read `pin residual max 0.0802 m`: one
station (the shoulder, where the chain cuts a corner the joints do not) sat
80 mm off the polyline. 80 mm of residual is **80 mm of POP** that the first
constraint pass takes out the instant the chain arms. Carrying the offset makes
the pin exact by construction, so there is nothing to pop; the number is still
printed, now as evidence of how far the chain departs from the joint line
rather than as an error.

### 2.5 The wiring

`SledRig` / `DrawInfo` gained three fields — `rider_mass_kg`, `mass_kg`,
`cg_height_m` — straight reads of `sim::SledParams` in `app/main.cpp`, the
one-number rule. All three zero means "not supplied" and the whole stage
machine stays off rather than dividing by a zero mass.

The chain runs **after every pose pass** (so the pin reads the DRAWN man) and
**before the scarf** (whose anchor is the neck, and would be a frame stale the
day the body drives the spine).

---

## 3. WHAT IS *NOT* IN THIS RUNG, AND WHY IT IS A SEPARATE ONE

**§7 item 3's second half — blending the drawn rider onto the chain frames — is
NOT built.** That is a deliberate stop, not an omission of convenience:

* The arms are IK-welded to the grips and the boots to the boards *inside*
  `pose_pass`, which runs BEFORE this block. Driving the pelvis from the chain
  means re-entering the pose solve with a chain-driven root, or the hands tear
  off the bar. That is a structural change to the R3 pose ladder, and it is the
  half Chad's eye has to grade.
* Doing it badly is worse than not doing it: it would put a broken superman in
  front of him and tell us nothing about the keep-out, which is the thing this
  ladder actually rests on.

**So the felt question is put to him a different way, and it is an honest one.**
`SEADS_BODY_CHAIN=1` draws the 16-link chain AND the per-station DRAWN-SURFACE
boxes — the boxes the keep-out actually grades, not a centreline and not a
ball. Blue = pinned to the man; orange = armed and integrating. That is
literally the geometry §1.1 of the body-chain handoff decided the whole ladder
on, and it answers **does his body rest on the machine, or float over it?**
without a single line of art depending on the answer.

---

## 4. THE GATE

`test/unit/test_rider_load.cpp`, six cases, and it deliberately **pins no asset
number** — the stations, mass and inertia come from `indy650.glb` and are
re-measured by the probe on every run. Re-typing them here would be THE LAW
broken an eighth time. Every leg is either a property that must hold for any
admissible model, or a statement about the arm formula, which has no asset in
it.

| # | what it pins |
|---|---|
| 1 | force AND moment balance close to 1e-6 over a 2-D sweep, no support ever pulls DOWN, and **all four cases are visited** (non-vacuity) |
| 2 | ★★★ the liveness ladder: 0 seated, 0 at +1 g and +3 g, **1.0 at free fall**, monotone, no step above 0.05 per 0.01 g, reversible |
| 3 | ★★★ the arm is a PRODUCT: either support carrying its resting share gives 0; both half-released gives 0.25 (no min/max/mean does); over-load clamps at 0; a degenerate reference refuses to arm |
| 4 | the weight reaches 1 exactly where nothing carries — the formula and the enumeration cannot fork |
| 5 | the low-pass is the house filter at tau 0.1 s on the SUBSTEP (the named mutation is "use dt") |
| 6 | the model→body map is `sim/sled.h`'s K2, and an involution on the two flipped axes |
| 7 | ★★★ **THE CREST** — with `W_supp >= 0` and `V <= 0`, a contact MUST stay pinned and the arm must NOT read superman. See below: this case exists because a mutation walked through cases 1–6. |

Plus the extraction's own proof: the probe's output is **bit-identical** across
the move (§2.1), three times.

**FULL GATE: 1601 tests, 5 red, ALL PRE-EXISTING, verified BY NAME** — the same
five `docs/SESSION_HANDOFF_20260827_r4a_bodychain.md` §3.1 names:

```
probe P-F: the relentless raider keeps the pump and shoots back
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

(The count moves as tests are added — this rung added seven — so a number
comparison lies; the names are the check.)

### 4.1 ★★★ THE MUTATION THAT SURVIVED, AND WHY IT MATTERED

Five mutations, each applied alone against a rebuilt binary:

| mutation | result |
|---|---|
| the arm returns `u_seat` alone (drop the product) | **killed** (case 3, 2 assertions) |
| the arm returns `min(u_seat, u_board)` | **killed** (case 3) |
| the low-pass steps on `dt` instead of the substep | **killed** (case 5) |
| the model→body map drops the z negation | **killed** (case 6) |
| **the CASE-F test reverted to the old `V <= 0` rule** | **SURVIVED cases 1–6** |

The last one is the finding. R4a Phase 0 replaced a `V <= 0` CASE-F test with
`W_supp < 0`, because the sign of the resultant vertical demand is only a PROXY
for "nothing below carries" — reading it throws away every real solution where
the hands haul DOWN on the bar while the seat presses up, i.e. **every crest
and every fall-away** (it under-reported the grip by up to 572 N).

**A mutation back to that wrong rule passed every leg I had written.** It had
to: CASE F is a self-CONSISTENT solution, so force and moment still close and
no support pulls down; and CASE A is still reached from the `V > 0` branch, so
even the four-case non-vacuity count stayed green. Only a leg that knows WHICH
branch is right can see it.

★ And it is not academic: under the wrong rule **the arming weight jumps to a
full superman on every crest** — the exact false-positive §1's corpus result
was claiming there are none of.

Case 7 is `tools/sled_probe.cpp`'s asserted leg (iv) brought into the gate, with
its own both-ways non-vacuity count. The mutant now dies on 6 assertions.

---

## 5. THE DRIVE — build it, open it, and one question

```
D:\flight_sim2\seads-recon\build-play\seads.exe
```

Built for you. Do NOT open `build\seads.exe` — that is `-O0` and every felt
call made on it was made on a binary paying a 45x penalty on the rider skinning
alone.

1. Drive normally first with **nothing set**. Everything must look exactly as it
   did — this rung draws nothing new by default. If anything moved, stop.
2. Then close it and open it again with the debug draw on:
   `set SEADS_BODY_CHAIN=1` then run the same exe. A blue chain and blue boxes
   should be riding the man, resting on the machine.
3. **THE QUESTION, and it is the whole rung:** with the boxes drawn — **does his
   body rest on the machine, or float over it?** If it floats, the suspect is
   named in advance (body-chain handoff §5.3): the torso boxes are conservative
   and their lateral / fore-aft extents are clamped by nothing.
4. Optional, to see it ARM without finding a big enough jump: `set
   SEADS_BODY_ARM=1` forces the stage weight to 1 and the chain goes orange and
   starts trailing from the grips. That is superman's physics, drawn.
5. `SEADS_BODY_CHAIN=2` also logs the live stage numbers each frame.

⚠ **What a forced arm looks like PARKED, so it does not read as a defect.** I
took a smoke shot at 0 km/h with `SEADS_BODY_ARM=1`: the chain folds into a
heap hanging off the bars beside the machine. That is what 2.2 m of limp man
hanging from the grips with no airspeed SHOULD look like — superman is a
motion state, and a stationary machine has nothing to stream him with. **Judge
the arm on a jump, not in the yard.**

★ The load-time log also cross-checks itself against the instrument, and they
agree exactly: the game prints `I_pitch 7.091 kg m2, rest split seat 0.6688 /
board 0.3312`, and `seads_sled_probe griphold` prints `I_pitch 7.09063` and
`seat_frac +0.6688 / board_frac +0.3312` — two different code paths reading two
different sources (`rest_world` in the game, cgltf node transforms in the
probe) landing on the same model.

---

## 6. OPEN, AND WHOSE IT IS

1. ~~**Nothing is committed yet**~~ — **COMMITTED** on `sandbox/r4a-phase0`:
   `2228182b3` (the rung) and `109e51fd2` (the four defects his drives found).
   18 ahead of the remote, **NOT pushed** — the push is yours to ask for.
2. **The drawn-rider blend** (§3). The next rung, and the one that needs the
   pose ladder re-entered.
3. **The scarf's missing `omega x v`** (§2.2) — MEASURED to be immaterial in
   ordinary riding, so it is NOT an open task. It becomes one only if a
   high-`omega` state (a rollover, a tumble) ever reads wrong.
4. **`seat_keepout_m`** — still 0.0, still your dial, still meaning a clearance
   held off the DRAWN surface.
5. **`drag_k = 0.005/m` for a body** is still my derivation, not measured off
   him. Carried over unchanged.
6. ~~**The return-to-pose spring**~~ — **CLOSED by §8.2/§8.4**: built as
   `TrailChainParams::pose_stiff_hz`, ruled by Chad ("hold his shape, trail from
   it"), shipped at 1 Hz and modulated by `stage_arm`. PROVISIONAL on the value
   only — he drove it and the body held (§8.5).
7. **The coat sink** — still attributed and parked. Untouched by this rung.

---

## 7. THE NEXT RUNG

Blend the drawn rider onto the chain frames, which means:

1. Decide where the pose solve re-enters. The chain gives the pelvis a position;
   the hands must stay welded to the bar (Chad's ruling: **superman happens
   while he is still holding on**), so the arm IK has to re-solve after the root
   moves, not before.
2. Drive the legs from the pelvis down — §7.3 stage 3's own words, "legs trail
   from the hips as a damped chain."
3. Blend by `stage_arm`, so stage 0 is bit-identical to the signed R3 pose.
4. Then build it for him and give him the absolute path.

---

## 8. ★★★ CHAD'S FIRST DRIVE, 2026-08-27 — TWO FINDINGS, BOTH FIXED

> "yea it goes to orange and then he is flailing all around even if tipped over
> like a firehose unattended"

### 8.1 IT ARMED ON ROLL ALONE, AND THAT WAS MY ERROR

Measured after his report — `stagesel`'s new ROLL LADDER, at rest, **no
acceleration anywhere in it**:

```
    roll     seat_frac  board_frac    arm   case
       0 deg    0.6688      0.3312  0.0000   S
      45 deg    0.4729      0.2342  0.0858   S
      60 deg    0.3344      0.1656  0.2500   S
      90 deg    0.0000      0.0000  1.0000   S
     180 deg    0.0000      0.0000  1.0000   F
```

**Rolling the machine armed the chain with the machine parked.** The cause is
the rod model's own documented blindness: the supports are FRICTIONLESS and
carry only along body +Y, so tipping swings that axis off vertical and both
loads fall as `cos(roll)`. The model is RIGHT — a frictionless seat cannot hold
a man sideways — and it is the wrong question. A man lying on his tipped-over
machine is not supermanning; by Chad's own ruling
(`R4A_THROW_RULING_20260825.md` §5.6) that is the STAND self-right branch.

★ **I wrote that blindness into `render/rider_load.h`'s own header and then
built the stage selector on top of it without asking what roll does.** And §1's
corpus result could not catch it: the corpus has no tip-overs. The honest form
of that finding was never just "it cannot be proved to fire" — it was **"it
cannot be BOUNDED either"**, and I only said the first half.

**THE FIX ADDS NO CONSTANT.** The references were the model's frozen at-rest
UPRIGHT split. They are now **LIVE**: the same solve at the same attitude and
the same lean with the **acceleration set to zero**. The weight then measures
what it was always meant to — how much support the machine took away by
ACCELERATING — not which way down happens to point.

| | reference | live | arm |
|---|---|---|---|
| rolled 60°, at rest | 0.3344 | 0.3344 | **0** |
| rolled 60°, free fall | 0.3344 | 0.0000 | **1** |
| on its side, at rest | 0.0000 | 0.0000 | **0** (nothing left to lose) |

Pinned by gate case 8, which asserts BOTH halves — roll alone never arms, and a
real unweighting at that same roll still does (6 of the 13 rolls swept, which is
every roll upright enough to have support to lose).

### 8.2 THE FLAIL — RULED, AND THE SPRING THAT ANSWERS IT

His words were the exact failure `SUDBURIAN_LADDER.md` §7.6 bans a ragdoll to
avoid: *"floppy, weightless motion."* A trailing chain with no angular stiffness
is a ragdoll in that respect, and open item 6 of this handoff had already named
it.

**RULED BY CHAD, asked directly: HOLD HIS SHAPE, TRAIL FROM IT** — the man keeps
a recognisable body and the legs trail behind it; the dial is **how far a jolt
can pull him out of the pose.**

Built as `TrailChainParams::pose_stiff_hz` — a per-particle **critically damped**
spring toward a target pose the caller supplies. It cannot ring (the failure it
removes is oscillation, and a fix that oscillates is not one), its stiffness is
clamped below the explicit-integration stability limit `f < 1/(2·pi·dt)` so no
dial can blow the chain up, and **0 Hz is bit-identical** to the pre-spring
solver, so the scarf does not move and the arm Chad rejected survives as the A/B.

★ **The target costs nothing new: it is the PIN** — the drawn man's own R3 pose,
which this block already evaluates every frame. The spring pulls him back toward
exactly where he would be if he were not being thrown.

**THE PULL-OUT LADDER** — how far a 40 m/s² jolt held 0.1 s drags the TOE off the
pose, on the real 16-link measured body (gate case 8 of `test_body_chain.cpp`):

| stiffness | toe pull-out |
|---|---|
| **0 Hz** | **1.703 m** ← the bare chain. The firehose. |
| **1 Hz** | **0.285 m** ← **SHIPPED**: a readable trail, the body still his |
| 2 Hz | 0.085 m |
| 4 Hz | 0.026 m |
| 8 Hz | 0.002 m ← rigid; the legs may as well be welded |

1 Hz is the rung that best matches his own words, and it is a MEASURED rung, not
a value interpolated between two. **It is PROVISIONAL — he drives it.**
`SEADS_BODY_POSE_HZ=<hz>` walks the ladder without a rebuild; `=0` is the
firehose arm.

### 8.3 ★★★ "THE GREEN BOXES WERENT THERE" — AND THAT WAS MY INSTRUMENT

His second sentence, and it has its OWN cause on top of §8.1. The debug draw
picked its colour with

```cpp
const bool armed = sm.stage_arm > 0.0f;      // <- a LATCH
```

so **any** nonzero weight painted the whole thing orange. Combined with §8.1 —
roll alone produced a small nonzero weight, and a sled on terrain is never
exactly level — orange was the permanent state and **green was a colour he
could not see.**

★ It is the same disease this program keeps paying for, wearing a different
hat: **a BINARY read of a CONTINUOUS quantity.** The whole ruling under test is
that stages 0-3 are *continuous and reversible*; an instrument that can only say
"0" or "not 0" cannot show that, and cannot distinguish `arm = 0.0001` (a sled
sitting on a slope) from `arm = 1.0` (free fall). Two separate defects were
hiding behind one colour.

The colour is now a RAMP over `stage_arm`: green → yellow → orange as the weight
goes 0 → 1, links blue → orange with it. Partial arming is now visible AS
partial, which is the only way he can judge a continuous stage machine by eye.

### 8.4 ★★★ SECOND DRIVE: "FLAILING ALONG MOSTLY GREEN" — THE RELEASE WAS A LATCH

The roll fix, the low-pass and the colour ramp all worked: his log is **84.4 %
of frames below 0.05** — green. And he was still flailing.

**Green means PINNED, and a pinned chain cannot flail.** So it was not pinned.
The release read the weight as a LATCH:

```cpp
if (sm.stage_arm <= 0.0f || !sm.body.primed) { /* pin */ } else { /* integrate */ }
```

A weight of 0.0007 cut him completely loose — free-flying on a 1 Hz spring —
while the colour, reading the SAME number continuously, showed him as good as
seated. **Measured on his own log: 51 % of frames released.**

★★★ **THIRD INSTANCE OF ONE DISEASE IN MY OWN CODE, and I fixed it in the
instrument last round and left it in the mechanism**: a BINARY read of a
CONTINUOUS quantity. §7.3's whole ruling is that stages 0-3 are continuous and
reversible; a release that is on or off cannot express that, and the two reads
of `stage_arm` had silently forked — one continuous, one not.

**THE FIX IS CHAD'S OWN SENTENCE MADE LITERAL.** He named the dial as "how far
the jolt can pull him out of it", and how far he can be pulled *is how
unweighted he is*. So the weight MODULATES THE STIFFNESS —
`pose_stiff_hz = base / stage_arm` — instead of gating the solver. Stage 0 is
welded to his pose; stage 3 is the shipped stiffness and free to trail; nothing
switches. The constraint pass is untouched, which a positional blend between two
configurations would not be (it would shorten his links).

MEASURED (gate case 9, the same 40 m/s² jolt):

| stage weight | toe pull-out |
|---|---|
| 0.01 | 0.0000001 m — welded |
| 0.05 | 0.0000001 m — welded |
| 0.25 | 0.026 m |
| 1.00 | 0.285 m |

Under the latch every one of those rows was 0.285 m. Case 9 asserts the ratio
(a nearly-seated man must be an order of magnitude less free than an unweighted
one) and its own non-vacuity.

### 8.5 ★★★ THIRD DRIVE — THE BODY HOLDS. CHAD'S VERDICT

> "it seemed to be holding up when airborne on a big jump I saw orange, the
> flailing was the scarf this time"

**The body chain is holding.** His log, 8813 frames:

| stage weight | frames | |
|---|---|---|
| < 0.05 (welded to his pose) | 7161 | **81.3 %** |
| 0.05 - 0.5 (partly free) | 987 | 11.2 % |
| >= 0.5 (real superman) | 665 | 7.5 % |

peak 0.999, mean 0.086. **46 arming episodes**, 3-19 frames each, with peak
frame accelerations of 10-91 m/s² behind them — real events, not noise. That is
§7.3's continuous ladder behaving: mostly seated, occasionally pulled out of
shape, fully free only when the machine genuinely drops away from him. And the
one thing this rung was built to make possible — **orange on a big jump** — is
what he saw.

### 8.6 THE SCARF — NOT THIS RUNG, AND RULED ACCEPTED

The flail he saw this time was the SCARF, and I checked mechanically rather than
assuming: **my diff touches zero lines in the scarf block**, the solver's new
spring term is guarded (`pose_stiff_hz > 0 && i < n_pose`) and the scarf sets
neither, so its integrator is bit-identical; the 27-case trail-chain suite is
green on the unchanged path.

The mechanism is named for whoever meets it next: the scarf was given the
four-term non-inertial field on 2026-08-27, which is what makes it fly on
acceleration, and his log shows frame accelerations to **91 m/s²** on that jump.
A light ribbon (drag_k 0.153) with **no angular stiffness** in a 9-g field is a
firehose by construction — the identical missing mechanism the body just got.
His sign-off on the scarf came with "didnt notice because I was immersed", so a
jump of that size was plausibly never in that drive.

★ **RULED BY CHAD, 2026-08-27: LEAVE IT. "A scarf should whip."** This is
ACCEPTED BEHAVIOUR, not an open defect — do not "fix" it. The pose spring is
available to it for free if that ruling ever changes (zero-default, one line at
the call site), and that is the only reason it is written down here.

### 8.7 THE NEXT DRIVE

Same exe, same path. `SEADS_BODY_CHAIN=1`, get on the sled.

1. Blue (pinned) must still ride him, and **tipping the machine over must NOT
   turn it orange any more.** That is 8.1.
2. When it does go orange, **does he hold his shape and trail, or is he still a
   firehose?** That is 8.2 and the dial is `SEADS_BODY_POSE_HZ`.
3. If he is too stiff — the legs read welded — go DOWN the ladder (0.5). If he
   still whips, go UP (2, 4). One value per drive.
