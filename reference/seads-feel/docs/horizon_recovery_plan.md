> **LANDED (2026-07-07).** S7-hrz + S7-nest were implemented, flown ("totally perfect"), and merged — see §11/§12. The "Status: PLANNED" line below is the frozen plan-time snapshot; this doc is now the design record.

# Horizon recovery — righting the world after a split-S (plan, 2026-07-07)

*Status: PLANNED, nothing implemented. Consulted with Chad 2026-07-07; this doc is the
execution plan. REVISED same day, three times: (1) Chad ruled the trigger is the
FREELOOK RELEASE (D6) — supersedes the original manual-key/auto-gate try order;
(2) a companion QoL change folded in (§5b, D7/D8): freelook keyboard-flight aim
NESTING + release aim := velocity — same release moment, SEPARATE dial and flight;
(3) **RED-TEAMED** by a fresh-context Fable 5 consult (verdict SOUND-WITH-FIXES, §10)
— the load-bearing gauge-move premise HELD, but the roll mechanism was restructured
from a per-tick chase of a recomputed local-up target to an OPEN-LOOP FIXED-ANGLE
roll captured at release (F1, the P0), plus P1 fixes to §5b's up-pole and Method 2.
Branch context: `sandbox/flight-tuning-2`; S7-cam3 and S8-drone are now COMMITTED
(gate 185/185), so the target drone exists. Read
`docs/milestone_feel_bug_attribution.md` and the `dogfight-systems` skill first.*

---

## 1. The problem (Chad's words, attributed)

> "When I perform a split-S the world is such that I am flying straight and level and
> the world is above me. Previously the orientation flipped naturally, but that made my
> mouse aim directions opposite after the maneuver."

**Attribution (do not re-litigate — this is the S7-cam3 milestone):** after a split-S
the *plane* exits wings-level UPRIGHT. What is inverted is the **aim/camera frame**
(`input::AimFrame`), which pitched 180° through the bottom of the half-loop and now
carries its up pointing at the ground. Because camera-up = the carried frame's up
(S7-cam3), the pilot sees the world above him. This is the *accepted trade* of the
pole-free carried frame — the world stays inverted until the pilot rolls the aim back
over. The old "flip naturally" behavior (horizon-locked camera) inverted the mouse
because the camera and the mouse basis were **two different frames** that diverged
through the loop.

## 2. The insight that makes a fix possible now (the whole plan rests on this)

The mouse basis and the camera are now **ONE quaternion** (`loop.aim`): `apply_mouse`
rotates about the frame's own axes (`app/instructor_tick.h:207`, 3-arg overload) and
the camera renders from the same frame (`app/main.cpp:125/:289`,
`cam_up = loop.aim.up()`). Therefore:

- **A roll of the frame about its own `forward()` axis is a GAUGE move.** It does not
  move the aim direction — the instructor (`ci.target_dir_world = aim.forward()`)
  literally cannot see it. No control transient, no trajectory change, no golden risk
  from the roll itself.
- **The mouse cannot INVERT through the roll, because the mouse basis and the screen
  basis share the same UP vector** (`aim.up()` feeds both). Honest form (red-team F6):
  since S7-cam Phase 1 the two bases are NOT identical at every instant — the rendered
  screen basis re-orthogonalizes `aim.up()` against the LAGGED `cam_fwd`
  (`render/camera.cpp`), so they differ by O(δ), δ = angle(aim forward, camera
  forward). That divergence is bounded, pre-existing (it IS today's accepted reticle
  float), has no inversion mechanism (shared up), and is LARGEST exactly at the
  release moment (a D8 aim jump opens δ while `lag_base = 0.2/s` closes it over ~1 s)
  — expect to FEEL the ordinary float there and do not mis-attribute it to the roll.
  Chad's head-rotation-device metaphor still holds: what you see and how your hand
  maps onto it move *together*.
- The old inversion bug is only reproducible by rolling ONE of the pair (e.g. a
  camera-only variable in `main.cpp`). **The righting rotation must be applied to
  `loop.aim` — never to a render-side copy.**

The frame's 3 rotational DOF split: 2 are the aim direction (stay PURE RAW, untouched,
forever — Chad's ruling holds), 1 is the roll about it (the gauge DOF). Horizon
recovery steers ONLY the gauge DOF back toward local-up. The primitive already exists
and is zenith-guarded: `AimFrame::level_up_to` (`input/aim_frame.h:130`, currently
vestigial).

## 3. Standing rulings this touches (must be superseded EXPLICITLY, not silently)

1. **"The mouse is PURE RAW / no auto-leveling of ANYTHING"** (eb265f3, CLAUDE.md
   Learned, S7-mouselevel). An automatic frame roll modifies the frame that feeds
   `apply_mouse`. Scope of the supersession: the aim DIRECTION still never auto-moves;
   only the roll-about-aim gauge DOF is steered. Needs Chad's explicit ruling (he asked
   for this — 2026-07-07) + a SPEC §0 supersession entry when it ships.
2. **Why this is NOT the S7-mouselevel dead-end restacked — the honest version
   (rewritten after red-team F1 refuted the first version):** `roll_toward_local_up`
   converged the mouse basis toward a SEPARATE horizon-locked camera; with the shared
   frame that mouse-vs-screen disagreement is structurally impossible. But the first
   draft's claim "`local_up` depends on position, so no feedback loop can form" was
   WRONG for a per-tick roll toward a recomputed target: the roll's target
   `P_⟂forward(local_up)` moves with the mouse-driven `forward`, closing exactly the
   "per-tick modification of a control-driving quaternion" loop the S7-mouselevel
   lesson bans — and near the zenith the projected target FLIPS ~180° across the pole,
   slewing the basis at full `rate` under an active sweep. **The fix is structural:
   the roll is OPEN-LOOP — capture the misalignment angle and direction ONCE at the
   release edge, then roll that FIXED angle about the frame's own forward with the D3
   profile.** No target to chase ⇒ no feedback path, no pole-flip, no per-tick gate to
   chatter, and termination by construction. The residual cost that remains real:
   while the roll is active, a screen-straight mouse drag traces a slightly curved
   path in the WORLD (the axes rotate under the drag; on screen it still tracks the
   cursor), and a pilot who maneuvers mid-roll ends NEAR upright, not exactly — the
   next freelook tap trims the remainder. Accepted trades, flown for.
3. **"Inverted stays inverted"** (S7-loop-invert `wings_level_band`; `[auto_level]` is
   OFF when inverted). Untouched by this plan for the split-S case (the plane exits
   upright). If Chad also wants the PLANE to auto-roll upright from inverted-at-rest
   (post-Immelmann), that is a SEPARATE dial (extending `[auto_level]` past the fold),
   a separate flight, and its own supersession — do not bundle it. See Decision D4.
4. **The §9.2/AT-14 camera-up contract** (red-team F7): CLAUDE.md's sphere invariants
   name the carried frame's up as the sole frame-carried exception, "differs from
   local_up only by real holonomy." The recovery roll deliberately DISCARDS carried
   holonomy at each firing, so the letter of that clause breaks — silently, because
   the unit AT-14 pins exercise transport (untouched; the roll is a new op), not the
   contract sentence. The SPEC §0 entry must supersede this clause explicitly: after
   this lands, camera-up differs from local_up by (real holonomy − recovered angle).

## 4. Decision ledger (Chad rules; defaults proposed)

| # | Decision | Status / default |
|---|---|---|
| D6 | **Trigger = FREELOOK RELEASE** (RULED, Chad 2026-07-07): the recovery roll arms on the freelook press and runs when the key is released, folded into the camera's ease-back settle. Players naturally freelook with mouse-aim, so the world rights itself as a consequence of a player action — never autonomously. | RULED. Supersedes D1/D2. |
| D1 | ~~Manual key first, or straight to automatic?~~ | Superseded by D6. A dedicated key survives only as the Method-3 escape hatch. |
| D2 | ~~Discrete snap+ease or continuous slow roll?~~ | RULED with D3 (Chad 2026-07-07): neither extreme — "snap to gentle settle." |
| D3 | **Motion profile** (RULED, Chad 2026-07-07): *"not too much of a snap, just a quick uniform movement that is eased at the end of settling in the new pilot view orientation about the velocity vector."* | Shape (REVISED per red-team F1/F2 — OPEN-LOOP): at release, capture θ₀ = misalignment angle and the roll SIGN once; then per tick `Δθ = min(rate, settle·θ_remaining)·dt` counts the CAPTURED angle down to 0 (constant `rate` while far, eased capture at the end, no overshoot, terminates by construction — the latch clears at θ_remaining == 0). Starting knobs: `rate = 150°/s`, `settle = 5 /s` (a full 180° recovery ≈ 1.0 s uniform + ~0.4 s ease ≈ 1.4 s). Both in `[horizon_recovery]`; Chad tunes by feel. The settle axis is the velocity vector via D8 (release aim := guarded v̂, roll about the new forward — the camera's natural rest). |
| D4 | Also roll the PLANE upright from inverted-at-rest? | DEFER — separate dial, reverses S7-loop-invert, not needed for the split-S (plane exits upright). |
| D5 | ~~Trigger only on big misalignment, or trim any drift?~~ | MOOT under D6: a rate-limited roll toward alignment is a no-op by construction when already aligned (a 2° trim at 45°/s lasts 40 ms, imperceptible), so no misalignment band or deadband is needed at all. |
| D7 | **Freelook keyboard-flight aim NESTING** (asked by Chad 2026-07-07, see §5b): while freelook is held AND override keys are active, the aim tracks the nose per tick (aim dot nested with the nose indicator). | PROPOSED scope: only `freelook && any_override`. Freelook WITHOUT keys keeps the held-turn behavior unchanged (the 4d lesson: unconditional reset makes the held turn a surprise). Chad to confirm scope — INCLUDING the partial-override nuance (red-team F11): under a roll-ONLY override, per-tick nesting nulls the pitch/yaw pointing every tick (those axes hold trim) where today they chase the once-snapped aim. Plausibly exactly "nested with the nose," but it is a behavior change on the non-overridden axes — Chad rules. |
| D8 | **Freelook release target = VELOCITY** (was: nose, conditional): on release after keyboard flight, aim := guarded v̂ so the aim sits on the flight path — the instructor commands no surprise turn, and the recovery roll + camera settle converge on the same axis. | PROPOSED. The v̂ is the CALLER'S OWN guarded copy — `normalize(state.velocity)` under the speed guard (red-team F5: the snapped aim becomes `ci.target_dir_world`, a CONTROL input, so reading the sim's held `last_vhat` is the §9.6-violation class, not an instrument read; precedent for the caller-side guard: the camera anchor in `app/main.cpp`). Below `v_ballistic` / tail-slide (v̂·nose < 0) fall back to nose. Note the small release hop (nose→velocity differ by AoA; lands inside the ease-back settle — fly it). |
| D9 | **Mid-roll override keypress** (red-team F10): what happens if a keyboard override is pressed while the recovery roll runs? | PROPOSED: CANCELS the roll (level-consistent with re-press-cancels and with the open-loop D3 shape; a mid-roll keyboard jink means the pilot is maneuvering again). Chad to confirm. |
| D10 | **§5b nesting at the up-pole** (red-team F3, P1): a vertical override pull during freelook sweeps the nose through the carried up — per-tick `snap_forward_to_nose` then whirls/flips the projected up ~180° in a tick, roll-snapping the live orbit view (and its 0.9999 fallback becomes a per-tick non-hysteretic gate). | MUST be ruled before §5b is implemented. Default proposal: a hysteretic cone-hold of the last valid up across the pole (pure state function — the AT-9 lesson binds), with a MANDATORY test leg flying a vertical override loop under nesting (bases-must-separate / mechanism-FIRING discipline). Alternative: accept-and-document the snap (measure-zero maneuver). |

**What D6 buys (why it beats the original try order):** the trigger is an INPUT event
(same class as the override keys), so the trajectory stays a deterministic function of
the input trace — no hysteretic calm-detection legs, no dwell counters, no pure-state
gate to get wrong, and the AT-9 frame-quantization trap has nothing to bite. It also
softens the §3.1 ruling supersession: the world never rights itself autonomously, only
ever as a consequence of something the pilot did.

## 5. Methods, in try order

**Prerequisite (step 0): DONE** — S7-cam3 and S8-drone are committed (gate 185/185).
Remaining before Method 1: Chad's fly-confirm of S7-cam3 if still outstanding, and
tag `pre-horizon-recovery`.

**Common to all methods:** behind a config knob DEFAULTING OFF (`[horizon_recovery]
rate = 0` ⇒ mechanism dead) until Chad blesses — knob-off must reproduce today's
goldens bit-identically (the 4c strict-superset discipline: that no-op IS the
regression proof). Mechanism lives in the SHARED tick path (`app::tick` / harness
`ClosedLoop`) beside the shared `input::Freelook` state machine — never main.cpp-only —
so the mirror-equivalence pin keeps covering it and the release event is read from the
ONE pinned freelook module, never re-derived per caller.

### Method 1 — freelook-release recovery roll, quick-uniform with eased settle (PRIMARY, per D6/D3)

- **Trigger:** arms on the freelook press; the roll starts on RELEASE, folded into the
  camera's existing ease-back settle (one perceived motion, not an event). Never rolls
  DURING the hold — the pilot is orbiting the camera to look around; rolling the
  underlying frame would drag the orbit view with it.
- **Mechanism (OPEN-LOOP, per red-team F1 — the P0 restructure):** at the release
  edge, compute ONCE: the misalignment angle θ₀ between `aim.up()` and
  `P_⟂forward(local_up)`, and the roll SIGN. Then per tick, while θ_remaining > 0,
  roll the frame about its own current `forward()` by `Δθ = min(rate,
  settle·θ_remaining)·dt`, counting the CAPTURED θ₀ down. NEVER recompute the target
  from local_up per tick — a recomputed target moves with the mouse-driven forward
  (the banned control-driving-quaternion loop) and flips across the zenith. The roll
  is a pure countdown: terminates by construction (F2), no target to chase, no
  per-tick gate. A pilot who maneuvers mid-roll ends NEAR upright, not exactly —
  accepted; the next freelook tap trims it. At ~1.4 s total, roughly the first
  300 ms rides inside the CQ2 mouse suspension; the rest is live-mouse (§3.2).
  - **Guards run ONCE, at capture** (one-shot events, so no hysteresis needed):
    zenith no-op (forward ∥ local_up at release — no meaningful "upright" straight
    up/down; note `level_up_to`'s cone is |dot| > 0.9999 ≈ 0.81°) and the exact-180°
    antiparallel tie-break (deterministic sign, e.g. the plane's current bank sign;
    guard BOTH degenerate ends, the S7-cam lesson).
  - **Termination + inactivity are STRUCTURAL (F8):** the inactive path (rate == 0,
    or θ_remaining == 0) must SKIP the quaternion math entirely — a zero-angle
    `angleAxis` through `normalize` can still perturb LSBs (the S3 near-identity
    class), and the knob-off strict-superset proof plus the mirror `pos_err == 0.0`
    pin rest on exact bits.
  - **Cancel rules:** freelook re-press cancels (level, not edge — the 4d latch
    lesson); a mid-roll OVERRIDE press cancels per D9. Next release recaptures fresh.
  - **Latch hygiene (F9):** the roll state lives beside the `input::Freelook` state
    and is cleared by `fl.reset()` — so GROUNDED/crash respawn, focus loss
    (`instructor_focus_loss`), and the F1 raw-mode toggle all kill a mid-roll latch
    (the round-3 "respawn inherits the dead life's ease-back" class). Pin each leg.
  - **CQ2 composition:** the ≤300 ms suspension is NOT extended (the cap is ruled).
  - **Conditional aim snap composes:** if an override was used during freelook, the
    release snaps the aim FIRST (:= nose today; := caller-guarded v̂ once §5b/D8
    lands), and θ₀ is captured about the NEW forward. Different DOF — cannot fight.
- **Files:** `input/aim_frame.h` (fixed-angle roll helper beside `level_up_to`),
  `input/aim_state.h` / `app/instructor_tick.h` (release-edge capture + countdown),
  `config/controller.toml` `[horizon_recovery] rate/settle` + loader.
- **First-flight question (the M1a-style single question):** *frequency of surprise* —
  check-six mid-dogfight means every release starts a horizon roll right as you
  re-engage. Post-split-S that's exactly what's wanted; mid-hard-scissors it may feel
  like the world squirming. Expectation: fine, because the roll only does anything
  when significantly misaligned (mostly post-vertical-maneuver — in a flat hard turn
  the carried up stays near local_up). Fly it.
- **Success:** freelook-tap after a split-S → the world smoothly rights itself as the
  camera settles; mouse never inverts before/during/after; a mid-roll drag stays
  screen-straight.
- **Abort if:** the mouse INVERTS or diverges BEYOND today's reticle float through the
  roll → the shared-frame premise is wrong somewhere — STOP the whole plan and
  re-attribute (§7). Scoped per red-team F6: the ordinary lag float (screen basis
  trailing the frame by the chase-lag angle) is pre-existing and will be at its most
  visible at the release moment (the D8 aim jump opens the lag gap) — do NOT
  mis-attribute it to the roll and burn the plan on flight 1. Instrument first: extend
  the "mouse-up stays screen-up" pin to run MID-roll with `cam_fwd == aim.forward()`
  (isolates the roll), plus a separate leg with `cam_fwd` deflected (attributes the
  lag component).

### Method 2 — same trigger, bare snap (fallback; RESTRUCTURED per red-team F4)

- **Mechanism:** on release, snap the frame with `level_up_to(local_up)` instantly —
  the rendered camera pops WITH it (it reads the same frame). NO render-side eased-up
  variable: the original design ("ease only the rendered camera roll") required
  exactly the camera-only-copy split that §2 names as the one way to reproduce the
  inversion bug, protected only by fp-fragile CQ2 window timing (the 4d ±1-tick
  lesson) — a residual eased-up angle at the first live-mouse tick IS the old bug in
  miniature. The mouse is suspended at the snap instant anyway (release starts the
  CQ2 window), so the pop is never under a live drag.
- **When:** if Method 1's rolling recovery feels wrong mid-fight (the squirm) or the
  mid-roll drag-curl is felt. Trade: an instant 180° world pop — visually harsh; that
  harshness is WHY Method 1 is primary.
- **Files:** as Method 1 minus the fixed-angle helper (`level_up_to` suffices).

### Method 3 — dedicated manual "right the horizon" key (escape hatch)

- **Mechanism:** Method 2's snap+ease on an explicit keypress (edge-triggered, routed
  frame→tick like the override keys). No coupling to freelook at all.
- **When:** if Chad wants righting fully decoupled from freelook habits, or as the
  minimal proof-of-concept if Method 1's first flight raises attribution doubts.

*(The original auto-fire design — hysteretic calm-state trigger: misalignment band +
deadzone + low-|ω| + upright + tick-dwell — is RETIRED by D6. Revive it only if a
future ask wants recovery with no player action at all; its trigger legs and the
anti-chatter/AT-9 guardrails are preserved in §7/§8 for that case.)*

## 5b. Companion QoL change — freelook keyboard-flight aim nesting (D7/D8)

*Added 2026-07-07 (Chad): a gameplay quality-of-life change that shares the freelook
release moment with Method 1, so it belongs in this plan — but it is a SEPARATE dial
and a SEPARATE flight (one dial per flight; do not land it in the same commit as the
recovery roll).*

**The KEEP (pin it, don't break it):** outside freelook, a held keyboard override
maneuvers the plane while the aim stays EXACTLY where the mouse put it (the
parallel-transported world vector; pursuit suspended). Chad flight-tested this
2026-07-07 and relies on it — "I can control where my mouse aim will be exactly upon
release of the keyboard override." A naive nesting implementation could accidentally
drag the aim to the nose here too; the contrast (override outside freelook → aim
UNMOVED) needs its own executable pin.

**The change (SPEC §9.5 amendment — supersession required):**
- **During:** while `freelook && any_override`, the aim tracks the nose per tick
  (`snap_forward_to_nose` each tick, after transport). The aim dot and the nose
  indicator read nested on the HUD — no HUD change needed, it's a consequence of
  aim == nose. This is the per-tick strengthening of the EXISTING rule (snap once at
  the first override of the hold); the `override_used` latch still drives the
  conditional release behavior. **⚠ UP-POLE (red-team F3, P1 — D10 must be ruled
  first):** the first draft claimed "keeps the carried up, so camera roll stays
  continuous" — FALSE at the pole. A vertical override pull sweeps the nose
  through/near the carried up; the per-tick Gram-Schmidt then whirls the projected up
  (rate ∝ 1/miss-angle) and FLIPS it ~180° across the crossing — a one-tick roll-snap
  of the live orbit view, the zenith-class singularity re-imported by nose-slaving
  the frame (the "unreachable in normal flight" comment on `snap_forward_to_nose`'s
  fallback is written for the ONE-SHOT snap and is false per-tick). Additionally the
  0.9999 fallback becomes a per-tick non-hysteretic gate (chatter class). Default fix
  per D10: hysteretic cone-hold of the last valid up across the pole (pure state
  function — AT-9 binds), plus a MANDATORY test leg flying a vertical override loop
  under nesting (mechanism-FIRING discipline).
- **On release (override was used):** aim := guarded v̂ (D8) instead of := nose — the
  aim lands on the flight path, so the instructor commands no surprise turn, and the
  Method-1 recovery roll then rights the world about that same axis while the camera
  settles to its rest behind the velocity vector. One composed settle moment.
- **Unchanged:** freelook WITHOUT override keys — aim stays held/carried (the held
  turn, preserved per the 4d lesson), release does not move it (only the D6 recovery
  roll runs).

**Why it's compatible with this plan (the same two-DOF argument):** during freelook
the mouse does not feed the aim at all (it drives the orbit camera), so per-tick
nose-capture touches no part of the raw mouse→aim path — it is a state-driven aim
event, same class as the existing snaps. At release, the aim-target change (direction
DOF) and the recovery roll (gauge DOF about that direction) govern different degrees
of freedom and cannot fight; ordering is aim := v̂ FIRST, then the roll about the new
forward.

**Guards + tests:**
- v̂ source + degeneracy (REVISED per red-team F5): the release target feeds
  `ci.target_dir_world` — a CONTROL input, not an instrument read — so the caller
  computes its OWN guarded v̂ (`normalize(state.velocity)` under the speed guard;
  precedent: the camera anchor in `app/main.cpp`), never the sim's held `last_vhat`
  (that read is the §9.6-violation class the S4a red-team named). Below `v_ballistic`
  or tail-slide (v̂·nose < 0) → fall back to nose.
- aim == nose exactly puts `target_body` at (0,0,−1) EVERY tick under nesting — this
  is covered by the existing lateral-magnitude gate on `bank_error` (the 4b red-team
  pole guard, which already owns the every-reset case); cited here explicitly so the
  coverage is by design, not luck (red-team F11).
- Determinism: keys are inputs, per-tick capture is pure state — AT-9 safe; lives in
  the shared `input::Freelook`/`app::tick` path like every freelook rule.
- The 4d freelook pins in `test_instructor_tick.cpp` (rules 2/3: first-override snap,
  conditional release reset) WILL move — BY DESIGN. Re-pin deliberately against the
  amended §9.5; do not blind-re-record. Freelook-off must stay a byte-for-byte no-op
  (the 4d strict-superset proof).
- Both release orderings (override key up before vs after the freelook release) must
  stay pinned, as the 4d lesson demands — the nesting changes WHEN the aim tracks,
  not the release-overlap semantics (φ_held recapture etc. untouched).

## 6. Fallback plan (if things go wrong)

1. **Git hygiene:** step 0 commits S7-cam3; tag `pre-horizon-recovery` before Method 1;
   ONE commit per method attempt; each independently revertable. Knob-off = instant
   behavioral revert without touching code.
2. **Ultimate fallback is TODAY:** the current behavior (world stays above you after a
   split-S, roll the aim over to fix it by hand) is the already-accepted trade. Shipping
   nothing is an acceptable outcome; do not force it.
3. **Stacked-patch smell (the S7-mouselevel signature):** if a method needs a new gate
   to survive each new failure, or fix N re-breaks what N−1 fixed — STOP, revert to the
   tag, re-attribute in a FRESH context. Three gate revisions was the historical cost
   of ignoring this.
4. **Escalation order is M1 (eased-settle roll) → M2 (snap+ease) → M3 (manual key)** — all
   three share the freelook/shared-frame premise, so a CORRECTNESS failure (the mouse
   misbehaves through a roll) in any of them invalidates the whole plan → stop and
   re-attribute. A *feel*-only failure in M1 (the squirm / the felt curl) permits M2;
   a feel failure in M2's coupling-to-freelook permits M3.

## 7. Diagnostic checklist (symptom → suspect → instrument)

| Symptom | Suspect | Check |
|---|---|---|
| Mouse inverts during/after recovery | Roll applied to a render-side copy, not `loop.aim` — the frames split | The `test_instructor_tick.cpp` "mouse-up stays screen-up" pin, extended to run MID-recovery; grep that the roll writes `loop.aim` |
| Aim/reticle JUMPS when recovery fires | Roll axis isn't `forward()` | Unit pin: `forward()` bit-identical across the roll |
| World rolls the long way, or direction differs run-to-run | Antiparallel (exactly 180°) tie-break missing | Unit pin at `up == −local_up_projected` exactly: deterministic sign, no NaN (guard BOTH ends — the S7-cam `normalize(cross)` lesson) |
| Camera freezes / NaN pose | Degenerate cross at zenith or antiparallel poisoned the carried frame | `level_up_to`'s zenith no-op pin + the antiparallel pin above |
| Roll keeps running / restarts oddly around freelook taps | Re-press-cancels latch wrong (edge vs level, the 4d aim-snap lesson: latch on the HOLD, not a rising edge) | Direct `Freelook`-style step test with exact-integer tick counts (the 4d fp-fragility lesson); pin press-cancels + release-restarts |
| Rolls DURING the freelook hold (orbit view drags) | Roll gated on the wrong state leg | Pin: roll inactive while freelook held |
| 30 fps vs 240 fps trajectories diverge (AT-9) | A frame-quantized signal crept into the roll's per-tick logic (the trigger itself is an input event — fine; the ROLL must be pure tick-state) | AT-9 mirror with recovery FIRING mid-run — the S7-mouselevel trap was that every test exercised the no-op case |
| *(retired calm-state variant only)* fires mid-maneuver / chatters | A trigger leg not calm enough / not hysteretic | Open-loop dwell-with-ripple test across each threshold pair (the S6 anti-chatter lesson) |
| Golden moved with knob OFF | Mechanism isn't a strict superset | Bit-compare; the knob-off no-op is the regression proof |
| Golden moved with knob ON | A golden scenario's scripted mouse deltas got remapped by a recovery firing | Expected IF the scenario trips the trigger — confirm intent before re-recording, per the standing rule |
| Recovery fights the view during freelook/override | Missing cancel interlock | Pin: roll CANCELS on freelook re-press AND on override press (D9) |
| Roll never ends / runs across maneuvers | Termination missing (exponential capture never reaches 0) or a recomputed per-tick target re-inflating θ (the F1 P0) | D3 is a CAPTURED-angle countdown: pin θ_remaining hits exactly 0 and the latch clears; grep that no per-tick `local_up` read feeds the roll |
| Respawn/focus-loss/F1-toggle inherits a mid-roll latch | Latch not cleared by `fl.reset()` (the round-3 dead-life class) | Pin each reset leg kills an active roll (F9) |
| Goldens/mirror move at knob OFF by LSBs | Inactive path still runs zero-angle `angleAxis`+`normalize` (the S3 near-identity class) | F8: inactive path must SKIP the quaternion math; bit-compare |
| Orbit view roll-snaps ~180° during a vertical freelook+override pull | §5b per-tick nesting at the up-pole (F3) | The D10 cone-hold + the mandatory vertical-loop-under-nesting test leg |
| Aim drags to the nose during a NON-freelook override | §5b nesting leaked outside its `freelook && any_override` scope | The §5b KEEP pin: override outside freelook → aim world-vector UNMOVED (Chad's tested-and-relied-on behavior) |
| Aim jumps wildly on freelook release at low speed / tail-slide | Release := v̂ with degenerate velocity | §5b guard: below `v_ballistic` or v̂·nose < 0 → fall back to nose; pin both fallback legs |
| Reticle visibly hops at release (nose→velocity) | Inherent AoA offset (D8), question is magnitude | Should land inside the ease-back settle; if felt, D8 falls back to := nose (one-line flip) |
| Straight screen drag curls the world path mid-roll | Inherent (§3.2), question is magnitude | If felt: lower `rate`, or switch to M2 (the CQ2 suspension kills the curl by construction) |

## 8. Guardrails (the standing lessons that bind this work)

- ONE dial per flight; hypothesis→observed in `docs/flight-log.md`; **CHAD FLIES every
  step** — harness numbers explain, they never judge.
- Gate (`cmake --build build` + `ctest`) after every change; moved golden → STOP.
- Every trigger leg hysteretic; dwell in ticks; pure state function only.
- Test with the mechanism ACTUALLY FIRING on a non-trivial path — the no-op case
  proves nothing (S7-mouselevel red-team escape).
- Red-team via `adversarial-review` in a separate context — the aim frame and
  camera-up are §6 targets; never self-review.
- When it lands: SPEC §0 supersession entry (scoped per §3.1), CLAUDE.md Learned line,
  config comments on every knob, update `docs/next_agent_brief.md` thread 1, and a
  short "how it was solved" section appended HERE — this doc is the milestone record.

## 9. Why we believe it will work (the one-sentence version)

The world's orientation relative to the camera is a free gauge: rolling the SINGLE
shared aim/camera frame about the aim direction is invisible to the instructor, and
because the mouse basis and the screen basis share the same up vector the mouse can
never invert through it — provided the roll is an OPEN-LOOP fixed angle captured at
release, never a per-tick chase of a target that moves with the mouse-driven forward.

## 10. Red-team record (fresh-context Fable 5 consult, 2026-07-07, pre-implementation)

Plan-stage adversarial review, separate context, refute-stance, chartered on: the
gauge premise, the release pileup, D8 degeneracy, §5b nesting, determinism/tests, and
new S8 consumers. **Verdict: SOUND-WITH-FIXES** — the premise held on every lane, the
mechanism wrapped around it did not. All findings folded into the sections above:

- **F1 (P0)**: the per-tick roll toward a RECOMPUTED local-up target closes the banned
  control-driving-quaternion loop through the mouse-driven forward and flips across
  the zenith at full rate — the first draft's "no feedback loop can form" was the
  plausible-but-wrong claim, and its calm-gating defense cited machinery D6 had
  deleted (the two same-day revisions had not been re-reconciled). FIX: open-loop
  fixed-angle capture at release (§3.2, D3, Method 1).
- **F2 (P1)**: no termination condition → captured-angle countdown, latch clears at 0.
- **F3 (P1)**: §5b per-tick nesting roll-snaps the live view ~180° at the up-pole
  under a vertical override pull → D10 (cone-hold default, mandatory firing test).
- **F4 (P1)**: Method 2's rendered-ease was the camera-only-copy split §2 bans,
  fp-fragile at the CQ2 boundary → bare snap.
- **F5–F12 (P2/NIT)**: D8 v̂ is a control input, caller-guards its own copy (F5); the
  premise wording weakened to "shared up ⇒ no inversion" + abort criterion scoped
  above the pre-existing lag float so flight 1 can't mis-attribute it (F6); the
  §9.2 "differs only by real holonomy" clause added to the supersession list (F7);
  knob-off must structurally skip the quaternion math (F8); latch cleared on every
  reset leg (F9); mid-roll override press = cancel, D9 (F10); D7 partial-override
  nuance surfaced for Chad (F11); stale file:line citations fixed (F12).
- **Lanes that HELD:** the instructor cannot see the roll (`ci.target_dir_world` is
  the only aim read); the release pileup composes in both 4d key orderings; the S8
  gunsight/drone/HUD code reads nose/positions/velocities only — never the aim frame
  or camera basis — so the roll cannot corrupt it.

## 11. How it was solved — Method 1 IMPLEMENTED (S7-hrz, 2026-07-07)

Landed same day, exactly as red-teamed. Gate **185 → 194**, all green.
**FLY VERDICT (Chad, 2026-07-07): "oh my it is perfect!"** — the shipped
`rate = 150` / `settle = 5.0` stand; the §5 first-flight question (frequency of
surprise) resolved in favor. Flight-log entry recorded.

**What landed (SPEC §0 `S7-hrz` is the ledger entry):**
- `input/aim_frame.h`: `roll_about_forward(angle)` (the gauge move; forward drift
  < 1e-11 over a full recovery, pinned) and `up_misalignment(ref_up)` (the ONE-SHOT
  capture: signed short-way angle, zenith cone → exactly 0.0, exact-antiparallel
  deterministic via atan2 — no `normalize(cross)`, so no NaN at either end).
- `input/aim_state.h`: `Freelook::Step.released` (the release edge, set beside the
  rule-3 snap) and `input::HorizonRecovery` — the open-loop countdown latch:
  `capture()` once at the edge, `step()` per tick with the D3 profile
  `w = min(rate, settle·θ)`, terminal snap under `kFinishEps` (0.57°) so `remaining`
  reaches EXACTLY 0 and the inactive path costs zero quaternion math (F2/F8).
- `app/instructor_tick.h`: the wiring after `apply_mouse` — capture on `fs.released`
  (AFTER the rule-3 snap, so the angle is about the released forward), canceled
  (level) by `freelook_held || any_ovr` (D9; a release with a key still held never
  arms — the 4d overlap), all behind `rate > 0` (F8). `LoopState.recov` cleared by
  the GROUNDED pairing and `instructor_focus_loss` (F9).
- `config/controller.toml [horizon_recovery]` + loader (rate ≥ 0, settle > 0 when
  enabled).

**Verification:** 9 new test cases (frame primitives in `test_aim_frame.cpp`,
tick-level in `test_instructor_tick.cpp`), all §7-diagnostic-derived, all with the
mechanism FIRING. Mutation-verified three ways: **M1** per-tick recomputed target
(the F1 P0 restacked) → the mid-roll-sweep residual collapses 10.3° → 0.00° and the
open-loop leg FAILS; **M2** terminal snap removed → the exact-0 termination legs
FAIL; **M3** capture sign flipped → 48 monotone-convergence failures. The knob-off
strict-superset proof is the untouched existing suite (goldens + mirror never fire
a misaligned release) plus an explicit `rate = 0` no-op leg.

**DIFF RED-TEAM DONE (fresh-context Fable 5, separate context, 2026-07-07):
verdict SOUND-WITH-FIXES — no shipped-behavior defect;** the implementation matches
the plan-stage red-team's demanded shape on every attack lane (sphere, loop
integrity, edges, numerics, caller glue, the AT-14 firewall). Findings, all
test/hygiene, all landed:
- **P2-1** capture-order vs the rule-3 snap was untested (override used INSIDE the
  hold → snap + capture fire the same tick) — new leg: righted about the SNAPPED
  forward; kills the hoist-the-recovery-block-above-the-snap mutant. **The
  RE-REVIEW round caught the first version's kill claim as FALSE** (a
  level-flight release makes `up_misalignment` invariant under a yaw-only snap —
  the geodesic axis is ∥ local_up — and second-order under pitch-only; the S7-push
  "re-derive which knob each leg actually pins" class): strengthened to a 90-tick
  pitch+yaw hold (~49° gap; mutant parks at ~8.7° vs ~1e-4° correct), and the kill
  re-verified by hand in the authoring context (mutant applied → leg FAILS).
- **P2-2** the mirror-equivalence pin CANNOT cover this mechanism (harness
  ClosedLoop carries a bare vec3 aim — the roll is unrepresentable there; the
  plan's "shared path so the mirror covers it" was optimistic) — new dedicated
  trajectory-neutrality leg: identical misaligned freelook-tap trace, rate 150 vs
  rate 0, **pos_err observed EXACTLY 0.0 at max_digits10** (the roll's ~1e-16/tick
  forward perturbation vanishes at the float `sim::Inputs` seam), pinned < 1e-9
  (S1: never a hide-band). Seam documented in `test/harness/instructor.h`.
- **P3-1** `capture()` under-threshold now clears a stale latch (unreachable via
  today's callers; load-bearing for §5b).
- **P3-2 ACCEPTED (documented, not landed):** an AT-9 30-vs-240 fps leg with the
  recovery FIRING — reviewer verified the roll consumes only sim_dt, key-level
  inputs, and pure tick state (no frame-quantized signal), so the risk is low;
  land it if §5b touches the frame→tick seam.
- **NIT (noted):** a net-zero both-keys override leaves the mask off (S5
  convention), so it does not cancel the roll — behaviorally invisible.

## 12. §5b IMPLEMENTED — S7-nest (2026-07-07, same day)

Chad confirmed D7/D8 in his own words ("in free look with keyboard controls, move
the mouse all around with the nose dot, nested together, and when the free look is
released we get the orientation with the new velocity and the new mouse aim all
aligned and our camera is chasing from behind") and S7-nest landed. Gate
**196 → 200**. SPEC §0 `S7-nest` is the ledger entry.

- **D7 (nesting)**: aim rides the nose per tick while `freelook && any_override`
  (subsumes rule 2); freelook without keys unchanged (held turn preserved). The
  §5b KEEP is pinned per tick against the independent `transport_rotation` path
  (override outside freelook → aim transport-pure).
- **D8 (velocity release)**: rule-3 release snap lands on the CALLER-guarded v̂
  (below `v_ballistic` / tail-slide → nose). At release the recovery captures
  about the new forward — aim, roll axis, and camera rest all on the flight path.
- **D10 RESOLVED — no cone-hold needed.** F3's up-pole assumed a STATIC carried
  up under a large forward gap; per-tick nesting caps the gap at ~ω·dt, so the
  carried up rotates rigidly with the nose. The mandatory instrument leg drives
  the nose 102° through the initial up: max per-tick camera-up step **0.24°**
  (bound 3° — if a future change trips it, implement the cone-hold then).
- Mutation-verified ×3 (MN1 nesting leak → KEEP leg fails; MN2 D8 revert →
  separated-bases leg fails at a 7.4° release gap; MN3 tail-slide guard dropped
  → backward-velocity leg fails). Harness seam extended
  (`test/harness/instructor.h`): ClosedLoop keeps 4d-era one-shot nose snaps.

**FLY VERDICT (Chad, 2026-07-07): "totally perfect! everything hits — exactly as
I wanted."**

**S7-nest DIFF RED-TEAM DONE (fresh-context Fable 5, one scoped round, 2026-07-07):
verdict SOUND-WITH-FIXES — shipped code correct on every lane attacked** (branch
logic enumerated across all Freelook-rule combinations; velocity-guard boundary
semantics incl. NaN/zero/perpendicular; the snap_forward_to_dir delegation
bit-identical for every consumer; nesting×CQ2×recovery×drone interactions;
GROUNDED/crash/focus-loss edges). Both findings were TEST-side, both landed and
mutation-verified by hand:
- **P2**: the low-speed fallback leg's velocity was nose-PARALLEL, making the
  `v_ballistic` guard mutation-invisible (the "bases must SEPARATE" class) →
  velocity re-pinned ~35° off the nose; guard-deletion mutant now FAILS the leg.
- **P3**: the F5 "never reads the sim's `last_vhat`" claim was only accidentally
  pinned (the two copies coincide on the honest path — the S4a "poison the copy
  it claims isn't read" class) → `last_vhat` poisoned before the release tick;
  a last_vhat-swap mutant now FAILS both aim checks.
- **P3 noted, accepted:** the guard reuses the ballistic ENTRY threshold — in
  (30, 40] m/s the controller can still be latched ballistic while the release
  trusts v̂; one-shot event so no chatter (the hysteresis rule targets dwelling
  gates); `v_ballistic_exit` is the one-line conservative swap if ever felt.

**This milestone is COMPLETE.**
