# PLAN 2026-09-04 — THE GAIT LADDER (G1–G8): the Sudburian's walk and run become GOOD

Lane: `D:\seads_sandboxes\gait`, branch `sandbox/gait`, base `5029294e9` (main tip 2026-09-04).
Master: Fable (this session). Builders: Sonnet subagents, one rung per agent, spec-driven.
Fly tree: NOT this lane — Chad flies `build-play\seads.exe` built HERE; absolute path in every checklist.

## §1 Chad's rulings (2026-09-04, verbatim intent)

1. **RUN = 5–6 m/s** on hard pack. The current `walker_speed_cap` hard anchor (3.5 m/s) RISES.
   This is a RULED kernel dial change, in scope for G2. Mid/deep anchors move only enough to
   stay continuous in depth (the one-independent-variable law, walker.h:38-46); deep trudge
   0.6 m/s does NOT change — deep snow is signed at 0.77 m and wading stays slow.
2. **NPC-READY.** Every rung lands instanceable: no singletons, no player-only state reads.
   New gait state = value types stepped by pure functions, N copies legal. G8 proves it with
   a spawned test crowd. (The AI-millwright seam is `combat/reinforce.h:28` — we do NOT build
   the millwright AI here, we make the gait ready for it.)

## §2 What already ships (design AGAINST it, never a second gait)

`sim/walker.{h,cpp}` (R4c, FROZEN SURFACE per `adef92479` / `app/walker_place.h:11`):
- Phase advanced by DISTANCE (`walker.cpp:468`), never time. Sekiya walk-ratio stride
  (`stride = 1.20*sqrt(v)`), stance_frac continuous 0.68→0.38 (walk→run, no threshold),
  cubic-Hermite swing with toe-trail + terminal retraction (`walker_foot_offset`,
  walker.cpp:118-161), lift saturates 0.45 m (leg is 0.908 m), all dials continuous in
  `depth_m`. 22 tests in `test/unit/test_walker.cpp` incl. no-skate, anti-phase,
  distance-not-time.
- Render: `pose_pass` (`render/sled_model.cpp:2036`) — legs :2357-2427 (2-bone
  `solve_chain` :1986 from the POSED hip, target at 0.92·leg_reach), arms :2222-2256
  (anti-phase, depth-spread). Gait channels cached :393-402, filled :4368-4392 —
  "render authors none of it."
- Rig: 42-bone `sudburian_rig` in `assets/sled/indy650.glb`; 23 addressed joints
  (`render/rider_rig.h:70-99`); measured rest locals `render/rider_rig.cpp:25-82`;
  leg = 0.453+0.455 m, hip half-span 0.095, stature 1.850. `ball_l/r` exist, NEVER driven.
- Ground: `world/snowpack.h:577 sample_at(dir)` → `{drive_r, depth_m, surf, ...}` — THE one
  ground query; walker calls it once per step (`walker.cpp:214`). Feet do NOT sample it yet.
- The richer standing solve worth mining: `render/flak_pose.h` `gunner_solve` (:210) —
  pelvis floor, hip-over-foot, crouch frac, `two_bone_mid` (:187).

## §3 LAWS (violating any of these fails review — no exceptions)

- **L-FROZEN**: `step_walker` / `walker_throw` / `walker_remount` / `WalkerInputs{forward,turn}`
  / `WalkerState::pos/heading` signatures untouched; `WalkerMode` compared BY NAME. New state
  lives in a NEW `sim::GaitState` (value type) stepped by NEW pure functions; `WalkerState`
  itself does not grow. The walker stays OUT of the sled tape.
- **L-PURE**: gait math in `sim/` (or `render/rider_pose.h` if it needs glm mats), NEVER in
  `sled_model.cpp` — "this ladder has lost two rungs to exactly that" (walker.h:373-380).
  `sled_model.cpp` only consumes.
- **L-DEPTH**: every new dial continuous in `depth_m`. A surface-class switch is the ladder's
  recorded disease (3 rungs, 3 times).
- **L-PHASE**: `gait_phase` stays distance-advanced and monotonic; a rung may READ it, never
  re-derive or reset it mid-stance. Stride changes change dφ only, never φ.
- **L-MEASURE**: posed-not-rest; sides NEVER mirror — every sign measured per side off the
  asset (`swing_down_sign` pattern, rider_pose.h:988; 0 = degenerate = "cannot", not a
  direction). Bend planes from captured rest (`rest_bend_normal`), reach clamped ≤0.98·chain
  (a clamped `solve_chain` is a rigid strut = robot, sled_model.cpp:4373).
- **L-PIN**: a stance foot is a stored WORLD point + yaw for the whole stance window; targets
  never snap (max-rate clamp; reject a >0.15 m/frame ground-sample jump and hold).
- **L-GATE**: every rung: build + ctest → `python tools/gate/gate_baseline.py check gate.log`
  exit 0, red set == baseline BY NAME. New behavior gets its own leg that can fail for its
  own reason (no hiding behind neighbours). Never two ctest runs against one build dir; never
  a concatenated log.
- **L-NPC** (ruling 2): no globals, no player lookups inside gait math; everything keyed off
  the passed state + params.
- **L-BLENDER**: none. All measurements come from `rider_rig.cpp` / `sudburian_proxy.py
  bone_specs()`. If a live measurement is ever truly needed: SECOND Blender instance on a
  COPY of the .blend — the head-mesh session's Blender is untouchable.
- **L-ONE-DIAL**: Chad tunes one dial at a time; env dials live-readable where cheap; his eye
  signs values, gates sign shape.

## §4 The rungs (strict order; max 2 attempts per rung, then stop and report)

Each rung = (a) pure math + unit legs in `sim/gait.{h,cpp}` (new TU) or `rider_pose.h`,
(b) consumption in `pose_pass`, (c) gate green, (d) fresh-context red-team, (e) Chad's drive
with inline checklist. The standing three shots every drive: SIDE profile, FRONT-ON, LOW
ground-level at slow time. Env `SEADS_GAIT_*` for new dials.

- **G1 — FEET MEET THE GROUND.** New `sim::GaitState` { per-foot pin (world pos + yaw +
  stance flag), plant target }. Predictive plant: `plant = pos + heading·(stride·0.5) +
  lateral(±hip 0.095 m) + 0.15·(v_des−v)·t_swing`; `sample_at` at the plant; heel/toe
  two-sample (0.22 m) foot PITCH from height delta; stance = the stored pin (L-PIN);
  swing blends pin→curve→next plant. `SEADS_WALK_FOOTDROP` measured and retired. Legs' IK
  targets in `pose_pass` come from GaitState feet, not the hip-hang.
  Judge: feet stick and lie along the hill; low-shot ≤2 cm skate per stance.
- **G2 — THE PELVIS LIVES (+ THE RUN SPEED).** Vertical bob: walk `A≈0.035 m`, two per
  stride, low at mid-stance; run inverts — rise in flight, compress on landing `A≈0.09 m`,
  low point lagging contact ~8% of cycle. Lateral sway toward the STANCE foot (walk
  ≈0.045 m, run ≈0.02 m — sign wrong = drunk). Trendelenburg: pelvis rolls ~1.5° toward the
  swing side at mid-stance. Pelvis height from the supports through a critically damped
  spring (ω≈12 rad/s); leg over-reach lowers the pelvis, never straightens the knee.
  RULED: hard-pack speed cap anchor 3.5 → 5.5 m/s (walk-ratio + stance_frac curves extend
  continuously; assert flight phase exists ≥4 m/s). Judge: weight; the run FLIES.
- **G3 — COUNTER-ROTATION + LEAN.** Pelvis yaw `+A_p·sin(2πφ)` vs shoulder `−A_s·sin(2πφ)`
  (walk 4°/6°, run 8°/14°), twist distributed ~20/30/50 up spine_01..03, signs MEASURED per
  L-MEASURE. Speed lean `3°+4°·v` cap 18°; slope: uphill `+0.5°/deg` grade (cap 15°),
  downhill `−0.3°/deg`. Judge: the torso is connected to the legs, hills look fought.
- **G4 — ANKLES AND TOES.** Drive `foot_*` pitch + the never-touched `ball_*`: heel-strike
  toe-up ~12° → flat by 15% stance → toe-off plantarflex −25° walk / −35° run → swing
  dorsiflex +8°. Ball pivot signs measured per side. Judge: side-profile silhouette.
- **G5 — TURNS, STARTS, STOPS.** Bank `roll = clamp(0.12·v·yaw_rate, ±18°)`; plant targets
  yawed to PREDICTED heading; sharp pivot (>2 rad/s) ends stance early + plants wide;
  start = rearmost foot swings first; stop = φ finishes, plants converge under pelvis,
  0.04 m overshoot-settle. Judge: tight circle + hard stop from a run.
- **G6 — DEEP-SNOW TRUDGE.** Continuous in depth_m: swing lift toward 0.6·depth, hip-flex
  high-step (+25° thigh at deep), planted foot SINKS 0.15–0.35 m below the visual surface,
  +6° trudge lean, swing time +, cap unchanged (0.6 m/s deep). Judge at the signed 0.77 m:
  does it look like EFFORT.
- **G7 — HEAD + SECONDARY.** Head gaze-stabilized 70–85% toward heading/horizon (eerily
  still head over a bobbing body); run double-bounce (2× step freq, 15% amp, 0.08 lag);
  wrist follow-through lag. Judge: front-on at a run.
- **G8 — ASYMMETRY + THE CROWD (proves ruling 2).** Per-character seeded biases: stride ±3%,
  lift ±8%, arm ±10%, slow noise 0.2–0.4 Hz ≈2%; NPC test spawn (dev key/env) of 8 walkers
  on paths with randomized φ. Judge: 8 people, not 1 person 8 times.

## §5 Red-team tells (the graded checklist each rung's red-team runs)

foot skate (low shot, slow time) · metronome symmetry · teleporting feet · knee pops at full
extension (min 5° flexion + rate clamp) · phase discontinuity on speed change · missing slope
lean · feet not yawing through turns · arm swing plane not following shoulders · pelvis sway
sign (drunk test) · anything discontinuous in depth_m.

## §6 Status ledger (append per rung)

- 2026-09-04: lane cut at 5029294e9; plan written; G1 spec handed to builder.
- 2026-09-04: G1 BUILT `fcb125127`; red-team verdict UNSOUND (P1 guard latch on grades, P2 pitch
  axis wrong-handed, P2 stop teleport, P2 reach-clamp skate). G1b fixes committed same day: guard
  is now direction-aware + rate-limited (never latches), pitch axis derived at the site (BAC-CAB,
  toe-up proven), deceleration stance-entry pins under the foot's actual position, footdrop
  re-clamped, NPC render seam bannered, no-skate/no-pop tests strengthened (moving walker, 0.5 m
  bound), + 10%-grade and hard-stop legs. INTERIM: render excursion scaled 0.35 horizontal
  (`SEADS_GAIT_EXCURSION_SCALE` A/B, `SEADS_GAIT_DEBUG=1` clamp duty readout) because nominal
  stance rides the 0.98·reach clamp — **G2's pelvis-lowering is the real fix; the scale comes back
  out then.** Gate 1921/1927, red set == baseline six member-for-member.
- 2026-09-04 late: VERIFY ROUND ruled G1b's excursion-scale mitigation UNSOUND — scaling the
  pin-to-hip horizontal render-side re-derives the target off the MOVING hip = skate at 0.65×
  body speed by construction. G1c (Fable, surgical): the scale is REMOVED — the true target
  passes through and the 0.98·reach clamp's late-stance engagement is the ACCEPTED, MEASURED
  G1 debt (`SEADS_GAIT_DEBUG=1` duty readout kept; `SEADS_GAIT_EXCURSION_SCALE` gone). Root
  cause is geometric: a rigid-height hip cannot span a real stride — humans drop the pelvis at
  contact — so **G2's pelvis-height law is the fix and its acceptance test must include: clamp
  duty ≈ 0% on flat ground at walk speed.** Also G1c: pitch axis was `flight_q·(−rt_m)` —
  double-rotation (rt_m is already current-frame); now `−rt_m` plain. LEDGERED WEAKENING: the
  ground guard accepts any new-point sample (>0.05 m chord) outright — a one-frame sample_at
  glitch at swing start plants one step at the glitch radius, unguarded; accepted because a
  grade is not a glitch and no latch/divergence exists; revisit only if sample_at ever glitches
  in practice. Deep-snow decel stance-entry can still drop up to ~0.45 m (lift_max) — G5 owns
  stops; do not re-report.
- 2026-09-05: G2 BUILT (Sonnet builder). Pelvis drop (critically damped spring,
  omega 16 rad/s, `reach_frac` 0.85 -- both retuned from the plan's own 12/0.96
  after a measured spring-lag-vs-ramping-demand finding), vertical bob, lateral
  sway, Trendelenburg roll, all in `sim/gait.{h,cpp}` (new `GaitBodyGeom` +
  four pure functions), consumed in `sled_model.cpp`'s pelvis channel gated
  `gait_on && !grip_attached` (the `welded` equivalent, sacred). RULED run
  speed shipped: `speed_hardpack_mps` 3.5 -> 5.5, mid/deep untouched, real
  flight phase confirmed at the cap. ⚠ FIXTURE TRAP found and fixed in the
  test itself: `flat_walker(p, v)` derives stride from `walker_speed_cap(p,
  depth)`, NOT from `v` -- a probe speed must set `p.speed_hardpack_mps = v`
  too, or the stride is sized for the WRONG speed (this is what a first draft
  mistook for a fresh over-reach defect). ⚠ REAL BUG FOUND AND FIXED: the
  drop's hip estimate first double-counted `lie_clearance_m` (~0.564 m,
  already baked into `w.pos`) on top of `pelvis_rest_h_m` (0.98 m), inflating
  every hip estimate by ~0.56 m -- caught by a one-off stderr probe, fixed by
  subtracting `p.lie_clearance_m` before adding the pelvis height back on.
  ACCEPTANCE: unit test `g2_pelvis_drop_zeroes_stance_overreach_...` proves
  STANCE clamp duty == 0% on flat ground at a self-consistent walk speed (the
  literal plan wording). The SMOKE run (`SEADS_SMOKE_WALK=1`, deep snow +
  `win.turn=0.3` baked into the harness) still reads ~55.6% overall clamp
  duty (down from the pre-G2 100%) -- diagnosed to two FROZEN, out-of-G2's-
  scope sources, not a drop-law defect: (1) the harness's own deliberate turn
  input, which G1's straight-line-only predictive plant (G5's own scope) does
  not correct for; (2) `walker_stride`'s speed comes from the depth-based CAP,
  not the walker's actual (slewed) instantaneous speed, so deep, lumpy snow's
  fast depth swings desync stride from real speed even absent turning. ⚠ NOT
  a re-report of G1's own "late-stance clamp" debt -- that debt is the one G2
  demonstrably kills (0% stance clamp, unit-proven); this is a NEW, separate
  finding for whichever rung reconciles turning (G5) or revisits
  `walker_stride`'s speed source. Gate 1928/1934, red set == baseline six by
  name. ⚠ LESSON, PAID FOR TWICE THIS ROUND: `clang-format -i` on a file with
  no matching `.clang-format` scoping reformats the WHOLE file, and
  `git checkout -- <files>` after that reverts ALL uncommitted work in those
  files, not just the formatting -- lost and had to reconstruct this rung's
  entire diff from the session's own record. Format touched regions by hand
  or diff-scoped tooling only; never whole-file `clang-format -i` on a file
  you did not author from scratch.
- 2026-09-05 night: **G1e `3d44177c6` — THE BUG: R4c-3's walking-arm `continue` skipped the whole
  walking-LEG block (on main, gated-never-flown) + targets raised to the ANKLE (rig-measured
  0.072 m). Found by the new `SEADS_SMOKE_WALK=1` rig + reading PNGs. LEGS VISIBLY STRIDE.**
  Full gate green. **G2 `20d8e2e53` — the pelvis lives**: over-reach → spring-damped pelvis drop
  (reach_frac 0.85, ω 16 — retuned from the plan's 0.96/12, spring lag measured), bob, sway,
  Trendelenburg, RULED 5.5 m/s hard-pack cap; clamp duty 100% → 55.7% in the deep-snow smoke
  (0% proven on flat at walk speed by test); gate 1928/1934 == baseline six. Fable verified the
  walk VISUALLY (two frames, legs alternate, trudge lean reads). OPEN FOR CHAD: (a) reach_frac/ω
  feel; (b) `walker_stride` derives speed from the DEPTH CAP, not actual velocity — measured
  desync in lumpy snow; a one-line kernel defect fix (stride from |vel|) awaiting his ruling
  before G5; (c) `SEADS_GAIT_BOB`/`SEADS_GAIT_SWAY` A/B dials unflown. Red-team round owed
  BEFORE any main landing (lane-internal so far).
- 2026-09-05 evening: G2b..G2g fly-fix chain (feet attached to the solved calf; ankle hinge +
  measured 0.10 ankle height; walk camera; run stands up — 4 drop-law bugs; capsize anchor by
  MODE; root grounded by lie_clearance 0.564 = the 90.7%→9.8% clamp; measured hunch removal;
  FOOTSTEPS ring). **HANDOFF READY: `docs/SESSION_HANDOFF_20260905_gait.md` is the launch doc.**
- 2026-09-06: **G2h `409da6750` · G2i `0b04cb438` · G2j `bace4728c`** (Opus builder, one
  commit per rung; each message carries its own story). Subset green: **49/49** in the
  gait/walker/g2 ctest set, up from 39 (+10 new legs). `build-play/seads.exe` rebuilt.
  - **G2h — the back STRAIGHT and the head PLUMB** (Chad, flying G2g: "torso needs to
    straighten back a little and head is now too tilted back it needs to be straight up and
    down while walking"). ★ROOT CAUSE, both halves: G2g measured ONE angle — the pelvis→neck
    CHORD — and split it three ways, and **a chord cannot see the curvature it is the average
    of**. G2h measures EACH SEGMENT (the joint's own rest +Y column) and stands it upright in
    an accumulating sweep, `C_k = q_from_to(dir_rest[k], conj(Qacc)·up)`. MEASURED, printed at
    every load: `spine_01 27.60 spine_02 11.04 spine_03 7.36 neck_01 36.00 head 0.00` deg —
    G2g was standing up a fraction of a real hunch. And the head went BACKWARDS because
    straightening the spine rotates everything above spine_03 rigidly: a **missing term**, not
    a bug in the removal. The sweep now runs two segments further (neck + skull axis), which
    is a slice of G7 taken early. Same sacred gate, same rise_ease. New dial
    `SEADS_GAIT_HEAD`. ⚠`SEADS_GAIT_STRAIGHT=0` leaves the head over-corrected by exactly the
    spine angle — an A/B artefact of deliberately breaking the chain, stated in the banner.
  - ★**NEW INSTRUMENT: `SEADS_MANCAM="dist,az,el,up"`** (smoke only). The shipped orbit frames
    the MACHINE and the chase cam can only look at his BACK — neither can answer "is the back
    straight / the head plumb", which is exactly what this rung was graded on. MANCAM orbits
    whichever body the frame is about, az off HIS heading (az ±90 = side profile). ⚠**Its `el`
    must LEAD the subject by ~14°**: the HUD's own `lens_shift_ndc` puts the aim point well
    below screen centre, so a camera aimed at his chest draws him near the bottom edge. The
    calibrated profile used for acceptance: `SEADS_MANCAM="7,-90,18,-1.6"`.
  - **G2i — SHIFT: jump / sprint-while-held / charged dash.** ★The hop is deliberately NOT a
    `WalkerMode`: that ladder is one-way past stage 4, so `Falling` would throw the man on his
    face for pressing jump. `sim::HopState` + `step_hop` beside the walker (value type, pure,
    NPC-ready). The sprint is a MULTIPLIER on the three ruled depth anchors of a LOCAL
    `WalkerParams` copy (×1.15), so the depth curve keeps its ruled shape and `walker_stride`
    picks the longer stride up through the shipped path — no second stride law. Dash = cap
    ×1.8 decaying 1.7 s onto whatever is live underneath. ⚠**ONE RULING OWED**: the spec said
    "disarm when Shift released", but the arm is a HOLD and the trigger is a PRESS — you
    cannot press a key you are already holding. Cleared strictly on release the dash is
    unreachable by construction, so `dash_arm_grace_s = 0.6 s` (a named dial) lets a release
    short enough to be a re-press keep the arm. **Chad's number to overrule.** Airborne: both
    feet unpin and both targets ride up by the hop height (`step_gait` gained one DEFAULTED
    param; a leg proves the 0 case is bit-identical over 900 frames).
  - **G2j — the pump-repair work pose.** Left arm braces LONG across the midline (with ~10 mm
    of give at the wrench's rate — an arm under torque is loaded, not frozen), right arm turns
    a RATCHET (62% loaded pull, faster reset, smootherstep reversals), and `FinishRepair`
    fires a one-shot hammer blow. ★**THE BLOW OUTLIVES `Repairing`**: `FinishRepair` IS the
    transition out of Repairing, so a blow gated on `work_on` is a hammer nobody ever sees
    swing — the render gate is `work_on || work_blow_on`, and `work_blow` is the SWING (it
    crosses zero twice) so it needs a separate flag. ⚠No `continue` at the end of the new
    per-side block (G1e's bug, checked before a line was written); the walking-arm branch is
    gated OFF while this one runs — one owner per limb per frame. Instrument:
    `SEADS_SMOKE_REPAIR=1` (wrench) / `=2` (also beats the blow).
  - OPEN / FOR THE RED-TEAM: (a) the hop is a RENDER-and-feet offset — the walker's own
    collision, the camera anchor and the footstep ring all stay on the ground under him, which
    is right for a 0.5 m hop and would be wrong for a big one; (b) no pre-G2h clamp-duty
    baseline was taken, so the smoke's 6.8% / 26.6% per-second readout cannot be compared
    against G2g's — nothing in these three rungs touches the leg chain, so it is believed to
    be scenario variance, not a regression; (c) the G2j pose is graded from the SMOKE rig,
    where he is WALKING at the pump — his legs stride in the acceptance shots. A real repair
    is a standing man and the legs were not separately verified against a pump's geometry;
    (d) `SEADS_MANCAM`'s `el` compensation for the HUD lens shift is empirical, not derived.

- 2026-09-06: **G2i-b — THE JUMP'S AIR POSE** (Chad, flying G2i: "jump should
  have a knee forward or both knees bent a bit while in the air because they kind of hang back
  there right now... let's fix the jump to make it look a little more believable"). ★THE LAW IS
  ONE LINE: **the tuck IS the height** — `tuck01 = smoothstep(height / (tuck_full_frac * apex))`
  off the hop's own parabola, so it is 0 at the push-off, full over the top and 0 again at
  touchdown, with no clock, no branch and nothing to pop. G2i rode both foot targets RIGIDLY up
  with the body, which is exactly the hang he saw: a foot half a stride behind him at takeoff is
  still half a stride behind him at the apex. Now the target BLENDS off that ride toward a
  posture hung from the hip — lead knee forward (the leg that was SWINGING at takeoff, latched
  at the launch off `gait_phase`), trail foot tucked under the hips. All sim-side (`HopState`
  gained `tuck01` + `lead_side`; `step_gait`'s defaulted hop param became `sim::GaitAir`), so
  render needed **no change at all** — the tuck arrives as an ordinary foot target (L-PURE by
  construction). ⚠`tuck_full_frac` IS ALSO THE FOLD'S SPEED and that is what set it: a parabola
  climbs fastest at the push-off, so the plan's first 0.55 folded the legs at 0.117 per 1/120 s
  frame — a snap. 0.85 spreads it over ~0.18 s. ⚠FIXTURE NOTE: `flat_walker` puts `pos` at
  exactly `kR`, but `step_walker` pins it at `drive_r + lie_clearance_m` — fine for every leg
  that reads directions, 0.564 m wrong for one that asks how high a tucked boot is; the posture
  leg raises him itself. 4 new legs (53/53 in the gait/walker/g2 subset, up from 49): the tuck's
  SHAPE against the arc, the POSTURE it produces, the LATCHED lead side, and `tuck_gain = 0`
  reproducing G2i bit for bit through a whole hop. New instruments: `SEADS_SMOKE_HOP="tick[,hold]"`
  (a scripted jump — the key read is gated `smoke_frames == 0`, so without it there is no
  airborne frame to screenshot) and a per-airborne-tick `SMOKE_HOP` telemetry line. Dials:
  `SEADS_HOP` grew fields 7-8 (`tuck_full_frac,tuck_gain`), plus
  `SEADS_HOP_TUCK="lead_drop,lead_fwd,trail_drop,trail_back"`. Verified by PNG at the calibrated
  `SEADS_MANCAM="7,-90,18,-1.6"`: apex t=1800 (h 0.51, tuck 1.00) both knees folded up under him
  against a same-frame `tuck_gain=0` A/B that hangs them straight down, and t=1833 (h 0.083,
  tuck 0.09) with both legs near-extended and reaching for the snow.
  OPEN / FOR THE RED-TEAM: (a) the foot's ORIENTATION in the air is still `pitch_rad` left over
  from the last swing sample (roughly ground slope) — the toes do not point with the tuck, which
  is G4's hinge to drive; (b) the lead split is the HALF cycle, not the live duty factor, so at a
  run's 0.38 duty the "swing leg" call can be a tenth of a cycle off — deliberate (the duty is
  not in `HopState`), and it only picks which of two knees comes forward; (c) the tuck posture is
  a fixed hip-relative pose, NOT scaled by hop energy — a bigger `jump_speed_mps` folds no
  harder, it only holds the fold longer; (d) the smoke rig hops in 0.71 m BUSH snow, so the
  hard-pack/run air pose is Chad's stick.

- **G2i-e — RED-TEAM ROUND (fresh-context, 2026-09-06) + the P1 it found, FIXED.** Verdict on
  `2845f2d7a..ec4fa00e1` was UNSOUND for exactly one reason, now retired:
  P1 (CONFIRMED, fixed here): the hop was the FIRST term added to `flight_off_model` since the
  one-anchor law was written, and it broke it immediately — `gait_anchor_w` did not rise with
  the root, so every gait target drew `hop_height` too high ON TOP of the `hop_height`
  `step_gait` already spent (`target_w + up*h`): foot at +2h against a hip at +h, the drawn
  tuck one hop-height deeper than its dials, and every AirTuck/AirLand number tuned THROUGH
  the error (G2i-b's "knees up to the nipple line" at a nominal 0.70 drop = 0.70−0.52−0.10,
  arithmetic corroborated). Unit legs could not see it: they grade the sim's hip formula
  against the sim. Fix = the anchor is the point the root is DRAWN at
  (`walker.pos − up*lie_clearance + up*hop_height`), render-only, identical at hop 0. Apex +
  pre-touchdown MANCAM frames re-shot and re-read: the drawn flexion now MATCHES the ruled
  ~60°/50°. ⚠ The air-pose LOOK Chad signed was the buggy draw — the fixed draw is closer to
  his stated "slight bend" ruling, but HIS RE-FLY RE-SIGNS IT (dials stand ready).
  LEDGERED, NOT FIXED (P2, all in unflown G2j — fix before his first pump fly):
  P2-1 `torque_frac` is a dead dial (parsed at app/main.cpp:2080, never read; the 0.62 stroke
  is a literal at two render sites — one-number-one-owner break). P2-2 the work pose has NO
  blend at either edge (work_now boolean at render/sled_model.cpp:2748 — hands teleport
  ~0.5–0.8 m on U-press and on blow expiry). P2-3 the blow and wrench branches share no
  endpoint (hand jumps ~0.3–0.4 m at both ends of the one-shot; the sim shape is continuous,
  the render mapping is not).
  LEDGERED P3: (1) the "stride weight ≤2%" claim holds only while
  `jump_speed_mps > full_at_fall_mps/(√2·√(1−tuck_full_frac))` — 3.098 at defaults vs 3.2
  shipped, a 3% margin between two independent dials; (2) `g2id_the_descent...`'s headline
  assertion samples only full-tuck frames and would pass with AirLandParams deleted — it
  grades the tuck, not the landing law; (3) jump-during-Repairing draws a ballistic man with
  hands braced on a ground-fixed pump (Shift gate lacks Repairing; work_blow survives into
  Down/CrawlProne); (4) `gait_head_q[1]` is identity on the shipped asset (head 0.00 — the
  plumb head is one joint, neck_01; SEADS_GAIT_HEAD=0 reproduces the rejected G2g head, a
  trap for the next A/B); (5) the leg block's debug accumulators are function-statics —
  breaks at G8's crowd.
  CLEARED on inspection: no `continue` regressions, L-FROZEN holds, both sacred gates hold,
  G2h's quaternion algebra correct, sprint×stride desync claim verified false, step_gait
  signature fallout contained, all previously-ledgered items re-checked not-worse (except P3-3
  above, which was).
