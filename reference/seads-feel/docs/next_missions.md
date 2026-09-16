> **OVERTAKEN (2026-07-08).** Mission A (RMB aim-zoom) LANDED as S9-zoom; Mission B was executed as the MB instructor pass — Chad's fly verdict: "flight kernel is near perfect now." What remains of B is `docs/mission_b_instructor_plan.md` §3. Do not restart either mission from this file; kept for Chad's verbatim words.

# Next missions (Chad, 2026-07-07 — each its OWN session + branch/worktree)

*Written at the close of the horizon-recovery milestone (S7-hrz + S7-nest, both
flown: "totally perfect"). Two missions, deliberately separated. Chad's words are
quoted or near-verbatim — the repo's most expensive error is mis-mapping them
(CLAUDE.md "Lessons learned"), so a fresh agent should ENTER PLAN MODE and confirm
the felt intent before coding either.*

---

## Mission A — RMB aim-zoom camera (small, camera-only, own branch)

Chad: *"add some right mouse button aim zoom for the camera so that I can free
look zoom to wherever I point and it holds there; when I release RMB, the view
zooms back out to normal. Also when in free look, zoom in to where the reticle is
pointed and aiming so that I can take far shots better aimed."*

- **Hold RMB → the view zooms toward where the AIM/RETICLE points** (not the
  screen center of the current chase pose), and HOLDS there while held.
- **Release RMB → zoom eases back out to normal.**
- **Works during freelook too**: the zoom target is the reticle/aim direction
  (the far-shot use case — better-aimed long shots), not the freelook orbit
  direction.
- Design notes for the implementer: this is CAMERA-ONLY (FOV + framing —
  cosmetic, downstream of the aim; RA9-safe by construction, same class as the
  chase lag). Interacts with the lens-shift frustum (`off_center_frustum` /
  `lens_shift_ndc` — the reticle-centering must stay consistent under the zoomed
  FOV) and the `[camera]`/`[ui]` config precedent (zoom FOV, ease rates = tune
  data, not code). Mouse-aim sensitivity likely wants a compensating scale under
  zoom (deg/px feels faster when zoomed) — ASK Chad; that touches the mouse gain
  (a pure gain, not smoothing) so it is legal but is HIS call. RMB is a new
  device binding (input/ + main.cpp caller glue — remember no ctest runs
  seads.exe; keep the pure pieces pinnable).

## Mission B — "Instructor to Chad's delight": coordinated high-performance
## flight + the energy game (deep research → new worktree)

Chad: *"a call for deep research on attaining coordinated high performance
flight from the cascade instructor."* This is the big one — arcade-shooter feel,
the combat egg made real. Run the `deep-research` skill on arcade flight-model /
instructor design (WT instructor, arcade energy models, rudder-assisted tracking)
BEFORE coding, then open a dedicated worktree.

**Chad's requirements (his words, lightly structured):**

1. **Rudder — fix the split personality:**
   - The keyboard OVERRIDE full-deflection rudder "exceeds this as it was a
     misunderstanding from a previous agent that thought I wanted the gross
     rudder to be way higher" — the top end **"needs a little nerfing."**
   - But the CASCADE INSTRUCTOR "needs to use more rudder (even skidding) in
     turns at high mouse-aim deflection — that is a good thing."
   - "Rollover banking turns should have rudder in it based on the angle of my
     deflection."
   - "More effective tactical use of rudder"; "I want the nose to snap to the
     mouse aim a little better."
   - (Map: `c_yaw` plant authority vs `yaw_scale`/`yaw_maneuver_frac`/`K_coord`
     cascade usage — note the LANDED ceilings: yaw_scale > 1.2 corkscrews the
     split-S (AT-15); the research must find rudder-pointing gains that DON'T
     re-open that, or reshape the fade.)
2. **Elevator / G:** "a way higher gain on the elevator"; "let the instructor
   give me high g's — up to 10 g's is okay"; "use the elevator for high-g
   turning, pull-ups, dives." (NOTE: current `n_max` = 16 — Chad's "10 g" may
   mean the FELT sustained envelope, not the instantaneous clamp; CONFIRM which
   before touching `[g_limits]`. Pitch snap comes from `c_pitch`/`n_max`, never
   `K_theta` past 3.2 — the PIO wall stands.)
3. **Banking:** "banking is good" — don't touch what works.
4. **Authority hierarchy:** "the highest flight-control deflection will ALWAYS
   come from the keyboard override key presses" — override > instructor,
   preserved as a design invariant.
5. **Stall behavior:** "I want to be able to stall out and fall, but the
   instructor regains its coordination — not spin all the way to crash — unless
   I stall near the deck, then natural consequences, I crash." (Ballistic-regime
   recovery shaping; the deck makes it honest.)
6. **The energy game (plant + tuning):**
   - "Top end speed will need to be increased."
   - "Energy retention in a zoom" increased; "less retained for high-g
     maneuvers so it balances and REWARDS EFFICIENT ENERGY MANAGEMENT."
   - "Energy rewards coordinated turns — utilization of the combat egg for
     flight performance in a dogfight."
   - "Smooth lines of flight retain more energy than when a defender zooms away
     and must dodge trailing fire — jittering defensive flying bleeding some
     energy based on the AMOUNT OF DEFLECTION of control surfaces."
   - (Map: `Cd0`/`k_induced`/thrust for top speed + zoom retention; induced
     drag already scales Cl² — the deflection-bleed idea likely wants a
     control-surface drag term in the PLANT (a `sim/` change — SPEC-supersession
     territory, and AT-12's energy gate must stay tune-independent). The
     harness instruments exist: AT-12 speed-retention printout, `ctrl_fly` CSV.)
7. **Framing:** "This will be an arcade shooter." Feel over fidelity, always.

**Process requirements:** deep-research first (cite real arcade-instructor /
energy-model prior art); then plan-mode with Chad's confirmation of the felt
intent per knob; ONE DIAL PER FLIGHT with the flight log; the drone fleet
(S8-drone) now makes the Tracking star honestly gradable — use it. New worktree;
"to Chad's (my) and hopefully other humans' delight."
