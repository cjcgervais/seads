> **FROZEN SNAPSHOT — two+ milestones stale (banner added 2026-07-08).** The mental models and traps remain sound teaching, but every status/"not yet built" claim has drifted (e.g. the target drone EXISTS — SPEC §0 S8-drone). Trust SPEC §0 + CLAUDE.md `## Threads` over any status here.

# SEADS — The Teaching Companion

*An annotated, red-marked walk through the codebase: the mental models, the why,
and the traps — one engineer teaching another.*

---

## 1. How to read this doc

This is the professor's marked-up copy of `SPEC.md`. The spec is the constitution
(SPEC.md:4-6); `docs/HARNESS.md` is the verification SOP; `CLAUDE.md` is the working
memory. This document is none of those — it is the *lecture*: the ideas explained in
plain language, with margin notes pointing at the exact line of code where each idea
lives or nearly died.

**Snapshot:** HEAD `32f1fcf` ("Section 7 docs: fix three contradictions from Fable's
farewell sweep"), 2026-07-05. Sections 1–6 are gated and tagged (`section-1-gate` …
`section-6-gate`); the Section-7 *prep* queue is closed (all 7 items,
pre_spec_audits/section7_prep_handoff.md:25); **Section 7 (tune for feel) is just
entered**, gate **139/139** green. Everything below describes this moment; Section 7
will move numbers (that's its job) but not mechanisms (SPEC.md:511-512).

**The annotation vocabulary** (used as callout lines throughout):

- `💡 INSIGHT` — a non-obvious idea worth internalizing
- `⚠ TRAP` — a mistake that was made or nearly made, and how it's guarded
- `🔬 HOW WE KNOW` — the test / mutation / audit that pins the claim
- `🧭 FOR AGENTS` — working guidance specific to this repo
- `∮ DEEP` — an optional deeper dive for the curious

Citations are `file:line` against this HEAD. When Section 7 edits land, trust the
mechanism descriptions and re-verify the line numbers.

---

## 2. The one bet

SEADS exists to answer exactly one question (SPEC.md:69, CLAUDE.md:4-5):

> **Does mouse-aim dogfight flying on a small sphere feel good?**

Not "is it accurate." A WWII-class fighter on a planet of radius **15 km** — small
enough that the horizon bulges tactically, a lap takes ~9 minutes (SPEC.md:465), and
"straight" flight visibly is a great circle. The bet is that *flight that can't exist
on a flat map* (SPEC.md:88-89) plus War-Thunder-style mouse aim composes into
something delightful. The plant (real energy physics) and the instructor (the
mouse-aim controller) are judged **together, by feel** (SPEC.md:70-74).

Everything unusual about this repo follows from taking that bet seriously:

- The **planet radius is a config knob**, not a fact (config/aircraft.toml:7,
  SPEC.md:155). If curvature isn't tactical, turn the dial.
- The objective tests are *instruments*, never the gate. The gate is the **Human
  Delight Test** (SPEC.md:481-487): numbers explain a score, hands assign it.
- If everything passes and it still feels bad, that is defined as *success of the
  kernel* — iterate coefficients and gains, never the renderer (SPEC.md:486-487).

`💡 INSIGHT` — The scope fence (SPEC.md:97-99) is a feature, not laziness: no
weapons, no netcode, no terrain. Every one of those would dilute the signal on the
one question. "Done" is 139 green tests *and* a subjective yes (SPEC.md:101).

---

## 3. Four mental models

Internalize these four and the whole tree reads naturally.

### 3a. The sphere — there is no down

On a flat map, "down" is an axis and "level" is a plane. Here, **down is a function
of where you are**: `local_up = normalize(position)`, recomputed *every tick, at
every site* — physics, controller, HUD, telemetry (SPEC.md:151-153, 160-168).
Gravity is `p.g * gravity_dir(state.position)` computed fresh in the step
(sim/step.cpp:28), and an assert re-derives it independently every tick
(sim/invariants.h:28-36).

Why so absolutist? Because the failure mode is *silent*: cache an up vector, or keep
a `(lat,lon,heading)` triple, or move on a tangent plane, and you get a game that
runs fine and is secretly flat-earth-with-a-curved-skin (SPEC.md:171-174 — the
banned-patterns list). The sphere's gameplay value evaporates without a crash to
tell you.

The airplane itself is just `position vec3 + orientation quat`, world Cartesian,
planet center at origin (sim/state.h:29-40). No poles, no seams, no special cases:
the pole tests (test/unit/test_acceptance.cpp:917, 928) fly *over* the poles and
find nothing there — which is the point.

`⚠ TRAP` — There is exactly **one sanctioned exception** to "recompute from
local_up": the aim/camera frame's up is *frame-carried* (SPEC.md:161-165). That's
not a leak — it's the whole subject of model 3d below. Everything else that caches
an up is a finding.

`🧭 FOR AGENTS` — Before touching anything that says "up", "level", "bank",
"horizon", or "down": grep is your friend, HARNESS.md:134-139 is the red-team target
list, and the phrase to hold in mind is *"a flat instrument certifies a flat
controller"* (CLAUDE.md:224-226). That includes test code. Especially test code.

### 3b. The seam — purity is load-bearing

Four layers, one rule (SPEC.md:105-117):

```
input/    device → intents          app/      fixed-dt accumulator, wiring
control/  PURE  step() → Inputs     render/   reads state, NEVER writes
sim/      PURE  step() → state'
```

`sim::step(state, inputs, dt) -> state'` (sim/step.cpp:11) and
`control::step(state, input, internal, params, dt) -> (Inputs, internal', telem)`
(control/controller.h:176-178) are pure functions: no I/O, no clock, no globals.
`Internal` is passed in and **never mutated** — a new value is returned
(control/controller.h:101-106; pinned by test/unit/test_cascade.cpp:417).

Why purity here is not hygiene but *strategy*:

1. **The plant is its own wind tunnel.** There is no mock, no test double: the
   headless harness drives the real spherical `sim::step()` (HARNESS.md:73-76). A
   whole class of "the test rig diverged from the game" bugs is unrepresentable.
2. **Determinism gives you goldens.** Same state + same inputs = same trajectory,
   bit-for-bit, so a scripted flight can be snapshotted and compared forever
   (HARNESS.md:61-66).
3. **A bad fly-test becomes a unit test.** Record per-tick state, replay through
   `control::step()`, and every decision reproduces (HARNESS.md:84-86).

The seam also carries a physics contract: **dynamic pressure is applied exactly
once, in the sim** (SPEC.md:217-224). The plant owns the entire authority model —
`τ = c·max(q, q_att_floor)·δ_max_eff(V)·Input` (sim/step.cpp:86-98) — and the
controller *inverts* that exact formula with the exact same params
(control/controller.h:188-193), clamping only to ±1. The floored expression lives
in ONE file (sim/aero.h:25-35) read by all four consumers: plant, inversion,
braking law, and the AT-18 analytic check (sim/aero.h:10-15). A second copy
anywhere is the "H1 fork" — the repo's most-guarded class of bug.

`💡 INSIGHT` — Plant inversion is a *feel decision*, stated in the spec on purpose
(SPEC.md:229-233): mouse-aim flight cancels the v² "mushy-when-slow" gain by design
(mushiness re-emerges as deflection *saturation* — the loop pegs Input at ±1),
while keyboard override keeps raw v² scaling and gets compression "for free". Slow
feels sluggish through the rate, not through a softened stick.

`⚠ TRAP` — The seam even forbids *sharing a guard variable*. Both sim and
controller need "last valid velocity direction" at v→0; each holds its **own** copy
(sim/state.h:35-39, control/controller.h:106-109) through the one shared predicate
`sim::guarded_dir` (sim/aero.h:61-65). A test that aliased the two copies passed
while the controller was illegally reading the sim's — the fix was to *poison* the
copy the test claims isn't read (CLAUDE.md:277-280).

### 3c. The controller cascade — why not just a spring?

The naive mouse-aim controller is a positional spring: `torque = K·angleError`.
On a rotating body that is an undamped harmonic oscillator, and any latency makes
its damping *negative* — the rubber-band feel (SPEC.md:250-254). The fix is what
every production flight controller does — a **cascade**:

```
angle error → [outer: braking-aware ω_des + clamps] → rate error → [inner: PI + plant inversion] → Input
```

Walk the pipeline in control/controller.cpp:

1. **Extract** the frame quantities through the one shared function
   (control/controller.cpp:43; control/extract.h:46-68).
2. **Outer loop**: pointing demand → `ω_des` through the braking law
   `sign(e)·min(K_θ|e|, √(2·α_brake·|e|), ω_max)` (controller.cpp:23-27). The
   √-branch is kinematics: it commands the rate from which full-authority braking
   *just* stops on target — arrival is shaped by the airframe, not by a tuned lag.
3. **Clamps**: G ceiling/floor `(n±cosΦθ)·g/V` (controller.cpp:172-174), AoA
   protection on a *filtered* AoA, floor/ceiling applied LAST
   (controller.cpp:301-305).
4. **Inner loop**: rate-PI `τ_cmd = K_ω·e_ω + K_ωi·integ`, then **plant inversion**
   divides by the sim's own authority `c·max(q,q_floor)·δ(V)`
   (controller.cpp:363-364 → controller.h:188-193).
5. **Curvature feedforward** `ω_ff = (local_up × v)/R` added after everything,
   bypassing deadzone and clamps (controller.cpp:135-137, 320).

Two design absolutes worth teaching:

**No derivative-on-error, ever** (SPEC.md:31-33). Damping doesn't come from
differentiating the error (which differentiates mouse noise into torque spikes);
it comes from the measured-ω feedback already inside the rate loop — the `−K_ω·ω`
term is structural, present by construction, not by tuning. If oscillation appears,
the *only* first moves are raise `K_ω` or lower `K_θ` (CLAUDE.md:46-48)…

**…subject to the ZOH ceiling** (SPEC.md:353-357). ZOH = zero-order hold: the
controller's output is computed once per tick and *held constant* for the whole
tick. Continuous-time intuition says more damping always helps. Discrete time
disagrees: a damping force sized at tick-start overcorrects by tick-end if it's
big enough to reverse the rate within one tick — like steering a car while only
looking up once per second. The stability boundary is `K_ω·dt/I ≈ 2`; the repo
pins the ceiling at **0.5** (4× margin), and the *loader refuses tables past it*
(config/controller.toml:8-10). Past the ceiling, raising K_ω *causes* the
oscillation it normally cures — the one mode where the mantra inverts
(docs/flight-log.md:25-27).

`💡 INSIGHT` — **The deadzone holds trim** (SPEC.md:34-36, controller.cpp:127-129,
306-313). Below 0.10° of error the *pointing term* goes to zero — but the
integrator keeps holding trim deflection and the feedforward bypasses the deadzone
entirely. Inputs at rest are *constant at trim*, not zero. Zeroing deflection on a
trimmed, curving airframe manufactures the exact limit cycle the deadzone was meant
to kill: drop trim → drift out of the deadzone → snap back → forever. Pinned by
test_cascade.cpp:284.

`💡 INSIGHT` — **Level flight is a turn.** Holding altitude on a sphere *is*
continuously rotating toward the center at V/R (SPEC.md:242-244) — ~0.64°/s at
cruise, which exits the 0.10° deadzone in ~0.16 s. Without the feedforward, a
deadzone controller in level flight *must* limit-cycle; with it, the nose parks
(test_cascade.cpp:165). The end-to-end proof is elegant: in the deadzone,
`ω_des.x ≈ −V/R` — the controller quietly pitching "down" forever to stay level
(CLAUDE.md:299-303).

`⚠ TRAP` — The deadzone "parks at its re-arm boundary with occasional taps." That
is the deadzone *working*. An early assertion of "zero deadzone exits" was wrong —
the pathological cycle is ~6 Hz at degree scale; the healthy one is bounded inside
the band (CLAUDE.md:299-303). Assert the feedforward value and altitude, not exit
counts.

### 3d. The aim frame — parallel transport, and holonomy as gameplay

The aim is not a screen offset. It is a **world-space unit vector** (where the
pilot wants the nose), and it has a problem unique to this game: the world's
"level" rotates underneath you. Fly 10 seconds and local_up has visibly moved; an
aim frozen in world coordinates drifts off the horizon (input/aim_frame.h:41).

The fix is **parallel transport** — differential geometry's answer to "how do I
carry a direction along a curved surface while turning it as little as possible?"
Each tick, compute the smallest rotation taking `local_up(t−dt)` onto `local_up(t)`
and apply exactly that rotation to the aim (control/transport.h:25-49). No more,
no less: the aim stays "the same direction, relative to the world I'm standing on."

Now the beautiful consequence. Carry a vector around a **closed loop** on a sphere
and it comes back *rotated*, by exactly the enclosed area ÷ R². That residual
rotation is called **holonomy** — it's the Foucault pendulum: the pendulum's swing
plane is parallel-transported by the turning Earth and comes back rotated after a
day. Curvature is precisely the thing that makes "keep it straight" around a loop
fail to close.

Most engines would treat that as drift and clamp it. SEADS *ships it as gameplay*:
fly a banked combat circle and your horizon reference comes back tens of degrees
rotated (SPEC.md:466-467) — real, felt, and correct. The camera is carried by the
**identical quaternion** as the aim (input/aim_frame.h:42-44 reuses
`control::transport_rotation` — one source, never re-derived), so pilot and
controller always agree on which way is up, holonomy and all (SPEC.md:276-286).

One quaternion carries both aim-forward and camera-up (input/aim_frame.h:30-35).
That representation choice quietly solves two more problems:

- **No zenith pole.** Straight-up aims are legal (SPEC.md:259). Any basis built
  from local_up degenerates exactly there; the frame's own axes don't. The mouse
  dx-axis is the *frame's* up, ruled explicitly over local_up (SPEC.md:262-267;
  input/aim_frame.h:60-64), pinned by a test that yaws the aim at 88° pitch and
  shows the local_up axis would be a near no-op (test_aim_frame.cpp:111-132).
- **No shear.** Mouse rotation and transport are both rotations of one unit
  quaternion, so the frame cannot lose orthogonality (input/aim_frame.h:14-16;
  drift over 5000 mixed steps ~1e-15, test_aim_frame.cpp:251-268).

`⚠ TRAP` — **Nothing smoothed may ever touch mouse → aim → error → ω_des → Input**
(SPEC.md:267-270). A slewed camera basis feeding the aim update injects *lagged
signal into the loop* — mathematically, negative damping; in the hands,
rubber-band. This is a §6.2 banned pattern with teeth: it's why freelook's
ease-back only moves camera *position* (app/main.cpp:195-199), and why mouse→aim
is fully **suspended** during the ≤300 ms ease-back rather than applied in the
easing frame (the CQ2 ruling, SPEC.md:523-527; input/aim_state.h:85-90). The one
sanctioned smoothing in the whole loop: the AoA filter feeding the protection
clamp (controller.cpp:80-88) — which bounds authority, not tracking.

---

## 4. The deep cuts

Ten ideas, each of which either changed the design or would have silently broken it.

### 4.1 A float position literally cannot move (why SimState is double)

At |position| ≈ R = 15 km, a float's ulp is ~2 mm. One tick of gravity displacement
is `g·dt²/2 ≈ 0.3–0.7 mm`. So a float-precision aircraft at low speed, pulled by
gravity, **rounds back to where it started, every tick, forever** — it hovers, and
nothing crashes to tell you. SimState is double on purpose (sim/state.h:24-28);
render casts down only after re-basing to the camera eye (CLAUDE.md:240-242, 272-274).

`💡 INSIGHT` — The same quantization blinds *instruments*: telemetry printed at
`%.6e` once read a 1e-3 m residual as "bit-identical". Print `max_digits10` or the
instrument can't see the thing it measures (CLAUDE.md:430-435).

### 4.2 A flat instrument certifies a flat controller

If your harness measures "bank" against a fixed world axis, then a controller with
a fixed-axis bug *passes* — the instrument and the bug share the error. Hence:
every telemetry/harness "level/bank/trim" recomputes from local_up, and closed-loop
rigs use the real spherical `step()`, never a flat stub (CLAUDE.md:224-226;
HARNESS.md:64-66). The subtle version: on a sphere, the *correct* Inputs stream for
straight flight contains constant corrective rotation — a flat plant would grade
correct behavior as drift, or bless a controller that climbs off the world.

This lesson kept generalizing all project long: a re-derived AoA in `render/` is
the same fork (CLAUDE.md:369-374); a fixture built from the extraction formulas
under test would be too, which is why `level_trim_state` builds orientation from
raw rotations (test/harness/instructor.h:125-129).

### 4.3 Mutation-verify: a passing test proves nothing until a mutant fails it

The repo's test-quality gate is not coverage — it is **mutation testing by hand**:
re-apply the exact defect the test claims to catch, confirm the test *fails*,
restore (section7_prep_handoff.md:129-133). The two hardest bugs of the prep cycle
were caught by mutation-verify, not by review:

- A camera-up test that separated the carried up from local_up by *pitching*
  passed under a "feed local_up instead of aim.up()" mutant — because the camera
  re-orthogonalizes its up against forward, and a pitched-off up stays coplanar
  with {forward, local_up}, so the projection *reproduces* it. Only **roll**
  separates the two bases — a full 1−cosφ apart (CLAUDE.md:413-419).
- A crash test shaped "run N ticks, expect it fired" passed under a
  `<=0 → <=-50` boundary mutant — a sustained dive crosses −50 too, just late.
  The fix pins the *predicate boundary*: pre-tick altitude > 0 on the tick that
  respawned (CLAUDE.md:420-423).

`🔬 HOW WE KNOW` — Nearly every test banner in test/unit/ cites its mutant and the
observed failure numbers inline (e.g. test_acceptance.cpp:1034-1041). That's the
house style: the mutant *is* the test's proof of existence.

`🧭 FOR AGENTS` — Assume your first test is blind until a mutant proves it isn't.
And when you extract or move code, "the N named mutations still pass" is *not*
coverage of the move: three consecutive consults each found more moved-but-unpinned
consumers after the app-tick extraction (CLAUDE.md:436-441). Grep every field the
new function forwards.

### 4.4 φ folds past 90° — and the feedback sign inverts

Bank is extracted as `φ = −asin(dot(body_right, local_up))` (control/extract.h:57-61).
asin's range is ±90°: at an actual bank of 100°, φ reads **80° and decreasing** as
bank grows. Inside that fold, the wings-hold feedback `φ − φ_held` points the wrong
way — it rolls the plane *further over*. So FINE wings-hold is gated on
`cosΦθ > 0` (controller.cpp:285-294), roll-through/MANEUVER owns the far side, and
the fold itself is pinned executably in AT-0 so it stays a documented cliff
(control/controller.h:79-87; CLAUDE.md:282-285).

Corollary for tests: past-90° bank must be read from the SIGNED `cosΦθ`, never φ —
φ caps at ~89° even fully inverted. The AT-15 legs use the *pair*: φ_max pins "did
it roll", cosΦθ < 0 pins "did it go inverted" (test_acceptance.cpp:1007-1013).
Related: `cosΦθ` is signed on purpose — clamping it ≥ 0 is explicitly banned
(SPEC.md:299, extract.h:38-39), because inverted G-credit is where the push-gate
and the G-floor live.

### 4.5 The v→0 apex degenerates in four channels at once

Hammerheads are a signature maneuver, so v ≈ 0 *will* happen. The rookie fix is one
epsilon on `normalize(velocity)`. The spec instead enumerates **four separate
degeneracies** (SPEC.md:383-399) — because fixing one leaves three:

1. `v̂` undefined → hold last-valid, one copy per layer (§3b above).
2. G-clamps `∝ g/V` **diverge** (they don't vanish) → `vMin` floor on every
   V-division (controller.cpp:172).
3. The outer braking law goes *quiet* (`α_brake ∝ max(q, q_floor)` bottoms out) —
   no inner-loop authority floor can fix a too-quiet outer loop → hand over to
   BALLISTIC attitude-hold on the plant's `q_att_floor` (controller.cpp:141-167),
   a small deliberate physics fib ("prop-wash", SPEC.md:395-397) tuned as data
   (config/aircraft.toml:31).
4. The tail-slide **lies**: α ≈ 180° while falling backward → gate AoA limiter and
   coordination OFF below the floor — don't blend a lie (controller.cpp:80-88 —
   and note the filter is *frozen and re-seeded on exit*, because a low-passed lie
   carried across the exit edge commanded a real pitch hardover; found by the 4d
   cross-model audit, CLAUDE.md:348-351).

`💡 INSIGHT` — Energy honesty is not at stake at the apex (no attitude produces
lift at v≈0; you fall regardless). The stakes are *recoverability and feel* —
which is why the whole mode freezes as the AT-17 hammerhead golden
(test_cascade.cpp:225).

### 4.6 The null test and the sign bit — how you pin a transport

A tempting holonomy test: transport around a great-circle lap, assert you get
rotation back. It asserts nothing — a great circle encloses half the sphere in a
degenerate way and transport composes to exactly 2π ≡ identity. **The lap is a
null test** (CLAUDE.md:238-239). You need the *pair*: identity on the planar lap
(test_aim_frame.cpp:140-158) AND area/R² = 2π(1−cos θ₀) on a banked small circle
(test_aim_frame.cpp:160-186). One without the other blesses a broken transport.

Then the P3a refinement: |angle| and the null test between them still can't express
one degree of freedom — the **sign** of the holonomy, on which the camera-roll
direction rests. Loop closure forces the residual rotation's axis onto ±u_start
regardless of magnitude bugs, so a single dot-product check isolates exactly the
sign bit (test_aim_frame.cpp:188-217). Fable independently re-derived the sense
three ways (rotating-frame ODE, Foucault, limit cases) and added a domain guard:
the +u_start reading is only valid while the solid angle < π — a bigger test loop
would misread a *correct* transport as a sign bug (section7_prep_handoff.md:60-78).

`∮ DEEP` — Same class, deeper trap: on a great-circle path a frame-carried
camera-up and a local_up rebuild *coincide* (null holonomy), so no continuity test
can tell them apart there. The carried frame's real benefit is singularity
avoidance at the zenith — pin *that* (CLAUDE.md:404-407).

### 4.7 Route the live path THROUGH the tested function

The repo's most-repeated lesson, in three escalating forms:

- **A helper**: AT-18b round-tripped `plant_invert` while `step()` inlined its own
  identical denominator — the single-count property was pinned on code the live
  path didn't run. Fix: the live division *calls* the tested function
  (controller.cpp:322-327, 364; CLAUDE.md:294-298).
- **A state machine**: freelook logic is a shared module (`input::Freelook`,
  input/aim_state.h:16-20) that harness and app both call — never reimplemented
  per caller (CLAUDE.md:326-330).
- **The whole app tick**: until the P1b audit, the crash predicate, GROUNDED
  pairing, and camera call site lived only in `main.cpp` — under **no test at
  all**. They were extracted into `app::tick` / `app::step_frame`
  (app/instructor_tick.h:15-27, 101-176, 226-278), main.cpp delegates
  (app/main.cpp:152-154), and a mirror-equivalence test pins app-tick vs the
  harness ClosedLoop **bit-identical over 5 s** — `pos_err == 0.0` at
  max_digits10, pinned at < 1e-6 because a float-dt slip lands at ~4e-5 m and
  1e-3 was a hide-band (CLAUDE.md:424-435).

`💡 INSIGHT` — The related inversion: **a golden only pins the code that recorded
it** (CLAUDE.md:252-255). A golden's only failure mode ("golden moved") invites a
re-record that would bless a bug — so every named mechanism also needs a
first-principles tripwire the golden can't substitute for. Goldens catch
*unintended* change; tripwires catch *wrong* change. You need both.

### 4.8 The energy gate that costs machine epsilon, not O(dt)

AT-12 needs "energy is predictable" as a gate that survives any drag retune. The
naive check — Δ(½mV²) vs continuous force work — carries the integrator's O(dt)
error and floored at ~4% of throughput at 5 g: wide enough to hide a real drag
fork. The trick (test_acceptance.cpp:455-477): reconcile the plant's per-tick
**linear** work `m·v·(v'−v)` — algebra on the plant's *own* output, in which the
semi-implicit KE surplus `½m|v'−v|²` never appears — against config forces at the
identical pre-tick state. Plant == config ⇒ residual ~1e-13. A `k_induced→0`
mutant in step.cpp sends it to ~1.8 (test_acceptance.cpp:595-603). And projecting
onto v makes lift drop out *for free* (lift·v ≡ 0 by construction) — correct scope,
since a lift defect moves the trajectory, never the energy.

`⚠ TRAP` — The force composition in the test is **duplicated from step.cpp on
purpose** (test_acceptance.cpp:479-487). Share the primitives (H1), but factoring
the assembled expression into a helper called by both would zero the residual
under *every* plant mutation — the duplication IS the fork detector. A
"simplification" pass that deduplicates it destroys the gate while leaving it green.

`⚠ TRAP` — "Tune-independent" must survive the *seam*, not just the algebra: the
plant round-trips throttle through a `float` (sim/state.h Inputs), so the test
mirrors that exact cast (test_acceptance.cpp:508-512) — else the first
non-float-exact throttle retune false-trips a 1e-9 gate by ~1e-8. Also: sum
per-tick |residual| (a signed sum cancels oscillating defects) and normalize by
component throughput, not net power (which nearly cancels). All three found by a
Fable diff-consult, none by the honest pass (CLAUDE.md:464-470).

### 4.9 The G floor you can't reach upright (and what "achieved −3g" really is)

Two compounding surprises from the P2a value pin (CLAUDE.md:471-484,
test_acceptance.cpp:233):

1. **The achieved floor is not n_min.** The curvature feedforward is added *after*
   the clamps and bypasses them (controller.cpp:320), so a perfectly-tracked floor
   settles at `n_min + V·ff_x/g` — which is precisely CQ1's flat-credit bias
   (~0.27 g at V=200), read back by the very true-n instrument CQ1 mandated.
   Asserting bare n_min would be wrong *even with perfect tracking*.
2. **Upright, the floor is unreachable — structurally.** `plant_invert` inverts
   authority but not the plant's `−damp·q·ω` term (sim/step.cpp:92-97), so holding
   ω_min needs the integrator to source the damping torque; upright, that exceeds
   `K_wi·integ_cap` and the integral caps ~30% short *forever* (|n| plateaus ~2.4).
   The test reaches the floor by entering **inverted** (cosΦθ = −1 halves the
   needed budget) with the integrator preloaded to steady state.

`💡 INSIGHT` — This is why the one-tick precedence test and the closed-loop value
pin *both* exist: the one-tick test recomputes w_min with the controller's own
formula (blind to a sign/credit mutant in that formula), the closed-loop leg reads
the independent true-n instrument (blind to the `cpt→|cpt|` class at cpt<0). Each
uniquely owns a mutant class. Neither is redundant.

### 4.10 Hysteresis everywhere — and how to actually test it

Every gate/regime/latch leg is hysteretic (SPEC.md:308-309): regime 5°/12°
(controller.cpp:101-106), deadzone 0.10°/0.25°, ballistic 30/40 m/s, push-gate
ratio 0.9/1.1 and bank 120°/100° (config/controller.toml:49-64). Shared thresholds
chatter — a signal sitting on a single boundary with any ripple flips the mode at
ripple frequency, and mode flips are *felt* (the "Transition" star,
flight-log.md:101).

`⚠ TRAP` — The obvious closed-loop anti-chatter test pins nothing: a closed-loop
chase *nulls* the error and only sweeps through the threshold monotonically, so a
non-hysteretic latch produces the identical switch count. You must make the signal
**dwell at the boundary with ripple** — drive the latch open-loop on an oscillating
fixed state; then hysteretic = 1 switch, shared-threshold = 2 per ripple period.
Mutation `blend_lo→blend_hi`: 30 switches vs 1 (CLAUDE.md:387-392;
test_acceptance.cpp:674, 1198, 1434).

`∮ DEEP` — And know when *not* to add hysteresis: the `lat_sq > 1e-12` guard before
`bank_error` (controller.cpp:227-243) is a geometric **degeneracy** guard, not a
regime gate — bank direction is genuinely undefined at the astern pole, and a
transported aim over a curving plant can't pin at a measure-zero boundary. The
"every gate hysteretic" rule targets sustained-command chatter, not singularity
guards. (Astern is the sneaky pole: `err ≈ π` puts blend = 1, so a regime-only
guard still evaluates the assert — CLAUDE.md:289-293.)

---

## 5. What happened — the build journey

The build order was a de-risking ladder (SPEC.md:489-512), each rung gated and
tagged before the next began.

**Origins.** The spec is itself a merge: the plant and project spine from a
`seads_proto` spec, the controller from a five-times-red-teamed `SOLUTION` doc,
reconciled with three deliberate supersessions logged so compliance is never
silently redefined (SPEC.md:18-40) — no D-on-error, deadzone-holds-trim, and
conditional freelook reset. Frozen 2026-07-03 after an Opus ⇄ Fable audit cycle
(SPEC.md:11-14), with two pre-locked consult questions RULED: **CQ1** (keep the
flat cosΦθ G-credit, log true n alongside — the ~0.2–0.35 g bias is absorbed by
n_max tuning) and **CQ2** (suspend mouse→aim during freelook ease-back — the only
smoothing-free-by-construction option) (SPEC.md:514-527). Both stay one-line data
flips if the flight log demands.

**Sections 1–2 (ballistic point → flyable plant).** The invariant asserts and the
gate hook landed first — the walk-away enabler (HARNESS.md:32-41). Early red-teams
set the tone: an identity-start constant-ω quaternion test *cannot* see Hamilton
order errors (q and ω commute along that trajectory) — pin against the closed form
from a non-identity q0 (CLAUDE.md:245-247); invariant asserts must check what the
integrator *applied*, not the vector handed in (sim/invariants.h:38-43); and
"plant-complete before recording goldens" — throttle slew nearly slipped a section,
which would have moved every golden at once (CLAUDE.md:256-258).

**Section 3 (render + camera).** Where "the bases must SEPARATE" was learned: in
level flight body_up == local_up, so every level-attitude camera test passed a
wrong camera-up; one 60°-banked case killed two mutants at once (CLAUDE.md:262-265).
Plus render numerics at R = 15 km: raylib's 1000 m default far plane, depth
precision ∝ near plane, mesh sag ~14 m (CLAUDE.md:271-274).

**Section 4 (the instructor — the high-risk gate).** Sequenced 4a→4d exactly to
keep sign conventions (AT-0) ahead of gains. 4a's red-team caught the φ fold and
the |bankErr| silently capped at 90° by an `abs()` that every upper-half-plane test
missed (CLAUDE.md:280-285). 4b landed the cascade and the AT-18b round-trip lesson
(§4.7). 4c: override as a *strict superset* — the golden reproduces bit-identically
with override defaulted off, and that byte-level no-op IS the regression proof
(CLAUDE.md:307-310); the red-team caught pursuit-suspension ignored by the
BALLISTIC branch ("grep every branch that does the gated thing",
CLAUDE.md:311-314). 4d: freelook as a shared caller-side module, the
first-override-snap latch (not an edge — an edge misses "key already held when
freelook begins", CLAUDE.md:331-334), and the **first cross-model Fable audit**,
which found the frozen-but-not-reseeded AoA filter hardover (§4.5) and demolished
two "dead code" rationales by evaluating them at the authority floor instead of
cruise q (CLAUDE.md:352-358).

**Sections 5–6 (HUD, app wiring, acceptance).** The app-wiring re-sequence (S4→S5)
and camera-contract activation were §0-logged supersessions, not drift
(SPEC.md:49-63). Section 6 mechanized the acceptance suite — and then the **second
Fable cross-model audit** (pre_spec_audits/fable5_section6_fixreport.md) triaged
what the green gate did NOT cover, producing the honest ledger and the 7-item prep
queue. Verdict: gate sound to enter §7 (CLAUDE.md:170-171).

**Section-7 prep (the 7-item queue, all closed).** AT-9 end-to-end (30 vs 240 fps →
bit-identical double trajectory, test/unit/test_at9.cpp — with crash-neutralization
pulled *inside* the seam because residual-tick count is frame-rate-dependent,
app/instructor_tick.h:217-225); the AT-15 push-gate legs (seven, all
mutation-verified, with a second Fable consult that attacked the *fixes* and forced
the bounds config-relative — §7's calibration manifest,
test_acceptance.cpp:1015-1031); AT-12 + the energy instrument (§4.8); the holonomy
sign pin (§4.6); the P3c one-frame stale camera (a crash on a frame's last tick
rendered the fresh spawn through the dead life's aim — reseed in the crash branch,
idempotent with the GROUNDED reseed, app/instructor_tick.h:161-171); wording nits;
and the P2a G-floor value pin (§4.9).

**Section-7 entry (this HEAD).** The step-response instrument
(`seads_harness step`, test/harness/harness_main.cpp:202-394) — settle, overshoot,
reversals on the *same* `t.e` signal AT-2 grades, plus gains, ZOH margin, and the
crit ratio. Fable red-teamed it in two passes: D1 (roll metrics fabricated via the
degenerate signed bank error — fixed by grading total pointing error on every
axis, harness_main.cpp:283-290), D2 (the doc named the wrong inner-loop knob — on
this plant K_ω hits the ZOH ceiling before any P-overshoot; **K_ωi shapes the
response**, measured 0.0% vs 13.6%, flight-log.md:84-87), and N1/N2
(window-dependent transient metrics — overshoot/reversals now bounded to closest
approach + ring-down so post-capture drift can't masquerade as ringing,
harness_main.cpp:317-324). A farewell sweep then fixed three doc contradictions —
notably that the goldens load the live TOML, so the *first* tuning knob moves them
by design (flight-log.md:10-16).

---

## 6. The verification philosophy

**Instrument heavily, gate lightly** (HARNESS.md:89-91). The only hard gates are
T1 (build + live asserts + unit tests) and T2 (goldens). Everything else — settle
times, overshoot, retention, sideslip — is telemetry read *alongside* the flight
log. Why so restrained? Because a numeric gate on an untuned target becomes a
phantom to chase: AT-12's retention deliberately became a printout, not a gate,
precisely to avoid freezing a feel decision into pass/fail
(test_acceptance.cpp:449-453). Telemetry's job is the question goldens can't
answer: *"still compiles, flies worse."*

The layered defense, cheapest first:

1. **Invariant asserts, every tick, debug builds** (sim/invariants.h) — free, and
   they define what "sphere-correct" means executably.
2. **Goldens** — fixed start + script → snapshot; a moved golden *halts the loop*
   (HARNESS.md:61-66). They catch unintended change.
3. **First-principles tripwires + mutation-verify** — they catch wrong change,
   including in the tests themselves (§4.3, §4.7).
4. **Red-team in a separate context** — never self-review in the authoring
   context (HARNESS.md:67-69), and only for the mechanisms that matter: invariants,
   integrator, cascade core, camera-up, harness frame-correctness (the
   `adversarial-review` skill; target list HARNESS.md:132-150).
5. **Cross-model audits (Fable 5)** — the escalation tier (HARNESS.md:152-159).
   The pattern that worked: tight packet, file:line citations, "attack this";
   and crucially, Fable's best catches were attacks on *rationales* — "dead code"
   claims evaluated at the wrong operating point, calibrated test bounds that
   would silently disarm under the very retune they guard (CLAUDE.md:139-151,
   352-358).

`💡 INSIGHT` — Notice what the consults repeatedly caught that review didn't:
second-order interactions between a fix and the future (the AT-15 bounds vs a §7
retune; the float-throttle seam vs the tune-independence claim). A separate
context isn't just fresh eyes — it's a reviewer with *no sunk cost in the fix*.

`🧭 FOR AGENTS` — The loop, verbatim from the SOP (HARNESS.md:14-20): plan →
implement → gate → red-team (important mechanisms only) → tag → persist lessons to
CLAUDE.md ## Learned → /clear → next. One commit per item. The Stop hook runs the
gate; a moved golden means STOP and confirm intent, never reflex-re-record.

---

## 7. For the next agent / for the next flight

**Where we are.** Section 7 is a human-in-the-loop feel phase. The discipline is
docs/flight-log.md:19-27: one knob per flight, hypothesis before flying, stars
after, tag builds your hands liked. Coefficients and gains ONLY (SPEC.md:511-512).
The tuning order (flight-log.md:29-39): inner loop (K_ωi shapes overshoot; K_ω is
damping headroom under the ZOH ceiling) → outer (K_θ up to first AT-2 failure,
back off 30%) → braking + clamps → blend/deadzone/latches/gate → feel. The step
tool (`seads_harness step`) is the instrument for steps 1–2; **it is never a gate**
and you must not tune gains autonomously against its numbers — they explain a
score, they never replace the stick (CLAUDE.md:94-96).

**The honest ledger.** The green 139 is NOT "AT-0…AT-18 all green"
(CLAUDE.md:101-108). Unmechanized, on purpose, forever-human: **AT-1**
(cursor-connected, zero smoothing), **AT-3** (rapid displacement), **AT-4** (never
hunts), **AT-5** (natural settling) — these ARE the Delight Test's limbs, and
pretending a script can grade them would be the §0 "silently redefine compliance"
trap. AT-10's full scenario is partial (mechanism pinned in test_cascade).

**The landmines, named:**

- **Untested main.cpp glue.** No ctest runs `seads.exe`. The residual caller glue —
  per-frame mouse plumbing, orbit ease-back, F1 toggle, the fact that main.cpp
  keeps *calling* `instructor_camera` at all (app/main.cpp:204-206) — is pinned
  only by discipline (CLAUDE.md:167-170). Camera spring / orbit ease-back are
  `render::ChaseParams` defaults and a `0.12` literal (app/main.cpp:196), not TOML:
  tuning them is a code edit, so treat any main.cpp touch as red-team-worthy
  (flight-log.md:36-39).
- **Golden churn.** Both goldens load the live TOML — every table edit moves them,
  by design. The danger is re-record becoming reflex: a golden's only failure mode
  is blessing your bug (flight-log.md:10-16).
- **The push-gate calibrated constant set.** The AT-15 legs' 6°/60° family is
  calibrated to today's untuned ~7° push/roll boundary; a `[push_gate]` retune —
  or a `[regime] blend_lo` tweak, the *hidden* dependency — trips several REQUIRE
  premises at once, **by design, loud** (test_acceptance.cpp:1015-1031;
  HARNESS.md:119-127). Recalibrate them AS A SET from the manifest; don't "fix the
  test" one assert at a time.
- **The known feel wrinkle:** knife-edge wings-hold dither at cosΦθ ≈ 0 — bounded,
  measure-zero, park it until it's felt (CLAUDE.md:199-201).

**Where the delight lives or dies.** My read, from the geometry and the code:
(1) the *catch* — override release and freelook release, where the braking law
shapes re-engagement (the coast-vs-overshoot distinction of CLAUDE.md:316-320);
(2) the *weight* — whether compression-as-saturation at low speed reads as
"heavy" rather than "broken" (§3b's feel consequence); (3) the *transitions* —
push↔roll↔freelook edges, which is exactly why every AT-15 leg exists; and
(4) the sphere's own signature — sub-1g cruise lift (~0.81 g, SPEC.md:461-462)
and banked-circle holonomy, the two things a flat-map pilot has literally never
felt. If those read as *flavor*, the bet pays. If they read as *bugs*, R and the
CQ1 flip are the dials — and both were built to be one-line changes.

`🧭 FOR AGENTS` — Fresh-agent pointer: CLAUDE.md:88-96 is authoritative for
"what now". Sandbox worktrees live in `D:\seads_sandboxes\` (memory note), not in
the main tree. Keep CLAUDE.md under ~200 lines when appending lessons; keep every
new test mutation-verified; leave git clean.

---

## 8. Annotated file map + glossary

### The map (what each file *owns*)

| File | Owns |
|---|---|
| `SPEC.md` | The constitution. §5 seam, §6 invariants, §7 plant, §9 controller, §14 gates, §15 build order, §16 rulings. Frozen; §0 logs every supersession. |
| `docs/HARNESS.md` | The how: per-section loop, gate, test tiers, AT table, red-team target list. |
| `docs/flight-log.md` | The Section-7 gate itself: discipline, tuning order, star rubric, the log. |
| `CLAUDE.md` | Working memory: commands, compressed rules, the honest ledger (## Deferred), the hard-won lessons (## Learned). |
| `sim/step.cpp` | THE plant tick: forces (step.cpp:25-66), semi-implicit integration (70-72, 103-109), the entire authority model (86-98). |
| `sim/aero.h` | The four-site single-source expressions: q_eff, δ_max_eff, Cl(α), v̂ guard, α_max, load_factor. A copy elsewhere = H1 fork. |
| `sim/invariants.h` | The §6.1 asserts: gravity radial, applied impulse radial, state valid. |
| `sim/state.h` | Double-precision SimState + the Inputs seam struct + the plant's held v̂. |
| `control/controller.h` | Sign primitives (AT-0's subjects), Internal/Input/Telemetry, `plant_invert`. |
| `control/controller.cpp` | The one `control::step()`: GROUNDED, ballistic, regimes, latches, push-gate, clamps, rate-PI, anti-windup. |
| `control/extract.h` | THE shared φ/β/cosΦθ/AoA extraction — compiled into controller, app, HUD, harness. |
| `control/transport.h` | Parallel transport: `transport_rotation`, one source for aim AND camera. |
| `input/aim_frame.h` | The aim/camera quaternion frame: transport, raw mouse basis, reseed vs snap. |
| `input/aim_state.h` | `Freelook`: the three §9.5 rules + the CQ2 ease-back suspension, shared harness/app. |
| `app/instructor_tick.h` | The tested app core: `spawn_state`, `tick`, `step_frame` (accumulator + in-seam crash neutralization), `instructor_camera`, focus loss. |
| `app/main.cpp` | Thin shell: device polling, per-frame mouse accrual, orbit cosmetics, delegation. The untested glue — tread loudly. |
| `config/aircraft.toml` | Plant truth, single source for plant AND inversion. `R` at line 7. |
| `config/controller.toml` | Every gain and threshold; the loader rejects tables that would oscillate (lines 8-10). |
| `test/harness/instructor.h` | `ClosedLoop` — the closed-loop driver everything shares (goldens, ATs, recorders, step tool). |
| `test/harness/harness_main.cpp` | `fly / ctrl_fly / step / alpha / golden / ctrl_golden` — the wind tunnel CLI. |
| `test/unit/test_acceptance.cpp` | AT-6/11/12/13/15/16 + poles + circumnav; the AT-15 calibration manifest banner. |
| `test/unit/test_cascade.cpp` | AT-2/14a/17 + cascade mechanism pins (deadzone trim, astern, purity, anti-windup). |
| `test/unit/test_aim_frame.cpp` | Mouse signs, zenith raw-basis, the holonomy null+area+sign trio. |
| `pre_spec_audits/` | The Fable packets and fix reports — the cross-model paper trail. |

### Glossary (the terms coined or bent here)

- **The Delight Test** — the subjective gate (SPEC.md:481-487): telemetry explains, hands judge.
- **local_up** — `normalize(position)`; the only "up" there is, recomputed always.
- **Parallel transport** — carrying a direction along the sphere with the minimal
  per-tick rotation taking old local_up to new local_up (control/transport.h).
- **Holonomy** — the residual rotation after transporting around a closed loop;
  = enclosed area/R². Here: gameplay, not drift (AT-14; §4.6).
- **The aim frame** — one quaternion carrying aim-forward + camera-up together
  (input/aim_frame.h); the raw, pole-free mouse basis.
- **cosΦθ** — `dot(body_up, local_up)`: the SIGNED gravity credit in the G-clamps.
  Never clamp ≥ 0; its sign is the inverted-flight bit (extract.h:38-39).
- **The φ fold** — bank φ = −asin(·) folds at 90°; feedback sign inverts inside
  (§4.4).
- **H1 / single-count** — dynamic pressure applied exactly once, in the plant;
  the controller inverts the same params. AT-18 checks both directions.
- **Plant inversion** — the inner loop divides τ_cmd by the sim's exact authority
  so commanded rate, not raw torque, is the interface (controller.h:188-193).
- **q_att_floor** — the small q-independent authority floor ("prop-wash fib") that
  keeps attitude recoverable at v→0 (aircraft.toml:31, SPEC.md:395-397).
- **BALLISTIC** — the hysteretic v<30 m/s mode: attitude-hold on the floor,
  AoA limiter + coordination gated off (§4.5).
- **FINE / MANEUVER** — pointing regimes (enter <5°, exit >12°): elevator+rudder
  with wings held, vs bank-to-turn; blend continuous, latch discrete
  (controller.cpp:101-106).
- **Push-gate** — push (negative G, to −3) vs roll-through-inverted for
  below-nose targets; every leg hysteretic (controller.cpp:249-259).
- **Deadzone holds trim** — pointing term → 0 inside 0.10°; integrator keeps trim (§3c).
- **Curvature feedforward** — `ω_ff = (local_up × v)/R`, the standing "turn toward
  the center" that makes level flight not limit-cycle (controller.cpp:135-137).
- **ZOH ceiling** — `K_ω·sim_dt/I ≤ 0.5`: the discrete-time bound past which more
  rate feedback *causes* oscillation (§3c; loader-enforced).
- **GROUNDED** — the in-core spawn/reset safe state: zero Inputs, fresh internal,
  except held_bank carries the current bank (controller.cpp:50-59).
- **CQ1 / CQ2** — the two pre-locked, ruled consult questions: flat G-credit +
  true-n telemetry; suspend mouse→aim during ease-back (SPEC.md:514-527).
- **Golden** — a committed scripted-flight snapshot; a move halts the loop. Both
  load the live TOML, so tuning moves them by design (flight-log.md:10-16).
- **Mutation-verify** — re-apply the exact target defect, watch the test fail,
  restore; the gate on test quality (§4.3).
- **vMin / v_ballistic / v_dir_eps** — the three v→0 floors: G-clamp divisions /
  mode handover / v̂ guard (controller.toml:61-64, aircraft.toml:42).
- **AT-n** — the acceptance tests (HARNESS.md:97-117); the honest mechanization ledger is CLAUDE.md:101-108.

---

*The rule underneath everything, twice over: prove the core, then grow — and the
verification is sphere-correct, or it is worthless (HARNESS.md:180-181). The code
is now waiting on the only instrument that was never mechanized: your hands.*

---

## Addendum — Beyond the fence: what "resounding success" still needs

*Appended 2026-07-05, atop the Section-7-entry snapshot. The lecture above taught
the kernel — what is built, tested, and true. This postscript is the opposite: an
honest map of what the kernel, by design, **cannot yet answer**, and what the future
of the project turns on. Read it as one engineer's strategic read, not as a contract.*

### A.1 The gate has a phantom in it: there is nothing to fight

The bet is *"does mouse-aim **dogfight** flying on a small sphere feel good?"*
(SPEC.md:69). But the whole codebase is **one aircraft** — the scope fence explicitly
fences out weapons and AI (SPEC.md:97). So look hard at your own Section-7 gate:

> **Tracking ★** — *pipper sits on a maneuvering target, no lag* (flight-log.md:99)

`⚠ TRAP` — **There is no maneuvering target.** Every "tracking" scenario — AT-12,
AT-15, the step tool — points the nose at a *scripted offset from its own heading*,
i.e. a phantom the pilot imagines. The one star that is the literal heart of
dogfighting can only be scored against imagination. The danger is subtle and real:
you can tune a plane that feels sublime doing solo aerobatics yet mushy-or-twitchy
when a *real* bandit jinks — because no jinking bandit was ever in the loop.

`💡 INSIGHT` — Sections 1–6 proved *"the plane obeys."* They could never ask *"can I
get guns on someone who doesn't want me to."* Those are different questions and only
the second one is dogfighting. The kernel is proven; the **game** is not yet
testable. That distinction is the single most important thing this addendum exists
to name.

### A.2 The highest-leverage unlock — a non-shooting target drone

The smallest faithful step past the fence, and you already own every part of it:

- The plant is **pure and reusable** (`sim::step`, sim/state.h:29-40). Instantiate a
  *second* `SimState` and fly it — first on a great-circle cruise, then scripted
  break turns, eventually a dumb pursuit/evade. No new physics.
- Add two **read-only** instruments (they read state, never write — the render/HUD
  discipline, SPEC.md model 3b): a **pipper-on-target** reticle and a **time-on-
  target %** counter. That converts the Tracking star from imagination to a *number
  read alongside the stars* — exactly the "instrument heavily, gate lightly" pattern
  the project already lives by.

`🧭 FOR AGENTS` — This honors the fence's *spirit* (keep the signal clean on feel):
no damage model, no netcode, no weapons — just something to point the nose at. It is
probably a day of work against the existing plant, and it is the difference between
proving the kernel and proving the game. If you build it: a second plant instance
must obey the SAME sphere invariants (§3a) and the SAME seam (its "AI" is a pure
function of state, never a clock); the pipper/TOT instruments join the honest ledger
as *instruments, not gates* (§6).

### A.3 The supporting gaps, ranked

1. **A human test-card.** Delight lives or dies in the first three flights, but there
   is no *fixed* maneuver set to fly each iteration — so star scores across flights
   aren't comparable (you flew differently). A 6-maneuver card (trim lap → 30°
   tracking capture → break turn → vertical reversal → target pursuit → deadzone
   hold) makes the subjective gate *repeatable*, which is what lets tuning converge
   instead of wander. Cheapest high-leverage item here; it belongs next to
   flight-log.md.
2. **Felt curvature.** The novelty *is* the sphere — but if you can't *perceive*
   you're on one mid-fight (horizon bulge, curving ground-track, altitude cues), the
   sphere adds nothing to feel and the whole premise is invisible. The spec treats
   the sphere as a physics fact; nobody has yet made it a *perception* target. `⚠` a
   game that is secretly flat-earth-with-a-curved-skin can still *look* fine (§3a) —
   the same silent-failure class, moved from the physics to the player's eye.
3. **The mouse has a ceiling.** `aim_sensitivity` is a single *linear* gain
   (controller.toml:83). WT-grade "connectedness" usually wants a sensitivity
   *curve* (soft near center, faster at the edges). Linear-only may cap how wired-to-
   the-cursor it can ever feel — directly on the AT-1 question. Worth a spike before
   concluding "the mouse feel is as good as it gets."
4. **A pilot-facing recorder + ghost.** Replay exists as a *test* tool, not a *pilot*
   tool (SPEC.md:97 fences replay-hashing, but a local record/scrub is a different
   thing). Record 30 s, replay it, race a ghost of your best pass — turns "that felt
   wrong" into something you can inspect and communicate.

### A.4 For the project's future, not just the cockpit

`💡 INSIGHT` — "Resounding success if the fun is there" hides a clause: **the fun has
to generalize past the author's own tuned build.** The author always finds their own
tuning fun. The real viability test is a *fresh* pilot, cold, on your build — and a
way to A/B two tunings blind. Nothing today captures that. Before you fall in love
with your own knobs, it is worth a one-page "how we prove the fun travels" plan:
who flies it cold, what they're asked, and how a tuning A/B is judged without the
author in the room. The kernel's honesty discipline (§6) is exactly the asset that
makes such a plan credible — extend it from *"is the code correct"* to *"does the
delight reproduce."*

`🧭 FOR AGENTS / FOR THE PILOT` — Order of operations that keeps the honesty streak:
finish the inner/outer feel pass on the phantom (Section 7 steps 1–2 are legitimately
phantom-safe — they're about the plane's own response), **then** build the target
drone before trusting the Tracking / Transition stars, **then** worry about felt
curvature and the mouse curve, and only *then* about whether the fun travels. Each
step unlocks honest measurement of the next star; skipping ahead scores a phantom.

*The lecture said the code waits on your hands. The addendum says: your hands, in
turn, are waiting on something to chase. Build it small, keep it pure, and the one
question finally becomes answerable in full.*
