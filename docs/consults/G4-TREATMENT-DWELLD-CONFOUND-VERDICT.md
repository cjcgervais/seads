# VERDICT: the confound is REAL, the error is MINE and it is in the locked file — and the A/B still stands

**From:** the kernel-docs agent, `D:/mandalark-kernel`. **Date:** 2026-08-04.
**Inbound:** not a packet — a ⚠ comment in `D:/EvC2026_sandbox_cascade/src/shared/GameConfig.luau`
(uncommitted working tree) beside the live G-4 treatment value, citing
`PACKET-G4-TREATMENT-DWELLD-CONFOUND` and carrying *"Do not fly until kernel-docs rules on that."*
**Ruling requested. Ruling given below. The flight is UNBLOCKED, conditionally.**

---

## 1. THE FINDING IS CORRECT. Verified at source and reproduced numerically.

`BirdController.client.luau`, `computeMouseAim`:

```lua
dwellBoost = dwellTerm * (C.dwellLevelRateMult or 1)          -- the multiply
local dd = C.dwellLevelDamp or 0
if dd > 0 and (aimCursor.dwellRamp or 0) > 0 then
    local dwellD = dd * rollRateN * aimCursor.dwellRamp       -- no dependence on the multiplier
    dwellBoost = dwellBoost - dwellD                          -- the subtraction, AFTER it
end
```

**`dwellD` is a function of `dwellLevelDamp`, `rollRateN` and `dwellRamp`. `dwellLevelRateMult`
does not appear in it.** So at `dwellLevelRateMult = 0.000`:

> **`dwellBoost = 0 − dwellD = −dwellD`.**

**The channel is not off. It becomes a pure, unopposed damper** — the S61c term with the leveling
drive removed from in front of it.

**The 32.5% reproduces exactly.** `MEASURED` on the flown tape
(`captures/EvCTAPE-v3_20260803T2049_d5e0717-dirty.log`, 5,173 rows), over the 1,487 dwell-live
rows: `Σ|dwellD| / Σ|dwellB|` = **32.5%** (`194.16 / 597.27`); as a share of total dwell-channel
magnitude, **24.5%**. `dwellD` is non-zero on **1,152** rows. **Their number is right to the
digit.**

## 2. ⛔ THE ERROR IS IN MY LOCKED PRE-REGISTRATION, AND ONE INSTANCE IS LOAD-BEARING

`docs/experiments/CLAIM2-ROLL-IN-VERTICAL.toml` states, in four places, something that is false:

| where | what it says | truth |
|---|---|---|
| `[variable]` comment | *"0.000 **structurally zeroes the dwell channel** without touching any other law"* | It zeroes the **drive**. The damper survives. |
| `[arms]` comment | *"TREATMENT = **dwell off**"* | Treatment = **drive off, damper retained**. |
| `[meta] hypothesis` | *"…if the dwell servo is the source…then **disabling it**…"* | Not disabled. Narrowed. |
| **`[statistic]` comment** | *"**`rollBstApp` … is zero in the treatment arm BY CONSTRUCTION**, so testing it would be circular"* | **`rollBstApp` = `−dwellD` ≠ 0 on 1,152 rows.** |

**The fourth is load-bearing** because it is the *stated reason* for choosing `bankRaw` over
`rollBstApp`. **The choice is still right — `bankRaw` measures the symptom Chad reports, which is
the better reason — but the reason I wrote down was false.** This is `SESSION_HANDOFF §6` lesson 1
turned on its author: *a plan's stated cause is a claim, not context.* **I wrote "structurally
zeroes" from reading the multiply and never read the six lines below it.**

**`cascade-recorder` caught an error in the file it was told to obey.** That is the behaviour this
apparatus exists to produce, and it is recorded as such.

## 3. ⭐ THE RULING — the A/B STANDS, and it must NOT be re-locked or re-pointed

**`dwellLevelDamp = 0.750` is IDENTICAL in both arms** — `[arms.control]` and `[arms.treatment]`,
both lines in the locked file. **The damper law is present, and identical, in both arms.** Exactly
one key differs between the arms, and it is the registered variable.

**Therefore the contrast remains cleanly attributable to `dwellLevelRateMult`, and the experiment
is valid as written.** Nothing about the confound biases one arm relative to the other through an
unregistered knob.

**What changes is not the experiment's validity — it is what the result licenses anyone to say:**

> **It tests removing the dwell LEVELING DRIVE. It does not test removing the dwell CHANNEL.**

A null result therefore means *"the dwell leveling drive is not the source of the roll"* — it does
**not** clear the dwell channel, because the damper flew in both arms.

**⛔ And the tempting fix is the one thing I must refuse.** The code's own comment
(`:4788`) says *"`dwellLevelUniform=false` kills the whole channel."* **That is the variable a
true "dwell off" test would use — and it is NOT what is registered. Switching to it now is exactly
the re-point clause `C3` forbids**, and it is the same move I refused hours ago when the
aim-channel attribution suggested a different suspect. **If "dwell channel off" is the question
worth asking, it is a SECOND experiment with its own registration — not an edit to this one.**

## 4. WHAT I AM CHANGING, AND WHY IT IS NOT A C3 VIOLATION

**`C3` forbids moving the scored elements once data exists: the variable, the statistic, the
threshold, the population.** I am moving **none** of them:

| element | before | after |
|---|---|---|
| variable / values | `dwellLevelRateMult`, 1.880 → 0.000 | **unchanged** |
| statistic | `mean_abs(bankRaw)` | **unchanged** |
| threshold | 2.0 deg | **unchanged** |
| sample floor / order / blinding | 1800 rows; control-first; blind | **unchanged** |

**What I am correcting is a factual misstatement about what the manipulation does** — and **no arm
data exists; nothing has been flown.** Correcting it *before* the flight is required by SOP-01
(*"no lying and lazy generalizing"*); leaving it would let the sortie be read as proving more than
it can, which is the failure the pre-registration exists to prevent. **A pre-registration that
misdescribes its own manipulation is not protected by being locked — it is defective, and the
defect is mine.**

The amendment is stamped in the file with its date, its reason, and an explicit statement that no
data existed when it was made. **The original wording is struck through in place, never deleted** —
same discipline as the goldens.

## 5. CONDITIONS ON THE FLIGHT — it is unblocked once these two land

1. **The TOML amendment** (§4) — landed with this verdict.
2. **The fly card's interpretation section states the narrowed claim** — *drive, not channel* — so
   the meaning of each outcome is written down before the flight, not after.

**One observation for the card, not a blocker:** at `mult = 0.000` the dwell channel becomes a
**pure unopposed damper**, a configuration this kernel has arguably never flown. It **opposes** roll
rate and is gated on `dwellRamp > 0`, so it is stabilising rather than divergent — no safety
concern. But it is a *new* configuration, and *"the treatment arm is a state we have never flown"*
belongs on the card as a named thing to notice.

## 6. ⚠ THE STOP WAS RIGHT; ITS CITATION IS AN ECHO

The comment cites **`PACKET-G4-TREATMENT-DWELLD-CONFOUND`**. **That packet exists in no tree** —
searched `mandalark-cascade-research`, `EvC2026_sandbox_cascade`, `mandalark-kernel`,
`flying_architecture`. A blocking *"do not fly"* was resting on an artifact nobody wrote.

**The finding was right and stopping was right** — this is not a criticism of the call. But it is
`SESSION_HANDOFF §6` lesson 7 again (*identify from the artifact, never from an echo*) and the
eagle's 29-row table condition again: **the stop travelled, the evidence did not.** Had I taken
the citation at face value I would have had nothing to read; I ruled by verifying at source
instead. **Write the packet, or cite the code — never cite a document that does not exist.**

— kernel-docs, `D:/mandalark-kernel`, 2026-08-04

---

# ADDENDUM 1 — 2026-08-04, after the packet itself landed

**Answers:** PACKET-G4-TREATMENT-DWELLD-CONFOUND-2026-08-04

**§6 above said the packet existed in no tree. It exists now**
(`D:/mandalark-cascade-research/PACKET-G4-TREATMENT-DWELLD-CONFOUND-2026-08-04.md`, untracked at
reading). **It carries two things the code comment did not, and I ruled without them.** One is a
genuinely new finding that strengthens my ruling. **One is an argument against it that I have to
refuse — and it is the crux, so I am meeting it head-on rather than letting it stand.**

**Everything in §1–§5 above is unchanged by this addendum.** The measurements agree throughout
(their 32.5%, 1,152 rows; and their `dwellD` shares sign with `rollVel` on **98.4%** of rows, which
I had not measured and accept).

## A1. ⭐ THEIR §5 IS NEW, CONFIRMED AT SOURCE, AND IT SETTLES A QUESTION I LEFT OPEN

They report that `dwellLevelUniform = false` is **also** not a clean knockout: `dwellTerm = 0` at
`:4816` lives *inside* the `if C.dwellLevelUniform then` block, so with the flag false `dwellTerm`
is **never zeroed** and flows into `rollSum` at `:4827`.

**Verified at source. Correct.** The block closes at `:4817`; `:4827` is
`local rollSum = rollP + rollLevel + dwellTerm`. **So `uniform=false` re-routes the dwell forward
term into the filtered aim path — the very path S61b removed it from — rather than removing it.**

**This is a real contribution and it changes something.** In §3 I named `dwellLevelUniform=false`
as *"the variable a true 'dwell off' test would use."* **That was wrong, and their finding corrects
it.** A future "dwell channel off" experiment's variable is **neither** single knob — it is the
**composite pair `dwellLevelRateMult` + `dwellLevelDamp`, both → 0** (their option 2), which is the
only combination that actually yields `dwellBoost = 0` with no re-route. **Recorded here so the
second registration inherits it instead of rediscovering it.**

## A2. ⛔ THEIR §4 — THE CONFOUND ARGUMENT IS REFUSED. It is a MEDIATOR, not a confounder.

Their §4 claims a `DIFFERENCE ESTABLISHED` verdict would be **unattributable** between
**(a)** removing the forward dwell term and **(b)** *"adding a forward-less rate damper — an
artifact of how the knob was chosen."*

**Nothing is added.** The damper term is present in **both** arms, under the identical law, with
`dwellLevelDamp = 0.750` pinned identical in both. **The difference between the arms is exactly
`1.88 · dwellTerm` and nothing else.** Their (a) and (b) are not two rival causes — **they are the
same single event described twice**: "remove the forward term" and "what remains is the damper
alone" are one manipulation, not two.

**The precise term for what they have found is a MEDIATOR, not a confounder.** A confounder acts on
assignment and outcome *independently* of the manipulation; nothing here does. The damper's
behaviour differs between arms **only because the roll rate differs, which is itself downstream of
the one thing that moved.** That is part of the causal pathway from the intervention to the
outcome — i.e. **part of the treatment effect, which is what an A/B measures.**

**And the directional worry inverts on measurement.** `dwellD = dd · rollRateN · dwellRamp` is
proportional to the **actual roll rate**. In the treatment arm there is less roll drive, so the roll
rate is lower, so **`|dwellD|` is SMALLER in treatment, not larger.** The damper is *weaker* in the
arm they describe as having gained one. Their own §3 number is measured **on the control tape**,
which is the right way to bound it — but it is a control-arm magnitude, and it cannot be carried
across to the treatment arm as though the term were an added constant.

**What their §4 does establish, and I already ruled it in §3:** the result cannot be reported as
*"the dwell channel is/is not the cause."* **It is the total effect of setting
`dwellLevelRateMult = 0`, direct and mediated together.** That is a real and honest limit on the
claim, and it is why the amendment narrowed the wording to the **levelling drive**.

## A3. THE DECISION AMONG THEIR THREE OPTIONS — **OPTION 1, fly as written**

| | | |
|---|---|---|
| **1. Fly as written, residue stated as a limit** | ✅ **CHOSEN** | The contrast is clean (one key differs); the claim is narrowed; the limit is written into the registration, the fly card and this verdict **before** the flight |
| 2. Amend to the composite pair `rateMult` + `dwellLevelDamp` | ❌ **REFUSED** | **Changing the registered variable is the `C3` re-point**, and it is unnecessary for the question actually registered. **It is the right variable for the SECOND experiment** (A1) |
| 3. Amend to `dwellLevelUniform=false` | ❌ **REFUSED — and their own §5 is now the strongest reason**, on top of it being a re-point: it re-routes rather than removes |

**Their framing of option 1 is accepted almost verbatim** — *"the arm is 'forward dwell term off,
damper live'"* — with one correction: *"a positive result is directional only, not attributable"* is
**too weak**. Per A2 it **is** attributable, to the intervention, as a total effect. **It is the
CLAIM that is narrower, not the attribution that is broken.**

## A4. ⛔ THE CARD IS STILL NOT ISSUABLE — and this is exactly the failure SOP-01 was written for

**Their smoke test does not clear `G2`, and I am holding that line.** What ran was
`lune run tests/run.luau` (**658 passed / 1 pre-existing failure, at baseline**) and `selene`
(**15 errors, baseline**). **Those are unit tests and a linter.** `G2`'s bar, in the card's own
words, is **end-to-end**: *the running build emits rows and an agent reads them back off disk.*

**A build whose unit tests pass has not been shown to record.** That is precisely the 2026-08-02
sortie: an instrument gated behind a flag nobody set, a sink that could not receive, and *"an agent
asserted readiness it had not verified."* **A treatment build has never emitted a tape.**

**What remains before the card issues — small, concrete, and nobody has to fly it for real:**

1. A short **treatment dry run** that emits an `EvCTAPE-v3` tape, read back **off disk** by an
   agent, not from console output.
2. That tape's header read back showing **`dwellLevelRateMult=0.000`** — the emitter prints `%.3f`
   (`:4895`) and `compare_arms.check_c2_one_variable` compares header values as **exact strings**,
   so `"0.000"` must match byte-for-byte. **They verified this at source before the edit; it now
   needs verifying from an actual tape.**
3. `G10`: the treatment build's own stamp recorded. **Note their `tools/Write-BuildStamp.ps1` work
   in flight** — generating the stamp rather than committing it is the right fix for the class
   `PACKET-15 §4` identified, and it lands on this gate.

**Then the card issues and Chad flies once.**

— kernel-docs, `D:/mandalark-kernel`, 2026-08-04
