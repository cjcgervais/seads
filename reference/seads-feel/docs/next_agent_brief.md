# NEXT AGENT — ultrathink brief (2026-07-07)

> **UPDATE (2026-07-07, later): S8-drone LANDED.** The non-shooting AI target-drone fleet
> + lead-angle gunsight is committed and cross-model audited (`25a8cd9` → `635f87a` →
> `ffe6b55`, gate **185/185**; ledger SPEC §0 `S8-drone`, CLAUDE.md ## Deferred top). The
> "highest-leverage unlock" this brief flags below (a target to fight) now EXISTS — the
> **Tracking star is honestly gradable**. Next steps in CLAUDE.md ## Deferred. The rest of
> this brief is the earlier snapshot; the camera fix + S8-drone are now committed, so the
> "UNCOMMITTED / gate 156" lines below are stale.
>
> **Read this whole file, then the `dogfight-systems` skill
> (`.claude/skills/dogfight-systems/SKILL.md`), `docs/milestone_feel_bug_attribution.md`,
> and `docs/world_art_direction.md`. ULTRATHINK. This repo punishes plausible-but-wrong
> feel changes more than anything else — attribute before you touch, and CHAD FLIES
> every feel change.** Branch: `sandbox/flight-tuning-2` (off `main`). Gate **156/156**.
> Working tree is UNCOMMITTED (the camera fix + these docs are on disk, not committed).

## Your goal — master the skill, then use it

Become the engineer who can knowingly implement and tune **camera + aircraft systems
with an independent RAW mouse and a camera-chased reticle**, and make a dogfight *fun*.
The `dogfight-systems` skill is your text: the three feel subsystems (raw mouse→aim,
chase camera, instructor cascade), which feel each owns, the attribution discipline,
and the domain physics mapped to the real config knobs. Internalize it before coding.

## Where things stand (three threads)

**1. CAMERA / MOUSE — SOLVED this session (S7-cam3).** camera-up = the carried aim-frame
up (`app/main.cpp` `cam_up = loop.aim.up()`), reversing the S7-cam2 horizon-lock. Result:
**mouse-up == screen-up at every attitude** (the "mouse inverts after a split-S" is
gone), the raw mouse is byte-for-byte untouched, and the horizon rolls with the aim
through a loop ("rolls with me"). `render::ease_level_up` + the `[camera] level_rate`
knob are deleted. New mutation-verified pin: `test_instructor_tick.cpp` "mouse-up stays
screen-up at any attitude". The camera also **already centers the reticle** (`[camera]
lead = 1.0` ⇒ the rest target IS the aim), so the once-planned "Dial 2" is effectively
already shipped — **don't redo it.**
- *Not yet done:* Chad's final fly-confirm (he was mid-assessment: "might be good"),
  the red-team (`adversarial-review`, separate context — camera-up is a §6 target), and
  the commit + SPEC §0 already written (S7-cam3). **First camera job: get Chad's fly
  verdict → red-team → commit.** If he's happy, move on.
- *Read `docs/milestone_feel_bug_attribution.md` first.* It is the lesson that unlocked
  this: the prior sessions burned weeks patching the MOUSE for a bug that was in the
  CAMERA. Attribute the feel symptom to the right subsystem before touching anything.
- *Next camera milestone PLANNED (2026-07-07): `docs/horizon_recovery_plan.md`* —
  righting the world after a split-S (the "world above me" trade) by rolling the
  SHARED aim/camera frame about the aim direction, which is invisible to the
  instructor and cannot invert the mouse. **Trigger RULED (D6): the FREELOOK
  RELEASE**; **profile RULED (D3): "snap to gentle settle"** — a quick uniform roll
  (~150°/s) easing into alignment, folded into the ease-back settle (input-triggered,
  so no gate machinery; snap+ease and a manual key are the fallbacks). Chad wants to
  fly it next; prerequisite: land S7-cam3 first (fly verdict → red-team → commit).
  A companion QoL change rides the same plan (§5b, D7/D8): freelook keyboard-flight
  aim NESTING (aim tracks nose while `freelook && override`) + release aim :=
  guarded velocity — same release moment, SEPARATE dial/flight, amends SPEC §9.5.
  **The plan is RED-TEAMED** (fresh-context Fable 5, plan §10, SOUND-WITH-FIXES, all
  folded): the roll is OPEN-LOOP (fixed angle captured at release — NEVER a per-tick
  chase of a recomputed local-up target, that was the P0), and §5b's up-pole (D10)
  must be ruled before nesting is coded. **Method 1 IMPLEMENTED + FLOWN (S7-hrz,
  SPEC §0, plan §11): Chad — "oh my it is perfect!"; its diff red-team DONE
  (SOUND-WITH-FIXES, all landed). §5b IMPLEMENTED too (S7-nest, SPEC §0, plan
  §12): aim nests to the nose in freelook keyboard flight, release lands on the
  guarded VELOCITY, D10 resolved empirically — no up-pole under per-tick nesting
  (0.24° max camera-up step through a 102° pull). Gate 200/200,
  mutation-verified ×3.** **BOTH FLOWN AND VERDICT-PERFECT (Chad 2026-07-07);
  milestone CLOSED, merged to main.** The camera/mouse/freelook thread is DONE.
  **Next missions (each its own session + branch): `docs/next_missions.md`** —
  (A) RMB aim-zoom camera; (B) "instructor to Chad's delight": deep-research →
  coordinated high-performance flight (rudder-in-turns, higher elevator gain,
  stall-recover-not-spin) + the energy game (combat egg, deflection bleed).

**2. FLIGHT FEEL — the open mission (Chad's words):** *"fast, punchy, more elevator
authority, better maneuvering, aggressive-smooth turning, a sweet spot — fly the combat
egg with ease."* This is instructor/plant tuning. Chad has already pushed the airframe
hard (`c_pitch` 7→11, `n_max` 10→16, `n_min` −4→−8, `integ_cap` 0.5→2.0, `Cl_max`
1.4→1.8, `p_max` 180→260, `c_roll` 15→24, `c_yaw` 4→16, `yaw_scale` 0.5→1.2, `K_coord`
3.0→1.0 for skid, `bank_align_power` 3→6).
- The skill §4 maps the physics to the knobs: **corner speed = the sweet spot**
  (`Cl_max` / `n_max`); **coordinated turn = bank then pull** (`bank_align_power`,
  `K_coord`, `yaw_scale`); **pitch punch is bounded by PIO** — get snap from AUTHORITY
  (`c_pitch`/`c_roll`/`c_yaw`) and rate ceilings (`n_max`/`p_max`/`Cl_max`), NOT from
  cranking `K_theta` past its 3.2 ceiling (that rings AT-2 = the ZOH/PIO wall).
- Discipline: `docs/flight-log.md` — one knob per flight, hypothesis→observed, Chad
  flies each, tuning order (inner→outer→braking→clamps→feel), coefficients/gains only.
- **Honest ledger:** there is NO target to fight (one aircraft). The **Tracking** star
  cannot be honestly graded until a non-shooting **target drone** exists (`TEACHING.md`
  Addendum A.2 — a second pure `sim::SimState`, same invariants, read-only
  pipper/time-on-target instruments). That is the highest-leverage unlock for real
  dogfight feel. Phantom-safe feel work (the plane's own response) is legitimate now.

**3. THE WORLD — the legible little world (see `docs/world_art_direction.md`).** The
far-side/southern-hemisphere disorientation (it made Chad motion-sick) is a
PRESENTATION problem: the physics is position-invariant, but the world is real Earth
(a global orientation anchor everyone has memorized) + a fixed sun. The fix is
**content, not camera**: a **1940s black-and-white small-town-Canada "little world"**
with big, known-size, radially-oriented features (redwoods, houses, a stadium) that
tell you up/altitude/speed everywhere; the **planes are the only saturated color** —
shiny painted metal, complementary team pairs (orange+blue, purple+yellow, green+red).
First cut: drop Earth → B&W world + saturated-plane pass → ONE hero town. Visual-first
(no collision v1). Lives in the `sandbox/planet-art` worktree.

## The discipline (non-negotiable — CLAUDE.md "Lessons learned")

- **ENTER PLAN MODE + ASK Chad** clarifying questions before any non-trivial
  flight/control/camera change. Confirm the axis, the reference frame, and the FELT
  behavior in his words. A wrong structural change is the most expensive mistake here.
- **ONE dial per flight, a test for every change, CHAD FLIES each one.** Never tune to
  harness numbers — they explain a score, they never replace the stick.
- **No auto-leveling of anything. The mouse is RAW.** "Level" = wings-level via
  ailerons referenced to local-up — never pitch/elevator, never the airframe platform.
- Gate after every change (`cmake --build build` + `ctest`). A moved golden → STOP,
  confirm intent, re-record deliberately. Red-team the important mechanisms in a
  SEPARATE context (`adversarial-review`) — never self-review.

## Pointers

- Skill: `.claude/skills/dogfight-systems/SKILL.md`
- The unlocking lesson: `docs/milestone_feel_bug_attribution.md`
- World / art: `docs/world_art_direction.md`
- Camera fix: SPEC §0 `S7-cam3`; `app/main.cpp` `cam_up`; `test_instructor_tick.cpp`
  "mouse-up stays screen-up at any attitude"
- Feel loop: `docs/flight-log.md`; `docs/TEACHING.md` §7 + Addendum
- Constitution / SOP: `SPEC.md` §6 (sphere) / §9 (controller); `docs/HARNESS.md`; `CLAUDE.md`
