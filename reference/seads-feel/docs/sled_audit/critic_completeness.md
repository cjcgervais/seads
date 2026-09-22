# COMPLETENESS CRITIC — what is MISSING from the sled ride audit

Read-only pass over every file in `D:/seads_sandboxes/sled-audit/docs/sled_audit/`
(D-A, D-B, D-C, D-D, D-E, R1/R2/R3/R4 + their four refutation passes, the three ladders,
the three judges, both red-teams, `ladder_synthesis.md`) plus `tools/sled_tape_audit.py`,
`generated/gate/known_reds.txt` and spot source reads. **Nothing was fixed.** Nothing built,
no dial, no config, no golden, nothing pushed. This file is the only one this pass wrote.

"Synthesis" below = `docs/sled_audit/ladder_synthesis.md` (REVISION 2, the fold-applied merged
ladder), because that is the document that goes in front of Chad.

Ordered by what it costs to close, not by drama. Each item names **where it lives**, **what the
synthesis does with it (nothing, unless stated)**, and **why it is a gap** under the audit's own
rules.

---

## A. CHAD'S WORDS ON FILE THAT NO RUNG, RULING OR OMISSION LINE ADDRESSES

**A1. ★ "EVERYTHING IS GOOD ON THE SNOWMACHINE DRIVE TEST" (2026-08-21, D-C §1 / S6).**
D-C calls this *"the only unqualified sled drive sign in the whole record after drive 4."*
The synthesis's §0 scoreboard grades the machine against the 2026-08-12 §0/§0b bar and marks
"roll resistant — **no**", and nowhere mentions that nine days later he signed a drive off
without qualification. The audit's headline framing ("she rolls 4.32/min against a ruling that
she should not") is built on the earlier sentence alone. Either that sign is about the ride and
it belongs in the §0 scoreboard, or it is about K-WS1's pose ladder only and the synthesis must
say so. `grep -c "EVERYTHING IS GOOD" ladder_synthesis.md` = 0.

**A2. "Corkscrew is alive and well." (2026-08-13, D-C O2) and the RC1 bar
"360-spin a road WITHOUT rolling" / "post-roll spins like a top ~10x" (2026-08-12, D-C §1).**
Zero occurrences of "corkscrew" or "360" anywhere in the synthesis. O2's fix
(`release_floor_frac 0.3`) is on main and unflown; his own stated acceptance test (the 360 on a
road) is never proposed as a drive line, and C3 (the yaw-arrest term, `side_yaw_mu`) is never
graded against it. No rung, no ruling, no "not this audit" line.

**A3. "WOT should lift the skis to ~30 deg" (felt item 4, D-C O4).** Appears twice in the
synthesis and both times as an *aside* inside another rung's law check ("already unpaid at a
measured p50 3.6 deg"). D-C O4 states the mechanism question — *"thrust is pegged at
`max_thrust_n 2272` and rpm at 8000 through the whole window, so the ask is not a thrust
shortage"* — and nobody carries it. Unpaid felt item, no rung, no omission line.

**A4. "should be able to launch in the air" (felt item 2, D-C O11).** The synthesis §6 item 8
**refuses** the "29 % land upright = a failed jump" reading (correctly — 93 % of counted air
events are sub-second hops) and then leaves his actual ask unanswered, with no successor
measurement and no "not this audit" line. Refusing the wrong reading of a word is not the same
as addressing the word.

**A5. "The conditions should be palpable and observable in the sled performance"
(`docs/roost_consult_packet.md` §1).** Quoted verbatim in the synthesis §0 roster and then never
cited by a single rung. It is the sentence D-D's channel table exists to answer (C7/C12 roost bar
vs drawn spray, the surface slip cue absent on hard surfaces) and the synthesis carries none of it.

**A6. The buck ruling is a word about LANDING HARDNESS, and the synthesis says there is none.**
§3 refuses the landing-damper knee because *"Chad has never complained about landing harshness;
he asked for punchy … Not a rung until there is a word."* But D-C §1 (2026-08-29) records, verbatim:
*"If it is too big a bump he may land in superman pose, that second, unseated hit from the landing
would then cause his grip to let go … the hardness of the landing casue for letting go or the bars
and falling off to the side."* That is a word about landing hardness, and `R4-landings.refute.md`
measured the mechanism it names (R4-19: **the kernel's rider absorbs exactly zero landing energy
on any path** — `sim/rider_grip.h` is a one-way instrument, no force returns to the body). The
refusal may still be right; as written it rests on a claim the record contradicts.

**A7. "more places with a bit less snow makes the sled go faster around there" (2026-09-10, D-C O13).**
Not built on main; absent from the synthesis entirely. No rung, no omission line, not routed.

**A8. "I think we should build it. Because then it has the foundation it needs." (gyro, 2026-08-27,
D-C S9/O6).** Ruling **R6** asks him *"Do you want to steer in the air at all?"* — a ruling he has
already given in the affirmative for the build; what is unruled is the **value** (`k_gyro`,
`k_gyro_react` ship 0.0, *"built, not approved"*). Re-asking a taken ruling as if it were open is
the failure mode memory `chad-take-the-ruling` names. The R6 text should ask for the **value and a
drive**, not for permission to build.

**A9. `right_seed_frac`'s config comment still describes the behaviour he ruled OUT (D-A §5).**
`config/scenario.toml:2098-2099` reads *"a full LEAN counts like 10 deg of attitude error — his
lean is what picks the side"*, which is exactly what `31e0dd96a` deleted after his v4 drive
(*"I dont understand why yo are clonlating the lean mechanism with the righting"*). The shipped
field now multiplies the latched brace. This is a lying instrument standing against a ruling of
his and it appears nowhere in the synthesis — not even as a report like R10.

---

## B. TAPE METRICS NAMED BY A STRAND AND NEVER COMPUTED

**B1. ★ X1, X2 and X3 — the three "free, read-only, one pass" legs — are ALL unimplemented.**
`tools/sled_tape_audit.py` has no exit-surface attribution, no `rolled` edge counter, and no
`in.steer` decay reader (`grep -n "exit_surface|rolled_edge|in_steer" tools/sled_tape_audit.py`
returns nothing). The synthesis is honest that *"None has been run"* — but:
- **X1 is the decisive killer of Rung 1's tape support**, named by all three ladders, by both
  red-teams (`redteam_mechanism.md` §5 item 1: *"the single cheapest thing on the board"*), and run
  by none. Rung 1 is rank 1.
- **X3 is a stated PRECONDITION of Rung 3** and the synthesis says Rung 3's *"rank-3 position does
  not stand until X3 has run."* It did not run. Rung 3 is presented at rank 3 anyway.
- The audit's own hard rules permit adding files under `tools/`, and the resumed run owned that file.

Three free legs, one of which can delete a rank-1 rung and one of which can delete a rank-3 rung,
were specified and not executed.

**B2. The backflip/rollover separator instrument (D-A §8 item 1) has no leg.** D-A states exactly
what closes ruling **R4**: a joint instrument (`air_s > 0` at entry AND pitch-dominant rotation) in
`tools/sled_tape_audit.py` — *"D-B's file, not mine."* The synthesis makes R4 a **ruling** and
hard-gates Rung 4 on it, but never schedules the instrument. Even after Chad rules "a backflip is
not a rollover", no metric in the corpus can apply his definition. Rung 4's two-sided invariant has
no denominator and no path to one.

**B3. The `mount_or_other` override class — 9 records, uninspected** (`R4-landings.refute.md`
FIXES OWED item 1). Pooled `override_classes` are `{episode_start: 84, autoright_R: 146,
mount_or_other: 9}`. The refutation states plainly that R4-04's killing mutation ("only two classes
exist, so this is closed") is **false as written** and must be re-run against those 9. The synthesis
builds Rung 3's whole invariant on `O` records and never mentions the third class.

**B4. Braking authority — D-B-8's number is flagged "for R1" and reaches no ruling.** MEASURED:
braking decel p90 **7.85 m/s² = 0.80 g on snow** against a LITERATURE 0.4–0.5 g, and brake duty is
**8.7 %, the highest of any bucket** (up from 3.8 % at v13g). Chad signed *"Brakes work on the road
pretty good now."* (D-C S3). The synthesis contains no ruling, no rung and no omission line for it;
`brake` appears only inside other rungs' prose. A measured 2x-real number on a **signed** dial is
exactly the class charter §2 says gets reported.

**B5. Rollover rate per surface at EXIT, and the surface-controlled kernel comparison** (D-B-11:
*"the surface-controlled comparison — same surface, same speed band, across tags — is owed and is the
right instrument"*). Owed by D-B, carried as a refusal in §6 item 4, never scheduled as a leg.

**B6. D-E-05's own killing mutation is a free test and is not on the leg list.** *"raise
`px_full` from 300 to 600; band entries per minute must fall for the same driving; if rollovers/min
do not follow, the correlation was ride intensity and this finding is wrong."* The synthesis refuses
`lean_px_full` on significance grounds (correct) but drops the test that would settle it.

**B7. `rd.speed` while mounted (D-D UNVERIFIED-3).** D-D-R2 is **blocked** on measuring whether the
aeroplane's airspeed is ~0 while the player is on the sled. Neither the measurement nor the rung it
blocks survives into the synthesis (see D3).

---

## C. REFUTATIONS THAT WERE RUN AND THEN IGNORED

**C1. ★ `R3-player-experience-REFUTE.md` F3 — the censored count is one low in EVERY variant.**
The refuter re-derived R3's own `spd.json` and got **16 scorable episodes, 5 censored**, not 15/4,
*"and the offset is exactly +1 in all eight sweep rows."* The synthesis Rung 4 still reads
**"4 of 15 episodes right-censored (so 7 s is a floor)"**. An explicitly refuted count, quoted
unchanged, inside a rank-4 rung's tape evidence.

**C2. `R3-…-REFUTE.md` F1 — "Publish the probe" — not done, and it now infects the fold.**
R3's three headline numbers (16 episodes, p50 7.02 s, the 15/52 four-predicate invariance) exist only
in an ephemeral session scratchpad (`r3_probe.py`, `marks*.json`, `spd.json`, `rec_A..D.json`). The
refuter could verify them only because that directory *happened* to survive, and asked for them to be
copied under `docs/` (which the hard rules permit). `ls docs/sled_audit/` shows **no** `r3_probe.py`
and **no** `marks.json`. Same class, same pass: D-D's scripts (`dd_channel_probe.py`, `dd_pass2.py`,
`dd_roost.py`) are scratchpad-only by D-D's own admission, and **both red-teams' tape passes were
never published either** — the synthesis §6 item 9 admits it *"did not re-run their tape passes."*
So the ladder's newest and most decision-changing MEASURED numbers (Rung 4's `wv == 0` fractions,
Rung 7's 2.32 % armed duty, Rung 8's 156/209/3010 band occupancy) are **unreproducible from anything
in the worktree**. Only `de_input.py` and `tape_summary.json` are published.

**C3. `R3-…-REFUTE.md` F5 — R3-M3 is a corroboration of R3-M1, not an independent finding**
(1 − 15/52 = 71 % is arithmetically the complement of 3.25 crossings/episode). Rung 4 cites
*"D-B-2 / R3-M3 / PJ read A"* as three converging sources for the 71 %. Two of the three are the same
arithmetic.

**C4. `R1-real-dynamics-REFUTE.md` §3 — R1-26, which the refuter calls "the strand's best finding",
is used as a killing mutation and never REPORTED.** `track_rail_half_m` went 0.14 → 0.19 in
`9e7e48a12` with the surrounding comment left untouched, so *"the file has said 0.14 and run 0.19
since 2026-08-12"*, and `ROLL_COMFORT_HANDOFF.md` §1 — **the charter** — still prints 0.14. The
synthesis §1(a) uses 0.14 only as a mutation ("at the comment's real-Indy rail half … 0.452 g /
24.3 deg") and never files the documentation defect. This is the same shape as ruling **R10** (the
gi4 handoff's 4106 N), which the synthesis *did* file. One doc-vs-bytes drift reported, an identical
one silently consumed.

**C5. `R1-…-REFUTE.md`'s closing escalation — "a measurement to take, not a change to make" — has no
leg.** The escalation is that ski `mu_lat` 0.55–0.72 against SSF 0.510 g makes a low-speed
flat-ground **untripped** friction rollover available by construction, and it calls this *"the
cheapest live candidate for Chad's actual complaint that this audit has produced."* The synthesis
adopts the refutation into §1(b), qualifies it with RT-B P2-13's `tanh` correction — and then §6
item 5 admits *"that §1(b)'s 'available by construction' has been observed"* is **not** claimed, with
X1 offered as the proxy. X1 measures surface attribution, not friction rollover. **No probe leg in §5
takes this measurement.** L1 (hands-off full lock on TrailMain) is adjacent but is written for RC-1.

**C6. The gate's own red on this exact proposition is never mentioned.** `generated/gate/known_reds.txt`
carries `snow  sled_slides_before_it_tips_on_flat_snow` — a baseline red for five weeks, whose *name*
asserts the proposition R1-10's refutation overturned. It sits beside
`sled_grip_ceiling_stays_below_the_tip_threshold` and `sled_assist_reference_plane_is_load_weighted`,
both squarely inside this audit's subject. `grep -c "known_reds|slides_before_it_tips|grip_ceiling"
ladder_synthesis.md` = **0**. This is D-C **O16** and it is invisible in the merged ladder (see E).

**C7. `R2-game-feel-REFUTE.md` §3's corrected A/B is absent.** The refutation resolved R2-H2's sample
selection (publish all six 09-17 tapes: override delta **0.96 → 0.42/min, 2.3x**, not 4.2x) and
confirmed both of its conclusions — **severity collapsed** (longest episode 53.60 s → 5.09 s;
episodes ≥5 s 5 → 1; rolled share 4.4x) while **frequency did not move**. That is the single largest
piece of evidence in the corpus that the shipped comfort stack *works on severity*, and the
synthesis's §0 scoreboard ("roll resistant — no · land on skis — no movement") never carries it.
Nor does §6 list it as a refusal.

**C8. `R2-…-REFUTE.md` §8 item 2 — "Ask Chad what he was doing in tapes 86-91 before any of them is
used as a free-riding baseline."** Ruling **R1** asks only for a felt word per tape ("good / rolled
too much / didn't notice"). It does not ask the question the refuter says must come first: were
86 and 88 (the two highest-onset-rate drives of the day) free riding or deliberate rollover tests?
Every v17 number in the audit assumes free riding.

**C9. `R4-landings.refute.md` — the corpus-homogeneity caveat is not carried.** *"It is NOT established
for anything the terrain-clip lane touched, and no claim in the strand should be read as covering
`facet_contact`."* Tape 91 is a different build (`-53-g7b377c6f0`) from 86–90 (`-33`), is the only
tape with Road/TrailMain, is by a wide margin the worst, and is quoted in **five** of the eight rungs.
`grep -c "facet_contact" ladder_synthesis.md` = 0.

**C10. D-D-F5 contradicts D-B-9 and nobody adjudicates.** D-D: *"`g_eff`'s pooled max of 1 766 m/s² is
a one-tick numerical artefact at a tape discontinuity (respawn / `O` record)"* and any future g-driven
cue needs the `epoch` guard `render/trail_chain.h` already uses. D-B §3 prints 1766 (tape 88) and 2561
(tape 16) as per-tape maxima and D-B-9 calls the per-tape `max` the only trustworthy figure up there.
One strand's measured artefact against another strand's trusted number; the synthesis carries neither
and reconciles nothing.

---

## D. WHOLE RUNGS AND WHOLE STRANDS THAT VANISHED WITHOUT A LINE

The synthesis states *"Four rungs were dropped for the 8-rung budget; §2 names each and why"* and §3
is headed *"WHAT THIS MERGED LADDER DELIBERATELY DOES NOT DO"*. Of the 24 rungs on the three ladders,
**two are in neither the ladder, the drop list, the refusal list nor the omissions list.**

**D1. ★ PLAYER-JOURNEY PJ-5 — "The roll he is grading never reaches his eye"
(`render::kSledCamRoll`, identity 0.0).** MEASURED: `app/main.cpp:9827` is `pose.up = sup;`
**unconditionally**; tilt past 20 deg on **23.42 %** of v17 ticks and past 45 deg on **8.92 %**;
`app/rest_horizon.h` — the v13 horizon-roll law **already shipped and shared by the aeroplane AND the
Sting** — is not included by the sled path at all. Chad's word: *"she's too unsteady … I rule that it
should be roll resistant"* — **roll is the quantity he is grading, and it never reaches the eye.**
`grep -c "PJ-5|horizon|kSledCamRoll" ladder_synthesis.md` = 0. Not a rung, not dropped, not refused.
(It carries its own honest risk flag — the reverted R1 motion-blur FX, D-D UNVERIFIED-2 — which is a
reason to rank it low, not a reason for it to disappear.)

**D2. ★ PHYSICS-HONEST RUNG 5 — the friction circle (`[sled_comfort] lat_combined_frac`,
identity 0.0).** The **only** rung in the audit that proposes to touch RC-3's drift mechanism, and the
direct constructive answer to R1-19's refutation. The synthesis adopts the refutation into §1(e)
(*"there is no friction circle in this kernel"*) and uses it as the **reason** Rung 7 makes the drift
audible instead of tuning it — while never naming the rung written on the other side of that same
finding, nor its measured sizing (TrailMain `T/budget` 2272/4974 = 0.457 ⇒ track lateral mu
0.70 → 0.623 at WOT), nor its own honest limit (inert on Bush, i.e. 49 % of his driving). Judge 2's
graft list does not cover it either. It may well deserve refusal — but under RC-5 and this audit's own
rules it must be refused **out loud**, as `lat_mu_scale` was.

**D3. Three of D-D's six candidate rungs are gone, and D-D's five DEFECTS with them.**
Only D-D-R1 (rpm blend), D-D-R3 (slip voice) and D-D-R4 (assist tell) survive into the synthesis, the
first two as drops and the third as Rung 6. Missing entirely, with no drop line:
- **D-D-R2 / C11 — the sled has no wind and no sound keyed to its own speed.** `wind_synth` is driven
  by `rd.speed`, **the AEROPLANE's airspeed**, with no mount gate, while **44.48 %** of ticks are above
  8 m/s. This violates `README_SOUNDSCAPE.md` §2's standing doctrine by name.
- **D-D-R6 / C13 — the track never turns.** `belt_speed_ms` runs a mean **20.50 m/s faster than the
  ground** and has **zero consumers** in `render/` or `app/`; `render/sled_model.cpp`'s 7 687 lines
  contain no belt/cleat/scroll channel. The half of PJ-4 the synthesis kept is the *sound*; the half it
  dropped without a word is *the track is visibly still while it is spinning*.
- **D-D-R5** = D1 above.
- **The defects themselves** — D-D-F1 (`sim/sled.h:1307-1310` names a consumer that does not exist),
  D-D-F2 (`render/draw.cpp:2674-2676` *"the bar and the spray are the same number"* is false; measured
  mean ratio 0.838, spray effectively absent on 6.84 % of bar-lit ticks), D-D-F5 (the `epoch` guard) —
  are lying instruments in the same class as R10 and A9, and none is reported.
- **C5 — there is no camera shake / g cue at all** (`g_eff` p99 3.35 g, 2.65 % of ticks above 2 g) and
  **C4 — the FOV is constant at every speed**. Both named as holes by D-D; neither survives.

**D4. D-A §3.1(b) — the player brake is discontinuous at a stop.** `sim/sled.cpp:1250-1256` applies
full brake force whenever `v_fwd > 1e-9` with **no taper**; the force can drive `v_fwd` negative inside
one substep and then vanish. D-A gives it a killing mutation (`tanh(v_fwd/v_ref)`). It is on a dial
Chad **signed**. Absent from the synthesis.

**D5. D-A §4.4/§4.5 — the RC-8 violation is used as plumbing advice and never filed as a finding.**
Four `SledComfort` fields are not loadable from TOML at all, and **two of them are live shipped
mechanisms** — `side_yaw_mu = 0.35` (C3 yaw arrest, ON) and `side_right_wref_rads = 2.5` (C2 rate gate,
ON). The whole of `SledParams` is compile-time only: *"12+ named mechanisms Chad cannot A/B from the
seat"*, against RC-8 verbatim (*"EVERY MECHANISM IS A DIAL"*). The synthesis turns this into a
six-touch plumbing note for Rungs 2/3/5 — and then **ruling R11 asks him to rule on
`side_right_wref_rads` and the 40–140 deg window without saying that ruling needs a recompile.**

**D6. D-E-08 — the sled bypasses the house's own mouse-conditioning primitive.** No deadband anywhere
on any sled axis; the sled's lean reads `live.mouse_dx` raw at `app/main.cpp:6425`, **upstream** of the
`input::aim_curve` call at `:6230` that the aircraft aim axis runs (with `aim_curve_quant_px`, a
documented sub-pixel guard). D-E records it specifically because *"nobody has written it down"* and
because it makes a lean deadband a **new** mechanism rather than a tuning. Written down nowhere in the
synthesis.

---

## E. OPEN DEBTS FROM D-C WITH NO RUNG AND NO EXPLICIT "NOT THIS AUDIT" LINE

Checked all 16 OPEN rows and all 8 CONTRADICTIONS in `D-C_felt_history.md`.
Addressed: O3 (named inside Rung 8's killing mutation), O5, O6, O7 (Rung 1), O8 (Rung 5), O9 (named in
§3 omissions), O14 (ruling R1), O15 (partly, via Rung 7 and the assist A/B).
**Unaddressed, with no rung and no refusal line:**

| D-C | the debt | status in the synthesis |
|---|---|---|
| **O1 / C8** | *"I tried a 360, still rolls over way too easily"* — **he has never driven GI3**, and *"the felt record contains no drive of GI3 at all"* | §6 item 3 says the ranking may not survive R1; the **GI3-specific** never-flown fix is never named |
| **O2** | *"Corkscrew is alive and well"*; `release_floor_frac 0.3` on main, unflown | absent |
| **O4** | WOT 30 deg catwalk, p50 3.6 deg, thrust already pegged | aside only (A3) |
| **O11** | *"should be able to launch in the air"* | reading refused, ask unanswered (A4) |
| **O12** | *"the SNOWMACHINE sometimes sinks into the plowed road"* (2026-09-05), routed away, never answered by a sled lane, *"unconfirmed either way"* | absent |
| **O13** | *"more places with a bit less snow makes the sled go faster"* | absent (A7) |
| **O16** | **4 of main's 6 baseline gate reds are sled debts BY NAME, red for five weeks** | absent (C6) |
| **C1** | the trip band drifted from his signed **50/60** to a shipped **60/70** (the prior survey's 70/80 is stale by one phase) | absent — a signed number that moved, unreported |
| **C2** | `right_assist_nm`: the comment in the **same commit** derives **1500**, the file ships **2400**, and *"Nobody has ruled it"* | the A/B (0.0 vs 2400.0) is a §5 leg; the **1500-vs-2400 contradiction and the unruled state are never put to him as a ruling** |
| **C6** | the sled cam taus (0.06 / 0.12) are compiled literals in **no TOML**, overridable only by an env var — another RC-8 hole on a **signed** feature | absent |

Charter §2 (*a finding against a signed number is reported to Chad, never re-derived*) was applied
once, to the gi4 handoff's 4106 N, as ruling **R10**. **C1, C4 (R1-26), B4 and D5 are the same class
and none of them is filed.**

---

## F. CLAIMS IN THE SYNTHESIS CARRYING NO KILLING MUTATION

The document's own standing rule is *"Every numeric claim carries its killing mutation. A claim
without one is decoration."*

**F1. ★ Every MEASURED number the fold imported from the two red-teams.** Each is a headline of the
rung it was folded into, and none carries a mutation, a published script, or a re-run:
- Rung 4: `wv == 0` on **45.8 %** of past-90 ticks / **75.0 %** of the stall window / **89.7 %** of
  tape 91's, and `wo > 0.8` on **1 570 of 1 570**. (The rung's stated mutation is for the *dial swap*,
  not for these counts.)
- Rung 7: **2 011** armed ticks, **1 022 (50.8 %)** with `hull_engage_lp < 0.05`, **839** of them on
  tape 89; armed duty **2.32 %**. (Epistemics are named — 120 Hz vs 1 440 Hz, `hull_engage_lp` as a
  proxy, `grip.attached` unpinned — which is honesty, not a killing mutation.)
- Rung 8: **156** ticks in +20…+60 deg against **209** past +60; **3 010** in −20…−60; **10 316**
  catwalk ticks.
- Rung 6: `assist_active_frac` **0.798 … 0.937**, magnitude p50 5–75 N·m, p95 215–765 N·m.

§6 item 9 states this pass *"did not re-run their tape passes"* — so these numbers are simultaneously
(a) load-bearing, (b) unpublished, (c) unmutated and (d) un-re-derived. That is three of the audit's
own rules at once.

**F2. Rung 3's scale claim: "146 R presses, 144 of them at a body tilt ≥ 75.06 deg."** No mutation.
It also sits unreconciled beside `R4-landings.refute.md`'s independently re-derived
*"146 of 146 carrying `tilt_before_deg`, min **69.3 deg**, p50 111.9, zero below 45"* — a different
count at a different threshold, from a pass that was published.

**F3. The §0 scoreboard** ("roll resistant — no · possible-not-the-rule — no · land on skis — no
movement · self-righting by chance — no · not impossibly hard — no") is the document's opening verdict
and carries **no** killing mutation for any row, while D-B-11 (adopted in §6 item 4) says the
bucket-to-bucket rates it rests on are confounded four ways and *"no kernel verdict may be read off
that table."*

**F4. Rung 2's tape evidence** (full lean **14.8–43.6 %** of every v17 drive, 26.5 % pooled, 44 % on
tape 91; lean holds a partial value for **36.1 s** against steer's 0.500 s) carries no mutation; the
rung's three mutations are all about the dial's mechanism.

---

## G. PROCESS / STRUCTURAL

**G1. The drop rule was never applicable and the synthesis presents it as applied.** *"Drop any rung
two judges scored under 5 on intent_heard"* — but **no judge scored rungs**; all three scored
**ladders** on intent_heard (judge 1: 10 / 7 / 8; judge 3: 9.5 / 7 / 8). The synthesis substitutes
ladder for rung ("No ladder was scored under 5 … so no rung was dropped by that rule") without noting
that the per-rung instrument the rule needs does not exist. Judge 1's per-criterion **lean_enhancer**
scores of **3 and 4** for player-journey and physics-honest are the only sub-5 numbers anywhere, and
they are criterion scores, not rung scores.

**G2. PJ-7 was taken as "method, not rung" on a 2-of-3 judge count, and judge 2 dissented in writing**
(*"PJ-7 **whole**"*, graft #3, with ruling R3 travelling with it, because the flag owns **both** the
throttle and the drone launch — i.e. the battle). The synthesis records the tally, not the dissent's
reasoning. Acceptable call; the dissent should be visible beside it.

**G3. The synthesis's own provenance is unstated in a small, checkable way.** It gives worktree HEAD
`ed0a43ce8`, while D-C §0 establishes that the worktree's local `main` ref is **stale**
(`de523afd1`) against `origin/main` (`533c86409`), and that **every** D-C verification was run against
`origin/main`. Nothing in the synthesis says which ref its own source reads were taken against.

**G4. No file in `docs/sled_audit/` is a drive-ready checklist.** Every rung carries a drive-checklist
line and ruling R1 gates all of them, but there is no single page Chad can be handed with (i) the
one-line-per-tape ask for 86–91, (ii) the live-tunable A/Bs already reserved for him
(`class_blend_m`; `traction_mu` once plumbed), and (iii) the eleven rulings. Memory
`chad-fly-checklist-inline` says the checklist goes **in the reply**, with the absolute exe path.
Nothing here names an exe or a launch path.

---

## WHAT I CHECKED AND FOUND COMPLETE (so the list above is a measurement, not a mood)

- Every P0, P1 and P2 finding in **both** red-teams (`redteam_law_feel.md` 3/5/5;
  `redteam_mechanism.md` 2/10/7) is folded, in place, with a fold-log row. I found no unapplied item.
- All eight of judge 3's grafts and all eight of judge 1's (G1–G8) are present in the merged ladder.
- Chad's §0 / §0b quotations in the synthesis are verbatim against
  `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md`; RT-A checked the same and found the one
  elision marked and immaterial.
- Every rung carries an identity value, a felt problem in his words, a mechanism at file:line, an
  invariant and a drive-checklist line. The fold's source-claim verification table (§7) is real: the
  thirteen re-verified lines are correctly cited.
- `R1-REFUTE`'s four refutations (R1-7, R1-10, R1-19, R1-28) are adopted, not laundered — §6 item 8
  names three of them by number as readings the ladder refuses to use.
- No dial was moved, no config edited, no golden touched, nothing built or pushed by any file in this
  directory, as each one claims.

---

*Completeness critic, sled ride audit, 2026-09-18. Read-only against main. Nothing fixed, nothing
built, no dial changed, no config edited, no golden moved, nothing pushed. The only file this pass
wrote is `docs/sled_audit/critic_completeness.md`.*
