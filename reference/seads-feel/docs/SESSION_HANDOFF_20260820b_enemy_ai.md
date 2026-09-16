# ENEMY-AI RUNG E7 — SESSION HANDOFF, 2026-08-20 night (Opus)

LAUNCH LINE: "Read docs/SESSION_HANDOFF_20260820b_enemy_ai.md in
D:\seads_sandboxes\enemy-ai; continue the E-ladder." The binding spec + ledger
is docs/ENEMY_AI_E1_E2_SPEC.md, sections RUNG E7 and E7.A–E7.O.

## WHAT HAPPENED
This session picked up rung E7 mid-flight after a model switch. Fable had
written the whole rung; the last edits were unbuilt and a `REQUIRE(false)` dial
sweep was left running. A parallel Fable BUILDER agent was also still live in
this worktree for ~75 min (leftover shell — Chad confirmed it was not a
red-team). Its report is reconciled; its findings and this session's agree.

## THE HEADLINE — THE INSTRUMENT WAS UNDER-RESOLVED (spec E7.I)
P-A's `rounds_at_player`/`hits` could not resolve anything this ladder was
ruling on. Arms whose engaged bank cap differs by HALF A DEGREE — the same
aeroplane by any physical argument — score 4–39 rounds and 0–18 hits at the
shipped kEnsemble 8. The cause is the ensemble (8 phase rotations of ONE script,
averaged over a chaotic furball), and it is REPAIRABLE: the same pair converges
to within 18% by n=64.

★ THE "39 ROUNDS / 18 HITS" E6 BASELINE IS A LUCKY ENSEMBLE — 4.88 rounds per
engagement at n=8 against a converged 1.94. Every E7 arm, and the E6.3 posture
table before it, was compared against the single luckiest cell in the sweep.

CONSEQUENCE: the E7.1 "refutation" is WITHDRAWN. At n=64 hits per engagement run
0.359 (E6) / 0.391 (E7.1) / 0.328 (E7.1+2) / 0.438 (E7.4) — flat inside the
instrument's ~20% resolution. Chad's named bank repeal costs no lethality and
HALVES the crashes (7 -> 4). It survives measurement.

## WHAT SHIPS
  ace_bank_cap_deg 72 / mid_bank_cap_deg 65 / deadband 30/90   (E7.1, ON)
  bfm_defensive_range_m 1400                                    (E7.3, ON)
  slash_doctrine FALSE                                          (E7.2, HELD — defect)
  pursue_track_pitch_gain 0.0                                   (E7.4, NEW, off)

## WHAT IS OWED, IN ORDER
1. ★ E7.2's DEFECT (spec E7.N). The slashers have NEVER FIRED A ROUND through
   the real fire pipeline. Every E7.2 test asserts the `guns_hot` MODE FLAG and
   none counts a round. `probe P-D` (a SIGNED E3 rung) reads 0 rounds with the
   doctrine on, 18/18 with it off — isolated to that one dial. Hypothesis, NOT
   yet measured: the fire gate's `coordinated` clause (flying down its own nose)
   is what a steep slashing dive violates. Needs: a leg that counts REAL rounds,
   the `coordinated`/cone measurement through a slash, the fix, then P-D on.
2. ★ E7.4 DESERVES ITS OWN RUNG. `pursue_track_pitch_gain` is built, scoped and
   loader-checked, shipping at 0.0. At 1.4 it has the best hit rate of any arm
   measured (0.438/eng, +22% on E6 with 24% fewer rounds fired) — but DO NOT
   STACK IT on the full table: SHIPPED+E7.4 is the one resolvably bad cell
   (hits 0.172). Attribution is deterministic and clean (spec E7.E): at the
   shipped 2.2 the pitch channel LIMIT-CYCLES while tracking — 14.2 deg of
   flight-path swing and n = 5.5 to hold a turn needing 1.74 — and the cycle
   collapses between gain 1.8 and 1.4, where net turn rate RISES.
3. ★ THE E1.1 CLOSURE RESIDUAL (spec E7.L.3). P-H's spec bar (a >= 2 s sustained
   tracking solution) is NOT MET (~0.5 s) and E7 cannot reach it: the ace is foe
   for only ~24% of the run and spends ~78% of that in Intercept. It cannot
   CLOSE on a 275 m/s player. No bank cap, doctrine or break holds a solution on
   a man you never arrive behind. OPEN FOR CHAD'S RULING.
4. A RULING ON kEnsemble. Raising it permanently re-baselines every pinned P-A
   test, so it is a deliberate re-pin with Fable adjudicating, never silent.
   `fly_arm` now takes the ensemble size explicitly (default kEnsemble, so every
   existing caller is byte-unchanged).
5. E6.3's shipped posture was chosen on the n=8 instrument (39/18 vs 31/12 vs
   24/9 — all inside the noise band). It should be re-taken at n=64 before it is
   trusted again.

## RE-PINS TAKEN (all deliberate, all in spec E7.L / E7.O — none silent)
P-A decision-table acceptance, the P-A `10x` bar, the P-H comparatives, and the
G-honesty probe's cap/porpoise claims. Each carries its measurement in-comment.

## TWO DEFECTS FIXED THIS SESSION
1. `under_the_gun` (the E7.3 acceptance probe) never booked a HIT, so the break's
   `under_fire` predicate could never arm: 0 defensive ticks with the dial fully
   on. E7 unit tests went 6/10 -> 10/10.
2. `took_fire` was booked only for PROJECTILE hits — i.e. only the player's guns.
   AI-vs-AI fire is an abstract DPS drain (app/instructor_tick.h) that booked
   nothing, so the break would have armed only against the player and every
   drone-on-drone fight would have kept the elevator bob. Booked at that seam.

## PROCESS RULE PAID FOR THIS SESSION (spec E7.G)
BEFORE WRITING A WORKTREE, CHECK FOR LIVE AGENTS ON IT. Two writers collided for
~75 min: a scratch probe was truncated before it ran, two builds died on
`ld.exe: cannot open output file ... Permission denied`, and one of those
silently ran a STALE binary whose result was briefly believed (caught by an
mtime check — ALWAYS mtime-check a binary before trusting its output).
