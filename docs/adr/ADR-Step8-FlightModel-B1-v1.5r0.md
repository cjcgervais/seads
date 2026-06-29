# ADR-Step8-FlightModel-B1-v1.5r0 — Real flight model, Phase B1: longitudinal energy (thrust/drag → TAS + throttle)

**Status:** Accepted
**Date:** 2026-06-29
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.5r0 (proposed; bumps from v1.4r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

SEADS through v1.4r0 flies a **constant-TAS kinematic placeholder**: per-aircraft state is
`(lat, lon, psi, phi, alt, tas)`; heading comes only from bank (coordinated-turn law
`ψ̇ = g₀·tan(φ)/V`), the vertical channel is a *commanded climb rate* clamped to an envelope LUT, and
**`tas` never changes**. There is no energy, no throttle, no speed-vs-turn trade — the things that make
a dogfight. This ADR opens Track B (NEXT_STEPS §8): make speed a real integrated state driven by
thrust and drag. It is **core kernel work**, so it is a **seal** and follows the full ritual.

**Rails touched:** none of the immutable rail *values* change (R, Δt, g₀, ATM_TOP, SOFT, geometry,
realm, determinism bans, wire format all stay). The seal is required because the model **moves the
scenario golden `world_hash`es** (a golden hash never moves silently — CLAUDE.md §2).

**Immutable rails that must stay green:** `spec_monotone_check`, `atm_top_probe`, det_math oracle,
the GEO-001/KIN-001 wire format, and **GOLDEN-SK-Sphere-001** (see §2 — it stays byte-identical).

## 2) Decision

### 2.1 State vector — UNCHANGED in B1
Keep the sealed 6-tuple `(lat, lon, psi, phi, alt, tas)` and the canonical snapshot layout (6×f64,
protocol unchanged). `tas` becomes a **truly integrated state** rather than a constant. **No new
stored state** in B1: flight-path angle `γ` and AoA `α` remain *derived* (the vertical channel is
still the commanded climb rate). Promoting `γ` to a stored state is **B2's** decision, and *that* is
the one that will touch the wire (KIN-002).

### 2.2 Inputs — `Command` gains `throttle`
`struct Command { double target_phi; double target_climb; double throttle = 0.0; };`
`throttle ∈ [0,1]` (clamped in the kernel). Default `0.0` so every existing `Command{phi,climb}`
construction still compiles (throttle = idle). The **pitch axis stays commanded-climb-rate** in B1;
**commanded-g vs AoA is deferred to B2** (recommended: commanded-g in B2, AoA/stall in B3).

### 2.3 Per-airframe aero parameters (added to the envelope schema)
New scalar fields in each `data/tuning/envelopes/*.json` under `tuning`, and as scalars on the
`Envelope` struct (`flight_types.h`), emitted as hex-floats by `gen_envelope_tables.py`:
`mass_kg`, `wing_area_m2` (S), `cd0`, `induced_k` (k), `thrust_static_n` (T₀), `v_max_mps` (V_max).

### 2.4 Model constants (kernel-level hex-float, NOT new rail fields)
`RHO0 = 1.225` kg/m³ (sea-level ISA, constant — consistent with the "still air / constant
atmosphere" rail) and `V_MIN = 30.0` m/s (a hard speed floor to keep `ψ̇ = g₀tanφ/V` and the drag
solve well-defined; a *real* stall model is B3). Both live in the kernel + reference as documented
hex-float constants. **ISA density-vs-altitude is explicitly deferred to B5** (it would need
`det_exp`/`det_pow` + MPFR coverage and is a rail change).

### 2.5 Per-tick op order in `step(cmd,env)` — FROZEN (mirror Python ↔ C++ op-for-op)
1. `V = tas`.
2. **Bank slew → φ** — *verbatim as today* (phi_max(V), roll_rate(V), clamp cmdφ, clamp Δ, update,
   clamp). Unchanged.
3. **Climb clamp** — `req = clamp(target_climb, climb_min(V), climb_max(V))`. Unchanged. `req` is the
   commanded vertical rate (m/s), used both as the energy climb cost and the vertical advance.
4. **Energy update (NEW), all with current `V`:**
   - `n   = 1.0 / det_cos(φ)`           (level coordinated-turn load factor; φ < phi_max < 90° ⇒ cos>0)
   - `q   = 0.5 * RHO0 * V * V`          (dynamic pressure)
   - `qS  = q * S`
   - `L   = n * mass * g0`               (lift to sustain the turn, level)
   - `CL  = L / qS`
   - `Dp  = qS * cd0`                    (parasitic drag)
   - `Di  = induced_k * CL * CL * qS`    (induced drag)
   - `D   = Dp + Di`
   - `thr = clamp(throttle, 0.0, 1.0)`
   - `T   = thr * thrust_static_n * (1.0 - V / v_max_mps)`; if `T < 0` then `T = 0`  (prop falloff)
   - `Vdot = (T - D) / mass - g0 * req / V`   (climb bleeds speed; dive adds speed)
   - `Vnew = V + Vdot * dt`; if `Vnew < V_MIN` then `Vnew = V_MIN`
   - `tas = Vnew`
5. **`advance_(i, req)`** using the UPDATED `tas` (so `ψ̇` and the great-circle step use the new V).

No fused multiply-add anywhere (enforced by `-ffp-contract=off` / `/fp:strict`; intermediate
variables make the op order explicit). Every op is correctly-rounded IEEE +−×÷ or `det_cos`.

### 2.6 The no-arg `step()` / GOLDEN-SK-Sphere-001 — UNCHANGED (kinematic anchor)
The no-argument `Kernel::step()` (used only by the straight golden runner) keeps the pure kinematic
tail (constant TAS). **GOLDEN-SK-Sphere-001 stays byte-identical** (`529c6a05…9218fe16`, unchanged
since Pass 1) and remains the cross-toolchain determinism anchor for the great-circle/trig core. The
energy model lives **only** in `step(cmd,env)`, which is the path all gameplay, scenarios, lockstep
and prediction use.

### 2.7 det_math budget — ZERO new primitives
B1 uses only `+ − × ÷` and the existing `det_cos`. It does **not** even need `det_sqrt` (which
already exists and is reserved for B2/B3 stall/corner-speed work). No det_math change ⇒ the oracle
and `gen_detmath_vectors` are untouched.

### 2.8 Wire / reseal scope
`tas` is already on the KIN-001 wire; `throttle` is an **input, not state**, so it is **not** on the
wire. **No snapshot protocol bump in B1** (stays protocol 2). GEO-001, snapshot, interp codecs are
untouched. Only **values** produced by the kernel change.

### 2.9 Goldens moved by this seal
- **UNCHANGED:** GOLDEN-SK-Sphere-001.
- **REGENERATED (hashes move = the seal):** GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — each scenario gets
  an explicit cruise `throttle` per phase so it doesn't merely decay.
- **NEW:** GOLDEN-SK-Accel-001 — wings-level throttle step (idle→full) showing TAS accelerate toward
  a thrust=drag equilibrium, then a chop to idle showing decel toward V_MIN.
- **Net parity vectors regenerated** (same kernel path): `lockstep_vectors.h`, `predict_vectors.h`
  (their scenarios keep throttle = 0 default; parity = C++≡Python is preserved, only the digests
  move). `geo001/snapshot/interp` vectors unchanged.

## 3) Rationale

- **Energy is the heart of the sim.** Thrust−drag makes speed real; induced drag from the bank load
  factor `n = 1/cosφ` makes **turning bleed speed** (the core energy-fight mechanic) and gives every
  airframe a natural sustained-turn / cruise equilibrium. The `−g₀·req/V` term makes **climbing cost
  speed and diving build it** — for free, from the same energy bookkeeping.
- **Determinism-cheap.** No new transcendental ⇒ no new MPFR risk; bit-identity rests on the same
  fixed-op-order discipline already proven for `lut_eval`. The AArch64/FMA risk (the usual divergence
  source) is unchanged.
- **Minimal blast radius.** Keeping the no-arg `step()` kinematic preserves the most-tested golden and
  confines the seal's golden churn to the scenario goldens. Reusing `ref_kernel.step_scenario` means
  lockstep/predict inherit the model with no logic edits — only vector regeneration.
- **Honest scoping.** `γ`/AoA/throttle-on-wire and stall are real follow-ons (B2/B3), called out so
  the wire reseal lands when the state actually grows, not speculatively now.

## 4) Consequences

**Positive:** real speed, throttle, and energy-vs-turn trade; the `--fly` viewer's yaw/throttle
re-seed hack and pitch-cue exaggeration can begin to retire (throttle becomes a real `Command` field
in B1; pitch becomes real in B2). **Negative / watch:** scenario goldens move (expected, that's the
seal); B1 has **no stall** so `CL` is unbounded at low speed — bounded only by `V_MIN` (documented;
fixed in B3). New per-airframe params are an initial balance pass (retuning later is data-only, B4).
**No new gate**, but `tuning_probe.py` should grow an energy sanity check (positive params, V_max >
stall, thrust>0).

## 5) Alternatives Considered

- **Make the no-arg `step()` energy-driven too** (move all 4 goldens incl. Sphere): rejected — needless
  churn of the determinism anchor; the straight golden is a great-circle/trig test, not a flight test.
- **Commanded-g / AoA pitch now:** rejected for B1 — it requires promoting `γ` to state (wire reseal)
  and a stall model; sequenced as B2/B3 to keep each seal small and auditable.
- **ISA density vs altitude now:** rejected — needs `det_exp`/`det_pow` + MPFR + a rail change; B5.
- **Throttle on the wire:** rejected — throttle is an input; the state it affects (`tas`) is already
  on the wire, so remotes/late-join reconstruct correctly without it.
- **Explicit throttle everywhere (no default):** rejected for B1 — a `0.0` default keeps lockstep/
  predict/viewer Command constructions compiling and in-parity with zero edits.

## 6) Acceptance & Probes

Must PASS under v1.5r0:
- `python tools/spec_monotone_check.py config/rails/atm.json`   (rails/roster unchanged)
- `python tools/det_math_oracle.py`                              (det_math untouched ⇒ still PASS)
- `python tools/tuning_probe.py data/tuning/envelopes/*.json`    (+ new aero sanity)
- `python tools/atm_top_probe.py --ceil 8000 --soft 100`
- `for g in gen_*; do python tools/$g.py --check; done`          (all generated headers in sync)
- `python -m pytest tests/property`                              (incl. NEW energy properties)
- **GOLDEN-SK-Sphere-001 world_hash = `529c6a05…9218fe16` (UNCHANGED).**
- GOLDEN-SK-{Turn,Climb,TurnClimb,Accel}-001 reproduced bit-for-bit **C++ ≡ Python** under GCC+Clang
  (and MSVC + AArch64 in guardian CI) — the new sealed hashes.
- `ctest` 7/7 under GCC + Clang (lockstep/predict digests updated).

New energy property tests (`tests/property/test_energy.py`):
- zero-throttle, wings-level ⇒ TAS strictly decreases (until V_MIN);
- some throttle, wings-level ⇒ TAS converges to a fixed equilibrium (thrust=drag);
- banking at fixed throttle bleeds more speed than wings-level (induced drag);
- a steady climb costs speed vs level at the same throttle.

## 7) Ledger Discipline

Receipt via `tools/make_receipt.py` (overall PASS), seal **v1.5r0** in `docs/seals/`, this ADR, and a
NEXT_STEPS update marking B1 done and B2 next. Receipt fields: seal v1.5r0, the four scenario
`golden_sha256` (new) + Sphere (unchanged), commit_sha, toolchain matrix, all gates, signoff Forge.

## 8) Implementation Notes

Build flags unchanged (`DeterminismFlags.cmake`). Files: `flight_types.h` (Command.throttle, Envelope
aero scalars), `tools/envelopes.py` (+ loader fields), `tools/gen_envelope_tables.py` (emit scalars),
`tools/gen_scenario_params.py` (Phase.throttle), `src/kernel/kernel.{h,cpp}` (energy in step(cmd,env)),
`src/kernel/scenario_main.cpp` (cmd.throttle), `tools/ref_kernel.py` (RHO0/V_MIN + energy in
step_scenario + run_scenario throttle), the 3 scenario JSONs (+throttle) + new Accel scenario, and
regeneration of `envelope_tables.h`, `scenario_params.h`, `lockstep_vectors.h`, `predict_vectors.h`.
Mirror-first: get the Python reference + Python gates green (it *defines* the new goldens), then
bit-match C++ and verify cross-toolchain before sealing.
