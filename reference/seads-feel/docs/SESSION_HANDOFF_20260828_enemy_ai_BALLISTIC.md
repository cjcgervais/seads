# ENEMY-AI — RUNG D3, THE BALLISTIC DECK RUN. HANDOFF 2026-08-28

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260828_enemy_ai_BALLISTIC.md; take
the next enemy-AI rung."**

**READ FIRST, IN THIS ORDER:**
1. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon, IN FULL. **An agent that does not know the loop will
   "fix" the AI into something that cannot play the game. Standing rule.**
2. `docs/SESSION_HANDOFF_20260828_enemy_ai_DECK.md` — the rung before this one
   (S1-DECK / S2-TUNNEL / S3-GUNS), whose §4 work list is still the queue.
3. `CLAUDE.md` standing laws (top of file).

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`, **nothing
pushed**.

---

# ★★★ NERF WATCH — FIRST, BECAUSE IT IS THE STANDING INSTRUCTION

**NOTHING WAS REDUCED. The AI got harder, and the numbers are the probe's.**
[.bdeck], shipped arm C vs arm O (the master dead switch = the pre-D3 machine,
bit-for-bit `0x12dd30472363fcf5`), one 22.4-minute tape-7 replay:

| | O = pre-D3 | C = D3 SHIPPED | |
|---|---|---|---|
| enemy pressure (on-station rate) | 0.0392 | **0.0448** | **+14.3% OFFENSE** |
| enemy crashes | 10 (0.45/min) | **8 (0.36/min)** | −20%, vs Chad's signed 0.94 |
| deck-crossing survival | 10/19 = 0.526 | **14/20 = 0.700** | +33% |
| gun-mute on the deck | 3.5% | **2.9%** | fewer muted ticks |

`raid_dps_frac` untouched. No pursuit clamp added. `extend_leash` still true.
**The D2 rung's parked −6.4% capability reduction is GONE** — see §3.

**The two things that are WORSE, stated:**
* **Gun-mute DURING a live ballistic phase is 3.9%** against the 2.9% fleet
  deck baseline. It is the terrain latch cycling under a 243 m/s sprint, not a
  new mute — no `wants_fire` line was touched in any phase.
* **RUN/SPENT deck AGL median 149 m** against the fleet errand median 124 m.
  The sprint rides ~25 m higher than ordinary deck flight. Both numbers are
  printed side by side in the probe, every run.

---

# 1. WHAT LANDED

Chad, 2026-08-27, verbatim — and the phases map onto it 1:1:

> "they should go back to whatever bubble if they have one left, **climb to a
> decent altitude** and then **parabolic dive** to the deck to cross the no air
> zone to the enemy bubble to attack" / "even if they are pointed upward a bit
> they will sail to the other bubble"

`BallisticPhase` (`drone/drone.h`) now runs
`NONE → RETURN → CLIMB → DIVE → RUN → SPENT`, monotone, one-shot per
raid-order lifetime, reset on respawn / order drop / tunnel routing.

* **App owns the transitions** (`app/instructor_tick.h`, the D2 block) — it
  alone holds conquest, the live ellipses and the ground. `drone/` stays
  conquest-blind.
* **Drone owns the steering**, phase-keyed **inside the raid branch**, so every
  ruled predicate is inherited verbatim: defend > regroup > raid priority,
  ENGAGED never dragged, `raid_fight_in_place` snapshots, leash suppression,
  C1's errand speed off-phase. **No state machine gained a new exit.**
* **Guns: not one line touched in any phase.** A maverick-mode implementation
  would have arrived structurally DISARMED (lock L3); that is the argument that
  kept it out of the maverick machine.

## THE HONEST HEADLINE, AND IT IS THE PROBE'S

★★★ **THERE IS ALMOST NO GAP TO BALLISTICALLY CROSS, AND THIS IS NOT A CRASH
FIX.** Outside a dome the air is FULL below 120 m AGL and zero only by 320
(`config/game.toml:152,155`); the dead band is purely VERTICAL. At HEAD the
median crossing already spent **0.0 s** in dead air, and **ZERO of 10 enemy
wrecks died in thin air** (all 10 below 60 m AGL, 7 of 10 with a TUNNEL
disposition — the bore grinder, still the open C2 debt). The "leave the dome at
1,700–1,900 m and mush down at 124 m/s" population is the **pre-S1-DECK**
machine and no longer exists.

So the ballistic phase is the **dome-skirt transition**, and the crossing
itself is powered deck flight. What shipped is **spectacle + arrival energy**.

## THE SHIPPED DIALS (`config/scenario.toml`, all loader-REQUIRED)

```
raid_ballistic_climb_agl_m    = 2000.0   # 0 = THE WHOLE-FEATURE DEAD SWITCH
raid_ballistic_climb_max_s    = 60.0     # ADVANCES to the dive, never aborts
raid_ballistic_dive_gamma_deg = 18.0
raid_ballistic_dive_speed     = 245.0
raid_ballistic_align_min      = 0.94     # HORIZONTAL, see §2
raid_ballistic_run_agl_m      = 500.0
regroup_return_deck_run       = true     # flipped ON, cost measured at ZERO
```

## WHAT IT LOOKS LIKE FROM THE COCKPIT

The designated deck runner (rank 0, `raid_deck_run_slots = 1`) noses up inside
its own dome and climbs at the airframe's proven 26° pull-up angle to a
**measured apex of 2,018 m AGL**. It commits the moment it is both high enough
and pointed at the pump — **0 of the commits fired on the timeout**, so what
you see is a deliberate wing-over onto the target bearing, not a clock running
out. Then a committed **18° dive at full throttle**, 55 s of it, down to 500 m
AGL. It arrives through the top of the deck's air column at a **measured
238 m/s** against the fleet's 160, levels into the 100 m lane under the S1-DECK
track law, and **sprints the deck at 243 m/s for ~2 minutes** before bleeding
to the on-station speed inside the enemy dome.

⚠ **ONCE per 22-minute match on this replay.** The sequence is one-shot per
raid-order lifetime and there is one designated runner, and the replay carries
one rank-0 order lifetime (886 s, printed as the probe's denominator). **That
is Chad's question 1 in §5.**

⚠ **The climb is only 5.5 s of it** (330 ticks). The fleet already cruises near
2,000 m, so the wind-up is short; the visible manoeuvre is the dive and the
sprint.

---

# 2. THE FIVE RED-TEAM CORRECTIONS, ALL FOLDED, ALL LOAD-BEARING

1. **The commit alignment is HORIZONTAL.** `drone::horizontal_align` projects
   both the velocity and the great-circle bearing into the local horizontal
   plane. The design's full-3D dot could never exceed cos(26°) = 0.899 in a 26°
   climb, so a 0.94 gate **could never pass** and every dive would have
   committed by timeout from ~3,500 m — the E16 higher-is-worse trap, with the
   climb dial inert in the commit path. **Measured at HEAD: 1 commit, 0 by
   timeout.** The correction is the reason the feature works.
2. **`raid_ballistic_run_agl_m` clears BOTH pull-up bands** — loader-checked
   against `deck_avoid_agl_enter_m + avoid_lookahead_s · dive_speed ·
   sin(dive_gamma)` (= 287 m) **and** against `avoid_agl_release_m` (400).
   Handing over lower lands the transition inside a latched 26° panic climb.
3. **The OFF state really is the pre-feature machine.** The stage-1 RETURN is
   gated on `raid_ballistic_climb_agl_m > 0` in the app **and** the fixture
   mirror, so arm O reproduces `0x12dd30472363fcf5` bit-for-bit. That is a
   CHECK, not a claim.
4. **The re-pins are DECLARED, not discovered.** Shipping ON moves the shipped
   machine, so `[.bdeck]`'s head pin and `[.regroup]`'s S pin both moved to
   `0x2ec5baddc8346cfe`, and `[.regroup]`'s `rets == 0` clause was re-aimed.
   Every one is named here and commented at its literal.
5. **Derived, never welded.** The pull-out bound is a both-configs GATE leg
   (`test_conquest_match.cpp`, beside S1-DECK's) computing
   `n_eff = 0.5·avoid_pull_net_g` and
   `h_band = deck_agl_m + deck_soft_m − deck_track_agl_m` from the live tables,
   with a non-vacuous half (a dive twice as steep must FAIL it). The dive speed
   is documented as a FREE dial that merely happens to equal `v_redline`, not
   an identity that would silently follow it.

---

# 3. ★★★ WHAT THE PROBE FOUND THAT NOBODY PREDICTED

## 3a. The first build dived into its own back yard for 161 seconds

`DIVE → RUN` was authored as `d.deck_scope && agl < run_agl`. **Chad's dive
starts INSIDE his own dome**, where `deck_scope` is false by construction (the
air at 400 m is full). So the phase could not advance until the runner had flown
all the way out of its own bubble — holding the committed −18°, with the track
law excluded, straight down into its own terrain and porpoising off the in-dome
latch for a **measured 161 s (9,690 DIVE ticks), gun-mute 22.3% against a 2.2%
fleet baseline.**

Fixed by grading the dive's end on **altitude alone**. DIVE ticks 9,690 → 3,284;
in-phase gun-mute 22.3% → 3.9%.

★ **THE LAW:** *a location predicate is not an altitude predicate, and a phase
that begins inside the thing the predicate excludes can never leave it.*

## 3b. Arriving at 243 m/s at the pump COST 5.4% of the offense

With the sprint ending only at the enemy dome the runner arrived far too hot to
close a 1,000 m on-station orbit — the C1 defect at 2×. Measured:
pressure **0.0392 → 0.0371**, crashes 10 → 12.

Fixed by the design's own recommended option (a): the sprint also ends at a
**bleed range derived from the airframe**, not dialled —
`mass / (0.5·ρ·S·Cd0) · ln(v_sprint / v_errand)` = **10.3 km** on the shipped
table, so the bleed to the on-station speed happens across the enemy dome's air.
Pressure **0.0371 → 0.0448**, crashes 12 → 8, survival 0.545 → 0.700.

★ **THE LAW, again:** *the mechanism story can be right about the diagnosis and
wrong about the priority.* Arrival ENERGY was never the last gate; arriving
**spendable** is.

## 3c. Stage 1 (the RETURN) is SHADOWED by stage 2, and therefore free

`regroup_return_deck_run` shipped ON and `[.regroup]` arm F (it forced back OFF)
is **bit-identical** to the shipped arm; stage-1 returns armed **0 on both**.
Stage 1 needs `ballistic == NONE` **and** `own_frac > 1.0`; D3's already-home
arming fires at `own_frac <= 1.0` and latches CLIMB first. The rank-0 runner's
order always arms while it is still inside its dome, so the CLIMB wins the race
every time. **That is Chad's own sentence resolving itself** — "go back to
whatever bubble IF THEY HAVE ONE": it has one, and it is in it.

⚠ **So D2's −6.4% is gone, and NOT because the dive paid it off — it is
SHADOWED. Stage 1's own flight is consequently NOT exercised at this head.**
Stated here rather than left for a green light to imply.

---

# 4. VERIFICATION

## Mutations — each applied, built, run, and RESTORED

| | mutation | result |
|---|---|---|
| **MUT-1** | delete the DIVE's `target_gamma` | **RED** — `dive.gamma −12.66` vs required `< none.gamma − 3 = −16.14` |
| **MUT-2** | mis-key the track-law exclusion (DIVE → RUN) | **RED** — the dive pins at **exactly −12.0°**, the predicted stillbirth |
| **MUT-3** | revert the router's ballistic guard | ⚠ **BIT-IDENTICAL — DID NOT GO RED** |

★ **MUT-1's FIRST VERSION SAILED THROUGH GREEN and that is the lesson worth
carrying.** The clause was `dive.gamma < −12.0`. An aeroplane pointed at a pump
30 km away and 3 km below is **already** descending ~13° on `aim_at`'s own
elevation channel, so the threshold passed with every line of the DIVE steering
deleted. **A threshold clause grades a floor, not a delta** — the clause is now
`dive.gamma < none.gamma − 3.0` against the same aeroplane flying the same raid
order with no phase, and it collapses.

⚠ **MUT-3 IS NOT VERIFIED AND IS NOT CLAIMED.** The router guard (a
phase-committed drone is not stamped for the bore) is **unexercised on this
replay**: reverting it reproduced the shipped hash exactly and every phase-tick
count to the digit. No rank churn ever meets a committed diver here. It stays in
as defence in depth, **unverified** — the S2 precedent.

## Closed-loop flight legs (gate, `test/unit/test_drone.cpp`)

Written **because a moved hash proves the ORDER exists, not that an aeroplane
turned** (the D2 MUT-3 law). Both grade metres and degrees:

* *"D3 the ballistic phases actually fly the climb and the dive"* — CLIMB gains
  +637 m where the same order with no phase loses −457 m; DIVE is ≥3° steeper
  and ≥20 m/s faster than that same order.
* *"D3 an armed DIVE outdives the deck track law it is excluded from"* — with
  real ground and a bare deck atmosphere, the errand command measures **exactly
  −12.0°** (the track law's cap) and the DIVE measures **−20.05°**. Graded on
  B2's `cmd_gamma` witness, and `REQUIRE(d.deck_scope)` makes it non-vacuous.
  ⚠ −20.05 not −18: the extra 2° is the **E11 air-seek**
  (`avoid_air_dive_gamma` 0.35 rad = 20.053°) taking a `min()` in thin air. It
  can only ever steepen, so the clause is a bound, not an equality.

## THE GATE — 1572/1578, SIX REDS, NO SEVENTH ★ BUT THE SIXTH CHANGED IDENTITY

Full serial run, 3,312 s. Total grew 1575 → 1578 (+3 legs, all green).

| # | test | verdict |
|---|---|---|
| 58 | `probe P-F: the relentless raider keeps the pump and shoots back` | BASELINE, red on purpose. **Do not bend P-F a fourth time.** |
| **98** | **`E12.1: the raider backfill keeps a faction's pump offense alive`** | ★ **NEWLY RED — see below** |
| ~~99~~ | `E12: the enemy's pump offense does not regress below tape 7` | ★ **NOW GREEN.** The documented baseline red PASSES at this head. |
| 829, 830, 868, 871 | sled | BASELINE — the four GI4 sled debts (renumbered by the +3 legs; same four by name). |

★★★ **D3 SWAPPED WHICH OF THE TWO E12 LEGS IS RED, AND IT IS ATTRIBUTED, NOT
ASSUMED.** I re-ran both against the pre-D3 machine (`raid_ballistic_climb_agl_m
= 0`, the master dead switch, everything else identical):

| | pre-D3 | D3 shipped |
|---|---|---|
| E12 (offense vs tape 7) | **FAIL** (the documented baseline) | **PASS** |
| E12.1 (backfill keeps offense alive) | **PASS** (7/7, rate **+0.0013**) | **FAIL** (0.0334 vs 0.0385) |

**Nothing was bent. E12's 1.15 factor and E12.1's threshold are untouched.**

E12 going green is the same +14.3% offense the headline reports, arriving at a
signed threshold it used to miss.

★ **E12.1's red is a REAL INTERACTION and it is the next agent's first job.**
`raider_backfill` churns `raider_rank`, and `deck_run = rank < raid_deck_run_slots`
— so **backfill churns WHO the designated ballistic runner is**, and the D3
sequence is one-shot per raid-order lifetime tied to that slot. That is exactly
the yank the router's ballistic guard was built for. ⚠ **And MUT-3 said the
guard is inert** — at the SHIPPED `raid_dps_frac`. E12.1 runs at
`raid_dps_frac = 0.5`, a different operating point, and there the churn clearly
bites. **The guard is not proven to be the fix, and it is not claimed as one.**
Instrument `bal_lost` under a forced rank-churn fixture before touching either
threshold.

## The noise floor MOVED and it must be quoted

`|C−N|`: survival **0.0000**, crashes/min identical, pressure **0.0001** — the
headline deltas clear it by 56×. **But arrival |v| at 320 m is now 9.90 and
dead-air/crossing 0.62** (both were 0.0000 pre-feature). Any delta on those two
under ~99 m/s / ~6.2 s **is not a result**. The 238 m/s in-phase arrival is
**n = 1** — one crossing — and is reported as an observation, never a statistic.

---

# 5. ★ THE ONE QUESTION THAT IS CHAD'S

**Should the ballistic run RE-ARM, or stay one-shot?**

It is one-shot per raid-order lifetime, which on this replay means **he sees the
manoeuvre once in 22 minutes** — the rank-0 runner does it, and then raids
normally for the remaining 21. He asked for this partly to *watch* it ("it would
be cool in general to see them climb up then perform a parabolic dive"), and once
a match may not be what "in general" meant.

Three ways, and it is his ruling, not mine:
* **(a) leave it one-shot** (shipped). Monotone, cannot loop, cheapest.
* **(b) re-arm from SPENT** whenever the runner is back inside its own dome with
  the enemy pump alive — he would see it every sortie. Costs the monotone
  guarantee; needs a hysteresis so it cannot chatter.
* **(c) raise `raid_deck_run_slots`** above 1 so more than one pilot does it —
  but that is his own "a few designated runs" dial and moving it changes the
  tunnel/deck mix, not just the spectacle.

**Nothing was quietly picked.** The literal reading ships.

Still open and unchanged from the last handoff: `bfm_intercept_chase_unfaded`
(a/b), E12's red, the bore grinder (C2), the millwright "fair game" ruling, and
the raid attack pattern / E18 dials.

---

# 6. WHAT I DID NOT DO, AND ITS PRICE

* **One replay only.** Every number is a single 22.4-min tape-7 match. There is
  no ensemble. The 8-vs-10 wreck delta in particular is well inside Poisson
  noise for a 10-event count; `|C−N|` measures **fixture** noise, not sampling
  noise, and cannot license it.
* **The `[.bdeck]` L arm says higher is WORSE, and I did not chase it.**
  Doubling `raid_ballistic_climb_agl_m` to 4,000 m drops pressure 0.0448 →
  0.0180 and raises crashes 8 → 10. That is E16's "domes are DOMES" showing up
  in a new place. 2,000 m is not tuned to that result — it was chosen before it
  — but the dial is clearly not flat and nobody has swept it.
* **The router guard is unverified** (MUT-3 above).
* **C2, the bore grinder, is untouched** and is still the largest open debt:
  7 of 10 wrecks at HEAD carry a tunnel disposition.
* **`d.cmd_gamma`/`cmd_bank` (B2) are now load-bearing in a gate leg.** They
  were REPORT-ONLY. Nothing reads them in flight — but a future rung that
  repurposes them will break that leg, and should.
* **E12.1's red is attributed but not MECHANISED.** I know D3 caused it and I
  know which coupling (backfill → `raider_rank` → `deck_run` → the one-shot
  slot). I did not build the forced rank-churn fixture that would show
  `bal_lost > 0` and prove the router guard either fixes it or does not. Price:
  the guard ships unverified twice over — inert under MUT-3 at the shipped
  table, and untested at the operating point where the interaction is visible.

---

# 7. THE FLY

Build relinked. Open **`D:\seads_sandboxes\enemy-ai\build-play\seads.exe`**
(`pre-reconcile-20260821-103-g68936237e`, 88,717,986 B).

What to look for, once per match, from the enemy side:
1. A SUDBURY raider **noses up inside its own dome** and climbs steadily to
   ~2 km over the rock.
2. It **rolls onto the bearing of your surface pump** and pushes over into a
   sustained ~18° nose-down run at full throttle — about a minute of it.
3. It **levels into the deck lane at ~100 m** and crosses at ~243 m/s, roughly
   twice the speed anything crossed at before.
4. About 10 km out it **eases off** and settles onto the pump at the ordinary
   attack speed rather than blowing through it.

If it looks deliberate, it is doing what you described. If you only see it once
and want it every sortie, that is §5's question.
