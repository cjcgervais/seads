# ENEMY-AI — THE REPAIR RUNG. HANDOFF 2026-08-26

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260826_enemy_ai_REPAIR.md; take the
next enemy-AI rung."**

**READ FIRST, IN THIS ORDER:**
1. `docs/AUDIT_FINDINGS_20260825_synthesis.md` — the six-consult audit. **This
   handoff assumes you have read it and does not repeat its evidence.**
2. `docs/ENEMY_AI_E1_E2_SPEC.md` — the binding ledger.
3. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the new
   millwright/AA canon. **Every consult you spawn gets §1–2 pasted in.**

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`. Gate
**1540/1545**, the 5 reds are the documented baseline (4 GI4 sled debts +
`probe P-F` clause (2), red on purpose — **do not bend P-F a third time**).
Commits `cc0c38626 → 2d89a3db3`, **none pushed.**

---

# ★★★ THE ONE THING TO UNDERSTAND BEFORE YOU TOUCH ANYTHING

**The audit found that our instruments were measuring arithmetic, not the AI.**

* `enemy_onstation_s` is **saturated**, not stuck: it equals exactly
  `2 × pump_kill_seconds / raid_dps_frac` (verified 4/4 across a sweep). It stops
  accruing when a pump dies and every arm kills both pumps. **It is a receipt.**
* **Five of six arms in the E17 sweep are bit-identical.** The sweep advertised
  six and ran three.
* **All 20 of P-H's crashes happen in full air** (`mean air at death 1.00`) while
  39% of enemy plane-seconds are below 0.25 air. Its `uniform_field(300)` returns
  exactly 15300.0 m in every direction — it cannot represent the deck's frame,
  the vacuum crash, or the 11289.9 m wreck shell.
* **P-B's control arm is mis-constructed** and E18 was shipped OFF on a false red.

★ **THEREFORE: FIX THE INSTRUMENTS BEFORE YOU CHANGE ONE LINE OF AI BEHAVIOUR.**
Any behavioural result measured on today's fixtures is uninterpretable. This is
not caution — it is the finding.

---

# 1. THE WORK, IN ORDER. DO NOT REORDER WITHOUT A REASON.

## PHASE A — REPAIR THE INSTRUMENTS (no game behaviour changes at all)

Each of these is small, verified-located, and independently committable.

**A1. Re-point P-H at the real DEM.**
`test/unit/test_conquest_match.cpp:316` — `w.hf = uniform_field(300.0);`
The donor already exists and links in this binary:
`test/unit/test_tunnel_track_probe.cpp:93` `load_real_dem()` (relief_scale 350.0,
u_offset 0.806, stb_image resolved against `test_tunnel_mesh.cpp`).
★ **Do this as its own commit with nothing else moving.** This fixture has
already been caught twice flying a world the game does not ship; changing the
world and a dial together makes the next A/B unattributable.
⚠ Expect the tape-7 calibration legs to move (they are labelled INFORMATIONAL);
check, do not assume. Expect DEM decode cost per arm.

**A2. Replace the saturated on-station counter.**
`test/unit/test_conquest_match.cpp:784` — `if (!tgt.alive) continue;`
Either accrue unconditionally into a second counter, or report
`enemy_onstation_s / player_pump_alive_s`. **Both are pressure; the current
field is a receipt.** Then re-read every ruling that cited on-station seconds —
the signed "42.0 → 46.2 s" compares a real number against a ceiling.

**A3. Fix `pre_e2`'s allowlist.**
`test/unit/test_stope_probe.cpp:131` — it enumerates seventeen E2 fields and
copies `shipped` first, so any NEW dial leaks into the CONTROL arm. Invert it:
start from `DroneParams{}` and copy the non-E2 fields forward. Used by P-B
(`:503`) and P-C (`:790`).
★ **Then re-test E18** (`strike_attack_alt_m = 300.0`). It may well be
shippable — it made the deep pump die for the first time (`run_crashes` 10→0,
`dead2` 0→1) and was refused on a false red.

**A4. Fix my raid probe's airframe.**
`test/unit/test_raid_persist_probe.cpp:104` — `drone::DroneParams dp;` uses
**struct defaults (115 m/s)** while the shipped bandit cruises **85 m/s**
(`config/scenario.toml:26`). Turn radius goes as V², so it measures an aeroplane
turning **1.83× wider than the game's** — and turn radius is the exact quantity
that probe exists to measure. Use `kScen.drone`. Its own banner claims it already
does this; **the banner is the bug report.**

**A5. Add a LIVENESS ARM to every sweep**, and print `max |alt_err|` /
`max |gamma_cmd|/cap` from inside P-H. A bit-identical arm means *blind fixture
OR dead branch* and you cannot tell which without one.

**A6. Delete or fix the stale recorded table** at `drone/drone.h` in the E18
block — its `[.e18dive]` numbers (337/443 m) do not reproduce on the current
build (measured 1460/1417). A comment that describes a measurement stops
describing it the moment the build moves.

## PHASE B — MAKE THE AI VISIBLE (changes nothing Chad signed)

**B1. Give every point of AI damage a visible source.**
`app/instructor_tick.h:62-65` already *promises* this — "real tracers fly for the
look; only the player is swept against rounds" — and it is **false in practice**,
because `wants_fire` is never true (0 of 70,220 samples in tapes 10+11).
Sites: the AI-vs-AI credit at `app/instructor_tick.h:1663` (`ai_guns_on`) and the
pump credit at `:1889` (`damage_pump`), plus per-tick `HitSpark` on the pump for
AI credit, mirroring the player-only path.
⚠ **Cosmetic pool ONLY** — must not be swept against the player, or the signed
difficulty moves. Log it as cosmetic and keep the real gun defect (§C) open
behind it.

★ This is the highest experience-value change available and it cannot regress
the balance. Chad signed a win he could not see happen.

## PHASE C — THE BEHAVIOUR, once A is done and B has landed

**C1. Set `speed_target` in the raid and defend branches.** One line each.
Today neither sets one, so both inherit the pursuit bump and loiter at a measured
**156 m/s**, where the minimum turn radius at 64.7° bank is **1173 m** — against
`raid_range_m = 1000.0` (`combat/raid.h:40`). **The raider cannot fit inside the
objective it is ordered to loiter in.** Nothing downstream can fix that. At
~123 m/s the radius is 729 m.
⚠ Do **not** widen `raid_range_m` to fix it — measured, that buffs the enemy
2.5× and the ally by 6.8 s.

**C2. The standoff law, BESIDE `aim_at` — never inside it.**
Four BFM modes and the flown, signed dogfight ride `aim_at`. Build
`aim_station()` next to it and switch only the three objective call sites:
`drone/drone.h:1931` (strike), `:2240` (**defend — never inspected by anyone**),
`:2292` (raid).
Shape (from C1's consult): aim at a **standoff station** on a cylinder about the
objective, radius `max(0.85 × envelope, 1.3 × turn_radius)`, at attack height;
vertical channel is **unity-gain feed-forward + small residual** — exactly
`bore_track`'s shape, which is the only law in the tree that gets this right;
and lateral yields to vertical keyed on **`|gamma_cmd|/cap`**, not on a proxy.
★ It subsumes and **retires all four E17/E18 dials**.

**C3. The terrain-avoid latch.** Raiders hold a **median 412 m** over a surface
pump against `avoid_agl_release_m = 400.0`, and the latch **mutes the guns**
(`drone/drone.h:2524`). Nothing in the millwright/AA canon can be built on an
airframe forbidden to descend with live guns.
⚠ **RESOLVE THE CONTRADICTION FIRST** (§3) — C4 says the latch *causes* the
floor; C1 says the saturated law's equilibrium merely *sits above* it. They imply
different fixes. Settle it with a measurement before building either.
⚠ This is the CFIT net; Chad's signed budget is 0.94 crashes/min (currently
0.80). Keep the hard 250 m enter-latch; make only the *look-ahead arming*
capability-based; scope it to an active attack order.

**C4. The allied wing.** `kDefendersPerFaction = 2` (`app/instructor_tick.h:61`)
was never scaled when E13 made the wings 7 and 3, so VALLEY tasks **3 of 3 with
zero float** while SUDBURY tasks 5 of 7. Add a **reachability gate** to
`assign_defense` in the shape of the shipped `transit_reach_s_per_km` (0 = OFF) —
your two surface pumps are **161° apart**, so defend orders currently point
through the planet core.

---

# 2. THE RULINGS THAT ARE CHAD'S, NOT YOURS

1. **The raid attack pattern** (`raid_attack_alt_m` / `raid_reattack_m`) — still
   OFF, now clearly optional: the AI already wins without it.
2. **`raid_dps_frac`.** Any real geometry fix multiplies on-station efficiency
   (0.84–1.52% today). **Pair it with a walk-back** or his signed "good easy /
   medium baseline" moves under him with nobody touching a difficulty dial.
   ★ Do not let a geometry fix smuggle in a difficulty change.
3. **What "fair game" means for the snowmobiler.** An AI that can dive on a man
   on a sled is an AI that can dive on *him* at low level. Different game.

---

# 3. THE CONTRADICTION TO RESOLVE BEFORE BUILDING

The six reviewers named six different "one changes". Five are complementary. One
is a genuine causal disagreement:

> **What makes the 412 m raid floor?**
> **C4:** the terrain-avoid look-ahead latch arms at ~487 m in a dive and holds
> them there. **C1:** the saturated pursuit law's own equilibrium happens to
> settle 30 m above the latch's release threshold, and the latch never fires.

Both fit the data. **They imply different fixes**, and building the wrong one
wastes a rung. Settle it cheaply: instrument whether the avoid latch actually
engages during raid ticks (the tape's `fs` flag, or a counter in P-H). If it
never engages, C1 is right and the latch is innocent.

---

# 4. THE LAWS — now paid for SEVEN times

* ★★★ **A CONSTANT THAT DESCRIBES THE SHIPPED TABLE STOPS DESCRIBING IT THE
  MOMENT THE TABLE MOVES.** Newest instances: `pre_e2`'s allowlist (a *list* is
  the same trap as a constant), and **the probe I wrote to enforce this law used
  struct defaults itself** (A4). Read the loader. Always.
  ⚠ Exception worth knowing: **RaidOrder/DefendOrder gains have NO TOML key** —
  for those two branches the header *is* the shipped table. Check before quoting.
* ★★★ **A BIT-IDENTICAL ARM MEANS BLIND FIXTURE *OR* DEAD BRANCH.** Add a
  liveness arm to tell them apart.
* ★★★ **A CHANGE THAT MOVES THE CONTROL ARM OF THE PROBE THAT GRADES IT CANNOT
  BE GRADED BY THAT PROBE** — and check whether the *control* is at fault before
  blaming the change. E18 was refused on a false red.
* ★★★ **WRITE THE DIFFERENTIAL BEFORE BELIEVING YOUR MECHANISM STORY.**
* ★★★ **WHEN YOU ADD AN EXIT TO A STATE MACHINE, IT INHERITS EVERY DEFERRAL THE
  OLD EXITS CARRY.**
* ★★ **MEASURE THE NOISE FLOOR FIRST.** `[.e18dive]`'s crash/survive column flips
  on **9 m** of clearance. Repeat-run noise is exactly 0 (the sim is fully
  deterministic) — the floor that bites is sensitivity to irrelevant
  perturbation, and it is large.
* ★★ **DETERMINISM IS THE DIFFERENTIAL TOOL.** Tapes 10 and 11 share 43.6% of
  drone samples bit-for-bit. **Any real change must break that identity** — if it
  does not, the edit is inert and the mechanism story is wrong.
* ★★ **GUARD EVERY `normalize()`** — RaidOrder's default target is the origin.

---

# 5. HOUSEKEEPING

* **Do not push.** Main advances on Chad's stick. Commit freely on
  `sandbox/enemy-ai`.
* **Do not touch `assets/`.**
* Regenerate the graph in the SAME commit as any structural change
  (`tools/graph/graphify.py`, then `graph_query.py check`).
* Relink the fly build (`cmake --build build-play --target seads`) before
  finishing so Chad can fly whatever you leave.
* ⚠ **Never relink while a sweep is running** — a failed build silently leaves
  the old binary and it WILL answer you. And a hung `ctest` can hold
  `seads_tests.exe` and block every relink: the test *"orbit inertia:
  back-to-back grabs blend by the EMA factor"* hung 80 minutes on 2026-08-24 and
  **nobody has investigated why.**
* If you spawn consults: fresh context each, paste the packet's §1–2 game-loop
  block, require a number or a `file:line` per claim plus what would falsify it,
  and forbid builds/relinks if they run in parallel.

## What Chad should find when you are done

A short, blunt note saying which instruments are now trustworthy, what changed in
the AI, what it measured **on a fixture that can see it**, and what is still off
with its price written down.
