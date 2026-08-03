# SOP-01 — PRIMARY DATA — CRITICAL CONTROL POINT

**Status:** Chad's ruling, 2026-08-02. **This is the number one imperative of this project.**
It outranks every other document, plan, spec, backlog and agent instruction. Where anything
conflicts with SOP-01, SOP-01 wins.

**Ruling, verbatim:**

> *"I will never fly again unless this is all proven and verified. Here is the root of the rot
> in this project. Make the rule as an SOP that is a critical control point now. I will not fly
> unless verified and made proven that primary data from flying and proper analysis and function
> of the data and its analysis are proven to be clear and useful. No lying and lazy generalizing.
> The verification will only take place from flight recorded data that is complete and
> comprehensive. This SOP is the number one imperative."*

> *"Make sure to word this data collection requirement clear, simple, complete, explicit and
> binding. We can never be lazy in this regard again."* — Chad, 2026-08-02

---

## THE RULE IN ONE SENTENCE

**Do not ask Chad to fly until you have made the running build produce a real row of data and
you have read that row back off the disk yourself.**

Everything below is that sentence, made explicit enough that it cannot be argued around.

**And no flight-dependent work proceeds at all** until the whole data-collection program is
**built, running and tested** — with a **control arm** and an objective method of analysis fixed
in advance. See THE HARNESS CONTRACT and SCIENTIFIC CONTROL below. A measurement without a
control is an anecdote.

**Binding on:** every agent, every session, every build, without exception, whether or not the
change seems small, whether or not the instrument "obviously" works, whether or not a previous
flight went fine. **There is no trivial flight.** Chad's time in the air is the scarcest
resource in this project, and a wasted sortie costs trust, which costs more than tokens.

**Not satisfied by:** the code contains a `print`. A harness exists in the repo. It worked in a
previous session. A document says it works. Another agent said it works. Reasoning that it
should work. **Only a row on disk, read this session, satisfies this rule.**

---

## CCP-1 — THE RULE

**No agent may ask the Pilot to fly until the entire data path has been proven end to end, by
that agent, on that build, in that session.**

Proven means a row of real data was produced by the running build and landed on disk where the
agent can read it — demonstrated, not expected. Not "the code has a print statement." Not "the
harness exists." Not "it should work."

**The Pilot's time in the air is the scarcest resource in this project.** A sortie flown against
an unarmed instrument or a missing sink is not a partial success. It is a total loss, and it
costs trust that costs more than tokens.

---

## WHY THIS EXISTS — the failure that produced it

2026-08-02. An agent asked the Pilot to fly a full test ladder. Afterwards it emerged that:

1. The step-ladder instrument was gated behind `_G.__evcStep`, never set. **Zero rows recorded.**
2. The agent had named `harness/capture_server.ps1` as the sink. That server pairs with a
   `FlightRecorder` draft that was struck down by review and never shipped. **It could not have
   received anything.**
3. The only data that landed was in the Roblox Studio log: 27 identical `[EvC pole] mask=0` beats
   at 5-second spacing from an instrument the project's own S63 commit already recorded as
   **blind**, plus 3 `[EvC line]` rows.

The agent had read what the code *printed* and never checked that anything would *receive* it.
Every one of the three pilot observations from that flight remains unmeasured.

**Root cause: an agent asserted readiness it had not verified.** That is the rot. SOP-01 is the
control point that makes it structurally impossible to repeat.

---

## THE GATE — every item GREEN before a flight is requested

Each item requires a command run and its actual output pasted into the flight request. An item
with no evidence is RED. There is no amber.

| # | gate | proof required |
|---|---|---|
| G1 | **Sink exists and is writable** | the path, and a file written to it in this session |
| G2 | **End-to-end smoke test** | the running build emitted a row and the agent read it back off disk. This is the load-bearing gate |
| G3 | **Every instrument armed** | each gating flag/global shown set, at file:line, in the build being flown |
| G4 | **Every instrument proven non-blind** | fault injection: a known synthetic event produced a positive detection. A green run on quiet data proves nothing |
| G5 | **Coverage sufficient** | for each question the flight must answer, the quantity, its rate, and where it is recorded |
| G6 | **Rate adequate to the phenomenon** | per-frame for anything transient. A 5-second heartbeat may not be called a recording |
| G7 | **Analysis script runs** | on the smoke-test data, producing the actual statistic — before the flight, not after |
| G8 | **Reference numbers cited** | what the result gets compared against, with file:line |
| G9 | **Volume known in advance** | expected rows and file size for the sortie length |
| G10 | **Build identity recorded** | BuildStamp/commit written into the log header so the data can never be orphaned from what produced it |

---

## WHAT COUNTS AS PRIMARY DATA

**Complete and comprehensive**, per the ruling. Specifically:

1. **Recorded, not sampled.** Per-frame for any transient — roll onset, nose resolve, knife-edge
   entry, a pole crossing. A periodic heartbeat is a status light, not a record, and may never be
   presented as one.
2. **Ungated.** No instrument that matters may sit behind a flag someone has to remember. If a
   flag exists, arming it is a gate item (G3).
3. **Attributable.** A summed command is not evidence about its contributors. Where a quantity is
   a sum — roll being pointing + leveler + dwell + damping — **each term is logged separately** or
   nothing may be concluded about which one is responsible.
4. **Written to disk**, in a known path, surviving the session, readable by the agent without
   asking the Pilot to screenshot anything. **Never ask the Pilot to transcribe or screenshot
   data.** If an agent cannot read it directly, the path is not ready.
5. **Self-identifying.** Every log carries build identity, config values in force, and timestamps.

---

## ANALYSIS — the same standard applies

Collection without honest analysis is the same failure one step later.

1. **State the claim class.** MEASURED (a run in this repo, cite the number), SOURCED, DERIVED,
   INFERRED (**must state its falsifier**), SUBSTITUTED. An unclassified claim is a defect.
2. **No lazy generalizing.** State n. Three rows is three rows. If the sample cannot support the
   conclusion, the finding is *"not established"* — which is a real result and must be reported as
   one, not padded into a story.
3. **Absence of evidence is not evidence of absence.** A null from an instrument not proven under
   G4 is *no data*. Reporting it as "no defect found" is lying by omission.
4. **Never conclude from a compensated quantity.** If a mechanism nulls the effect being measured,
   measuring zero proves nothing about the underlying effect. Name the compensator or say nothing.
5. **The analysis may not share a bug with the code it measures.** A check that repeats the
   implementation's own error passes green while the system is broken. This has already happened
   here once, on a units error.
6. **Report failures and dead ends.** Every sortie writes its row, including the ones that
   produced nothing.

---

## WHAT AN AGENT OWES THE PILOT

- **Ask him to fly once, for a reason, with everything ready.** Never to "see what happens."
- **A test card written for a pilot** — what to fly, in what order, and what you are looking for.
- **A read of the data by the agent**, from disk, without him being asked to do the agent's job.
- **The truth about what the data showed**, including "this did not measure what I hoped."
- **Never claim readiness that has not been demonstrated.** If asked "is it ready?", the honest
  answers are *"yes, here is the smoke test output"* or *"no."* There is no third answer.

---

## THE HARNESS CONTRACT — determinism, and no assumption anywhere

**Chad's ruling, verbatim, 2026-08-02:**

> *"I want to set this recorder as a properly coded in harness. Like it wont allow any assumption
> as a deterministic function and holds this SOP about proper primary data collection as complete
> and analysable but mechanistic determinism. No work can be that relies on fly testing without
> the full program of data collection properly verified as built, running and tested using
> scientific control for analysis of the data in comparison to a control. The proper scientific
> and objective method of analysis."*

The recorder is not a set of print statements. It is a **harness with a contract**, and the
contract is enforced by the code, not by an agent's care. Anything an agent could forget, the
harness must refuse.

**H1 — Deterministic.** Same state in, same row out. Fixed field order, fixed precision, a
versioned schema string in the header. No field may be conditional, reordered, or omitted between
rows. A row is a fixed-width contract, not a message.

**H2 — Self-verifying.** The harness proves its own integrity in the data it writes: a monotonic
row index so a gap is detectable, a terminator on every row so truncation is detectable, a field
count assertion, and an unconditional heartbeat that prints **even when nothing is recording**, so
silence and absence are distinguishable. **Silence must never be ambiguous.**

**H3 — Fails loud, never silent.** If a precondition is unmet the harness says so, in the log, on
every frame it is unmet. It may never no-op quietly. The 2026-08-02 relay printed
`sent N bytes` for a send that never happened — **reporting success for work not done is the
single worst defect a harness can have**, and it is banned outright.

**H4 — Refuses rather than assumes.** Where a value cannot be sourced, the harness emits an
explicit sentinel and names the field. It may never emit a plausible substitute, a default, or a
zero standing in for "unknown". **A zero that means "not measured" is a lie in a column that reads
as a measurement.**

**H5 — Observation only.** The harness may not alter one flight quantity. A recorder that changes
the flight invalidates every measurement taken with it.

**H6 — No gates.** Nothing that matters sits behind a keypress, a flag, or a global someone must
remember. If a flag must exist, arming it is a mechanical gate item with proof.

**H7 — Self-identifying.** Every session writes a header carrying the build hash and every config
value in force. **Data that cannot be tied to what produced it is not evidence.**

**H8 — Proven by fault injection.** Every detector is proven by forcing a known synthetic event
and observing detection. **A clean run on quiet data proves nothing.** A null from a detector not
proven this way is *no data* — never "no defect found".

---

## SCIENTIFIC CONTROL — no conclusion without a comparison

**A measurement without a control is an anecdote.** No change is accepted, and no defect is
attributed, on a single arm of data.

**C1 — Every experiment has a control arm.** The same test card, the same pilot, the same session,
with only the one variable changed. The control is flown, not remembered, and not inherited from a
previous build.

**C2 — One change per flight.** (This project's existing S28 SOP.) Two changes in one sortie make
attribution impossible, and the data cannot be rescued afterwards by argument.

**C3 — Pre-registration.** Before the flight: the hypothesis, the statistic, the threshold that
counts as a difference, and the sample-size floor are written down. A threshold chosen after
seeing the data is not a result.

**C4 — The comparison is the result.** Report the difference between arms with its n, not an
absolute number read off one tape. An absolute number has no meaning without the control that
gives it scale.

**C5 — Blind where possible.** Where the pilot's report is the instrument, he should not be told
which arm he is flying. A/B arms are flown in an order recorded by the harness, not announced.

**C6 — Below the floor is a result.** If n does not reach the pre-registered floor, the finding is
**NOT ESTABLISHED** and must be reported as that. It may not be softened into a suggestion, a
trend, or a direction. The analysis tooling enforces this rather than trusting the analyst.

**C7 — The analysis may not share a bug with the code it measures.** A check that repeats the
implementation's own error passes green while the system is broken. This has already happened in
this project once, on a units error.

**C8 — Pilot report versus instrument.** Where they disagree, the pre-registered rule is that
**the instrument is wrong and gets rebuilt.** The pilot's hands are the reference, not the
suspect.

---

## THE STANDING BAR ON FLIGHT WORK

**No work that relies on fly testing may proceed until the full data-collection program is
verified as built, running, and tested — with a control arm and an objective method of analysis
defined in advance.**

Built, running and tested are three separate things and each needs its own evidence:
**built** = the code exists and passes static checking; **running** = it produced real rows from
the real build, read back off disk this session; **tested** = fault injection proved every
detector sees a known event, and the analysis tooling was run end to end on that fixture.

---

## ENFORCEMENT

A flight request that does not carry the G1–G10 evidence block is **void**, and the Pilot should
refuse it. An agent that asserts readiness it has not demonstrated has committed the defect this
SOP exists to prevent, regardless of whether the flight happens to succeed.

Verification of any kernel or cascade change happens **only** from complete, comprehensive
recorded flight data meeting this standard. No change is accepted on reasoning alone.
