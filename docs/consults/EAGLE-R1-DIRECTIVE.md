# DIRECTIVE → eagle: **implement nothing. Measure the deflection curve.** R-1 may be malformed.

**From:** the kernel-docs agent. **Date:** 2026-08-03.
**Standing:** Chad's feel signal on R-1, below. **This is a measurement order, which is delegated
(`DELEGATION.md` §1.2). It rules nothing and changes no specification.**

---

## 1. CHAD'S WORDS, VERBATIM — the source, not a paraphrase

> *"I dont know what to rule 80 degrees bank for a 30 degree deflection that will likely arrive
> too slow.. probably have to gho with the crab I feel so bogged down in minutia seems too slow
> anyways"*

**Read as a LEAN, not a ruling** — hedged ("probably", "I feel"), and *going with the crab* would
contradict `M1-PLANT-002`, his own clause and reserved to him (`DELEGATION.md` §2.3). **Nothing
in it is being carried as a decision.** What it does establish:

- **Reading 1 is rejected on feel.** An 80° bank for a routine nudge is not the aeroplane he
  wants, and he expects it to *"arrive too slow"* regardless.
- **He is prepared to reconsider the no-crab rule** rather than accept that manoeuvre.
- **He is being asked too many detailed questions.** That is a defect in how this program puts
  decisions to him, and it is being fixed on this side rather than passed to him as another
  question.

---

## 2. ⚠ THE QUESTION MAY BE MALFORMED — test this before he is asked again

**Every number in R-1 comes from a 30° lateral step.** `M1 §5` uses it, your sweeps use it, the
`80° / 5.8 g` figure derives from it.

**But a 30° aim step is not what Chad flies most of the time.** Tracking is mostly 2–10°
corrections. At 140 m/s a 30° heading change is a large commitment, so **the bench may be testing
the rare case while the feel complaint is about the common one.**

His own phrasing points the same way: *"80 degrees bank for a 30 degree deflection"* reads as
*the response is disproportionate to the ask* — a statement about the **shape of the
deflection→bank relationship**, not about one operating point.

## 3. WHAT TO MEASURE — the curve, not the corner

Sweep aim-step size across at least **2°, 5°, 10°, 20°, 30°** and report, per step size:

| | |
|---|---|
| resolve time | same predicate you already use |
| peak bank | the disproportion Chad is objecting to |
| peak \|β\| | how much crab a coordinated arm actually needs at *small* deflections |
| dip | `SPEC-PRED-007` |

Run **both arms** — `rollout_bank_gain = 0.0` (as-built) and the roll-out law on — so it is
like-for-like.

**What this answers:** if small deflections already resolve promptly *and* coordinated, the crab
is only needed for large steps, R-1 collapses to **reading 3** (deflection-dependent), and
`M1-PLANT-002` may not need relaxing at all. If small deflections are *also* slow coordinated,
his lean is correct and the no-crab rule is genuinely too expensive — a specification question
with a curve under it instead of one operating point.

**Either result shrinks the decision he has to make. That is the entire point of running it.**

## 4. WHAT NOT TO DO

- **Do not implement, do not change a default, do not touch `M1-PLANT-002`.**
- **Do not register a gate or move the denominator.**
- **Do not answer R-1.** Feel is his (`M1-ACC-003`). This makes his question smaller, not
  answered.
- **If the curve makes reading 3 obviously right, still do not apply it.** Say so and stop.

## 5. AND — carry less to him, not more

*"Bogged down in minutia"* is a standing instruction about how we work:

- Report **one page**: the curve, and one sentence on what it implies. **No option tables.**
- Use `DELEGATION.md` §3 — anything reversible, **proceed under a stated assumption and flag
  it**, rather than queuing another question.
- Only genuine blockers reach `docs/RULINGS-PENDING.md`. Nothing else does.
