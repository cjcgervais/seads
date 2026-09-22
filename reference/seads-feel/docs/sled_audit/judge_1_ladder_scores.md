# JUDGE 1 of 3 — ladder scoring against ROLL_COMFORT_HANDOFF §0 / §0b

Read-only. Nothing built, no dial moved, no config edited, no test run, no probe, nothing
pushed. The only file this judge wrote is this one.

**Scoring law.** Seven criteria 0-10. `intent_heard` carries **weight 2** (the brief's "top
weight"); the other six carry weight 1. `total = (2*intent + roll + lean + drift + fun +
nogov + measurable) / 8`, reported on the 0-10 scale. Chad gave no ranking among comfort /
fun / feedback, so those three thirds of the charter are weighed **equally** — a pure output
channel is not discounted for being an output channel.

**The bar, verbatim** (`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0 and §0b):

> "I rule that it should be roll resistant." · "allow me to land on my skis more often after a
> roll (even though R works). But not every time — allow it to happen." · "drift around corners
> and then with throttle, straighten out. The feel is really good. Don't lose the feel, except
> the constant rolling." · "Make it possible to roll but not the rule." · "Just allow the
> balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be the
> necessary condition of not rolling over." · "Work on the math to achieve a balance of fun and
> accuracy to real physics."

§0b: "It will ruin the feel to have a governor. **Make it less honest but dont ruin it.** …
the measure of it shall be **if my intent is heard**. Leaning shall enhance the ride an just
make it more stable and slef righting by chance more. … **SLiding banging, punchy, jumps. Just
make it more stable.**" · "**Allow for bad driving too but keep the benefit there for good
riding. DOnt make it impossibly hard, there is a batttle going on as well.**"

---

## 0. WHAT I VERIFIED MYSELF BEFORE SCORING

Every claim below was read out of the shipped source in this worktree this session, not
relayed. Each is a claim that **separates** the ladders, so a judge who did not check it would
be ranking prose.

| # | claim, and whose ladder leans on it | verdict | killing mutation |
|---|---|---|---|
| V1 | physics-honest §1(b): the shipped ski `mu_lat` table exceeds the kernel's own SSF on six of seven surfaces | **CONFIRMED** at `sim/sled.cpp:162-193`: Bush 0.55 · TrailMain 0.70 · TrailTributary 0.72 · Road 0.60 · RockOutcrop 0.60 · MineWorks 0.58 · LakeIce 0.22 | any row reading below 0.5097; or the rollover proving normal-load-starved in the L1 probe |
| V2 | physics-honest §1(e) / Rung 5: no friction circle — lateral is `normal * mu_l` alone | **CONFIRMED** at `sim/sled.cpp:1056-1059`: `bite = -normal * mu_l * tanh(slip_ang / slip_ref_rad)`, no thrust term in scope | find any term in the lateral expression reading the Mohr-Coulomb budget |
| V3 | physics-honest Rung 7: `brake` does not appear in the belt expression | **CONFIRMED** at `sim/sled.cpp:1354`: `rep_belt = max(dv * v_cmd_b, max(v_bf, 0.0))` | `brake` appearing anywhere in the coast floor |
| V4 | feel-first Rung 8 **and** PJ-1: the R block does not recentre the bars | **CONFIRMED** — the R block zeroes `sled_lean_lat` and `sled_lean_fwd` only; `app/main.cpp:8263-8266` (the C key) zeroes all three incl. `sled_steer_cmd  // SK-1c: C recentres the bars too` | `sled_steer_cmd = 0.0` appearing inside the R block |
| V5 | PJ-1's second half: the throttle is held at zero *by the roll* and released the instant R clears it | **CONFIRMED** at `sim/sled.cpp:293-294`: `const double throttle = (s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle);` | the throttle gate reading anything but `s.rolled` |
| V6 | PJ-7: the battle is gated on the roll flag | **CONFIRMED** at `app/main.cpp:7907-7910`: `sting_stance_ok = (Afoot && man_upright) || (Sled && !sled.rolled)` | the predicate widening off `sled.rolled` |
| V7 | feel-first Rung 4 / PJ-6: the app centres the bars 50 % faster than the kernel can slew them | **CONFIRMED** — `app/main.cpp:8253` `const double back = 3.0 * frame_dt;` against `steer_rate_per_s = 2.0` | the app rate matching the kernel rate already |
| V8 | feel-first Rung 1 / PJ-2: the class blend is structurally dead on main | **CONFIRMED** — `config/world.toml:645` `class_blend_m = 0.0` | the blend branch being taken at 0.0 |
| V9 | feel-first Rung 3: `plane_lat_lean_gain` exists, is applied to ski plate only, and ships 0.0 | **CONFIRMED** — `sim/sled.h:1061` `= 0.0`; applied once at `sim/sled.cpp:1102` `lat *= 1.0 + p.plane_lat_lean_gain * align_m` | the term multiplying a track patch, or a second application site |

Nine for nine. **No ladder in this set is quoting source it did not read**, which is worth
saying plainly, because it means the ranking below is a ranking of *judgement*, not of
accuracy.

---

## 1. SCORES

| ladder | intent (x2) | roll-resistant not impossible | lean = enhancer | drift canon kept | fun / arcade | no governor | measurable | **TOTAL** |
|---|---|---|---|---|---|---|---|---|
| **FEEL-FIRST** | **10** | 8 | **10** | **9** | **9** | 9 | 9 | **9.25** |
| PHYSICS-HONEST | 7 | **9** | 4 | 8 | 7 | **10** | **10** | **7.75** |
| player-journey | 8 | 6 | 3 | 8 | **9** | **10** | 9 | **7.63** |

### FEEL-FIRST — 9.25

**intent_heard 10.** It is the only ladder organised around §0b's own sentence rather than
around a discipline. Its §0 scoreboard walks each of his clauses and marks it yes/no
("roll resistant — no · sliding/banging/punchy/jumps — **yes** · Don't lose the feel — **yes**
· Not impossibly hard — no"), which is exactly the form "the measure of it shall be if my
intent is heard" asks for. Seven of eight rungs are a sentence of his made into a dial. The
refusals are as intent-driven as the rungs: it declines `roll_damp_nms` **because the kernel's
own comment calls it "the flick, the drift's body language,"** and it declines
`steer_hold_frac` because M12 measured no carveable angle on Road, so a hold alone buys a more
repeatable rollover. Deductions I looked for and did not find: no rung sells him a
sim-fidelity argument he rejected, and no rung moves a number he signed.

**roll_resistant_not_impossible 8.** Rung 1 (`class_blend_m`) is the largest *measured* roll
effect anywhere in the three ladders (bankgraze yaw -482.7° → -11.6°, roll 166.6° → 52.7°,
ROLLED → no rollover) and Rung 2 attacks the second half of the ruling — "land on my skis more
often" — at the one term written for it. Rung 2's invariant is the best-written "not
impossible" fence in the set: `never_recovered` must fall **while
`rollovers_past90_per_min` does not fall below ~2/min**, i.e. it fails itself if the rolls
stop. Held short of 9 because it does not touch V1 — the friction-rollover-by-construction
that physics-honest derived — so if D-B-6's unrun exit-surface mutation kills the trail
attribution, Rung 1 loses its tape support and the ladder's roll answer thins to Rung 2.

**lean_enhancer 10.** Rung 3 is the single best rung in all twenty-four. It is §0's literal
sentence ("ENHANCE ability, i.e. tighten a turn"), it finds the *right* term
(`plane_lat_lean_gain`, ski plate only) against the wrong one the tree already ships
(`lean_bite_gain 0.18`, which multiplies the **track** too — measured self-defeating: 0.18 →
0.45 took the lean-in carve from 30 m to 295-1037 m, "because the track out-gains the skis"),
it carries the measured radius ladder (33.2/33.5/33.7/33.9 m flat, 30 m leaned), its killing
mutation watches the **non-leaned** radius (if the flat ladder moves, the term is leaking off
`align_m` and is not a lean reward), and it states its own honest limit — inert where
`rho_eff = 0`, so it **cannot** answer "part of what is so fun on the road is driving by lean"
and must not be sold as if it does. That last sentence is why this is a 10 and not a 9.

**drift_canon_kept 9.** RC-3 is protected by *non-touch*: no rung goes near `roll_damp_nms`,
`track_lat_mu` or the thrust path, and Rung 5 makes the drift **audible** instead of tuning it
("it cannot change the ride, only what he can hear of it"). One point off because the ladder
never measures the drift: RC-3's own words are "measure it before touching anything, and
re-measure after — a probe leg, not a hope," and no rung here schedules that leg.

**fun_arcade 9.** Sliding (Rung 5 slip voice), banging and jumps (Rung 6's 60° gate, which
keeps the wheelie by construction because D-B-7 measured *nothing lives between* p95 14.25°
and p99 66.75°), punchy explicitly left alone with a reason — "Chad has never complained about
landing harshness; he asked for *punchy*. Not a rung until there is a word." Backflip fenced
by `w_contact`, which is identically 0 airborne.

**no_governor 9.** Every rung passes. Rung 6 is the one new kernel term, and it is exactly
§0b's own worked example ("a contact-gated roll-stability moment") rotated onto pitch,
damping-only so it cannot ring, and the ladder flags it as its riskiest and says it must not
be built before he rules. One point off only because a new torque, however lawful, is the
shape a governor takes.

**measurable 9.** Identity value on all eight, killing mutation on all eight including
self-undermining ones (Rung 4: "if `cmd_minus_bar_abs_max` stays at 0.35 after the change,
something other than the release rate is producing the divergence and **the finding is
wrong**"), and a §4 refusal list that drops four readings its own sibling strands published
(R1-10, R1-19, R3-M8, "29 % land upright = a failed jump"). Held at 9 because several
invariants are next-drive metrics rather than probe legs, and because Rung 1's decisive
mutation is named and **not run**.

### PHYSICS-HONEST — 7.75

**intent_heard 7.** The frame is in structural tension with the ruling that supersedes
everything: §0b rejected the honest-mechanism law by name, and "Make it real and it gets
stable" is a discipline's answer to a question he asked in feel terms. The ladder knows this
and pays for it honestly — its §0 carries the licence explicitly and files every rung against
"a departure from reality is a defect only when it also costs a named feel or produces *the
constant rolling*" — which is why this is a 7 and not a 4. But two rungs still fail the intent
test on their own terms: **Rung 7 has no felt payoff at all** by its own admission ("this rung
is a precondition, not a payoff"), and Rung 1 is the one rung in the whole set that can *lose
the feel*, which it states plainly ("This trade is a ruling, not a measurement"). Its §4
ruling 1 — **fly the current build and re-rule, because every roll complaint on file is
against a kernel that has since changed** — is the sharpest single paragraph in the entire
audit and pulls the score back up.

**roll_resistant_not_impossible 9.** The only ladder that answers "I rule that it should be
roll resistant" with a **derived mechanism** rather than a correlation: SSF 0.5097 g / 27.01°
against a shipped table of 0.55-0.72 (V1, confirmed), φ = 0.856 putting inside-ski lift at
0.362 g, and the damper table showing a 4 m/s arrival unlocking **2.24× gravity's own peak
tipping torque** in lateral force alone. Four rungs converge on the roll. The "not impossible"
half is explicitly preserved: Rung 1's invariant requires WINTER_LAW §2.4c.1's 25° side
approach at 16 m/s to **still go over**, and it survives because a slope adds `tan θ` with no
μ involved. Not 10 only because Rung 1's whole size is probe-only and the probe was not run.

**lean_enhancer 4.** The biggest gap in the set. §0's most-quoted ask — lean tightens the turn
— gets no rung. Rung 6 routes *airborne* authority through the player's lean, which is real
credit and is why this is not a 2, but it is a different sentence of his, and it hands the
lean axis a second airborne meaning beside THE THROW, which his 2026-08-26 correction
separated on purpose. `plane_lat_lean_gain` does not appear anywhere in the ladder.

**drift_canon_kept 8.** Rung 5 is the most *mechanically* correct drift work in the set — it
identifies that step 2 of the real drift sequence cannot happen (V2, confirmed) and that
whatever produces his signed drift is the heading-rotation effect, not the circle. It is also
the only rung that **touches** the behaviour he signed, and it knows it: "RC-5 bites hardest
here." It fences itself with R2's condition adopted verbatim ("No candidate may land without
that leg") and schedules the L2 before/after probe RC-3 demands. Scored 8 rather than 9
because touching a signed feel — even correctly, even fenced — is a larger drift risk than
feel-first's non-touch.

**fun_arcade 7.** Punchy is protected by deriving Rung 2's knee from the machine's own spring
(3.322 m/s) rather than choosing it, and by pinning trail chop bit-identical below the knee —
that is good work. But the ladder also names the risk it cannot dismiss: the buck reads
`a_body` and is **signed on the PAIR, not the gain**, so a softer spike is a quieter buck.
Rung 7 buys nothing felt. Rung 8 is real fun-and-feedback value. Net: a competent but
stability-weighted ladder in answer to a man who wrote "the point is this should be fun."

**no_governor 10.** Best in set, and not by abstention. Rung 4 **removes** kernel-commanded
rider shift (up to 0.35 m of `right_shift_cmd`) during ordinary slope riding, restoring §0b's
"the player's lean is the only thing that moves the rider" — it is the only rung in twenty-four
that moves the tree *toward* the governor law rather than merely not away from it. Rung 6 is
the same instinct: air authority routed through the player's own mass.

**measurable 10.** The gold standard. Every number recomputed from shipped parameters rather
than quoted; the denominator corrected once, up front, so no rung repeats the 26.5 % mass
error (`mass_kg` 331 → 3246.0 N, not 4106 N); the strand's own central finding **inverted**
because a refutation pass beat it (R1-10 → the V1 table); §5 lists five probe legs owed with
the admission "None was run by this ladder"; and §6 refuses six claims outright, including
"that any of this is FELT" and "that the rollover rate is a kernel ranking" (confounded four
ways, including an input-encoding change between tape 40 and 48). Rung 7's invariant was
verified against the consumer graph before being asserted.

### player-journey — 7.63

**intent_heard 8.** It hears the sentence the other two under-hear — "**DOnt make it impossibly
hard, there is a batttle going on as well**" — and it is the only ladder that traces that to a
mechanism (V6: the drone is gated on the roll flag, so 5.74 % of his ride is time in which
neither the throttle nor the weapon answers). Its RULING 1 and RULING 6 are the right two
questions. Held to 8 because the **top ruling**, "I rule that it should be roll resistant,"
receives one rung that is explicitly not a build (PJ-2: "it is an ask, and it was reserved for
him a week ago") plus a landings rung ranked last, and because §0's lean-enhancer sentence is
answered by *showing him the assist* rather than by making the lean buy anything.

**roll_resistant_not_impossible 6.** PJ-2 shares the best measured cure with feel-first. After
that: PJ-7 is the flag's *release*, PJ-8 is the landing tail, and nothing else aims at the
roll. That is a deliberate, stated choice ("six of eight touch app/ or render/ only"), and it
is defensible under equal weighting — but the criterion asks what the ladder buys on his
first ruling, and the answer is one A/B he already owns.

**lean_enhancer 3.** Lowest in the set. PJ-3 makes the *machine's* help visible, which is a
real answer to "keep the benefit there for good riding" but is not the lean. RULING 4 (lean
vs sting aim, and the observation that option (b) is the §0b governor line and needs his word)
is a genuine find, correctly left as a ruling rather than forced into a rung — but a ruling is
not an enhancer.

**drift_canon_kept 8.** Safe by non-touch; PJ-4 makes slip audible, same instinct as
feel-first Rung 5. No drift measurement scheduled.

**fun_arcade 9.** The best failure-cost work anywhere: a crash is a **chain** (3.25 past-90
crossings per merged episode), it costs p50 7.02 s / p90 18.13 s / max 38.4 s to get back to
speed, and PJ-1 fixes the moment he re-enters the fight. PJ-8 is ranked last **on purpose**
with the reason stated: "it is the only rung on this ladder that could take away something he
asked for by name."

**no_governor 10.** Six of eight rungs cannot add a force at all, and the one place a governor
could enter (RULING 4 option (b), the app letting the lean fall to centre while the launcher is
shouldered) is flagged as the governor line and refused a rung until he rules.

**measurable 9.** The most *new* measurement of the three: read D (every autoright `O` record
and the `T` records around it, byte-streamed) and read E (the consumer grep) are original and
load-bearing, and read D is the finding that changed its own ranking. Its killing mutations
are unusually self-destructive and correct — PJ-1's third mutation concedes "the tape cannot
tell a held A/D key from a stale accumulator … in which case zeroing the command buys one frame
and **the rung is decoration**," and PJ-7 concedes "a null result from the tape does not clear
the mechanism — it only fails to find it" (120 Hz pins against a 1440 Hz test). Held at 9
rather than 10 because PJ-3's and PJ-5's acceptance is "his word" with no number, which is
correct under RC-5 but is not measurable in this criterion's sense.

---

## 2. VERDICT — FEEL-FIRST, and what to graft onto it

**FEEL-FIRST wins on the criterion Chad himself named as the measure.** It is the ladder a man
who wrote "the measure of it shall be if my intent is heard" would recognise as an answer to
his own sentences. Its Rung 3 is the only serious attempt in twenty-four rungs at the one ask
he repeated in both §0 and §0b, and its refusals (`roll_damp_nms`, `track_pitch_half_m`, the
three airborne dials, a landing knee) are refusals *for his reasons*, not for a discipline's.

It is also the ladder with the thinnest mechanism spine and the smallest set of scheduled
probe legs, and both of those holes are filled exactly by the two ladders it beat. The grafts
below are ranked by what they buy the winner.

### G1 — PJ-1's throttle measurement into feel-first **Rung 8** (highest value)
Same dial, but PJ measured what feel-first's Rung 8 missed: across all six autoright `O`
records, **throttle command = 1.00 in 6 of 6** — because `sim/sled.cpp:293-294` (V5, confirmed)
holds the throttle at zero *because she is rolled* and releases it the instant R clears the
flag. Feel-first Rung 8 currently sells "the bars are hard over"; with this it sells "**R hands
him back a machine pointed the wrong way at wide-open throttle**," which is a different and
much worse sentence, and it earns a second invariant. Graft PJ-1's honest third mutation too
(the tape cannot distinguish a held A/D key from a stale accumulator — only his drive settles
it), which feel-first Rung 8 lacks. **This graft probably promotes Rung 8 up the ladder.**

### G2 — physics-honest **Rung 4** (`right_tilt_surf_frac`) promoted from feel-first's R2(b) ruling to a rung
Feel-first carries "its tilt gate reads gravity, not the hill" as half of a question. Physics-
honest turns it into a rung with identity 0.0: blend the self-right's reference from radial up
toward the `phi_surf` already in scope at `sled.cpp:1423`. Take it, for three reasons feel-first
should care about more than physics-honest does. (a) It is the only rung in the set that
**removes** kernel-commanded rider mass movement — §0b's own forbidden thing — so it makes the
ladder's no-governor claim active rather than passive. (b) It answers §0's "better traversing,
**say up a hill**" directly. (c) Its scale is measured, not rhetorical: on a conformal 20.05°
side-hill `w_tilt = 0.9997` (full authority) while `config/world.toml`'s own snowpack block
records terrain slope p95 = 21.1° over 79 847 land samples, and STAND is held hard 6.8-39.0 %
of his drives. Carry physics-honest's own killer with it: if `phi_surf` degenerates to radial
up when fewer than three patches are loaded, the blend is a no-op in exactly the tipped case.

### G3 — physics-honest **§1(a)-(e)**, the derived spine, as feel-first's missing §1
Feel-first has no quantitative account of *why* she rolls; it has eight good rungs and a
correlation. Graft the spine: SSF **0.5097 g / 27.01°** from the trapezoid, against the shipped
`mu_lat` table of **0.55-0.72 on six of seven surfaces** (V1, confirmed by me); φ = 0.856 with
inside-ski lift at 0.362 g; the damper as a lateral multiplier (**2.24×** gravity's peak tipping
torque at a 4 m/s arrival); and the two-unrelated-friction-laws finding (V2). This does not add
a rung — it gives Rung 1 and Rung 6 a *why*, and it gives Chad a number to rule against in
feel-first's own R5. Graft the corrected denominator with it (**3246.0 N**, not 4106 N) so no
rung inherits the 26.5 % error.

### G4 — physics-honest **Rung 3** (`traction_mu`), which feel-first omits entirely
The cheapest rung in the whole set and feel-first does not have it. His ruling is **taken**
("at high speeds, getting pulled in and flipping out like 15x is not desirable so yes take care
of that"), the mechanism is **built**, the sweep is **on the books** (`docs/snowform_measurements.md`
§M8.1: at μ ≥ 0.6 the Bush runaway does not develop; at μ ≥ 3.0 his own traverse is
byte-identical), and it **ships at 0.0**. It also pays a debt feel-first's own Rung 3 names:
`sim/sled.h` records `traction_mu` as one of the two instruments meant to pay felt item 5, both
shipping at zero. Carry physics-honest's honest limit with it — it is **inert on Road by
construction** (`rho_eff = 0`), so it does **not** fix the bank-strike superspin, and anyone
selling it as that is selling the wrong thing.

### G5 — PJ-7's method: a free read-only leg that can kill the rung before a line is written
PJ-7's first leg is "add a `rolled` edge counter to `tools/sled_tape_audit.py` and run it over
the 84 tapes; if no tape shows a machine resting in the 65-75° band with a chattering flag, the
rung is dead before a line of kernel is written." Graft the **method** onto every feel-first
rung that lacks one — starting with Rung 1, whose decisive test (D-B-6's exit-surface
attribution) is named in three documents and **has never been run**, is read-only, and costs
one pass over `tape_summary.json`.

### G6 — physics-honest **§5 probe legs** as preconditions on feel-first's §3
Feel-first's rulings name no probe legs. Graft three: **L2** (the drift before and after —
RC-3's explicit demand, and R2's condition on *every* candidate, which feel-first's Rung 5 does
not currently satisfy); **L1** (hands-off full lock, RC-1's only possible trial — "Chad never
drives hands-off, so 206 minutes of tape contain no such trial"); and **D-1** (pack absorption
during a real impact, the largest single unmeasured number in the audit, which decides whether
feel-first is right to leave the landing damper alone).

### G7 — PJ-3 (`kAssistTell`) as a feel-first render rung
Feel-first's angle is "what the hand feels," and its blind spot is that the thing holding the
machine up on **79.8-93.7 %** of ticks at p95 up to **765 N·m** — half the 1505 N·m tipping
budget — reaches no eye, ear or panel (`assist_nm`'s only consumers tree-wide are two resets in
`app/spawn_policy.h`). It is render-only, identity 0.0 = pixel-identical, and it rides the
instrument he already chose and described himself ("make the grid blue and the ball slag
orange"). Graft PJ-3's own killer as the rung's whole design: the assist's median is 5-75 N·m,
so a tell gated on `assist_nm != 0` is lit almost always and carries no information — **the
band, not the duty, is the rung**, and if no informative band exists the rung is dead.

### G8 — physics-honest Rung 7's **ordering argument** into feel-first's ruling R6
Feel-first's R6 asks "do you want to steer in the air at all?" and notes the belt is the
blocker. Graft the precondition explicitly: `rep_belt` (V3, confirmed — `brake` is not in the
expression) means throttle can give nose-up and brake structurally **cannot** give nose-down at
any dial value, so arming `k_gyro_react` first hands him the half that makes a landing worse.
Ask the ruling with `belt_brake_frac` attached as its precondition, not after it.

### Not grafted, and why
Physics-honest **Rung 1** (`lat_mu_scale`) is the one rung I would keep off the winning ladder.
It is the best-derived rung in the set and it is also the only one that can lose the feel he
signed — its own text says "This trade is a ruling, not a measurement," O5 ("turning too
unstable, can't hold a carve") is already unpaid, and M12 measured no carveable steer angle on
Road at any speed. Its target band is a **splice** of car-tyre lateral and snowmobile
longitudinal data with no measured snowmobile lateral-g number in hand. Keep it as **§3's
ladder to show him** — the shape feel-first already uses for `roll_damp_nms` — not as a rung.

---

## 3. WHAT THIS JUDGE REFUSES TO CLAIM

1. **That any ladder's rung will feel like anything.** None was built, probed or flown.
2. **That the ranking survives ruling 1.** All three ladders independently found the same
   hole: **there is no felt report for the six drives of 2026-09-17**, and every roll complaint
   on file predates the GI3 rollfix. If his word on tapes 86-91 says "she's fine now," the
   intent_heard column is re-scored against a different sentence and this verdict moves.
3. **That the trail attribution is safe.** D-B-6's exit-surface mutation is named in all three
   ladders and **run by none**. Feel-first Rung 1 and PJ-2 both rest on it; feel-first argues it
   survives the mutation either way ("at the trail" vs "on the trail") and that argument is
   plausible, not verified.
4. **That the weighting is his.** intent_heard at weight 2 is the brief's instruction; the
   equal weighting of comfort / fun / feedback is the brief's reading of his silence on
   ranking. He has ruled neither.

---

*Judge 1 of 3, sled ride audit, 2026-09-18, against `ed0a43ce8`. Read-only. No dial changed,
no config edited, no golden moved, no test run, no probe built, nothing pushed. The only file
this judge wrote is this one.*
