# ENEMY-AI — RUNG E8 "THE ENERGY FIGHT", HANDOFF 2026-08-21 (evening)

LAUNCH LINE: "Read docs/SESSION_HANDOFF_20260821b_enemy_ai.md; take the next
enemy-AI rung." Binding spec + full ledger: `docs/ENEMY_AI_E1_E2_SPEC.md`,
rung E8's sections are **E8.0–E8.3**. This supersedes
`docs/SESSION_HANDOFF_20260821_enemy_ai.md` — read the SUPERSESSION below
before you trust anything in it.

## ★★★ SUPERSESSION — THE PRIOR HANDOFF'S #1 RUNG WAS FRAMED ON A WRONG PREMISE
It named **E1.1 CLOSURE** next, because "the ace ... cannot CLOSE on a 275 m/s
target." Chad's own 49-minute tape refutes that: enemies were inside 900 m of
him for **200 seconds**, closest approach **14 m**. They close fine.

What they cannot do is FIGHT once there — **124 m/s against his 202, nose a
median 149° off him.** That is rung E8, and it is Chad's ask in his own words:
*"Need to give enemies more energy fighting abilities as that is how I am able
to beat them."* Do not re-open E1.1 on the closure premise; the measurement is
in spec §E8.0.

## WHAT CHAD FLEW AND SAID (tape 100, 2026-08-21, 48:59, signature VERIFIED)
`seads-recon/build-play/conquest_tape_100.jsonl`. His words:
> "Ai ally's are pretty good at pump destruction, so are the enemies on
> surface, I have yet to see them successful at shutting down the pump in the
> tunnel. Need to give enemies more energy fighting abilities as that is how I
> am able to beat them. I almost dies to them last match though"

Every clause of that is measured true, and one is stronger than he put it:
- **He did not almost die — he DIED.** `i=4` got guns on him at 12:46–12:48
  (897→477 m), 5 rounds, 82.8 → 17.0 hp in 1.3 s, engine and left wing
  destroyed; he hit the ground at 15:13 (`pd cause=ground`, eng 0.00/wl 0.00).
  **The kill chain works when it fires. It fired once in 49 minutes.**
- Allies killed BOTH enemy pumps (surface 04:22, DEEP 11:20). Enemies ground
  his surface pump 19620 → 1357 but never touched the deep one: **pump 2 ended
  at 19620.0, untouched.**
- The match was decided at **12:14**; the remaining **36:45 were empty sky**
  (score frozen 270-0, no enemy raid window after 12:39, no further waves).

## STATE
- Tree: `D:\seads_sandboxes\enemy-ai` on `sandbox/enemy-ai`. **NOT committed,
  NOT pushed** — see OPEN below.
- `D:\flight_sim2\seads-recon` is on `main` and is DIRTY with the **costume
  rung's** work (`assets/character/sudburian_src/*`, `indy650.glb`). It is not
  ours; do not stage or clean it.
- ⚠ **DO NOT RELINK WHILE A PROBE IS RUNNING.** The E7.G two-writers failure
  recurred this session, self-inflicted: a background `seads_tests.exe` sweep
  held the exe and the next `cmake --build` died on
  `ld: cannot open output file seads_tests.exe: Permission denied`.

## WHAT THIS RUNG BUILT
1. **The fire-gate witness** — `PursueCmd::w_coord_cos / w_cone_cos /
   w_range_m`, mirrored to `DroneState` beside `wants_fire`. A read-only
   per-tick REPORT written by the shipped gate's OWN expressions, so a probe
   can NAME the vetoing clause instead of re-deriving it (an H1 fork would let
   the instrument certify itself). ★ Nothing may ever branch on these — that is
   what makes the whole block provably behaviour-neutral by construction.
2. **Probe P-S** (`test_stope_probe.cpp`) — the acceptance E7.2 should have
   had: it counts REAL rounds out of the real fire pipeline and censuses the
   gate. Closes E7.N items (a) and (b).
3. **`bfm_perch_lag_m`** (0 = pre-E8 bit-for-bit) + ★ **the dive-envelope
   loader tripwire**, `atan(height/lag) <= 0.8 * pursue_max_gamma`.
4. **`bfm_fight_speed_mps`** (0 = the patrol cruise, bit-identical) — the E8.2
   engaged fight speed.

## ★★★ THE TWO FINDINGS THAT MATTER MOST
**(1) E7.N's `coordinated` hypothesis is REFUTED. It measures 100% OPEN in both
arms.** The slasher's defect is POINTING GEOMETRY: the perch sat 700 m over the
220 m *tracking* lag point — essentially directly overhead — so the dive onto
him needed **72.6°** against a **30°** `pursue_max_gamma`. Over a whole pass
the nose never came within **96°** of the lead point.

**(2) And fixing the geometry is NOT ENOUGH — the doctrine is refuted too.** A
sweep of standoff × pass duration × release fraction produced ZERO rounds in
every arm. The binding constraint is the dive envelope, monotonically:
`30° → -0.033, 45° → +0.221, 60° → +0.480` best slash cone cos. **Even at
double the envelope the slash never gets within 60° of a solution**, because a
diving attacker's depression angle GROWS as the pass closes. `slash_doctrine`
stays `false` — but now **REFUTED ON MEASUREMENT**, not held on a mystery.
`pursue_max_gamma` is NOT the fix on offer: it is the terrain pull-up gain too.

## ★★★ THE THIRD FINDING — AND IT IS THE ONE THAT SHOULD PICK THE NEXT RUNG
The fight-speed sweep was run at n=32, then CONVERGED at n=64 (spec §E8.4).
**The effect does not survive.** Whole spread on rounds/hits ~1.2x, no monotone
shape — inside the band the E7.I law forbids ruling on. The n=32 pass had shown
a clean interior optimum (rounds 1.88→2.66→1.66, hits 0.28→0.59→0.31) with
exactly the shape a corner-speed trade should have, agreed across three
columns. **It was noise.** ★ A plausible mechanism story that predicts the shape
you observe is not evidence the shape is real.

But the two STABLE columns move monotonically: `in_band` **+16%**, `foe_min`
+4%. **Faster fighters buy PRESENCE and convert none of it into rounds.** Put
that beside E8.1 (96° off through a whole slash; still 60° off at double the
dive envelope) and beside the tape (nose 149° off while inside 900 m for 200 s)
and it is one conclusion said three ways:

  ★★★ **THE WALL IS POINTING, NOT ENERGY.** Every road this rung drove — raise
  the fight speed, fix the perch geometry, lengthen the pass, double the dive
  envelope — moved POSITION and left the GUN SOLUTION untouched.

**SO THE NEXT RUNG IS THE TRACKING LAW, NOT ANOTHER POSITIONING DIAL.** The
standing candidate is the prior handoff's #2, **E7.4 — the pitch limit cycle**:
`pursue_pitch_gain` 2.2 limit-cycles while tracking, 14.2° of flight-path swing
and n=5.5 to hold a turn needing 1.74. A nose thrashing 14° vertically cannot
hold a gun solution. It is BUILT, scoped to the two ENGAGED seams, and shipping
at 0.0. ⚠ Its two standing warnings still bind: do NOT stack it on the full
table (SHIPPED+E7.4 was the one resolvably bad cell, ~2x loss), and it must
stay scoped — `pursue_pitch_gain` is ALSO the terrain-avoid pull-up gain.

## STATE OF THE GATE AND THE DIALS
- **Gate 1521/1525** at `bfm_fight_speed_mps = 150`. The only 4 failures are
  the pre-existing GI4 sled debt, verbatim: `sled_slides_before_it_tips_on_flat_snow`,
  `sled_grip_ceiling_stays_below_the_tip_threshold`,
  `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.
- Graph regenerated in-tree; `graph_query.py check` green.
- The shipped value then moved 150 → **120** on the converged table, so ★ **the
  full gate must be re-run at 120** before this lands.

    bfm_fight_speed_mps 120     E8.2  ON  (for the STICK, not the numbers)
    bfm_perch_lag_m     1700    E8.1  ON  (dead while slash_doctrine is false)
    slash_doctrine      false   E7.2  REFUTED ON MEASUREMENT (was: held)
Walk-backs are one line each: `bfm_fight_speed_mps 0`, `bfm_perch_lag_m 0`.

## OPEN — WHAT THE NEXT AGENT OWES BEFORE ANY OF THIS LANDS
1. **Re-run the full gate at the shipped 120** (the gate above was taken at
   150). Expect the same 4 GI4 sled failures and nothing else.
2. ★ **An independent fresh-context red-team**, per the standing rule. It was
   NOT done this session (the operator forbade subagents). It should re-derive
   E8.0's attribution from the tape, E8.1's refutation from probe P-S, and
   E8.4's retraction from the two sweeps — and it should ask the question this
   session could not answer about itself: whether P-A's stepped scripted player
   can express the defect Chad felt at all, since every road in this rung
   dead-ended on the same column.
3. **Nothing is committed or pushed.** Land on `sandbox/enemy-ai` first, then
   the prior handoff's sequence: merge `origin/main` INTO it, build, full gate,
   `git push origin HEAD:main` as a fast-forward. `generated/graph/` conflicts
   on any merge — resolve by REGENERATING, never by hand-merging.

## CHAD'S FLY CHECKLIST (once the sweep has picked the value and the gate is green)
Fly a conquest match ~15 min and judge these, in this order:
1. **Do they FIGHT you now?** The number to beat is brutal: 5 rounds aimed at
   you in 49 minutes, 0.2 s under fire. Anything that reads as "they got guns
   on me more than once" is the rung working.
   ⚠ **BE TOLD HONESTLY: the harness could NOT resolve a difference here.**
   At n=64 the fight-speed dial moved rounds and hits by ~1.2x, which is inside
   its noise floor. It ships at 120 because it is the top arm on the hits
   column at both ensembles, because it costs nothing (crash rate unchanged),
   and because it answers your ruling — NOT because the numbers earned it.
   Your stick is the only instrument that can settle this one.
2. **Can you still CATCH a runner?** This is the named trade and the one that
   would fail the rung. Patrol/raid cruise is untouched at 85 by construction,
   so a bandit on an errand should be as catchable as ever — but an ENGAGED one
   that disengages now extends faster. If fleeing bandits became tedious to
   run down, say so: walk-back is `bfm_fight_speed_mps = 0`, one line.
3. **Do they still turn with you, or do they just go fast and wide?** The
   corner-speed law is preserved by construction (same align²-faded bumps, only
   the floor moved), but this is exactly what a green gate cannot judge. If
   they feel like they are flying past instead of fighting, that is the turn
   rate trade landing and the value is too high.
4. **The tunnel pump** — unchanged this rung (E8.3 is attribution only). Expect
   the enemy still never shuts yours down; confirm it still reads that way so
   the next rung has a second data point.
5. **Watch for an empty second half.** Tape 100 went quiet at 12:14. If the
   match dies again once the pumps resolve, that is its own rung and it may
   matter more to a 49-minute session than any dial here.

Walk-backs, one line each: `bfm_fight_speed_mps 0`, `bfm_perch_lag_m 0`.
`slash_doctrine` is already `false` and should stay there.
