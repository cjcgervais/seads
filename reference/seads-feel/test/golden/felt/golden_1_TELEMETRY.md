# GOLDEN #1 — derived telemetry & conclusions
Source: golden_1_first_v5_flight.seadsrec (22,032 ticks / 183.6 s,
sig-verified). Derived purely from the recording's own per-tick SimState pins
(the double-precision truth, S1 discipline) — no re-simulation involved.
Companion CSV: golden_1_telemetry.csv (10 Hz decimation: t, V, alt, climb,
throttle, flap).

## The numbers of the best-yet flight
- Duration: 183.6 s
- Speed: min 0.0 / mean 177.0 / max 296.1 m/s
  (max exceeds the old trainer's LEVEL top ~200 — the rung-D engine in use)
- Altitude: min -2085.7 / max 2143.8 m
- Climb rate: max +223.3 m/s, max descent -246.0 m/s
- Peak per-tick acceleration ~ 50.6 g — CONTACT SPIKE, not aerodynamics
  (the landing/taxi ground-contact jolts; the aero envelope's true peaks
  need the replay-diff pass with load_factor, a queue item)
- NEGATIVE minimum altitude = A TUNNEL RUN IS IN THE GOLDEN: -2086 m below
  the sphere reference is mine-shaft depth (the ~2 km Creighton-class bores
  of the master plan) — golden #1 carries open-sky combat AND the tunnels.
- Time below 150 m sphere-alt: 105.2 s (deck work — landing/taxi
  lives here; Chad landed this generation)
- Flaps deployed: 83.0 s of the flight (the recorder's
  flap-usage channel working — the data Chad asked about)

## Conclusions
1. The rung-D energy model is visible in the raw pins: sustained V >200 m/s
   with positive climb segments — unreachable on the 9 kN trainer.
2. Deck work (landing + taxi) is present in-recording; the taxi prop-strike
   nit lives somewhere in the final low-alt segment — when the ground round
   opens, THIS recording localizes it (replay + inspect the last seconds).
3. This file + CSV are the calibration pair for replay_diff.py: any future
   kernel change replays the .seadsrec and diffs against these derived
   curves; tolerance breach = feel drift, attributed to a maneuver + tick.
