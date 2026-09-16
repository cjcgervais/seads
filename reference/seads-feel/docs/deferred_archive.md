# SEADS — Deferred/handoff ledger archive (moved out of CLAUDE.md 2026-07-08 harness audit)

> Verbatim archive of the CLAUDE.md `## Deferred` section as of commit 72cadabf.
> The live thread state now lives in CLAUDE.md `## Threads` (exactly ONE CURRENT banner).
> Entries below are HISTORY — their imperatives are superseded; follow the pointers only.

- **FRESH AGENT START HERE (2026-07-08, CURRENT): ORCHESTRATOR — read `docs/orchestrate_handoff.md`.**
  On `sandbox/world-sudbury` (`b379fd89`, gate 270/270). The `/orchestrate [module]` tri-model
  pipeline (Opus architect / Gemini asset factory / Fable math sniper) is live
  (`.claude/skills/orchestrate/SKILL.md`, `tools/gemini/`, `docs/orchestrate_modules.md`). **Fleet Rig
  rig-A is COMPLETE** (`docs/fleet_rig_plan.md`): A.1 pure core (`render/rig.{h,cpp}` + `to_ray_fields`),
  A.2 procedural mirror fleet (mirror VS/FS in `rig.cpp` + `draw.cpp` `DrawMesh` integration + cubemap
  hoist + `[fleet_rig]` config), A.3 headless asset-validator ctest (`test_asset_validator.cpp`), and
  the ★ Fable after-red-team folded (P0: raylib's default VS lacks fragPosition/fragNormal → authored an
  explicit mirror VS; validator parser hardened; two-sided units). Chad flew it on a real NVIDIA GL33
  box — the mirror shades per-surface. **NEXT (pick one): rig-B** (state-driven surface deflection from
  the commanded `sim::Inputs`, threaded read-only app→render + the differential firewall leg — see
  `fleet_rig_plan.md` rig-B) **or env Stage 3 = `scatter`** (the first environment module through the
  engine, `docs/little_planet_plan.md`). Fleet-rig FEEL tuning (mirror `reflectivity`/`fresnel_power`/
  colors in `config/world.toml [fleet_rig]`) is open for the flight log. NEVER touch `sim/`/`control/`.
- **FRESH AGENT START HERE (2026-07-08 late, CURRENT): Chad's fly verdict on the MB-lean
  pass = "flight kernel is near perfect now! Grand resounding success" (logged on the rows);
  since then LANDED: aim_sensitivity 0.18→0.16 (his "little slower" ask) + S-reticle
  (SPEC §0 — display-only capped ease of the reticle draw killing the integer-mouse
  staircase; the ONLY smoothing anywhere near the aim and it is a DISPLAY COPY, single
  consumer = the reticle projection, cap derived from quant_px·sensitivity; §9.1/AT-9/
  goldens untouched by construction). Next = CHAD FLIES the remaining rows, then MB-6.**
  Gate **247/247**. Evening session (Chad's post-fly
  asks; plan-stage AND diff Fable red-teams, all P1s folded, 7 lean mutants killed): **MB-lean**
  (SPEC §0, NEW cascade mechanism) — FINE tracking now LEANS `held_bank` toward
  `clamp(lean_gain·az, ±lean_max)` (az = de-rolled lateral azimuth on the axis
  `(cosΦθ, sin e.phi) == cross(nose_b, up_b)` — EXACT at every attitude; the diff red-team's
  P1-1 caught that a phi_full cos/sin de-roll is exact only at zero pitch, ~7° phantom lean
  at pitch45/bank40, pinned by the PITCHED companion leg): the magnet (steady err 0.5°→0.093°,
  the rudder/coordination standoff closed by lift) + sustained shallow bank at moderate
  deflection (4.5° carrot → steady 29.9° bank, 0 wing-wag); knob-off lean_gain=0 pinned
  BIT-identical; freelook now CARVES a held lateral aim (named side scope). **[deadzone]**
  0.10/0.25→0.05/0.12 (trim-hold leg reshaped to fraction+episode-TAIL — the head carries the
  re-arm tap transient; K_wi/deadzone coupling named). **MB-right dials** 1.0 s/180°/s (config-
  relative legs self-tracked; window premise re-derived; inverted_rate ≤ p_max tripwire).
  Prior same day (gate 243/243, plan-stage Fable red-team, 4 readout mutants killed): **MB-4
  EXECUTED** `yaw_scale` 1.5→2.0 (controller golden re-recorded deliberately; AT-15/AT-16/
  MB-rud walls held; step-yaw residual 0.647→0.505°, dwell probe clean — yaw's PIO wall is
  unmeasured, Chad's fly is the sentinel, 1.8 the fallback) + `aim_sensitivity` 0.15→0.18
  (moves nothing — verified); **MB HUD** — the AoA dial is now an ATTITUDE dial (pitch needle
  = asin(dot(nose,local_up)), HOLDS its read; full-range roll-arc bank pointer via
  `bank_full = atan2(−dot(body_right,local_up), cosΦθ)` — the asin φ FOLDS at 90°, never feed
  a roll gauge φ; slim AoA stall strip LEFT of the gauge), telemetry + labeled FLAPS/GEAR
  (commanded-latch labels + slew bars) moved to the bottom-left stack (top strip was
  unreadable). New FlightReadout fields pinned in test_hud (γ-slip/fixed-up/φ-fold/abs
  mutants all killed; the −100° sample is the abs-kill — upper-half-plane-only sampling is
  blind, S4a). Prior overnight (gate 240/240): each mechanism plan- and/or
  diff-red-teamed fresh-context, all mutants killed incl. the reviewers' own, SPEC §0 entries
  MB-aim/MB-flaps/MB-7c): **MB-aim** — the rate-keyed mouse acceleration curve
  (`input/aim_curve.h`, `[ui]` knobs; below-knee bit-identical linear, flicks up to 2.5×; keyed
  on |delta|/frame_dt at the device accrual, AT-9 untouched — the curve's own invariance is the
  power-of-two scale leg in test_aim_curve; quant_px guard vs fps-scaled pixel noise, loader
  rejects its disarm); **MB-flaps + gear** — force-only plant devices (F cycles clean/combat/
  landing, G toggles gear; deploy slew; Cl shift AFTER the stall clamp + quadratic drag tax; lift
  washes out above `wash_lo..wash_hi` while drag stays — over-speed flaps are SPEED BRAKES, the
  documented default Chad may swap for auto-retract; `sim::total_lift_coeff` is the ONE
  composition plant + true-n instrument share — the red-team caught the bare-Cl G under-read);
  **MB-7c energy legibility** — wind audio ∝ V (hushed by atm_frac), the AR AoA dial (arcs keyed
  to the same aoa_max as the clamp + vortices), wingtip vortex trails, SPD/ALT/G cluster with
  trend chevrons (all read-only, gunsight-firewalled; tunables = code constants in
  render/wind_audio.h, vortex.h, draw.cpp). **PENDING CHAD FLIES (all pre-filled flight-log
  rows, 2026-07-08): MB-lean (the magnet + shallow-bank + the freelook-carves scope),
  deadzone 0.05/0.12, MB-right 1 s/180°/s (watch the knife-edge coast), yaw_scale 2.0 +
  aim_sensitivity 0.18 (sensitivity moves the AIM, yaw_scale moves the CHASE), the MB HUD
  pass (incl. the bank-pointer-convention question — one sign flip if it reads backwards),
  MB-aim curve, MB-flaps/gear (incl. the speed-brake ruling), MB-7c (judge on
  the spiral test-cards), plus the earlier elevator/envelope/MB-atm rows + MB-right composition
  (P2-2).** THEN, per `docs/mission_b_instructor_plan.md` §3: MB-6 stall recovery (nose-to-wind,
  binary knob — the porpoise is AMPLIFIED above 6 km, SPEC §0 MB-atm seam; NOTE the flap tail-
  slide note, MB-flaps P3-3), MB-4 yaw_scale retune, MB-9 holistic envelope re-fly, MB-8
  deflection drag (Chad opt-in — flaps' quadratic tax may already scratch it), Mission A. Window
  is 1920×1080 resizable (main.cpp glue).
- **PRIOR (2026-07-07): S8-drone LANDED** — the non-shooting AI target-drone
  fleet + lead-angle gunsight is committed (`25a8cd9` → `635f87a` → `ffe6b55` on `sandbox/flight-tuning-2`,
  gate **185/185**), and cross-model audited (two Opus red-teams + a Fable 5 audit, all findings fixed;
  ledger in SPEC §0 `S8-drone`). What this unlocks: **the Tracking star is now honestly gradable** — there
  is finally a fleet of bandits (patrolling gentle banked weaves, `drone/drone.h` autopilot) to chase and a
  ballistic lead pipper + time-on-target meter (`render/gunsight.*`) to grade it, all tunable in
  `config/scenario.toml` (count/size/spread/program/gunsight). NEXT STEPS, pick one: (a) **tune the fleet +
  gunsight FEEL** against `docs/flight-log.md` now that Tracking is real (drone `turn_bank`/program, the
  gunsight cone/range, `aim_sensitivity`); (b) a **reactive/evading bandit** (extend `drone::ai_aim`/the
  autopilot — the `age_ticks` phase seam is built for it; keep it pure); (c) the **camera horizon-recovery**
  thread (`docs/horizon_recovery_plan.md`, planned — roll the shared aim/camera frame on freelook release);
  (d) the **WORLD** (`docs/world_art_direction.md`, `sandbox/planet-art`). The three original threads below
  are now: camera (S7-cam3 committed `0560ca7`; horizon-recovery planned), flight-feel (UNBLOCKED — a real
  target exists), world (open).
- **PRIOR (2026-07-07): read `docs/next_agent_brief.md`** (branch `sandbox/flight-tuning-2`
  off `main`). Three threads: (1) camera/mouse SOLVED — S7-cam3, camera-up = the
  carried aim-frame up, mouse-up == screen-up at every attitude (awaiting Chad's fly + red-team + commit);
  (2) the open FLIGHT-FEEL mission (fast/punchy/more-elevator/aggressive-smooth turning) — instructor/plant
  tuning via the `dogfight-systems` skill + `docs/flight-log.md`; (3) the WORLD — the legible "little world"
  (`docs/world_art_direction.md`): 1940s B&W small-town Canada, saturated punchy planes, which is ALSO the
  fix for far-side disorientation. Master the `dogfight-systems` skill first; the earlier
  `docs/mouse_loop_audit_handoff.md` is SUPERSEDED (its problem is solved). The lesson that unlocked it:
  `docs/milestone_feel_bug_attribution.md`.
- **FRESH AGENT: §7 feel worklist is in `docs/section7_worklist.md`** (2026-07-05, branch
  `sandbox/flight-tuning`) — ordered, one-at-a-time items: (1) split-S won't flip past vertical ⭐,
  (2) camera must roll 180° through inverted, (3) more yaw, (4) deferred aim clamp. Read it first for
  what's landed this session (S7-ovr/ovr2, S7-cam Phase 1, S7-push, pitch-rate tune) before starting.
- **FRESH AGENT START HERE — SECTION 7 (tune for feel) IS UNDERWAY.** The Section-7-*prep* queue is
  CLOSED (all 7 items done; log in `pre_spec_audits/section7_prep_handoff.md`). Section 7 proper is a
  human-in-the-loop feel loop gated by the Delight Test — read `docs/flight-log.md` (the operationalized
  gate: one knob per flight, hypothesis→observed, star rubric, tuning order). Objective instrument for
  tuning-order steps 1–2: `seads_harness step [axis] [deg] [V] [ticks] [csv]` (closed-loop step
  response → settle/overshoot/reversals + gains + ZOH margin; kept alive by `step_harness_smoke`; NEVER
  a gate — AT-2 owns pitch-step pass/fail). Gate 139/139 on `main`. Coefficients/gains ONLY (SPEC §15.7);
  the user flies each change — do NOT tune gains autonomously against harness numbers (they explain a
  score, they never replace the stick).
- **CLOSED in Section 6** (`section-6-gate`): the §0 supersessions (T0 toolchain, **S4→S5** app-wiring
  re-sequence, **S5-cam** camera-contract activation) are now in the frozen SPEC §0 "Post-freeze
  supersessions" list, and the stale forward-refs (SPEC §15.3, `camera.h`) point at it. AT-11 full AoA
  sweep + AT-16 banked-tracking |β| + AT-13/AT-14/circumnav/poles landed in `test/unit/test_acceptance.cpp`.
- **HONEST LEDGER — `section-6-gate` is NOT "AT-0…AT-18 all green" (SPEC §14 obj.4).** Do not read the
  green gate as full AT coverage; that would be the §0 "silently redefine compliance" trap. Mechanized:
  AT-0/2/6/7/8/11/13/14/16/17/18 (**AT-6** landed post-gate — `test_acceptance.cpp` "AT-6: the same
  pull is heavier…": one-tick 20° pull at V=140 vs 224, emitted pointing pitch == the config-derived
  braking-law min, the G-clamp `w_max∝1/V` is the binding branch at both, ratio tracks 140/224;
  mutation-verified against `V_clamp→v_min`). **NOT mechanized anywhere → Section 7:** AT-1
  (cursor-connected / zero-smoothing), AT-3 (rapid displacement), AT-4 (never hunts), AT-5 (natural
  settling). **AT-12 MECHANIZED post-AT-15** (`test_acceptance.cpp` "AT-12"; queue item 3): a
  closed-loop sustained max-rate turn (10 s, full throttle, ~5 g / ~87° bank — the shared
  `harness/instructor.h` `at12_*` driver, also flown by the `ctrl_fly` CSV recorder). Two jobs SPLIT:
  (1) speed-retention PRINTOUT (the Section-7 drag-knob readout — NOT gated; gating an untuned
  retention target would re-create the AT-6 phantom-chase); (2) a tune-INDEPENDENT energy GATE — the
  plant's per-tick linear work `m·v·(v'−v)` (algebra on the plant's OWN output, so the semi-implicit KE
  surplus `½m|v'−v|²` never enters and the reconciliation is EXACT, not O(dt)) reconciles with the
  config grav+thrust+drag work (lift·v ≡ 0 by construction — SCOPE, not a hole: a lift defect moves the
  trajectory, never the energy). Plant==config ⇒ rel ~1e-13; a step.cpp drag-code fork (mutation:
  `k_induced→0`) sends rel to ~0.2. Certifies the plant obeys its own `Cd=Cd0+k·Cl²` law (so the drag
  knob is tuned against an HONEST instrument) at ANY drag tune. **ONE Fable diff-consult** (found the
  float-throttle seam that false-tripped the tune-independence claim on a retune, the signed-residual
  cancellation risk, and the near-cancelling net-power denominator — all fixed: float-cast mirror,
  per-tick abs residual, component-throughput denominator). **AT-9 MECHANIZED post-P1b**
  (`test_at9.cpp`): `app::step_frame` extracts the shipped accumulator frame (accum.advance + per-frame
  mouse-consume + tick fan-out + in-seam crash neutralization; main.cpp delegates), and the same
  scripted 3-flick aim sequence at 30 fps (`4*sim_dt`) vs 240 fps (`0.5*sim_dt`) flies the BIT-IDENTICAL
  double trajectory (pos_err==0.0, LoopState-wide ==); companions pin exact tick count + efficacy + a
  0-tick-frame pending-carry, a forwarding-mirror leg pins every FrameInput field step_frame forwards
  (the round-4 moved-consumer catch), and an absolute spawn-reference pins the mid-frame-crash
  neutralization. Two Fable consults (plan + impl). **Partial (mechanism pinned, full scenario not):**
  AT-10 (unwind
  pinned in `test_cascade`; the pinned-10 s-then-center closed-loop scenario pending). **AT-15 MECHANIZED
  post-P1b** (`test_acceptance.cpp` "AT-15" legs; P2b closed before touching the §7 `push_gate` table):
  SEVEN legs, all mutation-verified: 60°-below rolls through inverted (closed-loop, `cosΦθ<0` at the
  first-invert tick welded to `|φ|>90°` fold; corkscrew guard is the emitted pitch as a FRACTION of the
  w_min budget pre-inversion — clean 0.03 vs align:=1 corkscrew 1.03 — plus the plain `n_min>−3` floor;
  nose captures) · mid-push ratio-exit handoff (`edges==2`) · ratio-band no-dither (`switches≤1` vs
  collapse 8) · bank-band no-dither (bankErr ripple straddling `push_gate_bank`, `switches≤1` vs collapse
  7) · heavier at 1.6V (6°-below pushes at V, rolls at 1.6V — behavioral via `omega_z`) · closed-loop
  sustained PUSH from a banked entry (wings held, real −1.3g, `bank change <10°`) · the bank-side exit
  clause. **TWO Fable diff-consults.** First found 3 blind spots (Leg 2 passed a corkscrew `align:=1`; no
  closed-loop push existed; bank-exit clause was dead code) — all fixed. Second attacked the FIXES and
  their implications: the tightened bounds were CALIBRATED to today's untuned tables while guarding the
  retune of those tables — worst, the first fix's `n_min>−1.0` both false-fails a legit tune AND silently
  DISARMS if `[g_limits] n_min` softens (the corkscrew's G scales with the budget). Reformed to be
  config-relative / budget-relative: the corkscrew fraction above (scales with any n_min retune), the
  bank-exit swap angle DERIVED from `push_gate_bank_lo`, Leg 5's live-roll floor DERIVED from
  `blend_lo/blend_hi/p_max`. **Honest sub-note (supersession):** HARNESS's "15°→pushes / 60°→rolls" are
  illustrative; at the CURRENT untuned table the boundary is ~7° (6° pushes, 8° rolls), so the legs use
  6°/60° and `test_cascade`'s "15° rolls" is correct-for-now. A §7 `push_gate` retune (or a `[regime]
  blend_lo` tweak — a HIDDEN dependency of the whole 6°-family) trips the legs' REQUIRE premises LOUD;
  the boundary-tied constants are enumerated AS A SET in the AT-15 banner (`test_acceptance.cpp`) and
  flagged at HARNESS.md AT-15. **P1b CLOSED post-gate:**
  the shipped app tick is now `app::tick`/`app::instructor_camera`/`app::instructor_focus_loss` in
  `app/instructor_tick.h` (main.cpp delegates), pinned by `test/unit/test_instructor_tick.cpp` — crash
  predicate (first-crossing boundary, both modes), GROUNDED neutralization, the frame-carried camera
  call site, the CQ2 mouse gate + ease-back suspension, the freelook aim-snap (rules 2/3), GROUNDED
  reseed/throttle/prev_up, and a `ClosedLoop` mirror-equivalence leg (position EXACTLY bit-identical
  over 5 s — observed `pos_err == 0.0` at max_digits10 — pinned `< 1e-6 m`, tightened from the P1b
  `< 1e-3`; a float-`dt` slip lands ~4e-5 m and is now caught, was a hide-band). A 2nd
  Fable consult on the IMPLEMENTATION caught that the mutation-verify had stopped at the 3 named
  mutations and missed two SPEC-mandated consumers the extraction moved onto the shipped path (the CQ2
  ease-back `mouse_aim_live`, the freelook snap) — both now pinned; 8 mutations verified. **Coverage
  round 3 (AT-9 Fable consult re-attack, same class):** two MORE moved consumers pinned — the keyboard
  override passthrough `ci.override_mask/sign -> control::step` (delete it and the live override goes
  dead; `any_ovr` reads `in.` not `ci.` so the snap leg missed it) and the GROUNDED `fl.reset()` (a
  respawn mid-ease-back would inherit the dead life's mouse-suspend); both mutation-verified. **Residual
  caller-glue (acceptable — NO ctest runs `seads.exe`, only `seads_harness`):** the raw-mode grounded
  neutralization (`main.cpp` post-crash `raw_in={}`; no live bug — post-crash the caller zeroes it and
  the only other grounded-raw path is an F1 toggle where the pilot is actively flying), the per-frame
  `mouse_consumed`/orbit/ease-back-decay/F1-toggle, and that main.cpp keeps CALLING `instructor_camera`
  (the mutation-unexpressibility holds only while it does). **Full triage: Fable 5 cross-model audit
  `pre_spec_audits/fable5_section6_fixreport.md` (P0/P1a/P1b/P2b/P2c; verdict: gate sound to enter §7).
  **All of P2a/P2b/P2c now DONE** (P2b=AT-15, P2c=AT-12; **P2a closed-loop G-floor value pin DONE**,
  queue item 7 — `test_acceptance.cpp` "AT-11: sustained G floor settles true-n at the derived
  setpoint": flies the sustained floor and reads the INDEPENDENT true-n instrument to close the gap
  that the one-tick (b) test recomputes `w_min` with the controller's own `(n_min−cpt)·g/V`, so a
  sign/credit mutant is invisible; oracle = derived setpoint `n_min + V·ff_x/g`, ±0.15 g;
  mutation-verified `n_min+cpt`→−3.9 and drop-cpt→−3.4 both FAIL, both green on (b). ONE Fable
  consult re-attacking its own report proposal.) **P3c one-frame stale camera DONE** (queue item 5): reseed `aim`
  in the crash branch of `app::tick` (`instructor_tick.h`) so a last-tick crash doesn't render the
  fresh spawn through the DEAD life's aim frame (astern/inverted camera pop) — idempotent with the
  next GROUNDED reseed (next-tick transport is identity + the pairing overwrites `q` from the same
  seed ⇒ post-GROUNDED bit-identical, trajectory untouched, OUT of byte-identical work). Test leg
  (`test_instructor_tick.cpp` crash-reset case) pins aim==nose AND up==local_up on the respawning
  tick's RETURN (before any next tick). ONE Fable diff-consult (SOUND-WITH-NITS): retuned the leg's
  dive heading `-Z`→`+Y` so BOTH bases separate under the mutant (a `-Z` heading coincides with the
  spawn nose, letting a rebuild-up-only mutant pass the forward check — S3 "bases must SEPARATE"), and
  flagged a sibling residual now fixed — `render::CameraOrbit` reset on respawn in `main.cpp` (dead
  life's freelook orbit no longer eases in on the fresh spawn; cosmetic, caller glue). **P3a
  holonomy-sign pin DONE**
  (`test_aim_frame` banked-circle case: canonicalized `q_rel` axis ∥ **+u_start** — a +φ loop encloses
  the +Z cap on its LEFT ⇒ positive rotation about the outward base normal, NOT −u_start; margin 1e-3;
  mutation-verified, `cross(b,a)` flips dot to −0.65).**
- **[DONE — queue item 3] Controller CSV telemetry leg (HARNESS §4 `ctrl_fly`; CQ1 true-n write).**
  `harness::CtrlCsvTelemetry` (`test/harness/telemetry.h`) + the `seads_harness ctrl_fly [ticks] [csv]`
  mode: closed-loop AT-12 max-rate turn written with the full controller columns — e, ω/ω_des, Inputs,
  integ, regime/push/ballistic/deadzone/pursuit, φ/β/α/aoa_filtered, and the true `load_factor` beside
  the flat `cos_phi_theta` G-proxy (CQ1 — the felt gravity-bias made visible per tick). Read alongside
  the flight log in Section-7 drag tuning; NOT a gate (kept alive by the `ctrl_harness_smoke` ctest).
- **cosΦθ>0 knife-edge wings-hold dither (P3-a) → Section 7 feel** (bounded, measure-zero attitude; add
  hysteresis or a `max(0,cosΦθ)` fade when tuning).
