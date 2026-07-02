# ADR — Step 8 Flight Model, Phase B5: ISA atmosphere (density vs altitude) — seal ATM-Sphere v1.21r0

**Status:** accepted (sealed v1.21r0, 2026-07-02)
**Depends on:** B1 energy (v1.5r0), B2 lift/pitch (v1.6r0), B3 limits/stall (v1.7r0), B4 retune (v1.8r0)

## Context

Since B1 the flight model used a CONSTANT sea-level density (`RHO0 = 1.225`); altitude changed
nothing but the ceiling clamp. The original B5 plan (§8.5 of the roadmap) deferred ISA density
because it seemed to force `det_exp`/`det_pow` into det_math with full MPFR coverage — "big lift".
That fear predates the envelope LUT machinery: since v1.3r0 the kernel evaluates sealed
piecewise-linear tables through a deterministic `lut_eval` (pure `+,-,*,/`, fixed op order,
bit-identical Python ↔ C++). A density RATIO is just one more sealed table.

## Decision

**1. σ(alt) is a sealed 17-node LUT, not a runtime power law.**
`sigma(h) = rho(h)/rho0`, nodes every 500 m over the whole ATM band [0, 8000 m]. Node provenance
is the ICAO ISA troposphere — `sigma(h) = ((T0 − L·h)/T0)^(g0/(Rs·L) − 1)`, `T0=288.15 K`,
`L=0.0065 K/m`, `Rs=287.05287 J/(kg·K)`, `g0` = the gravity rail — evaluated OFFLINE by the new
provenance tool `tools/gen_isa_lut.py`; the emitted f64 hex-floats are the sealed spec, shared
bit-for-bit between `ref_kernel.py` (`ISA_SIGMA_ALT`/`ISA_SIGMA`/`air_sigma`) and `kernel.cpp`
(same names). Runtime is `lut_eval` — **ZERO new det_math** (ninth consecutive
zero-new-transcendental seal). Max chord error vs the power law is ~2.2e-4 absolute near the
ground (guarded by `test_isa.py::test_sigma_lut_tracks_the_power_law`); node values equal the
power law exactly (bit-for-bit, guarded).

`lut_eval` in kernel.cpp was generalized from its hard-coded 5-node form to take a node count.
For any given `(xs, ys, x)` the op sequence is IDENTICAL to the old body (only the last-node
index is parameterized), so every sealed envelope-LUT product is bit-for-bit unchanged — proven
by the goldens (the trajectories re-validated before σ was applied... and by Sphere/lockstep
staying green after).

**2. σ applies at exactly TWO points in the envelope-driven step** (`step_scenario` ↔
`Kernel::step(cmd, env)`, mirrored bit-for-bit; `sigma = air_sigma(alt)` with the PRE-step
altitude, matching the V the solve uses):
- `q = 0.5 * RHO0 * sigma * V * V` — dynamic pressure. Drag (parasitic AND induced) falls
  aloft, and the **B3 aero ceiling `n_aero = cl_max·qS/(m·g0)` becomes altitude-dependent**:
  stall/corner TAS rise with altitude, sustained turns bleed harder up high (induced drag at
  fixed n carries 1/σ).
- `T = thr * thrust_static_n * (1 − V/v_max) * sigma` — **engine power scales with density**
  (sea-level power is NOT held to altitude).

**3. Thrust ∝ σ (the conservative choice).** With BOTH q and T scaled, level top TAS stays near
the sealed B4 historical values at every altitude (σ cancels in the parasitic-drag balance;
induced costs a little) — no airframe exceeds its B4 top speed anywhere in the band. The
alternative (hold sea-level power — "perfect supercharger") inflates top TAS ~1/√σ ≈ +50% at
8000 m, past the historical envelope. Real WWII engines sat between (supercharged to a critical
altitude); **per-airframe critical-altitude modeling is deferred** as a data-driven follow-up
(an envelope scalar + one comparison; its own seal).

**4. Deliberately untouched:**
- the **no-arg kinematic path** (`_advance`/`advance_`) — no aero at all ⇒ **the Sphere golden
  is byte-identical** (the seal's anchor);
- the **projectile advance** — `PROJ_DRAG_K` stays a lumped GLOBAL ("a bullet is a bullet",
  G3 doctrine). ISA-scaling round drag would be its own kernel seal;
- gravity (constant rail), still-air (no wind — `atmosphere: still_air` unchanged), the wire
  (**no protocol change** — σ is derived from `alt`, which already rides GEO-001).

## Golden consequences (a genuine MODEL seal)

Every scenario flies at 800–7800 m where σ < 1 ⇒ **the 12 scenario goldens move on genuinely
changed trajectories** (NOT strip-provable, NOT value-only — this is B2-class). Sphere is
byte-identical. All sealed stories were RE-VERIFIED under B5:
- **Hit-001**: the tail-chase kill still lands (a6m2 hp 0, tail 0, lhb 0; p47d kills 1).
- **Winchester-001**: depletion at tick 891 exactly (fire-schedule-driven, σ-independent).
- **EngineOut-001**: same 4-round head-on connect, same 26.25→14.25→2.25→0 drain VALUES, hp 22
  survivor — now decelerating to ~137 m/s (not ~131) because thinner air bleeds the thrustless
  glider more slowly.
- **YakLa-001 RE-DESIGNED**: under B5 the original profiles degenerated (Yak-3 peaked 157.1 —
  never crossed its 160 breakpoint; La-7 peaked 122.6 — never crossed 125, pull aero-limited at
  n_aero ≤ 5.99). The golden's raison d'être is LUT-segment traversal, so its schedules were
  re-designed against measured B5 dynamics using DIVE phases (gravity is σ-independent): all
  crossings restored (160 up/down; 125 up/down + 85 down = three La-7 segments), the g-8 pull
  binds the STRUCTURAL limit throughout (n_aero [8.70, 9.00] > 8, measured), γ stays inside the
  documented ±90° envelope (max +46.8°), and the closing bursts are unchanged (ammo 140→111 /
  170→136, 63 live rounds sealed). Scenario JSON description re-written with measured numbers.
- **NEW GOLDEN-SK-Altitude-001** (the B5 golden): two Spitfire Mk Vs, IDENTICAL schedules,
  altitude the ONLY variable (800 m vs 6900 m; no guns ⇒ provably non-interacting). Divergent
  full-throttle acceleration (~10.7 m/s by t=2000), the SAME g-6 pull un-clamped low
  (n_aero > 11) vs AERO-CLAMPED high (n_aero [4.90, 5.25] < 6), and both zooms cross a σ-LUT
  node (1000 m / 7000 m) ⇒ interpolation AND segment switching sealed. guardian.yml gains it in
  all three golden lists (13 goldens total).

## Downstream fingerprint

- **Regenerated:** `scenario_params.h` (YakLa redesign + Altitude), `lockstep_vectors.h`,
  `predict_vectors.h` (embedded trajectories moved), `session_vectors.h` (digest
  `d0e94e2e…` → `21aaab49…`; checkpoints at ticks 1/50 byte-identical, 100+ move;
  **FINAL_WEAPON facts identical** — the astern kill lands on the same ticks).
- **Byte-identical (verified `--check`):** `event_vectors.h` (the per-round journal's integer
  milli-hp deltas + ticks did not move), snapshot/weapon/framing/geo001/interp/detmath/envelope
  vectors (no wire or tuning change).
- **Riders:** `trajectory.js` regenerated (same 6-ship dogfight, same 3 kills / 18 events);
  `test_stall.py` + `test_region_toughness.py` made σ-aware (the expected-value helpers carry
  the same σ the kernel uses; the glider-vs-powered window extended 300→500 ticks).
- **Property tests 177 → 182** (+5 `tests/property/test_isa.py`: node values ≡ power law
  bit-for-bit, LUT tracks the law < 2.5e-4, end-clamping, altitude degrades acceleration on all
  8 airframes, aero ceiling scales EXACTLY with σ through the kernel).
- Rails 300→310: new `atmosphere_density` block + `flight_model` text (B5).

## Rejected alternatives

- **Runtime `det_pow`/`det_exp`** — rejected: needless MPFR/ULP/FMA-audit surface for a smooth
  1-D function of a clamped state; the LUT is the established sealed-table pattern.
- **Thrust held at sea-level power** — rejected (see Decision 3): unhistorical top speeds aloft.
- **Gagg–Ferrar power lapse `(σ−0.117)/0.883`** — deferred with supercharger modeling: another
  constant to seal with no fidelity claim behind it at this granularity.
- **Scaling projectile drag by σ(alt)** — deferred: moves every firing golden for a second-order
  effect on 2.5 s-lifetime rounds; kept the lumped-global doctrine.
- **Leaving YakLa-001 as-was** — rejected: its breakpoint-crossing coverage would have silently
  degenerated (the non-degeneracy tests only guard the setup, not the crossings); re-designing
  the scenario inside the seal keeps the golden honest.

## Gates

15/15 receipt PASS; Sphere `6914a994…2b13eb20` byte-identical GCC + Clang; all 12 scenario
goldens C++ ≡ Python bit-for-bit on GCC AND Clang locally; ctest 18/18 both toolchains; 182
property tests; all 13 generated headers in sync; cross-toolchain matrix (MSVC + AArch64) proven
in guardian CI.
