# CONVERGED LADDER — angle: PHYSICS-HONEST

> **The angle, stated once.** *Make it real and it gets stable.* Every rung below is a
> **physical term the real machine has and this kernel lacks or mis-scales** — not a fudge
> factor, not a governor, not an arcade patch. Where the honest term also happens to be the
> arcade-lawful one §0b licenses, that is stated, not hidden.

**Scope and law.** Read-only against main. Audit worktree `D:/seads_sandboxes/sled-audit`,
branch `audit/sled-ride`, kernel text byte-identical to `origin/main` (verified by D-A's
`git diff --stat HEAD origin/main -- sim/sled.h sim/sled.cpp sim/rider_grip.h` returning
empty). **Nothing here is built.** No dial moved, no config edited, no test run, no probe
built, nothing pushed. Every rung names ONE dial and its identity value and stops there.

**Confidence vocabulary.** MEASURED · DERIVED · LITERATURE · GUESS (labelled inline).
**Every numeric claim carries its killing mutation.** A claim without one is decoration.
**No feel recommendation rests on a harness number alone** — each rung cites a tape event
AND one of Chad's verbatim words.

**Sources converged.** D-A (kernel mechanism), D-B (tape forensics, 84 tapes / 206.1 min),
D-C (felt history), D-D (feedback loop), D-E (input map), R1 (real dynamics) + its refutation
pass, R2 (game feel) + refutation, R3 (player experience) + refutation, R4 (landings) +
refutation. Where a refutation pass killed a claim, **the refutation wins** and is cited.

---

## 0. THE CHARTER, VERBATIM — the bar every rung is measured against

`D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0**
(drive 4, 2026-08-12):

> *"Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more often
> after a roll** (even though R works). But not every time — allow it to happen. Also I've
> seen lots of snowmobiles **drift around corners and then with throttle, straighten out. The
> feel is really good. Don't lose the feel**, except the constant rolling. **Make it possible
> to roll but not the rule.** However the game feels like a nicely made sim as there is
> dynamic room and **the balance mechanism works really good. Just allow the balance of body
> mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be the necessary
> condition of not rolling over.** Just allow leaning a certain way in a particular condition
> to be the OPTIMAL weight distro for better traversing, say up a hill or around a corner.
> **Work on the math to achieve a balance of fun and accuracy to real physics, just as our
> airplane ontological counterpart does.**"*

Same file **§0b** (superseding, 2026-08-12 night):

> *"no I dont like thge idea in the handoff at all! It will ruin the feel to have a governor.
> **Make it less honest but dont ruin it.** Get a fable consult and the measure of it shall be
> if my intent is heard. **Leaning shall enhance the ride an just make it more stable and slef
> righting by chance more.** It should be arcadey to a degree so that it is fun., **SLiding
> banging, punchy, jumps. Just make it more stable.**"*

> *"please build autmatically and make this kernel way better ... **I am afraid it might ruin
> something that is reallt good right now but just dont ruin it.** Allow for bad driving too
> but keep the benefit there for good riding. **DOnt make it impossibly hard, there is a
> batttle going on as well.** And the point is this should be fun as well, not a true sim. But
> I do want the finess and all the mechanisms available for tuning ..."*

§0b's own decode, which this ladder treats as binding: **"NO GOVERNOR. No automatic rider
micro-balance, no autopilot moving the rider's mass. The player's lean stays the only thing
that moves the rider."** And **"RC-8 — EVERY MECHANISM IS A DIAL."**

**★ The licence, and why a physics-honest ladder is still allowed.** §0b says *"Make it less
honest but dont ruin it"* — RC-6 is dead as written. So **a departure from reality is not, by
itself, a defect.** It is a defect only when it also costs one of his named feels, or produces
the one thing he named as broken (*"the constant rolling"*). Every rung below is filed against
that test, never against reality alone. (This caution is R1 §9.1's, adopted here in full.)

---

## 1. THE ONE-PAGE MECHANISM, DERIVED — why this machine rolls

This is the spine the whole ladder hangs on. All four numbers were **recomputed this session**
from the shipped parameters, not quoted.

**(a) What it takes to tip this machine.** The support polygon is a **trapezoid** — two skis
wide and forward, one track narrow and aft — so the tipping line is the diagonal from the
downhill ski to the downhill rail. With `stance_m 0.927`, `ski_fwd_m 0.86`, `track_aft_m 0.52`,
`track_rail_half_m 0.19`, `cg_height_m 0.564` (`sim/sled.h:543-569`):

| tipping line | lateral arm | SSF (= lateral g to tip) | static tip angle |
|---|---|---|---|
| ski centreline → rail outer | **0.28747 m** | **0.5097 g** | **27.01°** |
| ski outer → rail outer | 0.30919 m | 0.5482 g | 28.73° |

DERIVED, exact arithmetic. *Killing mutation:* move `cg_height_m` or `track_rail_half_m`.
At the comment's real-Indy rail half of 0.14 m the first row falls to 0.452 g / 24.3° — i.e.
**the kernel is 12.7 % more roll-resistant than the machine its own comment describes**
(R1-26). Correcting that would move *against* Chad's ruling and is not proposed.

**(b) What the kernel's snow is allowed to push sideways.** `sim/sled.cpp:1056-1059`:

```
const double mu_l = g.is_track ? p.track_lat_mu : d.mu_lat;
double bite = -normal * mu_l * std::tanh(slip_ang / max(p.slip_ref_rad, kEps));
```

and the shipped surface table, read this session at `sim/sled.cpp:162-193`:

| surface | ski `mu_lat` | track `mu_lat` (constant, `sled.h:1193`) | vs the 0.5097 g tip |
|---|---|---|---|
| TrailTributary | **0.72** | 0.70 | **1.41× over** |
| TrailMain | **0.70** | 0.70 | **1.37× over** |
| Road | **0.60** | 0.70 | **1.18× over** |
| RockOutcrop | 0.60 | 0.70 | 1.18× over |
| MineWorks | 0.58 | 0.70 | 1.14× over |
| Bush | **0.55** | 0.70 | **1.08× over** |
| LakeIce | 0.22 | 0.70 | 0.43× (under) |

**Six of seven surfaces ship a lateral coefficient above the machine's own static tip
threshold.** An **untripped, flat-ground, hands-off friction rollover is available by
construction** — it is the shipped table, not a trip and not a solver artefact. MEASURED (the
table) + DERIVED (the comparison). *Killing mutation:* any of those seven rows reading below
0.5097, or the rollover proving to be normal-load-starved in practice (the probe leg, §5).

> ⚠ This inverts R1's self-declared central finding. R1-10 claimed *"the sled slides out
> before it rolls; any flat-ground roll is a trip or an artefact."* **`R1-real-dynamics-REFUTE.md`
> §1 refuted it against this same table and this ladder adopts the refutation.** R1-10's
> *real-machine* half survives and is the reason the mis-scale matters.

**(c) The inside ski lifts long before the tip, and that is correct.** Front roll stiffness
= `2·susp_k·(stance/2)²` = 19 764.6 N·m/rad; rear (rail split) = `2·susp_k·rail_half²` =
3 321.2 N·m/rad ⇒ **φ = 0.856** of the roll is reacted at the front. Inside-ski lift at
`a = (ski_share/2)·stance/(h·φ)` = **0.362 g**. DERIVED. So the shipped machine lifts a ski at
0.36 g, and then — because the surface can still supply 0.55–0.72 — **keeps going to 0.51 g and
over.** On a real machine the lift is where the story *ends*, because the snow runs out first
(R1-20, R1-29). *Killing mutation:* `susp_k` differing front/rear, or `track_rail_half_m = 0`
(the split branch is not taken and φ → 1.0, lift at 0.310 g).

**(d) And the landing spike multiplies (b).** `sim/sled.cpp:705` —
`normal = susp_k*x + susp_c*xdot`, with `xdot = -dot(v_patch, up_i) / axis_dot` and
`axis_dot = max(dot(body_up, up_i), 0.25)` — **linear, unbounded, and up to 4× the CG's own
sink rate on an off-angle arrival.** The tangential bite is applied at the *contact point*
(`bite_at_contact_frac 1.0`, `sled.cpp:988-993`), which sits exactly `cg_height_m` below the
CG. So a landing does not merely bang:

| sink rate | damper normal, one patch | lateral it unlocks (μ 0.70, slip 0.30 rad) | roll torque at 0.564 m | vs gravity's own peak tipping torque (1931.8 N·m) |
|---|---|---|---|---|
| 2 m/s | 7 200 N | 3 838 N | 2 165 N·m | **1.12×** |
| 4 m/s | 14 400 N | 7 677 N | 4 330 N·m | **2.24×** |
| 8 m/s | 28 800 N | 15 354 N | 8 660 N·m | **4.48×** |

DERIVED from shipped constants. **The landing spike is a lateral-force multiplier, and the
lateral force is what tips him.** That is the mechanism behind "landings mostly fail".
*Killing mutation:* `bite_at_contact_frac = 0.0` moves the arm to 0.354 m and the 4 m/s row
falls to 1.41×; or a touchdown slip angle systematically below ~0.05 rad, which makes `tanh`
kill the term — **that is a probe question, not a tape question** (§5, leg D-1).

**(e) One patch, two unrelated friction laws.** Longitudinally the track is **Mohr-Coulomb on
the snow**: `tau_max = c_eff + sigma·tan(phi_deg)`, budget `area·tau_max` (`sled.cpp:1167-1174`).
Laterally it is **Coulomb on a constant**: `normal · 0.70` (`sled.cpp:1056`). Neither consults
the other. On TrailMain at rest load (track carries 0.86/1.38 = 62.3 % of 3246.0 N = 2022.9 N):
budget **4 974 N**, thrust cap **2 272 N** (`max_thrust_n`), lateral demand **1 416 N** — a real
contact patch would have to share one budget; this one spends both. DERIVED. *Killing mutation:*
find any term coupling them; `grep` over `sim/sled.cpp` returns only the per-surface **brake**
budget (`mu_brake`, 1232-1246), which caps longitudinal braking and never charges lateral. This
is the refutation `R1-real-dynamics-REFUTE.md` §1 ran on R1-19, adopted here.

**(f) The denominator, once, so no rung repeats the error.** The machine weighs **3246.0 N**
(331 × 9.80665; `rider_mass_kg` is split OUT of `mass_kg` and never enters a force). Every
"of its 4106 N" in `docs/gi4_ride_handoff.md` understates the contact collapse by 26.5 %
(D-A §2.4, R4 refutation R-1).

---

## 2. THE LADDER

Fixed shape per rung: **felt problem in Chad's words → tape evidence → mechanism (file:line) →
THE ONE DIAL with identity value → measurable invariant + killing mutation → one
drive-checklist line → law check.**

"Identity value" means **bit-identical**: at that value the shipped tree is reproduced byte for
byte, per the `kernel-v14-leanlead` precedent (ONE dial, identity = the old tree).

**Where a dial is new**, the rung also carries the RC-8 plumbing: the key lands in
`[sled_comfort]` (the only block besides `[snowpack]` that reaches the kernel), with a
`require(...)` in `config/load_scenario.cpp` **and its own `CHECK` in
`test/unit/test_load_scenario.cpp`** — because that TEST_CASE currently covers only 22 of the
35 loaded keys and all three of main's config-vs-default drifts hide in the 13 it skips
(D-A §4).

---

### RUNG 1 — **The snow can push sideways harder than the machine can stand up**

**Felt problem, his words.** §0: *"she's too unsteady … **I rule that it should be roll
resistant.** … Don't lose the feel, **except the constant rolling**. **Make it possible to roll
but not the rule.**"* §0b: *"**Just make it more stable.**"*

**Tape evidence.** MEASURED, D-B: **4.32 past-90° rollovers per minute on the v17 tapes — one
every 13.9 s — and 5.74 % of the drive spent past 90°** (52 events in 721.5 s). And the signal
is **surface-localised in exactly the direction the table predicts**: pooled over the whole
corpus, **TrailMain 16.88 rollovers/min (ski μ 0.70) vs Bush 4.13 (μ 0.55) vs LakeIce 0.85
(μ 0.22)**, with TrailMain exceeding Bush in **12 of 12** tapes carrying >20 s of both
(p ≈ 0.00024 under equal rates), across four kernel buckets and four weeks. `sled_tape_91`
ends with the machine **upside down at 131.4° tilt, 0.41 m/s, clean exit** — he shut the night
down on its roof.

**Mechanism.** `sim/sled.cpp:1056-1059` (the bite law) against `sim/sled.cpp:162-193` (the
surface table) and `sim/sled.cpp:56-81` + `sim/sled.h:543-569` (the trapezoid). §1(b) above:
six of seven surfaces ship μ_lat **above** the kernel's own 0.5097 g tip. §1(c): the machine
lifts a ski at 0.362 g and has nothing left to stop it at 0.510 g.

**THE ONE DIAL.** `[sled_comfort] lat_mu_scale` — a single multiplier on the *steered*
patches' lateral coefficient at `sled.cpp:1056` (`mu_l *= p.comfort.lat_mu_scale`).
**Identity value: 1.0 — bit-identical.**
For scale, DERIVED: the scale that brings each surface to the 0.5097 g tip is **0.708**
(TrailTributary), **0.728** (TrailMain), **0.850** (Road), **0.927** (Bush). At **0.70** the
snow rows read 0.385–0.504 — under the tip on every surface, and inside the measured
winter-surface lateral band (**LITERATURE**, 0.18–0.28 car-tyre lateral on winter roads;
0.32–0.42 g snowmobile locked-track longitudinal — and note that band is a **splice**, flagged
by R1's own refutation: **no measured snowmobile lateral-g number is in hand**). **No value is
proposed here. The number is his, off a sweep and a seat.**

**Invariant.** On the probe's L1 leg (charter §5): hands-off, full lock, TrailMain,
v ∈ {8, 12, 16, 20, 25} m/s — **peak |roll| stays under 20° and the machine reaches a steady
slide** (RC-1). AND the L2 drift trace stays in band (RC-3). AND **WINTER_LAW §2.4c.1's bank
still rolls him**: a 25° side approach at 16 m/s must still go over (*"A too steep side
approach at speed should also roll the snowmachine"*) — that is the "possible to roll" half,
and it survives because a slope adds `tan θ` to the demand directly, with no μ involved.

**Killing mutation.** Set `lat_mu_scale = 1.0` → `mu_l` byte-identical, every golden row
unmoved. **And the claim's own killer:** if the L1 probe shows hands-off full lock **already**
fails to roll on TrailMain at scale 1.0, the mechanism is normal-load-starved (planing lift
holds the patches at a few percent of weight) and this rung is wrong. **The tapes cannot settle
it — Chad never drives hands-off, so RC-1 has no tape trial anywhere in 206 minutes (D-B §6).
This is probe-only and the probe leg is owed.**

**Drive-checklist line.** *"On the groomed trail at ~15 m/s, hold full lock hands-off the mouse
until it settles: does she slide around like you asked, or still go over?"*

**Law check.** Depth 0.77 untouched (`[snowpack]` not opened). No governor (a coefficient, not a
torque; nothing reads or moves the rider's mass). Wheelie kept (`track_pitch_half_m`, lean
authority and the aft ceiling untouched). STAND rights / LEAN throws untouched. Ragdoll not
involved. One surface: the per-surface table is the existing `SurfaceDials` roster read off the
one `drive_surface = terrain + depth`; no second ground truth.

**Risk, named.** This is the rung that can *lose the feel*. Lateral μ buys cornering force as
well as rollover, and O5 (*"turning too unstable, can't hold a carve"*) is already unpaid —
M12 measured **no carveable steer angle on Road at any speed**. Lower μ pushes the nose wider
before it lets the tail go. §0 pulls both ways in one breath: *"Let me slide around a bit
arcade"* against *"Don't lose the feel."* **This trade is a ruling, not a measurement.**

---

### RUNG 2 — **The landing damper is linear, unbounded, and it is a lateral-force multiplier**

**Felt problem, his words.** §0b: *"**SLiding banging, punchy, jumps.** Just make it more
stable."* — punchy is **wanted**, in the same sentence as more stable. And `docs/gi4_ride_handoff.md`
§1 item 6, verbatim: *"**flips happen too fast and easy**"*; item 2: *"**should be able to launch
in the air**."*

**Tape evidence.** MEASURED, D-B-9: **58 air events on the v17 tapes, 26 of 58 landings above
10 g peak |g_eff|, max 600.9 m/s² (61 g, tape 91); only 29.3 % land upright.** And it has not
moved: landed-upright was **27.6 % at v13g** four weeks and three signed kernels earlier
(D-B-2). R4-03: landing severity rises **monotonically with hang time** — the physics is intact,
the ceiling is missing.

**Mechanism.** `sim/sled.cpp:705` — `normal = susp_k*x + susp_c*xdot`, `susp_c = 3600`
(`sled.h:886`), **no blow-off, no ceiling**; `xdot` divided by `axis_dot ≥ 0.25`
(`sled.cpp:650-653`) so an off-angle arrival scales it up to **4×**. DERIVED: **3.33 g per m/s
of sink rate**, before the spring has moved at all, and ζ = **0.799** per patch — a linear
damper at a damping ratio a road car would call extreme, applied to 5–16 m/s shaft speeds.
LITERATURE: real dampers are **digressive** (`n` between 0.5 and 0.7, shim-stack blow-off)
precisely so high-speed impacts do not pass a linear force; *"nearly every modern race damper
above club level is digressive."* **And §1(d): that spike multiplies the bite of Rung 1 — a
4 m/s arrival unlocks 2.24× gravity's own peak tipping torque in lateral force alone.**

**THE ONE DIAL.** `[sled_comfort] susp_blowoff_frac` — above a knee derived from the shipped
constants (**not** a second literal: the knee is the shaft speed at which the damper force
equals the spring's full-stroke force, `susp_k·susp_travel_m/susp_c` = 46000·0.26/3600 =
**3.322 m/s**, DERIVED), the damping coefficient tapers toward
`susp_c·(1 − susp_blowoff_frac)`. **Identity value: 0.0 — bit-identical** (the taper multiplies
to 1.0 and the expression at 705 is unchanged for every shaft speed).

**Invariant.** **Trail chop is bit-unchanged.** At shaft speeds below the 3.322 m/s knee —
which is the whole of ordinary riding, and the ride Chad signed — the force must be identical
to today's to the last bit. Above it, the probe's landing leg on a 1.7 s send (his measured p90
hang) reports peak |g_eff| down and the post-landing tilt at +0.5 s down, **with the bump-stop
path untouched** (`susp_stop_k` stays the 6× rate past `susp_travel_m`).

**Killing mutation.** Set `susp_blowoff_frac = 0.0` → every existing suspension golden unmoved.
**And the claim's own killer:** if a fit record exists for `susp_c = 3600` that fitted it to
*landing* behaviour rather than trail ride quality, the reading changes — **R4 searched `docs/`
and found none** (R4 §6 item 2). Second killer: if the pack (not the shock) already takes the
impact, the damper never sees these shaft speeds — the pack's realised absorption during a
0.1 s impact is **the single largest unmeasured number in the whole audit** (R4-08, probe leg
D-1, `z` is clamped to `min(d_total, z_cap)` with `d_total ≤ susp_rest_m 0.21`).

**Drive-checklist line.** *"Send the same jump twice — hard flat landing then into the deep
stuff — does the hard one still bang, and does it still throw you sideways when you land a bit
crooked?"*

**Law check.** Depth 0.77 untouched. No governor (a damper valve, not a torque, and it cannot
act on an airborne machine — there is no contact). Wheelie kept. STAND/LEAN untouched. Ragdoll
banned and not involved — **and note the buck is SIGNED on the PAIR, not the gain**
(`buck_gain 0.02` / `decay_per_s 3.0`, 2026-08-30 *"just the right amount of novelty
suprise"*): the buck reads `a_body`, so a softer spike is a **quieter buck**, and that is a
felt regression risk this rung must A/B, not assume away. One surface: untouched.

**Risk, named.** *"punchy"* is in his signed sentence. A knee that is too low takes the bang
out of the game he asked for. That is why the knee is DERIVED from the machine's own spring,
not chosen, and why the invariant pins trail chop bit-identical.

---

### RUNG 3 — **A track cannot push harder than the snow it is standing on**

**Felt problem, his words.** 2026-08-24, verbatim (quoted into `sim/sled.h` at `traction_mu`):
*"**at high speeds, getting pulled in and flipping out like 15x is not desirable so yes take
care of that**"* — and in the same ruling the lift itself **stays**: *"it is important for
traversing atop the snow"*, *"at slower speeds I think it is good too because [I'd] like to
jump the snowbanks."*

**Tape evidence.** MEASURED, D-B: **WOT 52 % of tonight's drive time** and `sled_tape_91` at
**72 % WOT with 23 rollovers in 173 s** — he rides with the thumb pinned, which is exactly the
regime this ceiling governs. D-C O8: his ruling was **taken**, the mechanism was **built**, and
it **ships at 0.0**.

**Mechanism.** `sim/sled.cpp:1211-1222`. Both existing caps on thrust are **ENGINE** ceilings
(`max_thrust_n 2272`, `engine_power_w/|v|`). The contact ceiling is a branch gated on
`p.traction_mu > 0.0` and it is the **only** one the snow gets a say in. The source names the
loop in its own comment: *"`shear`'s cohesion term (area*c_eff) and `roost_thrust` are both
load-INDEPENDENT, so without this a track carrying 55 N still pulls at max_thrust_n — the term
that feeds the speed→lift→contact-down→speed runaway."* At the §9 attractor the running
surfaces hold **62 N of 3246.0 N — 1.91 % of the machine's weight** — while the drivetrain is
still pegged (D-A §2.4; note the corrected denominator).

**THE ONE DIAL.** `traction_mu` (`sim/sled.h:1006`). **Identity value: 0.0 — bit-identical**
(a branch; `≤ 0` is the pre-item-2 kernel exactly). **MEASURED sweep already on the books**
(`docs/snowform_measurements.md` §M8.1): at **μ ≥ 0.6** the Bush runaway does not develop
(`t(|roll|>20°)` 6.22 s → 1.33 s; `v_end` 24.8 m/s *rising* → 0.1 m/s), and at **μ ≥ 3.0**
Chad's own traverse is **byte-identical to baseline** (14.1 / 14.5 / 15.3 m/s). So a value
exists that pays his ruling and costs his traverse nothing.

**Invariant.** The emergence matrix and trail-speed sweep rows stay inside the charter's §2
bit-identity tolerance (`track_clearance 0.255`, Bekker constants, trail speeds are SEALED),
AND the §9 Bush runaway leg terminates (`v_end` falls instead of rising), AND the snowbank jump
he asked to keep still launches.

**Killing mutation.** `traction_mu = 0.0` → the branch at `sled.cpp:1219` is dead, bit-identical.
**And the honest limit, stated by the measurement itself:** M8.2/M8.3 record it is **inert on
Road by construction** (`rho_eff = 0`, no planing, full contact) — so **it does not fix the
bank-strike superspin Chad reported.** Anyone selling this rung as the fix for that is selling
the wrong thing (that one is `class_blend_m`, §4 ruling 3).

**Drive-checklist line.** *"Pin it wide open across the deep bush at speed — does she still get
pulled in and flip out, or does she just run out of push?"*

**Law check.** Depth 0.77 untouched. No governor (a friction ceiling on thrust, not a torque;
nothing moves the rider). Wheelie kept — **and check it**: the launch wheelie stands on the
track's thrust, so the sweep must show WOT+stand+lean-back pitch not collapsing (his item 4
ask, *"WOT should lift the skis to ~30°"*, is already unpaid at a measured p50 3.6°). STAND/LEAN
untouched. Ragdoll not involved. One surface: reads the existing per-patch `normal`.

**Risk, named.** This is the cheapest rung on the ladder (built, measured, ruled) and the one
most likely to be mis-sold. Its measured win is the **Bush planing runaway**, not the Road roll.

---

### RUNG 4 — **"Tipping over" is an attitude relative to the ground you are on, not to the planet's centre**

**Felt problem, his words.** §0: *"Just allow leaning a certain way in a particular condition to
be the OPTIMAL weight distro for better traversing, **say up a hill** or around a corner."*
And his self-right spec, verbatim in `config/scenario.toml [sled_comfort]`: *"IF ON THE SEAT
AFTER A ROLLOVER PRESSING THE STAND BUTTON … **MACHINE NEEDS TO BE TIPPING OVER** … NOT ABOVE
5KM/H … IT CAN FAIL TO RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD."*

**Tape evidence.** MEASURED, D-B/D-A §6: **STAND is held hard (`in.stand > 0.5`) for 6.8–39.0 %
of the v17 drives, mean ≈ 24 %** — STAND is a *routine riding control* on his tapes, not a
rescue key. The catwalk pose (stand ∧ hard lean-back ∧ WOT ∧ grounded) fires on **11.9 %** of
tonight's ticks. So the gate below is armed during ordinary riding, not only during a crash.

**Mechanism.** `sim/sled.cpp:1680-1687`: `tilt = acos(up_body.y)` where `up_body = Rᵀ·up_cg` —
**tilt off gravity**, so a machine sitting perfectly conformal on a side-hill reads the slope
angle as "tipping over". DERIVED: on a conformal **20.05°** slope `w_tilt = 0.9997` — full
authority. `config/world.toml`'s own snowpack block records terrain slope **p95 = 21.1°** over
79 847 land samples, so **~5 % of the world's land is at or past the full-authority edge of this
gate.** The kernel fixed exactly this defect everywhere else and said why
(`sled.cpp:1367-1371`: *"red-team P1-1/2: the earlier gravity-tilt gate armed rigid hull
contacts during ordinary slope riding and misread pitched crashes as rollovers"*) — the hull
terms were moved to `phi_surf`; **the self-right was not.** Two aggravators, both MEASURED:
(i) `right_dir_eps` ships **0.04** (a **2.29°** dead-cone, not the documented 10°), so the
direction term is effectively `sgn()`; (ii) the block writes **up to 0.35 m of rider lateral
displacement** through `right_shift_cmd` (1791 → consumed 1401-1407) — *the kernel moving the
rider's mass*, which is the thing §0b forbids by name, authorised only by his later *"MACHINE
NEEDS TO BE TIPPING OVER"* ruling — **and the gate deciding "tipping over" is the broken one.**

**THE ONE DIAL.** `[sled_comfort] right_tilt_surf_frac` — blends the gate's reference from
radial up toward the contact plane already computed and in scope at `sled.cpp:1423`:
`tilt_ref = mix(acos(up_body.y), |phi_surf|, right_tilt_surf_frac)`.
**Identity value: 0.0 — bit-identical.** At 1.0 flat ground is unchanged (φ_surf ≡ tilt there)
and a conformal side-hill reads ~0, so the term goes silent on the hill and stays fully armed
for the actual roll-over it was built for.

**Invariant.** Every leg in `test/unit/test_sled_selfright.cpp` (11 TEST_CASEs) passes unmoved
at 1.0 — they are flat-ground legs — **plus a new leg that does not exist today**: a machine
settled conformal on a 20° side-hill with STAND held and speed < 1.389 m/s receives **zero**
self-right torque and **zero** commanded rider shift.

**Killing mutation.** `right_tilt_surf_frac = 0.0` → the expression is `acos(up_body.y)`
verbatim. **And the claim's own killer:** if `phi_surf` degenerates to radial up whenever
fewer than three patches are loaded (the fallback at `sled.cpp:1372-1425`), the blend is a no-op
in exactly the tipped case — which would be *safe* but would also mean the rung buys less than
it claims. **Unverified on tape: no field in the corpus joins slope, stand and low speed**
(D-A §8 item 4). Probe-only.

**Drive-checklist line.** *"Sit her across a steep side-hill at a crawl and hold STAND — does
she try to stand herself up off the hill when you didn't ask?"*

**Law check.** **This rung moves TOWARD the law, not away.** No governor: it *removes*
kernel-commanded rider shift during ordinary slope riding, restoring §0b's *"the player's lean
is the only thing that moves the rider."* STAND still rights and LEAN still throws — the
2026-08-26 separation (`31e0dd96a`) is untouched, and his *"I dont understand why yo are
clonlating the lean mechanism with the righting"* correction stays honoured. Depth 0.77
untouched. Wheelie kept. Ragdoll banned, not involved. One surface: `phi_surf` is fitted through
the three patches' own ground points, **no new ground query** (INV-1 intact).

**Risk, named.** **⚠ The self-right also has NO contact gate at all** — between `sled.cpp:1679`
and `1800` there is not one reference to `normal_sum`, `side_normal_sum`, `ground_contact` or
`w_contact`, and the speed gate reads *horizontal* speed, so a near-vertical drop stays armed.
DERIVED airborne authority: **2400/49.7 = 48.3 rad/s²**, one held press ≈ 1080 N·m·s ⇒
**≈ 1244 °/s of free roll with no ground under the machine.** That is a **separate rung with its
own ruling** (§4 ruling 6) — it is *not* folded in here, because adding `&& ground_contact` is a
behaviour change Chad has never been asked about, and because no tape can currently see it
(`right_assist_nm_now` is deliberately absent from the tape's pin roster).

---

### RUNG 5 — **One contact patch, one budget: the friction circle**

**Felt problem, his words.** §0: *"Also I've seen lots of snowmobiles **drift around corners and
then with throttle, straighten out. The feel is really good. Don't lose the feel**, except the
constant rolling."* This is RC-3 — **the drift is canon and it is signed.**

**Tape evidence.** MEASURED, D-B-8/D-E-03: the throttle is **binary in practice** — WOT 52 %,
closed 40 %, genuinely modulated only **7.3 %**, and the longest hold at a partial value in
20 minutes (0.35–0.63 s) is shorter than the ramp itself. D-B-5, MEASURED: **above ~10 m/s full
lock yields *less* yaw rate than three-quarter steer** (at 30+ m/s, 0.108 vs 0.197 rad/s) — the
machine stops answering the bars exactly where a drift would be. He is asking the machine to
rotate and it will not.

**Mechanism.** §1(e): `sled.cpp:1167-1174` computes a true Mohr-Coulomb shear budget
`area·(c_eff + σ·tan φ)` for the longitudinal direction, and `sled.cpp:1056-1059` computes
lateral force from a completely independent `normal·μ_l`. **There is no friction circle and no
combined-slip coupling anywhere in the kernel** — `sim/sled.h:1028` calls the lateral law a HOLE
that *"reads the BEKKER NORMAL REACTION only."* So step 2 of the real drift sequence — throttle
spends the track's remaining lateral budget, which *sustains* the slide — **cannot happen.**
(This is R1-19's own killing mutation firing, run by `R1-real-dynamics-REFUTE.md` §1 and adopted
here. What survives of R1-19 and needs no circle: thrust acts along the **chassis heading**, so
rising speed-along-heading rotates the velocity vector toward it and β shrinks — that is very
likely where his signed straighten-out already comes from.)

**THE ONE DIAL.** `[sled_comfort] lat_combined_frac` — on the **track patch only**, scale the
lateral bite by the fraction of its Mohr-Coulomb budget the thrust is not already using:
`mu_l *= mix(1.0, sqrt(max(0, 1 − (T/(area·tau_max))²)), lat_combined_frac)`, with `T`,
`area` and `tau_max` all already in scope in the same block.
**Identity value: 0.0 — bit-identical.** No new constant: the budget is the kernel's own.
DERIVED sizing on TrailMain at rest load: `T/budget` = 2272/4974 = 0.457 ⇒ the track's effective
lateral μ falls **0.70 → 0.623 at WOT**, and rises straight back the instant he lifts. **This is
a feel rung, not a stability rung** — 0.623 is still above the 0.5097 tip, so it does not
substitute for Rung 1.

**Invariant.** The charter's **L2 drift leg**: a throttle-kick mid-corner produces a yaw-slip
trace that *grows* under sustained throttle and *collapses* on lift — measured before and after,
as RC-3 demands (*"measure it before touching anything, and re-measure after — a probe leg, not
a hope"*). AND straight-line trail speeds and the emergence matrix stay inside §2's bit-identity
tolerance, because at zero slip angle the lateral term is already zero.

**Killing mutation.** `lat_combined_frac = 0.0` → `mu_l` untouched. **And the claim's own
killer:** in powder the budget collapses with the load — on Bush at the §9 attractor
`area·tau_max` ≈ 545 N against a track carrying 62 N, so the *lateral* demand there is ~43 N and
the circle is **inert where he spends 49 % of his driving**. If the probe shows the circle never
bites outside TrailMain/Road, this rung is a trail-only feel change and should be ranked and
sold as one.

**Drive-checklist line.** *"Kick the tail out on the groomed trail and then feed it throttle —
does she hold the slide while you're on it and straighten when you stay in it?"*

**Law check.** Depth 0.77 untouched. No governor (a coefficient on an existing contact force).
Wheelie kept — **and check it**: the circle only *reduces* lateral, never longitudinal, so
thrust and the launch wheelie are untouched by construction. STAND/LEAN untouched. Ragdoll not
involved. One surface. **RC-5 (THE FEEL IS SIGNED) bites hardest here** — this rung touches the
one behaviour he explicitly signed, so it cannot land without the before/after L2 trace in the
report.

**Risk, named.** R2 §14 item 16, adopted verbatim as a condition: *"Nothing here measures the
drift Chad loves … **No candidate may land without that leg.**"*

---

### RUNG 6 — **In the air the rider's body is the only honest control, and it is switched off**

**Felt problem, his words.** `docs/gi4_ride_handoff.md` §1 item 2, verbatim: *"**should be able
to launch in the air**."* §0b: *"SLiding banging, punchy, **jumps**."* And 2026-08-27, on the
gyroscopics: *"**I think we should build it. Because then it has the foundation it needs.**"*

**Tape evidence.** MEASURED, D-B-9 / R4-01: **58 air events on the v17 tapes (4.82/min), hang
p50 0.39 s and max 2.43 s, and only 29.3 % land upright.** R4-09: the landing attitude is
decided **entirely at the lip** — two in three are past 45° because two in three *launches*
were, and nothing in between could have helped.

**Mechanism.** `sim/sled.cpp:2096-2102` — K-WS1/K2, the airborne momentum exchange:
`dω = −k·I⁻¹·dL_exch`, where `exch_l_now` (built at `sled.cpp:467` from the rider's own mass and
position, `L_exch = μ(r_rel × v_rel)`) is spent as a **rate delta that telescopes**, so a full
throw nets an **attitude change and never a rate** — momentum-conserving, no free impulse. It
ships at **`k_air_shift = 0.0`** (`sim/sled.h:859`) and is **not reachable from any TOML block**
(D-A §4.5). DERIVED sizing from the source's own numbers: μ·y·v_rel ≈ 40 kg·m²/s over
`I.x = 158.7` ⇒ **≈ 0.25 rad/s of transient pitch per throw.**
This is the honest, Trials-shaped, rider-mass route — `R2-game-feel-REFUTE.md` §4 read the block
and established it: *"it is already the mechanism, shipped at 0.0 … the open question is Chad's
ruling on turning it on, not which mechanism to build."*

**THE ONE DIAL.** `k_air_shift` — **made loadable from `[sled_comfort]`** per RC-8.
**Identity value: 0.0 — bit-identical** (branch at `sled.cpp:2096`; every existing tape and
every airborne golden replays byte for byte, and the source's own contract *"identically 0
airborne, so backflips and the airborne-lean anti-cheat leg stay bit-exact"* is preserved at 0).

**Invariant.** **Airborne with the lean command frozen, attitude is bit-identical to today** —
the term can only ever be paid for by the rider actually moving. AND a full throw's net rate
returns to zero (the telescoping property, asserted as a test, not assumed). AND the backflip
**stays possible** (charter §4: *"measure the backflip stays POSSIBLE"*, and §0's first sentence
is *"Yep I can do backflips"*).

**Killing mutation.** `k_air_shift = 0.0` → the branch is dead. **And the claim's own killer:**
if `exch_l_now` at `sled.cpp:467` is not in fact derived from rider mass and position, this is a
free impulse and the rung is a governor in disguise — `R2-game-feel-REFUTE.md` §4 flags that it
read the comment block but **not the full arithmetic at 467**. **Read line 467 before this rung
is opened.**

**Drive-checklist line.** *"Off the biggest lip you've got — throw your weight forward and back
in the air: does the machine answer you at all before it lands?"*

**Law check.** No governor — the *opposite*: this routes air authority **through the player's
own lean**, the only thing §0b allows to move the rider. Depth 0.77 untouched. Wheelie kept.
STAND rights / LEAN throws: **⚠ check carefully** — lean in the air currently means THE THROW
(`31e0dd96a`, *"leaning is for the flying off the handlebar direction"*), so this rung gives the
same input a second airborne meaning. **That is a ruling, not an implementation detail** (§4
ruling 6). Ragdoll banned, not involved. One surface.

---

### RUNG 7 — **The driveshaft is a flywheel with a brake on it, and the kernel's has no brake**

**Felt problem, his words.** §0: *"allow me to **land on my skis more often** after a roll."*
§0b: *"**jumps**."*

**Tape evidence.** MEASURED, D-B: **brake duty 8.7 % on the v17 tapes — the highest of any
bucket, up from 3.8 % at v13g** — he is reaching for the brake more than he ever has, and
R4-02's p90 hang of 1.74 s gives a full 1.7 s in which a real rider would be on it. 26 of 58
landings above 10 g; 29.3 % upright.

**Mechanism.** `sim/sled.cpp:1354`:
`rep_belt = max(dv * v_cmd_b, max(v_bf, 0.0))` — **`brake` does not appear in the expression.**
The belt is driven to at least the body's forward speed whatever the brake is doing, so the
rotor's momentum `L_raw = −rotor_inertia_track_kgm2 · rep_belt / drive_radius_m`
(`sled.cpp:1960`) can only ever **rise**. Consequence, MEASURED from one shipped expression
(R4-14): **throttle in the air can give nose-up authority; brake in the air structurally cannot
give nose-down, at any dial value.** The real rider's primary corrective — LITERATURE, two
independent instructors: *"The throttle controls the pitch of the sled. If you push on the
throttle, it will bring the nose of the machine up; **the brake will bring the nose down**"*
(Hanke, SnoRiders); *"**Tap the brake in the air** longer depending on how tail heavy you are"*
(Chaffin, BCA) — **has no channel in this kernel.**

**THE ONE DIAL.** `[sled_comfort] belt_brake_frac` — the coast floor in that expression becomes
`max(v_bf, 0.0) · (1 − belt_brake_frac · brake)`. **Identity value: 0.0 — bit-identical.**
**Verified this session that this is safe to touch:** `rep_belt` feeds only `rep_rpm` (1358),
the rotor momentum `L_raw` (1960) and the `belt_speed_ms` readout (2144) — **the thrust path
computes its own `v_cmd`/slip independently at 1114-1224**, so this expression cannot move a
single newton of thrust.

**Invariant.** With `k_gyro_react` still at 0 (it is), the *only* observable change at any
`belt_brake_frac` is the `belt_speed_ms` / `engine_rpm` readout under braking — which is also
**more honest** (a CVT-engaged two-stroke with the brake on does slow its track). Every force,
every torque and every golden is unmoved.

**Killing mutation.** `belt_brake_frac = 0.0` → the expression is byte-identical.
**And the claim's own killer:** if a tape or audio golden pins `belt_speed_ms`/`engine_rpm`
under braking, this is not readout-only and the rung grows a second invariant. `grep` finds
`belt_speed_ms` consumed **nowhere** in `render/` or `app/` (D-D-F1) — its only readers are the
tape and the probe — so the exposure is the tape goldens alone.

**Drive-checklist line.** *"In the air off a big lip, squeeze the brake — does the nose come
down at all, or does she land however she left?"* (expect NO change until `k_gyro_react` is
ruled — this rung only makes that dial safe.)

**Law check.** Depth 0.77 untouched. No governor. Wheelie kept (thrust path untouched, verified
above). STAND/LEAN untouched. Ragdoll not involved. One surface.

**Risk, named — and it sets the ORDER.** **This rung is a precondition, not a payoff.** It does
nothing felt on its own; what it does is make the *next* dial safe. `k_gyro_react`
(`sim/sled.h:768`, ships 0.0) is the reaction wheel — DERIVED worth **17–22 °/s** of pitch, and
LITERATURE-confirmed as a shipped, named, default-on mechanic in the biggest off-road franchise
("Reflex Gyro", Rainbow Studios, *MX vs ATV Legends*, who pair it with an **airborne-throttle
cut on by default**). **Arming `k_gyro_react` before this rung would hand Chad exactly the half
of in-air control that makes a landing worse** — nose-up on throttle, and nothing to bring it
back down. R4-14's words, adopted: *"Arming the dial without touching the belt expression is the
wrong order of operations."* ⚠ The source also records honestly that the belt is **kinematic**,
so it *"spins up for free — the chassis is charged for momentum whose energy was never charged
to the engine."* That is a named, accepted fidelity limit, not a discovery.

---

### RUNG 8 — **The engine sings the thumb, not the engine**

**Felt problem, his words.** §0: *"**Work on the math to achieve a balance of fun and accuracy
to real physics**, just as our airplane ontological counterpart does."* And
`roost_consult_packet.md` §1: *"The conditions should be **palpable and observable in the sled
performance**."*

**Tape evidence.** MEASURED, D-D: **the coast is 8.60 % of every drive, at mean 19.34 m/s with
the kernel's own engine at 4 147 rpm — and the voice sings 5.8 semitones flat**, because the
audio follower's target is the **thumb**, not the engine. On **10.24 %** of the ticks where both
channels move, they move in **opposite directions**: he lifts and the machine's note goes the
wrong way. The kernel computes `engine_rpm` honestly off the *same* clutch blend as the thrust
(`sled.cpp:1356-1360`, *"`engine_rpm` is mapped off the SAME blend, never a second one"*) and
**no consumer anywhere reads it** — `belt_speed_ms` and `track_slip` likewise have zero readers
outside the tape and the probe (D-D-F1, D-D-F3).

**Mechanism.** `render/sled_audio.h` / `SledSynth::set_drivers` — the follower's target is the
thumb. The physical quantity exists one struct away and never crosses.

**THE ONE DIAL.** `render::kSledRpmBlend` (+ `SEADS_SLED_RPM_BLEND`). `set_drivers` takes a
second argument, the kernel's rev fraction `k = clamp((engine_rpm − 1700)/6300, 0, 1)`, and the
follower's target becomes `mix(thumb, k, kSledRpmBlend)`. **Identity value: 0.0 —
bit-identical**; `test_sled_audio.cpp` passes untouched. **The onset detector keeps running on
the thumb** — the brap is an input event and must not be moved.

**Invariant.** At 0.0 the rendered audio buffer is sample-identical. At any blend, the **brap
onset latency is unchanged** (the detector is on the thumb), and the measured coast-pitch error
falls from 5.8 semitones toward zero.

**Killing mutation.** `kSledRpmBlend = 0.0` → sample-identical. **And the claim's own killer:**
at blend 1.0 the tone stops answering a tap the instant the thumb moves (the kernel's rpm lags
the clutch), which costs the brap's immediacy — so this is a **swept** dial. If the measured
thumb-vs-rpm gap ever falls below ~0.08 (a quarter of the follower's own 0.35 idle-ratio sweep)
the change is inaudible and **the rung is dead.**

**Drive-checklist line.** *"Let off at speed and coast — does she still sound like she's at full
song, or does she drop off the way she should?"*

**Law check.** Depth 0.77 untouched. No governor. Wheelie kept. STAND/LEAN untouched. Ragdoll
not involved. One surface. **Nothing in `sim/` is opened** — this rung lives entirely in
`render/`, which is why it can be A/B'd on the same drive as a kernel rung without confounding
it. ⚠ It is also the only rung here that touches the **feedback** half of the equal-weighted
three (comfort / fun / feedback), and it is the highest measured payoff D-D found.

---

## 3. WHAT THIS LADDER DELIBERATELY DOES **NOT** PROPOSE

| not proposed | why | source |
|---|---|---|
| `roll_damp_nms` (ships 800) | It is the dial that buys the **arc** as well as the safety — MEASURED lean-in arc on Bush: 323 m at damp 0, 143 m at 150, 42 m at 400, **30 m at 800**. Moving it is a *ruling on the arc-vs-flick ladder*, not a fix. | D-A §6.1 |
| `track_rail_half_m` 0.19 → 0.14 (the "honest" rail) | It is a real 12.7 % stability surplus, and removing it moves **against** *"Just make it more stable."* | R1-26 |
| `plane_lat_load_frac` | **Hypothesis FALSIFIED, not unfinished**: tip onset 0.318 → 0.334 g (5 % of the gap) and the 16/20 m/s carve cells bifurcate into a −28° non-turning runaway. | `sim/sled.h:1081-1089` |
| `lean_return_tau_s` (auto-centring the rider) | Turning it on **is a governor** in the §0b sense, and side-hilling is HELD for many seconds — an auto-centring lateral axis cannot side-hill. | `sim/sled.h:826-828` |
| `track_pitch_half_m` (to buy the 30° wheelie) | MEASURED green window is one notch wide (0.20 → 0 red; 0.18 and 0.22 → 5 and 7 red), and identity 0.0 is the **89.9° runaway**. Item 4 belongs at `k_gyro_react`, after Rung 7. | D-A §6.4 |
| contact-gating the `rolled` readout | **Already shipped and already debounced** (`rolled_persist_s 0.3`, `rolled_grace_s 0.2`, in every tape header). Do not spend a rung on it. | R2 §10.3 |
| a lower `base_m` to make her plane | **Forbidden by WINTER_LAW.** 0.77 m is the p50 of the depth distribution; `base_m 0.85` is the parameter that yields it. "Restoring base_m to the signed 0.77" would silently *shallow* the world while claiming to honour the ruling. | D-C C3 |
| widening the reach box to a third "hang-off" rung | The distance is a **labelled GUESS** (0.45 m) and dies to any measurement; at 0.38 m it is indistinguishable from the standing rung. | R1-32 |
| making `track_lat_mu` surface-aware | Real and honest (the constant 0.70 makes the machine **3.2× rear-biased on ice**, the exact inverse of the real sport, R1-28 as inverted by its refutation) — but LakeIce is 3.4 % of the corpus and has the **lowest** rollover rate (0.85/min), so it buys the least of anything measured. Named here so it is not lost. | `R1-...-REFUTE.md` §1 |

---

## 4. RULINGS CHAD MUST GIVE FIRST

Ordered. **Nothing above should be built before 1 is answered.**

1. **★ FLY THE CURRENT BUILD AND RE-RULE.** There is **no felt report of any kind** for the six
   drives of 2026-09-17 — not in `docs/`, not in `LANES.toml`, not in a commit, not in memory
   (D-C O14). Worse: **the felt record contains no drive of GI3 at all** — he drove a
   pre-`bdcc9c7b3` binary an hour after the rollfix committed, so *every roll complaint on file
   is against a kernel that has since changed*, and MEASURED the machine did change: longest
   lie-down **53.60 s → 5.09 s**, R presses **0.96/min → 0.42/min**, share of drive past 75°
   **16.78 % → 3.81 %** (R2 §11.3, with its own confound named: not a controlled A/B).
   His words, §0b: *"**I am afraid it might ruin something that is reallt good right now but
   just dont ruin it.**"* **Tuning against a five-week-old complaint is the largest single risk
   in this audit.**

2. **Is a backflip a rollover?** Every "roll resistant" number on this ladder counts *attitude*,
   not *intent*, and §0's first sentence is *"Yep I can do backflips."* No instrument in 206
   minutes of tape separates a deliberate send from a crash (D-A §8 item 1). **Without his
   definition there is no acceptance bar for Rung 1.**

3. **`class_blend_m` — the A/B he has never been handed.** `config/world.toml:645` ships **0.0**,
   which makes `surf_mix` identically zero and the per-patch class blend a dead branch. The
   measured effect at 1.0: bank-graze yaw **−482.7° → −11.6°**, roll **166.6° → 52.7°**,
   **ROLLED → no rollover**. `LANES.toml:939` records the ruling keeping it off, verbatim:
   *"⚠ THE STICK RULING: [snowpack] class_blend_m STAYS 0.0 — it is live-tunable and it is
   CHAD'S A/B, not the lane's call."* **This is the one measured fix for the one rollover he
   named by mechanism, and nobody has asked him.**

4. **The three config-vs-default drifts, none of which a test covers.** `right_assist_nm`
   **0.0 → 2400.0** (the file's own prose argues 300 → 900 → 1500 and closes *"Chad rules the
   final value on his drive"*; **2400 appears in no justifying sentence**);
   `right_charge_push_s` **1.0 → 0.6**; `right_dir_eps` **0.1736 → 0.04**, making the dead-cone
   **2.29°**, not the documented 10°, so the direction term is effectively `sgn()`. He drove v3
   at 2400 and said *"it works now, **but** a multiple press from full inversion should not be
   able to right it and it looks terrible"*; v4 then changed the mechanism under him and
   **no drive of the fixed form is on record** (D-C §6 item 3). **2400 carries a partial,
   superseded pass and no current one.**

5. **How much cornering grip may be spent to buy roll resistance?** Rung 1's whole trade.
   §0 says both *"Let me slide around a bit arcade"* and *"Don't lose the feel"* in one
   paragraph, and O5 (*"turning too unstable, can't hold a carve"*) is already unpaid.
   **Measurement cannot settle this one.**

6. **May the air have authority at all — and what does LEAN mean up there?** `k_air_shift`,
   `k_gyro_react` and `k_gyro` are all **built, honest, bit-identical at zero, and shipped at
   zero**; his 2026-08-27 ruling was *"I think we should build it"* and the spec's own line is
   *"Chad has not flown it. Until he does, it is built, not approved."* Rung 6 gives LEAN a
   second airborne meaning beside THE THROW, which his 2026-08-26 correction separated on
   purpose. **Separately: should the self-right get a contact gate?** It currently has none —
   DERIVED **≈1244 °/s of free roll in the air** on a held press — and adding `&& ground_contact`
   is a behaviour change nobody has put to him.

7. **Should R recentre the bars?** MEASURED, D-E-06: **six rightings in the current tapes, zero
   at centre, four at FULL LOCK** — `app/main.cpp:8287-8365` zeroes `sled_lean_lat`/`_fwd` and
   never touches `sled_steer_cmd`. He gets the machine back upright with the bars hard over, on
   a machine M12 says rolls at *any* non-zero steer on Road. His word: *"I need a key for now
   that lets me autoright until we get the guy running back to the snowmachine."* One line —
   but it is **his key**, so it is **his call**. (Corpus scale: 146 R presses, **144 of them at
   ≥ 75.06° tilt** — R is being used exactly as designed, for only ~20 % of unrecovered
   rollovers.)

8. **`traction_mu`'s value.** His *"yes take care of that"* ruling is **taken**, the mechanism is
   **built**, and it **ships 0.0**. The sweep is on the books (≥ 0.6 kills the runaway; ≥ 3.0 is
   byte-identical on his own traverse). **He picks the number; nobody else can.**

---

## 5. PROBE LEGS OWED BEFORE ANY OF THIS IS BUILT

All in `tools/sled_probe.cpp`, built in `D:/seads_sandboxes/sled-audit/build` only, per the
charter. **None was run by this ladder.**

- **L1 (charter §5) — RC-1's only possible trial.** Hands-off full lock on TrailMain at
  v ∈ {8, 12, 16, 20, 25}. **Chad never drives hands-off, so 206 minutes of tape contain no such
  trial** (D-B §6). Rung 1 cannot be sized without it.
- **L2 — the drift, before and after.** RC-3's explicit demand. Rung 5 cannot land without it,
  and R2 §14 item 16 makes it a condition on *every* candidate.
- **D-1 — pack absorption during a real impact.** Instrument `sink_m[Track]` and the spring/pack
  force split over a 1.7 s send onto Bush vs Road. This is the **largest single uncertainty in
  the audit** and it decides whether Rung 2's damper ever sees the shaft speeds §1(d) assumes.
- **The steer-inversion causation leg (D-B-5).** Commanded steer sweep at fixed speed. The tape
  establishes the correlation; the confound (the 0.9–1.0 cells are 4–8× oversampled because he
  reaches for full lock *when the machine is already not turning*) means **only the probe can
  establish direction of causation.**
- **The clean assist A/B (R2 §14 item 11).** Replay tape 1 through `seads_sled_probe` at
  `right_assist_nm` 0.0 vs 2400.0 and compare episode-duration histograms. **No dial may move on
  the confounded before/after alone.**

---

## 6. WHAT THIS LADDER REFUSES TO CLAIM

1. **That any of this is FELT.** Every rung names a mechanism, a dial and an identity value and
   stops. RC-5 stands: *"THE FEEL IS SIGNED."*
2. **That the rollover rate is a kernel ranking.** v13g 6.30/min → v15 2.16 → v17 4.32 is
   confounded at least four ways, all measured (surface mix, stand duty, speed, and an
   **input-encoding change between tape 40 and tape 48**). **No kernel verdict may be read off
   that table** (D-B-11).
3. **That §1(b)'s "available by construction" has been observed.** It is DERIVED from the shipped
   table and the shipped geometry, and it is **corroborated** by the 12/12 TrailMain-over-Bush
   sign test — but D-B-6's own killing mutation (attribute each rollover to the surface at
   *exit* rather than entry) is **NOT RUN and is owed**. Until it is, read "on the trail" as
   "**at** the trail".
4. **That the self-right ever fires in the air.** DERIVED from the code structure only.
   `right_assist_nm_now` is deliberately absent from the tape's pin roster, so **no tape in the
   corpus can be asked** (D-A §8 item 2).
5. **That the winter lateral-friction band is a snowmobile number.** It is a splice of car-tyre
   lateral and snowmobile longitudinal data; the SAE 2015 cornering table is **paywalled and was
   not obtained on either attempt** (R1 §7, §9.0). Rung 1's *relationship* is exact; its
   *target value* is LITERATURE-thin and belongs to a sweep.
6. **Anything about how it feels today.** See ruling 1. There is no felt report for the current
   build, and this document does not manufacture one.

---

*Converged by the physics-honest strand, 2026-09-18, against `ed0a43ce8` (kernel text ==
`origin/main`). Read-only. No dial changed, no config edited, no test run, no probe built,
nothing pushed. The only file this strand wrote is this one.*
