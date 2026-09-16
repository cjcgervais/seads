# THE OVERNIGHT AI AUDIT — SYNTHESIS
### 2026-08-25. Six independent consults + my verification. Read this first.

Packet: `docs/CONSULT_PACKET_20260825_ai_audit.md`. Six reviewers, no shared
context, each given the game loop explicitly and one question. **Every headline
number below I re-measured myself** before writing it down.

---

# ★★★ THE VERDICT

**The enemy AI is a competent pump-killing machine and a non-existent opponent.**
It wins — tape 11, both your pumps in 7:36 while you watched — and every reviewer
that looked at the mission logic agreed it works. But:

> **IT HAS NEVER FIRED A SHOT AT YOU.**
> Tapes 10 and 11: **0 enemy rounds, 0 `wants_fire` samples out of 70,220.**
> Tapes 7/8/9 fired 60/29/13. The current machine fires nothing.

Everything the AI does to you is an unrendered subtraction: pumps die to DPS
credited on a proximity envelope with no tracer, its own aeroplanes die to
terrain with nothing shooting them, and AI-vs-AI kills happen with no rounds in
the air. **You signed a win you could not see happen.**

---

# 1. WHAT CONVERGED — three reviewers, three methods, same place

C2 (forbidden from reading any doc), C6 and C4 arrived independently at the same
core. I verified all of it:

| measurement | value |
|---|---|
| enemy rounds fired, tapes 10 + 11 | **0** (`ef` events) |
| `wants_fire` samples, tapes 10 + 11 | **0 / 70,220** |
| `wants_fire` within 1500 m of a surface pump, tape 11 | **0 / 723** |
| deaths, tapes 7–11 | **104: 68 terrain (65%), 23 player (22%), 13 AI-v-AI (13%)** |
| terrain crashes by mode | **TRANSIT 27**, RUN 20, PATROL 15, DIVE_IN 6 |
| tunnel entries → in-net wrecks | **36 → 20 (56% of everything that enters dies inside)** |
| RUN starts → CLIMB_OUT | **31 → 7 (23% complete the standing mission)** |
| in-net wreck radii | **19 of 20 inside a 67 m band** at the deep-pump radius 11289.9 |
| raider altitude over a surface pump | **median 412 m**, five drones at 402–456 |

★ That last one is the whole shape of the thing: **`avoid_agl_release_m = 400.0`.**
The terrain-avoid latch *is* the raid orbit floor. It arms at ~487 m in a dive,
commands a climb, and **mutes the guns** (`drone/drone.h:2524`). The AI cannot get
down to where a ground target lives, and while it tries, it cannot shoot.

---

# 2. THE ROOT CAUSE — one defect, six instances (C1)

`maverick::aim_at` multiplies a **feed-forward** quantity by a **tracking gain**:

    st.target_gamma = clamp(k_el * gc_elevation(pos, target), ±gamma_cap)

`gc_elevation` already *is* the flight-path angle you must fly to arrive. It is
the answer, not an error. Multiplying it by 3.0 commands a dive three times
steeper than the target is, so it clamps — **past 9.93° of elevation for the raid
and defend branches.**

★ **`bore_track` is the only law in the tree that gets this right** —
`slope_ff_gain * gpath + track_gain * alt_err` at unity gain. It was built for the
bore PIO and never carried back to the four ground-objective callers.

**Six call sites**, of which one was never inspected by anyone:

    RAID          drone.h:2291   saturates past  9.9° elevation
    DEFEND        drone.h:2240   saturates past  9.9°   ★ NEVER LOOKED AT
    STRIKE divert drone.h:1931   saturates past 16.7°
    DIVE_IN       maverick.h     saturates past 18.3°
    TRANSIT       maverick.h     saturates past 10.0°
    BFM ×4        bfm.h          — LEAVE ALONE, the signed dogfight rides it

**And it is a TERMINAL-PHASE defect, which is worse than I wrote.** Measured:
**100% saturated inside 1 km of the pump, 0% beyond 4 km.** My own config comment
says "pinned for the whole sortie" — that is wrong and a reviewer checking it
fleet-wide would have dismissed the finding. Inside the damage envelope the pitch
command carries *zero information about the pump* — it is the constant −29.8°.

## ★★★ AND UNDERNEATH IT, THE FINDING OF THE NIGHT

**The raid envelope is smaller than the aeroplane's turn radius.**

    raid_range_m                    = 1000 m   (combat/raid.h:40)
    measured raid speed             = 156 m/s
    min turn radius at 64.7° bank   = 1173 m

**The raider physically cannot fit inside the objective it is ordered to loiter
in.** No aim-point offset, no gain, no altitude dial can fix a vehicle that does
not fit. And the reason it is at 156 m/s: **the raid branch sets no
`speed_target` at all** — it inherits the pursuit speed bump. *Nobody chose 156
m/s.* At ~123 m/s the radius is 729 m and the envelope becomes flyable.

That is a **one-line** change in a branch that currently makes no speed decision.

---

# 3. THE ALLIES — you are not outnumbered, you are alone (C3)

`raiders_for_team()` scales duty with wing size and carries a nine-line
justification. **`kDefendersPerFaction = 2` is a bare constant that was never
scaled when E13 made the wings 7 and 3.**

    SUDBURY (7):  3 raiders + 2 defenders = 5 tasked,  2 FREE
    VALLEY  (3):  1 raider  + 2 defenders = 3 tasked,  0 FREE

Your wing is 100% conscripted every tick. There is no spare pilot for anything
emergent — which is where "flying beside you" would have come from.

**Allied on-station against the enemy SURFACE pump: 0.0 s in both current tapes**
(5v5 tapes delivered 21.6 and 23.2 s). Against the deep pump, 5.0–5.4 s versus
the enemy's 27.6–50.0 s against yours.

**And the defend order points through the planet.** Your two surface pumps are
**161.2° apart — a 29,882 m chord on a planet 30,488 m across.** A defend order
from the far side aims ~73° below the horizon against a 29.8° gamma cap. C3
measured your sole VALLEY raider holding that impossible order for **27.7% of
tape 11**, at a median 29,425 m from the pump it was "defending", heading error
89.3°, distance *increasing*. It flew neither of its two orders.

Also: **nobody defends deep pumps at all** (`combat/raid.h:237` — "deliberately
out of scope"). In tape 11 the deep pump died **first**.

---

# 4. WHAT THIS MEANS FOR THE SNOWMOBILER AND THE AA GUN (C4, C6, C1, C3)

All four reviewers that were asked reached the same conclusion: **the new canon
cannot be built on this AI as it stands.**

* **The snowmobiler is unattackable.** He sits at 0 m AGL, 412 m below the lowest
  altitude any AI aeroplane holds over a pump, behind a latch that mutes the guns
  on the way down. He also *moves*, so the bank channel pins as well.
* **The AA gun will be handed the easiest target in the game.** The raid
  equilibrium is a *constant-bank, constant-altitude, constant-radius helix* —
  zero jink, zero energy variation. You will not have to lead it.
* **…and it will have almost nothing to shoot at**: raiders are inside 1000 m of
  the surface pump for 18% of a match, in bursts of median 2.6 s, at the outer
  edge.
* **There is no threat concept anywhere** in `drone/` or `combat/` — no threat
  volume, no suppression, no attrition awareness. A drone at 5 HP flies exactly
  like one at 100.
* **The repair loop may make the surface offensive unwinnable.** One on-station
  raider removes 4.33 %HP/s, but the *time-averaged* rate across a whole match is
  **0.09–0.12 %HP/s** — 13–18 minutes per pump. The longest continuous pass ever
  flown was **5.6 s**. Any repair faster than ~13 minutes wins by default. The AI
  needs a **burst-and-commit** behaviour, not a chip.
* **A real fairness bug is waiting**: when you dismount, `assign_foes` still
  tracks your *parked aeroplane*. Up to 3 enemies will hold player-slots against
  an empty plane while you are 500 m away on a sled — and nothing attacks you.

★ **The silver lining, and it is real:** the AI's air-to-air failure is a 5° cone
solution against a 300 m/s manoeuvring target. Against a 20 m/s snowmachine that
discipline should hold trivially. **The ground war may be the first place in this
game where the enemy's guns visibly work.**

---

# 5. ★★★ OUR INSTRUMENTS ARE LYING (C5) — THE MOST IMPORTANT SECTION

**The on-station counter is not stuck. It is SATURATED, and its value is pure
arithmetic off the TOML:**

    enemy on-station  ==  2 × pump_kill_seconds / raid_dps_frac
    0.50 → 60.0    0.65 → 46.2    0.80 → 37.5    1.00 → 30.0     (4 for 4)

It stops accruing when a pump dies (`test_conquest_match.cpp:784`). Every arm
kills both pumps, so it always reads the ceiling. **It is a receipt, not a
measurement.** Every ruling of the form "arm B earned more on-station than arm A"
where both killed both pumps is an identity. The signed "42.0 → 46.2 s"
improvement is a real number compared against a wall.

**Five of six arms in my own E17 sweep are bit-identical** — the sweep advertises
six arms and runs three, because the dials are 0 or never cross threshold.
Over **2,723 s of tunnel duty per match, P-H's in-bore error never once exceeds
300 m**, while your real tapes reach **1,630–2,062 m**. A factor of seven.

**P-H is blind by construction.** `uniform_field(300)` returns exactly 15300.0 m
in every direction. All 20 of its crashes happen in **full air** (mean air at
death 1.00) while 39% of enemy plane-seconds are below 0.25 air. It cannot
represent the deck's frame, the vacuum crash, or the 11289.9 m wreck shell — that
radius does not exist inside it.

**And P-B's control arm is mis-constructed, not the dial.** `pre_e2()` resets a
hand-enumerated allowlist of seventeen fields; `strike_attack_alt_m` is not in it,
so the *control* arm inherits the fix from the TOML. **My decision to ship E18 off
was based on a false red.** The dial is correctly scoped and innocent.

---

# 6. THE ERRORS THE BARRAGE FOUND IN MY OWN WORK

Recorded plainly, because the pattern matters more than the instances.

1. **I broke my own law inside the probe I wrote to enforce it.**
   `test_raid_persist_probe.cpp:104` declares `drone::DroneParams dp;` — struct
   defaults — while its own banner says "the params come from the SHIPPED
   loaders, never a hand-copied table." Default cruise is **115 m/s**; the shipped
   bandit cruises **85 m/s**. Turn radius scales as V², so my probe measured an
   aeroplane turning **1.83× wider than the game's** — and turn radius is the exact
   quantity that probe exists to measure.
2. **My E17 sweep ran three arms while reporting six.**
3. **"33–54 m below the pump radius" was wrong** (−39.8 to +27.2), and I had
   conflated a death-event measurement with a last-sample one.
4. **"7 of 7 across tapes 10 and 11" was far too narrow** — it is **20 wrecks
   across tapes 7 through 11.** A fix scoped to the last two tapes would have been
   scoped wrong.
5. **"Both channels pinned for the whole sortie" was too broad** — 100% inside
   1 km, 0% beyond 4 km.
6. **I shipped E18 off on a false red** (§5).
7. **The `[.e18dive]` table recorded in `drone/drone.h` does not reproduce on the
   current build** — stale numbers in a comment, the very failure this ladder has
   paid for six times.

---

# 7. THE CONTRADICTIONS — where the reviewers disagree

Each was asked for its single highest-value change. **They do not agree**, and the
disagreement is the useful part:

| reviewer | its one change | its logic |
|---|---|---|
| C1 | give raid/defend a `speed_target` (then a standoff-orbit law) | nothing downstream can fix a vehicle that doesn't fit the envelope |
| C4 | replace the terrain-avoid look-ahead latch with a recovery-capability test | the 400 m floor blocks everything else, including all of §2 |
| C2 | move the arena floor / deep pumps so there is room under the target | geometry fix, cannot regress behaviour |
| C3 | a reachability gate on defend orders | recovers 27.7% of your wing from an impossible order |
| C5 | re-point P-H at the real DEM | four dead instruments become live at once |
| C6 | give every point of AI damage a visible source | the only fix that changes nothing you signed |

**C1 vs C4 is a genuine causal disagreement** about the 412 m floor: C4 says the
latch causes it; C1 says the saturated law's equilibrium happens to sit 30 m above
the latch's release. Both are consistent with the data. **They imply different
fixes and it is worth resolving before building either.**

My own read: **C6 and C5 are not really competing with the rest.** C6 changes
nothing you signed and makes the game legible. C5 is a precondition for believing
any future measurement at all. The behavioural three (C1/C4/C2) are one argument
about where to cut.

---

# 8. WHAT I WOULD DO, IN ORDER

1. **Fix the instruments first — nothing else can be believed until then.**
   Re-point P-H at the real DEM (the loader already exists and works); replace the
   saturated on-station counter with a rate; fix `pre_e2`'s allowlist; fix my raid
   probe to read the shipped loader. None of this touches the game.
2. **Make the AI visible** (C6). Cosmetic tracers from the abstract DPS sites and
   hit sparks on the pump. Moves no trajectory, changes no damage, cannot regress
   the difficulty you signed. It is the single biggest experience win available.
3. **Set `speed_target` in the raid and defend branches.** One line. It is the
   only change that attacks the turn-radius impossibility.
4. **Then** the standoff-orbit law beside `aim_at` — *not* a change to `aim_at`
   itself (four BFM modes and your signed dogfight ride it). It subsumes and
   retires all four E17/E18 dials.
5. **Re-test E18 after fixing `pre_e2`** — it may well be shippable, and it makes
   the deep pump fall for the first time.
6. **The allied wing** — scale `kDefendersPerFaction`, add the reachability gate.

---

# 9. THE RULINGS THAT ARE YOURS

1. **The raid trade** — still open, and now clearly optional: the AI already wins.
2. **Difficulty.** Any real geometry fix multiplies on-station efficiency, which
   is 0.84–1.52% today. **Pair it with a walk-back on `raid_dps_frac`** or your
   signed "good easy/medium baseline" moves under you without anyone touching a
   difficulty dial.
3. **What "fair game" means for the snowmobiler.** An AI that can dive on a man
   on a sled is an AI that can dive on you at low level. That is a different game
   from the one you signed, and it is the right moment to decide it deliberately.
