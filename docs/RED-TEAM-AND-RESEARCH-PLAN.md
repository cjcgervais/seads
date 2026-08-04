# THE RED TEAM AND THE RESEARCH PLAN — 2026-08-04, on Chad's word

**Chad's intent, verbatim. This is the specification. Quote it, never paraphrase it:**

> *"ANYTHINGN FO THE ARCHITECT... WE SHOULD RED TEAM EVERYTHING AND DO AN AUTORESEARCH DREAM PLAN
> TO FIND OUT WHAT IS WRONG. jUST EVERYONE TAKE MY INTENT. i WANT A CASCADE THAT i CAN FLY AND
> PROPERLY RECORD THATS ALL. fOR AN EAGLE."*

**Two requirements, and they are the whole of it: he can FLY it, and it RECORDS PROPERLY.**
Everything below is ranked by distance from those two words. **Anything that does not serve
"fly" or "record properly" is not on this plan.**

**"Just everyone take my intent"** — so this document does not ask him a single question. Every
open choice below is made, with the reasoning stated so it can be argued with by an agent, not by
him.

---

## 0. ⭐ THE HEADLINE — one sortie can earn three gates, and the third is the one nobody planned

**The recorder's largest known blind spot is FIXED, its own author wrote down exactly what would
prove the fix, and that proof has never been flown.**

`cascade-recorder`'s commit `f64b81a` — *"nine columns stopped lying"* — is excellent work, and
its message ends with a warning it wrote against itself:

> ⛔ *"This will move G18 from RED to GREEN, and that is ONLY legitimate once fault-injected. A
> recorder change that turns a red board green is the manufactured-PASS shape this project has
> shipped twice. **The success test is one dry free-look block: those nine columns must VARY or
> read 0 within the episode. Until that is flown, the GREEN is unearned and must not be quoted.**"*

**`MEASURED` here today: the only flown v3 tape contains ZERO free-look frames.**
`keyMask` takes exactly two values across all 5,173 rows — `{0, 64}`. **Bit 256 never sets.**

**So the proof its author demanded has not happened, and cannot have.** Nine columns —
including `mouseDx`/`mouseDy`, the entire record of what the pilot's hand did — behave one way on
every tape we hold and a different way on the path **Chad actually flies**. The whole v9 camera arc
is about free-look releases. **He free-looks constantly. The recorder has never been watched while
he does.**

### The plan that falls out, and it costs zero extra flights

**The fly card already opens with a free warm-up that is explicitly not recorded as either arm.**
Put a deliberate **free-look block** in that warm-up — recorded, but as a *recorder-verification*
block, never as arm data. One sortie then earns:

1. **G-4** — the claim-2 A/B (its two blocks, unchanged, untouched by any of this).
2. **G-2 end-to-end** — the treatment build proves it can emit a tape at all.
3. **G18 / the free-look freeze** — the green its author refused to claim, **earned on the path
   Chad flies**, and earned *before* the arms so a defect stops the sortie rather than spoiling it.

**Nothing about the registration, the arms, the statistic or the blinding changes.** The warm-up
was already outside the data by rule; this only makes it *recorded* rather than discarded.

---

## 1. WHAT IS WRONG — the honest list, ranked by distance from "fly and record properly"

| # | what is wrong | serves | owner | state |
|---|---|---|---|---|
| **W1** | **A treatment build has never emitted a tape.** Unit tests (658/1) and a linter are not `G2`. `G2` is end-to-end: the build emits rows and an agent reads them **off disk**. This is the 2026-08-02 lost sortie exactly | **fly** | cascade-recorder | **THE ONLY THING BETWEEN CHAD AND FLYING** |
| **W2** | **The free-look freeze fix is unproven** (§0). Nine columns, `mouseDx`/`mouseDy` among them, on the path he actually flies | **record** | cascade-recorder | fixed at `f64b81a`, **unearned green** |
| **W3** | **The tape header is truncated at 1,022 chars**, losing `keybit32/64/128/256` — **the legend for the free-look bit itself**. Rows are fine (max 323). W2 and W3 are the same axis: the one record that overflowed is the one nobody checks | **record** | harness | F1, open |
| **W4** | **The jitter is LOCATED but not FIXED.** Roll, the aim channel, commanded upstream of the plant. No fix designed, none registered. *Located ≠ buttery* | **fly** | kernel-docs + cascade-recorder | open, and it is the main drive |
| **W5** | **"The cascade" and "the eagle" each have two referents.** Sandbox vs game; doc vs code. This has already caused one two-writers-on-one-file incident (`V024`) | both | kernel-docs | §3 below settles it |
| **W6** | **Nothing has been adversarially verified end-to-end by an agent that did not build it.** `G-10` open, `G-3` board open (23/28 green, 2 red, 3 pending), `G-5`'s `C3` RED unledgered | both | architecture, harness | open — **this is the red team** |
| **W7** | **The `-dirty` in the build stamp is the stamp itself** — the artifact whose only job is recording what the build was is what makes it look unreproducible | record | cascade-recorder | found by architecture (P15 §4); fix in flight |

**W1 and W2 are the whole of Chad's sentence.** W1 is "fly." W2+W3 are "properly record."
**Everything else on this board can wait behind those three.**

---

## 2. THE RED TEAM — everyone, and nobody grades their own work

**The rule, and it is the only rule that makes a red team mean anything:**
**no agent verifies what it produced.** Assignments are chosen so each reviewer is the one with
the least stake and the most independent instrument.

| # | target | red team is | the specific question, not a vibe |
|---|---|---|---|
| **R1** | **The recorder, end-to-end** — does a tape record what the pilot did? | **architecture** | Take one flown tape and one claim per column class. **Can the tape prove its own contents without consulting the emitter?** `G-2`'s F1 says no for the header. Sweep the rest |
| **R2** | **The `f64b81a` free-look fix** | **harness** | Fault-inject it. Its author says the green is unearned without it and is right. **Does the check fail when the freeze is reintroduced?** A check that cannot fail is not a check |
| **R3** | **The G-4 experiment as built** — registration, arms, fly card | **architecture** | It has already been wrong once today (see §4). **Assume it is still wrong.** Re-derive the one-variable property from the two tape headers, not from the TOML |
| **R4** | **The jitter attribution** (aim channel, `1.102 /s`) | **cascade-recorder** | It was produced by kernel-docs on cascade-recorder's instrument. **Re-derive the channel split independently.** If it does not reproduce, it is not a finding |
| **R5** | **This agent's own docs and graph** | **architecture** | It has landed four self-caught defects today (§4). **Assume a fifth.** Start with `audit_graph`'s pairing and CONTESTED logic |
| **R6** | **The eagle's `M1` claims** | **kernel-docs** | Reference implementation is judged by whether it *demonstrates the spec*. `M1-PLANT-010` is UNDEMONSTRATED — neither deleted nor satisfied. **Do not let it drift to "satisfied" by repetition** |

**Every red-team result lands as a packet with an `**Answers:**` declaration, so the board can
credit it.** A red team whose result the board cannot see is the defect this program fixed today.

---

## 3. ⛔ THE TWO-REFERENT AMBIGUITY — settled here, not sent to Chad

**"For an eagle" has two possible readings and I am not asking him which.** His intent is the
tiebreaker and it is unambiguous: *"a cascade that I can fly."*

- **The `eagle` sandbox** (`D:/mandalark-kernel_sandbox_eagle`) is a **reference implementation**
  proving `MANDALARK1`. **Chad cannot fly it.** It has no game around it.
- **EvC2026** — *"Eagle to the Rescue"* — is **the game with the eagle in it, and it is the thing
  he flies.**

**Therefore "for an eagle" = the EvC2026 / cascade-recorder track**, which is exactly where `G-4`,
the recorder and the fly card already are. **This is consistent with `GOAL §3`'s ruling, not a
reversal of it** — no ruling is being overturned and none needs to be.

**The standing wording rule, because this has already cost a two-writer collision:** never write
"the eagle" or "the cascade" unqualified. **Say which tree and which artifact.** A path plus a
branch plus a commit is an identifier; a name is not.

---

## 4. WHY THIS PLAN ASSUMES ITS OWN AUTHORS ARE WRONG

**Five defects were caught today, and four of them were in the work of the agent that found
them:**

- `audit_graph` could not credit a named packet — **found in this agent's own instrument**, and it
  had been hiding a six-packet backlog.
- The declaration scanner then **silently truncated a wrapped list** — the same class, one commit
  later.
- The CONTESTED check **reported a file as disputed on a row that said the opposite**.
- `CLAIM2-ROLL-IN-VERTICAL.toml` claimed `0.000` *"structurally zeroes the dwell channel."* **It
  does not** — found by `cascade-recorder` while obeying it.
- `architecture` exported `git archive HEAD`, labelled it with a stale sha, and **refuted two of
  its own published conclusions** when it re-ran pinned.

**That is the base rate this plan is built on.** Six red-team assignments, none self-graded, and
an explicit instruction to each reviewer to assume the thing in front of them is wrong. **The
program's failure mode is not laziness — it is agents diagnosing a class and then producing an
instance of it within the hour.** `flying_architecture` named it best: *"diagnosing a class does
not immunise you against it."*

---

## 5. SEQUENCE — what actually happens, in order

1. **`cascade-recorder`: the treatment dry run** (W1). Emit a tape, read it back **off disk**,
   confirm the header carries `dwellLevelRateMult=0.000` byte-for-byte. **Clears `G2`/`G10`.**
2. **In the same dry run: a free-look block** (§0/W2). Nine columns must **vary or read 0** within
   the episode. **Clears the green its author refused to claim.**
3. **`harness`: fault-inject that check** (R2) — reintroduce the freeze, confirm the check fails.
   **A green that cannot go red is not evidence.**
4. **THE CARD ISSUES. Chad flies once.** Warm-up, free-look block, then the two blind blocks.
5. **Red team fans out** (§2) — R1, R3, R5 to `architecture`; R4 to `cascade-recorder`; R6 to
   `kernel-docs`. These do **not** block step 4; they run alongside.
6. **Then, and only then, the jitter fix** (W4). The aim channel is located; a fix is a
   registration written **before** the run that scores it, and the acceptance test already exists:
   this statistic against v12's `1.03 /s`.

**Steps 1–3 are hours of desk work, not a sortie.** Step 4 is the first time Chad flies since
2026-08-02, and by then three gates are green instead of one.

---

## 6. WHAT THIS PLAN REFUSES TO DO

- **It does not re-point `G-4`.** The registration is locked; the aim-channel finding does not
  re-aim it; `C3` forbids it. The narrowing to *levelling drive* is a prose amendment, already
  landed, moving no scored element.
- **It does not ask Chad to choose between mechanisms.** *"Just everyone take my intent."*
- **It does not add a bar, a gate or a doc that serves neither "fly" nor "record properly."**
  This program's real risk now is not that it moves too fast — **it is that the apparatus grows
  faster than the flying.** Chad has not flown since 2026-08-02. **Every item above either gets
  him into the air or makes the tape from that flight trustworthy. Nothing else qualifies.**
