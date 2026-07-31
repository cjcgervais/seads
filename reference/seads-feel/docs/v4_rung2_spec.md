# V4 RUNG 2 SPEC — the FULL-TRAVERSE REBOUND capture (S-rimshot)

Program: `program.md` ask 2 — Chad's FINAL spec, 4 iterations, the last two verbatim:
"it should hit the opposite edge of the aim circle then dead centre directly and faster —
right now it approaches the edge and slows in" and "It shouldn't slow down for the drop —
it should hit the opposite rim then centre. Instant." Relaunch restatement: "it just goes
straight to the opposite side of the aim circle without slowing down and like a dead blow
reflects direct to centre instantly." The superseded ζ-based framings are DO-NOT-BUILD.

## The profile (two phases, at a genuine snap-capture event ONLY)

- **Phase 1 — CARRY THROUGH, no slow-in.** The nose does NOT decelerate into the aim: it
  carries its rotation rate through the capture point and swings PAST, to nearly touch the
  OPPOSITE EDGE of the on-screen mouse-aim circle. Overshoot amplitude is scaled to the
  AIM-CIRCLE GEOMETRY (the reticle ring), NOT a percentage of the step.
- **Phase 2 — THE DROP IS INSTANT.** From the far rim, the return to DEAD CENTER is
  completely UNSHAPED — no taper, no ease, no managed settle. Maximum available authority
  the whole way, terminated by the plant-inverted hard stop AT center. At aim-circle scale
  (~0.25-0.3°) a max-rate return is ~0.03-0.1 s — ONE beat: rim → center → done. Any
  visible easing into center is OFF-SPEC. Never faked display-side (S-retclamp lesson: the
  nose marker never lies about the plane).

## STEP 1 — ATTRIBUTION (do this FIRST, ledger row BEFORE any mechanism code)

Instrument which term shapes today's approach. The suspects, from the cascade
(control/controller.cpp seek_law): the braking sqrt branch `sqrt(2·k_b·alpha_max·|e|)`
(decelerates the flick), and the LINEAR outer taper `K_theta·e·(1+expo·e)` (owns small e —
exponential creep, τ ≈ 1/K_theta ≈ 0.31 s). Extend `seads_harness step` (or a CSV
post-process) to report, per tick of the approach from a >30° parked-aim step: |e|, |ω|,
and WHICH seek_law branch is active (linear/parabolic vs braking sqrt vs w_max vs the G/AoA
clamp). Report the handoff radii (where braking hands to linear) and the rate profile over
the last 2° of approach at V ∈ {140, 220}. Ledger row `2.0 capture-attribution` with the
branch-handoff numbers. Design phase-1 against what the numbers actually say.

## The mechanism (controller state machine — control/, dial-gated)

New `[capture]` config section + ControllerParams + Internal state. A hysteretic
snap-capture event machine owning ω_des on the pointing axes during the event:

- **ARM**: err > `snap_on` (ship 30°, hysteretic pair with `snap_off` ~25°) while the aim
  is MOVING or immediately after (the big deflection exists). Never arms during smooth
  small-error tracking (memo trap #10).
- **ENGAGE (phase 1)**: the aim PARKS (in.aim_moved false) while armed and the nose is
  closing. From engage until the far rim: the pointing demand does NOT taper — suppress the
  braking-sqrt cap and the linear taper on the CLOSING axis; hold the demand at the
  carry-scaled incoming rate (`carry` × the rate at engage, clamped by w_max + the G/AoA
  envelope — physics wall stands). The deadzone zeroing MUST NOT truncate the event: the
  machine owns the pointing demand through the crossing (the deadzone latch is deferred
  until the event completes — spec the handoff exactly and pin it).
- **RIM DETECT**: the nose crosses the aim (the error component along the approach axis
  flips sign) and the far-side error reaches `rim_frac` × `circle_deg` (ship 0.85 × 0.27°;
  "nearly touches" band 70-95%). Phase 1 ends. (If the envelope can't carry that far — low
  V, heavy G clamp — the rim bound simply isn't reached; fall through to phase 2 at the
  actual apex: the honesty wall, name it on the fly card.)
- **RETURN (phase 2)**: ω_des on the closing axis = −sign × `return_w` (ship 60°/s, dial),
  clamped by the envelope, UNSHAPED — no taper as center approaches. Exit at center
  crossing (error along the return axis ≤ 0 or |e| < deadzone_lo): hand ω_des back to the
  normal law THAT TICK — at e≈0 the cascade commands ~0 and the plant inversion + damping
  ff kill the residual rate in the inner-loop constant (the "dead blow" stop; the residual
  past-center drift at this scale is sub-pixel — measure it, report it, never ease it).
- **DISARM legs** (each hysteretic/edge-clean): event completes (center exit); the aim
  MOVES again mid-event (hand re-engaged — abort to normal law instantly, the pilot owns
  it); override/freelook/ballistic/grounded (abort + reset); err re-grows above snap_on
  mid-event (the aim was re-flicked — re-arm fresh).
- `carry = 0` is the STRUCTURAL OFF: the whole machine never arms, every expression tree
  bit-identical legacy (the S-hrz rate=0 pattern). Ship `carry = 1.0`.
- **Dials**: `carry` (0=off / 1=full incoming rate), `rim_frac`, `circle_deg` (the
  on-screen ring's angular radius at 1× zoom — tune data; the 9 px ring ≈ 0.27° at 1080p
  60° fovy — VERIFY against the actual render reticle constant and document), `return_w`,
  `snap_on`/`snap_off`.

## Axis scope

The event acts on the POINTING axes (pitch primary; yaw via its pointing path). Roll is
untouched (bank geometry). The carry/return demands pass through the EXISTING AoA/G clamp
sequence and yaw ceiling — protection is never bypassed (same shape as S-aimff's insertion).
Interplay with S-aimff: the aim is PARKED during the event, so ω_aim = 0 — orthogonal by
construction; state it in a comment and pin with one leg (aim_ff on + capture event →
identical to aim_ff off during the event).

## Instrument (extend `seads_harness step` — circle-scaled capture readout)

New printed block after the existing PARK block, computed from the same traces:
- approach rate AT the capture point (|ω| when e first < circle_deg) vs peak approach rate
  — the NO-SLOW-IN proof (ratio ≈ carry at the dial, ≈ taper at legacy);
- overshoot amplitude in RIM units (max far-side excursion / circle_deg) — the "nearly
  touches" band check (0.70-0.95 at the dial);
- reversal count across the event (MUST be exactly 1);
- return time (far-rim apex → first center crossing) and terminal drift (mean |e| next
  0.25 s);
- machine-greppable: `CAPTURE V=<> step=<> approach_ratio=<> rim=<> reversals=<>
  return_ms=<> drift_deg=<>`.
Baseline rows (carry=0) then dial rows: V ∈ {140, 220}, steps {45°, 90°}. Ledger rows
`2.1 capture-baseline` / `2.2 rimshot`. The `track` scenario MUST show no change (smooth
tracking never arms — run it and say so in the ledger row note).

## Tests (trap classes)

1. Event fires: >30° step, parked aim → machine traverses ARM→ENGAGE→RETURN→done; exactly
   one reversal; overshoot in the rim band (config-relative: bounds from `[capture]` +
   circle_deg, never constants).
2. carry=0 bit-identity: same scenario, byte-identical Output vs pre-diff arithmetic
   (gated-tree discipline).
3. NEVER during tracking: constant-rate moving aim (aim_moved true) at any error → machine
   never leaves IDLE (state probe or behavior-identical leg).
4. Abort legs: aim moves mid-CARRY → normal law that tick; override mid-event; ballistic
   entry mid-event. Each with non-no-op premises (REQUIRE the event was live first).
5. Envelope: carry demand at low V stays within G/AoA clamps (config-relative).
6. Deadzone handoff: after center exit the deadzone latches normally (trim held; no
   re-fire; the event cannot re-arm from rest — hysteresis pinned OPEN-LOOP on a scripted
   err/aim_moved sequence, the dwelling-signal discipline).
7. Goldens: ctrl_golden's steps are 25°/20° — BELOW snap_on, so goldens must NOT move even
   at the shipped dial. If any golden moves → STOP, report. AT-2/AT-15/AT-16: audit which
   legs drive >30° parked-aim steps with toml-loaded params; if one legitimately enters the
   event and its settle/reversal bound fails, do NOT silently retune — report it as an
   intended-move candidate with the number, and leave the test red only if unavoidable
   (prefer scoping the fixture below snap_on ONLY where the leg's purpose is legacy-shape
   pinning, with a comment; the parent session arbitrates).

## Constraints

- NEVER touch sim/. The raw mouse→aim path untouched. No smoothing anywhere in the event
  (the carry/return are DEMAND replacements, not filters). Every gate hysteretic. No bare
  numbers in control/ — all through params/toml (degrees at the boundary). The event
  reshapes ω_des ONLY — inner loop, integrator anti-windup, coordination, curvature ff all
  untouched. Comments explain constraints in the file's voice.
