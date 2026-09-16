# ENEMY-AI — RUNG E9 "THE TRACKING LAW" (E7.4), HANDOFF 2026-08-22

LAUNCH LINE for the next agent: "Read docs/SESSION_HANDOFF_20260822_enemy_ai.md;
take the next enemy-AI rung." Binding spec + full ledger:
`docs/ENEMY_AI_E1_E2_SPEC.md`, this rung is **§E9.0–E9.7** (§E9.6–E9.7 are the E8 red-team ledger). It supersedes
`docs/SESSION_HANDOFF_20260821b_enemy_ai.md`, whose three OPEN items are now all
paid: the gate at 120 was re-run, E8 was committed and pushed as `dcc5c53c8`,
and the RED TEAM IS DELIVERED AND ADJUDICATED (see below — it found six P0s in
E8, three of which are fixed here).

## WHAT THIS RUNG IS
E8 ended by naming its own successor: "★★★ THE WALL IS POINTING, NOT ENERGY …
the next rung should attack the tracking law itself (E7.4's pitch limit cycle
is the standing candidate: it is BUILT and shipping at 0.0)." This is that rung.
It did NOT come from a new ask of Chad's — his standing ask is E8's.

## STATE
- Tree: `D:\seads_sandboxes\enemy-ai` on `sandbox/enemy-ai`, **committed, NOT
  pushed** (see LANDING). `origin/main` is E8 at `dcc5c53c8`.
- `D:\seads_sandboxes\enemy-ai-rt` is a DETACHED scratch worktree pinned at
  `dcc5c53c8` that the red team was given. Its report is ADJUDICATED (spec
  §E9.6) and its probes are preserved at `docs/e8_redteam_probes.patch`, so the
  worktree is now disposable:
  `git worktree remove D:/seads_sandboxes/enemy-ai-rt --force`.
- `D:\flight_sim2\seads-recon` is on `main` and is DIRTY with the SWEATER/COSTUME
  rung's work. It is not ours; do not stage or clean it.
- ⚠ THE TWO-WRITER RULE STILL BINDS, and it bit twice this session in its
  quieter form: a build that FAILED left the previous binary in place and the
  next run reported the mutant's answer. **After any `cmake --build`, confirm it
  linked before you believe the numbers.** (Spec §E9.2b, process note.)

## WHAT SHIPPED — ONE DIAL, ONE LINE
    pursue_track_pitch_gain  0.0 -> 1.5    (config/scenario.toml)
Nothing else in the table moved. Walk-back is that one line back to `0.0`,
structurally bit-for-bit pre-E9 and proven by TWO independent off-forms.

## HOW THE VALUE WAS PICKED (and it is not a P-A number)
★ ON THE DETERMINISTIC PLANT, because the furball cannot resolve this and never
could. The saturated turn is one aeroplane, one saturated command, no chaos.
At 30 deg bearing, peak-to-peak flight-path swing / load factor / net turn:

    track_gain   2.2(off)   1.8    1.7    1.6    1.5    1.4    1.0
    swing (deg)   14.07   12.42  10.61   7.38   4.20   2.86   0.22
    n (needs 1.74) 5.52    3.34   2.37   1.68   1.32   1.21   1.13
    net turn deg/s 2.21    2.03   2.27   2.71   2.87   2.85   2.65

★ THE BAR IS THE GUN'S OWN CONE: `fire_cone_deg` is 14, and a cycle that swings
the flight path by more than HALF of it has eaten a quarter of the pointing
budget in each direction before any tracking error is paid. **1.6 fails at 7.38;
1.5 is the first value inside the bar** — and it is also the peak of net turn
rate and retained speed on the nose. The porpoise was eating the turn.

★ AND THE SCOPED DIAL WAS NEVER THE ONE MEASURED BEFORE: E7.E/E7.F lowered
`pursue_pitch_gain` GLOBALLY. Scoped vs global now measured IDENTICAL to the
last bit at this seam, so E7.E's numbers transfer to the dial that ships.

## WHAT THE FURBALL SAID (n=64, on the CURRENT shipped table)
NOTHING about the shipped value, in either direction, and that was the question
put to it. Rounds 1.797 -> 1.703 (-5%), hits 0.469 -> **0.531 (+13%)**, crashes
unchanged. The ±30% scatter across arms is non-monotone; no response curve
exists for this dial on this fixture.

★★ READ AGAINST THE **REAL** NULL FLOOR (the red team's, n=64, `ace_bank_cap`
±1.5 deg — arms that cannot fight differently: rounds 1.578-1.859, hits
0.359-0.547), and this is the sharper statement:
  * **1.5 sits INSIDE the null band on every column** — indistinguishable from
    off, which is exactly the "costs nothing resolvable" the rung asked for;
  * **1.0 falls BELOW it on both** (rounds 1.250, hits 0.297): the one arm the
    furball CAN resolve, and it resolves it as WORSE. Do not chase the cliff
    past its knee.
⚠ MY FIRST DRAFT OF THIS SECTION compared that scatter to my own 1% "floor arm"
(gain 2.1999 vs 2.2) and concluded parameter-space chaos. That arm bounds
HARNESS REPRODUCIBILITY ONLY — I wrote the caveat and then used the number
anyway. Corrected in §E9.3. **A caveat you write and then step over is worth
less than no caveat at all.**

★★★ AND THE PATTERN THAT PICKS THE NEXT RUNG: `in_band` +14% here, +16% on
E8.2's fight speed. TWO unrelated dials on different seams both buy PRESENCE and
neither converts it — the FOURTH rung to end on that column. The red team
answered why: see THE INSTRUMENT below.

## ★ THREE DEFECTS THIS RUNG FOUND IN THE GATE (none fixed by moving the dial)
1. `pre_e7()` and the decision table's E6 arm never zeroed E7.4, so the OFF arm
   of every differential in `test_enemy_ai_e7.cpp` silently carried the cure.
2. ★★★ THE DECISION TABLE'S SLASHER ROW HAD BEEN INERT SINCE E8: it read
   `shipped.slash_doctrine`, which E8 ruled FALSE, so the table printed FOUR
   IDENTICAL ROWS under four labels and passed. Now armed by definition, with a
   non-vacuity clause, plus the E7.4-alone row it decomposes to.
3. TWO DELIBERATE RE-PINS, both recorded in §E9.2b/§E9.3 and neither silent:
   `probe P-F` clause (0) (`== 0.0` -> a fraction of the pressing arm), and the
   two rare-event ABSOLUTE HIT FLOORS at kEnsemble 8 — withdrawn on evidence,
   because the same table at n=64 has hits UP 13%.

## THE GATE
**1525 / 1529 at the shipped table** (1524/1528 before the red-team fixes added
the loader leg). The only 4 failures are the pre-existing
GI4 sled debt, verbatim and unchanged since E8:
`sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.
★ AND THE GAME TARGET WAS BUILT AND LINKED (`build/seads.exe`), not only the
test target — this house has a recorded case of a green gate that never
compiled the game. Graph regenerated in-tree; `graph_query.py check` green.
The E7.4 legs are MUTATION-VERIFIED 4/4 (leak the dial into the terrain-avoid
pull-up, leak it into the raid errand, admit 0.0 as a real gain, ignore the dial
— each turns a leg red).

## ★★★ RED TEAM — DELIVERED AND ADJUDICATED (full ledger: spec §E9.6–E9.7)
An independent fresh-context red team ran against `dcc5c53c8` in its own
detached worktree, per the standing rule and E8's OPEN item 2. It **CONFIRMED
E8's gate independently** (1521/1525, the four GI4 sled failures verbatim, at the
shipped 120 — which pays that handoff's OPEN item 1 from a second tree) and
confirmed every headline number of E8.0's tape attribution. It also found 19
findings, 6 of them P0. ★ EVERY P0 WAS REPRODUCED IN THIS TREE BEFORE IT WAS
BELIEVED — a report is not evidence.

FIXED IN THIS COMMIT:
* ★★ **E8's documented walk-back `bfm_perch_lag_m = 0` DID NOT LOAD** — the
  tripwire E8 shipped rejects its own off-value and takes the GAME and the whole
  test binary down at config load. (0 falls back to `lag_dist_m`, which IS the
  pre-E8 geometry the tripwire exists to reject: the rung contained a
  contradiction.) Now gated on `slash_doctrine`, with a loader test pinning
  BOTH directions through the real TOML. Missed originally because the one leg
  using the off value set it programmatically, bypassing the loader.
* ★★ **The fire-gate witness could report the exact inverse of both E8.1
  findings and probe P-S stayed green.** Every census number that ruling rests
  on was unasserted. The cosines are now HOISTED so gate and witness cannot
  drift, P-S pins the census per field, and the inverted-witness mutation now
  turns it RED (re-verified here).
* "the two stable columns move MONOTONICALLY" — false on E8's own printed table
  (`in_band` 1258 -> 1223 AT THE SHIPPED ARM). Corrected in the spec and in the
  TOML comment Chad reads, along with the "top arm at both ensembles" clause.
* A test named "bit identical" that asserted the opposite now measures identity.
* All four witness fields are cleared per tick, not just the liveness flag.

NOT FIXED — RECORDED, AND THEY ARE RUNGS:
* ★★ **"ZERO rounds in every arm" and "never within 60 deg even at DOUBLE the
  dive envelope" are BOTH FALSE on the shipped probe.** Reproduced here: at
  double the envelope the slasher fires **14 rounds** (cone cos 0.537), and at
  `perch_lag` 1200 it points to within 23 deg. E7.2 is not "unflyable in this
  airframe" — it is clamped by `pursue_max_gamma`, a dial shared with the
  terrain pull-up. **That is exactly the problem E7.4 just solved by scoping.**
* **E8.2 fixes 49% of its own diagnosis**: Intercept is 51% of the in-close
  ticks E8.0 attributes, and E8.2 excludes it on a rationale that is false
  inside `attack_range`.
* The E8.1 sweep §E8.1 describes (45 arms) is NOT in the shipped code (6 arms).

## ★★★ AND THE FINDING THAT SHOULD PICK THE NEXT RUNG — THE INSTRUMENT
The red team answered E8's own owed question: **can P-A's stepped scripted
player express the defect Chad felt? No, and not close.** Against his own tape:
his hardest in-close turn is 25 g and his MEDIAN is 4.3; the script's MAXIMUM is
2.36. His speed varies 90–309 m/s; the script's is **constant 275 = 112% of
`v_redline`**, a target no bandit can ever match, ever. ★ So the ENERGY column
is **dead by construction** — E8.4's null was predicted by the fixture, not
measured about the mechanism — and "the wall is POINTING, not energy" is half a
real finding and half an instrument artefact. This rung is the FOURTH to
dead-end on the same column, and E9.3 recorded that independently before the
report arrived.
**NEXT RUNG (both agents concur): replay Chad's RECORDED trajectory from
`conquest_tape_100.jsonl` as the target into `drone::tick`.** The house owns the
pattern already (the sled rung's `tape_360_chad_repro`); the recorder writes
pos+vel at 5 Hz, so interpolation is the only work. Then: the EMPTY SECOND HALF
(the last tick any enemy held him as foe is **14:18 of 48:59 — 71% of his
session with zero contact**), the tunnel pump, the scoped dive envelope, the
Intercept ceiling. Ranked with reasons in §E9.7.

## LANDING (not done — deliberately)
This is committed on `sandbox/enemy-ai` and NOT pushed. E8 went to main unflown;
this one waits for Chad's stick and for the red-team adjudication. When both are
in: merge `origin/main` INTO the branch, rebuild, full gate, then
`git push origin HEAD:main` as a fast-forward. `generated/graph/` conflicts on
any merge — resolve by REGENERATING, never by hand-merging.

## CHAD'S FLY CHECKLIST — fly a conquest match ~15 min and judge, in this order
1. **Does their NOSE settle on you now?** This rung damped a pitch limit cycle
   that swung the nose 14 deg vertically while tracking — half a fire cone of
   thrash. The felt signature to look for is the opposite of "elevator dipping
   up and down": a bandit that steadies and holds instead of sawing. Your tape
   100 number to beat is brutal — 5 rounds aimed at you in 49 minutes.
2. ★ **THE NAMED TRADE: do they still turn with you ABEAM?** This is the one
   thing the harness cannot rule and the one that would fail the rung. On the
   nose the change is free (they turn 30% FASTER). Off the nose they turn
   slower: the base-tier bandits lose about a fifth of their abeam turn rate
   (22 -> 18 deg/s) and keep a little more speed for it. If they feel like they
   go fast and WIDE instead of fighting, that is this trade and the walk-back is
   one line: `pursue_track_pitch_gain = 0.0`.
3. **Do the aces still get down low without flying into the ground?** The dial
   is scoped OFF the terrain pull-up on purpose (proven bit-for-bit), but a
   by-product of this change is that an ace in a hard abeam turn no longer
   DIVES 290 m to get its turn rate. Low fighting should look calmer, not more
   dangerous. If you see them mush into terrain, say so — that is a P0.
4. **The tunnel pump** — unchanged this rung and unchanged since you flew it.
   Expect the enemy still never shuts yours down; confirm it still reads that
   way so the pump rung has a second data point.
5. **The empty second half** — tape 100 went quiet at 12:14 with 36:45 of empty
   sky. Still unaddressed. If it happens again, that may matter more to a
   49-minute session than any dial on this ladder.

## ★ THE RED TEAM'S PROBES ARE PRESERVED — `docs/e8_redteam_probes.patch`
Its two n=64 probes are NOT committed as tests (they were written against
`dcc5c53c8` and would conflict with this rung's own sweep), but the patch is
kept in the tree because this house has already lost one noise-floor probe to a
two-writer collision and rewritten it. It contains:
  * `RT: E7.4 stack on TODAYs table` `[.rtstack]` — the six-cell table that
    settled E7.J's no-stack warning;
  * ★★ the **n=64 NULL FLOOR** probe (`ace_bank_cap` 71.5/72/72.5/73 — arms that
    cannot fight differently). THIS IS THE INSTRUMENT THE LADDER KEEPS NEEDING
    AND KEEPS NOT HAVING. It is what corrected E8.4's surviving finding AND my
    own §E9.3, and it should be LANDED as a permanent hidden probe by whoever
    takes the instrument rung, so no future rung has to measure its floor by
    hand or, worse, assume one.
