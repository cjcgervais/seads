# ENEMY-AI — RUNGS E9/E10/E11, **THE GAME LOOP IS SIGNED**. HANDOFF 2026-08-23

LAUNCH LINE: "Read docs/SESSION_HANDOFF_20260823_enemy_ai.md; take the next
enemy-AI rung." Binding spec + full ledger: `docs/ENEMY_AI_E1_E2_SPEC.md`
(E9 = §E9.0–E9.7 incl. the E8 red-team ledger, E10/E11 = the final sections).
Supersedes `docs/SESSION_HANDOFF_20260822_enemy_ai.md`, whose OPEN items are all
paid.

## ★★★ CHAD SIGNED IT (conquest_tape_7, 2026-08-23, 22:23, signature VERIFIED)
> "it was good, they attacked underground, had my first dogfight underground,
> more formidable opponents that shoot at me, and not insanely difficult, very
> nice balance for now, **I think this qualifies as a game loop.** I had fun,
> many suspenseful moments."
and, ruling the follow-up:
> "Right now im satisfied with the enemies **as long as they dont regress**. I
> like them attacking and defending. The added speed for them has helped."

**THE BASELINE NO RUNG MAY REGRESS** — this is the number, not the feeling:

    metric                     tape 100    tape 5   tape 6   TAPE 7 (SIGNED)
    rounds aimed at him        5 / 49 min    18       14      39 / 22 min
    time under fire                0.8 s     43 s    295 s    843 s (37%)
    AI terrain crashes/min           --     0.93     1.50     0.94
    his deaths to enemy fire          1        0        0        0

★ His only death was his own terrain crash at 02:50, every component at 1.00.
★ "Not insanely difficult" is measurable too: **39 rounds aimed, none hit.** The
balance he signed is a fleet that shoots OFTEN and connects RARELY. Do not read
"more lethal" as the goal — he has told us twice what the target feel is.

## STATE
- **PUSHED TO MAIN @ `651ae181b`**, gate **1528/1533**. `sandbox/enemy-ai`,
  `main` and `D:\flight_sim2\seads-recon` are all on that commit.
- `D:\flight_sim2\seads-recon` is DIRTY with the COSTUME/SWEATER lane's work
  (`assets/character/*`, `assets/sled/indy650.glb`). Not ours. Do not stage,
  clean, or commit it. This session's push touched zero `assets/` files, which
  is why the fast-forward was safe — check that again before the next one.
- `D:\seads_sandboxes\enemy-ai-rt` is the E8 red team's detached worktree. Its
  report is adjudicated (§E9.6) and its probes are preserved at
  `docs/e8_redteam_probes.patch`. Disposable:
  `git worktree remove D:/seads_sandboxes/enemy-ai-rt --force`.
- FLY BUILD: `build-play` (RelWithDebInfo). Rebuild with
  `cmake --build build-play --target seads` — the `seads_tests` target REFUSES
  to build there by design (SPEC 6.1 wants an assert-live gate).

## WHAT SHIPPED
    pursue_track_pitch_gain  1.5     E7.4/E9  (the tracking pitch gain)
    ace_bank_cap_deg         78.0    E10      (was 72)
    mid_bank_cap_deg         76.0    E10      (was 65)
    ace_bank_track_hi_deg    0.0     E10      the DEADBAND IS OFF (was 90)
    avoid_air_dive_agl_m     1500    E11      ⚠ UNCONFIRMED, see below
    bfm_fight_speed_mps      120     E8       (unchanged)
    slash_doctrine           false   E7.2     REFUTED, see §E9.6 — reopenable
Plus E11's conquest fix (no dial): a faction with no living pumps has no sky and
the clock arms on it. Walk-backs are one line each and named at every dial.

## ★★★ THE FINDING THAT BROKE THE WALL, because three rungs missed it
Turn rate is `g*tan(phi)/V` under a BANK limit and `g*sqrt(n^2-1)/V` under a G
limit. **Chad flies the second — his own tape reads a median 4.3 g in close with
peaks of 25 — and the AI flew the first at n = 1.3-5.5.** At his speed that is
72 deg/s against 8. E7 blamed the bank cap's *repeal*, E8 blamed energy, E9
blamed the pitch cycle; all three were downstream of an aeroplane that was never
allowed to pull. And above the cap sat the E7.1 deadband, which handed the
authority BACK to the ruled 55 inside 30 deg of his tail — the only cone where a
gun solution exists.
★ It came out of a throwaway line of his ("if I want a sharp turn I can cut
throttle a bit"), not from the ladder's plan. When he says something offhand
about his own machine, go and measure the airframe.
★ AND E9'S OWN PREMISE FELL WITH IT: the 14-deg pitch limit cycle E9 existed to
cure was an ARTEFACT of the deadband handing back 55 deg. At 78 deg of bank the
same fixture reads 0.19 deg of swing.

## ★★ THE PLANT REFUSED THE BIGGER NUMBERS — do not re-litigate without measuring
    cap        72     76     78     80     83
    turn deg/s  6.2    8.7   10.9   14.8   26.3   (30 deg bearing, 144 m/s)
  * 83: the aeroplane MUSHES it — achieved 46-54% of the command at 50/70/90 deg
    bearing, with 103-120 deg of flight-path swing.
  * 80: holds the bank but swings 13.9 deg abeam (half the fire cone) AND sits on
    an INSTABILITY BOUNDARY — sweep `pursue_track_pitch_gain` at cap 80 and
    bearing 50 reads clean at 1.5, catastrophic at 1.2 and 1.0, clean at 0.8.
  * The loader bound of 80 was RIGHT FOR THE WRONG REASON ("80 deg is n = 5.8,
    past the airframe's usable load factor" — `n_max` is 32). It now cites the
    measurement.
  * ★ 1.5 is on the good side of that instability band. A future rung lowering
    `pursue_track_pitch_gain` MUST re-take `[.e10hold]` first.

## THE REGRESSION GUARD (his ruling, as a test)
`E10: the SIGNED tracking authority does not regress` pins the BEHAVIOUR, not
the dials — four clauses, because three of them can each be met while giving the
rung away: the turn rate, the LOAD FACTOR behind it (so it cannot be
commanded-and-mushed), that the authority REACHES the tracking cone at all
(config-relative vs `pursue_max_bank`), and that the turn stays CLEAN against
half the fire cone. Mutation-verified both ways. **If you need to break it, come
and say so out loud — it quotes him.**

## ⚠ ONE RED TEST IS ON MAIN ON PURPOSE
`probe P-F: the relentless raider keeps the pump and shoots back`, clause (2).
E6.5's corroboration died honestly: with the tighter turn the YIELDING raider now
stays 2.5 km from the pump and fires 13 rounds, while the PRESSING one ends
4.7 km away with 3 s on station of 90. It was never really keeping the pump; it
only looked that way beside a raider that flew further. **This is CHAD'S RULING
TO RE-MAKE. It has already been re-pinned twice this session — do not bend it a
third time.** The four other reds are the pre-existing GI4 sled debt, verbatim.

## ★★★ THE NEXT RUNG IS THE INSTRUMENT, AND IT IS NOW OVERDUE
Two independent agents reached this from different directions.
1. **P-A's scripted player cannot express his fight** (E8 red team): its hardest
   turn (2.36 g) is below his MEDIAN in-close turn (4.3 g), and it flies a
   CONSTANT 275 m/s = 112% of `v_redline`, unreachable by any bandit forever. So
   the ENERGY column is dead by construction there.
2. **P-A NEVER DESTROYS A PUMP**, so its bubbles never shrink, so the air-seek
   dive never fires — which is why FIVE candidate fixes for the crash chain all
   measured null on it. The fixture is structurally blind to a whole class of
   conquest defects.
BUILD: a conquest probe that actually kills pumps, and/or replay Chad's RECORDED
trajectory from `conquest_tape_7.jsonl` as the target into `drone::tick`. The
house already owns the pattern (the sled rung's `tape_360_chad_repro`), and the
recorder writes pos+vel at 5 Hz — interpolation is the only work.

## OPEN, RANKED
1. ★ THE INSTRUMENT (above). Nothing else is worth measuring first.
2. THE CRASH CHAIN — diagnosed, unconfirmed: thin air → air-seek dive → redline →
   control authority compresses to `min_frac` 0.4 → the pull-up (ARMED on 100% of
   wrecks) cannot rotate. `avoid_air_dive_agl_m` ships ON but NULL-measured;
   walk-back is 0. Chad is content with the current rate ("I dont see problems
   with crashing"), so this is not urgent — but do not claim it is fixed.
3. TUNNEL-NET COLLISIONS — 6 of 21 wrecks in tape 7, RUN duty, near-level flight.
   A different defect from (2).
4. E8.2's INTERCEPT EXCLUSION (red team P1-8): Intercept was 51% of in-close
   ticks in the tape E8.0 attributed, and E8.2 excludes it on a rationale that is
   false inside `attack_range`. The dial fixed 49% of its own diagnosis.
5. THE SCOPED DIVE ENVELOPE (red team P0-1/2/3): E7.2 is not "unflyable", it is
   CLAMPED by `pursue_max_gamma`, which is shared with the terrain pull-up — the
   exact problem E7.4 solved by scoping. At double the envelope the slasher fires
   14 rounds.
6. E8.3 THE TUNNEL PUMP and THE EMPTY SECOND HALF — both still unbuilt.

## ★ PROCESS, PAID FOR IN THIS SESSION
* **A FAILED BUILD LEAVES THE OLD BINARY AND IT WILL ANSWER YOU.** Bit twice:
  once from a mutation loop that restored a file without rebuilding, once from a
  compile error inside a `&&` chain. **Confirm the link before believing any
  number.** A result that contradicts an impossible-to-contradict fact (arm names
  that no longer exist, a "cure" that changes nothing) is the tell.
* `git checkout -- <file>` to undo a mutation ALSO discards uncommitted work in
  that file. Copy aside, or commit first.
* ★★★ **AN ARM DEFINED BY READING THE SHIPPED TABLE STOPS BEING AN ARM THE MOMENT
  THE TABLE MOVES, and nothing goes red when it does.** Hit E8's decision table
  (four identical rows under four labels, passing for a whole rung) and then E9's
  own leg (its "wall" evaporated when E10 moved the cap). Construct the regime.
* **MEASURE THE NULL FLOOR BEFORE RULING ON A COLUMN.** The crash column looked
  like a 4x cost; the floor came back at ±10% so it was real — but the same
  discipline retracted a "parameter chaos" claim of mine that used a floor arm
  too small to bound anything. ★ A caveat you write and then step over is worth
  less than no caveat at all.
* **THE PLANT IS THE RESOLVABLE INSTRUMENT.** Pick values on the deterministic
  saturated turn; ask the furball only "does this cost anything at 2x". Every
  ruling this session that stuck came off the plant.
* **A GUARD THAT CHECKS ONE CONDITION LETS BAD VALUES THROUGH.** The G-honesty
  leg checked turn cleanliness at ONE bearing, which is how cap 80 nearly
  shipped — it passes the bank-hold clause everywhere.
* Do not relink while a probe is running; copy the exe if you need both.

## CHAD'S FLY CHECKLIST (next time there is something to fly)
1. **Do they still fight like tape 7?** 39 rounds aimed at him in 22 min, 843 s
   under fire, and none of them hitting. Both halves matter — more lethal is NOT
   the goal, he has ruled the balance twice.
2. **Do they still attack and defend?** His words. Underground contact and pump
   defence are what turned this into a loop for him.
3. **Does the match still END?** Kill both enemy pumps: their dome must go to
   ZERO and the countdown must arm (13:02 → 13:04 in tape 7).
4. **Crashes** — he is content at ~0.94/min. If it climbs, item 2 in OPEN is why.
