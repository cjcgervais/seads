# SOP-02 — HANDOFF STANDARDS

**Status:** binding. Subordinate to [SOP-01](SOP-01-PRIMARY-DATA.md), which outranks everything.

**The rule in one sentence:**

> **A handoff exists to make the next session productive in five minutes without trusting a
> word of it — every claim it makes is either reproducible by a command or labelled as
> unverified.**

---

## WHY THIS EXISTS

This project has repeatedly lost days to handoffs that read well and were wrong.

- A plan and a state-of-play both said the kernel must be rebuilt from scratch because the plant
  was decoupled. **v12/SEADS works and was never the problem** — the cascade was, and only after
  a port. Two sessions worked the wrong problem from a confident document.
- A 806-line cascade specification diagnosed a crab term the eagle **already nulls**, and never
  once mentioned the knob that nulls it. It also reversed eight of the pilot's standing rulings
  without citing any of them.
- A handoff asserted a data sink was ready. It could not receive anything, and a sortie was lost.
- A figure — "11 of the 16 tests" — was wrong (it is 12) and had already propagated into a second
  document before anyone re-read the source.

**The common failure is not laziness. It is confidence surviving the loss of its evidence.**
A claim written by a session that had the evidence in front of it reads identically to a claim
invented by one that did not. SOP-02 makes the difference legible.

---

## THE STANDARD

### H-1 — Every claim is classed
`MEASURED` (a command was run — cite it) · `SOURCED` (cite `file:line`) · `DERIVED` (show the
step) · `INFERRED` (**must state what would falsify it**) · `SUBSTITUTED`.
**An unclassified claim is a defect. `INFERRED` with no falsifier is a defect.**

### H-2 — Measured means re-runnable
A `MEASURED` claim carries the exact command. If the next session cannot paste it and see the
same thing, it was never measured. **Prefer a command over a number**: numbers go stale silently,
commands do not.

### H-3 — Separate what is known from what is believed
Three headings, always, in this order: **MEASURED**, **CONTESTED**, **UNKNOWN**. Never blend
them into narrative. An `UNKNOWN` may not be resolved by argument in a handoff — if it could be,
it was not unknown.

### H-4 — State what is NOT built
Louder than what is. The expensive failure is a session assuming a component exists. List it
explicitly, including things that were designed but never implemented.

### H-5 — Name the pilot's standing rulings, verbatim
Chad is the authority; his words outrank every document. Quote them. Never paraphrase a ruling.
Any recommendation that contradicts one must say so in the same sentence.

### H-6 — A minimum reading list, in order
Exact paths, with one line each on why. **Everything not on the list is archived.** A handoff
that implies "read the directory" has failed — context is the scarce resource and an unread
document is indistinguishable from a wrong one.

### H-7 — One concrete next action
Not a menu. What to do first, and what proves it worked.

### H-8 — Declare the author's own reliability
State where you are least confident and why. A session deep in context is the least reliable
narrator of its own work, and should say so.

### H-9 — Record failures and dead ends
What was tried and did not work, so it is not retried. Dead ends are expensive to rediscover and
cheap to write down.

### H-10 — No flight claim without a green gate
A handoff may not describe the build as flight-ready unless `gate.py` printed GREEN and its
output is pasted. SOP-01 CCP-1.

---

## WHAT A HANDOFF MUST NOT DO

- **Assert readiness that was not demonstrated.** The only honest answers to "is it ready?" are
  *"yes, here is the output"* and *"no."*
- **Report a null from an unproven instrument as an absence.** A detector not proven by fault
  injection produces **no data**, never "no defect found."
- **Carry a number without its provenance.** Every figure names the run or the source that
  produced it, and the kernel identity it was measured on.
- **Recommend reversing a pilot ruling without quoting the ruling.**
- **Pad.** Length is not thoroughness. Every line that does not change what the next session does
  is a line that pushes out one that would.

---

## THE SHAPE

```
1. STATE IN TEN LINES        what is true now, each with its proving command
2. THE LAW                   SOP-01 in five lines, and why it exists
3. BUILT AND TRUSTED         only what someone re-verified. SUSPECT listed separately
4. NOT BUILT                 explicit, including designed-but-unimplemented
5. STANDING RULINGS          the pilot's words, verbatim
6. OPEN QUESTIONS            ranked; for each, what would close it and whether it needs a flight
7. NEXT ACTION               one paragraph, concrete, with its success test
8. READING LIST              exact paths in order; everything else is archived
9. AUTHOR'S UNCERTAINTY      where this document is weakest
```

---

## THE TEST OF A HANDOFF

> **Could a competent stranger, having read only this and the files it lists, take the next
> action correctly — and detect it if this document were lying?**

If the second half fails, the handoff is a story, not a handoff.
