# Decisions

Standing decisions for the flight kernel. Each entry: date, decision, why, status.

---

## 2026-08-03 — ✅ R-1 RULED: LEAD WITH RUDDER. Make it behave like v12. SETTLED.

**Chad's ruling, verbatim — this is the specification, quote it, never paraphrase it:**

> *"I rule to lead with rudder! ALL THE WAY IM TIRED OF BEING SO HELD TO COORDINATED FLIGHT FOPR
> THE SAKE OF A STRAIGHT LINE. V12 ACHIEVED THIS tHIS IS AN ARCADE GAME AND NEED A QUICK RESOLVE
> TO AIM. tHIS IS SETTLED ., mAKE IT BEHAVE LIKE V12"*

**Status: SETTLED. Chad sent this to the other agents himself and said *"im tired of hearing about
it."* No agent reopens it, re-measures it, or queues a follow-up question about it.**

### ⭐ THE PILOT RULED FIRST AND THE INSTRUMENT AGREED — keep this one

**He ruled without the curve in front of him.** The deflection curve was ordered as a
*precondition* — `EAGLE-R1-DIRECTIVE.md` said in terms *"do not rule this until the curve is in."*
He ruled anyway, and **the curve then corroborated him at every step size that matters:**

| aim step | coordinated `res90` | v12 crab `res90` | cost of coordination |
|---:|---:|---:|---:|
| 1° | 2.37 s | 0.32 s | **7.4×** |
| 2° | 3.20 s | 0.33 s | **9.7×** |
| 3° | 3.62 s | 0.36 s | **10.1×** |
| 5° | 4.05 s | 0.40 s | **10.1×** |

**Flat.** There is no small-deflection regime where coordinated flight is cheap — and small
deflections are what he tracks with.

> *"I feel so bogged down in minutia seems too slow anyways"* **was a correct reading of the
> aeroplane, not a mood.** It was read at the time as a pacing complaint about process. It was
> telemetry.

`THE LAW` clause 3 covers pilot-vs-instrument disagreement: the instrument is wrong and gets
rebuilt. **It has no clause for this case — they agreed independently, with the pilot ruling
first, on a question two agent sessions had failed to resolve.** That is worth more than either
alone, and it is the strongest evidence this project has that Chad's feel reports are primary
data in the SOP-01 sense rather than input to be translated.

**The rejected alternative measures worse**, which closes the loop: `ARM 2` — the roll-out law,
the `M1-PLANT-004` bank-and-elevator direction that `M1-STATE-005` named as *"the likely
direction"* — gives `res90` **8.04 s at 10°** and **12.11 s at 30°** against as-built's `4.03 s`
and `0.60 s`. **The specification's own stated direction buys dip and pays for it in exactly the
currency he had just said he was out of.**

*(Corroborated independently by `architecture` in `V022` / PACKET 11, which read the curve at
source. Accepted in `consults/PACKET-11-VERDICT.md`.)*

**He rejected the frame, not a point inside it.** R-1 offered three readings; this is none of
them. The question assumed coordinated flight as the constraint and asked what to pay for it. He
ruled that the constraint itself is wrong for this game: *an arcade game needs a quick resolve to
aim.*

**What it overrules — recorded once, here, and not re-raised with him:**

- **`M1-PLANT-002`** (*"The rudder's job is to keep the aeroplane coordinated, not to point it"*)
  is overruled **by its own author**. The pointed rudder is now the intended mechanism.
- **`M1-PLANT-009`** (peak |β| ≤ 2.0°) and **`M1-PLANT-010`** (the dip bar) are both exceeded by
  the v12 behaviour he has ruled for — measured, not assumed: v12 on the eagle bench gives
  pk|β| `9.53°` and dip `4.539°` at a 30° step.
- **`M1-STATE-005`**, called *"the open engineering problem of this specification"*, is
  **dissolved rather than solved.** It asked how to get coordination without paying resolve time.
  He has ruled that it is not to be bought at all.

**The target is already measured**, so nothing needs re-deriving: arm 3 of the deflection curve is
v12's shipped dials on the eagle bench (`consults/R1-DEFLECTION-CURVE-VERDICT.md`) — 30° step,
`resolve1 1.60 s`, `pk bank 65.6°`. Amending M1 and hitting that arm is the eagle's motion.
**`R-2` and `R-3` are dead as posed** — both were conditional on readings the ruling bypassed.

**The lesson this pass paid for, and it is about how questions are put to him.** The measurement
ordered in `EAGLE-R1-DIRECTIVE.md` was correct and worth running — it disconfirmed its own
hypothesis and produced the exact target the ruling now points at. But R-1 spent two agent
sessions and one of Chad's sittings offering him a choice **inside a frame he did not accept**,
and his answer was to throw the frame out. *"Bogged down in minutia"* was the early signal and it
was read as a pacing complaint. **It was a framing complaint.** When he pushes back on a
question's detail, test whether the question's premise is the thing he is rejecting — before
refining the options.

---

## 2026-08-03 — the docs agent WORKS THE QUEUE DIRECTLY (Chad's ruling), and two defects it found in its own tooling

**Chad's ruling, verbatim:** *"I choose option 1 do it here"* — in answer to a question about who
does the remaining work, given that every separate agent session needed him to press start.
**Status: STANDING.** The kernel-docs agent now executes pending work itself rather than
sequencing other sessions to do it.

**The authority rule is NOT relaxed by this, and was not violated.** This agent reads the other
trees and writes only paths inside its own `write_authority`. Verified mechanically, not asserted:
after landing `G-2` and the `R-1` curve, `tools/audit_graph.py` reported every dirty file in this
repo as `dirty, in-authority (kernel-docs)`, and nothing dirty in any other tree was attributable
to it. **Verification of another agent's work does not require writing in its tree** — both items
below were verified by *re-running the other agent's tool at source* and comparing.

**What was landed this pass:** `G-2` DONE (`consults/G2-RECONCILE-VERDICT.md`, three findings, the
first being a silently truncated tape header with no guard) and the `R-1` deflection curve
reproduced (`consults/R1-DEFLECTION-CURVE-VERDICT.md` — R-1 is **not** malformed and does **not**
collapse to reading 3).

### Two defects in this agent's OWN tool, found because the tool was actually run

Recorded because `tools/` is this agent's authority, which removes the "someone else owns it"
backstop — the same reasoning `CLAUDE.md` applies to `goldens/`.

1. **`audit_graph.py` reported `R-1 is BLOCKING` for a ruling whose heading says
   `NOT BLOCKING`.** The test was `"BLOCKING" in heading`, and `"BLOCKING"` is a substring of
   `"NOT BLOCKING"`. It had been firing falsely from the moment R-1 was held. **A check that
   inverts on the negation of its own keyword is worse than no check — it teaches the reader to
   discount the RED**, which is the same class as the INERT-CHECK LAW in `CONTRACTS.tsv`.
   Fixed: negations are tested first and win.

2. **The audit died with `UnicodeEncodeError` on a non-ASCII character in `agents.tsv`** — on a
   cp1252 console, mid-report, *after* the REDs had printed. **The audit must never fail because
   of a character in the data it audits.** Fixed at the stream, not by policing the data.

**And one defect in this agent's own practice, in the same pass:** it wrote an action item into
the eagle's `blocked_on`, which is the field for *what you await from someone else*. The audit
correctly went RED (`CONTESTED`) on the eagle's dirty tree as a result. **The register was
corrected rather than the check silenced** — the instruction moved to `motion`, `blocked_on`
returned to `none`. A message channel is not a status field.

## LIVE-BRANCH WATCH-ITEM — `feel/kernel-v5` moves past the seal (reconciliation is DONE)

**Resolved 2026-07-24:** the v4→v5 reconciliation merged. `main` in the game trees is
**`game-kernel-v5` (`36ee936e9`)** — the full game (tunnels, ballistics, Sudbury, Bf 109)
flies kernel v5, gate 797/797, first landing ever put down, Golden Felt Flight #1 flown on
that build. The old "feel branch diverges from a v4 main" danger no longer exists.

**Current resting state (2026-07-30 late night):** the feel branch is **sealed as
`flight-kernel-v12-2026-07-30` @ `e362df289`** (gate 404/404, tag + branch pushed,
verified — NOTE: v12's tag is LIGHTWEIGHT, a convention deviation from v6–v11's
annotated tags, noted in VERSIONS). Seal lineage: v6 `cfe1bd7fe` → v7 `51eb5b9e3` → v8
`ae7ae8f23` (pre-fly, never snapshotted) → v9 `29787debc` (camera arc closed) → v10
`f86ee7b9f` (buttery cascade) → v11 `0602d8292` (S-rollmix + blend instrument + P-helm)
→ **v12 (S-straightline: the tracer-line dip closed; "the baseline for a quality flight
kernel")**. Grafted to recon `2116f6ea3` (914/914), build-play re-stamped. **Golden #5
ordered, not yet flown.** The watch discipline stays — a session may move the branch
past v12 at any time:

- `reference/seads-feel/` is snapshotted at **`e362df289` = the v12 seal (2026-07-30
  late night)**, no purity exceptions. If the live tip has moved past that, the live
  tree is ground truth again until the next re-snapshot.
- **Every future session must check the branch state first** (read-only `git -C
  D:\flight_sim2\seads-feel log --oneline` / `git status` — never write there) before
  treating any dial value, snapshot, or cascade Code section as current. The branch has
  been observed to move between two commands of the same session.

---

## 2026-08-03 (evening) — ⛔ ITER-18: THE OVERSHOOT WAS NOT A DEFECT. IT IS THE PRICE OF THE TURN. A FEEL QUESTION STANDS FOR CHAD

**The eagle corrected its own iter-17 finding, and this agent had amplified the wrong half.**

`M1-COORDINATION-FINDING` §4 called the 86° bank on a 30° step an *overshoot-and-recover
problem*. This agent then paired it with `M1-STATE-004`'s *"roll first, pull later"* and recorded
the convergence as **"much stronger evidence than either alone."** That amplification was wrong,
and it was wrong in the most seductive way available: **two independent lines agreeing on a
conclusion that neither had actually tested.**

**The physics, which nobody had run:** a coordinated level turn gives `ω = g·√(n²−1)/V`. At
`V = 140 m/s`, 30° of heading in `1.60 s` costs **~80° of bank and ~5.8 g**. The ~86° the trace
showed is approximately *what the turn requires*. Every measured arm lands where that table
predicts.

**And it explains v12.** v12 buys its `1.60 s` with **9.3° of sideslip** — it points the nose
*without turning the aeroplane*. That is exactly the pointed rudder `M1-PLANT-002` outlaws and
`M1-STATE-003` names as the origin of the crab. **Chad wrote M1 to forbid the very mechanism that
gives v12 its fast resolve**, and the cost of that is now measured rather than assumed.

### The roll-out law was built, works as specified, and makes the aeroplane worse

Behind `rollout_bank_gain`, default `0.0`, knob-off arm **bit-identical** — ladder unchanged at
`246/250`, same four failures at the same values. With it on: bank tracks deflection
(`88.6° → 31.3°`), dip goes to `0.000°`, and **resolve30 goes from `5.36 s` to never inside a
10 s bench.**

### ⛔ THE FEEL QUESTION — Chad's alone, and no sweep can answer it

**A routine 30° aim step becomes an 80°-bank, ~5.8 g manoeuvre.** Nothing forbids it (`n_max`
is `32`) and it is genuinely coordinated. Three readings offered, none chosen:

1. **Hold `1.60 s`, accept the bank.** *Control is king* read as: the aeroplane goes where you
   point, promptly, whatever it costs in attitude — and the pilot asked for it by deflecting 30°.
2. **Relax the resolve target, keep the aeroplane docile.** `M1-PLANT-004` literally: bank
   proportional to deflection, `31°` of bank, a clean line at `dip 0.000°`, and it costs seconds.
   *A different aeroplane, not a worse one.*
3. **The target is deflection-dependent.** Small corrections prompt and gentle; a 30° step is a
   commitment. The eagle finds this most consistent with Chad's own `M1-PLANT-001` — *"the
   steepness of the bank angle I will need for the tightness of the turn radius I want to
   achieve."* **Reading 3 would change what `M1-PLANT-011` says — a specification change, Chad's
   alone.**

**Nothing was left changed:** no dial, no pin, no gate, no denominator movement, no golden
re-recorded. `rollout_bank_gain`/`_max` are not spec constants and carry no `PIN-` gate — pin debt
**recorded, not incurred.**

**Lesson for this agent, recorded because it is the reusable part:** convergence between an
independent measurement and a prior static analysis is *not* verification when neither tested the
underlying claim. It felt like corroboration; it was two descriptions of the same unexamined
assumption. **The check that would have caught it was arithmetic anyone could have run.**

---

## 2026-08-03 (later still) — TWO RULINGS MADE IN THE EAGLE'S SESSION, recorded here second-hand

**⚠ PROVENANCE, stated first because it is the point.** This agent did **not** witness either
ruling. Both were made by Chad in the eagle's own session and are recorded in that tree
(`GATE_REGISTRY.md` amendment 2, `70603ec`+). They reached this repo because the loop read the
tree, **not because anyone relayed them** — which is the agent graph working as designed. Per
`AGENT-GRAPH.md` §2c both are recorded with their source document named. **Chad should confirm
both if either is mis-stated here.**

### RULING 1 — the eagle is a REFERENCE IMPLEMENTATION, not the kernel EvC2026 ships

This answers `GOAL-2026-08-03.md` §3, the open question this agent raised and explicitly
declined to answer. It is **reading 2** of the three offered.

Quoted from `GATE_REGISTRY.md`: *"Chad ruled that this kernel is a **reference implementation
proving `MANDALARK1`**, not the kernel EVC2026 ships… A reference implementation is judged by
whether it demonstrates the specification's claims — so the `M1-PLANT-*` bars, which have **no
gate in this 250 yet**, are the part that matters most, and `250/250` would not by itself mean
the spec is proven."*

**This changes what the standing goal means.** The flyable cascade Chad tests and rules on comes
from the **EvC2026 / cascade-recorder** track. The eagle's deliverable is **the specification
proven**, and its gate score is not the measure of that.

### RULING 2 — the gate denominator moves `257 → 250` (amendment 2)

Ruled on `docs/PROPOSAL-257-REDERIVATION.md`, with parts **declined**. Seven gates carved out of
the count and **kept in the report**: four `SPEC-CAM-A0*` (the `SPEC-GAP-001` / Q9 carve-out,
plus a bar measured on a pacing law `M1 S-1` deletes) and three `BAR-BLEND-*` (measured on a
spherical v12 tape flown by a human, which `M1-ACC-002` forbids from blessing this kernel).

**Note the landing number is 250, not the proposal's 256.** Not a discrepancy: the eagle
declined to register the twelve candidate `M1-PLANT-*`/`M1-CAM-*` gates and the `SPEC-LINE-A0*`
legs *because their benches do not exist yet* — *"registering a count for gates whose benches do
not exist is how 259 acquired its arithmetic error."* `250` is openly flagged as **a number with
a known pending addition.**

**Three properties that make it checkable rather than trustworthy**, all the eagle's own:
nothing stopped being measured (removal is from the denominator, not the report, and the
carve-out list lives beside `EXPECTED_TOTAL` in the pre-registered document so no gate author
can exempt their own gate); **it does not buy a pass** — the three blend bars were *passing*, so
striking them removes 3 from numerator and denominator alike, and the score moved
`249/257 → 246/250` with four gates still red; and the deviation is **named, not hidden** —
point 3 states that rule 3 requires a carve-out be recorded *before* a run and that these four
camera gates were discovered untestable *during* runs at iter 14, so this is *"an openly-recorded
deviation from rule 3… not a clean application of it."*

### RULING 3 — `M1-PLANT-011` resolve **working target** `≤ 1.60 s`, deliberately NOT a gate

(Superseding this agent's earlier note that the number appeared unset — it was set in the same
session, in `docs/M1-PLANT-011-TARGET.md`, committed at `a643497`.)

**`≤ 1.60 s`** to resolve a 30° lateral aim step from level cruise — v12's own figure from
`M1 §5`, on the reasoning that it is what Chad flies today so anything slower is a regression he
would feel. **It is not registered, not in the 250, and must not be added to the ladder.**

**The registered bar is set only AFTER Chad flies it** (`M1-ACC-003`: *feel is the first
instrument*, and resolve time is the one bar here he can judge with his hand on the stick).

**Why that split is the right one, in the eagle's words:** *"A working target that is not a pass
condition cannot be gamed for a pass, so it can be used to steer engineering without touching
`SPEC-ACC-004`. The moment it goes into the ladder, setting it becomes a registration event and
has to happen before the run it scores."*

A counter-argument was considered and rejected on measurement, not preference: v12 bought its
`1.60 s` with the pointed rudder `M1-PLANT-002` outlaws, so equal resolve from a coordinated
aeroplane might be unobtainable — rejected because `sweep_authority` moved peak `|β|` `9.53° →
0.91°` without hitting an authority ceiling, and `trace_step30` shows the cost is an
**86°-overshoot-and-recover**, not a gain trade.

### RULING 4 — `OPEN_QUESTIONS` Q9 carve-out RULED (the `SPEC-CAM-A01/A02/A03/A06` camera gates)

Folded into amendment 2: those four are **reported, not counted**.

**Ladder state after all four rulings: `246/250`.** Four red remain, none of them a bar that was
lowered: `SPEC-CTL-A04`, `GOLDEN-CTRL-900`, `GOLDEN-CTRL-1200` (all `SPEC-ACC-005/006` STOPs) and
`BAR-STRAIGHTLINE-DIP` (a real bar miss).

---

## 2026-08-03 (later) — ITER-17 RE-RULED: **OPTION 2, M1 GOVERNS — REMOVE THE CAUSE** (Chad's word). Supersedes the Option A entry below

**Chad's ruling:** *"I went with 2."* — Option A is **withdrawn**. The `blend` gate stays. The
tree stays at `249/257` while the cause is worked, and that is accepted, not a failure.

**Why A fell, and it is not because the measurement went against it.** The eagle prototyped A,
measured it (dip `1.030° → 0.466°`) and reverted it, leaving the tree clean. A works. It is
barred anyway, by Chad's own specification:

- **`M1-PLANT-010`** — *"The straight-line property must be a **property of the plant**, not of
  a downstream correction. The dip bar must be met with the S-STRAIGHTLINE mechanism
  **disabled**."*
- **`S-8`** (supersession register) — S-STRAIGHTLINE is **Provisional**; *"the dip's cause is
  over-rudder; **remove the cause**"* — with the rationale, written 2026-08-01: *"The mechanism
  is gated on `blend`, so it is off in FINE — exactly where the lean law banks. **A patch that
  is absent where the problem now occurs.**"*

**Chad diagnosed this hole two days before the eagle measured it, and ruled the direction then.**
Option 2 is therefore not a new decision; it is `M1` being obeyed.

### ⛔ THIS AGENT'S DEFECT — the reason Chad ruled A on incomplete grounds

`docs/consults/EAGLE-ITER17-QUESTION.md`'s option table was written **against the v12 mirror**
and never mentioned `M1-PLANT-010` or `S-8`. But `M1-SCOPE-001/003` are explicit: *"It replaces
the v12 mirror as the governing document… where this specification and the v12 mirror disagree,
**this specification wins**."* The v12 mirror is *"evidence, not law"* (`M1-SCOPE-002`).

**I grounded a ruling in a superseded authority.** That is the same class as the back-filled v1
archive (`C3`): a document that looks official, cited without checking whether it still governs.
Chad was not choosing against `M1` — he did not know it was in the room. Registered as contract
**`C8`** so it cannot recur, and `C8`'s scope is wider than this incident: `M1`'s own header says
it supersedes the v12 mirror *"for this sandbox **and for EVC2026**"* — which puts the
cascade-recorder's tree under it too.

### The correction to Option 2 as the eagle framed it

The eagle proposed pursuing the `yaw_scale`/`K_coord` direction (`A4` = `0.730°`,
`A5` = `0.187°` with S-STRAIGHTLINE fully disabled). **`M1-STATE-005` says that is not the
direction:** *"The likely direction is more turning authority from bank and elevator — so the
rudder is not needed to point — **rather than further trading between the three dials above.**"*

`A4`/`A5` are exactly that trade. They buy coordination by **removing rudder authority**, which
costs resolve time — `M1 §5` measures **5.45 s to resolve a 30° step at `K_coord 16` against
v12's 1.60 s** — and `M1-PLANT-011` bars a configuration that meets one bar by losing the other.
Peak bank also moves `57.4° → 83.3° → 86.9°` on a half-second flick: a character change, not a
tuning nudge. **`A4`/`A5` are evidence the cause is reachable, not the fix.**

**First move, and it is a prerequisite rather than a parallel task:** build `M1 §5`'s 30° step
scenario into the ladder so resolve time is **measured, not assumed**. No arm settled inside the
existing 5 s bench, so that bench cannot arbitrate this trade at all.

### What stands unchanged

The bar stays `< 1.0°` and the denominator stays `257` (`SPEC-ACC-004`). Goldens do not move —
A was reverted, so `SPEC-ACC-006` is not engaged. `M1-STATE-005` is openly marked **OPEN** —
*"No configuration yet meets `M1-PLANT-009` and `M1-PLANT-011` together… the open engineering
problem of this specification"* — so choosing 2 accepts that the tree sits short of `257` while
that is worked.

**The deeper reason S-STRAIGHTLINE was ever provisional**, from `M1-SCOPE-004`: v12's numbers
were reached by *"compensating for an underpowered plant, and the goldens then froze those
compensations in place."* Thrust doubled (`9000 → 18000`) and the G limit doubled (`16 → 32`),
and the dials were never re-derived. S-STRAIGHTLINE is one of those compensations.

### ⚠ AMENDED 2026-08-03 (loop pass 4) — three corrections forced by `flying_architecture`'s `V021`

**1. "A" and "2" are labels from two different option sets, and this entry did not say so.**
`A/B/C/D` are from `docs/consults/EAGLE-ITER17-QUESTION.md` §4 (written by this agent).
`1/2/3/4` are from the **eagle's own** question to Chad, built on its
`docs/ITER17-MEASUREMENT.md`. Chad's two utterances — *"I chose option A"* and *"I went with 2"*
— answer **different documents**. The substance of the supersession is unchanged, but a bare
option label is **not an identifier**. Registered as a standing law in `AGENT-GRAPH.md` §2c.

**2. The Option A entry is superseded IN PART, not in full.** Its **control-arm warning is not
retired**: the v12-through-the-eagle-bench comparison was never run, and the eagle has since
shown it is **unrunnable by that route** — Euclidean vs spherical plant, no C++ build on that
box, and reporting a number from it would be the `SPEC-ACC-039` echo failure. A bare
supersession pointer would have killed that warning by implication, which is the reverse of
what happened. The warning stands and is now known to be unsatisfiable **as originally stated**.

**3. The record failed to establish that Chad's selection was informed — the defect is the
record, not the act.** `V021` correctly observes that *"what was selected is not what is being
built"*: Option 2 **as the eagle framed it** was the `yaw_scale`/`K_coord` trade, and this agent
substituted `M1-STATE-005`'s bank/elevator direction. `V014` says a mechanism is not an agent's
to pick that way, and on the artefacts alone that flag is correct.

**The missing fact, recorded here because nothing in the tree carried it:** the bank/elevator
correction was **put to Chad before he selected**, in the advisory that his *"I went with 2"*
answered — under the heading *"One correction to option 2 as the eagle framed it,"* citing
`M1-STATE-005` verbatim and stating that `A4`/`A5` are *"evidence the cause is reachable, not
the fix."* He selected **2-as-corrected**, having read it. That is disclosure before selection,
not substitution after it. **But an auditor reading only the tree could not know that, and was
right to flag it.** An informed selection whose record omits the disclosure is indistinguishable
from an agent's substitution — so the disclosure is now part of the record.

**Status:** ruled; dispatched at `docs/consults/EAGLE-ITER17-VERDICT-2.md`. The Option A entry
below is **SUPERSEDED IN PART** — its option choice and propagation clause are void; its
control-arm warning survives. Retained for its reasoning trail, not as standing guidance.

---

## 2026-08-03 — ~~EAGLE ITER-17 RULED: **OPTION A, ungate S-STRAIGHTLINE from `blend`**~~ — **SUPERSEDED the same day by the Option 2 entry above. Retained as history, not guidance.**

**Chad's ruling, verbatim:** *"I chose option A for the iter17 - question."*

Option A, as put to him in `docs/consults/EAGLE-ITER17-QUESTION.md`: *let the straight-line
correction work in FINE too, so the nose holds its line through the whole transition.*

**What the ruling is founded on (measured, not asserted):** the leading `blend *` that makes
S-STRAIGHTLINE structurally zero in FINE is **in the v12 spec** (`SPEC-LINE-003`,
`TESGI-SEADS-KERNEL-SPEC-v12-MIRROR.md:901`) **and in the shipped C++ kernel**
(`reference/seads-feel/control/controller.cpp`, identical expression). The eagle did not
introduce it; it ported v12 faithfully and thereby exposed the gap. It became visible only
because eagle iter 16 fixed the lean law — before that `held_bank` never left 0, the aircraft
tracked small offsets flat on rudder, and nothing banked in FINE.

**⚠ THE FACT CHAD DID NOT HAVE WHEN HE RULED, recorded because it is material and because he
may want to revisit:** the blend-gated form is **flown and approved**. On 2026-07-30 he flew
S-straightline and said *"yes I really like it. This is now the baseline for a quality flight
kernel."* That is the v12 seal and it is what `COMS-1`'s truth-check cleared on. **Option A
therefore is not repairing a mechanism he rejected — it is changing one he accepted.** Raised
to him the same session; the ruling stands unless he says otherwise.

**Do NOT conflate two instruments.** The `0.92/1.08/4.50°` figures in the 2026-07-30 entry are
**line-closure under 15/30/60° flicks**. The eagle's bar measures **dip depth**
(`asin(nose_elev@entry) − asin(min nose_elev)`, `SPEC-PRED-007`), whose v12 reference is
**`0.58°`**. Different quantities; an earlier draft of this entry treated the 30° closure of
`1.08°` as if it corroborated the eagle's `1.030°` dip. It does not.

### Scope of the ruling — this is the load-bearing part

1. **A is implemented in the eagle**, whose provisional goldens are explicitly unblessed and
   whose `line_hold_ff = 0.0` kill-switch is a structural off arm. Cheap and reversible.
2. **A does NOT propagate to `feel/kernel-v5` on this ruling.** The C++ mechanism is
   flown-approved and sealed at v12; changing it requires Chad's stick, not a bench bar. That
   is a separate gate and a separate ruling. This repo is read-only to that tree regardless.
3. **The §3 diagnostic still runs**, now as *validation* rather than as a gate on the ruling:
   the eagle's `1.030°` is a **synthetic bench** number and v12's `0.58°` was **flown**. If the
   bench scenario enters the transition harder than a hand can, A may be tuning against an
   artefact.

### The risk A carries, named so it is checked rather than discovered

`blend` is not what enforces the non-cancellation rules — `SPEC-LINE-006..009`'s one-sided
`w_dn`/`w_up` parasitic definition does that, and it survives ungating. What `blend` plausibly
buys is **suppression of pitch activity during fine tracking**. Removing it may therefore trade
dip depth for **pitch chatter in FINE**. The eagle must report `BAR-SMOOTH-PITCH` (and YAW/ROLL)
**alongside** `BAR-STRAIGHTLINE-DIP`, not the dip bar alone. A run that fixes the dip and moves
a smoothness bar is not a pass.

**The exact form of the ungate is the eagle's to derive and state, not this agent's to pick** —
delete the factor, floor it, or arm a FINE-scoped weight. Whatever it chooses, `SPEC-LINE-003`
is being deviated from and the deviation must be registered in `EUCLIDEAN_DERIVATION.md`'s
manner: derived openly, marked as a claim, never absorbed as a fact (`SPEC-SCOPE-018`).

**Status:** ruled, dispatched to the eagle at `docs/consults/EAGLE-ITER17-VERDICT.md`. The bar
stays at `< 1.0°` — pre-registered, and `SPEC-ACC-004` forbids moving it to fit a result.

---

## 2026-07-31 — FLY-CARD TEMPLATE AMENDED (Chad's word): every card opens with a FREE WARM-UP before instruments

Chad's ruling, verbatim in the eagle ledger (same-day entry there): *"always fly a
bit first to practice regular feel and the manouvers you will do before pressing f8
and going through the card becasue you can feel if something is off before you make
judgements on what is being measured."* **The template's card order is now: warm-up
(ordinary flying + dry runs of the card's own maneuvers) → instruments on → judged
rows → verdict.** **Why:** the pilot's feel is the first instrument and runs before
the measured ones — founding evidence is the 2026-07-31 eagle camera regression,
caught in free flight BEFORE the E1.2 card could be contaminated (Chad refused the
flight; STOP ledgered). This slots beside lesson 2 (instruments must model both
hands): the warm-up is the check that the whole rig — build, camera, feel — is the
one you think you're judging. **Status:** standing, federation-wide (both vessels).

---

## 2026-07-31 — THE MANDALARK MANIFESTO ADOPTED (Chad's word): `docs/MANDALARK_MANIFESTO.md` is the constitution's raison d'être + mode d'emploi

At the close of the eagle adjudication session, Chad ruled his founding intent into
the record: the Mandalark Gaming Engine's chronicle, with `mandalark-kernel` as its
basis and this repo as the proxy engine. His words verbatim in the manifesto file;
headline: *"proper sops and primary source data for information, testing hyposthesis
then reasoning from data is the true anti slop."* The manifesto distills the
operating law already paid for in these ledgers (primary sources over echoes;
instrument before ranking; pre-registration with the unexpected outcome included;
founder's words as specification; two-agent domain-separated verification; honest
supersession; character-is-a-ruling; the repo as proxy engine). **Standing:** the
manifesto governs spirit; narrower procedural rules win on mechanics. Amendments by
Chad's word only. **Why:** the methodology was named by its owner while watching it
correct its own adjudication echo against the archived artifact — the record of that
session (DECISIONS-evc-eagle.md, 2026-07-31 entries) is the manifesto's founding
evidence. **Status:** standing.

---

## 2026-07-30 (late night) — CONSULT: the eagle receives v12 — reply issued, EAGLE REGISTERED as a governed vessel (Chad's R7)

The EvC2026 eagle engineer's consult packet
(`D:\EvC2026\docs\CONSULT-KERNEL-V12-RECEIVE-PACKET.md` @ `166bd58`) was answered in
full (asks A–G; reply relayed through Chad). Highlights on the record: the v12
free-look release contract clarified — **CQ2 is a mouse→aim SUSPENSION (0.30 s of
dropped deltas), not a camera ease** — their STAGE-2 sketch confirmed with the CQ2
addition; the camera lead law stated (`[camera] lead = 1.0` toward the AIM,
deflection-scaled lag) with the categorical keys ruling (no key fallback — the
S-keychase class, excised upstream); the pole-free mechanism stated (the aim's OWN
carried frame; camera a pure consumer; their camera-basis attribution endorsed as the
S7 family); **`line_hold_ff` identified as the eagle's ready-made character knob**
(0 = v11 curl, 1 = plane-straight — the per-aviary doctrine clause executed as a
dial); the rung-D stale-envelope method + compensation-decay audit list issued for
their bottom-end work; the C/D split ordered MEASURED before ranked (our own
attribution-flip scar, applied outward). COMS-1 confirmed as a PLANE promise — the
eagle's player promise will be its own COMS entry.

**R7 executed: `docs/DECISIONS-evc-eagle.md` created** — the eagle's vessel ledger in
this repo, seeded with Chad's R1–R7 batch, the CS-registry sync rule, and the adopted
governance conventions (annotated eagle-v1 retro-seal, four-file goldens with
BuildStamp identity, fly-card template, pre-registered decision rules).

---

## 2026-07-30 (night, latest) — S-STRAIGHTLINE FLOWN AND APPROVED: "This is now the baseline for a quality flight kernel." Seal word GIVEN (→ v12) + Golden #5 ordered

**Chad flew S-straightline (build `ecbf51c6a`, pushed pre-fly per his precedent) and
approved. Verbatim:**

> "yes I really like it. This is now the baseline for a quality flight kernel. This v11
> is the standard. Lets seal it and fly the golden."

**Numbering note (the felt_flight_18 class, resolved the same way):** "this v11" refers
to the build in his hands, which is sealed-v11 PLUS S-straightline — the new seal is
**v12**. Flagged to Chad; carried as v12 unless he rules otherwise.

**What the verdict closes:** the tracer-line dip thread (attributed by instrument to the
crab's parasitic vertical component, fixed by the axis-correction FF, closures
0.92/1.08/4.50° against 2.23/5.05/8.31°), the G-bite question (approved implicitly in
"I really like it" — the honest high-G entry is part of what he is calling the
standard), and **COMS-1's truth-check: SATISFIED ON THE STICK** — "your tracers go where
you pointed them" is now true as a player will experience it, with the known-limits
ledgered (endgame droop, top-rudder window, push-branch kink). The rendering itself
still awaits Chad's wording approval — truth and copy-approval are separate gates.

**Pipeline armed on this side, executes when the v12 seal tag lands:** verify tag +
ls-remote → re-snapshot `reference/seads-feel/` at the flown seal → VERSIONS row →
DECISIONS mirror → handoff. **GOLDEN #5 ordered by Chad's word** — first golden on
recorder v2 (native `telem_blend`/`telem_held_bank` pins). Checklist additions paid for
by this arc: spawn-altitude check on the card (the felt_flight_17/felt_flight_3 class,
two occurrences); tracers as a flown instrument (note in the card what the pilot was
watching); candidate NEW predicate for #5 — a straight-line/dip-depth predicate derived
from nose-elevation pins under flick, now derivable because the boundary state is on the
tape. Identify the tape from its own data; file counter ≠ golden number.

---

## 2026-07-30 (night, later) — S-STRAIGHTLINE ATTRIBUTION OVERTURNED BY THE INSTRUMENT: the dip is the CRAB's parasitic vertical component, not gravity sag. Axis correction approved with a compressed re-audit

**Commit 1's phase-resolved instrument falsified the plan's physics before any mechanism
landed:** the gravity-deficit FF closes 2–4% of the measured dip (2.23→2.20, 5.05→4.92,
8.31→7.93) — gravity can source ~0.2° of a 5° dip. The measured mechanism: the rudder
sweeps the nose toward the aim at up to ~55°/s about the BODY yaw axis; once banked,
sin(bank) of that sweep points down (~20°/s of nose-drop at 50° bank). The dip is the
crab steering the nose downhill because its axis is bolted to a banked airframe.

**The fix shape (Chad ruled: proceed):** axis correction — `pitch = −yaw·sinφ/cosφθ`,
exactly the pitch that makes the nose sweep to the aim about LOCAL UP instead of the
banked body axis (math verified: zeroing the net vertical component of the yaw sweep
requires q = r·tanφ, fold-safe form as stated). Same dial, same gates, signed (also kills
the reversal kink); a commanded dive comes through the pitch demand and is untouched.
Trade, stated honestly: full cancellation on a hard flick is a genuine high-G level pull
(the real-airplane behavior); very large flicks keep an envelope-bounded residual.

**The prior audit's approval does NOT carry over — compressed re-audit required on the
changed core, four conditions issued:** (1) the correction must key on the EMITTED yaw
(post yaw_gate/blend), never the raw demand — else it cancels a yaw that isn't being
commanded (the P1 trap); (2) the C2d-waiver rationale must be REWRITTEN — "aim-
independent coordination" is now false; the new bound story is structural
(≤ emitted-yaw·tanφ, knife-faded, AoA/G envelope as the wall), stated on its own terms;
(3) G-bite promoted from sentinel to HEADLINE fly-card condition; (4) oracles re-derived
+ per-flick closures re-registered, envelope residual surfaced to Chad pre-fly.

**Lesson (the discipline's best day):** the gravity story was correct PHYSICS wrongly
ATTRIBUTED — it survived plan-mode, a fresh red-team, and this ledger's audit (which
verified the formula but could not test whether it was THE mechanism). Only the
pre-registered instrument killed it, before a fly was spent on a card that would have
failed. Verifying a formula is not verifying an attribution; only measurement closes
that gap. Instrument-first is now three-for-three.

---

## 2026-07-30 (night) — AUDIT of the S-straightline plan (line_hold_ff, bank-coordinated pitch feedforward): APPROVED, no blocking corrections — ⚠ SUPERSEDED ABOVE: the plan's physics was falsified by its own commit-1 instrument; the approval's physics section and C2d-waiver rationale are VOID

The plan was audited against this ledger and the v11 snapshot. **Physics verified by
independent re-derivation:** the FF magnitude `(g/V)·(1−c²)/c` is the exact body-pitch
component of a coordinated level turn's angular velocity (ω = g·tanφ/V vertical, pitch
component ω·sinφ), and it is the pitch-axis SIBLING of the flown coordination yaw demand
— not a new term class. Claimed magnitudes reproduce by hand (0.81°/s @30°, 4.2°/s @60°,
knife-faded ~13°/s ≈ 4.6 g @85°, V200).

Notable audit findings, all sound: (1) the deliberate NO-w_push-cap deviation from the
C2d scar is correctly reasoned (w_push ≈ 0 on a pure lateral flick would neuter the FF at
zero sag; C2d targeted pointing-law helpers, this is aim-independent coordination) with
replacement bounds named and the dive_gate anti-climb closure comment REQUIRED so a
retune can't silently reopen it; (2) the FF↔servo composition satisfies the stated
condition (feedback-plus-feedforward, servo structurally silent at sag=0, pinned by
two vacuity-tripwired legs); (3) all four of Chad's plan-mode rulings baked (roll-in
wins / crab enough / keep-gate-add-FF / COMS-1 on the stick). **The honest crux,
pre-registered in the plan:** deep flicks commit to bank angles where the FF is
knife-faded — commit 1's phase-resolved instrument measures how much of the 8.3° @60°
dip lives out-of-reach BEFORE the mechanism lands, and the residual is surfaced to Chad
before he flies if the roll-in share is < ~half. 15/30° closures expected near-full.

Minor asks (relayed, non-blocking): exact gate-count pre-registration before each run
(lesson 4); Chad's spec sentence verbatim on the fly card.

---

## 2026-07-30 (night) — v11 SEALED: S-rollmix (the 5–10° slam thread CLOSED), sealed on Chad's word after the flown verdict

**Sealed `flight-kernel-v11-2026-07-30` @ `0602d8292`** (annotated tag `20f14817c`,
pushed, verified by this agent local + ls-remote). Content over v10: S-rollmix
(`roll_target_mix = 1.0`; 0.0 = bit-identical v10), the blend/held_bank instrument +
recorder v2, SPEC First Principle 5 (P-helm), and the S-straightline stub with the COMS-1
stake. Gate 395/395. Grafted to recon `01b28231a` same night — gate 905/905, zero golden
movement, recorder v2 threaded through recon's TickHook seam (control::Telemetry through
the hook typedef → step_frame → main.cpp), so **every future F9 tape carries blend and
held_bank natively**; build-play re-stamped on sealed v11. The graft session also caught
the stale-exe trap live (build failed while ctest passed on the old binary — caught,
fresh relink, clean re-run): the relink lesson is now twice-paid.

This repo's seal pipeline ran the same night: `reference/seads-feel/` re-snapshotted at
`0602d8292` (no purity exceptions — the snapshot README states it), VERSIONS.md seal row,
this entry, handoff updated. **No golden flight for v11** — the S-rollmix fly was a card
fly, not a golden session; goldens #1–#4 stand, none superseded. The next golden (#5)
should wait for a natural seal-flight occasion and will carry the new telem pins natively.

**Open after the seal:** S-straightline (consult packet pending — its five questions aim
at the rung-C2 align_f floor scar tissue, the verbatim pre-July-6 "pushed up in a turn"
failure, and whether MB-rud's crab already satisfies the rudder-leads clause of Chad's
spec). COMS-1 stays PENDING S-straightline. Horizon-gate stays parked-pending-recurrence
(recurrence A/Bs must set `roll_target_mix = 0.0` for the #4-equivalent baseline).

---

## 2026-07-30 (night) — STANDING INTENT: the mouse-helm comfort doctrine (Chad's words — quote, never paraphrase)

**This is not a mechanism ruling — it is the stated PURPOSE the mechanism rulings serve,
given by Chad the night the dip was ruled a flaw. It grades every future feel thread.
Verbatim:**

> "This ruling is available to me becasue I am being honest about how it feels. I wanted
> to adjust to the character (and that might be an actual feature in another aviary game
> such as an eagle flying game). But for what I learned about coordinated flying when I
> was learning simflying is how to keep my aim steady depends on the flying I am doing.
> So you be lining up my aim while trying to intuit a dip and rise as I am also moving
> laterally is more to process. A smoothly controlled plane's nose will follow a straight
> line to its target. Feeling comfortable at the mouse helm will be something I want to
> offer my players so that they might better focus on situational awareness, strategy,
> tactics and their required manouvers in realtion to the flying adversary. I want to be
> undpedictable to them not myself. :)"

**What this settles, permanently:**
- **The flaw-vs-character test now has a stated purpose behind it:** kernel quirks tax
  the pilot's attention budget, and that budget belongs to situational awareness,
  strategy, tactics, and maneuvering against the adversary — never to compensating for
  the plane. "Unpredictable to them, not myself" is the one-line form; use it.
- **The straight tracer line is doctrine, not preference:** a smoothly controlled
  plane's nose follows a straight line to its target. Deviations exist only when the
  pilot commands them ("Curver are nice, but I can decide what and when").
- **Character is a per-game (and per-vessel) design choice, not a kernel default** — his
  own example: dip-and-rise might be a genuine FEATURE in an eagle-flight game. This is
  the intent-level root of the A6M2 pattern (re-arming parked mechanisms as character
  dials for other vessels): the KERNEL stays clean; character is added deliberately,
  per aviary.
- **Why the honesty discipline pays:** the ruling was "available" only because the felt
  report was honest. The verbatim-words rule, the flown-table rule, and the
  identify-from-data lessons all serve this — the ledger exists so intent like this can
  be reached, stated, and kept.

---

## 2026-07-30 (night, later) — RULING: THE DIP IS A FLAW. The 2026-07-06 roll-first spec is SUPERSEDED by Chad's own refined law: pull arrives WITH the bank, not after it

**Chad's ruling, verbatim (this is the new thread's specification — quote it, never
paraphrase it):**

> "the dip is a flaw, whether pre existing or not. I feel that a normal flight would
> apply, rudder first to lead the bank a little before the elevator but I feel the
> elevator increase should smoothly coorelate to banking increase. they are both supposed
> to align to the final turn angle at the saem time is what I hypothesize. I think we
> need a new thread to get this right as we are so close to getting it right why stop
> now. The line that my tracers draw should be straight and my gunnery should have a
> predictable path via my control surface operation not accept 'character' that I need to
> compensate for. A straight line is the shortest distance, a dip is a delay. ... Curver
> are nice, but I can decide what and when that looks like via inputs rather that
> adjusting to quirks."

**Both agents were asked flaw-or-character; this ledger's answer: FLAW — a bug in the
SPEC, not in the code.** The mechanism (`bank_align_power = 6`, S7-turn2) is a correct
implementation of the 2026-07-06 ruling "roll first, don't get pushed up in a turn."
Chad's flying has now refined that ruling: the two documented failure modes are
pull-BEFORE-roll (power=1: the climb he ruled out in 2026-07-06) and pull-AFTER-roll
(power=6: the dip, measured 2.23/5.05/8.29° at 15/30/60° flicks). The new spec is the
third shape neither implements: **pull grows WITH the bank, both arriving at the final
turn angle together** — the elevator phased to bank PROGRESS, not gated on
bank-error-to-target. Rudder-leads is already flown-in (MB-rud, "nose should crab
immediately and bank immediately") and is consistent with his "rudder first" clause.

**Why FLAW is the right answer by this project's own standards:** (1) his words are the
specification, and the ruling is explicit; (2) the mission standard — this is a GUNNERY
kernel, and a trajectory quirk the pilot must compensate for during a firing solution is
a cost paid every fight ("my gunnery should have a predictable path"); (3) the player-
authority principle every prior ruling upholds — curves on command, never as furniture.
"Character" is what you keep when it doesn't fight the mission; this fights the mission.

**Supersession bookkeeping — CORRECTED BY CHAD, same night.** This ledger first filed the
supersession as "a spec refinement, not a compensation decay." Chad overruled that
reading with the history only he carries, verbatim:

> "The previous ruling were often the compensation of an underpowered plant, and I was
> having some pole lock so my ruling in this case may have also been a compensatory
> measure without the proper solution."

So the 2026-07-06 roll-first ruling is the **FOURTH flown instance of the
compensation-decay law** (R6): (1) yaw_scale 2.2→2.0, (2) Fly-13's crabbing preference,
(3) the cue-ball retirement, (4) bank_align_power's hard roll-first gate — each a dial or
ruling tuned against a low-authority plant (and, here, the pole-lock era) that became a
bias once authority rose. The law's predictive corollary strengthens accordingly: **when
a v10-era behavior traces to a pre-power-increase ruling, audit whether the ruling was
compensation before treating it as preference** — and the A6M2 (low-authority vessel)
brief inherits the converse: the roll-first gate shape may be worth RE-ARMING there as a
character dial, exactly like the parked capture machine.
The new thread is mechanism-shaped (a gate re-key, not a dial walk: power-down on the
same cos(bankErr)^p law only interpolates between climb and dip — it cannot express
simultaneous arrival). Belongs to seads-feel plan mode with the usual pipeline: consult
→ plan → audit → instrument-first if a new pin is needed → one dial → Chad flies.
S-rollmix's own disposition (seal word, recon graft) remains pending and SEPARATE.

**Chad flew S-rollmix (build fc3ea6970 card, tip 39aa52a48) and the thread's symptom is
gone. Verbatim:**

> "its interesting it definately feels smooth, I tried firing my guns (tracers) while I
> flew. While it definately feels buttery, and there isnt rebounding, one thing I did
> notice is that there seems to be a dip smaller if I move more gradually but If I do a
> flick there is a kind of a dip curl. I tried with tracers it goes staright at first but
> its like as its waiting for full bank there is a bit of a lull in the z height gain
> (first part of the shallow dip, then it reaches the full bank and then the elevator
> kicks in and you get the upswing of the dip it forms with viewing the line of tracerrs,
> ---> straight, shallow dip --> straight The dip is a shalllow us shape. Now im not
> saying its is really bad but it is there.."

**The slam verdict:** smooth, buttery, no rebounding — the 5–10° thread's reason to exist
is answered. (Note the instrument: he flew with TRACERS as a trajectory readout — a new,
sharper felt instrument than any prior fly, worth reusing on future cards.)

**The dip observation — kernel-base attribution hypothesis (pre-A/B, stated before the
number so it can't be rationalised):** the flick dip is almost certainly PRE-EXISTING v10
character, not S-rollmix — flicks past blend_hi are bit-identical by exact guard, golden
blend==1 prefix, and the equality legs. The geometry is the composition of two of Chad's
OWN flown rulings: (1) `bank_align_power` 3→6 (S7-turn2, 2026-07-06, "roll first, don't
get pushed up in a turn") holds the elevator ~0 until the bank aligns — the lull in
z-gain; (2) the S-holdline sag servo (rung C2b–d, "hold the line of my mouse inputs")
fights proportionally-from-zero once the nose falls below the aim's elevation line —
which is WHY the dip is shallow, not deep; then align kicks in → the upswing. Straight →
shallow U → straight is that handoff's tracer signature. Gradual = smaller dip is
consistent: through the band the bank deepens progressively (S-rollmix), so alignment is
already partial when full commit arrives — less pull-hold to pay.

**Discriminating A/B (30 seconds, pre-registered):** `roll_target_mix = 0.0`, same flick,
same tracers. Dip persists → v10 character confirmed; dip vanishes → the hypothesis is
WRONG and S-rollmix owns it (STOP and re-attribute — flicks were supposed to be
bit-identical, so this outcome would also indict the equality legs). If confirmed
character and Chad wants less dip, the honest dial is `bank_align_power` (lower = pull
sooner = shallower dip, but re-admits some of the "pushed up in a turn" climb he ruled
OUT — a feel trade only he can re-rule; `pull_floor` is already 1.0 = full servo). That
would be its own thread with its own card — not folded into this one.

**A/B RESULT (added same night — the pre-registration above is CLOSED, hypothesis
CONFIRMED):** the seads-feel session had independently run the offline half of exactly
this A/B before the hypothesis arrived (convergent attribution from two directions —
this ledger's reasoning from the bit-identity proofs, theirs from nose-elevation traces).
Lateral flicks 15/30/60° at V200, `roll_target_mix` 1 vs 0: dip depth **2.23 / 5.05 /
8.29°, identical in both arms to three decimals** (the mix arm 0.02° *shallower* at 15°).
Verified from the artifact: live-tree ledger row `291dc5314`, flight-log table read
directly. **The dip is v10 character** — the S7-turn2 roll-first trade, flown since the
seal, drawn visible for the first time by the tracer instrument. NOT an S-rollmix
regression. Both dispositions rest with Chad: (1) S-rollmix seal word → recon graft +
recorder-v2 port; (2) dip = flaw-or-character ruling (if flaw: `bank_align_power` down,
own thread, own card, revisits his own 2026-07-06 ruling).

---

## 2026-07-30 (later) — AUDIT of the blend-band roll-continuity plan (two commits, roll_target_mix)

The seads-feel session's plan was audited against the sealed v10 snapshot and this ledger
(relayed through Chad). **Verdict: sound and consult-compliant — approved with three
corrections.** Verified true in the snapshot: the composite `roll = blend*roll_maneuver +
(1-blend)*roll_hold` with roll_maneuver ungated, while pitch and yaw both already carry the
`(1-blend) + blend*gate` continuity composition — the "roll never got the fix" framing is
real. No threshold moves (sidesteps AT-15 and the center_band loader wall); lean gate
unmoved; knob structurally off; golden re-record per the carry=0 pre-stated-bar precedent.

Corrections issued:
1. **BLOCKING — acceptance set:** recverify must run on the four canonical sealed goldens
   in THIS repo (`goldens/golden_{1..4}_*.seadsrec`), by explicit path, read-only — not on
   recon build-dir copies (the lesson-7 error shape).
2. **Horizon-gate A/B confound:** after the mechanism lands, a recurrence A/B against
   Golden #4 must set `roll_target_mix = 0.0` to reconstruct the #4-equivalent baseline
   (the disclosed below-nose side-scope is adjacent geometry). Stated on the fly card.
3. **Disposition on rejection:** categorical rejection = revert to a branch per the
   S-keychase/S-aimclamp/S-retclamp precedent, never a live knob parked at zero.

Minor: hand-verify the e_lean sign against the +K_phi·roll_hold_demand vs
−sqrt_law(bank_eff) asymmetry (author the oracle leg from a hand-computed sample); exact
gate counts pre-registered before the run; v2 reader keys has_telem off the header tag,
not per-line stream state; Chad's ruling goes on the card verbatim.

---

## 2026-07-30 (later) — CONSULT: prior-advice issued for the 5–10° blend-boundary roll slam thread

The seads-feel session requested the kernel base's memory before entering plan mode on the
one active feel thread (consult packet relayed through Chad; reply relayed back the same
way). PRIOR-ADVICE ONLY — no design ruling made. The reply's substance, so the record does
not live only in a chat transcript:

1. **Binding rulings found:** `blend_hi` 12→9 is a flown Chad ruling ("roll a little
   sooner", 2026-07-06) — widening the band back is ruled-against absent a new word;
   `bank_align_power` 3→6 ("roll first") couples the boundary's roll shape to a second
   flown ruling; `lean_max`-alone is already refuted (v10 entry); capture stays parked.
2. **The largest prior: MB-lean is the prior flown fix for this exact class** — its own
   comment block says it was built so a moderate lateral aim holds a shallow bank
   "instead of the blend band slamming to the ~90 deg bank_error and auto-leveling
   back" (Chad's "magnet" approval, 2026-07-08). The 5–10° slam is the same defect
   surviving outside MB-lean's `err < blend_lo` gate. The thread is coverage-completion,
   not a new mechanism.
3. **The frozen-lean gate is load-bearing twice** (pole-freedom of the de-roll azimuth;
   rest/righting/ENGAGE semantics) — advice: do not move it; add continuity on the
   MANEUVER side (bank-to-turn's target approaching the lean target as blend→0).
4. **Scar tissue favors shrinking the target gap over hysteresis** (hysteresis cannot
   close a tens-of-degrees target disagreement; the knee would track the band edge per
   the pre-registered §A8 falsification; MB-lean is the flown win for target
   continuity). Proportional-from-zero, no floors (the S-holdline AT-12 slingshot).
5. **Exposure beyond AT-15:** the loader wall `center_band <= blend_lo/2` (blend_lo < 3°
   refuses to load; couples to the separate center_band fly); the parked capture
   machine's ENGAGE gate and its 28 self-armed legs key on blend_lo; rest/righting legs
   key on both dials; controller goldens load the shipped config so a deliberate move
   needs a pre-stated bar + pre-registered gate count (the carry=0 precedent); Golden
   #4's smoothness predicate (< 1.1/s body-rate reversals) is the felt regression bar
   the fix must not spend.

---

## 2026-07-30 (later) — HORIZON-GATE thread RE-STATUSED: PARKED-PENDING-RECURRENCE (did not reproduce on the sealed tape)

**The live ledger's final word for the day (`cdfa7b0b4`, seads-feel flight-log) re-statuses
the horizon-gate nose-referenced-arm thread from "queued behind blend-boundary" to
PARKED-PENDING-RECURRENCE.** On the Golden #4 tape (felt_flight_18, canon v10) Chad could
not produce a rollover — "The knife edge held on pitching up down manouvers" — and both
steep-down entries held on the pins (`cos_phi_theta` ≥ +0.475; see golden_4_TELEMETRY.md).

**Why parked, not closed:** the 7 pin-derived rollovers on earlier (unpromoted) tapes
remain the documented gap — one clean tape does not un-document them. But the live agent's
principle is right and adopted here: **do not design against a symptom that no longer
reproduces.** Candidate explanations, all noted UNTESTED: the retired capture machine's
arrival dynamics (carry=0 landed at v10), lean 8's bank channel, or maneuver-geometry luck.

**Re-entry condition:** first recurrence. The before-state tapes are preserved, and Golden
#4's clean segment B is the ready-made A/B against any recurrence tape. The fix work, if
re-opened, should request a dedicated recording set (stated in golden_4_TELEMETRY known
limits).

**Net thread state after this ruling: ONE active mechanism thread** — the 5–10°
FINE↔MANEUVER blend-boundary roll slam (plan-mode, mechanism-shaped). This supersedes the
"two threads queued in order" line in the v10 seal entry below.

---

## 2026-07-30 — v10 SEALED (buttery cascade): pool-ball capture RETIRED-PARKED by ruling; two supersessions; the compensation-decay law

**Sealed `flight-kernel-v10-2026-07-30` @ `f86ee7b9f`, gate 391/391.** Chad flew the exact
dial values (as recon's logged FLIP-2 table, byte-matched to the commit) and approved the
buttery small-deflection feel before ruling the seal. Three dials moved this session:
`lean_gain` 6→8 (rung 1, flown partial, kept), `side_cone_enter/exit` 27.5/32.5→37.5/42.5
(rung F — a lineage heal, not a tune: the sealed table had diverged from the table Chad
actually flies), and `[capture] carry` 1.0→0.0 (the headline, below).

**RULING — the pool-ball capture machine is RETIRED, PARKED NOT DELETED.** Chad, verbatim:

> "I think we can park the cue-ball behavior and defer it to a future modification if I
> need it. I think the plant and flight control authority as it is now gives me sufficient
> closing ability to intercept an opponent diverging away from my gun solution, especially
> with the powerful elevator at full bank in this kind of situation. So park the cue ball
> behavior as retired for now."

The pinned attribution: the machine's center-carry caused the small-deflection
rudder/elevator flapping (sick-state 6.24 / 3.27 reversals/s; carry=0 predicted ≲1% and
Chad's A/B confirmed on the stick — "the pool ball feel is gone"). Machinery kept whole
behind 28 self-armed test legs; a silent re-arm fails the loader pin loud. **Re-entry
condition:** closing ability against a diverging gun solution degrading.

**SUPERSESSION — Fly-13's "crabbing? yes!" preference is reversed by Chad's own ruling:**

> "This ask was before I got the power and flight control authority. The more rudder was
> an attempt to get the nose to meet the mouse aim centre more deliberately. Now that the
> power has increased I want to try to balance the banking back in… My preference has
> evolved and I want to see if it is better with a more balanced approach with less
> crabbing."

`center_band` stays 1.5 at this seal — the shrink is its own future fly, now with a real
ruling behind it (it was previously blocked for lacking one, not for being wrong).

**THE COMPENSATION-DECAY LAW (three flown instances, now predictive):** dials and
mechanisms tuned against a low-authority plant become biases when authority rises —
(1) `yaw_scale` 2.2→2.0, Fly A: "the old yaw bias was partly compensation for a slower
plant"; (2) the Fly-13 crabbing reversal above; (3) the cue-ball retirement above. First
predictive use: the A6M2 (low-authority vessel) brief inherits "evaluate re-arming the
parked capture machinery (carry=1) as a character dial before inventing anything new."

**Parked with conditions:** `lean_max` 30→40 (the shelf hypothesis was REFUTED by the
pins — the 5–10° bounce is the FINE↔MANEUVER blend-boundary roll slam, pre-dating rung 1;
mechanism thread queued). Side cone 45/50 (superseded: the knife-edge inversions occur at
aim ~22° above horizon, ~2° off-plane — no cone reaches them; needs a nose-referenced arm,
mechanism thread queued behind the blend-boundary one).

**Lesson 8 — read the flown table / fly the identified build.** Twice in one day the
graded artifact wasn't the assumed one (Card 1 flown on the recon build's old table; the
side cone flown at 37.5/42.5 while the ledger said 27.5/32.5). Every fly card now states
the flown build's actual dial values read from config at fly time, plus a pre-registered
felt tell for the build itself. Same principle as lesson 7, extended from recordings to
executables and tables: **identification comes from the artifact's own data, never from
the instruction that created it.**

**Status:** Golden Felt Flight #4 pending (slow-tracking smoothness predicate to be
recovered from the sealed tape's own pins; pitch-down set framed as the horizon-gate
BEFORE-state — documented-bad, not blessed). Mechanism threads queued: blend-boundary
slam, then horizon-gate arm.

---

## 2026-07-29 (night) — v9 FLOWN AND APPROVED: all three conditions pass; CQ2 window KEPT by ruling

**Chad flew v9 (`a307a8a69` build) and approved all three fly-card conditions. Verbatim:**

> "All 3 conditions are settled on this session. Instant snap on release of space — check.
> Keys dont override the cam and snap back to oblique view — check. Nose was always welded
> to the aim and vice versa in this kernel — it was the camera only that needed
> modification; the aim wedded to nose was a default behavior in freelook. Keys dont affect
> camera — check. We're all good on this one."

**CQ2 ruled — the 0.30 s easeback window STAYS. Verbatim:**

> "The .3 seconds mouse dead time is not noticeable as I need that .4s to observe / orient
> myself and am able to respond in time without noticing."

The window's mechanism rationale may be dead, but Chad has given it a *new*, current
rationale from the cockpit: it covers the orientation beat after the snap. It is no longer
a contradiction of "instantly re-established" — authority returns before he reaches for
it. **Do not flip it; do not treat it as debt.** If it ever surfaces again, this entry is
the ruling of record.

**Card-1 deflection trade: accepted as flown.** No sustain, no further mechanism. Closed.

**Chad's framing of what v9 actually was, worth keeping:** the aim-to-nose weld was the
kernel's default freelook behavior all along — *the camera was the only thing that needed
modification.* The fourth camera round succeeded when the fix finally matched that shape:
camera-only, one instant snap, aim untouched.

**Now unblocked (standing decisions permitting):** the seads-feel agent seals v9 and pushes
(branch was 6 ahead, unpushed); graft to seads-recon proceeds per its open item. On this
side, once the v9 seal exists: `reference/seads-feel/` may be re-snapshotted at it (it is
now a FLOWN seal), and Golden Felt Flight #3 can be scheduled on the sealed build.

---

## 2026-07-29 (evening) — v9 MEASURED AND LANDED: the pre-registered prediction was RIGHT, both terms were real

**v9 "S-nosesnap" is implemented and green** on `feel/kernel-v5` @ `a307a8a69` (gate
388/388, zero moved goldens, red-team SOUND-WITH-FIXES, no P0). **NOT yet flown by Chad;
not yet pushed to origin (tip is 6 ahead) as of this entry.** Commit order verified from
the live tree: rulings registered alone → instrument + v8 baseline → mechanism → mutation
hardening → docs → red-team folds. The discipline held.

**The measurement, graded against the pre-registration below** (v8 unchanged baseline →
after v9), from the new `comfort_freelook_release_keys` instrument:

| metric | v8 baseline | after v9 |
|---|---|---|
| `nose_at_fire_deg` (term A) | **18.552** — at its `aoa_max` cap | 0.578 |
| `vel_at_fire_deg` | 0.272 — proves the old velocity-referenced metric was blind | 17.956 |
| `nose_after_1s_deg` | 23.098 | **40.861** ⚠ see below |
| `nose_after_3s_deg` | 18.824 | 16.783 |
| `updebt_after_release_deg` (term B) | **44.904** | 0.003 |

**Verdict under the pre-registered rule: COMPARABLE — both terms real.** A capped and
persistent (~18.6°), B larger but transient (44.9°). The pre-registration's named
repeat-risk happened in the data exactly as written: A read ~18° and looked like solid
confirmation while B sat 2.4× larger. A threshold test on A alone would have bought a
fifth round. Both halves were implemented unconditionally per Chad's ruling anyway — the
rule ended up grading the diagnosis, which is what survives of it by design.

**Honest attribution, recorded as reported:** term B **predates v8** — S-keychase had been
masking a long-standing up-debt by re-anchoring forward. The up half of the felt oblique
was never a v8 regression.

**The one number that got worse — expected, and why:** `nose_after_1s_deg` rose 23.1° →
40.9°. This is the pre-named Card-1 trade, not a defect: post-snap, hard key-only turning
walks the nose away from the parked aim, and the lag camera follows the aim, re-opening a
~17°+ deflection view until the mouse takes over. The pre-registration's "honest reading
2" called this in advance: ordinary lag against a parked aim cannot stay behind a plane
turning on keys. **Chad's call after flying; the sustain stays dead unless he rules it
back.**

**Also retired, per the weld ruling:** no-keys mid-turn freelook drift 75.5° → 1.46°
(the carve is gone); out-of-a-loop orient rolled-world 178.7° → 0.000° (instant upright).
`comfort_mouseaim_keys` — the v8 win — bit-identical.

**Open on Chad (from the fly cards):**
1. **CQ2 easeback window** — for 0.30 s after any release, mouse deltas are dropped
   (`[freelook] easeback_time`, ruled 2026-07-03). This contradicts "mouse aim authority
   is instantly re-established," and its original rationale is reported dead twice over.
   If the first third-second of mouse feels dead after release, it's this window, not the
   snap. One-line flip, **waits on Chad's ruling**.
2. **Card-1 deflection trade** (above) — genuine physical tension, Chad's call.
3. Walk-back scope: `release_orient_with_keys = false` disables the with-keys snap ONLY;
   the true walk-back for v9 is the v8 seal tag `flight-kernel-v8-2026-07-29`.

---

## 2026-07-29 (later still) — RULING: in freelook the aim is WELDED to the nose, ALWAYS — the no-keys "parked carve" is retired

**Status: RULING. Chad's answer to the seads-feel agent's direct question ("nested always /
keep the carve / other"). He wrote option 3 himself. Verbatim, typo-fixes bracketed:**

> "In freelook, keys are the only means of aiming as the nose aim becomes welded to the nose
> directionality since mouse inputs now control camera during the freelook phase. Upon
> release the camera snaps back to chase and the mouse aim authority is instantly
> re[e]stablished. The camera is at that instant in chase view and is now subject to and
> dependent on mouse aim inputs, the nose following the cascade."

> "If I were to be mid turn and press freelook, then I am in freelook and my nose holds the
> last directionality it had before the spacebar press and hold (freelook). Then my mouse has
> no control over the plane and snaps around the nose. The nose maintains its heading and
> stops whatever input i[t] was giving it via mouse as the mouse can no longer influence the
> nose because we are in freelook. I have only the keyboard over[r]ide gross inputs to
> control the plane as my mouse is now controlling my camera for situational awareness. My
> nose is controlled by my careful qweasd inputs."

> "Releasing the space (freelook) allows the camera to snap back into alignment camera →
> tail → nose → nose indicator dot nested inside center of mouse aim... I now having released
> the space bar given back mouse aim authority and the nose and camera follow
> deterministically."

**What this rules:**

1. **Freelook ENTRY welds aim := nose — every freelook, keys or not.** The pre-freelook
   mouse-aim command stops driving the plane at the spacebar press. The nose *holds its
   heading*; it does not keep carving the old commanded turn. This retires today's no-keys
   "parked aim keeps the carve" behavior — a deliberate behavior change, ruled by Chad in
   answer to a direct either/or, not an incidental side effect of v9.
2. During freelook: mouse → camera only; qweasd keys are the sole control of the plane.
3. Release: mouse-aim authority is instantly re-established; the camera snaps to the chase
   alignment **camera → tail → nose → nose-indicator dot nested inside the mouse-aim
   center** (and upright, per the ruling below). The aim does not move — it was welded to
   the nose the whole time. Release must be a no-op on the aim **in all cases**, by
   construction, because entry did the welding.
4. This closes the code-vs-model gap flagged in the previous entry (§5b nesting observed
   only in the keys-held path): the nesting is now spec for ALL freelook, implemented at
   entry as a weld, not per-tick only when keys are held.

**Addendum (same day):** the guard stated the banked-release composition back to Chad —
camera on the tail line, rolled so the horizon is level — and Chad confirmed verbatim:
*"yes upright relative to the horizon and behind the plane is correct."* Together with
"Instantaneous. No need for anything else," the Step 2B question ("does snap-to-chase mean
upright too, and same-instant?") is **fully answered: upright, at the snap instant, behind
the plane.** The 2026-07-07 "eased" ruling is superseded for the freelook-release case. If
B dominates or is comparable, the seads-feel agent implements upright-in-the-snap — **it
does not re-ask.** Chad has now stated this three times.

**Second addendum (same day) — Chad removes the measurement GATE on the up fix entirely.
Verbatim:**

> "No, for 2B nothing to measure!! Use my words here not a previous ruling. Snap to view
> upon release of freelook, no eased anything as I need to immediately view the back of my
> plane, the aim, the nose, everything — making it lag there is going to disorient."

**What this rules:** the instant full snap — camera behind the plane, upright to the
horizon, aim and nose in view, in one un-eased step — is **spec unconditionally**. It does
not wait on the A-vs-B comparison; there is no "if B dominates" branch for it. This
partially supersedes the pre-registered decision rule below **by direct ruling**, which is
the one legitimate way to supersede a pre-registration: the rule existed to stop an agent
rationalising numbers into a preferred fix, not to stop Chad specifying the behavior.

**What survives of the pre-registration:** the instrument is still built first and the
baseline numbers still recorded on unchanged v8 — as *evidence* (before/after, and proof of
which term carried the felt oblique), not as a *gate*. The "both small → STOP and
re-attribute" arm survives for the forward term's diagnosis. The golden tripwire survives
untouched. v9 therefore implements BOTH halves: aim law (nose, unconditional weld) and
camera law (one instant snap — forward AND up together).

---

## 2026-07-29 (later) — CHAD CORRECTS THE v9 "LAW" QUOTE: the aim NEVER snaps, and the release snap IS upright

**Status: RULING. Amends the model statement inside the pre-registered v9 entry below. The
measurement rule itself (comparative A vs B, coupling, golden tripwire) is unchanged.**

The seads-feel v9 plan opened with a quote of Chad's ("Wherever the nose is pointing when I
release the freelook, I snap the camera and the aim…"). **Chad retracts that phrasing as wrong
on two counts, in his own words:**

> "It is w[r]ong precisely because I dont snap the camera and the aim... The aim is waiting
> for me nesting around the nose indicator circle. Freelook ensures that the aim and nose are
> nested together as one. Saying I snap the camera is incorrect. The camera is dependent on
> the aim so releasing freelook just snaps my camera to the aim deterministically, I am not
> controlling the camera to that directionality."

> "In freelook I am operating the camera with my mouse movement. My aim is in that freelook
> mode attached to the nose direction and are inseparable. The aim is waiting for me when my
> camera snaps based on the release mechanism from free look. Instantaneous. No need for
> anything else."

> "I should after releasing space (freelook) be looking directly at the rudder of my plane
> given my camera is now chasing the tail of my plane and looking at the currently aligned
> mouse aim and nose pointing indicator."

> "Only the release of freelook sets me directly looking at my plane from behind, **orients my
> view as upright relative to the earth** and whatever velocity or turn rate it is currently
> happening I have control with the mouse aim and the camera is completely dependent on mouse
> aim never keyboard over[r]ide."

(Bracketed letters are typo fixes only; wording untouched. Emphasis on "upright" is this
agent's, flagged as such.)

**Corrected model — what changed vs the retracted quote:**

1. **The aim never snaps. Nothing "changes the aim" at release — not even nominally.** During
   freelook the aim is nested to the nose, inseparably; the mouse is operating the *camera*.
   At release the aim is simply *waiting there*. The release moves ONLY the camera, and
   deterministically — Chad is not steering it there. Consequence for the code: setting
   `ci.target_dir_world := nose` at release must be a **no-op in every freelook release,
   keys or no keys** — the aim is already on the nose because freelook nests it there. Any
   measured aim jump at release is a defect by definition.
2. **The release snap includes UPRIGHT relative to the earth.** This is new, from Chad
   directly, and it **pre-answers the Step 2B question** ("does 'snap to chase' mean upright
   too?") — **yes**. "Instantaneous. No need for anything else." The 2026-07-07 ruling
   ("eased, not a snap") is **superseded for the freelook-release case specifically**;
   it was made for ordinary releases, and Chad has now ruled the freelook release upright.
   The pre-registered measurement still runs first and the dominant term still gets fixed —
   but if B dominates or is comparable, **no question to Chad is needed; the ruling is here.**
3. Everything else stands: one snap at release, then ordinary mouse-aim lag; keys never touch
   the camera; the mouse activates nothing.

**Docs correction:** the SPEC quote the v9 plan carries must be replaced with the corrected
wording above — the retracted sentence must not land in SPEC.md as law.

---

## PRE-REGISTERED (2026-07-29) — v9 decision rule, written BEFORE the measurement

**Recorded in advance deliberately.** The last three camera rounds each interpreted numbers
after the fact and each picked a cause that turned out to be partial. This entry fixes the
decision rule, the predictions, and the falsification condition **before** the v9 scenario is
run, so the result cannot be rationalised into agreeing with a preferred fix.

**Chad's model — the whole of it. Nothing may be added.**
1. Release freelook → camera snaps to chase, **directly behind the plane**. Every time. Keys
   held or not; keys are irrelevant to it.
2. After that snap: mouse-aim with the ordinary lag camera.
3. **Keys never touch the camera. Ever.** v8 got this right and it stays.
4. The mouse activates nothing — it moves the aim, the camera lags it.

No latch, no sustain, no key-triggered mode. The "sustain mechanism" framing was this agent's
and was wrong; Chad rejected it twice. **Dropped, and not to be reintroduced without a new
ruling.**

**Symptom being diagnosed** (Chad, flying v8, localised to the *instant*, not the drift):
*"If I release freelook with keys override still getting input it does not give chase but it is
reverted to the old behavior of an angled view from across the loop manoeuvre at an oblique top
down view."*

### The two candidate causes and their predicted magnitudes

| # | candidate | mechanism | predicted magnitude |
|---|---|---|---|
| **A** | **forward term** | `orient_snap_dir` returns the **velocity**, so the cut lands behind the flight path, not behind the aircraft | **≤ ~20°** — bounded by `[aoa] aoa_max = 20.0` plus modest sideslip |
| **B** | **up term** | `orient_fired` cuts `cam_fwd` but **never touches `cam_up`**; `cam_up = loop.aim.up()` carries loop holonomy, and the eye is *lifted along up* | **up to 180°**, righting only over ~1–1.6 s (`horizon_recovery rate = 150 deg/s` + eased tail) |

### The decision rule — comparative, not a threshold

**The dominant term is the term that gets fixed.** Compare `nose_at_fire_deg` against
`updebt_after_release_deg`.

- **A dominates** → change `orient_snap_dir` to return the nose.
- **B dominates** → bring the camera-up upright **as part of the one snap**, instead of leaving
  it to S7-hrz's open-loop roll. ⚠ This contradicts Chad's 2026-07-07 ruling (*"not too much of
  a snap, just a quick uniform movement that is eased at the end"*) — made for ordinary
  releases, not a 180° debt out of a loop mid-fight. **It therefore becomes one question for
  Chad: does "snap to chase" mean upright too?** Ask it with the numbers attached.
- **Comparable** → both need fixing; the plan needs a second half.
- **Both small** → **both hypotheses are wrong. STOP and re-attribute.** Do not proceed to a fix.

⚠ **A threshold test on A alone is not acceptable**, and this is the specific repeat-risk: a
reading of ~18° looks like a solid confirmation of A while B sits at 120°. Fixing A then
delivers 20° of a 120° problem and buys a fourth round.

### Known coupling — the two causes are NOT independent

The S7-hrz debt is captured about the **post-snap forward on the same tick** (load-bearing
ordering, established v6). Changing the snap target from velocity to nose therefore changes the
captured angle and the resulting roll. **`updebt` must be re-measured after any change to A**,
never assumed to have held still.

### Standing guardrail for this change specifically

`orient_snap_dir` sets the **aim** — `ci.target_dir_world`, a **control input**, not a camera
value. This is the first change in the whole camera arc that can legitimately move a controller
golden. **If a golden moves: STOP and confirm intent. Never re-record — a re-record blesses the
bug.**

---

## 2026-07-29 — v8 FLOWN, PARTIAL REJECT: the release snap now EVAPORATES (v9 needed)

**Chad, flying v8:** *"Release of freelook is now giving me oblique view rather than chase…
the most important part of the change is now reverted. The freelook then release should snap
back to chase view even if I am holding the key… we threw out the baby with the bathwater and
reverted to whack-a-mole."* Mouse-aim + keys (the v8 target) is **confirmed fixed**; the
freelook-release case is **regressed**.

**Nothing is mechanically reverted — verified in-tree.** `release_orient = true`,
`release_orient_with_keys = true`, the `ovr_ok` predicate fires, and `app/main.cpp` still
hard-cuts `cam_fwd = loop.aim.forward()` on `orient_fired`. The D9 retirement is fully intact.

**The defect is that the snap no longer persists.** At the release tick: aim := guarded
velocity, camera cut to it — correct, chase-behind. But the aim is then **parked**, and the
pilot keeps turning on the keys. `ease_chase_forward` pulls `cam_fwd` toward a *static* target
at `lag_base + lag_gain·defl` ≈ 0.2 /s, so the camera effectively holds a fixed world direction
while the aircraft rotates out from under it. In a hard turn the view is oblique within a
fraction of a second. **S-keychase had been supplying the *sustain*** by re-anchoring to the
live flight path; excising it removed the sustain, not the snap. **The snap fires and is
erased.**

### Why this was missed — three failures, recorded because each is reusable

1. **The v8 plan's attribution was false, and this agent endorsed it across three audit
   passes.** The plan stated: *"Chad's original oblique complaint was caused by the D9
   exception. Retiring D9 fixed that."* **This file says the opposite**, in the heading of the
   2026-07-28 entry below: *"retire the D9 exception — ⚠ did NOT fix the reported symptom."*
   The measurement was explicit (0.375° at the fire, then re-opening) and is the entire reason
   S-keychase was built. The audits went deep on *implementation* — a vacuous test pin, gate-count
   accounting, the `drive()` instrument gap, the bounded `max_step` — and **never checked the
   premise against the ledger.** The section labelled "Attribution, stated honestly" was the one
   part taken on trust. **Lesson: audit the attribution before the implementation. A plan's
   stated cause is a claim, not context, and the ledger is the place to check it.**
2. **Fly card 2 tested firing, not persistence.** Its criterion was "the one snap *still
   fires*." Firing is a tick-level fact; the felt question was whether the resulting view
   *holds* for the second after. A kernel with exactly this defect passes that card — and did.
   **Lesson: a fly card for a discrete event must state how long the result must survive.**
3. **The sequence Chad actually flies has still never been instrumented.**
   `comfort_turnsteady_keys` starts on keys with no freelook; `comfort_mouseaim_keys` is mouse +
   keys with no freelook. **Neither models freelook → release → continue on keys**, which is the
   complaint in v7 *and* in v8. This is the fourth camera mechanism measured against something
   adjacent to how he flies. **The v9 scenario must be that exact three-phase sequence.**

**Also: the 16.34° "accepted drift" figure understated the real case.** In
`turnsteady_keys` the instructor keeps pursuing the parked aim, which drags the aircraft back
and bounds the divergence. Under real hard key input after a release the aircraft leaves much
further, which is why this reads as "very distracting" rather than as a mild 16°. A
scenario-limited number was quoted as if it were the general one.

### The resolution — the two rulings collide, and only one shape satisfies both

Chad's rules **"keys affect neither"** and **"release should put me in chase even while holding
keys"** conflict in this one case. A one-shot snap can only persist if either the **aim** tracks
the aircraft (ruled out — keys must never carry the aim) or the **camera** sustains. Therefore
the camera must sustain, **armed by the freelook release, not by the keys.** That is not key
authority: it is the freelook release having a *duration* rather than an *instant* — which is
exactly *"only the precedence of the freelook push shall do that."*

This is the original S-keyprec brief (*gate the anchor on freelook precedence*). When the
arm-rule question came back and Chad rejected the three options as framed, the plan swung to
**full excision**, and the excision took the sustain with it. **v8 was right about mouse-aim and
wrong to discard the release sustain.** v9 restores the anchor gated on freelook precedence:
arm on the freelook release edge, disarm when the pilot takes the mouse again (his "then I am in
mouse aim mode"). Open sub-question for Chad, unresolved: whether releasing the *keys* should
also disarm, and whether the disarm-on-mouse transition needs blending.

**Status:** v8 stands sealed (`ae7ae8f23`) and is **partially rejected on the stick**. Do not
graft v8 to seads-recon. Fly card 3 (the deflection-shot question) is still outstanding and is
independent of this.

---

## 2026-07-29 — RULED + SEALED v8: override keys have NO camera authority (S-keyprec) — ⚠ SEE PARTIAL REJECT ABOVE

**Supersedes S-keychase (v7) in full.** Chad, flying v7: *"When I am flying in mouse aim the
snap back to chase is occurring with every hard key press… if I input some aileron to cut into
their path sooner, I get a disorienting snap to the chase cam which throws off my aim and feels
unnatural."* His rule, verbatim: **"Only the precedence of the freelook push shall do that."**

**Decision.** There is exactly **one** camera automation — the snap to chase on freelook
release (`release_orient` / `release_orient_with_keys`, fires with or without keys held,
untouched). After it the camera lags the **aim** under mouse authority alone, permanently.
**Override keys reach the trajectory and never the camera.** The whole law, in Chad's words,
now identical in `SPEC.md` §9.2 and the cascade entry so the three cannot fork:

> **freelook** — aim carried, camera free.
> **mouse-aim** — aim free, camera bound to the aim.
> **keys** — affect neither.

**S-keychase is removed, not gated.** `key_anchor_rate = 0.0` reaches the same behaviour in one
line and was rejected: the ruling is categorical, so a live knob at zero is a loaded gun — a
future tuner raising it silently re-breaks a flown ruling. Precedent: S-aimclamp and S-retclamp,
the other flown-rejected camera/aim mechanisms, were fully reverted with code preserved on a
branch. **Walk-back is therefore a revert, not a dial** — `sandbox/s-keychase-retired`, or the
`flight-kernel-v7-2026-07-29` tag. A real downgrade from a one-line knob, accepted deliberately;
it is why the sandbox branch was mandatory.

**Accepted trade, ruled by Chad:** with the mechanism gone, flying on keys without touching the
mouse leaves the camera ~16.3° oblique in a sustained key turn — the camera showing where he is
pointing; the cure is to move the mouse. ⚠ **Not yet confirmed on the stick — fly card 3.**

**Status:** sealed `flight-kernel-v8-2026-07-29` (`ae7ae8f23` — the handoff brief names
`49e5f93da`, which is the mechanism commit; the seal includes the docs and review-bar commits on
top). Gate 387/387 (−2 retired cases, +1 new leg), zero moved goldens, red-team clean (no P0/P1,
gate re-run and every comfort number reproduced independently). **NOT YET FLOWN**, not grafted.
⚠ **First kernel seal made before Chad flew it.** The convention permits it (a seal is tagged +
pushed + gate-green at seal time), but v5/v6/v7 all carried his verdict in the tag message and
this one does not. Do not read v8 as carrying a stick verdict.

### Two lessons, both generalizable

1. **Two fixes for one defect; the second was the bug.** The original oblique complaint was
   caused by the **D9 exception** (releasing freelook with keys held fired no snap at all).
   Retiring D9 fixed it, and that fix stands. S-keychase was stacked on top to also flatten the
   *standing* state — an over-correction, and the second mechanism is the one that fought his
   mouse. When a fix lands and the symptom persists, re-attribute before stacking.
2. **A flown approval didn't hold, for a structural reason.** S-keychase was approved "precisely
   perfect" against `comfort_turnsteady_keys`, which **parks the aim and flies on keys alone.**
   Chad flies mouse-aim **and** keys simultaneously and no scenario modeled that, so the defect
   was structurally unmeasurable — his approval was genuine but scoped to a case he doesn't fly.
   Third camera mechanism in a row validated against a case he doesn't fly (cf. S-aimclamp,
   S-retclamp). **The rule: a feel mechanism's instrument must model both hands at once.**

### The evasion no test can close — a review bar, not a pin

The red-team built a working evasion: re-adding key→camera coupling as a **defaulted parameter**
on `MiniCamera::advance`, wired only from `app/main.cpp`, reproduces the retired anchor and
**passes the entire suite** — no ctest runs `seads.exe`. No unit test can close it. The
countermeasure is recorded where it can act rather than as prose: **`main.cpp`'s
`ease_chase_forward` call stays branch-free on key state; a conditional there is a ruling
violation on sight** (review bar at the call site and in `SPEC.md` §0). Read that call site first
when reviewing any camera change.

### Correction to an earlier finding in this file

The audit claimed `comfort_turnsnap_ovr` had been measuring the wrong camera law because
`comfort_detail::drive()` dropped `keys_flying`. **The latent defect was real** — the flag had to
be threaded by hand and the convenience wrapper silently took the `false` default, the exact
instrument fork `chase_anchor`'s purity existed to prevent, and it would have caused a **false
abort** of the v8 investigation by showing no snap on v7. **But there was no existing damage:**
`turnsnap_ovr` holds its override only while freelook is *also* held, which the selector
excluded, and when the wrapper was fixed **no comfort number moved.** A real defect with zero
measured consequence; v8 removes the class outright, since the seam no longer takes key state.

---

## 2026-07-29 — SUPERSEDED BY v8 — FLOWN-APPROVED: the camera ANCHOR (S-keychase) — "precisely perfect"

⚠ **Retired in v8 (entry above). Do not implement from this entry.** Kept because the reasoning
trail matters: the measurement-provenance error, the gunnery reclassification, and the
"never blend the handback" corollary are all instructive, and all three were superseded when the
mechanism was removed.

**Chad's verdict on the stick, 2026-07-29: "now it is precisely perfect… It's the right
set up for my camera now."** Approved as the camera's finished state, and the trigger for
sealing the kernel as **v7** (advisement below).

**Chad ruled option 1**, landed as `5ea20d8c7` on `feel/kernel-v5`, gate **388/388**, zero
moved goldens, compiler-clean verified at the gate (the "new diagnostics" noise was clangd
missing include paths, not the build).

**The handback swing read fine on the stick** — consistent with the corrected ≈16°/≈0.7 s
estimate below, not the ≈96° first feared. No mitigation needed; the hard switch stands
and the "blend the handback" fallback is unused. Recorded so a future reader knows the
hard switch was flown deliberately, not by omission.

**Mechanism.** While override keys are flying and freelook is not held, the chase camera's
rest target swaps from the parked aim to the flight path, caught at a constant
`[camera] key_anchor_rate = 6.0 /s` (≈0.17 s). Mouse-aim flying is bit-identical **by
construction** — with no key held the helper returns the caller's own dials verbatim.
Selection goes through one pure helper (`render::chase_anchor`) called by both
`app/main.cpp` and the comfort instrument (`MiniCamera::advance`), so the shipped law and
the measured law cannot fork — the same route-the-live-path-through-the-tested-function
discipline as `app::tick`. Walk-back: `key_anchor_rate = 0.0` (structural off).

### ⚠ Measurement provenance — the 95.8° that motivated this cannot move

**Recorded because the mis-attribution is the reusable lesson.** The
`COMFORT turnsteady standing_oblique_deg 95.823` figure that drove this whole change is
from a scenario that holds **no override key**: `comfort_turnsteady` calls `turn_reaim`
every tick, which models a **mouse** pilot dragging the aim through the turn. There,
`keys_flying` is false, so `chase_anchor` returns the unchanged dials — **by the very
bit-identical property that makes S-keychase safe.** That 95.823° will still read 95.823°
after this fix, forever. It was never Chad's case.

Chad's case is the keyboard one, and it is measured by the scenario built for it,
`comfort_turnsteady_keys` (both arms, self-evidencing):

| leg | standing oblique |
|---|---|
| `turnsteady_keys_off` (v6 law) | **16.34°** |
| isolated law leg | 89.62° → 0.00° |
| `turnsteady` (mouse pilot — untouched, and untouchable, by this fix) | 95.82° |

So the fix targets the right case, but **the real magnitude of the reported symptom is
≈16°, not ≈96°** — roughly six times smaller than the headline number implied. The 89.62° →
0.00° leg is an *isolated law* check (the helper in isolation), not the end-to-end symptom.
Whether a standing 95.8° oblique in a sustained **mouse** turn is itself a problem is a
separate, unasked question — it is presumably the approved "camera follows the aim" feel,
but nobody has put that number to Chad.

### Watch-item: the handback swing — magnitude corrected downward

S-keychase is a **hard switch** on `keys_flying`. `cam_fwd` is carried and eased so there is
**no pop** at the switch, but the *rest target* jumps from the flight path back to the
parked aim the instant the last key comes up, and the camera then chases it under the
approved mouse-aim dials (`rate = lag_base + lag_gain·defl = 0.2 + 0.7·defl`).

**Correction to the earlier estimate in this file:** that estimate used defl ≈ 1.67 rad,
taken from the mis-attributed 95.8° mouse figure, and gave ≈96° over ≈1.2 s. Using the
*measured keys* deflection (≈16.3° ≈ 0.285 rad): rate ≈ 0.40 /s against a 0.285 rad gap —
**≈16° of swing over ≈0.7 s.** Mild, likely unremarkable on the stick. The concern was real
in kind but roughly 6× overstated in magnitude.

**It still scales with how far the aim actually parks.** In the scenario the instructor
keeps pursuing the parked aim on non-overridden axes, which is *why* it only reaches 16°.
A longer or harder key-turn than the 8-second scripted one parks the aim further off and
grows the handback proportionally, so the fly is still worth doing deliberately: hard
key-turn, mouse held still, then let go and watch the handback — key-down is the part
already known fixed. **If it reads wrong, blend the handback over a couple of tenths**; do
not raise the lag dials, which would move approved mouse-aim feel. It does not violate the
standing camera-independence constraint (state→camera throughout).

### RULED (2026-07-29) — the keys must NOT carry the aim. It is the whole point.

The option was put to Chad as a deeper fix: have the override keys **carry the aim along**,
so releasing them never hands back what was then (wrongly) called a stale target. **He ruled
against it, decisively, and the reason is a design statement about what the two input modes
ARE** — verbatim:

> "Not letting the keys carry their aim is precisely the point. Having freelook pressed is
> the mode that carries my aim and cam is free. In mouse aim, mouse is free, camera fixed —
> and it's the only way to access forward-looking oblique deflection shots in a merge while
> holding hard on a key and maintaining aim freedom at the same time. If I want to harness
> the aim into my nose then I press freelook."

**This is the mode duality, stated for the first time and now load-bearing:**

| mode | aim | camera |
|---|---|---|
| **freelook held** | **carried** (welded to the airframe) | **free** (orbit follows the mouse) |
| **mouse-aim** (no freelook) | **free** (mouse owns it) | **fixed** (anchored per `[camera] lead`) |

The keys are deliberately *outside* that duality: they move the airframe **without** taking
the aim, which is what buys **aim freedom while pulling hard** — the pilot holds a key
through the merge and keeps steering the reticle independently for a deflection shot. Making
the keys carry the aim would collapse the two modes into one and delete that capability.
Any future proposal to "fix" the parked aim under keys must be refused on this ruling.

**Chad's forward read, worth keeping as a prediction to check later:** "more can now be done
in mouse aim mode, and I suspect then my freelook will be only for situational awareness and
when I want to see / need to see obliquely — as opposed to not just *being* oblique."
I.e. v7 is expected to shift freelook from a *flying* verb to a *looking* verb. If a later
session finds freelook usage dropping and mouse-aim carrying more of the fight, that is this
prediction coming true, not a regression.

### Refinement (same day) — S-keychase is a GUNNERY mechanism, and the parked aim is a PLAN

Chad corrected the framing above, and it matters enough to restate: the value of flying on
keys without freelook is not only that the mouse stays free. Verbatim:

> "I can plan my aim for when I release the key override… but also, and most importantly,
> the fact that I am looking through the nose line of the plane from behind, or see its
> angle from behind its velocity — and it may be obliquely aligned at an enemy, but I see
> where I am shooting and plan my aim when hard-pressing maneuver without freelook."

Two corrections to how this repo had been describing it:

1. **"Stale target" was the wrong word and should not be reused.** The parked aim is
   **deliberate** — the pilot is pre-placing where he intends to be aiming when the keys come
   up. It is a *plan*, not a leftover. The defect was never that the aim was parked; it was
   that the **camera** was anchored to it, spending the eye on the plan at the moment the
   pilot needs the shot.
2. **The behind-velocity anchor is a shooting reference, not a comfort fix.** Sitting behind
   the velocity is what makes the **nose-versus-velocity angle** visible — the gun line
   against the flight path. The aircraft can be flying one way and obliquely lined up on an
   enemy another way; from behind the flight path that offset is legible and the shot can be
   read. From behind the parked aim it is not. S-keychase should therefore be classified with
   the gunnery/instrument mechanisms, not with the comfort program.

**Consequence for the handback:** the ≈16° swing on key release is the camera going *to the
place the pilot decided to look*. That is why it flew as "precisely perfect" rather than
intrusive, and it is an argument **against** ever adding the blend that was held in reserve —
softening it would blur the moment the plan arrives. Recorded so a future session does not
"improve" it.

### Consequence — the 95.82° mouse figure is very likely a FEATURE, not a defect

`COMFORT turnsteady standing_oblique_deg 95.82 / converged 0` was logged above as an
unasked question. **Chad has now effectively answered it without being asked.** That
scenario is a sustained *mouse* turn, where the camera anchors to a free aim — which is
precisely the "forward-looking oblique deflection" geometry he just described as the
capability he wants. The camera showing where the aim points rather than where the plane
goes **is the deflection view.**

⚠ **Therefore the instrument's own predicate is mode-blind and should not be read as a
verdict.** `comfort_detail::converged = oblique_deg < 10 ∧ up_debt_deg < 10` encodes an
assumption that the camera *ought* to end up behind the flight path. That is right for
keyboard flying (S-keychase now delivers it) and **wrong for mouse-aim**, where a large
standing oblique is the intended capability. `turnsteady converged 0` is the instrument
measuring the wrong goal for that mode, not a failure to converge.
**Recommendation to the harness agent (this repo cannot edit the live tree):** either
mode-qualify `converged`, or rename the mouse-mode metric so it reads as *deflection
geometry* rather than as a comfort failure. Left as-is it will keep being mistaken for a
defect — it already was once, in this very session, where it motivated a change it could
never affect.

**What the instrument says** (comfort table, shipped config, measured by the harness agent):

```
COMFORT turnsteady standing_oblique_deg 95.823   converged 0
COMFORT orient     oblique_at_fire_deg   0.375
COMFORT orient     peak_oblique_deg     51.269   (after the fire)
```

The orient verb fires correctly and collapses the camera to **0.375°** off the flight path.
Then it re-opens to ~96° and **never converges**. The D9 work fixed the release *instant*;
Chad reported the *standing* state, which returns within about a second of the cut.

**Mechanism.** The chase camera's rest target is anchored to the **aim** (`[camera] lead =
1.0` — "1.0 = the camera's rest target IS the aim"). Under mouse-aim that is correct and is
the approved feel: the aim is where you're going, and the reticle-floats-then-centres
behaviour comes from `lag_base`/`lag_gain`, not from `lead`. But when Chad flies on the
**override keys**, the keys move the aircraft while the aim stays **parked** — so the plane
flies out from under the aim and he watches it obliquely. **This is not lag against the
plane; it is the camera anchored to the parked aim.** Raising the lag cannot fix it, because
the *target*, not the catch rate, is what answers the wrong question here.
⚠ **Refined later the same day** (see the S-keychase entry above): the parked aim is
**deliberate** — the pilot's plan for the release — so it is not a "stale" target, and the
anchor swap is a **gunnery** mechanism (it makes the nose-versus-velocity gun line legible),
not a comfort one. Earlier wording in this file that called it stale is superseded.

**Chad's original sentence already said this** — "even if still turning and pressing hard
keys for control surfaces" — and the earlier framing (snap-at-release vs. a permanent
behind-lock that would delete `lag_gain`) presented a false pair and steered to the release
edge. The harness agent has said so plainly; recorded here because the framing error is the
reusable lesson, not the measurement.

**The three options put to Chad:**
1. **Anchor behind the flight path while override keys are held** (recommended by the
   harness agent). Rest target switches from the parked aim to the flight path only while a
   key is down and freelook isn't. **Mouse-aim flying is untouched** — `lead`/`lag_base`/
   `lag_gain` unchanged, so nothing Chad approved moves. Releasing the keys hands the camera
   back to the aim-anchored chase.
2. **Behind the flight path whenever freelook isn't held.** The literal reading of Chad's
   sentence, applied to mouse-aim too. **This does change approved feel:** the
   reticle-floats-off-centre-then-closes behaviour goes away, because the camera stops
   following the aim.
3. **Dial the lag faster** (`lag_base`/`lag_gain`). One line, no new mechanism, but
   **cannot fully fix it** — the target is still the parked aim, so a hard sustained
   key-turn still stands off, and it speeds up the mouse-aim float Chad liked.

**Docs-side check, so this is judged as a feel question and not a safety one: all three
options are clean against the standing camera-independence constraint** (below). Each is a
change to the camera's *rest target*, i.e. state→camera; none creates a camera-derived
quantity feeding `pitch`/`roll`/`yaw`/throttle, so the motion-sickness rubber-band cannot
form. Option 2 is nonetheless the one to fly most carefully: it removes a
visual-motion cue Chad has already approved, and approved feel is the thing this repo is
least willing to lose by accident.

**No cascade entry owns the camera anchor yet.** `lead`/`lag_base`/`lag_gain` are described
only in `controller.toml` comments and inside the freelook entry's Code section. Whichever
option is ruled, this mechanism has earned its own four-level entry — flagged as doc debt.

---

## 2026-07-28 — LANDED: retire S-relorient's D9 exception ("truly redundant") — ⚠ did NOT fix the reported symptom

Supersedes the OPEN QUESTION raised earlier the same day (S-relorient's D9 exception has no
second chance). **Chad ruled, and the change landed** — `feel/kernel-v5` commits
`35e31695f` (mechanism) + `13631ba92` (red-team folds), gate **386/386**, zero moved
goldens. **Not sealed, not tagged, not pushed**, and — the important part —
**⚠ it did not resolve the oblique-camera symptom Chad reported.** It fixed the release
*instant*; the symptom is a *standing* state. See the follow-on open question immediately
above this entry (camera anchor / standing oblique). Read the two together or this entry
reads as a success it wasn't.

**The report that forced it.** Chad, on the sealed v6: "there are cases where I press space,
use override keys for flying, then let go of space and continue with the override keys — and
the camera goes to an oblique angle." His spec, verbatim, and it is the sentence the whole
change serves:

> "Anytime my finger isn't pressing freelook, I am in chase camera directly behind and using
> mouse aim — even if still turning and pressing hard keys for control surfaces."

**The ruling.** The D9 "you're still maneuvering" exception is **retired**. The finger
leaving Space is the whole trigger. Two sub-rulings, both Chad's:
- **(a) Fire the FULL verb**, identical to a clean release — not a camera-only variant.
- **(b) Snap at release, keep the flown-in chase lag afterward** — not a permanent
  behind-lock (that would delete `[camera] lag_gain` and is a separate fly).

He is **keeping** the double-tap, but wants it *truly* redundant — he had to reach for it
precisely because the release failed. That is the test of this change: the double-tap
becomes a genuine backup rather than the only way out of a stuck state.

**Root cause — three sites in `app/instructor_tick.h`, all gated on `any_ovr`:** the
`release_orient` predicate (`orient_fired` withheld ⇒ `main.cpp` never hard-cuts `cam_fwd`
nor zeroes the orbit); the S7-hrz capture (`recov.reset()` instead of `capture()` ⇒ the
up-debt never rolls off); and the double-tap (the manual escape hatch suppressed under the
same condition). Two aggravating facts carried over from the open question and confirmed in
trace: all three are gated on `fs.released`, a **one-tick edge**, so there is **no second
chance** — letting go of the keys later re-fires nothing; and it is a **split, not a clean
no-op** — rule 3 still moves the reticle to guarded velocity while the camera doesn't cut.
The residual oblique Chad sees is `cam_fwd` on `ease_chase_forward`, which in a sustained
turn never converges. The orbit is *not* the culprit (it decays ~120 ms every non-freelook
frame regardless).

**Shape of the change** (worktree `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5`;
`app/` + `config/` only — the kernel firewall holds by construction, with one tune-data
field in `control/params.h` where `freelook_release_orient` already lives):
- **New knob, the standing structural-off-switch pattern:**
  `freelook_release_orient_with_keys`, **default `false` = today's shipped behaviour exactly**
  (every knob-off arm bit-identical), read via `optional_bool` like `release_orient`, set
  `true` in `config/controller.toml` as Chad's fly value.
- **All three sites relaxed behind that one knob**, so the two triggers can never diverge
  again. The `in.freelook_held && any_ovr` nesting branch is untouched — that is the
  while-held rule, not the release.
- **Sub-ruling with reach beyond the release edge, flagged as a real behaviour change:** with
  the knob on, an override pressed *later* no longer aborts an in-progress horizon roll.
  Required by "even if still turning and pressing hard keys"; safe because the D3 roll is
  open-loop — a gauge move about the aim forward, invisible to `control::step`, so it cannot
  fight the keys.
- **Reuse, not a third copy:** the guarded-velocity snap is currently duplicated verbatim at
  the release and double-tap sites. It gets hoisted to one file-local helper called from
  both — behaviour bit-identical, but it makes "one verb, two triggers" a property of the
  code rather than of two paragraphs of comment.
- **Code banners rewritten to the flown truth.** Three banners still state the D9 rationale
  as settled ("the next clean release orients"); they name the walk-back instead, so the next
  reader doesn't re-derive the dead rationale.

**Test discipline.** The existing pin "override held at release = legacy, no camera cut" is
**re-scoped, not deleted** — kept verbatim as the knob-OFF arm, proving legacy is exactly
reproducible. New legs (each mutation-verified, on a binary proved fresh): knob-ON full verb
fires; horizon debt retires with the key still down; **one-shot** — letting go of the keys
later fires nothing more (this guards against anyone "fixing" it with a deferred latch on
top, which was the shape floated in the open question and is now explicitly *not* the
design); double-tap fires with an override held; and knob-OFF ⇒ bit-identical across the
whole four-phase script (the strict-superset proof). Repro: hold Space → press override
mid-hold → release Space with the key still down → release the key several ticks later;
**assert on the phase after the release, not the release tick.** Harness seam respected:
`ClosedLoop` models the knob-OFF release and has no `orient_fired`, so these pins stay in
`test_relorient.cpp` (app::tick only). **No controller golden may move. If one does: STOP.**

**Trade, stated honestly.** With D9 retired, a release-while-holding-keys snaps the aim to
the flight path mid-maneuver — the same trade CARD 1 already flags for the double-tap. Chad
ruled for it; at that instant he is flying on the keys, which keep the turn. Walk-back is one
line (`release_orient_with_keys = false`) and restores today's kernel exactly.

**Status:** LANDED on `feel/kernel-v5` (`35e31695f` + `13631ba92`), gate 386/386, zero moved
goldens, **not sealed / not tagged / not pushed**, and **NOT YET FLOWN** — the fly card is
pre-filled in the live tree's `docs/flight-log.md`. 6 new test legs (both arms each); the
three pre-existing D9 pins re-scoped to pin the knob rather than deleted; three mutants
killed at 4/2/1 cases; fresh-context red-team returned no P0/P1, added two mutants of its
own (one proving the walk-back arm is genuinely pinned), and its P2s were folded.

**Doc gaps I flagged are all closed in the diff** (verified in-tree): `SPEC.md` §9.5 now
states the release fires the orient verb independent of held keys; S-relorient has a §0
supersession entry (line ~736); and the fourth site I found — the D9 clause buried inside
**S7-hrz's own §0 entry** — carries an explicit ⚠ RETIRED marker. Cascade entry:
`docs/cascade/freelook-orient-verbs.md`.

⚠ **`reference/seads-feel/` is now STALE for this mechanism.** It is a snapshot of
`cfe1bd7fe`; `app/instructor_tick.h`, `control/params.h`, `config/*` and `SPEC.md` all moved
after it. Re-snapshot only once Chad has flown and sealed — a snapshot of an unflown,
unsealed tip would enshrine a behaviour that may yet walk back.

---

## 2026-07-28 — Rudder-bias trim + S-relorient, Chad-approved ("okay we have a winner")

Two changes landed on `feel/kernel-v5` in one session, both flown and approved on Chad's
stick. Commits: `274432f35` (S-relorient), `385a43dbd` (yaw_scale), `468f2b352` (red-team
folds), `b2019cf43` (flight-log rows). Gate 380/380 at each step.

**1. Rudder trim: `yaw_scale` 2.2 → 2.0.** Chad's ask: "a little too much rudder bias in
the equation" — confirmed symptoms: nose sits crabbed / rudder always working, plus
violent snap-back at speed. No v5 commit had touched the yaw ladder; the pre-v5 tuning was
being exercised harder by v5's stronger energy model (higher V ⇒ the q-scaled yaw terms
bite more). 2.0 is the previously-flown MB-4 value, away from the AT-16 β wall.
**Chad's verdict carries a causal insight worth keeping:** "Now that the flight kernel was
given a more sufficient engine per weight ratio, the mouse aim and nose is responding
better without the need of so much rudder... it feels much better now to not have to chase
the mouse with so much rudder but now the plant is able to respond." — i.e. rung D's T/W
0.61 is *why* less rudder authority is needed: the airframe can now follow the aim with
lift instead of skidding onto it with yaw. Pre-agreed fallback rungs (NOT taken — symptom
resolved): `Cy_beta 2.5 → 1.5` if speed snap-back survived; `center_frac 0.0 → 0.3` if
crab-at-rest survived (⚠ that one walks back the Rung-M1 "nose in the MIDDLE" ruling and
was flagged as such). Walk-back: 2.2. Full ladder history:
`docs/cascade/rudder-coordination-ladder.md`.

**2. S-relorient: every freelook release fires the ORIENT verb.** Chad's ask: releasing
freelook should auto-orient (the double-tap behavior, automatic). Mechanism: on the
freelook release edge, the aim snaps to guarded velocity and the camera hard-cuts behind
the flight path — the same tested path the S-orient double-tap runs. Deliberate
exceptions: sub-stall/ballistic releases land on the nose (velocity lies there), and a
release while an override key is still held keeps legacy behavior (pilot is actively
maneuvering). Knob: `release_orient` in `[freelook]` — **optional-with-default-false in
the loader** (fixtures untouched, knob-off bit-identical legacy), `true` in the shipped
toml. Walk-back: one line, `release_orient = false`. The double-tap still works and is now
redundant; retiring it is an open question for Chad.

**Process notes worth preserving:** the plan-stage audit (this repo's session) caught a
real ordering defect — the snap must fire BEFORE the S7-hrz horizon-recovery capture so
the up-righting measures against the new forward on the same tick (release-orient
therefore rights the horizon slightly *better* than the double-tap did). The fresh-context
diff red-team came back SOUND-WITH-FIXES; its one real find (nothing pinned the fire as
one-shot — a re-fire-every-tick mutant survived the whole suite) was folded and
mutation-verified. 7 new test legs + 4 loader legs.

**Status:** LANDED and Chad-approved on `feel/kernel-v5`; in `reference/seads-feel/` as
of the 2026-07-28 re-snapshot; not yet reconciled to the game trees. Cascade entry:
`docs/cascade/freelook-orient-verbs.md`.

---

## 2026-07-28 — Auto-right quickening: `inverted_delay` 1.0 → 0.5 s ("3/3")

Same-day follow-up ask, flown and approved ("yes perfect as expected 3/3!" — the third of
three approvals that session). One TOML dial on the MB-right mechanism (Chad 2026-07-07:
"need to roll over on bank after about 2 s no gross inputs if belly up... slow roll off
ailerons"): the belly-up **rest timer** before the wings slow-roll upright halves;
`inverted_rate` stays 180°/s (the roll itself is unchanged, it just arms sooner). Landed
`e1684fdbb`, gate 380/380; verdict logged `cfe1bd7fe`.

Not a delicate change, and the reasoning is worth keeping: single dial, roll rate
untouched, and the scripted golden never dwells inverted so no goldens moved. One test
tripped **deliberately** — the "inverted plane at rest STAYS inverted" leg carries a
config-relative premise calibrated to the 1.0 s dial (`REQUIRE(window > 60)` ticks); at
0.5 s the inside-the-delay window is 48 ticks. That is the repo's designed tripwire for
exactly this kind of retune: the premise was re-derived honestly (floor 36 ticks, reasoning
in the comment) and the mutant it guards (un-gated wings-hold righting the plane) was
re-verified to die in the shorter window.

**Fly sentinel (standing):** a loop apex or slow roll where the hand rests a full half
second now auto-rights sooner. If it starts stealing inverted maneuvers, walk-back is
1.0, or 0.75 splits the difference.

---

## 2026-07-23 — The v5 rung ladder (rungs A → E), `feel/kernel-v5`

Chad flew `main` (v4-approved) and reported small, smooth mouse adjustments made the
elevator/rudder overshoot the aim circle and bounce — "at those small deflections it is
treating them like they are big deflections." Five rungs of measured, Chad-approved fixes
followed, each with its own dial and its own explicit walk-back/kill value (see
`tuning/evc2026-v5-rungE.md` §2 for the full table with pre-rung values). In order:

- **Rung A — S-truedepth** (`dc0b1d01c`): a capture event's "glance depth" is capped at its
  own momentum-earned stopping distance rather than a fixed rim target. Dial:
  `capture_depth_frac = 1.5` (walk back to `≤ 0` for the v4 fixed-rim behavior).
- **Rung A2 — sub-wall curve** (`c8e98afa3`): Chad's fly-1 verdict — "better, just needs a
  little more" on small/fine adjustments. Dial: `capture_depth_pow = 2.0` (walk back to
  `1.0` = rung A bit-identically).
- **Rung C — hold-the-line** (`f523177e9`): Chad's fly-2 ruling — "It should hold the line of
  my mouse inputs and try to get to my mouse until full stall — it might sink a bit as I
  begin the stall but the nose should stay where my mouse is asking." Dials: `K_aoa: 5.0 →
  10.0`, `pull_floor: 0.0 → 1.0` (0.0 is the structural OFF / bit-identical legacy tree).
- **Rung D — the arcade energy model** (`c0625ede1`): Chad's ruling — "give me the power — I
  had been intuiting all along that the airframe is being underserved" (this **supersedes**
  his own earlier 2026-07-08 `T_max = 9000` ruling). Dials: `k_induced: 0.05 → 0.015`,
  `T_max: 9000 → 18000` (T/W 0.31 → 0.61), `n_max: 16 → 32`. **Correction:** an earlier draft
  of this repo's tuning notes had these dial directions backwards — `0.05`/`9000`/`16` are
  the values you walk BACK TO (pre-rung-D), not rung D's shipped values.
- **Rung E — the knife edge + the red arrow** (`89447aba5`): see the entry below.

Full detail, measured before/after grids, and red-team notes for every rung:
`reference/seads-feel/docs/v5_kernel_handoff.md`.

---

## 2026-07-23 — Push/split-S commitment gate moved to 45° (rung-E)

**Decision:** The push/split-S commitment gate's `push_horizon_enter` threshold moves from
1.0° below horizon to **45.0°**, with `push_horizon_exit` at 40.0° (5° hysteresis band).
Commitment now requires BOTH lateral deflection AND genuine down-aim past 45° below horizon.

**Why:** At 1.0°, the old side-cone gate degenerated at big lateral deflections — a long
lateral drag whose aim merely grazed slightly below the horizon (an artifact of the
aim-frame's own arc, not player intent) became eligible for committed nose-down, producing
"mystery dives" the player never asked for. Chad's own words: "I think it's the knife edge
set too high on the horizon... nose-down with the bank over should need my down input too,
past like 45 degrees." Moving the gate to 45° means a shallow lateral graze can never reach
it, while a genuine big flick (hard lateral + mouse well down) still commits cleanly and can
carry a full split-S through past inverted.

**Companion decision:** the aim reticle stays raw/unclamped (it is not re-pinned to the
screen edge when off-frame); a separate red arrow is drawn at the screen edge pointing at the
true aim direction whenever it goes off-screen. This makes the otherwise-invisible
below-horizon curve of an off-screen lateral drag visible, while the 45° gate makes it
harmless even when unnoticed.

**Status:** LANDED on `feel/kernel-v5` (commit `89447aba5`, 2026-07-23) — this is real,
committed code, not a proposal. **Pushed to origin** (as of the same evening), and **not
reflected** in either the
`reference/evc2026/` snapshot (a different, prior-generation Luau kernel that never had this
mechanism) or in `main` (still v4). See `docs/cascade/push-gate-knife-edge.md` and the
reconciliation watch-item above.

---

## Standing constraint — Camera independence for motion sickness

**Decision:** The camera may lag, ease, and visually swing toward the aim direction or
heading, but it must never be an input to the flight control law — only a one-way,
downstream function of state. No camera-derived quantity (FOV, orbit angle, zoom factor,
lag-eased heading) may feed back into `pitch`/`roll`/`yaw`/throttle.

**Why:** This is the single mechanism standing between "the camera looks like it's swinging
toward centre" (the intended illusion — see `docs/cascade/mouse-aim-instructor-cascade.md`)
and actual motion sickness. If camera motion ever leaked into control, the mismatch between
commanded and actual aircraft response would be subtle, constant, and — especially on
SEADS's small non-euclidean sphere where curvature is always present — very hard for a
player to build a stable mental model around (see
`docs/cascade/spherical-earth-non-euclidean.md`).

**Evidence this is taken seriously in the existing code:** `BirdController.client.luau`'s
right-click aim-zoom is explicitly documented as "AWARENESS-ONLY... cannot affect flying" and
its steering is suspended while it's active specifically so the FOV change can't feed back
through the camera-projection aim into pitch/bank.

**Status:** standing, not a one-time decision. Applies to both EvC2026 and SEADS camera work.

---

## 2026-07-23 — Recorder graft landed: SOUND-WITH-ONE-FIX, schema gains raw_flap/raw_gear

The felt-flight recorder proposal was grafted into seads-feel (`d1e7dbe6b` on
`feel/kernel-v5`, gate 372/372) after a four-gate review run, not trusted: symbol walk,
tick-level tap at the accumulator seam (AT-9), structural read-only, and the differential
leg on the real spherical plant (same seed, recorder on/off, `LoopState` bit-identical;
480-tick round-trip replay onto stored pins; tamper signature verified). Two new permanent
tests in the gate.

**The one fix, and the lesson:** the proposal flattened `raw_in` as
pitch/yaw/roll/throttle, but `sim::Inputs` also carries `flap_cmd`/`gear_cmd`, which
raw-mode flights command inside `raw_in`. A recorded raw-mode flight with flaps would have
replayed with a clean airframe — bit-perfect divergence of exactly the silent kind this
harness exists to kill. Schema now carries `raw_flap`/`raw_gear`. Changed while the
`.seadsrec` format was still v1-unshipped, so it was free; a day later it would have been a
migration. **Standing rule: when flattening a struct into a recording schema, walk every
field of the source struct, not the fields you remember.** Proposal copies in
`harness/seads_recorder_proposal/` are synced to the grafted versions
(`test/harness/recorder.h`, `test/unit/test_recorder_firewall.cpp` at `d1e7dbe6b`), which
are now authoritative. Deliberate scope note: the in-game record toggle (main.cpp key
wiring) lands as its own small change at first real recorded flight.
