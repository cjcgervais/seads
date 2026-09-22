# LADDER — FEEL-FIRST

**Angle.** Arcade-lawful under Chad's *"less honest but dont ruin it"*: every rung is **one
dial that changes what the hand feels**. The wheelie, the drift and the backflip are
untouched by construction — no rung moves `roll_damp_nms`, `track_pitch_half_m`, or any
airborne torque.

**Worktree** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
**READ-ONLY against main.** Nothing built, no dial changed, no config edited, no test run,
no probe, no `seads.exe`, nothing pushed. The only files this pass wrote are this one.

**Confidence vocabulary.** MEASURED / DERIVED / LITERATURE / GUESS (labelled every time).
**Every numeric claim carries its killing mutation.** No feel recommendation rests on a
harness number alone: each cites a tape event **and** one of Chad's words, verbatim, with
doc and section.

**Sources read in full:** `D-A_kernel_mechanism.md`, `D-B_tape_forensics.md`,
`D-C_felt_history.md`, `D-D_feedback_loop.md`, `D-E_input_map.md`, `R1-real-dynamics.md`
+ `-REFUTE`, `R2-game-feel.md` + `-REFUTE`, `R3-player-experience.md` + `-REFUTE`,
`R4-landings.md` + `.refute`, plus `ROLL_COMFORT_HANDOFF.md` §0/§0b and `WINTER_LAW.md`.
Where a refutation pass killed a claim, **the refuted form is not used here** (noted per
rung).

---

## 0. THE CHARTER, VERBATIM — the bar every rung is scored against

`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0** ("Chad's ruling (drive 4,
2026-08-12 — verbatim, this is the acceptance bar)"):

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more often
> after a roll** (even though R works). But not every time — allow it to happen. Also I've
> seen lots of snowmobiles **drift around corners and then with throttle, straighten out.
> The feel is really good. Don't lose the feel**, except the constant rolling. **Make it
> possible to roll but not the rule.** However the game feels like a nicely made sim as
> there is dynamic room and **the balance mechanism works really good. Just allow the
> balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be
> the necessary condition of not rolling over.** Just allow leaning a certain way in a
> particular condition to be the OPTIMAL weight distro for better traversing, say up a hill
> or around a corner. **Work on the math to achieve a balance of fun and accuracy to real
> physics, just as our airplane ontological counterpart does.**"

Same document **§0b** (SUPERSEDING, 2026-08-12 night):

> "no I dont like thge idea in the handoff at all! It will ruin the feel to have a governor.
> Make it less honest but dont ruin it. Get a fable consult and **the measure of it shall be
> if my intent is heard**. Leaning shall enhance the ride an just make it more stable and
> **slef righting by chance more**. It should be arcadey to a degree so that it is fun.,
> **SLiding banging, punchy, jumps.** Just make it more stable."

> "please build autmatically and make this kernel way better ... **Allow for bad driving too
> but keep the benefit there for good riding. DOnt make it impossibly hard, there is a
> batttle going on as well.** And the point is this should be fun as well, not a true sim.
> But **I do want the finess and all the mechanisms available for tuning** ... keeping the
> good but making it easier / more fun?"

The felt report, `docs/gi4_ride_handoff.md` §1 (his words, left column):
> 1 *"tips over too easy"* · 2 *"should be able to launch in the air"* · 3 *"turning too
> unstable, can't hold a carve"* · 4 *"WOT should lift the skis to ~30°"* · 5 *"rider weight
> needs authority, not thrown into a spin"* · 6 *"flips happen too fast and easy"*

Also on file, verbatim: *"If I lean back and hit a snowbank, I flip multiple times end over
end... like 5x"* and *"I DO want a wheelie ... even MORE so"* (2026-08-13, `D-C` §1 / O3);
*"I am not enough able to hold a steady turn as I am using wsad"* and *"I dont like the
steering via mouse coupled with lean they have to be two independent things, **part of what
is so fun on the road is driving by lean**"* (SK-1d, 2026-08-25, carried in `app/main.cpp`);
*"I need a key for now that lets me autoright until we get the guy running back to the
snowmachine"* (`app/main.cpp`); *"The conditions should be **palpable and observable in the
sled performance**"* and *"Snow that is properly shaded and **felt**, not some cheap drawn
in illusion"* (`docs/roost_consult_packet.md` §1, 2026-08-27).

**Where the machine stands against that bar, tonight (MEASURED, D-B §7):** roll resistant —
**no** (4.32 past-90 rollovers/min, one every 13.9 s). Possible to roll but not the rule —
**no**. Land on my skis more often — **no movement** (29.3 % upright vs 27.6 % at v13g).
Self-righting by chance more — **no** (71 % never recover). Sliding/banging/punchy/jumps —
**yes**. Don't lose the feel — **yes**. Not impossibly hard — **no**.
`sled_tape_91` **ends upside down**: last tick tilt 131.4°, speed 0.41 m/s, throttle 0,
clean `# sig` exit. He shut the night down with the machine on its roof.

---

## 1. THE LADDER

Fixed shape per rung: **felt problem in Chad's words → tape evidence → mechanism (file:line)
→ THE ONE DIAL with identity value → measurable invariant + killing mutation → one
drive-checklist line → law check.**

Identity value = the number at which the build is **bit-identical to today**. Every rung is
a branch or a multiply-by-zero, never a 0-weight lerp.

---

### RUNG 1 — THE SEAM THAT SPINS YOU

**Felt problem, his words.** *"she's too unsteady"* … *"Don't lose the feel, **except the
constant rolling**"* … *"**Make it possible to roll but not the rule**"* (§0). And
`docs/snowform_measurements.md` §M8.3 names his own reported mechanism: the Road bank-strike
superspin.

**Tape evidence.** MEASURED, D-B-6: **16.88 past-90 rollovers per minute on TrailMain
against 4.13 on Bush and 0.85 on LakeIce**, pooled over 206.1 min. Per-tape sign test:
of the 12 tapes carrying >20 s of both TrailMain and Bush, **TrailMain's rate exceeds Bush's
in 12 of 12** (p ≈ 2⁻¹² ≈ 0.00024 under equal rates), across four kernel buckets and four
weeks. Tonight's `sled_tape_91`: **7 rollovers in 27 s of trail (15.6/min) against 11 in
106 s of bush (6.2/min)** — and that tape ends at 131.4° of tilt. Control in the other
direction: `sled_tape_90`, **90 s on lake ice with zero rollovers**, all 7 of its rollovers
in its 19 s of bush. *A groomed trail is where a snowmobile should be most planted; his is
where it rolls 4.1× hardest.*

**Mechanism.** `world/snowpack.cpp:700-746` classifies the patch's surface off the corridor
hit and, `if (p.class_blend_m > 0.0 …)`, ramps `surf_mix` across the corridor edge.
`sim/sled.cpp:630-636` consumes it: `gs.surf_mix > 0.0 ? blend_dials(dials[surf],
dials[surf_b], surf_mix) : dials[surf]` — an explicit branch, so `surf_mix == 0` takes the
original reference. `config/world.toml:645` is `class_blend_m = 0.0`, therefore **`surf_mix`
is identically zero in the shipped game and `blend_dials` (`sim/sled.cpp:200-213`) is never
called** (MEASURED, D-A §5.7). One ski crossing the corridor edge steps `rho_eff` from 0 to
260 under that ski alone — a pure yaw+roll couple — in one tick.

Measured on `seads_sled_probe bankgraze` (`config/world.toml:640-644`, the tree's own record):
**blend 0.0 → yaw −482.7°, roll 166.6°, ROLLED; blend 1.0 → yaw −11.6°, roll 52.7°, no
rollover**, with 6/10 m/s cells unchanged. `snowform_measurements.md` §M9.1: a **25 cm**
lateral move, **straight, throttle open, NO steer input**, takes the machine from roll 2.4°
to **89° of roll at 16 m/s** and **1.34 rotations at 24 m/s**.

**THE ONE DIAL.** `[snowpack] class_blend_m` — **identity 0.0** (shipped;
`config/world.toml:645`). A branch, bit-identical.

**Invariant.** On his next drive, past-90 rollovers/min **on TrailMain and Road** falls
toward the Bush rate while the Bush rate itself moves less than its own tape-to-tape spread
(4.1 vs 6.2/min across tonight's two trail-bearing tapes). Probe invariant, already written:
`bankgraze` at 16 and 24 m/s must not roll, **and** the 6/10 m/s cells must stay unchanged.

**Killing mutation.** Attribute each rollover to the surface at **exit** rather than entry
(D-B-6's own owed mutation, **NOT RUN**): if TrailMain's rate collapses, he was *crossing*
the trail, not rolling on it, and this rung is aimed at a boundary that is only a
coincidence. Until that is run, read "on the trail" as **"at the trail"** — which is in fact
the reading this rung acts on, so the rung survives the mutation in direction either way.
Second mutation: `class_blend_m = 1.0` and `blend_dials` still never executing (a rename or
a dead `c.found`), which would make the dial cosmetic.

**Drive checklist line.** *"Run the Onaping trail and the plowed road at 40–90 km/h with
`SEADS`'s `class_blend_m` at 0 then at 1, and tell me whether the machine still gets grabbed
when one ski touches the edge — same run, same speed, both ways."*

**Law check.** Depth 0.77 — **UNTOUCHED**: `g.depth_m` is computed at `world/snowpack.cpp:702`
*before* the blend block and is not read or written by it; `base_m = 0.85` (the parameter that
yields the signed p50 0.77 m) is not in scope. No governor — nothing reads or moves the
rider's mass. Wheelie kept — no pitch term. STAND/LEAN — untouched. Ragdoll banned — this
*reduces* uncommanded rotation (−482.7° → −11.6° yaw, MEASURED). One surface — this is **not**
a second surface system: it is the existing per-patch class lookup made continuous over
`class_blend_m` metres instead of stepping, exactly as `sim/sled.cpp:200-213` was written to do.

**Standing.** `LANES.toml:939` records the ruling that keeps it off, and it is deliberate:
*"⚠ THE STICK RULING: [snowpack] class_blend_m STAYS 0.0 — it is live-tunable and it is
**CHAD'S A/B, not the lane's call**."* **This rung is a ruling request, not a build.**

---

### RUNG 2 — THE SECOND ROLL, NOT THE FIRST

**Felt problem, his words.** *"Let me slide around a bit arcade but **allow me to land on my
skis more often after a roll** (even though R works). **But not every time — allow it to
happen**"* (§0); *"Leaning shall enhance the ride an just make it more stable and **slef
righting by chance more**"* (§0b).

**Tape evidence.** MEASURED, D-B-2 / R3-M3: **71 % of tonight's rollovers never return to
steady driving** (37 of 52), and the metric has not moved in four weeks and three signed
kernels — 69 % pre-reconcile, 70 % v13g, 67 % v15, **71 % v17**. The recovery *count* is
invariant across four different recovery predicates (15 recoveries in every one; only the
*times* move). R3-M1: the 52 past-90 crossings merge into **16 crash episodes — 3.25
crossings per crash**, one crash every 45 s. R3-M2: a crash costs **p50 7.02 s, p90 18.13 s,
max 38.41 s**, with 4 of 15 episodes right-censored (so 7 s is a **floor**). D-B-3: he is
**not** reaching for the backstop — 5 R presses against 52 rollovers. `sled_tape_90`:
**1 recovery out of 7 rollovers**.

**Mechanism.** `sim/sled.cpp:1545-1607` — RC item C2, the side-contact righting bias, the
kernel's only "she slides back onto her skis" term. It harvests lateral slide momentum:
`wv` ramps on lateral speed only (1.5→6.0 m/s), `w_load` on hull load, `wt` is a smoothstep
window on `|phi_surf|` open from 40° to 140°, and `wo = clamp01(1 − |ω_z|/wref)` fades it
**out** above `side_right_wref_rads` — so a machine that has *stopped* spinning gets the
full bias. `sim/sled.h:1035-1049` records the shipped pair's own trade-off, verbatim:
> *"(700, 2.5) is the measured best-of-9 … smooth, non-oscillatory settling (**stalls
> ~105-125 deg rather than swinging**) … **the 15 m/s full recovery is not alive at this
> pair**, which is the honest trade-off reported for the supervising session, not silently
> forced."*

**A machine stalled at 105–125° with near-zero roll rate is exactly the 71 %**, and it is
exactly the population `wo` hands full authority to. The dial was shipped conservative and
the source says so.

**THE ONE DIAL.** `[sled_comfort] side_right_gain_nm` — **identity 700.0** (shipped;
`sim/sled.h:517`, `config/scenario.toml`). Already TOML-reachable today.

**Invariant.** `never_recovered` fraction on his next drive falls from 71 % while
**`rollovers_past90_per_min` does not fall below ~2/min** (if the rolls stop, the rung has
broken *"Make it possible to roll but not the rule"* in the other direction). Second
invariant, from the source's own measured failure mode: the recovery must remain
**non-oscillatory** — no episode may cross ±140° of `phi_surf` more than twice.

**Killing mutation.** Raise the gain and re-run the `onside_trace` L4 sweep: if the extra
recoveries arrive via **violent repeated near-180° oscillation** (MEASURED as the failure
mode of every higher-gain cell of the original best-of-9), the rung is dead as a gain move
and must become a `side_right_wref_rads` / window move instead. Second mutation: D-B-2's
recovery predicate — widen to tilt < 45° or drop the 0.5 s hold and the unrecovered fraction
must fall; it is the **trend across four buckets under one predicate** that carries the claim.

**Drive checklist line.** *"Roll her deliberately at 20, 40 and 60 km/h, five times each, and
tell me how many times she slid back onto her skis by herself — and whether any of them
looked like a fish flopping."*

**Law check.** Depth 0.77 — untouched. No governor — C2 adds `torque_body.z` at
`sim/sled.cpp:1606` and **never reads or moves `rider_lat_m`**; it is the §0b arcade-lawful
term, and it *"may only AMPLIFY side-slide momentum that already exists"* (its own header,
`sim/sled.cpp:1536`). Wheelie kept — roll axis only. STAND rights / LEAN throws — untouched;
R remains the backstop and `wv` is zero below 1.5 m/s so *"a stationary machine NEVER
self-rights"*. Ragdoll banned — **this is the rung's live risk**, and the non-oscillation
invariant above is the fence. One surface — untouched.

---

### RUNG 3 — THE LEAN HAS TO BUY THE ARC

**Felt problem, his words.** *"the balance mechanism works really good. **Just allow the
balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be the
necessary condition of not rolling over.**"* (§0) and felt item 5: *"rider weight needs
authority, not thrown into a spin"*; felt item 3: *"turning too unstable, can't hold a
carve"*.

**Tape evidence.** MEASURED (D-A §6.3, `tools/sled_tape_audit.py`): he holds **full lean
(|lean| > 0.98) for 14.8 – 43.6 %** of every v17 drive — 26.5 % pooled tonight, 44 % on tape
91. R3-M6: lean sits in the **intermediate band on 54 % of riding ticks** while steer is
bimodal — *"lean is genuinely modulated"*. D-E-04: the lean axis holds a partial value for up
to **36.1 s** (against the steer axis's all-corpus maximum of 0.500 s). **He is asking the
reward channel for everything it has, continuously, and holding it there.**

**Mechanism.** Two lean-reward paths exist and the shipped one is the wrong one.
`lean_bite_gain = 0.18` (`sim/sled.cpp:1067`) multiplies the μ bite on **every patch
including the track** — MEASURED self-defeating for an arc (`sim/sled.h:1092-1097`: 0.18 →
0.45 took the lean-in carve from **30 m to 295–1037 m of radius**, *"because the track
out-gains the skis"*). `plane_lat_lean_gain` (`sim/sled.h:1061`, applied
`sim/sled.cpp:1101-1102` as `lat *= 1.0 + p.plane_lat_lean_gain * align_m`) can only ever add
**ski** plate, on steered patches only — and it **ships 0.0**. And the strongest lean reward
was regressed on purpose: `sim/sled.h:1154-1157` records that shipping
`plane_lift_split_frac = 1.0` re-pinned test 2336 — *"lean-in no longer tightens the arc by
10 % (28.640 m vs 28.632 m free) — which is **Chad's own felt item 5, rider weight
authority**. It is carried as an open debt."* Both instruments that were supposed to pay that
debt (`traction_mu`, and a `w_contact` floor) ship at **0.0** and **never built**.

**THE ONE DIAL.** `plane_lat_lean_gain` — **identity 0.0** (`sim/sled.h:1061`). ⚠ It is a
`SledParams` field, and **the whole of `SledParams` is compile-time only** (D-A §4.5), so the
rung's first act is **one loader line** promoting it into `[sled_comfort]`, per RC-8
(*"EVERY MECHANISM IS A DIAL"*). At 0.0 the multiply is `× 1.0` — bit-identical.

**Invariant.** Steady-radius probe on Bush at v0 = 8/12/16/20 m/s: at gain 0 the shipped
carve is a dead-flat **33.2 / 33.5 / 33.7 / 33.9 m**, tightening to **30 m** with lean-in
(`sim/sled.h:1050-1054`, MEASURED). The invariant is that **lean-in tightens the radius by a
felt margin at every one of those four speeds, and the non-leaned radius does not move at
all**. Plus the pinned gate leg `sled_lean_into_the_carve_tightens_the_radius` must return
to ≤ 0.90 (today 0.9003, missing its own 10 % pin by 0.03 pp).

**Killing mutation.** Sweep the gain and watch the **non-leaned** radius: if the flat 33 m
ladder moves at all, the term is leaking off `align_m` and is not a lean reward. Second, and
the honest limit: the term is inert wherever `plane_lat_gain` is inert — `rho_eff = 0` on
**Road and LakeIce** (`sim/sled.cpp:1083`). So this rung **cannot** answer *"part of what is
so fun on the road is driving by lean"* (SK-1d) and must not be sold as if it does. Third:
raising it past the point where the inside ski's plate makes a roll couple reproduces
`plane_lat_load_frac`'s measured failure (tip onset 0.318 → 0.334 g, and a −28° non-turning
runaway at 16/20 m/s, `sim/sled.h:1081-1089`).

**Drive checklist line.** *"Pick one corner on the bush trail and take it at the same speed
three times: neutral, leaned in, leaned out. Does leaning in now tighten the line — and does
NOT leaning still hold the same line it always did?"*

**Law check.** Depth 0.77 — untouched (the term multiplies lateral plate, not draft). No
governor — the **player's lean is the only input**; the kernel moves nothing. Wheelie kept —
lateral axis only. STAND rights / LEAN throws — untouched; this is lean as *enhancer*, which
is §0's literal ask. Ragdoll banned — n/a. One surface — the term rides the existing
per-surface `rho_eff`, adds no class.

---

### RUNG 4 — THE BARS STOP LYING

**Felt problem, his words.** *"I am not enough able to hold a steady turn as I am using
wsad"* (SK-1d, 2026-08-25, carried verbatim in `app/main.cpp`); felt item 3 *"turning too
unstable, can't hold a carve"* (gi4 §1).

**Tape evidence.** MEASURED, D-E-01: across **301 steer excursions in 9 tapes, ZERO held a
partial angle** (peak < 0.98 held ≥ 0.25 s). The median excursion's plateau is **0.0167 s —
one frame at 60 fps**. Corpus-wide (D-B-4): **1,574 excursions into the 0.3–0.7 band across
206 minutes, and the longest one lasted 0.500 s**; band-dwell p99 is 0.305–0.365 s in every
bucket. The same tapes show the lean axis holding up to **36.1 s**. MEASURED, D-E-02:
`cmd_minus_bar_abs_max = 0.3500` on **5 of 9 tapes** (DERIVED ceiling 0.350), and
`bar_behind_during_decay_p90 = 0.250–0.328` over 570–671 decaying ticks per tape.

**Mechanism.** `app/main.cpp:8244` pushes the bar command at `2.0 * frame_dt`, matching the
kernel's `steer_rate_per_s = 2.0` (`sim/sled.cpp:303`). `app/main.cpp:8253` releases it at
`3.0 * frame_dt` — **50 % faster than the machine can act**. So from full lock the *command*
reaches zero in 0.333 s while the *bars* need 0.500 s, and for that third of a second the
command, the HUD and his hands show him up to **35 % of full lock the skis are not doing**.
SK-1d's own comment, twenty lines above, states the law it then breaks: *"a command that
outruns the slew just queues up and lies to the HUD."* It fixed that on the push and
reintroduced it on the release.

**THE ONE DIAL.** `[sled_input] steer_centre_per_s` — **identity 3.0** (today's literal at
`app/main.cpp:8253`). The rung asks him to rule it to **2.0**.

**Invariant.** `cmd_minus_bar_abs_max` collapses from 0.350 to **one tick of quantization
(≈0.017)** on every tape with no `hands_on` break, and `bar_behind_during_decay_p90` → ≈0.

**Killing mutation.** If `cmd_minus_bar_abs_max` stays at 0.35 after the change, something
other than the release rate is producing the divergence and the finding is wrong. (Tape 91's
0.983 is a `hands_on == false` episode — the kernel forces `steer_target = 0` while the app
command persists — and must be excluded, or it masks the result.) Second: this rung does
**not** create a hold. A hold is `steer_hold_frac` (D-E's E2), and D-E carries an explicit
**do-not-ship-alone** warning on it — M12 measured *"there is no carveable steer angle on
Road at ANY speed tested"*, so a hold shipped before a plant-side rung makes the rollover
**more repeatable**, not less. Rung 4 deliberately stops short of that.

**Drive checklist line.** *"Come out of three corners on the road and three in the bush and
tell me whether the bars now let go at the same speed they take up — or whether straightening
still feels like it happens twice."*

**Law check.** Depth 0.77 — untouched. No governor — this is an **input-layer** change in
Criterion's sense (R2-07: `Input Layer Assists` vs `Simulation Assists`); it shapes what the
key *means* before an untouched kernel, and is out-ridable by construction. Wheelie kept —
steer axis only. STAND/LEAN — untouched. Ragdoll — n/a. One surface — untouched.

---

### RUNG 5 — THE DRIFT YOU CAN HEAR

**Felt problem, his words.** *"I've seen lots of snowmobiles **drift around corners and then
with throttle, straighten out. The feel is really good. Don't lose the feel**"* (§0) — RC-3,
the drift is canon. And *"Snow that is properly shaded and **felt**, not some cheap drawn in
illusion"* / *"The conditions should be **palpable and observable in the sled performance**"*
(`docs/roost_consult_packet.md` §1, 2026-08-27).

**Tape evidence.** MEASURED, D-D-R3 over the six v17 tapes (86 578 T records, 721.5 s):
mean `|track_slip| = 0.522` in contact and moving; the belt runs a mean **20.50 m/s faster
than the ground**; **|slip| > 0.3 on 53.03 % of all ticks** — 70.2 % of RockOutcrop time,
65.4 % of Bush; and **33.77 % of ground-moving ticks have |slip| > 0.3 with `roost_flux` <
0.02**: the track is spinning and there is **neither a plume nor a sound**. Defect D-D-F3:
`track_slip` has **zero consumers** outside the tape and the probe. The single most legible
state a snowmobile has — hooked up vs breaking loose — reaches no output channel at all.

**Mechanism.** `sim/sled.cpp` writes `track_slip` and `belt_speed_ms` into `SledState`;
`grep` finds no reader in `render/` or `app/`. The engine voice is driven by
`sled_synth.set_drivers(mounted ? sled_thumb : 0.0, clamped_dt)` (`app/main.cpp:11820`) — the
player's **thumb**, not the machine. `render/sled_audio.h:294` sets `kSledRunGain = 0.90` for
the riding bed; a fourth layer multiplies to zero before the soft-clip, so a zero gain is
bit-identical.

**THE ONE DIAL.** `render::kSledSlipVoice` (+ `SEADS_SLED_SLIPVOICE`) — **identity 0.0**.
Filtered noise whose level rides `|track_slip| × contact` and whose brightness rides
`belt_speed_ms`. Lives entirely in `render/`; no kernel byte moves.

**Invariant.** The layer's level must correlate with `|track_slip|` and must be **inaudible**
when `track_slip` is near zero (a constant hiss is the failure). Its level must sit **under**
`kSledRunGain`, swept against `render/mix_levels.h`, not chosen.

**Killing mutation.** If `track_slip` were near-constant the layer is a constant hiss and
worthless — it is not (p50 0.395, p90 0.955, so it spans the range). The live risk is the
opposite: at **53 % duty** it can become the loudest thing in the mix. Second mutation, and
this one would kill the rung: pitching the *engine* off `ground_speed_ms` instead of
`engine_rpm` destroys the very rpm-vs-ground divergence this layer exists to expose
(R2-15) — the two channels must stay separate.

**Drive checklist line.** *"Kick the back end out on a corner and feed it throttle until she
straightens — can you now HEAR the moment she hooks up, with your eyes on the trail?"*

**Law check.** Depth 0.77 — untouched. No governor — an output channel touches no force.
Wheelie kept, STAND/LEAN, ragdoll, one surface — all untouched; nothing in `sim/` changes.
**This is the purest "don't ruin it" rung on the ladder: it cannot change the ride, only
what he can hear of it.**

---

### RUNG 6 — OVER THE BACK

**Felt problem, his words.** Felt item 6: *"**flips happen too fast and easy**"* (gi4 §1);
and *"If I lean back and hit a snowbank, **I flip multiple times end over end... like 5x**"*
(2026-08-13) held against, in the same breath, *"**I DO want a wheelie ... even MORE so**"*
and felt item 4 *"WOT should lift the skis to ~30°"*.

**Tape evidence.** MEASURED, D-B-7, under `stand > 0.5 ∧ lean_fwd < −0.30 ∧ throttle > 0.90
∧ air_s == 0` (ground contact — an airborne backflip is excluded by construction): tonight's
pitch is **p50 0.75°, p90 9.25°, p95 14.25°, p99 66.75°, max 85.4°**, with the condition
holding **11.9 % of drive time**. *There is no populated band between "nose down on the snow"
and "past 60° and going over"* — a spike at zero and a tail straight to the flip.
`sled_tape_86` reaches catwalk p95 **66.2°** at 14.7 % duty; `sled_tape_87` reaches **3.8°**
at 15.6 % duty — **the same commanded pose produced either nothing or a flip, on two tapes
four minutes apart.** That bimodality is both the complaint and the licence: a gate that
opens above 60° **cannot touch the 30° wheelie he asked for**, because nothing lives between.

**Mechanism.** There is **no pitch damping anywhere in this kernel.** `grep -nE
"torque_body(\.[xyz])?\s*(\+=|-=|=)" sim/sled.cpp` returns exactly five sites (verified in
`R4-landings.refute.md` R-2): `:602` per-patch (contact), `:1608` C2 (roll, z), `:1655` C3
(yaw-arrest), `:1792` the STAND self-right (roll, z), `:1881` the comfort assist (roll, z).
**Every comfort term in the kernel is a roll or yaw term; pitch has none.** Pitch restoration
comes only from the track's fore/aft normal split (`sim/sled.cpp:916-949`,
`track_pitch_half_m = 0.20`), which saturates the moment one split point unloads — i.e.
exactly when the nose is already up. The one dial that would stiffen it,
`track_pitch_half_m`, is the **wrong** place: `sim/sled.h:605-610` MEASURES that at 0.45 the
rider's fore-aft authority collapses to 0.32× of W/L (felt item 5 again), and its green
window is narrow (0.20 → 0 red; 0.18 and 0.22 → 5 and 7 red).

**THE ONE DIAL.** `[sled_comfort] pitch_arrest_nms` — **identity 0.0** (new; absent today, so
0.0 is the absence of the term and is bit-identical). Shape, exactly mirroring the shipped
C2/A precedent: `τ_y = −gain · ω_y · w_contact · w_band`, with `w_band` a smoothstep opening
from **60°** of pitch and `w_contact` the existing `sim/sled.cpp:1842-1844` gate that is
*"identically 0 airborne"*.

**Invariant.** The catwalk pitch **p99 falls from 66.75° toward the p95 of 14.25°** while
**p50, p90 and p95 do not move** and the catwalk duty (11.9 %) does not move. The backflip
must remain: the summit test's airborne tilt (OPEN-SF1-FLIGHT-ROT, 165.5° off the St. Charles
lip at ~21 m/s) must be **bit-identical**, which it is by the `w_contact` gate.

**Killing mutation.** Set `w_band` to open at 0° instead of 60° — the wheelie dies and felt
item 4 with it; that is the test that the 60° gate is load-bearing, and it must be run and
reported. Second: drop the `air_s == 0` clause from the catwalk measurement and p95 jumps as
airborne backflips contaminate the sample — that clause is what makes this a catwalk
measurement at all (D-B-7). Third, the honest limit: **this rung does NOT address the
snowbank end-over-end.** `gi2tumble` measured aft+stand at **2.8 turns @ 18 m/s and 4.4 @ 20**
against neutral-seated 1.8 which *recovers* — that tumble is largely airborne and a
contact-gated term cannot reach it. The tumble rung was fenced out of GI3 and **was never
built** (D-C O3); it is a separate rung and must not be claimed here.

**Drive checklist line.** *"WOT + stand + lean back on the flats: do the skis still come up
the way you wanted — and does she still go all the way over the back when you hold it?"*

**Law check.** Depth 0.77 — untouched. No governor — a body torque, nothing reads or moves
`rider_lat_m`/`rider_fwd_m`; it is the §0b arcade-lawful shape (*"a contact-gated
roll-stability moment"*, §0b's own example, rotated onto pitch). **Wheelie kept — by the 60°
gate, and the invariant above is what proves it.** Backflip kept — by `w_contact`, which is
identically zero airborne (`sim/sled.h:352`). STAND rights / LEAN throws — untouched (this is
a damping term on ω_y, not a righting torque; it never picks a side). Ragdoll banned —
damping only, no stiffness, so it cannot ring. One surface — untouched.
**⚠ This is the only rung that adds a new kernel term. It is the riskiest on the ladder and
it should not be built before Chad rules on it.**

---

### RUNG 7 — THE ENGINE SINGS THE ENGINE

**Felt problem, his words.** *"**Work on the math to achieve a balance of fun and accuracy to
real physics**, just as our airplane ontological counterpart does"* (§0); *"The conditions
should be **palpable and observable in the sled performance**"* (roost packet §1). And the
audio ask he already gave, carried in `render/sled_audio.h`: *"**Tapping keys gives the brap
brap ramp up**... then chose a tone for the sustained machine sound... Make sure to balance
the sounds."*

**Tape evidence.** MEASURED, D-D §5 and D-D-R1, on the six v17 tapes: the **coast is 8.60 %
of every drive** at mean **19.34 m/s** with the engine at **4 147 rpm**, and during it the
voice sings **5.8 semitones flat** — the tone is following his thumb, which is closed, while
the machine is still turning 4 000 rpm at 70 km/h. On **10.24 %** of the ticks where both
channels move, they move in **opposite directions**. D-B-8: throttle is near-binary — WOT
52 %, closed 40 %, **modulated only 7.3 %** — so the thumb is a square wave and the engine is
not.

**Mechanism.** `app/main.cpp:11820`: `sled_synth.set_drivers(mounted ? sled_thumb : 0.0,
clamped_dt)`. The engine voice's only driver is the **player's key**. `engine_rpm` is a pinned
`SledState` field (`test/harness/sled_tape.h`, `SLEDTAPE_PIN_D`) with **zero consumers** in
`render/` or `app/`. Related lying instrument, D-D-F1: `sim/sled.h:1307-1310` claims the rig
reads `belt_speed_ms`; `grep` shows **nothing does**.

**THE ONE DIAL.** `render::kSledRpmBlend` (+ `SEADS_SLED_RPM_BLEND`) — **identity 0.0**.
`SledSynth::set_drivers` takes a second argument, the kernel's rev fraction
`k = clamp((engine_rpm − 1700)/6300, 0, 1)`, and the follower's target becomes
`mix(thumb, k, blend)`. At 0.0 the call is bit-identical and `test_sled_audio.cpp` passes
untouched. **The onset detector keeps running on the thumb** — the brap is an input event and
must not be moved.

**Invariant.** During coast (throttle 0, `engine_rpm` > 3 000) the voice's pitch must track
`engine_rpm`, not zero; the measured 5.8-semitone gap must close. The **tap response must not
slow**: time from key-down to first brap transient unchanged.

**Killing mutation.** At blend 1.0 the tone stops answering a tap the instant the thumb moves
(the kernel's rpm lags the clutch) — which costs the brap's immediacy, the one thing he asked
for by name. So the dial must be **swept**, not set. And if the measured gap ever falls below
~0.08 (a quarter of the follower's own 0.35 idle-ratio sweep) the change is inaudible and the
rung is dead.

**Drive checklist line.** *"Get her to 70 km/h, let off the throttle completely, and tell me
whether she now sounds like she is still turning — and whether a tap still braps the instant
you tap it."*

**Law check.** Depth 0.77, no governor, wheelie, STAND/LEAN, ragdoll, one surface — **all
untouched**. Nothing in `sim/` changes; this is a `render/` wiring change with a zero default.

---

### RUNG 8 — R HANDS THE BARS BACK STRAIGHT

**Felt problem, his words.** *"I need a key for now that lets me **autoright** until we get
the guy running back to the snowmachine"* (carried verbatim at `app/main.cpp:8273-8274`); and
*"allow me to land on my skis more often after a roll (**even though R works**)"* (§0) — R
works, and he is still unsatisfied.

**Tape evidence.** MEASURED, D-E-06, read straight out of the `O` records and the `T` record
immediately preceding each: **six rightings in the current tapes, zero at centre, FOUR at
`steer_actual = 1.0`** (tapes 86 t5804, 88 t46589, 91 t6372, 91 t15130). R sets the machine
upright, on the surface, with all motion killed — **and with the handlebars still hard over
and the command still asking for hard over.** The moment he opens the thumb (0.400 s to WOT)
he is in a full-lock turn he did not ask for, on a machine M12 measured as having *"no
carveable steer angle on Road at ANY speed tested"*. Scale, corpus-wide: **146 R presses, 144
of them at a body tilt ≥ 75.06°** — the key is used exactly as designed, not as a teleport.

**Mechanism.** `app/main.cpp:8355-8357` — the R block zeroes `sled_lean_lat` and
`sled_lean_fwd` and **does not zero `sled_steer_cmd`**. Twenty lines above, `app/main.cpp:8263-8267`,
the C key zeroes **all three**, with the comment *"SK-1c: C recentres the bars too"*. The two
recovery keys disagree, and R — the one used after a crash — is the one missing the line.

**THE ONE DIAL.** `[sled_input] right_recentre_steer` — **identity 0.0** (= today's absence
of the line). At 1.0, R recentres the same three commands C already does.

**Invariant.** No `O` record is followed by `steer_actual` > 0.1 sustained past **0.500 s**
(the kernel's own bar slew time from full lock at `steer_rate_per_s = 2.0`).

**Killing mutation.** The kernel's `steer_actual` is not in the override's reset list, so the
`O` record's own tick will still show a non-zero bar — the test is the **0.5 s after**, not
the tick itself. If the next tapes still show `steer_actual` pinned at 1.0 for > 0.5 s after
an `O`, the app-side command was not the cause and this is wrong. Second: `app::autoright_legal`
gates the key, so presses the guard rejects never reach the tape at all — a legality change
silently changes what the count means.

**Drive checklist line.** *"Next time you press R after a roll, don't touch anything for one
second, then give it throttle — does she now go straight instead of immediately cutting hard?"*

**Law check.** Depth 0.77 — untouched. No governor — it zeroes a **player command**, exactly
as C already does; it moves no mass and applies no force. Wheelie kept, STAND rights / LEAN
throws (`sim/sled.cpp:1731-1742`'s latched-brace law untouched), ragdoll banned, one surface —
all untouched.

---

## 2. WHAT THIS LADDER DELIBERATELY DOES NOT TOUCH

Named so nobody has to rediscover why.

- **`roll_damp_nms` (800).** The single biggest arcade-lawful lever in the kernel and the
  ladder refuses it. `sim/sled.h:362-365` says why in its own words: *"★ SHIPPED AT ZERO on
  the consult's do-not list: damping is the dial that kills **the flick, the drift's body
  language** and backflip initiation off a lip."* It was raised to 800 on Chad's own
  2026-08-13 ruling (`b971d14a9`), and the arc-vs-flick ladder (Bush lean-in carve **323 m at
  0, 143 m at 150, 42 m at 400, 30 m at 800**, MEASURED `sim/sled.h:378-382`) is a **ruling,
  not a bug**. My angle — *don't lose the feel* — puts it out of scope. It belongs in §3 as a
  ladder to show him, not as a rung.
- **`track_pitch_half_m` (0.20).** Identity 0.0 is the pre-S2b single-point track, MEASURED
  to run the wheelie away to **89.9° at t = 6 s, never coming back** (`sim/sled.h:576-581`).
  Green window is narrow. Not a place to buy the wheelie back.
- **`k_gyro`, `k_gyro_react`, `k_air_shift`** — all three airborne. `backflip untouched` is
  the angle's own fence, and R4-14 shows arming `k_gyro_react` today buys only the **harmful
  half**: `rep_belt = max(dv·throttle·track_speed_max_ms, max(v_bf, 0))` (`sim/sled.cpp:1354`)
  cannot fall below body forward speed, so **throttle gives nose-up and the brake gives
  nothing**. See ruling R6.
- **`steer_hold_frac` / a real bar hold (D-E's E2).** Carries D-E's own do-not-ship-alone
  warning: with no carveable angle on Road, a hold makes the rollover *more repeatable*.
- **`lean_px_full` (300).** D-E-05's correlation is n = 9 and **does not reach p < 0.05**
  (r = +0.815, partial +0.693 vs a 0.707 critical value), and his mouse CPI is unknown. See
  ruling R7.
- **A landing-damper knee.** R4's whole force/pressure/damping arithmetic is **26.4 % low** —
  it used 4104 N where the kernel uses **3246 N** (`R4-landings.refute.md` R-1). The corrected
  mechanism survives (the landing spike is the damper, linear at **3.33 g per m/s of sink**,
  no blow-off, `sim/sled.cpp:705`) but **no number of that strand may be quoted yet**, and
  Chad has never complained about landing harshness — he asked for *"punchy"*. Not a rung
  until there is a word.

---

## 3. RULINGS CHAD MUST GIVE FIRST

Ordered. Nothing on the ladder should be built before R1–R3 are answered.

**R1 — `class_blend_m`: rule it.** `LANES.toml:939`: *"⚠ THE STICK RULING: [snowpack]
class_blend_m STAYS 0.0 — it is live-tunable and it is **CHAD'S A/B, not the lane's call**.
The lane measured 1.0 as the value it would pick; it ships 0.0 and he decides in the seat."*
It is the top rung, it is already tunable from `config/world.toml:645`, and it costs nothing
but one drive.

**R2 — the STAND self-right at 2400 N·m: is that the machine you want?** Three unanswered
things about one block (`sim/sled.cpp:1679-1800`). (a) **The value was never ruled.** The
comment directly above it in `config/scenario.toml` derives **1500** (*"1500 gives a one-press
ceiling of 18.6 + asin(1500/1932) = 70 deg... Chad rules the final value on his drive"*) and
the next line ships **2400** — both entered in the same commit `8e8c6f67d`; the kernel default
is **0.0** (`sim/sled.h:224`) (D-C C2). (b) **Its tilt gate reads gravity, not the hill.**
`sim/sled.cpp:1680-1687` uses `acos(up_body.y)`, so a machine sitting *conformal on a
side-hill* reads the slope as "tipping over": on a 20.05° slope `w_tilt = 0.9997`, and
`config/world.toml`'s own snowpack measurement records terrain slope **p95 = 21.1°** over
79 847 land samples — **~5 % of the world's land is at or past the full-authority edge of this
gate**, and STAND is held hard on **6.8–39.0 %** of his drives. Against his own §0: *"allow
leaning a certain way ... for better traversing, **say up a hill**."* (c) **There is no contact
gate at all** — the only torque in the kernel that can fire airborne (`R4-landings.refute.md`
R-2), shut off only by a low-passed **horizontal** speed gate, so a near-vertical drop reaches
it. Your call: keep 2400, or rule the gate to `|phi_surf|`, or both.

**R3 — give the last six drives a word.** D-C O14, and R3's UNVERIFIED-1: **there is no felt
report for tapes 86–91** (2026-09-17, 20:11–21:39). Every felt attribution older than five
weeks rests on the 512 s tape of a binary that did not contain the rollfix. **Until you say
one line per tape, no number in this audit licenses a dial move.** One line each is enough:
"good", "rolled too much", "didn't notice".

**R4 — name the failure-cost bar.** Today a crash costs **p50 7.02 s, p90 18.13 s, max
38.4 s** to get back to 80 % of the speed you carried in, against *"there is a batttle going
on as well"* (§0b). R3-P2 proposes **p50 ≤ 4 s, p90 ≤ 8 s** and labels the figures a **GUESS**
(they are half of today's, a reachable step, not a derived optimum). Adopt a number, or rule
"just drive it and I'll tell you."

**R5 — the drift ladder.** Before anyone touches `roll_damp_nms`, look at the MEASURED
arc-vs-flick ladder (Bush lean-in carve **323 m @ 0 / 143 @ 150 / 42 @ 400 / 30 @ 800**) and
say whether 800 is still the trade you want. **This ladder proposes no move** — it asks
whether the number you ruled on 2026-08-13 is still your number.

**R6 — do you want to steer in the air at all?** Three dials are written, commented and
shipping at zero (`k_gyro`, `k_gyro_react`, `k_air_shift`), and `k_gyro_react` is the same
term Rainbow ships by name as **Reflex Gyro** in *MX vs ATV*. But today the kernel can only
give the **nose-up** half (R4-14), which is the half that makes a landing worse. Rule whether
in-air control is wanted; if yes, the belt expression (`sim/sled.cpp:1354`) is the blocker and
must be fixed first, not the dial.

**R7 — your mouse.** CPI and Windows pointer sensitivity. D-E's *"600 raw counts full-scale =
~19 mm of desk travel"* assumes 800 CPI and is a **GUESS** in that factor. No lean-scale
number can be picked without it.

---

## 4. WHAT THIS LADDER REFUSES TO CLAIM

- **No rung here has been built, measured in a probe, or flown.** Every "invariant" is a test
  to run, not a result.
- **Every rung's ranking is a judgement**, not a measurement. Chad gave no priority ranking;
  comfort, fun and feedback are weighed equally per the audit brief.
- **The R1-10 "slides before it rolls" reading is NOT used.** Its refutation stands: the
  shipped table gives ski `mu_lat` **0.55 Bush / 0.70 TrailMain / 0.72 TrailTributary**
  (`sim/sled.cpp:162-193`) against an SSF of ~0.51, so a friction-driven flat-ground roll is
  available **by construction** on four of seven surfaces.
- **The R1-19 "friction circle" mechanism for the drift is NOT used.** There is no friction
  circle in this kernel (`sim/sled.cpp:1057-1059` reads normal load and slip angle only).
  Whatever produces the drift Chad signed, it is not that — which is exactly why Rung 5 makes
  the drift **audible** rather than trying to tune it.
- **The "air is a tenth of the game" and "29 % land upright = a failed jump" readings are NOT
  used.** 93 % of the counted air events are sub-second hops (hang p50 0.39 s; only 4 of 58
  exceed 1.0 s), so those are bump statistics, not jump statistics.
- **The R3-M8 "combat and riding are serialized" framing is NOT used** — refuted:
  `app/main.cpp:7907` and `app/player_mode.h:283` make P deployable **from the seat**, carrying
  Chad's own 2026-09-04 word. The mouse therefore has a **third** claimant (lean / freelook /
  sting aim, three-way exclusive) that no strand's control-budget table lists.
- **D-B-6's exit-surface mutation was not run**, so the trail attribution is *"at the trail"*,
  not *"on the trail"*. Rung 1 is written to survive that either way; nothing else here leans
  on it.
- **No probe was built and no tape was written.** Every number is read from source in this
  worktree or from a tape Chad recorded, opened read-only.

---

*Ladder feel-first, sled ride audit, 2026-09-18. Read-only against main. No dial changed, no
config edited, no goldens moved, nothing built, nothing pushed.*
