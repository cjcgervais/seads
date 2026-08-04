# → `eagle`: a smoothness scenario that can SEE bank authority. Proven on the bench, ready to install.

**From:** the kernel-docs agent. **Date:** 2026-08-03.
**Unblocks:** `GOAL §0` item 3 — the last thing standing in the main drive.
**This is a scenario design, not an edit.** `tests/` is yours; nothing was written there.

---

## 1. The defect, stated precisely

`BAR-SMOOTH-*` **cannot observe bank authority at all.** Its oscillation scenario commands
roll through the **keyboard override** (`override_mask[3] = i.roll ~= 0`), and `STEP 16`
overwrites the roll channel outright. So the lean law never reaches the plant on the axis being
measured.

**Measured, not argued:** `lean_gain` at `8.0 / 10.0 / 12.0 / 14.0` produces **byte-identical**
`BAR-SMOOTH` numbers — PITCH `1.067`, YAW `0.267`, ROLL `0.300` at every value.

**Why it matters right now:** Chad's fix has two halves — *"WE HAD TOO MUCH YAW WHEN WE TURNED
IT DOWN AND TURNED UP TH E BANK IT GOT TO BUTTERY."* **The bench can currently see one of them.**
Bank-up is the hypothesis for how to afford *more* yaw without the jitter returning, so an
instrument blind to bank cannot close the drive.

## 2. The fix is one line of scenario design

**Drive the aim laterally and command NO roll.** The aircraft must then bank itself through the
lean law, which puts bank authority in the loop:

```lua
local function bank_input(tick: number, dt: number): any
    local t = tick * dt
    return { pitch = 0.10 * math.sin(2*math.pi*t/12.0),
             yaw   = 0.60 * math.sin(2*math.pi*t/7.0),   -- lateral aim sweep
             roll  = 0.0,                                 -- <-- THE POINT
             throttle = 1.0 }
end
```

Everything else is `BAR-SMOOTH`'s unchanged: same `run_controller_trace`, same
`count_reversals`, same 3600 ticks, same `eps`. **Only the input function differs**, so results
stay comparable in kind to the existing bar.

## 3. Proof that it works — run on this bench, `yaw_line_gain = 0.25`

| `lean_gain` | PITCH | YAW | **ROLL** | peak bank | max blend |
|---:|---:|---:|---:|---:|---:|
| 8.0 (shipped) | 0.60 | 0.27 | **0.53** | 77.8° | 1.00 |
| 12.0 | 0.60 | 0.27 | **0.67** | 77.7° | 1.00 |
| 16.0 | 0.67 | 0.27 | **0.73** | 77.7° | 1.00 |

**Two things are established:**

1. **The dial is observable.** ROLL reversal rate moves `0.53 → 0.67 → 0.73`. On the existing
   scenario it does not move at all.
2. **Bank is genuinely exercised** — peak bank **~78°**, `blend` reaching **1.00**, i.e. the
   bank-turn fully commits rather than the lean law idling below `blend_lo`.

## 4. ⚠ What this does NOT establish — do not read a conclusion into row 3

**More `lean_gain` gives MORE roll reversals here.** That is a **datum, not a verdict.** A
higher-authority bank manoeuvres more, and more *commanded* reversals are not the same thing as
*jitter*. Whether this scenario's reversal count is a good jitter proxy **on the roll axis** is
an open question that the instrument's author must answer — the existing bar inherited its
credibility from v12 reference numbers that do not exist for this new scenario.

**So: install it as a REPORTED measurement first, not a pass condition.** Give it v12-equivalent
reference numbers before it gates anything. Reporting-not-gating is the same discipline the
`SPEC-CAM-A0*` carve-out already uses in this tree.

**Peak bank barely moves across the three arms (77.8 / 77.7 / 77.7)** because the manoeuvre sits
at `blend = 1.0`, where the bank-turn owns the bank and the lean law does not. **If you want the
lean law itself isolated, the sweep must stay BELOW `blend_lo = 5°` of aim error** — a second,
gentler scenario. Stated because it is the obvious next question and the obvious wrong assumption.

## 5. One defect in this agent's own probe, recorded

The first run of this probe printed **`peak bank 0.0 deg` for every arm, confidently.** `phi` is
**not** a recorded harness field; `r.phi` was `nil`, and `nil or 0.0` produced a clean, plausible,
entirely fictional column. It was caught only because 0.0° was obviously wrong for a manoeuvring
run. Bank is now derived from the recorded quaternion.

**A missing field that reads as a valid zero is the same class as the inert check** — it does not
fail, it agrees with you. Whoever installs this should derive bank the same way rather than
trusting a field name.
