# SEADS — Harness & Verification SOP

Companion to `SPEC.md` (the constitution). This file is the **how**: the per-section loop, the
deterministic gate, the test tiers, the headless harness, and the acceptance-test contract.
It is deliberately light — the harness must stay visibly lighter than the kernel it harnesses.
Nothing below describes infrastructure that exists; everything is built in the section that needs it.

---

## 1. The per-section loop

Each SPEC §15 section is a goal with a definition-of-done:

```
PLAN → IMPLEMENT → VERIFY (gate hook, free) → RED-TEAM (important mechanisms only)
     → GATE passes → tag commit → PERSIST lessons to CLAUDE.md ## Learned → /clear → next
```

Do not start section N+1 until N's gate passes. The tagged commit at each green gate is the rollback
point. Never tune two loops (inner/outer) in one session.

| SPEC §15 section | Gate | Red-team focus |
|---|---|---|
| 1 Ballistic point | orbits/falls correctly; asserts green | gravity == −normalize(pos) everywhere; no fixed down axis |
| 2 Flyable aircraft | golden committed; stall + zoom behave | integrator order; quat renorm; **AT-18 derived-vs-measured α_max** |
| 3 Render + camera | camera-up = raw local_up (raw mode; the §9.2 frame-carried camera arrives in 4), stable, poles incl. | render reads-only; no slewed basis anywhere near aim (SPEC §9.1) |
| 4 Instructor | AT-0..AT-18 green (see §5) | the full contract; S1/S3; H1–H4; **the cross-model audit lands here** |
| 5 HUD | readouts correct-frame | G/AoA velocity-relative, local_up-aware |
| 6 Acceptance | SPEC §14 objective 1–5 | poles; circumnav; **holonomy pair**; **apex golden**; crash-reset |
| 7 Tune for feel | Delight Test; flight-log stars trend up | one knob per flight; tunnel metrics read alongside, never as gates |

## 2. The deterministic gate (build it in Section 1 — it's the walk-away enabler)

`.claude/hooks/gate.sh` (hooks on this box run under Git Bash; POSIX sh; note the **`-C Debug`** —
ctest finds nothing without it under the Visual Studio multi-config generator):

```sh
#!/bin/sh
set -e
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`.claude/settings.json`:

```jsonc
{ "hooks": {
    "Stop":         [ { "hooks": [ { "type": "command", "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/gate.sh", "timeout": 300 } ] } ],
    "SubagentStop": [ { "hooks": [ { "type": "command", "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/gate.sh", "timeout": 300 } ] } ]
} }
```

Non-zero exit blocks the stop and prints the failure into the transcript. Run configure + build + test
by hand once before trusting the hook.

## 3. Verification tiers

- **T1 — deterministic (every change, via the gate, ~free):** build clean; **invariant asserts**
  (SPEC §6.1) compiled into debug; unit tests (Cl(α) incl. cap, drag ∝ v², gravity toward center from N
  positions, quat norm over N ticks, NaN guards).
- **T2 — goldens (every change, ~free):** fixed start state + fixed input sequence → N ticks → snapshot,
  committed, compared with float tolerance (single C++ runtime — no cross-engine vectors needed).
  A moved snapshot **halts the loop**: confirm intent before committing.
  **Controller goldens use the real spherical `sim::step()` as plant — never a flat stub.** On a sphere
  the *correct* `Inputs` stream contains constant corrective rotation; a flat plant would flag correct
  behavior as drift, or bless a controller that climbs off the world.
- **T3 — adversarial red-team (gates + important mechanisms only; costs tokens):** a reviewer in a
  **separate context** (subagent or cross-model) attacks the diff against §6's target list, returns
  P0–P3 findings with file:line evidence. Never self-review in the authoring context.

## 4. The harness (test/harness/) — the wind tunnel, sphere-native by construction

There is no separate rig to audit and no toy plant: **the pure spherical `sim::step()` is already a
headless function — it IS the plant.** The harness is a small driver, built in Section 1–2, grown with
the controller:

- **Step injector:** scripted `targetDir`/`AimIntent` sequences (steps, flicks, override-release,
  hammerhead) driven through `control::step()` + `sim::step()` at fixed dt.
- **Telemetry:** per-tick CSV via `std::ofstream` (C++ has a filesystem — no export contortions):
  e, ω, ω_des, Inputs, integ, regime/push/ballistic flags, φ, β, AoA, V, altitude, **true load factor
  alongside the G-proxy** (SPEC §16 CQ1).
- **α_max measurement:** full deflection per axis, read ω̇ — feeds the braking law, and is asserted
  against the analytic value (AT-18).
- **Open-loop replay:** the §9.7 purity contract means recorded per-tick state fed back through
  `control::step()` reproduces every decision — a bad fly-test becomes a repeatable unit test.
- It prints settle time / overshoot / reversal count, so tuning is numbers, not vibes.

**Instrument heavily, gate lightly:** harness metrics (overshoot, settle, sideslip) are read *alongside*
the flight-log stars — they explain why a pass felt better; they never become pass/fail gates competing
with the Human Delight Test. The only hard gates are T1 + T2. Telemetry's whole job is the question
goldens can't answer: "still compiles, flies worse."

## 5. Acceptance tests (the contract; run via harness, confirmed in-app where feel matters)

AT-0 runs before any tuning. "Budget" = AT-7's plant-bound formula `max(2°, ω₀²·I/(2·τ_max) + 1°)`.

| ID | Requirement | Pass criterion |
|---|---|---|
| AT-0 | Sign conventions vs glm/raylib reality | canonical targets (right/left/up/down-15°/down-60°/astern) → ω_des signs per SPEC §7 table; **injected-state**: β≠0 → yaw sign; (φ_held−φ)≠0 in FINE → roll sign; **through-extraction**: synthetic `SimState` at known bank/sideslip driven through the shared `control/extract.*` → same signs (tests the glue instead of injecting past it) |
| AT-1 | Cursor connected, zero smoothing | ω_des ≠ 0 same tick as mouse move; audited: nothing smoothed on mouse→aim→Inputs or mouse→camera-rotation |
| AT-2 | No oscillation | 30° step at cruise: ≤1 overshoot ≤1.5°; settle <0.5° within frozen `T_settle`; no limit cycle |
| AT-3 | Rapid displacement | 180° flick: aim/camera instant; turn at clamps; latched direction stable; arrival per AT-2 |
| AT-4 | Never hunts | cursor still 10 s: `Inputs` constant at trim (NOT zero — trim + feedforward); nose jitter <0.05° RMS |
| AT-5 | Natural settling | last 5° of a step: \|Input−trim\| monotone decreasing, exponential envelope |
| AT-6 | Heavier at speed | same offset at V and 1.6V: achieved ω ratio matches the ω_max(V) clamp prediction |
| AT-7 | Override-release catch | full pitch override 2 s, release at several e₀: overshoot ≤ budget; ≤1 reversal; exponential ω decay |
| AT-8 | Freelook compose, no snap-back | freelook + keyboard 180° roll, release both: commanded Inputs continuous; release step ≤ budget; target ≡ nose; ease ≤300 ms outside loop; **no transient inverted mouse control during ease-back (released looking aft) — CQ2-invariant: holds whichever mechanism (suspend vs easing-frame) is ruled** |
| AT-9 | Frame-rate independence | same scripted run at 30 vs 240 render fps: identical sim trajectory (accumulator correctness) |
| AT-10 | Windup bounded & unwindable | cursor pinned 10 s then centered: integral capped; return meets AT-2 |
| AT-11 | AoA protection | full-up at low speed + injected inverted-stall AoA: no departure; protection obeys both G floor and ceiling; **inactive at cruise AoA**; override *can* exceed (by design) |
| AT-12 | Predictable energy | sustained max-rate turn 10 s: speed retention ≥ target % (a PLANT requirement — tune drag with a number, on purpose) — **SUPERSEDED as mechanized (CLAUDE.md ## Deferred):** retention is a PRINTOUT, not a gate (gating an untuned target re-creates the AT-6 phantom-chase); the tune-INDEPENDENT gate is the per-tick linear-work energy reconciliation. Set the retention target in §7 from the flight log, then judge by feel. |
| AT-13 | Clean rebirth | die mid-override cursor astern → respawn: reset ran, no dive at old cursor; crash at altitude ≤ 0 → reset; banked spawn → no uncommanded leveling roll |
| AT-14 | Sphere trio | (a) level flight 60 s: zero deadzone limit cycle (feedforward live); (b) great-circle lap: transported aim/camera return ≈ identity (transport accumulates no spurious rotation); (c) banked small-circle circuit: holonomy = enclosed area/R² within tolerance (tens of degrees — the gameplay phenomenon); freelook-held aim transported throughout |
| AT-15 | Push-vs-roll GEOMETRY gate (S7-push) | below-nose & ahead of the wing-line → **noses down** (push, wings held, bank change <10°); past vertical (behind the wing-line) → rolls through inverted; V-independent (gate decision); mid-push **geometry** exit (target goes behind) → clean handoff; no dither at any leg |
| AT-16 | Banked tracking | 45°-banked turn, ±3° offsets 15 s: no FINE↔MANEUVER limit cycle; bank within ±5° of capture; **\|β\| ≤ threshold throughout (coordination verified closed-loop — C7)** |
| AT-17 | Apex / BALLISTIC golden | scripted hammerhead through v < v_ballistic: no NaN anywhere; nose recoverable (attitude-hold acts); AoA limiter + coordination gated off below floor; clean hysteretic re-entry to the cascade; energy honest (falls, no hover) |
| AT-18 | Authority single-count, both ways | **(a) plant side:** per axis, α_max measured (injector) vs derived (`c·max(q, q_att_floor)·δ_max_eff/I`) agree within tolerance at two speeds, **one below the floor crossover (~24 m/s with appendix values — this verifies the floor itself)** — a mismatch means q applied twice (H1) or the params forked. **(b) controller side (round-trip):** for a grid of (τ_cmd, V) below saturation, the controller's emitted `Input` fed back through the sim torque formula reproduces τ_cmd within tolerance — catches a second q/δ division in `control/` that (a) is structurally blind to |

**AT-15 geometry gate (S7-push, 2026-07-05):** the push/roll decision is now on TARGET GEOMETRY, not
the −G budget: push (nose down) while the below-nose target is ahead of / just past the wing-line
(`target_body.z ≤ −cos(down_enter)`), roll into the loop once it goes behind (`z > −cos(down_exit)`).
The config band is `[push_gate] down_enter/down_exit` = 88°/96° (turnover ~92°, just past straight-down),
so the mechanized legs use 60° (push) / 135° (roll-through, past vertical) and `test_cascade` uses
6/60/85° push, 135° rolls. **When Section 7 retunes `down_enter/down_exit` (or `[regime] blend_lo`, a
hidden dependency — engagement needs `err > blend_lo` for `have_bank`), recalibrate the boundary-tied
constants AS A SET; the enumerated list lives in the AT-15 CALIBRATION MANIFEST banner in
`test_acceptance.cpp`.** Moving the band trips several legs' REQUIRE premises at once — by design (they
fail LOUD, not silently vacuous).

**Golden vectors are recorded only after AT-0 and the 4b fixes land** — goldens recorded from wrong code
freeze the bugs in.

## 6. Red-team target list (what T3 attacks)

- **Sphere (S1/S2/S3):** any fixed up/down axis, cached `up`/`north`, `(lat,lon,heading)`, tangent-plane
  step? Every "up/level/bank/vertical/horizon/down/cosΦθ" — in `control/`, in harness/telemetry
  measurement code, **AND in the shared state-extraction `control/extract.*`** — recomputed from
  `local_up` each tick? **App loop and harness compile the SAME extraction function — a second
  implementation anywhere is a finding.** Closed-loop rigs use the spherical `step()`?
- **Loop integrity (H1–H4):** q applied exactly once — both AT-18 legs green? No render/camera state inside `control/`?
  ≤1 smoothing stage per signal path — **enforce by grep: `lerp|slew|filter|smooth|ease` on every path
  into the aim update and the control loop**? Aim basis raw (SPEC §9.1)? dt = fixed sim_dt everywhere?
- **Controller state:** anti-windup unwinds (not freeze-only)? integrator frozen-not-zeroed on override?
  `φ_held` captured on all three edges? every gate/regime/latch leg hysteretic? deadzone holds trim?
  feedforward added post-deadzone? pursuit derived in-core? `vMin` on every V-division? ballistic
  entry/exit hysteretic; AoA/coordination gated below floor?
- **Numerics:** semi-implicit Euler order; quat renorm; NaN guards (`normalize(v)` at v≈0); lap closes.

`.claude/skills/adversarial-review/SKILL.md` (build in Section 2): *reviewer runs in a separate
context; attack the diff against HARNESS §6; report P0–P3 with file:line + fix; iterate ≤5 rounds or
until no P0/P1.*

## 7. Division of labor & escalation

**Opus 4.8 builds** (high reasoning; xhigh only for sections 2, 4b, 6). **Fable 5 is the second model:**
one batched audit at gate 4d (controller + harness against this contract, esp. S3 and AT-18), optionally
one more after section 6. Consult-packet discipline: tight context, narrow questions, cite files/lines,
defined output format — never paste the project. The two pre-locked consult questions (SPEC §16) are
RULED — CQ1: flat `cosΦθ` credit + true-n telemetry; CQ2: suspend mouse→aim during ease-back; each
stays a data/one-line flip if the flight log demands a re-ruling.

## 8. Flight-Test-Log (`docs/flight-log.md`) — operationalizes the Delight Test

One variable changed per flight; hypothesis → expected → observed; harness numbers alongside the stars
(they explain the score, they don't replace it). Tag builds your hands liked (`golden-NN-<why>`) —
never lose one.

| Date | Change (one knob) | Roll ★ | Pitch ★ | Tracking ★ | Oscillation ↓ ★ | Transition ★ | Notes (harness metrics) |
|---|---|---|---|---|---|---|---|

**Tuning order (one knob per flight):** 0) injector + AT-0 + α_max → 1) inner loop alone (scripted
ω_des steps; raise K_ω to slight overshoot, back off 30%) → 2) outer (raise K_θ till AT-2 first fails,
back off 30%; freeze `T_settle`) → 3) braking + clamps (k_b via AT-7; n/p/AoA limits; K_aoa
inactive-at-cruise) → 4) blend/deadzone/latches/gate → 5) feel (yawScale, ramp, camera spring, energy
target). If oscillation ever reappears: raise K_ω or lower K_θ — never anything else first, subject
to the ZOH ceiling `K_ω·sim_dt/I ≤ 0.5`: at the ceiling lower K_θ instead (past it, raising K_ω
*causes* the oscillation — the one mode where the mantra inverts).

---

*The rule underneath everything: prove the core, then grow — and the verification is sphere-correct, or
it is worthless.*
