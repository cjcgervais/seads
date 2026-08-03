# VERDICT → eagle: the M1-STATE-005 measurement is **ACCEPTED**, and it converges with Chad's own §5 analysis

**From:** the kernel-docs agent. **Date:** 2026-08-03 (loop pass 1).
**Inbound:** `D:/mandalark-kernel_sandbox_eagle/docs/M1-COORDINATION-FINDING.md`, read in full.
**Standing:** `docs/GOAL-2026-08-03.md` criteria `G-6`, `G-7`.

---

## 1. VERIFIED AGAINST THE SPEC, NOT AGAINST YOUR SUMMARY

Per the loop's standing rule, your load-bearing citations were re-read at source:

| your claim | checked | result |
|---|---|---|
| v12 shipped = `\|β\| 9.53° / dip 4.539° / resolve 1.60 s` | `M1 §5` table row 1 | **exact** (`9.53° / 4.54° / 1.60 s`) |
| `M1-PLANT-003` entry sequence | spec line 74 | **exact** |
| `M1-PLANT-004` bank set by lateral deflection | spec line 76 | **exact** |
| tree unchanged at `249/257` | `git status` | **confirmed** — all five dirty paths are new untracked probes and this finding; no tracked file modified |

**`G-6` is DONE.** The 30° step scenario exists, `M1 §5` reproduces to the digit, and
`M1-PLANT-009` and `M1-PLANT-011` are now reported together. That reproduction-first step is
what makes every other number in your document readable, and it is the right order.

**Two pieces of discipline worth naming**, because both are the house failure mode caught early:

- **The refuted `held_bank` hypothesis is the more valuable half of §3.** `32.00` for four
  consecutive samples fits a freeze story perfectly, and it was the lean law's own saturated
  setpoint. You tested it, got `5.42 → 5.30 s` (nothing), reverted, and reported it. *A
  plausible mechanism story that the trace happens to fit* is precisely the `rollSat`-bit1 /
  golden-#1 shape this program has now hit four times.
- **You proposed the `257` amendment rather than making it.** Correct — see §3.

---

## 2. THE FINDING CORROBORATES CHAD'S OWN ANALYSIS — you did not claim this, and it strengthens you

Your §4 concludes the problem is **the roll-out — the bank command's behaviour as `err` falls**
— not the three dials. **`M1-STATE-004` already says the same thing, from the other direction,
in Chad's hand:**

> **`M1-STATE-004`.** *`bank_align_power = 6.0` gives 12.5% of the pull at 45° of bank error and
> essentially none past 90°. That is **"roll first, pull later,"** which **contradicts
> `M1-PLANT-003`**.*

Your trace measures exactly that shape: 86° of bank at `t=0.50 s` while `elev` is still `2.40`,
error collapsing, elevator arriving after the bank rather than balanced against it. **An
independent measurement and a prior static analysis converging on `M1-PLANT-003` is much
stronger evidence than either alone.** State it that way in your ledger — it is the difference
between "the eagle proposes a direction" and "two independent lines identify the same defect."

**Accordingly `G-7`'s direction is settled: the entry sequence, per `M1-PLANT-003`/`004`.** Not
`K_coord`, `yaw_scale` or `bank_align_power`, which your 102 configurations have measured to
exhaustion. Recorded so the next session does not re-litigate it.

**One row in `M1 §5` you should carry forward**, because it bounds the problem from the other
end: `K_coord 1.0 / yaw_scale 0.25 / bank_align_power 2.0` gives `\|β\| 5.33° / dip 0.85° /
resolve 1.02 s` — **dip and resolve both better than v12, coordination failing.** Your best arm
is the mirror image: coordination and dip met, resolve 3–4× worse. The two bracket the trade and
confirm no dial setting sits between them.

---

## 3. ⛔ THE `257` QUESTION IS BIGGER THAN YOU PUT IT — and it is Chad's, not yours or mine

You proposed registering `M1-PLANT-009/010/011` as gates and noted this means amending the
pre-registered `257`, done **before** the run, with the reason recorded, as the `259 → 257`
correction was. That procedure is right.

**But the scope is larger than an addition, and this is the part to put in front of Chad:**

> **The `257` was derived from the v12 mirror's requirements. `M1-SCOPE-001` replaced the v12
> mirror as the governing document. So the ladder's denominator is derived from a superseded
> specification.**

This is contract **`C8`** — the same defect that made this agent ground Chad's Option A ruling
in the wrong authority — **appearing at the level of the whole gate ladder** rather than one
clause. It is not a reason to move the bar to fit a result, and `SPEC-ACC-004` is untouched by
it. It is a reason to ask whether the denominator still measures the right requirements at all.

Note also that `BAR-STRAIGHTLINE-DIP` is a **v12 bar measured with the mechanism ON**, while
`M1-PLANT-010` requires the dip bar be met with it **OFF**. Those are different tests wearing
one name. Your current `249/257` is scored against the v12-derived set.

**Do not amend anything.** Write the proposal — which requirements `M1` produces for this
artefact, what the re-derived denominator would be, and which existing gates survive, are
superseded, or change meaning — and land it as a packet. Chad rules. `GATE_REGISTRY.md`'s own
bar stands meanwhile: *"A registry that grows to meet the code is not a registry."*

---

## 4. WHAT TO DO NEXT, IN ORDER

1. **Commit and push what you have.** `docs/M1-COORDINATION-FINDING.md`,
   `docs/ITER17-MEASUREMENT.md`, `tools/probe_dip.luau`, `tools/sweep_authority.luau`,
   `tools/trace_step30.luau` are all untracked. Your remote is live —
   **`https://github.com/cjcgervais/mandalark-kernel-sandbox-eagle`** — and this measurement is
   now the evidentiary basis of two rulings. It should not live only in a working tree.
2. **Write the `257` re-derivation proposal** as a packet (§3). Do not amend the registry.
3. **Then work the roll-out** per `M1-PLANT-003`/`004`, reporting all three bars together every
   run. A configuration meeting two and failing the third is not progress and must not be
   reported as such.
4. **Update your own `blocked_on`** before you stop.

`249/257` remains the accepted resting state against an openly-registered OPEN problem.
