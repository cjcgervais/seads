# ENEMY-AI — THE OVERNIGHT AUDIT. HANDOFF 2026-08-25

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260825_enemy_ai_AUDIT.md; run the
overnight enemy-AI audit."**

Supersedes `docs/SESSION_HANDOFF_20260824c_enemy_ai.md` for *direction* only —
that document and `docs/ENEMY_AI_E1_E2_SPEC.md` remain the binding record of
what was built and why. Read both before starting.

---

## ★★★ CHAD'S INSTRUCTION FOR THIS SESSION, VERBATIM

> lets make a handoff now to ask the next instance to ggo for an aumomatic
> overnight audit and fix of the work done in enemy-ai. To use a series of
> consults and with careful deliberation from analysis of the tapes to take out
> well verified major takeaways from the tapes and what was done so far. To also
> get context free red teams to analyse the ai. Make sure that game loop context
> is explicit in the consult packet. For next sessio please write the handoff
> and include this message

**Read that as the whole brief.** It is an UNATTENDED, OVERNIGHT run: nobody is
awake to answer a question. Every blocking ambiguity must be resolved by
recording BOTH readings and shipping the safe one OFF, never by guessing and
never by stalling.

---

# 1. WHY THIS SESSION EXISTS

Three rungs (E16, E17, E18) were built in about a day, and the honest state is:

* **The AI now wins.** Tape 11: it took both of Chad's pumps in 7 min 36 s while
  he loitered and watched. He signed it — *"I call that a win!"*
* **But three of the last four attempted fixes are shipped OFF or unverified**,
  and the same defect has now been found in THREE different places. Nobody has
  stepped back and audited the whole thing with fresh eyes.
* **I got the attribution wrong once and the shipped table wrong twice** in the
  space of two rungs (§6). That rate of self-correction is the reason Chad asked
  for context-free red teams rather than another build session.

The job is **audit first, fix second.** Do not open a new rung until §3 is done.

---

# 2. ★★★ THE GAME-LOOP CONTEXT BLOCK — PASTE THIS INTO EVERY CONSULT PACKET

Chad asked for this explicitly. **A red team that does not know the game loop
will "fix" the AI into something that cannot play it.** This block is
self-contained on purpose; it is drawn from `config/game.toml [conquest]`,
`Game_loop_idea/MASTER_PLAN.md` §4 and §2.5, and the E-ladder spec — verify it
still matches config before pasting, and update it here if it has drifted.

```
=== SEADS / SCARCE SKIES — THE CONQUEST GAME LOOP (context for this review) ===

THE WORLD. A ~15 km-radius spherical planet (Sudbury, Ontario, on a real DEM),
permanently winter. The air is SCARCE: breathable air exists only inside two
faction DOMES plus a thin "go-anywhere" deck that hugs the terrain. Outside
both, lift and thrust lapse and an aeroplane mushes and falls. Air is the
strategic resource; the map is mostly hostile vacuum.

THE TWO SIDES. VALLEY (blue) and SUDBURY (red), each a union of 3 atmosphere
bubbles over its towns. THE PLAYER FLIES VALLEY, and is OUTNUMBERED 3 v 7
(Chad's E13 ruling: "We can be outnumbered and that will increase the
difficulty"). The whole AI maverick fleet is SUDBURY and hostile.

THE OBJECTIVE — PUMPS. Each faction has exactly TWO pumps that make its air:
  * a SURFACE pump, in the open
  * a DEEP pump, in the "black stope" -- an underground cavern (the ARENA)
Killing an enemy pump:
  * SHRINKS the victim's dome by half (both pumps dead => THE DOME IS GONE,
    floor 0.0 -- the faction literally loses its air)
  * GROWS the destroyer's own dome by 25% of baseline
A pump takes ~15 s of sustained all-guns-on-target fire (pump_kill_seconds).
AI raiders/strikers deal a configured FRACTION of player battery DPS while
"on-station" (in range and pointed at the pump) -- the damage is credited
app-side on that envelope, NOT by per-round hit tests.

THE TUNNEL. An underground bore connects Errington (VALLEY side) to Murray
(SUDBURY side), ~1800 m below the surface at its deepest, passing through the
ARENA -- a 4200 m-wide, 2600 m-tall open room where both DEEP pumps sit. The
tunnel holds FULL AIR regardless of the dome war, and the terrain-crash
predicate is SUSPENDED inside the net volume (flying 2 km under the terrain is
the point). Leaving the net while still underground = an instant
deep-penetration wall strike. The tunnel is the back door: it is how a strike
reaches the enemy DEEP pump, and how the player can be flanked.

THE AI'S STANDING MISSION (Chad's 2026-07-26 ruling): Murray tunnel -> black
stope -> kill the VALLEY deep pump -> Errington tunnel -> the VALLEY surface
pump. THE PLAYER'S JOB is to stop them and kill the two SUDBURY pumps.

THE FEEL CHAD IS AFTER, in his own words across the ladder:
  * "I want killers to contend with. That is a rule."
  * "get out of easy mode and increase its ability to destroy pumps"
  * "they should relentlessly attack it and fight me at the same time, not fly
    away to safety and then fight me"
  * "they need to try to keep killing the pump, not shoot it and fly away"
  * on the AI taking both his pumps unaided: "I call that a win!"
He has SIGNED the current difficulty as "a good easy / medium baseline" and has
signed the game loop itself. THE BAR IS NOT "the AI should win" -- IT ALREADY
WINS. The bar is that it should be READABLE, FAIR, and look like flying.

WHAT COUNTS AS A REGRESSION HERE:
  * an AI that stops fighting the player in the air (AI-vs-AI attrition and
    player pressure are both content)
  * an AI that presses objectives so hard the air war goes quiet
  * aeroplanes that die to geometry rather than to the player
  * anything that reads as a bug on screen (no tracers, sliding, teleporting)
=== END GAME-LOOP CONTEXT ===
```

---

# 3. THE AUDIT PROTOCOL — DO THIS IN ORDER

## 3.1 The tape pass (evidence before opinion)

Eleven tapes exist, `build-play/conquest_tape_1..11.jsonl`; analyzer
`tools/ai_tape.py <tape>`. **Tapes 7-11 are the ones flown against the current
machine.** Tape 10 and 11 are the E16/E17 world; tape 11 is the win.

Produce a **TAKEAWAYS LEDGER** — Chad asked for "well verified major takeaways".
The bar for an entry:

1. It is measured from at least **two** tapes, or from one tape plus a
   closed-loop probe that reproduces it.
2. It states the NUMBER, not the impression.
3. It names what would falsify it.
4. ★ It has been checked against the **SHIPPED CONFIG TABLE**, not a struct
   default (see §6 — this exact error was made twice).

Known-good starting points, all re-verifiable:
* every in-net wreck across tapes 10 AND 11 dies 33-54 m below the deep pumps'
  radius (11289.9) — 7 of 7
* tape 11's wrecks are BIT-IDENTICAL to tape 10's for the drones the player
  never touched (same tick, same metre) — the early match is deterministic, and
  that is a powerful differential tool: **any real change must break that
  identity.**
* P-H's `on-station` counter reads an identical 46.2 s across five different
  trajectories — **suspected stuck; audit it before trusting any raid ruling
  ever made on it.**

## 3.2 The consult series (fresh context each time)

Chad asked for **a series of consults** and **context-free red teams**. Run them
as separate agents with NO memory of this session's conclusions. Each gets the
§2 block, the artefacts, and ONE question. **A red team must re-derive from the
artefact, never from my report** — that is the standing rule
(`red-team-major-work` memory).

Suggested series (adapt, don't follow blindly):

| # | consult | gets | asks |
|---|---|---|---|
| C1 | THE SATURATION THESIS | §2 + `drone/drone.h` raid + strike blocks + `bore_track` | "Three separate pursuit laws were found saturating both channels at once. Is that one defect or three coincidences? Is there a single correct fix?" |
| C2 | THE TAPE RED TEAM | §2 + tapes 9/10/11 + `tools/ai_tape.py` | "Derive the top 5 AI failures from the tapes alone. Do not read the handoffs." |
| C3 | THE SHIPPED-OFF AUDIT | §2 + §4's table | "Four dials are built and shipped OFF. For each: should it ship, and what is the evidence?" |
| C4 | THE FIXTURE AUDIT | §2 + `test_conquest_match.cpp`, `test_stope_probe.cpp` | "P-H is blind to a defect that killed 7 aeroplanes; P-B's control arm moves when a dial moves. Which of these fixtures can still be trusted, and for what?" |
| C5 | THE GAME-FEEL RED TEAM | §2 + tape 11's timeline | "The AI wins in 7:36 against a passive player. Is that the game Chad described? What is missing?" |
| C6 | THE VERIFIER | everything the above produced | "Attack each conclusion. Which survive?" |

Then **converge**: contradictions between consults are the most valuable output.
Write them down rather than picking a winner.

## 3.3 The fix pass

Only after 3.1 and 3.2. Rules:
* Every dial 0-OFF and bit-identical when off. No exceptions.
* Every fix gets a **differential** — an arm that CANNOT differ, and a liveness
  arm that MUST (§6).
* **Never bend a gate to green.** The baseline is 1540/1545 with 5 known reds.
* If a fix cannot be paid for, **ship it OFF and write down the price.** That is
  the E15/E17/E18 precedent and Chad has accepted it three times.

---

# 4. STATE: WHAT IS BUILT, AND WHAT IS OFF

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`.
**Commits — NOT PUSHED:**

    cc0c38626  E15/E16/E17: the deck's frame, two saturated pursuit laws
    0c8ec01e0  E17 FLOWN: the enemy won unaided; the deaths are the STRIKE DIVERT
    4d9c4b6a8  E18: the strike divert -- diagnosed, probed, SHIPPED OFF

Gate **1540/1545**. The 5 reds are the documented baseline: 4 GI4 sled debts +
`probe P-F` clause (2), red **on purpose** by Chad's ruling. **Do not bend P-F a
third time.**

| dial | value | state |
|---|---|---|
| `deck_terrain_relative` | true | **ON**, flown, signed |
| `transit_reach_s_per_km` | 11.0 | **ON**, flown |
| `run_recover_alt_m` / `run_recover_bank_cap_deg` | 300 / 25 | ON but **UNPAID FOR** — proven live, never shown to fix anything flown |
| `run_stall_s` / `run_stall_arc_m` | 8 / 150 | ON but **UNPAID FOR** — same |
| `raid_attack_alt_m` / `raid_reattack_m` | 0 / 0 | **OFF** — works in isolation; takes AI-vs-AI attrition to zero |
| `strike_attack_alt_m` / `strike_glide_limit` | 0 / false | **OFF** — makes the deep pump die for the first time; moves P-B's control arm |

## Instruments (all hidden `[.tag]`, all read config, never gate legs)

    [.e17raid]   isolated: does a raider come back? (closed loop)
    [.e17track]  replays tape tracks through the SHIPPED net + real DEM
    [.e17sweep]  a whole match per arm, WITH A LIVENESS ARM
    [.e18dive]   ★ closed-loop on the shipped net + real DEM; REPRODUCES the wreck
    [.e16air] [.e16sweep] [.e12sweep] [.e15sweep]   earlier rungs

⚠ A P-H arm is ~45-60 s; a sweep is minutes. **Never relink while one is
running** — a failed build leaves the old binary and it WILL answer you.

---

# 5. THE THREE OPEN RULINGS (Chad's, not the audit's)

1. **The raid attack pattern.** Now optional — the AI already wins without it.
   Turning it on makes raiders persist but takes AI-vs-AI attrition to zero,
   because the raid branch mutes guns except at the player. One line.
2. **`strike_attack_alt_m = 300.0`.** Makes the deep pump actually fall
   (`run_crashes` 10→0, `dead2` 0→1) at the cost of the strafing read (no
   tracers). Blocked on P-B's control arm moving.
3. **The general law.** The same saturated-pursuit defect now sits in the raid,
   the strike divert, and (suspected) the bore. **Is there one fix instead of
   three?** That is C1, and it is the most valuable question in this handoff.

---

# 6. ★★★ THE LAWS THIS LADDER HAS PAID FOR — THE AUDIT'S OWN CHECKLIST

* ★★★ **A CONSTANT THAT DESCRIBES THE SHIPPED TABLE STOPS DESCRIBING IT THE
  MOMENT THE TABLE MOVES.** Paid 6+ times. **I did it again in E18** — wrote the
  entire first analysis against `StrikeOrder{}` struct defaults (6500 m / 35°)
  instead of config (9000 / 50°), and "arms at ~6490 m" was an artifact of a
  6500 threshold in my own analysis script. **Every probe must read the
  LOADER.**
* ★★★ **WRITE THE DIFFERENTIAL BEFORE BELIEVING YOUR MECHANISM STORY.** E17: I
  was certain the tunnel guidance commanded a descent. It was commanding a
  **full +44.1° climb, pinned at the cap, the whole way down.**
* ★★★ **A BIT-IDENTICAL ARM MEANS BLIND FIXTURE *OR* DEAD BRANCH — ADD A
  LIVENESS ARM TO TELL THEM APART.** A 1 m arm proved E17's branch live while
  the shipped value was a no-op.
* ★★★ **WHEN YOU ADD AN EXIT TO A STATE MACHINE, IT INHERITS EVERY DEFERRAL THE
  OLD EXITS CARRY.** My RUN bail ignored `ro.hold_exit` and pulled strikers out
  of the chamber mid-strafe (P-C → zero rounds).
* ★★★ **A CHANGE THAT MOVES THE CONTROL ARM OF THE PROBE THAT GRADES IT CANNOT
  BE GRADED BY THAT PROBE.** E18 vs P-B.
* ★★ **GUARD EVERY `normalize()`.** `RaidOrder`'s default target is the ORIGIN;
  the NaN reached the plant as a hard fail-fast in three tests.
* ★★ **MEASURE A METRIC'S NOISE FLOOR WITH ARMS THAT CANNOT DIFFER BEFORE
  RULING ON IT.** E18's crash/survive was non-monotone (300 survived; 150, 500
  and alt+glide did not).
* ★★ **WHEN A BEHAVIOUR RESISTS SOUND FIXES, STOP FIXING THE BEHAVIOUR AND
  MEASURE THE ENVIRONMENT IT IS BEING ASKED TO SURVIVE.** That was E16 — five
  sound fixes had been aimed at a fleet obeying orders correctly into a world
  that had stopped honouring its own documented floor.
* ★ **A REPORT CAN BE TRUE AND ANSWER A QUESTION NOBODY ASKED.**

---

# 7. HOUSEKEEPING FOR AN UNATTENDED RUN

* **Do not push.** Main advances on Chad's stick. Commit freely on
  `sandbox/enemy-ai`.
* **Do not touch `assets/`.**
* Regenerate the graph in the SAME commit as any structural change
  (`tools/graph/graphify.py`; `generated/graph/` is tracked) and run
  `tools/graph/graph_query.py check`.
* Relink the fly build (`cmake --build build-play --target seads`) before
  finishing, so Chad can fly whatever you leave.
* ⚠ A hung `ctest` can hold `seads_tests.exe` and block every relink. The test
  *"orbit inertia: back-to-back grabs blend by the EMA factor"* hung for 80
  minutes on 2026-08-24 and **nobody has investigated why** — that is itself a
  worthwhile audit item.
* Leave a handoff and ring `BELL.md` if the overnight-loop convention is in use.

## What Chad should find in the morning

1. The **TAKEAWAYS LEDGER** (§3.1) — the verified truths about this AI.
2. The **consult/red-team findings**, including the contradictions.
3. Any fix that could be paid for, gated and committed; anything that could not,
   **shipped OFF with its price written down**.
4. A short, blunt answer to: **is the enemy AI good, and what is the single
   biggest thing still wrong with it?**
