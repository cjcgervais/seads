# MERGED LADDER — SYNTHESIS

**Base angle: FEEL-FIRST.** All three judges scored it best (9.25 / 63 / 64). Its organising
sentence is Chad's own — *"the measure of it shall be if my intent is heard"* (§0b) — and every
rung is **one dial that changes what the hand feels**. Grafted onto it: the derived mechanism
spine and two rungs from PHYSICS-HONEST, one rung and one measurement from PLAYER-JOURNEY.

> **⚠ REVISION 2 — RED-TEAM FOLD APPLIED, 2026-09-18.** Two adversarial passes
> (`redteam_law_feel.md`, LAW-FEEL lens; `redteam_mechanism.md`, MECHANISM lens) both returned
> **SOLID_WITH_FIXES / LAND-WITH-FIX**. **All 5 P0 and all 9 P1 findings are folded into the text
> below, in place, each marked `⚠ FOLDED (RT-A …)` or `⚠ FOLDED (RT-B …)`; §7 is the FOLD LOG.**
> Two rungs were **DO-NOT-LAND as written** and are now corrected, not dropped: **Rung 4's dial
> changed** (`side_right_gain_nm` → `side_right_vmin_ms`) and **Rung 8's axis was wrong** (ω_y is
> **yaw** in this kernel, not pitch). **Every source claim the fold rests on was re-verified by hand
> against the shipped tree this pass.** No rung was dropped outright; **two now carry a conditional
> death** (Rung 3 on leg X3, Rung 8 on leg L6) and one carries a conditional drop (Rung 6, if a
> physics-inert `sim/` field is refused for a readout).

**Worktree** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
**READ-ONLY against main.** Nothing built, no dial changed, no config edited, no test run, no
probe, no `seads.exe`, nothing pushed. The only file this pass wrote is this one.

**Confidence vocabulary.** MEASURED / DERIVED / LITERATURE / GUESS (labelled every time).
**Every numeric claim carries its killing mutation.** No feel recommendation rests on a harness
number alone: each cites a tape event **and** one of Chad's words, verbatim, with doc + section.

**Drop rule applied.** "Drop any rung two judges scored under 5 on intent_heard." No ladder was
scored under 5 on intent_heard by any judge (feel-first 10 / 9 / 9.5; physics-honest 7 / 7 / 7;
player-journey 8 / 8 / 8), so **no rung was dropped by that rule**. Four rungs were dropped for
the 8-rung budget; §2 names each and why.

---

## 0. THE CHARTER, VERBATIM — the bar every rung is scored against

`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0** (drive 4, 2026-08-12 — the
acceptance bar):

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll resistant.**
> Let me slide around a bit arcade but **allow me to land on my skis more often after a roll**
> (even though R works). But not every time — allow it to happen. Also I've seen lots of
> snowmobiles **drift around corners and then with throttle, straighten out. The feel is really
> good. Don't lose the feel**, except the constant rolling. **Make it possible to roll but not
> the rule.** However the game feels like a nicely made sim as there is dynamic room and **the
> balance mechanism works really good. Just allow the balance of body mechanism to ENHANCE
> ability, i.e. tighten a turn instead of having to be the necessary condition of not rolling
> over.** Just allow leaning a certain way in a particular condition to be the OPTIMAL weight
> distro for better traversing, say up a hill or around a corner. **Work on the math to achieve
> a balance of fun and accuracy to real physics, just as our airplane ontological counterpart
> does.**"

Same document **§0b** (SUPERSEDING, 2026-08-12 night):

> "no I dont like thge idea in the handoff at all! It will ruin the feel to have a governor.
> Make it less honest but dont ruin it. Get a fable consult and **the measure of it shall be if
> my intent is heard**. Leaning shall enhance the ride an just make it more stable and **slef
> righting by chance more**. It should be arcadey to a degree so that it is fun., **SLiding
> banging, punchy, jumps.** Just make it more stable."

> "please build autmatically and make this kernel way better ... **Allow for bad driving too but
> keep the benefit there for good riding. DOnt make it impossibly hard, there is a batttle going
> on as well.** And the point is this should be fun as well, not a true sim. But **I do want the
> finess and all the mechanisms available for tuning** ... keeping the good but making it easier
> / more fun?"

Felt report, `docs/gi4_ride_handoff.md` §1 (his words, left column):
1 *"tips over too easy"* · 2 *"should be able to launch in the air"* · 3 *"turning too unstable,
can't hold a carve"* · 4 *"WOT should lift the skis to ~30°"* · 5 *"rider weight needs authority,
not thrown into a spin"* · 6 *"flips happen too fast and easy"*

Also on file, verbatim: *"If I lean back and hit a snowbank, I flip multiple times end over
end... like 5x"* and *"I DO want a wheelie ... even MORE so"* (2026-08-13, `D-C` §1 / O3); *"at
high speeds, getting pulled in and flipping out like 15x is not desirable so yes take care of
that"* (2026-08-24, quoted into `sim/sled.h` at `traction_mu`); *"I dont like the steering via
mouse coupled with lean they have to be two independent things, **part of what is so fun on the
road is driving by lean**"* (SK-1d, 2026-08-25); *"I need a key for now that lets me autoright
until we get the guy running back to the snowmachine"* (`app/main.cpp:8279-8286`); *"MACHINE
NEEDS TO BE TIPPING OVER … NOT ABOVE 5KM/H … IT CAN FAIL TO RIGHT GIVEN THE SITUATION,
PROGRESSIVE HOLD"* (2026-08-26, `config/scenario.toml [sled_comfort]`); *"The conditions should
be **palpable and observable in the sled performance**"* (`docs/roost_consult_packet.md` §1).

**Where the machine stands against that bar, tonight (MEASURED, D-B §7 / PJ read A):** roll
resistant — **no** (4.32 past-90 rollovers/min, one every 13.9 s). Possible to roll but not the
rule — **no**. Land on my skis more often — **no movement** (29.3 % upright vs 27.6 % at v13g).
Self-righting by chance more — **no** (71.2 % never recover). Sliding/banging/punchy/jumps —
**yes**. Don't lose the feel — **yes**. Not impossibly hard — **no**. `sled_tape_91` **ends
upside down**: last tick tilt 131.4°, speed 0.41 m/s, throttle 0, clean `# sig` exit.

---

## 1. THE MECHANISM SPINE, DERIVED — why she rolls
*(GRAFT G3, unanimous: physics-honest §1(a)–(f). It adds no rung; it gives Rungs 1, 5, 6 and 8
a why, and gives Chad a number to rule against. All recomputed that session, not quoted.)*

**(a) What it takes to tip her.** Support polygon is a **trapezoid** — two skis wide and forward,
one track narrow and aft — so the tipping line is the diagonal from downhill ski to downhill
rail. With `stance_m 0.927`, `ski_fwd_m 0.86`, `track_aft_m 0.52`, `track_rail_half_m 0.19`,
`cg_height_m 0.564` (`sim/sled.h:543-569`): lateral arm **0.28747 m**, **SSF 0.5097 g**, static
tip **27.01°**. DERIVED, exact arithmetic. *Killing mutation:* move `cg_height_m` or
`track_rail_half_m` — at the comment's real-Indy rail half of 0.14 m it falls to 0.452 g / 24.3°.

**(b) What the kernel's snow is allowed to push sideways.** `sim/sled.cpp:1056-1059` computes
lateral as `-normal * mu_l * tanh(slip_ang / slip_ref_rad)`. The shipped ski table at
`sim/sled.cpp:162-193` (verified by two judges independently this round): TrailTributary **0.72**,
TrailMain **0.70**, Road **0.60**, RockOutcrop **0.60**, MineWorks **0.58**, Bush **0.55**,
LakeIce 0.22; track lateral is a constant **0.70** (`sled.h:1193`). **Six of seven surfaces ship a
lateral coefficient above the machine's own 0.5097 g tip threshold** — an untripped, flat-ground
friction rollover is available **by construction**. MEASURED (table) + DERIVED (comparison).
*Killing mutation:* any of those rows reading below 0.5097, or the rollover proving
normal-load-starved in practice (probe leg L1, §5).
⚠ **FOLDED (RT-B P2-13) — the table mu is only DELIVERED at slip ≳ 0.3 rad.** The `tanh(slip_ang /
slip_ref_rad)` factor with `slip_ref_rad = 0.300` (`sim/sled.h:1187`) means Bush at 0.10 rad of slip
delivers **0.177 g**, not 0.55 — so the friction rollover is available **in a big slide, not in a
carve**. The headline stands for the sliding regime he signed and must not be quoted at a carve.
Pointing the other way, and omitted from the original spine: `bite *= 1 + lean_bite_gain·align_m`
(`sim/sled.cpp:1067`, gain 0.18) takes Bush to **0.649 g** at full aligned lean — **leaning INTO a
turn moves the machine toward its own tip threshold**, which is itself part of felt item 1.
⚠ This **inverts** R1's own headline ("she slides before she rolls"); `R1-real-dynamics-REFUTE.md`
§1 refuted it against this table and both this ladder and physics-honest adopt the refutation.

**(c) The inside ski lifts long before the tip, and that is correct.** Front roll stiffness
`2·susp_k·(stance/2)²` = 19 764.6 N·m/rad; rear (rail split) `2·susp_k·rail_half²` =
3 321.2 N·m/rad ⇒ **φ = 0.856** of roll reacted at the front; inside-ski lift at **0.362 g**.
DERIVED. She lifts a ski at 0.36 g and the surface can still supply 0.55–0.72, so she keeps going
to 0.51 g and over. On a real machine the lift is where the story ends because the snow runs out
first. *Killing mutation:* `susp_k` differing front/rear, or `track_rail_half_m = 0` (φ → 1.0,
lift at 0.310 g).

**(d) The landing spike multiplies (b).** `sim/sled.cpp:705`: `normal = susp_k*x + susp_c*xdot`,
**linear, unbounded**, `axis_dot` floor 0.25 giving up to 4× the CG's own sink rate off-angle; the
tangential bite is applied at the contact point (`bite_at_contact_frac 1.0`, `sled.cpp:988-993`),
exactly `cg_height_m` below the CG. A **4 m/s** arrival ⇒ 14 400 N damper normal ⇒ **10 080 N of lateral
capacity at μ 0.70**, of which `tanh` delivers 7 677 N **only if the touchdown slip angle is
0.300 rad**. ⚠ **FOLDED (RT-B P1-12): that slip assumption was silent and it is a GUESS.** The chain
is linear in `tanh(slip/0.300)`, so state the table, not one row (DERIVED arithmetic on a GUESSED
slip):

| touchdown slip | lateral force | roll torque | × gravity's tipping torque (933.1 N·m) |
|---|---|---|---|
| 0.05 rad | 1 665 N | 939 N·m | **1.01×** |
| 0.10 rad | 3 241 N | 1 828 N·m | **1.96×** |
| 0.20 rad | 5 874 N | 3 313 N·m | **3.55×** |
| 0.30 rad | 7 677 N | 4 330 N·m | **4.64×** |
| 0.60 rad | 9 717 N | 5 481 N·m | **5.87×** |

⚠ **FOLDED (RT-B P1-11): the denominator is fixed at 933.1 N·m** — `W × lateral arm` =
3 246.0 × 0.287465, which is what §1(a) actually derives. The document previously carried three
different roll-torque denominators (933.1, 1 505 = W × half-stance, and an unsourced 1 931.8). **Only
933.1 is used anywhere below.** The old "2.24× (1931.8 N·m)" headline was both the wrong denominator
**and** an unstated slip assumption; against the right one the 0.30 rad row is **4.64×**.
*Killing mutation:* `bite_at_contact_frac = 0.0` drops that row by the arm ratio; or a touchdown
slip angle systematically below ~0.05 rad, which makes `tanh` kill the term and drops the whole
table to ~1× — **a probe question, not a tape question** (leg D-1, §5, and the table above is what
D-1 is now required to land on).

**(e) One patch, two unrelated friction laws.** Longitudinal is Mohr-Coulomb on the snow
(`sled.cpp:1167-1174`); lateral is Coulomb on a constant (`sled.cpp:1056`). **There is no friction
circle in this kernel** — `grep` finds only the per-surface brake budget (`mu_brake`, 1232-1246),
which caps longitudinal braking and never charges lateral. DERIVED. This is why Rung 7 makes the
drift **audible/measured** rather than trying to tune it: whatever produces the drift Chad signed,
it is not a circle.

**(f) The denominator, once, so no rung repeats the error.** The machine weighs **3246.0 N**
(331 × 9.80665; `rider_mass_kg` is split OUT of `mass_kg` — `sim/sled.h:543`, `:784-785`, and
`grep -n "p.mass_kg" sim/sled.cpp` returns one line, 224). ⚠ **FOLDED (RT-B P2-18): the honest
claim is "`rider_mass_kg` never enters the machine's WEIGHT", not "never enters a force"** — it
does enter forces, at `sim/sled.cpp:459/464`, as the grip's reduced mass. And **the roll-torque
denominator is 933.1 N·m** (§1(d)); the half-stance figure 1 504.5 N·m is a different arm and
`sim/sled.h:546` says it is **not** the rollover's.
**RULING R10 (new, folded from RT-A P2-2).** Charter §2 says *a finding against a signed number is
reported to Chad, never re-derived*. This is one: every *"of its 4106 N"* in
`docs/gi4_ride_handoff.md` §2 used `(331 + 87.5) × 9.81`, but `sim/sled.h:543` reads
`mass_kg = 331.0  // machine + rider, TOTAL`. **The bytes are on 3246.0 N; the gi4 handoff's 4106 N
is wrong and overstates the denominator by 26.5 %.** Reported, not corrected in place. **No rung
below inherits the old number, and no number is quoted to him on an unshown denominator.**

---

## 2. THE LADDER

Fixed shape per rung: **felt problem in Chad's words → tape evidence → mechanism (file:line) →
THE ONE DIAL with identity value → measurable invariant + killing mutation → one drive-checklist
line → law check.**

Identity value = the number at which the build is **bit-identical to today**. Every rung is a
branch or a multiply-by-zero, never a 0-weight lerp — the `kernel-v14-leanlead` precedent.

**Where a dial is new or compile-time**, the rung carries the RC-8 plumbing. ⚠ **FOLDED (RT-B
P1-7): "one loader line" was wrong and `[sled_input]` DOES NOT EXIST.** Verified this session:
`grep -rn "sled_input" config/ app/` returns **nothing**; `config/load_scenario.h:46` carries only
`sim::SledComfort sled_comfort{}`, and `app/main.cpp:2481` (`sled_params.comfort =
scen.sled_comfort`) is the **single** bridge from TOML to `SledParams`. A field that lives directly
on `SledParams` (Rungs 2 and 5) has **no** loader path at all. The real, per-rung file list for any
new dial is **six** touches:

1. `sim/sled.h` — the struct field (on `SledComfort`, or a **new** bridge for a `SledParams` field),
2. `sim/sled.cpp` — the read site,
3. `config/load_scenario.cpp` — the `require(...)`,
4. `config/load_scenario.cpp` — the range `check(...)`,
5. `config/scenario.toml` — the key,
6. `test/unit/test_load_scenario.cpp` — its own CHECK (that TEST_CASE covers only 22 of 35 loaded
   keys today and all three config-vs-default drifts hide in the 13 it skips, D-A §4).

**Rungs 2 and 5 are therefore `sim/` edits, not config edits**, and each must say so in its own law
check. Any rung that wants a table that does not exist must create it **deliberately**, as its own
named act, not as a footnote.

---

### RUNG 1 — THE SEAM THAT SPINS YOU
*(feel-first Rung 1, with player-journey PJ-2's two-sided invariant grafted and D-B-6's
exit-surface leg made its first act — graft G5/consensus.)*

**Felt problem, his words.** *"she's too unsteady"* … *"Don't lose the feel, **except the constant
rolling**"* … *"**Make it possible to roll but not the rule**"* (§0). `docs/snowform_measurements.md`
§M8.3 names his own reported mechanism: the Road bank-strike superspin.

**Tape evidence.** MEASURED, D-B-6 / PJ read C: **16.88 past-90 rollovers per minute on TrailMain
against 4.13 on Bush and 0.85 on LakeIce**, pooled over 206.1 min. Per-tape sign test: of the 12
tapes carrying >20 s of both TrailMain and Bush, **TrailMain's rate exceeds Bush's in 12 of 12**
(p ≈ 2⁻¹² ≈ 0.00024) across four kernel buckets and four weeks. `sled_tape_91`: **7 rollovers in
27 s of trail (15.6/min) against 11 in 106 s of bush (6.2/min)**, and it ends at 131.4° of tilt.
Control the other way: `sled_tape_90`, **90 s on lake ice with zero rollovers**, all 7 of its
rollovers in its 19 s of bush. *A groomed trail is where a snowmobile should be most planted; his
is where it rolls 4.1× hardest.* Read §1(b)–(c) with it: the class step lands a load transfer on a
machine already past its 0.362 g lift threshold, not on a coincidence at a boundary.

**Mechanism.** `world/snowpack.cpp:700-746` ramps `surf_mix` across the corridor edge only
`if (p.class_blend_m > 0.0 …)`. `sim/sled.cpp:630-636` consumes it as an explicit branch, so
`surf_mix == 0` takes the original reference; `config/world.toml:645` is `class_blend_m = 0.0`
(verified this session), therefore **`surf_mix` is identically zero on main and `blend_dials`
(`sim/sled.cpp:200-213`) is never called** (MEASURED, D-A §5.7). One ski crossing the corridor edge
steps `rho_eff` from 0 to 260 **under that ski alone** — a pure yaw+roll couple — in one tick.
The tree's own probe record (`config/world.toml:640-644`): **blend 0.0 → yaw −482.7°, roll 166.6°,
ROLLED; blend 1.0 → yaw −11.6°, roll 52.7°, no rollover**, 6/10 m/s cells unchanged.
`snowform_measurements.md` §M9.1: a **25 cm** lateral move, straight, throttle open, **no steer
input**, takes her from roll 2.4° to **89° at 16 m/s** and **1.34 rotations at 24 m/s**.

**THE ONE DIAL.** `[snowpack] class_blend_m` — **identity 0.0** (shipped, `config/world.toml:645`).
A branch, bit-identical. Live-tunable today, no recompile.

**Invariant (PJ-2's shape, grafted).** TrailMain's and Road's past-90 rate per minute must **fall**
**AND Bush's must not fall by more than the trail's does**. If both fall together it is a stability
change, not a boundary fix, and it bought "roll resistant" by a route his ruling did not authorise.
Probe invariant, already written: `bankgraze` at 16 and 24 m/s must not roll, **and** the 6/10 m/s
cells must stay unchanged.

**Killing mutation.** **Run D-B-6's exit-surface attribution first** — attribute each rollover to
the surface at *exit* rather than entry. It is named in all three ladders, **run by none**, it is
read-only, and it costs one pass over `tape_summary.json`. If TrailMain's rate collapses under
re-attribution he was *crossing* the trail, not rolling on it, and the tape support for this rung
goes. Until it is run, read "on the trail" as **"at the trail"** — which is the reading this rung
acts on, so it survives in direction either way. Second: `class_blend_m = 1.0` with `blend_dials`
still never executing (a rename, a dead `c.found`) makes the dial cosmetic. Third: the bankgraze
numbers are probe, not tape — if the shipped trail's class step is smaller than the road's, the
probe overstates the prize.

**Drive-checklist line.** *"Same trail out of Chelmsford twice, `class_blend_m` 0.0 then 1.0 at
40–90 km/h — does she stop snatching when one ski touches the edge, and does the drift still come
back when you get on the throttle?"* **Second line, folded (RT-A P1-2): *"then get off and WALK the
same corridor edge both ways — do your feet feel any different?"*** — the walker is the second
consumer and no tape records him.

**Law check.** Depth 0.77 **UNTOUCHED** — `g.depth_m` is computed at `world/snowpack.cpp:702`
*before* the blend block and is neither read nor written by it; `base_m = 0.85`
(`config/world.toml:554`, the parameter that yields his signed p50 0.77 m) is not in scope and must
not be the dial. No governor — nothing reads or moves the rider's mass. Wheelie kept — no pitch
term. STAND/LEAN untouched. Ragdoll banned — this *reduces* uncommanded rotation (−482.7° → −11.6°
yaw, MEASURED).
⚠ **One surface — FOLDED (RT-A P1-2): the blast radius is TWO consumers, not one.** `surf_mix` is
also read by **`sim/walker.cpp:98-107`** (`walker_mu`, verified this session: it blends `mu_of(surf)`
toward `mu_of(surf_b)` by `surf_mix`), so `class_blend_m 0.0 → 1.0` **changes the man on foot at
every corridor edge**, and **no sled tape records the walker**. It still adds no surface class — it
removes a discontinuity between two that already exist, for both bodies, exactly as
`sim/sled.cpp:200-213` and that walker comment were written to do — but the rung must be graded on
foot as well as in the seat, and it is live while *"feet in asphalt Hwy 144 W"* is open on the same
corridor geometry.
⚠ **Honest limit, folded (RT-B P2-15): a SINKABLE dial still steps at the edge at any
`class_blend_m`.** `blend_dials` (`sim/sled.cpp:200-213`) takes the **dominant class** for the
non-blendable dials, and `surf_mix` ramps only to **0.5** (`world/snowpack.cpp:731,743`) — it never
completes a handover. This rung softens the μ/ρ step; it does not delete every step.

**Risk, named (folded, RT-A P1-2).** `world/props.cpp` uses an **unblended `nearest()`** — the lane
that owns it records this as its own open item — so the DRAWN corridor edge and the DRIVEN one can
diverge under a non-zero blend. That divergence is not this rung's to fix and must be reported, not
absorbed.

**Standing.** `LANES.toml:939`: *"⚠ THE STICK RULING: [snowpack] class_blend_m STAYS 0.0 — it is
live-tunable and it is **CHAD'S A/B, not the lane's call.** The lane measured 1.0 as the value it
would pick; it ships 0.0 and he decides in the seat."* **This rung is a ruling request, not a
build** (ruling **R2** — ⚠ folded ruling-id collision fix, RT-A P1-4: §4 numbers it R2 and this
paragraph previously called it R1). **Routing, folded:** the A/B belongs to the lane that owns
`world/snowpack.*` — the road-repair lane holds **both** `class_blend_m 0.0 vs 1.0 (his stick)` and
the `props.cpp` unblended `nearest()` as its own owed items, and records that it touched
*nothing* in `world/snowpack.{h,cpp}`. This audit hands it over; it does not run it.

---

### RUNG 2 — THE LEAN HAS TO BUY THE ARC
*(feel-first Rung 3, unaltered — the only rung on any of the three ladders that buys the ground arc
for the lean. Judges: "decisive on lean_enhancer", "the best rung in all 24".)*

**Felt problem, his words.** *"the balance mechanism works really good. **Just allow the balance of
body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be the necessary
condition of not rolling over.**"* (§0); felt item 5 *"rider weight needs authority, not thrown
into a spin"*; felt item 3 *"turning too unstable, can't hold a carve"*.

**Tape evidence.** MEASURED (D-A §6.3, `tools/sled_tape_audit.py`): he holds **full lean
(|lean| > 0.98) for 14.8–43.6 %** of every v17 drive — 26.5 % pooled tonight, 44 % on tape 91.
R3-M6: lean sits in the **intermediate band on 54 % of riding ticks** while steer is bimodal —
*"lean is genuinely modulated"*. D-E-04: the lean axis holds a partial value for up to **36.1 s**
against the steer axis's all-corpus maximum of **0.500 s**. **He is asking the reward channel for
everything it has, continuously, and holding it there.**

**Mechanism.** Two lean-reward paths exist and the shipped one is the wrong one. `lean_bite_gain
= 0.18` (`sim/sled.cpp:1067`) multiplies the μ bite on **every patch including the track** —
MEASURED self-defeating for an arc (`sim/sled.h:1092-1097`: 0.18 → 0.45 took the lean-in carve from
**30 m to 295–1037 m of radius**, *"because the track out-gains the skis"*). `plane_lat_lean_gain`
(`sim/sled.h:1061`, verified 0.0 this session) is applied once, `sim/sled.cpp:1101-1102`, **inside
`if (g.steered)`** — verified this session — as `lat *= 1.0 + p.plane_lat_lean_gain * align_m`, so
it can only ever add **ski** plate on steered patches. And the strongest lean reward was regressed
on purpose: `sim/sled.h:1154-1157` records that shipping `plane_lift_split_frac = 1.0` re-pinned
test 2336 — *"lean-in no longer tightens the arc by 10 % (28.640 m vs 28.632 m free) — which is
**Chad's own felt item 5, rider weight authority**. It is carried as an open debt."* Both
instruments meant to pay that debt (`traction_mu` — Rung 5 here — and a `w_contact` floor) ship at
**0.0**.

**THE ONE DIAL.** `plane_lat_lean_gain` — **identity 0.0** (`sim/sled.h:1061`). ⚠ Verified this
session: it is a compile-time `SledParams` field with **no entry in `config/load_scenario.cpp`**,
and **there is no `[sled_input]`/`SledParams` bridge at all** (§2 preamble, RT-B P1-7) — so the
rung's first act is the **six-touch RC-8 plumbing**, a `sim/` edit, not "one loader line". At 0.0
the multiply is `× 1.0` — bit-identical.
⚠ **FOLDED (RT-B P1-5): this dial sits in SERIES with the one the rung calls the wrong path.**
`align_m` is computed **only inside** `if (p.comfort.lean_bite_gain > 0.0)` (`sim/sled.cpp:585-592`,
verified this session), so with `lean_bite_gain = 0` the lean multiplier is exactly 1.0 for **any**
`plane_lat_lean_gain`. The rung does not replace `lean_bite_gain`; it rides on it.

**Invariant.** Steady-radius probe on Bush at v0 = 8/12/16/20 m/s: at gain 0 the shipped carve is a
dead-flat **33.2 / 33.5 / 33.7 / 33.9 m**, tightening to **30 m** with lean-in (`sim/sled.h:1050-1054`,
MEASURED). Lean-in must tighten the radius by a felt margin at **every one of those four speeds**.
Plus **`lean_bite_gain` must stay > 0** (folded, RT-B P1-5) or the dial is inert by construction.
⚠ **FOLDED (RT-A P0-3): the "pinned gate leg must return to ≤ 0.90" clause is STRUCK.** The shipped
assertion is `test/unit/test_sled.cpp:2647` `REQUIRE(r_lean < 0.93 * r_free)` — **0.93, not 0.90** —
and the leg **passes today at 0.9003**. The old clause silently demanded tightening a pinned test,
a `test/` edit this audit forbids. Worse, the same test's own printf assigns that ratio elsewhere:
*"debt: back under 0.9000 via 9.7 items 2+3"* = **`traction_mu` (Rung 5) and a `w_contact` assist
floor** (a dial this ladder does not propose). **The 0.90 debt moves to Rung 5; the `w_contact`
floor is named in §3's omissions.**

**Killing mutation.** ⚠ **FOLDED (RT-B P1-6): the old killer could not fail.** *"The non-leaned
radius must not move"* is **guaranteed** — `lean_frac 0` gives `align_m` exactly 0 and the
multiplier exactly 1.0 at any gain — so it was decoration. Three that can fail replace it:
**(a) wrong-way lean must be bit-identical** (`align_m ≥ 0` is the house rule at
`sim/sled.cpp:1098-1102`; if leaning out moves the arc, the term is not reading alignment);
**(b) `track_slip` and the track's lateral force must be unmoved** — the whole claim is that this
buys **ski** plate on steered patches only (`sim/sled.cpp:1101`, inside `if (g.steered)`), and if
the track's lateral moves, the rung has reproduced `lean_bite_gain`'s measured self-defeat;
**(c) straight running must be bit-identical** — `align_m`'s own yaw-rate ramp is
**0.02→0.07 rad/s** (`sim/sled.cpp:588-591`), so a held lean on a straight must still produce
exactly zero. Then the honest limit:
it is inert wherever `plane_lat_gain` is inert — `rho_eff = 0` on **Road and LakeIce**
(`sim/sled.cpp:1083`) — so this rung **cannot** answer *"part of what is so fun on the road is
driving by lean"* (SK-1d) and must not be sold as if it does. Third: raising it past the point
where the inside ski's plate makes a roll couple reproduces `plane_lat_load_frac`'s measured
failure (tip onset 0.318 → 0.334 g, and a −28° non-turning runaway at 16/20 m/s,
`sim/sled.h:1081-1089`).

**Drive-checklist line.** *"Pick one corner on the bush trail and take it at the same speed three
times: neutral, leaned in, leaned out. Does leaning in now tighten the line — and does NOT leaning
still hold the same line it always did?"*

**Law check.** Depth 0.77 untouched (the term multiplies lateral plate, not draft). No governor —
**the player's lean is the only input; the kernel moves nothing.** Wheelie kept — lateral axis only.
STAND rights / LEAN throws — untouched; this is lean as *enhancer*, §0's literal ask. Ragdoll — n/a.
One surface — rides the existing per-surface `rho_eff`, adds no class.

---

### RUNG 3 — R HANDS HER BACK TURNED **AND** AT WIDE-OPEN THROTTLE
*(feel-first Rung 8 with player-journey PJ-1 grafted whole — graft G1, unanimous across judges and
the highest-value graft on the board. PJ measured the half feel-first missed, so the rung is
promoted from 8th to 3rd.)*

**Felt problem, his words.** *"I need a key for now that lets me **autoright** until we get the guy
running back to the snowmachine"* (`app/main.cpp:8279-8286`); *"allow me to land on my skis more
often after a roll (**even though R works**)"* (§0) — R works, and he is still unsatisfied; and
*"**DOnt make it impossibly hard, there is a batttle going on as well**"* (§0b).

**Tape evidence.** MEASURED, PJ read D (raw byte stream of `sled_tape_{85,86,88,91}.sledtape`,
every `O` record and the `T` records around it) corroborating D-E-06: **six rightings in the
current-map corpus — zero at centre, `steer_actual` 1.0000 in FOUR of six, throttle command 1.00 in
SIX of six.** Tape 91 @ t15130 holds **exactly 1.0 for the full 1.5 s** after the righting. Bars
still ≥ 0.1 at +0.50 s in 4 of 6. All five v17 presses land on the **last past-90 exit tick** of
their crash episode — the press is what ends the episode, so this is the shape of *every* recovery
he takes. Scale, corpus-wide: **146 R presses, 144 of them at a body tilt ≥ 75.06°** — the key is
used exactly as designed, not as a teleport.

**Mechanism.** Two halves.
(i) `app/main.cpp:8355-8357` — verified this session — the R block sets `sled_lean_lat = 0.0` and
`sled_lean_fwd = 0.0` and **never touches `sled_steer_cmd`**, while ninety lines earlier
`app/main.cpp:8263-8266` the C key zeroes all three: `sled_steer_cmd = 0.0;  // SK-1c: C recentres
the bars too`. The two recovery keys disagree and R — the one used after a crash — is the one
missing the line.
(ii) `sim/sled.cpp:292-293` — verified this session — `const double throttle = (s.rolled ||
!hands_on) ? 0.0 : clamp01(in.throttle);` — **the throttle is held at zero BY the roll flag and
released the instant R clears it.** ⚠ **FOLDED (RT-B P2-17): `hands_on` is the SECOND reason that
throttle is zero**, and `grip.attached` is **absent from the tape's pin roster**, so *the split
between "zero because rolled" and "zero because the hands came off" is not measurable on tape.*
Ruling R3 must be asked about both. So R does not hand back a merely turned machine: it hands back
a machine pointed the wrong way **at wide-open throttle**, on a surface M12 measured as having *"no
carveable steer angle on Road at ANY speed tested"*.

**THE ONE DIAL.** `autoright_recentre` — **identity 0.0** (= today's absence of the line,
bit-identical). At 1.0 the R block zeroes `sled_steer_cmd` beside the two lean commands, exactly as
C already does.
⚠ **FOLDED (RT-B P1-7): `[sled_input]` DOES NOT EXIST** — `grep -rn "sled_input" config/ app/`
returns nothing, and the only TOML→params bridge is `sim::SledComfort` (`config/load_scenario.h:46`
→ `app/main.cpp:2481`). Two honest routes, and the rung must **pick one out loud**: (i) create
`[sled_input]` **deliberately**, as its own named act with the six-touch plumbing plus a new struct
and bridge; or (ii) carry it as an `app/`-side compile-time constant beside the other input
constants and skip TOML entirely, which is what an `app/` input-layer dial with no kernel consumer
actually wants. **This ladder recommends (ii)** — it keeps the rung's own law-check claim (`sim/`,
`config/`, `test/golden/` unchanged) true.

**Invariant.** Two, and report both. (a) **No `O` record is followed by `|steer_actual| > 0.1`
sustained past 0.500 s** — the kernel's own bar slew time from full lock at `steer_rate_per_s = 2.0`
(`sim/sled.cpp:303`). Today that is 4 of 6; report it as "autorights returned straight / autorights".
(b) The throttle command in the first tick after an `O` — today 1.00 in 6 of 6 — is a **ruling**
(R3), not this dial; it must be *reported* on the same drive so he can rule on it with the number in
front of him.

**Killing mutation.** The kernel's `steer_actual` is not in the override's reset list, so the `O`
record's own tick will still show a non-zero bar — the test is the **0.5 s after**, not the tick
itself. If the next tapes still show `steer_actual` pinned past 0.5 s after an `O`, the app-side
command was not the cause and the rung is dead. Second: `app::autoright_legal`
(`app/player_mode.h`) gates the key, so presses the guard rejects never reach the tape and a
legality change silently changes what the metric counts. Third, and it is the honest one — **the
tape cannot tell a held A/D key from a stale accumulator.** Tape 91 @15130 (full lock the whole
1.5 s, throttle pinned, machine at 0.1 m/s) is equally consistent with him simply *holding* a steer
key through the press, in which case zeroing the command buys one frame and **the rung is
decoration.** ⚠ **FOLDED (RT-A P1-3): the tape settles this for free, and the answer is now a
PRECONDITION.** `app/main.cpp:8244-8256` decays `sled_steer_cmd` at `3.0*dt` **unconditionally**
whenever the key is not held, and the tape carries the command itself (`test/harness/sled_tape.h:198-201`
writes `in.steer`; `:105` pins `steer_actual`; D-E-02's `cmd_minus_bar_abs_max` already reads both).
A **held** key shows `in.steer` flat at ±1.0 after the `O`; a **released** key shows a 3.0/s ramp to
zero in 0.334 s. **Leg X3 = the `in.steer` decay signature over the 1.0 s after each of the six `O`
records** (§5, free, read-only). **If X3 shows held keys, this rung is decoration and comes off the
ladder — its rank-3 position does not stand until X3 has run.** His drive is still the acceptance;
X3 is only what decides whether to spend a drive on it.

**Drive-checklist line.** *"Roll her on the trail, hit R, and take your hands right off A and D —
does she drive away straight, or does she turn the moment you touch the throttle?"*

**Law check.** Depth 0.77 untouched. **No governor** — the player's own key zeroing the player's own
command, exactly as C already does; it moves no mass and applies no force. Wheelie kept. STAND
rights / LEAN throws — untouched; R is the scaffolding key on a separate path and
`sim/sled.cpp:1679-1800` is not modified, nor is the latched-brace law at `1731-1742`. Ragdoll — n/a.
One surface untouched. `sim/`, `config/`, `test/golden/` all unchanged — this is an `app/`
input-layer dial.

---

### RUNG 4 — THE SECOND ROLL, NOT THE FIRST
*(feel-first Rung 2, unaltered. The only rung on any ladder aimed at RC-4 and the 71 %.)*

**Felt problem, his words.** *"Let me slide around a bit arcade but **allow me to land on my skis
more often after a roll** (even though R works). **But not every time — allow it to happen**"* (§0);
*"Leaning shall enhance the ride an just make it more stable and **slef righting by chance more**"*
(§0b).

**Tape evidence.** MEASURED, D-B-2 / R3-M3 / PJ read A: **71.2 % of tonight's rollovers never
return to steady driving** (37 of 52), and the metric has not moved in four weeks and three signed
kernels — 69 % pre-reconcile, 70 % v13g, 67 % v15, **71 % v17**. The recovery *count* is invariant
across four different recovery predicates (15 in every one; only the *times* move). R3-M1: the 52
past-90 crossings merge into **16 crash episodes — 3.25 crossings per crash**, one crash every 45 s.
R3-M2: a crash costs **p50 7.02 s, p90 18.13 s, max 38.41 s** to get back to 80 % of the speed
carried in, 4 of 15 episodes right-censored (so 7 s is a **floor**). D-B-3: he is **not** reaching
for the backstop — 5 R presses against 52 rollovers. `sled_tape_90`: **1 recovery out of 7
rollovers**.

**Mechanism.** `sim/sled.cpp:1545-1607` — RC item C2, the side-contact righting bias, the kernel's
only "she slides back onto her skis" term. `wv` ramps on lateral speed only (1.5→6.0 m/s), `w_load`
on hull load, `wt` is a smoothstep window on `|phi_surf|` open from 40° to 140°, and
`wo = clamp01(1 − |ω_z|/wref)` fades it **out** above `side_right_wref_rads` — so a machine that has
*stopped* spinning gets the full bias. `sim/sled.h:497-512` records the shipped pair's own
trade-off, verbatim (⚠ **FOLDED, RT-A P2-1: the citation was `sim/sled.h:1035-1049`, which is the
`plane_lat_gain` planing-lateral comment. Corrected and re-verified this session**): *"(700, 2.5) is the measured best-of-9 … smooth, non-oscillatory settling
(**stalls ~105-125 deg rather than swinging**) … **the 15 m/s full recovery is not alive at this
pair**, which is the honest trade-off reported for the supervising session, not silently forced."*
**A machine stalled at 105–125° with near-zero roll rate is exactly the 71 %**, and it is exactly
the population `wo` hands full authority to. The dial was shipped conservative and the source says so.

**THE ONE DIAL.** ⚠ **FOLDED (RT-B P0-1) — THE DIAL CHANGED. It is
`[sled_comfort] side_right_vmin_ms` — identity 1.5** (`sim/sled.h:518`,
`config/scenario.toml:2141`, both verified this session, no drift). **Not `side_right_gain_nm`.**

The reason is arithmetic and it kills the old dial on the rung's own population. `sim/sled.cpp:1555-1559`
computes `wv = clamp01((v_side − side_right_vmin_ms) / (side_right_vref_ms − side_right_vmin_ms))`
and the torque at `:1606-1607` is `gain · wv · wt · w_side · w_load · wo` — so **`wv == 0` multiplies
the gain to zero**, and no value of `side_right_gain_nm` can reach a machine whose lateral speed is
under 1.5 m/s. MEASURED over tapes 86–91 (RT-B, read-only): **`wv == 0` on 2 277/4 973 (45.8 %) of
past-90 ticks, on 1 178/1 570 (75.0 %) of the stall window this rung argues from** (tilt 100–130°,
|ω_z| < 0.5), and on **795/886 (89.7 %)** of tape 91's. In that same window `wo > 0.8` on **1 570 of
1 570** — so the rung's `wo` reading was right and its **dial was wrong**: three quarters of the
stalled population is behind a gate the old dial cannot open.

**`side_right_gain_nm` (700.0) is kept as a SECOND dial**, graded only on the **2 696 still-sliding
past-90 ticks** where `wv > 0`. Both are already TOML-reachable
(`config/load_scenario.cpp:549-554`).
*Killing mutation for the swap itself:* `side_right_vmin_ms = 0.0` would open `wv` at rest and make
the gain sufficient after all — it ships **1.5 in the header AND the TOML**, verified, no drift.
⚠ **And the fence that must stay green while it moves:** `test/unit/test_sled.cpp:2649`
`sled_onside_recovery_is_momentum_not_magnetism`, whose own comment names
**`side_right_vmin_ms 0 with the gain high`** as what kills it. Lowering this dial walks toward the
anti-magnetism line **on purpose**; that leg is the wall.

**Invariant.** Two-sided, and both sides are the point: `never_recovered` must **fall from 71.2 %**
**while `rollovers_past90_per_min` does not fall below ~2/min** — if the rolls stop, the rung has
broken *"Make it possible to roll but not the rule"* in the other direction. Second, from the
source's own measured failure mode: the recovery must remain **non-oscillatory** — no episode may
cross ±140° of `phi_surf` more than twice.

**Killing mutation.** Lower `side_right_vmin_ms` (and only then raise the gain) and re-run the
`onside_trace` L4 sweep: if the extra recoveries arrive via **violent repeated near-180°
oscillation** (MEASURED as the failure mode of every higher-gain cell of the original best-of-9), or
if `sled_onside_recovery_is_momentum_not_magnetism` goes red, **the rung is dead as written** — a
parked machine that stands itself up is the cheat the kernel forbids by name, and no amount of
recovered percentage buys it. Second: D-B-2's recovery predicate — widen to tilt
< 45°, or drop the 0.5 s hold, and the unrecovered fraction must fall; it is the **trend across four
buckets under one predicate** that carries the claim, not its level. Third, unbounded and named:
the invariant's ~2/min floor is compared against a tape-to-tape spread this ladder never bounded —
tonight's two trail-bearing tapes alone run 4.1 vs 6.2/min. **Fourth (folded out of the killing
mutation, RT-A P1-5): `side_right_wref_rads` and the 40–140° window are the rung's SECOND-ORDER
dials and belong to ruling R11, not to this dial's mutation list** — moving the window is a
different rung with a different claim.

**⚠ RULING R11 (new, folded from RT-A P1-5) — hard gate on this rung, and it needs his two
sentences side by side.** *"Leaning shall enhance the ride an just make it more stable and **slef
righting by chance more**"* (§0b) against the 2026-09-03 P-key retirement's *"**STAND alone rights
the tipped sled**"* and his own self-right spec, *"IF ON THE SEAT AFTER A ROLLOVER **PRESSING THE
STAND BUTTON**"* (`config/scenario.toml:2053-2056`). C2 rights the machine **with no player input at
all**, and `wo = clamp01(1 − |ω_z|/wref)` (`sim/sled.cpp:1603-1606`) makes it **strongest on a
machine that has stopped** — the population furthest from *"by chance"* and closest to the
magnetism he had the kernel forbid. **Is a kernel-side righting with no press what "by chance" meant,
or did he mean the press should more often succeed?** The two answers point at different rungs.
**This rung is also hard-gated on R4** (is a backflip a rollover) — without it the two-sided
invariant has no denominator.

**Drive-checklist line.** *"Roll her deliberately at 20, 40 and 60 km/h, five times each, and tell
me how many times she slid back onto her skis by herself — and whether any of them looked like a
fish flopping."*

**Law check.** Depth 0.77 untouched. No governor — C2 adds `torque_body.z` at `sim/sled.cpp:1606`
and **never reads or moves `rider_lat_m`**; it is the §0b arcade-lawful term and *"may only AMPLIFY
side-slide momentum that already exists"* (its own header, `sim/sled.cpp:1536`). Wheelie kept — roll
axis only. STAND rights / LEAN throws untouched; R remains the backstop and `wv` is zero below
1.5 m/s so *"a stationary machine NEVER self-rights"*. Ragdoll banned — **this is the rung's live
risk**, and the non-oscillation invariant above is the fence.

---

### RUNG 5 — A TRACK CANNOT PUSH HARDER THAN THE SNOW IT STANDS ON
*(GRAFT G4, physics-honest Rung 3, judges 1 and 3. Cheapest paid ask in the audit: the ruling is
TAKEN, the mechanism is BUILT, the sweep is ON THE BOOKS, and it ships at 0.0. Drives in the same
session as Rung 1.)*

**Felt problem, his words.** 2026-08-24, verbatim (quoted into `sim/sled.h` at `traction_mu`):
*"**at high speeds, getting pulled in and flipping out like 15x is not desirable so yes take care of
that**"* — and in the same ruling the lift itself **stays**: *"it is important for traversing atop
the snow"*, *"at slower speeds I think it is good too because [I'd] like to jump the snowbanks."*

**Tape evidence.** MEASURED, D-B: **WOT 52 % of tonight's drive time**, and `sled_tape_91` at
**72 % WOT with 23 rollovers in 173 s** — he rides with the thumb pinned, which is exactly the
regime this ceiling governs. D-C O8: his ruling was **taken**, the mechanism was **built**, and it
**ships at 0.0**.

**Mechanism.** `sim/sled.cpp:1211-1222`. Both existing caps on thrust are **ENGINE** ceilings
(`max_thrust_n 2272`, `engine_power_w/|v|`). The contact ceiling is a branch gated on
`p.traction_mu > 0.0` and is the **only** one the snow gets a say in. The source names the loop in
its own comment: *"`shear`'s cohesion term (area*c_eff) and `roost_thrust` are both
load-INDEPENDENT, so without this a track carrying 55 N still pulls at max_thrust_n — the term that
feeds the speed→lift→contact-down→speed runaway."* At the §9 attractor the running surfaces hold
**62 N of 3246.0 N — 1.91 % of the machine's weight** (corrected denominator, §1(f)) — while the
drivetrain is still pegged.

**THE ONE DIAL.** `traction_mu` (`sim/sled.h:1006`, verified 0.0 this session). **Identity 0.0 —
bit-identical**: a branch, and `≤ 0` is the pre-item-2 kernel exactly. ⚠ Verified this session: it
has **no entry in `config/load_scenario.cpp`** and lives on `SledParams`, which has **no TOML bridge
at all** — so like Rung 2 it needs the **six-touch RC-8 plumbing** (§2 preamble, folded RT-B P1-7),
a `sim/` edit, **not** "one loader line". ⚠ **Folded in from Rung 2 (RT-A P0-3): the
`sled_lean_into_the_carve_tightens_the_radius` 0.90 debt is THIS rung's**, by the pinned test's own
printf — *"debt: back under 0.9000 via 9.7 items 2+3"*, where item 2 is `traction_mu`. Today the leg
passes at 0.9003 against a shipped `REQUIRE(r_lean < 0.93 * r_free)`; **the 0.90 is a debt to report,
never an assertion to tighten.**
**MEASURED sweep already on the books** (`docs/snowform_measurements.md` §M8.1): at **μ ≥ 0.6** the
Bush runaway does not develop (`t(|roll|>20°)` 6.22 s → 1.33 s; `v_end` 24.8 m/s *rising* → 0.1 m/s),
and at **μ ≥ 3.0** Chad's own traverse is **byte-identical to baseline** (14.1 / 14.5 / 15.3 m/s).
A value exists that pays his ruling and costs his traverse nothing.

**Invariant.** The emergence matrix and trail-speed sweep rows stay inside the charter §2
bit-identity tolerance (`track_clearance 0.255`, Bekker constants, trail speeds are SEALED), **AND**
the §9 Bush runaway leg terminates (`v_end` falls instead of rising), **AND** the snowbank jump he
asked to keep still launches.

**Killing mutation.** `traction_mu = 0.0` → the branch at `sled.cpp:1219` is dead, bit-identical.
**And the honest limit, stated by the measurement itself:** M8.2/M8.3 record it **inert on Road by
construction** (`rho_eff = 0`, no planing, full contact) — so **it does not fix the bank-strike
superspin.** That one is Rung 1. Anyone selling this rung as that fix is selling the wrong thing.

**Drive-checklist line.** *"Pin it wide open across the deep bush at speed — does she still get
pulled in and flip out, or does she just run out of push?"*

**Law check.** Depth 0.77 untouched. No governor — a friction ceiling on thrust, not a torque;
nothing moves the rider. Wheelie kept — **and check it**: the launch wheelie stands on the track's
thrust, so the sweep must show WOT+stand+lean-back pitch not collapsing (his item 4 ask, *"WOT
should lift the skis to ~30°"*, is already unpaid at a measured p50 3.6°). STAND/LEAN untouched.
Ragdoll not involved. One surface — reads the existing per-patch `normal`.

---

### RUNG 6 — SHOW HIM THE HAND THAT HAS BEEN HOLDING HER UP
*(GRAFT G7/G2/G4, player-journey PJ-3 — the one graft all three judges took whole. It is the
precondition for Chad grading Rungs 2 and 4 from the seat at all.)*

**Felt problem, his words.** *"**Just allow the balance of body mechanism to ENHANCE ability, i.e.
tighten a turn** instead of having to be the necessary condition of not rolling over"* (§0) and
*"**Allow for bad driving too but keep the benefit there for good riding**"* (§0b). Both sentences
are unanswerable from the seat today: he cannot learn an enhancement he has never been shown, and
he cannot tell good riding from bad if the machine's contribution is silent.

**Tape evidence.** MEASURED, PJ read A over the six v17 drives: `assist_active_frac` =
**0.798 / 0.860 / 0.866 / 0.876 / 0.884 / 0.937** — the comfort assist applies a non-zero body-z
torque on **79.8–93.7 % of ticks on every single drive** — magnitude p50 **5–75 N·m**, p95
**215–765 N·m**. ⚠ **FOLDED (RT-B P1-11): restated against the ONE denominator, 933.1 N·m** (§1(d));
the old "51 % of the roll budget" used `W × half-stance = 1 504.5 N·m`, an arm `sim/sled.h:546` says
is **not** the rollover's. Against 933.1 N·m the p95 is **82 %** — a bigger number, and it is the
right one.
⚠ **FOLDED (RT-A P0-2) — and this is the correction that changes what the rung MEANS.** `assist_nm`
is **not** the comfort assist's spring; it is spring **plus damper**:
`sim/sled.cpp:1876-1882` sets
`s.assist_nm = (−stiff·tanh(φ/roll_ref_rad) − roll_damp_nms·ω_z) · release_eff · w_contact`,
with `roll_stiff_nm 450.0` / `roll_ref_rad 0.14` (`config/scenario.toml:2016-2017`) and
`roll_damp_nms 800.0` (`:2027`) — all verified this session. Because `tanh ≤ 1` bounds the spring
half at **450 N·m**, and the *same* `release_eff·w_contact` scales both halves, **every |assist_nm|
above 450 N·m is damper-majority**: at the p95 of 765 N·m **at least 315 N·m (41 %) is
`800·ω_z`.** A tell driven off the raw scalar is a **roll-RATE meter**, and it would burn brightest
exactly when Chad **flicks her** — the term `sim/sled.h:362-365` calls *"the dial that kills the
flick, **the drift's body language**"*. **Read the magnitudes as: spring ≤ 450 N·m by construction;
everything above that is roll rate.** The 82 % headline is NOT a comfort number. And PJ read E: `grep -rln "assist_nm" render/ app/` returns exactly one file,
`app/spawn_policy.h`, and that is **two resets**. Same for `right_assist_nm_now`.

**Mechanism.** The term is written at `sim/sled.cpp:1881` (`torque_body.z += tq`, the A + B2 comfort
assist) and stored in `sim::SledState::assist_nm`. `render/draw.h:537-633` carries `sled_air_s`,
`sled_hull_engage`, `sled_susp_x[3]`, `sled_grip_attached`, `sled_weight_x/y`, `sled_hud_xray` —
**and no assist field.** The instrument he already chose is right there: the x-ray grid on the
sweater, `render/draw.cpp:5250-5330`, `sled_hud_xray` default true, `H` toggles, and his SK-1d words
(commit `c2b7bfeeb`, 2026-08-25): *"make it pasted to his white/grey sweater … **make the grid blue
and the ball slag orange**, glowing as the themed color code."*

**THE ONE DIAL.** ⚠ **FOLDED (RT-A P2-4): the BAND is the dial; `kAssistTell` is demoted to a
display gain** — the rung's own text already said *"the BAND, not the duty, is the whole rung"*, so
the band gets the identity. `[render] assist_tell_band_nm` — **identity ∞ / off: no band opens, the
x-ray panel is pixel-identical.** `render::kAssistTell` (+ `SEADS_SLED_ASSIST_TELL`, identity 0.0)
grades the slag-orange ball's glow **above** that band.
⚠ **FOLDED (RT-B P2-14): the tell must be ADDITIVE, behind `if (kAssistTell > 0.0f)`** — if it
*scales* an existing glow, then 0.0 puts the ball **dark** and the pixel-identity claim is false.
⚠ **FOLDED (RT-A P0-2): the tell must be gated on the SPRING component alone**, not on
`assist_nm` — and the spring is **not stored anywhere today**, so this needs **a new
`sim::SledState` field** written beside `s.assist_nm` at `sim/sled.cpp:1877-1882`. Two fields
carried into `render::DrawInfo`, beside `sled_hull_engage`.

**Invariant.** At 0.0, a frame capture of the HUD region is byte-identical. Above 0, acceptance is
**his word** — *"did you notice when she was helping you?"* — never a number (RC-5). The one number
worth reporting beside it is the **fraction of ticks the tell is lit** under the chosen band, which
must sit far below the 79.8–93.7 % duty or the tell is wallpaper.

**Killing mutation.** The assist is non-zero ~86 % of the time but its **median is 5–75 N·m —
0.5 to 8 % of the 933.1 N·m roll budget.** A tell gated on `assist_nm != 0` is lit almost always and
carries no information, so **the BAND is the whole rung**; if no band exists on the **spring** that
is both informative and infrequent, **the rung is dead**. Second, folded (RT-A P0-2): if the tell
is built on the raw scalar it lights on **roll rate**, so the mutation that kills the build is
*"hold a steady lean at constant roll angle — does the ball stay lit?"* If it only lights during
flicks, the tell is measuring the drift, not the help. Third: every figure quoted to him must name
**the mass and the arm** it used (§1(d)/§1(f), ruling R10) — 765 N·m is 82 % of 933.1, 51 % of
1 504.5 and 40 % of the unsourced 1 931.8, and only the first is this document's denominator.

**Drive-checklist line.** *"Ride the bush with `SEADS_SLED_ASSIST_TELL=0.6` — does the orange ball
light up at moments you agree the machine was saving you, or is it just on all the time?"*

**Law check.** Depth untouched. **No governor** — this *shows* a torque that already ships; it adds
no term and moves no mass. It is the opposite of the thing the kernel's own comment forbids (*"a
hidden nudge is the RNG-shaped sin this kernel forbids"*, `sim/sled.h` at the self-right block).
Wheelie / STAND / LEAN / ragdoll / one surface untouched.
⚠ **FOLDED (RT-A P0-2): the old law-check line "`render/` and `app/` only; `sim/` … unchanged" is
FALSE and is struck.** Gating the tell on the spring component requires a **new `sim::SledState`
field** — a `sim/` touch, physics-inert (a store, no torque, no branch on it) but a `sim/` touch
nonetheless, and it must be declared as one. **If a `sim/` touch is unacceptable for a readout, the
rung is DROPPED** rather than rebuilt on the raw scalar — a tell that lights on roll rate while
claiming to show comfort is worse than no tell. `config/` and `test/golden/` unchanged either way.

---

### RUNG 7 — "TIPPING OVER" IS RELATIVE TO THE HILL, NOT TO THE PLANET
*(GRAFT G2, physics-honest Rung 4, promoted by all three judges from feel-first's ruling R2(b) into a
rung. It is the only proposal in the whole audit that REMOVES kernel-commanded rider mass movement —
it moves the tree toward §0b's law rather than merely not away from it.)*

**Felt problem, his words.** §0: *"Just allow leaning a certain way in a particular condition to be
the OPTIMAL weight distro for better traversing, **say up a hill** or around a corner."* And his
self-right spec, verbatim in `config/scenario.toml [sled_comfort]`: *"IF ON THE SEAT AFTER A
ROLLOVER PRESSING THE STAND BUTTON … **MACHINE NEEDS TO BE TIPPING OVER** … NOT ABOVE 5KM/H … IT CAN
FAIL TO RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD."*

**Tape evidence.** ⚠ **FOLDED — the old paragraph was WRONG and is struck.** It read the raw STAND
duty (6.8–39.0 %, mean ≈ 24 %) and the 11.9 % catwalk as *"the gate below is armed during ordinary
riding"*. **It cannot be**: the self-right runs behind a hysteretic low-passed speed gate with
`right_assist_max_ms = 1.3888888888888888 m/s = 5 km/h` (`config/scenario.toml:2088`, his own
*"NOT ABOVE 5KM/H"*), re-arming at 0.8× (`:2089`) on a 0.5 s low pass (`:2108`), and the tree ships
the leg *"selfright: above 5 km/h it cannot happen"* (`test/unit/test_sled_selfright.cpp:301`).
Every duty quoted above is **at speed** and arms nothing (RT-A P1-1).

**The corrected evidence, MEASURED over 86 578 v17 ticks** (RT-B P1-8, read-only over tapes 86–91 —
the tape pins every input this gate reads except `hands_on`, so this is an **upper bound**):
`stand > 0.5` = **25.6 %**, but `stand > 0.5 ∧ ground_speed_ms < 1.3889` = **2.62 %**, and with
`tilt > 0.25 rad` (the ramp's own lower edge, `config/scenario.toml:2105`) = **2.32 %**. The old
paragraph overstated the armed duty by **≈ 11×**.
**And the first real evidence FOR the rung:** of **2 011 armed ticks, 1 022 (50.8 %) have
`hull_engage_lp < 0.05`** — armed with the hull not engaged, i.e. *not* lying on her side — and
**839 of those are `sled_tape_89`, which has ZERO rollovers.** That is the shape the rung predicts:
a conformal machine at a crawl reading the planet as "tipping over".
**Epistemics that travel with it:** `hull_engage_lp` is a *low-passed proxy* for `|phi_surf| > 25°`,
the tape samples at 120 Hz against the kernel's 1 440 Hz, and `grip.attached` is unpinned — so a
null under-detects and these are sustained states, not edges.

**Mechanism.** `sim/sled.cpp:1680-1687`: `tilt = acos(up_body.y)` where `up_body = Rᵀ·up_cg` —
**tilt off gravity**, so a machine sitting perfectly conformal on a side-hill reads the slope angle
as "tipping over". DERIVED: on a conformal **20.05°** slope `w_tilt = 0.9997` — full authority — and
`config/world.toml`'s own snowpack block records terrain slope **p95 = 21.1°** over 79 847 land
samples, so **~5 % of the world's land is at or past the full-authority edge of this gate.** The
kernel fixed exactly this defect everywhere else and wrote down why (`sled.cpp:1367-1371`:
*"red-team P1-1/2: the earlier gravity-tilt gate armed rigid hull contacts during ordinary slope
riding and misread pitched crashes as rollovers"*) — the hull terms were moved to `phi_surf`;
**the self-right was left behind.** The aggravator that **is** in scope: the block writes **up to 0.35 m of rider lateral
displacement** through `right_shift_cmd` (1791 → consumed 1401-1407) — *the kernel moving the
rider's mass*, the thing §0b forbids by name, authorised only by his later *"MACHINE NEEDS TO BE
TIPPING OVER"* ruling — **and the gate deciding "tipping over" is the broken one.**
⚠ **FOLDED (RT-A P2-5): `right_dir_eps = 0.04` (a 2.29° dead-cone against a documented 10°,
confirmed at `config/scenario.toml:2102`) is NOT in this rung's scope** and has been moved to §3's
named omissions. One dial per rung; this rung does not touch it.
⚠ **FOLDED (RT-B P2-16), for the record:** `right_assist_min_tilt_rad` is a **required TOML key**
(`config/load_scenario.cpp:514-515`, `config/scenario.toml:2094` = 0.35) that **`sim/sled.cpp` never
reads** — the ramp's `right_tilt_lo_rad` 0.25 / `right_tilt_hi_rad` 0.35 owns the gate, and the TOML
itself says so. Anyone reading the config to find the threshold finds a decoy.

**THE ONE DIAL.** `[sled_comfort] right_tilt_surf` — **identity: OFF, a BRANCH, bit-identical.**
⚠ **FOLDED (RT-B P1-10) — the reference changed and the blend became a branch.**
*Do not blend toward `|phi_surf|`.* `phi_surf` (`sim/sled.cpp:1423-1425`) is a surface-relative
**ROLL**: with `c = body_up · n_surf` and `r = (R·x) · n_surf`, `|phi_surf| = atan2(|r|, c)` while
tilt is `acos(c)` — equal **only when pitch is zero**. At exactly 90° of pure pitch `phi_surf = 0`
while tilt = 90°, and just past it `phi_surf` jumps to 180°: at frac 1.0 the gate becomes a **step
function in pitch**, and a nose-down 85° crash — the exact case `sim/sled.cpp:1367-1371` was written
about — would have the self-right **REFUSE**.
**Use the surface-relative TILT instead:** `tilt_ref = acos(clamp(dot(up_body, n_surf), −1, 1))`.
`n_surf` is declared at `sim/sled.cpp:1372`, in the **same substep scope** as the self-right `if` at
`:1679`, so there is no plumbing to add; and it degenerates to `acos(up_body.y)` **exactly** in the
stated <3-patch fallback. On flat ground it is unchanged; on a conformal side-hill it reads ~0, so
the term goes silent on the hill and stays fully armed for the rollover it was built for — and it
stays correct in pitch, which `|phi_surf|` does not.
**Write it as a BRANCH, not `glm::mix` at t = 0** (RT-B P1-10 third point): this ladder's own §2 law
forbids a 0-weight lerp; `kernel-v14-leanlead` is the precedent. The direction term
(`e = −up_body.x`, `sim/sled.cpp:1762-1765`) **stays gravity-referenced** — this rung changes what
counts as "tipping over", not which way she is pushed.

**Invariant.** Every leg in `test/unit/test_sled_selfright.cpp` (11 TEST_CASEs) passes unmoved
**with the branch taken** — they are flat-ground legs, where `n_surf` is up and the two references
agree — including `selfright: above 5 km/h it cannot happen` (`:301`). **Plus a new leg that does
not exist today**: a machine settled conformal on a 20° side-hill with STAND held and speed
< 1.389 m/s receives **zero** self-right torque and **zero** commanded rider shift. **Plus, folded
(RT-B P1-10): a nose-down 85° attitude on flat ground must STILL right** — the leg that would have
caught the `|phi_surf|` version. **Plus leg L3** (§5): the hill-traverse before/after, because this
rung removes hill-time righting authority and nothing else on the ladder can see that.

**Killing mutation.** OFF → the expression is `acos(up_body.y)` verbatim, the branch not taken.
**And the claim's own killer, carried from the source ladder:** if `n_surf` degenerates to radial up
whenever fewer than three patches are loaded (the fallback at `sled.cpp:1372-1425`), the change is a
**no-op in exactly the tipped case** — safe, but the rung buys less than it claims.
⚠ **FOLDED (RT-B P1-9): the old line "Unverified on tape … probe-only" is STRUCK — it was wrong.**
The tape pins `position`, `orientation`, `ground_speed_ms` and `hull_engage_lp` (`SLEDTAPE_PIN_D`)
and `in.stand` (`TickRec`), which **is** the gate's own arithmetic (`sim/sled.cpp:1680-1682`) minus
only `hands_on`. It was measurable all along and it has now been measured (above). What remains
genuinely unaskable of the tape is **`right_assist_nm_now` / `right_shift_cmd` / `right_charge`** —
absent from the pin roster — so *the torque itself* is still probe-only; the **gate** is not.

**Drive-checklist line.** *"Sit her across a steep side-hill at a crawl and hold STAND — does she try
to stand herself up off the hill when you didn't ask?"*

**Law check.** **This rung moves TOWARD the law, not away.** No governor: it *removes* kernel-
commanded rider shift during ordinary slope riding, restoring §0b's *"the player's lean is the only
thing that moves the rider."* STAND still rights and LEAN still throws — the 2026-08-26 separation
(`31e0dd96a`) is untouched and his *"I dont understand why yo are clonlating the lean mechanism with
the righting"* correction stays honoured. Depth 0.77 untouched. Wheelie kept. Ragdoll not involved.
One surface: `phi_surf` is fitted through the three patches' own ground points, **no new ground
query** (INV-1 intact).

**Risk, named.** ⚠ The self-right also has **NO contact gate at all** — between `sled.cpp:1679` and
`1800` there is not one reference to `normal_sum`, `side_normal_sum`, `ground_contact` or
`w_contact`, and the speed gate reads *horizontal* speed, so a near-vertical drop stays armed.
DERIVED airborne authority: 2400/49.7 = 48.3 rad/s², one held press ≈ 1080 N·m·s ⇒ **≈ 1244 °/s of
free roll with no ground under the machine.** That is a **separate rung with its own ruling** (R6) —
it is *not* folded in here, because adding `&& ground_contact` is a behaviour change Chad has never
been asked about, and no tape can see it (`right_assist_nm_now` is deliberately absent from the
tape's pin roster).

---

### RUNG 8 — OVER THE BACK
*(feel-first Rung 6, unaltered, and deliberately last: it is the only rung on the merged ladder that
adds a new kernel term.)*

**Felt problem, his words.** Felt item 6: *"**flips happen too fast and easy**"* (gi4 §1); and *"If I
lean back and hit a snowbank, **I flip multiple times end over end... like 5x**"* (2026-08-13) held
against, in the same breath, *"**I DO want a wheelie ... even MORE so**"* and felt item 4 *"WOT should
lift the skis to ~30°"*.

**Tape evidence.** MEASURED, D-B-7, under `stand > 0.5 ∧ lean_fwd < −0.30 ∧ throttle > 0.90 ∧
air_s == 0` (ground contact — an airborne backflip is excluded from the *sample*): tonight's pitch is
**p50 0.75°, p90 9.25°, p95 14.25°, p99 66.75°, max 85.4°**, the condition holding **11.9 % of drive
time**. `sled_tape_86` reaches catwalk p95 **66.2°** at 14.7 % duty; `sled_tape_87` reaches **3.8°**
at 15.6 % duty — **the same commanded pose produced either nothing or a flip, on two tapes four
minutes apart.** That bimodality is the complaint.
⚠ **FOLDED (RT-B P1-3): the "there is no populated band" argument is DELETED — it is false.**
MEASURED under the rung's own predicate over **10 316 catwalk ticks**: the **+20…+60° band holds 156
ticks** against **209 past +60°**; on tape 91 alone it is **107 against 52**. The band is populated,
and at tape-91 scale it is *twice as populated as the flip*. **What protects the 30° wheelie is the
smoothstep OPENING at 60°, not emptiness** — say that, and only that.
⚠ **FOLDED (RT-B P1-4): the nose-DOWN half is 19× bigger.** The −20…−60° band holds **3 010 ticks**.
An unsigned `|pitch|` band would fire on **≈ 29 % of catwalk time** and would damp a machine
**pulling her nose out of a dive**. The band must be **SIGNED — nose-up only.**

**Mechanism.** There is **no pitch damping anywhere in this kernel.** `grep -nE
"torque_body(\.[xyz])?\s*(\+=|-=|=)" sim/sled.cpp` returns exactly five sites (verified in
`R4-landings.refute.md` R-2): `:602` per-patch (contact), `:1608` C2 (roll, z), `:1655` C3
(yaw-arrest), `:1792` the STAND self-right (roll, z), `:1881` the comfort assist (roll, z). **Every
comfort term in the kernel is a roll or yaw term; pitch has none.** Pitch restoration comes only
from the track's fore/aft normal split (`sim/sled.cpp:916-949`, `track_pitch_half_m = 0.20`), which
saturates the moment one split point unloads — i.e. exactly when the nose is already up. The one
dial that would stiffen it, `track_pitch_half_m`, is the **wrong** place: `sim/sled.h:605-610`
MEASURES that at 0.45 the rider's fore-aft authority collapses to 0.32× of W/L (felt item 5 again),
and its green window is narrow (0.20 → 0 red; 0.18 and 0.22 → 5 and 7 red).

**THE ONE DIAL.** `[sled_comfort] pitch_arrest_nms` — **identity 0.0**, and **gated as a branch**,
`if (p.comfort.pitch_arrest_nms > 0.0)`, not an add-of-zero (folded, RT-B P2-19: the §2 law).
⚠ **SECOND DIAL, named (folded, RT-A P2-4): `[sled_comfort] pitch_arrest_open_deg` — identity 180.0
(a band that never opens = bit-identical).** The rung's own killing mutation sweeps the band, which
makes it a dial with its own identity, its own RC-8 loader line and its own
`test/unit/test_load_scenario.cpp` CHECK. It ships at the proposed **60.0** only if Chad rules it.
⚠ **FOLDED (RT-B P0-2) — THE AXIS WAS WRONG.** `sim/sled.h:772` labels the body axes
**"X pitch, Y yaw, Z roll"** (verified this session; corroborated by `roll_damp_nms · ω_z` into
`torque_body.z` at `sim/sled.cpp:1878-1881`, and by the pitch-rate write
`angular_vel.x -= k_gyro_react·dL/I.x` at `:2070`). The old spec — *"τ_y = −gain · ω_y"*, *"a damping
term on ω_y"* — is a **YAW damper in this kernel**, landing on the drift Chad signed. Correct shape:

```
torque_body.x += −pitch_arrest_nms · s.angular_vel.x · w_contact · w_band   // X = PITCH
```

with `w_band` a **signed, nose-up-only** smoothstep opening from **60°** of pitch (never `|pitch|`)
and `w_contact` the existing `sim/sled.cpp:1842-1844` gate.

**Invariant.** The catwalk pitch **p99 falls from 66.75° toward the p95 of 14.25°** while **p50, p90
and p95 do not move** and the catwalk duty (11.9 %) does not move.
⚠ **FOLDED (RT-B P0-2), the fence that proves the term did not land on the drift: the `|ω_y|`
(yaw-rate) distribution must be BIT-IDENTICAL.** On the wrong axis the old invariant could not fail.
⚠ **FOLDED (RT-A P0-1): the backflip is MEASURED, not asserted.** The old text said the summit
send's 165.5° airborne tilt is bit-identical *"which it is by the `w_contact` gate"* — but
`w_contact` (`sim/sled.cpp:1842-1844`) is zero **airborne** and **non-zero in the contact ticks
before takeoff**, and a backflip off a lip is **initiated in contact**. A 60°-band pitch damper can
bite exactly the ticks that set the airborne rotation rate. The charter (§1) demands the backflip be
**measured** — *"do NOT delete them … measure the backflip stays POSSIBLE"* — so:
**new invariant clause: the maximum pitch reached while `w_contact > 0` on a send must sit BELOW the
band opening.** **Probe leg L6 (§5) is a PRECONDITION of this rung.** If that max exceeds 60°,
**the rung is dead as written** and the band must move or the gate must change.

**Killing mutation.** Set `pitch_arrest_open_deg` to 0 instead of 60 — the wheelie dies and felt
item 4 with it; that is the test that the 60° opening is load-bearing and it must be run and
reported. Second, folded (RT-B P1-4): build the band **unsigned** and it fires on ≈ 29 % of catwalk
time, damping her out of a dive — if the nose-down duty moves at all, the sign was lost.
Second: drop the `air_s == 0` clause from the catwalk measurement and p95 jumps as airborne
backflips contaminate the sample — that clause is what makes this a catwalk measurement at all.
Third, the honest limit: **this rung does NOT address the snowbank end-over-end.** `gi2tumble`
measured aft+stand at **2.8 turns @ 18 m/s and 4.4 @ 20** against neutral-seated 1.8 which
*recovers* — that tumble is largely airborne and a contact-gated term cannot reach it. The tumble
rung was fenced out of GI3 and **was never built** (D-C O3); it is a separate rung and must not be
claimed here.

**Drive-checklist line.** *"WOT + stand + lean back on the flats: do the skis still come up the way
you wanted — and does she still go all the way over the back when you hold it?"*

**Law check.** Depth 0.77 untouched. No governor — a body torque; nothing reads or moves
`rider_lat_m`/`rider_fwd_m`; it is §0b's own worked example (*"a contact-gated roll-stability
moment"*) rotated onto pitch. **Wheelie kept — by the 60° gate, and the invariant above is what
proves it.** ⚠ Backflip kept — **claimed on `w_contact` (identically zero airborne, `sim/sled.h:352`), PROVEN
only by probe leg L6**; the airborne half is free, the contact-phase initiation is the open question
(RT-A P0-1). STAND rights / LEAN throws untouched (a damping term on the **pitch** rate `ω_x`, not a
righting torque; it never picks a side).
Ragdoll banned — damping only, no stiffness, so it cannot ring. One surface untouched.
**⚠ This is the only rung that adds a new kernel term. It is the riskiest on the ladder and it must
not be built before Chad rules on it (R7), with `belt_brake_frac` attached as its precondition if he
also says yes to in-air control (R6).**

---

## 3. WHAT THIS MERGED LADDER DELIBERATELY DOES NOT DO

**Dropped for the 8-rung budget (all four were real; named so nobody rediscovers them as omissions):**

- **feel-first Rung 4, `steer_centre_per_s` 3.0 → 2.0 (the bars stop lying)** — ⚠ same plumbing
  finding as Rung 3: **there is no `[sled_input]` table** (RT-B P1-7), so this too is either an
  `app/`-side constant or a deliberately created table. MEASURED
  and cheap: `app/main.cpp:8244` pushes the bar at `2.0*dt` matching the kernel's
  `steer_rate_per_s = 2.0`, while `:8253` releases at `3.0*dt` — **50 % faster than the machine can
  act** — so `cmd_minus_bar_abs_max = 0.3500` on 5 of 9 tapes (D-E-02). It is the same finding as
  PJ-6. Dropped only because the merged ladder already spends a rung on an `app/` steer-command fix
  (Rung 3) and Rung 3 carries a measured crash context this one does not. **Ship it beside Rung 3
  if he wants two input lines in one drive.**
- **feel-first Rung 5, `render::kSledSlipVoice` (the drift you can hear).** MEASURED: mean
  `|track_slip| = 0.522`, |slip| > 0.3 on **53.03 %** of ticks, and **33.77 %** of ground-moving
  ticks have |slip| > 0.3 with `roost_flux` < 0.02 — the track is spinning with neither plume nor
  sound, and `track_slip` has **zero consumers** (D-D-F3, PJ read E). Dropped because Rung 6 takes
  the one output slot with unanimous judge backing, and because RC-3's actual demand is the L2
  **measurement** leg (§5), not a new audio layer. Second in line if he wants the drift legible.
- **feel-first Rung 7 / physics-honest Rung 8, `render::kSledRpmBlend`.** MEASURED: the coast is
  **8.60 %** of every drive at 19.34 m/s with the engine at 4 147 rpm while the voice sings **5.8
  semitones flat**, because `app/main.cpp:11820` drives the synth off the **thumb**. Real, but it is
  the weakest tie to any sentence of his and it was the only rung duplicated across two ladders with
  nothing added.
- **player-journey PJ-7, `[sled_comfort] rolled_release_frac` (the flag with no hysteresis).** Two of
  three judges said take its **method, not the rung**. Kept as a **free read-only leg** (§5) and a
  **ruling** (R3). Its mechanism stands: `sim/sled.cpp:254` is a single
  `dot(body_up, up_cg) < cos(1.31)` edge — **75.06°, no release threshold** — while
  `config/scenario.toml:2124` says the C1 hull rests a downed machine *"~65-75 deg"*, i.e. the
  threshold sits **inside the resting band**.

**Named omissions folded in this pass (real, in scope of some rung's mechanism, deliberately NOT
built here — named so no reader mistakes them for scope):**

- **A `w_contact` assist floor** (`docs` §9.7 item 3). It is the *other* half of the pinned
  lean-carve debt the test's own printf names (*"back under 0.9000 via 9.7 items 2+3"*), alongside
  `traction_mu` (Rung 5). **No rung proposes it**; it is named here so the 0.90 debt is not read as
  paid by Rung 5 alone. (RT-A P0-3.)
- **`right_dir_eps = 0.04`** (`config/scenario.toml:2102`, confirmed) — a **2.29°** dead-cone against
  a documented 10°, so the self-right's direction term is effectively `sgn()`. It aggravates Rung 7's
  case and **Rung 7 does not touch it**; it belongs to ruling R6 or its own rung. (RT-A P2-5.)
- **`world/props.cpp`'s unblended `nearest()`** — the drawn corridor edge against the driven one
  under a non-zero `class_blend_m`. Owned by the road-repair lane, named as a risk on Rung 1, not
  fixed here. (RT-A P1-2.)
- **`right_assist_min_tilt_rad`** — a required TOML key `sim/sled.cpp` never reads. A decoy, not a
  dial. Reported, not removed (removing it is a `config/` + loader edit this audit forbids).
  (RT-B P2-16.)

**Refused outright, with the reason (carried forward from all three ladders):**

- **physics-honest Rung 1, `[sled_comfort] lat_mu_scale`.** Best-derived rung in the whole audit and
  **the only one that can lose the feel he signed** — it spends cornering grip to buy roll
  resistance against an unpaid felt item 3 (*"turning too unstable, can't hold a carve"*), its own
  text says *"this trade is a ruling, not a measurement"*, M12 measured no carveable steer angle on
  Road at any speed, and its target band is a **splice of car-tyre lateral and snowmobile
  longitudinal** data with no measured snowmobile lateral-g number in hand. **It is ruling R5's
  ladder to show him and the named successor to Rung 1, not a build.**
- **physics-honest Rung 6, `k_air_shift`.** It hands LEAN a second airborne meaning that his
  2026-08-26 correction separated on purpose. Ruling R6 already puts the question to him in the right
  form. (Judge note this round: its unrun killer was closed — `sim/sled.cpp:463-468` shows
  `exch_l_now` is the rider's reduced mass, 87.5·243.5/331 = 64.4 kg, crossed with his own relative
  position and velocity, zero hands-off, so it is **not** a free impulse and not a governor in
  disguise.)
- **`roll_damp_nms` (800).** The biggest arcade-lawful lever in the kernel, and refused because
  **he ruled it** (2026-08-13, `b971d14a9`) and `sim/sled.h:362-365` says what it costs in its own
  words: *"damping is the dial that kills **the flick, the drift's body language** and backflip
  initiation off a lip."* The arc-vs-flick ladder (Bush lean-in carve **323 m @ 0 / 143 @ 150 /
  42 @ 400 / 30 @ 800**, MEASURED) is ruling R5, not a rung.
- **`track_pitch_half_m` (0.20).** Identity 0.0 is the pre-S2b single-point track, MEASURED to run
  the wheelie away to 89.9° at t = 6 s and never come back (`sim/sled.h:576-581`). Narrow green
  window. Not a place to buy the wheelie back.
- **`steer_hold_frac` / a real bar hold (D-E's E2).** Carries D-E's own **do-not-ship-alone**
  warning: with no carveable angle on Road, a hold makes the rollover *more repeatable*. It waits on
  a plant-side rung — which is physics-honest Rung 1, above, and it is a ruling.
- **`lean_px_full` (300).** D-E-05's correlation is n = 9 and **does not reach p < 0.05**
  (r = +0.815, partial +0.693 against a 0.707 critical value), and his mouse CPI is unknown (R8).
- **A landing-damper knee (physics-honest Rung 2 / PJ-8).** The corrected mechanism survives — the
  landing spike is the damper, linear at **3.33 g per m/s of sink**, no blow-off,
  `sim/sled.cpp:705`, and §1(d) is built on it — but R4's whole force arithmetic was **26.4 % low**
  (`R4-landings.refute.md` R-1) and Chad has **never complained about landing harshness**; he asked
  for *"punchy"*, and the buck he signed reads `a_body`. **Not a rung until there is a word.**

---

## 4. RULINGS CHAD MUST GIVE FIRST

⚠ **FOLDED (RT-A P1-4): the blanket "nothing should be built before R1–R3" is replaced by a
per-rung precondition column.** A blanket gate hid which rung actually waits on what, and two rungs
wait on things R1–R3 never ask.

| Rung | Hard preconditions before a line is written |
|---|---|
| **1** — the seam | **R2** (his A/B ruling) · leg **X1** · route through the lane that owns `world/snowpack.*` |
| **2** — lean buys the arc | **Rung 1's ruling** (same drive) · `lean_bite_gain > 0` |
| **3** — R hands her back | **leg X3** (free, tonight) — *if X3 shows held keys the rung comes off the ladder* · **R3** for the throttle half |
| **4** — the second roll | **R4** (is a backflip a rollover — the invariant has no denominator without it) · **R11** (new) |
| **5** — the track's ceiling | **R2**'s drive (rides along) · the six-touch plumbing |
| **6** — show him the hand | **R1** (a word on tapes 86–91) · a ruling that a physics-inert `sim/` field is acceptable for a readout |
| **7** — tipping relative to the hill | **R7**-class word on changing the self-right gate · leg **L3** (the hill leg it removes authority on) |
| **8** — over the back | **R7** · leg **L6** (the backflip precondition) · leg **L3** |
| *all* | **R1**, and **L2 before/after** as a landing condition (§5) |

**⚠ Ruling-id collision, fixed (RT-A P1-4):** Rung 1's *Standing* paragraph previously called its
ruling **R1** while this section numbers it **R2**. This section is authoritative: **R1 = give the
last six drives a word; R2 = `class_blend_m`.**

**R1 — Give the last six drives a word.** D-C O14 / R3 UNVERIFIED-1: **there is no felt report for
tapes 86–91** (2026-09-17, 20:11–21:39). Every felt attribution older than five weeks rests on a
512 s tape of a binary that did not contain the GI3 rollfix, and every rollover complaint on file
predates a kernel that has since measurably changed. **This is the cheapest and highest-value thing
available, and until you say one line per tape no number in this audit licenses a dial move.** One
line each is enough: "good", "rolled too much", "didn't notice". *(All three ladders make this their
ruling 1; judge 1 named it the finding that could re-score the entire verdict.)*

**R2 — `[snowpack] class_blend_m`, 0.0 vs 1.0: rule it.** `LANES.toml:939` reserved it for you a
week ago — *"it is live-tunable and it is CHAD'S A/B, not the lane's call."* Rung 1 is entirely this
ruling; there is nothing to build, only an A/B to drive. Rung 5 (`traction_mu`) rides the same drive.

**R3 — Should the machine still cut the throttle the whole time she is on her side?**
`sim/sled.cpp:292-293` zeroes the throttle whenever `rolled` is set — **5.74 % of your v17 ride
time** — and `app/main.cpp:7907-7910` (`sting_stance_ok = (Afoot && man_upright) || (Sled &&
!sled.rolled)`) gates the drone on the same flag. Between five and six percent of your ride is time
in which **neither the machine nor the weapon answers**, against *"DOnt make it impossibly hard,
there is a batttle going on as well."* A second half of the same ruling: that flag is a single hard
edge at **75.06°** with no release hysteresis (`sim/sled.cpp:254`) while the C1 hull rests a downed
machine at *"~65-75 deg"* — should it release later than it latches?

**R4 — Is a backflip a rollover?** §0's first sentence is *"Yep I can do backflips."* Every
roll-resistance number on every ladder counts **attitude, not intent**, and no instrument in 206
minutes of tape separates a deliberate send from a crash. Without your definition, **Rung 4's
two-sided invariant has no denominator** — the "must not fall below ~2/min" floor is counting your
backflips as failures.

**R5 — The drift ladder, and the grip trade.** Before anyone touches `roll_damp_nms`, look at the
MEASURED arc-vs-flick ladder (Bush lean-in carve **323 m @ 0 / 143 @ 150 / 42 @ 400 / 30 @ 800**)
and say whether 800 is still the trade you want. **This ladder proposes no move.** Attached to the
same ruling: physics-honest's `lat_mu_scale` would spend cornering grip to buy roll resistance
(scale 0.708 TrailTributary / 0.728 TrailMain / 0.850 Road / 0.927 Bush to reach the tip) — that is
the named successor to Rung 1 and it needs your word, because it is the one move that can cost you
the sliding you signed.

**R6 — Do you want to steer in the air at all?** Three dials are written, commented and shipping at
zero (`k_gyro`, `k_gyro_react`, `k_air_shift`), and `k_gyro_react` is what Rainbow ships by name as
**Reflex Gyro** in *MX vs ATV* (LITERATURE). But today the kernel can only give the **nose-up** half:
`sim/sled.cpp:1354` is `rep_belt = std::max(dv * v_cmd_b, std::max(v_bf, 0.0));` — **`brake` does not
appear**, verified by two judges independently — so throttle gives nose-up and brake structurally
**cannot** give nose-down at any dial value, which is the half that makes a landing worse.
**If you rule yes, `[sled_comfort] belt_brake_frac` (identity 0.0, readout-only, `rep_belt` feeds
only `rep_rpm`, `L_raw` and the `belt_speed_ms` readout) comes FIRST.** Ask the ruling with that
precondition attached, not after it. A separate half of the same ruling: the STAND self-right has
**no contact gate at all** (≈1244 °/s of free airborne roll on a held press, DERIVED) — should it
require ground?

**R7 — `pitch_arrest_nms`: do you want a floor under the flip at all?** Rung 8 is the only rung that
adds a new kernel term. The 60° gate is argued out of your own measured catwalk bimodality so the
30° wheelie survives by construction — but it is a new torque, and a new torque is the shape a
governor takes. Your word before a line is written.

**R10 — The gi4 handoff's 4106 N (new, folded RT-A P2-2; a report, not a question).** Charter §2:
a finding against a signed number is **reported to Chad, never re-derived**. `docs/gi4_ride_handoff.md`
§2 computed `(331 + 87.5) × 9.81 = 4106 N`, but `sim/sled.h:543` reads `mass_kg = 331.0  // machine
+ rider, TOTAL`. **The bytes are on 3246.0 N.** Nothing in this audit was left quoting 4106, and
§1(d)'s roll-torque denominator is fixed at **933.1 N·m**. Your word is only needed on whether the
old document gets corrected.

**R11 — "Self-righting by chance more": with a press, or without one? (new, folded RT-A P1-5.)**
Your two sentences, side by side: *"Leaning shall enhance the ride an just make it more stable and
**slef righting by chance more**"* (§0b) against *"**STAND alone rights the tipped sled**"*
(2026-09-03) and *"IF ON THE SEAT AFTER A ROLLOVER **PRESSING THE STAND BUTTON**"*
(`config/scenario.toml:2053-2056`). Rung 4's C2 term rights the machine **with no press at all**,
and its `wo` weight makes it **strongest on a machine that has stopped moving** — the population
furthest from *"by chance"*. **Rung 4 is hard-gated on this.**

**R8 — Your mouse.** CPI and Windows pointer sensitivity. D-E's *"600 raw counts full-scale = ~19 mm
of desk travel"* assumes 800 CPI and is a **GUESS** in that factor. No lean-scale number can be
picked without it.

**R9 — Do you want to be able to lean and aim at the same time?** ⚠ **No rung was written, because
the law question comes first.** The Sting launches from the seat (`app/main.cpp:7907-7910`, your
2026-09-04 ruling) and while the launcher is shouldered the mouse drives `sting_aim_az/el` and **the
lean freezes wherever it was** (`app/main.cpp:6336 / 6363 / 6414`; `lean_return_tau_s` is **0.0 in
all 84 tape headers**, so there is no return). You hold `|lean| > 0.98` for 15–44 % of your ride
time, so the frozen body is usually a leaned-over one. Three answers: (a) leave it frozen; (b) let
the lean fall to centre while shouldered — **which is the app moving the rider without you, i.e. the
§0b governor line, and needs your explicit word**; (c) re-bind the aim off the mouse.

---

## 5. LEGS OWED BEFORE ANY OF THIS IS BUILT
*(GRAFT G5/G6: physics-honest's probe legs made preconditions, and player-journey's free read-only
method attached to the rungs that lacked one. **None has been run.**)*

**Free, read-only, no build — run these first:**

- **X1 — the exit-surface attribution mutation (D-B-6's own, owed by all three ladders, run by
  none).** One pass over `tape_summary.json` re-attributing each rollover to the surface at episode
  **exit**. **It can kill Rung 1's tape support before a line is written**, and it is the decisive
  test named in every ladder. *Attach to Rung 1.*
- **X3 — the held-key test (new, folded RT-A P1-3; Rung 3's PRECONDITION).** `app/main.cpp:8244-8256`
  decays `sled_steer_cmd` at `3.0·dt` **unconditionally** when no key is held, and the tape writes
  `in.steer` (`test/harness/sled_tape.h:198-201`) beside the pinned `steer_actual` (`:105`). Read the
  **`in.steer` decay signature over the 1.0 s after each of the six `O` records**: flat at ±1.0 = a
  **held key** and Rung 3 is decoration; a 3.0/s ramp to zero in 0.334 s = a **stale command** and
  Rung 3 is real. Free, read-only, one pass. *Attach to Rung 3, and do not spend a drive on that rung
  until it has run.*
- **X2 — the `rolled` edge counter** (PJ-7's first leg). Add it to `tools/sled_tape_audit.py` and run
  it over the 84 tapes. If no tape shows a machine resting in the 65–75° band with a chattering flag,
  that rung is dead before a kernel line is written. **Epistemics that must travel with it:** the
  tape pins at 120 Hz while the kernel runs the test at 1440 Hz, so **a null result does not CLEAR
  the mechanism — it only fails to find it.** *Attach to R3.*

**Probe legs (`tools/sled_probe.cpp`, built in `D:/seads_sandboxes/sled-audit/build` only):**

- **L1 — RC-1's only possible trial:** hands-off full lock on TrailMain at v ∈ {8, 12, 16, 20, 25}.
  **Chad never drives hands-off, so 206 minutes of tape contain no such trial** (D-B §6). The
  plant-side ruling (R5) cannot be sized without it.
- **L2 — the drift, before and after.** RC-3's explicit demand: *"measure it before touching
  anything, and re-measure after — a probe leg, not a hope."* **Made a landing condition on every
  rung on this ladder**, not on one rung — the merged ladder refuses to tune the drift, which makes
  measuring it the only proof it was kept.
  ⚠ **FOLDED (RT-A P2-3): RC-5 requires the before/after of THIS SECTION'S WHOLE ENVELOPE for any
  change, not L2 alone.** The §5 before/after table — L1, L2, **L3**, D-1 — is hereby a **landing
  condition on every rung**, and a rung that moves a leg it never named has not landed.
- **D-1 — pack absorption during a real impact.** Instrument `sink_m[Track]` and the spring/pack
  force split over a 1.7 s send onto Bush vs Road. **The largest single unmeasured number in the
  audit**; it decides whether §1(d)'s damper arithmetic ever sees the shaft speeds it assumes, and
  therefore whether leaving the landing damper alone is right.
- **The steer-inversion causation leg (D-B-5).** Commanded steer sweep at fixed speed. The tape gives
  the correlation; the confound (the 0.9–1.0 cells are 4–8× oversampled because he reaches for full
  lock *when the machine is already not turning*) means only the probe can establish direction.
- **L3 — the hill-traverse leg (folded, RT-A P2-3; the charter's own, and it was missing).** The SF1
  side-slope at a fixed line, **lean uphill vs none** — the only instrument for §0's *"say up a
  hill"*, and the charter names it. **Rung 7 REMOVES hill-time righting authority and no leg on the
  old ladder could tell whether the hill got worse.** *Precondition of Rungs 7 and 8; before/after on
  both.*
- **L6 — the lip trace (new, folded RT-A P0-1; Rung 8's PRECONDITION).** Pitch angle, `ω_x`,
  `normal_sum` and `w_contact` over the **0.5 s of contact before the St. Charles takeoff**. Rung 8's
  bit-identity claim for the backflip rests on `w_contact` being zero where the band opens — but
  `w_contact` is non-zero in the contact ticks that **set the airborne rotation rate**. If the
  maximum pitch reached while `w_contact > 0` exceeds the band opening, **Rung 8 is dead as written**.
  Charter §1 demands the backflip be *measured*, not proved by construction.
- **The clean assist A/B (R2 §14 item 11).** Replay through `seads_sled_probe` at `right_assist_nm`
  0.0 vs 2400.0 and compare episode-duration histograms. **No dial may move on the confounded
  before/after alone.**

---

## 6. WHAT THIS LADDER REFUSES TO CLAIM

1. **No rung here has been built, measured in a probe, or flown.** Every "invariant" is a test to
   run, not a result. No probe was built and no tape was written; every number is read from source
   in this worktree or from a tape Chad recorded, opened read-only.
2. **That any of this is FELT.** RC-5 stands: *"THE FEEL IS SIGNED."* Picking a value is his seat.
3. **That the ranking survives ruling R1.** There is no felt report for the six drives of
   2026-09-17 and every roll complaint on file predates the GI3 rollfix. **If his word on tapes
   86–91 is "she's fine now", this ladder is re-scored against a different sentence and the order
   changes.** This is the largest single risk in the audit and it is a ruling, not a measurement.
4. **That the rollover rate is a kernel ranking.** v13g 6.30/min → v15 2.16 → v17 4.32 is confounded
   at least four ways, all measured (surface mix, stand duty, speed, and an input-encoding change
   between tape 40 and tape 48). **No kernel verdict may be read off that table** (D-B-11).
5. **That §1(b)'s "available by construction" has been observed.** DERIVED from the shipped table and
   geometry, corroborated by the 12/12 sign test, but **X1 is not run**. Until it is, read "on the
   trail" as "**at** the trail". Rung 1 is written to survive either way; nothing else leans on it.
6. **That the self-right ever fires in the air.** DERIVED from code structure only;
   `right_assist_nm_now`, `right_shift_cmd` and `right_charge` are absent from the tape's pin
   roster, so **no tape in the corpus can be asked** about the *torque* (D-A §8 item 2). ⚠ **Folded
   (RT-B P1-9): its GATE is a different matter and WAS askable** — `in.stand`, `ground_speed_ms`,
   `orientation` and `hull_engage_lp` are all pinned; the old "probe-only" label on Rung 7 was a
   measurement this ladder declined to take, and it has now been taken.
7. **That the winter lateral-friction band is a snowmobile number.** It is a splice of car-tyre
   lateral and snowmobile longitudinal data; the SAE 2015 cornering table is **paywalled and was not
   obtained**. R5's *relationship* is exact; its *target value* is LITERATURE-thin.
8. **The R1-10 "slides before it rolls" reading is NOT used** (refuted, §1(b)). **The R1-19 friction
   circle is NOT used** — there is none in this kernel (§1(e)). **The "29 % land upright = a failed
   jump" reading is NOT used** — 93 % of counted air events are sub-second hops (hang p50 0.39 s;
   4 of 58 exceed 1.0 s), so those are bump statistics, not jump statistics. **The R3-M8 "combat and
   riding are serialized" framing is NOT used** — refuted; P deploys from the seat, which is why the
   mouse has a *third* claimant (R9).
9. **That the fold's own measurements were re-run by this pass.** The tape numbers folded in
   tonight (Rung 4's `wv == 0` fractions, Rung 7's armed duty, Rung 8's band occupancy) were
   measured by the mechanism red-team over tapes 86–91, read-only, at 120 Hz against a 1 440 Hz
   kernel. **This pass verified their SOURCE claims against the shipped code by hand** —
   `sim/sled.h:772` axis labels, `sim/sled.cpp:1555-1559` `wv`, `:1876-1882` `assist_nm`,
   `config/scenario.toml:2088` / `:2105`, `test/unit/test_sled.cpp:2647`, `sim/walker.cpp:98-107`,
   `sim/sled.cpp:583-594`, `sim/sled.h:518` — and **did not re-run their tape passes.**
10. **Every rung's ranking is a judgement**, not a measurement. Chad gave no priority ordering;
   comfort, fun and feedback are weighed equally per the audit brief, and intent is weighted double
   per the judging law.

---

## 7. FOLD LOG — what the red-teams changed, 2026-09-18

**RT-A = `docs/sled_audit/redteam_law_feel.md`** (LAW-FEEL lens; 3 P0, 5 P1, 5 P2).
**RT-B = `docs/sled_audit/redteam_mechanism.md`** (MECHANISM lens; 2 P0, 12 P1/P2 batched).
Both verdicts: **SOLID_WITH_FIXES → LAND-WITH-FIX.** Every P0 and P1 below is applied.

**Verified by hand this pass, before folding** (read-only, shipped tree at `ed0a43ce8`) — the fold
does not rest on a red-team's word for any load-bearing line:

| Claim folded | Verified at | Result |
|---|---|---|
| Body axes are **X pitch, Y yaw, Z roll** | `sim/sled.h:772` | **CONFIRMED** — Rung 8 was a yaw damper |
| `wv` gates the C2 torque on `side_right_vmin_ms` | `sim/sled.cpp:1555-1559`, `:1606-1607` | **CONFIRMED** — `wv == 0` ⇒ torque 0 at any gain |
| `side_right_vmin_ms = 1.5` in header **and** TOML | `sim/sled.h:518`, `config/scenario.toml:2141` | **CONFIRMED**, no drift |
| `assist_nm` = spring **+** `roll_damp_nms·ω_z` | `sim/sled.cpp:1876-1882` | **CONFIRMED** — the tell's scalar is not the assist |
| Pinned lean-carve assertion is **0.93**, not 0.90 | `test/unit/test_sled.cpp:2647` | **CONFIRMED** — plus the printf's "9.7 items 2+3" debt |
| Self-right speed gate = 5 km/h, hysteretic, low-passed | `config/scenario.toml:2088-2089`, `:2108`; `sim/sled.cpp:1676-1695` | **CONFIRMED** |
| Ramp `right_tilt_lo/hi` owns the gate; `min_tilt_rad` unread | `config/scenario.toml:2094`, `:2105-2106` | **CONFIRMED** (TOML says so itself) |
| `surf_mix` has a **second** consumer, the walker | `sim/walker.cpp:98-107` | **CONFIRMED** |
| `align_m` exists only under `lean_bite_gain > 0` | `sim/sled.cpp:583-594` | **CONFIRMED** — Rung 2 is in series with it |
| `plane_lat_gain` ships **0.3** (Rung 2 not cosmetic) | `sim/sled.h:1055`, `sim/sled.cpp:1083` | **CONFIRMED** |
| `[sled_input]` does not exist; one TOML→params bridge | `grep -rn sled_input config/ app/`; `config/load_scenario.h:46`; `app/main.cpp:2481` | **CONFIRMED** |
| Rung 4's "(700, 2.5) best-of-9 / stalls 105-125°" quote | `sim/sled.h:497-512` (**not** `:1035-1049`) | **CITATION CORRECTED** |
| Denominators: `W×arm` = **933.1 N·m**; `W×half-stance` = 1 504.5 | arithmetic on `sim/sled.h:543-569` | **CONFIRMED** — 1 931.8 has no source, struck |

### P0 — applied
- **P0/RT-B-1 · Rung 4: THE DIAL CHANGED.** `side_right_gain_nm` cannot reach 75.0 % of the stall
  window it argues from (`wv == 0`). **New dial `[sled_comfort] side_right_vmin_ms`, identity 1.5**;
  the gain is kept as a *second* dial on the still-sliding population; the anti-magnetism test
  `test/unit/test_sled.cpp:2649` is named as the wall.
- **P0/RT-B-2 · Rung 8: THE AXIS WAS WRONG.** `τ_y = −gain·ω_y` is a **yaw** damper here. Rewritten
  as `torque_body.x += −pitch_arrest_nms · s.angular_vel.x · w_contact · w_band`, gated by a branch
  on `> 0.0`; **`|ω_y|` bit-identity added as the invariant** that proves it did not land on the drift.
- **P0/RT-A-1 · Rung 8: the backflip is asserted, not measured.** `w_contact` is non-zero in the
  contact ticks before takeoff. **Probe leg L6 added as a hard precondition**, plus the invariant
  clause *"max pitch while `w_contact > 0` on a send must sit below the band opening"*; if it does
  not, **the rung is dead as written**.
- **P0/RT-A-2 · Rung 6: the tell's scalar is not the assist.** Spring ≤ 450 N·m by construction;
  everything above is `800·ω_z`, so the raw tell is a **roll-rate meter** that brightest on his
  flick. The tell is re-specified on the **spring component**, which needs a new `sim::SledState`
  field — **the "render/ and app/ only" law check is struck as FALSE**, and the rung carries a
  conditional drop if that `sim/` touch is refused. Magnitudes restated.
- **P0/RT-A-3 · Rung 2: the "≤ 0.90 pinned gate leg" clause is STRUCK.** The shipped assertion is
  0.93 and the leg passes at 0.9003; the clause silently demanded a `test/` edit. The 0.90 **debt**
  moved to Rung 5 (its owner by the test's own printf) and the `w_contact` assist floor is named in
  §3's omissions.

### P1 — applied
- **RT-A-1 / RT-B-8,9 · Rung 7's tape evidence was FALSE and is replaced.** The quoted STAND duties
  are all above the 5 km/h gate and arm nothing. The corrected, MEASURED armed duty is **2.32 %**
  (an ~11× overstatement), and the rung gains **real** tape support: 1 022 armed-but-hull-free ticks,
  839 of them on a zero-rollover tape. RT-A's "no tape evidence — probe-only" is itself superseded by
  RT-B's measurement; **the rung keeps its rank.**
- **RT-B-10 · Rung 7 must not blend toward `|phi_surf|`** (a surface ROLL, which steps at 90° of
  pitch and would make a nose-down crash REFUSE to right). Reference changed to
  `acos(dot(up_body, n_surf))`, written as a **branch**, not a 0-weight `mix`.
- **RT-A-2 · Rung 1's blast radius is two consumers**: `sim/walker.cpp:98-107` puts the man on foot
  in scope at every corridor edge. Law check, drive checklist and routing all updated; `props.cpp`
  named as a risk; the A/B routed to the lane that owns `world/snowpack.*`.
- **RT-A-3 · Rung 3's decoration question is answerable on tape for free** → **leg X3** added as a
  precondition; the rung's rank does not stand until it runs.
- **RT-A-4 · the blanket "build nothing before R1–R3" is replaced by a per-rung precondition table**
  (§4), and the **R1/R2 ruling-id collision** in Rung 1's Standing is fixed.
- **RT-A-5 · Rung 4 gets ruling R11**, quoting *"slef righting by chance more"* against *"PRESSING
  THE STAND BUTTON"*; the `wref`/window dials move out of its killing mutation into that ruling.
- **RT-B-3,4 · Rung 8's "no populated band" argument is DELETED** (156 ticks in +20…+60° against 209
  past +60°) — the wheelie is protected by the **opening at 60°**, not by emptiness — and the band is
  specified **SIGNED, nose-up only** (the nose-down half holds 3 010 ticks, 19× as many).
- **RT-B-5,6 · Rung 2**: `align_m` only exists under `lean_bite_gain > 0` (added to the invariant),
  and the decorative killer (*"the non-leaned radius must not move"* — guaranteed) is replaced by
  three that can fail: wrong-way lean, track lateral, straight running.
- **RT-B-7 · "one loader line" is wrong everywhere.** `[sled_input]` does not exist; the real
  six-touch file list is stated in §2 and Rungs 2 and 5 are re-labelled **`sim/` edits**.
- **RT-B-11,12 · the spine carried three roll-torque denominators.** Fixed on **933.1 N·m**; §1(d)
  and Rung 6 restated against it (Rung 6's p95 is **82 %**, not 51 %), and §1(d)'s silent
  `slip = 0.300 rad` assumption is labelled **GUESS** and replaced with a five-row table attached to
  probe leg D-1.

### P2 — applied
RT-A P2-1 (Rung 4 citation `sim/sled.h:497-512`) · RT-A P2-2 (**ruling R10**, the gi4 handoff's
4106 N) · RT-A P2-3 (**leg L3** added; the §5 envelope's before/after is a landing condition on
every rung) · RT-A P2-4 (Rung 8's band named as a second dial `pitch_arrest_open_deg`, identity
180.0; Rung 6's band promoted to THE dial, `kAssistTell` demoted to a display gain) · RT-A P2-5
(`right_dir_eps` moved out of Rung 7 into §3) · RT-B P2-13 (§1(b) qualified: table μ is delivered
only at slip ≳ 0.3 rad; the countervailing `lean_bite_gain` term named) · RT-B P2-14 (the tell must
be **additive** behind `if (kAssistTell > 0.0f)`) · RT-B P2-15 (a sinkable dial still steps at the
edge at any `class_blend_m`) · RT-B P2-16 (`right_assist_min_tilt_rad` is an unread required key) ·
RT-B P2-17 (`hands_on` is the second reason the throttle is zero, and `grip.attached` is unpinned) ·
RT-B P2-18 (`rider_mass_kg` never enters the machine's **weight**, but does enter forces at
`sim/sled.cpp:459/464`) · RT-B P2-19 (Rung 8 gated by a branch, not an add-of-zero).

### Rungs dropped
**None outright.** Both red-teams' per-rung verdicts were LAND or LAND-WITH-FIX; the two
DO-NOT-LAND-as-written rungs (4 and 8) were *corrected*, which is what the fixes asked for. What
changed instead is that **three rungs now carry a stated death condition** that a free or cheap leg
can trigger:

| Rung | Dies if |
|---|---|
| **3** | leg **X3** shows a held steer key through the `O` records — then it is decoration |
| **6** | a physics-inert `sim::SledState` field is refused for a readout — then drop it rather than rebuild it on the roll-rate scalar |
| **8** | leg **L6** shows contact-phase pitch above the band opening on a send — then dead as written |

**Ranking after the fold: UNCHANGED, 1–8.** RT-A argued Rung 7 down (zero tape support) and RT-B
then *measured* tape support for it, so the two findings cancel. Rung 3's rank-3 is the one position
now explicitly **provisional**, on X3.

### What the fold did NOT do
No dial was moved, no config edited, no test or golden touched, nothing built, no probe run, no
`ctest`, no `seads.exe`, nothing pushed. The only file this pass wrote is this one. **Ruling R1
still stands above everything**: there is still no felt word from Chad on tapes 86–91, which are the
six drives every measured number in this document and in both red-teams rests on.

---

*Merged ladder, sled ride audit, 2026-09-18. Base angle FEEL-FIRST; grafts from PHYSICS-HONEST
(§1 spine, Rungs 5 and 7, the R6 ordering constraint, the probe legs) and PLAYER-JOURNEY (Rung 3's
throttle half, Rung 6 whole, Rung 1's two-sided invariant, rulings R3 and R9). Read-only against
main. No dial changed, no config edited, no goldens moved, nothing built, nothing pushed. The only
file this pass wrote is this one.*
