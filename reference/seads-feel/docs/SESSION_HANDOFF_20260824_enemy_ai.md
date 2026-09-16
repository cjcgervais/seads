# ENEMY-AI — RUNGS E12 · E13 · E14 · E15. HANDOFF 2026-08-24

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260824_enemy_ai.md; take the next
enemy-AI rung."** Binding spec + full ledger: `docs/ENEMY_AI_E1_E2_SPEC.md`
(§E12 → §E15.R, the final sections). Supersedes
`docs/SESSION_HANDOFF_20260823_enemy_ai.md`, whose OPEN item 1 — THE INSTRUMENT
— is PAID (probe P-H exists).

---

## ★★★ START HERE: THE NEXT RUNG IS DECIDED, AND IT IS NOT A CHOICE

**STOP THE AI DYING IN VACUUM.** It is no longer a side finding — it is the
critical path, and it is now *blocking* another rung outright.

The evidence, assembled from four independent places this session:

    tape 8 crash forensics    14 of 22 AI crashes were OUTSIDE their own dome
                              11 of 22 steeper than -30 deg (to -69 deg)
                              0 of 22 were SLOW (every one 121-243 m/s)
    E13 roster A/B            enemy crashes 19 -> 36 for +40% aeroplanes
    E15 dial sweep            +3 s/km of transit rope: crashes 37 -> 103
    tape 9 transits           strikers stall outside the dome and time out

**THE MECHANISM, and it is not what Chad guessed (nor what I guessed):** out
there they are not stalling, they are **UNSUPPORTED**. Lift is
`rho * atm_frac_at` and is NOT floored; torque IS floored (`max(q,
q_att_floor)`, `q_att_floor = 280 Pa`, `[authority]` in aircraft.toml). So the
aeroplane keeps *pointing* while the wing quietly stops working — it sinks at
full speed, gets steep, and the terrain pull-up (armed on 100% of wrecks) cannot
rotate it. That torque floor is the entire reason anything can fly out there at
all. ⚠ `min_frac = 0.4` is a SPEED compression floor, NOT an air term — do not
confuse the two; they read alike and mean nothing alike.

**WHY THEY ARE OUT THERE AT ALL** is by design and correct: the containment
leash is exempted for raid sorties (`d.raid.active`), for committed tunnel
dispositions (`maverick::is_tunnel_mode`) and for live fights. E12 made raid
duty *continuous*, so there is now always a pair outbound. The exemptions are
right; what is missing is any notion that the air out there does not hold them.

**WHY IT BLOCKS THE TUNNEL RUNG (§E15.R):** E15's fix is correct and green and
cannot be paid for, because more transit rope is mechanically more time in the
place they die. Fix the vacuum, then turn E15 on with ONE edit.

★ FOR CHAD, IN HIS OWN TERMS: he watched an enemy "coming back to the bubble
from the south, crashed before getting to the air... too high / slow and fell at
too steep an angle." Steep — right. High — right. **Slow — no**, and that is the
useful correction, because it points at LIFT, not at stall. His crash is in the
tape: drone 0 at 958 s, outside the dome, tracking almost straight home (0.90),
−22.8°, 200 m/s, hit at 139 m AGL.

⚠ **DO NOT re-run the five candidate fixes the E10/E11 handoff already measured
NULL** (longer warning, steeper climb, altitude-faded bank cap, the seek fade).
They were measured on P-A, which never destroys a pump and therefore never
shrinks a bubble — the fixture was blind. **P-H is not.** Re-measure there
first; that is what P-H was built for.

---

## STATE

- Worktree `D:\seads_sandboxes\enemy-ai`, branch `sandbox/enemy-ai`, five
  commits on top of `55ffb819b`:

      0dfd95e15  E12  the enemy can win (backfill + probe P-H)
      b611bfbca  E12  flown: Chad's tape 8
      dfa61721e  E13  the outnumbered roster, 7 v 3
      25ad02150  E14  a defender slot goes to someone who can fly it
      4f3d2ee8e  E15  the tunnel timeout was a range limit (SHIPPED OFF)

- **NOT PUSHED.** He said "save and commit this for now" — that is not a push
  instruction. Main still waits on his word.
- Gate **1538/1543**. The 5 reds are EXACTLY the pre-existing set: 4 GI4 sled
  debts (`sled_slides_before_it_tips_on_flat_snow`,
  `sled_grip_ceiling_stays_below_the_tip_threshold`,
  `sled_assist_reference_plane_is_load_weighted`,
  `sled_debug_sink_is_write_only`) and `probe P-F` clause (2), which is **red on
  purpose** — Chad's ruling to re-make, already re-pinned twice, **do not bend
  it a third time**.
- Change set touches **zero `assets/` files**. The costume lane's dirty tree in
  `D:\flight_sim2\seads-recon` is unaffected — re-check that before any push.
- FLY BUILD: `build-play` (RelWithDebInfo), relinked and verified newer than
  every compiled source. `cmake --build build-play --target seads`.
  The `seads_tests` target refuses to build there by design (SPEC 6.1 wants an
  assert-live gate); tests build in `build`.
- Graph regenerated in-tree, `graph_query.py check` OK.

---

## WHAT SHIPPED (every dial has a named one-edit walk-back at its site)

    raid_backfill              true      E12.1  game.toml   [was: absent]
    raid_dps_frac              0.65      E12.2  game.toml   [was 0.5]
    difficulty_params(4)  {3, 11.0, 14.0} E12.3 combat/kill.h [was {3,8,11}]
    kSudburyTeamSize           7         E13    conquest.h  [was 5/5]
    raiders_for_team()      proportional E13    conquest.h  [was a bare 2]
    assign_defense tunnel skip  on       E14    raid.h      [no dial: a fix]
    transit_reach_s_per_km     0.0       E15    scenario.toml  ★ SHIPS OFF

---

## HIS RULINGS THIS SESSION, VERBATIM (these are the authority)

1. **"get out of easy mode and increase its ability to destroy pumps"**, plus
   **"You can actually lose"** and **"They can get a bit deadlier too."**
   ⚠ That last one REVERSES the previous handoff's bolded "more lethal is NOT
   the goal, he has ruled the balance twice" — one day later, unprompted.
   HIS WORDS WIN. It ships as one separately-revertible dial (E12.3).
2. On tape 8: **"a good easy / medium baseline for difficulty"** + "save and
   commit this for now".
3. **"We could just increase the number of enemies now 2/1. We can be
   outnumbered and that will increase the difficulty."** → E13.
4. On tape 9: **"that last match was pretty good!"** + the three observations
   that became E14 and E15.

---

## THE FLY RECORD — WHAT ACTUALLY CHANGED FOR HIM

    metric                     tape 7 (SIGNED)   tape 8 (E12)   tape 9 (E13)
    match length                  22.4 min         17.6 min       9.8 min
    HIS pumps lost                    0            1 (surface)   1 (surface)
    his deaths to enemy fire          0            1 (GUNS)          0
    enemy score                       0              100            100
    rounds aimed at him           39 (1.74/min)  16 (0.91/min)      0
    enemy time HUNTING him        848 s (19.1%)  393 s (8.3%)   284 s (8.5%)
    enemy time on RAID duty      1016 s (22.9%) 1398 s (29.5%)  44-52% of live
    AI crashes                     0.94/min       1.25/min       0.51/min
    his gun kills                     9              4               3
    his ALLIES' kills                 -              -               4

★ **TWO FIRSTS IN TAPE 8**: the AI took one of his pumps (11:39) and enemy fire
killed him (16:53, right wing shot off, `air=1.000` — not a thin-air death).
★ **THE TRADE E12 MADE, and nobody should later read it as a defect:** the raid
was paid for OUT OF THE HUNT. Raid duty and hunt duty draw on the SAME wing, and
E12.1 made raid duty continuous. Rounds fell 1.74 → 0.91/min — **but 39 rounds /
0 kills became 16 rounds / 1 kill.** He ruled the result good. Do NOT "fix" it
by walking E12 back.
★ **TAPE 9's ZERO ROUNDS IS NOT CLAIMED AS A SEPARATE EFFECT.** The duty
collapse is attributable and measured; a 9.8-minute match on top of it is a
small sample. What IS solid: of 49 s spent inside the [60,900] m gun band
holding him as foe, 15 s was BFM Extend and 5 s Defensive (guns-cold BY DESIGN),
leaving **~29 s of Offensive/Yoyo/Intercept with no fire intent at all.** That
is the E1/E7 fire chain, and it is where "aggressive" actually lives.

---

## ★★★ THE INSTRUMENT — PROBE P-H (`test/unit/test_conquest_match.cpp`)

A whole 22-minute conquest match: real loaders, real arena and domes, the app's
own helpers and tick order, and **CHAD REPLAYED** out of his own tapes —
trajectory at 5 Hz, the pilots his guns killed, the pumps his side destroyed.
Distilled by `offline_tool/distill_conquest_tape.py` into
`test/golden/conquest/`, which now holds **three** tracks:

    tape7_chad_replay.txt   the SIGNED match (5/5 roster)  <- P-H's default
    tape8_chad_replay.txt   the E12 fly      (5/5 roster)
    tape9_chad_replay.txt   the E13 fly      (7/3 roster)  <- the ONLY one
                                                              flown at the
                                                              SHIPPED roster

⚠ **ITS FOUR LIMITS ARE IN THE FILE BANNER AND EVERY CLAIM MUST CARRY THEM:**
the replay is OPEN LOOP (a changed AI changes where his victims were), his BANK
is not in the tape, his side's pump offense IS the replay (so its credit is
suppressed — otherwise the same damage counts twice), and there are NO
BALLISTICS (attrition is the replayed kill schedule; P-H says nothing about
lethality — that is P-A's job).
⚠ **IT IS A RELATIVE INSTRUMENT.** Read deltas between arms, never absolute
difficulty. It is enemy-favourable against the real game.
⚠ **THE TAPE-7 CALIBRATION IS NOW INFORMATIONAL.** It reproduced tape 7's
attrition to +0.2% — at 5/5, once, before any arm was believed (§E12.I). E13's
re-split means the fixture can no longer fly the world tape 7 was flown in, so
that clause is wing-SCALED and explicitly demoted in the file. **A fresh
calibration wants a track flown at the shipped roster — `tape9` IS that track,
and wiring it in is cheap, useful, and would also retire P-H's n=1 caveat.**

HIDDEN SWEEPS (measuring instruments, not gates — one whole match per value):
`[.e12sweep]`, `[.e15sweep]`. Run with `./build/seads_tests.exe "<name>" -s`.
A P-H arm is ~45 s; budget for it and run them in the background.

---

## OPEN, RANKED

1. ★★★ **THE VACUUM CRASH DEFECT.** See the top of this file. Critical path.
   It blocks E15 and it is the largest single lever on AI quality left.
2. ★★ **E15, WAITING ON (1).** Mechanism, latch, loader band and three tests are
   in place and green; `transit_reach_s_per_km = 0.0 -> 11.0` is the one edit.
   Its contract tests name their own rate rather than reading the shipped 0.0,
   deliberately, so they do not go vacuous while it waits.
3. ★★ **THE FIRE CHAIN — "not aggressive".** ~29 s of Offensive/Yoyo/Intercept
   inside the gun band produced zero fire intent in tape 9. E1/E7 territory,
   P-A is the fixture; E8.2's Intercept exclusion (red team P1-8) and the E7.2
   scoped dive envelope (P0-1/2/3) are still open there.
4. ★ **THE HUNT/RAID CONTENTION.** Both duties draw on the same wing; E13 put 3
   of 7 permanently outbound. It is a ROSTER question (wing size, pool, or a
   third duty state), NOT a dial, and he ruled the current result good — so this
   is a design question to put to HIM, not a regression to chase.
5. **THE TRANSIT STALL, second half.** E15 explains the timeouts; it does not
   explain drone 4 covering 1.8 km in 150 s. Negative closure / FIX overshoot is
   unexamined.
6. **A NEW LOSS CONDITION HE HAS NOT SEEN**: 3 allies vs 7 with the same pool of
   4 makes victory-by-wipe against HIM materially reachable.
7. Carried forward unchanged: tunnel-net collisions (5 of 22 in tape 9, a
   DIFFERENT defect from (1) — near-level, underground); E8.3's tunnel pump and
   the empty second half.
8. `probe P-F` clause (2) — **Chad's ruling to re-make. Do not bend it.**

---

## ★★★ PROCESS PAID FOR THIS SESSION — READ BEFORE EDITING

* ★★★ **THE LAW, NOW PAID FOR FIVE TIMES** (`ai_guns_on`'s band, E8's decision
  table, E12's two, E13's five, E15's one): **a constant that DESCRIBES the
  shipped table stops describing it the moment the table moves, and NOTHING GOES
  RED.** Two were genuinely silent: a test that spelled
  `replace_all(src, "raid_dps_frac = 0.5", ...)` **passed vacuously** once the
  dial moved, and the E2.1 grace test was measuring E15 instead of the grace.
  **Write tests against the RULE, and take mutations from the LIVE line.**
* ★★★ **I BUILT A DEAD BRANCH AND MY OWN TEST CAUGHT IT.** `defend_fight_in_place`
  gave pump defenders their guns back, mirroring E6.5, with a confident comment
  claiming it fixed his complaint. It could never fire: `fight_hot` releases the
  defend branch at 2500 m and the guns only open inside 900 m. **Write the
  differential BEFORE believing your own mechanism story.**
* ★★ **CANCEL A TERM ON BOTH SIDES OR NEITHER.** P-H's first cut divided the DPS
  credit by `battery_dps` but not the pump budget, so its pumps were
  `1/raid_dps_frac` too tough and it reported a WEAKER enemy than the game has.
  **A probe wrong in the flattering direction is the dangerous kind.**
* ★★ **MEASURE THE OBVIOUS HYPOTHESIS BEFORE FIXING IT.** "The Sudbury mouth
  must be further away" was false — the geometry FAVOURS the enemy (12.4 km vs
  15.1 km). Measuring it is what stopped a plausible wrong fix.
* ★ **A TEST GOING RED CAN BE THE TEST WORKING.** P-B's clause (2) welded a 30 s
  bar with a comment asking that a retune "move the pin honestly instead of
  silently" — it did. Re-derived, not re-welded. Same for the furball crash
  clause, where `crashes == 0` turned out to be the OBSERVED value on the old
  spawn geometry, never a bound; re-anchored to **Chad's own signed 0.94/min**
  and marked a CEILING, NOT A TARGET.
* ★ **HIS TAPES ARE AN INSTRUMENT, NOT A REPORT.** Every finding this session
  came out of `conquest_tape_*.jsonl` with a dozen lines of Python. Per-drone
  `raid`/`strike`/`def`/`leash`/`wf` flags, `rpi`/`spi` targets, `foe`, `mav`
  and `bfm` modes, positions at 5 Hz, pump HP, and the `dk`/`da`/`dc`/`dr`/`ef`
  event streams. **Decode the tape before theorising.**
* ⚠ A failed build leaves the old binary and it WILL answer you. Confirm the
  link before believing any number. Do not relink while a probe is running.

---

## ★ CHAD'S FLY CHECKLIST (E14 is the only unflown behaviour change)

1. **Do their pumps get DEFENDED now?** E14's whole content: two defender slots
   used to go to pilots mid tunnel-run who structurally could not fly the order.
   Attack their surface pump and see whether more than one shows up.
2. **Can you still lose?** Both your pumps should stay under real pressure.
3. **Do they still attack and defend?** His words, and the thing that made this
   a loop. Underground contact is still weak — that is items 1/2 in OPEN.
4. **Crashes.** He signed 0.94/min; tape 8 read 1.25. If it climbs, that is the
   critical path, not a mystery.
5. **Is "a bit deadlier" a bit, or a lot?** Nothing we own predicts his dodge;
   that number is his alone.
