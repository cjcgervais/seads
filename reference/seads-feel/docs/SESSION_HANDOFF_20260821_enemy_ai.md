# ENEMY-AI E-LADDER — HANDOFF, 2026-08-21 (E1–E7 LANDED ON MAIN)

LAUNCH LINE: "Read docs/SESSION_HANDOFF_20260821_enemy_ai.md; take the next
enemy-AI rung." Binding spec + full measurement ledger: docs/ENEMY_AI_E1_E2_SPEC.md
(rung E7's sections are E7.A–E7.O). This supersedes
docs/SESSION_HANDOFF_20260820b_enemy_ai.md, which was written pre-fly.

## ★ DO THIS FIRST, BEFORE YOU WRITE ANYTHING

THIS REPO HAS SEVERAL LANES LIVE AT ONCE (enemy-AI, character/scarf, sled/kernel)
sharing ONE git dir across many worktrees. Two of them collided during the E7
session and it cost real time. Sixty seconds of checking prevents it:

    git worktree list
    powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='seads_tests.exe' or Name='ctest.exe' or Name='ninja.exe'\" | Select-Object ProcessId,CommandLine | Format-List"
    git -C <the tree you mean to use> status --short

If another lane holds uncommitted files in a tree, or is running a gate against
its `build/`, THAT TREE IS THEIRS. Work in your own. What collision looks like
when you miss it: a scratch file truncated before it ran, builds dying on
`ld.exe: cannot open output file ... Permission denied`, and — the dangerous one
— a build that FAILED TO LINK so your test run silently executes the PREVIOUS
binary and you believe its numbers. ALWAYS mtime-check a binary before trusting
what it printed.

## STATE — THE RUNG IS LANDED AND NOTHING OF ITS OWN IS PENDING

★ NO HASH IS NAMED HERE ON PURPOSE. This doc was first written pinning
"main @ <hash>" and that line was stale within the hour, because two other lanes
were landing at the same time. Find the state yourself, it takes one command:

    git log --oneline --first-parent -15 origin/main
    git log --oneline -1 -- docs/ENEMY_AI_E1_E2_SPEC.md

- The E-ladder E1–E7 is MERGED INTO main AND PUSHED (`cjcgervais/seads_sandbox1`).
  Its rung commit is "Rung E7, the killers: the repeal survives, the instrument
  did not"; the spec ledger is docs/ENEMY_AI_E1_E2_SPEC.md §E7.A–E7.O. Chad flew
  it and signed it (below). Nothing from this rung is uncommitted anywhere.
- WORK IN `D:\seads_sandboxes\enemy-ai` (branch `sandbox/enemy-ai`). It owns
  this lane's `build/` and `build-play/`.
- ⚠ **DO NOT WORK IN `D:\flight_sim2\seads-recon`.** As of 2026-08-21 that tree
  is the CHARACTER / SCARF lane's active worktree — they hold uncommitted work in
  `assets/character/sudburian_src/` and run their gate against its `build/`. It
  was briefly this lane's fly tree; it is not any more.
- GATE BASELINE: at the E7 rung commit this lane measured **1484/1488**, the 4
  failures being the pre-existing GI4 sled debt verbatim
  (`sled_slides_before_it_tips_on_flat_snow`,
  `sled_grip_ceiling_stays_below_the_tip_threshold`,
  `sled_assist_reference_plane_is_load_weighted`,
  `sled_debug_sink_is_write_only`). ★ THAT TOTAL HAS ALREADY MOVED — other lanes
  have since added tests (the character lane reported 1516/1520). TAKE YOUR OWN
  BASELINE BEFORE YOU CHANGE ANYTHING, and judge by WHICH tests fail, never by
  the total. If exactly those 4 sled tests fail, you are clean.
- Rebuild `build-play` and MTIME-CHECK IT before believing any fly result, and
  before handing Chad a binary. `tools/ai_tape.py --selftest` must print
  SELFTEST OK; a real tape must read `signature: VERIFIED`.

## CHAD FLEW IT AND SIGNED IT (tape 4, 2026-08-21)
> "I dominated but I was attacked, ai was relentless still trying to attack pump
> on surface... allies helped with the pumps so it didnt feel like a grind, I was
> like a defender... I am happy with the state of it right now."

Tape 4 (`enemy-ai/build-play/conquest_tape_4.jsonl`, VERIFIED, 16:20, 260–0):
his felt report matched the tape almost line for line.
- THE TUNNEL KILL he guessed at was BOTH: `i=4` was killed by an ALLY in the net
  at 07:14.40 — he entered at 07:14.69, three tenths of a second later — and four
  others (`i=7,9,2,6`) terrain-crashed in there.
- He crashed TWICE, not once (03:27.35, 04:00.39), both `air=1.000` with all
  components at 1.00: honest terrain contact, NOT the vacuum-mush failure mode.
- HIS surface pump was ground **19620 → 2087 hp** by relentless raids. The
  pressure he felt is measured, and he nearly lost it.
- The E7.3 defensive break DOES appear live (bfm mode 6 on `i=2/3/4`) but at
  <0.5% of ticks — see the residual below for why.

## WHAT SHIPS
    ace_bank_cap_deg 72 / mid_bank_cap_deg 65 / deadband 30/90   E7.1  ON
    bfm_defensive_range_m 1400                                    E7.3  ON
    slash_doctrine FALSE                                          E7.2  HELD (defect)
    pursue_track_pitch_gain 0.0                                   E7.4  built, OFF
Walk-backs are one line each: `ace_bank_cap_deg 0`, `slash_doctrine false`,
`bfm_defensive_range_m 0`, and `ace_bank_track_hi_deg <= lo` for the flat cap.

## THE NEXT RUNGS, IN THE ORDER I WOULD TAKE THEM
1. ★★ **E1.1 CLOSURE — the residual his own flight proved.** Over 16 minutes,
   rounds aimed AT HIM: **3**. Two hit. Total time under fire: **0.2 s**. Nearly
   all 90 shots on the tape were drones shooting each other at 14–26 km. They
   press the pump relentlessly and still cannot get guns on the player.
   ATTRIBUTION (probe P-H): the ace holds him as foe only ~24% of a run and
   spends ~78% of THAT in Intercept — it cannot CLOSE on a 275 m/s target. No
   bank cap, doctrine or break holds a solution on a man you never arrive
   behind, and this is also why the defensive break had almost nothing to work
   with. **This is the rung if he wants the AIR to feel dangerous.** It is also
   why P-H's spec bar (a ≥2 s sustained tracking solution) is unreachable today
   (~0.5 s) and is recorded as an open residual rather than asserted.
2. ★★ **E7.4 — the pitch limit cycle.** `pursue_pitch_gain` 2.2 LIMIT-CYCLES
   while tracking: 14.2° of flight-path swing and n = 5.5 to hold a turn needing
   1.74 (≈4 G into pitch thrash), collapsing between gain 1.8 and 1.4 where net
   turn RISES (2.84 vs 2.39 °/s) and 4 m/s more speed is kept. ONE mechanism
   behind BOTH of Chad's complaints — a nose thrashing 14° vertically cannot
   hold a gun solution AND is what he saw as "elevator dipping". Best hit rate
   of any arm measured (0.438/eng, +22% on E6 with 24% fewer rounds fired).
   The dial is BUILT, scoped to the two ENGAGED seams only, loader-checked
   `[0, pursue_pitch_gain]`, shipping at 0.0.
   ⚠ **DO NOT STACK IT ON THE FULL TABLE** — SHIPPED+E7.4 is the one resolvably
   bad cell (hits 0.172, a ~2x loss). And it must stay scoped:
   `pursue_pitch_gain` is ALSO the terrain-avoid PULL-UP gain, so lowering it
   globally is a CFIT risk.
3. ★ **E7.2's DEFECT** (spec E7.N). The slashers have NEVER FIRED A ROUND
   through the real fire pipeline. Every E7.2 test asserts the `guns_hot` MODE
   FLAG; none counts a round. `probe P-D` — a SIGNED E3 rung — reads 0 rounds
   with the doctrine on and passes 18/18 with it off, isolated to that one dial.
   Chad's "both doctrines approved" is BLOCKED ON A BUG, not overridden.
   OWED: (a) a leg that counts REAL rounds out of the real fire gate — the
   acceptance this rung should have had; (b) measure `coordinated` and the fire
   cone THROUGH a slash (hypothesis, unmeasured: a steep dive's flight path lags
   its nose and fails the `coordinated` clause); (c) re-run P-D with it on — it
   is the regression sentinel.
4. **A RULING ON `kEnsemble`.** Raising it permanently re-baselines every pinned
   P-A test, so it is a deliberate re-pin with Fable adjudicating, never silent.
   `fly_arm` already takes the ensemble size explicitly (default `kEnsemble`, so
   every existing caller is byte-unchanged).
5. **RE-TAKE E6.3's POSTURE at n=64.** It was chosen on the broken instrument
   (39/18 vs 31/12 vs 24/9 — all inside the noise band) and is not trustworthy
   until re-measured.

## ★★★ THE LAW THIS RUNG PAID FOR — READ BEFORE RULING ON ANY PROBE NUMBER
Before a dial ruling rests on a probe metric, MEASURE ITS NOISE FLOOR: run arms
that **cannot** behave differently and look at the spread. If that spread covers
the gap you are ruling on, the number is not evidence.

Measured here: at the shipped `kEnsemble 8`, arms differing by HALF A DEGREE of
engaged bank cap scored **4–39 rounds and 0–18 hits**. The instrument is
DETERMINISTIC (identical arms reproduce byte-for-byte), so this is not
randomness — it is SENSITIVITY: `rounds_at_player` is a rare-event count over a
chaotic furball averaged across 8 phase rotations of ONE script.

It is repairable by ensemble size: the same 55-vs-55.5 pair reads 4.88 vs 2.50
rounds/engagement at n=8, 2.00 vs 2.00 at n=16, 1.94 vs 2.28 at n=64. **n=64
resolves a genuine 2x effect and cannot resolve 20%** — so never write an
acceptance of the form "15% better". Report RATES PER ENGAGEMENT, never raw sums.

★ THE TRAP THAT COST THE MOST: the "39 rounds / 18 hits" E6 baseline this ladder
treated as the number to beat was the single LUCKIEST cell in the sweep — 2.5x
the converged value — and every candidate was measured against it. Rung E7 was
nearly walked back on it. Prefer the STABLE columns (`in_band` held to ~±10%
across the same noise floor where rounds swung 10x).

## PROCESS NOTES PAID FOR THIS SESSION
- **BEFORE WRITING A WORKTREE, CHECK FOR LIVE AGENTS ON IT.** Two writers
  collided for ~75 min: a scratch probe was truncated before it ran, two builds
  died on `ld.exe: cannot open output file ... Permission denied`, and one of
  those silently ran a STALE binary whose result was briefly believed.
  **ALWAYS mtime-check a binary before trusting its output.**
- Landing sequence that worked: commit on the sandbox branch → merge
  `origin/main` INTO it (so the gate runs in the tree that owns `build/`) →
  build → full gate → `git push origin HEAD:main` as a fast-forward.
- `generated/graph/` conflicts on any merge. It is a GENERATED artifact:
  resolve by REGENERATING (`tools/graph/graphify.py`), never by hand-merging.
