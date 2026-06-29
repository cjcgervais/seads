# ADR-Step8-FlightModel-B3-v1.7r0 — Real flight model, Phase B3: limits & stall (C_Lmax, accelerated stall, structural g, corner speed)

**Status:** Accepted
**Date:** 2026-06-29
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.7r0 (proposed; bumps from v1.6r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

B2 (v1.6r0) made pitch real (flight-path angle `γ` a stored state, driven by a commanded load
factor `n`), but `n` was only bounded by a **global placeholder clamp** `[N_MIN, N_MAX] = [−3, 9]`.
There is no per-airframe limit, no stall: a slow aircraft can magically pull 9 g, and the wing makes
unbounded lift. B3 fixes that — the headline of an actual dogfight sim — by bounding the achievable
load factor with the real **V-n diagram**: a per-airframe **structural g limit** AND an
**aerodynamic ceiling** from `C_Lmax`. The **corner speed**, the **accelerated stall** (turn collapse
at low speed), and the **1 g stall speed** all fall out of these two limits.

It is **core kernel work** → a **seal**, full ritual (this ADR, mirror-first Python↔C++, Auditor
pass, cross-toolchain green). Unlike B1/B2 it is a **conservative extension**: the limiter only acts
at the edges of the envelope, so it leaves ordinary flight bit-for-bit unchanged (see §2.6).

**Rails touched:** no immutable rail *value* changes (R, Δt, g₀, ATM_TOP, SOFT, geometry, realm,
determinism bans, wire format). The `flight_model` descriptor string is updated and the header
`version`/`seal` bump (160→170, v1.6r0→v1.7r0). The seal is required because B3 is sealed
flight-model physics with new sealed envelope parameters and a **new sealed golden**
(GOLDEN-SK-Stall-001), even though it moves **no existing** `world_hash` (§2.6).

## 2) Decision

### 2.1 No new state, no wire change — `n` stays derived
B3 adds **no** state variable. Load factor `n` is computed per tick and never stored, so the
canonical 7-tuple `(lat,lon,psi,phi,alt,tas,gamma)`, the snapshot byte layout, and the KIN-002 wire
are **all unchanged**. This is the cheapest possible reseal: no protocol bump, no `interp`/`snapshot`
vector change beyond the envelope-scalar additions below.

### 2.2 New per-airframe envelope scalars (the V-n limits)
Three scalars are appended to every envelope (and to `tools/envelopes.py AERO_FIELDS`, the single
source of truth feeding the C++ `Envelope` struct and all three vector generators):

- **`cl_max`** — maximum usable lift coefficient (the aerodynamic ceiling).
- **`n_max_struct`** — positive structural g limit.
- **`n_min_struct`** — negative structural g limit (≤ 0).

`cl_max` is set **coherently** with each envelope's existing `stall_tas_mps`: the C_Lmax-implied
1 g stall speed `√(2·m·g₀ / (ρ₀·S·cl_max))` matches the declared stall speed (a tight invariant the
`tuning_probe` now enforces, |Δ| ≤ 0.5 m/s). The values are a **balance pass** (the spread reflects
the un-calibrated v1.2r0 stall/mass/area data); a holistic aero retune is **B4** (data-only).

### 2.3 The limiter (the physics) — replaces the global clamp
Per tick, after the bank slew and using the dynamic pressure `qS = ½ρ₀V²·S` (which depends only on
`V`, so it is hoisted above the load-factor step and **reused** by the drag solve):

```
n_aero = cl_max * qS / (m * g0)                 # most |n| the wing can lift at this q
n_hi   = min(n_max_struct,  n_aero)             # structural OR aerodynamic, whichever is lower
n_lo   = max(n_min_struct, -n_aero)
n      = clamp(n_cmd, n_lo, n_hi)
```

Everything downstream (lift, induced drag, `V̇`, `γ̇`, `ψ̇`) is exactly the B2 model, now fed the
**limited** `n`. Two regimes emerge from one rule:

- **Above the corner speed** `V* = √(2·n_max_struct·m·g₀/(ρ₀·S·cl_max)) = stall·√n_max_struct`:
  `n_aero > n_max_struct`, so the **structural** limit binds — you can pull full g.
- **Below corner:** `n_aero < n_max_struct`, so the **aerodynamic** ceiling binds — the achievable
  `n` falls with `V²`. In a sustained turn this is the **accelerated stall**: induced drag bleeds
  speed → `n_aero` drops → the turn can't be held → more speed bleeds. The **1 g stall speed**
  (where `n_aero = 1`) is the same rule at `n_cmd = 1`: below it the wing can't even hold level
  flight, so the nose drops and the aircraft mushes/descends — all emergent, no special-casing.

**Post-stall model:** lift is **held at C_Lmax** (an AoA-limited cap), not dropped — a stable,
deterministic ceiling. A discontinuous lift break / departure is **optional and deferred**.

### 2.4 Per-tick op order in `step(cmd,env)` — FROZEN (mirror Python ↔ C++ op-for-op)
1. `V = tas`. 2. Bank slew → φ (verbatim B1/B2). 3. **`q=½ρ₀V²`, `qS=q·S`** (hoisted). 4. **Limiter:**
`n_aero=cl_max·qS/(m·g₀)`; `n_hi=n_max_struct`, `if n_aero<n_hi: n_hi=n_aero`; `n_lo=n_min_struct`,
`neg=−n_aero`, `if neg>n_lo: n_lo=neg`; `n=clamp(n_cmd,n_lo,n_hi)`. 5. Trig (`cphi,sphi` of new φ;
`cg,sg` of OLD γ). 6. Drag/thrust **reusing `q,qS`**: `L=n·m·g₀`, `CL=L/qS`, `D=qS·cd0+k·CL²·qS`,
`T=thr·T₀·(1−V/Vmax)` (≥0). 7–10. Speed, γ, ψ, position, altitude — **identical to B2**.

No FMA anywhere (intermediates make order explicit); every op is correctly-rounded IEEE `+−×÷` or
`det_sin/det_cos`.

### 2.5 det_math budget — ZERO new primitives
`n_aero` and the clamps are `+ − × ÷` and IEEE comparisons only. Reuses the sealed `det_sin/det_cos`.
No `det_sqrt`/`exp`/`log`/`pow` in the kernel (corner/stall speeds are computed only in the *probe*
and *tests*, where libm is allowed). ⇒ the MPFR oracle and `gen_detmath_vectors` are untouched; the
AArch64/FMA divergence surface is unchanged.

### 2.6 Conservative extension — the six prior goldens are BYTE-IDENTICAL
This deviates from the §8.5 roadmap note ("hashes WILL change"), and the deviation is the honest,
auditable outcome: B3 is a **limiter**, and no existing scenario flies near its limits. Concretely
`clamp(n_cmd, n_lo, n_hi)` returns `n_cmd` **bit-for-bit** whenever `n_lo ≤ n_cmd ≤ n_hi`, which
holds for every prior golden (all command `g ∈ [0.3, 1.8]`, with `n_max_struct ≥ 7` and `n_aero ≫ 2`
at their 150–250 m/s speeds). The hoisted `q/qS` produce identical bits and are reused, and the extra
`n_aero` ops never feed the returned `n`. Verified: **GOLDEN-SK-Sphere/Turn/Climb/TurnClimb/Accel/
Pitch all reproduce their v1.6r0 hashes** under GCC + Clang. The lockstep/predict **digests** are
likewise unchanged (their scenarios don't bind); only those generated headers' envelope-scalar
literals grow. This "limits never bind ⇒ B2 verbatim" property is a *stronger* correctness statement
than churning the goldens would be.

### 2.7 NEW golden — GOLDEN-SK-Stall-001 (exercises BOTH limit branches and BOTH binds)
A two-ship scenario (3,000 ticks) that pins the new math while staying well inside ±90° γ (max ≈ 50°
/ 67°, so the `cosγ→0` singularity is never approached):
- **AC0 (Ki-61):** a hard banked turn at `g=2.0`, low throttle — induced drag bleeds speed through
  the corner so `n_aero` falls below the commanded 2 g (n collapses 2.0 → ~0.56 = **accelerated
  stall**), mushes near `V_MIN`, then unloads to wings-level full throttle and **recovers**.
- **AC1 (P-47D):** wings-level g-limit zooms — a `g=10` command is capped at the **structural**
  `n_max_struct=8.5`, trading speed for altitude, with push-overs between pulls.

Together they exercise the `min(n_max_struct, n_aero)` branch **both ways** and both the aerodynamic
bind (AC0) and the structural bind (AC1). Sealed hash `1a57b6d1…a0526fc6` (C++ ≡ Python, GCC + Clang).

### 2.8 Probes & properties
- `tuning_probe` gains `validate_b3_limits`: bounds (`cl_max∈[0.8,2.0]`, `n_max_struct∈[4,12]`,
  `n_min_struct∈[−6,0)`), C_Lmax↔stall coherence (≤0.5 m/s), and corner speed `< v_ne`.
- New `tests/property/test_stall.py` (8 tests): recovers the limited `n` from a 1-tick wings-level
  step (`n = 1 + γ·Vnew/(g₀·dt)`, the Vnew cancels) and proves: the limiter matches the clamp
  formula; accelerated stall caps `n` at `n_aero` (over-commanding saturates); the structural limit
  binds above corner; the corner-speed crossover (full structural g just above, less just below);
  the 1 g stall speed matches `stall_tas_mps`; **limits don't bind in normal flight** (the §2.6
  property, per airframe); an over-corner turn is unsustainable and stalls; determinism.

## 3) Rationale
- **A dogfight sim needs the V-n diagram.** Corner speed, accelerated stall, and per-airframe g
  limits are what make airframes feel different and make energy fighting emergent — the whole point.
- **C_Lmax-ceiling (not commanded-AoA) is the right B3 model:** it bolts onto the B2 commanded-g
  axis with one clamp, needs no new state/wire, and stays determinism-cheap (no new det_math).
- **Conservative-by-construction:** a limiter that only acts at the edges is *correct* to leave
  ordinary flight untouched. Not churning the prior goldens is the honest result and a stronger
  audit signal than regenerating them would be.

## 4) Consequences
**Positive:** real stall / corner speed / structural-g per airframe; accelerated-stall energy spiral
emergent; the `--fly` viewer gains a real flight envelope (a hard pull at low speed now mushes
instead of teleporting g). **Negative / watch:** `cl_max` values are a balance pass (wide spread,
e.g. yak3 1.78 / a6m2 1.19 — flagged for the **B4** holistic retune); post-stall lift is capped, not
broken (no departure/spin yet); the `cosγ` vertical singularity is still only documented (B3+); the
`climb_max/min` envelope LUTs remain vestigial (retained for B4).

## 5) Alternatives Considered
- **Commanded-AoA + explicit stall break:** rejected for B3 — needs an α state and a discontinuous
  post-stall lift model; the C_Lmax cap on the existing g-axis is stabler and decouples seals.
- **Drop post-stall lift (departure):** deferred — a lift cliff invites determinism-sensitive
  branchy behavior; the held-at-C_Lmax cap already gives the turn-collapse spiral. Optional later.
- **Deliberately churn the prior goldens (to match the roadmap note):** rejected — forcing hash
  changes for their own sake is dishonest; the byte-identity *is* the correctness proof (§2.6).
- **Put the limits in rails (global):** rejected — they are per-airframe; they belong in the
  envelopes (sealed data referenced by goldens), like the B1 aero block.
- **Derive `cl_max` from `stall_tas` in the loader:** rejected — explicit `cl_max` is tunable for B4
  and the probe enforces coherence anyway; a derived value hides the parameter.

## 6) Acceptance & Probes
Must PASS under v1.7r0:
- `spec_monotone_check` (rails/roster unchanged; version 160→170), `det_math_oracle` (untouched),
  `tuning_probe` (+B3 limit checks), `atm_top_probe`.
- all `gen_*.py --check` in sync; `snapshot_ref`, `lockstep_ref`, `predict_ref` self-tests PASS.
- `pytest tests/property` — **72** (incl. +8 `test_stall.py`).
- **ALL SEVEN goldens** (Sphere + Turn/Climb/TurnClimb/Accel/Pitch unchanged + new Stall) reproduced
  bit-for-bit **C++ ≡ Python** under GCC + Clang (and MSVC + AArch64 in guardian CI).
- `ctest` 7/7 under GCC + Clang (lockstep/predict vectors regenerated for the new envelope scalars;
  digests unchanged).

## 7) Ledger Discipline
Receipt via `tools/make_receipt.py` (overall PASS), seal **v1.7r0** (SEAL_CARD + history row), this
ADR, NEXT_STEPS marking B3 done / B4 next, memory updated. Receipt fields: seal v1.7r0, golden
hashes (six unchanged + new Stall), commit_sha, toolchain matrix, all 12 gates, signoff Forge.

## 8) Implementation Notes
Files: `data/tuning/envelopes/*.json` (×8: `cl_max,n_max_struct,n_min_struct`), `tools/envelopes.py`
(`AERO_FIELDS`), `tools/ref_kernel.py` (hoist `q/qS`, limiter, retire `N_MIN/N_MAX`),
`src/kernel/flight_types.h` (Envelope scalars), `src/kernel/kernel.cpp` (mirror limiter, retire
constants), regenerated `envelope_tables.h`/`scenario_params.h`/`lockstep_vectors.h`/
`predict_vectors.h`, `config/scenarios/GOLDEN-SK-Stall-001.json` + sealed golden,
`tools/tuning_probe.py` (`validate_b3_limits`), `tests/property/test_stall.py`,
`config/rails/atm.json` (version/seal/flight_model), `.github/workflows/guardian.yml` (+Stall in all
three golden lists), SEAL_CARD, NEXT_STEPS. Mirror-first: Python reference + Python gates green
(defines the golden), then bit-match C++ and verify cross-toolchain before sealing.
