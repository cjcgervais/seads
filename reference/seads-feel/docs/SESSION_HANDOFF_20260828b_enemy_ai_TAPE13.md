# ENEMY-AI — TAPE 13 FLOWN. THE ENGAGED DECK HOLE. HANDOFF 2026-08-28

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260828b_enemy_ai_TAPE13.md; take the
next enemy-AI rung."**

**READ FIRST, IN THIS ORDER:**
1. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon, IN FULL. **An agent that does not know the loop will
   "fix" the AI into something that cannot play the game. Standing rule. Every
   consult you spawn gets §1–2 pasted in.**
2. `docs/SESSION_HANDOFF_20260829_enemy_ai_BALLISTIC.md` — the rung that just
   shipped, its gate, and **its §2 re-measured headline table (quote that, not
   the 08-28 one)**.
3. `docs/SESSION_HANDOFF_20260828_enemy_ai_DECK.md` — the deck rung this one is
   about; §4 is still the queue (C2 the bore, the last metre, the sled).
4. `CLAUDE.md` standing laws (top of file).

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`, HEAD
`0d7030325`, **nothing pushed**, tree clean. Gate **1572/1578, six reds**
(P-F clause 2 red on purpose; E12.1 newly red and attributed; four GI4 sled
debts). Fly build `build-play/seads.exe`
(`pre-reconcile-20260821-104-ge2b5ac92c`).

★ **NEW EVIDENCE: `build-play/conquest_tape_13.jsonl`** (13,085,974 B,
2026-08-28 18:01) — Chad's fly of the ballistic rung. Analyzer `tools/ai_tape.py`.
Screenshot one minute later:
`D:\flight_sim2\Game_loop_idea\Game_Screenshots\game_end_ 08_28.png`.
**Neither has been analysed. That is job zero.**

---

# ★★★ WHAT CHAD SAW, VERBATIM. THIS IS THE SPEC.

> "I just flew it, I watched enemy ai leave their own zone when I was going to
> attack their pump and then crash, they are not going to the deck... There is a
> photo in reference pics in game loop idea called game_end_08_28 ... It shows
> all the respawns staying in a zone that has now air, they have no pumps left
> and they are fighting one of my allies in their zone maybe one is going to
> attack that last remaining pump of mine..."

Three observations, and **two of them are the same defect**:
1. Enemy aeroplanes **leave their own zone** when he moves on their pump, **and
   crash**.
2. **"They are not going to the deck."**
3. At end state, respawns **sit in a zone that now has no air**, no pumps left,
   **fighting one of his allies inside that dead zone**.

**The screenshot** (read it yourself; do not take this summary as evidence):
OURS 240 / THEIRS 100, PLANES 8, PUMPS 1/4. The blue ALLIED AIR ellipse is drawn
in the north. **The ENEMY AIR label in the south has NO ellipse around it — the
enemy dome is gone.** Roughly seven orange markers are clustered in the
south-centre around the two orange pump icons, with **one blue ally marker in
among them**. Two blue markers sit inside the allied bubble.

---

# 1. ★★★ THE MECHANISM. CONFIRMED IN THE CODE, NOT INFERRED.

**`drone/drone.h:3335-3339`:**

```cpp
const bool deck_errand =
    !ballistic_owns_gamma &&
    (deck_errand_order || maverick_committed || !d.engaged);
if (deck_on && lk.ok && deck_errand)
    target_gamma = deck_track_gamma(lk, dp);
```

**AN ENGAGED AEROPLANE NEVER GETS THE DECK TRACK LAW.** The comment three lines
above says so outright: *"an ENGAGED tick is never overridden — Chad's 'chase you
anywhere' means BFM owns the nose."*

★ **THE CRASH NET STILL FIRES; THE ALTITUDE DISCIPLINE DOES NOT.** The forward
eyes, the 60/110 deck band and the `avoid_gamma` pull-up sit BELOW this block and
are **not** gated on `deck_errand` — verify at `drone/drone.h:3340-3400`. So an
engaged drone outside a dome gets the panic climb **without** the law that would
have held it at 100 m AGL. **That is the pre-S1-DECK defect, still live, for
exactly the population that is fighting.**

## Why this explains all three of Chad's observations

* **"They are not going to the deck."** Correct, and by construction: they are
  ENGAGED, so `deck_track_gamma` is skipped and BFM owns the nose.
* **They leave their zone when he attacks the pump, then crash.** HYPOTHESIS,
  and the strongest one: he attacks → they engage him → pursuit is deliberately
  unleashed ("chase you anywhere") → they follow him past the dome edge → now
  outside, engaged, with no deck law → climb/mush in vacuum → crash. This is the
  measured 1,700–1,900 m exit + 124 m/s mush shape from the DECK handoff, which
  S1 fixed **only for errand ticks.**
* **The dead-zone cluster fighting an ally.** A collapsed faction respawns in its
  own zone at 100 m AGL (S4, `place_on_faction_deck`) — correct and shipped — but
  the moment it engages the ally that is in there with it, **the deck law drops
  off and nothing holds it in the breathable band.**

★★★ **THIS IS THE SAME COLLISION THAT MADE STAGE 0 FIRE ZERO TIMES.** The
BALLISTIC handoff §2⚠C records the regroup trigger matching 1,123 ticks of which
**all 1,123 were ENGAGED** and therefore shadowed. **`d.engaged` is now shadowing
two separate rungs' worth of work.** Treat that as the finding, not as two
coincidences.

---

# 2. THE RULING THIS NEEDS — AND WHY IT IS NOT A PURSUIT NERF

Chad ruled **"chase you anywhere"** (2026-08-26, explicit answer to a direct
question) and ALSO ruled **"they need to fly low on the deck and not crash in the
low air zones"** and **"survive the deck."** Tape 13 is where those two meet.

**The distinction to put to him, and DO NOT pick it yourself:**
chasing anywhere is about **direction and target** — go wherever he goes, never
break off. Holding the deck band is about **altitude**. Outside a dome, above
~320 m AGL there is no flight at all, only ballistics — so a pursuit law that
commands an altitude it cannot hold is not pursuing, it is dying. Making pursuit
respect where flight exists is arguably not a clamp on pursuit.

★ **BUT IT DOES CHANGE BEHAVIOUR HE SIGNED, so it is his call.** Options to put
to him with numbers measured, not adjectives:
 * **(a)** engaged ticks get the deck track law outside a dome (fixes the crash;
   a bandit can no longer follow him vertically out of the deck band);
 * **(b)** engaged ticks get only a floor/ceiling clamp, not the full track law
   (keeps more BFM freedom, less altitude discipline);
 * **(c)** leave it (they keep dying, and "not going to the deck" stays true).

⚠ **Whatever he rules, ship it ON and MEASURE it.** Standing instruction: *"stop
nerfing them in secret."* A capability measured as harmful ships OFF **loudly**,
as a named key with its table beside it.

---

# 3. THE WORK, IN ORDER

## 0. ANALYSE TAPE 13 FIRST. Everything below is a hypothesis until you do.
`build-play/conquest_tape_13.jsonl`, analyzer `tools/ai_tape.py`. ★ This is the
**first tape flown with the B2 command witness** (`ta` terrain-avoid latch, `gc`
commanded gamma, `bc` commanded bank, `af` air fraction, `ds` deck scope, `vt`
router verdict, `cr` rounds-in-air) — the instruments built two rungs ago exist
precisely so this question stops being inferred. Measure:
 * every enemy death: mode, `d.engaged`, AGL, `af`, `ds`, `ta`, `gc` at death;
 * how many died ENGAGED and outside a dome (the predicted dominant bucket);
 * how many died in the bore (the standing 62.5% grinder — do not confuse them);
 * the dome-exit population: who left, under what order, engaged or not;
 * post-collapse: what the dead-dome faction did, at what AGL, engaged fraction.
★ **Compare against the S4 measurement that said a collapsed faction flies "100%
in deck scope, median 116 m, ENGAGED 61%."** That was measured on a probe arm
where BOTH domes were already dead. Tape 13 is the real case and may disagree —
if it does, the probe arm was structurally blind and must be said so.

## 1. THE ENGAGED DECK HOLE. The rung, once Chad rules §2.
`drone/drone.h:3335-3339`. One predicate. ⚠ The probe MUST carry a **foes-present
arm** — this defect lives only where there is a fight, and every existing deck arm
grades errand flying. A bit-identical arm here means blind fixture, not success.

## 2. C2 — THE DIVERT STANDOFF LAW. **Still the largest open debt.**
6 of 8 wrecks at the last HEAD carried a tunnel disposition. Unchanged from the
DECK handoff §4.0: build `aim_station()` beside `maverick::aim_at`, switch only
the three objective call sites, leave the four BFM callers alone.

## 3. E12.1's RED — attributed, not mechanised. See BALLISTIC §4.0.
`raider_backfill` churns `raider_rank`, which churns WHO the designated ballistic
runner is, mid-sequence. **MUT-3 (the router's ballistic guard) is bit-identical
and therefore UNVERIFIED.** Instrument `bal_lost` under a forced rank-churn
fixture before touching any threshold. **Do not bend E12.1.**

## 4. SWEEP `raid_ballistic_climb_agl_m`. Nobody has.
2,000 m ships; 4,000 m costs 0.0268 of pressure and 2 wrecks. The shipped value
was chosen **before** the L arm was measured.

## 5. Carried forward: the last metre on "attack me"
(`bfm_intercept_chase_unfaded` OFF — rounds at player 54→23, hits 16→2 when ON;
**do NOT re-attempt a speed fix**, instrument the aim chain via B2's `gc`/`bc`);
the in-dome chase gun-mute; the sled combat anchor (parked-plane targeting).

---

# 4. THE RULINGS THAT ARE CHAD'S, NOT YOURS

1. ★ **NEW — the engaged deck hole.** §2 above, options (a)/(b)/(c).
2. **Re-arm the ballistic dive, or leave it one-shot?** He sees it once per
   22 minutes and said he wanted it "in general".
3. **Stage 0's collision with unlimited pursuit** — leave it literal (fires
   never), drop the pump-attack condition (−35% offense, refused as an
   unrequested walk-back), or let a regroup preempt an ENGAGED tick.
4. ★ **The lower attack floor over his pump.** p10 345 m → 133 m (BALLISTIC §2⚠D).
   Real, 6× the noise floor, and it breaks S1-DECK's own signed proof that the
   in-bubble band was untouched. **He has now flown it but has not commented.**
5. **Reinforcement waves for a dead-dome faction.** A collapsed faction can now
   breathe on its own deck; the veto is also what blocks victory-by-wipe.
6. **E12.1's red** — pin the tape-7 arm's heightfield, or re-baseline?
7. **The bore grinder** — fix the divert, or let raiders skip the deep pump.
8. **"Fair game" for the snowmobiler** + sledder respawn semantics.
9. **The raid attack pattern** and **E18** — both still OFF, both still open.

---

# 5. THE LAWS — carried forward, and what tape 13 adds

**ADDED:**
* ★★★ **A SCOPE PREDICATE IS A SILENT SHADOW.** `d.engaged` now shadows two
  rungs: the regroup trigger (1,123 of 1,123 ticks) and the deck track law (every
  fighting aeroplane outside a dome). **When you scope a law, enumerate the
  population it EXCLUDES and measure that population's outcomes** — an excluded
  population is invisible to every arm that grades the included one.
* ★★★ **A PROBE ARM CAN BE STRUCTURALLY BLIND TO THE REAL CASE.** S4's
  "collapsed faction flies 100% in deck scope" was measured where both domes were
  already dead. Chad's tape has one dome alive and an ally inside the dead zone.
  **Grade the case the player actually flies.**
* ★★ **TWO OF THE PLAYER'S OWN RULINGS CAN COLLIDE, AND THE COLLISION IS A
  FINDING, NOT A BUG.** Surface it as ONE question with numbers; never silently
  pick a side.

**CARRIED FORWARD (see BALLISTIC §6 for the full list, all still paying):**
re-measure every headline at the final head with the noise arm · a Catch2 name
with a comma selects nothing · build the instrument before the feature · a
location predicate is not an altitude predicate · the noise floor is
per-statistic and moves when the feature lands · a constant or hand-maintained
list that describes the shipped table stops describing it the moment the table
moves (READ THE LOADER; ⚠ exception: RaidOrder/DefendOrder gains have no TOML
key) · a fixture-wide change moves every arm that shares the fixture · a
bit-identical arm means blind fixture OR dead branch · a moved hash proves the
order exists, not that an aeroplane turned · when you add an exit to a state
machine it inherits every deferral the old exits carry · guard every
`normalize()` · a hand-mirror is not a caller · ⚠ `[.deckx]`'s deck-AGL p10 is
contaminated, nobody may quote it.

---

# 6. HOUSEKEEPING

* **Do not push.** Main advances on Chad's stick. Commit freely on
  `sandbox/enemy-ai`.
* **Do not touch `assets/`.** Do not work on main. Never touch another
  `D:\seads_sandboxes\*` or `D:\flight_sim2\seads*` worktree — one object store,
  other branches checked out.
* **DO NOT TOUCH** `drone/bfm.h`, `maverick::aim_at` itself, the four BFM call
  sites, `sim/`, `control/`.
* **THE GRAPH, NOT GREP**, regenerated in the SAME commit as any structural
  change, then `check`. ⚠ **The committed graph is already stale on LOC** (
  `app/instructor_tick.h` 2790 vs 2825; `test_conquest_match.cpp` 3816 vs 3830),
  attributed to `28581e90c` running graphify mid-work. Zero symbol/edge drift.
  **Regenerate it in your first structural commit.**
* Relink the fly build before finishing and **quote the stamp you actually
  built**. ⚠ Never relink while a sweep runs; a concurrent `ctest` from another
  worktree will pollute your gate output.
* The census scripts (`gun_census.py` / `gun_forensics.py`) are **still only in a
  session scratchpad, not committed.** Fourth handoff carrying this.
