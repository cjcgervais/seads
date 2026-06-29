# ADR-Step4-Scenarios-v1.3r0 — Envelope-driven flight inputs + scripted-timeline goldens

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge — kernel/data
**Seal:** ATM-Sphere v1.3r0
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Through v1.2r0 the kernel ignored the tuning envelopes: every aircraft flew a straight great-circle
(φ=0, climb input hardcoded 0 in both `Kernel::step` and `ref_kernel.py`). With all 8 envelopes
sealed, the kernel can now *maneuver*. This adds new world dynamics, so it is a **seal event**
(minor bump → v1.3r0). Immutable rails that MUST stay green and are untouched here: sphere
R=15000/flattening=0; ATM-only; ATM_TOP=8000/SOFT=100; g₀=9.80665; Δt=0.01; S² coordinated turn
`ψ̇=g₀·tan(φ)/V`; det_math only; GEO-001 wire. The existing **GOLDEN-SK-Sphere-001** world_hash
`529c6a05…9218fe16` MUST remain byte-identical.

## 2) Decision
Add an envelope-driven flight-input model and three new sealed goldens driven by **scripted-timeline**
inputs.

- **Dynamics** (`src/kernel/kernel.cpp` + `tools/ref_kernel.py`, bit-identical op order):
  the current step-body tail is factored into a shared `advance_(i, req)` /  `_advance`. The no-arg
  `step()` (straight golden) calls it with `req=0` and φ unmodified ⇒ **byte-identical to v1.2r0**.
  A new `step(cmd, env)` overload, per aircraft and in this fixed order:
  `phimax=lut_eval(phi_max,V)`, `rollrate=lut_eval(roll_rate,V)`,
  `cmdphi=clamp(target_phi,±phimax)`, `step_max=rollrate·dt`, `delta=clamp(cmdphi−φ,±step_max)`,
  `φ=clamp(φ+delta,±phimax)`, `req=clamp(target_climb, climb_min(V), climb_max(V))`, then `advance_`.
  TAS is held constant (no energy model) — a documented step-4 approximation.
- **LUT interpolation** `lut_eval` (clamped piecewise-linear, only `+ − * /`) added to both
  `tools/detmath_ref.py` and `src/kernel/kernel.cpp` with identical op order. deg→rad is baked into
  the LUT nodes once by `tools/envelopes.py` (convert-then-interpolate), so the kernel interpolates
  pure-radian tables.
- **Scenarios** in `config/scenarios/<id>.json` (one per golden; kept out of the immutable rails
  file). Schedule = step function of `u32` tick (integer phase select; no float in control flow).
  - `GOLDEN-SK-Turn-001` — ki61 @ TAS 140, roll to 45° bank at tick 200, hold; 6000 ticks.
  - `GOLDEN-SK-Climb-001` — bf109f4 @ TAS 80, 12 m/s climb from 7800 m into the ceiling; 3000 ticks.
  - `GOLDEN-SK-TurnClimb-001` — spitfire_mk5 @ TAS 120, roll in then add 8 m/s climb; 4000 ticks.
- **Generated headers** (hex-float, `--check` in CI): `tools/gen_envelope_tables.py` →
  `src/kernel/envelope_tables.h`; `tools/gen_scenario_params.py` → `src/kernel/scenario_params.h`
  (start angles + scheduled bank targets pre-converted to radians). Shared types in
  `src/kernel/flight_types.h`.
- **Runner**: new `seads_scenario --id <GOLDEN-SK-…>` (`src/kernel/scenario_main.cpp`); the Sphere
  runner `golden_main.cpp` is untouched.
- **Seal**: `config/rails/atm.json` header → `ATM-Sphere v1.3r0` / version 130 (the `golden` object
  is NOT touched, so the Sphere hash is preserved).

New sealed world_hashes:
| Golden | world_hash |
|--------|------------|
| GOLDEN-SK-Turn-001 | `6160540ccd57c17ceb9020559e077ed3ddc042b2552f5b9e64c3b57313f152ee` |
| GOLDEN-SK-Climb-001 | `74b9d556c6acec587cff6a38d1e816e1ab68355c4bfbd9ffe512b5032d9b6682` |
| GOLDEN-SK-TurnClimb-001 | `f7193b998bf55956dc648a08c4c94f7c5d387fc3cff1c4b7fbcb61ce7cedd413` |
| GOLDEN-SK-Sphere-001 | `529c6a05…9218fe16` (UNCHANGED) |

## 3) Rationale
The reduce-to-current proof (target_phi=0, φ=0 ⇒ φ stays 0; target_climb=0 ⇒ req=0) guarantees the
straight golden is byte-stable, and the Sphere keeps its own no-arg runner regardless. Bit-identity
across MSVC/Clang/GCC × x64/AArch64 holds because every new op is `+ − * /` (no FMA — banned by
`DeterminismFlags.cmake`), constants flow through hex-float headers, and control flow is integer-only.
Locally verified: GCC + Clang (winlibs MinGW) reproduce all four goldens bit-for-bit against the
Python reference.

## 4) Consequences
Positive: aircraft now bank and climb within their envelopes; unblocks future maneuver content and is
the first multi-golden seal. New CI coverage (2 generator `--check`s; build/run/validate of 3 goldens
per toolchain; per-golden aggregation). Negative: constant-TAS approximation (energy/drag deferred to
a later seal); scenario TAS must lie inside the chosen envelope's LUT domain (asserted by design;
end-clamped `lut_eval` is well-defined regardless).

## 5) Alternatives Considered
Constant held commands (rejected — user chose scripted timeline for richer maneuvers).
Scenarios inside `atm.json` `golden`/`goldens[]` (rejected — bloats the immutable rails surface and
risks perturbing the Sphere object). Interpolate-then-convert for φ_max/roll_rate (rejected —
`convert(interp)` ≠ `interp(convert)` bit-wise; bake conversion into nodes instead). Re-using
`golden_main.cpp` for scenarios (rejected — keep the sealed Sphere runner untouched).

## 6) Acceptance & Probes
- `python tools/spec_monotone_check.py config/rails/atm.json` → PASS
- `python tools/tuning_probe.py data/tuning/envelopes/*.json` → PASS
- `python tools/atm_top_probe.py --ceil 8000 --soft 100` → PASS
- `python -m pytest tests/property` → PASS (incl. 3 scenario sealed-hash + bank-limit + zero-command)
- `gen_envelope_tables.py --check` / `gen_scenario_params.py --check` (+ existing 3) → PASS
- `GOLDEN-SK-Sphere-001` world_hash **unchanged** (`/golden`).
- `ref_kernel.py --scenario … ` + `validate_snapshot.py` for the 3 new ids → PASS.
- CI `guardian.yml`: MSVC + GCC/Clang × x64/AArch64 reproduce all four goldens; aggregation PASS.

## 7) Ledger Discipline
Forge Audit Card `docs/cards/FORGE_AUDIT_CARD-Step4-v1.3r0.md`; `docs/SEAL_CARD.md` v1.3r0 row;
Chronicler `tools/make_receipt.py` (now also validates the 3 scenario goldens), `overall: PASS`.

## 8) Implementation Notes
No build-flag change; determinism bans remain enforced; no new det_math (linear interpolation is
`+ − * /`). Tuning/scenario data parsed once into tables before the sim; no live-reload across the
lockstep boundary. Envelope JSONs keep their v1.2r0 headers (values unchanged; they ride the seal).

**Appendix — Roster (sealed 8):** P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, P-51.
