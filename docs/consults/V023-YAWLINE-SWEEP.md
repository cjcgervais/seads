# V023 — the yaw line-making gain, swept. Measurement only; nothing registered.

**From:** the kernel-docs agent, writing the cascade under `V024`. **Date:** 2026-08-03.
**Kernel change:** eagle `29b6491`, `yaw_line_gain` — **committed OFF, bit-identical.**
**Bench:** the eagle's own, copied read-only into scratchpad; `tools/` is not this agent's to
write. Measurement functions transcribed verbatim from `sweep_deflection.luau`.

**Self-check passed:** `yaw_line_gain = 0.0` reproduces `sweep_deflection` `ARM 1` **to the
digit** (`2.37 / 3.20 / 3.62 / 4.05 s`). That is the third independent confirmation that
knob-off is bit-identical, after the A/B gate run and the unexecuted-branch argument.

---

## 1. The knob does what the ruling asked

On the **coordinated** arm (`K_coord 24`), raising `yaw_line_gain` monotonically:

- **shortens resolve** — `res90` at 1°: `2.37 → 2.02 → 1.75 → 1.35 → 0.84 → 0.34 → 0.13 s`
  across gains `0 → 0.5 → 1 → 2 → 4 → 8 → 16`;
- **reduces peak bank** — 1°: `5.6° → 2.2° → 1.9°`. It points with yaw instead of banking, which
  is the ruling in one measurement;
- **raises sideslip** — `|β|` 1°: `0.14° → 0.69° → 0.92°`. **That is the crab, and it is what
  was ruled for.**

At gain 8 the 1° step lands essentially on v12 (`β 0.69 / res90 0.34 s / bank 2.2°` against
v12's `0.72 / 0.32 / 2.3`). **It does not reach v12 at 3–5° even at gain 16** on that arm.

## 2. ⚠ A MISREADING OF THIS AGENT'S OWN GRID, CORRECTED

A first pass read the grid row `K_coord 1.0, yline 4.0` as a discovered match to v12
(`0.33/0.36/0.38/0.43 s` against `0.32/0.33/0.36/0.40`). **It is not a discovery.**

**The eagle already ships v12's dials** — `K_coord 1.0`, `yaw_scale 2.0`,
`bank_align_power 6.0`, `K_theta 3.2`, **each PIN-gated** in `tests/gates/Constants.luau`. The
`{24.0, 0.25, 2.0, 8.0}` set is the sweep's *coordinated experiment arm*, not the shipped
kernel. So that row is near the shipped config, and "it matches v12" is close to a tautology.

**Recorded because the number looked like a result and was not.** Checking what the constant
actually is at source, rather than trusting a sweep label, is the only reason it was caught.

## 3. What turning it ON does to the board — and why it must stay OFF

`yaw_line_gain = 4.0` set as the default, full gate run, then **reverted; the tree is clean at
the committed OFF state**:

| | |
|---|---|
| gate | **239/250** (from 246/250) |
| goldens | `SPEC-CTL-A04`, `GOLDEN-CTRL-600/900/1200` all move — **a moved golden is a STOP by design** (`SPEC-ACC-005/006`) |
| `BAR-STRAIGHTLINE-DIP` | changes character entirely: *"the dedicated dip scenario produced **ZERO** parasitic-class committed near-level entries … cannot certify `SPEC-PRED-007`'s bar with zero data points; **this is a scenario or kernel finding, not a pass**"* |

**That last row is the interesting one and it is NOT being claimed as a win.** It is consistent
with the parasitic-dip population having been *eliminated* — which would be `M1-PLANT-010`
satisfied in the plant, exactly what `V023` was aimed at. It is equally consistent with the
scenario no longer qualifying entries. **The gate refuses to score it and so does this agent.**
Distinguishing the two is the next measurement, and it needs the dip scenario read at source.

## 4. What is owed to Chad, and it is one number plus one consequence

**Not queued as a question** — recorded here so it can be ruled in one line whenever he next
touches this:

1. **The gain.** `SPEC-ACC-004` requires registration **before** the run that scores it, so the
   default stays `0.0` until he says otherwise. The measured behaviour above is the evidence.
2. **The goldens will move, and that is a STOP by design.** Any nonzero gain changes the flown
   aircraft, so the controller goldens must be **deliberately re-baselined on his word** — never
   silently re-recorded. *A re-record blesses the bug* is a standing rule of this program, and it
   applies here even though the change is intended.

**Nothing was registered, no dial was taken, the denominator did not move, and no spec clause
was edited.**

---

# ⛔ ADDENDUM 2026-08-03 — H1 IS REFUTED. The dip is NOT gone; the INSTRUMENT went blind.

§3 above offered two readings of the zero-entry result and refused to pick one. **Measured now,
and the answer is the unfavourable one.** The dip scenario was run at both gains with `blend`
and `nose_elev` instrumented directly (scratchpad copy; the eagle tree was not touched).

| | `yaw_line_gain = 0` (shipped) | `yaw_line_gain = 4` |
|---|---:|---:|
| max `blend` | **1.0000** | **0.1558** |
| first tick `blend >= 1` | 117 | **never** |
| qualifying entries | 1 | **0** |
| `nose_elev` minimum | **−1.066°** | **−1.377°** |

**H1 — "the parasitic dip is eliminated, `M1-PLANT-010` satisfied in the plant" — is REFUTED.**
The nose still dips, and its minimum is **deeper**, not shallower.

**H2 — "the scenario stopped qualifying entries" — is CONFIRMED, with a mechanism.** The
bank-turn barely engages: `blend` peaks at `0.156` and never reaches `1`, so
`SPEC-PRED-007`'s entry condition is never met. Yaw resolves the aim flick before the bank-turn
can commit — which is *"lead with yaw"* working exactly as ruled, and is precisely why the
instrument stops seeing anything.

## Why this matters more than the sweep above

**`BAR-STRAIGHTLINE-DIP` goes INERT under a yaw-led kernel.** A bar whose entry predicate
depends on the bank-turn committing cannot measure a kernel in which the bank-turn no longer
commits. That is this program's named failure class — *a check that stops matching is not a
passing check* (`CONTRACTS.tsv`, INERT-CHECK LAW) — arriving inside the eagle's own gate, caused
by this change. **The gate's refusal to score it was correct and is the only reason this was
visible at all.**

## ⚠ A CORRECTION TO GUIDANCE THIS AGENT PROPAGATED

`PACKET-11-VERDICT.md` §2 and the eagle's `blocked_on` both carry architecture's instruction that
**`M1-PLANT-010` is "NOT reversed — SATISFIED"** and must not be deleted from `M1`.

**That claim is now unsupported by measurement.** `M1-PLANT-010` demands the straight-line
property be *a property of the plant*. On this evidence V023 does **not** deliver it: the dip
persists and is slightly worse. **The clause should be neither deleted nor marked satisfied —
it is UNDEMONSTRATED, and its instrument no longer works.**

**For the eagle, drafting the M1 amendment now:** do not write "satisfied" into `M1` on the
strength of PACKET-12 §6 or PACKET-13 §5. Both were reasoned before anyone ran the scenario.
**Re-instrumenting the straight-line bar for a yaw-led kernel is a prerequisite to claiming
anything about `M1-PLANT-010`.**

**Nothing changed in the eagle tree as a result of this. `yaw_line_gain` remains `0.0` and
committed OFF.**

---

# ⛔ ADDENDUM 2 — the gain WAS registered on Chad's word, it FAILED its own bars, and it is reverted

**Chad, verbatim, 2026-08-03: *"YOU HAVE MY WORD."*** Given in answer to two named items: the
gain value, and the deliberate golden re-baseline.

**Scope note, recorded rather than assumed:** the goldens live in `tests/`, which `V024` did
**not** give this agent — `V024` gave `src/Kernel/**`. **His authorisation for the golden
re-baseline is real but is the eagle's to execute**, and has been relayed rather than acted on.
The gain is `src/Kernel/**` and was this agent's to set.

## What was done, and what it cost

A gap in this agent's own evidence was closed first: every earlier sweep ran on the
**coordinated experiment arm**, never on the **shipped dials Chad would actually fly**. On the
shipped config (`line_hold_ff = 1.0`, all four pointing dials PIN-gated and untouched):

| `yaw_line_gain` | 1° | 3° | 5° | 10° | 30° dip |
|---:|---:|---:|---:|---:|---:|
| 0 (shipped) | 0.32 s | 0.36 | 0.40 | 0.56 | **1.15°** |
| 2 | 0.23 | 0.25 | 0.27 | 0.38 | 2.49° |
| **4** | **0.18** | **0.19** | **0.21** | **0.30** | **3.37°** |
| 8 | 0.13 | 0.14 | 0.16 | 0.26 | 4.54° |

**It roughly halves tracking time in the 1–10° band.** It buys **nothing** at 30° and makes the
30° dip worse. `4.0` was registered on that basis, before the run that scores it
(`SPEC-ACC-004`), and the full gate was then run.

## ⛔ IT FAILED. 239/250, and three failures are NOT the goldens

| failure | reading |
|---|---|
| **`SPEC-AIMFF-A01`** | ***A SPEC VIOLATION, and it fails at ANY nonzero gain.*** The clamp now bounds the TOTAL yaw to `±yaw_max`; `SPEC-AIMFF-002` requires the pedestal shape, whose result can reach `-yaw_max + yaw_coord`. Expected `-2.18166156`, got `-0.959931089`. **This is structural, not magnitude** — no gain makes it green |
| **`SPEC-LINE-A01-15/30/60`** | flick closure degrades: `1.243°` vs bound `0.92`; `2.93` vs `1.08`; **`14.32` vs `4.50`** — 3× worse at 60° |
| **`BAR-SMOOTH-PITCH`** | pitch body-rate full-reversal **`1.467 /s` vs the `1.1 /s` bound** (v12 measured `0.80`). **A smoothness regression — the opposite of "buttery"** |
| goldens ×4 | move, as expected — a STOP by design |

**REVERTED. `yaw_line_gain` is `0.0` and the eagle tree is clean.** The shape stays committed
(`29b6491`) and off.

## The honest conclusion, and it is not "pick a smaller gain"

**`SPEC-AIMFF-002` gates everything.** The pedestal inversion — which *is* Chad's ruling, not a
tuning choice — **structurally violates the clause as written.** Until that clause is amended,
no value of `yaw_line_gain` can produce a green gate, so hunting for a smoothness-safe gain now
would be tuning underneath a spec violation.

**That amendment is already the eagle's live motion** (it is drafting the `M1` amendment this
session). `SPEC-AIMFF-002` must join `M1-PLANT-002` and `S-8` on the reversed list — **it is the
clause that encodes coordination-over-pointing, which is exactly what Chad reversed.** Nobody
had identified it as in scope; the gate did.

**Then, and only then:** re-tune the gain against `BAR-SMOOTH-PITCH` and `SPEC-LINE-A01`, which
are real feel regressions and not bookkeeping.
