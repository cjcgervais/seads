# GOLDEN #3 — derived telemetry & conclusions
Source: golden_3_v9_seal_flight.seadsrec (17,512 ticks / 145.9 s, sig-verified
at sealing). Derived purely from the recording's own per-tick SimState pins (the
double-precision truth, S1 discipline) — no re-simulation involved. Companion
CSV: golden_3_telemetry.csv (10 Hz decimation: t, V, alt, climb, throttle, flap;
1,460 rows).

Derivation constants, so every number below is reproducible: `sim_dt = 1/120 s`,
`R = 15000 m`, `alt = |position| - R`, `climb = velocity · r̂`, `V = |velocity|`,
body frame +Y up (SPEC §7), `body_up = quat · (0,1,0)`, `g₀ = 9.80665`.

## The numbers
- Duration: 145.9 s (2.43 min)
- Speed: min 34.5 / mean 199.5 / max 281.1 m/s
- Altitude: min 104.1 / max 2500.0 m
- Climb rate: max +216.3 m/s, max descent −231.6 m/s
- **Peak aerodynamic per-tick acceleration 27.0 g at t = 56.0 s** — under the
  rung-D `n_max = 32` cap (see conclusion 3 and the predicate below)
- Time below 150 m sphere-alt: 28.7 s
- Flaps deployed: 46.1 s
- Overrides held: 30.7 s (21.0% of ticks)
- Mouse active: 38.1 s (26.1% of ticks)
- Raw-mode ticks: 0 (a pure instructor-mode flight)
- **Freelook: 20 presses / 20 releases; orient_cmd fired 0 times** (all releases
  auto-oriented — the v9 one-instant-snap law, every time)
- Inverted: 8 episodes, longest dwell 5.21 s
- **Crash landing at t ≈ 140.9–142.5 s**: V collapses to 34.5 m/s, aircraft flat
  on terrain at 104.2 m sphere-alt (climb 0.0) for ~2 s — then a **respawn
  teleport at t = 142.5 s** (|Δp| = 10,170 m in one tick), ~3.4 s of post-respawn
  flight including one final release (t = 144.3), recording stopped.

**The g-peak predicate, stated because the naive number is wrong by 51×:** the
raw per-tick max is 1,387.8 g at t = 142.5 s — that is the respawn teleport, not
flight. The stream contains exactly ONE positional discontinuity (that tick;
|Δp| = 10.2 km against a ≤2.3 m normal step). Peak *aerodynamic* g is computed
excluding ticks with |Δp| > 10 m. Any future comparison (and any
`harness/replay_diff.py` calibration against this file) must handle that one
teleport tick or its diff will explode there spuriously.

**Inversion predicate (golden #2's, verbatim):** `dot(body_up, local_up) < -0.5`
with a ≥0.25 s dwell. Episodes (start/dwell): 5.8/1.94, 13.5/0.72, 23.4/4.32,
28.0/2.18, 51.4/1.27, 65.0/5.21, 85.0/1.38, 98.9/0.57 s.

## Reconciliation against Chad's flight report
- "did maybe 10 free look presses or more" → derived 20. Consistent ("or more").
- "crash landed too" → confirmed in the pins (the t ≈ 141–142.5 ground episode).
- "shot down a couple of bandits" → **not derivable**: the recording pins
  SimState, which carries no combat/hit channels. Recorded on Chad's word alone.
- "didnt notice any harmful wait on the mouse aim to engage after the snap" →
  the CQ2 0.30 s easeback window, ruled KEPT the same day (see DECISIONS.md).

## Conclusions
1. **This is the recording that pins the v9 release law in the wild.** 20
   releases, 20 instant auto-orients, 0 double-taps — and two of the releases
   (t = 28.7 and t = 65.4) happen INSIDE inversion episodes: freelook released
   while inverted, with the one-snap-upright law carrying it. No prior golden
   exercises a freelook release at all (verified at v9 planning). If a future
   kernel regresses the snap — forward target, instant upright, or the weld —
   this stream is where it shows.
2. **First golden with combat, a crash landing, AND a respawn discontinuity.**
   The teleport tick is a feature for calibration: replay tooling must prove it
   handles a mid-stream respawn without cascading a false diff.
3. **The G-ceiling reads differently than golden #2, and that is information:**
   27.0 g peak vs #2's cap-riding 32.1 g. Same law, different flying — a combat
   flight with 46.1 s of flap (deflection-shot energy management) rather than
   #2's clean 0.9 s. The cap is a ceiling, not an attractor.
4. **Envelope position: the mixed/combat case.** #1 slow/dirty/ground, #2
   fast/clean/air — #3 sits between: kills, deck time (28.7 s below 150 m), heavy
   flap use, aerobatics (8 inversions), a crash, a respawn. No golden supersedes
   another; this one adds the profile the other two bracket but never fly.

## Known limits of this derivation
- Load factor is raw per-tick |Δv|/dt from the pins — it includes every
  discontinuity except the one excluded by the stated teleport predicate.
- No combat, control-surface, or `control::Telemetry` channels exist in the
  pins; kills are unverifiable from the file (see reconciliation).
- The header's `kernel=` string is WRONG (says v7) — see golden_3_VERDICT.md
  provenance before trusting the header; the fnv1a signature covers it, so it
  is preserved as recorded, never edited.
