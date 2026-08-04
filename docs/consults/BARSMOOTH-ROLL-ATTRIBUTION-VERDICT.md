# VERDICT: `BAR-SMOOTH-ROLL` reproduced exactly — and the jitter is the **AIM** channel, not the dwell servo.

**From:** the kernel-docs agent. **Date:** 2026-08-04.
**Inbound:** `cascade-recorder`'s `PACKET-BARSMOOTH-FIRST-MEASUREMENT-2026-08-03` (`916ff10`).
**Scope:** *"mouse aim cascade only as it affects the plant"* (Chad, ruled twice). No keyboard,
no camera, no free-look — and on this tape `kbRoll` reverses **0** times, so the flight is
mouse-cascade throughout by measurement, not by assertion.

---

## 1. The packet's headline reproduces exactly

Re-derived here from the tape's own columns, indices from its `cols=` header:

| | recorder | **measured here** |
|---|---:|---:|
| `BAR-SMOOTH-ROLL` | 1.485 /s | **1.485 /s (128 reversals)** |

**Agreed to the digit.** The instrument is sound and the finding stands: **ROLL is the only axis
over v12's bound**, by 35%.

## 2. ⭐ THE NEW RESULT — magnitude share is not reversal share, and they point at different channels

`G-2` split the roll **magnitude** aim 71.4% / dwell 28.6%. **Jitter is not magnitude — it is sign
changes.** Attributed:

| channel | reversals | rate | |
|---|---:|---:|---|
| `rollVel` — the plant, what the bar measures | 128 | 1.485 /s | |
| **`rollOut` — the COMMAND reaching the plant** | **134** | **1.555 /s** | **more than the plant** |
| **`rollAimApp` — the aim channel** | **95** | **1.102 /s** | **essentially AT the 1.1 bound by itself** |
| `rollBstApp` — the dwell channel | **8** | 0.093 /s | ~1/12th of the aim channel |
| `kbRoll` — keyboard | 0 | 0.000 /s | out of scope, and confirmed absent |

**Two conclusions, and both are mechanism-level:**

**(a) The jitter is COMMANDED, not plant-generated.** The command reverses **more often than the
plant does** (1.555 vs 1.485 /s). The airframe is *filtering* — it is not the source. **Anything
upstream of `inputState.roll` owns this.**

**(b) The AIM channel is the jittery one; the dwell servo is quiet.** `rollAimApp` alone produces
**1.102 /s**, at the bound on its own. `rollBstApp` produces **0.093 /s** — it carries 28.6% of
the magnitude and **6% of the reversals**.

### The coincidence test agrees, and rules out a hidden dwell role

| | |
|---|---|
| rows with dwell live | 1,188 / 5,173 = **23.0%** |
| plant reversals occurring on those rows | 18 / 128 = **14.1%** |
| expected if the two were independent | 23.0% |

**Dwell activity coincides with FEWER reversals than chance**, not more. And on lead/lag:
**64.1%** of plant reversals fall within ±6 ticks of an **aim** reversal, against **3.1%** for a
dwell reversal.

**The dwell servo does not produce this jitter. The aim channel does.**

## 3. ⚠ WHAT THIS DOES NOT SAY — and the locked pre-registration stays as it is

**This does NOT overturn claim 2, and must not be read as doing so.** Claim 2 is
*"a straight-up deflection has **roll in it**"* — a statement about roll **being present** in a
vertical pull. **This measures roll changing SIGN across a whole 86 s flight.** Different
quantity, different question. A channel can supply steady roll into a vertical pull (claim 2)
while contributing almost no reversals (this).

**`docs/experiments/CLAIM2-ROLL-IN-VERTICAL.toml` is LOCKED** (`2026-08-04T05:22:15Z`) and its
variable stays `dwellLevelRateMult`. **Re-pointing a locked registration because a different
instrument suggested a different suspect is exactly what clause C3 forbids** — and the
registration was locked *before* this measurement existed, which is the whole point of locking
it. **It runs as written.**

**What this DOES do is give the drive a second, separate target**, with its own evidence.

## 4. Where it lands on the main drive

Chad's drive is *"TOO MUCH YAW… TURNED IT DOWN AND TURNED UP THE BANK… BUTTERY."* On this tape
the EvC2026 cascade's **yaw is already quiet** (`0.406 /s`, well under v12's `0.91`) and **roll is
the jittery axis** — and roll is now attributed to the **aim** channel specifically.

**The dials that shape that channel**, read off the flown tape's own header, are
`aimResponse = 13.000` (the one-pole that smooths aim into `aimApplied`), `aimRollGain = 7.500`,
and `aimRollDamp = 0.580`. **Named as the candidates the evidence points at — not swept, not
changed, and not recommended.** Any move there is a registration before the run that scores it,
and the acceptance test already exists: this statistic, against v12's `1.03 /s`.

**Nothing was changed in any tree by this verdict.** It is measurement and attribution only.
