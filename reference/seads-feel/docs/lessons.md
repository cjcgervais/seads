# SEADS — Learned lessons corpus (moved out of CLAUDE.md 2026-07-08 harness audit)

> Verbatim archive of the CLAUDE.md `## Learned` corpus as of commit 72cadabf, plus all
> lessons appended since. APPEND NEW DURABLE LESSONS HERE (same one-bullet format, tagged
> with the mechanism name). CLAUDE.md keeps only the ~10 highest-traffic trap classes inline.

- A raylib mesh shader loaded via `LoadShaderFromMemory(nullptr, fs)` gets raylib's DEFAULT vertex
  shader, which (GL33, `rlgl.h`) emits ONLY `fragTexCoord`/`fragColor` with `uniform mvp` — NOT the
  `fragPosition`/`fragNormal`/`matModel`/`matNormal` a lighting/reflection FS reads. So a custom mesh FS
  MUST author its own VS (the `render/planet.cpp` `kVS` pattern; `DrawMesh` auto-uploads
  `mvp`/`matModel`/`matNormal` by name). Relying on the default VS links on a LENIENT driver (positions
  still come from `mvp`, so the geometry draws) but leaves the FS inputs UNDEFINED — the effect silently
  doesn't compute (rig-A.2 drew a flat blue plane, no reflection) — and fails to link on a STRICT one.
  The killer: **no ctest runs `seads.exe`** (only `seads_harness`/the pure libs), so a shader-linkage or
  undefined-input bug is INVISIBLE to the green gate — the Fleet Rig gate was 263/263 with a mirror that
  never mirrored. Fold: when a new shader lands, VISUALLY confirm it draws once (a `--smoke N shot.png`
  screenshot), and pin the shader SOURCE's declared uniforms against an allowlist headlessly (the rig-A.3
  validator — declared ⊇ active, so a source check is stronger than GL introspection AND needs no
  context). Caught by the cross-model Fable AFTER-red-team, not the honest gate. (rig-A after-consult P0)
- A headless GLSL-source uniform-allowlist validator must parse STATEMENTS (split on `;`, commas as
  separators), not whitespace tokens: a `;`-glued or comma-listed smuggle (`...;uniform float u_time;`,
  `uniform float a,b;`) — exactly what a minified/generated shader emits — slips a whitespace tokenizer,
  and the `got == allowlist` equality still passes because the extra name is never even seen. Also reject
  any `#` directive except `#version` (a `#define` can macro-paste a keyword past the parser). A
  "mutation-verified" banner on the whitespace parser was calibrated to today's hand-formatting — the
  AT-15 trap, for a text parser. (rig-A.3 after-consult P1-1)
- RMB-hold ZOOM (S9-zoom, `sandbox/flight-tuning-2`, 2026-07-07) is a pure CAMERA/FOV feature — it cannot
  move a controller golden (the instructor sees only `aim.forward()`) and is RA9-safe: the eased `fovy` and
  the pipper-re-centered `render_fwd` are strictly DOWNSTREAM (render pose only), never fed to mouse→aim.
  The ONE graze of the mouse gain — `aim_gain_scale` (halves sensitivity while zoomed for precision) — is a
  BINARY function of the RMB button, NOT the eased fovy: an fov-eased gain would be the frame-quantized
  signal driving the control-relevant aim that diverged AT-9 (the S7-mouselevel trap); a scalar magnitude on
  the raw delta is not a basis smoothing. Re-center on the pipper ONLY in mouse-aim, gated by an EASED
  `recenter_gate` (a hard step whips the camera at the freelook edge). One dynamic `fovy` feeds Camera3D +
  off-center lens frustum + reticle/pipper projection + `lens_shift` (recomputed at that fov) — a stale 60
  anywhere desyncs the reticle. Tunables are code constants in `render/camera.h` (`kZoomFovyDeg`,
  `kZoomEaseTime`), Chad's ruling. (S9-zoom)
- A new DEFAULTED `FrameInput`→`TickInput` field (the round-4 forwarding trap, now 5th instance) needs a
  forwarding-mirror leg (`test_at9`) on a PRE-freelook applied-mouse frame — a post-release frame's mouse is
  CQ2-suspended (~36 ticks at sim_dt=1/120, longer than the whole test schedule) so the gain never reaches
  the aim and the leg passes VACUOUSLY. Mutation-verify ONLY on a binary you PROVED is fresh: a locked
  `seads_tests.exe` silently fails the relink (`ld: Permission denied`, easy to miss under a `grep error`),
  so a "mutant not caught" was a STALE binary — `rm` the exe + confirm the relink first. (S9-zoom red-team P1-1)
- The "up-is-down after a split-S" trade is now RIGHTABLE without touching the raw mouse (S7-hrz, SPEC §0,
  `docs/horizon_recovery_plan.md`): because mouse basis and camera are ONE quaternion (S7-cam3), a roll of
  `loop.aim` about its own forward is a GAUGE move — invisible to the instructor, mouse-up == screen-up
  preserved. Trigger = freelook RELEASE (a player action — no gate machinery); the angle is CAPTURED ONCE
  at the edge and counted down OPEN-LOOP — a per-tick recompute of the local_up target closes the banned
  control-driving-quaternion loop through the mouse-driven forward and flips at the zenith (the plan-stage
  fresh-context red-team's P0; red-teaming the PLAN before coding caught it for the price of a consult).
  The S7-mouselevel ban below still binds everything CONTINUOUS/autonomous; this is the one sanctioned,
  scoped exception. (S7-hrz)
- "The mechanism lives in the shared tick path so the mirror-equivalence pin covers it" is only true if
  the harness mirror can REPRESENT the mechanism: ClosedLoop carries a bare vec3 aim, so an
  AimFrame-level behavior (the S7-hrz roll) is unrepresentable there and the mirror covers ONLY its
  knob-off arm — an app-only mechanism needs its own dedicated end-to-end neutrality pin (rate-on vs
  rate-off, same input trace, pos_err at max_digits10). Same class as "a golden only pins the code that
  recorded it," applied to the mirror. Bonus, from the RE-REVIEW round: a load-bearing ORDERING comment
  ("capture AFTER the snap") is not a test, and the FIRST leg written for it was a FALSE kill — at a
  level-flight release `up_misalignment` is INVARIANT under a yaw-only aim:=nose snap (the geodesic axis
  is ∥ local_up, which fixes local_up) and second-order under pitch-only, so the hoist mutant stayed
  green; the kill needs a pitch+yaw hold dragging the release forward well off-level BOTH ways. Verify
  the mutant actually FAILS before writing "mutation this kills" — and have the re-review RE-RUN the
  mutants, not just read the legs. (S7-hrz diff red-team)
- DO NOT ease/re-level the MOUSE BASIS (the aim frame's up/right) to fix "up-is-down after a loop" — ANY
  such easing curls an active sweep (the eased up/right feed the next apply_mouse) and is the §9.1-banned
  smoothing on the mouse→aim path, the same class as the rejected `level_up_to`. Chad's standing rule: the
  plane chases the RAW aim, no auto-leveling of anything. The whole S7-mouselevel `roll_toward_local_up`
  saga (three gate revisions, each exposing a new failure) ended in full removal; the post-maneuver
  up-is-down is the accepted trade of the pole-free frame. Two hard-won sub-lessons from the dead-end,
  worth keeping: (1) a per-tick modification of a CONTROL-DRIVING quaternion gated by a FRAME-quantized
  signal (an idle timer reset on the per-frame mouse delta) is frame-rate-dependent and diverges the
  trajectory (~1cm, AT-9) — any such gate must be a pure STATE function; (2) it slipped the first red-team
  + the mirror test because both exercised only the no-op case (level cruise, zero step) — a "downstream/no
  rubber-band" claim must be tested with the easing ACTUALLY FIRING on a non-trivial path. (S7-mouselevel) 
- Reversing a landed gate to a bare boolean (`cos_phi_theta > 0`) re-introduces a knife-edge chatter the
  hard rule ("every gate hysteretic") forbids. A `smoothstep(-band,+band,x)` FADE fixes it AND stays
  golden-bit-identical — smoothstep is EXACTLY 1 for x≥hi, so upright (cos≥band) is unchanged; it only
  bites within ±band of the pole, which real banked turns pass through only in MANEUVER (roll_hold weight
  (1-blend)≈0), so no golden moves. A multiplicative fade beats a latch here (no state, no chatter, exact
  passthrough). (S7-loop-invert red-team P2-1)
- A flat-frame instrument certifies a flat-earth controller: telemetry/harness "level/bank/trim" must
  recompute from `local_up`; closed-loop rigs use the spherical `step()`, never a flat stub. (S3)
- Dynamic pressure once, in the plant; controller inverts the SAME params. Measured-vs-derived α_max
  catches plant-side double-counting only; the τ_cmd→Input→τ round-trip catches the controller side
  (AT-18, both legs). (H1)
- The deadzone holds trim — zeroing deflection on a trimmed, curving airframe manufactures the limit
  cycle it was meant to kill.
- Freelook reset is conditional (only-if-override-used); unconditional reset makes the held turn a surprise.
- No D-on-error: damping comes from measured ω in the rate loop; D-on-rate amplifies noise.
- A slewed camera basis feeding the aim update = smoothing inside the loop = negative damping =
  rubber-band. Aim basis is raw; only the rendered camera eases. (RA9)
- The v→0 apex degenerates in FOUR channels (v̂ undefined; G-clamps diverge; outer braking law goes
  quiet on q_att_floor; tail-slide α lies) — an inner-loop authority floor alone fixes none of the
  other three. (RA3)
- A great-circle-lap holonomy test is a null test (2π ≡ identity) — assert identity on the lap AND
  nonzero area/R² on a banked small circle. (RA8)
- Sim state is DOUBLE precision: at |p| ≈ R = 15 km, float ulp (~2 mm) is the order of one tick of
  gravity displacement (g·dt² ≈ 0.7 mm) — a float-position point at low speed literally cannot move.
  Render casts down at the seam; telemetry prints max_digits10 or the instrument is quantization-blind. (S1)
- Test/harness exes link `-static`: the gate runs under Git Bash, whose PATH shadows libstdc++-6.dll
  (0xc0000139 in every test); binaries must not care which shell launched them.
- An identity-start, constant-ω quaternion test cannot see Hamilton order/sign errors (q and ω commute
  along that whole trajectory) — pin the body-frame convention with a non-identity q0 against the
  closed-form q0·exp(tω/2). Mutation-verified both ways. (Section 1)
- Invariant asserts must check what the integrator APPLIED (Δv radial), not the vector handed to the
  assert — a mutated integration line sails past a claims-checking tripwire. (Section 1 red-team)
- Catch2 test names must stay ASCII: `catch_discover_tests` round-trips the name through ctest and the
  Windows codepage mangles `§` etc. — the filter then matches nothing and the test "fails" as no-tests-ran. (S2)
  RECURRED 2x in R5 (em-dash) and again in R6 (Greek `ρ`) — DESPITE being documented here. It is
  insidious because the FIRST run flags it (a visible ctest failure), so it costs one build cycle, not
  a silent ship; but the fix is trivial and the recurrence rate says: write test names in plain ASCII
  by reflex, never paste the SPEC/comment symbol into a `TEST_CASE(...)` string. A CI grep for
  non-ASCII bytes in `test/**` would end it mechanically. (S2 · R5 · R6)
- A golden only pins the code that recorded it; its failure mode ("golden moved") invites a re-record
  that would bless a bug. Every named mechanism needs a first-principles tripwire the golden can't
  substitute for — e.g. BOTH one-tick semi-implicit order tests, translational AND rotational (the
  rotational mutation is invisible to constant-ω quat tests and to AT-18a, which reads ω only). (S2 red-team)
- Implement every spec-assigned plant behavior BEFORE recording goldens (throttle slew §9.8 nearly
  slipped to Section 4 — landing it later would have moved every golden at once). Plant-complete first,
  then record. (S2 red-team)
- Config values with sim/ semantics live in one place: `sim/aero.h` holds the four-site expressions
  (q_eff floor, δ_max_eff ramp, Cl cap, v̂ guard) — plant, telemetry, later controller inversion and
  AT-18 all call THESE; a re-derived copy anywhere (even a `speed >= eps` branch) is an H1-class fork. (S2)
- A "which up" contract needs a test state where the candidate bases SEPARATE: in level flight
  body_up == local_up, so every level-attitude camera test passed a camera-up built from body axes
  (mutation-proven, both preference-order and 30% blend). One 60°-banked case with a tight
  `dot(up, local_up)` bound killed both. Same class as "flat instrument certifies flat controller". (S3)
- `2·acos(|q_rel.w|)` has a ~3e-8 noise floor near identity (acos of 1−ulp) — quaternion-angle
  assertions need ≥1e-6 tolerance or a different metric; 1e-9 fails on exact-match cases. (S3)
- A geometric clamp solved by quadratic root: with f(0) ≥ 0 > f(1) the in-segment crossing is the
  SMALLER root — taking the larger one clamps to t=1 and returns the offending point untouched, a
  clamp that silently never fires. Pin such clamps with a case asserting the point MOVED. (S3)
- Render numerics at R = 15 km: raylib's default far plane is 1000 m (rlSetClipPlanes; raylib ≥5.5);
  depth precision scales linearly with the NEAR plane (2 m, not 0.5); coarse-mesh overlays need
  offset > edge sag R(1−cos(π/slices)) ~ 14 m, not "a few metres". Double→float only after re-basing
  to the camera eye. (S3)
- A two-copies seam test must POISON the copy it claims isn't read: the v→0 guard test aliased the
  sim's held v̂ with the caller's, so extract() reading SimState.last_vhat (the exact §9.6 violation)
  passed. Same class as "separate the bases", applied to state copies. (S4a red-team)
- Angular-error sign tests confined to the upper half-plane leave magnitude structure unpinned — an
  abs(y) in bank_error survived every case with y ≥ 0, silently capping |bankErr| at 90° (would have
  structurally disabled roll-through-inverted in 4b). Every angle primitive needs a beyond-±90° case
  and an exact atan2(+0, −) = +π latch-boundary pin. (S4a red-team)
- φ = −asin(·) FOLDS past 90° bank (bank 100° reads φ = 80° and decreasing) — inside the fold,
  wings-hold feedback sign INVERTS. 4b's FINE wings-hold must gate on cosΦθ > 0; the fold is pinned
  executably in AT-0. Also: aim := nose (every reset) puts target_body at exactly (0,0,−1) — gate the
  EVALUATION of bank_error on the regime, never compute-both-and-blend (it asserts). (S4a)
- Boundary pins via derived quantities are fragile: |eps·(quat-rotated axis)| landing exactly on eps
  is toolchain luck — pin the predicate itself with an exactly representable magnitude too, or a flag
  change silently evaporates the mutation coverage while the test keeps passing. (S4a red-team)
- bank_error = atan2(x,y) is degenerate at BOTH lateral poles: aim := nose (target_body→(0,0,−1)) AND
  exactly astern (→(0,0,+1)). Astern is the sneaky one — err ≈ π puts blend = 1, so a blend/regime-only
  guard STILL evaluates it and the assert aborts. Gate on the lateral magnitude x²+y², not the regime;
  exact astern is owned by the +X elev pull-through. Every atan2-lateral decomposition needs the pole
  guard, and "we're in MANEUVER" does not imply "off the pole." (4b red-team)
- A round-trip/property test that exercises a HELPER the production path re-implements proves nothing
  about production: AT-18b round-tripped plant_invert while step() inlined its own identical denominator
  — the H1 single-count was unpinned on the live path. Route the live division THROUGH the tested
  function so the pinned code IS the shipped code. Same class as "golden only pins the code that
  recorded it." (4b red-team)
- Curvature feedforward's end-to-end proof is ω_des.x ≈ −V/R in the deadzone (level flight pitches DOWN
  at V/R to hold the great circle) + altitude holds tight; a wrong sign loops/dives. The deadzone then
  PARKS at its re-arm boundary with occasional taps — that is the deadzone working, NOT a limit cycle;
  "zero exits" is the wrong assertion (the pathological cycle is ~6 Hz degrees-scale, this is bounded to
  the deadzone). Assert the feedforward value + altitude, not exit count. (4b)
- Bank-to-turn banks ~90° for a purely-lateral target: bankErr = atan2(x,y) with y≈0 is ±90° regardless
  of offset magnitude, so even a small lateral aim commands a hard roll to bring it overhead. Correct,
  not a bug — don't "fix" it by capping the roll demand. (4b)
- Override lands as a strict SUPERSET: Input/Internal gain override fields DEFAULTED OFF, so the 4b golden
  reproduces bit-identically — that byte-level no-op IS the regression proof that 4c didn't perturb the
  pure instructor. Do NOT extend the scripted golden flight with an override leg (moves the golden, loses
  the signal); pin override with ATs instead. Same discipline as "plant-complete before recording." (4c)
- A per-contract gate must hold in EVERY branch that does the gated thing: "pursuit suspended while an
  override is held" (§9.5) was gated on the cascade but the BALLISTIC attitude-hold — itself a form of
  pointing the parked cursor — silently ignored it until red-teamed. Grep every branch that computes a
  pointing ω_des, not just the main one. Same class as "flat instrument certifies flat controller." (4c)
- AT-7's "overshoot" is the release COAST (kinematic stopping distance ω₀²·I/(2·τ_max), the nose carrying
  past the release angle as its rate is arrested), NOT overshoot-past-aim; "≤1 reversal" is about the
  CATCH transient (release→closest-approach), which is monotone. The untuned gains then show a slow,
  bounded, self-correcting tracking drift of the held world-aim — Section-7 feel territory, afflicts
  mouse-aim equally, NOT the release catch. Scope AT-7 to the catch window; a running-min lobe counter
  over the full window miscounts the coast AND the steady drift as oscillation. (4c)
- "Consent to leave the envelope" (override bypasses AoA/G) needs its OWN test at ramp=1 driven into
  stall/beyond-n_max: every other override test runs at ramp≈0.1 where the clamp wouldn't bind anyway, so
  a refactor re-applying protection to a held axis passes the whole suite otherwise. Same class as "a
  golden only pins the code that recorded it." Assert override_sign is exactly ±1 (fail loud on caller
  misuse; the plant clamps authority but the HUD/flight-log would misreport |Input|>1). (4c red-team)
- Freelook is 100% caller-side (the pure core has no freelook flag): its logic is a SHARED pinned module
  (`input/aim_state.h`, `input::Freelook`) that BOTH the harness ClosedLoop and the eventual app call —
  never reimplemented per caller, or the two drift (the "route the live path THROUGH the tested function"
  discipline, applied to a caller state machine, not just an inner division). Freelook-off ⇒ aim path a
  byte-for-byte no-op ⇒ the 4b golden reproduces (strict superset, same proof shape as 4c). (4d)
- The aim-snap latch snaps on the FIRST `any_override` of a hold, NOT on a rising edge: an edge guard
  silently misses "override already held when freelook begins" (no edge inside the hold) — the same
  "grep every branch that does the gated thing" trap as 4c's ballistic pursuit. One latch (`override_used`)
  serves double duty: fires the once-per-hold snap AND drives the conditional release reset. (4d)
- The CQ2 ease-back window length is fp-fragile: an off-by-one (decrement-before-compare, `>=` vs `>`)
  shifts the first-live tick by exactly one, and the fp boundary of `ceil(easeback/sim_dt)` is ALSO ~one
  tick — so a realistic-values test must slop ±1 and the mutation hides in the slop. Pin the length with
  EXACT integers (`dt=1, easeback=5` — `5−1−1−1−1−1==0` with zero rounding) in a direct `Freelook::step`
  test, asserting the precise tick. Same class as S4a's "pin the predicate with an exactly-representable
  magnitude, not a derived quantity." (4d red-team)
- Override held THROUGH the freelook release (key up on a LATER tick than Space) is orthogonal to the
  release snap: `aim := nose` sets the POINTING target while the held key still drives its axis in-core
  (pursuit stays suspended), and the later key-release is the ordinary 4c catch (φ_held recaptured at the
  live bank) — aim and φ_held govern different references and never fight. SPEC §9.5 sequences each event
  but not the overlap; the behavior is DECIDED here and both release orderings (key-before-Space and
  Space-before-key) are pinned. (4d)
- A signal gated OFF in a regime must have its FILTER frozen AND re-seeded on exit, never left running: the
  AoA low-pass ingesting the tail-slide's α≈±π through BALLISTIC carried that lie across the exit edge into
  the protection clamp — a bounded but felt uncommanded pitch hardover. Same class as "the deadzone holds
  trim" / "a blended α is banned." The fix must ALSO pin the CONSUMER (the clamp reads the FILTERED value,
  not raw e.alpha) or a raw-α mutation makes the whole freeze+reseed dead code. (4d audit)
- A "dead code / unreachable / can't-pin" dismissal of authority-limited logic must be evaluated at the
  AUTHORITY FLOOR (c·q_att_floor, low speed) and across ALL entry branches, never at cruise q or only the
  large-signal branch: the anti-windup unwind term (`|| eo·tau_cmd<0`) is LIVE below ~53 m/s (every
  hammerhead — K_wi·integ_cap 10000 > floored pitch denom 1960, so the capped integral alone saturates),
  and push_mode DOES engage at 6°-below/100 m/s (a below-nose, ahead-of-the-wing-line target; the
  historic "ratio 0.80 ≤ lo" rationale is superseded by S7-push's geometry gate but the engage-at-6°
  point stands). The cross-model re-review's whole value was attacking these first-model rationales, not
  just the code — both were pinned in one tick from `reset()` instead of deferred. (4d audit)
- The aim/camera frame is ONE quaternion carrying both aim-forward AND camera-up (`input/aim_frame.h`):
  transport right-multiplies a unit rotation, mouse rotates about the frame's OWN axes — so the two
  accumulate the IDENTICAL holonomy (§9.2) and the frame can never shear or degenerate at the zenith
  aim (the pole-free representation §9.1 mandates over Euler/local_up bases). The RAW-basis ruling is
  pinned executably by contrasting a frame-up yaw (moves the aim) with the banned local_up-axis yaw
  (near no-op) at forward≈local_up. (S5)
- The great-circle-lap holonomy NULL test (RA8) generalizes to the aim frame and de-risks §6 AT-14 at
  unit level: assert identity on a planar great-circle up-lap (constant transport axis → exactly 2π ≡
  identity) AND area/R² = 2π(1−cosθ₀) on a banked small circle (varying axis → tens of degrees). One
  without the other blesses a broken transport. (S5)
- HUD readouts are built against the shared `control::extract` + a SINGLE-SOURCE `sim::load_factor`
  (moved out of `controller.cpp` into `aero.h`) — a re-derived AoA/bank/G in `render/` is the same
  "flat instrument certifies flat controller" fork as in the harness, pinned by a 60°-banked case where
  a fixed-world-up bank reads wrong AND a velocity-along-nose-while-climbing case where an attitude
  proxy would report 30° AoA instead of 0. The HUD is a passive read of `SimState.last_vhat` (an
  instrument on the plant), NOT a second control layer — the §9.6 no-share seam doesn't bind it. (S5)
- The live app loop mirrors the tested `ClosedLoop::tick` tick-for-tick (transport → GROUNDED pairing
  fl.reset+aim.reseed+control::reset → Freelook → control::step → sim::step), reusing the SHARED
  extract/transport/Freelook so the shipped path IS the pinned path. Mouse delta is per-FRAME (accrued
  in `pending_*`, consumed once, DROPPED on grounded/CQ2-suspend — never queued into a smoothed basis)
  while transport is per-TICK; the freelook ease-back decays camera POSITION (orbit angles) only, and
  the CQ2 `Freelook::mouse_aim_live` gate is the SEPARATE guard keeping the eased camera basis out of
  the aim. Two cosmetic smoothings, both off the mouse→aim→ω_des loop. (S5)
- Focus-loss uses `snap_forward_to_nose` (keeps carried-up holonomy), NOT `reseed` (which re-seeds up
  from local_up) — §9.5 mandates only "aim:=nose", and a held override at focus loss is just an
  ordinary release inside `control::step` (mask off → ramp→0, φ_held recaptured), so no stuck
  deflection. A net-zero override axis (both keys) leaves the mask OFF (`s!=0`), so the caller can never
  trip `control`'s `assert(|override_sign|==1)`. (S5)
- A hysteresis anti-chatter test must make the signal DWELL at the boundary with ripple: a closed-loop
  chase nulls the pointing error to 0 and only sweeps THROUGH the threshold monotonically, so a
  single-threshold (non-hysteretic) latch produces the identical switch count — the test pins nothing.
  Drive the latch OPEN-LOOP on a fixed state (err ≡ the commanded offset) oscillating across the
  threshold; then hysteretic = 1 switch, single-threshold = 2/ripple-period. Mutation-verified
  (blend_lo→blend_hi → 30 switches vs 1). (S6 red-team)
- The G floor/ceiling clamps are UNREACHABLE by normal flight (the push-gate keeps demand inside the
  ±budget, and overdamped closed-loop G barely reaches n_max) — they bind on the AoA-PROTECTION-pushback
  path, exactly where SPEC §9.3b says "applied LAST." Test structurally: inject an over-stall FILTERED
  AoA so the pushback saturates, then the G-limit (not the pushback gain) sets the emitted pitch rate.
  A closed-loop "never exceeded n_max" check passes trivially and catches no mutation. (S6)
- Closed-loop holonomy is CONFOUNDED on the real plant: transport is BY the local_up rotation, so on an
  OPEN leg any local_up-referenced metric is null by construction, an open arc's ~V·t/R pointing swing
  dominates the enclosed-area holonomy, and a full lap drifts (untuned gains, aim residual ~55°) far more
  than the holonomy. The exact null+area PAIR belongs at unit level (test_aim_frame, RA8); the closed-loop
  acceptance verifies INTEGRATION (the quaternion frame reproduces the vector `transport_aim` path to fp,
  stays live) + circumnav sanity (lap closes to ~0.3% of circumference), NOT the law. (S6)
- On a GREAT-circle path a frame-carried camera-up and a local_up rebuild COINCIDE (null holonomy) — no
  continuity/direction test can distinguish them there. The frame-carried camera's benefit is SINGULARITY
  AVOIDANCE at the zenith (the local_up basis degenerates, |local_up ⟂ forward|→0, while the carried
  frame stays well-formed); pin THAT, not continuity. (S6 red-team)
- Asserting orthonormality/unit-length of an AimFrame pins a TYPE invariant (its quaternion is normalized
  each step — the property holds for ANY input, correct transport or not), so it catches no bug: 3600
  unit-quat products drift ~6e-15, below any 1e-9 bound even with the normalize deleted. Don't assert
  what the type guarantees; cross-check against an INDEPENDENT path (the vector `transport_aim`) for real
  correctness. Same class as "a golden only pins the code that recorded it." (S6 red-team)
- A camera-up test that separates aim.up() from local_up by PITCHING (or any move that keeps up coplanar
  with {forward, local_up}) cannot catch a "feed local_up instead of aim.up()" mutation: aim_chase_camera
  re-orthogonalizes whatever up it gets against forward, and the projection of local_up onto ⟂forward
  REPRODUCES the coplanar carried up. Only ROLL (forward level ⟂ local_up, up rolled off about the frame's
  own forward) makes the two diverge — a full 1−cosφ apart. The deeper form of the RA8/S6 null-holonomy
  trap; the mutation-verify (not the honest pass) is what exposed it — a pitch setup passed under the
  mutant. (P1b, camera leg)
- A crash/threshold-predicate test shaped "run N ticks, expect it fired" passes under a boundary mutation
  (`<=0`→`<=-50`) because a sustained dive crosses −50 too — it fires, just late. Pin the PREDICATE
  BOUNDARY: record the pre-tick altitude and assert it was >0 on the tick that respawned (first crossing).
  Same S4a "pin the predicate itself" lesson, for an app-loop respawn. (P1b)
- The app tick's crash-reset/transport/GROUNDED pairing are MODE-COMMON (outside main.cpp's if(raw_mode)),
  not "the instructor branch" — extract the whole tick with a raw flag (one `app::tick` for both modes) or
  the crash predicate forks into an untested raw copy. A mirror-equivalence test (app::tick vs the harness
  ClosedLoop from one start, bit-identical over 5 s) converts the "mirrors ClosedLoop EXACTLY" comment
  into an executable pin — there is NO ctest that runs the app binary, so the extraction's faithfulness is
  otherwise unverified. (P1b, Fable consult F1/F7)
- An equivalence-bound test is only as good as its bound: the mirror leg's `pos_err < 1e-3` was a
  hide-band — the two paths are EXACTLY bit-identical (`pos_err == 0.0` at max_digits10), so a real
  transcription slip that drifts sub-mm sails through. Print the residual at max_digits10 (S1: a `%.6e`
  instrument read the 1e-3 pass as bit-identical) and pin JUST above the observed value, not at the
  eyeballed "close enough": a float-`dt` slip lands ~4e-5 m — invisible under 1e-3, caught under 1e-6.
  (Coverage round 3, AT-9 Fable consult P1-3)
- An extraction that MOVES a consumer onto the shipped path needs a leg PER moved consumer, and "the N
  named mutations pass" is not that: after P1b, THREE separate consults each found more uncovered moved
  consumers (round 2: CQ2 gate + freelook snap; round 3: the override-mask forward + the GROUNDED
  fl.reset). Grep every field the new function forwards and every gate it now owns; a green suite proves
  none of them. The override miss is the sharp one — `any_ovr` reads `in.override_mask` so the snap/
  freelook legs stayed green while the passthrough into `control::step` was dead. (Coverage round 3)
- Extracting a NEW seam LAYER (main.cpp -> `app::step_frame` -> `app::tick`) re-opens the whole
  forwarding surface a level up — the round-2/3 "moved consumer" trap a FOURTH time. A state-mirror leg
  (bit-equal LoopState vs a hand-composed reference) catches dropped DYNAMICS fields (throttle, freelook,
  override, raw), but is BLIND to a dropped REPORT field: `FrameResult.consumed_dx/dy` feeds only the
  cosmetic orbit, so zeroing it left the shipped orbit dead with the state mirror green — a report needs
  its own explicit `== offered` assertion. Mutation-verified (M1 report:=0 → state-eq still 1, consumed
  checks fail; M3 freelook:=false → state-eq 0). (AT-9 impl consult P1-1)
- A DIFFERENTIAL neutralization test (held-override vs hands-off, both respawn to the fixed spawn_state)
  is blind to every neutralization COMMON to both arms: dropping the throttle reset flew BOTH residual
  paths at the stale 0.3 (not cruise) and they stayed equal → green. Pair it with an ABSOLUTE reference
  (spawn_state stepped hands-off through the residual ticks) — that pins the common-mode
  throttle/freelook/raw neutralization AND the residual-tick count / crash tick for free. Same class as
  "a golden only pins the code that recorded it," applied to a differential. (AT-9 impl consult P1-2)
- An energy/closure check need not eat the integrator's O(dt) error: reconcile the plant's per-tick
  LINEAR work `m·v·(v'−v)` (algebra on the plant's OWN `v'`, so the semi-implicit KE surplus `½m|v'−v|²`
  never enters) against config forces at the identical pre-tick state — plant==config ⇒ residual is
  machine-eps, NOT ~4% (the naive Δ(½mV²)-vs-continuous-work closure floored at 4% of throughput at 5 g
  and would have masked a subtle drag fork). Project onto v so lift drops out for FREE (lift·v ≡ 0):
  that is the correct ENERGY quantity — a lift defect is a trajectory bug, not an energy one. The gate
  is a step.cpp-FORK detector — keep the force composition DUPLICATED from the plant (share only the
  aero.h primitives, H1); factoring the assembled expression into a shared helper zeros the residual
  under every mutation. Mutation-verified (`k_induced→0` → rel 1e-13→0.2). (AT-12)
- "Tune-independent" must survive the SEAM, not just the algebra: the plant round-trips throttle through
  `float sim::Inputs.throttle`, so a config-side reconstruction on the raw double drifts ~1e-8 and
  false-trips a 1e-9 gate on the FIRST non-float-exact throttle retune — mirror every lossy cast the
  shipped path takes. Also: sum the residual as `Σ|per-tick|` (a signed sum cancels an oscillating
  defect over a near-closed path) and normalize by COMPONENT throughput `Σ(|grav·v|+|thrust·v|+|drag·v|)`
  (the NET power nearly cancels and shrinks under an energy-neutral retune, inflating the rel floor).
  All three caught by the Fable diff-consult, none by the honest pass. (AT-12 Fable consult)
- The ACHIEVED closed-loop G floor is NOT the commanded `w_min`, and the commanded floor is NOT bare
  `n_min`. Two effects, both easy to miss when wiring the true-n instrument into a VALUE assertion (§7
  will re-tune `n_min`/gains against this): (1) the curvature feedforward `ω_ff=(local_up×v)/R` is added
  AFTER the clamps and BYPASSES them (SPEC §9.3), so a perfectly-tracked floor achieves `n_min+V·ff_x/g`
  — precisely CQ1's flat-cpt-credit bias (~0.27 g at V=200), read back by the very true-n instrument CQ1
  built. Assert the DERIVED setpoint `n_min+V·ff_x/g`, never bare `n_min`. (2) The floor is UNREACHABLE
  in sustained UPRIGHT flight: `plant_invert` inverts only authority (not the plant's `−damp·q·ω`), so
  holding `w_min` needs the integrator to source `damp·q_eff·w_min`; upright that exceeds `K_wi·integ_cap`
  and `integ` caps ~30% short FOREVER (|n| plateaus ~2.4, extends S6 "overdamped barely reaches n_max"
  to the floor). Reach it by INVERTING (cpt=−1 halves the budget to `|n_min+1|` under the cap, cpt flat
  at the pole, V quasi-stationary) + preloading `integ` to steady state. The value pin catches a clamp-
  formula sign/credit mutant (`n_min+cpt`, `drop cpt`) that the one-tick precedence test — which
  recomputes `w_min` with the SAME expression — is blind to; the one-tick test in turn owns the
  `cpt→|cpt|` class (invisible to the closed-loop leg at cpt<0). Mutation-verified. (P2a / AT-11 CL)
- Replicating a SEQUENTIAL multi-step clamp (AoA pushback THEN G floor/ceiling LAST, §9.3b) as a reusable
  per-direction bound is NOT one-side-min/max: `pitch_ceil=min(w_max,A_c)` alone omits the `w_min` floor,
  so at deep stall (`A_c<0<`… actually `A_c<w_min`) a held pitch-UP override commands a pushback PAST the
  −3 G floor — the exact envelope departure S7-ovr exists to kill. The faithful bound is
  `clamp(A_c,w_min,w_max)` for the ceiling AND `clamp(A_f,w_min,w_max)` for the floor (reproduces the
  4-step sequence's saturated output because `A_c≥A_f` always; `w_min≤w_max` since `n_max>n_min`, cpt
  cancels). A SIGN-only test (`omega_des.x<0`) is blind to it — pin the MAGNITUDE against the mouse path
  (`==mouse omega_des.x`, both carry the same ff so equality is exact). Symmetric ±ceilings (yaw_max,
  p_max, omega_bal) need no interval clamp — the defect is specific to the asymmetric `[n_min,n_max]` G
  interval. Same class as "a golden only pins the code that recorded it," for a reused clamp. (S7-ovr red-team P0)
- A rotate-toward helper (`normalize(cross(a,b))` axis) NaNs at the ANTIPARALLEL end (angle→π), not just
  the zero end: `cross` of opposite unit vectors is ~0, `normalize(0)`=NaN. Guarding only `angle > eps`
  (the already-arrived end) misses it. When the result is CARRIED across frames (a lagged camera-forward),
  ONE NaN poisons it forever until a reseed. Guard BOTH ends (`eps < angle < π-eps`); at π the geodesic is
  directionless so freezing that frame is correct. Reachable wherever the target can flip ~180° (a
  tailslide `vel_dir` reversal at the 1 m/s floor). A "no-NaN" test that only exercises the PARALLEL case
  (v==aim) is a FALSE banner — test the antiparallel case (v==-aim) explicitly; the parallel case is often
  "accidentally contained" (NaN target → the next `angle>eps` compares false → returns unchanged), which
  hides the real hole. (S7-cam red-team P1)
- A degenerate-basis fallback must span with MUTUALLY-PERPENDICULAR candidates from a stable frame
  (body_up→nose, as `chase_camera` does), NEVER a fixed world axis (not guaranteed ⟂ fwd) nor `local_up`
  (re-imports the zenith pole the carried frame exists to remove). Decoupling a forward that was
  previously ALWAYS ⟂ its up (camera-forward was aim.forward() ⟂ aim.up()) makes a long-dead fallback
  branch LIVE — audit every guard the decoupling reactivates. (S7-cam red-team P3)
- Decoupling a value a test fixture happened to make a no-op leaves the NEW behavior untested while the
  OLD assertion still passes: the camera-up test passed `cam_forward == aim.forward()`, so the S7-cam
  Gram-Schmidt-against-a-decoupled-forward (the whole point) never ran. Pin the decoupled case explicitly
  (`cam_forward ≠ aim.forward()`, rolled carried-up). Same class as "a golden only pins the code that
  recorded it." Meanwhile RA9's rubber-band guarantee held STRUCTURALLY — the lagged cam_fwd is only ever
  read into the camera pose, never back into mouse→aim (traced every read); a downstream-only consumer
  cannot form the loop. (S7-cam red-team P2 / RA9 confirm)
- A hysteretic gate has TWO knobs and a test's mutation must target the RIGHT one: the S7-push
  roll-through leg (135° below rolls) is gated on ENTRY by `down_enter` (`z ≤ z_enter`), so its
  "mutation: down_exit→179 forces a bunt" was FALSE — down_exit only governs EXIT of an already-engaged
  push, and at 135° push never engages, so down_exit is inert. The real forcing mutation is
  `down_enter→179`. Coverage still existed (Leg 3's open-loop exit guards down_exit), but a false
  "mutation-verified" banner actively misleads the retuner. When you re-key a gate, re-derive which knob
  each leg actually pins, don't transliterate the old mutation. (S7-push red-team P1)
- ⚠ SUPERSEDED at damp_ff_pitch=1 (S-dampff, SPEC §0): Deepening `n_min` (the −G / nose-down floor) WAS
  bounded ABOVE by `integ_cap` — the sustained floor needed the integrator to source `damp·q_eff·w_min`,
  preload ~ `(n_min+1)·V` vs the cap. The damping feedforward sources that torque directly (AT-11's
  preload scales by `(1−damp_ff_pitch)`), so on a ff'd pitch axis the n_min ladder re-derives against
  LIFT (`Cl ≤ Cl_max`) and Chad's spine only. Historic numbers kept for the damp_ff=0 A/B arm: at
  integ_cap=0.5 the ceiling was ~n_min=−4; "raising the nose-down rate a lot is really an integ_cap
  change" was true THEN. The pull rate (`n_max`) never had the coupling. (S7 pitch-rate tune → S-dampff)
- A camera-up degeneracy pole is NOT at 90° when the chase EYE is lifted: the resting view already tilts
  `atan(height/distance)` down, so the freelook orbit reaches the straight-down pole (view ∥ carried up)
  at `90−atan(h/d)` (~75° at h/d=9/34), and it is ASYMMETRIC — the eye-below pole at `90+atan(h/d)` is
  unreachable. A SYMMETRIC pitch clamp picked against a "90° pole" flips the view (the pilot's own report:
  "flips looking down, not up"); derive the OVERHEAD cap from the SAME chase geometry the framing tracks
  (a `distance` retune moves the pole) and cap only that side. Also: a "widen the clamp" change needs a
  sweep test through the REAL pose (`aim_chase_camera` over the orbit range), not just a loader value pin —
  the orbit path had zero coverage and the flip lived there. (§7 freelook widen, §6 red-team P1/P2)
- A vertical lens shift (reticle-at-center WITHOUT rotating the camera) must apply the IDENTICAL NDC offset
  to the 3D scene (off-center `MatrixFrustum`, `t=th(1+s)/b=th(s−1)` ⇒ `(t+b)/(t−b)=s`, scale preserved)
  AND the 2D reticle overlay (`ndc_y−s`), same sign — else reticle and world slide opposite ways. The 3D
  path is raylib caller-glue no ctest runs, so the sign agreement is by DERIVATION (verified in review);
  pin the pure pieces (`lens_shift_ndc`, `off_center_frustum`) and cross-check the centering value against
  `project_dir`'s independent dot-ratio path so the mutation actually fails. (§7 chase-cam lens shift)
- An ADDITIVE feature gated on a defaulted `nullptr` pointer is only HALF-pinned by the existing suite:
  the goldens/mirror run the `nullptr` path (proving absent ⇒ bit-identical), but a mutation INSIDE the
  gated block that touches the shared state passes the WHOLE green gate. Add a DIFFERENTIAL leg — same
  seed, one run with the feature (`&dw`) and one without (`nullptr`), REQUIRE the shared state
  bit-identical over N ticks — or the firewall is proven only by construction. Same class as "route the
  live path THROUGH the tested function," applied to a firewall. (S8-drone red-team P1)
- Reusing the mouse-aim INSTRUCTOR to fly a scripted mover (a target drone) makes its turn BISTABLE: a
  horizontal lateral aim saturates `bank_error` at ~90° (SPEC §9.3 "banks ~90° for a purely-lateral
  target"), so a constant azimuth lead gives either ~straight (below ~0.3 rad) or a committed ~90°-bank
  hard turn (above) — NO clean lazy-shallow turn. "More linear" came from a PROGRAM (straight legs +
  periodic alternating turns, a pure function of `age_ticks` — the §5 seam, no clock), not from a gentler
  constant lead. A true lazy turn needs a dedicated bank-hold AI. (S8-drone flight program)
- A read-only tracking instrument computed once per SIM TICK (in `app::tick`) and READ by the HUD from a
  snapshot is frame-rate independent (AT-9) AND single-source (the pipper + the % never disagree with a
  render-time recompute). Its denominator must MEAN something: gate "engaged" on a real range, not the
  whole forward hemisphere (a 14 km bandit "ahead" made TOT% ≈ 0 forever). (S8-drone gunsight meter)
- A "flat instrument certifies a flat controller" trap in the TEST FIXTURE: a fixed-axis mutation
  (`local_up → (0,1,0)`) is INVISIBLE if the test point sits on that world axis (+Y), where the hardcode
  coincides with the true up. Place frame-correctness tests at a GENERIC point (`normalize(1,1,1)·r`) so
  no world axis aligns with local_up and ANY hardcode separates. (S8-drone red-team P2)
- A gentle BANKED turn is not expressible through the mouse-instructor: its bank-to-turn banks ~90° for any
  lateral aim (bank_error saturates), and a small aim error gets nulled by a little turning so a shallow
  bank never SUSTAINS (bistable). A dedicated bank-HOLD PD autopilot (hold φ→target, hold γ→0 for level,
  null β for coordination) sustains any bank by construction — the plant's own `damp_*` makes modest P-gains
  overdamped (no ZOH ring, verified at the 45° ceiling AND low q, not just one point — the S6 trap). Level
  on the sphere via γ-FEEDBACK (no curvature feedforward) drifts slowly (a bounded climb to the drag-limited
  speed at full throttle) — bound it with a minutes-long altitude-band test, don't assume ≤10 s. (S8-drone
  gentle-turn autopilot)
- A control law with NO aero restoring moment (the plant has no weathervane yaw stability, step.cpp) makes a
  wrong-sign coordination term PURE positive feedback — β walks to ±90° with only rate-damping to bound it.
  Pin EVERY axis of a hand-rolled PD, not just the one the happy-path test happens to read: the bank-hold
  test asserted φ and γ but never β, so a flipped yaw sign (the runaway class) survived the whole green gate
  until the fresh-context red-team named it. Assert |β| small AND seed a sideslip and watch it null.
  (S8-drone red-team P1)
- A ballistic/intercept solve must find the EARLIEST positive root: a fixed-point (`t←|RHS(t)|/muzzle`) or
  Newton seeded near a LATER root converges to the wrong intercept (a fast head-on target has two — the
  bullet-forward hit and the bullet-chases-after-it hit; only the first is a real shot). Bracket the first
  sign change of `f(t)=|RHS(t)|−muzzle·t` (f(0)=range>0) then bisect — robust, no multi-root ambiguity, and
  a receding-faster target simply never crosses (unsolved). Watch the scan cell size vs the sub-second
  gun-range TTI. Gravity drop = `−0.5·grav·t²` in RHS (raises the aim); velocity inheritance =
  `w=target.vel−shooter.vel`. (S8-drone ballistics)
- An envelope/safety guard must cover ALL the axes the envelope depends on, not just the one a red-team
  named: clamping the drone's `turn_bank` at 45° "for the docile envelope" left `speed` and `throttle`
  (co-equal — a sub-stall speed or thrust-infeasible throttle also departs) open, so the fix was calibrated
  to today's table exactly like the AT-15 trap. Floor them cross-file against the airframe (the
  `load_controller` precedent), not with a bare number. Same for a numeric-method's validity precondition
  (the ballistic `f(t)` monotone within its time cap): if it holds only because `muzzle_speed >> plane
  speeds` today, add a loader tripwire (`muzzle ≥ 2·v_redline + g·kTMax`) or a retune silently disarms it.
  (S8-drone Fable audit P1)
- The "every gate hysteretic" rule applies to INSTRUMENT selection gates too, not just control regimes: the
  gunsight's engaged-target pick (nearest-of-N within range/cone) strobed the pipper between crisscrossing
  bandits at tick rate — the exact dwell-at-a-boundary chatter the rule forbids. Fix with a STICKY latch
  (keep the current target until a rival is materially closer, e.g. <0.85·range) + hysteretic range/cone
  bands, and EXTRACT the selector so the anti-chatter is pinnable OPEN-LOOP on an oscillating fixed rival
  (the S6 "a closed chase nulls to monotone, pinning nothing" discipline). (S8-drone Fable audit P1-2)
- A gate factor that is EXACTLY 1 on the golden's path hides a wrapped-term mutant from the ENTIRE
  suite, golden included: MB-rud's `(1-blend)+blend*yaw_gate` ≡ 1 in aligned flight, so a mutant
  wrapping COORDINATION inside it (breaking "coordination always outside the pointing gate") passed
  all 218 tests. Pin the invariant with a DELTA test at a non-1 gate value (seeded sideslip at the
  gate-floor aim: yaw delta == full K_coord·(−β)). Same class for BAND PLACEMENT: two samples that
  both sit OUTSIDE every candidate band placement (45°/105° vs a 0.15-wide band at 90°) pass a
  flipped/symmetric-band mutant bit-identically, and the only failing artifact is the golden — which
  the next retune re-records ("re-record blesses the bug"). Sample INSIDE the band and AT the
  boundary-exact edge, against a config-recomputed formula oracle. (MB-rud diff red-team P1-1/P1-2)
- COMMIT the honest green state BEFORE running mutation-verify reverts: a `git checkout <file>`
  meant to undo a mutant restores HEAD — which, mid-feature, is the PRE-feature file, silently
  destroying every uncommitted edit in it (happened to MB-rud's controller.cpp; re-applied by hand).
  Mutation testing's revert step assumes the baseline is committed. (MB-rud session)
- When replacing a FADE, walk the first ~500 ms of the primary use case tick-by-tick and name what
  the OLD fade was flown in to provide before deleting it: the plan's cos^power yaw gate would have
  deleted the mid-roll rudder crab Chad flew in with S7-yaw (roll-in has bank_eff≈90°, cos≈0) — the
  felt regression was caught at PLAN stage only because the red-team tick-walked the flick; the fix
  (smoothstep(−band,0,cos): full rudder to knife-edge, floor past it) keeps the crab AND kills the
  corkscrew. A fade's SHAPE encodes a flown-in ruling, not just a number. (MB-rud plan red-team P1-1)
- A latch's DONE/exit leg must live INSIDE (or after) the block whose output it gates: MB-right's
  disarm block ran BEFORE the righting roll-override, so on the DONE tick the latch cleared and the
  branch-computed roll — built against the already-decayed setpoint — shipped for ONE tick
  (p_max-clamped 4.5 rad/s at knife-edge; the mechanism's own clamp violated on its own exit).
  Moving the DONE leg into the apply site emits the recaptured (zero) demand the same tick. Pin exit
  transients with a WINDOW (exit tick + a few), not the exit tick alone — a recapture-delete mutant
  emits 0 on the exit tick and spikes one tick later. And a boundary no-arm pin must be OPEN-LOOP on
  a fixed state (S6): closed-loop, the partial wings-leveling escaped the band before the arm-
  threshold mutant could fire — the reviewer-proposed closed-loop leg passed under its own mutant.
  (MB-right diff red-team P1-1/P2-1/P3-1)
- The "fixture makes the new behavior a no-op" class (S7-cam P2) RECURRED twice in one mechanism:
  MB-atm's "lift thins with altitude" leg flew at alpha = 0 (lift ≡ 0 — the check was 0 == f·0, and
  Chad's literal ask "loss of lift by altitude" was pinned by NOTHING; the no-thinning mutant passed
  all 223 tests), and no leg at all covered the DAMPING torque (AT-18a deliberately measures from
  omega = 0, so it is damping-blind BY DESIGN — a sea-level-damped fork at altitude was invisible).
  For every term a mechanism scales, ask: which existing instrument is structurally blind to this
  term (alpha=0 kills lift, omega=0 kills damping, lift·v≡0 kills AT-12's view of lift), and give
  each blind term its own non-vacuous leg (REQUIRE the baseline term > eps before checking the
  ratio). (MB-atm diff red-team P1/P2-1)
- "Summing 2^m equal fp values is exact" is TRUE only pairwise — sequential `+=` of equal
  full-mantissa values is accident-exact to 4 terms and BREAKS at 8+, so a totals-across-frame-
  schedules bit-identity leg false-fails an honest implementation and grows the tolerance
  hide-band it exists to forbid. The rigorous frame-rate-invariance pin is per-call POWER-OF-TWO
  SCALE invariance (`f(4d, 4dt) == 4·f(d, dt)` — rounding commutes with 2^k scaling), probed
  in-band + at the cap + sub-knee. Companion trap: a rate keyed on delta/frame_dt must use the
  RAW frame_dt (a stall-clamped dt under-reports wall time: a hitch reads as a flick) and needs a
  quantization guard (integer 1-2 px deltas at high fps carry fps-SCALED rate noise a real-valued
  test literally cannot represent — give the guard an integer-quantizer end-to-end leg). (MB-aim)
- An in-band shape probe must sit where the candidate mutants SEPARATE: the band MIDPOINT t=0.5
  is the FIXED POINT of the `smoothstep <-> 1-smoothstep` flip (bit-equal there), so a midpoint
  oracle passed the flipped washout through the whole suite — probe at t=0.25 + a boundary-edge
  continuity pin. The MB-rud band-placement lesson's sharpest form: symmetric sample points are
  blind to symmetric mutants. (MB-flaps diff red-team P1-1)
- A NEW plant force term must be folded into every instrument that claims to read the plant's
  truth, or the instrument becomes the H1 fork: `load_factor` kept reading the bare `lift_coeff`
  while the plant flew Cl + flap shift — the "true n" HUD/CQ1 telemetry under-read ~2 g exactly
  in the slow flapped turn fight. Single-source the composition (`total_lift_coeff`) into plant
  AND instrument; keep only the TEST-side energy mirror duplicated (the fork detector). Also:
  a crash/GROUNDED neutralization of a new field is dead code until a crash leg COMMANDS that
  field into the crash (the "leg per neutralized field" trap, 6th instance). (MB-flaps)
- A "de-roll by the extracted bank" is itself bank-gauge-contaminated: `e.phi` (and its unfold)
  FOLDS PITCH into the roll reading (phi_full = asin(sinφ·cosθ) at Euler roll+pitch), so a
  cos/sin(phi_full) de-roll axis is exact ONLY at zero pitch — a pitched+banked vertical aim
  leaked ~7° of phantom lean through it, and the zero-pitch fixture was structurally blind
  (`flight_state` HAS no pitch parameter — check what the fixture builder can't represent
  before trusting a leg). The frame-true lateral axis needs no trig at all:
  `(cosΦθ, sin e.phi) == cross(nose_b, local_up_b)` from the two already-extracted fields —
  exact at every attitude, self-fading at nose-vertical (graceful decay instead of a
  noise-gauged direction). Same class as the φ-fold wings-hold trap (S4a), one level up:
  the FIX for bank contamination can re-import it through the instrument used to remove it.
  Bonus, same session: `a -= k·a` rewritten `a += k·(target − a)` is BIT-identical at
  target = ±0 (IEEE sign-symmetric rounding; the product tree is load-bearing) — the clean
  knob-off-arm proof shape for retargeting any exponential decay. (MB-lean diff red-team P1-1)
- Input-quantization jitter (integer mouse deltas staircasing a slow sweep) has a §9.1-LEGAL
  smoothing shape: ease a DISPLAY COPY whose only consumer is the draw (rename the field so
  no future consumer mistakes it for truth), with a HARD LAG CAP derived from the
  quantization scale itself (quant_px·aim_sensitivity — live-derived, never a welded
  constant) so the ease has zero authority over macro motion and every snap self-heals
  through the cap in one frame (no reseed plumbing). The raw aim stays raw; the fix lives
  entirely in render. Test the staircase as a UNIFORMITY pin (max AND min per-frame output
  step) — the input's mean rate must be preserved, so "output < half the input hop" is the
  wrong bound at drag rates near cap/τ. (S-reticle)
- A GLSL shading term can be a FIXTURE-NO-OP in its primary regime — the fixture-no-op trap in
  shader form. Stage-2 sky: `lum = base + haze·(uSkyDay − base)` with `base = mix(dusk, day,
  smoothstep(0, duskHi, eSun))`. In FULL DAYLIGHT (`eSun ≥ duskHi`) base saturates to `uSkyDay`,
  so the whole horizon-haze term multiplies by exactly ZERO — the day sky is a flat constant +
  dither, and `haze_density_light` is inert for the SKY dome (still live for the ground
  `sky_aerial` extinction, which is why grep didn't flag it dead). Caught only by a fresh-context
  cross-model red-team working the algebra, NOT by build/goldens (no ctest runs `seads.exe`) — the
  same blind spot as the mirror-VS P0. Fold: a shader term gated by `(A − f(uniforms))` self-nulls
  wherever `f` saturates to `A`; when you write a "gradient/variation" term, name the regime where
  it's supposed to fire and check ∂out/∂(view) ≠ 0 THERE (a source-validator can pin it), and make
  the doc claim the regime the term actually fires in — don't let a comment overclaim a gradient the
  monochrome stage doesn't deliver (that variation arrived a stage later, from scatter). (Stage-2 red-team F1)
- A config knob that is loaded + range-checked but consumed by NO shader/code is a silent-disarm
  smell EVEN when it's an intentional park: Stage-2 `horizon_definition` is the bound for a DEFERRED
  gate (the horizon-continuity column-check), so it reads as dead to a red-team that doesn't know the
  deferral. Fold: when you defer the CONSUMER of a config number, leave the number with a comment
  naming the deferred gate it guards (the plan doc is the source of truth), so a later
  audit/red-team doesn't mistake a park for the AT-15 disarm — and so the number gets retuned WITH
  the gate when it lands, not calibrated to a stale guess. (Stage-2 red-team F2)
- A default `--smoke` frame may not FRAME a new visual element, so "smoke passed" proves shader
  LINKAGE but not that the element DRAWS. Stage-3a's sun disc sat off-screen at the default epoch —
  the first smoke looked clean yet showed no disc. Fold: to visually certify a new render element the
  green gate is structurally blind to, force it into view with a THROWAWAY swap (Stage-3a pointed the
  eye→sun dir at `-cam_forward`, offset off the reticle so it wasn't masked), shoot, LOOK, then revert
  and rebuild before committing. Seeing "the shader compiled" is not seeing "the disc is there, right
  size, right place." (Stage-3a — extends the green-gate-blind-to-binary lesson.)
- A pure `t_cel` FUNCTION module (weather, seasons) has THREE homes for its vetted constants — the
  header defaults, the `world.toml` block, and the `test_load_world` literal pins — but its PROPERTY
  tests (distribution, spawn, slew) run on the HEADER DEFAULTS. So a legit toml retune (invited by the
  tuning comment) fails only the literal pins; the author updates those, and the property tests
  silently stop covering the SHIPPED values (a Chad-facing guarantee like "spawn is clear" can ship
  broken green). Fold: add a lock leg asserting `loaded_block == HeaderParams{}` — a retune now fails
  until BOTH the toml and the header defaults move together, which re-runs the property tests on the
  new design. Pair with config-RELATIVE property bounds (derive the slew/C1 cap from the params:
  `1.5·2π·Σ(wᵢ/Tᵢ)/(gate_hi−gate_lo)`, not a hard 0.020) so the same retune doesn't false-fail the AT-15
  way. (Stage-3 weather red-team P1-2)
- A distribution/shape test with WIDE bounds can pass a mechanism that dropped a load-bearing TERM.
  The weather variable's fast front (period3, Chad's ruled dynamic-arena tempo) could be deleted whole
  and still land inside every clear/haze/overcast band + pass spawn + pass C1 (slew only SHRINKS) —
  the periods "set tempo not mix," and nothing measured tempo. Fold: when a term exists for a FELT
  property no distribution pins (here: how fast a front rolls), add a tripwire for THAT property — a
  params-derived slew LOWER bound (`max_delta ≥ 0.55·slew_sup`) forces the fast term to contribute.
  A ratio-style bound (measured vs the analytic sup computed from the unchanged params) both kills the
  mutant AND survives a retune. (Stage-3 weather red-team P1-1 — the fixture-no-op class, tempo edition.)
- A Gemini asset-factory DRAFT is a draft twice over: review it against the SEAM *and* against the
  FULL design intent the recipe under-specified. The Stage-3 scatter GLSL came back mathematically
  faithful to the recipe but with TWO defects the recipe didn't pin: (1) a fixed-axis `vec3(0,1,0)`
  view term (the recipe said `dot(viewDir, up)` but `up` wasn't a function parameter, so the model
  substituted a world axis — a banned flat-earth basis on the sphere, caught by review + the seam
  grep); (2) a SYMMETRIC `view·up` Rayleigh phase that blue-washed the zenith/nadir, missing the
  plan's "horizon-weighted so it never becomes a permanent blue dome" (caught only by the AFTER
  red-team, because the recipe pinned the Mie gates but left "gentle, view-angle dependent" open).
  Fold: when packaging for Gemini, pass EVERY seam quantity the body needs as an explicit PARAMETER
  (never let it invent a basis), and copy the plan's shape rulings (horizon-weight, space-first)
  verbatim into the recipe — an under-specified recipe is filled by a plausible-but-wrong default.
  The reviewed body lives in the PURE core (`render/scatter_glsl.cpp`, raylib-free like `rig.cpp`'s
  mirror source) so the headless asset validator gates the generated string with no GL context; its
  guard is "declares ZERO uniforms" (it references the shared block), which catches a smuggled clock.
  (Stage-3 scatter — the FIRST real Gemini call.)

## Real-GIS Sudbury terrain (2026-07-09, autonomous SESSION #8)
- **Wrap a DISK, not a RECTANGLE.** Mapping a real map region onto a closed sphere via an equirect
  bounding box is an egg (two pole pinches + shear — the "most expensive line" the old hand-paint ruling
  feared). **Azimuthal-equidistant disk→sphere** sized to capture radius **r = π·R** makes true-1:1
  fill the WHOLE sphere with ONE antipodal pinch, zero back cap, zero inflation. The "scale vs fill"
  dilemma is false: at r=π·R they're the same thing. Chirality is preserved (non-mirrored) iff
  `refE = cross(refN, d_hero)` with up=d_hero (real E×N=U handedness) — verify with a landmark, not just
  a round-trip (a round-trip passes even when mirrored).
- **The equirect PNG is a TRANSPORT container, not the projection.** Bake the geographic projection
  OFFLINE by INVERSE-mapping each equirect texel (texel→dir→geo→sample the source raster once); the
  runtime stays pure 3D `fragDir`, no lat/lon. The offline python sampler MUST match the C++
  `HeightField::sample01`/`equirect_uv` bit-for-bit (−0.5 center, u-wrap, v-clamp, /255 bilinear) or the
  mesh reads garbage — single-source the convention, then a red-team can verify the replica line-by-line.
- **`fwidth()` in non-uniform control flow is undefined (GLSL spec).** An anti-alias/LOD fade computed
  inside `if (isWater)` is driver-dependent. Hoist the derivative (and the value it differentiates) into
  uniform control flow — compute for every fragment, branch only on the cheap use.
- **A concave polygon's centroid can fall OUTSIDE it (on land).** For a per-lake anchor/landing-height,
  use `representative_point()` (guaranteed inside), never `.centroid` — else a float-plane target gets a
  land elevation. And **flatten lakes AFTER the blur, and flatten EVERY lake pixel** (not just the deep
  EDT-interior, or narrow lake arms stay curved and the mirror/landing height is wrong there).
- **Theatrical-elevation single-source.** Relief is exaggerated (~2.4× here). Landing/water elevations
  must be the POST-remap value sampled from the FINAL committed DEM the runtime way — never the raw GIS
  metres — or planes land ~100 m off the visible mirror.
- **`.gitignore` has NO inline comments.** `source/  # cached` is a literal pattern that matches nothing;
  a 35 MB GeoTIFF nearly got committed. Put comments on their own lines. And the global `*.png` ignore
  needs an explicit `!assets/sudbury_*.png` negation (mirrors `!assets/earth_dem_2048.png`).
- **Space-first sky vs mirror-silver lakes is a real tension (Fable water-trap #1).** A near-black zenith
  makes a physically-correct mirror read DARK except at grazing angles / sun-glint. "Silver lakes" then
  needs an explicit water-only reflected-sky floor or a brighter tint — a deliberate art choice, not a
  bug. Flag it as a decision, don't silently brighten.
- **A symptom-cover can mask the real cause — verify the DATA before building a fix (SEADS water, 2026-07-10).**
  Big lakes rendered with straight-line "cover strips"/cuts. First diagnosis: the coarse ~120 m terrain
  MESH facets the shore cliff. I built a whole dedicated flat-water-surface pass (mesh per lake, shader,
  gate) — it only *partly* helped. Chad's second-fly clue was decisive: "the far side of Whitewater is
  straight all the length" — on a lake that ALREADY had a dedicated surface. That a *surfaced* lake was
  still straight proved the cause was upstream of the render. The real bug: OSM `natural=water` RELATIONS
  store the boundary as multiple `way` segments that must be STITCHED into one closed ring; the fetch
  closed each `way` alone, CHORDING a straight line across the gap (Whitewater = 6 open outer segments →
  a 3901 m straight edge). It corrupted the LANDMASK itself, so every relation-lake was wrong — and the
  dedicated surfaces, built from the same broken polygons, inherited the cut. **Lesson: when a fix only
  "helps a bit," suspect the diagnosis. Inspect the actual input data (I plotted the polygon: 655 verts
  but one 3901 m segment = obviously not natural) BEFORE building render-side machinery to hide it. The
  cheap 20-line data probe would have found the root cause before the multi-file mesh pass.** Proper OSM
  multipolygon assembly (stitch outer segments end-to-end by shared end-node; subtract inner rings as
  island holes) fixed all 1293+ lakes at the source. Corollary: force-closing a broken ring re-introduces
  the exact chord — DROP an un-closable ring, never chord it shut.
- **A handoff's claimed data defect can be STALE — verify against the real cached data before coding
  (SEADS lake dedup, 2026-07-10).** The handoff said "Kelly Lake resolves via largest-area name-dedup to
  a far ρ=60 km duplicate, so Chad's near Kelly gets no surface." A 40-line probe over the cached
  `water_raw.json` disproved it: near Kelly (12.9 km, 3.38 M m²) is *larger* than the far duplicate
  (0.63 M m²), so largest-area ALREADY picked it — `gen.h` already had near-Kelly with a surface. The
  handoff was written against an earlier bake. But the SAME probe found the real bug the handoff only
  hinted at ("could mis-resolve other duplicate names"): 8 named lakes (Hannah, Crooked, Ella, Pine,
  Bell, Beaver, McLaren, Spanish) resolved to a FOLDED far duplicate (ρ>R_MAX, renders as garbage) when
  a good in-disk instance existed. **Lesson: reproduce the claimed symptom from the actual data first —
  the fix's true scope comes from the data, not the prose.** Fix = dedup key `(rho_rep ≤ R_MAX, area)`
  descending: prefer an IN-DISK instance, then largest (NOT nearest-outright — that lets a nearer sliver
  of a same-named lake beat its main body). Bonus the probe didn't predict but the re-bake revealed: the
  `lakes_out[:250]` cap had been silently dropping near-Sudbury HERO lakes (Minnow/Robinson/Bethel) in
  favour of far-but-bigger lakes; in-disk-first ordering pulled them back into the label/target set. A
  cap that sorts by a raw-size metric silently drops the things you most care about when they aren't the
  biggest — order by relevance-to-play-area first, size second.

## rig-D — the real Bf 109 ingestion + rugged gear (2026-07-10, D.3/D.3b)
- **A locked/hung test process silently ships a STALE test binary AND a stale-plus-partial one is worse
  than fully stale.** The ctest run left a `ctest.exe` + `seads_tests.exe` HUNG (didn't exit); the next
  `cmake --build` could not relink `seads_tests.exe` ("Permission denied"), so ninja reported the link
  FAILED but **ctest ran the previously-linked binary anyway** — which had my `render_core` change
  (Zneg gear hinge) compiled in but NOT my `test_*.cpp` edits, a Frankstein mix that made the failure
  hard to read. Fix: `Stop-Process` the hung `ctest`/`seads_tests` BY ID (killing by name missed the
  hung pair once), then `Remove-Item` the exe and confirm it's gone BEFORE trusting a green ctest. The
  lessons-corpus "mutation-verify only on a binary you PROVED is fresh" extends to the gate itself.
- **A mirrored kinematic pair needs the mirror in the HINGE, not just the mesh (Fable P0-1).** Both main
  gear struts shared `Driven::Gear` + hinge +Z + the same `(ext−1)·gain` angle; about +Z that retracts
  the LEFT leg outboard (−X, correct) but sweeps the RIGHT leg ACROSS THE BELLY (also −X, wrong). Same
  class as the aileron antisymmetry — solved for `Roll/RollMirrored` but not re-checked for the gear
  when it was added. Fix = `hinge_axis = {0,0,−1}` on the right strut+door. The placeholder cubes being
  near-symmetric HID it on screen; only a real asset + a "right wheel never crosses the belly" test pin
  makes it visible. A new driven pair inherits the mirror bug of every pair before it — pin it firing.
- **When a rest pose gains a real value, absolute-angle golden assertions break — re-pin as a DELTA.**
  Baking splay/rake into gear `rest_rot` made `glm::angle(deployed.rot)==0` false (rest is no longer
  identity). The unfold test's INTENT (ext=1 rest, ext=0 swung by gain) survives by measuring the swing
  RELATIVE to rest: `glm::angle(inverse(rest_rot)·posed)`. Re-recording the absolute angle would have
  blessed whatever the new rest happened to be; the relative measure keeps the ext-vs-(ext−1) mutation
  guard intact.
- **Verify a render change on the PLANE, not the planet.** The chase cam sits behind, and the 109's
  mains sit forward under the nose — from dead astern the deployed gear is fully occluded and reads as
  "not deploying" even at `state.gear==1.000`. Confirming the value (a 1-line stderr print) separated
  "not deploying" from "not visible," then a plane-orbit debug cam (`SEADS_RIGCAM`, distinct from
  `SEADS_OBLIQUE` which frames terrain) showed the splayed stance. Build the inspection tool the asset
  actually needs; a terrain cam can't see an aircraft's underside.

## rig-D round 2 — inset control surfaces + welded wingtips (2026-07-10)
- **An "inset" surface is buried in its parent unless the parent is NOTCHED first.** A flat control
  surface butted onto a wing TE just interpenetrates the wing's own aft chord — it can't read "let in."
  The fix is to CUT a bay: truncate the parent airfoil ribs at a wall chord-fraction (0.66) over the
  surface span, and fill the aft with the surface. Cheap watertight trick: keep constant rib topology
  and CLAMP aft vertices to the wall fraction (`min(f, f_end)`); the collapsed verts merge under
  remove_doubles and the loft from a truncated rib to a full neighbour rib IS the bay side wall. Place
  DOUBLED ribs (one full, one truncated, same span station) at each bay edge for a clean vertical cut.
- **A round LEADING EDGE concentric with the hinge gives a constant deflection gap — the set-back hinge.**
  Author the surface nose as a circular arc centred ON the hinge axis (at the local origin); because the
  arc maps to itself under the hinge rotation, the slot gap to the bay wall is invariant at EVERY
  deflection — no LE corner can poke through or open a wedge. Consequence for the validator: the nose now
  bulges ~r_nose AHEAD of the origin, so a "hinge origin at/ahead of the LE" test (`lo.z > −0.06`) must be
  relaxed to clear the nose radius while still catching a centroid origin (`> −0.12`; centroid ≈ −0.22).
- **Bake dihedral/sweep into rest_rot, never into the mesh — the same lesson as the gear splay.** A
  straight hinge on a swept/dihedral wing: pick the hinge line through the wing TE at the surface's two
  end stations, put its tilt in `rest_rot = Rz·Ry` (mirror the pair L/R), keep the mesh flat with a
  canonical +X hinge. Then `rest_rot·angleAxis(θ,+X)` still deflects cleanly. A mesh shear would move the
  nose-arc centres off the hinge axis and DESTROY the constant-gap property.
- **When the rest pose gains a tilt, absolute-symmetry test legs break — re-measure RELATIVE to rest.**
  The ailerons' new tilted rest_rot has a tiny yz cross-term that breaks EXACT absolute antisymmetry of
  the deflected TE (~0.003 > the 1e-4 margin). The swing about the hinge (`rest⁻¹·posed`) is exactly
  antisymmetric — measure that. (Third time this pattern has recurred: gear splay, elevator-at-rest,
  aileron antisymmetry. A tilted rest pose invalidates every "== identity" and "== ±other" absolute pin.)
- **Weld a separate-node cap by making its seam rib an EXACT copy of the parent's edge rib** — a
  zero-gap butt joint reads welded without a risky merge, and (unlike an overlap) leaves the AABB/
  chirality/one-mesh legs untouched. An overlap buys nothing and risks coplanar shimmer (Fable).
- **Blender's splash screen cannot be closed via the Python API and it overlays viewport screenshots.**
  Bypass it entirely: `bpy.ops.render.opengl(write_still=True, view_context=True)` renders the 3D view to
  a file with NO UI/splash (set `overlay.show_overlays=False`, `shading.type='SOLID'` first).
- **Committing on a shared branch where a parallel agent has UNCOMMITTED work in a file you must also
  edit: isolate your hunk into the index with `git apply --cached`, never `git add <file>`** (which
  sweeps their hunks into your commit). Extract just your hunk from `git diff <file>` (awk from your
  `@@` header to the next `@@`, prepend the 4 diff-header lines), `git apply --cached --check` then
  `--cached` it, and VERIFY with `git diff --cached <file>` before committing. The old-side line numbers
  are HEAD-absolute, so a single hunk applies cleanly even with other hunks present. (S3: the audio
  agent's 37-line main.cpp change stayed uncommitted while my 14-line ribbon call committed alone.)
- **A full re-bake for ONE feature silently DRIFTS unrelated approved assets** — S3 (roads→ribbons)
  only needed the albedo + gen.h, but re-running `build_sudbury.py` also rewrote dem/landmask/normal/
  treedensity with imperceptible run-to-run resampling noise (normal 59% of px changed, but at meanΔ
  0.63/255 = LSB). Committing that drift would re-open a fly-approval Chad JUST gave and muddy
  attribution. Fix: `git checkout HEAD -- <the unrelated PNGs>` so the feature commit changes ONLY what
  it intends; pixel-diff every regenerated binary vs HEAD before staging to catch the drift.
- **clang-format -i reformats the WHOLE file, not just your new lines** — on a repo whose committed
  files aren't already format-clean it reflows dozens of untouched lines, burying your ~40-line change
  in a 230-line diff (and widening the conflict surface with parallel agents). Format your NEW files;
  for small edits to existing files, hand-match the surrounding style and skip the global format (or
  `git checkout` + re-apply the functional hunk) so the diff stays focused.
- A hysteretic band has a PHYSICS FLOOR: before drafting a tighter deadzone/gate band, compute the
  mechanism's own closed-loop equilibrium (here the MB-lean magnet parks at ~0.076 deg, set by
  lean/coordination — a 0.03/0.06 circle sat entirely BELOW it: the at-rest park fragmented to 29%
  and the nose could never rest inside; the pins caught it pre-fly, do NOT re-pin). Threshold-shaving
  past the floor makes the circle theater; the honest fix is structural (S-dz-motion). (Fly 5)
- A TIME-gated latch needs a DURATION pin, not just edge pins: every edge mutant of the S-dz-motion
  dwell died, but `rest_dwell*0.1` survived the whole suite — the dwell's real job (bridging the
  accumulator's (N-1) at-rest ticks per frame at low fps) was unpinned until a config-derived
  relatch-count leg (ceil(dwell/dt) +/- 1) landed. Same class as the moved-consumer trap: the new
  forwarded field (ci.aim_moved) also shipped untested until a sub-threshold-delta app::tick leg
  pinned the bit itself (a full-size delta would unlatch via err and mask the deletion). (S-dz-motion)
- A PI inner loop whose inversion omits a KNOWN plant torque converts that torque into integrator
  PRELOAD, and preload is a carry-past bug factory: the integral wound to source `damp·Q·ω` during a
  sustained rotation keeps torquing at capture, carrying the nose ~1° past the aim and draining only at
  τ = K_w/K_wi (2.5 s) — V-scaled (preload ∝ q·ω), so it lives at combat speed and hides at V=140. The
  gain ladder CANNOT fix it (K_wi walled by rest-hunt/AT-2; raising K_w shrinks the plateau but
  LENGTHENS the drain); the fix is completing the inversion (feed the known torque forward AT THE
  DEMAND ω_des — never measured ω, which would cancel the plant's physical damping). Stability argument
  that made it safe: the eo coefficient is ff-independent, so the discrete pole and ZOH ceiling are
  bit-untouched — but the OUTER effective gain rises by 1/sag (K_w/(K_w+damp·Q) was 0.5 at V=250), so
  every outer-loop wall measured pre-ff is in SAGGED units (probe before flipping; per-axis knob so the
  yaw/roll flown-in ladders don't invalidate in one flip). And the 2nd instance of the pattern: Chad's
  "I need more pitch down" was compensation for the carry-past, exactly as "more yaw" was compensation
  for the missing side-force — attribute the FELT ask to a mechanism before reaching for its gain.
  (S-dampff)
- Reflecting a POINT feature (stars) in flat mirror water is a near-no-op unless you SCATTER the
  sample. A flat water surface (radial normal) reflects a tiny solid angle, so every fragment samples
  ~the same one reflected-direction cell — a naive `water_stars(reflect(view,up))` term lit only 41 px
  across a whole scene (invisible). Fix = jitter the star-sample dir by a STATIC per-location micro-
  ripple (hash of the surface position, no clock) so adjacent water fragments sample different cells and
  a full starfield reads (41→9268 px). The SKY reflection itself stays a calm mirror (only the star
  sampling is jittered). And verify it with an A/B PIXEL-DIFF (star_reflect on vs off, same camera), not
  a raw pixel count on one frame — the count is confounded by the tiny water area + dim partial points
  (the window-lights signature-metric lesson, again). (S3 rivers-mirror)
- Chaikin corner-cutting to round a jaggy polyline BLOWS UP the vert count if you smooth the FINE line:
  N passes on an 8 m-simplified river gave 480k verts (2.8x). Coarsen FIRST (DP at 30 m for waterways vs
  8 m for roads) THEN Chaikin THEN densify — a coarse line rounded into large meanders reads better than
  a fine one rounded anyway, and the vert count drops to ~157k. Order: simplify(coarse) → smooth →
  densify (density governed by densify spacing, not the smoothing). Smooth ONLY the features that meander
  (rivers) — roads legitimately have corners. (S3 rivers-meander)

- A CENTROID crop/survival check is a FIXTURE-NO-OP for a LARGE feature that straddles a radial band.
  Lake Wanapitei (aeqd rho 27→42.4 km) was cut across its width by the real→wilderness fade annulus
  [36.5,41] km: the "keep lakes real" exemption only protected lake texels where the fade weight w<0.5
  (rho<38.75 km), so the outer third fell to wilderness along the constant-rho arc = a straight cut. The
  in-disk assert only checked each hero lake's CENTROID (34.9 km < mid-fade 38.75), so it never flagged
  that the EDGE reached 42.4 km. TWO lessons: (a) gate a large feature by its EDGE (min/max over the ring),
  never its centroid — the centroid passes while the rim is cut; (b) an exemption tuned on SMALL instances
  (a lake wholly inside w<0.5) silently bisects a LARGE one that spans the transition — key the exemption
  to the real boundary (rho<=R_MAX), not a soft midpoint. Also: distance_transform_edt is NON-periodic —
  wrap-pad the equirect seam or a feature straddling u=0/1 gets no collar across it. (Wanapitei repair)

- Point-glow perceptibility is VIEWING-ANGLE-relative, not just size/brightness. Street lamps read
  "perfect from far" but "barely perceptible looking straight down": from a grazing/far angle the dots
  FORESHORTEN and AGGREGATE near the limb into a legible glowing mass; looking straight down the SAME dots
  spread across the full 2D grid, each isolated + faint against the lit ground. The fix is per-lamp
  strength (min_px floor + core brightness), not the aggregate. And a size scan for a "straight cut"/
  anomaly that looks for ONE long edge MISSES a straight line made of many short COLLINEAR segments — diff
  against the ideal instead. (street-lamp fly-note + Wanapitei diagnosis)

## R4-TAILS (2026-07-15) — the drape-fork class + evidence provenance
- **A decal draped on the FIELD forks from the MESH the screen shows.** `radius_at` (bilinear
  field) vs the render mesh's facet interpolation diverged p99 ≈ 6 m / max 60 m at 118 m
  facets — the "roads elevated above terrain" defect was this divergence, not station
  spacing (densify contributed < 0.1 m). Anything that must READ as on-the-ground (roads,
  rivers, future decals) drapes on `facet_radius_at` — the mesh's own interpolation of the
  same field is the anti-fork, and a constant "lift" can never fix a divergence that is
  terrain-dependent. Corollary: lifts derived for one mesh config silently desync if
  [planet] subdiv/tiles change — the drape now reads the live mesh config so it self-heals.
- **Derivation numbers must be committed artifacts.** The adversarial reviewer (correctly)
  attacked a lift derivation whose measurement lived only in a side-channel run — the numbers
  were right but unprovable, and the reviewer's alternative reading (a coincidentally-matching
  wrong row of the committed report) was unfalsifiable until the measurement itself landed in
  the repo (measure_drape_gap.py §4b). If a dial's value cites a measurement, the measurement
  is part of the diff.
- **raylib 5.5 LoadImage truncates 16-bit grayscale PNG** (stbi_load path) — a 16-bit asset
  must ride an 8-bit RGB pack (R=hi/G=lo) or a raw sidecar. The unpack convention lives ONCE
  (world::dem16_unpack); the legacy 8-bit path decodes through the SAME call because
  LoadImageColors expands gray to r==g==b and r*257 == (r<<8)|r exactly.

## R4-FLY-4 (2026-07-15) — kernel-regime edits + reviewer empiricism
- **An orientation/omega edit in ground_contact is DEAD CODE**: sim::step integrates rotation
  AFTER the contact block, overwriting any attitude edit the same tick. A grounded attitude
  constraint must be its own post-rotation block. (Caught by the Fable DESIGN consult before
  a line was written — consult-first on kernel regimes pays.)
- **A cell-index over equirect uv must SPAN-INSERT, and with the exact spherical bound**:
  cell ground-width shrinks with cos(lat) (the aeqd hero axis maps to the equirect POLE, so
  real content reaches |lat| 88°+), and the planar u-span cap/(2π·cos) genuinely
  under-searches — but only poleward of ~89° once row-edge conservatism is counted. The
  first "killer test" for this passed under the mutant it claimed to kill (placed at 87.5°);
  the reviewer proved it EMPIRICALLY by reverting the fix and running the suite.
  Mutation-verify the killer test itself, at the latitude where the failure actually starts.
- **The moved-consumer trap, again (6th instance)**: the brake input had controller-emit
  coverage but ZERO app-chain coverage — three forwarding-drop mutations shipped a brakeless
  live mode through a green gate. Every passthrough field needs a divergence leg PER
  forwarding hop (tick + step_frame), the flap_cmd pattern.

## R4-POLISH (2026-07-15) — baked-field units + the degenerate-count edge test
- **A generated GIS scalar consumed by its NAME is a units bug waiting**: `GisLake.span_m`
  is `sqrt(area)` (a width-class scale from the bake, offline_tool/sudbury_fetch.py) — NOT a
  radius. Consumed raw as a radius it over-covered every round lake ~1.77x (splash on dry
  shoreland, review P1-1; fix r = span/sqrt(pi)). When consuming a baked field, read the
  BAKE for semantics, and put UNITS in the generated header comment.
- **A per-edge contract tested on a one-edge fixture degenerates to a latch test**:
  `reports == edges` with edges == 1 passes under a first-edge-only latch mutation
  (demonstrated live, review P2-1). Force the SECOND edge in-fixture (pop the state off the
  ground, let it re-capture) — the fixture-no-op class applied to event COUNTS.
- **A felt cause can be geometrically BACKWARDS while the felt symptom is real** — attribute with math,
  not with the pilot's theory. "Rounds feel too low, compensate for the small-planet non-euclidean
  geometry" was flown-true as a symptom but the sphere pushes rounds HIGH, not low: v_muzzle (805) ≈ 2×
  v_orbital (√(gR)≈408), so a level round CLIMBS relative to the co-altitude circle (+5.7 m @500 m). The
  actual missing term was velocity-inheritance × trim AoA (the round inherits V along the flight path,
  which rides below the nose by the AoA, so a boresight round departs AoA-below the sightline, −1.5 m
  @cruise → −5.3 m @3 G). 3rd instance of the S-dampff "felt-ask = missing term" pattern; the fix is the
  term (self-scaling −(conv/v)·v_perp_body cant), NOT a fixed dial. Do NOT ship the geometry back as a
  "wall/excuse" (Chad's spec-backward ruling) — plan backward from the felt spec, but let the math pick
  the mechanism and its SIGN. (velocity-aware harmonization, iter-15)
- **A compensation term folded into a MISS metric fires a false cue** — the pipper's out-of-envelope
  `predicted_miss` cue counted the inheritance-cancel cant as "uncompensated droop", so a hard-G pull
  (large v_perp) drove |cue| past the hit radius → FALSE red pipper, exactly when the pilot pulls lead,
  though the shot HITS ≤1.5 m. A cant that aligns DEPARTURE with the sightline produces NO miss; only
  the gravity Δg belongs in a holdover/miss cue. Keep the full cant in the AIM solve, the Δg-only part
  in the CUE. Fable red-team caught it (a cue test that never checked `out_of_envelope` was blind).
  (velocity-aware harmonization, iter-15)
- **Single-source a nose-vs-gun offset through ONE function to make a pipper↔round fork impossible, and
  it fixes the frame for free** — the pipper's old droop-cant was subtracted along WORLD-up while the gun
  canted in BODY +Y (coincide only wings-level → banked shots forked 1.3 m @30°/3.6 m @90°, a silent
  second "rounds low" contributor under banked lead). Routing BOTH the round's aim and the pipper's cant
  through one `harmonization_offset` (returning a body-frame vector rotated by attitude) makes them
  bit-consistent at any bank by construction. Verify the no-fork leg with an ABSOLUTE cross-check that
  fires the REAL canted `weapon::spawn` gun on the pipper and measures closest approach at 0/60/90° bank
  — a wings-level-only fixture is the classic no-op (body-up == world-up hides the frame bug). And the
  seam is cheap: gate the correction on g>0 + v_body=0 ⇒ every existing g=0/rest golden stays
  bit-identical, so the whole mechanism lands with ZERO moved goldens. (velocity-aware harmonization, iter-15)
- **A feature LOCATOR keyed to a proxy of a shape that a dial reshapes is a silent fork** — T13
  shrank the arena (`arena_a_m` 7350→4200) and the breach locator ("spine node nearest
  `arena_apex_r` by RADIUS") kept locating against the OLD shape's coincidence: holes/beacons
  landed 748–897 m from the true bore↔wall crossings while the SDF stayed correct (round-10
  blind spots + "Murray impassable"; the beacons LIT the fake holes = worse than no marker).
  When a config dial moves a GEOMETRY, grep every consumer that locates features of that
  geometry by a derived proxy (a radius, an arc, a fraction) and re-key them to the true
  intersection — computed ON the same function the SDF flies (world::ellipsoid_sd bisection),
  never a re-derived copy. Corollary: the fixture canon (7350) had accidentally hidden it —
  the regression pin must run at the SHIPPED dial value. (T14)
- **A mutation banner must be verified at the CONFIG the leg runs at** — "dropped `stretch`
  fails the rim walk" was measured TRUE at the 7350 fixture and FALSE at the shipped 4200 the
  new leg used (rim ~255 m < the 275 m hole; the crossing simply isn't oblique enough there).
  One mechanism, two dial values, opposite mutant visibility ⇒ the leg needs an arm at EACH
  value that makes its claimed kill honest (S7-push P1 class, 2nd instance). And the Catch2
  non-ASCII name trap bit a 4TH time (em dash on a TEST_CASE continuation line → ctest filter
  matches nothing → "No tests ran") — now a structural gate.sh tripwire scanning the
  TEST_CASE(...)-to-body window, not a remembered rule. (T14 red-team)
- AN AUDIT THAT REWARDS SEALING NEEDS A DUAL THAT REWARDS OPENING: the T16 leak audit
  drove leaks to zero and nothing scored the two DESIGNED openings — so zero was reached
  partly by walling off both entrances (fly-13: "both entrances are occluded"). Every
  coverage-style probe (leaks, holes, gaps) needs its complement pinned in the same suite:
  the flyable entry paths cast as rays that must stay CLEAR, with a geometry-lever arm that
  proves the clear-pins non-vacuous (T17 entrances-open vs cut_swallow_trim=false). Same
  class as "a golden only pins the code that recorded it" — a one-sided metric invites
  optimizing the visible side at the hidden side's expense. (T17)
- A CERTIFICATION SMOKE CERTIFIES ONLY WHAT ITS CAPTION NAMES: murray_bowl_t16.png was
  captioned (and accepted) as "the pit is lit" — the same frame plainly shows the mouth
  plugged by a black dome, unremarked. When a smoke is the closing evidence for a rung,
  enumerate what the frame is supposed to SHOW OPEN as well as what it shows fixed, or the
  defect ships inside the proof picture. (T17)
- TAG DIAGNOSTIC OUTPUT WITH THE ARM THAT PRODUCED IT: the leak audit's per-eye WARNs
  printed identically for the MAIN pass and the two MUTATION arms (which leak 58-71/eye BY
  DESIGN); grepping the binary's output read mutation noise as main-pass regressions and
  cost a multi-hour ghost chase re-"fixing" healthy geometry. Any test that runs the same
  instrument in both honest and sabotaged configurations must stamp every line with the
  configuration. (T17)
- STITCHING TWO RINGS 1:1 BY AZIMUTH INDEX IS ONLY SANE WHEN THE RINGS ARE ROUGHLY
  PARALLEL: a horizontal circle index-stitched to a near-vertical ellipse (the pit funnel
  onto the inclined tube end ring) TWISTS — the band tents over the opening at every
  azimuth and chords through the bore interior. The fix is an ADAPTER ring: map the target
  ring's verts to their own footprint azimuths on the source circle, stitch circle-to-circle
  (cannot twist), then run radially to the exact seam. And recess the adapter sub-metre
  ROCK-SIDE: coincident junction circles weld three surfaces onto one edge ring and read
  non-manifold. (T17)
- TRIMMING RENDER FACETS AGAINST OPEN VOLUMES HAS TWO FAILURE MODES AND ONE SAFE SHAPE:
  any-vertex dropping opens rock-void slivers at every volume junction (the dropped
  straddler's rock-backed half loses its cover); whole-facet-inside dropping leaves the lie
  standing (the defect facets are mostly straddlers). Subdivide mixed facets and drop only
  fully-swallowed leaves, KEEPING mixed leaves — over-covers by at most a leaf, can never
  open a void, at any recursion depth. And test CHORDS, not just corners, against convex
  volumes (verts + edge midpoints + centroid): an on-surface-cornered facet can still pass
  straight through a convex bore. (T17)
- A DECORATIVE VOLUME'S RENDER IS NOT ITS COLLISION VOLUME — SCOPE TRIMS PER PIECE: the
  T6-P0 open cuts are collision CYLINDERS but render as tapered cones/benches INSIDE them,
  so "yield to the neighbouring cut's volume" exposed the unrendered shell between cone and
  cylinder (rock-void leaks), and yielding the tube to the Errington pit gutted the DESIGNED
  recess adit. The trim's cut list is a per-piece design decision (audit-measured), not a
  universal rule — and if a cut's own boundary is load-bearing cover, render it once
  explicitly (the pit cut sleeve) rather than relying on drapes that other rules may drop.
  (T17)
- A COLLISION TRADE MADE WHEN NOTHING RENDERED THE REGION EXPIRES WHEN SOMETHING DOES:
  T6-P0 chose the open-cut CYLINDER because the visible hole was the removed terrain and
  nothing drew the shell — correct then. T16's benches drew a 46-degree funnel INSIDE that
  cylinder, silently converting everything under the staircase into invisible open
  collision; every climb-out shallower than the benches flew "inside the drawn wall" and
  died at an unrendered boundary ("I collided into nothing"). When a render rung reshapes
  what the pilot SEES as solid, re-audit every collision volume that was calibrated to the
  old picture — the seam between "decorative" and "boundary" moved. (T18)
- AN OPEN VOLUME THAT DEAD-ENDS AT A DESIGNED OPENING IS A TRAP AT THE OPENING: the bore's
  contains-volume ended exactly at the mouth ring, so the flyable corridor had a hard
  invisible wall AT the exit everyone flies. Every designed opening needs its open volume
  carried PAST the boundary until a real volume takes over (the mouth throat), and the
  handoff pinned by a VOLUME test (corridor samples contains) — a path-based test alone can
  be satisfied by making the dead pocket merely visible. (T18)
- CAP AN AUXILIARY OPEN VOLUME AT THE PLANE WHERE THE NEXT VOLUME OWNS THE AIR: the
  uncapped throat overlapped the bowl "redundantly" — harmless against the cylinder,
  but when the bowl tightened to the cone the redundant body poked OUT through the drawn
  staircase (83 m of open air behind a drawn bench). Redundant-overlap arguments are
  calibrated to the CURRENT sibling volume; a hard clip at the ownership boundary
  (rel <= floor+2) survives any sibling retune. Same class as config-relative bounds vs
  silent disarm, applied to CSG. (T18)
- KEY A GEOMETRIC TRIM ON THE FEATURE'S OWN FRAME, NOT A WORLD PLANE: "drop band facets
  touching ABOVE-FLOOR ring verts" meant "the brow over the arch" at Murray (ring centre ==
  floor) but "the entire band including the crater-floor wrap" at Errington (ring centre 90 m
  above the floor) — 20 see-through leaks. The portable form was above-RING-CENTRE. A trim
  tuned at one instance of a feature must be re-derived in the feature's intrinsic frame
  before applying to the next instance. (T18)
- A LETHALITY PROBE'S OWN GUARD RAILS CAN VACUATE IT: the T18 pure-vertical dive rows
  started 300 m above grade, and lethal_on_nothing's FIRST march sample tripped the
  "escaped (grade+30)" early-return — every row returned false without marching, so THREE
  candidate mutation arms all measured 0 and the pin was pure theater until the start
  height moved under the ceiling. When a new probe reuses a harness helper, check which of
  the helper's early-returns the new geometry hits on sample ONE — the fixture-no-op class
  lives in the harness, not just the fixture. And when a mutation arm measures 0, treat it
  as data about the PIN (which guard swallowed it?) before concluding the mechanism is
  unpinnable — here every "0" traced to the same first-sample return. (T23)
- AN SDF USED AS A DISTANCE PROXY MUST BE CONTINUOUS OVER THE PROBED SET: tube_signed_
  distance jumps from +1.9 to -87.4 across the bore's END-CAP plane (2.5 m between verts),
  so "every vert within one tooth of |sd|" — sound for ring verts ON the wall — is
  meaningless for trim leaves hugging the cap seam, and no numeric edge cap can be derived
  from sd there. Pin the mechanism's CONTRACT (a kept leaf is never fully swallowed beyond
  eps) instead of a metric the geometry breaks; add float-roundtrip slack (ulp ~2 mm at
  15 km) when the test re-reads float positions the trim classified as doubles. (T23)
- A WHOLE-FACET TRIM AGAINST A VOLUME OVERSHOOTS BY A FACET, AND THE COVER BEHIND THE
  OVERSHOOT IS A SEPARATE CONTRACT: the slot cut through the 40 m floor-disk facets always
  over-opened; it stayed invisible while the adapter bands happened to sit behind it, and
  T21 moving them 6 m down/4 m in EXPOSED it — the see-through only threads at 90 degrees,
  which no 70-80-degree fan sees. Trim-overshoot + moved-cover = a two-change defect
  neither change shows alone; the structural fix is subdivision at the boundary (voids
  impossible, over-cover allowed) — now on BOTH trims, not just the cut trim. (T23)
- A DRAPED DECAL LAYER MUST CONSUME THE SAME CUT LIST AS THE SURFACE IT DRAPES ON: the
  ribbons draped the heightfield while the terrain dropped triangles over the excavation
  cuts — lines hovering across the void ("almost overhang"). Any new draped layer (ribbons,
  future decals/scatter) must consume the shared cuts at build; export the ONE in-cut
  predicate (dir_in_any_cut) rather than letting each layer re-derive it. Put the pure clip
  in a raylib-free header so ctest pins the SHIPPED function (the app render TU links GL
  and no test can). (T24)

## T26 seal pass (2026-07-23)
- **Single-piece removal cannot attribute overlapping geometry.** Two agents burned large
  budgets "removing the collar band / trench drape / tube" one at a time with zero pixel
  change — coincident pieces keep painting the same pixels, so only removing ALL of them
  reveals anything. Attribution needs an isolation/signature instrument (render ONE
  candidate at a time, or tint each pass a signature color) — or a config-level toggle
  that gates the whole suspect ensemble (`headframe_on=false` cracked in one smoke what
  14 rebuild-probes could not). Corollary: probe on the SAME field the symptom renders on
  (a uniform-field probe vs a real-DEM smoke is a second confound).
- **A cosmetic cover piece can be load-bearing for leaks — and the reason can live in a
  DIFFERENT subsystem's defect.** The portal-side "lid" could not be dropped without
  opening census voids only because `fill_face`'s any-vertex cut trim over-excavated base
  terrain by a whole cell; with that latent bug fixed the coupling dissolved. Before
  declaring a cover un-droppable, ask what SHOULD be backing it and whether that backer
  is itself defective. And cross-worktree: parallel agents cannot see each other's
  landed fixes — the supervisor must re-run a falsified attempt when a sibling's fix
  changes its premise.
- **Fixed-offset ornament placement on a terrain-cut site is a slab factory.** The portal
  lintel/canopy hung off the rigid bore-mouth frame while the ground under them was
  excavated away; the neighboring ghost-ruins were immune because every footing conforms
  via ground_pt/surface_r. Any structure near an excavation must either conform to the
  real surface or be proven to sit over rock at ALL its extents. (And its satellite
  consumers — the lintel-top beacons — must ride the same gate, or removing the mesh
  leaves floating lights; grep for consumers of any deleted box.)

## v5 flight-kernel-v5 (2026-07-23)
- A mutation revert via `mv file.orig file` PRESERVES the orig's OLD mtime — ninja
  sees nothing to do, the object file still holds the MUTANT, and the next "clean"
  gate/harness run certifies (or fails) the mutant, not HEAD. The inverse of the
  stale-binary trap: here the SOURCE is honest and the BINARY lies. After ANY
  mutation revert: `touch` the file, rebuild, and require the compile line (not
  just the link) in the build output. Caught live: a 362/362 gate turned 2-fail
  because the post-restore rebuild printed "[1/1] Linking" — link-only = the
  mutant object shipped into both seads_tests.exe AND seads_harness.exe, so even
  the measured AFTER grid was contaminated until rebuilt. (v5 S-truedepth)
- A "wrong heading" symptom on an INSTRUMENT has to be attributed to its SOURCE
  before its MATH: the map arrow's rotation sign and north/east basis were
  correct all along — the defect was that it read the GROUND TRACK (velocity's
  tangential part) instead of the FACING (the nose). One source, two distinct
  reported symptoms: velocity FLIPS 180 deg against the nose in a tail-slide
  (SPEC 9's "tail-slide lies", alpha ~ 180) and its tangential part DEGENERATES
  to lateral noise on any near-vertical trajectory. "It gets spun around
  sometimes" was BOTH, and neither is a sign error. Two durable sub-rules: (1)
  a degeneracy guard's epsilon must be scaled to the QUANTITY, not to zero — the
  old bail was 1e-6 m/s, which a 200 m/s vertical dive never reaches while
  carrying ~1 m/s of pure noise; expressed as a UNIT-VECTOR fraction the same
  guard is a plain sine threshold and is dimensionally honest. (2) The house
  v-hat idiom (HOLD the last valid value, never recompute from noise) transfers
  from sim/ to any render instrument unchanged. (S-maparrow)
- "No conflicting or camouflaging colors" is enforceable, and a comment is not
  the enforcement: the map's basemap-is-greyscale ruling became a LOADER CHECK
  (paper/ink/water must be neutral within one 8-bit step), so re-tinting the
  lakes blue — the exact pre-existing camouflage — will not load. Same class as
  "config-relative bounds or silent disarm", applied to an ART ruling: a look
  decision that markers depend on is a CONTRACT, so give it a tripwire. Its
  companion on the same pass: a zero marker size is an INVISIBLE marker, so the
  loader rejects it rather than clamping — clamping would hide the very
  legibility defect the pass existed to fix. (S-mapread)
- Colour-code a tactical display on SHAPE FIRST, hue second: green/red is the
  most common colour-blindness confusion, so ally/enemy/objective/player each
  got their own glyph (circle/square/diamond/arrow) AND a lightness separation,
  with hue as the redundant third cue. And the two rules that actually killed
  the camouflage: (a) the BASEMAP gets zero chroma, so every coloured pixel is
  by construction tactical; (b) a TERRITORY WASH in a team's own colour sits
  underneath that team's own markers — outline-only is the default and the fill
  is a dial, not a look. (S-mapread)

- A "180 degrees opposite on the colour wheel" ask has a plausible WRONG answer
  that no reviewer can see: reversing the channels of (1.00, 0.55, 0.12) gives
  (0.12, 0.55, 1.00), which is 181.36 deg away, not 180 — the true complement is
  (0.12, 0.57, 1.00). A 1.36 deg hue error is invisible on screen, so "verify" can
  only mean an EXECUTABLE check: convert both to HSV and require hue separation
  == 180 (tight), equal S, equal V. Put the SAME predicate at the config edge as
  well as in the unit test, or the invariant only holds for the values that
  happened to be typed today. And make the test FORWARD-derive the complement
  from the source colour rather than merely cross-checking the two constants
  against each other — otherwise editing both to a wrong pair passes. Bonus
  gotcha: a shortest-arc hue metric reports the 181.36 deg miss as 178.64, so
  assert on |separation - 180|, not on the raw number you computed by hand.
  (S-mapteam)
- Hoisting a colour that lives inside a GLSL string does not need a uniform: build
  the shader source as `#version` + a generated `#define` + the body, and inject
  the shared constant at shader-build time. Zero per-frame cost, the value stays a
  compile-time constant to the GPU, and the H1 fork ("the slag orange" retyped
  into a second file) is closed by construction. It does require the raw string to
  carry NO `#version` line of its own — that is the whole trick. (S-mapteam)
- raylib's `DrawTriangle` CULLS anything not wound counter-clockwise in screen
  space, so a new 2D glyph can ship completely INVISIBLE with a green gate and no
  warning. Worse, the obvious "reverse it" fix is easy to get wrong: ROTATING the
  vertex list (a,b,c -> c,a,b) leaves the winding unchanged, so the second attempt
  looks different in the diff and renders identically. Reverse means SWAP two
  vertices. The only instrument that caught either round was reading the
  screenshot — the "green gate is blind to the app binary" rule applies to
  primitive winding, not just shader linkage. (S-mapteam)
- When a marker's FILL stops carrying ownership (a neutral objective colour), the
  ownership cue that remains must be re-verified in BOTH directions against the
  real plate — a ring that reads fine in one team colour can vanish in the other
  over a dark road or a grey lake. Give the ring its own dark casing and look at
  both, rather than reasoning that "it worked before": before, the fill was doing
  half the work. (S-mapteam)

- AN ICON IS ITS SILHOUETTE: at map-marker size (r ~ 9 px) an INTERNAL detail drawn in
  near-black inside a dark-green disc carries ~0.35 luma over two pixels and averages
  away in the downsample, so the round-2 P&ID pump wedge was convincing at 480 px and a
  green BLOB on the real chart. The fixes that worked all changed the OUTLINE (the
  discharge volute moved from a notch cut INTO the casing to a cone standing PROUD of it)
  or the LUMA (the plinth went from pump-green to black, and the one interior cue is
  LIGHT-on-dark). Corollary discipline: judge a marker on a 1:1 crop of the real chart
  with its neighbours in frame; the zoom is for verifying construction only, and a zoom
  that looks good is not evidence. (S-pumpglyph)
- A RING SIZED OFF THE CASING RADIUS PAINTS OVER A GLYPH THAT GREW: the owner ring was
  derived from `r` while the round-2 glyph fitted inside `r`; the moment the round-3
  volute reached 1.72r the ring annulus (r+3.8 .. r+8.6) covered the entire silhouette
  improvement — and it was INVISIBLE in the zoom, where the ring is proportionally
  thinner, so the first screenshot check passed it. Derive an enclosing decoration from
  the drawn extent, not from one component's radius, and the size dial then scales the
  whole assembly with nothing to re-tune. Same family as "config-relative bounds or
  silent disarm", applied to layout. (S-pumpglyph)
- A HALO/OUTLINE PASS TAKES A PIXEL PAD, NOT AN INFLATED RADIUS: re-running the same
  shape builder at `r + halo` uniformly scales every proportional offset, so a part at
  1.72r grows by halo*1.72 and the "outline" becomes a fat slab on that side only. Add
  the pad to each offset instead — an outline is a constant-width stroke by definition.
  (S-pumpglyph)
- "NEON" IN A DARK CHAMBER IS AN UNLIT ADDITIVE DRAW, AND ITS THICKNESS CANNOT COME FROM
  LINE WIDTH: the stope has no sun and no lamp, so anything shaded is invisible there
  (that was the reported defect, not a symptom of it) — the frame must ride the default
  UNLIT path with BLEND_ADDITIVE, depth-TEST on so rock still occludes it and depth-WRITE
  off so nested passes cannot fight. And `rlSetLineWidth`/`glLineWidth` is clamped to 1
  in GL core profile, so a width-based glow ships as a hairline on exactly the hardware
  it runs on: get thickness from N nested shells a fixed metric distance apart, which
  additionally blooms up close and collapses to one bright line at range. Pick the
  spacing from a screenshot at the real viewing distance — 0.8 m read as three separate
  parallel wires at 140 m, 0.4 m read as one tube. (S-pumpcube)
- A CLEARANCE BOUND IS ONLY REAL IF THE RADIUS IT GUARDS IS SINGLE-SOURCED, AND IT MUST
  COVER THE INNERMOST MEMBER: the wire frame's "cannot z-fight the body" argument rests
  on `half_extent_m > shell_radius`, which is worthless if the shell radius is a literal
  in draw.cpp and the bound is a copy of it in the loader — so the constant moved to
  `render::kDeepPumpShellM` and the sphere is drawn FROM it (H1). And a bound written
  only against the outer shell passes a table whose INNER shell is buried in the body:
  check `half_extent - (layers-1)*step`, i.e. the extreme member, not the parameter.
  (S-pumpcube)
- OWNERSHIP DECORATION NEEDS AN EXPLICIT NEUTRAL ARM EVEN WHEN NO NEUTRAL STATE EXISTS:
  pumps are only ever faction 0 or 1 today, so `ours ? ally : enemy` is "correct" — and
  it silently paints a DESTROYED pump in its last owner's colour, which is a stale
  ownership claim, the exact defect the mechanism was built to remove. Route the colour
  through one pure function that falls to a documented neutral for dead OR out-of-range,
  and pin both arms; the defensive branch costs one line and is the only thing standing
  between a future capturable pump and a wrong claim. (S-pumpcube)
- WHEN A FEATURE'S WHOLE PURPOSE IS "VISIBLE IN A PLACE THE PILOT GOES", BUILD THE CAMERA
  THAT GOES THERE BEFORE BUILDING THE FEATURE: `SEADS_TUNCAM_PUMP` (the SEADS_TUNCAM_*
  precedent) took ten minutes and turned "should read in the dark" into a screenshot, and
  it PRINTS the index / SURFACE-or-DEEP / faction so a shot cannot be mislabelled as the
  wrong team. Its limit is also worth recording: a 40 m frame around a 26 m body admits no
  useful INSIDE shot — from anywhere inside the frame you are within 8 m of the body and
  it fills the FOV — so that one arm is honestly unverifiable by screenshot and belongs on
  the fly card, not in a claim. (S-pumpcube)

- A PASSING VERIFICATION REPORT IS EVIDENCE ABOUT WHAT YOU MEASURED, NOT ABOUT WHAT YOU
  DID. The soundbank pipeline built a self-verifying report — every asset re-measured for
  integrated loudness and true peak, 15/15 PASS — and was wrong: ffmpeg's loudnorm engages
  LINEAR mode (one static gain, crest preserved) only when the gain fits under the
  true-peak ceiling AND `measured_lra <= LRA`, and otherwise falls back to DYNAMIC level
  riding SILENTLY — no warning, no non-zero exit. 9 of 15 assets had been dynamically
  processed, riding ~10 dB across a music bed and 15.3 dB across an ambience bed, i.e.
  exactly the pumping the manifest forbade. The gate could never catch it because dynamic
  mode hits the integrated target BETTER than linear: the two quantities being measured are
  the two a mode fallback cannot disturb. The fix was not a parameter but an ASSERTION —
  pass 2 re-emits `normalization_type` and a mismatch is a build failure. Generalization:
  when a tool has a silent fallback path, the check that matters is "did it do what I asked",
  not "is the output within tolerance" — a fallback usually EXISTS because it hits the
  headline number more easily. Same family as "a golden only pins the code that recorded it",
  applied to a third-party tool's mode selection. (soundscape rung, 2026-08-17)
- SIZE AN ENVELOPE AGAINST THE MEASURED ASSET, NOT BY FEEL — and pin it. The stope blast's
  duck was authored at 1.1 s hold "across the body of the rumble" while the asset was 33.4 s,
  so the music swelled back with ~29 s of blast still playing: the literal opposite of the
  requested occlusion. The asset itself then turned out to be 21 s of DIGITAL SILENCE
  (-70 LUFS from 15 s on), which ALSO broke the scheduler — one non-polyphonic Sound whose
  re-trigger restarts it, with three of four gaps shorter than the asset, so it truncated
  itself on 3 of every 4 cycles. Both are now `static_assert`s against a `kStopeBoomLen`
  constant. The contrast within the same change is the lesson: the intro cue times WERE
  derived from measured asset lengths and were correct to 3 decimals; the SFX envelope was
  not measured and was wrong twice. Measure every asset a timing constant refers to.
  (soundscape rung, 2026-08-17)
- A LOADING PAGE HAS NO FRAME LOOP, so "put music on it" is not a level decision, it is a
  BUFFER decision — and the size must be MEASURED against the longest UNINTERRUPTIBLE
  stage, not against the total load. SEADS' world build is 21.5 s, and 21.4 s of that is
  one call (`render::building_colliders`) with no way in. raylib only refills a music
  stream when `UpdateMusicStream` is called, so the gameplay-sized sub-buffer (4096
  frames = 186 ms) empties in the first blink and the "louder in the loading page" bed is
  silence for the entire page — with no error, no warning, and a fully green gate, since
  no ctest runs the binary. Sprinkling pump points through the load does NOT fix it: the
  bound is the worst single gap, and a 21 s call is one gap. The fix is a SEPARATE Music
  handle with a sub-buffer sized to a measured multiple of that gap (raylib keeps two
  sub-buffers, so the runway is 2x the size), freed when the page is done, leaving the
  gameplay bus and its small hitches untouched. The instrument came first and stayed:
  `SEADS_LOADPROF=1` prints every stage gap, so the constant can be re-derived on any
  machine instead of re-guessed. Same family as "size an envelope against the measured
  asset" — here the asset being measured is the LOAD. (soundscape rung, title-first
  intro, 2026-08-17)
- A COMPLETED SHAPE CAN STILL BE THE WRONG SHAPE: the intro was fully built, fully tested
  and green with the title card at the END and the music off throughout, because it was
  assembled from chat answers while Chad's two readmes read as 0 bytes on disk. His actual
  text says the opposite — "louder in the LOADING PAGE", "Resigned to fate for AFTER title
  screen and black screen". No amount of test coverage detects a correctly-implemented
  wrong spec. The tests that replaced them now pin the ORDER (title, then black, then
  voices) against quoted readme lines, not just the arithmetic — so the shape itself is
  the thing under test. Corollary to "a missing source-of-truth doc is a BLOCKING
  question": once the doc arrives, re-derive the SHAPE from it, not just the numbers.
  (soundscape rung, 2026-08-17)
- raylib's `GetFrameTime()` READ BEFORE `BeginDrawing()` REPORTS THE FRAME BEFORE LAST, so
  any clock accumulated that way is a full frame out of phase. `CORE.Time.frame` is written
  in EndDrawing as `update + draw` (rcore.c:944) where `update` was measured back in the
  PREVIOUS BeginDrawing (rcore.c:876) — so at the top of a loop it describes the frame that
  ended two presents ago. On the loading page, where each "frame" is a whole build stage,
  that shifted the page clock by an entire 21.9 s stage: `load_title_alpha` was still ~0
  when the long call began, so the title was invisible black for the whole load and snapped
  up only at the end. The feature was fully implemented, fully tested and completely absent
  on screen, with a green gate — no ctest runs the binary, and the LOADPROF instrument
  printed each stage's duration against the WRONG stage name, so even the measurement lied
  (it blamed `building_colliders` for `planet_heightfield`'s 21.9 s). Same class as the
  first-frame-dt bug that ate the intro on Chad's first fly, one frame further along. Rule:
  for any clock that must not inherit frame phase, use the ABSOLUTE clock (`GetTime()`)
  and subtract; `GetFrameTime()` is only honest read AFTER EndDrawing, and only for the
  frame it just closed. Corollary: a "wait for the effect to ramp" must be positioned
  BEFORE the long blocking work, not left to ramp against it. (soundscape rung, loading
  page, 2026-08-17 — found by the fresh-context red-team, not by the author)
- A REVERB PINNED THROUGH ITS ACCESSOR IS NOT PINNED: `wet_` had six assertions on
  `wet()` — entry ramp, exit ramp, the hard-zero snap — and every one passed with the
  wet mix DELETED FROM THE AUDIO PATH (`wet_g = kStopeWetMax` instead of `* w`), i.e.
  with the room snapping to full at the portal and hanging around through open sky.
  Three sibling mutants survived too (dry duck deleted, both allpass diffusers deleted,
  damping deleted). The legs that kill them all share one shape: assert the OUTPUT VALUE
  against a formula built from the constants the signal actually passed through (first
  reflection == wet * trim * 0.25 * g_allpass^2), not a bound like "> 0" or "< 0.5" that
  the mutant also satisfies. The state variable is not the mechanism; the samples are.
  (stope reverb red-team, 2026-08-17)
- A FEEDBACK COMB'S RESONANT GAIN IS 1/(1-g), AND A SUSTAINED SWEEPING TONE WILL FIND IT.
  Reverb gains sized for an impulse (guns, blasts) measured peak/input 1.79 on a
  full-scale sine parked on a comb harmonic — half the samples railing at the int16 clamp.
  The channel it shipped on was the bagpipe throttle drone, which is sustained AND sweeps
  its pitch with throttle, so it slides through every comb resonance by design. Transient
  test signals cannot see this: an impulse never builds up. Test a reverb with the WORST
  input its actual channel can produce (steady tone at 1/comb_delay), not with the input
  that motivated it. (stope reverb, 2026-08-17)
- A DERIVATION IS NOT A PIN: four tests and four static_asserts over a mix header all
  passed with the flown balance ENTIRELY REVERTED (music 0.95->0.55, wind 0.55->0.85,
  drone 0.30->0.42 — precisely the values Chad had complained about), because every
  assertion was an algebraic identity over the same constants it was guarding
  (`kWindStreamTrim / kMusicSurfaceGain == kWindFlownTrim / kMusicFlownSurface` holds
  for ANY value of those constants). Deriving levels from a single dial genuinely fixes
  the duplicate-literal problem, but the tests that come with it must pin at least one
  VALUE per flown constant or they pin only the shape of the arithmetic. Same family as
  "a golden only pins the code that recorded it", one level of abstraction up.
  (mix_levels red-team, 2026-08-17)
- WORSE THAN AN UNPINNED RULING IS A STATIC_ASSERT THAT PRE-EMPTS IT. Chad ruled "trim
  wind / engine further"; the first build multiplied music, wind and engine by one
  common dial, which leaves the wind:music ratio bit-identical — the single relationship
  he named could not move — and then added a static_assert whose message was "the
  music-to-wind ratio Chad has not yet judged has moved" to guarantee it never would.
  The reasoning felt principled (preserve the ratios of the fix he has not yet flown) and
  it silently converted an ambiguous word ("further" — further than before, or further
  than the music?) into a decision, welded into the build. When a ruling is ambiguous,
  the resolution goes in the REPORT as a question, never into an assert. The honest
  build is two dials: one for the ruling that is unambiguous, one for the one that
  isn't, each nameable in a fly report. (mix_levels red-team, 2026-08-17)
- A PARTIAL BUS MOVE INVERTS THE MIX YOU WERE FIXING: dropping music+wind+engine by 4 dB
  while leaving guns (0.55), combat sfx (0.85) and the blast (raylib's default 1.0 — it
  had no SetSoundVolume AT ALL) untouched raises every one of them 4 dB RELATIVE to the
  bed, which is the exact complaint the change existed to answer. When a "master" dial
  is introduced, enumerate every channel that reaches the device and put each one on it
  or state in the header why not; a channel whose level was never set is the easiest to
  miss because there is no literal to grep for. (mix_levels red-team, 2026-08-17)
- `SetAudioStreamBufferSizeDefault` IS GLOBAL AND STICKY, AND raylib ZERO-FILLS THE
  REMAINDER: a stream created after the music loads inherits their 4096-frame sub-buffer,
  and feeding it the usual 1024 frames makes raudio.c memset the other 3072
  (`raudio.c:2693`) — 25% signal, 75% silence, i.e. a 5.4 Hz chop plus ~0.4 s of queue
  latency, not the reverb tail that was intended. The file's own comment said "nothing
  after this point creates a 22050 Hz stream"; the new send made that sentence false and
  nobody re-read it. A load-bearing comment about global state is a TRIPWIRE — when you
  add the thing it says does not exist, the comment is the bug report. Set the default
  around every stream creation, and restore it. (blast send red-team, 2026-08-17)
- TEST THE RESAMPLER AT A RATE THE SHIPPED PATH DOES NOT USE: 44.1 kHz -> 22.05 kHz is an
  exact 2:1 decimation, so every output sample lands on a source sample and the
  interpolation branch NEVER RUNS — a nearest-neighbour mutant is bit-identical there and
  survives any test written at the shipped rate. Probe at 48 kHz (fraction live) to reach
  the interpolator, and upsample (decim < 1) to reach it without the anti-alias filter's
  group delay confounding the oracle. Bonus, learned by writing a wrong oracle first: a
  ramp through a 4-pole AA filter comes out offset by exactly the filter's group delay
  times the slope (measured 3.1e-4, predicted 1.5 samples), so "resampled ramp == ideal
  ramp" is a false expectation — pin the error's FLATNESS (a delay) against a
  nearest-neighbour sawtooth instead. And to catch a stereo downmix that silently keeps
  only channel 0, feed channels that CANCEL. (blast send, 2026-08-17)
- ★★★ A FIX IS ATTACHED TO A SIGNAL, NOT TO A CODEBASE. The R4a arming rung was
  drive-found to arm on ROLL ALONE (frictionless supports fall as cos(roll)) and was
  fixed by making the load reference LIVE instead of the frozen at-rest split. The very
  next rung needed the same model's OTHER term for stage 2's boot release, wrote
  `board_frac / board_frac_rest` — the frozen reference — and re-shipped the identical
  defect on a different signal, where the existing gate leg was structurally blind to it
  (case 8 grades `stage_arm`, not the boot release). Vigilance is not the countermeasure:
  the countermeasure is that the fixed expression becomes the ONE expression every
  consumer reads (`rider_support_release`, with `rider_stage_arm` now its product). This
  is the "single-source or fork (H1)" law applied to a BUG FIX rather than to a
  sim-semantic expression — a fix that lives in one caller is a fork waiting for its
  second caller. (R4a rung 2, plan red-team round 2, 2026-08-28)
- A REVIEWER'S RECOMMENDED FIX IS NOT AUTHORITY, AND ROUND 2 MUST BE ALLOWED TO ATTACK
  IT. Round 1's plan review recommended an up-sweep/down-sweep REVERSIBILITY gate leg;
  the plan adopted it; round 2 (fresh context, given round 1's report explicitly marked
  as itself under review) showed the graded function is stateless, so the sweep is
  bit-identical BY CONSTRUCTION — the fixture-no-op class, introduced by the review that
  was supposed to prevent it. Run round 2 in a FRESH context rather than resuming the
  round-1 reviewer (a reviewer asked to re-examine its own advice confirms it), and hand
  it the prior report as an artefact to attack, not as a premise. (R4a rung 2, 2026-08-28)
- AN IK SOLVER THAT CLAMPS AN UNREACHABLE TARGET IS A SILENT SATURATION, and the clamp's
  DIRECTION is a design decision nobody usually makes. `solve_chain` clamps the
  hip->target distance into [|l0-l1|, l0+l1] without telling the caller, so a blend that
  hands it an out-of-reach point gets a limb that stops moving while every caller
  believes it moved. The obvious repair — clamp the point radially into the annulus —
  pulls it TOWARD THE ROOT, i.e. toward the machine the whole keep-out exists to keep
  him out of, and it engages in the NORMAL state (a chain that trails 2.2 m against a leg
  that reaches 0.9). Clamp the BLEND WEIGHT along the segment instead: the target stays
  on its own start->end line, monotone in the weight, and the residual fork stays
  measurable. (R4a rung 2, 2026-08-28)
- A MEASURED-LOOKING NUMBER CAN STILL BE THE WRONG QUANTITY. The body chain's per-station
  probe boxes are measured off CPU-skinned vertices, so a probe CENTRE is the centroid of
  a flesh lobe — not a joint. Using one as an IK target displaces the limb by
  (centroid - joint): 11.6 mm at the thigh, 9.2 mm at the knee. "It came from the
  measurement script" is not the same as "it measures the thing you are about to use it
  for" — name the quantity, not its provenance. The right offsets were already in the
  script's hand (every station is built from a named L/R joint pair whose midpoint threw
  the offset away). (R4a rung 2, plan red-team round 1, 2026-08-28)

## R4a rung 2b -- the erratic legs (2026-08-28)

- **A KEEP-OUT THAT FORBIDS THE POSE MAKES THE POSE-SPRING ITS ENEMY.**
  `render/body_chain.h` had said in its own header, since the table was
  measured, that the SEATED pose is illegal against the body chain's seat
  keep-out -- by design, because he is sitting in it (pelvis 52.5 mm inside,
  thigh 38.6). A later rung then made that same seated pin the permanent
  attractor of the return-to-pose spring. Nobody reconciled the two, and the
  result was a spring and a constraint pass fighting every substep forever, with
  Verlet reading each ejection as velocity: 0.49 m in ONE substep, peak 1.21 m,
  never settling. Chad saw it as legs "going all over erratically". ★ The two
  facts were each written down, in files a few hundred lines apart, and the
  defect lived in the space between them -- so the question to ask when a rung
  makes something a permanent target is WHAT ELSE ALREADY HAS AN OPINION ABOUT
  THAT VALUE.

- **★★★ LEGALIZE BY ALLOWANCE, NOT BY RELOCATION.** The obvious fix -- project
  the illegal pose out of the solid and aim the spring at that -- was built,
  and then measured: it moves his pelvis 115 mm and his worst leg station 73 mm,
  i.e. a man sitting above his own machine. The cheap fix is to tell the
  keep-out how deep the POSE already sits and grade everything against that: at
  the pose the push is exactly zero, so there is nothing to fight, and going
  deeper still pushes back -- to the pose, not out of the seat. Nobody moves.
  ★ When a constraint and a target disagree, moving the target is one answer and
  ADJUSTING THE CONSTRAINT'S ZERO is another, and the second is free.

- **★★ A RULING IS ATTACHED TO THE DESIGN THAT PROMPTED IT.** Chad ruled
  per-limb grading for the leg stations to kill a 0.4 m residual -- a residual
  that belonged to the relocation fix, which was then abandoned. Applied to the
  allowance design it corrected nothing and COST: drawn-limb sink into the
  machine went 4.4 mm -> 120 mm, because two limbs deep in the solid each prefer
  their own lateral exit, those cancel, and the bone gets no push at all. The
  honest move was to build it, gate it, ship it OFF, document why in the file,
  and put the changed premise back in front of him -- not to apply the words to
  a design they were never spoken about, and not to drop them quietly either.
  Sibling of "a fix is attached to a SIGNAL, not to a codebase".

- **THE COMBINE MATTERS AS MUCH AS THE RULE.** The first per-limb combine took
  the MEAN of the two limbs' escapes. With one limb inside and one clear that
  delivers half the push the inside limb needs, and the drawn leg sinks by the
  half that was thrown away (4.4 mm -> 44.4 mm, before the deeper 120 mm result
  above). The reconcile has to be componentwise: opposite demands cancel (one
  translation cannot move two limbs apart), same-direction demands take the
  LARGER -- never the sum, never the mean.

- **★★★ A GATE CALIBRATED AGAINST THE MECHANISM, THREE TIMES IN ONE FILE.**
  Every one of the new gate legs failed honestly first, and each was wrong the
  same way -- it forbade the mechanism instead of the defect. (1) A peak
  per-substep bound forbade the 1 Hz spring's own gravity sag (g/omega^2 =
  0.248 m). (2) A two-sided seated-vs-seatless fork bound forbade the seat from
  HOLDING HIM UP, which is its job -- the honest invariant is one-sided (the
  keep-out may only hold him closer to his pose, never further) and scoped to
  while he is still AT the pose, because once he has genuinely travelled he is
  being deflected around a solid, not fought. (3) A straddle fixture put both
  probe boxes near the seat floor, where the cheapest face for BOTH is straight
  down -- the two rules then agree and the case proves nothing. That third one
  is this program's most-repeated test defect (the fixture-no-op class) and it
  was walked into again on the very rung written to fix a gate that could not
  fail.

- **THE WIRING IS THE STAGE MACHINE.** Rung 2's block in `render/sled_model.cpp`
  called itself "gather / call / solve / settle, with no arithmetic in it". It
  was not true: a branch on a stage weight, a choice of spring target and a
  stiffness law are decisions, and they were sitting in the one TU no test
  binary links -- which is exactly where the defect was. Six green gate legs
  coexisted with a visibly broken drive because all six graded the pure blend
  against a STATIC chain fixture, and none could reach the code that produced
  the chain. ★ When the drive disagrees with the suite, the first question is
  not "which test was wrong" but "what can the suite not execute".

- **AN EXPONENTIAL NEVER ARRIVES, AND SOMETHING WAS WAITING FOR IT TO.** The
  stage weight is low-passed and the pin/release branch asks `arm <= 0`. Decaying
  toward a raw 0 from 0.05 the published value stays strictly positive for order
  ten seconds, so ONE throttle blip held the release open for the rest of the
  drive. Fixed in the SIGNAL (`rider_load_lp_step` snaps to exactly 0 below a
  1e-4 noise floor on an exactly-zero target), never with an epsilon at the
  branch -- an epsilon on a live continuous weight is the latch disease itself.

## R4a superman — the buck, the arming memory, and four ways an instrument lies
(2026-08-29/30, `sandbox/r4a-phase0`, driven and signed: *"just the right amount of
novelty suprise"*)

- **★★★ AN INSTRUMENT WRITTEN BEFORE A MECHANISM KEEPS MEASURING THE WORLD WITHOUT
  IT — AND STAYS GREEN.** `probe_superman` was authored before the body chain had a
  return-to-pose spring. It never gained one. So every number it published for the
  life of that spring described a *springless* chain, while the spring was the
  dominant term in the shipped code — outmuscling honest body drag about 40:1.
  Four correct pseudo-force terms could measure clean over 31 tapes while the
  shipped rider did nothing. Nothing goes red when this happens. **Before trusting
  any rig, diff its model against the shipped one TERM BY TERM.**
- **★★★ A FILTER THAT PROTECTS A STATISTIC BURIES THE FINDING.** The same probe
  dropped every free-fall sample (`has_field = aeff > 2.0`) because a
  chord-angle-from-down is meaningless with no down. Correct for its own metric —
  and it discarded the exact regime under investigation, because `aeff` is the
  pseudo-field only and carries no drag term. **When you exclude data, write down
  what the exclusion makes UNANSWERABLE.**
- **★★★ A COLUMN THAT IS NOT PRINTED READS AS A ZERO, AND A ZERO LOOKS LIKE A
  MEASUREMENT.** A CSV format string carried six `%.4f` for seven arguments, so the
  post-landing charge was passed and never printed. The empty column read as
  "recovery 0.00 s" for every arm of the sweep, and was one step from shipping the
  WRONG dial — the best-separation setting leaves the rider limp for 20 s after a
  hard landing. Varargs will not warn you. **Count conversions against arguments.**
- **★★★ THE CONSTRAINT PICKS THE DIAL, NOT THE SCORE.** Separation peaked at
  `0.02/1.0` (24.4x). That arm was unusable. The shipped pair `0.02/3.0` scores
  worse (17.9x) and is right, because the second dial is bounded from below by how
  fast he must re-seat himself after touching down. **A sweep that optimises one
  number without measuring what it costs elsewhere is not a calibration.**
- **★★ NEVER QUOTE A SWEEP BEFORE EVERY ARM HAS THE SAME `n`.** A complete arm
  compared against an incomplete one reported the separation trend BACKWARDS, and it
  was stated to Chad before it was caught.
- **★★ A METRIC NEEDS A CONTROL THAT CAN CONVICT IT.** The wind-referenced
  displacement claim ("an adrift chain projects to nothing on the wind axis") is only
  half true — it fails for any *other* coherent push. The isotropy rows added to test
  it convicted the author's own headline within one run: across-wind displacement was
  3x the along-wind one.
- **★★ A CLOSED FORM AGREEING WITH A MEASUREMENT IS NOT CONFIRMATION WHEN BOTH USE
  THE SAME CONSTANTS** — and check the value you assumed for the free variable. A
  "match" at an assumed 25 m/s evaporated when the measured median turned out to be
  15.9 m/s.
- **★★ `core.autocrlf` CONVERTS ON CHECKOUT, NOT ON WRITE**, so a test that reads
  source text in binary mode and matches a pattern SPANNING A LINE BREAK is green
  only in the tree that WROTE the file, and red in every fresh checkout. Fifth
  instance of the family `.gitattributes` documents four times. Fix at the READ (one
  `\r` strip), not with another `eol=lf` pin — a pin is per-instance and cannot even
  repair an already-checked-out tree. ⚠ MSYS `grep` / `cat -A` / `sed` text-mode
  STRIP CRs and will confidently report a CRLF file as clean; byte-read or it is not
  evidence.
- **★ COUNT YOUR CORPUS.** 116 `sled_tape_*` files are **44 distinct drives** — one
  content appears 23 times. Every corpus claim, and the calibration behind a shipped
  dial, must use the distinct count.
- **★ ATTRIBUTE A RED, DO NOT ARGUE IT AWAY.** A sixth failure appeared after merging
  another lane. "My changes are render-side and ship off" is an argument, not a
  measurement — reverting the whole source surface to `origin/main`'s inside the
  merged tree and re-running proved it arrived from main. Match reds on NAME, never
  index: indices move whenever tests are added.
- **★ A FELT SPEC CAN INVERT A DIAGNOSIS.** Superman was hunted in free fall for a
  whole session. Chad's ruling — *"i mean the buck ... he may freefall but during
  that he would pull himself back to the seat"* — made the pose spring the RIDER, not
  the defect. Softening it, which is where the work was heading, would have deleted
  the behaviour he was asking for. **Ask which regime he means before optimising
  inside one.**
- **★★★ A CTEST COUNT CERTIFIES THAT THE TESTS BUILT, NEVER THAT THE GAME DID.**
  `sandbox/r4a-grip` was handed over with a published gate of 1731/1737 and the
  `seads` target had never once compiled on it: a main-merge took main's
  `app/main.cpp`, which assigns `info.cosmetic_projectiles`, alongside our side's
  `render/draw.h`, which had no such member (the S3-GUNS feature landed after the
  branch forked). No test TU links `app/main.cpp`, so ctest was structurally blind
  and every leg stayed green. Two folds. (i) THE HARNESS GATE WOULD HAVE CAUGHT IT
  — `.claude/hooks/gate.sh` builds every target before it runs ctest — so a
  published ctest count that no gate produced is not a gate result. Run the hook;
  when you run the steps by hand, read the BUILD's own exit code, because
  `cmake --build … | tail -5` hands you the tail's zero over the compiler's failure. (ii) A
  clean automatic merge is not a correct one: git merged two halves of one feature
  that disagreed, and the merge commit's own message truthfully listed "no conflict
  in app/". Conflict-free is not consistent. (R4a grip release, 2026-08-30)
- **★★ A MEAN OF LENGTHS CANNOT ANSWER A QUESTION ABOUT DIRECTION**, and it will
  answer confidently anyway. The superman rig had measured the body's excursion as
  `dmag`, the mean DISTANCE of the stations from the pose — a quantity in which the
  lateral question ("falling off to the side") is not merely unanswered but
  unrepresentable. Adding the same displacements averaged as VECTORS on the body
  axes took four lines and turned an assumption into a measurement. When a ruling
  names a DIRECTION, check that the instrument carries one before reporting on it.
  (R4a grip release, 2026-08-30)
- **★★★ MEASURE THE REGIME THE MECHANISM WILL FIRE IN, NOT THE POOLED CORPUS.**
  Over all 84 airborne windows the chain's touchdown excursion is 27% lateral at
  the median and its side is coherent (91.5% survive from peak to landing) and
  weakly predicted by the buck's own lateral push (66% against a 51% lagged
  control). Every one of those numbers is TRUE and none of them is about the
  release: in the 8 windows that exceed the grip capacity he is 1.25 m UP, 0.45 m
  AFT and 0.067 m LATERAL, and no predictor beats a coin at n = 8. A pooled
  statistic that is dominated by the ordinary case says nothing about the rare one
  the feature exists for — the same shape as this ladder's SUSTAIN defect, which no
  displacement percentile could see either. (R4a grip release, 2026-08-30)
- **★ COUNT THE CORPUS AGAIN, IT GROWS.** The 116 tape files that were 44 distinct
  drives are now 157 files and 45 distinct drives; he keeps driving. A corpus count
  is a measurement with a date on it, not a constant. (R4a grip release, 2026-08-30)
- **★★★ AN INERTNESS LEG MUST PROVE THE MECHANISM FIRED, OR IT IS TESTING TWO DEAD
  MACHINES.** `sled_the_grip_law_moves_no_golden` drives two machines that differ
  only in the grip dials and requires every dynamic field to agree bit for bit —
  the executable form of §7.7's "ship it so it cannot fire, prove the goldens
  unmoved". Its first draft set the live arm's capacity to 1.0 and asserted only
  `load > 0`. On a flat ball |g_eff| never leaves ~9.81, so the extension stayed
  near zero, the arm NEVER RELEASED, and a deliberate mutation that made the kernel
  read `attached` and scale velocity by 0.999999 **passed**. The fix was to the
  LEG, not the mutation: capacity 0 so any load releases, and
  `REQUIRE_FALSE(attached)` so the release is asserted rather than hoped for. The
  mutation is caught now. Same class as this ladder's four "a test that could not
  fail" traps: an A/B that proves equality has to prove the two arms were
  DIFFERENT first. (R4a grip release, 2026-08-30)
- **★★ A TEST THAT INHERITS A DEFAULT IS A TEST ABOUT TODAY'S DEFAULT.** Two legs
  written against `sim::GripParams`'s then-zero `unseat_gain` went red the hour the
  calibrated 0.0669 became the shipped default — correctly, and the fix was to state
  the OFF arm out loud (`gp.unseat_gain = 0.0`) rather than to inherit it. A leg
  whose subject is "the mechanism is off" must SET it off; otherwise its subject is
  really "the default happens to be off", which is a different and much weaker
  claim, and the day the default moves it either lies or breaks. (R4a grip release,
  2026-08-30)
- **★★★ HALF A TRIPWIRE IS WORSE THAN NONE, BECAUSE IT READS AS COVERED.** The R4a
  grip shipped its OFF-by-absence lines in the tape loader while its dials were
  absent from `SLEDTAPE_PARAMS_D` — so no tape could record them and no tape could
  turn them back on, and the `dial_gap` warning (computed over the COMFORT roster
  only) could never name them. Every prior OFF-by-absence dial IS in a roster; that
  is the half that makes the law work. A new dial needs BOTH halves — the roster
  entry and the OFF line — and if it lives on `SledParams` rather than `SledComfort`,
  check that the gap machinery actually reaches it. (R4a grip release, 2026-08-31)
- **★★★ A THRESHOLD READ OFF A SUBSAMPLE IS RIGHT ONLY BY LUCK.** The grip capacity
  was first read off the kernel state the tape replay hands back once per TICK, while
  the kernel steps the grip every SUBSTEP — 1 sample in 12, of a quantity whose whole
  meaning is its PEAKS. The probe's own comment had written the warning and the
  header read a capacity off that column anyway. Fix: the kernel writes the quantity
  into the write-only `SledDebugSubstep` sink and the instrument reads THAT. The
  correction happened to be small (75.72 → 78.34); the method was wrong either way.
  ★ Corollary: when an instrument warns about itself, the warning is a TODO, not a
  disclaimer. (R4a grip release, 2026-08-31)
- **★★★ GRADE THE MECHANISM ON THE WINDOWS *IT* SELECTS, NOT THE ONES THE PROTOTYPE
  DID.** "The release cannot inherit a side" was measured on the 8 windows the RENDER
  CHAIN's product exceeded 57.4 in — but the shipped kernel latches on a different
  set (6 of 10 overlap). Re-measured on the kernel's own windows the conclusion
  partly REVERSED: the buck's lateral push predicts the side 8 of 10, where the chain
  set said "no driver beats a coin at n=8". A selection rule is part of the
  measurement, and changing the mechanism changes the selection. (R4a grip release,
  2026-08-31)
- **★★ A MEDIAN OF ABSOLUTE VALUES CANNOT TELL YOU A DIRECTION, AND WILL LOOK LIKE IT
  DID.** "He is thrown 0.45 m AFT at the release" was a median of |aft|; the signed
  values were 6 of 8 NEGATIVE — forward, over the bars — and 8 of 10 on the kernel's
  own windows. The wrong direction was written into the ladder as spec. Whenever a
  quantity has a sign that matters, report the SIGN DISTRIBUTION beside the
  magnitude, never the magnitude alone. Same family: an axis that is rectified by
  construction (`up = mean(L)(1-cosθ)`, never negative) must not be used as the
  denominator of a "share" that is about direction. (R4a grip release, 2026-08-31)
- **★★ A SWEEP OVER A DIAL THE STATISTIC IS INVARIANT TO IS NOT FOUR MEASUREMENTS.**
  The grip's extension is exactly homogeneous in `unseat_gain`, and a positive scalar
  multiple preserves rank order identically — so the four rank correlations published
  across the calibration sweep were ONE statistic printed four times, their spread
  pure CSV print quantization. It was then read as corroborating the linearity it
  presupposes. Before quoting a sweep, ask which of its columns can even MOVE with
  the dial. (R4a grip release, 2026-08-31)
- **★★ A RIG WHOSE DEFAULT IS A CONFIGURATION NOBODY SHIPS ANSWERS QUESTIONS ABOUT A
  GAME THAT DOES NOT EXIST.** `tools/sled_probe.cpp` armed the KERNEL's arming memory
  automatically but left the RENDER chain's OFF unless a second env var was exported
  — so the two arms of the comparison the rig exists to make ran on different
  memories, and the published calibration reproduced only for someone who set a
  variable the recipe never named. Both arms now read one pair, defaulting to the
  SHIPPED, SIGNED values. (R4a grip release, 2026-08-31)
