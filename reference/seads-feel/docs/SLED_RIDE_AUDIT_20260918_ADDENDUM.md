# SLED RIDE AUDIT — ADDENDUM, ROUND 2
**2026-09-18.** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`.
Addendum to `docs/SLED_RIDE_AUDIT_20260918.md`. **It corrects that packet in two places and it
re-ranks its ladder against Chad's own morning words.**

**READ-ONLY, ALL OF IT.** Nothing built, no `ctest`, no `seads.exe`, no dial moved; no `sim/`
`control/` `config/` `test/` `golden/` file touched. His tapes in
`D:/flight_sim2/seads-recon/build-play` were opened `'rb'` by leg X1 and by nothing since; this
round's red-team fold opened none of them (it listed the directory, to name six files by absolute
path). **Files this round: `docs/sled_audit/X1_attribution.md`, `docs/sled_audit/NEWASK_mechanisms.md`,
`docs/sled_audit/ladder_v2.md`, `docs/sled_audit/RULINGS_20260918_chad.md`, `tools/sled_tape_x1.py`,
`docs/sled_audit/x1_v17.json`, `docs/sled_audit/x1_all.json`, and this one.**

**Confidence vocabulary** MEASURED / DERIVED / LITERATURE / GUESS, on every number.
**Every number names its killing mutation.**

---

## §A — HIS RULINGS, VERBATIM, AND WHAT THEY DID

> "86 to 91 all tapes are free riding, often rolling when hitting banks, throttle is all or nothing
> ive mitigated this by tapping and applying brake. Some of the being upside down on the ground is
> due to deliberate jumps for performing a flip off of a banks for fun but maybe only 10% of the time
> its a deliberate hit the bank hard to see what happens, other times its just going down the road
> and a ski hitting the bank on one side. a backflip sometime deliberate is not a roll over, throttle
> is cut unless im key pressing but if I key press throttle should ramp up, self righting with a
> press and I want to be able to self right by rocking bodyweight back an fourth while pressing
> stand on and off, gain pendulum momentum (not automatic re righting). I can only really turn
> sharply if I alternate gas/brake, but throttle should also be able to swing my tail around on
> account of the roost, esp with weight shifting of the sudburian.... Need to make it more dynamic
> with bodyweight (also for jump weight shift influence in air)... please continue automatically I
> have to go to work... bye and good luck. pace so as not to run out of tokens per session limit."

**The mapping is the audit's reading of his words, not his words** (full table:
`docs/sled_audit/RULINGS_20260918_chad.md`):

| packet # | his word | what it did |
|---|---|---|
| **R1** | all six tapes are **free riding**; rolls happen "when hitting banks"; ~10 % deliberate hard hits; some deliberate flips; the rest "a ski hitting the bank on one side" | **ANSWERED.** The v17 numbers stand as free riding. The bank-strike attribution is now his, in his words. |
| **R3** | "throttle is cut unless im key pressing **but if I key press throttle should ramp up**" | **ANSWERED.** `sim/sled.cpp:292-293` zeroes the thumb outright while `rolled`. → **rung 1 / B1.** |
| **R4** | "a backflip sometime deliberate **is not a roll over**" | **ANSWERED: NO.** Flips are separated from rollovers in every count from here on. |
| **R11** | "self righting **with a press** … rocking bodyweight back an fourth while pressing stand on and off, gain pendulum momentum **(not automatic re righting)**" | **ANSWERED.** It **kills the v1 rung 4** (`side_right_vmin_ms` makes the machine *more* automatic) and it promotes the pendulum. → **rung 2 / B2.** |
| **R6** | "more dynamic with bodyweight (also for jump weight shift influence in air)" | **DIRECTION ANSWERED** (yes to airborne authority); the value is his drive, and the rung stays blocked on the N5-PAIR ruling. |
| **NEW-A** | "throttle is all or nothing ive mitigated this by **tapping and applying brake**" | **NEW ASK.** → rung 4, and the red-team proved it is a **pair**, not a dial. |
| **NEW-B** | "throttle should also be able to **swing my tail around on account of the roost**, esp with weight shifting" | **NEW ASK.** → **rung 3 / B3.** |
| **NEW-C** | "often rolling when hitting banks … a ski hitting the bank on one side" | **NEW ATTRIBUTION.** X1 was re-aimed at it (§B). |

**Process ruling, taken literally:** *"please continue automatically … pace so as not to run out of
tokens."* This round ran unattended, at low concurrency, **read-only**; nothing landed on main and
nothing was built. Any build is a fresh lane he drives.

---

## §B — LEG X1, THE RESULT, IN TEN LINES (full: `docs/sled_audit/X1_attribution.md`)

**Instrument** `tools/sled_tape_x1.py`, the same past-90 episode definition as round 1, so the counts
join (86→8, 87→4, 88→10, 89→0, 90→7, 91→23). **Corpus** six v17 drives (12.02 min, 52 crossings),
then all 84 parsed tapes (206.08 min, 1 032 crossings). All MEASURED unless marked.

1. **DOES RUNG 1 SURVIVE? YES — AND LARGER, NOT SMALLER.** Attributed at the **takeoff** (the moment
   upstream of the whole event, present in 92 % of tumble events) TrailMain runs **11.98 tumble
   events/min against Bush 0.50 and Road 0.42 — 24×**, on 19.79 min and 237 events.
2. **And the corridor edge is under his skis at that launch.** **147 of 237 trail launches (62 %) had
   the two skis on DIFFERENT surface classes within 0.5 s of takeoff**, 187 (79 %) within 1.0 s, and
   **not one of the 237 ever launched without one** (bush: 18 % / 31 %, 13 % never).
3. **His one-sidedness is literally what the tape shows.** Of 198 corpus crossings with bank contact
   in the pre-window, **190 (96 %) are ONE-SIDED** — same ski high, or same ski in the deeper snow, on
   ≥ 90 % of contact ticks. That is *"a ski hitting the bank on one side"*, measured.
4. **⚠ REPORT AGAINST A SIGNED NUMBER #1 — the rate is not 4.32/min. It is 1.33/min.** 52 past-90
   crossings on last night's six drives are **16 tumble events**; one tape-91 tumble is **nine**
   crossings in 6.6 s. Round 1's *"one every 13.9 s"* is one every **45 s**.
   *Killing mutation:* the 2.0 s merge gap — 1 s gives 17 events, 3 s gives 16. **No gap ≥ 1 s saves
   the old headline.**
5. **⚠ REPORT AGAINST A SIGNED NUMBER #2 — "seven of ten never recover" inverts.** **14 of 16 tumble
   events (88 %) end back on the skis** (corpus 311/353, 88 %). The 71 % was counting the middle of a
   tumble as a failed recovery.
6. **The deliberate flip is 6 % of tumble events**, not 21 % of crossings (1 of 16 last night); rate
   with flips removed **1.25/min**. His *"maybe only 10 %"* is about deliberate bank **hits**, a
   different set again.
7. **His throttle report is exactly right, and worse than the complaint.** Over 206 minutes the thumb
   is held at an intermediate value for **159 ticks — 1.3 s — 0.011 % of drive time**. Duty: zero
   34 % / open band 8 % / **WOT 52–58 %, 72 % on tape 91**. He taps **9.6/min**, median key-held
   **0.22–1.16 s** against a 0.40 s ramp to full, and alternates gas↔brake **2.8/min** corpus-wide,
   **11.5/min on tape 88** — *"I can only really turn sharply if I alternate gas/brake"*, on the tape.
8. **The pendulum he asked for is already built and armed, and almost never fires.**
   `right_assist_nm = 2400` ships and is in all six v17 tape headers; the gate is open **13.4 %** of
   past-90 time and opened in **130 of 1 032** crossings. **The blocker is the 1.3889 m/s speed gate,
   not the press.**
9. **X1 cannot separate the class step from the bank face, and no tape ever can.** They are the same
   place by construction; P(edge|contact) 42 %, P(contact|edge) 59 %. **Only the `class_blend_m`
   probe separates them.** DERIVED + MEASURED.
10. **The honest limit on all of it:** `hands_on` is not pinned in the tape and
    `test/harness/sled_tape.h:415` forces `grip.unseat_gain = 0.0` on every tape-absent preset, so
    **every pump, righting and recovery count above is an UPPER BOUND.**

**And what X1 cost the ladder:** the two headlines round 1 ranked on (4.32/min, 71 % never recover)
are gone, so **the v1 rung 4 falls twice over** — once to X1, once to R11.

---

## §C — LADDER v2 (full text, with the red-team fold in place: `docs/sled_audit/ladder_v2.md`)

**Identity value = the number at which the build is bit-identical to today.** Every rung is a branch
or a multiply-by-zero, never a 0-weight lerp (`kernel-v14-leanlead` precedent). One dial each.

| # | rung | THE ONE DIAL | identity | touch surface | **tonight?** | his 2026-09-18 sentence |
|---|---|---|---|---|---|---|
| **1** | The key has to reach the track | `rolled_throttle_frac` | 0.0 | `sim/` + `app/` (env) | **YES — B1** | *"if I key press throttle should ramp up"* |
| **2** | The rock has to reach her | `right_stand_shift_frac` **+ `right_assist_max_ms` as its precondition** | 1.0 / 1.3889 (both shipped) | `app/` (env) — zero kernel | **YES — B2a then B2b** | *"self right by rocking bodyweight … gain pendulum momentum"* |
| **3** | The throttle has to swing the tail | `track_lat_slip_shed` | 0.0 | `sim/` + `app/` (env) | **YES — B3, its own night** | *"throttle should also be able to swing my tail around on account of the roost"* |
| **4** | The thumb has to have a middle | `throttle_power_frac` | 0.0 | `sim/` + `app/` (env) | **NO — N4-PAIR ruling owed** | *"throttle is all or nothing"* |
| **5** | The body in the air | `k_air_shift` | 0.0 | `sim/` + `app/` (env) | **NO — N5-PAIR ruling owed** | *"jump weight shift influence in air"* |
| **6** | The seam that spins you | `[snowpack] class_blend_m` | 0.0 | `config/` — **nothing to build** | **NO — R2 open, and it is road-repair's** | *"a ski hitting the bank on one side"* |
| **7** | A track cannot push harder than the snow it stands on | `traction_mu` | 0.0 | `sim/` + `app/` (env) | **YES — PROMOTED to B0, it is B1's ceiling** | — (his 2026-08-24 ruling, taken, unpaid) |
| **8** | The lean has to buy the arc | `plane_lat_lean_gain` | 0.0 | `sim/` + `app/` (env) | **YES, after B3's drive** | *"more dynamic with bodyweight"* |

**Dropped from the top eight, with the reason** (so nobody rediscovers them): v1 rung 4
`side_right_vmin_ms` — **out twice**, by R11 and by X1; v1 rung 3 `autoright_recentre` — off the
ladder pending leg X3, not refuted; v1 rung 6 `assist_tell_band_nm` — needs a new state field **and** a
ruling; v1 rung 7 `right_tilt_surf` — folded into rung 2 as its named second half; v1 rung 8
`pitch_arrest_nms` — **demoted by his own words**, *"a backflip … is not a roll over"* is him
defending the flip.

**Red-team verdicts on this ladder (both lenses, 2026-09-18):** LAND-WITH-FIX on rungs 1, 2, 3 and on
the first build; DO-NOT-LAND-as-one-dial on rung 4; correctly blocked on rung 5; LAND-and-PROMOTE on
rung 7. **All five P0s, all named P1s and the P2 are folded in place; `ladder_v2.md` §7 is the FOLD
LOG**, including the two places where the fold corrected the red-team itself.

---

## §D — THE FIRST BUILD — the packet a fresh lane can pick up

**One lane, one night, FIVE env vars, zero `config/` edits, zero loader edits, zero `render/` edits.
With no env var set the built exe is bit-identical to main for all five dials** (the
`SEADS_GRIP_CAPACITY` precedent, `app/main.cpp:2488`). **TOML is the landing path, not the drive path.**

| | what | dial | identity | arm / kill env var | files touched |
|---|---|---|---|---|---|
| **B0** | **the contact ceiling** (rung 7, promoted — it is B1's precondition) | `params.traction_mu` | 0.0 | `SEADS_SLED_TRACTION_MU` | `app/main.cpp` **only** |
| **B1** | throttle ramps while rolled | `comfort.rolled_throttle_frac` | 0.0 | `SEADS_SLED_ROLLED_THROTTLE` | `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`, `test/harness/sled_tape.h` |
| **B2a** | **the speed gate** (the measured blocker, driven before the rung) | `comfort.right_assist_max_ms` | 1.3888888888888888 | `SEADS_SLED_RIGHT_MAXSPD` | `app/main.cpp` **only** |
| **B2b** | pendulum righting | `comfort.right_stand_shift_frac` | 1.0 | `SEADS_SLED_STAND_SHIFT` | `app/main.cpp` **only** |
| **B3** | tail swing with lean | `params.track_lat_slip_shed` | 0.0 | `SEADS_SLED_TAILSHED` | `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`, `test/harness/sled_tape.h` |

**THE LAW THAT MAKES THE ENV ROUTE SAFE, AND IT IS NOT OPTIONAL.** Both **new** fields go into their
tape roster in the same commit that adds the field — `rolled_throttle_frac` → `SLEDTAPE_COMFORT_D`,
`track_lat_slip_shed` → `SLEDTAPE_PARAMS_D` (`test/harness/sled_tape.h`). `traction_mu` and
`right_assist_max_ms` are **already** rostered. Both new identities are the struct default, so every
older tape replays byte-identical and `dial_gap` reports the new names honestly.

**The three edits, in one line each.**
* **B1** — `sim/sled.cpp:292-293`: split the ternary into `thr_in = hands_on ? clamp01(in.throttle) : 0.0;`
  then `throttle = s.rolled ? p.comfort.rolled_throttle_frac * thr_in : thr_in;`.
  **`!hands_on` must keep forcing an unconditional zero** (R4a §7.4) — that half is not this dial's
  business.
* **B3** — hoist the six lines that build the track's longitudinal slip from `:1120-1145` (inside
  `if (g.is_track)` at `:1114`) to just above the lateral bite at `:1056`, **read** them below, never
  recompute; then, after `mu_l` is assigned and before the `tanh`:
  `if (g.is_track && p.track_lat_slip_shed > 0.0) mu_l *= std::max(0.0, 1.0 - p.track_lat_slip_shed * std::abs(trk_slip) * (1.0 + align_m));`
  **Tripwire (corrected by the red-team):** `grep -c "v_track - v_fwd) / std::max(v_track" sim/sled.cpp`
  must be **1** — the naive `grep -c "(v_track - v_fwd)"` returns **2** on the shipped tree and would
  red a correct edit or delete `roost_thrust`'s `v_rel`.
* **B0 / B2a / B2b** — three `std::getenv` lines beside B1's, in the `SEADS_GRIP_CAPACITY` shape,
  placed **before** `sim::SledState sled;` so the value the tape records is the value flown.

**The proof obligations, with owners.**
1. **The hoist proof is a human command, not a ctest leg.** Run on the lane's own build, **before the
   branch is written**: `<lane-build>/seads_sled_probe.exe tape D:/flight_sim2/seads-recon/build-play/sled_tape_<N>.sledtape`
   for **N = 86…91** — each must print `VERDICT: bit-exact` **and `dial gap: none`**, exit 0. Tapes
   read-only. `test_sled_tape.cpp:196` does **not** test this (it drives in memory); the corpus replay
   lives only in `tools/sled_probe.cpp:2389`.
2. **The hermetic surrogate the gate can carry (new, owed):** `sled_tail_shed_hoist_is_a_pure_refactor`
   — synthetic 600-tick `drive()` with the dial absent, final `SledState` pinned as a golden.
3. **THE WALL, run as a GATE before the drive, not as a leg after it:**
   `sled_onside_recovery_is_momentum_not_magnetism` (`test/unit/test_sled.cpp:2649`) **at B1's driven
   value** — a downed machine must not drive itself upright on track thrust.
4. **The leg that decides B2 (new, owed):** `selfright_a_timed_rock_beats_a_mistimed_one` — same 180°
   start, same STAND cadence, `lean_lat` square wave in phase vs 180° out of phase with
   `angular_vel.z`; the in-phase arm must right in strictly fewer seconds. Run it at **1.0 too**, as
   the positive control: at 1.0 it should show **no** discrimination.
5. **Plus:** `sled_rolled_throttle_frac_zero_is_the_shipped_zero` written on `engine_rpm` /
   `belt_speed_ms` **with the ≤ 3.0 m/s clause**; `sled_rolled_throttle_never_reaches_a_handless_rider`;
   `sled_tail_shed_is_track_only`; the straight-line leg **only** on a hermetic `slip_ang == 0` fixture
   (as first written it is guaranteed red, because the shed applies with no lean at all);
   `sled_wrong_way_lean_is_never_a_penalty` unmoved. `sled_track_lat_mu_is_the_rostered_value` was
   re-read this pass: it builds `const sim::SledParams p;`, the shed is 0.0 inside it — **discharged,
   no action.**

**THE DRIVE — seven launches, one variable at a time, tape every one** (full checklist with his keys:
`ladder_v2.md` §4.5). **At the top of his sheet, in these words:** *"If the rider is off the bars, W is
MEANT to do nothing — check the rider before you judge the dial."*
1. **baseline**, no env var — must feel exactly like main, or stop.
2. **B0 `=3.0` alone** — did anything change at all? (his 2026-08-24 ceiling ruling, finally tested).
3. **B1 `=0.15`, B0 still armed** — (a) gentle tip-over at walking pace, hands on, hold W two seconds;
   (b) then the bank strike. Two questions: is the **noise and roost alone** what he wanted, and does
   he want her to **move** — and how far.
4. **B2a `=4.0` alone** — press SHIFT while she is **still sliding**. Is she trying to come up where
   she used to ignore him? Does she come up **too easily**?
5. **B2b `=0.5`** — press-and-release on a rhythm, swing the mouse in time. **Does the timing start to
   matter?**
6. **B3 `=0.15`, its own run** — hold the throttle open through a leaned corner; then the same corner
   with no lean. **TWO answers owed, written separately: (i) did the tail come around, (ii) DID SHE
   ROLL MORE.** (ii) outranks (i); if it is worse the fix is the lean-gated fallback form
   (`(1.0 + align_m)` → `align_m`), not a smaller number. 0.3 next, 0.5 last.
7. **all of them together**, one line for the combination.

**Lane hygiene, non-negotiable:** fresh lane off current main in **its own worktree** — never in
`D:/flight_sim2/seads-recon`, his fly tree; announce to the sentinel **before any push to main**;
build `build-play`, launch it for him, and **put the checklist in the reply with the absolute exe
path the lane actually built.**

**⚠ WHAT THE FIRST BUILD IS NOT.** Not a landing (landing each dial means the six TOML/loader touches
and a red-team in front of it), not `class_blend_m` (rung 6, another lane, another night, on
TrailMain), not `throttle_power_frac` (rung 4, ruling owed), not `k_air_shift` (rung 5, ruling owed).

---

## §E — RULINGS STILL OWED (full one-liners: `ladder_v2.md` §5)

**Blocking a rung:**
1. **N5-PAIR (blocks rung 5)** — `k_air_shift` gives roll and yaw in the air, but **throwing your
   weight aft pitches the nose DOWN** (correct physics, opposite of expectation); the lever that
   lifts it is the throttle (`k_gyro_react`, built, shipped at 0.0). **Pair armed together, or
   `k_air_shift` alone with the nose-drop as a known?**
2. **N4-PAIR (blocks rung 4) — NEW this round** — *"all or nothing"* needs an **input** curve and an
   **output** ceiling: the cap scale leaves the bottom of the travel a track **brake**, scales every
   **transient** (including your launch, and your 0.22–0.35 s taps), and, because the cap is
   symmetric, cuts the track's **brake** too. **The pair, or neither?**
3. **R2 (blocks rung 6)** — `class_blend_m` 0.0 vs 1.0, reserved for you at `LANES.toml:939`. Nothing
   to build. **And it changes the man on foot at every corridor edge.**
4. **R7 (blocks the dropped pitch rung)** — do you want a floor under the end-over-end at all? **Your
   morning words point away from it.** Say so and it dies cleanly.
5. **R7-class word (blocks rung 2's second half)** — should the self-right measure "tipping over"
   against the **hill** instead of the **planet** (`sim/sled.cpp:1681` `acos(up_body.y)`)?

**New, raised this round — none blocks the first build:**
6. **B2-GATE** — `right_assist_max_ms` 1.3889 m/s is the measured blocker (armed 20.3 % / 35.6 % of
   the time you are over). Widened for the drive by env; **do you want it widened for good, and to
   what?**
7. **B1-CEILING** — confirming your 2026-08-24 `traction_mu` ruling may be **armed on the same night**
   as the rolled-throttle dial, because a spinning track on an unloaded machine is exactly the 55 N
   case that ceiling exists for.
8. **N2-CONFLICT** — 2026-08-26 you ruled *"I dont understand why yo are clonlating the lean mechanism
   with the righting"*; this morning you asked to **right by rocking your body**. **Which wins?**
9. **C2-PRESS** — the C2 side-righting term has **no press gate at all**. Does *"not automatic re
   righting"* reach it?
10. **RC-3 / drift character** — the direction of rung 3 is ruled; the **character** of the resulting
    drift is a drive answer. Say it after run 6.
11. **B3-vs-BANK** — rung 3 and your bank-strike complaint pull in opposite directions by
    construction. **If run 6 makes the bank strikes worse, which one do you want?**

**Carried from the packet, untouched this round:** R3 second half (the `rolled` latch is a hard edge
at 75.06° with no release hysteresis, inside the hull's own 65–75° resting band); R5 (the drift ladder
and the grip trade); R6 second half (the STAND self-right has **no contact gate**); R8 (your mouse CPI
— **no lean-scale number can be picked without it**); R9 (lean frozen while the Sting is shouldered);
**R10, R12, R13 — REPORTS, not questions** (`4106 N` vs the bytes' `3246.0 N`; a `scenario.toml`
comment describing behaviour you had deleted plus a required-but-never-read key; `track_rail_half_m`
documented 0.14 and running 0.19 since 2026-08-12, the trip band's signed 50/60 vs shipped 60/70,
`right_assist_nm`'s comment deriving 1500 against a shipped 2400); R14 (braking at 0.80 g on snow
against LITERATURE 0.4–0.5).

**And the two REPORTS this round created — §B lines 4 and 6 — are reports against numbers this audit
published to you yesterday.** *(R1-NEW: corrected in the packet, or left standing beside the
correction?)*

---

## §F — WHAT THIS ADDENDUM REFUSES TO CLAIM

1. **That any of it is FELT.** Nothing was built, probed or driven this round. Every "invariant" is a
   test to run. **THE FEEL IS SIGNED — picking a value is his seat.**
2. **That rung 3 is safe.** It is the one rung that can make his loudest *old* complaint worse; it
   says so at the rung, and run 6 asks the question that would kill it.
3. **That X1's re-score is final.** Every number in §B lines 4–6 carries the 2.0 s merge gap as its
   killing mutation, and `hands_on` is unpinned, so every pump and righting count is an upper bound.
4. **That rung 6 is demoted on merit.** It is not. It is the strongest measured finding in the audit
   and it is another lane's A/B.
5. **That the env-var route is the landing route.** It is the **drive** route.
6. **That the red-team's P1-5, P1-6, P1-7 and P2-1 bodies are its own.** They arrived as one-line
   verdict labels; the reasoning folded into `ladder_v2.md` is **this lane's re-derivation from the
   shipped source**, and it is marked as such in the FOLD LOG.
