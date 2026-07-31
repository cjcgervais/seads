# Comfort attribution — WHY dogfighting disorients (auto/comfort-orient, 2026-07-16)

Status: MECHANISM ANALYSIS COMPLETE (from the shipped code paths); numeric sections marked
`[instrument]` are filled by `seads_harness comfort` baselines (docs/comfort_ledger.tsv).

## The complaint, decomposed against the shipped code

Chad: *"pressing space for a quick orient doesn't exactly go behind you — it tracks you in
the centre from a front oblique angle moving with you around the turn... only after a few
turns maybe it does that. A full loop leaves me disoriented and nauseous. Multiple enemies
need more freelook freedom but it's also disorienting."*

### Finding 1 — a bare space tap is NOT an orient command (the biggest surprise)

`app::tick` (app/instructor_tick.h:242-254): freelook release WITHOUT an override key moves
**nothing** — no aim snap, no camera re-seat (the 4d conditional reset, by design: a held
turn must survive a look-around). The only thing a bare tap fires is S7-hrz horizon
recovery (line 271-281): an open-loop roll of the aim frame toward local-up. So when Chad
"presses space for a quick orient," he gets a roll correction only — the camera FORWARD
keeps doing what it was already doing: easing toward its rest target at
`lag_base 0.2 /s + lag_gain 0.7 /(s·rad)` (config/controller.toml [camera]).

**There is no orient verb in the control scheme.** Chad has been reaching for one that
doesn't exist; the closest available composite (freelook + override tap + release) snaps the
aim to guarded VELOCITY (S7-nest D8) and lets the lag camera settle behind it.

### Finding 2 — the oblique orbit IS the lag law chasing a rotating target

The chase rest pose is *behind velocity, leaning toward aim* (`ease_chase_forward`,
render/camera.cpp; lead=1.0). In a sustained turn, velocity direction rotates continuously
(a max-rate turn at V≈150 is ~15-20 deg/s of heading change), so the rest target itself
rotates. A first-order ease chasing a target rotating at ω settles not AT the target but at
a standing PHASE LAG behind it: angle ≈ ω / rate. At `lag_base 0.2 /s` the standing error in
a 17 deg/s turn is enormous (the camera can never close more than ~small angles per second
while barely deflected — `lag_gain` helps only at large deflection). The camera therefore
rides at a standing oblique angle "moving with you around the turn," converging only when
the turn stops or after enough laps that geometry cancels — **exactly the felt report**.
`[instrument: turnsnap standing-oblique angle + convergence ticks]`

Constitutional note: `lag_base`/`lag_gain` are Chad's feel dials (the float feel was tuned
30→0.7 deliberately) — the fix is NOT to crank them. The fix is a *discrete orient event*
that re-seats the camera forward directly (bypassing the ease for one commanded moment), the
same way S7-hrz is a discrete roll event.

### Finding 3 — vertical maneuvers bank an UP-DEBT the pilot has no way to see or retire

The aim frame (and camera-up = carried aim-up, S7-cam3) is parallel-transported and
holonomy-bearing BY DESIGN (SPEC §9.1, AT-14) — after an immelmann the carried up is ~180°
rolled from the local horizon; after a split-S likewise; chained maneuvers compound with the
turn geometry. True sphere holonomy at dogfight scale is negligible (enclosed area / R²,
~1 km² / 225 km²); **the debt is the maneuver's own rotation**, kept deliberately so the
mouse basis stays raw (S7-raw ruling). The code comments name the missing counterpart: *"a
gentle roll-to-local_up correction (roll-only about forward, never poles) addresses THAT
without re-coupling the mouse to the camera"* — S7-hrz built exactly that mechanism but
wired it to ONE trigger only: freelook release. If Chad doesn't tap space after every
vertical maneuver, the debt persists indefinitely — the world stays rolled on screen, mouse
left/right feels rotated, and each subsequent maneuver compounds the mismatch. That is the
inside-out/backwards nausea. `[instrument: immelmann/splits/loop/x3 up-debt degrees]`

### Finding 4 — during the maneuver there is NO stable reference at all

Between events, nothing on screen is world-stable: the sky is space-first (near-black
zenith, thin horizon rim), terrain fills the view only when nose-low, and the HUD has no
attitude/horizon cue at the periphery. In an immelmann the horizon rim leaves the frame
early and nothing replaces it. Vection + uncontrolled visual roll with no fixed reference is
the classic sickness recipe (see docs/comfort_research.md). This is a cue GAP, not a bug —
candidates Q5 (peripheral cues) address it additively.

### Finding 5 — freelook freedom is orbit-limited and amnesiac

Freelook orbit is eye-only, clamped (`freelook_orbit_yaw_max`/`pitch_max`), resets to zero
on entry, and has no snap positions (no check-six). Multi-bogey scanning therefore costs
continuous manual orbit work, and every release forfeits the scan pose. More freedom without
more disorientation wants: wider/faster DISCRETE view positions (snap left/right/six) that
return crisply to the chase pose — discrete transitions are the comfort-preserving shape
(research memo §2).

## Numeric baselines (seads_harness comfort, iter 0 — 2026-07-16)

| scenario | metric | baseline | reading |
|---|---|---|---|
| turnsteady | standing_oblique_deg | **96.1** | in a sustained max-rate turn the camera rides ~96° off the flight path and NEVER converges (`converged 0`) — Chad's "front oblique angle tracking around the turn," as a number |
| turnsnap_tap | converged_ticks | **-1** | a bare space tap changes NOTHING about the oblique while the turn continues (Finding 1 confirmed) |
| turnsnap_ovr | converged_ticks / updebt_at_release | **-1 (6 s window)** / **111.0°** | even the freelook+override composite takes >6 s to settle behind velocity; the release carries a 111° screen-roll debt that S7-hrz retires in 107 ticks (~0.9 s) |
| immelmann | final_updebt_deg | **180.0** | the pilot exits an immelmann with camera-up FULLY INVERTED vs the local horizon — "upside down inside out and backwards," exactly |
| splits | final_updebt_deg | **180.0** | split-S identical |
| loopx3 | updebt_after_loop1/2/3 | **~0.00** | a COMPLETE 360° loop returns the carried frame upright (holonomy at dogfight scale is negligible) — so full-loop nausea is the DURING (vection, no stable cues, 110° oblique transient), NOT residual debt |
| all | nan_events / crashed | **0 / 0** | M3 legality clean at baseline |

Attribution weights, revised: the half-vertical maneuvers (immelmann/split-S — Chad's named
worst cases) are dominated by (3) the 180° up-debt; the sustained-turn complaint is (2) pure
lag-vs-rotating-target geometry; the full-loop complaint is (4) cue-gap during the maneuver.
All three named mechanisms are real, separable, and each has a distinct candidate.

## The composed thesis

Disorientation = (2) a slow oblique convergence whenever the pilot asks "where am I going?"
mid-turn, × (3) a standing screen-roll debt after every vertical maneuver with only one
obscure retirement trigger, × (4) zero stable references while it happens, × (5) a look
system that taxes attention during exactly the multi-bogey moments the pilot most needs
spatial anchoring. No single dial fixes it; the ultimate solution is the COMPOSITION:
an explicit ORIENT verb (Q3) + automatic debt retirement at sane events (Q4) + peripheral
world-stable cues (Q5) + discrete freelook positions (Q6).
