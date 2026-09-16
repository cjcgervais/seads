# RED-TEAM — kernel v16 "S-yawbudget" (fresh context, 2026-09-15)

Lane `feel/lateral-yawbudget`, tip `a605a792b` (tree clean before and after; every mutation
below was reverted with `git checkout --` and the last build is the unmutated tree).
Build: `build-rt/` (Debug, Ninja, raylib from `build/_deps/raylib-src`). Nothing in
`build-gate/` or `build-play/` was touched. Commits under review: `b2bc840c5` (S-unload removed),
`18cc020ce` (docs), `a605a792b` (P1-3 pins + the 1e-6 floor).

## VERDICT: LAND-WITH-FIX

No P0. Three P1s: one is the dial's shape (a whole-rudder kill at near-zero bank, live on his
own tape and misdescribed by the P1-3 pin), one is a missing loader wall that lets a hand-edited
toml reproduce the documented collapse, one is the record (the dial's real duty cycle and its
structural inertness past 90 deg are nowhere). The S-unload removal is net-zero (verified). The
off arm is v15 by construction (verified by diff). Every mutation the tests claim to kill, they
kill (verified, two run live plus three of my own).

What I ran on the unmutated tree: `S-yawbudget*` 10 cases / 64 assertions green with the
committed numbers exactly (lat-90 41.01 -> 41.85, ev6 -186.5 -> -124.4, ev7 -490.2 -> -445.5,
ONSET ev6 -439.2 -> -102.1, ev7 -719.2 -> -606.3, ev1 -530.2 -> -428.9, ramp 337/439 armed 35
partial, split-S 61/480, loop 0/480); `S-righthand: the off arm` hash `dbdf52980174305e` green;
`S-righthand*`,`S-straightline*`,`AT-15*`,`feel tape*`,`TAPE*`,`tape*` 45 cases green.

---

## PART A — CODE

### P1-1  The dial kills the ENTIRE rudder at near-zero bank; the P1-3 split-S pin describes a different tick than the 54 it arms on
`control/controller.cpp:947-967`, `test/unit/test_yawbudget.cpp:590-650` (the split-S leg).

Mechanism. `scale = 1 - gate*(1 - budget/dig)` with `dig = yaw*sin(phi)`. Where the elevator is
clipped `budget == 0` exactly (params.h calls this "a BINARY kill"), so the scale is `1 - gate`
for ANY dig above the 1e-6 threshold — including a dig that is negligible because `sin(phi)` is
negligible. The rudder is then removed whole for no vertical benefit, and as `phi` crosses zero
the yaw command steps by `gate*yaw` (dig < 0 on one side of the crossing, > 0 on the other).

Measured (probe printf on every armed tick, then reverted):

| leg | armed | budget==0 | dig<1 deg/s AND killed | yaw removed / dig removed | worst-trade tick |
|---|---|---|---|---|---|
| P1-3 split-S | 61 | 54 | **54** | **5.74** | yaw 29.5 -> 0 for dig 0.24 deg/s at phi -0.5 |
| ONSET ev7 | 431 | 431 | 3 (19 at \|sin phi\|<0.2) | 1.26 | yaw 45.2 -> 0 for dig 0.05 deg/s at phi -0.1 |
| ev7 | 177 | 177 | 0 | 1.36 | |
| ev6 / ONSET ev6 | 231 / 337 | all | 0 | 1.06 / 1.03 | |
| COST lat-90 | 247 | 233 | 0 | 1.01 | |

His own flight (tape 6, `feel_tape_lateral6_gate.csv`, budget 1.0 live, 296 s): 4783 ticks with
scale < 0.5, of which **475 at |sin phi| < 0.2 (420 at |bank| < 10 deg)**, 26 runs, longest
**767 ms**, and **9 phi sign-crossings landing on a killed tick**. Bounded in his flying only
because a small bank goes with a small lateral aim (the long runs carry 0.8–9 deg/s of yaw); it
is unbounded in the astern pull-through, which is exactly the split-S leg (27–30 deg/s killed).

The commit message and the test comment say the leg arms because "its roll-through banks 44 deg
with the nose 45 deg below an aft aim and the rudder digging -5 deg/s". That is the FIRST armed
tick. 54 of the 61 are wings-level with a 0.2 deg/s dig. The pin `armed*5 <= ticks` (20%) passes
at 12.7% and would pass this shape by design; the record misdescribes the artefact.

**Fix (kernel, continuous, replaces the 1e-6 threshold):**
```cpp
const double budget = std::max(cp.yaw_vert_budget * avail, kBudgetFloor);  // kBudgetFloor = 1 deg/s = 0.0174533
if (-yv_dig > budget) { ... }                                                // no separate 1e-6 test
```
Continuous at `dig == floor` (scale -> 1), covers the roundoff case (1e-17 << floor). Measured
live (MUT3): split-S armed **61 -> 7**, sc_min 0.029, dAlt -580.9; loop still 0/480
bit-identical; lat-40 bit-identical; lat-90 41.85 -> 41.80, yaw -24.74 -> -25.05; the dive
recoveries cost 3–18 m: ev6 -124.4 -> -127.7, ev7 -445.5 -> -449.6, ONSET ev6 -102.1 -> -120.6,
ONSET ev7 -606.3 -> -614.9, ev1 -428.9 -> -437.9. Honest consequences: the committed fixture
numbers move and must be re-measured; the ramp pin breaks (P2-2); and it changes the flown
artefact in the zero-bank window (toward v15). Then pin: no armed tick with |sin phi| < 0.2 on
the split-S leg.

**Minimum if the lane will not re-fly:** keep the kernel, rewrite the P1-3 comment truthfully,
pin the actual shape (armed at |phi| < 0.5 deg == 54, |dAlt on-off| < 5 m), and carry the
tape-6 numbers above in `params.h`. The record must not say what the probe refutes.

### P1-2  `yaw_vert_gap_lo` has no lower wall; a hand-edited toml reproduces the documented collapse and loads clean
`config/load_controller.cpp:263-291` (the wall guards `gap_hi` only; the old `gap_lo` wall was
dropped on the measurement that -5 is fine).

Measured live (temporary test through `lateral_run`, V250, 480 ticks, then reverted):

| gap_lo (deg) | lat-90 turn OFF -> ON | lat-90 yaw OFF -> ON | lat-40 |
|---|---|---|---|
| -5 (shipped) | 41.01 -> 41.85 | -30.85 -> -24.74 | bit-identical |
| -20 | 41.01 -> 42.28 | -30.85 -> -22.18 | bit-identical |
| **-40** | 41.01 -> 38.27 | -30.85 -> **-0.52** (the plane stops following the mouse) | bit-identical |
| -60 | 41.01 -> 33.79 | -30.85 -> **+1.66** (flipped) | bit-identical |
| -89 | 41.01 -> 33.03 | +2.17 | bit-identical |

At sag 0 the gate is `smoothstep(lo, hi, 0)`: 0.26 at -5, 0.84 at -30, 0.96 at -80 — the
ungated form by another route. The banner prints a plausible number and nothing refuses it.
**Fix:** beside the gap_hi wall,
`check(c.yaw_vert_gap_lo >= std::sin(rad(-20.0)), "[coordination] yaw_vert_gap_lo >= -20 deg -- measured: -40 zeroes the V250 lat-90 yaw demand (-30.85 -> -0.52), the ungated collapse by the other edge");`
and a `CHECK(kCp.yaw_vert_gap_lo >= std::sin(rad(-20.0)))` in the "shipped dial is ON" case.
No kernel change, no fly.

### P1-3  The record omits the dial's duty cycle and its structural inertness inside the signature
`control/params.h:369-408`, `config/controller.toml:1029-1049`.

Tape 6 (his flight, 90.7% mouse-only): the dial is **armed on 17.2% of all ticks** (6123/35532)
and scales the yaw below 0.5 on **13.5%**; armed ticks read |bank| p10/50/90 = 24/74/86 deg,
sag p10/50/90 = 0.2/7.7/37.2 deg, G p50 16. It is not a rare-event actuator. And it is armed on
**0 of the 120 slice ticks** (aim above horizon, nose < -50, |bank_full| > 90): the block requires
`e.cos_phi_theta > 0`, i.e. |bank_full| < 90, so it is structurally inert once inside the
signature. It acts at ONSET only. The toml's "the gate never opens on a tracked turn" is true of
the scripted lat-40 leg, not of his flying. Both facts go in params.h and the toml block; the
landing announcement should say "onset-only mitigation, 17% duty on a lateral session".

### P2-1  Three fixtures are dead since `b2bc840c5`, and one of them is the honest record of what the ruling gave up
`test/fixtures/onset_t5_worst.csv`, `onset_t6_deck.csv`, `onset_t6_worst.csv` (156 KB) are
referenced by no test (`git grep` outside docs: nothing). Measured on the budget alone (temporary
test, reverted): t5worst -582.2 -> -527.1 (55 m — the "9%" the handoff table quotes, unpinned);
**t6_deck AGL 1 -> 0 on both arms: v16 does NOT save the deck event**; t6_worst -558.2 -> -554.2
(4 m, nil). Pin them (a "the budget does not save the deck" leg is what the ruling means) or
delete them.

### P2-2  The ramp pin's `partial` proxy is only a gate witness because budget == 0 on every armed tick today
`test/unit/test_yawbudget.cpp:541-563`. `partial` counts scale in (0.02, 0.98); with budget > 0
the scale is interior under a step gate too. Today ev6 has budget == 0 on 337/337 armed ticks so
it holds; with MUT3's floor the mutation arm reads partial 345/345 and the second half of the pin
inverts. Also the commit message says the step band "arms the same ticks" — it arms 329 vs 337.
Read the gate from `elev_gap`/sag directly (it is on the tape) instead of inferring it from the
scale.

### P2-3  The hash pin is not a witness for this dial; identity is by construction
`test/unit/test_loop_rollover.cpp:349-360` hashes `omega_des.z` only, on a rolling loop that
never opens this block. `S-yawbudget: budget 0 is bit-identical` compares two budget-0 arms with
different gaps — gap-independence, not v15-identity. The off arm IS v15: `git diff b697d24a5..HEAD
-- control sim` is one guarded block plus one Telemetry field defaulting to 1.0 (verified), so
no test can fail on it — but the landing text should not cite `dbdf52980174305e` as the proof.
A three-axis hash captured from the v15 binary would be.

### P2-4  The 1e-6 threshold is a roundoff guard and nothing more
Justified for what it does: MUT1 (threshold removed) brings back "armed 75/480" on the loop leg
with yaw of ~1e-9 rad/s scaled (v_end differs at the 14th digit) — a lying instrument, exactly as
the commit says; every fixture number is unmoved with or without it, so it masks nothing real.
But it is a hard threshold at budget == 0 (scale 1 -> 1-gate at dig = 1e-6), which is the P1-1
discontinuity relocated from 0 to 1e-6. Superseded by the P1-1 floor.

### P2-5  Loader: degree wrap through sin()
`load_controller.cpp:245-248`: `gap_hi = 170` loads as sin(170) = 0.174 = "10 deg" and the banner
prints asin = 10.0. Add `-90 <= gap_lo, gap_hi <= 90` range checks. The `unload_*` refusal is
correct (`at_path` on the coordination table; the five names are the complete set that ever
existed per the scrap diff; a stale toml with any of them is refused with the tag in the message).
`line_hold_ff` wall (`yaw_vert_budget > 0` requires `line_hold_ff > 0`) present and correct.

### P2-6  Stale tip in LANES; the running gate is not on the tip
`LANES.toml:274` says "tip b2bc840c5"; the tip is `a605a792b` and it changed
`control/controller.cpp`. `build-gate/` is gating `b2bc840c5` (401/2068 at 18:40). A landing per
the v15 record needs the gate on the tip that lands.

### Verified clean (no finding)
* S-unload removal net-zero: no `unload_` residue in code beyond the loader refusal and scar
  comments; Telemetry field, tape column, banner line, X-macro entry, five params gone
  (`git grep`). Tape 6's 160-column header is byte-identical to the shipped writer's
  (`build-rt/tape_e2e.csv` from the e2e test): the 161 -> 160 contract is the pre-unload
  contract restored. Nothing reads a tape by index (the roundtrip test derives from
  `kFeelTapeColumns`; no python tape readers in the repo; fixtures are 90-column compact files
  read by name).
* Off-arm/hash: the entire kernel diff since v15 is inside
  `if (cp.yaw_vert_budget > 0.0 && e.cos_phi_theta > 0.0)`; `SEADS_FEEL_DIALS` lists exactly
  `right_hand_rest` and `yaw_vert_budget`; the off arm reproduces `dbdf52980174305e`.
* Mutations the tests claim: MUT1 threshold removed -> loop pin reds (5 CHECKs). MUT2 gate
  deleted -> COST reds (turn 31.19, yaw +4.16) and the ramp pin reds (partial 0). Both kill.

---

## PART B — THE DECISION (independent second opinion)

**The case is mostly sound, but claim (3) is wrong on the code, and the slice definition is too
narrow for his worst events. "Accept and document" is defensible — it is in fact the standing
July ruling — but only with the record fixed as above, and the acceptance stated as a statement
about his hand, not about the kernel.**

Claim by claim:

1. **Predates every dial — SOUND, and the accept-ruling has a precedent.** `docs/flight-log.md:159`
   (2026-07-12, S-retclamp): *"if I deflect full left it starts to turn then goes into a long dive
   and tries to come back around."* The attribution at the time: the plane *"honestly flies the
   full far-aim carve (bank ~90, nose falls, the long dive-around)"*, and the row closes with
   *"accept off-screen aim as the cost of flick-and-park. Chad's call, nothing queued."* The
   "near perfect" verdict of 07-08 was given with this shape present. The fault is old; so is the
   acceptance.
2. **Committed at onset — SOUND.** The dial is inert past 90 deg of bank_full (cos_pt gate; 0 of
   120 slice ticks armed on tape 6); the onset fixtures recover 77% only when seeded before the
   roll-in, the mid-manoeuvre fixtures 9–16%. And tape 4's two 600/690 m dives never exceed
   |bank| 74 deg: the commit happens at 65–74 deg of bank with the aim +6..+13 deg ABOVE the
   horizon and the nose ending at -75..-79.
3. **Freelook holds the aim — WRONG.** `app/instructor_tick.h:934-987`: while freelook is held,
   KEYS OR NOT, `st.aim.snap_forward_to_nose(...)` runs every tick — the aim is WELDED to the
   nose; the pre-freelook lateral aim is erased at the spacebar press and the mouse never feeds
   the aim during freelook. Override keys: `control/controller.cpp:258-266` sets `ns.pursuit =
   false` while any key is held — the instructor stops chasing the aim entirely. A
   flick-then-freelook is a flick followed by ZERO lateral demand. His habit is precisely what
   avoids the fault, not what reproduces it. Tape 6 agrees: armed & freelook = 0, armed &
   override = 0; the t=51 slice was 100% mouse-only, the t=196 one was 100% override-driven with
   the aim at +73 deg (a different animal).
4. **Cost 400–600 m — SOUND.** Fault-class events (aim above horizon at onset, nose < -50):
   tape 6 t=51 452 m, t=161 558 m, t=122 336 m from 428 m altitude (the deck event); tape 4 599
   and 690 m. In the low game loop these are the whole fight.
5. **The law names it a fault — SOUND by definition**: aim +6 deg above the horizon, nose -79.

**Overstated:** the "slice signature" as defined (|bank_full| > 90) misses his worst dives —
tape 4 has 0 slice ticks by that definition and 389 ticks of aim_el > 0 & nose < -50. Use the
latter as the fault-class signature; the bank past 90 is a symptom of some events, not the
signature. And "he could be wrong" about never meeting it is unsupported by the code: with his
habit the instructor never receives a sustained far lateral aim. The honest framing is that a
mouse-only player (or him on a bad day) meets it at roughly 400+ counts/s of lateral flick —
every fault-class event on tapes 5/6 carries 412–565 counts of |dx| in the preceding second.

**Is "accept and document" defensible for a frozen-near-perfect kernel?** Yes, on three
conditions: (i) it is recorded as the RE-affirmation of the July ruling, not a new one; (ii) the
dial that lands is documented as an onset-only mitigation with its 17% duty cycle and the deck
non-save (P1-3, P2-1); (iii) the acceptance is written as a statement about the pilot population
("his hand does not produce it; a sustained max lateral flick does"), so the day the game loop has
a second pilot the fault is already named, not rediscovered. A kernel that is "near perfect" for
one pilot's habits and carries a 600 m dive for another's is not frozen-near-perfect; it is
frozen-near-perfect-for-Chad, and the record should say which.

**THE ONE MEASUREMENT** (from the normal-fight tape when it arrives; `tape_census.py` in this
session's scratchpad does it): the count of fault-class events per minute — aim_el > 0 & nose <
-50, with freelook and override both false for the preceding 1 s — together with the
distribution of 1-s |aim_dx| sums. If his normal fighting never exceeds ~300 counts/s laterally
and the count is 0 over the session, the fault is outside his loop by HAND, and accept-and-
document stands. If there is one such event, it is in his loop regardless of what he believes,
and the ruling should be the July one re-opened, not re-affirmed.

---

## Appendix — what was run
* MUT1 `-yv_dig > 1e-6` removed: loop pin reds (armed 75/480, sc_min 0, gap/yaw/v_end differ in
  the last digits); all fixture numbers unmoved. Reverted.
* MUT2 gate factor deleted: COST lat-90 reds (41.01 -> 31.19, yaw +4.16), ramp pin reds
  (partial 0/337); ONSET ev6 -101.4 (the gate is saturated on the dives). Reverted.
* MUT3 `budget = max(budget, 1 deg/s)`: numbers in P1-1. Reverted.
* Probe printf on armed ticks (fixtures): table in P1-1. Reverted.
* Temporary test (dead fixtures + gap_lo sweep): P2-1 and P1-2. Reverted.
* Tape census (`tape_census.py`, `tape_armed.py`, pure python) on tapes 4/5/6 in `build-play/`
  (read only). Tape 4: 0 freelook/override, 0 slice, 2 fault-class dives at |bank| 65–74.
  Tape 5: 2 slice events, both mouse-only, 428/311 m. Tape 6: 13 nose<-50 dives (9 with the aim
  deliberately below the horizon), 2 slice events, dial census as in P1-3.
* Clean rebuild after every revert; the final exe is the unmutated `a605a792b`.

---

## TAPE — `build-play/feel_tape_normal1_budgetonly.csv` (independent read, `tape_normal.py`)

122,727 rows, 160 columns, 1022.7 s, dt 8.33 ms; banner: `yaw_vert_budget 1.00 gap -5.0..10.0`,
no other lane dial. Airborne 99.9% (124 crashed ticks, 1 grounded). Of airborne ticks:
freelook 13.5%, override 11.2%, **mouse-only 77.9%**. Speed p10/50/90 = 65/246/286 m/s;
G p90/p99/max = 17.2/27.5/35.4. `alt` is height above the reference sphere, not AGL — it is
below 0 on more than 10% of the tape (p10 −1974 m: the stope), so "near the deck" cannot be
read from this column; I do not grade the deck pulls.

**Does the slice signature appear in normal fighting? NO.**
* SLICE (aim_el > 0 & nose < −50 & |bank_full| > 90): **0 ticks, 0 events.**
* FAULT-CLASS (aim_el > 0 & nose < −50, any bank — the definition that catches tape 4's 600/690 m
  dives): **0 ticks, 0 events.**
* Any nose < −50: 2124 ticks, 12 events — every one with the aim at −32..−86 deg elevation at
  onset (pilot-commanded dives; seven start at negative G, i.e. push-overs), drops 245–1484 m.
  None is a fault.

**Does the COVERAGE support acceptance? As a statement about his hand — yes. As a statement
about the kernel — no, and the record must not let the two blur.**
* He flicks laterally as hard as on the lateral sessions: 1-s |aim_dx| sums (mouse-only,
  airborne) p50/p90/p99/max = 46/268/921/1862 counts; 4712 windows above 400 and 3103 above 500
  — the level that preceded every fault-class event on tapes 5/6 (412–1161 counts).
* But he does not PARK the aim laterally. The aim's lateral angle off the nose (body frame,
  mouse-only, V > 150): p50/p90/p99/max = 0.8/7.0/37.9/85.6 deg; only **189 ticks (1.6 s of 17
  min) above 60 deg**, 22 above 80. Tape 6 by the same measure: p99 64.7, 431 ticks above 60.
* Sustained |lateral| > 60 deg for ≥ 0.5 s, mouse-only, V > 150: **ONE run**, 0.50 s at t=87.7
  (V 297, aim_el +5.2): the nose fell to −4.8, max |bank| 90, altitude drop 0. Tape 6: two runs,
  one of 1.10 s at t=122.8 → nose −80, drop 335 m (the deck event). So the tape carries **one
  exposure of the precondition and zero faults** — that is not evidence the kernel survives a
  parked lateral aim (tapes 4–6 show it does not); it is evidence his style holds the far aim
  for ~0.5 s, not the ~1 s the onset fixtures needed to commit.
* The largest flicks (1862 counts/s at t=937.6) were made with the aim already at −68 deg: the
  dive was the intent.
* The dial on this tape: armed 4.45% of airborne ticks (5461), scale < 0.5 on 2.76%; killed at
  |sin phi| < 0.2: 321 ticks in 22 runs, longest **858 ms** (the P1-1 shape, live in normal
  fighting too); armed & freelook 0, armed & override 0 (the code reading in Part B holds).

**Second opinion, tape in hand.** The signature does not appear in 17 minutes of his normal
fighting, and the mechanism is not "he never flicks hard" but "he never holds a far lateral aim
longer than half a second at speed" — freelook (weld to nose) and override (pursuit off) are
his reflex within that window. Acceptance is defensible on that basis and should be written on
that basis: *the fault requires a sustained (~1 s) far-lateral mouse aim at speed; on 17 min of
the pilot's normal fighting that precondition occurred once for 0.5 s and produced no dive; on
the deliberate lateral sessions it occurred repeatedly and cost 336–690 m per event; the dial
is an onset-only mitigation (9–77% on the fixtures) and does not save the deck event.* The
kernel line in CLAUDE.md should carry that sentence, not "near perfect". Landing v16 as
LAND-WITH-FIX (P1-1 minimum form, P1-2, P1-3) is consistent with the tape.
