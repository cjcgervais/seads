# ENEMY-AI — THE DECK-COMPETENCE RUNG. HANDOFF 2026-08-28

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260828_enemy_ai_DECK.md; take the
next enemy-AI rung."**

**READ FIRST, IN THIS ORDER:**
1. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon, IN FULL. **An agent that does not know the loop will
   "fix" the AI into something that cannot play the game. Standing rule. Every
   consult you spawn gets §1–2 pasted in.**
2. `docs/SESSION_HANDOFF_20260827_enemy_ai_PHASE_B.md` — the rung just closed.
   Its Phase B is **DONE** (B1+B2). Its Phase C is **PARTLY DONE**: C1 landed
   inside S2-TUNNEL; **C2, C3, C4 are NOT DONE.**
3. `docs/AUDIT_FINDINGS_20260825_synthesis.md` — the evidence base. ⚠ **Two of
   its headlines are now REFUTED** — see LAWS and §2 below.
4. `CLAUDE.md` standing laws (top of file).

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`, HEAD
`35526fba3`, **nothing pushed**, tree clean. Fly build relinked at
`build-play/seads.exe` (`pre-reconcile-20260821-97-g35526fba3`, 88,638,382 B).

---

# ★★★ THE STATE OF THE BRANCH

Six commits, `ec9819a94..35526fba3`. This rung answered Chad's four asks —
survive the deck, survive the tunnel, attack the pump, attack me — and got
three of them.

```
35526fba3  S3-GUNS-C: the tape's "cr" -- one number for "are there enemy rounds in the air?"
d6e5e2159  S3-GUNS-B: the chase-ceiling un-fade -- BUILT, SCOPED, MEASURED, SHIPPED OFF
1aa27ed8f  S3-GUNS-A: the cosmetic round pool -- every point of AI damage gets a visible source
6926d5a36  S2-TUNNEL: the raid router + the unfreeze + C1's errand speed
513cb0730  S1-DECK: the deck band + the forward eyes + the track law, as ONE law
31c9d8dfc  B2: the flown-command witness on the tape's drone row (ta/gc/bc/af)
```

## THE GATE — 1558/1564, SIX REDS, NO SEVENTH

| # | test | verdict |
|---|---|---|
| 58 | `probe P-F: the relentless raider keeps the pump and shoots back` | BASELINE, red on purpose. **Do not bend P-F a fourth time.** 3 of 4 assertions pass; the red clause is `test/unit/test_enemy_ai_e6.cpp:798`. |
| 99 | `E12: the enemy's pump offense does not regress below tape 7` | BASELINE (the DEM re-point). **Now a ONE-clause red — it was two.** `test/unit/test_conquest_match.cpp:1580`, `0.84215… > 0.94239…`. The near-ratio clause now PASSES. Nothing bent, 1.15 untouched. |
| 825, 826, 864, 867 | sled | BASELINE — the four GI4 sled debts. |

Total grew 1546 → 1564 (+18 legs, all green). Full serial gate 2965 s ≈ 49 min.
The *"orbit inertia"* 80-min hang **did not recur** and is still uninvestigated.

★ **E12 moved FURTHER red for a legible reason and I bent nothing.** Enemy
raid-order duty fell 1420 s → 992 s because the tunnel router parks raiders in
the bore, and `enemy_raidduty_s` is the numerator. Over the same two arms
**pressure ROSE 0.0346 → 0.0392** and **crashes FELL 20 → 10**. E12 measures
raid-order *seconds*, not offense. Chad's a/b ruling on it is still open.

---

# 1. WHAT LANDED, WITH THE NUMBERS THAT ARE TRUE AT HEAD

★★★ **THE HEADLINE NUMBERS IN THE S1-DECK COMMIT MESSAGE DO NOT REPRODUCE AT
HEAD.** S1's table was measured before S2-TUNNEL landed; the router changed the
population of deck episodes underneath the same probe. **Quote the HEAD column
only.** (This is the fixture-wide law from the last handoff, paid for again.)

| headline | S1 commit says | **TRUE AT HEAD** | control | noise floor |
|---|---|---|---|---|
| deck crossings survived / attempted | 14/18 = 0.778 | **10/19 = 0.526** | 9/23 = 0.391 (band off) | \|D−N\| = 0.000 |
| enemy crashes/min (signed 0.94) | 0.22 | **0.45** (10 / 22.4 min) | 0.71 band-off | \|R−N\| = 0.0000 |
| gun-mute on the deck | 1.4% | **2.7%** (fight-only 3.7%) | 48.5% / 50.6% band-off | — |
| median deck AGL | 113 m | **115 m** (inside the 120 m full-air lid) | 362 m band-off | — |
| tunnel CLIMB_OUT completions | — | **2 / 8 entries = 25%** | 0 / 4 pre-router | — |
| in-net deaths per entry | — | **62.5%** (5/8) | 100% (4/4) pre-router | — |
| enemy pressure (on-station rate) | — | **0.0392** | 0.0336 band-off | 0.000000 |
| raid AGL over pump p10/MED/p90 | — | 345 / **397** / 447 (n=14254) | 352/413/459 | — |

0.778 is **unverifiable, not wrong**: `[.deckx]`'s control arm keeps the router
ON, so it cannot be re-derived from HEAD.

Cross-check that licenses all of it: **DECK arm D, TUNRT arm R and E12's P-H
SHIPPED arm are the same machine** — enemy hash `b3c6f1eb391a6c3d` on both
probes, 10 crashes / 0.45 per min on all three.

## THE THREE LAWS THAT LANDED

**S1-DECK (`513cb0730`) — the deck band + the forward eyes + the track law.**
The root defect: outside the domes air is full only below 120 m AGL and zero by
320 m (`config/game.toml:152,155`), while the terrain-avoid pull-up entered at
250 and released at 400. **There was no altitude on a bare deck that was both
breathable and unlatched.** New deck-scoped band `deck_avoid_agl_enter_m = 60.0`
/ `deck_avoid_agl_release_m = 110.0` (`config/scenario.toml:590,592`), selected
by a hysteretic probe of the air at the *shipped* 400 m release altitude over
the drone's own ground point (`drone/drone.h:2350-2373`, state bool
`drone/drone.h:1336`). Inside a dome that probe reads full air, so the in-bubble
250/400 pair is untouched — proven by the raid-AGL row above.
Forward eyes: `deck_lookahead_s = 12.0` (`:611`), `avoid_pull_net_g = 3.0`
(`:632`) — **measured, not guessed**: the real `V·(dγ/dt)/9.81` of latched
pull-ups over a whole match is p10 0.84 / p50 5.00 / p90 11.68 (n=76750), so 3.0
under-claims the airframe.
Track law: `deck_track_agl_m = 100.0` (`:607`), `bore_track`'s shape (max
terrain-ahead feed-forward + 0.0045 rad/m residual), on **errand** ticks only —
`drone/drone.h:2917-2920`, predicate `deck_errand_order || maverick_committed ||
!d.engaged`.

**S2-TUNNEL (`6926d5a36`) — the raid router + the unfreeze + C1.**
★ **The blocker was ONE LINE.** `hold_runs` froze the maverick run scheduler
whenever a RaidOrder was armed, so a designated raider could **never** enter the
bore: the raid and the tunnel machine were mutually exclusive by construction.
That is why Chad's 2026-07-26 standing mission had never once been flown by the
raid code. Now `d.raid.active && !ro.raid_now` (`drone/drone.h:2147`, the
`raid_holds` term). The raid launch gets its **own third countdown**,
`MaverickState::raid_countdown` (`drone/maverick.h:562`, launched `:991`, spent
`:1037`) — deliberately NOT E3.2's `order_countdown`, so it neither consumes
`strike_concurrent_max` nor is frozen by `order_hold`.
Dials: `raid_route_via_tunnel = true` (`config/scenario.toml:1324`),
`raid_deck_run_slots = 1` (`:1329` — Chad's "a few designated runs"),
`raid_route_gap_max_m = 500.0` (`:1332`), `raid_speed_target = 123.0` (`:1342`,
C1, one dial feeding both the raid site and the defend site).
Result: raid ticks routed via tunnel 0 → 149,993; ticks flown inside the net
6,997 → 25,065 (3.6×); net entries 4 → 8; CLIMB_OUT completions 0/4 → 2/8.

**S3-GUNS-A/C (`1aa27ed8f`, `35526fba3`) — the cosmetic round pool.**
`app/instructor_tick.h:63-66` *promised* "real tracers fly for the look" and it
was FALSE (`wants_fire` 0 of 70,220 drone samples across tapes 10–11). Now true
by construction: the ballistic spawn solve was factored **verbatim** out of
`enemy_fire_tick` into `combat::aim_slew_round`, so the real round and the
cosmetic round leave the muzzle on ONE aim law, pinned bitwise on all three
velocity components. `CombatWorld::cosmetic_pool` + `cosmetic_fire_tick` +
`cosmetic_fire`, emitting at both abstracted-damage seams; the pump seam queues
the same HitSpark the player's own rounds make. Rendered in the SAME comet pass,
tinted **per round off `Projectile::friendly`** — allied AI-vs-AI fire must not
read as incoming.
The cosmetic guarantee is proven from both sides in
`test/unit/test_cosmetic_rounds.cpp` (8 cases / 57 assertions): structural
braces (lethal rounds parked in the cosmetic pool move zero HP through three
sweeps, each with a positive control in the same fixture) **and** a data belt
(damage forced 0.0 twice). MUT-A…D and MUT-E/F all went RED and were restored.
Behaviour identity across the commit: E12's two 22-min arms are 17-digit
identical.
Tape: `p` rows carry `cr` = cumulative cosmetic rounds. It can NEVER explain
lost HP, only the look of it. Tapes 1–12 predate it: `.get("cr", 0)`.

**B2 (`31c9d8dfc`) — the flown-command witness.** Four additive keys on every
tape drone row: `ta` (the hard-deck latch), `gc`/`bc` (the FINAL commanded
gamma/bank in radians, written at the ONE final seam **after** the terrain-avoid
override, the arena clamp, the shell guard and the E1.4 bank slew), `af`
(`atm_frac_at` at that row's own position, sampled after the crash/respawn
branch so a teleported drone reports its own air). Cost +44 B/row worst case =
+17.5% max on tape 12's 10,056,686 B; longest post-B2 row 274 B against
`buf[600]`. The analyzer on the real pre-B2 tape 12 diffs by **exactly 10 added
lines** and not one other character.

---

# 2. WHAT DID NOT WORK, AND WHY

★★★ **S3-GUNS-B — THE FIX FOR "ATTACK ME" — WAS BUILT, MEASURED MAKING THINGS
WORSE, AND SHIPS OFF.** `bfm_intercept_chase_unfaded = false`
(`config/scenario.toml:935`, table at `:897`). It is a named TOML key, one word
from live, with the whole measured table beside it.

The diagnosis holds at HEAD: the ruled 265 m/s Intercept ceiling is faded by
`align²` before use → `85 + 180·align²`, which at the measured `align²` p50 is
**144 m/s** against Chad's median **262**. An enemy holding a player slot spends
86.7–89.8% of it beyond the 900 m fire range.

Probe P-A, 8-arm ensemble, 80 sim-minutes/arm, ONE key flipped, noise floor
measured first (±1e-9 on two dials leaves rounds/hits/max_fire_range
**bit-identical** → floor 0):

| | OFF (shipped) | ON |
|---|---|---|
| rounds at the player | 54 | **23 (−57%)** |
| HITS on the player | 16 | **2 (−87%)** |
| closest approach | 16.96 m | 110.58 m |
| enemy crashes | 1 | 7 |

**MECHANISM:** the faster command *does* arrive — it arrives with more energy
than the turn can spend, blows through the merge, and never converts closure
into a firing solution. **Closing was never the last gate; HOLDING THE NOSE once
there is.** That inverts the design's own priority ordering.

⚠ **AND THE GATE WOULD NOT HAVE CAUGHT IT.** P-A's clause is
`rounds_at_player >= max(10, 10·pre_rate) = 20`. **Both arms pass.** A 57% loss
is invisible to every green light in this repo.

REFUTED, and it is good news: the airframe is **not** the gate. P-A's prologue
measures the shipped drone settling at **261.51 m/s** under a 265 command with
`throttle_ff` (legacy 175.08). No airframe-thrust ruling is needed.

The un-fade lives in its **own `intercept_ceiling` lambda** (`drone/bfm.h:1041`,
called only at `:1094`); `chase_ceiling`'s body is untouched and Extend still
calls the faded one at `drone/bfm.h:1162`. MUT-H (putting the un-fade in the
shared lambda) reproduced the red team's prediction exactly — Extend's FLEE
command jumped 145 → 265 — and the new gate leg caught it RED.

---

# 3. ★★★ NERF WATCH — every capability not at full strength, with its number

Chad's standing instruction: *"stop nerfing them in secret."* Verified against
**the loader** (`config/load_scenario.cpp`), not comments.

**SHIPPED ON, full strength:** deck band 60/110; forward eyes 12.0 s / 3.0 g;
track law 100 m; router ON, 1 deck slot, 500 m gap; C1 errand speed 123.0.
**`raid_dps_frac` UNTOUCHED.** Pressure went **UP** (0.0336 → 0.0392 vs the
band-off control). Nothing was walked back.

**SHIPPED OFF — one, and it is a capability Chad asked for:**
* `bfm_intercept_chase_unfaded = false` (`config/scenario.toml:935`). Measured
  refusal, in the open, §2. **It remains an unfixed defect: the AI still cannot
  hold a firing solution on Chad.**
* Pre-existing, still OFF, awaiting his ruling: `raid_attack_alt_m = 0.0`,
  `raid_reattack_m = 0.0`; `strike_attack_alt_m` (E18).

**THE ASYMMETRY HE RULED — VERIFIED AND IT HOLDS.** `extend_leash = true`
(`config/scenario.toml:1209`), a required key. **No new clamp was added to
engaged pursuit.**

---

# 4. THE WORK, IN ORDER

## 0. C2 — THE DIVERT STANDOFF LAW. **THE LARGEST OPEN DEBT.**
**The bore still kills 62.5% of the raiders it swallows** (5 of 8). Routing them
in improved per-entry survival from 100% dead to 62.5%; the design's bar was
~10%. **We are routing raiders into a grinder we only half-fixed.**
Build `aim_station()` beside `maverick::aim_at`, switch only the three objective
call sites (strike divert / **defend — still never inspected by anyone** /
raid). Leave the four BFM callers alone; Chad's signed dogfight rides them.
Shape: a standoff station on a cylinder about the objective, radius
`max(0.85 × envelope, 1.3 × turn_radius)`, at attack height; vertical channel =
unity-gain feed-forward + small residual (`bore_track`'s shape). It subsumes and
retires all four E17/E18 dials.
⚠ The probe MUST carry a **foes-present arm** — the red team's stratification
says the divert-free bore still dies 33% of the time, a second killer with the
same wreck signature (plausibly the E2.4 arena fight interrupt), and a
divert-only probe is structurally blind to it.

## 1. THE LAST METRE — the honest next step on "attack me".
P-A says speed is not the gate; **nose-tracking / aim-slew once in the cone is.**
Do not re-attempt a speed fix. Instrument the aim chain (B2's `gc`/`bc` are
already on every tape row) and measure what breaks the firing solution.
Chad's a/b ruling on `bfm_intercept_chase_unfaded` gates whether this starts
from ON or OFF.

## 2. RAID-VIA-TUNNEL IS ROUTED BUT THE POST-KILL RETURN HAS NO MECHANISM.
Once the pump dies `raid.active` clears, so `raid_now` cannot fire and nothing
collapses a home-bound bore run. The raider rides the leash home across the deck
under the S1-DECK law. **Stated, not hidden.** Either collapse the home run at
the moment of the kill (while the order is still live) or rule it acceptable.

## 3. THE CHASE-ANYWHERE GUN-MUTE INSIDE A DOME.
An enemy that follows Chad below ~250 m AGL **inside** a dome still has its guns
muted by the terrain-avoid latch (`drone/drone.h`, the avoid override sets
`wants_fire = false` and is NOT air-faded). The deck law covers **errand**
flight, not fights. **Not fixed, not claimed fixed.** This is C3's ground:
demote the latch so it stops clearing `wants_fire`, or extend the capability
band into engaged pursuit over in-dome low air.

## 4. THE SLED COMBAT ANCHOR — the fairness bug. Not touched.
While Chad is driving, enemies still hold slots against and shoot at his
**parked plane**, and the man on the sled is untargetable. Latent today (no
parked episode >20 s in any tape) but it makes "attack me" provably false the
moment the millwright loop lands. ⚠ The patch as designed ships an unruled half:
the `combat_player_tick` sweep over the sled segment IS the damage half, and
sledder respawn semantics are **unruled millwright canon** — that is Chad's
ruling #4 below.

## 5. S4 — THE TELEPORT. **HELD, NOT STARTED, NOT WEAKENED.**
The teleport is exactly as it was at `ec9819a94`. Its design is ready to execute
as written **plus three corrections, all independently re-derived this rung**:
* **MURRAY PROXIMITY IS FALSE.** From the baked constants:
  `kSudburyCenterDir · kTunnelMouthMurray = 0.66595` → 0.84202 rad × 15,000 =
  **12,630.3 m**; spawn ring 0.3 × 13,050 = 3,915 m. A deck respawn lands
  **8,715–16,545 m** from the portal. "Next to the Murray portal" is wrong by an
  order of magnitude. Either state the transit cost honestly or bias the
  golden-angle bearing at the portal (Chad's open question 1).
* **THE LATCH DISARMS ANY PLANE THAT OBEYS THE DECK RULING** — this was true
  pre-S1 and S1-DECK is the fix; re-verify the post-collapse fleet's latch duty
  now that the deck band exists, and add a latch-duty% row to `[.s4collapse]`.
* **Comment references to the deleted scramble helper: FIVE, not three.**
  `app/instructor_tick.h:438`, `:519-522`, `test_conquest_respawn.cpp:12`,
  `:410`, and the harness banner `test_conquest_match.cpp:27`. All five must be
  rewritten in the deletion commit. ⚠ `drone/drone.h:1847/2167/2279` use
  "scrambled" as plain English about defenders — **DO NOT touch those.**
  ⚠ The harness fork is real: `test_conquest_match.cpp:1044-1049` books
  `scrambled[]` and never relocates, while its banner at `:27` claims it runs.

## 6. E12's RED — Chad's ruling, still open. See §5.

---

# 5. THE RULINGS THAT ARE CHAD'S, NOT YOURS

1. **`bfm_intercept_chase_unfaded`** — (a) leave it OFF (they keep closing at
   ~144 m/s and mostly never reach him) or (b) turn it ON (one word) and accept
   they arrive fast and overshoot, shooting less but getting closer more often.
   His stick, not P-A, is the judge.
2. **Difficulty.** Pressure rose 0.0336 → 0.0392 with no dial moved. He ruled
   "leave it at full strength" and will rule after flying. **Do not pre-empt it.**
3. **The bore grinder.** Fix the divert so raiders survive the trip
   (recommended), or let raiders **skip the deep pump entirely** on a
   surface-pump run — the second is faster but it deletes half his own standing
   mission, so it is his.
4. **What "fair game" means for the snowmobiler**, and sledder respawn
   semantics. Blocks §4.4.
5. **E12's red**: pin the tape-7 arm's heightfield, or re-baseline the two rate
   clauses? ★ (b) moves a signed threshold. Do not do it silently.
6. **The raid attack pattern** (`raid_attack_alt_m` / `raid_reattack_m`, OFF)
   and **E18** (`strike_attack_alt_m`, OFF) — both still open from last rung.

---

# 6. TWO INSTRUMENTS THAT LIE, FOUND THIS RUNG

1. ⚠ **`[.deckx]`'s deck-AGL p10 is contaminated. Nobody may quote it.** The
   shipped arm reads **p10 = −842 m** — below ground. `deck_agl` is collected on
   `d.deck_scope` alone with no underground exclusion
   (`test/unit/test_conquest_match.cpp:1073`), and `deck_scope` is by design a
   pure observation computed everywhere (`drone/drone.h:2350-2373`). Inside the
   bore it probes air at ground+400 m — thin — so it latches true while the
   drone is 2 km under the DEM. **The behaviour is innocent:** the whole deck
   block is guarded by `!underground_mode && !arena_exempt`
   (`drone/drone.h:2918` region), so the deck law is structurally inert in the
   bore. **The median (115 m), survival and mute statistics are unaffected.**
2. ⚠ **The raid branch takes the track law on hot-fight ticks.**
   `deck_errand_order` is set in the raid branch under `d.raid.active &&
   (!fight_hot || dp.raid_fight_in_place)` (`drone/drone.h:2625`), and
   `raid_fight_in_place` ships **true** (`config/scenario.toml:1229`). So a
   raider fighting *while raiding* gets `deck_track_gamma` on its elevation
   channel. It is not a pursuit nerf (the raid branch, not BFM, owned that nose
   already), but S1's claim "an ENGAGED tick is NEVER overridden" is true only
   for the pursuit/BFM path, **not literally for `d.engaged`.**

---

# 7. THE LAWS — carried forward, now paid for TEN times

* ★★★ **A CONSTANT (OR A HAND-MAINTAINED LIST) THAT DESCRIBES THE SHIPPED TABLE
  STOPS DESCRIBING IT THE MOMENT THE TABLE MOVES.** **Read the loader. Always.**
  ⚠ Exception: RaidOrder/DefendOrder gains have NO TOML key — for those two
  branches the header *is* the shipped table.
* ★★★ **A FIXTURE-WIDE CHANGE MOVES EVERY ARM THAT SHARES THE FIXTURE.**
  **NINTH instance, this rung, and it burned a headline:** S1-DECK's
  0.778 / 0.22 / 1.4% were measured before S2-TUNNEL landed and **do not
  reproduce at HEAD** (0.526 / 0.45 / 2.7%). The router changed the population of
  deck episodes underneath the same probe. **Re-measure every headline at HEAD
  before writing it down.**
* ★★★ **NEW — A GREEN GATE CLAUSE CAN BE BLIND TO A 57% CAPABILITY LOSS.** P-A's
  `rounds_at_player >= max(10, 10·pre_rate)` passes on BOTH arms of a change that
  cuts rounds at Chad 54 → 23 and hits 16 → 2. **A threshold clause grades a
  floor, not a delta. If you care about the delta, assert the delta.**
* ★★★ **NEW — THE MECHANISM STORY CAN BE RIGHT ABOUT THE DIAGNOSIS AND WRONG
  ABOUT THE PRIORITY.** The 144-vs-262 m/s closure deficit is real and exactly as
  computed. Fixing it made "attack me" *worse*, because closing was never the
  last gate — holding the nose is. **Write the differential before believing your
  mechanism story.**
* ★★★ **NEW — A CAPABILITY YOU BUILT AND MEASURE AS HARMFUL SHIPS OFF **LOUDLY**,
  AS A NAMED KEY WITH ITS TABLE BESIDE IT.** That is the opposite of a secret
  nerf. Refusing silently, or shipping it because it was asked for, are both
  failures.
* ★★★ **NEW — ONE LINE CAN MAKE TWO SYSTEMS MUTUALLY EXCLUSIVE BY
  CONSTRUCTION.** `hold_runs` meant a raid order and a tunnel run could never
  coexist, so a standing mission signed a month earlier had **never once been
  flown**. When a feature "has never happened", look for the interlock before
  looking for the bug.
* ★★★ **NEW — THE PREDICATE, NOT THE LAW.** S1's first measured run put the
  track law on defend/raid/patrol only. A maverick TRANSIT leg has
  `tunnel_mode == true` and never enters that chain — so TRANSIT, the single
  biggest terrain-crash mode in the tapes (27 of 68), was the one leg the law
  could not reach. Same code, one predicate: survival 0.536 → 0.778, crashes
  15 → 5, **and the forward eyes flipped from harmful to helpful.**
* ★★★ **A BIT-IDENTICAL ARM MEANS BLIND FIXTURE *OR* DEAD BRANCH.**
* ★★★ **A CHANGE THAT MOVES THE CONTROL ARM OF THE PROBE THAT GRADES IT CANNOT
  BE GRADED BY THAT PROBE.**
* ★★★ **WHEN YOU ADD AN EXIT TO A STATE MACHINE, IT INHERITS EVERY DEFERRAL THE
  OLD EXITS CARRY.**
* ★★ **VERIFY FROM BOTH DIRECTIONS.** Every mutation this rung was required to go
  RED and then be restored. ⚠ **And say which ones did NOT:** S2's dead-dome
  guard removal and `raider_rank` tie-break flip **did not go red** and are not
  claimed — they stay in as defence in depth, unverified.
* ★★ **MEASURE THE NOISE FLOOR FIRST.** Every headline in §1 has one.
* ★★ **DETERMINISM IS THE DIFFERENTIAL TOOL.** 17-digit identity across a
  `git stash` proved S3-GUNS-A observation-only.
* ★★ **A HAND-MIRROR IS NOT A CALLER.** E12/P-H's `fly_match`
  (`test/unit/test_conquest_match.cpp:28,607,1212`) mirrors
  `app/instructor_tick.h` by hand. Bit-identity there proves a struct change
  inert; it proves **nothing** about emission. Drive the real `app::tick`.
* ★★ **THE SHIPPED DEFAULT CAN MAKE A SEAM STRUCTURALLY UNREACHABLE.**
  `all_vs_player` makes every drone's foe the player, which renders the AI-vs-AI
  emission seam unreachable in a naive fixture.
* ★★ **GUARD EVERY `normalize()`** — RaidOrder's default target is the origin.
* ★★ **THE `pre_e2` TRIPWIRE EARNS ITS KEEP.** It fired twice this rung
  (1312 → 1376 → 1408 → 1416), once on a single bool two layers down in a nested
  struct nobody was watching. Weld the audited size in the SAME commit, per its
  own instructions.

---

# 8. HOUSEKEEPING

* **Do not push.** Main advances on Chad's stick. Commit freely on
  `sandbox/enemy-ai`.
* **Do not touch `assets/`.** Do not work on main. Never touch another
  `D:\seads_sandboxes\*` or `D:\flight_sim2\seads*` worktree — one object store,
  other branches checked out.
* **THE GRAPH, NOT GREP.** `python tools/graph/graph_query.py
  file|symbol|impact|tests-for|who-includes …`. Regenerate the graph in the SAME
  commit as any structural change, then `graph_query.py check` (`layer check
  OK`).
* Relink the fly build (`cmake --build build-play --target seads`) before
  finishing.
* ⚠ **Never relink while a sweep is running**, and ⚠ **a concurrent `ctest` from
  another worktree WILL pollute your gate output file** — it happened this rung
  (`seads-recon`); the run was discarded and re-run clean. Check the binary
  timestamp against the newest source before believing a gate.
* The census scripts (`gun_census.py` / `gun_forensics.py`) are still only in the
  session scratchpad, **not committed**. Commit them beside the tapes' tooling so
  the tape numbers stay reproducible.

## What Chad should find when he flies this

Enemies down on the deck **with him** at ~115 m over the rock instead of
porpoising at 400 m in vacuum; raiders taking the tunnel; tracers in the air and
sparks on his pump; **10 wrecks in 22 minutes instead of 16**. And an enemy that
follows him out over the deck and keeps fighting — but still cannot reliably
hold a firing solution on him. That last one is the next rung.
