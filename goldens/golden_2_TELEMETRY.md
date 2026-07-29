# GOLDEN #2 — derived telemetry & conclusions
Source: golden_2_v6_seal_flight.seadsrec (19,844 ticks / 165.4 s, sig-verified
at sealing). Derived purely from the recording's own per-tick SimState pins (the
double-precision truth, S1 discipline) — no re-simulation involved. Companion
CSV: golden_2_telemetry.csv (10 Hz decimation: t, V, alt, climb, throttle, flap;
1,654 rows).

Derivation constants, so every number below is reproducible: `sim_dt = 1/120 s`,
`R = 15000 m`, `alt = |position| - R`, `climb = velocity · r̂`,
`V = |velocity|`.

## The numbers
- Duration: 165.4 s (2.76 min)
- Speed: min 144.2 / mean 245.0 / max 282.0 m/s
- Altitude: min 114.7 / max 2497.8 m
- Climb rate: max +264.9 m/s, max descent -199.4 m/s
- **Peak per-tick acceleration 314.8 m/s² = 32.1 g** — see conclusion 1
- Time below 150 m sphere-alt: 3.3 s
- Flaps deployed: 0.9 s
- Overrides held: 45.2 s (27.4% of ticks)
- Mouse active: 50.8 s (30.7% of ticks)
- Raw-mode ticks: 0 (a pure instructor-mode flight)
- Freelook releases: 20 — **all auto-oriented; orient_cmd fired 0 times**
- Inverted: 13 episodes, longest dwell 1.68 s

**Inversion is defined here as `dot(body_up, local_up) < -0.5`** (past ~120° of
roll, a genuine inversion) **with a ≥0.25 s dwell.** This definition is not
arbitrary — it was recovered by reconciling against the flight-log's reported
"13 episodes / longest 1.68 s", which it reproduces exactly. Stated explicitly
because the naive predicate (`dot < 0`, no dwell) yields 25 episodes / 5.74 s:
the extra 12 are sub-0.03 s knife-edge sign flips as the roll passes through
90°, not inversions. **Any future comparison must use the same predicate or the
counts will not match.**

## Conclusions
1. **The rung-D G-limiter is visible, and it is aerodynamic — not a contact
   spike.** Peak 32.1 g sits exactly on `[g_limits] n_max = 32.0`, the rung-D
   arcade cap Chad ruled in ("the wing-rip G-cap is not applicable to arcade").
   Golden #1's higher 50.6 g peak was ground-contact jolt from the landing/taxi;
   **this flight never touches the ground** (min alt 114.7 m), so 32.1 g is the
   airframe riding its own ceiling. Golden #1 and #2 therefore pin *different*
   things and neither replaces the other.
2. **S-relorient is exercised end-to-end and the double-tap is already dead
   weight.** 20 releases, 20 auto-orients, 0 double-taps — flown evidence for the
   "truly redundant" argument that later drove the v7 addendum. This is the
   recording that would catch a regression in the release verb.
3. **A fast, clean, high-energy flight with no deck work.** Mean 245 m/s with
   only 3.3 s below 150 m and 0.9 s of flap — the opposite profile to golden #1
   (105 s below 150 m, 83 s of flap, landing + taxi + a tunnel run). Together the
   two goldens bracket the envelope: #1 is the slow/dirty/ground case, #2 is the
   fast/clean/air-combat case.
4. **Calibration pair.** This file + CSV are the v6 baseline for
   `harness/replay_diff.py`: replay the stream against any future kernel and diff
   these curves. Tolerance breach = feel drift, attributable to a maneuver + tick.

## Known limits of this derivation
- Load factor is computed as raw per-tick |Δv|/dt from the pins, so it includes
  any discontinuity in the recording, not just sustained aerodynamic g.
- No control-surface or telemetry channels are derived here — the recording pins
  SimState, not `control::Telemetry`. A fuller pass needs the replay-diff run.
