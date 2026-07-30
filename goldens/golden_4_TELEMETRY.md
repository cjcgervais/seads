# Golden Felt Flight #4 — TELEMETRY (the v10 buttery-cascade flight)

Source recording: `golden_4_v10_seal_flight.seadsrec` (promoted from
`D:\flight_sim2\seads-recon\build-play\felt_flight_18.seadsrec`, identified from its own
data — see VERDICT). 13,674 ticks @ 120 Hz = **114.0 s**. `golden_4_telemetry.csv` is the
10 Hz decimation (1,140 rows: every 12th tick).

Derivation constants, so every number below is reproducible: `sim_dt = 1/120 s` (one
TickRecord per sim tick; the tick column is monotone +1), `R = 15000 m`,
`alt = |position| − R`, `climb = velocity · r̂`, `V = |velocity|`,
`nose = q·(0,0,−1)`, `body_up = q·(0,1,0)`, `local_up = r̂ = position/|position|`,
`cos_phi_theta = dot(body_up, local_up)` (the signed gravity read — `control/extract.h`
conventions, v10 reference snapshot). Angular rates are the recorded body-frame
`pin_angular_vel` (rad/s): wx ≈ pitch, wy ≈ yaw, wz ≈ roll.

**fnv1a note (paid for during this sealing):** `recorder.h`'s fnv1a is a VARIANT — offset
basis `1469598103934665603`, prime `1099511628257` (NOT the standard FNV-1a constants).
The hashed body is every line except the sig line, LF-normalized (`\r` stripped), each
line + `\n`. A standard-constants reimplementation mismatches on EVERY valid recording —
including sealed goldens #2/#3, which is how the error was caught (validate your
implementation against a sealed golden before trusting a STOP).

## Numbers

- Duration 114.0 s; alt 688–1695 m (never near ground; no landing/crash);
  V 214.3–270.3 m/s, mean 246.9 — a fast, clean, high-energy flight throughout.
- Freelook releases: 3 (all uneventful — the v9 camera behavior; not this golden's focus).
- **Steep-down entries** (predicate: `nose·r̂ < −0.7` (>44.4° below horizon), dwell
  ≥ 0.25 s): **2** — t=90.9–92.0 s (steepest 60.9° below horizon) and t=109.3–110.2 s
  (53.8°). **The knife edge HELD at both**: `cos_phi_theta` never dropped below **+0.475**
  during either entry — no bank-over, no inversion. This is Chad's "I couldn't produce a
  roll over" on the pins.
- **Inversion episodes** (golden #2's predicate, verbatim: `cos_phi_theta < −0.5` with
  ≥ 0.25 s dwell): **1** — t=55.7–56.2 s, dwell 0.50 s, min −0.554 (shallow; a full
  inversion is −1.0). **This episode is NOT a contradiction of Chad's verdict — it is the
  HORIZON-GATE before-state, captured in its exact documented geometry:** the nose rises
  to +0.37 (≈22° above horizon — the pre-registered ~22° trigger window), pitches down
  through the horizon, and the plane banks partially over (min cpt −0.554) before
  righting within ~1.5 s and sustaining the descent. Chad's Card-1 words for this exact
  behavior: "for some though it was only a partial bank and was able to sustain the pitch
  down." One partial, no full roll-over — verdict and tape agree.
- **Slow-tracking (butter) instrument** (stated definition: `cos_phi_theta > 0.7`
  (near-upright), freelook off, all three |body rates| < 0.5 rad/s (≈28.6°/s), gaps
  < 0.5 s bridged): longest window **8.8 s** (t=82.1–90.9 s, immediately preceding the
  first steep-down entry). Body-rate full-reversal rates inside it (±0.02 rad/s
  deadband): **wx(pitch) 0.80/s, wy(yaw) 0.91/s, wz(roll) 1.03/s.**

## The smoothness predicate (this golden's regression instrument)

**Predicate: in the longest slow-tracking window under the stated instrument definition,
body-rate full-reversal rate < 1.1/s per axis.** Measured on this tape: 0.80 / 0.91 /
1.03 (pitch/yaw/roll). A future kernel whose slow-tracking reversal rates materially
exceed these under the same instrument has lost the v10 butter.

**Honest scope:** the sick-state reference numbers (6.24/s rudder, 3.27/s elevator) were
SURFACE-deflection reversal rates from the seads-feel attribution addendum's instrument —
a different channel than this tape pins (the recording carries body rates, not surface
commands). The two are NOT directly comparable; the cross-instrument claim is only
directional (the pre-retirement flapping was an order of magnitude above any smooth-state
reading). This golden's own regression numbers are the body-rate figures above.

## Known limits of this derivation

- The aim direction is not directly pinned (aim_dx/dy are per-tick deltas); no FINE-band
  aim-error gating was applied — the slow-tracking window is defined on attitude + body
  rates + freelook only, as stated. Reconstructing aim would mean re-running input
  integration, which this repo's rule forbids conflating with pin derivation.
- The card asked for ≥30 s of butter; the longest single window under this strict
  instrument is 8.8 s (tracking interleaved with turns fragments the windows). The
  instrument definition is stated precisely so the comparison is like-for-like; do not
  widen the definition until a number looks better — supersede with a new stated
  instrument if a future golden needs one.
- The horizon-gate episode count (1) is a lower bound on the defect's felt frequency —
  Chad reported the symptom repeatedly on earlier (unpromoted) tapes; this tape sampled
  it once, partially. The thread's fix work should request a dedicated recording set.
- Header `kernel=flight-kernel-v10-2026-07-30` and build stamp
  `game-kernel-v5-38-gefeb7020a` are CLAIMS consistent with the green-word build; the
  signature covers the header, and identification was derived from content (see VERDICT).
