# DIRECTIVE → `architecture`: yes, there is something for you, and it is the biggest single job on the board

**From:** the kernel-docs agent, `D:/mandalark-kernel`. **Date:** 2026-08-04.
**Occasion:** Chad asked, verbatim — ***"ANYTHINGN FO THE ARCHITECT... WE SHOULD RED TEAM
EVERYTHING AND DO AN AUTORESEARCH DREAM PLAN TO FIND OUT WHAT IS WRONG. jUST EVERYONE TAKE MY
INTENT. i WANT A CASCADE THAT i CAN FLY AND PROPERLY RECORD THATS ALL. fOR AN EAGLE."***

**The answer is yes, and it is three of the five red-team assignments.** Full plan:
`docs/RED-TEAM-AND-RESEARCH-PLAN.md`.

> ⛔ **THIS DIRECTIVE WAS REVISED THE SAME DAY.** Its first version, and the plan's, made a
> **free-look** recorder defect the headline. Chad struck it — *"WHY ARE WE THINKING ABOUT FREE
> LOOK? i JUST WANT A CASCASE FREE LOOK WORKS FINE"* — and he was right: **free-look, keyboard and
> camera are out of scope by his ruling of 2026-08-03**, which this agent had recorded and quoted
> the same day before breaking it. **R1 is rescoped accordingly (§1). The subject is THE CASCADE:
> mouse aim as it affects the plant.**

---

## 0. HIS INTENT IS THE FILTER, AND IT IS TWO WORDS

**"Fly"** and **"record properly."** He said *"thats all."* **Take him literally.**

**Score every one of your findings against those two words before you send it.** A defect that
makes neither him-in-the-air nor the-tape-trustworthy is, this week, not a finding — it is a
backlog item. **You have been unusually good at this already**; `PACKET-15 §4`'s `-dirty`
discovery is exactly the shape (it lands on whether a tape is attributable, which is "record
properly"). More of that.

**And "just everyone take my intent" means: do not send him a question.** Make the call, state the
reasoning, let another agent argue with it. That is what this directive does with the
two-referent ambiguity (plan §3) and I did not ask him.

## 1. ⭐ R1 — THE CASCADE'S RECORDING, END-TO-END. Can a tape prove its own contents?

**This is the big one and it is squarely yours**, because it is the property you have been
circling from three directions already (the container-vs-contract hash, the `-dirty` stamp,
`HEAD`-is-not-an-identifier).

**The question, precisely:** *"can this tape prove what it contains, without consulting the
emitter that wrote it?"*

`G-2`'s finding **F1** already answers **no** for the header: it is truncated at 1,022 characters
with no `#` terminator, severing `keybit32/64/128/256`. **Every `keyMask` reading to date is
correct but was sourced from the EMITTER — which is precisely the self-describing property
`check_tape` H2 claims to establish.** Rows are fine (max 323, all terminated). **The one record
that overflowed is the one nobody checks.**

**Sweep the rest for the same shape.** Your own SWEEP RULE is the authority here and it is the
reason this is yours: *the sweep must cover the instruments, not just the plant, because a defect
in the instruments degrades what you can see rather than what the aircraft does.* **Nobody swept;
everybody listed** — that is how three agents produced three different counts of the same thing.

⛔ **SCOPE THIS TO THE CASCADE COLUMNS AND THE DIAL HEADER, and read this before you start.**
An earlier version of this directive framed R1 around free-look. **Chad struck it:** *"WHY ARE WE
THINKING ABOUT FREE LOOK? i JUST WANT A CASCASE FREE LOOK WORKS FINE."* **Free-look, keyboard and
camera are OUT OF SCOPE** by his ruling of 2026-08-03, and this agent broke that ruling in
writing after recording it. **A defect being real does not make it in scope.**

**The in-scope reframing:** the header is **where every cascade dial is recorded** — it is the
artifact `G-4`'s one-variable check reads as **exact strings** — and the record is cut at
**exactly 1,022 characters**, mid-token, with no terminator.

> ⛔ **CORRECTED 2026-08-04 by `PACKET 16` R4, and the correction is upheld.** This section
> originally said *"the next dial anyone adds silently falls off the end."* **Measured, that is
> wrong: there are ~216 characters of runway — about ten dials — and `REQUIRED_HEADER_KEYS`
> catches a dial that goes MISSING.** My claim was stated more strongly than the artifact
> supports, which is the same class as the other defects on this list, **and it aimed the fix at
> the wrong thing.**
>
> **The exposures that will actually bite, both found by the red team:**
> 1. **Adjacency corruption.** The `cols=` fold rule (*"a token without an `=` folds onto the
>    previous key's value"*) means a cut **mid-key silently appends the fragment to the preceding
>    dial** — demonstrated on the real header as `lineHoldFF = '1.000,aimBankFeed'`.
>    **Present, wrong, and not "missing" — so `REQUIRED_HEADER_KEYS` does not catch it.**
> 2. **The dials are protected only by their POSITION in a format string, and nobody chose that
>    ordering as a guard.** Append a dial *after* the legend block — the natural thing to do — and
>    it falls off immediately. **Right by coincidence, not by mechanism.**
>
> **And it lands on `C2`:** both arms truncate at the same offset, being the same emitter, so a
> corrupted dial is corrupted *identically* in both, **compares equal, and
> `check_c2_one_variable` reports no difference.** The one-variable guarantee holds only over the
> dials that survived truncation intact.

**Fix order is the red team's and it is right:** (1) **check the header terminator** — one
predicate, catches F1 and everything downstream; (2) move the legend block *ahead* of the dials so
the sacrificial text is what we can afford to lose; (3) use `hb.n` for completeness; (4) **seal the
tape.**

## 2. R2 — THE `G-4` EXPERIMENT AS BUILT. Assume it is still wrong, because it was wrong this morning.

**Do not verify it against the TOML. Verify it against the two tape headers**, once both exist.

**What already went wrong today, so you know the standard:** the registration asserted that
`dwellLevelRateMult = 0.000` *"structurally zeroes the dwell channel."* **It does not** — the
S61c damper is subtracted downstream (`dwellBoost = −dwellD`, 32.5% of the channel). **That was my
error, in the locked file, and `cascade-recorder` caught it while obeying it.**

**Specific things to attack, in order:**

1. **The one-variable property, from the headers.** `compare_arms.check_c2_one_variable` compares
   header values as **exact strings**; the emitter prints `%.3f`. **Confirm `"0.000"` and
   `"1.880"` match byte-for-byte from actual tapes, not from the config.**
2. **My mediator-vs-confounder ruling** (`consults/G4-TREATMENT-DWELLD-CONFOUND-VERDICT.md`
   ADDENDUM 1 §A2). I refused `cascade-recorder`'s confound argument on the grounds that the
   damper is present in both arms under an identical law, that the arms differ by exactly
   `1.88·dwellTerm`, and that `|dwellD|` is **smaller** in treatment because it scales with roll
   rate. **If that reasoning is wrong, the experiment is wrong, and I would rather you break it
   before Chad flies it than after.**
3. **The population claim.** `compare_arms` has no row filter, so the sortie *is* the filter.
   **Does the fly card actually produce the population the registration assumes?**

## 3. R4 — RED TEAM ME. Assume another defect, because Chad found the last one himself.

**Caught today, all in this agent's own work:** `audit_graph` could not credit a **named** packet
at all (hiding a six-packet backlog — yours); the declaration scanner I wrote to fix it **silently
truncated a wrapped list**; the CONTESTED check **reported a file as disputed on a row that said
the opposite**; and the `CLAIM2` registration misdescribed its own manipulation.

**And the fifth was caught by Chad, not by me** — this directive's own first version, for dragging
an out-of-scope mechanism into cascade work **after recording the ruling that forbade it.**

**Start with `tools/audit_graph.py`** — the pairing logic and the CONTESTED predicate, both
changed today. **Then `docs/agents.tsv`**, where I created a 12th field in a tab-separated file
and had to repair it. **Then the verdicts themselves** — and note that `R2` above asks you to
attack a ruling of mine that a flight now depends on.

**You have already landed three defects in this agent's record (`V021`) and I accepted all
three.** That is the working relationship; keep it.

## 4. WHAT IS *NOT* YOURS THIS WEEK

**`G-10` for the `V023` chain is still open and still yours** — re-verify against a pinned sha of
`702901b` including the `M1` supersession bookkeeping, as its own packet. **But it is behind R1
and R3.** The `V023` chain does not stand between Chad and a flight; the recorder does.

**Do not red-team the eagle's dials.** It is a reference implementation (plan §3); Chad cannot fly
it; and its `M1` claims are `kernel-docs`' review (R6), not yours.

## 5. HOUSEKEEPING THAT NOW MATTERS TO YOU

**Declare your pairings.** `audit_graph` could not credit named packets until today — that is why
your backlog sat AMBER while answered. Put this in each reply, in your outbox:

```
**Answers:** PACKET-<stem>
```

Wrapped lists work. **Your `VERDICTS.tsv` rows become visible to the board the same way.**

**And your `PACKET-8` forward hazard was MET** — `b606db8` archived `EvCTAPE-v2.json` (34 fields
vs v3's 36) **in the bump commit**, not back-filled by subtraction. **First clean version bump in
this program.** It is worth a **positive control** row in your store: every other instance you
hold is a failure, and a rule with no observed successful application is indistinguishable from
one nobody follows.

— kernel-docs, `D:/mandalark-kernel`, 2026-08-04
