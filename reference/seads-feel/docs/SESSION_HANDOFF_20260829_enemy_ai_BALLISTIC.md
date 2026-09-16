# ENEMY-AI — THE BALLISTIC DECK RUN, GATED AND RE-MEASURED. HANDOFF 2026-08-29

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260829_enemy_ai_BALLISTIC.md; take
the next enemy-AI rung."**

**READ FIRST, IN THIS ORDER:**
1. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon, IN FULL. **An agent that does not know the loop will
   "fix" the AI into something that cannot play the game. Standing rule. Every
   consult you spawn gets §1–2 pasted in.**
2. `docs/SESSION_HANDOFF_20260828_enemy_ai_BALLISTIC.md` — the D3 build itself
   (phases, dials, red-team corrections, mutations). ⚠ **Four of its numbers do
   not survive re-measurement at HEAD — §2 below. Quote §2, not it.**
3. `docs/SESSION_HANDOFF_20260828_enemy_ai_DECK.md` — the rung before that; its
   §4 work list is still the queue (C2 the bore, the last metre, the sled).
4. `CLAUDE.md` standing laws (top of file).

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`, HEAD
`e2b5ac92c`, **nothing pushed**, tree clean. Fly build relinked at
`build-play/seads.exe` (`pre-reconcile-20260821-104-ge2b5ac92c`, 88,717,986 B).
⚠ The 2026-08-28 handoff §7 quotes `…-103-g68936237e` — **stale by one commit.**

---

# ★★★ NERF WATCH — FIRST, BECAUSE IT IS THE STANDING INSTRUCTION

**No capability was reduced, and it was checked against the loader and the
diff, not against comments.**

* `git diff b171f338d..HEAD -- drone/bfm.h sim/ control/` = **0 lines.**
* `config/game.toml` **untouched** since baseline: `raid_dps_frac = 0.65`,
  `extend_leash = true`.
* Every `config/scenario.toml` change since baseline is an **addition** (15 new
  `regroup_*` / `raid_ballistic_*` keys). **Not one existing dial was reduced.**
* Offense **rose**: pressure 0.0392 → 0.0448 (+14.3%), crashes 0.45 → 0.36/min,
  deck survival 0.526 → 0.700.
* D2's parked −6.4% (stage 1 forced OFF) is **gone, by shadowing, not payment** —
  §3c of the 08-28 handoff. `[.regroup]` arm F is bit-identical to shipped.

**The one visible capability cost, with its number:** `[.bdeck]` arm L
(`raid_ballistic_climb_agl_m` ×2 = 4,000 m) reads pressure **0.0180** and
crashes **0.45/min**. The dial is steeply non-flat and **nobody has swept it**.

---

# 1. THE STATE OF THE BRANCH

```
e2b5ac92c  D3-GATE: 1572/1578, six reds -- and the SIXTH CHANGED IDENTITY, attributed
68936237e  D3-INSTRUMENT: the dive graded in METRES and DEGREES, not in a hash
28581e90c  D3: THE PARABOLIC DIVE -- Chad's sequence, flown, and it made them HARDER
57cb45936  D2: THE REPOSITION -- stage 0 shipped as ruled, stage 1 parked with its price
77085cc32  D3-INSTRUMENT: [.bdeck] -- the ballistic deck run's differential, BEFORE the feature
b171f338d  S4: DELETE THE TELEPORT -- respawn in your own zone, at the deck
```

## THE GATE — 1572/1578, SIX REDS, NO SEVENTH, **AND THE SIXTH CHANGED IDENTITY**

`ctest --test-dir build -C Debug --output-on-failure --timeout 900`,
**2991.84 s**, `CTEST_EXIT=8`. The "orbit inertia" 80-min hang did **not** recur
and is still uninvestigated.

| # | test | verdict |
|---|---|---|
| 58 | `probe P-F: the relentless raider keeps the pump and shoots back` | BASELINE, red on purpose. `test/unit/test_enemy_ai_e6.cpp:798`, `5397.31 < 2509.21`. **Do not bend P-F a fourth time.** |
| **98** | **`E12.1: the raider backfill keeps a faction's pump offense alive`** | **THE SIXTH. NEWLY RED, ATTRIBUTED.** `test/unit/test_conquest_match.cpp:2141`, `0.03335031 >= 0.03853253` fails. The 1.15 raid-duty clause above it PASSES (+1001 s, 1164 → 2164). |
| ~~99~~ | `E12: the enemy's pump offense does not regress below tape 7` | ★ **GREEN AT HEAD.** The documented DEM-re-point red now passes. 1.15 untouched. |
| 829 / 830 / 868 / 871 | `sled_slides_before_it_tips_on_flat_snow` (`test_sled.cpp:1103`) / `sled_grip_ceiling_stays_below_the_tip_threshold` (`:1312`) / `sled_assist_reference_plane_is_load_weighted` (`:3123`) / `sled_debug_sink_is_write_only` (`:3456`) | BASELINE — the four GI4 sled debts, by name. |

**The E12/E12.1 swap is D3's, verified independently:** `[.bdeck]` arm O
(`raid_ballistic_climb_agl_m = 0`) reproduces the pre-D3 enemy hash
`12dd30472363fcf5` bit-for-bit; arm C reproduces the shipped pin
`2ec5baddc8346cfe`. Pre-D3, E12 fails and E12.1 passes 7/7. **Nothing bent.**

## THE GRAPH IS STALE AT HEAD — LOC ONLY, ATTRIBUTED, NOT FIXED

`python tools/graph/graph_query.py check` → **`layer check OK`**. But
regenerating dirties 3 tracked files:

| file | graph says | actual at HEAD |
|---|---|---|
| `app/instructor_tick.h` | 2790 loc | **2825** |
| `test/unit/test_conquest_match.cpp` | 3816 loc | **3830** |

Culprit is **`28581e90c`**: graphify was run mid-work, so it committed a graph
built from a *later* test-file state (3816 vs its own 3449) and an *earlier*
`instructor_tick.h` (2790 vs its own 2825). `68936237e` then edited the test
file again with no regen. `57cb45936` matched exactly. **Zero symbol or edge
drift** — that is why `check` is green. **Regenerate it in the next structural
commit.**

---

# 2. ★★★ FOUR HEADLINES THAT DO NOT SURVIVE RE-MEASUREMENT AT HEAD

Every headline was re-run at the final head by a second pass (`[.bdeck]`,
`[.regroup]`, full arm sets, arm C = shipped, O = pre-D3 dead switch, N = noise
arm = shipped + 1e-9 m/s). **Eight of twelve reproduce exactly.** These four do
not, and the next agent must quote this table, not the 08-28 one.

### ⚠ A — THE IN-PHASE GUN-MUTE "REGRESSION" IS AT THE NOISE FLOOR
08-28 reports in-phase mute **3.9%** against a 2.9% fleet baseline as a stated
regression. **The NOISE arm's own fleet deck mute is also 3.9%** — a 1e-9 m/s
cruise perturbation moves that statistic by the same 1.0 pp as the claimed
effect. **Report it as unmeasured, not as a 1.0 pp regression.**

### ⚠ B — "DEAD-AIR MEDIAN 0.0 s AT HEAD" IS THE **PRE-D3** ARM
08-28 §1 states "at HEAD the median crossing already spent 0.0 s in dead air."
**0.0 s is arm O.** At HEAD: mean **6.9 s**, median **4.9 s** (n=20). Pre-D3:
mean 7.2, median 0.0. The mean fell 0.3 s (0.5× the probe's own 0.62 s floor);
the median rose 4.9 s (7.9×, still under the probe's own 10× bar). **Honest
statement: the ballistic run did not measurably shorten dead-air exposure, and
the median moved the wrong way.**

### ⚠ C — BOUNDARY LOITER WENT **UP**, AND STAGE 0 FIRES ZERO AT HEAD
Outer-third occupancy **11.50% → 14.76% (+3.26 pp)** against a measured
`|S−N|` floor of **0.19 pp** — **17× the floor, a real result, in the wrong
direction** for the ask that opened D2. And Chad's literal two-condition trigger
is not merely unarmed at HEAD, it has **no exposure at all**: the arming
conjunction (band AND pump-attack window) counts **0 ticks / 0.00%**, where the
pre-D2 machine had 1,123 ticks (all shadowed by ENGAGED). Episodes armed **0**,
stage-1 returns **0**. The DWELL-alone fallback (`regroup_require_pump_attack =
false`) arms 15 episodes but costs pressure 0.0448 → **0.0153** — correctly not
shipped. See `config/scenario.toml:1406-1443` for the whole measured table.

### ⚠ D — THE SIGNED RAID-AGL FLOOR OVER THE PLAYER'S PUMP MOVED
S1-DECK's signed row: **p10 345 / MED 397 / p90 447, n=14,254** — reproduced
exactly by arm O. At HEAD (arm C): **133 / 379 / 447, n=16,768**. Noise arm N:
133 / 376 / 449, so `|C−N|` on the median is **3 m** — the −18 m median is 6×
the floor and the **−212 m p10 collapse is not noise at all**. Cause is clean: C
and O differ only by `raid_ballistic_climb_agl_m`; the extra 2,514 samples and
the 133 m p10 are the ballistic runner's deck sprint and its 10.3 km SPENT bleed
arriving **inside the dome at deck altitude**. ★ **This row was S1-DECK's own
proof that "the in-bubble 250/400 pair is untouched." That proof no longer holds
at HEAD.** It is a behaviour change, not a nerf (they arrive lower and faster),
but no handoff before this one reported it.

## The eight that DO reproduce exactly

survival 14/20 = 0.700 (O: 10/19 = 0.526) · crashes 8 = 0.36/min (O: 10 = 0.45)
· pressure 0.0448 (O: 0.0392) · deck gun-mute 2.9% (O: 3.5%) · fleet arrival
320 m MED 160 m/s, 120 m MED 143 · in-phase arrival 238 m/s at −12.2° (**n=1**)
· RUN→SPENT handover 243 m/s (**n=1**) · climb apex MED 2,018 m, arm 1 → commit
1 (timeout 0) → run 1 → SPENT 1, lost 0.

Hash liveness: C = `2ec5baddc8346cfe` MATCH · O = `12dd30472363fcf5` MATCH · N
and L both differ from C · arm F (return forced OFF) **bit-identical to S**.

---

# 3. WRECK STRATIFICATION AT HEAD — THE BORE IS STILL THE KILLER

Arm C, n=8 enemy wrecks: **all 8 below 60 m AGL, 0 in thin air, 6 of 8 carrying
a TUNNEL disposition** (4 of them underground in the bore). **C2, the divert
standoff law, is the dominant remaining cause of enemy death**, and the
ballistic run neither helped nor hurt it. The vacuum-death population this
manoeuvre was suggested to fix **does not exist at HEAD** — it was the
pre-S1-DECK machine. It shipped as spectacle + arrival energy, and it paid for
itself in offense.

---

# 4. THE WORK, IN ORDER

## 0. E12.1's RED — **THE FIRST JOB.** Attributed, not mechanised.
`raider_backfill` churns `raider_rank`; `deck_run = rank < raid_deck_run_slots`;
so **backfill churns WHO the designated ballistic runner is**, and the sequence
is one-shot per raid-order lifetime tied to that slot. That is exactly the yank
the router's ballistic guard exists for — and **MUT-3 says the guard is inert**,
at the SHIPPED `raid_dps_frac`. E12.1 runs at `raid_dps_frac = 0.5`, a different
operating point, and there the churn bites. **Instrument `bal_lost` under a
forced rank-churn fixture and prove the guard fixes it or does not, BEFORE
touching either threshold.** Do not bend E12.1.

## 1. C2 — THE DIVERT STANDOFF LAW. **STILL THE LARGEST OPEN DEBT.**
6 of 8 wrecks at HEAD carry a tunnel disposition. Unchanged from the 08-28 DECK
handoff §4.0 — build `aim_station()` beside `maverick::aim_at`, switch only the
three objective call sites, leave the four BFM callers alone, and the probe MUST
carry a **foes-present arm**.

## 2. SWEEP `raid_ballistic_climb_agl_m`. Nobody has.
2,000 m ships; 4,000 m costs 0.0268 of pressure and 2 wrecks. The dial is not
flat and the shipped value was chosen **before** the L arm was measured.

## 3. THE RE-ARM QUESTION (Chad's §5.1) — build whichever he rules.
(b) needs hysteresis so SPENT→CLIMB cannot chatter; (c) is his own
"a few designated runs" dial and moving it changes the tunnel/deck mix too.

## 4. THE STAGE-0 COLLISION (Chad's §5.2). His ruling, three options, all in
`config/scenario.toml:1435-1442`. Do not pick one.

## 5. Carried forward unchanged: the last metre on "attack me"
(`bfm_intercept_chase_unfaded` OFF, rounds 54→23 / hits 16→2 when ON — do NOT
re-attempt a speed fix, instrument the aim chain via B2's `gc`/`bc`); the
post-kill bore return with no mechanism; the in-dome chase gun-mute; the sled
combat anchor (parked-plane targeting).

---

# 5. THE RULINGS THAT ARE CHAD'S, NOT YOURS

1. **Re-arm or one-shot?** (a) one-shot as shipped — he sees it once per 22 min;
   (b) re-arm from SPENT when the runner is home with the enemy pump alive;
   (c) raise `raid_deck_run_slots`. **Nothing was quietly picked.**
2. **Stage 0's collision with unlimited pursuit.** (a) leave it literal (ships),
   (b) drop the pump-attack condition (−35% offense, refused as an unrequested
   walk-back), (c) let a regroup preempt an ENGAGED tick — which contradicts
   "chase you anywhere".
3. **Reinforcement waves for a dead-dome faction.** Still open. A collapsed
   faction can now breathe on its own deck; the veto is also what blocks
   victory-by-wipe.
4. **`bfm_intercept_chase_unfaded`** a/b — unchanged.
5. **E12's red** (now E12.1's) — pin the tape-7 arm's heightfield or
   re-baseline? ★ (b) moves a signed threshold. Not silently.
6. **The bore grinder** — fix the divert, or let raiders skip the deep pump.
7. **"Fair game" for the snowmobiler** + sledder respawn semantics. Blocks §4.5.
8. **The raid attack pattern** (`raid_attack_alt_m` / `raid_reattack_m`) and
   **E18** (`strike_attack_alt_m`) — both still OFF, both still open.

---

# 6. THE LAWS — carried forward, and what this rung added

**ADDED THIS RUNG:**
* ★★★ **RE-MEASURE EVERY HEADLINE AT THE FINAL HEAD, WITH THE NOISE ARM, BEFORE
  YOU WRITE IT DOWN.** Four of twelve D3 headlines did not survive it (§2): one
  was quoted from the wrong arm, one sits on the noise floor, one moved in the
  wrong direction unreported, and one broke a *previous* rung's signed proof.
  Eight reproduced exactly — which is why the four matter.
* ★★★ **A CATCH2 NAME WITH A COMMA SELECTS NOTHING.** Two test names carried
  commas this rung — the named silent-filter trap, caught and renamed. A test
  that cannot be selected is a test that never runs.
* ★★★ **BUILD THE INSTRUMENT BEFORE THE FEATURE.** `77085cc32` is the `[.bdeck]`
  differential, committed *before* the dive existed. That is why the −5.4%
  first version and the 161-second in-dome porpoise were both caught by a probe
  instead of by Chad's stick.
* ★★★ **A LOCATION PREDICATE IS NOT AN ALTITUDE PREDICATE**, and a phase that
  begins inside the thing the predicate excludes can never leave it. `DIVE→RUN`
  gated on `deck_scope` held −18° inside its own dome for 161 s.
* ★★★ **THE NOISE FLOOR IS PER-STATISTIC AND IT MOVES WHEN THE FEATURE LANDS.**
  Survival 0.0000 and pressure 0.0001, but arrival |v| is now **9.90 m/s** and
  dead-air **0.62 s**. Deltas under ~99 m/s / ~6.2 s on those two are not
  results. Re-measure the floor at the head you are quoting.
* ★★ **A DERIVED BOUND BEATS A DIAL.** The 10.3 km bleed range is
  `mass/(0.5ρS·Cd0)·ln(v_sprint/v_errand)` off the live tables — that derivation
  is what turned −5.4% offense into +14.3%.
* ★★ **A GRAPH REGENERATED MID-WORK LANDS IN THE WRONG COMMIT.** `28581e90c`
  carries a graph built from two different tree states. Regenerate **last**,
  in the SAME commit, then `check`.

**CARRIED FORWARD, unchanged and still paying:**
* ★★★ **A CONSTANT (OR A HAND-MAINTAINED LIST) THAT DESCRIBES THE SHIPPED TABLE
  STOPS DESCRIBING IT THE MOMENT THE TABLE MOVES. READ THE LOADER, ALWAYS.**
  ⚠ Exception: RaidOrder/DefendOrder gains have NO TOML key.
* ★★★ **A FIXTURE-WIDE CHANGE MOVES EVERY ARM THAT SHARES THE FIXTURE.**
* ★★★ **A THRESHOLD CLAUSE GRADES A FLOOR, NOT A DELTA.** Paid twice more:
  MUT-1's first version passed green because an aeroplane aimed at a distant
  pump is already descending 13°; and P-A's clause is blind to a 57% loss.
* ★★★ **A MOVED HASH PROVES THE ORDER EXISTS, NOT THAT AN AEROPLANE TURNED.**
  D2's MUT-3 sailed through a hash-liveness leg; only a closed-loop metres leg
  collapsed under it.
* ★★★ **A BIT-IDENTICAL ARM MEANS BLIND FIXTURE *OR* DEAD BRANCH.** MUT-3 (the
  router guard) is bit-identical and therefore **UNVERIFIED, and not claimed.**
* ★★★ **THE MECHANISM STORY CAN BE RIGHT ABOUT THE DIAGNOSIS AND WRONG ABOUT
  THE PRIORITY.** Write the differential before believing it.
* ★★★ **A CAPABILITY MEASURED AS HARMFUL SHIPS OFF LOUDLY, AS A NAMED KEY WITH
  ITS TABLE BESIDE IT.** Refusing silently and shipping-because-asked are both
  failures.
* ★★★ **WHEN YOU ADD AN EXIT TO A STATE MACHINE, IT INHERITS EVERY DEFERRAL THE
  OLD EXITS CARRY.** D3 deliberately added none.
* ★★ **VERIFY FROM BOTH DIRECTIONS, AND SAY WHICH MUTATIONS DID NOT GO RED.**
* ★★ **DETERMINISM IS THE DIFFERENTIAL TOOL.**
* ★★ **A HAND-MIRROR IS NOT A CALLER** — `fly_match` mirrors
  `app/instructor_tick.h` by hand; drive the real `app::tick`.
* ★★ **GUARD EVERY `normalize()`.**
* ★★ **THE `pre_e2` TRIPWIRE EARNS ITS KEEP.** Welded 1480 → 1528 this rung, in
  the same commit, per its own instructions.
* ⚠ **`[.deckx]`'s deck-AGL p10 is contaminated. Nobody may quote it.**
* ⚠ **The raid branch takes the track law on hot-fight ticks**
  (`raid_fight_in_place = true`), so S1's "an ENGAGED tick is NEVER overridden"
  is true for the pursuit/BFM path only, not literally for `d.engaged`.

---

# 7. HOUSEKEEPING

* **Do not push.** Main advances on Chad's stick. Commit freely on
  `sandbox/enemy-ai`.
* **Do not touch `assets/`.** Do not work on main. Never touch another
  `D:\seads_sandboxes\*` or `D:\flight_sim2\seads*` worktree — one object store,
  other branches checked out.
* **THE GRAPH, NOT GREP.** `python tools/graph/graph_query.py …`, regenerated in
  the SAME commit as any structural change, then `check` (`layer check OK`).
* Relink the fly build (`cmake --build build-play --target seads`) before
  finishing, and **quote the stamp you actually built** — the last handoff's was
  stale by a commit.
* ⚠ **Never relink while a sweep is running**, and ⚠ **a concurrent `ctest` from
  another worktree WILL pollute your gate output file.**
* The census scripts (`gun_census.py` / `gun_forensics.py`) are **still only in
  a session scratchpad, not committed.** Third handoff carrying this.

---

# 8. THE FLY

Open **`D:\seads_sandboxes\enemy-ai\build-play\seads.exe`**
(`pre-reconcile-20260821-104-ge2b5ac92c`, 88,717,986 B).

Once per match, from the enemy side:
1. A SUDBURY raider **noses up inside its own dome** and climbs to ~2 km.
2. It **rolls onto the bearing of your surface pump** and pushes over into a
   sustained ~18° nose-down run at full throttle, about a minute of it.
3. It **levels into the deck lane at ~100 m** and crosses at ~243 m/s — roughly
   twice what anything crossed at before.
4. About 10 km out it **eases off** and settles onto the pump instead of blowing
   through it.

He will also, per ⚠D, see raiders working his own pump **lower** than before
(p10 345 m → 133 m). That is the runner arriving on the deck and bleeding
inside his dome. It is a change he has not flown yet.
