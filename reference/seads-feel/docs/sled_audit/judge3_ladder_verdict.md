# JUDGE 3 of 3 — LADDER VERDICT

Scope: read-only against main. Worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. No dial moved, no config edited, no golden touched, no test run,
no probe built, no `seads.exe` launched, nothing pushed. The only file this judge
wrote is this one. Four source spot-checks were run read-only to keep the graft list
from resting on relay; they are listed in section 5.

Bar: `D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` section 0
and section 0b, verbatim. Weighting as given: `intent_heard` carries top weight;
comfort, fun and feedback are EQUAL (Chad gave no ranking).

---

## 1. SCORES (0-10 per criterion)

| criterion | physics-honest | feel-first | player-journey |
|---|---|---|---|
| intent_heard (top weight) | 7 | **9.5** | 8 |
| roll_resistant_not_impossible | 9 | **9.5** | 8 |
| lean_enhancer | 5 | **9** | 4 |
| drift_canon_kept | 8 | **8.5** | 7.5 |
| fun_arcade | 7 | **9** | 7.5 |
| no_governor | 9 | 9.5 | **10** |
| measurable | **10** | 9 | 9.5 |
| **TOTAL** | **55** | **64** | **54.5** |

---

## 2. WHY — one paragraph per angle

**PHYSICS-HONEST (55).** The strongest document of the three and the second-best
ladder, and the gap is the angle itself. Section 0b retired RC-6 by name — *"Make it
less honest but dont ruin it"* — so *"make it real and it gets stable"* is an organising
premise Chad has already relaxed. The ladder knows this and carries the caution in its
own section 0 (a departure from reality is a defect only when it costs a named feel),
which is why it scores 7 and not 4. Its section 1 mechanism page (SSF 0.5097 g /
27.01 deg against a shipped ski `mu_lat` of 0.55-0.72 on six of seven surfaces) is the
best single piece of reasoning produced anywhere in this audit, and it earned it by
*inverting its own strand's headline finding* against the refutation pass. `measurable`
is a clean 10: every rung carries the identity value AND "the claim's own killer", and
its closing section is an explicit refusal list. Where it loses: `lean_enhancer` (5) —
the section 0 sentence Chad wrote most concretely, *"tighten a turn"*, gets no ground
rung at all; Rung 6 buys lean authority in the AIR, which is not what he asked for and
which the ladder itself flags as giving LEAN a second airborne meaning his 2026-08-26
correction separated on purpose. And `fun_arcade` (7) carries the ladder's own named
risk: Rung 1 lowering lateral bite is the most "ruin it"-shaped move on any of the three
ladders, against an unpaid O5 (*"turning too unstable, can't hold a carve"*), and it
says so.

**FEEL-FIRST (64) — THE WINNER.** It scores highest on the top-weighted criterion
because it is the only ladder whose rungs map one-to-one onto section 0 / section 0b's
own sentences: *"allow me to land on my skis more often after a roll ... but not every
time"* -> Rung 2, the only rung on any ladder aimed at RC-4 and the 71 % never-recovered
figure; *"tighten a turn instead of having to be the necessary condition of not rolling
over"* -> Rung 3, the only rung anywhere that buys the ground arc for the lean
(`plane_lat_lean_gain`, identity 0.0, with a non-leaned-radius killing mutation that
proves it is a lean reward and not a leak); *"flips happen too fast and easy"* held
against *"I DO want a wheelie"* -> Rung 6, whose 60-degree gate is argued out of the
MEASURED bimodality (catwalk p95 14.25 deg, p99 66.75 deg, nothing populated between) so
the 30 deg wheelie is preserved by construction rather than by hope. Rung 2's invariant
is the best single sentence of instrumentation in the whole audit: unrecovered fraction
must fall AND `rollovers_past90_per_min` must NOT fall below about 2/min — *"Make it
possible to roll but not the rule"* measured in both directions at once, which is the
half every other ladder only promises. `no_governor` 9.5 (Rung 6 adds a kernel term; it
is flagged as the riskiest rung and fenced behind his ruling). `measurable` 9, a notch
under physics-honest only because it has no recomputed mechanism spine of its own and
leans on D-B's recovery predicate — which it does flag.

**PLAYER-JOURNEY (54.5).** Best `no_governor` on the board (10) and it earned it twice:
six of eight rungs touch no force at all, and its Ruling 4 identifies a candidate —
letting the lean fall to centre while the launcher is shouldered — as the section 0b
governor line BY NAME and then refuses to write the rung for it. PJ-3 is the most
original reading in the audit: *"keep the benefit there for good riding"* is unanswerable
from the seat while the thing holding the machine up on 79.8-93.7 % of ticks reaches no
eye or ear. Its own-measurement table (six autoright `O` records read byte-wise, throttle
command 1.00 in 6 of 6) is the most valuable new fact any strand produced tonight. It
loses on `lean_enhancer` (4) — it writes no lean rung and says so — and on `intent_heard`
(8): section 0's opening complaint is about the RIDE (*"she's too unsteady"*), and a
ladder that ranks the ride seventh and eighth by construction is answering a question
adjacent to the one he asked. Ordering by journey-value-per-risk is defensible and
honestly declared; it is still not his ordering.

---

## 3. VERDICT — BEST ANGLE: **FEEL-FIRST**

The measure Chad set is *"the measure of it shall be if my intent is heard"*. Feel-first
is the ladder that hears the most of his sentences, refuses the dials he has already
ruled on (`roll_damp_nms`, `track_pitch_half_m`, the three airborne dials) instead of
re-litigating them, and is the only one that pays RC-2 — the lean as enhancer — on the
ground, where he asked for it.

Two convergences worth naming: three strands working from different angles independently
put `[snowpack] class_blend_m` first among rollover rungs (feel-first Rung 1, PJ-2, and
physics-honest's ruling 3), and two of three independently arrived at the slip voice.
Convergence under independent angles is weak evidence, but it is evidence.

All three ladders put the same thing first in their rulings and they are right: **there
is no felt report for the six drives of 2026-09-17.** One line per tape costs nothing and
is worth more than every number under it.

---

## 4. GRAFTS ONTO FEEL-FIRST — ranked, with what each fixes

1. **PJ-1 replaces feel-first Rung 8 outright** (`autoright_recentre`). Feel-first found
   the steer half; PJ-1 read the raw `O` records and found the throttle half — command
   1.00 in 6 of 6, `steer_actual` 1.0000 in 4 of 6, none at centre. VERIFIED BY ME,
   read-only: `sim/sled.cpp:292-293` is
   `(s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle)`, so the throttle is held down
   BY the roll flag and released the instant R clears it; the R block at
   `app/main.cpp:8355-8357` zeroes `sled_lean_lat` and `sled_lean_fwd` and never
   `sled_steer_cmd`, while `app/main.cpp:8263-8266` zeroes all three under
   `// SK-1c: C recentres the bars too`. Same dial, strictly larger finding. Carry PJ-1's
   honest caveat with it: the tape cannot tell a held A/D key from a stale accumulator,
   so the drive line must tell him to take his hands off.

2. **PJ-3 (`render::kAssistTell`) as a new rung.** Feel-first has two output rungs (slip
   voice, rpm blend) and neither shows the COMFORT ASSIST, which is non-zero on
   79.8-93.7 % of ticks at p95 up to 765 N.m and whose only tree-wide consumers are two
   resets in `app/spawn_policy.h`. This is the rung that answers *"keep the benefit there
   for good riding"*, and its killing mutation is already correct and load-bearing: the
   median is 5-75 N.m, so a tell gated on `assist_nm != 0` is wallpaper and the BAND is
   the whole rung.

3. **Physics-honest Rung 4 (`right_tilt_surf_frac`) promoted from feel-first's ruling
   R2(b) to a rung.** It is the only proposal on any ladder that REMOVES kernel-commanded
   rider shift (up to 0.35 m through `right_shift_cmd`) during ordinary slope riding —
   it moves TOWARD section 0b's *"the player's lean stays the only thing that moves the
   rider"*, not away. It answers section 0's *"say up a hill"* literally, and the kernel
   already fixed this exact gravity-tilt defect for the hull terms and wrote down why.
   Feel-first leaves it as a question; it deserves a rung.

4. **Physics-honest Rung 5's L2 DRIFT LEG, grafted as a CONDITION on every rung — not
   the friction-circle rung itself.** RC-3 is explicit: *"measure it before touching
   anything, and re-measure after — a probe leg, not a hope."* Feel-first refuses to tune
   the drift (correct) but does not make measuring it a gate (a gap). Take the condition,
   leave the circle: physics-honest's own killing mutation shows the circle is inert on
   Bush, which is 49 % of his driving, and RC-5 says the drift is the one behaviour he
   explicitly signed.

5. **Physics-honest Rung 3 (`traction_mu`) as a ruling-request rung beside feel-first's
   Rung 1.** Cheapest paid ask in the audit: his ruling *"yes take care of that"* is
   TAKEN, the mechanism is BUILT, it ships 0.0, and the sweep is already on the books
   (mu >= 0.6 kills the Bush runaway; mu >= 3.0 is byte-identical on his own traverse).
   Feel-first mentions it only inside Rung 3's prose as an unpaid instrument. Two
   ruling-request rungs sitting together — `class_blend_m` and `traction_mu`, both live-
   tunable, both reserved for him — is one drive, not a build.

6. **PJ-7's FIRST LEG ONLY, as free read-only work.** Not the `rolled_release_frac` rung
   (a frozen-kernel change resting on a DERIVED chatter nobody has observed), but its
   opening move: add a `rolled` edge counter to `tools/sled_tape_audit.py` and run it
   over the 84 tapes. VERIFIED BY ME: `sim/sled.cpp:254` is a single
   `dot(body_up, up_cg) < cos(1.31)` edge with no release threshold. If no tape shows a
   machine resting in the 65-75 deg band with a chattering flag, the rung dies before a
   kernel line is written — and PJ-7's own epistemics must ride along: a null result from
   120 Hz pins against a 1440 Hz test does not CLEAR the mechanism, it only fails to find
   it.

7. **Physics-honest Rung 7 (`belt_brake_frac`) grafted as the ORDERING CONSTRAINT on
   feel-first's ruling R6.** Feel-first says the belt expression is the blocker;
   physics-honest turns it into a named dial with identity 0.0 plus a verified proof that
   the thrust path is untouched. VERIFIED BY ME: `sim/sled.cpp:1354` reads
   `rep_belt = std::max(dv * v_cmd_b, std::max(v_bf, 0.0))` — `brake` does not appear in
   the expression. Arming `k_gyro_react` before this is fixed hands him nose-up authority
   and no nose-down.

8. **PJ's Ruling 3 into feel-first's ruling list.** *"Should the machine still cut the
   throttle the whole time she is on her side?"* — 5.74 % of v17 ride time is time in
   which neither the throttle nor the drone answers (`app/main.cpp:7907-7910`), against
   *"DOnt make it impossibly hard, there is a batttle going on as well."* Feel-first
   carries the battle context nowhere; this is the cheapest way in.

**Deliberately NOT grafted:** physics-honest Rung 1 (`lat_mu_scale`). It is the best
argued rung in the audit and it is the one that can lose the feel — it spends cornering
grip to buy roll resistance, against an unpaid *"turning too unstable, can't hold a
carve"*, and the ladder itself says *"this trade is a ruling, not a measurement."* Keep
it on the table as a ruling for Chad, behind `class_blend_m`, which buys a measured
rollover reduction (bankgraze yaw -482.7 deg -> -11.6 deg, ROLLED -> no rollover) at no
grip cost. Also not grafted: physics-honest Rung 6 (`k_air_shift`) — it hands LEAN a
second airborne meaning his 2026-08-26 correction separated on purpose, and feel-first's
ruling R6 already puts that question to him in the right form.

---

## 5. WHAT THIS JUDGE DID NOT DO

- Did not build, run, gate or fly anything. Every score is a reading of three documents
  plus these read-only source spot-checks: `config/world.toml:645` = `class_blend_m 0.0`;
  `app/main.cpp:8253` = `const double back = 3.0 * frame_dt;`; the R block at
  `app/main.cpp:8355-8357` against the C block at `:8263-8266`; `sim/sled.cpp:254`,
  `:292-293` and `:1354`. All six read exactly as the ladders quoted them.
- Did not verify any tape number. Where a score rests on a MEASURED figure, it rests on
  that strand's claim and its stated killing mutation, not on a re-derivation by me.
- Did not weigh the criteria groups unequally. Chad gave no ranking; only `intent_heard`
  carries top weight, and that is his own sentence (*"the measure of it shall be if my
  intent is heard"*), not an inference.
- Did not manufacture a felt report. Every ladder's first ruling stands: no dial on any
  of them may move until the six drives of 2026-09-17 have a word.

*Judge 3 of 3, sled ride audit, 2026-09-18, against `ed0a43ce8`. Read-only against main.*
