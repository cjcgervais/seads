# Autoresearch brief — the seads-feel-auto twin (2026-07-11)

Chad is bringing Karpathy's autoresearch agent-builder (lives in another codebase on this
box's D: drive) to run autonomous experiment loops against this flight kernel. **The
builder is invoked with the `/agent-builder` slash command** — use it (with this brief as
the input) to construct the experiment agent/skill for a selected target below. This file
is the standing brief for that agent: what to work on, what is FORBIDDEN, and how the two
folders relate.

## The two folders — never confuse them

| Folder | Branch | Who iterates here |
|---|---|---|
| `D:\flight_sim2\seads-feel` | `feel/rudder-flick` | Chad + the interactive Fable session — FEEL work, one dial per fly, Chad's stick is the judge |
| `D:\flight_sim2\seads-feel-auto` | `auto/feel-research` | The autoresearch loop — objective experiments only, overnight-safe |

Both are worktrees of `D:\flight_sim2\seads` (shared object store, independent checkouts).
Autoresearch findings come BACK as reports/instruments/attributions cherry-picked into the
feel branch deliberately — never as direct dial edits to `feel/rudder-flick`.

## THE HARD GUARDRAIL (constitutional — CLAUDE.md, the tuning discipline)

**No feel-dial tuning against harness numbers, ever.** The harness explains a score; it
never replaces the stick. An overnight loop that "optimizes" `aim_sensitivity`, `K_theta`,
`lean_gain`, `n_max`, camera lag, etc. against step-response or park metrics produces a
plausible-but-wrong table and is the most expensive failure mode this repo knows. Feel
dials move ONLY through Chad's hands in the feel sandbox.

## Selected objective targets (machine-checkable fitness — the sanctioned list)

1. **THE FF-DEFICIT HUNT (first, highest value).** The nose's park has a purely VERTICAL,
   V-scaled equilibrium: the pointing loop permanently tops up ~18% of the curvature
   feedforward V/R (measured 0.031° @V80, 0.028 @V140, 0.048 @V220, 0.050 @V250 — lateral
   exactly 0.0000 since S-wvane). This is the remaining blocker for "the nose sits in the
   MIDDLE of the aim". The question has a TRUE answer: WHY does pointing carry ~18% of V/R
   in steady flight? Candidate hypotheses to instrument and kill/confirm: rate-loop
   integrator steady-state vs the rotating demand; the AoA-above-velocity geometry vs the
   transported aim; an ω_ff frame/magnitude subtlety (`control/controller.cpp`, the
   `(local_up × v)/R` term); the aim-transport vs nose-rotation mismatch over the tick.
   Deliverable: the attribution + (if it is a correctable deficit) the one-line fix
   PROPOSAL with instruments — not a landed change. Success metric: the dwell-probe
   vertical park (the `seads_harness step` PARK readout, built 2026-07-11) explained to
   <2% residual, or the fix candidate demonstrated on a branch with the park collapsing
   toward the pointing floor (~0.006-0.01°).
   ⚠ UPDATE (S-dampff, SPEC §0, landed 2026-07-11 same day): the "rate-loop integrator
   steady-state vs the rotating demand" candidate is now MEASURED AND ELIMINATED — the
   pitch damping feedforward (damp_ff_pitch 1.0) makes rate tracking DC-exact with the
   integrator idle, and the standoff SURVIVES essentially unchanged (0.035° @V140 /
   0.056° @V250 post-ff vs 0.028/0.050 pre). The deficit is upstream of the inner loop:
   the remaining candidates (AoA-above-velocity geometry vs the transported aim; ω_ff
   frame/magnitude; per-tick transport-vs-rotation mismatch; quasi-static trim drift
   dα/dt under acceleration) are the live list. Instrument against damp_ff_pitch=1.
2. **The stability atlas.** Sweep (K_theta, K_w_*, K_wi_*, c_pitch/c_yaw/c_roll,
   yaw_scale·K_theta) space against the step harness (`seads_harness step <axis>`):
   settle/overshoot/REVERSALS (ring) + the dwell/rest probes (hunt) at several V. Output:
   the measured wall surfaces (where the ring/hunt boundaries ACTUALLY sit on today's
   plant — the documented walls are historical: K_theta 3.2 was measured on an older inner
   loop; the AT-16 β-walls are superseded by S-wvane, SPEC §0). This gives future feel
   rungs honest headroom numbers WITHOUT flying blind — explaining walls is the harness's
   sanctioned job.
3. **Drone/bandit improvement.** `drone/drone.h` — the P-only bank-hold autopilot turns
   uncoordinated (S-wvane honestly gentled the fleet ~14%); its kCoordP retune is deferred
   and OBJECTIVE (tracking error, turn-rate-at-bank, time-to-intercept vs a scripted
   chase). A better bandit = a better Tracking star for Chad. Keep it pure (a function of
   state, the §5 seam, no clock).
4. **Mutation-coverage sweeps.** Automate the repo's mutation-verify discipline: sweep
   candidate mutants over `sim/` + `control/` and report any that survive the full gate —
   each survivor is a test blind spot (this repo's lesson list is FULL of hand-found
   instances; a loop finds the tail).

## Ground rules for the loop, in this repo's terms

- Gate = `cmake --build build --config Debug` + `ctest --test-dir build -C Debug` (Ninja,
  MinGW — commands in CLAUDE.md; test exes link -static; rm a locked test exe before
  relinking or mutation runs test a STALE binary).
- Goldens load the live TOML: any table edit moves them BY DESIGN. In the auto twin that
  is fine on its own branch; NEVER push table edits at `feel/rudder-flick`.
- Read `docs/HARNESS.md` (verification SOP), SPEC §0 (the supersession log — S-wvane is
  the newest mechanism), and the `dogfight-systems` skill (the feel mental model) before
  the first experiment.
- Every finding needs the instrument that would let a human re-verify it in one command.

## State at brief-writing (2026-07-11)

S-wvane landed + APPROVED on the stick (fuselage side-force — "the rudder is doing its
work now"); gate 258/258 at `feel/rudder-flick` HEAD; the pitch thread (n_min ladder +
c_pitch attribution) and E2 (K_theta pair) are staged in the feel sandbox for Chad's next
sessions; the ff-deficit is the named blocker for "middle" and this brief's target #1.
UPDATE 2026-07-11 (later same day): the pitch thread executed — n_min −9 (Rung P1) +
**S-dampff** (SPEC §0, pitch damping feedforward — the carry-past attribution; read the
§0 entry before target #2, its superseded-knowledge ledger moves several documented
walls). **E2 is STALE** (the K_theta 3.6 pair was derived on the sagged plant — the
atlas in target #2 should re-measure the K_theta wall AT damp_ff_pitch=1 first); the
K_wi-drives-overshoot guidance is superseded on ff'd axes.
