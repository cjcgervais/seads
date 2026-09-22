# JUDGE 2 of 3 — ladder scoring against ROLL_COMFORT_HANDOFF §0/§0b

Worktree `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
**READ-ONLY.** No dial moved, no config edited, no golden touched, no ctest run, no probe
built, no `seads.exe` launched, nothing pushed. The only file this judge wrote is this one.

Weighting as given: **intent_heard carries top weight**; the three thirds Chad named
(comfort / fun / feedback) weigh **equal** — he gave no ranking.

---

## 0. WHAT I VERIFIED MYSELF BEFORE SCORING

A ladder's "measurable" score is worth nothing if its mechanism citations are relay. I read
each load-bearing line out of this worktree's shipped source. All confirmed:

| claim | cited by | verified |
|---|---|---|
| ski `mu_lat` 0.55 Bush / 0.70 TrailMain / 0.72 TrailTributary / 0.60 Road / 0.22 LakeIce | physics §1(b), feel-first §4 | **YES** — `sim/sled.cpp:162-193`, read verbatim |
| `class_blend_m = 0.0` shipped, TOML-reachable | feel-first R1, PJ-2 | **YES** — `config/world.toml:645`; `config/load_world.cpp:362` `require(...)` |
| `normal = susp_k*x + susp_c*xdot`, linear, `max(normal,0)` the only bound | physics R2, PJ-8 | **YES** — `sim/sled.cpp:705`; the bump stop is a separate additive branch at 706-707 |
| `rolled` = single hard edge at `cos(1.31)` = 75.06°, no release hysteresis | PJ-7 | **YES** — `sim/sled.cpp:254`; the dwell at 255-262 debounces the RISE only |
| throttle held at 0 by the roll flag | PJ-1, PJ-7 | **YES** — `sim/sled.cpp:293-294` |
| app self-centre 3.0/s vs kernel bar slew 2.0/s | feel-first R4, PJ-6 | **YES** — `app/main.cpp:8244` (`2.0*frame_dt` push) vs `:8253` (`3.0*frame_dt` release) |
| C zeroes all three commands, R zeroes only the two leans | feel-first R8, PJ-1 | **YES** — `app/main.cpp:8263-8266` vs `:8356-8357` |
| battle gated on the roll flag | PJ-7 | **YES** — `app/main.cpp:7907-7910` `sting_stance_ok` |
| `track_slip` / `belt_speed_ms` / `engine_rpm` have ZERO consumers | all three | **YES** — `grep -rln` over `render/` and `app/` returns nothing |
| `assist_nm` reaches only `app/spawn_policy.h` | PJ-3 | **YES** — one file |
| every `torque_body` write is roll(z), yaw or per-patch — **no pitch term** | feel-first R6 | **YES** — exactly five sites: `:602 :1608 :1655 :1792 :1881` |
| shipped-at-zero roster | all | **YES** — `sled.h:224` right_assist_nm 0.0 · `:768` k_gyro_react 0.0 · `:859` k_air_shift 0.0 · `:1006` traction_mu 0.0 · `:1061` plane_lat_lean_gain 0.0 · `:383` roll_damp_nms 800 · `:517` side_right_gain_nm 700 · `:886` susp_c 3600 |
| `plane_lat_lean_gain` is NOT TOML-reachable | feel-first R3 | **YES** — no hit anywhere in `config/`; the rung's "first act is one loader line" is correct |
| `rep_belt` cannot fall below body forward speed | physics R7, feel-first §2, PJ §4 | **YES** — `sim/sled.cpp:1354`, `brake` absent from the expression |

**One open killer, closed by this judge.** Physics RUNG 6 left its own killing mutation
unrun and said so: *"if `exch_l_now` at `sled.cpp:467` is not in fact derived from rider mass
and position, this is a free impulse and the rung is a governor in disguise — read line 467
before this rung is opened."* **I read it.** `sim/sled.cpp:463-468`:
`mu_exch = hands_on ? rider_mass_kg*(mass - rider_mass_kg)/max(mass,eps) : 0.0` and
`exch_l_now = mu_exch * cross(rider_r_rel, rider_v_rel)` — a reduced mass (87.5·243.5/331 =
**64.4 kg**, DERIVED) against the rider's own relative position and velocity, zero when the
hands are off. **It is not a free impulse. The rung survives its own killer**, and the ladder
that carried the killer honestly rather than assuming past it earns the point.

---

## 1. SCORES

| criterion | PHYSICS-HONEST | FEEL-FIRST | PLAYER-JOURNEY |
|---|---|---|---|
| intent_heard (top weight) | 7 | **9** | 8 |
| roll_resistant_not_impossible | **9** | **9** | 6 |
| lean_enhancer | 4 | **10** | 3 |
| drift_canon_kept | **9** | 8 | 7 |
| fun_arcade | 7 | **9** | 8 |
| no_governor | **10** | 9 | **10** |
| measurable | 9 | 9 | **10** |
| **TOTAL** | **55** | **63** | **52** |

### PHYSICS-HONEST — 55

**intent_heard 7.** The honest defence is in the document and it is a good one: §0b killed
RC-6, so *"a departure from reality is only a defect when it also costs a named feel or
produces the constant rolling"* — and the ladder files every rung against that test, not
against reality. But the organising sentence, *"make it real and it gets stable,"* still
argues from the side of the ruling Chad struck out, and the measure he set is *"if my intent
is heard,"* stated in feel-words. Recovered by §4 ruling 1 (fly the current build and
re-rule), the most intent-faithful paragraph in the audit: it measures that **every rollover
complaint on file is against a kernel that has since changed** (longest lie-down 53.60 s →
5.09 s, R presses 0.96 → 0.42/min, share past 75° 16.78 % → 3.81 %) and names tuning against a
five-week-old complaint as the audit's largest single risk.

**roll_resistant_not_impossible 9.** The only ladder with a first-principles account of *why*
this machine rolls: SSF **0.5097 g / 27.01°** from the trapezoid against a shipped table
supplying **0.55–0.72** on six of seven surfaces, so an untripped flat-ground friction
rollover is available **by construction**. Front stiffness share φ = 0.856 puts inside-ski
lift at 0.362 g, and the machine then has nothing left between lift and tip. It keeps the
"possible" half honestly (WINTER_LAW §2.4c.1's bank still rolls him via `tan θ`, no μ
involved). It also inverts its own source strand's headline (R1-10) and adopts the refutation
— the right behaviour, scored up.

**lean_enhancer 4.** The ladder's real hole. RC-2 is a third of the charter and the only rung
that touches it is RUNG 6, which is **airborne only** and sits behind a ruling about whether
LEAN may mean two things in the air. §3 declines `plane_lat_load_frac` for a measured reason
(tip onset moves 5 % of the gap; the 16/20 m/s carve cells bifurcate into a −28° non-turning
runaway) — correct — but never reaches for `plane_lat_lean_gain`, the ski-only,
steered-patch-only term sitting at 0.0 that is the direct instrument for *"tighten a turn
instead of having to be the necessary condition of not rolling over."* A physics-honest
ladder had every reason to find it and did not.

**drift_canon_kept 9.** Best of the three. RUNG 5 is the friction circle proposed as RC-3's
mechanism, sized off the kernel's own Mohr-Coulomb budget (0.70 → 0.623 at WOT, springing
back on lift), and it makes the L2 before/after trace a **landing condition** on every
candidate, quoting RC-3's own *"a probe leg, not a hope."* It then says the honest thing —
that the drift he signed probably already comes from thrust along the chassis heading rotating
the velocity vector, not from a circle — and that in powder the circle is inert where he
spends 49 % of his driving. RUNG 1 names itself as the rung that can lose the feel.

**fun_arcade 7.** Punchy is protected structurally (RUNG 2's knee is DERIVED from the
machine's own spring, 3.322 m/s, and trail chop is pinned bit-identical below it); the
snowbank jump, the wheelie and the backflip each get an explicit keep-check. But the headline
rung lowers lateral grip globally, which is the sliding he signed as well as the rolling he
didn't, and the only fun *addition* (air authority) sits behind a ruling. It alone catches
that the buck is signed on the PAIR, so a softer landing spike is a **quieter buck** — a felt
regression nobody else spotted.

**no_governor 10.** The highest on this axis, because RUNG 4 does not merely avoid a governor
— it **removes one that ships**. The STAND self-right writes up to **0.35 m of commanded
rider lateral displacement** (`right_shift_cmd`, 1791 → consumed 1401-1407), gated on
`acos(up_body.y)` so a machine sitting conformal on a 20° side-hill reads `w_tilt = 0.9997`,
in a world whose own snowpack block records terrain slope **p95 21.1°** over 79 847 samples,
while STAND is held **6.8–39.0 %** of his drives. That is the kernel moving the rider's mass
during ordinary riding — the thing §0b forbids by name — and physics is the only ladder that
files it as a rung with an identity value rather than as a question.

**measurable 9.** Identity values throughout, and uniquely a **second** killer per rung ("the
claim's own killer") beside the mutation. §5 names five owed probe legs; §6 refuses six claims
outright, including its own headline ("available by construction" is DERIVED, not observed;
D-B-6's exit-surface mutation is NOT RUN, so read *at* the trail, not *on* it). Docked one:
RUNG 1's whole acceptance bar (RC-1, hands-off full lock) **has no tape trial anywhere in
206 minutes** — correctly admitted, but it means the top rung cannot be sized from anything
in hand.

### FEEL-FIRST — 63 (BEST)

**intent_heard 9.** The only ladder built on §0b's own terms rather than in spite of them, and
the only one that opens with **his seven asks scored yes/no from measurement**: roll resistant
*no*; possible-but-not-the-rule *no*; land on skis more often *no movement* (29.3 % vs 27.6 %
at v13g); self-righting by chance more *no* (71 % never recover); sliding/banging/punchy/jumps
*yes*; don't lose the feel *yes*; not impossibly hard *no*. That table is the charter turned
into an instrument. It refuses `roll_damp_nms` because **he ruled it** on 2026-08-13 and asks
in §3 R5 only whether 800 is still his number. It refuses `lean_px_full` because lean is the
one mechanism he praised by name and the correlation is n = 9, r = +0.815 against a 0.707
critical value — under p < 0.05. Docked one: RUNG 6 adds a new kernel term he has never been
asked about (flagged as the riskiest rung on the ladder), and RUNG 3 needs a loader line
before it is a dial at all.

**roll_resistant_not_impossible 9.** Three rungs on this axis, each with a fence against
overcorrection. RUNG 1 (`class_blend_m`) is the best-evidenced roll cure in the audit — the
tree's own probe record is **blend 0.0 → yaw −482.7°, roll 166.6°, ROLLED; blend 1.0 → yaw
−11.6°, roll 52.7°, no rollover**, with the 6/10 m/s cells unchanged — against a measured
**16.88 rollovers/min on TrailMain vs 4.13 Bush vs 0.85 LakeIce** and a 12-of-12 per-tape sign
test. RUNG 2 pays RC-4 and writes the "not the rule" half as a **number**: `never_recovered`
must fall from 71 % **while `rollovers_past90_per_min` does not fall below ~2/min**, because
if the rolls stop the rung has broken *"make it possible to roll"* from the other side. That
two-sided invariant is exactly what the charter asks for and no other ladder writes it.
RUNG 6 pays *"flips happen too fast and easy"* off a measurement that is also its own licence:
catwalk pitch is **p50 0.75° / p95 14.25° / p99 66.75°** — a spike at zero and a tail straight
to the flip, with **nothing populated in between** — so a gate opening at 60° provably cannot
touch the 30° wheelie he asked for.

**lean_enhancer 10.** Decisive, and the reason this ladder wins. RUNG 3 finds
`plane_lat_lean_gain` (`sim/sled.h:1061`, ships 0.0, adds **ski** plate only, on steered
patches only) and pairs it with the measured dead-flat carve ladder **33.2 / 33.5 / 33.7 /
33.9 m** at 8/12/16/20 m/s, a pinned gate leg sitting at **0.9003** and missing its own 10 %
pin by 0.03 pp, and a leak test that is genuinely a leak test — *sweep the gain and watch the
**non-leaned** radius; if the flat 33 m ladder moves at all, the term is riding `align_m` and
is not a lean reward.* It also diagnoses why the shipped path fails: `lean_bite_gain 0.18`
multiplies the bite on **every patch including the track**, so raising it took the lean-in
carve from 30 m to 295–1037 m — *the track out-gains the skis.* And it names the honest limit
(inert wherever `rho_eff = 0`, i.e. Road and LakeIce, so it cannot answer SK-1d's *"part of
what is so fun on the road is driving by lean"* and must not be sold as if it does).

**drift_canon_kept 8.** RUNG 5 treats a signed feel the safest way available — make it
**audible** instead of tuning it — off a measurement that earns it: mean `|track_slip| 0.522`
in contact, `|slip| > 0.3` on **53.03 %** of ticks, belt running **20.50 m/s** faster than the
ground, and **33.77 %** of ground-moving ticks slipping hard with `roost_flux < 0.02`:
spinning, with neither plume nor sound. §2 refuses `roll_damp_nms` in the tree's own words
(*"the flick, the drift's body language"*). Docked two against physics: it refuses R1-19's
circle without proposing any measurement of the drift in its place, and it does not make the
L2 before/after trace a landing condition on the ladder as a whole.

**fun_arcade 9.** Two rungs (5, 7) are pure output channels that **cannot change the ride at
all** — the purest reading of *"dont ruin it"* available. RUNG 6 keeps the wheelie by the 60°
gate and the backflip by `w_contact` (identically 0 airborne) and proves both with invariants
rather than assurances. RUNG 8 removes a frustration without removing a challenge. Almost
nothing is taken away, and the one thing that could be (RUNG 6) is fenced by the bimodality
measurement. Docked one: it adds no new *thrill* — the fun it buys is fun restored, not fun
invented.

**no_governor 9.** RUNG 3 is player-lean-only and the kernel moves nothing. RUNGS 4 and 8 zero
a **player command**, exactly as the C key already does. RUNGS 5 and 7 touch no force. RUNG 2
rides the shipped C2 term whose own header says it *"may only AMPLIFY side-slide momentum that
already exists"* and which never reads `rider_lat_m` — that is §0b's own licensed shape, and
*"slef righting by chance more"* is his sentence. RUNG 6 is a new torque but damping-only,
contact-gated, and it never picks a side, so it cannot ring and cannot right. Docked one only
because a raised righting gain and a new pitch term are both moves in the direction a governor
lives, and both correctly ask for his word first.

**measurable 9.** Identity values are branches or multiplies-by-zero, never 0-weight lerps —
stated as a rule up front. Several mutations are unusually sharp: RUNG 8's *"the test is the
0.5 s after the `O` record, not the tick itself, because `steer_actual` is not in the
override's reset list"*; RUNG 4's exclusion of tape 91's 0.983 as a `hands_on == false`
episode that would otherwise mask the result; RUNG 2's fallback path (if the extra recoveries
arrive as violent near-180° oscillation the rung is dead as a **gain** move and must become a
window move). §4 refuses five inherited readings, including *"29 % land upright"* — 93 % of
counted air events are sub-second hops, so those are **bump** statistics, not jump statistics.
Docked one: RUNG 2's invariant compares next-drive rates against a tape-to-tape spread that is
acknowledged but never bounded.

### PLAYER-JOURNEY — 52

**intent_heard 8.** Its read is genuinely new, and it is the only ladder that hears *"DOnt make
it impossibly hard, **there is a batttle going on as well**"* as a mechanism rather than a
mood: the battle is **literally gated on the roll flag** (`app/main.cpp:7907-7910`), so
**5.74 %** of his v17 ride time is time in which neither the throttle nor the drone answers.
The five-sentence read (the crash is a chain of 3.25 tumbles; the onset is a dig-in at
13.68 m/s shed in 1–2 s; recovery hands back a hostile machine; the battle is gated; the
assist is invisible) is the audit's best account of what a *player* experiences. But the
centre of gravity is legibility, and the charter's headline asks are roll resistance and lean.
Docked also for PJ-5, which rests partly on a memory **paraphrase** of a reverted speed-FX
ruling — correctly labelled and correctly fenced (*"must never be shown to him as his own
sentence"*), but still an unverified word under a rung.

**roll_resistant_not_impossible 6.** One rung (PJ-2, the same `class_blend_m` A/B) plus a
recovery-cost rung. Its invariant is the **better-shaped** of the two class-blend invariants —
*TrailMain's rate must fall and **Bush's must not fall by more than the trail's does**; if both
fall together it is a stability change, not a boundary fix, and it has bought "roll resistant"
by a route his ruling did not authorise* — and it makes the owed exit-attribution mutation the
rung's own **first leg**, read-only, one pass over the corpus. But nothing else on the ladder
reduces roll frequency, and the ladder says so.

**lean_enhancer 3.** Nothing. §4 names the omission (*"No lean-scale change"*) with a real
reason, and RULING 4 is a lean question — but framed as a mouse-contention law question (lean
/ freelook / sting aim, three-way exclusive), not as RC-2. The partial credit is PJ-3's
argument, which is a good one: *he cannot learn an enhancement he has never been shown*, so
legibility is a precondition for lean-as-enhancer being gradeable at all.

**drift_canon_kept 7.** PJ-4 is feel-first RUNG 5's idea reached independently, with one extra
caveat worth keeping (*if `ground_speed_ms` is body-frame, a machine sliding on its side reads
~0 while genuinely moving and the 13.68 m/s dig-in claim softens — **read the writer before
quoting it to him***). Refuses `roll_damp_nms` and `steer_hold_frac` for the right reasons.
Nothing damages the drift; nothing measures or protects it either.

**fun_arcade 8.** PJ-1 is the highest fun-per-risk rung anywhere in the audit. PJ-7 restores
the throttle and the drone during a hostile 5.74 %. PJ-5 is a real immersion add. PJ-8 is
openly *"the only rung on this ladder that could take away something he asked for by name"*
and is ranked last for it. Three of eight rungs are legibility-only — valuable, but not punchy.

**no_governor 10.** Six of eight rungs touch `app/` or `render/` only; the two that touch
`sim/` are a readout's release threshold and a valve law on a force that already exists. PJ-3
**shows** a torque instead of adding one, and argues it against the kernel's own sentence (*"a
hidden nudge is the RNG-shaped sin this kernel forbids"*). RULING 4 explicitly flags its own
option (b) — letting the lean fall to centre while the launcher is shouldered — as *"the app
moving the rider without you, i.e. the §0b governor line,"* and refuses to write a rung for
it. Nobody else fenced a question they themselves raised.

**measurable 10.** The best instrumentation discipline of the three, and it earns the point
three times over. §0 is five independent reads with a **killing mutation for the whole table**
(the `SLEDTAPE_PIN_D` column contract: `ntok == 45` checked on every `O` record, `steer_actual`
cross-checked against the preceding `T` to nine digits). PJ-7's first leg is **free and
read-only** and can kill the rung before a line of kernel is written — add a `rolled` edge
counter to the tape audit and run it over 84 tapes — and it then states why a null result
would **not** clear the mechanism (120 Hz pins against a 1440 Hz test, so chatter under
8.33 ms is invisible to the corpus). PJ-8 carries the caveat that undercuts its own numbers
(velocity differenced across ticks while the kernel runs 12 substeps, so peaks read **low** —
the p50 column is trustworthy, p99/max may be sampling). PJ-1's third mutation is the most
honest sentence in the audit: *"the tape cannot tell a held A/D key from a stale accumulator …
only his drive settles that"* — which is why its checklist line tells him to take his hands off
the keys.

---

## 2. VERDICT

**BEST ANGLE: FEEL-FIRST (63).**

Three reasons, in order of weight.

1. **It is the only ladder that pays RC-2.** *"Just allow the balance of body mechanism to
   ENHANCE ability, i.e. tighten a turn instead of having to be the necessary condition of not
   rolling over"* is a third of §0 and is repeated in §0b (*"Leaning shall enhance the ride"*).
   Physics touches it only in the air and behind a ruling; player-journey does not touch it at
   all. Feel-first names the instrument, the measured baseline, the pinned gate leg it would
   un-miss, the leak test, and the limit.
2. **It holds all three equal-weighted thirds.** Comfort: rungs 1, 2, 6. Fun: 1, 6, 8.
   Feedback: 5, 7. Physics is comfort-heavy with one feedback rung and no ground-lean rung;
   player-journey is feedback-heavy with one comfort rung and no lean rung.
3. **Its refusals are as strong as its rungs**, and they are refusals *in his favour* — it
   declines the single biggest lever in the kernel (`roll_damp_nms`) on the ground that he
   ruled it himself, and declines `steer_hold_frac`, `track_pitch_half_m` and all three
   airborne dials with a measured reason each. Under a charter whose stated measure is *"if my
   intent is heard"* and whose loudest sentence is *"I am afraid it might ruin something that
   is reallt good right now but just dont ruin it,"* a ladder that knows what not to touch is
   worth more than a ladder that knows more physics.

**What feel-first is missing, and why the grafts below are not optional.** It has no
first-principles account of *why* the trail rolls him — it has the correlation
(16.88 vs 4.13/min) and the boundary mechanism, but not the load transfer underneath. It has
no rung for the battle. It misses that R hands the machine back at **WOT**, not merely turned.
And it carries the self-right's gravity-tilt gate as a ruling question when physics showed it
is a rung that **removes** a shipped governor.

**One correction for whichever ladder is built.** Physics RUNG 6's unrun killer is now run
(§0 above): `exch_l_now` at `sim/sled.cpp:467` IS derived from rider mass and relative
position, so `k_air_shift` is not a free impulse. If Chad ever rules R6 "yes", that is the
honest, player-driven route — but physics RUNG 7 (`belt_brake_frac`) is a **precondition**,
not a payoff: `rep_belt` cannot fall below body forward speed (`sim/sled.cpp:1354`, verified),
so arming `k_gyro_react` first hands him **nose-up on throttle with nothing to bring it back
down** — the half that makes a landing worse.

---

## 3. GRAFTS, RANKED

1. **PJ-1's throttle finding into feel-first RUNG 8.** Same dial. Feel-first found the bars
   (four of six rightings at full lock); PJ measured the other half out of the raw `O` records
   — **throttle command 1.00 in 6 of 6**, because `sim/sled.cpp:293-294` holds the throttle at
   zero *because she was rolled* and releases it the instant R clears the flag. R hands back a
   machine **turned AND at wide-open throttle**. Take PJ-1's third killing mutation with it
   (the tape cannot distinguish a held A/D key from a stale accumulator — only his drive
   settles it) and its checklist line, which tells him to take his hands off the keys.
2. **PJ-2's invariant shape onto feel-first RUNG 1.** *Bush must not fall by more than the
   trail does* — the test that separates a boundary fix from a stability change, and the one
   that stops the rung buying "roll resistant" by a route his ruling did not authorise. Take
   PJ-2's first leg with it: run D-B-6's **exit-surface attribution mutation**, read-only, one
   pass over the corpus, before anything is built.
3. **PJ-7 whole (`rolled_release_frac`).** Feel-first has no rung for *"there is a batttle going
   on as well."* One hard edge at 75.06° with no release hysteresis, while the C1 hull makes a
   downed machine rest at ~65–75° — **the threshold sits inside the resting band** — and the
   same flag owns both the throttle and the drone launch. Its first leg is a free read-only
   edge counter that can kill it before any kernel is touched, and its null-result caveat
   (120 Hz pins vs a 1440 Hz test) must travel with it. PJ's RULING 3 travels with it too:
   *should that flag own the throttle at all when there is a battle on?*
4. **PJ-3 whole (`kAssistTell`).** The biggest un-shown mechanism in the tree: the comfort
   assist fires on **79.8–93.7 %** of ticks on every v17 drive at p95 up to **765 N·m**, half
   the machine's 1505 N·m tipping budget, and `assist_nm` reaches exactly one file, for two
   resets (verified). Zero ride risk, and it is the precondition for Chad being able to grade
   RC-2 from the seat at all. Keep its killing mutation intact — the **band, not the duty, is
   the whole rung**; a tell gated on `assist_nm != 0` is wallpaper.
5. **Physics RUNG 4 (`right_tilt_surf_frac`) as a rung, not a ruling.** Feel-first carries this
   only as §3 R2(b). It should be a rung, because it is the one move in the whole audit that
   **removes** a governor that ships: up to 0.35 m of kernel-commanded rider lateral
   displacement, armed at `w_tilt = 0.9997` on a conformal 20° side-hill, in a world with
   terrain slope p95 21.1°, while STAND is held 6.8–39.0 % of his drives. The kernel already
   fixed this exact defect for the hull terms and left the self-right behind — its own comment
   at `sled.cpp:1367-1371` says why. Carry its unverified flag (`phi_surf` may degenerate when
   fewer than three patches are loaded, making the blend a no-op in exactly the tipped case)
   and the separate, unfolded finding that the self-right has **no contact gate at all**
   (≈1244 °/s of free roll on a held press in the air, DERIVED).
6. **Physics §1(a)–(d) as feel-first's missing §1.** The spine: SSF **0.5097 g / 27.01°**
   against a shipped `mu_lat` table of 0.55–0.72 on six of seven surfaces (verified
   line-by-line); φ = 0.856 putting inside-ski lift at 0.362 g; and the landing damper as a
   **lateral-force multiplier** (a 4 m/s arrival unlocks 2.24× gravity's own peak tipping
   torque). Feel-first cites the conclusion in its §4 and does not carry the arithmetic. With
   it, RUNG 1's class step (`rho_eff` 0 → 260 under one ski) reads as a load transfer on a
   machine already past its lift threshold, instead of as a coincidence at a boundary.
7. **Physics RUNG 1 (`lat_mu_scale`) held as the plant-side rung feel-first says it does not
   propose.** Feel-first's RUNG 4 and D-E's `steer_hold_frac` both defer to *"a plant-side rung
   this ladder does not propose"* — physics RUNG 1 **is** that rung, with its scale table
   already derived (0.708 TrailTributary / 0.728 TrailMain / 0.850 Road / 0.927 Bush to reach
   the tip). Graft it as a **named successor with its trade stated**, not as a build: it is the
   one rung that can cost him the sliding he signed, its acceptance bar (RC-1, hands-off full
   lock) has **no tape trial in 206 minutes**, and its target value is LITERATURE-thin (a
   splice of car-tyre lateral and snowmobile longitudinal; no measured snowmobile lateral-g
   number is in hand).
8. **Physics RUNG 7 as an ORDERING constraint on feel-first's §2 refusal.** Feel-first already
   refuses the three airborne dials. Add physics's ordering so the refusal survives a "yes":
   if Chad rules R6 affirmative, `belt_brake_frac` comes **first**, because `rep_belt` cannot
   fall below body forward speed and `k_gyro_react` alone buys only nose-up.
9. **Physics §4 ruling 2 — "Is a backflip a rollover?"** — into feel-first's §3. Every
   roll-resistance number on every ladder counts **attitude, not intent**, and §0's first
   sentence is *"Yep I can do backflips."* No instrument in 206 minutes separates a deliberate
   send from a crash. Without his definition, feel-first RUNG 2's two-sided invariant (*must not
   fall below ~2/min*) has no denominator.

**Already present in feel-first — do not double-graft:** the `class_blend_m` rung itself
(= PJ-2), the slip voice (= PJ-4), the rpm blend (= physics RUNG 8), the steer-release rung
(= PJ-6), the R-recentre rung (= PJ-1's dial), and *"give the last six drives a word"*, which
all three ladders rank first and which this judge endorses as the gate on every rung above.

---

*Judge 2 of 3, 2026-09-18, against `ed0a43ce8`. Read-only. No dial changed, no config edited,
no golden moved, no test run, no probe built, nothing pushed. Every source line in §0 was read
in this worktree.*
