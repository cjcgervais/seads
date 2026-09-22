# LEG X1 — ROLLOVER ATTRIBUTION AND THE THREE SEPARATORS
**2026-09-18.** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`.
**READ-ONLY.** Tapes in `D:/flight_sim2/seads-recon/build-play` opened `'rb'`, nothing written there.
Nothing built, no `ctest`, no `seads.exe`, no dial moved, no `sim/` `control/` `config/` `test/` file
touched. New files this leg, and only these: `tools/sled_tape_x1.py`,
`docs/sled_audit/x1_v17.json`, `docs/sled_audit/x1_all.json`, this document.

**Instrument** `tools/sled_tape_x1.py` — same tape reader and the **same past-90 episode definition**
as `tools/sled_tape_audit.py` (round 1), so the counts join: 86→8, 87→4, 88→10, 89→0, 90→7, 91→23,
identical to `SLED_RIDE_AUDIT_20260918.md` §1.1.
**Corpus** the six v17 drives of 2026-09-17 first (12.02 min, 52 crossings), then all 84 parsed tapes
(206.08 min, 1 032 crossings).

**Confidence vocabulary** MEASURED / DERIVED / LITERATURE / GUESS, on every number.

---

## 0. THE VERDICT IN NINE LINES

1. **Rung 1 SURVIVES X1, and it survives it larger, not smaller.** Attributed to the surface the
   machine was on **when it left the ground** — the causally correct moment, not the 90° crossing and
   not the exit — TrailMain runs **11.98 tumble events/min against Bush 0.50 and Road 0.42**.
   That is **24× bush**, on 19.79 minutes of trail and 237 events. MEASURED.
2. **And the corridor edge is under the skis at the launch.** Of those 237 trail launches, **147
   (62 %) had the two skis on DIFFERENT surface classes within 0.5 s of takeoff**, 187 (79 %) within
   1.0 s, and **not one of the 237 ever launched without one**. Against bush launches: 18 % / 31 %,
   and 13 % that never saw an edge at all. MEASURED. That is `class_blend_m`'s exact trigger, caught
   at the trigger moment.
3. **The packet's own killing mutation (i) DOES fire — on the number it was aimed at.** Re-attributed
   to the surface at episode **exit**, TrailMain falls 17.84 → **12.38** rolls/min per crossing, and
   at the tumble-event level 9.30 → **2.43**/min. So the *exit* reading does collapse the trail. The
   *takeoff* reading does not. Both are reported; §3 says why the takeoff one is the honest one.
4. **X1 cannot tell the class step from the bank face, and nothing on tape can.** They are the same
   place by construction (`bank_profile` is evaluated on `e = dist − half_w`, and `classify()` flips
   at that same edge). Co-occurrence over 1 032 crossings: P(edge | contact) 42 %,
   P(contact | edge) 59 %. **Only the probe separates them** — `class_blend_m` moves the class step
   without moving the bank. DERIVED from `world/snowpack.cpp:270-300` + MEASURED co-occurrence.
5. **THE RATE IS NOT 4.32/min. It is 1.33/min.** 52 past-90 crossings on last night's six drives are
   **16 tumble events** — a tumble re-crosses the 90° line as it rolls. One tape-91 tumble is **nine**
   crossings in 6.6 s. MEASURED. Round 1's "one every 13.9 s" is one every **45 s**.
6. **And "seven of ten never recover" inverts with it: 14 of 16 tumble events (88 %) end back on the
   skis** (corpus: 311/353, 88 %). The 71 % was counting the middle of a tumble as a failed recovery.
   MEASURED. ⚠ This is a **report against a signed number** — §2.3.
7. **The deliberate flip is 6 % of tumble events, not 21 % of crossings.** Chad: *"a backflip sometime
   deliberate is not a roll over."* Airborne + ≥0.35 s of hang + pitch-dominant rotation: 11 of 52
   crossings (21 %) but only **1 of 16 tumble events (6 %)**, because a flip generates several
   crossings. Rate with flips removed: **1.25 tumble events/min**. MEASURED.
8. **Chad's throttle report is exactly right and the number is brutal.** Over 206 minutes the thumb
   command rests at 0 or 1 and **nowhere else**: of 116 316 ticks in the open band (0.02–0.95),
   **159 — 0.14 % — are parked** (rate < 0.2/s over 66 ms). **0.011 % of all drive time is a held
   intermediate throttle.** He modulates by tapping (9.6 taps/min, median hold 0.22–1.16 s) and
   braking (3.4 presses/min, 2.8 gas↔brake alternations/min). MEASURED.
9. **The pendulum pump he is asking for is ALREADY IN THE KERNEL, ARMED, and it almost never gets to
   fire.** `right_assist_nm = 2400` ships and is in all six v17 tapes. Of 957.7 s past 90° corpus-wide
   the gate is open **13.4 %** of the time; it opened in **130 of 1 032** crossings. Where it opened,
   57 % of v17 episodes recovered against 22 % where it did not (n=7 vs 45 — small, directional).
   The blocker is the **1.3889 m/s speed gate**, not the press. MEASURED (upper bound: `hands_on`
   is not pinned).

---

## 1. WHAT X1 MEASURES, AND HOW

### 1.1 The tape's ground stream, decoded
`test/harness/sled_tape.h` emits `G tick dir.x dir.y dir.z drive_r depth_m surface` **before** each
tick's `T` line. MEASURED, this leg, on tapes 86 and 91: the stream has **exactly two modes** —

| G per tick | meaning | tape 86 | tape 91 |
|---|---|---|---|
| **48** = 12 substeps × 4 | the four CONTACT patches | 9 935 | 14 934 |
| **168** = 12 × 14 | the same four **plus the ten side-hull points** (`_side_hull_points 10`), queried only while the hull is engaged | 1 474 | 5 741 |
| other | mid-frame state overrides | 21 | 59 |

Reconstructed in the body frame at tape 86 tick 600 (lat = +right, fwd = +nose, metres from the CG),
the 4-query substep is **[left ski, right ski, track aft, track fore]**:

| slot | lat | fwd | patch |
|---|---|---|---|
| 0 | −0.499 | +0.800 | LEFT SKI |
| 1 | +0.428 | +0.800 | RIGHT SKI |
| 2 | −0.035 | −0.578 | TRACK AFT |
| 3 | +0.002 | −0.057 | TRACK FORE |

The tool **re-derives this per tick from the data** (the two most-forward samples are the skis; the
more negative lateral of those two is the left ski) and prints the per-tape mean slot geometry so the
map can be checked rather than believed. It **declines the bank read on every 168-sample tick**,
because the 14-query substep's internal order is NOT the 4-query one (a body-frame reconstruction at
tape 91 tick 1118 puts a hull point at index 3). Those ticks are counted, never silently dropped, and
they are hull-engaged ticks — the state **after** the trigger, never the half-second before it.
**Consequence: "was a HULL patch on a bank" is left UNMEASURED rather than guessed.** Skis and track
only.

### 1.2 The bank, as the tape can see it
`drive_r = radius_at + depth` (`world/snowpack.h:619`) — the driven surface. The bank is **snow, not
rock**: `bank_profile()` adds up to `bank_height_m` **1.30 m** over `bank_rise_m` **3.00 m** of
lateral run (`world/snowpack.cpp:207-218`, `config/world.toml:660-661`), peak grade **0.65**. Stance
is 0.927 m, so one ski on the inner face at peak grade is a **0.60 m step across the skis**. Three
tape-visible signatures, reported separately and never collapsed:

| name | definition | what it is |
|---|---|---|
| **grade** | `(drive_r_left − drive_r_right) / lateral span` | terrain/bank cross-slope under the skis |
| **depth split** | `depth_left − depth_right` | the bank's own snow amplitude, one ski vs the other |
| **corridor edge** | `surface_class_left ≠ surface_class_right` | **Rung 1's exact trigger** |

"BANK CONTACT" = grade ≥ 0.25 **or** depth split ≥ 0.15 m. Sensitivity thresholds
(0.15/0.25/0.40 and 0.10/0.15/0.30 m) are in the JSON.
*Killing mutation for every bank number below:* move the threshold. MEASURED corpus per-tick base
rates — grade ≥ 0.15 / 0.25 / 0.40 = **9.64 % / 4.14 % / 1.96 %**; depth split ≥ 0.10 / 0.15 / 0.30 m
= **2.88 % / 1.87 % / 0.19 %**. A 0.10 move in the grade threshold moves the base by a factor of
about 2.3 in either direction, so every absolute percentage in §4 is threshold-bound. The **ranking**
of trail against bush is not: it holds at all three.

### 1.3 The control that makes the bank numbers mean anything
A per-**tick** base rate is the wrong control for a pre-onset **window** question — bank contact is
bursty, so the tick rate understates how often any half-second contains one. X1 therefore counts
**non-overlapping 60-tick (0.5 s) blocks of drive time**, which is exactly the object an episode's
pre-window is, and compares like with like. Base and window are both computed over **every** tick
carrying four contact-patch samples (an earlier cut restricted the base to on-ground ticks while the
windows kept their airborne ticks; the two were then not comparable).

### 1.4 What X1 refuses to claim
* `hands_on` is not in `SLEDTAPE_PIN_D`. Every pump and righting count is an **UPPER BOUND** — the
  same limit RT-B P1-8 declared.
* `right_assist_nm_now` is not pinned either, so "the pump fired" is a reconstruction of its **gates**
  (armed ∧ push ∧ `w_tilt`), never a torque read.
* The kernel runs `right_gs_lp` / `right_charge` at 1440 Hz on the substep velocity; X1 reconstructs
  them at 120 Hz on the pinned `ground_speed_ms`. **DERIVED**, transient-inexact, steady-state exact.
* The throttle rate test is measured over **8 ticks (66 ms), not one**. The command is written once
  per render frame and the tape pins at 120 Hz, so a tick-to-tick difference alternates {0, 2×} at
  60 fps and reports a hard 50/50 transit/parked split that is pure aliasing — measured at **exactly
  0.500** on tape 86's first 3 000 ticks before the fix.

---

## 2. (c) THE SEPARATOR — DELIBERATE FLIP vs ROLLOVER

> *"a backflip sometime deliberate is not a roll over"* — Chad, 2026-09-18

### 2.1 The classes
Rotation is integrated over [cross − 0.5 s, cross] on the **body** axes, which `sim/sled.h:772` labels
**X pitch, Y yaw, Z roll**.

| class | test | reading |
|---|---|---|
| **FLIP_AIR** | airborne at the crossing, ≥ 0.35 s of hang in the preceding second, \|∫ω_x\| > \|∫ω_z\| | **a send** |
| FLIP_AIR_SHORT | airborne, pitch-dominant, < 0.35 s of hang | a clipped nose-over |
| ROLL_AIR | airborne at the crossing, roll-dominant | a barrel roll off a launch |
| ROLL_LANDING | on the ground at the crossing, touched down within 0.5 s | the landing spike |
| ROLL_GROUND | on the ground, no touchdown in 0.5 s | **the pure ground roll** |

### 2.2 Counts

**v17, per tape (MEASURED):**

| tape | min | crossings | FLIP_AIR | FLIP_SHORT | ROLL_AIR | ROLL_LAND | ROLL_GROUND | /min | /min − flips |
|---|---|---|---|---|---|---|---|---|---|
| 86 | 1.59 | 8 | 1 | 2 | 1 | 3 | 1 | 5.04 | 4.41 |
| 87 | 1.98 | 4 | 0 | 1 | 0 | 2 | 1 | 2.02 | 2.02 |
| 88 | 1.65 | 10 | 2 | 2 | 2 | 3 | 1 | 6.05 | 4.84 |
| **89** | 2.12 | **0** | — | — | — | — | — | **0.00** | 0.00 |
| 90 | 1.80 | 7 | 3 | 1 | 0 | 3 | 0 | 3.88 | 2.22 |
| 91 | 2.88 | 23 | 5 | 0 | 11 | 5 | 2 | 7.99 | 6.25 |
| **v17** | **12.02** | **52** | **11 (21 %)** | 6 | 14 | 16 | 5 | **4.32** | **3.41** |
| **corpus** | **206.08** | **1 032** | **238 (23 %)** | 103 | 328 | 233 | 130 | **5.01** | **3.85** |

**AND EVERY SINGLE ROLLOVER LAST NIGHT LEFT THE GROUND FIRST.** Of 52 v17 crossings, **44 had a
takeoff within 1 s, 50 within 2 s, 51 within 3 s, and ZERO had no takeoff at all**. Corpus:
745 / 918 / 984 within 1 / 2 / 3 s; **6 of 1 032 (0.6 %) never left the ground**. MEASURED.
*Killing mutation:* the takeoff tracker records the last 0→air transition without bounding how far
back it was; at a 3 s bound the claim is "within 3 s", not "because of". The 1 s column (85 % v17,
72 % corpus) is the one that carries weight.

### 2.3 ⚠ THE RE-CROSSING FINDING — and a report against a signed number
A tumbling machine crosses the 90° line **several times on the way down**. The round-1 episode
definition — which X1 deliberately did not change — counts each crossing as a rollover. Merging
crossings separated by ≤ 2.0 s of un-recovered time:

| | crossings | **tumble events** | events/min | mean crossings/event | worst |
|---|---|---|---|---|---|
| v17 | 52 | **16** | **1.33** | 3.25 | **9** |
| corpus | 1 032 | **353** | **1.71** | 2.92 | 15 |

Gap sensitivity (the killing mutation, and it is small): 1.0 s → 17 events, 2.0 s → 16, 3.0 s → 16
(corpus 412 / 353 / 332).

Tape 91's nine-crossing tumble, verbatim from the JSON — this is **one crash**, not nine rollovers:

```
t= 41.2s ROLL_GROUND  TrailMain -> TrailMain  v=37.3  tilt=172
t= 42.0s ROLL_AIR     TrailMain -> TrailMain  v=28.2  tilt=133
t= 42.5s FLIP_AIR     TrailMain -> Bush       v=27.4  tilt=174
t= 43.2s FLIP_AIR     Bush      -> Bush       v=26.6  tilt=123
t= 44.2s FLIP_AIR     Bush      -> Bush       v=19.0  tilt=137
t= 45.0s FLIP_AIR     Bush      -> Bush       v=13.8  tilt=104
t= 45.6s ROLL_LANDING Bush      -> Bush       v= 9.8  tilt=109
t= 46.4s FLIP_AIR     Bush      -> Bush       v= 7.9  tilt=123
t= 47.8s ROLL_LANDING Bush      -> Bush       v= 2.0  tilt=172  rec=True
```

**Three round-1 headlines are re-scored by this, and two of them invert:**

| round-1 headline | X1 at the tumble-event level |
|---|---|
| *"She rolls 4.32 times a minute — one every 13.9 s"* (§0.1) | **1.33/min — one every 45 s.** With deliberate flips removed, **1.25/min**. |
| *"Seven of ten rollovers never end — 71.2 % never return to steady driving"* (§0.2) | **88 % DO end back on the skis** (14/16 v17, 311/353 corpus). |
| *"deliberate flips must be separated"* (R4) | flips are **21 % of crossings** but **6 % of tumble events** (1 of 16). |

*Killing mutation for all three:* the merge rule. A tumble that genuinely stops, is driven for 1.5 s,
and rolls again is counted here as one event. Widening the gap to 3 s changes v17 not at all (16) and
the corpus by 6 %. Narrowing it to 1 s gives 17 and 412. **The headline "one every 13.9 s" does not
survive any gap ≥ 1 s.**
*And the honest limit:* "recovered" is round 1's own test — tilt < 20° **and** speed > 5 m/s held for
0.5 s, checked up to 20 s after the last crossing. A machine righted and left idling does not score.
**5 of the 16 v17 events used the R key** (`autoright_R`), so "recovered" is not all self-recovery —
§6.

---

## 3. (a) THE SURFACE AT ENTRY, AT EXIT, AND AT TAKEOFF

Rung 1's stated killing mutation (i): *"re-attribute each rollover to the surface at episode exit. If
TrailMain's rate collapses, you were crossing the trail, not rolling on it."*

### 3.1 Per crossing (corpus, 1 032)

| surface | minutes | rolls @ entry | /min | rolls @ exit | /min | change |
|---|---|---|---|---|---|---|
| Bush | 124.89 | 508 | 4.07 | 578 | 4.63 | +14 % |
| Road | 50.45 | 161 | 3.19 | 186 | 3.69 | +16 % |
| **TrailMain** | 19.79 | **353** | **17.84** | **245** | **12.38** | **−31 %** |
| LakeIce | 7.07 | 6 | 0.85 | 6 | 0.85 | 0 |
| RockOutcrop | 2.33 | 4 | 1.71 | 4 | 1.71 | 0 |
| TrailTributary | 1.54 | 0 | 0.00 | 1 | 0.65 | — |

19.7 % of crossings change surface between entry and exit; the big flows are
**TrailMain → Bush 90** and **TrailMain → Road 49**.

### 3.2 Per tumble event, entry vs exit (corpus, 353)

| surface | minutes | events @ first entry | /min | events @ last exit | /min |
|---|---|---|---|---|---|
| Bush | 124.89 | 92 | 0.74 | 198 | 1.59 |
| Road | 50.45 | 75 | 1.49 | 90 | 1.78 |
| **TrailMain** | 19.79 | **184** | **9.30** | **48** | **2.43** |

**So the exit attribution DOES collapse the trail — by 74 % at the event level.** Taken alone it
would end Rung 1's tape support, exactly as the packet warned.

### 3.3 ⚠ BUT EXIT IS THE WRONG END OF THE EVENT, AND THE GEOMETRY SAYS SO
A tumble beginning on the trail is *moving*, and the trail is a **ribbon** — `half_width` metres wide
inside a world that is otherwise bush. Where a tumble **stops** is therefore biased toward the wide
surface by construction, and "exit" measures the width of the surface, not the cause of the roll.
The 90°-crossing entry is already 0.5–1.5 s downstream of whatever did it. **The one moment in the
tape that is upstream of the whole event is the takeoff** — 92 % of tumble events corpus-wide have one
within 3 s (94 % on v17). Attributed there:

| surface | minutes | **tumble events @ takeoff** | **/min** | vs Bush |
|---|---|---|---|---|
| **TrailMain** | 19.79 | **237** | **11.98** | **× 24.0** |
| RockOutcrop | 2.33 | 3 | 1.29 | × 2.6 |
| Bush | 124.89 | 62 | 0.50 | × 1.0 |
| Road | 50.45 | 21 | 0.42 | × 0.8 |
| LakeIce | 7.07 | 0 | 0.00 | 0 |
| TrailTributary | 1.54 | 0 | 0.00 | 0 |

v17 alone (n = 16 events, 1 unattributable): TrailMain **11.14**/min, Road 1.49, RockOutcrop 1.13,
Bush 1.21, LakeIce 0.00.

**237 of 323 attributable tumble events — 73 % — launched from a surface that is 9.6 % of the drive
time.** MEASURED.
*Killing mutations.* (i) The takeoff surface is the surface at the 0→air tick, which can already be
the far side of a corridor edge — it is one tick downstream of the edge crossing, not upstream.
(ii) The trail's 19.79 min are **not** evenly spread across tapes; 12 of them are pre-reconcile.
(iii) Speed is confounded with surface — trail takeoff speed p50 is 11.9 m/s against bush 16.3 m/s,
so the trail is **not** launching him faster, which points the other way and is worth the ink.
(iv) v17 carries only **0.45 min of TrailMain**, so last night barely tests the trail at all: **any
Rung 1 A/B has to be driven on the trail, deliberately.**

---

## 4. (b) BANK CONTACT — ONE SIDE, AND WHEN

> *"often rolling when hitting banks … other times its just going down the road and a ski hitting the
> bank on one side"* — Chad, 2026-09-18

### 4.1 The 0.5 s before the 90° crossing, against a matched 0.5 s base

| corpus | blocks | base: grade | base: depth split | base: class split | base: any | crossings | pre: contact | pre: edge |
|---|---|---|---|---|---|---|---|---|
| **v17** | 1 239 | 9.5 % | 2.7 % | 4.0 % | 12.7 % | 52 | **4 (7.7 %)** | **2 (3.8 %)** |
| **corpus** | — | 6.4 % | 3.2 % | 6.2 % | 10.7 % | 1 032 | **198 (19.2 %)** | **140 (13.6 %)** |

Widening the window: corpus 1.0 s → contact 31.7 %, edge 27.6 %; 1.5 s → 41.0 %, 38.9 %.

**Read this honestly, both ways.**
* Over 206 minutes the enrichment is real: **contact 19.2 % vs a 10.7 % base (1.8×)** and **edge
  13.6 % vs 6.2 % (2.2×)**.
* **On last night's six drives it is ABSENT** — 7.7 % against a 12.7 % base, i.e. at or below chance.
  Five of those six tapes have **no plowed corridor at all** (Bush / LakeIce / RockOutcrop), so
  `bank_profile` banks do not exist in them, and tape 91 is the only one that can test it.
  ⚠ **No v17-only claim about banks is available from this corpus.**

### 4.2 One-sidedness — his exact word, and it holds
Of the 198 corpus crossings with bank contact in the pre-window, **190 (96 %) are ONE-SIDED** — the
same ski high, or the same ski in the deeper snow, on ≥ 90 % of the contact ticks. MEASURED. Chad's
*"a ski hitting the bank on one side"* is the shape the tape shows; it is not a two-ski event.

### 4.3 Which class it is, and this is where it lands
Bank evidence in the 0.5 s pre-window, by crossing class (corpus):

| class | n | contact | corridor edge | recovered |
|---|---|---|---|---|
| **ROLL_GROUND** (the pure ground roll) | 130 | **29.2 %** | **23.8 %** | 50 % |
| FLIP_AIR_SHORT | 103 | 33.0 % | 28.2 % | 19 % |
| ROLL_AIR | 328 | 22.3 % | 12.8 % | 17 % |
| FLIP_AIR | 238 | 12.6 % | 10.1 % | 24 % |
| ROLL_LANDING | 233 | 9.9 % | 6.0 % | 49 % |
| *(base)* | | *10.7 %* | *6.2 %* | |

The bank/edge signal is concentrated exactly where Rung 1's mechanism lives — **the ground roll, 2.7×
base on contact and 3.8× base on the corridor edge** — and is at base level in the landing class,
which has a different mechanism (§2(d) of the packet, the landing spike).

### 4.4 THE MEASUREMENT THAT DECIDES IT — bank lag at the launch
The bank is *not* only a thing you roll on. `bank_profile`'s own comment calls it **"a RAMP that
launches, never a step that stops the machine dead"** (`world/snowpack.cpp:209-211`). So the right
question is not "was there a bank 0.5 s before she passed 90°" but **"was there a bank 0.5 s before
she left the ground"** — and the answer is unambiguous:

| takeoff surface | tumble events | bank ≤ 0.5 s | ≤ 1.0 s | never seen | **edge ≤ 0.5 s** | edge ≤ 1.0 s | edge never | takeoff speed p50 |
|---|---|---|---|---|---|---|---|---|
| **TrailMain** | **237** | **123 (52 %)** | 136 (57 %) | **3 (1 %)** | **147 (62 %)** | 187 (79 %) | **0** | 11.9 m/s |
| Bush | 62 | 14 (23 %) | 19 (31 %) | 11 (18 %) | 11 (18 %) | 19 (31 %) | 8 (13 %) | 16.3 m/s |
| Road | 21 | 3 (14 %) | 8 (38 %) | 0 | 5 (24 %) | 12 (57 %) | 0 | 6.1 m/s |
| RockOutcrop | 3 | 1 | 1 | 1 | 1 | 1 | 0 | 12.1 m/s |

**Every one of the 237 trail-launched tumbles had a corridor edge under its skis at some point, 62 %
of them within half a second of leaving the ground, at 11.9 m/s.** MEASURED.
*Killing mutations.* (i) The lag trackers are global — "bank ≤ 0.5 s" means the last contact tick was
within 0.5 s, not that it caused the launch. (ii) On a narrow trail the skis straddle the edge often
by geometry alone; the bush row (18 % edge, 13 % never) is the control that makes the trail row mean
something, and it is a weak control because bush has no corridor. (iii) The hull-engaged ticks are
excluded (§1.1), which removes ticks late in a tumble, not before a launch.

### 4.5 ⚠ AND THE THING THE TAPE CAN NEVER DO
The corridor edge and the bank face **are the same place**. `corridor_eval` computes the bank on
`e = dist − half_w` — the distance **outside** the corridor edge — and `classify()` flips the surface
class at that same edge. Co-occurrence over 1 032 crossings: **P(edge | contact) 42 %,
P(contact | edge) 59 %.** No tape instrument can separate "`rho_eff` stepped 0 → 260 under one ski"
from "one ski climbed a 1.30 m ramp", because they happen at the same metre.
**The separation is a probe, and it already exists**: `class_blend_m` moves the class step and leaves
`bank_profile` untouched, and the tree's own bankgraze record is **blend 0.0 → yaw −482.7°, ROLLED;
blend 1.0 → yaw −11.6°, no rollover** (`config/world.toml:640-644`).

---

## 5. (d) THROTTLE — "ALL OR NOTHING"

> *"throttle is all or nothing ive mitigated this by tapping and applying brake"*
> *"throttle is cut unless im key pressing but if I key press throttle should ramp up"*
> *"I can only really turn sharply if I alternate gas/brake"* — Chad, 2026-09-18

The tape records `SledInputs.throttle` — the **command**, before `sim/sled.cpp:292-293` zeroes it for
`rolled`. `app/main.cpp:8289-8292` ramps it: **W held +2.5/s, released −6.0/s** (0→1 in 0.40 s,
1→0 in 0.167 s). The W key state is reconstructed from the sign of dθ/dt. DERIVED.

### 5.1 There is no intermediate throttle. There are two resting states.

| | ticks at 0 (≤0.02) | open band | WOT (≥0.95) | **open band that is RAMP TRANSIT** | **open band PARKED** | parked as % of all drive time |
|---|---|---|---|---|---|---|
| v17 | 40.2 % | 7.6 % | 52.1 % | 6 276 | **7** | **0.008 %** |
| corpus | 34.3 % | 7.8 % | 57.8 % | 110 949 | **159** | **0.011 %** |

"Transit" = \|Δthrottle\| > 1.0/s measured over 66 ms; "parked" = < 0.2/s. **Over 206 minutes of his
driving, the thumb command is held at an intermediate value for 159 ticks — 1.3 seconds.**
He is not failing to modulate. **The control has nothing to modulate with**: the only stable points of
a key-ramp with no detent are 0 and 1.
*Killing mutation:* the 66 ms window. At a 1-tick window the split reads exactly 50/50 and is pure
frame aliasing (§1.4); at 133 ms the transit share rises further. The claim "parked ≈ 0" is stable
from 33 ms up.

### 5.2 How he actually drives it — per tape (MEASURED)

| tape | zero/open/WOT | taps | taps/min | tap len p50 | **key-held p50** | taps to ≥0.99 | brake/min | **alt/min** | bursts | alts while steering |
|---|---|---|---|---|---|---|---|---|---|---|
| 86 | 27/10/63 % | 19 | 12.0 | 1.42 s | 1.07 s | 18/19 | 4.4 | 6.9 | 5 | 7 |
| 87 | 44/8/48 % | 20 | 10.1 | 1.22 s | 0.87 s | 12/20 | 4.0 | 2.5 | 1 | 5 |
| 88 | 32/12/56 % | 27 | 16.3 | 0.70 s | **0.35 s** | 16/27 | 8.5 | **11.5** | 6 | 16 |
| 89 | 49/7/44 % | 24 | 11.3 | 0.57 s | **0.22 s** | 12/24 | 6.1 | 4.7 | 4 | 9 |
| 90 | 78/2/20 % | 4 | 2.2 | 10.43 s | 10.09 s | 3/4 | 2.2 | 1.1 | 0 | 0 |
| 91 | 19/8/72 % | 32 | 11.1 | 1.52 s | 1.16 s | 26/32 | 3.5 | 4.9 | 4 | 11 |
| **v17** | 40/8/52 % | **126** | **10.5** | — | — | 87 (69 %) | 4.7 | **5.1** | 20 | 48 |
| **corpus** | 34/8/58 % | **1 988** | **9.6** | — | — | 1 590 (80 %) | 3.4 | 2.8 | 185 | — |

**Read `tap len p50` against `key-held p50`.** On tapes 88 and 89 the median press is **0.22–0.35 s**,
against a 0.40 s ramp to full: **he is releasing before the thumb has arrived.** That is the tapping,
measured. And the two tapes with the shortest presses (88, 89) are also the two with the most
alternation (11.5 and 4.7/min) and the most alternation while the bars are turned (16 and 9) — his
*"I can only really turn sharply if I alternate gas/brake"*, on the tape, in the two drives that did
it most.

**Tape 89 is the control that will not go away.** It has the shortest taps, the third-highest
alternation, the highest stand duty, and **zero rollovers in 2.12 minutes**. Tape 90 is the other:
78 % throttle-closed, 4 taps in 108 s, a 10.4 s cruise, zero bank exposure, and 7 crossings — all of
them FLIP/LANDING class on lake ice.
*Killing mutation for the whole section:* the W-key reconstruction assumes the ramp constants and a
spring-return with no other writer. `SEADS_SLED_DRIVE` pins the thumb in smoke runs only; no tape
here is a smoke run (all six v17 tapes carry a `# sig` footer and real O-records).

---

## 6. (e) RIGHTING — THE PUMP HE IS ASKING FOR IS ALREADY BUILT

> *"self righting with a press and I want to be able to self right by rocking bodyweight back an
> fourth while pressing stand on and off, gain pendulum momentum (not automatic re righting)"*
> — Chad, 2026-09-18

**`sim/sled.cpp:1679-1800` IS that mechanic**, and its comment says so in his own terms: *"the rock
lives in the rigid body's own state … the only new memory is a pusher who TIRES — which is what makes
'a few sustained pushes' emerge instead of being scripted"*, and *"NOTHING HERE DECIDES TO FAIL …
'IT CAN FAIL TO RIGHT GIVEN THE SITUATION' is EMERGENT, never a scripted refusal."*
It is **not** off in the game he drives: `right_assist_nm = 2400.0` in `config/scenario.toml:2085`,
a **required** loader key (`config/load_scenario.cpp:508-509`), **and in the header of every one of
tapes 86–91** (MEASURED, read from the tapes' own `# cparam` lines).
⚠ **And the comment above the block reads the other way.** *"The whole block is inside
`right_assist_nm > 0.0`, which is 0.0 by default and 0.0 for every existing tape (tape-absent
preset)."* Both halves were true when written and neither describes today: the **struct** default is
still 0.0 (`sim/sled.h:224`) and the tape-absent preset still forces 0.0
(`test/harness/sled_tape.h:403`) — but the **shipped config is 2400** and the current tapes name it,
so anyone reading that comment to find out whether Chad has a righting pump is told he does not.
Reported, not removed; it belongs with the two lying instruments in §2(l) of the packet.

### 6.1 What actually happens while she is over

| | past-90 time | armed (speed gate) | stand held | **armed ∧ stand** | **pump gate open** |
|---|---|---|---|---|---|
| v17 | 36.7 s | 20.3 % | 44.1 % | 12.5 % | **12.5 %** |
| corpus | 957.7 s | 35.6 % | 32.5 % | 13.4 % | **13.4 %** |

| | crossings | with ≥ 1 stand press | **with the pump gated open** | with ≥ 2 lean reversals (rocking) | ended by an R press |
|---|---|---|---|---|---|
| v17 | 52 | 22 | **7** | 5 | 5 |
| corpus | 1 032 | 432 | **130** | 176 | 162 |

**He is pressing STAND (44 % of past-90 time on v17) and the gate is shut anyway.** The binding
constraint is `right_assist_max_ms = 1.3889 m/s` on the low-passed speed: armed only 20.3 % of the
time she is over. `armed ∧ stand` and `pump gate open` are the same number to the decimal, which says
`w_tilt` is never the blocker — past 90° the tilt ramp (0.25→0.35 rad) is always saturated.

**Rocking is not happening because rocking is not a mechanic.** `lean_lat` reverses sign twice or more
in only **5 of 52** v17 crossings (176 of 1 032 corpus). There is nothing in the kernel that converts
a lean reversal into pendulum energy — `right_shift_cmd` is latched off the machine's own attitude and
`sim/sled.cpp:1747-1757` says in as many words that the **lean must not touch the righting**
(Chad's own 2026-08-26 correction). His new ask **reopens that ruling**: he now wants the rider's
rocking to drive the pump. ⚠ **That is a direct conflict between two of his rulings, 2026-08-26 vs
2026-09-18, and it is not resolved here.**

### 6.2 Does the pump help when it does fire?

| | n | recovered |
|---|---|---|
| v17 crossings with the pump gated open | 7 | **4 (57 %)** |
| v17 crossings without | 45 | 10 (22 %) |

Directional, n = 7, **not** a finding. But it points the way the mechanic claims to.
*Killing mutations.* (i) `hands_on` unpinned — every number here is an upper bound; a crossing where he
was thrown off scores "stand pressed, pump not fired" for a reason X1 cannot see. (ii) the 120 Hz
reconstruction of `right_gs_lp` differs from the kernel's 1440 Hz one in the transient, which is
exactly where the hysteresis flips. (iii) "recovered" is round 1's test and **5 of 16 v17 events used
R**, so self-recovery is smaller than 88 %.

---

## 7. THE PARAGRAPH — DOES RUNG 1 SURVIVE X1?

**Rung 1 survives X1.** Its stated killing mutation was that exit-attribution would collapse
TrailMain, and exit-attribution does collapse it — 17.84 → 12.38 rolls/min per crossing, 9.30 → 2.43
tumble events/min. But exit is the wrong end of the event: the trail is a narrow ribbon inside a bush
world, so *where a tumble stops* measures the width of the surface it stops on, not the cause of the
roll. Attributed instead to the surface the machine was on **when it left the ground** — the only
moment in the tape that is upstream of the whole event, and one that 92 % of tumble events have —
**TrailMain runs 11.98 tumble events/min against Bush 0.50 and Road 0.42, 24× bush, on 237 events and
19.79 minutes.** And the trigger is under the skis when it happens: **147 of those 237 launches
(62 %) had the two skis reading DIFFERENT SURFACE CLASSES within half a second of takeoff, 187 (79 %)
within a second, and not one of the 237 ever went without one** — against 18 % / 31 % / 13 %-never for
bush launches, at a *lower* trail speed (11.9 vs 16.3 m/s), so speed is not doing it. That is
`class_blend_m`'s exact mechanism, caught at the exact moment, and it is corroborated by the class
breakdown: the bank/edge signal is 2.7×/3.8× base in the pure **ground roll** class and at base level
in the **landing** class. **But the roll trigger is not the class step *rather than* the bank face —
the tape cannot make that distinction and never will**, because `bank_profile` is evaluated on
`e = dist − half_w`, the same edge where `classify()` flips, so the two co-occur by construction
(P(edge | contact) 42 %, P(contact | edge) 59 %) and the bank is explicitly *"a RAMP that launches"*.
What X1 can say is that **the corridor-edge REGION is the launcher**, and that a launch — not a
friction rollover — is what starts 92 % of these events. Two further cautions before this becomes a
drive: **last night does not test it** (tapes 86–90 have no plowed corridor at all; the v17
pre-onset bank rate, 7.7 % against a 12.7 % base, is at or below chance, and tape 91 carries only
0.45 min of trail), and **the rate this rung is being asked to fix is 1.33 tumble events/min, not
4.32 rolls/min** — 52 crossings on those six drives are 16 tumbles, 88 % of which end back on the
skis. Rung 1 remains the right first rung and it is still a drive, not a build; it must be driven
**on TrailMain, deliberately**, with the A/B's invariant read at the **tumble-event** level and
attributed at the **takeoff**, and the same drive should carry the Rung-1/bank ambiguity to the
bankgraze probe, which is the only instrument that can hold the bank still and move the class step
alone.

---

## 8. KILLING-MUTATION INDEX

| # | number | its killing mutation |
|---|---|---|
| X1-1 | TrailMain 11.98 events/min @ takeoff | takeoff surface is one tick downstream of the edge crossing; trail minutes are not evenly spread across tapes; trail launch speed is *lower* than bush |
| X1-2 | 62 % of trail launches have a corridor edge ≤ 0.5 s | lag trackers are global — "within 0.5 s" is not "because of"; the bush control is weak (bush has no corridor) |
| X1-3 | 52 crossings → 16 tumble events | the 2.0 s merge gap. 1 s → 17, 3 s → 16 (corpus 412/353/332). Any gap ≥ 1 s kills "one every 13.9 s" |
| X1-4 | 88 % of tumble events recover | "recovered" is round 1's tilt<20° ∧ v>5 m/s for 0.5 s within 20 s; **5 of 16 v17 events used the R key** |
| X1-5 | flips = 6 % of tumble events | the FLIP_AIR test (air ≥ 0.35 s ∧ pitch-dominant) counts an accidental nose-over as a flip; Chad's *"maybe only 10 %"* is about deliberate bank HITS, a different set |
| X1-6 | bank contact thresholds (grade 0.25, split 0.15 m) | move either; ranking is threshold-stable, percentages are not (sensitivity table in the JSON) |
| X1-7 | v17 shows no bank enrichment (7.7 % vs 12.7 %) | 5 of 6 tapes have no plowed corridor; n = 52 crossings / 16 events; 0.45 min of trail |
| X1-8 | throttle parked 0.011 % of drive time | the 66 ms rate window (1 tick = pure frame aliasing, measured at exactly 0.500); W-key reconstruction assumes the 2.5/6.0 ramp and no other writer |
| X1-9 | pump gate open 13.4 % of past-90 time | `hands_on` unpinned ⇒ upper bound; the 120 Hz `right_gs_lp` reconstruction differs from the kernel's 1440 Hz one exactly at the hysteresis flip |
| X1-10 | hull patches on a bank | **NOT MEASURED.** The 14-query substep's slot order is not the 4-query one; X1 declines those ticks rather than guess |
| X1-11 | `right_assist_nm` is 2400 in the shipped config and in tapes 86–91 | the struct default (`sim/sled.h:224`) and the tape-absent preset really are 0.0, so the source comment is not false — it is **no longer descriptive of the game he drives**. The tape headers are the evidence |

**Machine-readable:** `docs/sled_audit/x1_v17.json` (6 tapes, full per-episode records),
`docs/sled_audit/x1_all.json` (84 tapes). Tool: `tools/sled_tape_x1.py`, stdlib only, 7.3 s over
11.79 GB.
