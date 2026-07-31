# FLY CARD — S-rollmix: blend-band roll target continuity (the 5–10° roll slam)

**Build:** `D:\flight_sim2\seads-feel\build\seads.exe` @ **fc3ea6970** (feel/kernel-v5).
Rebuild first: `cmake --build build --config Debug`, launch from `D:\flight_sim2\seads-feel`.
**THE TABLE FLOWN (read from this build's config at card-writing time — the table-flown rule):**
`[regime] roll_target_mix = 1.0` (THE dial) · `blend_lo/hi = 5.0/9.0` · `bank_align_power = 6.0`
· `[auto_level] lean_gain = 8.0 / lean_max = 30.0` (the in-band anchor — pre-registered coupling)
· capture carry = 0.0 (parked) · yaw_scale 2.0 · aim_sensitivity 0.14.
Config values are IDENTICAL to canon v10 (efeb7020a) except the one new dial.

## ONE DIAL

`[regime] roll_target_mix` — flown at **1.0**. Walk-back **0.0** = bit-identical v10
(structural off, proven on the golden; also the **Golden-#4 baseline reconstruction arm**:
any future horizon-gate recurrence A/B must set 0.0 first). If rejected on the stick, the
mechanism is **reverted to a branch** (the s-aimclamp-rejected pattern), not shipped parked
at zero — kernel-base audit ruling.

## THE RULING (exact form, per the his-words-are-the-spec rule)

Chad's felt spec was given as a selected option, not free text — the EXACT option he
selected (AskUserQuestion, 2026-07-30): **"Progressive deepening — bank deepens smoothly
and continuously from the shallow lean toward the committed turn as the deflection grows —
no snap point anywhere in 5–10°. More deflection = proportionally more bank."** Companion
selections the same round: flicks past ~9° **"Unchanged"**; **"Instrument first"** (with
the docs agent's sealed-tape back-compat condition, met — see the commit-1 message).

## HYPOTHESIS → WHAT YOU SHOULD FEEL

Slow-add a lateral deflection through the 5–9° band (the bounce zone from your buttery-arc
tapes): the bank should now **deepen smoothly and progressively** from the shallow ~30° lean
toward the committed turn as the deflection grows — no aileron slam in/out at ~5°, no
"banks then auto-levels back" bounce, more deflection = proportionally more bank.
Committed flicks past ~9° and everything near center are **bit-untouched by construction**
(verified: exact-equality legs + the golden's blend==1 prefix bit-identical).

Mechanism in one line: inside the band the bank-to-turn's roll target is now the mix
`blend·commit + (1−blend)·lean-target` — the two roll targets that used to disagree by
tens of degrees (the pin-derived A6 tug-of-war) now agree at the boundary. It is the same
continuity treatment pitch and yaw already carry; roll was the one channel that never got it.

## SEGMENTS

1. **The slam test (the thread's reason to exist):** level cruise ~V180, slowly feed a
   lateral deflection from small through ~5–10° and hold it growing — the old build
   bounced/slammed here. Verdict word for the ledger.
2. **The flick fence:** hard lateral flicks 25°+ both directions — must feel EXACTLY like
   sealed v10 (any change here is a bug, walk back immediately).
3. **Near-center + butter:** small-deflection tracking and the Golden-#4 butter segment —
   the smoothness you sealed must not be spent (predicate: < 1.1/s body-rate reversals).
4. **Knife-edge / split-S:** roll-through-inverted unchanged (the continuity term fades
   out inverted by construction).

## SENTINELS (name them in the verdict if felt)

- Mid-band turn ENTRY too lazy? (The follow-up is a blend-shaping dial — never moving
  blend_lo.)
- In-band BELOW-NOSE adjustments now roll over with blend-proportional commitment instead
  of full-rate (the disclosed §6-sibling side-scope) — right or wrong on the stick?
- Red-team note: at level-entry geometry the softening lives mostly in err 5→~6.6°; deeper
  in the band at large bank-error both arms saturate the same. If a residual bounce
  survives at the band TOP specifically, say so — that distinguishes "mechanism inert
  there by saturation" from "mechanism wrong," and the tape now carries `blend` +
  `held_bank` directly (commit-1 instrument) so the A/B is readable offline.

## LEDGERED HONESTY

- The closed-loop hunt could NOT be reproduced in a scripted rig (three shapes tried; the
  cycle rides the pilot's add-rate) — **your hands are the instrument for the dynamic
  verdict**; the static algebra is pinned to 1e-12 oracles and the golden signature.
- Gate 395/395; controller golden deliberately re-recorded (knob-off arm proven
  bit-identical first; first divergent tick 289, blend 0.9899 — the band-only signature).
- Red-team: one scoped round, SOUND-WITH-FIXES; P1-1 (saturation-vacuous samples) folded,
  snap-point mutant now killed in ctest.
- After approval: graft to seads-recon (incl. the recorder v2 port — future tapes must
  carry the new pins) + re-stamp build-play, the v10 pattern.

## GATE-COUNT RECONCILIATION (391 → 395; the plan pre-registered ~398)

Every planned leg accounted for, none silently lost:
- Commit 1 planned +3, landed **+2 test cases** (391→393): the telem-mirror pin (new
  case in test_cascade) + the v1 back-compat fixture (new case in
  test_recorder_firewall); the planned "recorder v2 round-trip" leg landed as
  assertions INSIDE the existing round-trip case (per-tick telem equality +
  non-vacuous nonzero-blend), not a third case — ctest counts cases, not assertions.
- Commit 2 planned +4, landed **+2 test cases** (393→395): the oracle-equality case
  (now three SECTIONs after the red-team: level low-band, banked-70° upper-band,
  knife-edge gate) + the blend==1 bit-identity case. The planned knob-off-bit-identity
  leg MERGED into the oracle case (below-band exact-== sample + the knob-0 golden-arm
  proof at re-record). The planned in-band progressivity/hunt leg was **honestly
  DROPPED** (three rig shapes failed to reproduce the pilot-rate-coupled cycle —
  Learned entry; the dynamic verdict is this fly + the tape A/B), replaced
  structurally by the red-team's banked-70° snap-point-killing leg inside the oracle
  case.
