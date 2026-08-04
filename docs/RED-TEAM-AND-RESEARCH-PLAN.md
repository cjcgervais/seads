# THE RED TEAM AND THE RESEARCH PLAN — 2026-08-04, on Chad's word

**Chad's intent, verbatim. This is the specification. Quote it, never paraphrase it:**

> *"WE SHOULD RED TEAM EVERYTHING AND DO AN AUTORESEARCH DREAM PLAN TO FIND OUT WHAT IS WRONG.
> jUST EVERYONE TAKE MY INTENT. i WANT A CASCADE THAT i CAN FLY AND PROPERLY RECORD THATS ALL.
> fOR AN EAGLE."*

**And his correction of this document's first draft, same day, which is why it was rewritten:**

> *"WHY ARE WE THINKING ABOUT FREE LOOK? i JUST WANT A CASCASE FREE LOOK WORKS FINE"*

---

## 0. ⛔ THIS PLAN'S FIRST DRAFT WAS WRONG AND THE ERROR IS RECORDED, NOT QUIETLY FIXED

**The first draft made a free-look recorder defect its headline and reordered the whole board
around it.** Chad struck it in one line: **free-look works fine, and he asked for a cascade.**

**He is right, and it is worse than a bad priority call — it broke a standing ruling this agent
had written down itself and quoted hours earlier the same day.** From `DECISIONS.md`, his words,
ruled **twice** on 2026-08-03:

> *"mOUSE AIM CASCADE ONLY AS IT AFFECTS THE PLANT CO0ME ON!"*

and the standing instruction attached to it: **"Keyboard and camera and free-look are OUT OF
SCOPE. Do not reopen it, do not re-document it, do not raise defects in it, and do not drag it
into cascade work."** **The draft did exactly the last one.** `SESSION_HANDOFF §7` names the
mechanism: *his words are the specification; do not translate them into mechanism language and
then reason from your translation.* **A defect being real does not make it in scope.**

**The free-look column-freeze finding is factually true and is NOT deleted** — it is demoted to
where it belongs: a known limit, out of scope, owned by `cascade-recorder`, **gating nothing**,
listed in §6. **It is not on the critical path and no agent should spend a day on it.**

**The filter, restated and this time actually applied: a CASCADE he can FLY and that RECORDS
PROPERLY.** Cascade = **mouse aim as it affects the plant.** That is the subject. Everything else
is backlog.

---

## 1. ⭐ WHAT IS WRONG — the in-scope list

| # | what is wrong | serves | owner | state |
|---|---|---|---|---|
| **W1** | **A treatment build has never emitted a tape.** Unit tests (658/1) and a linter are not `G2`. `G2` is end-to-end: the build emits rows and an agent reads them **off disk**. This is the 2026-08-02 lost sortie exactly | **fly** | cascade-recorder | **THE ONLY THING BETWEEN CHAD AND FLYING** |
| **W2** | ⭐ **THE JITTER IS LOCATED AND NOT FIXED.** Roll is the only axis over v12's bound — `1.485 /s` vs `1.03`. It is **commanded, not plant-generated** (`rollOut` reverses *more* than the plant), and it is the **aim channel** (`rollAimApp` `1.102 /s` alone; the dwell servo `0.093 /s`). **This is the cascade, and it is what "buttery" means in numbers** | **fly** | kernel-docs + cascade-recorder | **THE ACTUAL WORK** |
| **W3** | **The `G-4` question is one dry run from being answerable** — does a straight-up pull have roll in it | **fly** | cascade-recorder | card written, not issuable |
| **W4** | **The header is cut at exactly 1,022 chars with no terminator.** `MEASURED`: payload 943 chars, cut mid-token at `keybit16=yaw`; the last cascade dial ends at char 727, leaving **216 chars ≈ 8 dials of headroom**. All 12 dials survive **because they sit early, not because anything protects them** — and that header is what `G-4`'s one-variable check reads **as exact strings**. `W2`'s fix will add dials | **record** | cascade-recorder (emit) + harness (guard) | F1, open, **reframed in scope** |
| **W5** | **"The cascade" and "the eagle" each have two referents.** Already caused one two-writers-on-one-file incident (`V024`) | both | kernel-docs | §4 settles it |
| **W6** | **Nothing has been adversarially verified end-to-end by an agent that did not build it.** `G-10` open, `G-3` board open, `G-5`'s `C3` RED unledgered | both | architecture, harness | **this is the red team** |

**W1 gets him in the air. W2 is the cascade he asked for. Everything else is behind those two.**

### What is NOT wrong, and is worth saying because it is the part he asked about

**The cascade's own recording is in good shape, and it is measured, not assumed.** On the flown
tape, `reconcile_rollout` passes **5,173 / 5,173** rows, `max|resid|` **1.0e-04** against a 2.0e-04
tolerance — **every roll term the cascade computes reconciles to the command that reached the
plant.** And the tape is pure mouse-cascade **by measurement**: `kbRoll` reverses **0** times,
`keyMask ∈ {0, 64}` — **no flight-axis key held anywhere on it.**

**So "properly record," for the cascade specifically, is largely already true.** The live risk is
`W4` — a header at its limit — not the cascade columns.

---

## 2. THE CASCADE WORK ITSELF — W2, and it is the thing he actually wants

**The jitter has a location and no fix.** Named, not moved: `aimResponse = 13.000` (the one-pole
that smooths aim into `aimApplied`), `aimRollGain = 7.500`, `aimRollDamp = 0.580` — read off the
flown tape's own header, so they are provably what was flown.

**The discipline that applies, and it is Chad's own:** *"DONT SET IT TO ZERO, MAKE THE RECOREDER
TO THE WORK PROERLY TO ANAYSE IT JUST LIKE THE SUCCESS WE HAD FOR V12."* **A dial move is a
registration written BEFORE the run that scores it.** The acceptance test already exists and needs
no invention: **this statistic against v12's `1.03 /s`**, bound `1.1`.

**Sequencing, and it is deliberate:** the fix comes **after** `G-4` flies, for one reason — `G-4`'s
control arm is the aeroplane as it stands, and moving a cascade dial now would destroy the control
arm before it is flown. **This is not caution, it is the same one-variable rule that makes any of
it mean anything.**

---

## 3. THE RED TEAM — everyone, and nobody grades their own work

**The only rule that makes a red team mean anything: no agent verifies what it produced.**

| # | target | red team is | the question |
|---|---|---|---|
| **R1** | **The cascade's recording, end-to-end** | **architecture** | *Can a tape prove its own contents without consulting the emitter?* `F1` says no for the header. **Scope it to the cascade columns and the dial header** — that is what must be trustworthy |
| **R2** | **The `G-4` experiment as built** | **architecture** | Verify against the **two tape headers**, never the TOML. It was already wrong this morning. **Attack the mediator-vs-confounder ruling specifically** |
| **R3** | **The jitter attribution** (aim channel, `1.102 /s`) | **cascade-recorder** | Produced by kernel-docs **on cascade-recorder's instrument**. Re-derive the channel split independently. **If it does not reproduce, it is not a finding — and W2 is built on it** |
| **R4** | **This agent's docs, graph and rulings** | **architecture** | Four self-caught defects today, **plus this document's first draft, caught by Chad.** Assume a fifth. Start with `tools/audit_graph.py` and `docs/agents.tsv` |
| **R5** | **The eagle's `M1` claims** | **kernel-docs** | `M1-PLANT-010` is UNDEMONSTRATED — neither deleted nor satisfied. **Do not let it drift to "satisfied" by repetition** |

**Every result lands as a packet with an `**Answers:**` declaration**, so the board can credit it.

---

## 4. ⛔ THE TWO-REFERENT AMBIGUITY — settled here, not sent to Chad

*"Just everyone take my intent"* means **do not send him a question.** So:

- **The `eagle` sandbox** is a **reference implementation** proving `MANDALARK1`. **Chad cannot
  fly it** — there is no game around it.
- **EvC2026** — *"Eagle to the Rescue"* — **is the game he flies.**

**"For an eagle" = the EvC2026 / cascade-recorder track**, which is exactly where `G-4`, the
recorder and the cascade already are. **Consistent with `GOAL §3`, not a reversal — no ruling is
overturned.**

**Standing wording rule:** never write "the eagle" or "the cascade" unqualified. **Say which tree
and which artifact.**

---

## 5. SEQUENCE

1. **`cascade-recorder`: the treatment dry run** (W1). Emit a tape, read it back **off disk**,
   confirm the header carries `dwellLevelRateMult=0.000` byte-for-byte. **Clears `G2`/`G10`.**
2. **THE CARD ISSUES. Chad flies once** — warm-up, then the two blind blocks. **First flight since
   2026-08-02.**
3. **`cascade-recorder`: R3**, independently re-deriving the aim-channel split — because **W2 is
   built on it** and it was produced by another agent on their instrument.
4. **Then the cascade fix** (W2): a pre-registered aim-channel dial move, scored against v12's
   `1.03 /s`. **After `G-4` flies, so the control arm survives.**
5. **Red team runs alongside**, blocking nothing.

---

## 6. KNOWN LIMITS — real, out of scope, gating nothing

**Listed so they are not lost, and explicitly NOT prioritised. No agent should spend a day on
these without Chad reopening them.**

- **Free-look column freeze.** Nine columns froze during free-look; fixed at `f64b81a`; the fix is
  unproven because no v3 tape contains a free-look frame (`keyMask ∈ {0,64}`, measured).
  **Chad: free-look works fine. OUT OF SCOPE by his ruling of 2026-08-03, restated 2026-08-04.**
  Owner `cascade-recorder`, whose own commit already states the success test. **It waits.**
- **The `-dirty` build stamp is the stamp itself** (architecture, `PACKET-15 §4`). Fix in flight.
- **`C3`'s v1-archive over-claim** — `audit_graph`'s only RED, marked EXPECTED. `harness`, `G-5`:
  ledger it with an owner and a date.

---

## 7. WHY THIS PLAN ASSUMES ITS OWN AUTHORS ARE WRONG

**Today's defects, all caught, four of them in the finder's own work:** `audit_graph` could not
credit a named packet, hiding a six-packet backlog; the scanner written to fix it silently
truncated a wrapped list; the CONTESTED check reported a file as disputed on a row saying the
opposite; the `CLAIM2` registration misdescribed its own manipulation; `architecture` exported a
moving `HEAD` and refuted two of its own published conclusions.

**And the fifth was this document, caught by Chad, for dragging an out-of-scope mechanism into
cascade work after ruling it out of scope in writing.**

**That is the base rate.** Five red-team assignments, none self-graded, each told to assume the
thing in front of it is wrong. `flying_architecture` put it best: **"diagnosing a class does not
immunise you against it."**
