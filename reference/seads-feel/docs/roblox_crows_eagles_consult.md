# Crows and Eagles — Roblox flight consult packet
*(Export from the SEADS feel department, 2026-07-12. This is a knowledge transfer for
Chad's Roblox game — NOT a SEADS work thread. Pair it with the Gemini deep-research pass:
items marked ⚠VERIFY are Roblox platform facts that should be checked against current
docs — the platform moves fast and this packet's Roblox knowledge has a cutoff.)*

---

## 0. Executive diagnosis: the porpoising

Porpoising (pitch undulation) in a Roblox flight game is almost always one of SIX causes,
and they stack. Triage IN THIS ORDER — each has a 5-minute test:

1. **Variable-dt control.** If your control code runs on `RenderStepped`/`Heartbeat` and
   either ignores `dt` or multiplies gains by a frame-varying `dt`, the loop's effective
   gain breathes with frame rate → oscillation that changes with graphics load. TEST: cap
   FPS (or run on a potato) — if the porpoise frequency/amplitude changes, this is it.
   FIX: §2 (the fixed-cadence control step).
2. **Network-ownership round trip.** If the aircraft's physics is server-owned while the
   pilot's input is client-side, every correction arrives ~100-200 ms late — a delay in
   the loop IS negative damping. This is THE classic Roblox flight porpoise. TEST: in
   Studio single-player (zero latency) does it vanish? FIX:
   `part:SetNetworkOwner(player)` on the whole aircraft assembly (⚠VERIFY current API);
   fly client-authoritative, validate server-side (§4).
3. **Derivative-on-error or smoothed inputs INSIDE the loop.** Any `lerp`/spring/tween on
   the target, the error, or the sensor reading that feeds a control force adds phase lag
   = negative damping. SEADS burned weeks on this class (a smoothed camera basis feeding
   aim). TEST: strip every smoothing from the input→error→force path; smooth only what
   the CAMERA/display shows. RULE: **nothing smoothed inside the loop, ever.**
4. **No rate damping (or damping from the wrong signal).** Damping must come from
   measured angular velocity (`AssemblyAngularVelocity`), i.e. a `−K_ω·ω` term — NEVER
   from differentiating the error (amplifies noise, adds lag). If your controller is
   pure P on pitch-attitude error, it is an undamped spring: it WILL porpoise. FIX: the
   cascade in §3.
5. **Integrator wind-up.** If you have an I term and the nose "carries past" the target
   then slowly comes back (long-period undulation), the integral wound up during the
   correction and keeps pushing after arrival. TEST: zero the I gain — if the porpoise
   period lengthens/vanishes, it's this. FIX: clamp the integral, add the damping
   feedforward (§3.4) so the integrator has nothing to wind up FOR.
6. **Gains over the step-rate ceiling.** Discrete control has a hard stability ceiling:
   as a rule of thumb keep `K_damping · dt / I < 0.5` (dt = your control step, I =
   effective rotational inertia). Doubling a gain that "should" help and getting WORSE
   oscillation is the signature. FIX: respect the ceiling; raise damping before
   stiffness, never past it.

An outer loop that is too hot relative to the inner loop is the seventh cause — but if
you build §3's cascade with the tuning order in §5, you can't create it accidentally.

---

## 1. The scale of the problem (why flight games porpoise on Roblox specifically)

- Roblox physics runs internally at **240 Hz substeps**, but YOUR scripts see it through
  `RunService` events at frame cadence (`PreSimulation`/`Stepped` before physics,
  `PostSimulation`/`Heartbeat` after ⚠VERIFY the current event names — the older
  `Stepped/Heartbeat` naming was being superseded by
  `PreSimulation/PostSimulation`). Forces you set persist across the substeps until you
  change them — so your control loop is a ZERO-ORDER HOLD at frame rate over a 240 Hz
  plant. That's fine IF you treat dt honestly and keep gains under the ceiling.
- The physics engine **throttles** under load (part count, collisions): your effective
  dt spikes, and a marginally-stable tune goes unstable exactly when the server is busy.
  Frame stability is therefore a CONTENT budget problem too (§6), not just a code one.
- Legacy movers (`BodyGyro`, `BodyVelocity`, `BodyPosition`) are deprecated and their
  P/D semantics are opaque — a mistuned BodyGyro is a porpoise machine. Use the
  **constraint actuators**: `VectorForce`, `Torque`, `AlignOrientation`,
  `AngularVelocity`, `LinearVelocity` (⚠VERIFY current recommended set).

---

## 2. Frame-stability architecture (the game-state layer)

The SEADS rule that transfers verbatim: **control is a pure function of (state, input,
dt) at a fixed cadence; render reads state and never writes it.**

- Run the flight controller on the PRE-physics event (`PreSimulation`/`Stepped`) so the
  forces you compute act on the very next physics step — reading state post-physics and
  applying forces pre-physics keeps one consistent loop phase.
- Use the event's REAL dt argument, and if you want bulletproof: run a **fixed-dt
  accumulator** (e.g. 60 Hz control ticks) inside the event — accumulate frame dt,
  execute whole control ticks, carry the remainder. Your gains then mean the same thing
  on every machine, every load. (This is SEADS's app loop; it's also why SEADS can prove
  30 fps and 240 fps produce bit-identical flight.)
- **Input accrual:** accumulate mouse deltas between control ticks and consume them once
  per tick — never per-render-frame into anything the controller reads.
- **Client flies, server referees.** The pilot's client owns the aircraft assembly's
  network ownership and runs the whole cascade locally (zero-latency loop). The server
  validates envelope (speed/energy/position sanity, fire rates) and replicates. Other
  clients see interpolated replication — smooth THAT display-side all you want; it's
  outside the loop.
- **One writer per state.** Aim/target state lives in one module; the camera and HUD read
  it; nothing display-side ever writes back into it. (SEADS's costliest bugs were
  violations of exactly this.)

---

## 3. The control cascade, translated to Roblox

Two loops, strictly layered. Outer: WHERE to point → desired rotation rate. Inner:
desired rate → torque. The pilot's mouse moves an AIM; the plane chases it. This is the
architecture that made SEADS feel "wired."

### 3.1 The aim
- The aim is a world-space direction the mouse moves directly — raw, unsmoothed, 1:1.
  The camera looks where the aim looks. (Smoothing the aim or camera-basis feeding it =
  cause #3 above.)
- Mouse → aim: rotate the aim about the aim frame's own right/up axes by
  `delta_px * sensitivity`. Keep sensitivity a config value; tune it LAST (§5).

### 3.2 Outer loop (pointing → desired rates)
```lua
-- per control tick, all in the aircraft's body frame:
local err = angleBetween(nose, aimDir)          -- pointing error
local errPitch, errYaw = decompose(err)         -- about body right / body up
-- braking law: rate demand grows with error but is CLAMPED, and shrinks
-- near the target so the nose arrives without overshoot:
local pitchRate = clamp(Kp_outer * errPitch, -maxPitchRate, maxPitchRate)
local yawRate   = clamp(Kp_outer * errYaw,   -maxYawRate,   maxYawRate)
-- roll: bank-to-turn — roll toward putting the aim in the vertical plane
-- (lift does the turning); wings-level when the aim is centered.
local rollRate  = bankToTurn(errYaw, bankNow)
```
- Clamp EVERY rate demand (these clamps are your G-limit / comfort envelope).
- A small **deadzone that holds trim**: inside ~0.05° of the aim, freeze the pointing
  demand but KEEP whatever steady deflection you were holding — zeroing it creates a
  limit cycle (SEADS lesson, verbatim transferable).

### 3.3 Inner loop (rate → torque), the anti-porpoise core
```lua
local w = body:GetAssemblyAngularVelocity()      -- measured, body frame
local eW = desiredRate - w                       -- rate error
integ = clamp(integ + eW * dt, -integCap, integCap)
torque = Kw * eW + Kwi * integ                   -- rate-PI
```
- **Damping = the `Kw * eW` term on MEASURED rate.** No derivative of anything.
- `integCap` small — the integrator exists only to hold trim.

### 3.4 The plant-inversion feedforward (what SEADS calls damp_ff — the "wired" feel)
If your aerodynamic model applies a damping torque `−D(V) * w` (it should — rotation
resisted proportional to airspeed), then the controller should ADD `D(V) * desiredRate`
to the torque up front. The air's tax is pre-paid; the rate loop tracks exactly; the
integrator idles; there is no sag at speed and no carry-past after an input stops. This
one term fixed SEADS's last three felt complaints. You control the plant's code in
Roblox exactly like SEADS does — so you can invert it exactly.

### 3.5 Getting inertia (the one thing Roblox hides)
Torque needs the assembly's rotational inertia and Roblox doesn't expose the tensor
(⚠VERIFY — there were proposals). MEASURE it instead, the SEADS way: an in-Studio probe
that applies a known constant torque for N ticks from rest and reads the achieved
angular acceleration (`I = τ / α`), per axis, once per aircraft design. Bake the numbers
into config. Instruments beat guesses.

### 3.6 Actuator choice
- Best control-authority match: a **`Torque` constraint** (body-relative) driven by §3.3
  — you own the whole law.
- Acceptable shortcut: **`AlignOrientation`** with `RigidityEnabled = false`,
  `MaxTorque`/`Responsiveness` tuned — but its internal PD is opaque; if you porpoise
  you can't see why. Use it for drones/props, not the player plane.
- Apply aero forces (lift/drag/thrust) with `VectorForce` at the assembly root; keep
  decorative parts `Massless = true` and welded so cosmetics never change the plant.

---

## 4. Publishing / performance checklist (frame stability is a content budget)

- **Network ownership** set explicitly for every aircraft (auto-ownership migrates and
  causes mid-flight physics handoffs = sudden porpoise onset). ⚠VERIFY API.
- **Aircraft assembly lean:** minimal collidable parts (Box collision fidelity), one
  root, welded massless cosmetics; Humanoid OUT of the physics (seat weld or
  VehicleSeat with the character non-colliding) — Humanoid physics fights yours.
- **StreamingEnabled** for a big map; keep the aircraft + its scripts in a
  non-streamed container so the plant never streams out mid-flight. ⚠VERIFY current
  streaming controls.
- Watch `workspace:GetRealPhysicsFPS()` (⚠VERIFY) in telemetry; if it dips below 240,
  physics is throttling — your content budget is blown and no tune will hold.
- Mobile: mouse-aim needs a touch mapping (virtual joystick moves the aim, NOT the
  plane) — decide early; it constrains the UI and the sensitivity table.
- Server: keep the authoritative validation cheap (position/velocity envelope checks a
  few times a second), never re-simulate flight server-side.

---

## 5. The tuning discipline (SEADS's most transferable asset)

1. Build a **step-response instrument FIRST**: a Studio command that snaps the aim N
   degrees off the nose in level flight and logs error-vs-time (settle time, overshoot,
   number of direction reversals). Porpoising becomes a NUMBER before it's a feeling.
2. Tuning order: inner loop before outer. (a) Set `Kw` so a rate step settles briskly
   with no reversal; (b) `Kwi` just enough to hold trim; (c) THEN raise outer `Kp` until
   pointing is crisp — if it rings, LOWER `Kp` or raise `Kw`, never anything else.
3. **One dial per play-test.** Log every change with a hypothesis and the felt verdict.
   Numbers explain a feeling; they never replace flying it.
4. The instrument's classic trap (SEADS hit it twice): a settle/oscillation counter that
   accidentally counts deadzone re-arm taps as "oscillation." Gate your reversal count
   on amplitude (> 0.1°).
5. When a felt complaint appears ("it sits high", "mushy at speed"), ATTRIBUTE it to a
   mechanism with the instrument before touching any gain — SEADS's biggest wins
   (carry-past = integrator wind-up; fade-at-speed = damping sag; nose-parks-high =
   a wrong radius constant) were all attributions, not tunings.

---

## 6. For the Gemini deep-research pass — the questions worth its time

1. Current best practice for the RunService phase to apply constraint forces
   (`PreSimulation` vs `Stepped`) and whether constraint targets are consumed per
   substep or per frame. (Determines §2's loop phase.)
2. Whether Roblox now exposes assembly inertia tensors or torque-at-point APIs
   (replaces §3.5's measurement probe).
3. `SetNetworkOwner` current semantics + anti-exploit patterns for
   client-authoritative vehicles (server envelope validation examples).
4. Physics throttling behavior: what triggers it, how it reports, how flight games
   detect and degrade gracefully.
5. Aerodynamics on Roblox: whether the built-in `EnableFluidForces`/aerodynamic force
   model (⚠ it existed in beta) is production-ready for a dogfighter, or whether a
   custom lift/drag model (SEADS-style, fully invertible) remains the right call —
   custom is STRONGLY favored by this packet: you can't invert a plant you don't own.
6. Mobile input patterns for mouse-aim flight (camera-relative virtual stick vs drag
   aiming) with published examples.

---

## 7. The one-page summary

Porpoising = phase lag or missing damping in a discrete loop. Kill it by: client-owned
physics (no network delay in the loop) → fixed-dt control on the pre-physics step →
damping from measured angular velocity (never error derivatives, never smoothed inputs)
→ capped integrator + damping feedforward (pre-pay the air) → gains under the step-rate
ceiling → outer loop slower than inner. Build the step-response instrument before
tuning, change one dial per flight, and let the pilot's hands — not the numbers — issue
the verdict. That discipline, more than any single gain, is what made SEADS's kernel
feel inevitable.
