# ENEMY-AI — PHASE B: MAKE THE AI VISIBLE. HANDOFF 2026-08-27

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260827_enemy_ai_PHASE_B.md; take the
next enemy-AI rung."**

**READ FIRST, IN THIS ORDER:**
1. `docs/SESSION_HANDOFF_20260826_enemy_ai_REPAIR.md` — the rung just flown.
   Its §1 Phase A is **DONE**; its Phase B and Phase C are **NOT**.
2. `docs/AUDIT_FINDINGS_20260825_synthesis.md` — the six-consult audit, the
   evidence base. ⚠ **Its §"one thing" bullet about P-H's crashes is INVERTED**
   — see LAWS below; everything else in it survived re-measurement.
3. `docs/ENEMY_AI_E1_E2_SPEC.md` — the binding ledger. ⚠ Five of its recorded
   on-station numbers are now VOID or unsafe (list below).
4. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon. **Every consult you spawn gets §1–2 pasted in.**

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`, HEAD
`334b6396b`, **nothing pushed**, tree clean. Fly build relinked at
`build-play/seads.exe` (`pre-reconcile-20260821-90-g334b6396b`).

---

# ★★★ THE STATE OF THE BRANCH

**Phase A landed in full. The instruments are repaired and NO AI BEHAVIOUR
MOVED.** Seven commits, `31c53150c..334b6396b`, zero edits to `drone/` logic,
`combat/`, `app/`, `assets/`, or any shipped dial. The only non-`test/` edits in
the whole rung are three comment blocks (`drone/drone.h` ×2,
`config/scenario.toml` ×1), all of them deletions or corrections of stale
recorded numbers.

```
334b6396b  C-CONTRADICTION SETTLED: the terrain-avoid LATCH makes the 412 m floor (C4)
4b601fdde  A6: DELETE the stale [.e18dive] table in drone/drone.h
8274d70e2  A5: liveness arm in every sweep + raid-saturation telemetry in P-H
fb66fcdb7  A4: the raid probe flies the SHIPPED airframe (115 -> 85 m/s)
94d1a3b0f  A3: invert pre_e2 -- the CONTROL arm no longer inherits new strike dials
31f71d5ca  A2: the on-station receipt replaced by a pressure RATE
b58a9a5d4  A1: re-point probe P-H's world at the real Sudbury DEM
```

## THE GATE — 1540/1546, six reds

| # | test | verdict |
|---|---|---|
| 58 | `probe P-F: the relentless raider keeps the pump and shoots back` | BASELINE, red on purpose. **Do not bend P-F a fourth time.** |
| **96** | **`E12: the enemy's pump offense does not regress below tape 7`** | **NEW. Yours to decide — see §1 item 0.** |
| 810 | `sled_slides_before_it_tips_on_flat_snow` | BASELINE — GI4 sled debt |
| 811 | `sled_grip_ceiling_stays_below_the_tip_threshold` | BASELINE — GI4 sled debt |
| 849 | `sled_assist_reference_plane_is_load_weighted` | BASELINE — GI4 sled debt |
| 852 | `sled_debug_sink_is_write_only` | BASELINE — GI4 sled debt |

Total grew 1545 → 1546: `pre_e2 tripwire: DroneParams has not grown past the
partition` (`test/unit/test_stope_probe.cpp:249`), and it passes. The new
`FLOOR sweep: who makes the 412 m raid floor`
(`test/unit/test_conquest_match.cpp:1641`) is hidden-tag `[.floor2x2]` and does
not enter the gate.

Full serial gate ≈ 33 min. P-H now costs 61.1 s per run (real DEM decode); E12
costs 115.9 s (two `fly_match` arms). The 80-minute *"orbit inertia:
back-to-back grabs blend by the EMA factor"* hang **did not recur in four full
gate runs** and is still uninvestigated.

## WHICH INSTRUMENTS YOU MAY NOW TRUST

* **P-H** flies the real 8192×4096 Sudbury DEM
  (`test/unit/test_conquest_match.cpp:373`). Enemy home ground elevation
  300 m → **68 m**; thin-air crashes **27 → 0**; mean air at death 0.12 → 0.99.
* **Pressure is a rate**, `MatchResult::onstation_rate()`
  (`test/unit/test_conquest_match.cpp:505-512`) = `enemy_onstation_s /
  player_pump_alive_s`. On the real DEM the seconds counter is **exactly
  degenerate** across E12.1's two arms (48.7 s vs 48.7 s, Δ 0.0); the rate
  separates them (+0.0004).
* **P-B / P-C's control arm is honest.** `pre_e2` inverted at
  `test/unit/test_stope_probe.cpp:131-243`: starts from `DroneParams{}` and
  copies the 63 non-E2 members forward. Census verified mechanically against
  `drone/drone.h:57-678` — **82 members = 63 copied + 19 E2**, exact set match.
* **The raid probe flies the shipped airframe** (`kScen.drone`,
  `test/unit/test_raid_persist_probe.cpp:104`), 85 m/s not 115.
* **Every sweep has a liveness arm and all of them are LIVE.** No blind fixture
  anywhere in `[.e12sweep] [.e15sweep] [.e16sweep] [.e17sweep] [.e17raid]`,
  P-B, P-C.

## WHICH ARE STILL LYING, OR NOW KNOWN VOID

* ⚠ **`ENEMY_AI_E1_E2_SPEC.md:2166-2167`** — the signed E16 "42.0 → 46.2 s". The
  46.2 **IS the ceiling** (`2 × pump_kill_seconds / raid_dps_frac = 2×15/0.65 =
  46.15`, `config/game.toml:394,425`). **The magnitude is VOID.** E16's
  conclusion survives on its pump and crash rows.
* ⚠ **`SESSION_HANDOFF_20260824c:224-225,264`** — E17's "identical 46.2 across
  five arms": every arm at the ceiling. That dial comparison is void.
* ⚠ **`SPEC:1692-1695`** (E12.1 "+48%") and **`SPEC:1892`** (E13.2 "−18%") —
  credit-truncated, magnitudes unsafe, directions carried by other rows.
* ⚠ **The seconds `CHECK`s were NOT retired**, only annotated:
  `test/unit/test_conquest_match.cpp:1090` and the P-H.0 15–70 s band. They can
  still degenerate. **Never weld a new ruling to them.**
* ⚠ `test/unit/test_stope_probe.cpp:249`'s `sizeof(drone::DroneParams) == 1312`
  is **MSVC Debug x64 specific** and blind to a field added inside existing
  padding.

---

# 1. THE WORK, IN ORDER

## 0. FIRST, THE RED YOU INHERIT — E12 (30 min, then Chad)

`test/unit/test_conquest_match.cpp:1363` and `:1380`, two hard `CHECK`s:

```
b.enemy_raidduty_s / b.player_pump_alive_s > a.… * 1.15
  → 0.97330068647297507 > 0.98914561099595499     (shipped is +13.2%, bar is +15%)
b.enemy_near_s / b.player_pump_alive_s > a.…
  → 0.0567522030566939 > 0.06319899387494253      (INVERTED)
```

`git blame` puts both lines at `cc0c38626` — pre-rung, signed. This rung never
touched either expression or the 1.15. What moved is the shared world builder at
`:373`. The tape-7 arm **explicitly pins the world** at `:1338-1343` ("★ E16:
THE WORLD IS PART OF THE ARM") — but it pins `deck_terrain_relative` and
`transit_reach_s_per_km`, **not the heightfield**, which A1 swapped underneath
it. On a flat shell terrain-relative and absolute deck are nearly the same
thing; on the real DEM they are not, and the comparison inverts.

Two honest fixes, and **the choice is Chad's, not yours**:
(a) give the tape-7 arm its own `uniform_field` so it keeps measuring the world
tape 7 was flown on and the E16 comment stays true; or
(b) re-baseline both rate clauses against the real DEM.
★ **(b) is moving a signed threshold. Do not do it silently.**
Corroboration that this leg's recorded numbers are already stale: its own
comment at `:1358-1361` says 2358 s / 2028 s; measured this rung, **2018 s and
2007 s**.

## PHASE B — MAKE THE AI VISIBLE. THIS IS THE RUNG. (the whole point)

**B1. Give every point of AI damage a visible source.**
`app/instructor_tick.h:63-66` already *promises* it — "abstracted like the pump
raids (real tracers fly for the look; only the player is swept against rounds)"
— and it is **false in practice**: `wants_fire` is 0 of 70,220 samples across
tapes 10 and 11.

Sites, all read and confirmed at HEAD:
* `app/instructor_tick.h:1663` — `combat::ai_guns_on(...)`, the AI-vs-AI credit.
* `app/instructor_tick.h:1889` — `combat::damage_pump(...)`, the pump credit.
* `app/instructor_tick.h:1798` — `combat::FxKind::HitSpark`, the existing spark
  emitter to mirror.

⚠ **COSMETIC POOL ONLY.** Must not be swept against the player, or the
difficulty Chad signed moves. Log it as cosmetic and leave the real gun defect
open behind it.

★ This is the highest experience-value change available and **it cannot regress
the balance**. It is also the only Phase-B/C item that does not first require a
ruling from Chad.

**B2 (do it inside B1). One byte on the drone row of `app/conquest_tape.h`.**
The tape's `fs` flag is `d.friendly_side` (`app/conquest_tape.h:236,244`) —
**not** the terrain-avoid latch, so the 2026-08-26 handoff's suggested instrument
does not exist. Add `terrain_avoid_engaged` to the row and Chad's own tapes can
answer the whole 412 m class of question without a fixture.

## PHASE C — THE BEHAVIOUR. Sequence is now MEASURED, not argued.

The contradiction is settled (§2). C4 is right about cause and **the latch is
innocent of being a bug**: switching it off drops the raid floor 413 → 10 m and
costs **16 → 29 crashes** (0.73 → 1.32/min against Chad's signed **0.94**) while
enemy pressure goes **DOWN** (0.0203 → 0.0192). Therefore:

**C1. Set `speed_target` in the raid and defend branches. Do this FIRST.**
Neither branch sets one, so both inherit the pursuit bump
(`drone/drone.h:2212`, `speed_target = dp.speed + bump_eff`) and loiter at a
measured **156 m/s**, where the minimum turn radius at 64.7° bank is **1173 m**
against `raid_range_m = 1000.0` (`combat/raid.h:40`). At ~123 m/s the radius is
729 m. ⚠ **Do not widen `raid_range_m`** — measured, it buffs the enemy 2.5×.

**C2. The standoff law, BESIDE `aim_at`, never inside it.** Build
`aim_station()` next to `maverick::aim_at` and switch only the three objective
call sites, all re-confirmed at HEAD:
`drone/drone.h:1953` (strike divert), `:2262` (**defend — still never inspected
by anyone**), `:2314` (raid). Leave the four BFM callers alone; Chad's signed
dogfight rides them.
Shape: aim at a standoff station on a cylinder about the objective, radius
`max(0.85 × envelope, 1.3 × turn_radius)`, at attack height; the vertical channel
is **unity-gain feed-forward + small residual** — exactly `bore_track`'s shape
(`drone/maverick.h:713`), the only law in the tree that gets this right; lateral
yields to vertical keyed on `|gamma_cmd|/cap`.
★ It subsumes and retires all four E17/E18 dials.
★ **The measurement says C2 is the only path that can also RAISE pressure.** The
raider is asking for a dive at **9.06× its own gamma cap** into a CFIT net that
must then catch it. Stop it asking; do not relax the net.

**C3. The terrain-avoid latch — DEMOTED, and do it AFTER C2.** Keep the hard
250 m enter-latch (`drone/drone.h:297`); the only thing worth touching is making
the *look-ahead arming* capability-based and scoping it to an active attack
order. The latch clamps bank, forces `avoid_gamma`, and sets `wants_fire = false`
at `drone/drone.h:2539-2546`; the state bool is `drone/drone.h:1085`, written at
`:2536-2538` and cleared at `:2549`, `:2554`, `:1692`.

**C4. The allied wing.** `kDefendersPerFaction = 2`
(`app/instructor_tick.h:61`, consumed at `:1265`) was never scaled when E13 made
the wings 7 and 3, so VALLEY tasks 3 of 3 with **zero float**. Add a reachability
gate to `combat::assign_defense` (`combat/raid.h:239`) in the shape of the
shipped `transit_reach_s_per_km` (0 = OFF) — the two surface pumps are **161°
apart** and defend orders currently point through the planet core.

---

# 2. WHAT PHASE A MEASURED THAT CHANGES THE PLAN

**THE 412 m CONTRADICTION IS SETTLED: C4, and it is not close.**
`[.floor2x2]`, six arms, real-DEM P-H, statistic = enemy raider AGL at every raid
tick inside 2 km of its target pump.

| arm | p10 / **MED** / p90 (n) | Δmed | latch ticks on raid duty | crashes |
|---|---|---|---|---|
| A SHIPPED | 361 / **413** / 462 (16846) | — | **54418 / 240889 = 22.6%** | 16 |
| B latch OFF | −0 / **10** / 1278 | **−404 m** | 0 | 29 |
| C gamma_cap 0.52→1.13 | 352 / **409** / 465 | −4 m | 44829 | 14 |
| D both | −0 / **12** / 1173 | −401 m | 0 | 29 |
| L cap 0.30 (liveness) | 364 / **419** / 463 | +6 m | 38484 | 14 |
| A′ cruise +1e-9 m/s | 357 / **413** / 463 | **0 m** | 53141 | 13 |

Arm A reads **413 m** against Chad's tape-11 **412 m** — the fixture hosts the
phenomenon. A′ breaks bit-identity yet moves the median 0 m: **noise floor = 0**,
so −404 is 404× the floor. The elevation channel *is* saturated (raw
`|k_el·gc_elevation|/cap` peaks at **9.06**, 19468 raid ticks past the cap) — but
unpinning the cap moves the floor **4 m**. **C1 is refuted as the cause of the
floor and remains correct as the cause of the dive.**

**Other findings that move the plan:**
* **A4: the raider comes back on its own at 85 m/s.** Closest approach over
  120 s with both E17 dials OFF: 400 m start **993 → 456 m**; 1500 m start
  **1813 → 478 m**; 4000 m start **3980 → 549 m**. The two "NEVER RETURNED"
  rows were the 115 m/s airframe. The defect the raid attack pattern was built
  for is much smaller than we reported. **Chad's ruling — see §3.**
* **A5: `run_stall_s` is INERT.** In `[.e17sweep]`, T1 (recover_alt only) == T2
  (recover_alt + `run_stall_s` 8.0) == S (SHIPPED), **bit-identical in every
  field**. It ships at 8.0 (`config/scenario.toml:184`). The fixture is now
  *proven* live, so this is a **dead branch or an unreachable condition**, not a
  blind fixture. **An ON, unpaid-for dial. New debt — triage it.**
* **A5: saturation is a standing readout.** `max|gamma_cmd|/cap = 1.00` on every
  arm of every sweep; 141–191 pinned seconds out of ~1500–2300 raid-order
  seconds; `max|alt_err|` 1756 m on every shipped-dial arm.
* **A3/E18 re-tested on the corrected control: E18 is strictly WORSE.** Same
  deep-pump kill (`dead2` 1 both arms), **106 s slower** (260.6 → 366.9 s),
  `run_crashes` 0 both arms, and **`strafe_rounds` 197 → 0, `max_fire_range`
  898.2 → 0 m** — every gun silent, at 695 m closest approach against
  `strike_station_range_m = 2000.0`. It buys 4 fewer total crashes (23 → 19) and
  pays 4 more arena crashes (5 → 9). The audit's E18 headline was measured
  against the leaky control and does not survive.
* **A6: the `[.e18dive]` table was stale and is deleted.** attack_alt 300
  recorded 337/443 m, measured **1460/1417**; attack_alt 500 recorded "arrived,
  still crashed", measured **1317/1271, BOTH SURVIVED** — outcome flipped.
* ⚠ **The E11 air-dive ramp is LIVE in the shipped game.**
  `avoid_air_dive_agl_m` ships **1500.0** (`config/scenario.toml:548`, a
  *required* key) with `avoid_air_dive_gamma = 0.35` by struct default and no
  TOML key. A comment saying "(shipped 0 = OFF)" was false and is corrected —
  **but every other reader of that comment in the tree is unaudited.**
* ⚠ **`config/scenario.toml:936-952` is now WRONG and was deliberately left.**
  It records E18's benefit as `run_crashes 10→0` / "deep pump NEVER DIED →
  DIES", measured against the leaky control. It belongs in whichever commit
  rules on E18, not in an instrument-repair commit. **Fix it in this rung.**

---

# 3. THE RULINGS THAT ARE CHAD'S, NOT YOURS

All three are open and each is answerable in one word.

1. **The raid attack pattern** (`raid_attack_alt_m` / `raid_reattack_m`, still
   OFF). A4 shows the raider re-enters the 1 km envelope from all three start
   states with both dials OFF. **Consider it, or close it as unnecessary?**
2. **E18** (`strike_attack_alt_m`, OFF). Measured strictly worse on the correct
   control. **Retire it outright (dial + fade + the eleven-line
   `scenario.toml:936-952` justification), or keep it dormant at 0.0 with its
   measurement rewritten?**
3. **`raid_dps_frac` (0.65).** Any real geometry fix (C1/C2) multiplies
   on-station efficiency. **Pair it with a walk-back**, or his signed "good easy
   / medium baseline" moves under him with nobody touching a difficulty dial.
   ★ Do not let a geometry fix smuggle in a difficulty change.
4. **What "fair game" means for the snowmobiler.** An AI that can dive on a man
   on a sled is an AI that can dive on *him* at low level. Different game.
5. **E12's red** (§1 item 0): pin the tape-7 arm's heightfield, or re-baseline
   the two rate clauses?

---

# 4. THE LAWS — now paid for EIGHT times

* ★★★ **A CONSTANT (OR A HAND-MAINTAINED LIST) THAT DESCRIBES THE SHIPPED TABLE
  STOPS DESCRIBING IT THE MOMENT THE TABLE MOVES.** Eighth instance this rung:
  the "(shipped 0 = OFF)" comment on `avoid_air_dive_agl_m`, which ships 1500.0.
  Earlier instances: `pre_e2`'s allowlist, the probe written to enforce this law
  using struct defaults itself, the `[.e18dive]` table, the `[.e17raid]` table,
  E12's own recorded 2358/2028 s. **Read the loader. Always.**
  ⚠ Exception: **RaidOrder/DefendOrder gains have NO TOML key** — for those two
  branches the header *is* the shipped table.
* ★★★ **NEW — A FIXTURE-WIDE CHANGE MOVES EVERY ARM THAT SHARES THE FIXTURE,
  INCLUDING THE ONE THAT THINKS IT PINNED THE WORLD.** E12's tape-7 arm pinned
  two dials and named the world in a comment; A1 swapped the heightfield
  underneath it and the arm's stated world became false with nothing going red
  until a rate clause inverted. **A pin that does not name every input is not a
  pin.**
* ★★★ **NEW — VERIFY FROM BOTH DIRECTIONS.** Every mutation-check in this rung
  was required to go RED and then be restored. Two of them (A1's DEM path and
  A1's `use_procedural` flip) found the same guard from opposite sides. A
  verification that cannot fail verified nothing.
* ★★★ **A BIT-IDENTICAL ARM MEANS BLIND FIXTURE *OR* DEAD BRANCH.** Now settled
  in both directions: the liveness arms proved the fixtures live, which
  *converted* `run_stall_s`'s bit-identity into a dead-branch finding.
* ★★★ **A CHANGE THAT MOVES THE CONTROL ARM OF THE PROBE THAT GRADES IT CANNOT
  BE GRADED BY THAT PROBE** — and check whether the *control* is at fault before
  blaming the change. E18 was refused on a false red; corrected, it is refused
  again, for a real reason.
* ★★★ **WRITE THE DIFFERENTIAL BEFORE BELIEVING YOUR MECHANISM STORY.**
* ★★★ **WHEN YOU ADD AN EXIT TO A STATE MACHINE, IT INHERITS EVERY DEFERRAL THE
  OLD EXITS CARRY.**
* ★★ **NEW — THE AUDIT'S OWN EVIDENCE CAN BE BACKWARDS AND THE CONCLUSION STILL
  RIGHT.** `AUDIT_FINDINGS_20260825_synthesis.md:29-32` says all 20 of P-H's
  crashes happen in full air (mean air at death 1.00). Measured at HEAD *before*
  A1: **30 crashes, 27 thin-air, mean air at death 0.12.** That is the *post*-fix
  shape — the report was inverted. The conclusion (a 15299.99 m featureless
  shell cannot host the 11289.9 m wreck radius) was re-derived independently and
  stands. **Re-derive the falsifier; do not inherit it.**
* ★★ **MEASURE THE NOISE FLOOR FIRST.** `[.floor2x2]` did: arm A′ (cruise +1e-9)
  breaks bit-identity and moves the median 0 m, which is what licenses the
  −404 m reading.
* ★★ **DETERMINISM IS THE DIFFERENTIAL TOOL.** Any real change must break
  bit-identity. Three separate times this rung, 17-significant-digit identity
  across a `git stash` proved an instrument observation-only.
* ★★ **GUARD EVERY `normalize()`** — RaidOrder's default target is the origin.

---

# 5. HOUSEKEEPING

* **Do not push.** Main advances on Chad's stick. Commit freely on
  `sandbox/enemy-ai`.
* **Do not touch `assets/`.** Do not work on main. Never touch another
  `D:\seads_sandboxes\*` or `D:\flight_sim2\seads*` worktree — one object store,
  other branches.
* **THE GRAPH, NOT GREP.** `python tools/graph/graph_query.py
  file|symbol|impact|tests-for|who-includes …`. Regenerate the graph in the SAME
  commit as any structural change, then `graph_query.py check` (`layer check
  OK`).
* Relink the fly build (`cmake --build build-play --target seads`) before
  finishing.
* ⚠ **Never relink while a sweep is running** — a failed build silently leaves
  the old binary and it WILL answer you. A hung `ctest` can hold
  `seads_tests.exe` and block every relink. The *"orbit inertia"* hang has not
  recurred in four full gates and is still uninvestigated.
* If you spawn consults: fresh context each, paste the packet's §1–2 game-loop
  block, require a number or a `file:line` per claim plus what would falsify it,
  and forbid builds/relinks if they run in parallel.

## What Chad should find when you are done

Enemy rounds in the air, sparks on his pump, and one number saying how many —
without a single line of trajectory or damage having moved.
