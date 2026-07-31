# FF-deficit attribution — SOLVED (2026-07-12)

Produced by the one-pass "final boss" workflow (two independent Opus derivations → Opus judge →
Opus verification executed **in the twin worktree** `D:\flight_sim2\seads-feel-auto`, branch
`auto/feel-research` — this sandbox untouched; plus a Sonnet mutation sweep, §below).
Autoresearch brief target #1 is **CLOSED as attribution**; the fix awaits **Chad's fly-ruling**.

## The verdict — curvature-feedforward RADIUS ERROR (confirmed <4% residual)

`ω_ff = cross(local_up, v) / ap.R` divides by the **baked planet radius** 15000, but level flight
at altitude h — and the parallel-transported aim riding the actual position motion — curve at
`V/(R+h)`. The ff over-commands pitch-down by `V·h/(R(R+h))`; the pointing loop cancels it with a
permanent nose-UP demand, parking the aim ABOVE the nose:

```
standoff = V·h / (R·(R+h)·K_theta)        (K_theta live = 3.2, not the documented 3.6)
```

At the harness's hardcoded h=3000: `h/(R+h) = 16.7%` — this **is** the observed "~18% of V/R".
This is the constitution's own class of bug (`altitude == length(position) − R`; **no baked
radius**) hiding inside the mandatory feedforward since Section 4b.

Evidence (twin worktree, new altitude-argument park probe):
- **Altitude discriminator:** park 0.030° @h=500 (latched floor) → 0.0507 @3000 → 0.0846 @8000.
- **V-family @h=3000 (all unlatched):** normalized invariant `standoff·R(R+h)/(V·h)` collapses to
  18.4–18.65° across V=200–280 vs predicted `1/K_theta = 17.9°` (the ~3% over-read is tail-window
  h-drift of the full-throttle rig — named, not yet isolated; critic gap #4).

**Killed hypotheses** (each by quantitative prediction, not vibes): trim-AoA/dα-dt (predicts 0 in
steady flight); frame under-read `|local_up×v|<V` (exactly V on the fixture; no gate touches
vertical pointing at cruise — also why lateral parks at exactly 0.0000); ZOH/tick-lag discretization
(O((V/R·dt)²) ≈ 1e-6°, four orders too small). Deadzone floor + rig-decel are TABLE CONTAMINATION
(dominate V≤140 rows), not sources — the upgraded instrument now prints tail-mean actual V/alt +
latch counts so they can never confound again.

## The fix — one line, PROPOSED on `auto/feel-research` commit `555367b46`

```
control/controller.cpp
- const glm::dvec3 omega_ff_world = glm::cross(e.local_up, s.velocity) / ap.R;
+ const glm::dvec3 omega_ff_world = glm::cross(e.local_up, s.velocity) / glm::length(s.position);
```

Bit-identical at h=0 (the knob-off arm, pinned). Post-fix the park collapses to the uniform
~0.026° deadzone-entry floor (CROSSED lo, 60/60 latched, 0.0000 lateral) at **every** V and
altitude — the 2.7× altitude spread is gone. Goldens moved **deliberately** (controller golden
re-recorded with the radius fix documented in its header; AT-11/AT-6/AT-14a test-side ff oracles
re-derived to `|position|` — each pre-fix failure verified as exactly the R/(R+h) signature first).
New tripwire in `test_cascade` ("curvature ff divides by |position|, not baked R"), mutation-
verified. **Gate 261/261 green in the twin.**

One-command re-verify (twin): `build/seads_harness.exe step pitch 20 250 2400 "" 3000`
→ closest ~0.026° CROSSED (was 0.046° hanging OUTSIDE); `"" 500` vs `"" 8000` now read the SAME floor.

## CHAD'S RULING — this is a felt change, your stick decides

Cherry-pick `555367b46` onto `feel/rudder-flick` and fly. Felt prediction: the nose stops sitting
a few hundredths high in cruise (V- and altitude-scaled — 0.085° at 8 km pre-fix), the "nose in
the MIDDLE of the aim" blocker removed at the root. Walk-back is a one-line revert. Side effect:
the `[deadzone] lo=0.03` comment's "attribute the 18% before dialing" is now satisfied — a future
deadzone shrink no longer chases this V-scaled floor. Note E2's staleness deepens: K_theta live is
3.2 (the 3.6 pair was never live).

## Mutation sweep (Sonnet, 3 isolated worktrees) — 24 mutants, 22 killed, 2 survivors

Survivors = test blind spots, with proposed closing legs (critic-ranked):
1. **M07 (real gap behind a no-op mutant):** the ROTATIONAL semi-implicit order is unpinned —
   the honest mutant is `next.angular_vel → state.angular_vel` in the quat update; the existing
   rotational leg uses constant-ω where both coincide. Close: one leg with non-zero torque so
   next≠state ω, orientation asserted against closed-form with the NEW ω.
2. **M23:** `unfold_bank` at exactly `cos_phi_theta == 0` (`>=` vs `>` returns π-fold). Close:
   one AT-0 CHECK at exact knife-edge asserting upright passthrough.
3. Broaden the new ff tripwire: probe h=8000 + V=280 points through the same oracle lambda.
4. Isolate the ~3% invariant over-read (throttle-0 or short-tail rerun; measurement, no code).
5. AT-11 oracle self-guard: pin oracle-vs-`/ap.R` ratio == R/(R+h) so a mis-derived oracle can't
   silently pass.

All 22 kills recorded with killing-test names in the workflow output (session artifact); the
catalog deliberately avoided every mutant already hand-verified in CLAUDE.md's Learned list.
