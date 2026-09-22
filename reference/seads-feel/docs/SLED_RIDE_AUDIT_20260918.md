# SLED RIDE AUDIT — 2026-09-18

**For Chad.** Packet writer's fold of the overnight sled-ride audit round.
**Worktree** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
**READ-ONLY against main.** No dial moved, no config edited, no golden touched, nothing built,
no `seads.exe` run, no ctest run, nothing pushed to main. The tapes in
`D:/flight_sim2/seads-recon/build-play` were opened `'rb'` and nothing there was written.

**Sources folded.** `docs/sled_audit/ladder_synthesis.md` (REVISION 2 — the merged ladder with both
red-team folds applied), the five strands `D-A`…`D-E`, four research reads `R1`…`R4` with their four
refutations, three ladders, three judges, two red-teams, and `docs/sled_audit/critic_completeness.md`
(38 gaps, nothing fixed — carried **verbatim** in §6).

**Confidence vocabulary** MEASURED / DERIVED / LITERATURE / GUESS, labelled every time.
**Every numeric claim carries its killing mutation** (§6 is the index; each also carries it in place).
**No feel recommendation rests on a harness number alone** — each cites a tape event **and** one of
your words, verbatim, with doc and section.

---

## 0. EXECUTIVE VERDICT — ten lines

1. **She rolls 4.32 times a minute** — one every 13.9 s — against your ruling *"I rule that it should
   be roll resistant"* (§0). MEASURED, 12.0 min of last night's v17 driving.
2. **Seven of ten rollovers never end** — 71.2 % never return to steady driving, and that number has
   not moved in four weeks and three signed kernels (69 / 70 / 67 / **71 %**).
3. **`sled_tape_91` ends upside down** — last tick tilt 131.4°, speed 0.41 m/s, throttle 0, clean
   `# sig` exit. You shut the night down with the machine on its roof.
4. **But the shipped comfort stack DID work on severity, not frequency** (`R2-…-REFUTE.md` §3,
   corrected A/B): longest episode **53.60 s → 5.09 s**, episodes ≥ 5 s **5 → 1**, rolled share 4.4×.
   Frequency did not move. That is the honest shape of the problem.
5. **Where she rolls is the most legible finding in 206 minutes:** TrailMain **16.88 rolls/min**
   against Bush 4.13 and LakeIce 0.85 — and TrailMain beats Bush **in 12 of 12** tapes that carry
   both. The groomed trail is the rollover surface. *(⚠ leg X1 can delete this — §6-B1.)*
6. **The mechanism is not a mystery.** Six of seven ski surfaces ship a lateral friction coefficient
   **above the machine's own 0.5097 g tip threshold** — an untripped friction rollover is available
   by construction, in a *slide* (the `tanh` gate at 0.300 rad means it is not available in a carve).
7. **And the boundary is a step, not a ramp.** `[snowpack] class_blend_m` ships **0.0**, so one ski
   crossing a corridor edge steps `rho_eff` 0 → 260 under that ski alone, in one tick. The tree's own
   probe: **blend 0.0 → yaw −482.7°, ROLLED; blend 1.0 → yaw −11.6°, no rollover.** It is live-tunable.
8. **Nothing here is built and nothing should be, until you give six drives a word.** There is **no
   felt report for tapes 86–91** (2026-09-17, 20:11–21:39). Every roll complaint on file predates the
   GI3 rollfix. **Ruling R1 can re-score this entire ladder.**
9. **The ladder is eight rungs, each ONE dial with an identity value that is bit-identical to today.**
   Rung 1 (`class_blend_m`) is **your A/B, already reserved for you in `LANES.toml:939`** — it is a
   drive, not a build. Rungs 2–8 need the rulings in §4 first.
10. **14 rulings are owed.** Ten questions, four reports-against-signed-numbers. §4 is one line each.

---

## 1. LAST NIGHT'S TAPES IN NUMBERS
*(MEASURED, `tools/sled_tape_audit.py`, stdlib-only, streaming, read-only. Corpus: 91 files,
11.79 GB, **84 parsed**, 7 skipped as zero-byte; 1 483 776 ticks = **206.1 minutes** of your recorded
driving across five kernel tags. 0 malformed lines. Machine-readable:
`docs/sled_audit/tape_summary.json`.)*

### 1.1 The six drives of 2026-09-17 (kernel v17)

**⚠ Last night is TWO builds.** Tapes 86–90 are `kernel-v17-tremor-signed-33-g4b8c686`; **tape 91 is
`…-53-g7b377c6f0`** — 20 commits later, carrying the road-repair merge. Tape 91 is also the only tape
of the six with Road/TrailMain, and it is by a wide margin the worst. It must not be pooled with
86–90 silently. *(This is `R4-landings.refute.md`'s corpus-homogeneity caveat, and it is the reason
`facet_contact` is out of scope for every claim below — critic gap C9.)*

| tape | build | dur | surface mix | rolls (% past 90°) | recovered / never | R | top m/s | air (upright) | stand | catwalk (p95) | lean/steer sat | WOT | brake | \|assist\| p50/p95 | end tilt |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 86 | -33 | 95 s | Bush 61 / Rock 20 / Ice 18 | **8** (5.4 %) | 4 / 4 | 1 | 38.1 | 10 (30 %) | 22 % | 14.7 % (**66.2°**) | 28 / 16 % | 63 % | 8.4 % | 75 / 715 | 2° |
| 87 | -33 | 119 s | Bush 54 / Ice 23 / Rock 23 | 4 (2.5 %) | 2 / 2 | 0 | **45.8** | 4 (25 %) | 25 % | 15.6 % (**3.8°**) | 28 / 20 % | 48 % | 11.6 % | 45 / 495 | 1° |
| 88 | -33 | 99 s | Bush 55 / Rock 23 / Ice 22 | **10** (7.6 %) | 2 / **8** | 2 | 44.9 | 10 (30 %) | 19 % | 9.1 % (2.8°) | 20 / 22 % | 56 % | 12.7 % | 55 / 495 | 2° |
| **89** | -33 | 127 s | Bush 37 / Ice 34 / Rock 29 | **0** (0.0 %) | — | 0 | 41.9 | 4 (**100 %**) | **39 %** | 22.7 % (3.2°) | 15 / 11 % | 44 % | 4.9 % | 45 / 455 | 2° |
| 90 | -33 | 108 s | **Ice 83** / Bush 17 | 7 (3.5 %) | 1 / 6 | 0 | 38.4 | 9 (**0 %**) | 7 % | 2.9 % (51.2°) | 15 / 5 % | 20 % | 11.9 % | 5 / 215 | 1° |
| **91** | **-53** | 173 s | Bush 61 / **Road 23 / Trail 16** | **23** (**12.7 %**) | 6 / **17** | 2 | 42.8 | 21 (29 %) | 33 % | 7.2 % (50.8°) | **44 / 33 %** | **72 %** | 5.2 % | 75 / **765** | **131°** |

Rollovers by the surface under the machine **at entry**:

| tape | Bush | LakeIce | RockOutcrop | Road | TrailMain |
|---|---|---|---|---|---|
| 86 | 5 in 59 s | 2 in 18 s | 1 in 19 s | — | — |
| 87 | 4 in 65 s | 0 in 28 s | 0 in 27 s | — | — |
| 88 | 7 in 54 s | 3 in 22 s | 0 in 23 s | — | — |
| 89 | 0 in 47 s | 0 in 43 s | 0 in 37 s | — | — |
| 90 | **7 in 19 s** | **0 in 90 s** | — | — | — |
| 91 | 11 in 106 s | — | — | 5 in 40 s | **7 in 27 s** |

**Tape 89 is the control that matters:** 127 s, zero rollovers, 100 % upright landings, the *lowest*
saturation duty of the six (lean 15 % / steer 11 %) and the *highest* stand duty (39 %). **Tape 90 is
the other control:** 90 s on lake ice with **zero** rollovers, and all 7 of its rollovers in its 19 s
of bush.

### 1.2 Where the machine stands against your own bar
*(`ROLL_COMFORT_HANDOFF.md` §0/§0b, verbatim, scored against last night. ⚠ Every row's killing
mutation is the same one and it is a **ruling**, not an arithmetic: see §6-F3.)*

| your word (verbatim, §) | last night | met? |
|---|---|---|
| *"I rule that it should be roll resistant"* (§0) | 4.32 rollovers/min, 5.7 % of time inverted | **no** |
| *"Make it possible to roll but not the rule"* (§0) | one every 13.9 s | **no** |
| *"allow me to land on my skis more often after a roll"* (§0) | 29.3 % upright vs 27.6 % at v13g | **no movement** |
| *"slef righting by chance more"* (§0b) | 71 % never recover | **no** |
| *"balance of body mechanism to ENHANCE ability, i.e. tighten a turn"* (§0) | lean holds (to 36.1 s) and does enhance below 10 m/s; steer has **no intermediate position at all** and inverts above 10 m/s | **half** |
| *"SLiding banging, punchy, jumps"* (§0b) | 4.82 air events/min, 26/58 landings over 10 g, top 164.8 km/h | **yes** |
| *"Don't lose the feel"* (§0) | speed envelope, jump rate, catwalk duty all at or above v13g-era | **yes** |
| *"DOnt make it impossibly hard, there is a batttle going on as well"* (§0b) | inverted 5.7 %, un-recovered 71 %, cannot be fought from | **no** |

**⚠ AND THE ROW THIS SCOREBOARD OWES YOU (critic gap A1).** On **2026-08-21** you wrote
*"EVERYTHING IS GOOD ON THE SNOWMACHINE DRIVE TEST"* — which `D-C_felt_history.md` §1/S6 calls *"the
only unqualified sled drive sign in the whole record after drive 4."* This scoreboard grades the
2026-08-12 bar alone. **Nine days after that bar you signed a drive off without qualification, and
this audit does not know whether that sign was about the ride or about the K-WS1 pose ladder.** It is
not resolved here. It is folded into ruling **R1**.

### 1.3 The five-bucket table, and why it is NOT a kernel ranking

| | pre-reconcile | v13g | v14 ⚠ | v15 | **v17** |
|---|---|---|---|---|---|
| tapes / drive min | 53 / 132.9 | 13 / 39.9 | 3 / 4.6 | 9 / 16.7 | **6 / 12.0** |
| **rollovers past 90° / min** | 5.18 | 6.30 | 0.87 | 2.16 | **4.32** |
| never recovered | 69 % | 70 % | — | 67 % | **71 %** |
| landed upright | 31.9 % | 27.6 % | 100 % | 35.0 % | **29.3 %** |
| **R** presses / min | 0.72 | 1.08 | 0.00 | 0.12 | **0.42** |
| steer sat (>0.98) / lean sat | 24.1 / 25.7 % | 26.0 / 24.5 % | 3.0 / 8.9 % | 11.5 / 18.3 % | **19.0 / 26.5 %** |
| WOT / zero / modulated | 58/34/8 % | 68/23/9 % | 3/93/3 % | 49/45/5 % | **52/40/7 %** |
| brake duty | 3.8 % | 3.8 % | 3.5 % | 6.9 % | **8.7 %** |
| stand duty | 35.3 % | 44.7 % | 1.5 % | 9.8 % | **25.6 %** |
| catwalk duty / p95 / p99 | 12.8 % / 14.3° / 51.3° | 13.1 % / 11.8° / 53.3° | 0.2 % / 6.3° | 1.5 % / 14.8° / 30.8° | **11.9 % / 14.3° / 66.8°** |
| speed p50 / p90 (m/s), top km/h | 7.9 / 26.9, 163.8 | 10.1 / 30.1, 164.8 | 0.1 / 2.9, 49.7 | 5.4 / 19.9, 122.0 | **6.4 / 27.1, 164.8** |

**⚠ `pre-reconcile-20260821` is a TIME WINDOW, not a kernel** (describe-n 55 → 551, three weeks of
kernel work under one base tag). **⚠ The v14 column is not a driving arm**: speed p50 = 0.12 m/s,
throttle at zero 93 % of the time, 4.6 min total — the sled *sat*. Its 0.87 rolls/min and 100 %
upright landings (n = 2) **must not** be read as "v14 fixed the roll."
**And D-B-11, adopted: the bucket-to-bucket rates are confounded at least four ways** (surface mix,
stand duty, speed, and an input-encoding change between tape 40 and tape 48). **No kernel verdict may
be read off that table.** The right instrument — same surface, same speed band, across tags — is
**owed and was not run** (critic gap B5).

---

## 2. MECHANISM FINDINGS — why she rolls
*(DERIVED unless marked. Every constant re-verified by hand against the shipped tree this round.)*

**(a) What it takes to tip her.** The support polygon is a **trapezoid** — two skis wide and forward,
one track narrow and aft — so the tipping line is the diagonal from downhill ski to downhill rail.
With `stance_m 0.927`, `ski_fwd_m 0.86`, `track_aft_m 0.52`, `track_rail_half_m 0.19`,
`cg_height_m 0.564` (`sim/sled.h:543-569`): lateral arm **0.28747 m**, **SSF 0.5097 g**, static tip
**27.01°**. DERIVED, exact arithmetic.
*Killing mutation:* move `cg_height_m` or `track_rail_half_m` — at the comment's real-Indy rail half
of 0.14 m it falls to **0.452 g / 24.3°**.
**⚠ AND THAT 0.14 IS A REPORT YOU ARE OWED (critic gap C4 → ruling R13).** `track_rail_half_m` went
0.14 → 0.19 in `9e7e48a12` with the surrounding comment left untouched, so **the file has said 0.14
and run 0.19 since 2026-08-12** — and `ROLL_COMFORT_HANDOFF.md` §1, *the charter itself*, still
prints 0.14.

**(b) What the kernel's snow is allowed to push sideways.** `sim/sled.cpp:1056-1059` computes lateral
as `-normal * mu_l * tanh(slip_ang / slip_ref_rad)`. The shipped ski table (`sim/sled.cpp:162-193`,
verified by two judges independently): TrailTributary **0.72**, TrailMain **0.70**, Road **0.60**,
RockOutcrop **0.60**, MineWorks **0.58**, Bush **0.55**, LakeIce 0.22; track lateral is a constant
**0.70** (`sled.h:1193`). **Six of seven surfaces ship a lateral coefficient above the machine's own
0.5097 g tip threshold** — an untripped, flat-ground friction rollover is available **by
construction**. MEASURED (table) + DERIVED (comparison).
*Killing mutation:* any of those rows reading below 0.5097, or the rollover proving normal-load-
starved in practice (probe leg L1, §6-E).
**⚠ Honest limit (RT-B P2-13): the table μ is only DELIVERED at slip ≳ 0.3 rad.** With
`slip_ref_rad = 0.300` (`sim/sled.h:1187`), Bush at 0.10 rad of slip delivers **0.177 g**, not 0.55 —
so the friction rollover is available **in a big slide, not in a carve**. Pointing the other way, and
omitted from the first reading: `bite *= 1 + lean_bite_gain·align_m` (`sim/sled.cpp:1067`, gain 0.18)
takes Bush to **0.649 g** at full aligned lean — **leaning INTO a turn moves the machine toward its
own tip threshold**, which is itself your felt item 1.
⚠ This **inverts** R1's own headline (*"she slides before she rolls"*); `R1-real-dynamics-REFUTE.md`
§1 refuted it against this table, and the refutation is what this packet uses.

**(c) The inside ski lifts long before the tip, and that is correct.** Front roll stiffness
`2·susp_k·(stance/2)²` = 19 764.6 N·m/rad; rear (rail split) `2·susp_k·rail_half²` = 3 321.2 N·m/rad
⇒ **φ = 0.856** of roll reacted at the front; inside-ski lift at **0.362 g**. DERIVED. She lifts a ski
at 0.36 g and the surface can still supply 0.55–0.72, so she keeps going to 0.51 g and over. **On a
real machine the lift is where the story ends, because the snow runs out first.**
*Killing mutation:* `susp_k` differing front/rear, or `track_rail_half_m = 0` (φ → 1.0, lift at
0.310 g).

**(d) The landing spike multiplies (b).** `sim/sled.cpp:705`: `normal = susp_k*x + susp_c*xdot` —
**linear, unbounded** — with an `axis_dot` floor of 0.25 giving up to 4× the CG's own sink rate
off-angle; the tangential bite is applied at the contact point (`bite_at_contact_frac 1.0`,
`sled.cpp:988-993`), exactly `cg_height_m` below the CG. A **4 m/s** arrival ⇒ 14 400 N damper normal
⇒ **10 080 N of lateral capacity at μ 0.70** — of which `tanh` delivers a fraction that **depends on
a touchdown slip angle nobody has measured**. So state the table, not one row (DERIVED arithmetic on
a **GUESSED** slip; denominator fixed at gravity's own tipping torque **933.1 N·m** = 3 246.0 N ×
0.287465 m):

| touchdown slip | lateral force | roll torque | × 933.1 N·m |
|---|---|---|---|
| 0.05 rad | 1 665 N | 939 N·m | **1.01×** |
| 0.10 rad | 3 241 N | 1 828 N·m | **1.96×** |
| 0.20 rad | 5 874 N | 3 313 N·m | **3.55×** |
| 0.30 rad | 7 677 N | 4 330 N·m | **4.64×** |
| 0.60 rad | 9 717 N | 5 481 N·m | **5.87×** |

*Killing mutation:* `bite_at_contact_frac = 0.0` drops every row by the arm ratio; or a touchdown slip
systematically below ~0.05 rad, which makes `tanh` kill the term and drops the whole table to ~1×.
**That is a probe question, not a tape question — leg D-1, §6-E, and it is the largest single
unmeasured number in this audit.**

**(e) One patch, two unrelated friction laws.** Longitudinal is Mohr-Coulomb on the snow
(`sled.cpp:1167-1174`); lateral is Coulomb on a constant (`sled.cpp:1056`). **There is no friction
circle in this kernel** — `grep` finds only the per-surface brake budget (`mu_brake`, :1232-1246),
which caps longitudinal braking and never charges lateral. DERIVED. Whatever produces the drift you
signed, **it is not a circle**.

**(f) The denominator, once, so nothing repeats the error.** The machine weighs **3 246.0 N**
(331 × 9.80665; `rider_mass_kg` is split OUT of `mass_kg` — `sim/sled.h:543`, `:784-785`). The honest
claim is *"`rider_mass_kg` never enters the machine's WEIGHT"*, **not** *"never enters a force"* — it
does enter forces, at `sim/sled.cpp:459/464`, as the grip's reduced mass. **The roll-torque
denominator is 933.1 N·m**; the half-stance figure 1 504.5 N·m is a different arm and `sim/sled.h:546`
says it is **not** the rollover's. *(→ ruling **R10**, a report: `docs/gi4_ride_handoff.md` §2's
*"of its 4106 N"* used `(331 + 87.5) × 9.81` and overstates the denominator by **26.5 %**.)*

**(g) The boundary is a step, and the step is a dial you already own.**
`world/snowpack.cpp:700-746` ramps `surf_mix` across a corridor edge only `if (p.class_blend_m > 0.0)`;
`config/world.toml:645` is `class_blend_m = 0.0`, **so `surf_mix` is identically zero on main and
`blend_dials` (`sim/sled.cpp:200-213`) is never called** (MEASURED, D-A §5.7). One ski crossing the
edge steps `rho_eff` 0 → 260 under that ski alone — a pure yaw+roll couple — in one tick. The tree's
own probe record (`config/world.toml:640-644`): **blend 0.0 → yaw −482.7°, roll 166.6°, ROLLED;
blend 1.0 → yaw −11.6°, roll 52.7°, no rollover**, with the 6/10 m/s cells unchanged.
`docs/snowform_measurements.md` §M9.1: a **25 cm** lateral move, straight, throttle open, **no steer
input**, takes her from roll 2.4° to **89° at 16 m/s** and **1.34 rotations at 24 m/s**.
**⚠ Blast radius: TWO consumers.** `surf_mix` is also read by `sim/walker.cpp:98-107` — so moving this
dial **changes the man on foot at every corridor edge**, and no sled tape records the walker.

**(h) Every comfort term in this kernel is a roll or yaw term. Pitch has none.**
`grep -nE "torque_body(\.[xyz])?\s*(\+=|-=|=)" sim/sled.cpp` returns exactly five sites: `:602`
per-patch contact, `:1608` C2 side-righting (roll), `:1655` C3 yaw-arrest, `:1792` STAND self-right
(roll), `:1881` comfort assist (roll). Pitch restoration comes only from the track's fore/aft normal
split, which **saturates the moment one split point unloads** — i.e. exactly when the nose is already
up. That is the mechanism under *"flips happen too fast and easy"*.
**⚠ And the axis labels are `sim/sled.h:772` "X pitch, Y yaw, Z roll"** — any proposal written on ω_y
is a **yaw** damper in this kernel, landing on the drift you signed. (RT-B P0-2 caught exactly that.)

**(i) The self-right measures "tipping over" against the PLANET, not the hill.**
`sim/sled.cpp:1680-1687`: `tilt = acos(up_body.y)` — **tilt off gravity** — so a machine sitting
perfectly conformal on a side-hill reads the slope angle as "tipping over". DERIVED: on a conformal
**20.05°** slope `w_tilt = 0.9997`, full authority — and `config/world.toml`'s own snowpack block
records terrain slope **p95 = 21.1°** over 79 847 land samples, so **~5 % of the world's land is at or
past the full-authority edge of this gate.** The kernel fixed exactly this defect everywhere else and
wrote down why (`sled.cpp:1367-1371`: *"the earlier gravity-tilt gate armed rigid hull contacts during
ordinary slope riding and misread pitched crashes as rollovers"*) — the hull terms moved to
`phi_surf`; **the self-right was left behind.** The aggravator: the block writes up to **0.35 m of
rider lateral displacement** through `right_shift_cmd` — *the kernel moving the rider's mass*, the
thing §0b forbids by name.
**MEASURED (RT-B P1-8, read-only over 86 578 v17 ticks, an upper bound because `hands_on` is unpinned):**
`stand > 0.5` = **25.6 %**, but armed (`stand ∧ speed < 1.3889 m/s`) = **2.62 %**, and with the ramp's
own tilt edge = **2.32 %**. **Of 2 011 armed ticks, 1 022 (50.8 %) have `hull_engage_lp < 0.05`** —
armed with the hull *not* engaged, i.e. not lying on her side — and **839 of those are `sled_tape_89`,
which has ZERO rollovers.** That is the shape the defect predicts.

**(j) The one recovery term the kernel has is gated shut on three-quarters of the population it is
for.** `sim/sled.cpp:1545-1607`, C2 side-contact righting:
`wv = clamp01((v_side − side_right_vmin_ms) / (side_right_vref_ms − side_right_vmin_ms))` and the
torque is `gain · wv · wt · w_side · w_load · wo` — **so `wv == 0` multiplies the gain to zero.**
MEASURED over tapes 86–91: **`wv == 0` on 2 277/4 973 (45.8 %) of past-90 ticks, on 1 178/1 570
(75.0 %) of the stall window** (tilt 100–130°, |ω_z| < 0.5), and on **795/886 (89.7 %)** of tape 91's.
In that same window `wo > 0.8` on **1 570 of 1 570**. The source itself names the trade-off it shipped:
*"(700, 2.5) is the measured best-of-9 … smooth, non-oscillatory settling (**stalls ~105-125 deg
rather than swinging**) … the 15 m/s full recovery is not alive at this pair"* (`sim/sled.h:497-512`).
**A machine stalled at 105–125° with near-zero roll rate is exactly the 71 %.**

**(k) R hands her back turned, at wide-open throttle.** `app/main.cpp:8355-8357` — the R block sets
`sled_lean_lat = 0.0` and `sled_lean_fwd = 0.0` and **never touches `sled_steer_cmd`**, while ninety
lines earlier the **C** key zeroes all three (`:8263-8266`, *"SK-1c: C recentres the bars too"*). And
`sim/sled.cpp:292-293`: `const double throttle = (s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle);`
— **the throttle is held at zero BY the roll flag and released the instant R clears it.**
MEASURED (PJ read D, raw byte stream of tapes 85/86/88/91): **six rightings in the current-map corpus
— zero at centre, `steer_actual` 1.0000 in FOUR of six, throttle command 1.00 in SIX of six.** Tape 91
@ t15130 holds exactly 1.0 for the full 1.5 s after the righting. Corpus-wide: **146 R presses, 144 of
them at a body tilt ≥ 75.06°** — the key is used as designed, not as a teleport.
**⚠ The tape cannot yet tell a held A/D key from a stale accumulator — that is leg X3, free, unrun, and
it is a PRECONDITION of Rung 3.**

**(l) Two instruments in the tree are lying, and one of them lies against a ruling of yours.**
`config/scenario.toml:2098-2099` still reads *"a full LEAN counts like 10 deg of attitude error — his
lean is what picks the side"* — **exactly the behaviour `31e0dd96a` deleted after your v4 drive**
(*"I dont understand why yo are clonlating the lean mechanism with the righting"*). The shipped field
now multiplies the latched brace. And `right_assist_min_tilt_rad` is a **required TOML key**
(`config/load_scenario.cpp:514-515`, `config/scenario.toml:2094` = 0.35) that **`sim/sled.cpp` never
reads** — anyone reading the config to find the self-right threshold finds a decoy. *(→ ruling R12;
the decoy is reported, not removed.)*

---

## 3. THE RANKED RUNG LADDER
*(Folded whole from `docs/sled_audit/ladder_synthesis.md` REVISION 2 — base ladder FEEL-FIRST, all
three judges' grafts applied, both red-teams' 5 P0 + 9 P1 folded in place. **Identity value = the
number at which the build is bit-identical to today.** Every rung is a branch or a multiply-by-zero,
never a 0-weight lerp — the `kernel-v14-leanlead` precedent. Full text, mechanism, invariant and law
check per rung: `docs/sled_audit/ladder_synthesis.md` §2.)*

**⚠ Plumbing, once, because "one loader line" was wrong.** `grep -rn "sled_input" config/ app/`
returns **nothing** — `[sled_input]` DOES NOT EXIST — and the single TOML→params bridge is
`sim::SledComfort` (`config/load_scenario.h:46` → `app/main.cpp:2481`). A field living directly on
`SledParams` has **no loader path at all**. Any new dial is **six touches**: the struct field, the read
site, the `require(...)`, the range `check(...)`, the TOML key, and its own CHECK in
`test/unit/test_load_scenario.cpp` (which covers only 22 of 35 loaded keys today, and all three
config-vs-default drifts hide in the 13 it skips).

| # | rung | THE ONE DIAL | identity | touches | hard preconditions |
|---|---|---|---|---|---|
| **1** | **The seam that spins you** | `[snowpack] class_blend_m` | **0.0** (shipped) | **none — live-tunable today** | **R2** · leg **X1** · route via the lane owning `world/snowpack.*` |
| **2** | **The lean has to buy the arc** | `plane_lat_lean_gain` | **0.0** | `sim/` — six-touch RC-8 plumbing | Rung 1's ruling (same drive) · `lean_bite_gain > 0` |
| **3** | **R hands her back turned AND at WOT** | `autoright_recentre` | **0.0** | `app/` only (recommended route) | leg **X3** — *if X3 shows held keys this rung comes OFF* · **R3** |
| **4** | **The second roll, not the first** | `[sled_comfort] side_right_vmin_ms` | **1.5** | `config/` — already TOML-reachable | **R4** · **R11** |
| **5** | **A track cannot push harder than the snow it stands on** | `traction_mu` | **0.0** | `sim/` — six-touch RC-8 plumbing | **R2**'s drive (rides along) |
| **6** | **Show him the hand that has been holding her up** | `[render] assist_tell_band_nm` | **∞ / off** | `render/` + **one physics-inert `sim::SledState` field** | **R1** · a ruling that an inert `sim/` field is OK for a readout |
| **7** | **"Tipping over" is relative to the hill, not the planet** | `[sled_comfort] right_tilt_surf` | **OFF (a branch)** | `sim/` + `config/` | **R7**-class word · leg **L3** |
| **8** | **Over the back** | `[sled_comfort] pitch_arrest_nms` (+ `pitch_arrest_open_deg`, identity 180.0) | **0.0** | `sim/` + `config/` — **the only NEW kernel term** | **R7** · leg **L6** · leg **L3** |
| *all* | | | | | **R1**, and the §6-E before/after envelope as a landing condition |

### Rung 1 — THE SEAM THAT SPINS YOU · `class_blend_m` 0.0 → 1.0
**Your words.** *"she's too unsteady"* … *"Don't lose the feel, **except the constant rolling**"* …
*"**Make it possible to roll but not the rule**"* (§0).
**Tape.** TrailMain **16.88 rolls/min** vs Bush 4.13 vs LakeIce 0.85; TrailMain > Bush **in 12 of 12**
tapes carrying both (p ≈ 2⁻¹² ≈ 0.00024) across four kernel buckets and four weeks. Tape 91: **7 rolls
in 27 s of trail (15.6/min) against 11 in 106 s of bush (6.2/min)**.
**Mechanism.** §2(g).
**Invariant (two-sided).** TrailMain's and Road's past-90 rate per minute must **fall** AND Bush's must
**not fall by more than the trail's does**. If both fall together it is a stability change, not a
boundary fix — and it bought "roll resistant" by a route your ruling did not authorise.
**Killing mutations.** (i) **Run leg X1 first** — re-attribute each rollover to the surface at episode
*exit*. If TrailMain's rate collapses, you were *crossing* the trail, not rolling on it, and this
rung's tape support is gone. It is free, read-only, one pass, named by all three ladders and **run by
none**. (ii) `class_blend_m = 1.0` with `blend_dials` still never executing makes the dial cosmetic.
(iii) the bankgraze numbers are probe, not tape.
**Honest limit.** `blend_dials` takes the **dominant class** for non-blendable dials and `surf_mix`
ramps only to **0.5** — this softens the μ/ρ step, it does not delete every step.
**Standing.** `LANES.toml:939`: *"⚠ THE STICK RULING: [snowpack] class_blend_m STAYS 0.0 — it is
live-tunable and it is **CHAD'S A/B, not the lane's call.**"* **This rung is a ruling request, not a
build.** The A/B belongs to the lane that owns `world/snowpack.*` (road-repair), which also holds the
`world/props.cpp` unblended `nearest()` risk. This audit hands it over; it does not run it.

### Rung 2 — THE LEAN HAS TO BUY THE ARC · `plane_lat_lean_gain` 0.0
**Your words.** *"**Just allow the balance of body mechanism to ENHANCE ability, i.e. tighten a turn
instead of having to be the necessary condition of not rolling over**"* (§0); felt item 5 *"rider
weight needs authority, not thrown into a spin"*.
**Tape.** You hold **full lean (|lean| > 0.98) for 14.8–43.6 %** of every v17 drive — 26.5 % pooled,
**44 % on tape 91** — and the lean axis holds a *partial* value for up to **36.1 s** against the steer
axis's all-corpus maximum of **0.500 s**. You are asking the reward channel for everything it has,
continuously, and holding it there.
**Mechanism.** Two lean-reward paths exist and the shipped one is the wrong one: `lean_bite_gain 0.18`
multiplies the μ bite on **every patch including the track** (MEASURED self-defeating for an arc —
0.18 → 0.45 took the lean-in carve from **30 m to 295–1037 m of radius**, *"because the track
out-gains the skis"*). And the strongest lean reward was regressed on purpose: `sim/sled.h:1154-1157`
records that shipping `plane_lift_split_frac = 1.0` re-pinned test 2336, *"lean-in no longer tightens
the arc by 10 %… which is **Chad's own felt item 5** … carried as an open debt."*
**⚠ This dial sits in SERIES with the path the rung calls wrong.** `align_m` is computed **only inside**
`if (p.comfort.lean_bite_gain > 0.0)` — with `lean_bite_gain = 0` the multiplier is exactly 1.0 for
**any** `plane_lat_lean_gain`. The rung rides on it, it does not replace it.
**Killing mutations.** (a) wrong-way lean must be **bit-identical**; (b) `track_slip` and the track's
lateral force must be **unmoved** — this buys *ski* plate on steered patches only; (c) straight running
must be bit-identical. **Honest limit:** it is inert where `plane_lat_gain` is inert — `rho_eff = 0` on
**Road and LakeIce** — so it **cannot** answer *"part of what is so fun on the road is driving by
lean"* (SK-1d) and must not be sold as if it does.

### Rung 3 — R HANDS HER BACK TURNED, AT WIDE-OPEN THROTTLE · `autoright_recentre` 0.0
**Your words.** *"I need a key for now that lets me **autoright**…"*; *"allow me to land on my skis
more often after a roll (**even though R works**)"* (§0); *"**DOnt make it impossibly hard, there is a
batttle going on as well**"* (§0b).
**Tape / mechanism.** §2(k).
**Invariant.** (a) No `O` record is followed by `|steer_actual| > 0.1` sustained past **0.500 s** (the
kernel's own bar slew time at `steer_rate_per_s = 2.0`). Today that is **4 of 6**. (b) The throttle
command in the first tick after an `O` — today **1.00 in 6 of 6** — is a **ruling (R3)**, not this
dial; report it on the same drive so you can rule with the number in front of you.
**Killing mutations.** The `O` tick itself will still show a non-zero bar — the test is the **0.5 s
after**. `app::autoright_legal` gates the key, so a legality change silently changes what the metric
counts. **And the honest one: leg X3.** `app/main.cpp:8244-8256` decays `sled_steer_cmd` at `3.0·dt`
**unconditionally** when no key is held, and the tape writes `in.steer` beside the pinned
`steer_actual` — flat at ±1.0 = a **held key** and **this rung is decoration**; a 3.0/s ramp to zero
in 0.334 s = a **stale command** and the rung is real. **Its rank-3 position does not stand until X3
has run. X3 has not run.**

### Rung 4 — THE SECOND ROLL, NOT THE FIRST · `side_right_vmin_ms` 1.5
**Your words.** *"allow me to land on my skis more often after a roll … **But not every time — allow
it to happen**"* (§0); *"**slef righting by chance more**"* (§0b).
**Tape.** **71.2 % never return to steady driving** (37 of 52), unmoved across four weeks and three
signed kernels. The 52 past-90 crossings merge into **16 crash episodes — 3.25 crossings per crash**,
one crash every 45 s. A crash costs **p50 7.02 s, p90 18.13 s, max 38.41 s** to get back to 80 % of
the speed carried in. You are **not** reaching for the backstop — 5 R presses against 52 rollovers.
**⚠ CORRECTION OWED (critic gap C1): the censoring count printed in the source ladder is one low.**
`R3-player-experience-REFUTE.md` F3 re-derived the same `spd.json` and got **16 scorable episodes, 5
right-censored**, not 15/4, *"and the offset is exactly +1 in all eight sweep rows."* **The p50 7.02 s
is a FLOOR either way; the counts in this packet are the refuter's.**
**⚠ AND (critic gap C3): "D-B-2 / R3-M3 / PJ read A" is not three sources.** R3-M3's 71 % is the
arithmetic complement of R3-M1's 3.25 crossings/episode. **Two of the three are the same arithmetic.**
**Mechanism.** §2(j). **The dial changed at red-team (RT-B P0-1): it is `side_right_vmin_ms`, NOT
`side_right_gain_nm`** — no value of the gain can reach a machine whose lateral speed is under
1.5 m/s, and that is three-quarters of the stalled population. `side_right_gain_nm` (700.0) is kept as
a **second** dial, graded only on the 2 696 still-sliding past-90 ticks where `wv > 0`.
**Invariant (two-sided).** `never_recovered` must **fall from 71.2 %** while `rollovers_past90_per_min`
**does not fall below ~2/min** — if the rolls stop, the rung has broken *"Make it possible to roll but
not the rule"* in the other direction. And the recovery must stay **non-oscillatory**: no episode may
cross ±140° of `phi_surf` more than twice.
**Killing mutation / the wall.** `test/unit/test_sled.cpp:2649`
`sled_onside_recovery_is_momentum_not_magnetism`, whose own comment names **`side_right_vmin_ms 0 with
the gain high`** as what kills it. Lowering this dial walks toward the anti-magnetism line **on
purpose**; that leg is the wall. If the extra recoveries arrive via violent repeated near-180°
oscillation, **the rung is dead as written** — a parked machine that stands itself up is the cheat the
kernel forbids by name.
**⚠ The two-sided invariant has NO DENOMINATOR until ruling R4** (is a backflip a rollover) — the
~2/min floor is currently counting your backflips as failures. **And no instrument in 206 minutes of
tape can apply your answer once you give it** (critic gap B2: the `air_s>0`-at-entry ∧ pitch-dominant
separator is specified and unbuilt).

### Rung 5 — A TRACK CANNOT PUSH HARDER THAN THE SNOW IT STANDS ON · `traction_mu` 0.0
**Your words, verbatim, quoted into `sim/sled.h` at the dial itself (2026-08-24).** *"**at high
speeds, getting pulled in and flipping out like 15x is not desirable so yes take care of that**"* —
and in the same ruling the lift itself **stays**: *"it is important for traversing atop the snow"*,
*"at slower speeds I think it is good too because [I'd] like to jump the snowbanks."*
**Tape.** **WOT 52 % of last night's drive time**, and tape 91 at **72 % WOT with 23 rollovers in
173 s** — you ride with the thumb pinned, which is exactly the regime this ceiling governs.
**Your ruling was TAKEN, the mechanism was BUILT, and it ships at 0.0.**
**Mechanism.** `sim/sled.cpp:1211-1222`. Both existing caps on thrust are **ENGINE** ceilings; the
contact ceiling is the **only** one the snow gets a say in. Its own comment: *"`shear`'s cohesion term
and `roost_thrust` are both load-INDEPENDENT, so without this a track carrying 55 N still pulls at
`max_thrust_n` — the term that feeds the speed→lift→contact-down→speed runaway."* At the attractor the
running surfaces hold **62 N of 3 246.0 N — 1.91 % of the machine's weight** — while the drivetrain is
still pegged.
**Sweep already on the books** (`docs/snowform_measurements.md` §M8.1): at **μ ≥ 0.6** the Bush runaway
does not develop (`t(|roll|>20°)` 6.22 s → 1.33 s; `v_end` 24.8 m/s *rising* → 0.1 m/s), and at
**μ ≥ 3.0** your own traverse is **byte-identical to baseline** (14.1 / 14.5 / 15.3 m/s). **A value
exists that pays your ruling and costs your traverse nothing.**
**Killing mutation.** `traction_mu = 0.0` → the branch is dead, bit-identical. **Honest limit:**
measured **inert on Road by construction** (`rho_eff = 0`) — **it does not fix the bank-strike
superspin.** That one is Rung 1. Anyone selling this as that fix is selling the wrong thing.
**Cheapest paid ask in the audit:** the ruling is taken, the mechanism is built, the sweep is on the
books. It drives in the same session as Rung 1.

### Rung 6 — SHOW HIM THE HAND THAT HAS BEEN HOLDING HER UP · `assist_tell_band_nm` off
**Your words.** *"Just allow the balance of body mechanism to **ENHANCE ability**"* (§0) and *"**Allow
for bad driving too but keep the benefit there for good riding**"* (§0b). Both are unanswerable from
the seat today: you cannot learn an enhancement you have never been shown, and you cannot tell good
riding from bad if the machine's contribution is silent.
**Tape.** `assist_active_frac` = **0.798 / 0.860 / 0.866 / 0.876 / 0.884 / 0.937** — a non-zero body-z
torque on **79.8–93.7 % of ticks on every single drive** — magnitude p50 **5–75 N·m**, p95
**215–765 N·m** (= **82 %** of the 933.1 N·m roll budget). And `grep -rln "assist_nm" render/ app/`
returns exactly one file, `app/spawn_policy.h`, **and that is two resets.** It reaches nothing you can
see.
**⚠ The correction that changes what the rung MEANS (RT-A P0-2).** `assist_nm` is **spring plus
damper**: `(−stiff·tanh(φ/roll_ref_rad) − roll_damp_nms·ω_z) · release_eff · w_contact`. Because
`tanh ≤ 1` bounds the spring half at **450 N·m**, **every |assist_nm| above 450 is damper-majority** —
at the p95 of 765 N·m at least **315 N·m (41 %) is `800·ω_z`**. A tell driven off the raw scalar is a
**roll-RATE meter** that would burn brightest exactly when you **flick her** — the thing
`sim/sled.h:362-365` calls *"the dial that kills the flick, **the drift's body language**"*. **The
tell must be gated on the SPRING component alone**, which is **not stored anywhere today** — so this
rung needs **one new, physics-inert `sim::SledState` field**. If a `sim/` touch is unacceptable for a
readout, **the rung is DROPPED**, not rebuilt on the raw scalar.
**The instrument you already chose is right there:** the x-ray grid on the sweater
(`render/draw.cpp:5250-5330`, `sled_hud_xray` default true, **H** toggles) and your SK-1d words:
*"make it pasted to his white/grey sweater … **make the grid blue and the ball slag orange**, glowing
as the themed color code."*
**Killing mutation.** The assist is non-zero ~86 % of the time but its **median is 5–75 N·m — 0.5 to
8 % of the roll budget.** A tell gated on `assist_nm != 0` is lit almost always and carries no
information, so **the BAND is the whole rung**; if no band on the **spring** is both informative and
infrequent, **the rung is dead**. Second: *"hold a steady lean at constant roll angle — does the ball
stay lit?"* If it only lights during flicks, it is measuring the drift, not the help.

### Rung 7 — "TIPPING OVER" IS RELATIVE TO THE HILL · `right_tilt_surf` off
**Your words.** §0: *"Just allow leaning a certain way in a particular condition to be the OPTIMAL
weight distro for better traversing, **say up a hill** or around a corner."* And your self-right spec,
verbatim in `config/scenario.toml [sled_comfort]`: *"IF ON THE SEAT AFTER A ROLLOVER PRESSING THE
STAND BUTTON … **MACHINE NEEDS TO BE TIPPING OVER** … NOT ABOVE 5KM/H … IT CAN FAIL TO RIGHT GIVEN
THE SITUATION, PROGRESSIVE HOLD."*
**Tape / mechanism.** §2(i).
**The dial.** **⚠ Do NOT blend toward `|phi_surf|`** (RT-B P1-10) — `phi_surf` is a surface-relative
**ROLL**, equal to tilt only when pitch is zero; at 90° of pure pitch it reads 0 and just past it it
jumps to 180°, so a nose-down 85° crash would have the self-right **REFUSE**. **Use the
surface-relative TILT:** `tilt_ref = acos(clamp(dot(up_body, n_surf), −1, 1))`. `n_surf` is already
declared in the same substep scope, so there is no plumbing to add, and it degenerates to
`acos(up_body.y)` **exactly** in the stated <3-patch fallback. **Write it as a BRANCH, not a
`glm::mix` at t = 0.** The direction term stays gravity-referenced — this changes what counts as
"tipping over", not which way she is pushed.
**Invariant.** All 11 TEST_CASEs in `test/unit/test_sled_selfright.cpp` pass unmoved **with the branch
taken**, including *"selfright: above 5 km/h it cannot happen"*. **Plus a new leg:** a machine settled
conformal on a 20° side-hill with STAND held at < 1.389 m/s receives **zero** self-right torque and
**zero** commanded rider shift. **Plus:** a nose-down 85° attitude on flat ground must **still right**.
**Plus leg L3** — this rung *removes* hill-time righting authority and nothing else on the ladder can
see that.
**This rung moves TOWARD the law.** It is the only proposal in the audit that **removes** kernel-
commanded rider mass movement.
**Risk, named.** The self-right has **NO contact gate at all** — between `sled.cpp:1679` and `1800`
there is not one reference to `normal_sum`, `ground_contact` or `w_contact`, and the speed gate reads
*horizontal* speed. DERIVED airborne authority: **≈ 1 244 °/s of free roll with no ground under the
machine** on one held press. **That is a separate rung with its own ruling (R6) and is NOT folded in
here.**

### Rung 8 — OVER THE BACK · `pitch_arrest_nms` 0.0 *(the only NEW kernel term; last on purpose)*
**Your words.** Felt item 6 *"**flips happen too fast and easy**"*; *"If I lean back and hit a
snowbank, **I flip multiple times end over end... like 5x**"* (2026-08-13) — held against, in the same
breath, *"**I DO want a wheelie ... even MORE so**"* and felt item 4 *"WOT should lift the skis to
~30°"*.
**Tape.** Under `stand > 0.5 ∧ lean_fwd < −0.30 ∧ throttle > 0.90 ∧ air_s == 0` (ground contact —
airborne backflips excluded from the *sample*): pitch **p50 0.75°, p90 9.25°, p95 14.25°, p99 66.75°,
max 85.4°**, the condition holding **11.9 % of drive time**. `sled_tape_86` reaches catwalk p95
**66.2°** at 14.7 % duty; `sled_tape_87` reaches **3.8°** at 15.6 % duty — **the same commanded pose
produced either nothing or a flip, on two tapes four minutes apart.** *That bimodality is the
complaint.*
**Mechanism.** §2(h). **The band is populated** (156 ticks in +20…+60° against 209 past +60°; on tape
91 alone **107 against 52**) — what protects the 30° wheelie is **the smoothstep OPENING at 60°, not
emptiness**. And **the nose-DOWN half is 19× bigger** (3 010 ticks in −20…−60°), so an unsigned
`|pitch|` band would fire on ≈ 29 % of catwalk time and damp her out of a **dive**. **The band must be
SIGNED — nose-up only**, and the axis is **X**, not Y.

```
torque_body.x += −pitch_arrest_nms · s.angular_vel.x · w_contact · w_band   // X = PITCH
```

**Invariant.** Catwalk p99 falls from 66.75° toward the p95 of 14.25° while **p50, p90 and p95 do not
move** and the 11.9 % duty does not move. **Plus the fence that proves it did not land on the drift:
the |ω_y| (yaw-rate) distribution must be BIT-IDENTICAL.** **Plus leg L6, a PRECONDITION:** the charter
demands the backflip be **measured** — *"do NOT delete them … measure the backflip stays POSSIBLE"* —
and `w_contact` is non-zero in the contact ticks that **set the airborne rotation rate**. If the
maximum pitch reached while `w_contact > 0` on a send exceeds 60°, **the rung is dead as written.**
**Honest limit.** This rung does **NOT** address the snowbank end-over-end. `gi2tumble` measured
aft+stand at **2.8 turns @ 18 m/s and 4.4 @ 20** against neutral-seated 1.8 which *recovers* — that
tumble is largely **airborne** and a contact-gated term cannot reach it. **It was never built** and it
is a separate rung.

### Dropped, refused and named — so nobody rediscovers them as omissions
**Dropped for the 8-rung budget (all four real):** `steer_centre_per_s` 3.0 → 2.0 (the bars release
**50 % faster than the machine can act**, `cmd_minus_bar_abs_max = 0.3500` on 5 of 9 tapes — **ship it
beside Rung 3 if you want two input lines in one drive**); `kSledSlipVoice` (mean `|track_slip| =
0.522`; **33.77 %** of ground-moving ticks have |slip| > 0.3 with `roost_flux` < 0.02 — the track is
spinning with **neither plume nor sound**, and `track_slip` has zero consumers); `kSledRpmBlend` (the
coast is **8.60 %** of every drive at 19.34 m/s with the voice singing **5.8 semitones flat**, because
the synth is driven off the **thumb**); PJ-7 `rolled_release_frac` (a single `< cos(1.31)` edge —
**75.06°, no release hysteresis** — while the C1 hull rests a downed machine at *"~65-75 deg"*, i.e.
the threshold sits **inside the resting band**; kept as ruling R3's second half).
**Refused outright, with the reason:** `lat_mu_scale` (the only rung that **can lose the feel you
signed** — it spends cornering grip against an unpaid felt item 3; **ruling R5, not a build**);
`k_air_shift` (hands LEAN a second airborne meaning your 2026-08-26 correction separated on purpose —
ruling R6); `roll_damp_nms` 800 (**you ruled it**, and `sim/sled.h:362-365` says what it costs: *"the
dial that kills the flick, the drift's body language"* — the arc-vs-flick ladder **323 m @ 0 / 143 @
150 / 42 @ 400 / 30 @ 800** is ruling R5); `track_pitch_half_m` (narrow green window: 0.20 → 0 red;
0.18 and 0.22 → 5 and 7 red); `steer_hold_frac` (do-not-ship-alone: with no carveable angle on Road a
hold makes the rollover *more repeatable*); `lean_px_full` (n = 9, does **not** reach p < 0.05);
a landing-damper knee (**but see ruling R14 and critic gap A6 — the 2026-08-29 buck ruling IS a word
about landing hardness, so the refusal's stated reason is wrong even if the refusal is right**).
**Named omissions, in scope of some rung's mechanism, deliberately not built here:** a `w_contact`
assist floor (the *other* half of the pinned lean-carve 0.90 debt); `right_dir_eps = 0.04` (a **2.29°**
dead-cone against a documented 10°); `world/props.cpp`'s unblended `nearest()` (owned by road-repair);
`right_assist_min_tilt_rad`, the decoy key.

### Whole rungs that VANISHED from the fold — reported, not adopted (critic gaps D1, D2, D3)
This packet does not adopt them and does not have the standing to; it refuses to let them disappear.
- **PJ-5, `render::kSledCamRoll`.** `app/main.cpp:9827` is `pose.up = sup;` **unconditionally**; tilt
  past 20° on **23.42 %** of v17 ticks and past 45° on **8.92 %**; `app/rest_horizon.h` — the v13
  horizon-roll law **already shipped and shared by the aeroplane AND the Sting** — is not included by
  the sled path at all. **Roll is the quantity you are grading, and it never reaches your eye.**
- **Physics-honest Rung 5, `lat_combined_frac` (the friction circle).** The only rung in the audit
  that proposes to touch the drift mechanism, with its own measured sizing (TrailMain `T/budget`
  2272/4974 = 0.457 ⇒ track lateral μ 0.70 → 0.623 at WOT) and its own honest limit (inert on Bush,
  i.e. 49 % of your driving). It may deserve refusal — **it was never refused out loud.**
- **D-D-R2 / C11 — the sled has no wind and no sound keyed to its own speed.** `wind_synth` is driven
  by `rd.speed`, **the AEROPLANE's airspeed**, with no mount gate, while **44.48 %** of ticks are
  above 8 m/s. *(Blocked on one unmeasured thing: whether `rd.speed` is ~0 while you are on the sled.)*
- **D-D-R6 / C13 — the track never turns.** `belt_speed_ms` runs a mean **20.50 m/s faster than the
  ground** and has **zero consumers** in `render/` or `app/`; `render/sled_model.cpp`'s 7 687 lines
  contain no belt/cleat/scroll channel. **The track is visibly still while it is spinning.**
- **D-A §3.1(b) — the player brake is discontinuous at a stop.** `sim/sled.cpp:1250-1256` applies full
  brake force whenever `v_fwd > 1e-9` with **no taper**; the force can drive `v_fwd` negative inside
  one substep and then vanish. On a dial you **signed**.

---

## 4. RULINGS NEEDED FROM YOU — fourteen, one question each
*(R1–R11 are `ladder_synthesis.md` §4. R12–R14 are **new this pass**, filed from
`critic_completeness.md` gaps A9/E-C2, E-C1, B4 — every one of them is charter §2's class: **a finding
against a signed number is reported to you, never re-derived.**)*

**R1 — Give the last six drives a word.** There is **no felt report for tapes 86–91** (2026-09-17,
20:11–21:39), and every rollover complaint on file predates a kernel that has measurably changed.
*One line each is enough: "good" / "rolled too much" / "didn't notice."* **Until you say it, no number
in this audit licenses a dial move — and if your word is "she's fine now", this whole ladder is
re-scored against a different sentence and the order changes.**
**⚠ Attached, and it must be asked first:** were tapes **86 and 88** — the two highest-onset-rate
drives of the night — free riding, or deliberate rollover tests? *Every v17 number here assumes free
riding.* **And: your 2026-08-21 "EVERYTHING IS GOOD ON THE SNOWMACHINE DRIVE TEST" — was that about
the ride, or about the pose ladder?**

**R2 — `[snowpack] class_blend_m`, 0.0 vs 1.0: rule it.** `LANES.toml:939` reserved it for you a week
ago. There is **nothing to build** — only an A/B to drive. Rung 5 (`traction_mu`) rides the same drive.

**R3 — Should the machine still cut the throttle the whole time she is on her side?**
`sim/sled.cpp:292-293` zeroes throttle whenever `rolled` is set — **5.74 % of your v17 ride time** —
and `app/main.cpp:7907-7910` gates the **drone** on the same flag. Between five and six percent of
your ride is time in which **neither the machine nor the weapon answers**, against *"DOnt make it
impossibly hard, there is a batttle going on as well."*
*Second half:* that flag is a single hard edge at **75.06°** with no release hysteresis while the C1
hull rests a downed machine at *"~65-75 deg"* — **should it release later than it latches?**

**R4 — Is a backflip a rollover?** §0's first sentence is *"Yep I can do backflips."* Every
roll-resistance number on every ladder counts **attitude, not intent**, and no instrument in 206
minutes of tape separates a deliberate send from a crash. **Without your definition, Rung 4's
two-sided invariant has no denominator and the ~2/min floor is counting your backflips as failures.**

**R5 — The drift ladder, and the grip trade.** Before anyone touches `roll_damp_nms`, look at the
MEASURED arc-vs-flick ladder (Bush lean-in carve **323 m @ 0 / 143 @ 150 / 42 @ 400 / 30 @ 800**) and
say whether **800** is still the trade you want. *This packet proposes no move.* Attached:
`lat_mu_scale` would spend cornering grip to buy roll resistance — **the one move that can cost you
the sliding you signed.**

**R6 — Do you want to steer in the air, and at what VALUE?** ⚠ **Your build ruling is already TAKEN**
— *"I think we should build it. Because then it has the foundation it needs."* (2026-08-27). What is
unruled is **the value**: `k_gyro`, `k_gyro_react`, `k_air_shift` are written, commented and shipping
at **zero**. *(Corrected this pass per `critic_completeness.md` A8 and memory `chad-take-the-ruling`;
the source ladder re-asked a taken ruling.)*
**⚠ Precondition attached, not after:** today the kernel can only give the **nose-up** half —
`sim/sled.cpp:1354` is `rep_belt = std::max(dv * v_cmd_b, std::max(v_bf, 0.0));` and **`brake` does not
appear** — so brake structurally **cannot** give nose-down at any dial value, which is the half that
makes a landing worse. **`belt_brake_frac` (identity 0.0, readout-only) comes FIRST.**
*Second half:* the STAND self-right has **no contact gate at all** (≈ 1 244 °/s of free airborne roll
on a held press) — **should it require ground?**

**R7 — `pitch_arrest_nms`: do you want a floor under the flip at all?** Rung 8 is the only rung that
adds a **new kernel term**. The 60° gate is argued out of your own measured catwalk bimodality so the
30° wheelie survives by construction — **but a new torque is the shape a governor takes. Your word
before a line is written.**

**R8 — Your mouse.** CPI and Windows pointer sensitivity. D-E's *"600 raw counts full-scale = ~19 mm
of desk travel"* assumes 800 CPI and is a **GUESS** in that factor. **No lean-scale number can be
picked without it.**

**R9 — Do you want to be able to lean and aim at the same time?** While the Sting launcher is
shouldered the mouse drives `sting_aim_az/el` and **the lean freezes wherever it was**
(`lean_return_tau_s` is **0.0 in all 84 tape headers**, so there is no return) — and you hold
`|lean| > 0.98` for 15–44 % of your ride, so the frozen body is usually a leaned-over one. Three
answers: **(a)** leave it frozen; **(b)** let the lean fall to centre while shouldered — *which is the
app moving the rider without you, i.e. the §0b governor line, and needs your explicit word*; **(c)**
re-bind the aim off the mouse.

**R10 — REPORT, not a question: the gi4 handoff's 4106 N is wrong.** `docs/gi4_ride_handoff.md` §2
computed `(331 + 87.5) × 9.81`, but `sim/sled.h:543` reads `mass_kg = 331.0  // machine + rider,
TOTAL`. **The bytes are on 3 246.0 N — the old document overstates the denominator by 26.5 %.**
Nothing in this audit quotes 4106. **Your word is only needed on whether the old document gets
corrected.**

**R11 — "Self-righting by chance more": with a press, or without one?** Your two sentences side by
side: *"slef righting by chance more"* (§0b) against *"**STAND alone rights the tipped sled**"*
(2026-09-03) and *"IF ON THE SEAT AFTER A ROLLOVER **PRESSING THE STAND BUTTON**"*
(`config/scenario.toml:2053-2056`). Rung 4's C2 term rights the machine **with no press at all**, and
its `wo` weight makes it **strongest on a machine that has stopped moving** — the population furthest
from *"by chance"*. **Rung 4 is hard-gated on this.**

**R12 — REPORT (new, critic gap A9): a comment in the tree still describes the behaviour you ruled
OUT.** `config/scenario.toml:2098-2099` reads *"a full LEAN counts like 10 deg of attitude error — his
lean is what picks the side"* — **exactly what `31e0dd96a` deleted at your correction** *"I dont
understand why yo are clonlating the lean mechanism with the righting."* The shipped field now
multiplies the latched brace. **Do you want the lying comment corrected?** *(Same ruling covers
`right_assist_min_tilt_rad`, a required TOML key `sim/sled.cpp` never reads — a decoy, reported not
removed, because removing it is a `config/` + loader edit this audit forbids.)*

**R13 — REPORT (new, critic gaps C4 and E-C1/E-C2): three signed numbers where the document and the
bytes disagree.** (i) `track_rail_half_m` has **said 0.14 and run 0.19 since 2026-08-12**, and
`ROLL_COMFORT_HANDOFF.md` §1 — **the charter** — still prints 0.14. (ii) The trip band drifted from
your signed **50/60** to a shipped **60/70**. (iii) `right_assist_nm`: the comment in the **same
commit** derives **1500**, the file ships **2400**, and *nobody has ruled it.* **Which of the three do
you want to rule, and which just corrected?**

**R14 — Braking is twice real, on a dial you signed (new, critic gap B4).** MEASURED: braking decel
p90 **7.85 m/s² = 0.80 g on snow** against LITERATURE 0.4–0.5 g, and brake duty is **8.7 %, the highest
of any bucket** (up from 3.8 % at v13g). You signed *"Brakes work on the road pretty good now."*
**Is 0.80 g the feel you want, or an accident?** *(Attached, and it is why the landing-damper refusal
needs re-reading: your 2026-08-29 buck ruling — "**the hardness of the landing casue for letting go**
or the bars and falling off to the side" — **is** a word about landing hardness, and `sim/rider_grip.h`
is a one-way instrument: **the kernel's rider absorbs exactly zero landing energy on any path.**)*

---

## 5. DRIVE CHECKLIST — RUNG 1 ONLY
**NOTHING WAS BUILT THIS PASS.** Rung 1 needs no build: `[snowpack] class_blend_m` is **live-tunable
TOML**. The exe path below is a **PLACEHOLDER** — it is the fly-tree binary as it stood at 23:29 on
2026-09-17 and it has **not** been rebuilt, verified or launched by this audit. **Whoever runs this
drive rebuilds it and replaces the path with the one they actually built.**

**PLACEHOLDER EXE — `D:\flight_sim2\seads-recon\build-play\seads.exe`**
**THE DIAL — `D:\flight_sim2\seads-recon\config\world.toml`, `[snowpack] class_blend_m` (ships 0.0).**
**⚠ ROUTING: this A/B belongs to the lane that owns `world/snowpack.*` (road-repair). This audit hands
it over; it does not run it. And `world/props.cpp` uses an unblended `nearest()`, so under a non-zero
blend the DRAWN corridor edge and the DRIVEN one can diverge — that is road-repair's open item, not
this one's.**

1. **Say the word on last night first (ruling R1).** One line for each of tapes **86, 87, 88, 89, 90,
   91** — "good" / "rolled too much" / "didn't notice". *And: were 86 and 88 free riding or rollover
   tests?* **Without this the drive below measures nothing you can act on.**
2. **Run leg X1 before spending the drive** (free, read-only, one pass over
   `docs/sled_audit/tape_summary.json`): re-attribute each rollover to the surface at episode **exit**.
   **If TrailMain's rate collapses, Rung 1's tape support is gone and this drive is for a different
   reason.** *It has not been run.*
3. **Same trail out of Chelmsford, twice, same line, same speeds (40–90 km/h):**
   run A with `class_blend_m = 0.0` (today), run B with `class_blend_m = 1.0`.
   **The question: does she stop snatching when one ski touches the edge?**
4. **And the other half, which is the whole invariant:** **does the drift still come back when you get
   on the throttle?** If the bush got calmer *too*, this was a stability change, not a boundary fix —
   and it bought "roll resistant" by a route your ruling did not authorise. **Say if the bush felt
   different.**
5. **Then get off and WALK the same corridor edge, both ways, in both runs.** `surf_mix` is read by
   `sim/walker.cpp:98-107` as well — **this dial changes the man on foot at every corridor edge, and
   no sled tape records him.** *(Live beside "feet in asphalt Hwy 144 W" on the same corridor geometry.)*
6. **Ride Rung 5's ask in the same session, at no extra cost:** pin it wide open across the deep bush
   at speed — **does she still get pulled in and flip out, or does she just run out of push?** *(That
   one is `traction_mu` and it needs the six-touch plumbing before it can be A/B'd — the drive line is
   here so the felt baseline is taken on the same day.)*
7. **The tape records itself.** Six new tapes, one line each afterwards. **That is ruling R1 for the
   next round, taken while it is fresh.**

---

## 6. METHOD APPENDIX

### 6-A. What this packet refuses to claim
1. **No rung here has been built, probed or flown.** Every "invariant" is a test to run, not a result.
2. **That any of it is FELT.** RC-5 stands: *"THE FEEL IS SIGNED."* Picking a value is your seat.
3. **That the ranking survives R1.** Largest single risk in the audit, and it is a ruling.
4. **That the rollover rate is a kernel ranking** (confounded four ways, D-B-11).
5. **That §2(b)'s "available by construction" has been OBSERVED.** DERIVED from the table and the
   geometry, corroborated by the 12/12 sign test; **X1 is not run.** Read "on the trail" as "**at** the
   trail". **⚠ And no probe leg takes the measurement `R1-…-REFUTE.md` calls *"the cheapest live
   candidate for Chad's actual complaint that this audit has produced"* — the untripped flat-ground
   friction rollover. X1 is surface attribution, not that. (critic gap C5.)**
6. **That the self-right ever fires in the air.** Its *gate* is measurable and was measured; its
   *torque* is not — `right_assist_nm_now`, `right_shift_cmd`, `right_charge` are absent from the
   tape's pin roster.
7. **That the winter lateral-friction band is a snowmobile number.** A splice of car-tyre lateral and
   snowmobile longitudinal data; the SAE 2015 cornering table is **paywalled and was not obtained**.
8. **Four readings are explicitly NOT used:** R1-10's *"slides before it rolls"* (refuted); R1-19's
   friction circle (there is none); *"29 % land upright = a failed jump"* (93 % of counted air events
   are sub-second hops, hang p50 0.39 s); R3-M8's *"combat and riding are serialized"* (refuted — P
   deploys from the seat).
9. **That the fold's own tape numbers were re-run.** Rung 4's `wv == 0` fractions, Rung 7's armed
   duty and Rung 8's band occupancy were measured by the mechanism red-team, read-only, at 120 Hz
   against a 1 440 Hz kernel. Their **source claims** were re-verified by hand against the shipped
   code; **their tape passes were not re-run, and their probe scripts are not published** (critic gap
   C2 — the same is true of R3's `r3_probe.py`/`marks*.json`/`spd.json` and D-D's three scripts).
   **Only `de_input.py` and `tape_summary.json` exist in the worktree.**
10. **Every rung's ranking is a judgement**, not a measurement.

### 6-B. Every number's killing mutation
*(The claim → what change would make it fail. A claim without one is decoration.)*

| # | claim | conf | killing mutation | run? |
|---|---|---|---|---|
| 1 | 4.32 rollovers/min, 5.74 % inverted (v17) | MEASURED | read `up` as flat +Y instead of `normalize(position)` → 0.0542 becomes **0.9577** | ✔ run |
| 2 | `frac_past90` is orientation-driven | MEASURED | replace orientation with random unit quats → 0.0542 → **0.4935** | ✔ run |
| 3 | the kernel's own `rolled` column is independent | MEASURED | both geometry mutations → **3.62 s, unchanged** | ✔ run |
| 4 | the yaw-vs-steer ladder is pairing, not binning | MEASURED | shuffle the steer column → every cell collapses to flat ≈0.26/≈0.13 | ✔ run |
| 5 | lean-band dwell p-max | MEASURED | drop the EOF flush → tape 86's 12.77 s max becomes 4.77 s **(this was a real bug, found and fixed)** | ✔ run |
| 6 | TrailMain 16.88/min is *on* the trail | MEASURED (rate) / **UNSETTLED** (attribution) | **leg X1** — re-attribute at episode **exit**; if the rate collapses, Rung 1's tape support dies | ✘ **NOT RUN** |
| 7 | SSF 0.5097 g / tip 27.01° | DERIVED | `track_rail_half_m` 0.19 → 0.14 (the comment's value) ⇒ **0.452 g / 24.3°** | arithmetic |
| 8 | six of seven surfaces exceed the tip threshold | MEASURED+DERIVED | any row below 0.5097, or the rollover proving normal-load-starved (**probe leg L1**) | ✘ not run |
| 9 | inside-ski lift at 0.362 g, φ = 0.856 | DERIVED | `susp_k` differing front/rear, or `track_rail_half_m = 0` ⇒ φ → 1.0, lift at 0.310 g | arithmetic |
| 10 | landing spike 1.01×…5.87× of 933.1 N·m | DERIVED on a **GUESSED** slip | `bite_at_contact_frac = 0.0`; or touchdown slip < ~0.05 rad ⇒ whole table → ~1× (**probe leg D-1**) | ✘ not run |
| 11 | `surf_mix` is identically zero on main | MEASURED | `class_blend_m` non-zero in `config/world.toml` | ✔ read |
| 12 | blend 0.0 → −482.7° yaw ROLLED vs 1.0 → −11.6° | MEASURED (tree's own probe) | `blend_dials` never executing under a rename/dead `c.found` ⇒ the dial is cosmetic | ✔ on file |
| 13 | `wv == 0` on 75.0 % of the stall window | MEASURED (red-team, **unpublished script**) | `side_right_vmin_ms = 0.0` would open `wv` at rest — it ships **1.5 in header AND TOML**, verified | ✘ not re-run |
| 14 | 71.2 % never recover, flat across 4 buckets | MEASURED | widen the recovery predicate (tilt < 45°, drop the 0.5 s hold) — the **trend under one predicate** carries the claim, not its level | ✔ run |
| 15 | 16 episodes / **5** right-censored | MEASURED (refuter's re-derivation) | the original 15/4 — **refuted, offset exactly +1 in all eight sweep rows** | ✔ run |
| 16 | six rightings, steer 1.0000 in 4/6, throttle 1.00 in 6/6 | MEASURED (raw byte stream) | **leg X3** — `in.steer` flat at ±1.0 after the `O` = a **held key** ⇒ Rung 3 is decoration | ✘ **NOT RUN** |
| 17 | catwalk p99 66.75°, band 156 vs 209, nose-down 3 010 | MEASURED (red-team, **unpublished**) | drop the `air_s == 0` clause and p95 jumps as airborne backflips contaminate the sample | ✘ not re-run |
| 18 | assist non-zero 79.8–93.7 %, p95 765 N·m = 82 % | MEASURED (red-team, **unpublished**) | quote it on a different arm: 765 N·m is 82 % of 933.1, **51 % of 1 504.5**, 40 % of the unsourced 1 931.8 — only the first is this document's denominator | ✘ not re-run |
| 19 | self-right armed duty 2.32 % (not 25.6 %) | MEASURED (red-team, **unpublished**), an **upper bound** (`hands_on` unpinned) | the raw STAND duty — **which is what the first reading used, and it overstated by ≈ 11×** | ✔ corrected |
| 20 | ~5 % of the world's land at the gate's full-authority edge | DERIVED on a MEASURED slope p95 21.1° | a slope distribution whose p95 sits below the 20.05° full-authority angle | ✔ on file |
| 21 | 1 244 °/s of free airborne roll on a held press | DERIVED from code structure only | **no tape can be asked** — `right_assist_nm_now` is absent from the pin roster; probe-only | ✘ not run |
| 22 | braking p90 0.80 g vs literature 0.4–0.5 | MEASURED / LITERATURE | a snowmobile braking source measuring ≥ 0.8 g on snow — **not obtained** | ✘ not obtained |
| 23 | `lean_px_full` correlates with rollovers | **NOT SIGNIFICANT** (n = 9, r = +0.815, partial +0.693 vs 0.707 critical) | raise `px_full` 300 → 600; if rollovers/min do not follow band entries, it was ride intensity | ✘ not run |
| 24 | five `torque_body` write sites; X = pitch | MEASURED (grep + `sim/sled.h:772`) | a sixth write site, or axis labels reading otherwise — **the old Rung 8 spec was on ω_y and was a YAW damper** | ✔ verified |
| 25 | `traction_mu` μ ≥ 0.6 kills the runaway, μ ≥ 3.0 byte-identical traverse | MEASURED (`snowform_measurements.md` §M8.1) | `traction_mu = 0.0` ⇒ the branch at `sled.cpp:1219` is dead, bit-identical | ✔ on file |
| 26 | the scoreboard in §1.2 | MEASURED rows / **UNMUTATED verdict** | **it has no killing mutation and cannot have an arithmetic one** — the bucket rates behind it are confounded four ways (D-B-11), so **the verdict's only killer is ruling R1** | **ruling** |

### 6-C. What the tapes structurally cannot settle
- **RC-1** (*"a plain full-lock turn at trail speed with hands-off weight must NOT roll"*) — **you
  never drive hands-off**, so 206 minutes of tape contain no such trial. Probe-only.
- **Causation in the steer inversion** — the 0.9–1.0 cells are **4–8× oversampled** because you reach
  for full lock *when the machine is already not turning*. Only a probe establishes direction.
- **Grip release / superman counts** — the kernel runs the grip law at **1 440 Hz** and the tape pins
  at **120 Hz**; the tool reports `grip_lower_bound` and **refuses** to convert it to a release count.
- **Tremor / knife-edge ringing** — 12 substeps at 120 Hz, pinned once per tick, so anything above
  60 Hz is aliased. **No spectral claim is made from `a_body`** beyond amplitude percentiles.
- **Where `|a_body|` maxima come from.** D-B prints per-tape maxima of **1 766** (tape 88) and **2 561**
  (tape 16) and calls the per-tape max the only trustworthy figure up there; **D-D says the 1 766 is a
  one-tick numerical artefact at a tape discontinuity** (respawn / `O` record) and that any future
  g-driven cue needs the `epoch` guard `render/trail_chain.h` already uses. **This packet adjudicates
  neither and uses neither number for anything** (critic gap C10).

### 6-D. Two bugs found in this audit's own new code, before anything was published
1. **Band runs still open at EOF were never flushed** — tape 86's longest held lean (12.77 s, the
   tape's maximum) was dropped and the metric reported 4.77 s: **a 2.7× understatement of exactly the
   quantity the metric exists to measure.**
2. **`band_dwell` shipped `out(bins=False)`** — with the bin array absent the pooled percentile
   silently returned the last bin edge (59.995 s) instead of failing.
**Same failure class both times: a metric that is wrong but plausible-looking is worse than one that
crashes.** Independent recompute caught them, **not review.**

### 6-E. Legs owed before any of this is built — **NONE HAS BEEN RUN**
**Free, read-only, no build (all three unimplemented in `tools/sled_tape_audit.py`; `grep -n
"exit_surface|rolled_edge|in_steer"` returns nothing):**
- **X1** — the exit-surface attribution. **Can kill rank-1 Rung 1 before a line is written.** *Attach
  to Rung 1.*
- **X3** — the `in.steer` decay signature over the 1.0 s after each of the six `O` records. **Rung 3's
  stated PRECONDITION; the synthesis says Rung 3's rank does not stand until it runs.** *Attach to R3.*
- **X2** — the `rolled` edge counter over the 84 tapes. **Epistemics that must travel with it: the
  tape pins at 120 Hz while the kernel runs the test at 1 440 Hz, so a null does NOT clear the
  mechanism — it only fails to find it.**
- **⚠ And a fourth, specified and unscheduled (critic gap B2):** the backflip/rollover separator
  (`air_s > 0` at entry ∧ pitch-dominant). **Ruling R4 hard-gates Rung 4 on a definition no metric in
  the corpus can apply.**
- **⚠ And a fifth (critic gap B3):** the pooled override classes are `{episode_start: 84,
  autoright_R: 146, mount_or_other: 9}` — **the 9 `mount_or_other` records are uninspected**, and
  `R4-landings.refute.md` states plainly that R4-04's killing mutation is **false as written** until
  they are.

**Probe legs (`tools/sled_probe.cpp`, built in `D:/seads_sandboxes/sled-audit/build` only):**
**L1** hands-off full lock on TrailMain at v ∈ {8, 12, 16, 20, 25} — RC-1's only possible trial ·
**L2** the drift, before and after — RC-3's explicit demand, *"a probe leg, not a hope"* ·
**L3** the SF1 side-slope at a fixed line, lean uphill vs none — the charter's own leg, and it was
missing; **Rung 7 removes hill-time righting authority and nothing else can see that** ·
**L6** the lip trace — pitch, ω_x, `normal_sum`, `w_contact` over the **0.5 s of contact before the
St. Charles takeoff**; **Rung 8's PRECONDITION** ·
**D-1** pack absorption during a real impact — **the largest single unmeasured number in the audit** ·
the **steer-inversion causation** sweep · the **clean assist A/B** (`right_assist_nm` 0.0 vs 2400.0
through `seads_sled_probe`; **no dial may move on the confounded before/after alone**).
**⚠ RC-5 landing condition:** the §6-E **whole envelope** — L1, L2, **L3**, D-1 — is a before/after on
**every** rung, not on one. **A rung that moves a leg it never named has not landed.**

### 6-F. Provenance and standing
- Source reads were taken in this worktree at HEAD `ed0a43ce8`. **⚠ The worktree's local `main` ref is
  stale (`de523afd1`) against `origin/main` (`533c86409`)**, and D-C's verifications were run against
  `origin/main`. **The two were not reconciled** (critic gap G3).
- **The gate's own reds say this out loud and no ladder mentions it** (critic gap C6):
  `generated/gate/known_reds.txt` carries **`sled_slides_before_it_tips_on_flat_snow`** — a baseline
  red for **five weeks**, whose *name* asserts the proposition `R1-10`'s refutation overturned — beside
  `sled_grip_ceiling_stays_below_the_tip_threshold` and
  `sled_assist_reference_plane_is_load_weighted`. **Four of main's six baseline gate reds are sled
  debts by name.**
- **The "drop any rung two judges scored under 5 on intent_heard" rule was never applicable** — no
  judge scored **rungs**; all three scored **ladders**. The per-rung instrument the rule needs does not
  exist (critic gap G1).
- **Judge 2 dissented in writing** on taking PJ-7 as method rather than a rung (*"PJ-7 **whole**"*,
  because the flag owns **both** the throttle and the drone launch — i.e. the battle). The tally was
  recorded; the dissent is recorded here beside it (critic gap G2).

### 6-G. THE COMPLETENESS CRITIC'S GAPS — VERBATIM
*(Reproduced exactly as returned. **Nothing in it was fixed**; this packet carries some of it into §2,
§3, §4 and §6 and leaves the rest standing. The full 28 KB file is
`D:/seads_sandboxes/sled-audit/docs/sled_audit/critic_completeness.md` and it is the authority, not
this excerpt.)*

> REPORT: `D:/seads_sandboxes/sled-audit/docs/sled_audit/critic_completeness.md` (written, exists)
>
> COMPLETENESS CRITIC — 38 gaps, nothing fixed. Scope read: D-A, D-B, D-C, D-D, D-E, R1/R2/R3/R4 + all four refutations, 3 ladders, 3 judges, 2 red-teams, `ladder_synthesis.md`, plus `tools/sled_tape_audit.py` and `generated/gate/known_reds.txt`.
>
> **A. CHAD WORDS UNADDRESSED (no rung, no ruling, no "not this audit" line)**
> - A1 ★ "EVERYTHING IS GOOD ON THE SNOWMACHINE DRIVE TEST" (2026-08-21) — D-C calls it the only unqualified sled sign after drive 4; the §0 scoreboard grades only the 2026-08-12 bar. grep = 0.
> - A2 "Corkscrew is alive and well" + the RC1 bar "360-spin a road WITHOUT rolling" / "spins like a top ~10x". grep "corkscrew"/"360" = 0.
> - A3 "WOT should lift the skis to ~30 deg" — appears only as an aside in two law checks.
> - A4 "should be able to launch in the air" — the wrong reading is refused (§6-8); the ask is left unanswered.
> - A5 "The conditions should be palpable and observable in the sled performance" — quoted in §0, cited by no rung.
> - A6 §3 refuses the landing rung as "not a rung until there is a word" — the 2026-08-29 buck ruling *is* a word about landing hardness ("the hardness of the landing casue for letting go").
> - A7 "more places with a bit less snow makes the sled go faster" (O13) — absent.
> - A8 Ruling R6 re-asks a taken ruling ("I think we should build it", gyro, 2026-08-27); what is unruled is the value.
> - A9 `config/scenario.toml:2098-2099` still says "his lean is what picks the side" — the behaviour `31e0dd96a` deleted at his correction. Unreported.
>
> **B. TAPE METRICS NEVER COMPUTED**
> - B1 ★ X1, X2, X3 — all three "free, read-only, one pass" legs are unimplemented in `tools/sled_tape_audit.py`. X1 can delete rank-1 Rung 1; X3 is a stated precondition of rank-3 Rung 3 and the synthesis says that rank "does not stand until X3 has run".
> - B2 D-A §8-1's backflip/rollover separator (`air_s>0` at entry ∧ pitch-dominant) — ruling R4 hard-gates Rung 4 but no leg can ever apply his answer.
> - B3 `mount_or_other: 9` override records, uninspected (R4-refute FIX 1 says R4-04's mutation is false as written); Rung 3 is built on `O` records.
> - B4 D-B-8 braking 0.80 g vs literature 0.4–0.5 g, brake duty 8.7% (highest bucket) on a **signed** dial — "flagged for R1", reaches no ruling.
> - B5 Exit-surface rates + the surface-controlled cross-tag comparison (D-B-11's "right instrument") — never scheduled.
> - B6 D-E-05's own mutation (px_full 300→600). B7 D-D UNVERIFIED-3 (`rd.speed` while mounted).
>
> **C. REFUTATIONS IGNORED**
> - C1 ★ R3-REFUTE F3: 16 scorable / 5 censored, "+1 in all eight rows". Rung 4 still prints "4 of 15 right-censored".
> - C2 ★ R3-REFUTE F1 "publish the probe" not done — and now the fold's own numbers (Rung 4 `wv==0`, Rung 7 2.32%, Rung 8 156/209/3010) are equally unpublished; §6-9 admits they were not re-run. Only `de_input.py` + `tape_summary.json` exist.
> - C3 F5: R3-M3 is the arithmetic complement of R3-M1; Rung 4 cites both as independent.
> - C4 R1-26 (`track_rail_half_m` documented 0.14, running 0.19 since 2026-08-12; the **charter** still prints 0.14) used as a mutation, never filed as a report — same class as R10, which *was* filed.
> - C5 R1-REFUTE's escalation (untripped friction rollover, "the cheapest live candidate") has no probe leg; X1 is not that measurement.
> - C6 `known_reds.txt` carries `sled_slides_before_it_tips_on_flat_snow` + 3 more sled reds, 5 weeks red. grep = 0.
> - C7 R2-REFUTE's corrected A/B (severity collapsed 53.60 s → 5.09 s, 4.4x rolled share; frequency unmoved; override delta 2.3x not 4.2x) — absent from the §0 scoreboard and from §6.
> - C8 R2-REFUTE §8-2 "ask what he was doing in 86-91" — R1 asks for a felt word, not whether 86/88 were rollover tests.
> - C9 R4-refute's "no claim covers `facet_contact`" — tape 91 is a different build, quoted in 5 of 8 rungs. grep = 0.
> - C10 D-D-F5 (1766 m/s² is a

*(The returned excerpt ends mid-sentence at C10; **sections D, E, F and G — whole vanished rungs and
strands, the eleven unaddressed D-C debts, the four unmutated claim classes, and the four process
findings — are in the file and are NOT reproduced here.** They are summarised, with attribution, in
§3's "vanished" list, §4's R12–R14 and §6-F. The file is the authority.)*

---

*Sled ride audit packet, 2026-09-18. Read-only against main. No dial changed, no config edited, no
golden moved, nothing built, no ctest run, no `seads.exe` launched, nothing pushed to main. Written
files this pass: `docs/SLED_RIDE_AUDIT_20260918.md` and the `[lanes.sled-audit]` section of
`LANES.toml`.*
