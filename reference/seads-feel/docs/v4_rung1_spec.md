# V4 RUNG 1 SPEC — track instrument + aim-rate feedforward (S-aimff)

Program: `program.md` (kernel v4, branch `auto/kernel-v4`, worktree `D:\flight_sim2\seads-v4`).
Research basis: `docs/v4_research.md` §6 + candidate 1. Ask 1 of 4: "more of a responsive
elevator when aiming at enemies" — the nose LEADS the moving mouse instead of chasing its error.

## Mechanism

Aim-rate feedforward: `ω_des(pitch,yaw) += K_ff · ω_aim` — the aim vector's own mouse-induced
angular velocity fed as an additive rate demand. Acts ONLY while the aim moves; steady state
untouched ("feedforward cannot cause oscillation" — it adds no error-loop gain).

### The signal (ω_aim) — frame-rate independence is THE design constraint

The repo bans frame-quantized signals driving control (the S7-mouselevel / AT-9 class; see
`app/instructor_tick.h` TickInput::aim_gain_scale comment). Mouse deltas arrive per FRAME and
are consumed into the aim on the FIRST tick of the frame — a naive ω_aim = (tick rotation)/dt
is a frame-rate-dependent spike train. The fix (Betaflight RC-interpolation shape):

- `app::tick` computes the rotation it actually applied to the aim this tick (axis·angle,
  world frame — the quaternion delta of `apply_mouse`, which EXCLUDES parallel transport by
  construction). It reports this in `TickResult`.
- `app::step_frame` knows `res.ticks` (N) for the frame. The consuming tick's FF rate is
  `rotation/(N·sim_dt)`, and step_frame FORWARDS THE SAME ω_aim value into the TickInput of
  ticks 2..N of that frame (a smear/ZOH over the frame). Total FF demand integral =
  K_ff·(frame rotation) regardless of N → frame-rate independent in the integral sense.
  - Mechanically: TickInput gains `glm::dvec3 aim_rate_ff{0}` (rad/s, world) plus a flag or
    convention: on the consuming tick app::tick computes ω_aim itself from the applied
    rotation and a new `int frame_ticks = 1` TickInput field (step_frame sets N); on
    non-consuming ticks app::tick just uses the forwarded `aim_rate_ff`. Report the value in
    TickResult so step_frame can forward it. Keep it simple and PURE; document the seam.
- `control::Input` gains `glm::dvec3 aim_rate_world{0.0}` — the mouse-induced angular
  velocity of the aim THIS tick. Defaults 0 ⇒ every existing caller/test/golden feeds 0 ⇒
  FF is structurally inert there (defense in depth beyond the gain gate).
- Controller-side: optional first-order low-pass at fixed sim_dt (`Internal.aim_rate_filt`,
  new dvec3 field) with `aim_ff_tau` (0 = no filter, pass-through — bit-identical arm; the
  smear already de-spikes ≥1-tick frames, the filter is a tuning refinement). Reseed/zero
  on GROUNDED reset (control::reset()).

### The insertion (control/controller.cpp)

In the cascade's POINTING branch (`ns.pursuit && !ns.deadzoned`), after `pitch`/`yaw` are
computed but BEFORE the AoA-protection + G floor/ceiling clamp sequence and inside the
yaw ceiling:

- Rotate the (filtered) aim rate into body: `aim_rate_body = sim::body_dir_of(orientation,
  aim_rate_filt_world)`.
- Gated ADD (the S-dampff / S-wvane ±0.0 discipline — at gain 0 the expression tree is the
  bit-identical legacy one):
  `if (cp.aim_ff_gain > 0) { pitch += cp.aim_ff_gain * aim_rate_body.x;
                             yaw   += cp.aim_ff_gain * aim_rate_body.y; }`
- Pitch then passes through the EXISTING 4-step AoA/G clamp sequence (protection bounds the
  FF — never bypass). Yaw: clamp the summed yaw to ±cp.yaw_max AFTER the add (the "highest
  deflection ALWAYS keyboard" shared-ceiling rule; today's yaw is bounded inside sqrt_law,
  the FF add must not escape the ceiling). Do this clamp ONLY under the gain gate so the
  gain-0 tree is untouched.
- NO roll FF (roll demand is bank geometry, not aim pursuit). NOT added in BALLISTIC (the
  attitude-hold branch is untouched). Override: the per-axis overwrite happens AFTER the
  pointing block, so a held axis discards the FF by construction — pin it with a test, don't
  re-derive.
- Deadzone: no special casing — a moving hand unlatches the deadzone instantly (aim-motion
  gate), so FF rides the live pointing; at rest ω_aim = 0 anyway.

### Config (config/controller.toml, new [aim_ff] section) + params + loader

- `gain = 0.3`   # [-] ω_des += gain·ω_aim; 0 = structurally OFF (bit-identical). Research:
                 # 0.3 start, PN framing N≤2 ⇒ stay ≤ 1.0; loader asserts 0 ≤ gain ≤ 2.
- `tau  = 0.01`  # [s] low-pass on the DERIVED rate (loop-shaping downstream of the raw aim —
                 # legal; the raw mouse→aim path is untouched). 0 = unfiltered.
- ControllerParams: `double aim_ff_gain = 0.0; double aim_ff_tau = 0.0;` (zero defaults —
  unloaded struct must fail inert, the params.h discipline). Loader reads them; add loader
  validation legs to the existing config test if one exists.
- The SHIPPED value is the fly value 0.3 (this branch is Chad's fly build; program license).

## The instrument (build FIRST, baseline BEFORE the mechanism)

New `seads_harness track` mode (test/harness/harness_main.cpp), the Q0 pattern:

`seads_harness track [axis] [pattern] [rate_dps] [V] [ticks]`
- axis: pitch | yaw (rotation axis = initial body_right / initial local_up-projected axes;
  the aim is rotated PER TICK by rate·dt about a world axis chosen at start — genuinely
  moving aim, not a set-jump).
- pattern:
  - `sweep`: constant-rate rotation (default 20°/s), after a 1 s settle. Metrics over the
    steady phase (skip the first 0.5 s of the sweep): mean |e| (the tracking lag error
    envelope), tracking lag ms = mean|e|/rate, peak |e|, slope-flip count of e (oscillation
    detector — MUST be ~0; rim-bounce during smooth tracking is trap #10).
  - `sine`: e sinusoidal aim, amplitude 10°, freq f (default 0.7 Hz): report phase lag [deg]
    and gain (nose amplitude / aim amplitude) via quadrature fit over whole cycles.
  - `reversal`: sweep at rate for 1.2 s, then INSTANT sign flip (the <100 ms 180° reversal
    trap probe): report the transient peak |e|, time to re-converge to the pre-flip envelope,
    and any anti-phase kick (nose rate opposing the new aim direction in the first 50 ms,
    the r>0 trap the memo warns about — should be absent for pure FF).
- Drives ClosedLoop with BOTH `aim_moved = true` and the new `aim_rate_world` set to the
  scripted rate while sweeping (ClosedLoop gains an `aim_rate_world` member forwarded into
  control::Input, default 0 — the moved-consumer discipline: one new field, one forwarding
  leg, one test leg).
- Print machine-greppable lines: `TRACK <axis> <pattern> <rate> <V> lag_ms=<..>
  mean_err_deg=<..> peak_err_deg=<..> flips=<..> phase_deg=<..> ...`
- Run the baseline (gain=0 via a config flip or a CLI override arg — prefer CLI arg
  `[gain_override]` so the baseline row needs no config edit; document that the arg exists
  for A/B only) at V ∈ {80, 140, 220}, pitch + yaw, sweep + sine + reversal. Record rows in
  `docs/v4_ledger.tsv` (baseline first, then gain=0.3).

## Tests (the repo trap classes, test/unit/ — follow existing test file conventions; Catch2
names must avoid `]` and `,`)

1. Fixture NON-no-op + kill: constant-rate moving aim (nonzero aim_rate_world, err held
   moderate), gain>0 vs gain=0 → ω_des pitch differs by ≈ gain·rate_body (REQUIRE the
   baseline term > eps first — the fixture-no-op killer).
2. Bit-identity at gain=0: a varied scenario INCLUDING a moving aim + nonzero
   aim_rate_world → Output bit-identical to a build-of-record path (compare against the
   same step with the field zeroed AND gain zeroed — both must match exactly).
3. Ceilings hold WITH FF: big aim rate + moderate error at low V → pitch ω_des within
   [w_min_pitch(+AoA floor), w_max_pitch(+AoA ceil)] bounds READ FROM CONFIG (config-relative
   bounds, never today's constants); |yaw ω_des| ≤ yaw_max + the coordination term's honest
   allowance (assert the POINTING+FF sum ≤ yaw_max by probing with β≈0).
4. BALLISTIC: below v_ballistic with aim_rate set → ω_des identical to aim_rate=0 (FF absent).
5. Override: axis held + aim_rate set → held axis's ω_des = override ramp value exactly
   (FF discarded by the overwrite).
6. Frame-rate integral leg (the AT-9 companion): drive app::step_frame with the same total
   mouse delta as 1×4-tick frame vs 4×1-tick frames (fixed state or short horizon) → the
   SUM over ticks of the controller-seen aim_rate_world·dt is EQUAL in both (the smear
   invariant). Use the app seam, not a re-derivation.
7. Moved-consumer legs: app::tick forwards the field (mouse applied → controller sees
   nonzero aim_rate; freelook held / grounded → sees zero); ClosedLoop forwards its member.
8. Filter: tau>0 → first-order step response of aim_rate_filt at fixed dt (one leg);
   GROUNDED resets the filter state.
9. Existing gates: AT-2 / AT-18 / ZOH loader checks stay green untouched. Goldens: must NOT
   move (all golden scenarios have aim_rate_world = 0 and the gain gate default-off in
   hand-built params; the SHIPPED toml gain=0.3 does not feed goldens because goldens load
   the toml... ⚠ CHECK: if controller goldens load config/controller.toml AND any golden
   scenario moves the aim, verify aim_rate stays 0 there — scripted SET-jumps supply no
   rate, so goldens stay unmoved even with the toml gain on. If a golden DOES move at
   gain=0.3, STOP and report — do not re-record without the parent session's sign-off.)

## Order of work

1. Instrument first: `track` mode + ClosedLoop aim_rate plumbing (controller untouched).
   Build + full ctest green. Run baseline rows (gain structurally absent = pure baseline),
   append to docs/v4_ledger.tsv with `iter=1.0 rung=track-baseline`.
2. Mechanism: params + loader + controller insertion + app::tick/step_frame smear seam.
3. Tests above. Full gate: `cmake --build build --config Debug && ctest --test-dir build -C
   Debug --output-on-failure` in D:\flight_sim2\seads-v4 (rm any locked seads_tests.exe
   first if a relink fails silently — the stale-binary lesson).
4. Re-run track at gain=0.3 (the toml value), append ledger rows `iter=1.1 rung=aim-ff`.
   Success spec: lag_ms materially down vs baseline at the same flips/reversal stability.
5. Do NOT commit — the parent session reviews, red-teams, and commits.

## Constraints (constitutional — violating any is a P0)

- NEVER touch sim/ (frozen plant). control/ + config + app/ + input/ + harness/tests only.
- The raw mouse→aim path is UNTOUCHED: no smoothing of the aim vector anywhere. The filter
  acts on the DERIVED rate demand only (downstream loop-shaping — the memo's explicit line).
- Angles radians internally; degrees only at the toml/CLI boundary.
- No bare numeric gains in control/ — everything through ControllerParams/toml.
- Every new gate/latch hysteretic (n/a here — FF is continuous, no latch; keep it that way).
- Comments: match the file's voice; explain constraints, not the diff.
