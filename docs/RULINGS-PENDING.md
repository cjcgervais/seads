# RULINGS PENDING — Chad's queue

**One place. Work it in a sitting; do not be interrupted per question.**
Counted by `tools/audit_graph.py` every run, so it cannot go quiet.

**How to use it:** rule by saying the number and your answer. Anything you don't rule on stays
queued and blocks only what its BLOCKS line says. **Agents append here instead of stalling**
(`docs/DELEGATION.md` §4) and never rule on their own entries.

**Format:** each entry carries the question, the options, the docs agent's recommendation, and
exactly what it is blocking. **A recommendation is not a decision** — where one is offered it is
so you can disagree cheaply.

---

## R-1 — ⏸ HELD, NOT BLOCKING · the iter-18 feel question · **eagle**

> **HELD 2026-08-03 on Chad's feel signal.** Verbatim: *"I dont know what to rule 80 degrees
> bank for a 30 degree deflection that will likely arrive too slow.. probably have to gho with
> the crab I feel so bogged down in minutia seems too slow anyways"*
>
> **Reading 1 is rejected on feel.** The rest is a LEAN, not a ruling — "probably", "I feel" —
> and going with the crab would contradict his own `M1-PLANT-002`, so it is not carried.
>
> **The question may be malformed, and that is being tested before he is asked again.** Every
> number here comes from a **30° step**; he mostly flies 2–10° corrections. The eagle is
> measuring the deflection curve (`consults/EAGLE-R1-DIRECTIVE.md`). If small deflections
> resolve promptly *and* coordinated, R-1 collapses to reading 3 and `M1-PLANT-002` may not need
> relaxing at all. **Do not rule this until the curve is in.**

**Should a routine 30° aim step be an ~80°-bank, ~5.8 g manoeuvre?**

Measured, not assumed: a coordinated level turn gives `ω = g·√(n²−1)/V`. At `140 m/s`, 30° of
heading in `1.60 s` costs ~80° of bank and ~5.8 g. v12 only achieves `1.60 s` by **skidding —
9.3° of sideslip, pointing the nose without turning the aeroplane** — which is the pointed rudder
`M1-PLANT-002` outlaws. **You wrote M1 to forbid the mechanism that makes v12 feel fast.**

| | reading | what you'd fly |
|---|---|---|
| **1** | Hold `1.60 s`, accept the bank | *Control is king* read literally: it goes where you point, promptly, whatever the attitude costs. Every 30° nudge is near-knife-edge and ~6 g |
| **2** | Relax the resolve target, keep it docile | `M1-PLANT-004` literally — 31° of bank, `dip 0.000°`, a beautifully clean line, and it costs seconds. *A different aeroplane, not a worse one* |
| **3** | Make the target deflection-dependent | Small corrections prompt and gentle; a 30° step is a commitment. **This changes what `M1-PLANT-011` says — a specification change, yours alone** |

**No recommendation offered.** This is `M1-ACC-003` — feel is the first instrument, and neither
agent will answer it. The eagle notes reading 3 sits closest to your own `M1-PLANT-001`
(*"the steepness of the bank angle I will need for the tightness of the turn radius I want"*).

**BLOCKS:** nothing right now — the eagle has delegated measurement work in front of it.

---

## R-2 — deferred, not blocking · register the resolve bar · **eagle**

`M1-PLANT-011`'s bar is a **working target of `≤ 1.60 s`, deliberately not a gate**. Converting
it into a **registered** bar requires you to fly it (`M1-ACC-003`), and registration must happen
*before* the run it scores (`SPEC-ACC-004`).

**Recommendation:** rule `R-1` first — its answer may change what the bar should be.
**BLOCKS:** nothing today. The eagle steers by the target meanwhile.

---

## R-3 — not blocking · `rollout_bank_gain` / `rollout_bank_max` pin debt · **eagle**

Neither is a spec constant; neither has a `PIN-` gate. If the roll-out law survives `R-1`, both
get registered — with your word, before the run that scores them. **Debt recorded, not incurred.**

**BLOCKS:** nothing. Becomes live only if `R-1` selects reading 1 or 3.

---

## R-4 — not blocking · fly-card and golden for the EvC2026 track · **kernel-docs**

`G-9` needs a fly-card you can fly once and rule on, per the fly-card protocol and your
2026-07-31 amendment (**free warm-up before instruments**). It should not be written until `G-2`
proves attribution works on the real tape — a card whose instruments cannot attribute what you
felt is the 2026-08-02 failure again.

**BLOCKS:** nothing yet; gated behind `G-2`.

---

## RULED AND CLOSED — 2026-08-03

| | ruling |
|---|---|
| ~~iter-17 option~~ | **Option 2 — M1 governs, remove the cause.** Option A withdrawn (it worked; `M1-PLANT-010`/`S-8` barred it) |
| ~~two-kernel question~~ | **The eagle is a reference implementation proving `MANDALARK1`**, not what EvC2026 ships. `GOAL §3` |
| ~~gate denominator~~ | **`257 → 250`**, amendment 2, seven gates reported-not-counted. Parts of the proposal declined |
| ~~`M1-PLANT-011` value~~ | **Working target `≤ 1.60 s`**, explicitly not a gate (see `R-2`) |
| ~~`OPEN_QUESTIONS` Q9~~ | Camera carve-out **ruled**; the four `SPEC-CAM-A0*` gates are reported, not counted |
| ~~the 20-second flight~~ | **Flown.** `G-1` DONE — verified at source, 5,173 rows, zero drops, both floors met |
