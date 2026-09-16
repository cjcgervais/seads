> **SUPERSEDED (2026-07-07) — do NOT work this brief.** The problem was the CAMERA, solved by S7-cam3 (camera-up = the carried aim-frame up; SPEC §0). The lesson that unlocked it: `docs/milestone_feel_bug_attribution.md`. Kept only as the dead-end ledger (don't restack the fixes it lists).

# HANDOFF — fresh audit: mouse-aim + camera FEEL through vertical maneuvers (loops / split-S)

**Written 2026-07-07 for a FRESH agent, after a long session of STACKING FIXES that kept trading one
failure for another. Chad wants a clean-context, first-principles AUDIT — do NOT just continue the last
patch. Read this whole file, then read SPEC.md §0 (the S7-* supersessions), CLAUDE.md (rules + Learned),
and `docs/section7_worklist.md`. Branch: `sandbox/flight-tuning-2` (off `main`). Gate: 160/160.**

---

## 0. The task
Make **mouse-aimed LOOPS and SPLIT-S feel good** — the mouse intuitive *throughout* the maneuver and the
camera readable — on the R=15 km sphere, WITHOUT violating Chad's rules (§1). The plane MECHANICALLY does
the maneuvers now (loop goes over, stays inverted, no corkscrew; split-S rolls through). What's still
wrong is the **FEEL of the controls + camera through the vertical part**. Audit fresh; find the real fix.

## 1. Chad's RULES — inviolable, he has repeated these all session
- **NO auto-leveling of ANYTHING.** The plane just chases the RAW mouse aim naturally. "You're not
  supposed to introduce anything like re-level."
- **The mouse basis must be RAW** — nothing smoothed/eased/slewed/re-leveled feeding mouse→aim (SPEC §9.1
  / the RA9 ban). Every attempt to "help" the mouse basis has failed and been reverted.
- **"Level" = WINGS-level via AILERONS, referenced to LOCAL-UP** — never pitch/elevator, never the
  airframe platform. Pitch ONLY points the nose at the raw mouse aim; it NEVER levels attitude.
- **A pure LOOP = ZERO roll, ends how it started.** A SPLIT-S / IMMELMANN = a 180° roll (belly ends toward
  local-down, going the new direction).
- **ONE DIAL AT A TIME, a test for every change, CHAD FLIES each one.** ENTER PLAN MODE + ask him explicit
  clarifying questions BEFORE coding a flight-model/control/camera change. Confirm the axis, the reference
  frame, and the FELT behavior in his words.

## 2. The felt problem RIGHT NOW (Chad's words, current HEAD)
The mouse+loop-invert fixes work mechanically, but flying a loop/split-S:
- **Camera flips ~180° at the top of a loop** (the world spins over). Chad flagged this; unclear if it's
  the disorienting thing or acceptable.
- **After a split-S / half-loop the mouse goes "all opposite / wacky controls"** — up/down (and left/right)
  feel flipped until you roll back upright.
These two are almost certainly the SAME root cause: "up" (both camera-up and mouse-up) is CARRIED/rotated
through the vertical maneuver instead of staying world-referenced, so past the top everything is inverted.

## 3. Current code state (what to KEEP vs what's the open sore)
HEAD `eb265f3`. Commits since `main` (`aeedbe7..HEAD`), all this session:
- **S7-cam-zenith** — camera-up TRANSPORTS over the zenith cone instead of freezing (fixed: camera stuck
  at top of loop + doubled/NaN reticle). `render/camera.cpp` `ease_level_up` + `project_dir` guard.
  **KEEP the reticle guard + no-stuck.** The "flip over the top" behavior is SUSPECT (see §2).
- **S7-raw** — removed the F1 screen-relative mouse; mouse is the PURE §9.1 carried frame. **KEEP.** This
  is the raw mouse Chad wants; it loops over the top cleanly.
- **S7-loop-invert** — the wings-leveling roll (auto-level + FINE/deadzone `roll_hold`) is FADED to 0 when
  inverted (`control/controller.cpp`, `wings_level_gate = smoothstep(-band,+band,cos_phi_theta)`,
  `[regime] wings_level_band`). So a loop STAYS inverted (no Immelmann) AND the corkscrew is gone (0° bank
  — the corkscrew WAS the inverted wings-hold). `roll_maneuver`/push untouched (turns + split-S still
  roll). **KEEP — this is solid and red-teamed.**
- **S7-mouselevel — TRIED AND FULLY REMOVED.** See §4.
The **open sore** is §2 (mouse-flip-after-maneuver + camera-flip). Everything else works.

## 4. DEAD-ENDS this session — DO NOT REPEAT (each was flown & rejected)
1. **F1 screen-relative mouse** (rotate the aim about the RENDERED CAMERA basis so mouse-up==screen-up):
   STALLS at the zenith — the horizon-locked camera can't define screen-right looking up the radial, so a
   slow mouse-up pins the aim at vertical. Also a smoothed (eased camera) basis feeding mouse→aim = RA9
   violation.
2. **`level_up_to`** (re-level the mouse frame's up to the world horizon each tick): flips the mouse PITCH
   axis at the zenith / across the rear hemisphere, pins the aim at vertical. Rejected long ago.
3. **S7-mouselevel `roll_toward_local_up`** (gently ease the mouse frame's up toward local_up, rolling
   about the aim's own forward): the idea was to fix the post-maneuver up-is-down. It went through THREE
   gate revisions and each exposed a new failure — (a) idle-timer gate = frame-rate-dependent (broke AT-9,
   ~1cm); (b) `err<blend_lo` gate = fired during a SLOW TRACKED loop (err stays small when the plane keeps
   up) and CURLED the continuous mouse-up back to centre ("mouse locked to centre, loop imperceptible");
   (c) `err + near-horizon` gate helped but the whole thing is STILL an easing of the mouse basis, which
   curls any active sweep. **RULED OUT by Chad. Any easing/re-leveling of the mouse basis is banned.**
4. **Pole-free hybrid handoff** (switch the mouse basis from screen-relative to the pole-free carried frame
   near the zenith): JUMPS at the hard switch, broke the split-S (the cone caught the nadir too), and
   reintroduced the up-is-down inversion. Reverted.
**Pattern:** every attempt to make "up" world-consistent for the mouse fights either the zenith pole or the
raw-basis rule. Stop trying to patch the mouse BASIS.

## 5. LEADS worth a fresh audit (not yet tried this session)
- **THE BIG ONE — reconsider the CAMERA, not the mouse.** The mouse "feels flipped after a maneuver" only
  because the CAMERA (what you see) and the MOUSE frame (how you control) DIVERGE: S7-cam2 made camera-up
  HORIZON-LOCKED while the mouse frame is CARRIED. If instead **camera-up = the carried aim-frame up**
  (the ORIGINAL §9.2 design, before S7-cam2), then camera and mouse are ALWAYS aligned → mouse-up ==
  screen-up at every attitude, pole-free, NO flip, NO stall — and it's a CAMERA change (RA9 explicitly
  allows easing/smoothing the CAMERA; only the mouse→aim basis is sacred). S7-cam2 abandoned the carried
  camera-up for "instability in normal flight" (it rolled on bank-to-turn / when looking around) — AUDIT
  whether that instability was real/unavoidable or a fixable artifact, because a carried camera may make
  the whole mouse-flip problem VANISH. This is the most promising unexplored direction.
- **Ask Chad what the camera should DO at the top of a loop** (he half-answered "smooth roll over the top"
  once, but that may be the disorienting thing now). Options to put to him: (a) roll over with the plane
  (carried — aligns the mouse, §above), (b) hold the horizon and let the plane appear inverted-in-a-level-
  frame, (c) something else. This is a PLAN-MODE question, not a guess.
- **Separate "loop" from "split-S" cleanly** in his intent: he wants a loop = pure pitch no roll; a split-S
  = rolls through. Confirm each maneuver's desired mouse+camera behavior independently.

## 6. Verification tools (all exist, use them)
- `seads_harness mouseloop [up|down] [deflect_deg] [V] [ticks]` — drives the SHIPPED `app::tick` with a
  scripted vertical mouse; prints aimElev / aimBehind / cPT / phi per tick. The single best instrument for
  "does the aim sweep over / does the plane corkscrew / stay inverted." (Temporarily edit the hardcoded
  `rate_deg_s` in `run_mouseloop` to test slow vs fast sweeps — a SLOW sweep the plane tracks is where
  bugs hide.)
- `ctest` legs in `test/unit/test_instructor_tick.cpp`: the mouse-UP loop (0° bank pin), split-S, RAW slow
  loop. **AT-9** (`test_at9.cpp`) is the frame-rate-independence tripwire — ANY per-tick change to the aim
  quaternion gated by frame timing breaks it (that's how dead-end 3a was caught).
- The controller golden (`test/golden/controller_golden.h`) must NOT move for a camera/mouse-basis change
  (upright flight is untouched). If it moves, STOP.

## 7. Process for the fresh agent
Plan → ENTER PLAN MODE + ASK Chad the felt questions (§5) BEFORE coding → implement ONE dial → test (gate +
a mutation-verified leg) → **Chad flies it** → red-team the mechanism (adversarial-review, separate
context) → commit. Do NOT stack another mouse-basis patch. If a fix touches SPEC contracts (§9.1/§9.2),
log a §0 supersession. The honest goal: the fewest, most-principled change that makes loops/split-S feel
right — most likely on the CAMERA side, leaving the raw mouse alone.
