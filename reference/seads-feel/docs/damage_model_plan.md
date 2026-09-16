# Component damage model — design packet

> **STATUS: v1 BUILT + Fable-reviewed (2026-07-14), gate 452/452, UNCOMMITTED.**
> Module `combat/damage.h`; wired in `app/instructor_tick.h` (damaged params to
> both kernels + roll bias), `combat/kill.h` (routing + derived HP), HUD component
> panel (`render/draw.*` + `app/main.cpp`), tests `test/unit/test_damage.cpp` (18
> cases) + wiring pins in `test_combat.cpp`. **NOT config-driven yet** —
> `DamageParams` uses the Fable-tuned defaults (a `[damage]` scenario.toml section
> is the obvious follow-up). **⓯ PENDING Chad's FLY** — feel of a hurt plane
> (engine-out glide, wing-break roll, pilot wallow). DEFERRED to v2: G-LOC, splash
> routing, deadzone/rate-clamp pilot effects, throttle-slew, drag-yaw, visual break.
>
> **FABLE IMPL RED-TEAM FOLDED (verdict SHIP-WITH-FIXES):** every pre-consult
> P0/Q was confirmed correctly folded; the review then caught + fixed:
> - **P0-A** `with_bias` ran on the always-on path and `roll + 0.0f` flips
>   `-0.0f`→`+0.0f` (breaks the zero-damage bit-identity). Now STRUCTURALLY gated
>   `if (dmg_roll_bias != 0.0)`.
> - **P1-1** `player_hp` is a 0..100 summary; the HUD max is now fixed 100 (was
>   `setup.player_hp` — a non-100 config would desync the bar). Resets derive it.
> - **P1-2** derived `player_hp` now written ABOVE the invuln early-return (no stale
>   HUD during invuln).
> - **P1-3** the crash-respawn now mirrors the death-respawn housekeeping (clear
>   enemy pool + arm invuln), so the two paths can't drift.
> - Test gaps closed: bias-wiring end-to-end (broken wing biases roll + banks the
>   plane), crash-branch damage reset, routing at a ROLLED orientation.

> **FABLE PRE-CONSULT FOLDED (2026-07-14, verdict SOUND-WITH-FIXES).** Rulings
> baked into the v1 below. The originals are preserved further down. Key fixes:
> - **P0-1 sign:** `roll +1 = roll LEFT` (sim/state.h), so a broken LEFT wing rolls
>   left ⇒ needs a POSITIVE bias ⇒ asymmetry is `(wing_right − wing_left)`.
> - **P0-2 untrimmable:** a CONSTANT roll bias is trimmed to zero by the rate-PI
>   integrator in ~2.5 s. Make it LOAD-PROPORTIONAL:
>   `bias = clamp(k·(wing_right−wing_left), ±0.5) · clamp(|n|/n_ref, 0.25, 1)`,
>   n_ref≈2 — the load term is the untrimmable part (lift asymmetry scales with Cl).
> - **P0-3:** compute `ap_d` ONCE, pass the SAME object to `control::step` AND
>   `sim::step` (a param mismatch is a ×4 gain error → ZOH-ceiling violation).
> - **P0-4:** reset DamageState on BOTH respawn paths (crash + death); gate the bias
>   off on grounded/respawn ticks.
> - **P0-6:** scale `Cl_max` AND `Cl_alpha` by the SAME factor (stall-alpha
>   invariant); pilot_gain is ONE factor across `K_theta/K_phi/K_w_*/K_wi_*`.
> - **Q5:** wing BREAK = a kill. Death = `pilot≤0 ∨ structure≤0 ∨ min(wing)≤0`.
>   So floors only ever model partial (0,1] damage (never a zero-wing dart).
> - **Q6 floors (fractions of base):** Cl ≥0.5, c_pitch/yaw ≥0.35, c_roll ≥0.25,
>   Cd0 ≤2×, pilot_gain ≥0.35, bias ≤±0.5. NEVER touch (whitelist): q_att_floor,
>   damp_*, mass, I_*, v_full/redline/min_frac, sim_dt, R, g, rho.
> - **Q4 routing:** body-frame impact offset, EXTENT-NORMALIZED regions (|x|>0.35R →
>   wing by sign(x); z<−0.25R → engine; z>+0.25R → structure; center → structure
>   65% / pilot 35%). +X = right wing. Deterministic, no rng.
> - **Deferred to v2:** G-LOC/input-dropout, splash routing, deadzone/rate-clamp
>   pilot effects, throttle-slew damage, drag-yaw coupling, visual wing break/smoke.

# (original v0 packet below — kept for the rationale)

# Component damage model — design packet (v0, for Fable pre-consult)

> Chad (2026-07-14): "start on the plane's damage model — engine vs pilot vs wing
> break — and a Fable consult to change how the CASCADE and FLIGHT FEEL are based
> on that damage." This is the PLAYER-damage side (the bandit-AI return fire that
> lands hits already exists: `combat/kill.h combat_player_tick` currently drops a
> single `player_hp` and respawns). We replace the single HP pool with COMPONENTS,
> and make each component reshape the flight.

## The one hard constraint — the frozen kernel (firewall)

`sim/` and `control/` are the frozen kernel; combat is WORLD-THREAD and must never
edit them (SPEC §5/§6, the standing rule this whole leg has honored). BUT both
kernel entry points take their params BY CONST REF:

- `sim::step(state, inputs, const AircraftParams& ap, dt)`
- `control::step(s, in, internal, const AircraftParams& ap, const ControllerParams& cp, dt)`

**So damage = a PURE transform on the params (and a bias on the Inputs) that the
app feeds into the UNCHANGED kernel.** No kernel source is touched; the plane
flies differently because it is *given different numbers*. This is the same
"world-thread modulates, kernel stays frozen" discipline as the drone AI.

**Gate-safety (mandatory):** every transform is IDENTITY at zero damage
(engine=1, pilot=1, wings=1, no input bias ⇒ params returned bit-for-bit, no
Inputs change). So all goldens + the `combat: cw=nullptr bit-identical` firewall
test stay green — damage only diverges the flight when a component is actually
hurt (the knob-off strict-superset discipline this repo lives by).

## Components (DamageState, world-side, lives in `combat::CombatWorld`)

```
struct DamageState {
    double engine     = 1.0;  // 1 ok .. 0 dead (no thrust, glide)
    double pilot      = 1.0;  // 1 ok .. 0 incapacitated -> death/respawn
    double wing_left  = 1.0;  // 1 ok .. 0 broken
    double wing_right = 1.0;  // 1 ok .. 0 broken
    double structure  = 1.0;  // airframe/tail/control-runs; overall authority
};
```
`player_hp` (the current single pool) is REINTERPRETED / replaced: pilot is the
lethal one. Death fires when `pilot <= 0` (reuse the existing per-tick
`app::tick` respawn, P0-A). Component values are `[0,1]` health; a hit SUBTRACTS
`ke_damage / component_hp_scale`.

## Damage routing — which component a hit damages (deterministic, no rng)

`combat_player_tick` already computes the impact point of each enemy round on the
player's swept segment. Map the impact into the player's BODY frame and pick the
component by region (a coarse hitbox, no per-part mesh needed):

- impact well FORWARD of CG (−Z, nose) → **engine**
- impact near CG / cockpit (center) → **pilot** (+ some structure)
- impact to the LEFT (−X) / RIGHT (+X) of centerline → **wing_left / wing_right**
- impact AFT (+Z, tail) → **structure**

Deterministic: derived purely from the impact geometry (body-frame offset), no
clock, no rng — a graze on the left wing always damages the left wing. A single
partition (dominant axis of the body-frame offset picks the region) is the v1
proposal; splash to neighbours optional.

## Damage → flight, the transforms (pure `apply_damage`)

### `apply_damage_aircraft(const AircraftParams& base, DamageState) -> AircraftParams`
Fed to BOTH `sim::step` AND `control::step` (H1: the controller inverts the SAME
plant it flies — see Q2). All multiplicative, floored so the plane stays
controllable (see Q6):
- **Engine:** `T_max *= engine` (dead engine ⇒ glide). Optionally throttle_slew↓.
- **Wing (both):** `w = 1 − min(wing_left, wing_right)` damage severity →
  `Cl_max *= (1−kL·w)`, `Cl_alpha *= (1−kL·w)` (mushy, stalls earlier),
  `c_roll *= (1−kR·w)` (sluggish roll), `Cd0 += kD·w` (draggy).
- **Structure:** `c_pitch, c_yaw, c_roll *= (1−kS·(1−structure))` — sloppy overall.

### The asymmetric wing ROLL — the interesting one (Q1)
A point-mass plant has no left/right lift, so a broken wing's *rolling tendency*
must be injected. Proposal: a constant **roll input bias**
`roll_bias = k_asym · (wing_left − wing_right)` added to the commanded
`Inputs.roll` fed to `sim::step` (clamped to ±1). The plane rolls toward the
more-broken wing; the (damaged) controller fights it through the SAME roll axis →
a visible, winnable struggle. (Alternative in Q1: a hidden angular-vel disturbance
the controller can't see.)

### `apply_damage_controller(const ControllerParams& base, DamageState) -> ControllerParams`
**Pilot damage degrades the CASCADE (the pilot, not the plane):**
- Scale the pointing + rate gains down (`K_theta, K_phi, K_w_*` `*= pilot_gain(pilot)`)
  → laggy, sloppy tracking, the nose wallows.
- Optionally widen the deadzone / drop rate clamps as pilot → 0.
- (Deferred? Q3) G-LOC: under high load factor with a hurt pilot, briefly zero the
  Inputs (grey-out) — realistic but a bigger mechanism.

## Per-tick wiring in `app::tick` (only when cw present AND damage nonzero)
```
ap_d = damage_zero(dmg) ? ap : apply_damage_aircraft(ap, dmg);   // else bit-identical
cp_d = damage_zero(dmg) ? cp : apply_damage_controller(cp, dmg);
o = control::step(st.curr, ci, st.internal, ap_d, cp_d, dt);
Inputs cmd = o.inputs;  cmd.roll = clamp(cmd.roll + roll_bias(dmg), -1, 1);
st.curr = sim::step(st.curr, cmd, ap_d, dt);   // raw mode: same ap_d + bias
```
`damage_zero` short-circuits to the untouched kernel path (the gate-safety seam).

## HUD
Extend the player-HP bar into a small COMPONENT panel (engine / pilot / wings /
structure), reusing the bar renderer. Damage cues: engine-out → prop stops +
smoke; wing gone → a visual break (later).

## OPEN QUESTIONS FOR FABLE (please red-team the design + rule these)

- **Q1 (wing roll model):** constant roll-INPUT bias (controller CAN counter it,
  fed through the roll axis it computes) vs a hidden angular-vel DISTURBANCE
  (controller fights blind)? Which is more stable AND feels like a real broken
  wing? Any risk the input-bias just gets trimmed out invisibly by the cascade?
- **Q2 (controller inversion — the H1 call):** feed the controller the DAMAGED
  plant params (it inverts the true degraded plant ⇒ stable, but the plane
  genuinely underperforms) vs NOMINAL params (pilot fights an unresponsive plane
  ⇒ "feels wrong", possibly unstable)? Proposal: damaged params to BOTH (H1
  preserved) + pilot degrades the GAINS separately. Is that the right split, or
  should some mismatch be intentional for feel?
- **Q3 (pilot cascade degradation):** exactly which ControllerParams to scale, and
  the shape of `pilot_gain(pilot)` (linear? floored so it never goes uncontrollable
  above pilot=0?). Does G-LOC / input-dropout belong in v1 or defer?
- **Q4 (hit routing):** is the dominant-body-axis partition acceptable, or should a
  hit split KE across components by weight? Determinism is mandatory (no rng) —
  confirm the geometry-only routing is sound.
- **Q5 (death & the player_hp migration):** pilot-health death vs structural death
  vs both. How should the existing single `player_hp` map — becomes `pilot`, or a
  separate `structure`, and does a fully-broken wing / dead structure also kill?
- **Q6 (controllability floors — critical):** the loader normally guarantees the
  controller is critically damped / ZOH-stable (`K_w >= 4 I K_theta`,
  `K_w dt / I <= 0.5`) and `plant_invert`'s denominator > 0 (`c_axis>0`,
  `q_att_floor>0`). Scaling `c_roll`, `Cl_max`, `K_*` DOWN at runtime bypasses those
  load-time guards. What FLOORS must the damage multipliers keep so a damaged plane
  is degraded-but-flyable and never divides by zero / oscillates by construction?
- **Q7 (firewall confirmation):** confirm that feeding damaged
  AircraftParams/ControllerParams into the unchanged `sim::step`/`control::step` is
  the sanctioned "world-thread modulates, kernel frozen" mechanism — i.e. damage is
  NOT a kernel modification and does not fork the H1 single-source params in a way
  that breaks a golden when damage == 0.

## Determinism / firewall checklist (for the red-team)
- No clock, no rng: routing = impact geometry, cadence = the existing combat pass.
- Identity at zero damage ⇒ every golden + the firewall memcmp test unmoved.
- No kernel source edited; damage is a param/input transform in the world thread.
- Death reuses the crash/`player_hp<=0` per-tick respawn (P0-A) — no new kernel coupling.
