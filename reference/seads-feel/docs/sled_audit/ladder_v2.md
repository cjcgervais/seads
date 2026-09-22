# LADDER v2 — RE-SCORED AGAINST HIS MORNING WORDS
**2026-09-18.** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`.
**READ-ONLY.** Nothing built, no `ctest`, no `seads.exe`, no dial moved, no `sim/` `control/`
`config/` `test/` `golden/` file touched. Tapes in `D:/flight_sim2/seads-recon/build-play` were not
opened this pass. **Files written by this pass: this one.**

**Inputs folded.** `docs/sled_audit/ladder_synthesis.md` (REVISION 2), `docs/SLED_RIDE_AUDIT_20260918.md`
§0–§4, `docs/sled_audit/X1_attribution.md` (leg X1, MEASURED), `docs/sled_audit/NEWASK_mechanisms.md`
(N1–N5, code-only), and **`docs/sled_audit/RULINGS_20260918_chad.md`** — which is the bar everything
below is scored against.

**Confidence vocabulary** MEASURED / DERIVED / LITERATURE / GUESS, on every number.
**Every number names its killing mutation.**

**★ FOLD ROUND 2 (2026-09-18, same day).** A both-lenses red-team of this document returned
**LAND-WITH-FIX** on rungs 1, 2, 3 and on the first build, **DO-NOT-LAND as one dial** on rung 4, and
**LAND — and PROMOTE** on rung 7. **Every P0 and P1 it raised is folded in place below**, each marked
`★ FOLDED P0-n / P1-n`, and **§7 is the FOLD LOG** — one row per item, including the two places where
the fold *corrected the red-team* and the three items that arrived as verdict-table labels with no
body. This round wrote no file but this one: **no build, no `ctest`, no `seads.exe`, no tape opened**
(the tape directory was listed, not read).

---

## 0. HIS WORDS, VERBATIM — the bar

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

---

## 1. WHAT HIS WORDS DID TO THE v1 LADDER — the re-score in ten lines

1. **He named four mechanics and ranked none of the old eight.** Of the v1 ladder's eight rungs,
   **exactly one** (Rung 1, `class_blend_m`) is named by a sentence he wrote this morning — and he
   named it as a *trigger* (*"a ski hitting the bank on one side"*), not as a fix. The other three
   sentences describe mechanics that were **not on the ladder at all**.
2. **R1 answered — all free riding — and that did NOT vindicate the v1 numbers; leg X1 cut them.**
   52 past-90 crossings on those six drives merge into **16 tumble events: 1.33/min, one every 45 s**,
   not 4.32/min / one every 13.9 s. And **88 % of tumble events end back on the skis** (14/16 v17,
   311/353 corpus), not 29 %. MEASURED, X1 §2.3. *Killing mutation:* the 2.0 s merge gap — 1 s gives
   17 events, 3 s gives 16; **no gap ≥ 1 s saves "one every 13.9 s."*
3. **So the two headlines the v1 ladder ranked on are gone**, and every rung whose case rested on
   *4.32/min* or *71 % never recover* falls with them. That is Rung 4, and it is out twice over (line 4).
4. **Rung 4 as written is OUT.** R11: *"self righting with a press … (not automatic re righting)"*.
   The C2 side-righting term (`sim/sled.cpp:1545-1607`, verified this pass) has **no `in.stand` gate
   anywhere in it** — it rights a downed machine with no press at all — and `side_right_vmin_ms` is the
   dial that would widen it. Lowering it makes the machine *more* automatic, in the exact direction he
   just ruled against. Dead as written.
5. **The backflip ruling is real but small at the event level.** FLIP_AIR is 21 % of crossings and
   **6 % of tumble events** — 1 of 16 last night. With flips removed the rate is **1.25/min**.
   MEASURED, X1 §2.2. His *"maybe only 10 %"* is about deliberate bank *hits*, a different set again.
   It does not re-rank anything; it removes an objection from a rung that is already out.
6. **His throttle report is exactly right and the number is worse than the complaint.** Over 206
   minutes the thumb is held at an intermediate value for **159 ticks — 1.3 seconds — 0.011 % of
   drive time**. MEASURED, X1 §5.1. The middle of the lever is a *track brake* below `v_fwd/46`
   (22 % of travel at 10 m/s), the live band is **8 %**, and the top 70 % is flat because
   `max_thrust_n` does not contain the word `throttle`. That promotes a rung that was not on the
   ladder at all.
7. **The pendulum he asked for is already built, armed and almost never reachable.**
   `right_assist_nm = 2400` ships and is in all six v17 tape headers (MEASURED). But the gate is open
   **13.4 % of past-90 time** — the blocker is the 1.3889 m/s speed gate, not the press — and the
   bodyweight half is eaten by a clamp (`sim/sled.cpp:401-406`, verified this pass). **The ask is one
   number he already owns**, and it is the sentence he spent the most words on.
8. **Rung 1 SURVIVES X1 and survives it larger.** At the **takeoff** — the only moment upstream of the
   whole event — TrailMain runs **11.98 tumble events/min against Bush 0.50**, 24×, and **147 of 237
   trail launches (62 %) had the two skis on different surface classes within 0.5 s**, with **not one
   of the 237 ever launching without one**. MEASURED. But it is still a **drive, not a build**, it is
   still **road-repair's A/B** (`LANES.toml:939`), R2 is still open, and **v17 carries only 0.45 min
   of trail** so last night barely tests it.
9. **"More dynamic with bodyweight" is the through-line of three of his four asks** — the rock, the
   tail swing, the air. That is what re-ranks the ladder: the top of v2 is the body, not the boundary.
10. **Three of the top four are new, and all three can be driven tonight.** That is §3.

**⚠ And one thing his words did NOT do.** He said *"deliberate jumps for performing a flip off of a
banks for fun"* and *"a backflip … is not a roll over."* **That is him defending the flip.** The v1
Rung 8 (`pitch_arrest_nms`, a floor under the end-over-end) therefore moves **down**, not up — a new
kernel torque that arrests nose-up rotation is the one rung his morning words point *away* from.
R7 remains owed and is now owed harder.

---

## 2. THE LADDER — EIGHT RUNGS

**Identity value = the number at which the build is bit-identical to today.** Every rung is a branch
or a multiply-by-zero, never a 0-weight lerp (the `kernel-v14-leanlead` precedent). One dial each.

| # | rung | THE ONE DIAL | identity | touch surface | **tonight?** | his 2026-09-18 sentence |
|---|---|---|---|---|---|---|
| **1** | **The key has to reach the track** | `[sled_comfort] rolled_throttle_frac` | **0.0** | `sim/` + `app/` (env) | **YES** | *"if I key press throttle should ramp up"* |
| **2** | **The rock has to reach her** | `[sled_comfort] right_stand_shift_frac` **+ `right_assist_max_ms` as its precondition (P0-4)** | **1.0** (shipped) / **1.3889** (shipped) | `app/` (env) — **zero kernel, zero loader** | **YES** | *"self right by rocking bodyweight back an fourth … gain pendulum momentum"* |
| **3** | **The throttle has to swing the tail** | `track_lat_slip_shed` | **0.0** | `sim/` + `app/` (env) | **YES** | *"throttle should also be able to swing my tail around on account of the roost, esp with weight shifting"* |
| **4** | **The thumb has to have a middle** | `throttle_power_frac` | **0.0** | `sim/` + `app/` (env) | **NO — N4-PAIR ruling owed** (P0-3/P1-6/P1-7) | *"throttle is all or nothing"* |
| **5** | **The body in the air** | `k_air_shift` | **0.0** | `sim/` + `app/` (env) | **NO — one ruling** | *"jump weight shift influence in air"* |
| **6** | **The seam that spins you** | `[snowpack] class_blend_m` | **0.0** | `config/` — **no build at all** | **NO — R2, and it is another lane's** | *"a ski hitting the bank on one side"* |
| **7** | **A track cannot push harder than the snow it stands on** | `traction_mu` | **0.0** | `sim/` + `app/` (env) | **YES — PROMOTED to B0, it is rung 1's CEILING and rides B1's night** (P0-2) | — (his 2026-08-24 ruling, taken, unpaid) |
| **8** | **The lean has to buy the arc** | `plane_lat_lean_gain` | **0.0** | `sim/` + `app/` (env) | **YES** (after rung 3's drive) | *"Need to make it more dynamic with bodyweight"* |

**Reading the "tonight?" column.** YES means: the dial exists or is three touches away, the identity
value is bit-identical, a kill env var is one line, and **no ruling he has not given stands between
the lane and the drive.** NO names the ruling.

---

### RUNG 1 — THE KEY HAS TO REACH THE TRACK · `rolled_throttle_frac` 0.0
**His words.** *"throttle is cut unless im key pressing but if I key press throttle should ramp up"*.
Attached, §0b: *"DOnt make it impossibly hard, there is a batttle going on as well."*
**Touch surface.** `sim/sled.cpp` (one line), `sim/sled.h` (one field), `app/main.cpp` (one env line),
`test/harness/sled_tape.h` (one roster entry). **No `config/`, no `render/`.**
**Mechanism.** `sim/sled.cpp:292-293`, verified this pass:
`const double throttle = (s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle);`
**The app already does what he asks and the kernel already throws it away** —
`app/main.cpp:8291-8293` ramps the thumb at 2.5/s up, 6.0/s down and `:8447` ships it unconditionally;
the app never reads `sled.rolled` on that path. DERIVED, structural: **the fix cannot live in `app/`**,
because the app cannot undo a zero applied downstream of it.
**And the blast radius is bigger than thrust.** `throttle` at :292 feeds five consumers: the CVT blend
(`:1137`), commanded track speed (`:1140`), `roost_thrust` through `flux` (`:1156, :1182`), the
engine-brake gate (`:1234`) and the **rpm/belt readout** (`:1341-1361`). So today, on his side, W
produces **no motion, no roost, and idle rpm** — *no feedback of any kind that the key was received*.
That silence is the whole felt signature of *"throttle is cut."* DERIVED, exact.
**The half that is already his.** `rolled` needs the >75° attitude to hold `rolled_persist_s` 0.30 s
**in ground contact** (`sim/sled.cpp:246-259`), so a backflip cannot latch it — his R4 ruling is
already honoured *in the kernel*; it was the audit's *counting* that conflated them.
**★ FOLDED P0-1 — THE WHOLE PAYOFF IS GATED ON `grip.attached`, AND THAT LATCH IS ONE-WAY.**
`sim/sled.cpp:291` reads `s.grip.attached`. The only kernel writer is `sim/rider_grip.cpp:93` —
`if (s.attached && s.load_lp > gp.capacity) s.attached = false;` — and **nothing in the kernel ever
sets it back**: the single re-attach in the whole program is `app::player_mount_request`
(`app/main.cpp:8422`, the R / mount seam). MEASURED by grep. So on any event where he was already
thrown off the bars, **W is meant to do nothing at any value of this dial**, and that is correct
(R4a §7.4) — `!hands_on` keeps its unconditional zero.
**How hard must the hit be before that happens? The number, DERIVED — and it CORRECTS the red-team's
premise: the latch is a HIGH bar, so B1 is reachable on most of the population he named.** The kernel
path runs `buck_gain = 0.0` (`sim/rider_grip.h:74` struct default; grep finds no override in
`sim/sled.h`, `app/main.cpp` or `config/` — the 0.02 buck is the DRAWN chain, R4a), so the memory
divisor is identically 1.0 and the solver is `ext' = 0.0669·max(0, |g_eff| − 9.81) − 1.0·ext`,
`load = |g_eff|·ext`, one-pole filtered at `load_tau_s` 0.1 s against `capacity = 70.0`
(`rider_grip.h:160, :164, :244, :249`, all MEASURED).
* **sustained:** equilibrium `ext = 0.0669(g − 9.81)` ⇒ `g² − 9.81g − 1046.3 > 0` ⇒
  **|g_eff| > 37.6 m/s² = 3.83 g, held for seconds** (the return spring's time constant is 1.0 s).
* **impulsive:** `Δext ≈ 0.0669(G − 9.81)·T` ⇒ at **G = 100 m/s² (10 g) he needs ≈ 0.12 s** of it
  (≈ 0.17 s once the 0.1 s low-pass is charged); at **G = 200 m/s², ≈ 0.03 s** (≈ 0.05 s filtered).
**Reading.** A ski clipping a bank at 12–16 m/s does not hold 3.8 g for a second, so **on most bank
strikes `hands_on` is still TRUE and B1 delivers.** A genuinely violent strike still unseats him, and
there B1 is silent by design. *Killing mutation:* `pose_hz` 1.0 — halve it and the sustained threshold
falls to **≈ 28 m/s²**; arm the kernel buck at 0.02 and it falls to **≈ 32 m/s²**; and every number
here dies if `unseat_gain` moves off 0.0669.
**⚠ AND ON HIS TAPES IT IS NOT EVEN ARMED.** `test/harness/sled_tape.h:415` forces
`t.params.grip.unseat_gain = 0.0` on every tape-absent preset, so across tapes 86–91 the rider
**cannot** come off — X1 §6.1's own killing mutation (i), and the reason every pump and righting
count in this audit is an upper bound. **No tape can tell us how often he is handless.**
**What it costs the rung, folded below:** the drive checklist opens with a **low-violence tip-over,
hands on** (§4.5 run 3(a)) *before* any bank strike, and carries the line *"if the rider is off the
bars, W is meant to do nothing — check the rider before you judge the dial."*

**Identity.** `0.0` ⇒ `0.0 * thr_in` ⇒ the same zero by the same arithmetic, every tape, every golden.
**Invariant (two-sided).** (a) at 0.0 the v17 corpus replays **byte-identical**; (b) at any non-zero
value `sled_onside_recovery_is_momentum_not_magnetism` (`test/unit/test_sled.cpp:2649`) **must still
pass** — a machine that drives itself upright on track thrust is the cheat the kernel forbids by name.
**Killing mutation.** 0.0 ⇒ the branch is arithmetically dead and the rung is cosmetic. **1.0 and
watch `never_recovered`: if it falls, the throttle is righting her and this has silently become the
rung R11 just ruled out.**
**Honest limit.** A spinning track on a machine lying on her side applies thrust at `tan_mount`,
`cg_height_m` off the CG, so it **will** produce a roll moment. That is physics, not a cheat — and it
is exactly why the value must be small and must be his.
**★ FOLDED P0-2 — B1 AND RUNG 7 ARE COUPLED, AND B1 ALONE CAN BUY THE EXACT CHEAT THE WALL LEG
FORBIDS.** The kernel says it at the ceiling, in its own words (`sim/sled.cpp:1212-1218`, read this
pass): *"`shear`'s cohesion term (area\*c_eff) and `roost_thrust` are both load-INDEPENDENT, so
without this a track carrying 55 N still pulls at `max_thrust_n`."* That ceiling is
`if (p.traction_mu > 0.0)` at `:1219`, and **`traction_mu = 0.0` ships** (`sim/sled.h:1006`,
MEASURED). **A machine on her side IS the 55 N case by construction** — the track is barely loaded.
So B1 at a non-trivial frac applies a near-`max_thrust_n` force at `tan_mount`, `cg_height_m` off the
CG, at the moment of least resistance: the largest roll moment this kernel can produce. **Three
consequences, all folded:**
1. **Rung 7 is PROMOTED onto B1's night as `B0`, B1's ceiling** — not a passenger on rung 6's drive.
   It is one more `app/main.cpp` env line (`SEADS_SLED_TRACTION_MU`); the mechanism at `:1211-1222`
   is already written, gated and shipped at 0.0. **A value chosen for B1 against the uncapped kernel
   is a value chosen against a machine that stops existing the day rung 7 lands.**
2. **`sled_onside_recovery_is_momentum_not_magnetism` (`test/unit/test_sled.cpp:2649`) runs at the
   driven value BEFORE the drive, as a gate.** This document already calls it THE WALL; a wall you
   walk into afterwards is a report, not a wall.
3. **The first candidate is capped at ≤ 0.15, not 0.35**, until that leg has been read at the value.

---

### RUNG 2 — THE ROCK HAS TO REACH HER · `right_stand_shift_frac` 1.0
**His words.** *"self righting with a press and I want to be able to self right by rocking bodyweight
back an fourth while pressing stand on and off, gain pendulum momentum (not automatic re righting)."*
**Touch surface.** `app/main.cpp` — **one env-override line and nothing else.** The field exists
(`sim/sled.h`, `right_stand_shift_frac = 1.0`), the loader requires it
(`config/load_scenario.cpp:532-533`), the TOML carries it (`config/scenario.toml:2115`), and the tape
pins it (`test/harness/sled_tape.h:87`). **Zero kernel lines. Zero loader lines. Zero test-harness lines.**
**What is already built, and he should be told before he drives it.** `sim/sled.cpp:1679-1798` **is**
the mechanic he described:
* **the pusher tires** — `right_charge` is a contracting ODE, drain 0.6 s / refill 0.7 s
  (`sim/sled.cpp:1712-1717`), so *"an infinite press injects FINITE energy and then nothing"*;
* **the push is phase-locked** — `w_pump = 0.5(1 + tanh(ω_z·dir/0.35))` (`:1774-1775`), textbook
  parametric pumping, ~0 against the swing;
* **the cadence is already gated** — `test/unit/test_sled_selfright.cpp:240 "a committed press beats
  mashing"`.
It is **armed in the game he drives**: `right_assist_nm = 2400.0` in `config/scenario.toml:2085` and in
**the header of every one of tapes 86–91** (MEASURED, X1 §6).
**So why does it not read that way to his hand — and this is the finding.** `sim/sled.cpp:401-406`,
verified this pass:
```
lat_target_sr = clamp(target_lat_m + right_shift_cmd*right_stand_shift_frac*lean_lat_stand_m,
                      -lean_lat_stand_m, +lean_lat_stand_m)
```
At the top of a fresh press the brace term alone is `1.0 × 1.0 × 0.35 = ±0.35 m` — **the entire reach
box.** So during the press his rock is either **discarded** (same side, already clamped) or
**subtractive** (opposite side, fighting the shove). DERIVED, exact algebra. **The two channels he
named are anti-phased by a clamp: the kernel takes his body away at the instant he shoves and gives it
back only in the half-cycle where he is NOT pushing.** A pendulum is pumped by moving the mass *in
phase*. That is precisely the difference between *"press a key and it comes up"* and *"rock it up."*
**His rock has a real lever and it is not small.** `sim/sled.cpp:1486` `ptc = pt - cg_off` — the hull
contact points move with his lean — and `rider_frac = 87.5/331 = 0.2644`, so a full ±0.35 m rock moves
the arm **±0.0925 m** ⇒ **±300 N·m** against gravity's 933.1 N·m tipping torque. DERIVED, exact.
**32 % of the gravity torque, if he is allowed to apply it while pushing.**
**Identity.** `1.0` = the shipped value. **No line of kernel changes, so the identity proof is that
nothing was written.**
**Suggested A/B** `0.5`: the brace keeps half the box (his *"STANDING AUTOMATICALLY HELPS PUSH YOU
OVER RIGHTED"* ruling is honoured at half strength) and his rock gets **±0.175 m = ±0.0462 m of hull
arm = ±150 N·m**, a sixth of gravity, at a phase **he** picks. DERIVED; the value is his drive.
**Invariant (three-sided).** (a) at 1.0 all 11 `test_sled_selfright.cpp` TEST_CASEs byte-identical;
(b) at 0.5 *"a committed press beats mashing"* (`:240`) **must still pass**; (c) **the new leg this
rung is for** — a well-timed rock must right her in fewer seconds than a mistimed one from the same
start. **No such leg exists today. It is owed by this rung and it is the leg that decides it.**
**Killing mutation.** `= 1.0` ⇒ bit-identical, rung dead. `= 0.0` ⇒ the automatic brace is gone
entirely, which **violates his own 2026-08-26 ruling** — so 0.0 is a mutation that must FAIL a leg,
**not a candidate value.**
**⚠ THE TREE PREDICTED THIS COMPLAINT, IN WRITING.** `test/unit/test_sled_selfright.cpp:236-239`,
verbatim: *"At FULL lean mashing also succeeds now: the automatic stand-shift plus his own lean is
enough authority that cadence stops mattering. That is a real property of the shipped dials, recorded
rather than hidden, and it is **the thing to watch if the mechanic ever starts to feel free.**"*
**Honest limit / the attached half.** Three other things gate this block and none of them is this dial:
the assist is armed on **2.62 %** of ticks while he stands on **25.6 %** (MEASURED, 86 578 v17 ticks)
because `right_assist_max_ms` is 1.3889 m/s; the pump gate is open **13.4 %** of past-90 time
(MEASURED, X1 §6.1); and *"tipping over"* is measured against the **planet**, not the hill
(`sim/sled.cpp:1681` `acos(up_body.y)`), so a rock on a side-hill is graded against the wrong angle.
**★ FOLDED P0-4 — THE RUNG'S DIAL IS NOT THE MEASURED BLOCKER, AND THE CLAMP IS INERT ON MOST OF
THE TIME SHE IS OVER.** `sim/sled.cpp:1795` and `:1799` set `right_shift_cmd = 0.0` in **both**
else-branches — whenever `p_eff == 0`, i.e. whenever `right_assist_armed` is false — and
`right_assist_armed` is the hysteretic gate at `:1694-1699` on
`right_assist_max_ms = 1.3888888888888888` (`config/scenario.toml:2088`; re-arm at 0.8× = 1.111 m/s,
`sim/sled.h:230`). X1 §6.1 MEASURED the gate open **20.3 % (v17) / 35.6 % (corpus)** of past-90 time
and `armed ∧ stand` at **12.5 % / 13.4 %**.
**Therefore on ~80–87 % of the time she is over, `right_shift_cmd` is exactly 0, the brace term in
`sim/sled.cpp:402-405` is identically zero, and the clamp is `clamp(target_lat_m, ±0.35)` — his own
reach, uneclipsed.** His rock is already fully live there, and it demonstrably does not right her. So
the N2 diagnosis explains **at most 13.4 %** of the problem — and §1 line 7 of this document already
said the blocker is the speed gate while the rung picked the other dial. **That contradiction is
resolved here, in B2's favour, but not with B2's dial alone.**
**THE FOLD — B2 becomes TWO env lines driven IN ORDER, which is what keeps the attribution:**
* **B2a — THE PRECONDITION.** `SEADS_SLED_RIGHT_MAXSPD`, a new env override on
  `comfort.right_assist_max_ms` — a `SledComfort` field (`sim/sled.h:229`), already loader-required
  (`config/load_scenario.cpp:510-511`), already rostered (`test/harness/sled_tape.h:81`): **the same
  zero-kernel, one-line shape as B2 itself.** Driven ALONE, at the shipped
  `right_stand_shift_frac = 1.0`. It opens the gate the pump already lives behind and changes nothing
  else. Identity = the TOML's `1.3888888888888888`.
* **B2b — THE RUNG.** `SEADS_SLED_STAND_SHIFT=0.5` on top, once he has felt B2a.
**And one sentence at the top of the B2 checklist either way:** *"this mechanic only exists once she
has nearly stopped sliding — let her come to rest, then press."* Without it the most likely drive
outcome is "didn't notice", for a reason this audit had already measured and not acted on.
**⚠ B2a IS A FEEL CHANGE TO A SHIPPED NUMBER, NOT AN IDENTITY.** Widening the gate makes the assist
available while she is still sliding — a different machine from the one he has been driving. It is a
**drive-only env override with a ruling owed** (§5, **B2-GATE**), never a landing.
**If 0.5 does not make the timing matter once B2a has opened the gate, the mechanic is not the
clamp — and that is a real, reportable outcome, not a tuning failure.**

---

### RUNG 3 — THE THROTTLE HAS TO SWING THE TAIL · `track_lat_slip_shed` 0.0
**His words.** *"I can only really turn sharply if I alternate gas/brake, but throttle should also be
able to swing my tail around on account of the roost, esp with weight shifting of the sudburian."*
**Touch surface.** `sim/sled.cpp` (a 6-line hoist + a 3-line branch), `sim/sled.h` (one field),
`app/main.cpp` (one env line), `test/harness/sled_tape.h` (one roster entry). **No `config/`.**
**Mechanism — four parts, and all four point the same way.**
* **(a) The track's sideways μ is a constant.** `sim/sled.cpp:1056`: `mu_l = is_track ? track_lat_mu
  : d.mu_lat`, `track_lat_mu = 0.70` — **the same 0.70 whether the track is locked, rolling, or
  spinning at 46 m/s.** There is no friction circle in this kernel; `grep` finds only the per-surface
  `mu_brake` budget (`:1232-1246`), which caps longitudinal braking and never charges lateral.
  DERIVED, structural, re-verified this pass. **In the budget is infinite in one axis. That is the
  whole answer to his ask.**
* **(b) So throttle can only UNLOAD the rear, and that is second-order** — and it points the wrong
  way, because thrust at `tan_mount` *loads* the track.
* **(c) The weight-shift half already works and is pointed backwards for this ask.** Leaning aft moves
  `cg_off.z` and **loads** the track (`sim/sled.cpp:523-527`) — at a constant μ that means **throwing
  your weight back makes the tail stick harder.** The exact inverse of a real sled.
* **(d) The roost he names is already computed and never turns the machine.**
  `roost_thrust = roost_gain·flux·rho_eff·area·v_rel²` (`:1182-1184`) applied as **pure forward thrust
  at `tan_mount`, on the centreline — yaw moment exactly zero.** DERIVED. **He is right that the roost
  is the physical carrier, and right that it does nothing for him today.**
**THE FORM.** The missing friction ellipse, **track only**, keyed on the track's own longitudinal slip,
scaled by his lean:
```
mu_l = is_track ? track_lat_mu : d.mu_lat;
if (is_track && track_lat_slip_shed > 0)
    mu_l *= max(0.0, 1.0 - track_lat_slip_shed * |trk_slip| * (1.0 + align_m));
```
`align_m` is the weight-shift half **for free**: already computed, already clamped ≥ 0
(`sim/sled.cpp:583-585`, the house rule that a wrong-way lean is never a penalty), already requiring a
real yaw rate. **Lean into it + throttle = the tail comes around; wrong-way lean = exactly the shipped
number.** That is *"esp with weight shifting of the sudburian"* in one existing variable.
**Identity.** `0.0` ⇒ the branch never runs ⇒ `mu_l` is the shipped expression, byte for byte.
**★ FOLDED P1-3 — THE SUGGESTED A/B WAS CALIBRATED AGAINST A SLIP DISTRIBUTION THIS AUDIT'S OWN
DATA REFUTES.** The struck line read: *"at the 0.10–0.30 slips of ordinary driving a 5–15 % shed,
invisible."* But X1 §5.1/§5.2 MEASURED WOT duty at **52 % (v17) / 58 % (corpus) / 72 % (tape 91)**,
and at WOT `v_track = throttle·46 = 46` against a trail speed of **11.9–16.3 m/s**, so
`trk_slip = (46 − v_fwd)/46 ≈ 0.65–0.74` (DERIVED from `sim/sled.cpp:1140-1144`). At `0.5` that is a
**33–37 % shed running straight**, and leaned into a developed yaw (`align_m → 1`) a **65–74 % shed:
`mu_l` 0.70 → 0.18.** Not invisible — it is most of the track's lateral grip, in the regime he rides
most, on a dial sold as a cornering aid. *Killing mutation:* the WOT duty itself — if 52–72 % is an
artefact of the ≥ 0.95 threshold, the whole re-derivation moves.
**THE LADDER, replacing the single 0.5:** **`0.15` first** (WOT-straight shed 10–11 %, leaned
20–22 %), then **`0.3`**, and **`0.5` only if 0.3 is inaudible**. DERIVED; the value is his drive —
**0.5 is now the top of the ladder, not its first step.**
**★ FOLDED P1-5 — THE FORM SHEDS IN A STRAIGHT LINE, WHICH IS NOT WHAT THE RUNG PROMISES.** With
`align_m = 0` the factor is `1 − shed·|trk_slip|`, **not 1**: a straight WOT run loses lateral μ with
no lean at all. Two consequences. **(i)** The owed leg `sled_tail_shed_is_inert_in_a_straight_line`
as first written is **guaranteed red** at any non-zero dial on any fixture whose slip angle is not
identically zero; it must be written on a hermetic flat fixture with zero commanded steer and zero
lean, where `slip_ang == 0` makes the lateral force zero *whatever* `mu_l` is — and it must say so in
its own comment. **(ii)** **The fallback form, if run 4 says the bank strikes got worse, is to make
the shed purely lean-gated** — `(1.0 + align_m)` → `align_m` — which is bit-identical to today
whenever he is not leaning into a real yaw rate, and is closer to his literal *"esp with weight
shifting of the sudburian."* **That fallback is one term of form, not a second dial.**
**Invariant (four-sided).** (a) **the hoist proof** — at 0.0 every golden and the whole tape corpus
byte-identical, which is the only thing that shows moving the six slip lines changed nothing;
(b) straight running (no steer, no lean) bit-identical at any value; (c)
`sled_wrong_way_lean_is_never_a_penalty` (`test/unit/test_sled.cpp:2546`) still passes — it measured
the last leak at the 4th decimal; (d) **ski patches bit-identical at any value** (owed leg).
**Killing mutation.** `= 0.0` ⇒ branch dead. **And the dangerous one:**
`sled_track_lat_mu_is_the_rostered_value` (`test/unit/test_sled.cpp:1470`) — **it must be re-read
before a line is written** to confirm it tests `p.track_lat_mu` and not the *effective* μ. If it reads
the effective value, this rung silently invalidates a pinned leg.
**⚠ HONEST LIMIT, AND IT IS LARGE.** Shedding the track's lateral μ **lowers the roll threshold in a
slide**: it removes the yaw resistance that keeps a ski-on-a-bank strike from becoming a spin.
**Rung 3 and his bank-strike complaint pull in opposite directions.** This is the one rung on the
ladder that can make the roll worse. It must be driven on a night that is **not** a `class_blend_m`
night, or the attribution is lost.

---

### RUNG 4 — THE THUMB HAS TO HAVE A MIDDLE · `throttle_power_frac` 0.0
**His words.** *"throttle is all or nothing ive mitigated this by tapping and applying brake."*
**Touch surface.** `sim/sled.cpp` (one line), `sim/sled.h` (one field), `app/main.cpp` (one env line),
`test/harness/sled_tape.h` (one roster entry).
**Tape.** **MEASURED, X1 §5.1, and it is the most one-sided number in the audit:** over 206 minutes the
thumb command is held at an intermediate value for **159 ticks — 1.3 seconds — 0.011 % of drive time**
(v17: 7 ticks, 0.008 %). Duty: **zero 34 % / open band 8 % / WOT 58 %**. He taps **9.6/min** with a
median hold of **0.22–1.16 s** against a 0.40 s ramp to full — *he is releasing before the thumb has
arrived* — and alternates gas↔brake **2.8/min** corpus-wide, **11.5/min on tape 88**, with 16 of those
alternations while the bars are turned. *"I can only really turn sharply if I alternate gas/brake"*,
on the tape, in the drive that did it most. *Killing mutation:* the 66 ms rate window (a 1-tick window
is pure frame aliasing, measured at exactly 0.500).
**Mechanism — three stacked, in order of size. DERIVED, exact.**
1. **The drawbar cap flattens the top, and `throttle` appears nowhere in it.** `sim/sled.cpp:1207-1211`:
   `cap = min(max_thrust_n, engine_power_w·breathing/max(|v_fwd|,2))`. Below **33.6 m/s** the power
   term never binds, so **the cap is the constant 2 272 N at every speed he rides.** Fully-developed
   shear: Bush 1 569 N (under), **TrailMain 5 280 N (2.3× over), Road 12 330 N (5.4× over)** — so on
   39 % of tape 91 the commanded thrust is **exactly 2 272.0 N from ~3 % of slip to full thumb.**
2. **Janosi saturates in a tenth of a slip.** Exponent `20.727·|slip|`: 87 % of full shear at
   `|slip| = 0.10`, 99.8 % at 0.30. `shear_K_m` is a **LITERATURE** modulus (0.01–0.05 m for snow/sand)
   and **is not the dial** — moving it is tilting the hardware.
3. **The thumb's lower half is a brake.** `drive_t = clamp((throttle−0.02)/0.08)` — the entire clutch
   engagement happens between 0.02 and 0.10, which the app's 2.5/s ramp crosses in **0.032 s** — and
   above that `v_track = throttle × 46`, so `slip` is **negative (reverse shear, up to 0.70 g of track
   brake) for every thumb below `v_fwd/46`.` At 10 m/s: **22 % of travel is a brake, 8 % is the live
   band, 70 % is flat.** At 25 m/s the neutral point is thumb 0.543; at 40 m/s, 0.870.
   **The thumb is a three-position switch and the sliver slides up the travel with speed.**
**THE FORM.** `cap *= 1.0 - throttle_power_frac*(1.0 - throttle);` — scale the **engine** ceiling by
the thumb. `max_thrust_n`'s own header calls itself *"0.70 g measured **full-throttle** accel"*
(`sim/sled.h:955-960`): **scaling it by the thumb is reading the header, not inventing physics.**
**Identity.** `0.0` ⇒ factor exactly 1.0 ⇒ the shipped expression. **And at `1.0`, *steady-state*
WOT is still bit-identical** (`throttle = 1` ⇒ factor 1.0) — his top speed and a fully-arrived thumb
are untouched by construction.
**★ FOLDED P0-3 — THE STRUCK CLAIM WAS FALSE, AND IT WAS THE LYING-INSTRUMENT SHAPE.** This line
used to read *"his launch wheelie … untouched by construction."* It is not. `app/main.cpp:8290-8292`
(verified this pass: `sled_thumb = std::min(1.0, sled_thumb + 2.5 * frame_dt)`) means **the thumb
ramps through the entire range on every launch — 0.40 s from rest — and the wheelie is the thrust
couple at `tan_mount`, largest exactly inside that 0.40 s.** Worse: X1 §5.2 MEASURED his median
*key-held* press at **0.22–0.35 s on tapes 88 and 89**, so those presses never reach WOT at all and
on the two drives that best match his complaint this dial weakens **100 %** of his throttle work —
while tapping **is his stated mitigation** (*"ive mitigated this by tapping and applying brake"*).
**The true statement: every transient is scaled; only a held, arrived thumb is bit-identical.** The
drive must therefore ask *"does she still lift the skis off the line?"*
**★ FOLDED P1-6 — THE FORM IS OUTPUT-SIDE ONLY AND CANNOT GIVE THE THUMB A MIDDLE BY ITSELF.**
(Reconstructed: this item reached the lane as a verdict-table label with no body — see §7.) The dead
lower half of the thumb is an **input-side** fact: `v_track = throttle × 46` (`sim/sled.cpp:1140`)
makes every thumb below `v_fwd/46` a reverse-shear track **brake**, and scaling the drawbar **cap**
does not move that neutral point by one percent of travel. `throttle_power_frac` alone flattens the
top without opening the bottom.
**★ FOLDED P1-7 — THE CAP IS SYMMETRIC, SO SCALING IT ALSO CUTS THE TRACK'S BRAKE.** (Same
provenance.) `T = std::clamp(T, -cap, cap)` (`sim/sled.cpp:1211`) bounds **both** signs, so at a low
thumb the scaled cap clamps the negative (engine-brake / reverse-shear) thrust too — the other half
of the gas↔brake alternation he uses to turn. The "honest non-identity" below named the coast band
and understated this.
**VERDICT AFTER THE FOLD: rung 4 is not one dial.** It is an **N4-PAIR** ruling (input curve +
output ceiling), and it is **off the buildable list until he rules it** — §5.
**Invariant (three-sided).** (a) 0.0 byte-identical; (b) **WOT byte-identical at every dial value** —
new leg `sled_throttle_power_frac_is_inert_at_WOT`, owed, does not exist; (c)
`sled_thrust_is_capped_by_snow_not_engine` (`test/unit/test_sled.cpp:891`) must still pass — this
scales an **engine** ceiling and must not become the snow's job, which is rung 7's.
**Killing mutation.** 0.0 ⇒ dead. And the one that exposes a mistake: drive at 1.0 and measure
time-to-40 m/s at **full** thumb — if it moves at all, the dial is leaking into WOT and the form is wrong.
**Honest non-identity, named now rather than discovered later.** At `frac > 0` and `throttle = 0` the
cap becomes 0, removing a small residual coast term in the **3.0–3.5 m/s clutch band**. If that is
felt, the fallback is an asymmetric form (`cap_drive` scaled, `cap_brake` untouched) — **two numbers,
so it is a different rung.**
**Why it is rung 4 and not rung 3.** Nothing blocks it: no ruling, no precondition, same night's
plumbing shape. It is **out of the first build only because it and rung 3 both change what a held
throttle feels like**, and driven together their attributions cannot be separated. **If he would
rather have the thumb than the tail, swap 3 and 4 — the build packet is the same shape.**

---

### RUNG 5 — THE BODY IN THE AIR · `k_air_shift` 0.0
**His words.** *"Need to make it more dynamic with bodyweight (also for jump weight shift influence
in air)."*
**Touch surface.** `sim/` (already written and gated), `app/main.cpp` (one env line). **The field is
already in `SLEDTAPE_PARAMS_D`** — no roster touch.
**Standing.** His **build** ruling was taken 2026-08-27 (*"I think we should build it. Because then it
has the foundation it needs."*). It is written, commented, gated and shipping at 0.0
(`sim/sled.cpp:2096-2098`), pinned by three legs (`test_sled.cpp:1505 / :1538 / :1596`).
**What it buys, DERIVED exactly** (I = 158.7 / 186.9 / 49.7 kg·m², `sim/sled.h:777`): a full lateral
sweep (±0.35 m) at `k = 1.0` buys **18.4° of roll** and 4.9° of yaw; a full fore→aft throw buys
**5.5° of pitch**. It is a **momentum exchange, not a torque** — applied to the rate, never through
`torque_body` — and it **telescopes**: a throw buys an **attitude, never a rate**.
**Identity.** `0.0` ⇒ branch dead. **`k = 1.0` is both the honest value and the ceiling**: `k` is the
fraction of exact conservation, and `k > 1` manufactures angular momentum from nothing. Fence it in
the range check, do not leave it to a typo.
**⚠ WHY THIS IS NOT IN THE FIRST BUILD.** **Throwing his weight AFT pitches the nose DOWN.** DERIVED,
exact, from this kernel's own sign convention (`test/unit/test_sled.cpp:4341-4346`: *"+omega.x is
nose-UP"*). That is correct physics and it is **the opposite of what a rider expects.** The lever that
actually brings the nose up in the air is the **throttle**, and this kernel already has that term,
built and also shipped at zero: `k_gyro_react` (`sim/sled.cpp:2068-2071`), pinned by
`sled_gyro_reaction_pitches_the_nose_UP_on_a_throttle_blip`. **He asked for one dial; the truth is
two, and offering `k_air_shift` alone and letting him discover the nose drop is the lying-instrument
shape.** That pairing is a ruling, not a build. → **RULING OWED (N5-PAIR).**
**Invariant.** (a) 0.0 byte-identical; (b) `sled_air_shift_is_inert_while_any_patch_touches` still
passes — no ground-contact steering; (c) **a throw-and-return must net exactly zero attitude change**
(out 18.4°, back −18.4°); a residue means the every-substep carry has broken and the term is a free
impulse.
**Killing mutation.** Flip the sign of `dl` — `test_sled.cpp:1538` *should* go red. **If it passes,
that leg is sign-blind and this rung ships on a test that cannot see the defect class it exists for.**
**Trap, already handled, worth not re-breaking.** `app/main.cpp:8398-8410` zeroes `sled.ws_exch_l` on an
autoright; the field is **not** in the tape pin roster, so replay rebuilds it as zero. **Turning this
dial on makes that line load-bearing for the first time.**

---

### RUNG 6 — THE SEAM THAT SPINS YOU · `class_blend_m` 0.0
**His words.** *"often rolling when hitting banks … other times its just going down the road and a ski
hitting the bank on one side."*
**Touch surface.** `config/world.toml:645`. **Live-tunable. There is nothing to build.**
**Tape, re-measured by X1 and LARGER than v1 claimed.** Attributed at the **takeoff** — the only moment
upstream of the event, present in 92 % of tumble events — **TrailMain 11.98 tumble events/min against
Bush 0.50 and Road 0.42 (24×)**, on 237 events and 19.79 minutes. And **147 of those 237 launches
(62 %) had the two skis reading DIFFERENT SURFACE CLASSES within 0.5 s of takeoff**, 187 (79 %) within
1.0 s, **and not one of the 237 ever launched without one** — against bush 18 % / 31 % / 13 %-never, at
a *lower* trail speed (11.9 vs 16.3 m/s), so speed is not doing it. **237 of 323 attributable tumble
events (73 %) launched from a surface that is 9.6 % of the drive time.** MEASURED.
**And his one-sidedness holds.** Of the 198 corpus crossings with bank contact in the pre-window,
**190 (96 %) are ONE-SIDED** — same ski high, or same ski in the deeper snow, on ≥ 90 % of contact
ticks. **His *"a ski hitting the bank on one side"* is literally the shape the tape shows.** MEASURED.
**Mechanism.** `world/snowpack.cpp:700-746` ramps `surf_mix` only `if (class_blend_m > 0.0)`; at the
shipped 0.0, `surf_mix` is identically zero and `blend_dials` is **never called**. One ski crossing the
edge steps `rho_eff` 0 → 260 under that ski alone, in one tick — a pure yaw+roll couple. The tree's own
probe: **blend 0.0 → yaw −482.7°, ROLLED; blend 1.0 → yaw −11.6°, no rollover.**
**⚠ WHY IT IS RUNG 6 AND NOT RUNG 1.** Not merit — three fences:
1. **`LANES.toml:939` reserved it for him and gave it to the lane that owns `world/snowpack.*`**
   (road-repair). This lane hands it over; it does not run it. **R2 is still open.**
2. **v17 carries 0.45 min of trail.** Last night barely tests it. Any A/B must be driven on
   **TrailMain, deliberately** — and five of the six v17 tapes have **no plowed corridor at all**, so
   the v17 pre-onset bank rate (7.7 % against a 12.7 % base) is at or below chance.
3. **Blast radius: two consumers.** `surf_mix` is read by `sim/walker.cpp:98-107`, so this dial
   **changes the man on foot at every corridor edge** — live beside *"feet in asphalt Hwy 144 W"*.
**⚠ AND THE THING NO TAPE CAN EVER SETTLE.** The corridor edge and the bank face **are the same place**:
`bank_profile` is evaluated on `e = dist − half_w` and `classify()` flips at that same edge
(P(edge|contact) 42 %, P(contact|edge) 59 %). **Only the probe separates them** — `class_blend_m` moves
the class step and leaves the bank untouched. That is what makes this an A/B and not an argument.
**Invariant (two-sided).** TrailMain's and Road's **tumble-event** rate per minute must fall AND Bush's
must not fall by more than the trail's. If both fall together it is a stability change, not a boundary
fix. **Read at the tumble-event level and attributed at the takeoff** — X1 changed both of those, and
the v1 drive checklist's per-crossing reading is now wrong.

---

### RUNG 7 — A TRACK CANNOT PUSH HARDER THAN THE SNOW IT STANDS ON · `traction_mu` 0.0
**His words, quoted into `sim/sled.h` at the dial, 2026-08-24.** *"at high speeds, getting pulled in and
flipping out like 15x is not desirable so yes take care of that"* — and in the same ruling the lift
**stays**: *"it is important for traversing atop the snow"*, *"at slower speeds I think it is good too
because [I'd] like to jump the snowbanks."*
**Touch surface.** `sim/` (already written, `sim/sled.cpp:1211-1222`), `app/main.cpp` (one env line).
Already in `SLEDTAPE_PARAMS_D` — no roster touch.
**Standing.** **His ruling was TAKEN, the mechanism was BUILT, the sweep is on the books, and it ships
at 0.0.** `docs/snowform_measurements.md` §M8.1: at **μ ≥ 0.6** the Bush runaway does not develop
(`t(|roll|>20°)` 6.22 → 1.33 s; `v_end` 24.8 m/s *rising* → 0.1 m/s), and at **μ ≥ 3.0** his own
traverse is **byte-identical to baseline** (14.1 / 14.5 / 15.3 m/s). **A value exists that pays his
ruling and costs his traverse nothing.**
**Tape.** WOT **52 %** of last night, **72 % on tape 91** — he rides with the thumb pinned, which is
exactly the regime this ceiling governs. At the runaway attractor the running surfaces hold **62 N of
3 246.0 N — 1.91 % of the machine's weight** — while the drivetrain is still pegged.
**Identity.** 0.0 ⇒ the branch is dead, bit-identical.
**Killing mutation / honest limit.** MEASURED **inert on Road by construction** (`rho_eff = 0`) — **it
does not fix the bank-strike superspin.** Anyone selling it as that is selling the wrong thing.
**Why rung 7 and not higher.** He did not name it this morning, and X1 cut the rollover headline it was
ranked against. **It is still the cheapest paid ask in the audit.**
**★ FOLDED P0-2 — IT NO LONGER RIDES RUNG 6'S DRIVE. IT IS **B0**, THE FIRST THING ARMED ON B1'S
NIGHT.** A spinning track on a machine lying on her side is the load-INDEPENDENT 55 N case this
ceiling was written for, word for word (`sim/sled.cpp:1212-1218`), so choosing rung 1's value against
the uncapped kernel would be choosing it against a machine that stops existing the day this lands.
See §4 and §4.5 run 2.

---

### RUNG 8 — THE LEAN HAS TO BUY THE ARC · `plane_lat_lean_gain` 0.0
**His words.** *"Need to make it more dynamic with bodyweight"* (2026-09-18); felt item 5 *"rider
weight needs authority, not thrown into a spin"*; §0 *"allow the balance of body mechanism to ENHANCE
ability, i.e. tighten a turn."*
**Touch surface.** `sim/` + `app/` (env). Already in `SLEDTAPE_PARAMS_D`.
**Tape.** He holds **|lean| > 0.98 for 14.8–43.6 %** of every v17 drive (26.5 % pooled, **44 % on tape
91**) and the lean axis holds a *partial* value for up to **36.1 s** against the steer axis's all-corpus
maximum of **0.500 s**. **He is asking the reward channel for everything it has, continuously.**
**Mechanism.** The shipped lean reward is the wrong one: `lean_bite_gain 0.18` multiplies bite on
**every patch including the track** — MEASURED self-defeating for an arc (0.18 → 0.45 took the lean-in
carve from **30 m to 295–1037 m of radius**, *"because the track out-gains the skis"*). The strongest
reward was regressed on purpose and carried as an open debt (`sim/sled.h:1154-1157`).
**⚠ It sits in SERIES with the path it calls wrong.** `align_m` is computed **only inside**
`if (lean_bite_gain > 0.0)` — at gain 0 the multiplier is exactly 1.0 for any value of this dial.
**Honest limit.** Inert where `plane_lat_gain` is inert — `rho_eff = 0` on **Road and LakeIce** — so it
**cannot** answer *"part of what is so fun on the road is driving by lean"* and must not be sold as if
it does.
**Why last.** **Rung 3 now occupies the "his lean does something" slot for the night**, and rung 3
routes through the same `align_m`. Driving both at once is two hands on one variable. This rung is the
next one *after* rung 3's drive says whether `align_m` is the right carrier.

---

## 3. DROPPED FROM THE TOP EIGHT — named, with the reason, so nobody rediscovers them

| v1 rung | what happened |
|---|---|
| **v1 Rung 4 — `side_right_vmin_ms`** | **OUT, twice.** (1) **R11**: C2 (`sim/sled.cpp:1545-1607`) has **no `in.stand` gate anywhere in it** — verified this pass — so lowering `vmin` makes the machine *more* automatic, against *"not automatic re righting."* (2) **X1**: the 71.2 %-never-recover it was ranked on is **12 %** at the tumble-event level. Its `wo` weight is also strongest on a machine that has **stopped moving** — the population furthest from *"by chance."* |
| **v1 Rung 3 — `autoright_recentre`** | **Off the ladder, not refuted.** The defect is real and MEASURED (six rightings, `steer_actual` 1.0000 in **four of six**, throttle command 1.00 in **six of six**). But **he did not name R once this morning**, X1 shows R ended only **5 of 16** v17 tumble events, and **leg X3 is still unrun** — if the tape shows a *held* A/D key, the rung is decoration. **Ride it beside the first build as a free `app/` line only after X3 runs.** ⚠ And note the new interaction: **with rung 1 armed, throttle 1.00 after an R press is HIS command, not a stale-zero artefact** — so rung 1 changes what that metric means. |
| **v1 Rung 6 — `assist_tell_band_nm`** | **Off the ladder.** It needs a new physics-inert `sim::SledState` field *and* a ruling that such a field is acceptable for a readout, and **he named nothing about seeing the assist this morning.** The finding stands (a non-zero body-z torque on **79.8–93.7 %** of ticks, p95 **215–765 N·m** = 82 % of the roll budget, reaching **nothing** he can see) and it keeps its correction: the tell must be gated on the **spring** component alone or it is a roll-RATE meter that burns brightest when he flicks her. |
| **v1 Rung 7 — `right_tilt_surf`** | **Folded into rung 2 as its named second half**, not a separate rung. `sim/sled.cpp:1681` `tilt = acos(up_body.y)` is reason (iv) why his rock is graded against the wrong angle on a slope — same mechanism, same block, same drive. It still **moves toward the law** (it is the only proposal that *removes* kernel-commanded rider-mass movement) and it still needs an R7-class word. |
| **v1 Rung 8 — `pitch_arrest_nms`** | **Demoted below the ladder by his own words.** *"deliberate jumps for performing a flip off of a banks for fun"* and *"a backflip … is not a roll over"* is him **defending the flip**. A new kernel torque that arrests nose-up rotation is the one rung his morning sentences point *away* from. **R7 is owed harder, not less.** The bimodality finding survives untouched (tape 86 catwalk p95 **66.2°** vs tape 87 **3.8°** on the same commanded pose, four minutes apart). |
| **v1 Rung 2 — `plane_lat_lean_gain`** | Kept, at **rung 8**, behind rung 3 — same reason: both route through `align_m`. |
| **v1 Rung 5 — `traction_mu`** | Kept, at **rung 7**. |
| **v1 Rung 1 — `class_blend_m`** | Kept, at **rung 6** — **not a demotion on merit** (X1 made its case 24× larger) but on **ownership**: it is road-repair's A/B by the `LANES.toml:939` stick ruling, R2 is open, and it moves the walker. |

---

## 4. THE FIRST BUILD — B0, B1, B2a, B2b, B3 (rungs 1, 2, 3 + the ceiling and the gate)

**The three loudest sentences he wrote, and the smallest set that answers all three — PLUS THE
CEILING AND THE GATE THE RED-TEAM PROVED ARE PRECONDITIONS, NOT PASSENGERS.** One lane, one night,
**five env vars**, **zero `config/` edits**, **zero loader edits**, **zero `render/` edits**.

| | rung | dial | identity | **kill / arm env var** | files touched |
|---|---|---|---|---|---|
| **B0** | **the contact ceiling (P0-2)** — rung 7, promoted: it is B1's precondition | `params.traction_mu` | 0.0 | `SEADS_SLED_TRACTION_MU` | `app/main.cpp` **only** (mechanism already shipped at `sim/sled.cpp:1211-1222`) |
| **B1** | throttle ramps while rolled | `comfort.rolled_throttle_frac` | 0.0 | `SEADS_SLED_ROLLED_THROTTLE` | `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`, `test/harness/sled_tape.h` |
| **B2a** | **the gate (P0-4)** — the measured blocker, driven before the rung | `comfort.right_assist_max_ms` | 1.3888888888888888 | `SEADS_SLED_RIGHT_MAXSPD` | `app/main.cpp` **only** |
| **B2b** | pendulum righting | `comfort.right_stand_shift_frac` | 1.0 | `SEADS_SLED_STAND_SHIFT` | `app/main.cpp` **only** |
| **B3** | tail swing with lean | `params.track_lat_slip_shed` | 0.0 | `SEADS_SLED_TAILSHED` | `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`, `test/harness/sled_tape.h` |

**Why env vars and not TOML, stated so the choice can be overruled.** `[sled_comfort]` is the only
TOML→`SledParams` bridge (`app/main.cpp:2477` `sled_params.comfort = scen.sled_comfort;` — the single
bridge, re-verified by grep this pass), and a `require(...)`'d TOML key is **strict**: adding one means
editing `config/scenario.toml`, `config/load_scenario.cpp` twice (require + range check) and
`test/unit/test_load_scenario.cpp`, and it makes the new dial **part of the shipped game on the first
build.** The env route is the **`SEADS_GRIP_CAPACITY` precedent** (`app/main.cpp:2488`, *"the one dial
Chad owns, reachable without a rebuild … a huge value restores stage 1 exactly, which is the kill
switch and the A/B"*): **with no env var set, the built exe is bit-identical to main for all five
dials**, and he arms them one at a time from the launch line. **TOML is the landing path, not the
drive path.** If the lane is told to land instead of drive, each dial adds its six touches then.

**⚠ THE LAW THAT MAKES THE ENV ROUTE SAFE, AND IT IS NOT OPTIONAL.** Both new fields **must** be added
to their tape roster — `rolled_throttle_frac` to `SLEDTAPE_COMFORT_D` (`test/harness/sled_tape.h:79-95`)
and `track_lat_slip_shed` to `SLEDTAPE_PARAMS_D` (`:59-74`) — **in the same commit that adds the
field.** The harness's own comment says why (`test/harness/sled_tape.h:318-338`): `load` starts from a
default-constructed `SledParams` and overwrites only what the tape names, so an unrostered dial means
**the drive and its own tape disagree about what was flown**, silently, while still reporting
"bit-exact." Both identities are the **struct default**, so every older tape replays byte-identical and
the `dial_gap` machinery reports the new names honestly.

### 4.1 B1 — `rolled_throttle_frac`, the exact edits in prose

* **`sim/sled.h`, `struct SledComfort`** — add one `double rolled_throttle_frac = 0.0;` **immediately
  below `rolled_grace_s`**, which is where the other two `rolled` dials live. Its comment carries his
  sentence verbatim and the reason the fix cannot live in `app/`: the app already ramps the thumb and
  ships it unconditionally; the kernel's `s.rolled` branch is the only thing between his thumb and the
  track, and an app-side fix would be a second, parallel throttle path — the lying-instrument shape.
* **`sim/sled.cpp:292-293`** — split the one ternary into two statements. Compute
  `thr_in = hands_on ? clamp01(in.throttle) : 0.0;` first, then
  `throttle = s.rolled ? p.comfort.rolled_throttle_frac * thr_in : thr_in;`.
  **`!hands_on` must keep forcing an unconditional zero** — a man thrown off the bars has no thumb on
  the lever (R4a §7.4) and that half of the branch is **not this dial's business.**
* **`app/main.cpp`, immediately after `:2477`** — one env override in the `SEADS_GRIP_CAPACITY` shape:
  `if (const char* e = std::getenv("SEADS_SLED_ROLLED_THROTTLE")) sled_params.comfort.rolled_throttle_frac = std::atof(e);`
  placed **before** `sim::SledState sled;` so the value the tape records is the value flown.
* **`test/harness/sled_tape.h`** — add `X(rolled_throttle_frac)` to `SLEDTAPE_COMFORT_D`, on the same
  line as `rolled_persist_s` and `rolled_grace_s`.
* **Nothing else.** No `config/`, no loader, no range check, no `render/`.

**Identity proof.** At `0.0` the kernel expression is `0.0 * thr_in` where the shipped one is a literal
`0.0` — the same IEEE zero by the same arithmetic, for every sign of `thr_in`, because
`0.0 * x == 0.0` for all finite `x` and `thr_in` is `clamp01`'d finite. **Proof obligation:** the v17
tape corpus replays byte-identical with the env var unset.
**★ FOLDED P0-5 — that obligation has no ctest home and no owner as written; the named command, the
named tapes and the hermetic surrogate that replaces it are in §4.3.**

### 4.2 B2 — `right_stand_shift_frac`, the exact edits in prose

* **`app/main.cpp`, beside B1's line** —
  `if (const char* e = std::getenv("SEADS_SLED_STAND_SHIFT")) sled_params.comfort.right_stand_shift_frac = std::atof(e);`
* **That is the whole build.** The field, the loader, the TOML key, the range check and the tape roster
  entry **all already exist**. **No kernel line changes.**
* **★ FOLDED P0-4 — and one more line beside it, which is B2a:**
  `if (const char* e = std::getenv("SEADS_SLED_RIGHT_MAXSPD")) sled_params.comfort.right_assist_max_ms = std::atof(e);`
  Same shape, same file, same zero-kernel diff. **It is driven first and alone** (§4.5 run 3a),
  because the gate it opens is the measured blocker and the clamp this rung frees is inert
  ~80–87 % of the time she is over.
* **★ FOLDED P0-2 — and B0, the ceiling, on the same three lines:**
  `if (const char* e = std::getenv("SEADS_SLED_TRACTION_MU")) sled_params.traction_mu = std::atof(e);`
  `traction_mu` is already in `SLEDTAPE_PARAMS_D`, so **no roster touch**; the branch at
  `sim/sled.cpp:1219` is already written and shipped at 0.0. **B1 is driven with B0 armed.**

**Identity proof.** With the env var unset, `sled_params.comfort` is `scen.sled_comfort` exactly as
today and no kernel line was written. **The identity proof is that the diff contains no physics.**

### 4.3 B3 — `track_lat_slip_shed`, the exact edits in prose

* **`sim/sled.h`, `struct SledParams`** — add `double track_lat_slip_shed = 0.0;` **beside
  `track_lat_mu` and `slip_ref_rad`**, with the comment carrying his sentence, the LITERATURE note
  (friction ellipse / combined slip is universal in tyre and terramechanics models and **this kernel
  has none** — `grep` finds only `mu_brake`, which caps longitudinal braking and never charges
  lateral), and **the honest limit in the header itself**: shedding track lateral μ removes the yaw
  resistance that keeps a ski-on-a-bank strike from becoming a spin.
* **`sim/sled.cpp`, THE HOIST** — the six lines that build the track's longitudinal slip
  (`engage_lo`, `engage_hi`, `clutch_blend`, `drive`, `v_cmd`/`v_back`/`v_track`, `slip`) currently
  live at **`:1120-1145`, inside `if (g.is_track)`, which opens at `:1114` — 58 lines BELOW the
  lateral bite at `:1056`.** Move them up to just above `:1056`, still inside the same per-patch scope
  (`v_fwd` is already in scope from `:1006`), wrapped in their own `if (g.is_track) { … }` that writes
  patch-scope locals (`trk_slip`, and whatever else `:1114`'s block still needs). **The block at
  `:1114` then READS those locals. It must not recompute them.** Two copies of the slip formula is how
  the CVT and the rpm readout drift apart, and `sim/sled.cpp:1338-1341` already says so in its own
  **★ FOLDED P1-1 — THE TRIPWIRE AS FIRST WRITTEN IS WRONG AND WOULD RED A CORRECT EDIT.**
  `grep -c "(v_track - v_fwd)" sim/sled.cpp` returns **2 on the shipped tree** — `:1144` (the slip)
  and `:1181` (`v_rel = std::abs(v_track - v_fwd)`, inside `roost_thrust`). MEASURED today. A lane
  obeying the old line either believes a correct hoist failed, or deletes `v_rel` and silently kills
  the roost — the very term his *"swing my tail around on account of the roost"* names. **The
  tripwire is `grep -c "v_track - v_fwd) / std::max(v_track" sim/sled.cpp` == 1**: the slip
  expression appears exactly once and `:1181` is untouched.
* **`sim/sled.cpp:1056`, THE BRANCH** — after `mu_l` is assigned and **before** the `tanh`:
  `if (g.is_track && p.track_lat_slip_shed > 0.0) mu_l *= std::max(0.0, 1.0 - p.track_lat_slip_shed * std::abs(trk_slip) * (1.0 + align_m));`
  **`align_m`, not the raw signed lean** — it is already clamped ≥ 0 (`:583-585`), which is the house
  rule that a wrong-way lean is never a penalty, and it already requires a real yaw rate
  (|ω_up| > 0.02 rad/s ramping to 1.0 at 0.07). **`g.is_track` is load-bearing:** without it this
  touches the ski plate the carve lives on, which is the discipline `plane_lat_lean_gain` uses at
  `:1102` (*"lean buys ski plate, never track plate"*).
* **`app/main.cpp`, beside B1's line** —
  `if (const char* e = std::getenv("SEADS_SLED_TAILSHED")) sled_params.track_lat_slip_shed = std::atof(e);`
* **`test/harness/sled_tape.h`** — add `X(track_lat_slip_shed)` to `SLEDTAPE_PARAMS_D`, beside
  `slip_ref_rad` and `track_lat_mu`.

**Identity proof, two parts.** (a) **the branch**: at `0.0` the `if` never runs and `mu_l` is the
shipped expression, byte for byte. (b) **the hoist**: hoisting is a *pure* refactor **only if the six
lines have no side effects and no input that changes between `:1056` and `:1114`** — they read
`p.*`, `throttle`, `v_fwd` and `clutch_blend`'s own inputs, all of which are fixed for the patch.
**★ FOLDED P0-5 — THE HOIST PROOF HAD NO OWNER AND NO COMMAND, AND THE LEG IT WAS PINNED ON DOES
NOT TEST IT.** `sled_tape_round_trip_replays_bit_identical` (`test/unit/test_sled_tape.cpp:196`, read
this pass) synthesises a tape **in memory** with `drive(f, true)`, loads it from an `istringstream`
and asserts `t.recs.size() == kTicks` and `t.params.comfort.lean_bite_gain == 0.1234567891234`. **It
never opens `D:/flight_sim2/seads-recon/build-play`.** The corpus replay exists only in the tool —
`tools/sled_probe.cpp:2389 probe_tape_replay`, dispatched at `:6390` as the `tape` subcommand — which
runs against files **outside the repo** and therefore can never be a ctest leg. The obligation is
real, so it is given an owner and a command here:
* **the command (a human's, not the gate's):**
  `<lane-build>/seads_sled_probe.exe tape D:/flight_sim2/seads-recon/build-play/sled_tape_<N>.sledtape`
  for **N = 86, 87, 88, 89, 90, 91** — the six v17 drives (91 `.sledtape` files are present in that
  directory, MEASURED by `ls`; they are opened **read-only** and nothing is written there). Each must
  print `VERDICT: bit-exact` **and `dial gap: none`** and exit 0. **The lane that writes the hoist
  runs it, on its own build, BEFORE it writes the branch.**
* **the hermetic surrogate the gate CAN carry (owed, new):**
  `sled_tail_shed_hoist_is_a_pure_refactor` — the existing `drive()` helper, one synthetic 600-tick
  run with the dial absent, final `SledState` pinned as a golden. **That is the thing a hoist bug
  reds inside ctest**, where a corpus replay can never live.
* **and stop citing `:196` for a property it does not have.**
The rest of the obligation stands: **it must be run before the branch is written, not after** —
otherwise a hoist bug and a dial effect are indistinguishable.

### 4.4 THE TEST LEGS, each with its killing mutation

**Owed by B1 (four legs, two new):**

| leg | what it asserts | killing mutation |
|---|---|---|
| `sled_rolled_throttle_frac_zero_is_the_shipped_zero` **(new)** | **★ FOLDED P1-2 — the leg as first written has no observable.** "Bit-identical to the shipped kernel" cannot be asserted from a unit test once the kernel is edited (there is no pre-edit kernel in the build), and `grep -n throttle sim/sled.h` finds **no `SledState` readout of the commanded throttle**. The observables that DO exist are `engine_rpm` and `belt_speed_ms`, computed from `throttle` **outside** the contact block (`sim/sled.cpp:1337-1361`, read this pass). Write it as: rolled, `hands_on`, `in.throttle = 1.0`, dial 0.0, **body forward speed ≤ 3.0 m/s** ⇒ `engine_rpm == 1700.0` exactly and `belt_speed_ms == max(v_bf, 0)`. **The 3.0 m/s clause is load-bearing**: above `clutch_engage_ms − 0.25 = 3.0` the clutch blend is non-zero and a coasting machine reports rpm above idle at zero throttle, so a leg asserting a flat 1700 would red on a machine still sliding. The bit-identity claim belongs to the golden set, not to a unit leg. | write the branch as `(1 - frac) * thr_in` by mistake ⇒ 0.0 delivers **full** throttle ⇒ `engine_rpm` pegs ⇒ instant red. This leg exists to catch exactly that sign/complement slip. |
| `sled_rolled_throttle_never_reaches_a_handless_rider` **(new)** | `grip.attached = false`, dial **1.0**, `in.throttle = 1.0` ⇒ commanded throttle **exactly 0.0** | collapse the two branches into one (`s.rolled ? frac*clamp01(in.throttle) : clamp01(in.throttle)`) and drop the `hands_on` guard ⇒ red |
| `sled_onside_recovery_is_momentum_not_magnetism` (`test_sled.cpp:2649`, **existing — THE WALL**) | at the driven value, a downed machine does **not** drive itself upright on track thrust | raise the dial until the leg passes only because she walked herself over. **That pass IS the failure**, and this leg is the wall the value is chosen against. |
| `sled_tape_round_trip_replays_bit_identical` (`test_sled_tape.cpp:196`, existing) | **★ FOLDED P0-5 — it does NOT assert the v17 corpus.** It drives a tape in memory and checks the round trip and one moved dial; the corpus replay is a **tool** run (`seads_sled_probe tape <abs path>`, §4.3), owned by a human on the lane's own build. Keep this leg for what it does test — that a new rostered dial survives the text round trip bit-exactly — and read the corpus obligation from §4.3. | remove the roster entry ⇒ the tape stops naming the dial ⇒ the `dial_gap` tripwire fires and the drive can disagree with its own tape |

**Owed by B2 (four legs, one new, and the new one is the rung):**

| leg | what it asserts | killing mutation |
|---|---|---|
| all 11 `test_sled_selfright.cpp` TEST_CASEs at `1.0` | byte-identical | — (no physics in the diff) |
| `selfright: a committed press beats mashing` (`:240`, existing) | **at 0.5** a 12 Hz mash still fails where a 1.0 s/0.7 s cadence rights her | if it **fails** at 0.5, the brace was carrying the whole righting and N2's diagnosis is dead. **That is a real outcome, not a bug** — report it, do not tune around it. |
| `selfright_a_timed_rock_beats_a_mistimed_one` **(NEW — THE LEG THAT DECIDES THE RUNG)** | from the same 180° start, same STAND cadence: drive `lean_lat` as a square wave **in phase** with `angular_vel.z` and **180° out of phase**; the in-phase arm must right in strictly fewer seconds | **if both arms right in the same time at 0.5, the clamp was not the blocker and the entire N2 diagnosis is wrong.** This leg must be written **before** the drive, and run at 1.0 as well as 0.5 — at 1.0 it should show **no discrimination**, which is the positive control. |
| `selfright: above 5 km/h it cannot happen` (`:301`, existing) | unmoved at any frac | — |

**Owed by B3 (five legs, two new, and one existing leg must be RE-READ first):**

| leg | what it asserts | killing mutation |
|---|---|---|
| **THE HOIST PROOF** — full golden set + **the six-tape corpus replay by name** (§4.3, a human command on the lane build), env unset, **run before the branch is written** | byte-identical | a second copy of the slip formula instead of a hoist. **★ P1-1: the tripwire is `grep -c "v_track - v_fwd) / std::max(v_track" sim/sled.cpp` == 1** — the old `grep -c "(v_track - v_fwd)"` form returns **2** on the shipped tree and would red a correct edit. |
| `sled_track_lat_mu_is_the_rostered_value` (`test_sled.cpp:1470`, existing — **★ DISCHARGED, RE-READ THIS PASS, NO ACTION**) | it constructs `const sim::SledParams p;`, so `track_lat_slip_shed` is **0.0 inside it** and the new branch is dead there at any shipped value; both halves — the `p.track_lat_mu == 0.70` assertion and the corner probe's `REQUIRE_FALSE(s.rolled)` — are unaffected. The owed re-read is done. | it would only bite if the leg were ever rewritten to construct params from the TOML or from a tape; if that happens, split it into a param leg and an effective-μ leg **first** |
| `sled_wrong_way_lean_is_never_a_penalty` (`test_sled.cpp:2546`, existing) | at the driven value, a wrong-way lean gives **exactly** the shipped number | use the raw signed lean instead of `align_m` ⇒ red. That leg measured the last leak at the **4th decimal**; this term must not open a new one. |
| `sled_tail_shed_is_track_only` **(new)** | ski-patch lateral bite bit-identical at **any** dial value | drop the `g.is_track` guard ⇒ the carve moves ⇒ red |
| `sled_tail_shed_is_inert_in_a_straight_line` **(new)** — **★ FOLDED P1-5, REWRITTEN: AS FIRST WRITTEN IT IS GUARANTEED RED** | at `align_m = 0` the shed factor is `1 − shed·|trk_slip|`, **not 1**, so a 600-tick WOT run on real ground changes `mu_l` and the leg reds at any non-zero dial. Write it on a **hermetic flat fixture, zero commanded steer, zero lean, where `slip_ang == 0`** — there the lateral force is zero whatever `mu_l` is, so the trajectory is bit-identical and the leg means something. Its comment must state that the straight-line shed is **real** and is the part that can make a bank strike worse. | if the fixture's `slip_ang` is not identically 0 the machine is already crabbing and the leg is measuring the wrong thing — **report the crab, do not relax the leg** |

### 4.5 THE DRIVE CHECKLIST

**⚠ BEFORE ANY OF IT.** Announce the lane to the sentinel before any push to main (memory:
*announce EVERY main push*). **Nothing here lands on main.** Fresh lane off `ed0a43ce8` or current
main, built in its own worktree — **never in `D:/flight_sim2/seads-recon`, which is his fly tree** —
and the drive exe path is the one the lane **actually builds**, named absolutely in the handoff, not a
placeholder. Build with `build-play`, launch it for him, and **put the checklist in the reply**.

**Keys, so the checklist is unambiguous.** W throttle · S brake · **A/D bars** · **mouse = lean**
(dx → lateral, dy → fore/aft; Q/E are the keyboard lean) · **LEFT SHIFT = STAND**, LEFT CTRL = tuck ·
R autoright · C recentre · H swaps the HUD.

**SEVEN LAUNCHES — RE-ORDERED BY THE RED-TEAM FOLD (P0-1 trial order, P0-2 ceiling first, P0-4 gate
before rung, P2-1 two answers on run 6). One variable at a time. Tape every one.**

**★ FOLDED P0-1, and it goes at the top of the sheet in this exact wording:** *"If the rider is off
the bars, W is MEANT to do nothing — check the rider before you judge the dial."* The HUD's grip
readout is `sled_grip_attached` (`app/main.cpp:10822`). DERIVED: it takes about **3.8 g held for a
second, or 10 g held for a sixth of a second**, to throw him off — so on an ordinary bank clip he is
still holding on and the dial should speak.

1. **BASELINE — no env var set.** Ride the way you rode last night, two minutes.
   *This run must feel exactly like tonight's main.* If anything feels different, **stop** — an
   identity is broken and nothing below is readable.
2. **B0 — `SEADS_SLED_TRACTION_MU=3.0`, alone.** ★ P0-2: **the ceiling goes on before B1, not after
   it.** `docs/snowform_measurements.md` §M8.1 says μ ≥ 3.0 leaves his traverse byte-identical
   (14.1 / 14.5 / 15.3 m/s) while killing the Bush runaway. **The question: did anything change at
   all?** If your traverse or your snowbank jumps feel weaker, say so — that is the ruling you gave
   on 2026-08-24 being tested, and it outranks everything below.
3. **B1 — `SEADS_SLED_ROLLED_THROTTLE=0.15`, with B0 still armed.** ★ P0-2 caps the first candidate
   at 0.15 (was 0.35) until the wall leg has been read at the value.
   1. **(a) FIRST, THE GENTLE ONE.** ★ P0-1: roll her over at walking pace with your hands on the
      bars — no bank, no speed. **On your side, hold W for two full seconds.** Today: silence, idle
      rpm, no roost, no motion.
   2. **(b) THEN the bank strike**, the way it actually happens to you.
   **The question is two questions, and they are different:** (a) is the **noise and the roost alone**
   what you were asking for? (b) do you want her to actually **move**? If (b), say how far — a machine
   that drives itself upright is the thing the kernel forbids by name, and the answer sets the value.
4. **B2a — `SEADS_SLED_RIGHT_MAXSPD=4.0`, alone (no STAND_SHIFT).** ★ P0-4: **the gate is the
   measured blocker; it is driven before the rung it blocks.** Today the righting pump only arms
   below **1.39 m/s** (5 km/h) and it is open only 13–20 % of the time you are over.
   > Tip her over and press SHIFT while she is **still sliding**, not only once she has stopped.
   **The question: is she now trying to come up at moments where she used to ignore you?** And the
   honest second one: **does she come up too easily?** — because this widens a number you have been
   driving for weeks.
5. **B2b — add `SEADS_SLED_STAND_SHIFT=0.5`.**
   > On your side, **press-and-release SHIFT on a rhythm — about one press a second — and swing the
   > mouse left-right in time with her rocking.** Today the mouse does nothing while SHIFT is down and
   > only bites in the gaps. At 0.5 your body is live the whole time.
   **The question: does the TIMING start to matter?** If a sloppy rhythm fails where a good one comes
   up, that is the pendulum you asked for. **If it feels worse — if she will not come up at all —
   say so and stop**, because that means the automatic brace was carrying the whole righting and the
   diagnosis is dead. Then try **0.25** and **0.75** on the same bank if you have the patience.
6. **B3 — `SEADS_SLED_TAILSHED=0.15`.** ⚠ **Not on the same run as anything else.** ★ P1-3: the
   first candidate is **0.15, not 0.5** — at 0.5, with WOT at 52–72 % of your driving, the track loses
   a third of its sideways grip in a straight line and two thirds in a developed yaw.
   > Third gear on the trail, 10–15 m/s, **set a lean into a corner and hold the throttle open instead
   > of tapping the brake.** The rear should step out under power, more the harder you lean into it,
   > and come back under you when you lift. **Then do the same corner with NO lean** — it should
   > barely move.
   **★ FOLDED P2-1 — THIS RUN OWES TWO ANSWERS, WRITTEN SEPARATELY, NOT ONE:**
   **(i) the drift:** did the tail come around under power, and did leaning into it do the work?
   **(ii) the bank:** **did she roll MORE?** This dial takes yaw resistance away from the track, which
   is the thing that keeps a ski-on-a-bank strike from becoming a spin. **If the bank strikes got
   worse, that is the finding and it outranks how good the drift felt** — and the fix is the
   lean-gated fallback form in rung 3 (`(1.0 + align_m)` → `align_m`), not a smaller number.
   If 0.15 is inaudible on both counts, step to **0.3**; **0.5 only after that.**
7. **Then all of them together, one run**, and one line for the combination.
8. **One line per run afterwards** — "good" / "worse" / "didn't notice" — **plus B3's two separate
   sentences and B0's traverse sentence.** That is next round's R1, taken while it is fresh.

**Not in this drive, deliberately:** `class_blend_m` (rung 6 — **a different night**, on TrailMain, and
it is road-repair's), `throttle_power_frac` (rung 4 — it and B3 both change what a held throttle feels
like), `k_air_shift` (rung 5 — one ruling owed first).

---

## 5. RULINGS STILL OWED — one line each

**Blocking a rung on this ladder:**
1. **N5-PAIR (blocks rung 5)** — `k_air_shift` gives roll and yaw in the air; **throwing your weight aft pitches the nose DOWN** (correct physics, opposite of expectation), and the lever that lifts it is the throttle (`k_gyro_react`, built, shipped at 0.0). **Do you want the pair armed on the same night, or `k_air_shift` alone with the nose-drop as a known?**
2. **R2 (blocks rung 6)** — `[snowpack] class_blend_m` 0.0 vs 1.0. Reserved for you a week ago (`LANES.toml:939`). Nothing to build, only an A/B — **and it changes the man on foot at every corridor edge.**
3. **R7 (blocks the dropped pitch rung)** — do you want a floor under the end-over-end at all? **Your morning words point away from it** (*"deliberate jumps for performing a flip off of a banks for fun"*) — say so out loud and the rung dies cleanly instead of sitting.
4. **R7-class word (blocks rung 2's second half)** — should the self-right measure "tipping over" against the **hill** instead of the **planet**? It is the only proposal in the audit that **removes** kernel-commanded rider-mass movement.

**New, raised by your morning words — none blocks the first build:**
5. **N2-CONFLICT** — on 2026-08-26 you ruled *"I dont understand why yo are clonlating the lean mechanism with the righting"*; this morning you asked to **right by rocking your body**. `sim/sled.cpp:1747-1757` says in as many words that the lean must not touch the righting. **Which ruling wins?** *(Rung 2 as built does not touch that line — it only stops the clamp from eating your rock — but the conflict is real and you should know it exists.)*
6. **C2-PRESS** — the C2 side-righting term has **no press gate at all** and rights a downed machine on its own. Does *"not automatic re righting"* reach it, or is C2's *only-while-sliding-above-1.5-m/s* gate exactly the *"self righting by chance"* you asked for in §0b? **This is the ruling that decides whether C2 stays as shipped.**
7. **RC-3 / drift character** — your morning sentence rules the **direction** of rung 3 (*"throttle should be able to swing my tail around"*), so the build is unblocked. What is **not** ruled is whether the resulting change to the drift's character is the drift you signed. **That is a drive answer, not a pre-build ruling — but say it after run 4.**
8. **B3-vs-BANK** — rung 3 and your bank-strike complaint pull in opposite directions, by construction. **If run 4 makes the bank strikes worse, which one do you want?**
8a. **N4-PAIR — NEW, and it BLOCKS rung 4 (red-team P0-3/P1-6/P1-7).** *"Throttle is all or nothing"*
   cannot be answered by one dial. The drawbar-cap scale (`throttle_power_frac`) is **output-side**:
   it flattens the top but leaves the bottom of the travel a track **brake** (`v_track = throttle×46`),
   it scales **every transient including your launch** (the thumb ramps 0→1 in 0.40 s and your median
   press is 0.22–0.35 s), and because the cap is symmetric it cuts the track's **brake** as well.
   **Do you want the pair — an input curve on the thumb AND the output ceiling — or neither?**
8b. **B2-GATE — NEW.** `right_assist_max_ms` is 1.3889 m/s (5 km/h) and the righting pump is armed
   only 20.3 % / 35.6 % of the time you are over. Widening it is one env line and it is the measured
   blocker — **but it changes a number you have been driving for weeks.** Drive-only tonight;
   **do you want it widened for good, and to what?**
8c. **B1-CEILING — NEW.** Rung 7's `traction_mu` is promoted to B0 because a spinning track on an
   unloaded machine is exactly the 55 N case the ceiling exists for. **Confirming you are happy for
   your 2026-08-24 ceiling ruling to be armed on the same night as the rolled-throttle dial** — the
   alternative is choosing B1's value against a machine that stops existing the day rung 7 lands.

**Carried unchanged from the packet, none touched this round:**
9. **R3, second half** — the `rolled` flag is a single hard edge at **75.06°** with no release hysteresis, while the C1 hull rests a downed machine at *"~65–75°"* — the threshold sits **inside** the resting band. **Should it release later than it latches?**
10. **R5** — the drift ladder and the grip trade: Bush lean-in carve **323 m @ damp 0 / 143 @ 150 / 42 @ 400 / 30 @ 800**. **Is 800 still the trade you want?** Attached: `lat_mu_scale` is the one move that can cost you the sliding you signed.
11. **R6, second half** — the STAND self-right has **no contact gate at all** (≈ 1 244 °/s of free airborne roll on a held press). **Should it require ground?**
12. **R8** — your mouse CPI and Windows pointer sensitivity. **No lean-scale number can be picked without it**, and rungs 2, 3 and 8 all ride the lean.
13. **R9** — lean and aim at the same time: while the Sting is shouldered the mouse drives the aim and **your lean freezes wherever it was** (`lean_return_tau_s` is 0.0 in all 84 tape headers). (a) leave it frozen, (b) let it fall to centre — *which is the app moving the rider without you*, (c) re-bind the aim.
14. **R10 — REPORT** — `docs/gi4_ride_handoff.md` §2's *"of its 4106 N"* used `(331 + 87.5) × 9.81`; the bytes run on **3 246.0 N**. Overstated by **26.5 %**. **Correct the old document, or leave it?**
15. **R12 — REPORT** — `config/scenario.toml:2098-2099` still describes the lean-picks-the-side behaviour **you had deleted**; and `right_assist_min_tilt_rad` is a **required** TOML key `sim/sled.cpp` never reads. **Correct the lying comment and the decoy key?**
16. **R13 — REPORT ×3** — `track_rail_half_m` has **said 0.14 and run 0.19 since 2026-08-12** (and the charter still prints 0.14); the trip band drifted from your signed **50/60** to a shipped **60/70**; `right_assist_nm`'s comment derives **1500** and the file ships **2400**. **Which do you rule, and which just get corrected?**
17. **R14** — braking decel p90 is **7.85 m/s² = 0.80 g on snow** against LITERATURE 0.4–0.5 g, on a dial you signed, and brake duty is up to **8.7 %**. **Is 0.80 g the feel you want, or an accident?**
18. **R1-NEW — the one this round creates** — X1 re-scored your rollover rate from **4.32/min to 1.33 tumble events/min** and *"seven of ten never recover"* to **88 % DO recover**, by merging re-crossings of the same tumble. **Those are reports against numbers this audit published to you yesterday.** Do you want the old figures corrected in the packet, or left standing beside the correction?

**And one more instrument that is no longer descriptive** (reported, not removed): the comment above
`sim/sled.cpp:1679` says the self-right block *"is 0.0 by default and 0.0 for every existing tape."*
Both halves were true when written; the **shipped config is 2400** and **tapes 86–91 all name it**, so
anyone reading that comment to find out whether you have a righting pump is told you do not.

---

## 6. WHAT THIS DOCUMENT REFUSES TO CLAIM

1. **That any of it is FELT.** Nothing here has been built, probed or driven. Every "invariant" is a
   test to run, not a result. **THE FEEL IS SIGNED** — picking a value is his seat.
2. **That rung 3 is safe.** It is the one rung on the ladder that can make his loudest *old* complaint
   worse, it says so at the rung, and the drive checklist asks the question that would kill it.
3. **That the first build's three rungs are independent.** B1 and B2 both act on a downed machine and
   **could interact** — a spinning track adds a roll moment (B1's honest limit) exactly where B2 is
   trying to make his timing matter. **That is why run 2 and run 3 are separate launches and run 5
   exists.**
4. **That X1's re-score is final.** Every number in §1 lines 2 and 5 carries the 2.0 s merge gap as its
   killing mutation, and `hands_on` is unpinned in the tape, so every pump and righting count is an
   **upper bound**.
5. **That rung 6 is demoted on merit.** It is not. It is the strongest measured finding in the audit
   and it is another lane's A/B.
6. **That the env-var route is the landing route.** It is the **drive** route. Landing any of these
   three means the six touches and a `config/` edit, and that is a different pass with a red-team in
   front of it (standing rule: fresh-context adversarial consult before landing).
7. **That N4's "binary thumb" is measured end-to-end.** X1 measured the *command*; the three-mechanism
   explanation is arithmetic on the shipped source. **A tape leg measuring the distribution of
   `in.throttle` between 0.10 and `v_fwd/46` is free and unrun.**


---

## 7. FOLD LOG — the 2026-09-18 both-lenses red-team, item by item

**Provenance.** The red-team returned verdicts **LAND-WITH-FIX** (rungs 1, 2, 3 and THE FIRST BUILD),
**DO-NOT-LAND as one dial** (rung 4), **DO-NOT-LAND, blocked correctly** (rung 5), **LAND-WITH-FIX,
not this lane's** (rung 6), **LAND — and PROMOTE** (rung 7), **LAND as scheduled** (rung 8).
Every item below was re-derived against this worktree (`audit/sled-ride`, tip `255b8a6fa`) before it
was folded; **nothing was taken on the red-team's word.** No build, no `ctest`, no `seads.exe`, and
no tape was opened (the tape directory was **listed**, to name the six files by absolute path).

| item | verdict | what changed, and where |
|---|---|---|
| **P0-1** grip latch gates B1 | **FOLDED — with a correction to its premise** | Rung 1 gained the latch block: one-way latch (`sim/rider_grip.cpp:93`), sole re-attach `app::player_mount_request` (`app/main.cpp:8422`), `unseat_gain` forced to 0.0 on every tape (`test/harness/sled_tape.h:415`). **The number the red-team asked for, DERIVED:** with the kernel buck at 0.0 (`sim/rider_grip.h:74`, no override anywhere) the unseat needs **|g_eff| > 37.6 m/s² = 3.83 g sustained**, or ≈ 0.12 s at 10 g / ≈ 0.03 s at 20 g (raw; ×≈1.4 after the 0.1 s low-pass). **CORRECTION: that is a HIGH bar, so B1 is reachable on most of the population he named** — the red-team assumed the latch had usually fired. Its three fixes are folded anyway: gentle trial first (§4.5 run 3(a)), the one-line rider warning at the top of the sheet, the number in the packet. |
| **P0-2** B1 × `traction_mu` coupling | **FOLDED IN FULL** | Rung 7 **promoted to B0** in the §2 table, the §4 build table and the checklist; `SEADS_SLED_TRACTION_MU` added in §4.2; first B1 candidate cut **0.35 → 0.15**; `sled_onside_recovery_is_momentum_not_magnetism` re-cast as a **pre-drive gate**, not a post-hoc leg. Kernel words re-read at `sim/sled.cpp:1212-1218`; `traction_mu = 0.0` confirmed at `sim/sled.h:1006`. |
| **P0-3** rung 4's "wheelie untouched" | **FOLDED IN FULL** | The false sentence is struck in rung 4 and replaced with the true one (*steady-state* WOT only). `app/main.cpp:8290-8292` ramp verified; X1 §5.2's 0.22–0.35 s median key-held press cited; the wheelie question added to the drive. |
| **P0-4** B2's dial is not the blocker | **FOLDED — both offered fixes, ordered** | The red-team offered (a) swap the dial or (b) add a checklist sentence. **Both are taken, as B2a then B2b**, because ordering preserves attribution where a swap would lose the clamp finding. `right_shift_cmd = 0.0` in both else-branches (`:1795`, `:1799`), gate at `:1694-1699`, `right_assist_max_ms` 1.3889 (`config/scenario.toml:2088`), rearm 0.8 (`sim/sled.h:230`), X1 §6.1 shares — all re-verified. New ruling **B2-GATE** raised in §5, because widening a signed number is a feel change, not an identity. |
| **P0-5** the identity proof has no harness home | **FOLDED IN FULL** | `test_sled_tape.cpp:196` read: it drives in memory and asserts `recs.size()` and one moved dial — it does **not** touch the corpus. The corpus replay is `tools/sled_probe.cpp:2389`, dispatched at `:6390`. §4.3 now names **the exact command, the six absolute tape paths, the exit condition and the owner**, adds the hermetic surrogate leg `sled_tail_shed_hoist_is_a_pure_refactor`, and the §4.4 row stops claiming the corpus. **Its "discharged" item is also folded:** `sled_track_lat_mu_is_the_rostered_value` constructs `const sim::SledParams p;` ⇒ the shed is 0.0 inside it ⇒ owed re-read **closed, no action**. |
| **P1-1** the hoist tripwire is wrong | **FOLDED IN FULL** | MEASURED today: `grep -c "(v_track - v_fwd)" sim/sled.cpp` = **2** (`:1144` slip, `:1181` `v_rel` in `roost_thrust`). Corrected to `grep -c "v_track - v_fwd) / std::max(v_track"` == 1 in §4.3 and in the §4.4 leg row. **The old form could have deleted the roost he asked for.** |
| **P1-2** B1's leg 1 has no observable | **FOLDED — with one correction** | Leg re-written onto `engine_rpm` / `belt_speed_ms` (`sim/sled.cpp:1337-1361`). **CORRECTION: `engine_rpm == 1700.0` is only exact below `clutch_engage_ms − 0.25 = 3.0 m/s`** — above it the clutch blend lifts rpm at zero throttle, so the red-team's leg form would red on a machine still sliding. The speed clause is now in the leg. |
| **P1-3** 0.5 calibrated against the wrong slips | **FOLDED IN FULL** | Rung 3's suggested A/B replaced by the ladder **0.15 → 0.3 → 0.5**, with the WOT arithmetic (`trk_slip ≈ 0.65–0.74`, shed 33–37 % straight / 65–74 % leaned) and its killing mutation. Checklist run 6 drives 0.15. |
| **P1-5** a guaranteed-red invariant | **FOLDED — body reconstructed** | Arrived as a verdict-table **label only**. Re-derived: at `align_m = 0` the factor is `1 − shed·|trk_slip|`, so `sled_tail_shed_is_inert_in_a_straight_line` **as written is guaranteed red** at any non-zero dial on any fixture with a non-zero slip angle. Leg re-written onto a hermetic `slip_ang == 0` fixture; **and the straight-line shed is now named as a real property**, with the lean-gated fallback form `(1.0 + align_m) → align_m` written into rung 3 and into the drive. |
| **P1-6** rung 4 is output-side only | **FOLDED — body reconstructed** | Label only. Re-derived from `sim/sled.cpp:1140`: the thumb's dead lower half is the `v_track = throttle×46` mapping, which the cap scale cannot move. In rung 4, and it is half of why rung 4 became a PAIR ruling. |
| **P1-7** the cap is symmetric | **FOLDED — body reconstructed** | Label only. Re-derived from `T = std::clamp(T, -cap, cap)` (`:1211`): scaling the cap also cuts the engine-brake / reverse-shear side — the other half of his gas↔brake turning. In rung 4. |
| **P2-1** two questions, not one | **FOLDED IN FULL** | Checklist run 6 now owes **two separately written answers** (the drift, and the bank), with the explicit statement that answer (ii) outranks answer (i). |
| **rung 5 / rung 6 / rung 8 verdicts** | **no change required** | The red-team agreed with the ladder: rung 5 stays blocked on N5-PAIR, rung 6 stays road-repair's A/B with R2 open, rung 8 stays after B3's drive answers whether `align_m` is the carrier. |

**What the fold did NOT do.** It did not build, gate, drive or land anything; it did not touch `sim/`,
`control/`, `config/`, `test/` or `golden/`; it did not open a tape; and it did not invent bodies for
P1-5, P1-6, P1-7 or P2-1 without saying so — those four arrived as one-line verdict labels and their
reasoning above is **this lane's re-derivation**, marked as such, from the shipped source.