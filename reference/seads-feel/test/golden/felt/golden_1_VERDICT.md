# GOLDEN FELT FLIGHT #1 — the first v5 flight golden
- File: golden_1_first_v5_flight.seadsrec (22,032 ticks / 183.6 s, fnv1a-signed,
  tag v5-reconcile@46051ca23; recorded 2026-07-24 on main == 36ee936e9).
- Chad's verdict on this kernel, same session: "this is the best, it's better
  than all the rest!" — flown the night the reconciliation merged to main
  (tag game-kernel-v5). Landing + bandit kills + the rung-F sacred-middle
  dive live in this generation.
- PURPOSE: the drift alarm. Replay this input stream against any future
  kernel change and diff the telemetry (mandalark-kernel harness/
  replay_diff.py speaks the same columns). Divergence beyond tolerance =
  the feel moved — attribute before shipping.
- The .gitignore excludes *.seadsrec by default; this one is force-added
  BECAUSE it is a golden. Promote future flights deliberately, never by
  default.
