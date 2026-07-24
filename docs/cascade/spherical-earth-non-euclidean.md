# Spherical-earth flight: the non-euclidean special case

SEADS (Spherical Earth Aerial Dogfighting System) is a deliberately small-radius sphere
world — the ground curves away fast enough that the geometry is a genuine non-euclidean
special case, not a flat-earth approximation with a rounding error.

---

## Feel

On a small enough sphere, "flying straight" and "flying level" stop being the same promise
they are on a flat map — hold a heading long enough and you'll come back around, and the
horizon itself is curving under you the whole time, not just at the edges of a huge, mostly-
flat world. That's the setting SEADS is built for.

Because the world curves, and because the camera necessarily has to interpret "level" and
"ahead" against a horizon that's always subtly turning, the same discipline that matters in
the flat EvC2026 world matters *more* here: the camera can only ever look like it's swinging
toward centre — it can never actually be the thing driving the aircraft. If the camera's own
motion ever leaked into the control input (even a little, even smoothed), a small-sphere
world's constant, everywhere-present curvature would turn that leak into a constant, subtle
mismatch between what the player commands and what happens — read as nausea rather than as a
one-off jolt. Player efficacy (do I feel like I'm flying this?) and player comfort
(am I going to feel sick?) both ride entirely on keeping that coordination airtight.

## Principle

Two separate claims, both load-bearing:

1. **The world itself is non-euclidean.** SEADS aircraft don't move on a flat plane with an
   altitude offset — they move on the surface of a sphere of finite, small radius. Straight-
   line motion is a great-circle arc, not a Cartesian ray; "bearing" is only locally
   meaningful, because parallel transport around a small sphere doesn't preserve direction
   the way it does (to good approximation) on a large one or a flat map. This is what "small
   sphere" is doing as a design choice: it makes the curvature felt, not just technically
   present.
2. **Camera independence from control is what keeps that world flyable without nausea.** The
   general principle (documented on the EvC2026 side, see
   `mouse-aim-instructor-cascade.md`) is: the camera may lag, ease, and visually swing toward
   the aim or heading — but that motion must be strictly downstream of the control law, never
   upstream of it. On a small non-euclidean sphere this isn't a nice-to-have, it's the whole
   game: a camera that even partially drives the aircraft would compound with the
   ever-present curvature into a control loop the player can't build a stable mental model
   of, which is exactly the mechanism that produces motion sickness in VR/flight-sim design
   generally. Keep the camera a pure, one-way function of state (never a source of control
   input) and the curvature stays legible instead of nauseating.

## Math

`Rails` (the kernel's fixed simulation constants) fixes the sphere radius at
`R = 15000.0` (metres) alongside `dt = 0.01` (100 Hz fixed step), `g0 = 9.80665`,
`atm_top = 8000.0` m, and a `soft = 100.0` softening constant.

Navigation is closed-form intrinsic-S2 great-circle stepping (`great_circle_step`): given a
current `(lat, lon)`, a bearing `psi`, an arc-length `s`, and the sphere radius `R`, it
computes `alpha = s / R` and advances position via spherical trigonometry (`det_sin`/
`det_cos` — a deterministic math layer, not libm) rather than any flat-plane vector add. This
is the literal mathematical expression of "the world is a sphere, not a plane."

The turn law is the standard coordinated-turn relation adapted to this setting:
`psi_dot = g0 * tan(phi) / V` — heading rate is driven by bank angle `phi` and true airspeed
`V`, with `cos(gamma) → 0` (a vertical flight path) documented as an edge case in the
kernel's own comments. Altitude is bounded against `atm_top` (8000 m), and air density falls
off via a 17-node ISA sigma lookup table across that same band — so the "how thin does the
air get, how high can you go" envelope is also finite and close to the ground, reinforcing
the "small world" feel rather than fighting it.

The horizontal hit-detection test for weapons fire (`projectile_hit_`) is likewise done in
spherical terms — the law of cosines on the sphere (`cosc = sin(lat_p)*sin(lat_j) +
cos(lat_p)*cos(lat_j)*cos(dlon)`, compared against a precomputed `COS_HIT_ANGLE`) rather than
flat Euclidean distance. Every piece of geometry in this kernel, not just the top-level
navigation, is genuinely spherical.

None of this — the sphere radius, the great-circle step, the coordinated-turn law — has
anything to say about the camera. The camera-independence principle is implemented entirely
on the EvC2026 side (see Code, below); SEADS supplies the non-euclidean geometry the
principle has to survive contact with.

## Code

Snapshot: `reference/seads/kernel.h`, `reference/seads/kernel.cpp`.

- `struct Rails` — `kernel.h` — `R = 15000.0`, `dt = 0.01`, `g0 = 9.80665`, `atm_top =
  8000.0`, `soft = 100.0`.
- `great_circle_step(lat, lon, bearing, s, R, lat2_out, lon2_out)` — `kernel.cpp` — the
  closed-form intrinsic-S2 step; used by both `Kernel::advance_` (aircraft) and
  `Kernel::advance_projectiles_` (bullets).
- `Kernel::advance_(std::size_t i, double req)` — `kernel.cpp` — per-aircraft kinematic tail:
  coordinated-turn (`psi_dot = g0*tan(phi)/V`) + great-circle horizontal advance + ceiling-
  clamped vertical.
- `Kernel::projectile_hit_(std::size_t p_idx)` — `kernel.cpp` — spherical law-of-cosines hit
  test against `COS_HIT_ANGLE`.
- `ISA_SIGMA_ALT[17]` / `ISA_SIGMA[17]` — `kernel.cpp` — the altitude-density lookup table
  bounding the usable vertical band under `atm_top`.
- Camera-independence principle (not in this snapshot's kernel — it's an EvC2026 concept):
  see `mouse-aim-instructor-cascade.md` § Code, `updateCamera(dt)` in
  `reference/evc2026/BirdController.client.luau`.
