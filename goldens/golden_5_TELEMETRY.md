# Golden Felt Flight #5 — TELEMETRY (the v12 straight-line flight)

Source recording: `golden_5_v12_seal_flight.seadsrec` (promoted from
`D:\flight_sim2\seads-recon\build-play\felt_flight_20.seadsrec`, identified from its own
data — see VERDICT). 6,696 ticks @ 120 Hz = **55.8 s**. `golden_5_telemetry.csv` is the
10 Hz decimation (558 rows: every 12th tick). **First golden recorded under recorder v2**
— `telem_blend` and `telem_held_bank` are native per-tick pins (columns 41/42), so the
regime-boundary state is on the tape, not reconstructed.

Derivation constants, identical to Golden #4's stated set so every number is
reproducible: `sim_dt = 1/120 s` (tick column monotone +1), `R = 15000 m`,
`alt = |position| − R`, `climb = velocity · r̂`, `V = |velocity|`, `nose = q·(0,0,−1)`,
`body_up = q·(0,1,0)`, `local_up = r̂`, `cos_phi_theta = dot(body_up, local_up)`,
`nose_elev = dot(nose, local_up)` (elevation angles in degrees via asin). Body rates are
the recorded `wx/wy/wz` (rad/s). fnv1a: the VARIANT constants and LF-normalized body per
Golden #4's note — **the LF-normalization clause bit again this sealing** (a raw-bytes
recompute mismatched every tape including sealed goldens; validated against Golden #4
before any STOP, per the documented procedure — the procedure works).

## Numbers

- Duration 55.8 s; alt 298–881 m (low, dense air — no thin-air content; take 1 was the
  thin-air false start, see VERDICT); V 229.9–285.4 m/s, a fast low session throughout.
- Freelook: 2.7 s total across the flight (releases exercised; not this golden's focus).
- **Inversion episodes** (Golden #2's predicate verbatim: `cos_phi_theta < −0.5`, dwell
  ≥ 0.25 s): **4** — t=11.9 s (0.35 s, min −0.560), t=16.1 s (0.84 s, −0.876), t=33.6 s
  (0.80 s, −0.857), t=49.3 s (1.62 s, −0.994 ≈ full inversion). Deliberate aerobatics in
  a maneuver-dense session; all recovered.
- **Steep-down entries** (`nose·r̂ < −0.7`, dwell ≥ 0.25 s): **1** — t=12.0 s (0.92 s,
  steepest −0.893 ≈ 63° below horizon). Recovered; no bank-over.
- **Regime-boundary state (FIRST TIME ON PINS):** blend distribution FINE 52.6% /
  in-band 16.9% / MANEUVER 30.5%; 111 FINE→band crossings; 139 in-band dwells, mean
  0.07 s, max 0.90 s. These are BASELINE numbers — no earlier tape carries blend, so
  there is no prior to compare against; recorded so future kernels have one.
  `held_bank` range −2.77…+1.68 rad (the inverted episodes carry it through the fold).

## THE STRAIGHT-LINE PREDICATE (this golden's regression instrument — NEW)

**Instrument definition (stated in full so the comparison is like-for-like):** a
*committed near-level entry* is a tick where `telem_blend` leaves 0 (prev ≤ 0, next > 0)
with `|nose_elev| < 0.1` (within ≈5.7° of level), reaching `blend ≥ 1` within 1.5 s;
entries deduped to the first crossing per 2 s window. Its *dip depth* is
`asin(nose_elev@entry) − asin(min nose_elev)` in degrees over the following 2 s. An
entry is *parasitic-class (U-recovered)* if nose_elev returns to within 0.035
(≈2°) of its entry value after the minimum, inside the window — the U-shape; entries
with no recovery are *commanded-descent class* and are excluded (they are the pilot's
arc, not the kernel's error — the aim is not pinned, so the U-shape is the aim-free
discriminator).

**Measured on this tape: 8 deduped committed near-level entries — 5 parasitic-class
(dips 0.00 / 0.00 / 0.00 / 0.12 / 0.58°), 3 commanded-descent class (60.0 / 40.0 /
16.1°, correctly excluded).**

**Predicate: in a committed near-level U-recovered entry under the stated instrument,
nose-elevation dip depth < 1.0°.** Measured max on this tape: **0.58°**, against the
sealed v11 baseline of 2.23–8.31° (offline `lathold`, 15/30/60° flicks, V200). A future
kernel whose parasitic-class dip materially exceeds this has lost the v12 straight line
— the thing Chad sealed as "the baseline for a quality flight kernel."

## Known limits of this derivation

- **This tape does NOT exercise Golden #4's butter predicate.** The longest slow-tracking
  window under #4's stated instrument is 4.5 s (vs #4's 8.8 s), inside a maneuver-dense
  session with no deliberate butter segment; its reversal rates (wx 0.67 / wy 2.02 /
  wz 1.12 /s) are NOT comparable like-for-like to #4's 0.80/0.91/1.03 measured on a
  deliberate tracking segment twice as long. No pass/fail is claimed in either
  direction. **#4 remains the butter golden; #5 pins the straight line** — goldens pin
  different things and none supersedes another.
- The aim direction is not pinned (aim_dx/dy are deltas); the U-recovery shape is the
  aim-free discriminator for commanded vs parasitic, as stated. A commanded *transient*
  dip-and-return by the pilot would be miscounted as parasitic; with 5 such events all
  ≤ 0.58° on this tape, the bound holds regardless.
- The blend/dwell numbers are baselines without a prior, as stated above.
- Header `kernel=flight-kernel-v10-2026-07-30` is a STALE CLAIM (the Golden-#3 class:
  stale seal string at build time). The build stamp `game-kernel-v5-46-g2116f6ea3` is
  the v12 recon graft commit — the tape is v12 by its stamp and its content (the
  parasitic dips measure at v12's closures, not v11's). Correction carried in the
  VERDICT per the never-edit-a-header rule.
