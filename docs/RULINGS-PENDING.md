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

## R-1 — ✅ RULED AND CLOSED 2026-08-03 · lead with rudder, behave like v12 · **eagle**

> **Chad's ruling, verbatim. This is the specification — do not paraphrase it, do not reopen it.**
>
> *"I rule to lead with rudder! ALL THE WAY IM TIRED OF BEING SO HELD TO COORDINATED FLIGHT FOPR
> THE SAKE OF A STRAIGHT LINE. V12 ACHIEVED THIS tHIS IS AN ARCADE GAME AND NEED A QUICK RESOLVE
> TO AIM. tHIS IS SETTLED ., mAKE IT BEHAVE LIKE V12"*
>
> **He chose an option that was not on the table.** None of readings 1–3 below is this. He
> rejected the frame — coordinated flight as the constraint — rather than picking a point inside
> it. The table is kept unedited underneath for lineage; it is superseded, not amended.
>
> **Chad delivered this ruling to the other agents himself** and said *"im tired of hearing about
> it."* **It is settled. No agent re-litigates it, re-measures it, or queues a follow-up question
> about it.** The measured target already exists: arm 3 of the deflection curve is v12's dials on
> the eagle bench (`consults/R1-DEFLECTION-CURVE-VERDICT.md`) — 30° step, resolve1 `1.60 s`,
> pk|β| `9.53°`, pk bank `65.6°`.
>
> **Consequence, recorded once and not re-raised:** `M1-PLANT-002` (rudder coordinates, does not
> point) is overruled by its author. `M1-PLANT-009` (peak |β| ≤ 2.0°) and `M1-PLANT-010` (the dip
> bar) are exceeded by the v12 behaviour he has ruled for. `M1-STATE-005` — *"the open
> engineering problem of this specification"* — is **dissolved rather than solved**: it asked how
> to buy coordination without resolve time, and he has ruled that it is not to be bought.
> Amending M1 to match is the eagle's motion.

---

### Superseded frame, kept for lineage only — the three readings he rejected

## R-1 — ⏸ was: HELD, NOT BLOCKING · the iter-18 feel question · **eagle**

> **HELD 2026-08-03 on Chad's feel signal.** Verbatim: *"I dont know what to rule 80 degrees
> bank for a 30 degree deflection that will likely arrive too slow.. probably have to gho with
> the crab I feel so bogged down in minutia seems too slow anyways"*
>
> **Reading 1 is rejected on feel.** The rest is a LEAN, not a ruling — "probably", "I feel" —
> and going with the crab would contradict his own `M1-PLANT-002`, so it is not carried.
>
> **The question was tested for malformation. It is NOT malformed.** The curve is in
> (`consults/R1-DEFLECTION-CURVE-VERDICT.md` — the eagle measured it, kernel-docs reproduced it
> at source to the digit). **The hypothesis that the bench was testing a rare case is
> disconfirmed:** bank asked per degree is **5.2–5.9:1 across 1–10°** and only **3.0:1 at 30°**,
> so the ratio Chad objected to is the *gentlest* point on the curve. **R-1 does not collapse to
> reading 3**, and the three readings below stand as written.
>
> **What changed is the size of the question, not its shape.** He is not ruling on a rare 30°
> corner. He is ruling on a **uniform ~10× price for coordinated flight at every deflection he
> flies** (verified on two independent resolve predicates), against **a crab he cannot feel below
> 3°** — v12's sideslip is inside the 2.0° bar at 1° (0.72°) and 2° (1.40°).
>
> **Still HELD, and no recommendation is offered** — this is `M1-ACC-003`, feel, and it is his.
> Put it to him **at his next sitting for this queue**, not as a fresh interruption.

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

**RELEASED 2026-08-03 — `G-2` is DONE**, per-row roll attribution is arithmetically valid on the
flown tape. This is now writable. **Sequenced behind `G-4`** by this agent's own judgement: `G-4`
pre-registers the claim-2 A/B, and a card written before that pre-registration exists is a card
whose instruments were chosen after the fact.

**BLOCKS:** nothing.

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
