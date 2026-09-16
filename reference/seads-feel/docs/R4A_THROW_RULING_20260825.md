# R4a — THE THROW, RULED BY CHAD 2026-08-25

His words, verbatim, and the authority for everything below:

> "if the bump is minor it might bounce em a bit, if its hard he might suyperman
> the way the scarf is, if it a hard enought jolt, and hands let go, its because
> there is a force pulling so the force persists throwing the sudburian in the
> direction of the jolt, a marker marks the snowmachine that is easy to see and
> the sudburian can run back to his snowmachine however far he was thrown verses
> where the snowmachine stopped. He can get thrown off in any direction if he
> supermans, his legs will hit the seat, but if he holds on he staysd on, if he
> lets go he will likely be thrown."

Earlier the same day, the ruling this one completes:

> "Most times he holds on but if the force is too big he can be thrown off
> relative to the jolt, hands coming off is necessary to be thrown but not
> sufficient so as to determine the thrownedness"

---

## 1. THE OPEN SPEC CONFLICT IS CLOSED — §7.3 STAGE 4 STANDS AS WRITTEN

A consult raised, correctly, that "hands coming off is necessary but not
sufficient" appeared to contradict `SUDBURIAN_LADDER.md` §7.3, whose stage 4 is
one gate (`grip load > grip capacity -> he is a free body`) and "a one-way
transition". Two readings were possible: a NEW recoverable hands-off state, or
graded severity with stage 4 intact.

**Chad ruled the second: "if he holds on he stays on, if he lets go he will
likely be thrown."** There is NO re-grab, NO "bounced loose and caught it".
Stage 4 remains one-way. Do not fork it.

What is graded is **WHICH STAGE HE REACHES**, and the ladder he described maps
one-to-one onto the stages the spec already has:

| his words | spec stage | hands |
|---|---|---|
| "if the bump is minor it might bounce em a bit" | 1 UNWEIGHTED / 2 BOARDS FREE | ON |
| "if its hard he might superman the way the scarf is" | 3 SUPERMAN | **ON** |
| "if its a hard enough jolt, and hands let go" | 4 RELEASE -> 5 TUMBLE | OFF |

★ **SUPERMAN HAPPENS WHILE HE IS STILL HOLDING ON.** This is the load-bearing
correction. Superman is not a symptom of losing the bar — it is the last and
most extreme ATTACHED state, and §7.3 already says so ("the scarf solver,
anchored at the grips"). Chad's "the way the scarf is" confirms the mechanism by
name. Every stage 0-3 is continuous and reversible; only 3 -> 4 is not.

So `grip_capacity_n` gates stage 4 ALONE. Stages 1-3 are selected by the load
levels BELOW capacity — which is precisely what the planned `seat_load_frac` and
`board_load_frac` fields measure. They are not diagnostics; they are the stage
selector.

## 2. FOUR THINGS THE SPEC DOES NOT SAY, NOW RULED

### 2.1 ★★★ THE JOLT IS A VECTOR, NOT A SCALAR

> "the force persists throwing the sudburian in the DIRECTION of the jolt"

The Phase-0 consult recommended severity as a scalar (the unheld impulse as a
departure speed). **That is now insufficient by ruling.** The throw has a
direction and it is the direction of the jolt.

This is a cheap correction rather than a rewrite, because the quantity is
already a vector upstream: the hand force is `H = (H_z, H_y)` in the sagittal
model and the unheld part inherits its direction. The severity becomes the
VECTOR unheld impulse; its magnitude is the departure speed already derived, its
direction is the throw heading. **Anything that records only a magnitude throws
away half the ruling and forces a corpus re-run to recover it.**

★ **REFINED BY CHAD, same session: "one direction for the jolt throw."** ONE
direction per throw event — a single vector, not a cone, not a distribution, not
a per-limb fan. Combined with "he can get thrown off in ANY direction if he
supermans": the direction VARIES between throws, but any given throw has exactly
one. Record one unit vector per event; do not build a scatter model.

⚠ STILL OPEN, and the refinement does not close it: the Phase-0 rod model is
**planar sagittal** — it can express fore-aft and up-down, and it CANNOT express
lateral. "One direction" is still one direction whichever plane it lives in, so
the question stands: can that vector carry lateral content at all? Either the
throw direction comes from a fuller force basis than the rod model carries, or
the lateral component comes from elsewhere. **Do not paper over it by
normalising a planar vector and calling it a heading** — that would look like an
answer and be fiction.

### 2.2 THE FORCE PERSISTS THROUGH THE RELEASE

> "its because there is a force pulling so the force PERSISTS"

The throw is not an impulse imparted at the instant of release and then
ballistic. The thing that overloaded his hands is still acting when they let go,
and it keeps throwing him. Consequence: the departure is not fully described by
a velocity at t_release; it has continued acceleration for as long as the cause
lasts. A model that hands the tumble a single initial velocity is a
simplification, and must be labelled as one.

### 2.3 HIS LEGS HIT THE SEAT DURING SUPERMAN

> "He can get thrown off in any direction if he supermans, his legs will hit the
> seat"

A collision the spec's stage 3 does not mention. The legs trail from the hips as
a damped chain and that chain STRIKES THE SEAT. This is both a visual (the legs
do not pass through the machine) and, plausibly, a force path — a leg striking
the seat is a contact that can redirect him. At minimum it is a collision
constraint on the chain; whether it feeds back into the throw is unruled.

### 2.4 THE MARKER AND THE RUN BACK

> "a marker marks the snowmachine that is easy to see and the sudburian can run
> back to his snowmachine however far he was thrown verses where the snowmachine
> stopped"

Spec §7.3 stage 7 already has "runs to the machine". Chad adds:
- a **MARKER** on the machine, deliberately **easy to see** — this is a HUD /
  world-marker feature, not a rider-animation one, and it is the thing that makes
  the run back playable rather than a hunt.
- the two rest positions are **DIFFERENT and both matter**: where HE stopped
  versus where the MACHINE stopped. The distance between them is the cost of the
  throw, and §7.4 already establishes the machine coasts to rest idling. So the
  run-back distance is an emergent consequence of the throw severity — which is
  the felt payoff of getting the jolt magnitude right.

## 3. WHAT THIS CHANGES IN THE PLANNED WORK

1. **The single tape pass must record the jolt as a VECTOR** (§2.1). Magnitude
   only = a re-run later. Cheap now.
2. **`seat_load_frac` / `board_load_frac` are promoted** from diagnostics to the
   stage-1/2/3 selector (§1).
3. **Superman needs the pseudo-force plumbing** flagged as structural finding 4
   in the Phase-0 handoff — `trail_chain` runs in SLED MODEL SPACE
   (`render/trail_chain.h:126`) where the machine's linear and angular
   acceleration are identically zero, so as shipped it CANNOT superman at all.
   Chad has now ruled superman is a core attached state on the common path, not
   an edge case, which RAISES that finding's priority: the intermediate rung of
   his own ladder is currently unbuildable.
4. **The leg-seat collision** (§2.3) is new work on the chain.
5. **The machine marker** (§2.4) is a HUD/world item, and Chad has said HUD is
   the live thread — it may belong to that lane rather than to R4a.

## 4. WHAT IS STILL OPEN

- **The any-direction throw versus the planar rod model** (§2.1). The sharpest
  open question; it is a modelling question, not a coding one.
- **"LIKELY be thrown"** — his word. Release is one-way and normally throws him;
  whether "likely" admits a rare non-throw, or is just how a man talks, is not
  worth a question on its own. Assume release throws him; revisit only if the
  felt result wants softening.
- **Whether the leg-seat strike feeds back into the throw** or is purely visual.

Nothing in this document was self-passed. Every rule above is his sentence or a
direct consequence of it, and the three open items are marked open rather than
resolved by inference.

---

## 5. THE SEATED SELF-RIGHT (Chad, 2026-08-26)

> "AND IF ON THE SEAT AFTER A ROLLOVER PRESSING THE STAND BUTTON AS IN REGULAR
> SEATED OPERATION WILL RIGHT THE SLEIGHT ONTO ITS SKIS, MAKE SURE WE CAN HAVE
> AN ANIMATION BASED ON POSITION. SO ITS EASY FOR A PLAYER TO UPWRIGHT
> THEMSELVES JUST SHIFT"

### 5.1 What it is

If he **stayed on** through a rollover — hands never let go, so he is still on the
machine, lying on its side — then the **STAND button**, the same one used in
ordinary seated riding, rights the machine onto its skis. No new key, no special
mode, no prompt. The player uprights himself by doing the thing he already does:
**shift his weight.**

### 5.2 Why this is the right shape, mechanically

- ★ **`stand` is ALREADY A TAPED INPUT** — one of the six in `in6`
  (`test/harness/sled_tape.h:193`). A righting driven by it is a normal input
  through the normal path, so it replays bit-exactly on every tape in the corpus
  with **no tape format change and no pin-roster extension**. Compare `KEY_R`.
- ★ **IT REPLACES A CHEAT WITH A MECHANIC.** `KEY_R` autoright
  (`app/main.cpp:3259`) is self-described scaffolding: *"a key for now that lets
  me autoright until we get the guy running back to the snowmachine... not
  physics... Replaced by the S8 walk-back embodiment later."* It teleports the
  machine upright and kills all motion. Worse, an external write to sled state
  emits a tape O record carrying a full 41-field pin, which cannot carry derived
  state — the reproducibility hole flagged this session. A stand-driven right has
  none of that.
- ★ **"ANIMATION BASED ON POSITION"** is the house pattern, not a new ask: the
  sled model already drives "every moving part a function of SledState"
  (`render/sled_model.cpp`). The righting must therefore be a CONTINUOUS
  FUNCTION OF STATE — never a canned clip, never a timer — so the body and the
  machine can be posed from how far through the roll he actually is.

### 5.3 What this ADDS to the spec

`SUDBURIAN_LADDER.md` §7.3 stage 8 RIGHT IT is the **ON FOOT** case — *"grabs the
highest point of the machine and rolls it down like spinning a heavy wheel"* —
reached only after being thrown, tumbling and getting up. Chad's ruling is the
**ON SEAT** case, which the ladder does not contain: he never left the machine,
so stages 4-7 never happened. It belongs as a branch off the rolled state, not
as a variant of stage 8.

### 5.4 ⚠ ONE CONFLICT TO PUT TO HIM BEFORE BUILDING

A ruling of 2026-08-21 made **P the side-hang "PULL" key**, whose stated jobs
include *"flip sled back over / tip-over recovery / hillside pre-stabilize"*.
That overlaps this ruling directly. Three readings, and it is his to pick:

  (a) P stays for the ON-FOOT flip-over (stage 8) and hillside pre-stabilise;
      STAND owns the ON-SEAT case. Complementary, both live.
  (b) STAND supersedes P for righting entirely; P keeps only the side-hang and
      hillside jobs.
  (c) P is dropped.

**Do not build until this is answered** — guessing here would either strand a
ruled key or ship two controls that do the same thing.

### 5.5 Open, and NOT to be self-passed

- Does righting need the machine to be **stopped**, or can it be done rolling?
- Is it **instant on press**, or does it take a continuous hold with the machine
  coming up progressively (which is what "animation based on position" implies,
  and what would let a half-hearted shift fail)?
- Can it FAIL — on a slope, against the roll direction, in deep snow? A right
  that always works is a button; one that can fail is a skill.

### 5.6 RULED 2026-08-26 — the whole mechanic, and P IS DROPPED

> "DROP P, MACHINE NEEDS TO BE TIPPING OVER STANDING AUTOMATICALLY HELPS PUSH
> YOU OVER RIGHTED. BUT ABOVE A CERTAIN SPEED IT CANNOT HAPPEN ABOVE 5KM/H.
> YRES IT CAN FAIL TO RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD, A SOCOND
> SUSTAINED PRESS MIGHT HELP IT FURTHER OR MAYBE THEY CANT AND JUST PRESS R IN
> THAT CASE.."

Every §5.4 / §5.5 question is answered:

| question | ruling |
|---|---|
| P vs STAND | ★ **P IS DROPPED.** The 2026-08-21 side-hang PULL key is cancelled. STAND owns righting. |
| when does it apply | the machine must be **TIPPING OVER** — tipped, not upright. Standing "automatically helps push you over righted". |
| speed | **NOT above 5 km/h.** Above that it cannot happen at all. |
| can it fail | **YES** — "it can fail to right given the situation". |
| instant or held | **PROGRESSIVE HOLD.** |
| repeat | a **second sustained press may help further** — and if it still will not come up, **press R**. |

★ **R SURVIVES, DELIBERATELY, AS THE ESCAPE HATCH.** `KEY_R` autoright was
written as scaffolding to be deleted (`app/main.cpp:3259`). Chad has now given it
a permanent job: the out when the mechanic honestly fails. Do NOT delete it, and
do not quietly "improve" it into physics — its whole value is being the thing
that always works when the thing that can fail did.

**Why "it can fail" is the design and not a caveat.** A right that always works
is a button. One that can fail is a skill: on a slope, shifting against the way
it went over, or bogged in deep snow, the weight shift is not enough and he has
to think. The failure is EMERGENT here, not scripted — the assist is a torque and
gravity on the tipped machine is another; when gravity wins, it does not come up.
Nothing tests an angle and decides to refuse.

### 5.7 THE SHIPPING CONSTRAINT — this one CAN move the tapes

Every R0-R3 rung was render-side and structurally could not touch a tape. **This
one is a kernel force change**, and unlike the planned R4a fields it is NOT
inert by construction: `stand` is a taped input, so any recorded moment where he
was tipped, below 5 km/h, and holding stand WOULD replay differently. The corpus
contains rollovers and he does press stand.

So it ships behind an OFF-by-default dial, per §7.7's proven pattern
(`assist_hull_frac` / `roll_stiff_vgain` / `release_floor_frac`, "all 0-OFF
bit-identical"):

- the strength param defaults to **0.0**, which makes the whole block a no-op,
- it is added to the tape's **name-keyed** param roster (backward-safe; the
  POSITIONAL pin roster is NOT touched),
- and the **tape-absent preset** sets it to 0.0, so every one of the 31 existing
  tapes replays bit-exactly against today's kernel,
- while the game's own config turns it on.

Miss that preset and every old tape silently replays with whatever the build
default of the day is — THE LAW, paid for five times.

---

## 6. ★★★ LEAN AND RIGHTING ARE DIFFERENT SYSTEMS (Chad, 2026-08-26)

> "I dont understand why yo are clonlating the lean mechanism with the righting
> after the roll over, im not going to ask a player to lean with the mouse when
> inverted... leaning is for the flying off the handlebar direction ,,, why is
> leaning coming into righting the sled?"

**He is right, and it was my error.** The v3/v4 self-right read the rider's
lateral offset into the righting direction, so which way the machine came up
depended on a control belonging to a different system. Two things are now
separated by ruling, permanently:

| system | control | what it decides |
|---|---|---|
| **THE THROW** | his **LEAN** | **which way he flies off the handlebars** |
| **THE RIGHTING** | the **STAND** button, alone | which way the machine comes up |

★ **NEVER ask the player to lean while inverted.** It is not a control he has
any business using in that state, and requiring it made the mechanic fail on his
drive for a reason no test could see.

**What the righting is allowed to use:**
- the **machine's own attitude** (`-up_body.x`) — the only honest source, since
  the machine is the thing lying over;
- the **shove's own committed side**, latched per press (`right_shift_cmd`), so
  a commitment reinforces itself the way a man's does;
- and the **automatic brace** — which is NOT leaning. The player commands
  nothing; it is the rider's body going where a man's body goes when he stands
  up to shove, which is his own earlier ruling: *"standing automatically helps
  push you over righted."* It writes the rider's lateral offset because that is
  the physical thing that moves, and it is the state the leg-push animation is
  posed from.

**What it must NEVER use:** `in.lean_lat`, or `rider_lat_m` as a total (which
carries his lean input inside it). Reading the total is exactly the bug — it
looks like reading the rider's position and is really reading his mouse.

★ MEASURED AFTER THE FIX: from full inversion the machine rights in one press at
lean 0.0 (3.6 deg) and lean 1.0 (4.8 deg) alike. Lean-independent, which is the
executable form of this ruling.

**Consequence for the throw, not yet built:** §2.1's open question — how the
planar rod supplies an any-direction throw — now has his answer. **The LEAN is
the throw direction.** That is a ruling to build against when the throw lands,
and it means the lateral component of the throw comes from a control he is
already using, not from a term the rod model cannot express.
