# SESSION HANDOFF — the enemy-AI lane, 2026-08-31

**LAUNCH:** *"Read `docs/SESSION_HANDOFF_20260831_enemy_ai.md` in
`D:\seads_sandboxes\enemy-ai`; do §8."*

**Lane:** `lanes.ai` — branch `sandbox/enemy-ai`, worktree
`D:/seads_sandboxes/enemy-ai`. Both `LANES.toml` and this doc are the lane's
own word; the registry entry is `verified` as of 2026-08-30.

---

## 0. READ THESE TWO FILES BEFORE YOU TOUCH ANYTHING

This changed on 2026-08-30 and it is not optional any more.

1. **`docs/CONTRIBUTION_SOP.md`** — how a lane lands work. Every rule in it was
   bought with a dated failure.
2. **`LANES.toml`** — who owns what (branch, worktree, source paths, status,
   in-flight), plus `tools/gate/lane_map.toml` for test→lane.

The parts that will bite you first:

- **Done = `.claude/hooks/gate.sh` green**, which checks the **RED SET, not the
  red count**, against `generated/gate/known_reds.txt`. A matching count proves
  nothing — two lanes once both reported "six reds" for different sixes.
- **`generated/gate/known_reds.txt` is machine-written. Do not hand-edit it, and
  never record a fresh red to go green.** Recording from your own gate log can
  silently *narrow* another lane's baseline and hide a test that genuinely
  passes elsewhere.
- **The nightly gates `origin/main`** into `D:\seads_sandboxes\_nightly_main\STATUS`.
  **Consume it, don't re-measure it.** ⚠ A STATUS whose `main:` SHA ≠
  `git rev-parse origin/main` is history, not a baseline; and `result:` ≠ `OK`
  means the gate DID NOT RUN — an instrument that could not run never says
  "no reds".
- **Escalation to another lane = an OBSERVATION** (test name, commit, repro
  command). **Never a diagnosis.** A lane published a theory naming another
  lane's kernel edit before running the measurement that exonerated it, and had
  to retract louder than the accusation.
- **End every session committed AND pushed to the sandbox branch**, even
  mid-rung. An uncommitted tree is invisible to every other lane.
- `sim/` and `control/` are the frozen kernel. No lane touches them, ever.

---

## 1. STATE

- **`origin/main` = `bbaf2a2d9`.** The lane branch is at the same commit and
  pushed. Tree clean.
- **GATE OK — the red set is EXACTLY the baseline, member for member: 6 failed
  of 1733.** `gate.sh` exit 0. Those six are the ruled debts:
  `E12.1`, `probe P-F` (ours), and the four `sled_*` (snow's).
- **Fly exe:** `D:\seads_sandboxes\enemy-ai\build-play\seads.exe`, stamped
  `pre-reconcile-20260821-264-gbbaf2a2d9`.
- ⚠ **NEITHER RUNG BELOW HAS BEEN FLOWN.** Both are built, gated, and pushed on
  Chad's word ("push it"), not on a fly. §2 is what to watch.

---

## 2. WHAT LANDED, AND WHAT TO ASK HIM ABOUT IT

### 2.1 `3e3aae60c` — ENV-3.5, the defence parabola as a ONE-SHOT

**Chad's doctrine, ruled 2026-08-30, and it is the SPEC for this manoeuvre, not
a one-time instruction:** *the dive ARMS ONCE (high AND far), is RIDDEN, and
EXITS on a gate (speed reached, or range) INTO THE MERGE.*
**⚠ IT IS NEVER A PER-TICK FLOOR.**

The dial block in `config/scenario.toml` and the seam in `drone/drone.h` both
carry the doctrine in full, deliberately — it lives next to the code so it
survives the session.

Shipped: `defend_dive_gamma_deg` 18 (30 inside 4 km),
`defend_dive_arm_alt_m` 400, **`defend_dive_exit_speed` 150**.

★★★ **THE TRAP, BECAUSE IT NEARLY SHIPPED TWICE.** The old build was a
sustained floor and measured `ai_damage` **EXACTLY 0** on the signed
certificate — the air war did not degrade, it *stopped*. His ruling was never
the defect; the build was. **But the first one-shot ALSO measured 0**, because
the exit gate was set at 245 = `v_redline`, and *the dive can never buy
redline*, so the gate never fired. **A one-shot with an unreachable exit IS the
sustained floor wearing a latch.** A bound is only a bound if the quantity it
tests can actually reach it. Swept at HEAD:

| exit speed | certificate |
|---|---|
| 150 | **GREEN** |
| 180 | `ai_damage` 1.03 |
| 210 | 0 |
| 245 | 0 |

Control (dive off) green — a cliff, not noise. The ceiling is now an **arm**
(`ENV3-A1`), not a comment.

★★ **`ENV3-A2` passed green the entire time the air war was dead**, because it
was a **stateless mirror of what became a stateful law**. A stateless mirror
cannot see a manoeuvre that never ends. It now carries the latch, with three
sections that grade the doctrine directly (the ride ends; it does not re-arm
inside one scramble; a new scramble gets a new shot).

**What to watch on his fly:** with their pump under attack and a defender
holding altitude — **one committed dive that ends in a merge**, not a long
mushing descent that arrives with nothing left.

### 2.2 `28aaac14f` — the DEATHMATCH

**He lost a match he had won.** Measured off tape 15:

| t | event |
|---|---|
| 504 s | his 2nd pump dies → his dome collapses, the 600 s clock arms on him |
| **1054 s** | **he kills THEIR LAST pump. Score 230–200 him. Clock reads 50.4 s** |
| 1104 s | buzzer → `out=2`, DEFEAT, while leading, every pump on the map gone |

**His ruling:** all pumps destroyed ⇒ the loss clock is **NULL**, no bubbles
anywhere, **team deathmatch, points battle to the last plane standing**. His
answer on the end condition: **waves stop, then last plane standing, points
break a tie.**

★★ **Most of it already existed — do not build it twice.** "No bubbles
anywhere" is already `collapse_bubble_if_pumpless`. "Last plane standing"
already works *once the clock stops*: waves need breathable air
(`enemy_waves_live` → `faction_air_breathable`), so a pumpless faction cannot
refill the sky, `reinforcements_live` goes false, and **the victory-by-wipe
route that flag was SUPPRESSING returns by itself.** The whole rung was nulling
the clock.

★ The one-shot "never re-arm" rule is **not repealed** — the clock is disarmed
outright, never re-pointed. Re-arming would hand the clocked side a free
extension.

⚠ **`countdown: arms ONCE` went red and it was SUPERSEDED, not broken.** With
two pumps per faction, "the second side loses its last pump" and "every pump on
the map is dead" are the SAME event, so its old assertion is now *unreachable*.
It keeps the still-true half (never re-POINTED) in its only reachable form.

**What to watch:** kill every pump; the clock must vanish and the HUD must read
`DEATHMATCH  <you> - <them>` in the clock's own slot.

### 2.3 His altitude report — ANSWERED, and the answer is "no defect"

His words: *"enemies and allies fly outside of the bubble and above me not
subject to the no air or they have a higher deck."* **Measured on tape 15, and
there is NO air-law asymmetry.**

- `sim::atm_frac_at` is **one shared function** for the player and the AI.
- The highest drone in the tape (2,158 m) had **`af` 0.000 and was falling at
  22 m/s** — fully subject to the no-air.
- In the window after his dome died: enemies median **1,183 m**, allies 469 m,
  **and he himself reached 2,833 m — higher than any enemy.** The air never
  refused him.
- The real cause: **his dome collapsed to 0 while theirs GREW to 1.5×**, and
  **air is faction-agnostic** — so the only high air on the planet was inside
  their bubble, and everything in it (allies included) looked like it was above
  him. Plus **the perch**, which deliberately takes 1,500–2,000 m deep inside.

⚠ The deathmatch ruling deletes this situation: after the last pump there are no
domes to grow or perch in. **If he reports it again, the thing that would
contradict this is enemies holding high station WITH AIR over HIS ground** —
get the rough match time and go back to the tape.

---

## 3. ★★★ THE CRLF TRAP — SOLVED, AND IT WAS NEVER OUR CODE

It bit **three separate trees for about an hour each** (snow, the nightly
worktree, and this one).

**`.gitattributes` pins the repo to LF — but merging the pin does NOT rewrite
files the merge did not touch.** `render/planet.cpp` kept its CRLF bytes, and
`test_snow_shadows.cpp` reads that file and matches a pattern spanning a line
break ⇒ **row-9 reports a FALSE red that belongs to the WORKING TREE, not to
main and not to any lane's code.**

**Here: 863 of 1007 tracked files were CRLF.**

★★★ **THE FIX, from a COMMITTED tree** (it overwrites the working tree —
commit first):

```
git rm --cached -r . -q  &&  git reset --hard
```

863 → 0 text files. The ~80 residuals are **binaries** (jpg/png/blend/glb)
whose bytes merely contain `\r\n`. Row 9 then flips **green with zero code
change** — all seven of its cases.

`gate.sh` now tripwires this in one second and prints the repair command. **Pull
main before gating.**

⚠ **I had the polarity backwards for a while:** when `sed -i` stripped
`conquest.h`'s CRLF I "restored" it from a backup — i.e. I repaired the file
back *into* the trap. Under the pin, LF is correct.

⚠⚠ **`sed -i` silently strips CRLF from a whole file.** If you must use it on a
CRLF file, expect a whole-file line-ending diff; check `git diff --numstat`
(a content-only edit should show insertions with few/no deletions).

---

## 4. ★★★ LAWS PAID FOR THIS SESSION

- **A one-shot with an unreachable exit IS a sustained floor wearing a latch.**
  Check that the gate quantity can actually be reached.
- **A stateless mirror-arm cannot see a stateful defect.** `ENV3-A2` was green
  for the entire time the air war was dead.
- ⚠⚠ **AN EXIT CODE BELONGING TO THE WRONG PROCESS READS EXACTLY LIKE SUCCESS.**
  `bash gate.sh | head -5` SIGPIPE-killed the gate after five lines and *the
  pipeline still exited 0* — I reported a gate that never ran. Same family as
  the stale-binary trap. **Redirect to a file; never pipe a gate through
  `head`.**
- **When a ruled feature measures harmful, check the BUILD against his WORDS
  before reporting the RULING as wrong.** His parabola was right; my build was
  the sustained floor.
- **RUN the mutation, never write one down.** Every required-red in this
  session's arms was executed and is recorded as verified.
- **`git rm --cached` + `reset --hard` is only non-destructive on a COMMITTED
  tree.** Check `git status` first — destroying uncommitted work with a hard
  reset is a failure this ladder has already paid for.

---

## 5. DEBTS AND OPEN QUESTIONS

**Ours, in the gate baseline (ruled debts, do not bend):**
- `E12.1: the raider backfill keeps a faction's pump offense alive`
- `probe P-F: the relentless raider keeps the pump and shoots back`

**Not ours:**
- The four `sled_*` reds are snow's (in snow-owned test files).
- `ballistic truth harness` (test #602) — **red only in the nightly's FRESH
  checkout; it RAN and PASSED here in a lived-in tree, 1.28 s.** Filed as an
  OBSERVATION in `LANES.toml` with the repro
  (`ctest --test-dir build -R "ballistic truth harness"`). **Snow's owner is
  chasing it. Do not diagnose it for them.**
- `PACKET_TO_ENEMY_AI_from_snow.md` is a **RETRACTION**: the merge is inert and
  `roll_tq[]` is **exonerated**. **Do not spend a rung bisecting snow's kernel
  edit.**

**Unruled / unbuilt:**
- The **S4 collapse** red's status and **his DIVE** were never separately ruled;
  a "very good" is not a ruling on either.
- **Nothing built is evidence a pump gets defended.** The closed-loop arm
  **T-1** ("first defender within 900 m in under 50 s, from 9 km, with foes
  present") is still not built.
- `seads-recon` (the r4a lane's worktree, `D:/flight_sim2/seads-recon`) —
  ⚠ **it is CHAD'S FLY TREE; a peer session ruled that no agent works in it.**
  As of 2026-08-30 it held a merge of our landing staged but uncommitted. Not
  ours to finish. **`ListAgents` / `LANES.toml` before ANY worktree operation.**

---

## 6. INSTRUMENTS

- Tapes: `build-play/conquest_tape_N.jsonl`; analyzer `tools/ai_tape.py`.
  **Tape 15 is the deathmatch evidence** (his lost match). `cq` rows carry
  `rs` (dome scale — how a collapse is read), `cd_f`/`cd_s` (the clock),
  `score`, `out`, and now **`dm`** (the deathmatch latch). `d` rows carry `ed`
  (signed depth to the nearest live dome edge) and `af` (air fraction).
- Planet radius `R = 15000`; altitude-above-sphere is **not** AGL — the deck is
  terrain-relative, so a drone at 1,183 m over high ground can be ≤120 m AGL
  with full air. **Use `af`, not raw altitude, to judge whether it is in air.**
- Graph: `python tools/graph/graphify.py` then `graph_query.py check` —
  **regenerate in the SAME commit** as any structural change.
- ⚠ A Catch2 test name containing a **comma** cannot be selected by the exe's
  own filter — it silently runs NOTHING and reads exactly like green. Reach it
  with `ctest -R`.

---

## 7. AWAITING HIM

1. **THE FLY.** Both rungs are unflown. §2.1 and §2.2 say what to watch.
2. Ruling still open: **the S4 red** and **the DIVE**.

---

## 8. NEXT, RANKED

1. **His fly of the deathmatch + the parabola**, and whatever he rules from it.
   Nothing below outranks a report from him.
2. **T-3 — the arm that can REFUTE the perch.** The perch shipped on pure-law
   arms only; **nothing yet shows a perched drone fights better.** T-3 is
   written so it CAN refute the rung. If the numbers do not move, the perch gets
   reported as spectacle.
3. **R-CLOSE — the biggest lever in the program.** AI-vs-AI combat has still
   never dealt a point of damage in the field: allies ended 14 min at 100.0 hp,
   enemy-on-foe range median **4,137 m**, and the 60–900 m gun band was open on
   **0.3%** of ticks. Foes ARE assigned and the DPS seam IS armed — **what fails
   is CLOSURE.**
4. **T-1**, the closed-loop defence arm (§5).
5. The closed-loop replays (i=0 / i=9), then C2.
