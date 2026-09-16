> **HISTORICAL (superseded 2026-07-08).** Every item below is CLOSED — landed or ruled: items 1-2 via S7-mouseloop/S7-cam3 (+ the S7-push gate rework), item 3 more-yaw via S7-yaw2 then MB-4/MB-rud, item 4 aim clamp DEFERRED/moot by Chad's ruling. The live feel loop is `docs/flight-log.md` + `docs/mission_b_instructor_plan.md`; the current thread map is CLAUDE.md `## Threads`.

# Section 7 — feel worklist (do one at a time, fresh context each)

> **FRESH AGENT (2026-07-07): the OPEN problem is the mouse-aim + camera FEEL through loops/split-S. Read
> `docs/mouse_loop_audit_handoff.md` FIRST** — it captures the problem, every DEAD-END (don't restack
> them), and the leads (the promising one: reconsider the CAMERA, not the mouse). Branch is now
> `sandbox/flight-tuning-2` (off `main`); gate **160/160**.

Branch: **`sandbox/flight-tuning`**. Gate: **158/158 green** as of 2026-07-06.

> **PROCESS (CLAUDE.md "Lessons learned"): ENTER PLAN MODE + ASK Chad explicit clarifying questions
> before any non-trivial flight-model/control/camera change. ONE DIAL AT A TIME, a test for EVERY
> change, and CHAD FLIES each one. "Level"=wings-level via ailerons/local-up (never pitch/platform).**

## === NEXT — Chad's dial list (2026-07-06, one at a time, a test each, HE FLIES) ===
Landed this session: F1 screen-relative mouse · F2 fold-safe upright roll · Item-2 lateral reversal
(world-horizon + sideways-cone push gate) · parabolic nose-seek · **NEXT-Item-2 freelook widen +
chase-cam lens shift + pole-flip fix (red-teamed, see below)**. All committed, gate 158/158. The
remaining dials Chad wants, IN HIS WORDS (confirm the exact meaning in PLAN MODE before coding each):
1. **Pure LOOP over the top — CAMERA half LANDED 2026-07-06 (S7-cam-zenith), AWAITING A FLY + red-team;
   the CORKSCREW half is still OPEN (control).** Chad's clarified ask: a pure straight-up loop must fly
   clean over the top, **zero roll, ends how it started**, camera following the aim over and the world flip
   deferred to ~past vertical; split-S/Immelmann keep their 180° roll (they work — untouched).
   **LANDED (camera):** at the zenith `render::ease_level_up` no longer FREEZES cam_up (which stuck the
   camera at the top, went singular in `project_dir` → the **doubled/glitchy reticle**, S7-cam2's cone-hold
   bug) — it now **parallel-transports cam_up over the pole** (prev_cam_fwd→cam_fwd swing, re-orthog) so the
   camera rolls SMOOTHLY over the top (Chad's ruling), staying orthonormal; horizon re-level resumes past
   the cone. Plus a `project_dir` degenerate guard (kills the double reticle). `prev_cam_fwd` threaded in
   `main.cpp` + harness `MiniCamera`. Pinned: `ease_level_up` transport + `project_dir` guard unit legs
   (test_camera, mutation-verified); loop/split-S over-the-top harness + ctest legs unmoved (gate 160/160).
   SPEC §0 S7-cam-zenith + §9.2 bullet updated. **STILL OPEN — the CORKSCREW is CONTROL-side, NOT the
   camera:** with the clean basis the scripted pure loop STILL banks ~67° (freeze==transport bank), because
   as the aim sweeps past vertical `target_body.y`<0 and `bank_error=atan2(x,y)`→180° commands a roll (the
   cascade reads over-the-top like a split-S). Chad's "no roll in a loop" needs a cascade fix: continue the
   loop by PITCH past vertical when the aim is in the plane's vertical plane (no lateral). This is the
   deferred §7 Item 1 corkscrew below — the NEXT (control) dial. **PENDING:** (1) Chad flies the camera fix
   (no stuck camera, no double reticle, smooth roll-over). (2) red-team S7-cam-zenith + S7-cam2 (HARNESS §6).
   (3) the control corkscrew dial.
   **S7-raw LANDED 2026-07-06 — AWAITING A FLY (this is the real fix for "mouse won't go over the top").**
   Chad flew the camera fix and reported the mouse-aim STALLS at ~vertical and retreats ("the mouse aim
   isn't raw then, is it"). DEAD-END first (reverted): a hybrid pole-free-near-zenith handoff (S7-polefree)
   — it broke the split-S (cone caught the nadir too), JUMPED at the hard base-switch, and reintroduced
   up-is-down inversion. Root cause (Chad nailed it): the F1 screen-relative mouse orients the delta by the
   EASED/LAGGED camera basis (a smoothed basis feeding mouse→aim = the RA9 ban), which degenerates at the
   zenith. FIX (Chad ruled): **remove F1, go RAW** — `apply_mouse` rotates the aim's OWN carried §9.1 frame
   (no camera coupling), which never poles. Result: a slow vertical mouse loops the aim ALL the way over and
   around, plane fully inverts, **0° bank (clean loop — no corkscrew at slow sweep!)**; the corkscrew was
   largely the F1 lateral injection (a fast flick still banks ~67°, the separate control dial). `app::tick`
   only; F1 test deleted, RAW slow-sweep clean-loop leg added (mutation-verified); no golden moved. Gate
   160/160. SPEC §0 S7-raw. **NEXT DIAL (Chad ruled in): gentle roll-to-local_up** — the raw carried frame's
   up drifts off local_up after a big maneuver (mouse left/right feels rotated); ease it back with a
   roll-ONLY correction about forward (never poles), gated so it self-levels at REST but does NOT fight an
   active loop. **PENDING:** (1) Chad flies S7-raw (slow mouse loops over cleanly; split-S still fine).
   (2) the gentle roll-to-local_up dial. (3) red-team (with S7-cam-zenith/S7-cam2).
   **THE CLEAN LOOP — designed whole (Chad ruled 2026-07-06), LANDED, AWAITING A FLY.** Chad flew S7-raw and
   found: (i) mouse goes OPPOSITE after a split-S, (ii) a loop won't complete — it IMMELMANNs (plane rolls
   upright at the top). Ruling: a loop STAYS inverted and keeps coming around while he holds up (pure pitch,
   never auto-rights; "if I stop the aim inverted it stays inverted; only rights near level"). TWO pieces:
   - **S7-loop-invert (Piece A, cascade):** re-gate the WINGS-LEVELING roll (auto-level held_bank decay +
     FINE/deadzone roll_hold) on `cos_phi_theta > 0` — REVERSES the unflown F2. Upright levels as before
     (unfold_bank kept, golden bit-unchanged); INVERTED emits ZERO roll → an in-plane loop rolls nothing and
     hangs inverted until the pilot rolls out by aiming. `roll_maneuver` + push bank-hold UNTOUCHED → lateral
     turns bank, DOWN split-S rolls through. Verified: held-up slow loop → cPT -1.00, stays inverted (phi 0).
     F2 belly-up test reworked to "stays inverted" (mutation-verified).
   - **S7-mouselevel (Piece B, aim frame):** `AimFrame::roll_toward_local_up` eases the frame's up to local_up
     by rolling about the aim's OWN forward (forward never moves → RA9, mirror bit-identical); disabled in the
     zenith cone; GATED on the mouse being IDLE (`[ui] aim_roll_idle` 0.15 s) so it re-levels AFTER a maneuver,
     never DURING a sweep (which curves the aim). `[ui] aim_roll_rate` 1.5/s. Pinned by unit legs.
   Gate 163/163, no golden moved. SPEC §0 S7-loop-invert + S7-mouselevel. **PENDING:** (1) Chad flies the
   whole loop (hold up → clean loop stays inverted & comes around, no Immelmann/roll; split-S still rolls;
   mouse re-levels after — no longer opposite; banked-upright rest still auto-levels). Tune
   `aim_roll_rate`/`aim_roll_idle`. (2) RED-TEAM the whole loop (HARNESS §6). (3) the fast-flick corkscrew
   (~67° at an aggressive sweep) is the remaining control residual.
2. **Freelook widen + chase-cam reticle centering — DONE 2026-07-06 (commits da1c371 + red-team
   d74e32a), RED-TEAMED + FLOWN + CONFIRMED by Chad (pole-flip fix "great, no longer pending").** Three
   things shipped: (a) freelook
   orbit YAW ±149°→±180° (swing fully to the front: straight behind / at the nose); PITCH widened +
   INVERTED (mouse up→camera down, Chad pref); clamp bounds are now `[ui]` config knobs
   `freelook_orbit_yaw_max`/`_pitch_max`. (b) A chase-cam VERTICAL LENS SHIFT so the resting reticle sits
   at SCREEN CENTER and the plane lower, WITHOUT rotating the camera (Chad: "same view angle") — pure
   off-center frustum (`render::lens_shift_ndc` + `off_center_frustum`, applied to the 3D scene AND the 2D
   reticle overlay, locked); `ChaseParams::distance 28→34` (zoom back). (c) Red-team P1 FIX: the
   camera-up flip pole is at `90-atan(height/distance)≈75°` NOT 90° (resting view already tilted down), so
   the pitch clamp is now ASYMMETRIC — overhead side auto-capped by `render::freelook_overhead_pitch_cap`
   (derived from chase geometry, ~70°), eye-below side to the config knob (89°). This is the flip Chad
   reported ("flips looking down, not up"). **CONFIRMED FLOWN — no flip; done.** (Residual: the main.cpp
   asymmetric-clamp WIRING is caller-glue, unpinned by ctest; the pure cap helper IS pinned.)
3. **Near-STALL straight-DOWN must NOT roll over.** At slow speed a PURE pitch-down (mouse straight
   down, no lateral) must nose down and dive — even if slow — WITHOUT the cascade triggering a rollover.
   A rollover is correct ONLY if there's ALSO lateral deflection > 22.5° (the sideways cone). Right now
   it rolls over too easily at low speed. Give the mouse authority to nose straight down at slow speed.
   (Likely the ballistic/low-speed path or the push gate at low V; the sideways-cone should already gate
   it — check why low speed still rolls.)
4. **Keyboard-override YAW top-end is TOO powerful (override ONLY).** A prior agent raised the yaw gains
   at the TOP END of the key press so the plane yaws way past its ability to coordinate. KEEP the
   feature that the cascade accepts SOME sideslip in maneuvering at high deflection — but cap the
   keyboard-override yaw ceiling back toward the flight model's real yaw authority. (Override yaw ceiling
   / `yaw_max` for the override path only — NOT the mouse cascade yaw.)
5. **More MOUSE PITCH authority + more pitch overall vs speed.** Increase the pitch the mouse aimer can
   command and the plane can achieve, scaled by speed (with proper high-speed compression). (Pitch is
   G-limit-clamped: `[g_limits] n_max`, `c_pitch`, and the compression `δ_max_eff(V)`.)
6. **Higher TOP SPEED + better ENERGY RETENTION.** The plane should reach a higher top speed and keep
   its energy. (Airframe: thrust `T_max`, drag `Cd0`/`k_induced` in `aircraft.toml`; AT-12 is the energy
   instrument.)
7. **PUNCHIER DIVE.** Pick up speed FASTER in a dive, zoom-climb LONGER, keep energy. (Drag/thrust/mass
   in `aircraft.toml`; read against the AT-12 energy printout + a dive scenario.)

## === (prior) turn-path / loop crux ===
Two live feel bugs + a new maneuver, all about the instructor choosing the RIGHT path to the aim:
1. **Can't LOOP or SPLIT-S with the MOUSE.** — **LANDED 2026-07-06 (S7-mouseloop) — AWAITING A FLY.**
   The REAL blocker was NOT the controller (a fixed over-the-top aim already loops — AT-15). It was the AIM
   MECHANICS: `input::AimFrame::level_up_to` (the S7-cam3 "screen-relative mouse", called each tick in
   `app::tick`) re-leveled the aim's mouse basis to the world horizon, which FLIPS the mouse PITCH axis at
   the zenith AND across the whole rear hemisphere (the horizon-up is antipodal to the carried up past half
   a loop — a hairy-ball singularity). So a mouse-up push PINNED the aim at ~90° elevation ("aim stuck,
   can't go past vertical") and the plane just flew straight up/down forever — mouse-only loops/split-S were
   impossible. Proven with a new mouse-driven harness instrument: `seads_harness mouseloop [up|down]
   [deflect_deg] [V] [ticks]` (drives the SHIPPED `app::tick`). **FIX (per Chad): eliminate the
   auto-leveling** — `level_up_to` REMOVED from the tick; the mouse basis is now the pure pole-free CARRIED
   §9.1 frame (transport + apply_mouse), so a sustained vertical mouse carries the aim cleanly over the top.
   Loop: aim sweeps past vertical, plane inverts (cPT→-0.97) and captures; split-S: rolls through inverted
   (cPT→-0.6) and pulls to upright. Pinned by two `app::tick` ctest legs (mouse-UP loop, mouse-DOWN
   split-S) — mutation-verified (re-adding `level_up_to` fails both). Gate 152/152.
   **ALSO REVERTED (per Chad):** the earlier `w_lat` "loop de-corkscrew" controller change — it made the
   near-vertical loop complete BY PITCH (suppressed the bank-to-turn roll, boosted the pull). Chad's ruling:
   NO auto-elevator leveling; the ONLY auto-rotation is BANKING; the plane must just chase the raw mouse aim
   naturally. So the controller is back to HEAD (golden un-moved), the plane rolls/banks to the aim as
   before. ~~**Trade-off accepted:** without `level_up_to` the aim can read "up-is-down" after a half-loop
   until the pilot rolls out (screen-relative mouse is gone).~~ **SUPERSEDED by F1 (below).** **PENDING:**
   (1) Chad flies the mouse loop/split-S. (2) The DEFERRED roll trigger (below) — the near-vertical pull
   still corkscrews / "won't roll at rest"; that's the next-session banking fix, NOT this one.

   **F1 — screen-relative mouse RESTORED (LANDED 2026-07-06, S7-screen-rel) — AWAITING A FLY.** The
   "up-is-down after a half-loop" trade-off above is GONE, WITHOUT resurrecting the broken `level_up_to`.
   `input::AimFrame::apply_mouse` gained a camera-basis overload: the mouse delta now rotates the aim about
   the RENDERED CAMERA screen basis (`r=cross(cam_fwd,cam_up)`, `u=cross(r,cam_fwd)`) instead of the aim
   frame's own carried up/right. The camera is horizon-locked (`render::ease_level_up`) so its right/up are
   screen-consistent at any attitude → mouse-up == up on screen, always. Crucially the aim FORWARD stays
   the free pole-free carried §9.1 vector (only the rotation AXES are camera-referenced), and the camera's
   cone-hold freezes the screen-right axis near vertical — so a sustained vertical mouse STILL sweeps over
   the top (this is why it is NOT the `level_up_to` failure). Zero-mouse is a strict no-op → RA9 holds,
   mirror-equivalence bit-identical (`pos_err==0`). `TickInput`/`FrameInput` carry `cam_fwd`/`cam_up`
   (main.cpp threads the previous frame's horizon-locked basis; one-tick lag negligible). Harness
   `mouseloop` + the two `app::tick` loop/split-S legs now drive the REAL camera basis (shared
   `harness::MiniCamera` calling the single-source `ease_chase_forward`/`ease_level_up`) — both still
   "FOLLOW THROUGH & CAPTURE". New `test_instructor_tick` leg: inverted carried frame + mouse-up climbs the
   aim toward SCREEN-up (mutation: zero the cam basis → frame-axis fallback → moves DOWN, fails).
   Gate 153/153. **PENDING:** Chad flies it; the §9.1 supersession still owes a red-team (HARNESS §6).
   **F2 — the plane won't stay upright / roll at rest (LANDED 2026-07-06, S7-upright) — AWAITING A FLY.**
   The "belly-up at rest" half of the roll-trigger bug is FIXED. Root cause: every automatic roll
   (rest auto-level, FINE wings-hold, deadzone rest-roll, push bank-hold) was GATED on `cos_phi_theta > 0`
   and measured bank with `e.phi = -asin(...)`, which FOLDS past 90 deg (its feedback sign inverts). So the
   ailerons QUIT when inverted — an Immelmann left you belly-up forever. Fix: a fold-safe bank
   `unfold_bank(phi, cos_phi_theta)` (controller.h) that sign-extends SPEC's phi past the fold — bit-EXACT
   to e.phi upright (so the controller golden does NOT move, even pitched) and monotone/correctly-signed
   through inversion. All four wings-hold sites now measure with it and the `cos_phi_theta > 0` gates are
   REMOVED (BALLISTIC alone keeps the upright-only path — out of scope). So a plane left inverted at rest
   ROLLS to upright (roll-only; pitch untouched). Pinned: AT-0 unfold_bank legs (upright bit-exact,
   inverted 180, the 120-deg fold both signs) + a closed-loop `control::step` leg (belly-up at 150 deg,
   aim on the nose, rolls to cos_phi_theta=1.0 with max|pitch|~0.01; mutation: restore the gate → stuck
   belly-up, fails). Controller golden UNCHANGED; AT-15 unmoved; AT-11 (a diagnostic that ARTIFICIALLY
   holds an inverted floor) re-pinned to hold its attitude against the new roll-to-upright. Gate 154/154.
   **PENDING:** Chad flies it; F2 (cascade roll + the fold-safe primitive) owes a red-team (HARNESS §6).
   **STILL DEFERRED — the near-vertical loop CORKSCREW** (a separate symptom of the same roll trigger,
   NOT fixed by F2's un-gating): the near-vertical pull still corkscrews. Do NOT fix by pitch. That is the
   banking-trigger / shortest-route work of Item 2 below.
2. **Lateral banking picks the WRONG way (THE CRUX).** — **STRUCTURAL FIX LANDED 2026-07-06 (S7-latrev) —
   AWAITING A FLY + roll-feel tune.** On a far-lateral aim (banked right, mouse hard LEFT) the plane stayed
   banked right and PITCHED the belly at the aim instead of rolling left. ROOT CAUSE (found with the new
   `seads_harness latflick` instrument): `push_mode` decided "down vs roll" from BODY-frame quantities
   (`bank_eff`, `target_body.z`) that are BANK-CONTAMINATED — a world-lateral aim projects body-BELOW when
   you're banked 80 deg (`tb.y=-0.93`, `bankErr=-172`), so push engaged and pushed the belly at it while
   holding the wrong bank. FIX: a WORLD-horizon gate on push (`[push_gate] horizon_enter/exit`, aim world-
   elevation `dot(aim, local_up)`, bank-INDEPENDENT) — push now engages ONLY when the aim is genuinely
   below the horizon (Chad's "never chase by elevator-down unless the mouse calls for nose-down"). A
   lateral flick (aim ~at the horizon) now ROLLS to the aim: `latflick` shows the reversal roll +79 deg ->
   through 0 -> -89 deg (left), push=0, pitch small and only pulling INTO the settled bank. Controller
   golden UNCHANGED, split-S still pushes/rolls-through (aim below horizon), AT-15/cascade push legs green.
   Gate 154/154. **REFINEMENT LANDED (S7-latrev2, Chad 2026-07-06):** the world-horizon gate alone still
   pushed a "down-AND-to-the-side" flick (aim below horizon but lateral). Added a SIDEWAYS-cone gate
   (`[push_gate] side_cone_enter/exit`): push (pure pitch-down, no rollover) is now confined to a cone
   around LOCAL-down — the aim must be within ~22.5 deg of the plane's own VERTICAL plane (nose+local_up,
   bank-independent). A flick "left and a bit down" now ROLLS over to track (latflick down=30: bank +79 ->
   -54 deg left descending turn, push=0); a near-straight-down FORWARD aim still pure-pitches (cascade/AT-15
   push legs green — those dives sit IN the vertical plane). Reference is LOCAL-up, not the banked body.
   **PENDING:** (1) Chad flies the reversal + the down-lateral rollover. (2) **ROLL FEEL** — Chad's ask
   "roll response FAST but the roll GENTLE, not a disorienting snap", and "quicker K_phi". NOTE: raising
   K_phi is COUPLED — the load check `K_w_roll >= 4*I_roll*K_phi` (critical damping) fails at K_phi=6 unless
   K_w_roll rises too, and that historically saturated the roll override (AT-8) + over-rolled the banked spawn
   (AT-13). So it is a multi-knob fly-and-tune, NOT a one-liner. Also the big reversal already SATURATES
   p_max (260 deg/s), so p_max DOWN = gentler peak; K_w_roll = faster rate-build (initiation). One knob per
   flight, Chad flies. (3) Item 1 near-vertical corkscrew still separate. (4) F2 + this owe a red-team.
3. **Banking response time + leveling WITHOUT ruining the dive.** Faster bank/level, but a straight-down
   dive must still just dive (don't flip) — the push_gate (S7-push) owns that; don't break it.
4. **Dive → quick 180° bank + Immelmann follow-through.** In a dive, if the mouse is in a direction MORE
   than the 90° straight-down attitude (i.e. past vertical, wanting to come back up the other way), the
   instructor should do a QUICK 180° bank and pull through the curve (an Immelmann/split-S-out), camera
   following the mouse, plane naturally pulling through. Only past-vertical; ≤90° straight-down still dives.
   (Related to Item 1 split-S + the push/roll gate.)

## Already landed this session (context for a fresh agent)

## Already landed this session (context for a fresh agent)
- **S7-ovr** — keyboard override is a snappy IN-ENVELOPE rate command (respects AoA/G), not the old ±1
  bypass.
- **S7-ovr2** — keyboard override does NOT touch the aim; the mouse owns the reticle/camera (a keypress
  no longer swings the view). On release the instructor flies back to the mouse aim.
- **S7-cam Phase 1** — the chase camera is decoupled from the aim: it rests behind the **velocity** vector
  and eases toward the aim with lag (`render::ease_chase_forward`, `[camera] lead/lag_base/lag_gain`).
- **S7-push** — push-vs-roll decided on GEOMETRY (`target_body.z` vs `[push_gate] down_enter/down_exit` =
  88°/96°): nose down (push) while the below-nose aim is ahead of the wing-line, roll (loop) once it goes
  behind. Turnover ~92°.
- **Pitch-rate tune** — `[g_limits] n_max 6→10`, `n_min −3→−4`, `[aoa] aoa_max 14→18`; `Cl_max 1.4→1.8`.
- **K_coord 2→3** (a first yaw bump).
- **S7-snap/yaw/perf (2026-07-05)** — a run of feel tunes: `K_theta 3.0→3.2` (small-deflection nose snap;
  capped at 3.2 — above ~3.3 the deadzone parking HUNTS, not critical damping); `[auto_level] rate 1.0→2.0`
  (level sooner); `yaw_scale 0.5→1.0` + `yaw_max 60→90` (nose follows the mouse in yaw — was halved).
  **PERFORMANCE:** `n_max 10→16`, `n_min −4→−8`, `integ_cap 0.5→2.0`, `aoa_max 18→20` — target 90° in ~4 s
  both ways; measured (step harness): UP 29°/s@200, 22°/s@300; DOWN 32°/s@200, 23°/s@250. n_min held at −8
  (−10 looped the airframe — unsustainable). **AT-11 floor bound RELAXED** 0.05→0.10·|n_min| (the deep −8
  floor isn't quasi-static — it drifts, so it droops ~0.6 g below the derived setpoint vs 0.15 g for the
  shallow −4 floor; mutation coverage preserved — setpoint mutations shift >1 g). Yaw MANEUVER-fade still
  pending (soften `(1-blend)` if hard-turn yaw lags). **RED-TEAM owed** for the AT-11 relaxation + all §7
  cascade-adjacent changes. Goldens re-recorded (controller only; aircraft.toml untouched).

Each item's felt symptom is Chad's; the diagnosis/knobs are the starting point — verify by flying, don't
trust blindly. SPEC changes need a §0 supersession + the AT rework + a red-team (HARNESS §6). Config-only
feel tweaks just need the gate + a fly.

---

## Item 1 — Split-S: the plane won't flip inverted past vertical  ⭐ (Chad's main one)
**Symptom:** doing a split-S by mouse (now that nose-down works), aiming down past ~90° does NOT flip the
plane inverted to continue the loop. "The plane not turning over 180 when reaching past 90° seems lost."

**Diagnosis (hypothesis — CONFIRM by flying + telemetry):** the S7-push gate rolls when the aim is BEHIND
the wing-line (`target_body.z > down_exit`), i.e. behind the *nose*, in body frame. But in closed loop the
push makes the **nose chase the aim down**, so the aim stays ~ahead of the (pitching) nose and `target_body.z`
never crosses `down_exit` — the plane just keeps pitching nose-low toward the aim and never rolls. A true
split-S aim is DOWN-AND-BEHIND (rear hemisphere); if the mouse can't drive the aim there (relative-delta
aim, no Phase-2 clamp yet, disorienting camera), or the push keeps the nose caught up, the roll never fires.

**Where to look:** `control/controller.cpp` push-vs-roll gate (~line 281, `target_body.z` vs
`push_down_z_enter/exit`) and the bank-to-turn `align = max(0,cos(bank_eff))`; the closed-loop dynamics of
push-chases-aim. Consider: should "past vertical → roll" key off the aim relative to **velocity/world-down**
(does the pilot want to go BELOW the horizon behind them?) rather than relative to the chasing nose? Or a
"commit to the loop" latch once the aim passes straight-down. Fly `seads_harness ctrl_fly` telemetry
(target_body, push_mode, phi, cos_phi_theta) through a scripted mouse split-S to see what z actually does.
Ties into Item 2 (the camera must flip with it) and possibly the deferred aim clamp (Item 4).

**Scope:** likely a cascade-logic change (SPEC §9.3 + AT-15 rework + red-team). Investigate first.

## Item 2 — Camera must roll 180° through an inverted maneuver
**DECISION (2026-07-05): the roll-with-the-plane idea is REJECTED. Camera stays HORIZON-LOCKED.**
- Attempt 1 slaved camera-up to the airframe body-up EVERY tick (`align_roll_to` in `app::tick`). Chad
  flew it → CAMERA INSTABILITY: the continuous 1:1 coupling rolled the view on every bank-to-turn AND on
  just looking around (body_up projected ⟂ aim.forward rolled the camera as the aim swung off-boresight).
  Fully reverted (gate back to 144/144, stable world-carried camera-up restored).
- Asked Chad to pick the trigger. He chose **"only re-level the horizon"** (keep camera-up locked to the
  world horizon, NEVER roll with the plane — the plane just appears inverted-in-a-level-frame at the top
  of a loop) + **"smooth / eased"** transition. So the original Item-2 ask (camera rolls 180° through
  inverted) is SUPERSEDED by Chad's own ruling: he prefers a stable, self-re-leveling horizon.
- **LANDED (2026-07-05, S7-cam2) — awaiting a fly.** Chad flew the reverted stable camera and still saw
  the horizon/plane sit off ("the plane is in the upper part of my screen the way the camera looks
  sometimes") and asked for the gentle active re-level. Implemented **`render::ease_level_up`**: camera-up
  eases toward the world horizon (`local_up` projected ⟂ the lagged chase-forward) at exponential
  `[camera] level_rate` (default 1.5/s), driven per-frame in `main.cpp`; `app::instructor_camera` now takes
  this `cam_up` instead of `aim.up()`. Render-only, downstream of mouse→aim (RA9 holds); the aim frame is
  untouched (still the raw §9.1 mouse basis, still carries holonomy for AT-14). Zenith-safe (holds cam_up
  when the view is along the radial). Pinned by 4 `ease_level_up` unit legs (test_camera) + the reworked
  `instructor_camera` forwarding leg. Gate 148/148. SPEC §0 S7-cam2 + §9.2 bullet rewritten.
  **PENDING:** (1) Chad flies it — tune `level_rate` up/down for feel (bigger = snappier re-level). (2)
  **RED-TEAM** the horizon-lock + zenith hold (HARNESS §6, adversarial-review). (3) The "plane high in the
  frame" note may be a separate FRAMING issue (eye height/pitch offset in `[camera] height`/`distance`),
  not roll — check after flying; if it persists it's a chase-offset tweak, not the re-level.

**Original symptom (now superseded by the decision above):** during the split-S / roll-through, "need the
camera to flip over 180 degrees" — the camera should roll with the plane so you see the world invert.

**Diagnosis:** camera-up is the carried aim-frame up (`aim.up()`, re-orthogonalized against the lagged
`cam_fwd` in `render::aim_chase_camera`). The aim frame is world-carried and does NOT roll when the
*airframe* rolls, so the camera stays ~upright through a roll-through — disorienting when inverted. Want:
the camera roll to track the airframe (or the velocity/lift-vector) through the loop.

**Where to look:** `render/camera.cpp` `aim_chase_camera` (the `up`/`right` build), `app/instructor_tick.h`
`instructor_camera`, and how camera-up is sourced. Careful: camera-up carries holonomy + zenith-avoidance
(AT-14); a naive "use body_up" reintroduces the pole and the RA9/holonomy invariants. Probably: roll the
camera toward the airframe's bank while keeping a zenith-safe up. Red-team camera-up (HARNESS §6).

**Scope:** camera design (caller/render side) + SPEC §9.2 note + red-team. Depends on Item 1's roll firing.

## Item 2b — Smooth wings auto-level at rest  (LANDED 2026-07-05, S7-autolevel — awaiting a fly)
**Symptom/ask (Chad):** "a smooth autolevel for wings when flying straight and mouse is at rest."
**Landed:** while the pilot is NOT maneuvering (`err < blend_lo`) and upright (`cos_phi_theta > 0`),
`held_bank` decays exponentially toward 0 (`[auto_level] rate`, default 1.0/s) and the wings-hold eases the
wings LEVEL. Suspended by a keyboard override (pursuit=false), off inverted (the φ fold). The band is
`err < blend_lo` (not just the deadzone) — a residual bank keeps turning and sweeps the aim out of the tiny
deadzone, where a deadzone-only decay STALLS (~7°, observed + fixed). Starts from the captured bank, so a
banked spawn still fires no lurch (AT-13 preserved). `controller.cpp` (one decay + a deadzone-branch roll)
+ `[auto_level] rate`; pinned in `test_cascade` (levels 15°→0 at ~12°/s peak; an override suspends it).
**DECISION (Chad, confirmed): rest ALWAYS levels** (WT-style release-to-level) — you can no longer hold a
banked turn by resting; hold the aim off to the side to sustain a turn. This **superseded AT-16** (reworked:
sustained coordinated turn now holds the aim off-nose in MANEUVER; |β| check preserved) and **moved the
controller golden** (re-recorded, ticks 900/1200 — the "back to level" segment now auto-levels). Gate
150/150. SPEC §0 S7-autolevel logged. **PENDING:** (1) Chad flies it — tune `[auto_level] rate` (bigger =
quicker level). (2) **RED-TEAM** the deadzone roll + the `err<blend_lo` band (HARNESS §6).

## Item 3 — More yaw / skid  (LANDED 2026-07-06, S7-yaw2 — high-G turns + skid)
**RESOLVED.** Long saga (mis-scoped twice). Chad's final ruling: **keep high-G banked turns, add SKID
within the bank** (NOT flatten them — a bank cap made turns too low-G, 44°/1.5G, and was rejected). Landed:
`c_yaw 4→12` (yaw authority — the instructor drives the rudder onto the aim), `K_coord 3.0→1.0` (LESS
coordination = the banked turn slips → skid; β ~9° through roll-in, ~2° steady; 0.5 gives ~10° but breaks
AT-16 + Dutch-roll risk), plus `yaw_scale 1.2`, `yaw_maneuver_frac 0.8` (yaw pointing survives MANEUVER).
`K_phi 4→5` (faster bank to balance). The bank-cap (`maneuver_bank_max`, flat-turn) was fully removed.
AT-12/15/16 all green, goldens re-recorded. **TUNE:** lower K_coord for more skid. RED-TEAM owed for the
reduced coordination + yaw-fade. **The dead-end (for a fresh agent):** a bank cap flattens turns but they
go low-G (conflicts with the n_max high-G tune) — skid via coordination, not a bank cap.

## Item 3-OLD — More yaw: instructor should use a lot more of it  (superseded above)
**CORRECTION (2026-07-06):** Chad was measuring the INSTRUCTOR's rudder use in flight, NOT full deflection.
Cranking `c_yaw` (plant authority) 4→20 made RAW/full-deflection yaw twitchy ("way too much yaw") — the
wrong lever. **c_yaw REVERTED to 4.** Kept the yaw POINTING gains (yaw_scale 1.2, yaw_maneuver_frac 0.8,
yaw_max 90) — those are what make the instructor command the rudder. Measured (ctrl_fly, closed-loop
max-rate turn): instructor rudder `in_yaw ~0.15`, β~2.5° — it DOES use the rudder, but modestly, because
in a bank-to-turn the plane BANKS so the aim sits overhead (a pitch error, not a lateral/yaw error) and the
yaw-pointing has little to act on. **OPEN QUESTION for Chad:** does he want (a) flatter, skiddier turns
(nose yaws onto the aim, less bank — reduce bank-to-turn / weight yaw over roll), (b) more coordination
rudder in turns (K_coord up), or (c) the nose to point better at the aim (turn-rate, already raised)?
Awaiting his answer before touching it again.
**LANDED (cranked hard).** The yaw was **PLANT-capped** at ~23°/s (`c_yaw=4.0`) AND yaw POINTING faded to 0
in MANEUVER. Final: **`c_yaw 4→20`** (plant yaw authority, aircraft.toml — the raw-speed lever),
**`yaw_maneuver_frac 0→0.8`** (new `[coordination]` knob + cascade line `(1-(1-frac)*blend)` — yaw pointing
survives the turn), **`yaw_scale 0.5→1.2`**, **`yaw_max 60→150`**. Result: **~63°/s yaw at a 40° aim vs
11.5°/s (~5.5x)**. Trades in sideslip/skid (arcade nose-follow). **CEILINGS found the hard way:**
`yaw_maneuver_frac=1.0` (no fade) AND `yaw_scale>1.2` CORKSCREW the split-S (AT-15 budget_frac 0.015 at
1.2 → 0.998 at 2.0) — the fade's last 20% + capped yaw_scale keep the roll-through clean. For MORE yaw,
push `c_yaw` (plant speed), NOT yaw_scale/frac. Both goldens re-recorded; `test_override` "pursuit
suspended" leg reworked (live now yaws onto the aim → compares held to a coordination-only run). SPEC §0
**S7-yaw** owed + RED-TEAM the fade softening + the corkscrew ceiling.

**Original symptom:** "make the yaw go up a lot so that instructor uses more of that."

**Diagnosis/knobs:** in a hard turn the ONLY yaw is coordination `K_coord·(−β)`; the yaw POINTING term
`yaw_scale·sqrt_law(...)` is faded out by `(1−blend)` in MANEUVER. To use more yaw:
- `[coordination] K_coord` (now 3.0) → higher (4–5): more coordinating yaw in hard turns.
- `[coordination] yaw_scale` (now 0.5) → higher (0.8–1.0): more yaw pointing in FINE / on flicks.
- Optionally reduce the `(1−blend)` fade in `controller.cpp` so yaw pointing survives into MANEUVER
  (that's a small cascade change — more visible yaw-into-the-turn, at the cost of some sideslip).
Watch for Dutch-roll / yaw oscillation if K_coord goes too high (it's overdamped by design; one knob per
fly). Moves the controller golden (re-record).

**Scope:** mostly config (gate + fly, re-record controller golden). The blend-fade tweak is optional +
cascade-touching.

## Item 4 (deferred from S7-cam) — aim cursor clamp + self-centering
Still pending from the camera plan: clamp the aim to ~5/8 of the screen relative to the nose (Phase 2) and
the self-centering pull-back ∝ deflection (Phase 3). May be relevant to Item 1 (bounding/where the aim can
go). **DEFERRED AGAIN (2026-07-06, Chad):** the off-screen-reticle complaint ("mouse deflection zone bigger
than my screen") may be MOOT now that the camera catches the aim faster on big pulls (`[camera] lag_gain
4→9`, S7-yaw2 pass). Only revisit if, after flying, the reticle still leaves the screen on big throws
despite the faster follow. Lower priority.

---

### Re-record reminders when a change moves a golden
- `[g_limits]`/`[coordination]`/cascade change → `seads_harness ctrl_golden` → paste into
  `test/golden/controller_golden.h`.
- `config/aircraft.toml` (lift/plant) change → `seads_harness golden` → `test/golden/golden_flight.h`.
