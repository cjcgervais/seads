# reference/seads/ — point-in-time snapshot

These files are **copies**, taken 2026-07-23, of `D:\SEADS_2026\src\kernel\*.h` / `*.cpp` —
the deterministic C++ spherical-earth flight/combat kernel ("ATM-Sphere") for the SEADS
(Spherical Earth Aerial Dogfighting System) project:

- `kernel.h` / `kernel.cpp` — the `seads::Kernel` class: fixed 100 Hz step, struct-of-arrays
  aircraft + projectile state, great-circle navigation on a sphere of radius `Rails::R`
  (15000 m in this snapshot), ISA atmosphere model, ballistic projectiles, hit detection,
  region damage (engine/wing/tail).
- `flight_types.h` — shared types: `Lut5` (5-point lookup table), `Envelope` (per-aircraft
  tuning: mass, wing area, drag/lift coefficients, thrust, V-n limits, weapon roster),
  `Command` (per-tick target bank/load-factor/throttle/fire).
- `envelope_tables.h` — auto-generated per-airframe `Envelope` constants (A6M2, BF109F4,
  KI61, LA7, P47D, P51, SPITFIRE_MK5, YAK3), hex-float literals shared bit-for-bit with the
  Python reference kernel.
- `golden_params.h` / `golden_main.cpp` — the deterministic "golden" replay harness.
- `scenario_params.h` / `scenario_main.cpp` — scenario-driven replay harness.

**D:\SEADS_2026 is READ-ONLY from this repo and is the authoritative live tree.** This
kernel is pure physics/state — it has no camera code. The "camera independence for motion
sickness" principle referenced in `docs/cascade/spherical-earth-non-euclidean.md` is
implemented on the EvC2026 side (`BirdController.client.luau`'s `updateCamera`); this SEADS
snapshot supplies the non-euclidean geometry half of that doc (the great-circle math,
coordinated-turn law, sphere radius) since SEADS is the project that actually models a
small, non-euclidean sphere.

Do not hand-edit these files. Re-snapshot from the live tree per the extraction plan in the
repo root `CLAUDE.md`.
