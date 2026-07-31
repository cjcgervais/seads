# V4 RUNG 3 SPEC — freelook globe inertia (S-globelook)

Program: `program.md` ask 3. Research: `docs/v4_research.md` §4 (all three mitigations
MANDATORY). Chad's words: freelook "like it has some inertia, move it like a globe with
the hand." COSMETIC — app/main.cpp orbit block only; zero kernel contact, zero aim contact
(the orbit is the §9.2 cosmetic consumer, never feeds mouse→aim).

## Mechanism

Kinetic orbit while freelook is HELD (the globe): the orbit carries an angular velocity.

- While held AND the mouse moves this frame: INSTANT GRAB — the hand owns the globe. The
  orbit position updates exactly as today (delta·sensitivity), and the velocity estimate
  updates to track the hand (EMA over ~50 ms of delta/frame_dt is fine — the estimate only
  seeds the coast, it never modifies the held-motion position path, which must stay
  BIT-IDENTICAL to today while the mouse moves).
- While held AND the mouse is still: the globe COASTS — orbit advances at v(t) with
  v decaying as exp(-dt/τ), τ = `freelook_inertia_tau` (ship 0.2 s; research band
  150-250 ms — "a heavier globe").
- Velocity cap `freelook_inertia_cap` (ship 90°/s): applied to the coast velocity (and the
  seeded estimate) so a violent flick cannot spin the view fast enough to trigger vection.
- ANY new input while coasting = instant grab (the coast velocity is REPLACED by the live
  hand, never blended — the DCC-tool convention; inertia-without-instant-grab is nausea,
  memo trap #4).
- On freelook RELEASE: the existing ease-home decay (exp ~120 ms to zero orbit) is
  UNTOUCHED and also kills the coast velocity — release never coasts, it eases home as
  today (S-freelook360 short-way-home semantics preserved).
- Clamps: coasting respects the SAME bounds as the hand — yaw wrap (yaw_max=0 arm) or
  clamp, and the asymmetric pitch floor/cap. Hitting a clamp ZEROES that axis's coast
  velocity (no winding behind the wall).
- Resets: respawn / focus loss / orient verb / raw-mode toggle zero the velocity exactly
  where they zero the orbit today (grep every `orbit = render::CameraOrbit{}` site).

## Dials (config/controller.toml [freelook], ControllerParams, loader)

- `inertia_tau = 0.2`   # [s] coast decay; 0 = structurally OFF — the whole mechanism is
                        # skipped and the orbit block is bit-identical legacy (gate the
                        # code on tau > 0, the S7-hrz rate=0 pattern).
- `inertia_cap = 90.0`  # [deg/s] coast/seed velocity cap (stored radians in params).

## Where

app/main.cpp orbit block (~line 360-397) + the release-decay block (~450). State lives
beside `orbit` as caller persistents (a small struct, e.g. `render::OrbitInertia` in
render/camera.h if a pure home is preferable — pure + testable is better: put the
velocity/coast math in a pure render:: helper struct with its own unit legs, main.cpp owns
only the glue, the flight-audio pattern).

## Tests

Pure-helper unit legs (test/unit/, Catch2 names without `]`/`,`):
1. tau=0: helper emits zero coast always (structural off) — and the main.cpp glue is gated
   so the orbit math is the legacy expression tree.
2. Instant grab: coasting at v, an input frame replaces v with the hand's rate (no blend).
3. Coast decay: v(t) halves every tau·ln2; position integrates it (non-no-op fixture:
   REQUIRE v0 > eps before the ratio check).
4. Cap: seed above cap clamps to cap.
5. Clamp-zero: coast into the pitch floor zeroes pitch velocity.
6. Frame-rate: two 8 ms frames ≈ one 16 ms frame within tolerance (exp decay is
   dt-correct by construction — pin it).

NO ctest runs seads.exe (the green gate is blind to caller glue) — after landing, note on
the fly card that the feel proof is Chad's stick; the pure helper carries the tested law.

## Constraints

- NEVER touch sim/, control/, the aim path, or input::Freelook latches. The CQ2
  mouse→aim suspension and S7-hrz capture semantics are sacred — the inertia moves only
  the cosmetic orbit angles.
- The held-with-motion path must stay bit-identical (inertia adds motion only on
  mouse-still held frames and coast frames).
- Degrees only at the toml boundary.
