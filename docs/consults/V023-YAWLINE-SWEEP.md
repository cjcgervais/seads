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
