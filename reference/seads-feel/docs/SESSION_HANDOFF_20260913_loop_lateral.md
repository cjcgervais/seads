# SESSION HANDOFF — loop rollover + lateral nose-down (2026-09-13)

Lane: `feel/lean-lead-walkback` in `D:\flight_sim2\seads-feel`.
**LAUNCH LINE: read §0, then §6 (the lessons) before touching anything.**

---

## §0 STATUS

### ★ 2026-09-15 -- CHAD RULED: v16 = THE YAW BUDGET ALONE, unload REMOVED

Chad, verbatim: **"no deck save unload keep the yaw budget"** (option 3 of
§0.0 below). He was told the trade before ruling: the budget-only build is
the tape-6 exe (`c912ecf46`) he had described as *"a deep propensity to
really want to nose down still"*; the unload was what turned that into
*"I like it now"*. He ruled anyway. The tremor debt and the knife-edge
ringing stay parked for the rung AFTER v16 (his word: one signed change per
kernel version, the v14/v15 precedent).

**KERNEL v16 "S-yawbudget" = ONE dial, `[coordination]`:**

| dial | value | what it does |
|---|---|---|
| `yaw_vert_budget` | 1.0, gap `-5..10` deg | caps what the RUDDER asks of the vertical axis (ev6 77% / ev1 19% / ev7 16% / t5worst 9% of the dive removed on his onset fixtures) |

**DONE this session (`b2bc840c5`):** S-unload removed net-zero the v15
way (kernel block, five params, X-macro entry, telemetry field, loader
block, banner line, tape column 161 -> 160, the S-unload test legs incl.
the OPEN loop-cost case); scar comments at every site; the actuator table
stays in `control/params.h` with the unload row marked REMOVED; the loader
now REFUSES any `[coordination] unload_*` key; scrap tag
`scrapped/s-unload-20260915` = `40325f297`. S-yawbudget 8/44 green with the
committed numbers restored exactly (lat-90 41.01 -> 41.85, ev7 -445.5,
ONSET ev6 -439 -> -102) -- this is also the fix for the red-team's late P2
(its doc comments had been contaminated by the unload's presence in the
"budget off" arm). S-righthand hash `dbdf52980174305e` intact; 87 cases /
51918 assertions green on the neighbouring sets. Graph regenerated.

**RED-TEAM 2026-09-15 (fresh context, `docs/REDTEAM_20260915_v16_yawbudget.md`): LAND-WITH-FIX, all folded** -- P1-1 budget floor 1 deg/s (the dial removed the whole rudder wings-level for a 0.2 deg/s dig; fixture numbers moved 3-18 m, re-pinned), P1-2 gap_lo loader wall (-20), P2-5 degree range check, P2-1 the three dead fixtures pinned incl. "the budget does NOT save the deck", P1-3 duty cycle + inertness past 90 recorded in params.h/toml. Its second opinion: my freelook claim was WRONG (freelook welds aim := nose every tick, `app/instructor_tick.h`; his habit is what avoids the fault), acceptance is defensible if written as a statement about this pilot's hand. **ANALYST (`docs/TAPE_ANALYSIS_20260915_normal1.md`, normal-fight tape 17 min): strict slice 0; ONE real fault event at t 197-201 (freelook-exit flick 8.8x his p99, bank -41 -> -175, nose -33 vs aim +29 at onset, 607 m lost / 459 m excess, budget inert at onset, fatal 500 m lower); 30 of 32 dives deliberate; two kernel wing-strikes at 978/992 s from late pulls out of aim-down dives (no respawn).** CHAD after the tape: "I am actually pretty satisfied with the kernel now ... only uncontrolled manouvering will crash you ... I'm really good on this." Folds change the flown artefact (floor) -> one confirmation fly on the merged tip is owed per the SOP.

**IN FLIGHT:** full gate on `b2bc840c5` in the isolated `build-gate/`
(detached; log `build-gate/gate_v16_b2bc840c5.log`, status file beside it).
`build-play/seads.exe` REBUILT on this tip (the 16:55 exe was the
pre-fix unload build -- do not fly it).

**NEXT, in order:** (1) gate reads baseline six by name
(`python tools/gate/gate_baseline.py check build-gate/gate_v16_b2bc840c5.log`);
(2) Chad flies the budget-only exe -- hard lateral pulls at speed, two
loops, one Immelmann, one deliberate split-S, fresh tape
`feel_tape_lateral8_budgetonly.csv`; (3) fresh-context red-team of the
removal + the budget alone (the earlier red-team's P1-3 pins -- ramp-vs-step
per smoothstep band -- are still REQUIRED before landing); (4) land per
the v15 record: LANES verbatim, tag `kernel-v16-yawbudget-signed`, main
fast-forwards, CLAUDE.md kernel line, flight-log row. seads-recon resync is
the SENTINEL's.

**THE UNFLOWN CAVEAT is gone with the dial** (the deliberate split-S was
only a question for the unload's arm). What remains unflown is the
budget-only tree on loops -- the budget is bit-identical on a pure-pitch
pull by construction (gate never opens; pinned), so this is a confirmation
fly, not a discovery fly.

---

### §0-HISTORY (2026-09-13, superseded by the ruling above)


**TARGET 2 (the lateral nose-down) IS FLOWN AND SIGNED.** Chad, after the
second unload flight, verbatim:

> "I think it is okay now, could I be stalling in a sense when I am lower
> speed, like just a natural crash of consequence?"

then, after the stall question was answered with numbers:

> "I like it now, we can commit and push to main and seads before I run out
> of tokens"

**KERNEL v16 "S-lateral" = TWO dials, both `[coordination]`:**

| dial | value | what it does |
|---|---|---|
| `yaw_vert_budget` | 1.0, gap `-5..10` deg | caps what the RUDDER asks of the vertical axis |
| `unload_below_horizon` | 1.0, bank `90..105`, gam `5..20` deg | removes PULL where the lift vector is below the horizon and the flight path is falling below the aim |

App-side: feel-tape v5 contract (161 columns, complete-state seeding),
the launch banner naming every live dial, and six committed onset fixtures.

### ⛔ §0.0 LANDING ON HOLD -- a question for Chad first

**Kernel v16 is NOT on main.** The fresh-context red-team returned
**LAND-WITH-FIX**; the correctness fixes are folded (`b4864512c`), but one
finding changes what Chad signed, so it waits for his word.

**THE QUESTION FOR CHAD:**

> The unload dial **arms during the back half of a pure vertical loop** --
> nose below the horizon, inverted, your aim still ahead. Kernel v15 was
> signed on *"I can do the vertical loops and immelmans without a hitch."*
> Measured on a scripted loop it arms on **25.9%** of the loop and the loop
> finishes **57.6 m higher** than without it. The red-team's own loop
> scenario read **232 m lower** (unload only) / **110 m lower** (both on).
> The Immelmann is completely unaffected (bit-identical, 0% armed).
>
> **Land anyway, narrow the arm so it cannot fire in a loop, or drop the
> unload and keep only the yaw budget?**

**THE NUMBERS BEHIND IT** (scripted, level V250 at 3000 m):

| manoeuvre | both OFF | yaw budget only | unload only | both ON | unload armed |
|---|---|---|---|---|---|
| LOOP dy90 | +369.8 m | +369.8 | **+427.4** | **+427.4** | **25.9%** |
| IMMELMANN dy70 | +567.5 m | +567.5 | +567.5 | +567.5 | 0.0% |
| red-team's loop | -1091 m | -- | **-1323** | **-1201** | 17.3% |

My scripted loop and the red-team's disagree in sign; I have **not**
reproduced theirs and have **not** refuted it. Either way the loop's shape
changes, which is what matters against v15's signature.

**AN ERROR OF MINE IT EXPOSED.** §0.3 below reports the loop protection as
*"verified on his hands, 0 of 723 ticks."* That was **incomplete**: it only
examined nose above +60 deg -- the **climbing** half of a loop. The dial
arms on the **back** half. The Immelmann claim stands.

**WHAT WAS FOLDED** (`b4864512c`):

* **P0 sign guard** -- `if (arm > 0.0 && pitch > 0.0)`: only pull is ever
  removed; a push past 90 deg raises the nose and must never be scaled.
  Measured: guarded vs unguarded is **bit-identical to 6 decimals** on all
  five fixtures, so on these events the defect was **latent, not active**.
  **The deck save holds: AGL 0 -> 80 m.**
* **P1-2** -- the dial-off macro ignored its argument. The *suggested* fix
  itself was undefined behaviour: `ControllerParams& c = (obj)` expands to
  `c = (c)` in `off_arm()`, a reference bound to itself. It moved the v15
  hash to `6055236146bfb3d5` and **segfaulted the suite**. Fixed with a
  collision-proof binding name; hash back to `dbdf52980174305e`.
* **P1-4** -- `yaw_vert_budget` silently dead when `line_hold_ff == 0`;
  the loader now refuses it.
* **P2** -- a comment cited a test file that does not exist; the `gam`
  edges were mislabelled (they are sin-differences); telemetry scales are
  written before the S-rimshot CARRY. All corrected or documented.

> **⚠ FOR THE NEXT RED-TEAM -- read before proposing a macro fix.** The
> P1-2 fix this red-team *suggested* was itself **undefined behaviour**:
>
> ```cpp
> #define SEADS_FEEL_DIALS_OFF(obj) do { control::ControllerParams& c = (obj); ... } while (0)
> ```
>
> `off_arm()` names its object `c`, so that expands to
> `control::ControllerParams& c = (c);` -- a **reference bound to itself**,
> initialised from its own uninitialised value. It compiled cleanly. It wrote
> the dial zeros into garbage memory, left the real object's dials **ON**,
> moved the v15 pre-change hash `dbdf52980174305e -> 6055236146bfb3d5`, and
> **segfaulted the test suite**. The shipped fix binds a name no caller will
> ever use (`seads_feel_dials_obj_`). **Lesson: a macro that introduces a
> local must not reuse a plausible caller-side name, and a proposed fix is
> not exempt from being run.** Verify a suggested patch against the real
> call sites before treating it as correct.

**NOT FOLDED -- REQUIRED BEFORE ANY LANDING (P1-3):** a ramp-vs-step pin
for each smoothstep band, and a **direct non-push split-S leg** proving the
arm stays 0. AT-15 is a weak witness: it spends 208 of 240 ticks in
`push_mode`, where the block is unreachable. Deferred only because the
dial's shape is now Chad's call.

**INCIDENT, resolved:** a local commit was staged with `git add -A` and
swallowed 349 files of the red-team's throwaway `build-rt/`. **It never
reached origin** (the push was stopped; origin still read `9258b8f7a`). Because
origin had never seen it, the four unpushed commits were rewritten from
`9258b8f7a` so `build-rt/` appears in **no** commit on the branch (a backup
ref was held until the clean push verified). `build-rt/` is now ignored
(`14bb76928`) and this lane stages paths explicitly.

**STATE:** lane `feel/lateral-yawbudget`, fixes at `b4864512c` with this handoff on top, pushed. Set 75
cases / 9856 assertions green. main untouched at `b697d24a5`. The exe in
`build-play` (16:55, `9258b8f7a`) is the pre-fix build Chad flew and liked.

---

### §0.1 THE MECHANISM, and why five other actuators failed

The fault is an ELEVATION OVERSHOOT ending in a SLICE. The aim sits ABOVE
the horizon; the nose passes through it and continues 80-108 deg below;
`bank_full` is 95.7-103.4 deg median (peak 179.7), so the LIFT VECTOR IS
BELOW THE HORIZON and **the pull IS the dive**. The pitch channel commands
nose-UP in body frame on 100% of ticks for three of five events -- the
instructor is pulling correctly and the world takes it downward.

**THE ACTUATOR TABLE** (all measured on his own dives, seeded exact):

| actuator | result | why |
|---|---|---|
| bank limit (cap phi mid-event) | **REJECTED** | ev6 no effect; ev7 turn 15.07 -> 7.45 for 0.2 m WORSE |
| far-aim level-turn preference | **REJECTED** | every dive 3-8 m WORSE; phi 50/60/70 identical (cap saturates) |
| top rudder | **REJECTED** | inert (ev7 0.8 m of 606); on 50.0% of ticks the lifting rudder is the OPPOSITE sign to the aim-chasing rudder |
| `K_aoa` 10 -> 12.8 | **REJECTED** | 5-13 m of a 400-600 m descent, does not save the deck event, moves the kernel hash |
| `path_above_aim` (fade the roll) | **REJECTED** | ev7 20.6 m WORSE, ev1 8.6 m WORSE (turn 6.53 -> 3.54) |
| **`unload_below_horizon`** | **SHIPPED** | t6deck AGL 0 -> 80 (survives); t5worst -527 -> -262; ev7 -606 -> -538; ev1 -429 -> -391 |

The first four failed for one reason: **roll cannot raise a nose, and the
rudder is already claimed by the aim.** Only the G demand was left.

### §0.2 THE DISCRIMINATOR

He flies past 90 deg of `bank_full` on **50.7-57.4%** of the hard turns that
HOLD ALTITUDE -- past 90 is his normal cruise-fight attitude, not a fault.
A bank-only gate armed on 71.9% of the V250 lat-90 roll-in: the July
2026-07-06 flat-turn regression. What separates the populations is the
FLIGHT PATH:

    GOOD (tracked, holding altitude)  gamma p50  +19.4 deg   n=10170
    BAD  (inside an overshoot)        gamma p50  -55.1 deg   n=  332

Armed on `(aim_elev - gamma)`: **slices 89.5%, good turns 3.1%** at the 5 deg
edge. NO tau gate -- sustain-gating collapses slice coverage to 13-23%
because the slices are SHORT (dwell p50 0.87 s) and his good past-90 turns
are LONGER (p50 1.22 s); a tau separates them backwards.

### §0.3 RUN 2 (339 s) -- HE IS NOT STALLING

His question answered with numbers:

    AoA filtered p50 5.54  p90 15.38  p99 17.25  max 20.02
    ticks above aoa_max 20.0 :  3 (0.01%)   above plant stall 20.6 : 0
    BALLISTIC ticks : 0        min V on the whole tape : 140.0 m/s
    29 descent events: (a) stall/ballistic 0 | (b) slice 9 | (c) other 20
    slices start at V p50 229.6 (min 216.2) -- his FASTEST flying

Zero stalls, zero ballistic, never below 140 m/s. What he feels at the limit
is the **AoA limiter binding on 11.3% of ticks** -- the protection holding
him just under the cap, which reads as the aeroplane refusing to bite.
**Zero crashes** (lowest AGL 86 m).

**LOOP / IMMELMANN PROTECTION VERIFIED ON HIS HANDS: 0 of 723 ticks armed**
with the nose above +60 deg. (Run 1 could not test this -- he flew no loops.)

⚠ **THE ONE UNCLEARED CASE: a deliberate split-S with the aim held UP.**
409 of 1523 nose-below-60 ticks armed; on those `aim_elev - gamma` is p50
+35.6 deg, i.e. the aim was 36 deg ABOVE the flight path -- those are slices,
not split-Ses. But attitude alone cannot distinguish a committed split-S
flown with the aim still near the horizon, and the dial WOULD fire on it.
Ask him to fly one deliberately and say whether it felt held back.

### §0.4 THE HONEST COSTS

* **t5worst turn 48.92 -> 6.07 deg/s** -- it unloads and stops turning on that
  one event. ev1/ev7 turn FASTER (6.53 -> 14.99, 14.06 -> 16.75), so it is
  event-dependent, but the collapse is real.
* **V250 lat-90 turn 41.85 -> 39.54 (-5.5%)**, G unchanged. The first COST
  regression any surviving dial has had; lat 40 is bit-identical.
* `frac 0.5` is nearly inert on the dives, so **1.0 is the only useful value**
  -- the fallback is 0, not a smaller number.
* **3.1% of his good past-90 ticks** arm and lose G.
* **Past-90 dwell on descent ticks ROSE**: 7.3 / 19.7 / 33.7 / 37.4 / 27.0%
  across tapes 4/5/6/run1/run2. The aeroplane spends LONGER at the slice
  attitude because it unloads there instead of pulling through -- the gauge
  looks worse while the outcome is better. Tell him before he reads it.
* **MB-right co-activation 3.91%** and hand-rest veto 0.56% in run 2 (run 1
  had zero of both). Different axes, no observed conflict, but it is the
  first evidence they can be live together.

### §0.5 LANDING STATUS

Lane `feel/lateral-yawbudget`, pushed. Tips:

| SHA | what |
|---|---|
| `52d1732c5` | feel tape: the emitter named 52 columns while writing 74 |
| `7e951321e` | feel tape: seed the COMPLETE state, compiler-enforced |
| `56ac2022a` | S-yawbudget + compact fixtures |
| `4d79b7eca` | banner names yaw_vert_budget |
| `c912ecf46` | S-yawbudget gate at ONSET (`-5..10`), wall re-derived |
| `1b5d98e83` | **S-unload** |
| `9258b8f7a` | banner names unload_below_horizon |

**Gates:** `gate_arm2b` (`c912ecf46`) = baseline six + three harness smokes
"Not Run" because `seads_harness` was unbuilt in the fresh `build-gate/`;
all targets then built and those three re-run **3/3 PASS**, so that tip is
baseline-six-equivalent. `gate_unload` (`9258b8f7a`) running in `build-gate/`
with every target built -- **its verdict gates the landing.**

---

## §1 CHAD'S RULINGS — VERBATIM. These are the spec.

**THE GUN-DIRECTOR LAW (the governing one for the lateral work):**
> "the plane should follow my mouse; the best way to think is that I am
> directing my guns and an instructor would never crash me into the ground if
> I did not mount my mouse anywhere near there."

> "If I keep inputting upward deflection the airframe should stay in its
> orientation right around the loop... an Immelmann, where I stop inputting
> deflection at the top, auto rights the airframe."

> "the loop is sustained by me sustaining the motion, if I change the motion it
> should change the behavior"

> "direction can be changed 180 degrees with pitch alone that is an immelman"

> a held pull must stay clean for **"as many as I wish"** consecutive loops.

> "when I make a large sideways deflection of my mouse and ask the plane to
> follow it, it noses down crashing me if I am near the deck."

> "I can do the vertical loops and immelmans without a hitch. The sideways
> deflections sent me packing dirt everytime, the upward one still exist too.
> But you got it mostly good work."

**STANDING (2026-07-06, still binding):** fix turn problems via
**coordination/skid, NOT a bank cap**. `maneuver_bank_max` was removed in
`be47e351d` because a cap made turns 44°/1.5 G and he rejected it.
`docs/section7_worklist.md:317` — "a bank cap flattens turns but they go
low-G... skid via coordination, not a bank cap."

**STANDING (2026-08-06):** inverted righting carries no added delay once the
rest condition is met. PRESERVED by `right_hand_rest` — the rest condition now
includes the hand; `inverted_delay`/`inverted_rate` untouched.

---

## §2 S-righthand — what it is and why

**The defect, decomposed from Chad's own tape** (`feel_tape_loop.csv`, 10134
ticks), at his loop apex t=18.02. The roll command jumps **−0.7 → −122.8 °/s
in ONE tick** while the wings move 0.01°:

```
t=18.017  cosPT +0.002  phi -0.80  phi_full   -0.80  rhd   0.25 deg ->   1.2
t=18.025  cosPT -0.002  phi -0.81  phi_full -179.19  rhd 178.64 deg -> 893.2
```

`cos_phi_theta` crosses zero because the **NOSE is past vertical** (theta 89.1),
not because the aeroplane is inverted. `unfold_bank` then reports the bank as
−179.2° instead of −0.8°. Two terms follow:

1. the FINE wings-hold via that 180° flip, faded by `wings_level_gate`
   (123 → 0.6 °/s over 0.13 s) — **DEFERRED, P3-a**, integrated per apex on his
   tape: 8.6 / 1.3 / 6.5 / 6.8 / 0.0 / 0.5 deg, inside the ~10° bound;
2. **MB-right at EXACTLY `inverted_rate`** — `wdz = −180.0 °/s` held through
   the apex. It arms because its "at rest" test is `err < blend_lo` and his
   tracking err there is 4.6°, with `inverted_delay = 0`. He is mid-loop with
   the nose 86° up and the mouse still moving.

**The dial:** MB-right's roll AUTHORITY scaled by
`smoothstep(0, right_hand_rest, hand_rest)`. Continuous, no latch.

**Measured on his flight** (tape 2, flown at 0.25): MB-right wanted to fire on
2344 ticks; the veto suppressed **2178 (92.9%)**. At the six apices, wings level
at every one (max |phi| **9.4°**) against 180.0 °/s un-vetoed.

**⚠ COVERAGE:** the broad gate gives this veto ~ZERO coverage — harness
`ClosedLoop::aim_moved` defaults FALSE, so every suite scenario flies a hand at
rest and the gate reads 1.0. **The apex probe in `test_loop_rollover.cpp` is
the sole instrument.** A green gate says nothing about this dial.

**DEBT (deferred, in `params.h`):** the TREMOR case — "hand is live" is ANY
nonzero aim motion, so a ±1-count/frame tremor while belly-up caps the gate
(integrated righting 1.76° vs 117.75° with a still hand). Cure = windowed NET
aim displacement; a mechanism change, not a dial.

---

## §3 THE LATERAL NOSE-DOWN — mechanism found, dial PARKED

### 3.1 It is not v14 and not the lap guard

`lean_lead` 0.3 vs 0.0 changes altitude by **≤3 m** on every lateral sweep, and
the 0.0 arm reproduces the pre-v14 baseline (`c75bc502d`) **to the metre**:

| lat °/s | lean_lead 0.3 | lean_lead 0.0 | pre-v14 |
|---|---|---|---|
| 150 | −566.2 m | −566.7 | **−566.7** |
| 300 | −67.8 | −66.0 | **−66.0** |

### 3.2 The mechanism, from his real trajectory (`feel_tape_lateral2.csv`)

Worst event t=49.50: **dAlt −448 m**, gap 72°, phi 68°, **V 181 → 181 (NO
BLEED)**, n_pk 15.5, push 0%.

```
t=49.48  n=10.2 V=181 phi=-17.8 theta= -8.5 gap= -7.3 blend=1.00 wdx=+46.8 r_man=103.2
t=50.98  n=12.8 V=183 phi=-60.7 theta=-26.0 gap=-20.4 blend=1.00 wdx=+38.5 r_man= 12.4
t=51.98  n=13.9 V=187 phi=-46.7 theta=-39.3 gap=-30.4 blend=1.00 wdx=+36.1 r_man= 25.7
```

**No term pitches the nose away from the aim.** `wdx` is a sustained
**+34…+47 °/s nose-UP** command throughout; `push_mode` 0%; `roll_hold` and
`roll_right` both 0.0. The nose falls because at 55–66° of bank that demand
buys only `cos(phi)` ≈ 40–57% of world-vertical rate.

### 3.3 THE CLAMP — why no pitch-side dial can fix it

Read from the live controller on his seeded dives:

| | t=47.983 | t=184.25 | t=144.5 |
|---|---|---|---|
| AoA peak | 19.0° | 19.2° | 18.4° |
| **AoA pushback binds** | **112/720** | **333/720** | **187/720** |
| G ceiling binds | 0/720 | 0/720 | 3/720 |

```
t=50.48  AoA 17.4  w_max 98.2  pitch_ceil 26.0  wdx 25.9   <- pinned to the ceiling
```

`wdx` sits **exactly on `pitch_ceil` = `K_aoa·(aoa_max − α_f)`** while the G
budget `w_max` is 3–4× higher. There is **no `q_max` pitch-rate cap** in the
config; the ceiling is AoA.

**This is why `pitch_bank_comp` (candidate a) was inert:** it scales the demand
*before* the clamp, and the AoA pushback truncates it straight back. Multiplying
a number the next line overwrites changes nothing. **(a) is buried.**
**(c), a V-aware G ceiling, is dropped** — there is no bleed to prevent (V is
steady or rising on every worst event).
**(b), `bank_yaw_trade`, is dead on arithmetic:** at ω = 26 °/s, V = 200,
`a_lat` = 90.8 m/s²; pure bank needs φ = 83.8° (n = 9.3); cutting to n = 6 needs
**3.35 g of fuselage side force**, which does not exist. Skid buys ~`Y/W`.

### 3.4 THE REMAINING LEVER — needs Chad's ruling, DO NOT BUILD

With pitch AoA-limited and G unbinding, the only lever is **the bank the turn
asks for at a given aim** (`bank_error` sizing). The trade to put to him:

- today a lateral aim commands a bank the pitch channel cannot support at its
  AoA margin, so the nose leaves the aim and the aeroplane descends;
- bank only as much as the pitch channel can still hold the aim's elevation at
  the current AoA margin — **aim-conditioned**, not a fixed ceiling;
- hard turns stay hard when the nose can follow; it softens **only** when the
  alternative is losing the aim into the ground, which is the gun-director law.

This is close to the `maneuver_bank_max` he rejected. The difference — fixed
ceiling vs aim-conditioned — is the whole question, and it is **his** call.

**THE INVARIANT to grade any fix** (measured on his tape): whenever
`|aim_world_elev| < 45°` and `err > blend_lo`, require
`|nose_world_elev − aim_world_elev| ≤ 30°`. **19 of 61 events violate it
today**, worst **100.3°** (at only 14° of bank).

---

## §4 THE FEEL TAPE + REPLAY — the instrument that settled all of this

**Recipe.** App-side, the `step_frame` `TickHook`; zero cost when unset.

```powershell
$env:SEADS_FEEL_TAPE="D:\flight_sim2\seads-feel\build-play\feel_tape_X.csv"; & "D:\flight_sim2\seads-feel\build-play\seads.exe"
```
fly, quit, then:
```powershell
$env:SEADS_FEEL_TAPE="...\feel_tape_X.csv"; .\build\seads_tests.exe "S-righthand: replay a recorded feel tape"
```

Every launch also appends the dials to `build-play\seads_launch.log` (Explorer
launches have no stderr — Chad: *"it just opens the game no config lines"*).
stderr, the launch log and the tape header come from ONE string, so a flight can
never be attributed to dials it did not fly.

**⚠ TAPE VERSION STATUS.** v5 (aim-frame quaternion + `Internal` latches +
`aoa_ceil`/`w_max_p`) is IN THE TREE BUT NOT YET SUFFICIENT. The round-trip test
(`test_tape_roundtrip.cpp` — flies a banked turn, re-seeds mid-window from the
v5 columns alone, any divergence is a missing column by construction) reads:

```
seed tick 300 (phi 87.6) -> 7.50 s: |dphi| 1.083 deg  |dtheta| 0.497  |dalt| 2.549 m
seed tick 600 (phi 15.9) -> 5.00 s: |dphi| 1.580 deg  |dtheta| 1.314  |dalt| 1.798 m
seed tick 900 (phi 64.3) -> 2.50 s: |dphi| 0.774 deg  |dtheta| 0.531  |dalt| 0.408 m
```

That is ~100× better than v4 (which diverged **~100° of roll** in 6 s) but not
the < 0.01° needed. **Still missing:** the S-rimshot capture block (`cap_ux`,
`cap_uy`, `cap_w_hold`, `cap_crossed`, `cap_err0`, `cap_d_allow`,
`cap_stall_ticks`, `cap_refractory`, `cap_inbound`, `cap_rim_t`), `ovr_ramp`,
`aim_rate_filt`, `any_override`, `push_mode`. A half-seeded `capture` is the
likeliest single culprit. **Finish against the round-trip; it needs no flight
from Chad.**

---

## §5 THE SCARS — do not rebuild these

- **`maneuver_invert_band`** (landed `31d109915`, walked back `157245d84`,
  removed): a `cos_phi_theta` fade on the maneuver roll limb is a SELF-LOCKING
  WALL — reaching inverted REQUIRES rolling through `cos == 0` and that limb is
  the only roll that does it, so the airframe parks at the knife-edge. Broke
  AT-15, the mouse-DOWN split-S and the capture jink.
- **`lap_roll_frac`** (removed `37d5f5d83`): the aim-laps-the-nose guard fired
  on **0 of 10134 ticks** of Chad's flying. Its premise (`target_body.z > 0`)
  occurs on zero ticks of his hand. Cost 272 m on a 300 °/s lateral sweep — a
  crash mode that did not exist before it.
- **`maneuver_bank_max`** (removed 2026-07-06 by Chad): see §1.

---

## §6 LESSONS — read these before the next rung

1. **TAPE THE PILOT FIRST.** Both removed mechanisms were built and validated
   against scripted sweeps **2–8× faster than Chad's actual hand** (his loop
   pull is 22 °/s median, p90 58; the probes used 90 and 180). The first tape
   falsified the entire premise in one reading: `lap` fired 0 times,
   `target_body.z > 0` never happened. **Do not build a feel mechanism before
   a tape of the manoeuvre exists.**
2. **A NAME-FILTERED `ctest -R` IS NOT A GATE.** `ctest -R "...|cascade|
   acceptance"` filters TEST NAMES — `AT-15: …` and `roll_target_mix: …`
   contain neither word, so the subset skipped exactly the tests that caught a
   4-new-red regression, and it was reported green. **Run
   `tools/gate/gate_baseline.py check`, or name the tests.**
3. **SCRIPTED PROBES MUST MATCH HIS HAND.** A probe that flies a different
   manoeuvre grades nothing: the first S-righthand probe reached `p_max` via
   the maneuver limb where his tape shows MB-right owning the apex at −180 —
   ON and OFF hashed IDENTICAL. Craft the state at his MEASURED condition.
4. **A test file's tail can be silently truncated** by a `s[:a] + new` rewrite;
   the fast set then passes because the legs are gone. Count `TEST_CASE`s after
   any scripted edit.
5. **Python edits rewrite LF files as CRLF** (`app/main.cpp` is LF; the rest of
   this checkout is CRLF). Normalise before committing or the diff is 24k lines.
6. **Gate a `build-play` rebuild on `tasklist` IN ITS OWN STEP** — twice this
   session a rebuild ran while a session was open because the check and the
   build shared a command.

7. **§6.7 — THE VOID BATTERY. A reader that defaults is a reader that lies.**
   This one cost a night, so it gets the full anatomy.

   Three defects composed:

   - `app/main.cpp`'s row writer grew to **74** columns (v4's per-tick seed,
     then v5's aim quaternion and `control::Internal`), but its header string
     was hand-written and stopped at **52**. The data was on disk, named
     nowhere.
   - The probe's CSV reader resolved columns BY NAME and returned **`0.0`**
     for any name it could not find — no warning, no error.
   - So seeding "Chad's 3 worst dives" silently produced `angular_vel = 0`, an
     aim quaternion of `(0,0,0,0)`, and a freshly reset controller. The
     aeroplane was not diving. Both arms of the counterfactual followed the
     same non-dive and the dial measured **ON == OFF** — which was then
     reported as evidence about the dial.

   Why nothing caught it: `test_tape_roundtrip.cpp` round-tripped an
   **in-memory `struct Snap` that the test itself owned**, and the writer it
   was supposedly proving lived in `main.cpp`, which is not linked into the
   tests. Its comment `// EXACTLY the v5 tape columns` was an assertion, not a
   check. Commit `62ceaeff3`'s "proved by a round-trip test" was therefore
   true of the struct and false of the tape.

   The same class bit a second time in the same file set:
   `test_loop_rollover.cpp`'s reader was POSITIONAL with v2-era offsets
   (`alt = v[29]`). Once the row reached 74 columns that index is `hand_rest`,
   so the bench was grading altitude against the hand-rest timer.

   **The rules that follow, now enforced in code:**
   - A reader **NEVER** defaults a missing column. `harness::FeelTape::load`
     takes an explicit required-name list and **throws**, naming every missing
     column and both column counts.
   - The writer never spells a header and the reader never spells an index.
     Both derive from `app::kFeelTapeColumns` (`app/feel_tape_columns.h`).
   - A format proof must run **the shipped writer**. If the writer is
     unreachable from the tests, MOVE IT (`app/feel_tape.h`) rather than
     simulate it. The end-to-end case asserts header-count == row-width, which
     is the exact divergence that was invisible for a night.
   - **Check the sign against the source, not against a remembered table.**
     `elev_gap = theta - aim_elev` = nose − aim, so NEGATIVE is nose-below-aim.
     A whole gate was reasoned about with this inverted.

   The general lesson, which is the expensive one: **the battery agreed with
   itself at every step.** The tests passed, the probe ran, the numbers were
   plausible and internally consistent. Nothing was wrong except that the
   instrument was not connected to the aeroplane. When a counterfactual
   reports "no difference", suspect the harness BEFORE believing the dial —
   and prove the seed reproduces the recorded truth before reading anything
   downstream of it.

---

## §7 WHAT TO DO NEXT

1. Finish the tape v5 column set against `test_tape_roundtrip.cpp` until all
   three seeds read < 0.01°. No flight needed.
2. Put §3.4 (aim-conditioned `bank_error` sizing) to Chad as ONE question with
   the trade named. Do not build it first.
3. Chad's land word on S-righthand (`3d6de7ec1`), then push the lane.
4. Deferred, with numbers: P3-a (§2), the TREMOR debt (§2), the 40 °/s V140
   slow-pull rollover (untouched by any dial, pinned ON==OFF so it stays
   visible).
