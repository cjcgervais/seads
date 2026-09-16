# K-WS1 — WEIGHT-SHIFT MADE REAL IN THE KERNEL (plan v1, Fable, 2026-08-20 night)

**Ruling:** Chad, on the R3-WS(c) report: "carry it, make it real and better." This is
the previously-deferred kernel rung (SUDBURIAN_R3_WEIGHTSHIFT_SPEC.md §9), now GREEN-LIT.
**Order:** builds AFTER R3-WS(d) lands (the kneel changes the honest CG ceiling this
rung must be sized against). **Scope:** `sim/` — the first sim change of the whole
weight-shift program. This is the DRIVE SURFACE: maximum care, red-team before build,
every dial 0-OFF bit-identical, Chad's drive is the only claim upgrade.

## Base facts (measured, from the R3-WS program)
- The ONE coupling today: `cg_off = (rider_mass/mass) * (-lat, up, -fwd)` moves the
  three contact-patch mounts only (`sim/sled.cpp:313-317` at the time of the scout).
- **Airborne weight shift does NOTHING** (no patches loaded → cg_off unread by any
  airborne term). Chad's original ask: "more authority in the air due to weight
  shifting" for the 87.5 kg Sudburian.
- Wheelie/catwalk on the ground IS coupled (aft cg → rear patch load → rise) — the felt
  weakness there is RANGE (the clamp), not a missing term.
- `lean_aft_max_m = 0.25` vs the animation's honest delivered ceiling (post-(c): 0.218
  seated / 0.202 stand; post-(d): re-measured, expect seated ≈ 0.27–0.30). Fwd 0.45 is
  honest (R1a/R1c solved) and untouched.
- Rider 87.5 of 331 kg; inertia currently held constant under lean (named ~16 % roll
  error at full stand lean, `sim/sled.cpp:302-312`).

## K1 — Honest, BIGGER aft authority (the clamp made real)
- `lean_aft_max_m` 0.25 → the MEASURED post-(d) seated ladder ceiling C_0(1) (baked
  with provenance from the animation bake's reported number; expect ≈ 0.27–0.30 =
  MORE aft authority than today, honestly delivered).
- **Stand-dependent ceiling:** the animation delivers less aft CG at full stand
  (≈ 0.20). Add `lean_aft_max_stand_m` and clamp with
  `aft_max_eff = lerp(lean_aft_max_m, lean_aft_max_stand_m, rise_frac_now)` —
  the kernel stops asking for CG the body cannot deliver, killing the 38.7 mm lie
  instead of carrying it. Both constants named, pinned, provenance-commented.
- Ship ON (it is honesty, not feel). A/B env or param path back to 0.25 flat for one
  drive if Chad wants the comparison.

## K2 — Airborne authority (the real physics, not a cheat)
- Mechanism: INTERNAL MOMENTUM EXCHANGE. In flight, when the rider moves his mass, the
  machine+rider system conserves angular momentum: a rider CG velocity relative to the
  chassis imparts the opposite momentum to the chassis. The kernel already slews
  `rider_fwd_m/lat_m/up_m` (lean_tau_s 0.18) — their per-tick DERIVATIVES are available
  deterministically (state deltas, no clock, tape-safe).
- Term: `τ_exchange = k_air_shift * m_r * (r_cg × d(r_cg)/dt)`-style pitch/roll rate
  injection applied to the body ONLY in proportion to unloadedness
  `air_frac = 1 - clamp(total_patch_load / (mass*g*kLoadRef), 0, 1)` — grounded riding
  is untouched (patch path already handles it), pure flight gets full authority.
- Additionally the STATIC part: while airborne, gravity acts at the COMBINED CG; the
  aerodynamic/hull reference points do not move with cg_off today. Move the moment arms
  of drag/hull about the SHIFTED CG (the `pt - cg_off` pattern already used at
  `sim/sled.cpp:1189,1226`) wherever a flight term still assumes the unshifted origin —
  enumerate every such term in the build report; touch nothing that is not enumerated.
- ONE feel dial `k_air_shift`, default sized so a full stopped→full-aft swing in flight
  yields a visible but not silly pitch rate (propose from arithmetic: ΔL = m_r·Δr·v_rel;
  size, then Chad's drive judges). `k_air_shift = 0` must be BIT-IDENTICAL to HEAD —
  the 0-OFF law, GI3/GI4 precedent. Ship ON at the sized default per Chad's "make it
  real" (he drives, he dials).
- The 16 % constant-inertia approximation stays (named, unchanged) — inertia work is NOT
  this rung; note it forward.

## K3 — The catwalk ("the sled rises to him")
- No new term until measured: with K1's bigger honest range, re-measure the
  stopped→WOT wheelie vs aft lean sweep (the GI4 probe/debug-sink machinery) BEFORE and
  AFTER. If the felt rise-authority is now adequate on numbers, K3 is a measurement, not
  a change. If not, the candidate lever is named (rear-patch load share vs lift split —
  GI4 §9.7 territory) and goes to Chad as a question, NOT built unilaterally.

## Gates & safety (drive surface!)
1. All dials 0/flat → bit-identical suite: the FULL sled gate must read exactly today's
   1260/1264 with the same 4 known GI4 names (lines 1074/1253/2766/3120 record).
2. Tape law: input schema unchanged (lean_fwd already recorded); `tape_360_chad_repro`
   and the flip-fence/provocation goldens must stay green with dials OFF; with dials ON,
   re-run and REPORT deltas — no golden is re-cut without listing it.
3. New non-vacuous gates: (a) airborne pitch-rate response to an aft step is nonzero
   with the dial ON and exactly zero OFF; (b) aft_max_eff honesty — kernel demand never
   exceeds the baked animation ceiling at any rise_frac; (c) grounded trajectories with
   dial ON but zero flight time are bit-identical (air_frac gating proof).
4. Roll interactions: run the roll fence (GI3) and the −28° attractor probe with K1 ON —
   aft CG moves patch loads; report both, expect no movement, treat any as a finding.
5. Determinism: derivative from state deltas only; no clock, no statics.

## Process
Fresh-context Fable red-team audits THIS PLAN (with §13) before any build. Opus builds
after R3-WS(d) lands. Commit on sandbox/gi4-ride, NO PUSH. Claim ceiling: BUILT+GATED;
"real and better" is upgraded only by CHAD'S DRIVE.

## RED-TEAM AMENDMENTS (2026-08-20 plan audit — BINDING, override the sections above)

K-A1 (B-P0-1) **`sled_airborne_lean_is_inert` (test_sled.cpp ~:1492) fails BY DESIGN
with K2 ON** — it is the named anti-cheat leg of the sim/sled.cpp:302-312 comment.
Re-form it into the pair: inert at k_air_shift = 0 (bit-identical), responsive at
k ≠ 0; rewrite the :302-312 comment in the same commit. Gate 1's "exactly 1260/1264"
applies to the ALL-DIALS-OFF config only; the ON-config result is a published DIFF of
the failure NAME SET (any of the 4 known names changing status — fixed, moved, or a
new mode — is a Chad-facing finding), never a silent count.

K-A2 (B-P0-2) **K1's 2-point lerp is refuted by its own data** — the measured C(1) row
is non-monotone in s (0.2183/0.2146/0.2083/0.1990/0.2021; kneel-on steepens low-s).
Pin the FULL 5-slice row in sim (piecewise-linear in rise_frac, provenance from the
bake print), plus a ctest CROSS-CHECK in the test TU (which links sim and render):
sim's pinned row == the live bake within ε — a re-export can never silently strand the
kernel constants. HONEST HEADLINE for Chad, stated before his drive: with C_0(1) ≈
0.25 (amendment D2), K1 buys ≈ ZERO seated gain over today's clamp and REMOVES ~48 mm
of demand at full stand — it kills the lie, it does not add range. A/B path back to
flat 0.25 armed for one drive.

K-A3 (B-P0-3) **air_frac from total patch load is NOT "grounded untouched"** — planing
unloads the patches (62 N of 4106 at the −28° attractor; ~913 N in a WOT carve), so a
load-based air_frac ≈ 0.78–0.99 WHILE GROUNDED. Either define grounded by CONTACT
(any patch pen > 0 forces air_frac = 0; gate the cliff), or keep load-based and
(a) delete gate 3c's false bit-identical promise, (b) extend gate 4 to run the carve
sweep, the roll fence AND probe_attractor with **K2 ON**, not just K1. (The catwalk
itself is safe: track carries ≈ mg ⇒ air_frac ≈ 0 ⇒ no double-spend with K1.)

K-A4 (B-P0-4) **K2's discrete form**: τ = k·m_r·(r × dr/dt) is a MOMENTUM, not a
torque — integrating it as acceleration diverges under constant slew. The conserving
form is **Δω = −k·I⁻¹·Δ[μ·(r_r × v_rel)]** per tick (the DELTA of the exchange
momentum), with reduced mass μ = m_r·m_mach/m = 64.4 kg and the RIDER offset r_r
(NOT cg_off — that under-scales 0.070×). Needs ONE new persistent state 3-vector
(prev exchange momentum) — deterministic, input-driven, tape-safe; the state-struct
change is named in the report. SIGN IS MEASURED BY TEST, never typed — the honest
physics: chassis pitches nose-DOWN while the rider throws aft, and NO sustained rate
once he stops (sizing: μ·y·v_rel ≈ 40 kg·m²/s ⇒ ~0.2 rad/s transient decaying with
the slew; net attitude a few degrees). A PERSISTING pitch rate cannot come from body
English — the honest sustained mechanism is the TRACK AS A REACTION WHEEL (throttle
blip in flight), which is NOT in this rung. Ship: K2 built ON-capable at the honest
scale k = 1.0, **default OFF**; the smoke evidence + the mechanism menu (honest
transient / track reaction wheel / feel-boosted k) goes to Chad and HIS DRIVE decides
ON — a real term whose honest sign surprises the hand is exactly how "real" gets
ruled "backwards" and reverted.

K-A5 (B-P1-5) rise_frac ≡ the animation's s (same clamp01(rider_up_m/stand_rise_m),
single-sourced) — confirmed, no feedback loop. TWO couplings the build must own:
(a) K1's rise-varying clamp drags rider_fwd_m during an in-flight stand — that motion
FEEDS K2's derivative (physically correct; STATE it); (b) raising lean_aft_max_m
scales the RENDER bake's demand step (~8%) — the animation LOG_FATAL and the (d)
kneel cells are RE-VERIFIED in the K1 commit, in the same gate run.

K-A6 (B-P2-7) Strike the "move drag/hull moment arms" bullet: hull arms already take
pt − cg_off; gravity is torque-free at the CG by definition; drag has NO application
point in this kernel — "moving" it would CREATE a first-ever aero moment (new physics,
breaks airborne-inert at k=0). The enumeration is expected to come back EMPTY.

K-A7 (Q9) K3's sweep: extend tools/sled_probe.cpp `probe_pitch_sweep` (~:221) with a
lean_fwd column loop (or a new probe_ws_wheelie mode) — ~30 lines, named here so the
builder extends, not invents.

---

## R3-WS(d) MEASURED HANDOFF (filed 2026-08-20 in the R3-WS(d) commit)

The animation rung landed with the kneel ON. These are the numbers this plan was
waiting on, taken off the shipped bake (`SLED R3-WS(c) BAKE C(1) per stand slice`,
printed at every load — the label still says (c), the values are (d)'s):

| stand s | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
|---|---|---|---|---|---|
| **C_s(1), R3-WS(d)** | **0.2786** | 0.2668 | 0.2306 | 0.2041 | **0.2021** |
| C_s(1), R3-WS(c) | 0.2183 | 0.2146 | 0.2083 | 0.1990 | 0.2021 |

- **C_0(1) = 0.2786 m.** D2's kill line for the K1 "more seated authority" claim was
  0.255 — the bake is above it, so **K1 SURVIVES, measured.** K1's `lean_aft_max_m`
  target is therefore **0.2786**, not a guess.
- **The sign of the lie has FLIPPED at zero stand.** The kernel asks for 0.25 and the
  seated ladder now delivers 0.2786, so the seated CG_z clamp lie is gone and the
  player's mouse tops out at **u ≈ 0.93 — the full kneel is currently unreachable in
  play.** Raising the clamp to 0.2786 is what makes the kneel Chad ruled on actually
  reachable; that is now a REASON for K1, not just an authority upgrade.
- **The stand slices did NOT move** (the kneel is dead at stand by construction), so
  K-A2's non-monotonicity finding stands and is now STRONGER: the row falls
  0.2786 → 0.2021 and is non-monotone at the top. A 2-point lerp in `rise_frac_now`
  over-promises by up to 36 mm at s = 0.5. Size `lean_aft_max_stand_m` = **0.2021**
  and treat the middle of the row as the error term, or carry the 5-point table.
- Measured CG_z solver residual over today's shipping range: **27.4 mm** (was 38.7).
  Declared CG_y departure: **144.5 mm**, unchanged — the kneel's own share is +83.7 mm
  and the worst is still the standing crouch.
- Ladder-side numbers this rung must not regress: worst arm ratio 1.4289 against HEAD's
  frozen 1.4292, arm no-regression +0.0431 (law: ≤ +0.05), worst leg 1.1185.
- ⚠ **OPEN-R3WS-STANDSLEW** (new, R3-WS(d) §14.2): the ANIMATION already spends
  0.0574 m of joint travel per 60 Hz tick of `lean_up_m` slew at full aft, against the
  program's own 0.060 m continuity bound. If this rung touches `lean_tau_s`,
  `lean_rate_ms` or `stand_rise_m`, it moves that number directly.

---

# BUILD RECORD — K-WS1 BUILT + GATED, 2026-08-21 (Opus, sandbox/gi4-ride)

**Claim ceiling: BUILT + GATED.** "Real and better" is upgraded only by CHAD'S DRIVE.
K1 SHIPS ON. **K2 IS BUILT AND DEFAULTS OFF** (`k_air_shift = 0`) — per K-A4, his drive
decides. K3 was a MEASUREMENT and it produced a QUESTION, not a change.

## What landed

| | file | what |
|---|---|---|
| K1 | `sim/sled.h` `kAftCeilC1[5]`, `lean_aft_max_m` 0.25→**0.2786**, `aft_ceiling_curve` (0-OFF, ships 1.0) | the honest, stand-dependent aft ceiling |
| K1 | `sim/sled.cpp` (rider slew block) | `aft_max_eff` piecewise-linear in `rise_frac_prev`, used by BOTH the target and the clamp |
| K2 | `sim/sled.h` `k_air_shift` (0-OFF), `kRiderSeatOffY/Z_m`, `SledState::ws_exch_l` | the one new persistent state 3-vector |
| K2 | `sim/sled.cpp` (rider slew + integrator) | `dω = −k·I⁻¹·Δ[μ·(r_rel × v_rel)]`, gated on CONTACT |
| K-A1 | `sim/sled.cpp:302-312` comment + `test/unit/test_sled.cpp` | the anti-cheat leg re-formed into the PAIR |
| K-A2 | `test/unit/test_rider_pose.cpp` | the row cross-check against the shipped GLB |
| K-A3 | `sim/sled.cpp` `ground_contact` | grounded is CONTACT, not load |
| K-A7 | `tools/sled_probe.cpp` `wswheelie`, `wsair` + `SEADS_WS_FLAT`/`SEADS_WS_KAIR` | the K3 sweep and the K2 smoke |
| tape | `test/harness/sled_tape.h` | both dials in the roster + in the TAPE-ABSENT zero list |

## The pinned row, verified three ways

`kAftCeilC1 = {0.2786, 0.2668, 0.2306, 0.2041, 0.2021}`

1. **The LIVE BAKE**, off `build-play/seads.exe` on the shipped asset:
   `s=0.00 0.2786  s=0.25 0.2668  s=0.50 0.2306  s=0.75 0.2041  s=1.00 0.2021` — EXACT.
2. **The independent re-derivation** in the test TU (`ws_test_curve`, same GLB, no shared
   code with `bake_weight_shift`): worst |pinned − measured| = **4.0 mm** (at s = 1).
3. **The kernel's own demand**, swept in the runtime coordinate: over-ask ≤ 1e-9 m at
   every stand from 0.00 to 1.00.

⚠ **DEVIATION, stated.** K-A2 asks for the cross-check against "the live bake".
`render/sled_model.cpp` is in the raylib-only `seads` target and `seads_tests` links no
raylib, so no ctest can call `bake_weight_shift`. The ctest reference is therefore the
independent re-derivation (which reads the same GLB and moves with a re-export exactly as
the bake does); the LIVE bake is verified by hand above and printed at every start.

## K1 — what it actually buys

- **The full kneel is REACHABLE.** MEASURED through the SHIPPED inverse (`ws_invert`, not
  by dividing the two ceilings — C_0 is not linear in u): at the old clamp of 0.25 m the
  player's mouse topped out at **u = 0.9198**; at the shipped 0.2786 it reaches
  **u = 1.0000**. The last 8 % of the kneel Chad ruled ON had never been reachable in
  play. It is now, and both facts are gated.
- **The lie is dead at every stand.** Measured over-ask, before → after:
  s = 0 **−28.6 mm (under-ask)** → 0; s = 0.5 **+19.4 mm** → 0; s = 1 **+47.9 mm** → 0.
- **A 2-point lerp would NOT have done it** (K-A2 confirmed): the row is concave, and an
  end-to-end lerp over-promises a MEASURED **9.8 mm at s = 0.50** and **16.8 mm at
  s = 0.75**. (K-A2's own "~36 mm at s = 0.5" figure does NOT reproduce — the measured
  worst is 16.8 mm at s = 0.75. Reported, not repeated.)
- **The A/B arm**: `aft_ceiling_curve = 0` + `lean_aft_max_m = 0.25` is the pre-rung
  kernel bit-for-bit, gated by `sled_aft_ceiling_curve_is_bit_identical_at_zero`.

## K2 — the honest transient, measured

Discrete form as implemented, per SUBSTEP:

```
r_rel   = (−lat, up, −fwd) + (0, kRiderSeatOffY_m, kRiderSeatOffZ_m)
v_rel   = Δr / h                       (state delta, no clock, no statics)
L_exch  = μ · (r_rel × v_rel),  μ = m_r·m_mach/m = 64.4 kg
Δω      = −k · I⁻¹ · (L_exch − s.ws_exch_l)      [airborne only]
s.ws_exch_l = L_exch                             [always]
```

⚠ **K-A4's shorthand is REFUTED by arithmetic, and the amendment's own sizing is what
refutes it.** Written literally as `r_r = (−lat, up, −fwd)`, a pure aft throw has r
parallel to v and the term is IDENTICALLY ZERO. The two-body exchange momentum is taken
about the RELATIVE POSITION of the masses = seated offset + displacement. The seated
offset is MEASURED off the shipped GLB: rider CG model (0.00033, 0.81774, −0.35734) →
body **(0.0003, +0.3537, +0.3573) m**. K-A4 sized the transient at ~0.2 rad/s from
μ·y·v_rel ≈ 40 with an assumed y ≈ 0.44; the measured arm 0.354 gives μ·y·v_rel = 31.9
and Δω = 31.9/158.7 = **0.201 rad/s** — the same answer. The sizing only closes with the
corrected form.

**Measured on a real jump** (`sled_probe wsair 16 <k> 6`, launch 16 m/s + 6 m/s up, full
aft throw from 0.25 s of flight; airborne window 0.02 .. 1.13 s):

| k_air_shift | peak Δ(pitch rate), IN THE AIR | at touchdown | net airborne Δattitude |
|---|---|---|---|
| **0.0 (shipped)** | **+0.000 deg/s** | 0.0000 | **+0.000 deg** |
| 1.0 | **−11.508 deg/s = −0.2008 rad/s** at t = 0.25 | −0.0865 deg/s = **0.75 % of peak** | **−2.275 deg** |

- **SIGN, MEASURED not typed: NEGATIVE = NOSE DOWN while the rider throws AFT.** That is
  the honest physics and it is the opposite of what a hand expects.
- **IT DECAYS.** −11.5 → −7.4 → −4.6 → −2.9 → −1.8 → −1.2 → −0.73 → −0.46 → −0.29 →
  −0.18 → −0.11 deg/s. There is no `else` branch and no damping doing that — the deltas
  telescope, so once the body stops moving every borrowed rad/s has been handed back.
  What survives is 2.3 deg of ATTITUDE.
- ⚠ The biggest number in that trace (+55 deg/s at t = 1.15) is the LANDING, two machines
  with different aft CG hitting the snow. It is NOT this term and the probe now windows
  it out.

## K-A3 — grounded is CONTACT, and the plan's load gate is refuted

`air_frac` load-based would read **0.96 at the −28 deg attractor** (62 N of 4106 on the
patches) and **0.78 in a WOT carve** (~913 N) — i.e. it would spend airborne authority
all over the two most fragile places in GI4. Grounded is therefore ANY penetrating patch
OR hull point, taken from the kernel's own `patch in the air` test one line above. The
payoff: gate 3(c)'s bit-identical promise is a FACT, not a hope
(`sled_air_shift_is_inert_while_any_patch_touches`: a 4 s WOT carve with all three rider
axes scrubbing, k = 1.0, bit-identical in position AND angular_vel).

Measured with K2 ON anyway, per K-A3's own requirement — all IDENTICAL, zero diff lines:
`probe attractor` (t(|roll|>20) 6.2167 s, v_end 24.8, rolled=no), `probe carve_sweep`,
`probe roll`, `probe gi3_sweep`.

## K-A6 — the enumeration came back EMPTY, verified

Every torque site in `sim/sled.cpp` was enumerated (`add_at` ×22, `torque_body +=` ×2,
`torque_body.z +=` ×1):
- all `add_at` patch sites are behind the `normal <= 0 && x <= 0 → continue` gate;
- the hull sites already take `pt − cg_off` and are behind `pen <= 0 → continue`;
- the plate term reads `patchN`; the C3 yaw term is gated on `hull_engage_lp` which is fed
  by `side_normal_sum`; the RC roll assist multiplies by `w_contact`;
- gravity and drag are applied to `force` at the CG with NO application point.

Nothing to move. The bullet is struck.

## K-A5 — both couplings owned

(a) The rise-varying ceiling DRAGS `rider_fwd_m` during an in-flight stand, and that
motion feeds K2. **Measured**: holding full aft at 0.2775 m and then standing drags him
**75.4 mm** forward to the standing ceiling, and K2 reads a **+0.363 rad/s** peak from it
(nose UP — opposite sign to the aft throw, because the drag is forward motion). Gated by
`sled_the_stand_ceiling_drags_the_aft_lean_and_K2_feels_it`.

(b) Raising `lean_aft_max_m` scaled the render bake's demand step 0.0039 → **0.0044 m**
(+11.4 %). **RE-VERIFIED IN THIS COMMIT on the live bake**, worst pose delta per demand
step, by slice:

| s | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 | bound |
|---|---|---|---|---|---|---|
| after K1 | 0.0274 | 0.0294 | 0.0319 | **0.0433** | 0.0308 | 0.060 |

All green, no LOG_FATAL, worst slice at 72 % of budget (was 65 %). `monotone 1`, min
dC/du 0.00041 > 0. The (d) kneel cells and the continuity-in-s number (0.0643 per tick,
OPEN-R3WS-STANDSLEW) are unchanged — this rung touches none of `lean_tau_s`,
`lean_rate_ms`, `stand_rise_m`.

## The three gate configs, SERIAL (`-j1`), one build dir at a time

Baseline re-measured on HEAD `90b75ce0f` in this session, not remembered:
**1262 / 1266**, failures `sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.
The rung adds **7** cases, so the new total is 1273.

| config | tally | failure NAME SET vs baseline |
|---|---|---|
| **1. ALL DIALS OFF** (`aft_ceiling_curve 0`, `lean_aft_max_m 0.25`, `k_air_shift 0`) | 1266 / 1273 | the 4 known names, UNMOVED + 3 NEW cases that pin the SHIP config by design |
| **2. SHIPPED** (K1 on, K2 off) | **1269 / 1273** | **the 4 known names, and NOTHING ELSE** |
| **3. K2 ON** (`k_air_shift 1.0`) | 1268 / 1273 | the 4 known names + `sled_airborne_lean_is_inert_at_k_zero` (by design, K-A1) |

- **Gate 1 is the one that matters and it is clean**: every PRE-EXISTING test behaves
  exactly as it did on HEAD, and the 4 known GI4 names are the same 4, in the same
  statuses — none fixed, none moved, no new mode. The 3 extra OFF-config failures are all
  mine and all deliberate: `K-WS1: the kernel's pinned aft-ceiling row ...` (its
  reachability leg asserts the SHIPPED clamp reaches u = 1), `sled_aft_demand_never_
  exceeds_the_measured_ceiling` (asserts `aft_ceiling_curve == 1.0` — "it SHIPS ON"), and
  `sled_the_stand_ceiling_drags_the_aft_lean_and_K2_feels_it` (needs > 0.27 m of seated
  aft, which a 0.25 clamp cannot give).
- **Goldens: ZERO moved, in ALL THREE configs.** `tape_360_chad_repro`,
  `tape_360_chad_provocation`, `tape_360_chad_attempt1_provocation`,
  `sled_tape_round_trip_replays_bit_identical`, `sled_tape_off_arm_is_bit_identical` —
  green everywhere. Nothing was re-cut. The TAPE-ABSENT rule does the work: both new
  dials are in the roster AND in the reader's zero list, so a tape cut before this rung
  replays the kernel it was cut on.

### ⚠ FINDING (caught by gate 3, fixed in this commit)

With `k_air_shift > 0`, `sled_tape_round_trip_replays_bit_identical` **diverged at the
override tick** (tick 150, field `orientation.w`). Cause, root-caused not guessed: the
replay rebuilds state with `s = override.pin`, and `ws_exch_l` is deliberately NOT in the
pin roster (adding it would move `pin41` → `pin44` and break every existing `.sledtape`).
So the record run carried a live momentum history across the R-key autoright and the
replay did not.

The fix is the physically correct one, not a schema bump: **a teleported machine has no
momentum history**, so `autoright` now zeroes `ws_exch_l` alongside `velocity`,
`angular_vel`, `rolled_hold_s` and `air_s` — in `app/main.cpp` AND in the tape fixture's
copy of the same mutation. Round-trip green at k = 1.0.

## K3 — MEASURED. It is a QUESTION for Chad, not a change.

`sled_probe wswheelie 6 [flat]` — stopped → WOT, peak pitch vs aft lean, both depths,
seated and standing. Bush 0.77 m, seated:

| lean_aft | aft_m BEFORE | peak pitch BEFORE | aft_m AFTER | peak pitch AFTER |
|---|---|---|---|---|
| 0.00 | 0.0000 | 10.47 deg | 0.0000 | 10.47 deg |
| −1.00 | 0.2500 | **11.39 deg** | 0.2786 | **11.50 deg** |

**The whole aft-lean catwalk authority is ~1.0 deg of peak pitch on a 10.5 deg launch —
about 9 %.** K1 raises it from +0.92 to +1.03 deg (a 12 % improvement ON that 9 %, i.e.
+0.11 deg absolute). Standing, K1 HONESTLY REDUCES it (+0.87 → +0.76 deg) because the
standing ceiling is really 0.2021. Ski load at full aft, Bush seated: 196 N neutral →
141 N before → **136 N** after (−31 %).

**Verdict: K1 did NOT make the catwalk adequate, and it never could have.** The arithmetic
ceiling: the rider is 87.5 of 331 kg, so 0.2786 m of body travel is only 0.0736 m of CG
travel against a 1.38 m wheelbase = **5.3 % of the wheelbase**. That is physically correct
and no honest clamp can beat it.

**QUESTION FOR CHAD (K3 goes no further without a ruling).** The candidate levers, none
built:
- **(i) GI4 §9.7 item 2** — traction-limited thrust. The thrust couple at the track is
  what actually lifts the nose; today it is not traction-limited, so it does not grow when
  the aft shift buys grip.
- **(ii) GI4 §9.7 item 3** — floor `w_contact` / rear-patch load share. Aft CG unloads the
  skis by 31 % already; the rise it buys is capped by how the planing lift is split.
- **(iii) Accept it.** ~1 deg of modulation on a 10.5 deg launch may simply be what a
  331 kg machine does, and the felt weakness may be the LOOK, not the physics — which
  R3-WS(d) has now changed anyway.

## Open questions / debts

1. **K2 ON or OFF?** Built at the honest k = 1.0, shipped at 0. The menu, per K-A4:
   honest transient (what is built — 0.2 rad/s, 2.3 deg, decays), the TRACK AS A REACTION
   WHEEL (a throttle blip in flight — the only honest SUSTAINED mechanism, NOT in this
   rung), or a feel-boosted k > 1 (dishonest, but it is his game).
2. **K3's lever** — the three-way above.
3. ⚠ **OPEN-KWS1-SAG.** `kRiderSeatOffY_m` is measured through `render::kSagDefaultM`
   (0.10 m), a VISUAL stance dial with no kernel owner. It is the number the shipped
   renderer actually draws, so it is the honest one — but K2's authority scales LINEARLY
   with it. Named, not buried.
4. The 16 % constant-inertia approximation is UNCHANGED and now also underlies K2's
   `I⁻¹`. Noted forward; inertia work is still not this rung.

---

# RE-SIZE RECORD — R3-WS(e), 2026-08-21 (Opus, sandbox/gi4-ride)

**Chad's ruling, same day, hours after K-WS1 landed:** *"defer the kneeling fit for
now, what we will do is not make kneeling for longitudinal movement… eventually [the]
pose activated for a hanging to a side lean… a hotkey so you can pull a side / unstick
it… key is going to be P for pull."*

The kneel LEFT the fore-aft ladder (`render::kKneelRuntimeGain` 1 → 0, held for the
future R4-SIDE "P" key). K-WS1 was sized against the kneel-inclusive ladder, so **every
number in the BUILD RECORD above that carries a 0.2786 is SUPERSEDED.** The record is
kept whole — it is the history of how the row was derived and why the row exists — and
this section carries the live numbers.

## The row, re-baked kneel-free on the live binary

| stand s | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
|---|---|---|---|---|---|
| R3-WS(d), kneel ON — **SUPERSEDED** | 0.2786 | 0.2668 | 0.2306 | 0.2041 | 0.2021 |
| **R3-WS(e), kneel-free — SHIPPED** | **0.2183** | **0.2146** | **0.2070** | **0.1984** | **0.2021** |
| (R3-WS(c), for reference) | 0.2183 | 0.2146 | 0.2083 | 0.1990 | 0.2021 |

`sim::kAftCeilC1 = {0.2183, 0.2146, 0.2070, 0.1984, 0.2021}`,
`lean_aft_max_m = 0.2786 → 0.2183`. Both re-pinned from the load-time bake print in the
same commit as the gain flip, provenance-commented, never retyped from this table.

⚠ The kneel-free row is NOT identical to (c)'s at every slice: s = 0.50 and s = 0.75
read 0.2070 / 0.1984 where (c) read 0.2083 / 0.1990. R3-WS(d) changed more than the
gain — the antiparallel bend-normal fix and the kneel s-band both move the seat/stand
weight split at those slices even with the kneel weight at zero. **Measured, not
assumed**; that is exactly why the row was re-baked instead of reverted to (c)'s.

## The three headlines, restated honestly

1. **K1 now REMOVES aft demand; it does not add authority.** The seated ceiling falls
   60.3 mm from (d) and lands 31.7 mm BELOW the pre-K-WS1 flat clamp of 0.25. That flat
   0.25 had been over-asking seated since long before this program — the last ~12.7 % of
   the player's aft mouse travel bought no pose at all — and the kneel had briefly
   masked it. **This is the number Chad should be told before he drives.**
2. **The reachability argument survives, with its sign flipped.** Under (d) the claim
   was "the old clamp cannot reach the top of the ladder"; now the old clamp over-shoots
   it. The property gated is unchanged: the clamp lands ON the ladder top — live bake
   reads **u = 0.9998** at the shipped clamp (s = 0), and the kernel's own over-ask
   sweep holds at **≤ 1e-9 m at every stand**.
3. **The 5-point row is still not a 2-point lerp.** On the new row an end-to-end lerp
   (0.2183 → 0.2021) over-promises a MEASURED **3.2 mm at s = 0.50** and **7.8 mm at
   s = 0.75**. Smaller than (d)'s 9.8 / 16.8 mm because the row is flatter. Still a lie.

## K-A5(b) re-verified — the demand step SHRANK, so continuity IMPROVED

`lean_aft_max_m` scales the render bake's demand step: **0.0044 → 0.0034 m (−22.7 %)**.
Worst pose delta per demand step, by slice, on the live bake:

| s | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 | bound |
|---|---|---|---|---|---|---|
| after K1 (clamp 0.2786) | 0.0274 | 0.0294 | 0.0319 | **0.0433** | 0.0308 | 0.060 |
| **after R3-WS(e) (clamp 0.2183)** | 0.0211 | 0.0230 | 0.0250 | **0.0340** | 0.0241 | 0.060 |

All green, no `LOG_FATAL`. Worst slice at **56.7 %** of budget (was 72 %). `monotone 1`,
`min dC/du 0.00041 > 0`. Continuity-in-`s` unchanged in kind: worst **0.0576** m per
0.0884 of stand slew at s 0.63 / u 0.80 against the kneel-free floor **0.0574**
(add **+0.0002**, allowance 0.030) — `OPEN-R3WS-STANDSLEW` still open and still owned by
the STAND key, not by this rung.

## K-A5(a) re-measured — the stand drag is a quarter of what it was

The clamp still drags a full-aft rider forward when he stands, and K2 still feels it:
seated **0.2175** (the slew is exponential, so he asymptotes just under the 0.2183
ceiling) → stood **0.2021**, dragged **0.0154 m**, peak Δω_x **+0.30828 rad/s**. Under
(d) the drag was 0.0754 m. Both test pins moved to the measured numbers with headroom.

## What did NOT change

- **K2 is untouched and still defaults OFF** (`k_air_shift = 0.0`). Not this rung.
- `kRiderSeatOffY/Z_m` — the rider's seated CG offset is a REST-pose measurement and the
  rest pose has no kneel in it: body y **0.353743**, z **0.357343**, unchanged.
- K3's verdict and its three-way question for Chad stand. If anything K1's contribution
  to the catwalk is now smaller still: the aft-lean lever is 0.2183 m of body travel =
  0.0577 m of CG on a 1.38 m wheelbase, **4.2 % of the wheelbase** (was 5.3 %).
- `aft_ceiling_curve` still ships at 1.0, still 0-OFF bit-identical, and the A/B arm back
  to the pre-rung kernel is still two numbers — now `aft_ceiling_curve = 0` **plus**
  `lean_aft_max_m = 0.25`.
