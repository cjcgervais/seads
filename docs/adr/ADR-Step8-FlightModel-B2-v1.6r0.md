# ADR-Step8-FlightModel-B2-v1.6r0 — Real flight model, Phase B2: lift & pitch (flight-path angle γ from commanded-g)

**Status:** Accepted
**Date:** 2026-06-29
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.6r0 (proposed; bumps from v1.5r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

B1 (v1.5r0) made **speed** a real integrated state (thrust − drag), but **pitch is still fake**: the
vertical channel is a *commanded climb rate* clamped to an envelope LUT, teleporting altitude with no
attitude state. There is no angle of attack, no flight-path angle, no "altitude is earned by trading
energy". B2 fixes that: promote **flight-path angle `γ`** to a real state and drive the vertical channel
from a **commanded load factor `n` (g-command)** acting through the lift vector. This is the change that
makes nose attitude physical and couples pitch to energy (pulling g bleeds speed via induced drag).

It is **core kernel work** → a **seal**, full ritual (this ADR, mirror-first Python↔C++, Auditor pass,
all four goldens regenerated + a new pitch golden, cross-toolchain green). It is **also a wire reseal**:
`γ` becomes state, so it joins the KIN block (**KIN-001 → KIN-002**, snapshot **protocol 2 → 3**) so
remotes / late-join can reconstruct attitude.

**Rails touched:** no immutable rail *value* changes (R, Δt, g₀, ATM_TOP, SOFT, geometry, realm,
determinism bans). The `kinematics.turn_law` descriptive string is generalized (the law below reduces to
the old `ψ̇ = g₀·tanφ/V` for the level-coordinated-turn special case) and the `wire.kin` block gains
`gamma_scale` + bumps to KIN-002. The seal is required because the model **moves every golden
`world_hash`** (incl. Sphere — the state vector grew) and the **wire format changes**.

## 2) Decision

### 2.1 State vector — ADD `γ` (flight-path angle)
Per-aircraft state grows from the sealed 6-tuple to **7-tuple `(lat, lon, psi, phi, alt, tas, gamma)`**.
`psi` is the **track heading** (azimuth of the horizontal velocity); `gamma` is the **flight-path angle**
(velocity vector above horizontal). **Load factor `n` and AoA `α` stay derived** (recomputed per tick,
never stored) — fewer states ⇒ smaller wire ⇒ smaller reseal. The canonical hashing snapshot
(`Kernel::snapshot()`, raw LE-f64) appends `gamma` as the **7th f64** (field order
`[lat,lon,psi,phi,alt,tas,gamma]`). This is why **GOLDEN-SK-Sphere-001 changes** (see §2.6).

### 2.2 Inputs — `Command.target_climb` → `target_g`
`struct Command { double target_phi; double target_g; double throttle = 0.0; };`
`target_g` is the **commanded load factor `n`** (dimensionless, lift/weight). Replaces the B1
commanded climb rate. Wings-level **`n = 1` holds level flight**; a banked **level** turn needs
`n = 1/cosφ`; **`n > 1/cosφ` pitches the nose up** (climb, earned by energy); `n < cosγ` pushes over
(descend, gains speed in a dive). Commanded-g (not commanded-AoA) is the recommended B2 pitch axis: it
is stable, trivially clampable, and defers the stall model to **B3** (commanded-AoA needs `C_Lmax` now).

### 2.3 The 3-DOF point-mass model (the physics)
Standard banked point-mass equations of motion, with lift `L = n·m·g₀` perpendicular to velocity, banked
by `φ` about the velocity vector:

- `V̇   = (T − D)/m − g₀·sin γ`               (gravity now acts along the flight path)
- `γ̇   = (g₀/V)·(n·cos φ − cos γ)`            (excess normal force curves the velocity vector up/down)
- `ψ̇   = (g₀/V)·(n·sin φ / cos γ)`            (horizontal turn component; track heading)
- `alṫ = V·sin γ`                             (altitude is **earned**, ceiling soft-band predamp applies)
- horizontal ground speed = `V·cos γ`         (great-circle arc shortens as you climb)

`D` is exactly the B1 drag (`qS·cd0 + k·CL²·qS`, `CL = n·m·g₀/qS`), so **pulling g raises `n` → raises
`CL` → raises induced drag → bleeds speed** (the energy-vs-turn trade, now in the pitch plane too).

**Generalization check:** at `γ=0, n=1/cosφ` the model gives `γ̇=0` (stays level) and
`ψ̇=(g₀/V)(sinφ/cosφ)=g₀·tanφ/V` — *exactly* the sealed B1 coordinated-turn law. B2 strictly generalizes
B1; the old turn law is its level special case.

### 2.4 Per-tick op order in `step(cmd,env)` — FROZEN (mirror Python ↔ C++ op-for-op)
1. `V = tas`.
2. **Bank slew → φ** — *verbatim as B1* (phi_max(V), roll_rate(V), clamp cmdφ, clamp Δ, update, clamp).
3. `n = clamp(target_g, N_MIN, N_MAX)` — global placeholder structural clamp (per-airframe `C_Lmax` is B3).
4. Trig (single eval, fixed order): `cphi=det_cos(φ)`, `sphi=det_sin(φ)`, `cg=det_cos(γ)`, `sg=det_sin(γ)` — `cg/sg` are **OLD** γ.
5. **Drag/thrust with current `V`, load factor `n`** (B1 algebra): `q=½ρ₀V²`, `qS=q·S`, `L=n·m·g₀`,
   `CL=L/qS`, `D=qS·cd0 + k·CL²·qS`, `thr=clamp(throttle,0,1)`, `T=thr·T₀·(1−V/Vmax)`; if `T<0`,`T=0`.
6. **Speed:** `Vdot=(T−D)/m − g₀·sg`; `Vnew=V+Vdot·dt`; if `Vnew<V_MIN`,`Vnew=V_MIN`; `tas=Vnew`.
7. **Flight-path angle (uses Vnew, OLD γ):** `gdot=(g₀/Vnew)·(n·cphi − cg)`; `γ = γ + gdot·dt` (NEW γ).
8. **Track heading (uses Vnew, OLD γ):** `psidot=(g₀/Vnew)·(n·sphi / cg)`; `psi=wrap_2pi(psi+psidot·dt)`.
9. **Horizontal advance:** `cgN=det_cos(γ_new)`; `s=Vnew·cgN·dt`; `great_circle_step(lat,lon,psi,s,R)`.
10. **Altitude:** `sgN=det_sin(γ_new)`; `w=Vnew·sgN`; `w_eff=ceiling_climb_rate(w,alt,atm_top,soft)`;
    `alt=clamp(alt + w_eff·dt, 0, atm_top)`.

`Vnew` in the rate denominators mirrors B1's `advance_` (which used the updated `tas`). No FMA anywhere
(intermediate vars make op order explicit); every op is correctly-rounded IEEE `+ − × ÷` or `det_sin/det_cos`.

### 2.5 det_math budget — ZERO new primitives
Reuses `det_sin`, `det_cos`, `wrap_2pi` (all sealed). No `det_sqrt`/`exp`/`log`/`pow`. ⇒ the MPFR oracle
and `gen_detmath_vectors` are untouched; the AArch64/FMA divergence surface is unchanged.

### 2.6 The no-arg `step()` / advance_ — UNCHANGED behavior; Sphere hash MOVES (state grew)
The no-arg `Kernel::step()` (straight golden) keeps the pure kinematic tail and **never touches `γ`**, so
the Sphere aircraft flies the identical trajectory (γ ≡ 0). But the snapshot now appends a 7th f64, so
**GOLDEN-SK-Sphere-001's `world_hash` changes** for the first time since Pass 1 — the only reason is the
extra `gamma=0.0` field. Its first 48 bytes/aircraft are byte-identical to the v1.5r0 golden. This is the
honest consequence of growing the state vector (a state-vector change cannot leave the canonical snapshot,
hence the hash, untouched) and is exactly why B2 is sealed.

### 2.7 Wire / reseal scope — KIN-002, protocol 3
`gamma` joins the auxiliary KIN block: `wire.kin.format` **KIN-001 → KIN-002**, new `gamma_scale = 1e6`
(micro-degree, reusing the bearing/phi style), and `SNAPSHOT_PROTOCOL` **2 → 3** (KIN section now carries
`(id, phi_q, tas_q, gamma_q)`). GEO-001 stays geography-only and byte-unchanged. `throttle` and `target_g`
are **inputs, not state** → not on the wire (the state they drive — `tas`, `gamma` — is). Remote
interpolation (layer 4a, `interp`) is **not** changed in B2: it interpolates the GEO geography for smooth
remote *position*; attitude (γ) from KIN-002 is available for a later presentation pass. interp vectors
stay byte-unchanged.

### 2.8 Goldens & scenarios moved by this seal
- **ALL FOUR existing goldens regenerated** (Sphere via §2.6; Turn/Climb/TurnClimb/Accel via the new model).
- Scenario schema: `climb_mps` → **`g_cmd`** (commanded load factor). Re-authored so each exercises real
  pitch: Turn = level coordinated turn at `g≈1/cos45`; Climb = pull g>1 into the ceiling (predamp);
  TurnClimb = banked climbing pull; Accel = wings-level g=1 (pure longitudinal — behaviorally identical to
  B1 modulo the appended γ=0).
- **NEW: GOLDEN-SK-Pitch-001** — wings-level pull-up (g≈2) → zoom climb bleeding speed, then push-over
  (g≈0.5) → dive rebuilding speed. Pins the γ integrator + the pitch/energy coupling. Stays well inside
  ±90° (the `cos γ → 0` vertical singularity in `ψ̇` is documented, not exercised).
- **Net parity vectors regenerated** (same kernel path, now 7-tuple + 3-tuple commands):
  `lockstep_vectors.h`, `predict_vectors.h`, `snapshot_vectors.h`. `geo001/interp` vectors unchanged.

### 2.9 Documented limits (B2 scope boundary)
- **Vertical singularity:** `ψ̇` has `cos γ` in the denominator → undefined at γ=±90° (pure vertical).
  Scenarios/tests/viewer stay bounded; full loops/verticals are post-B3 work. `γ` is **not** wrapped or
  clamped (kept a clean physical state); it is deterministic regardless of magnitude (same reduction both
  impls), so this is a *modeling* limit, never a desync risk.
- **No stall / unbounded `C_L`:** `n` is clamped only by the global `[N_MIN,N_MAX]=[−3,9]` placeholder.
  Per-airframe `C_Lmax`, accelerated stall, structural-g and corner speed are **B3**.
- **Envelope `climb_max/min` LUTs become vestigial** for the kernel (vertical is now emergent). They are
  **retained** in the schema/struct/tables (untouched generators, cross-toolchain-reproduced) for B3/B4
  limit work; the kernel simply no longer consults them.

## 3) Rationale
- **Pitch must be a state for a dogfight sim.** Energy fighting is vertical: zoom climbs, dives, the
  pull-up that trades speed for nose position. A commanded climb-rate can't model that; `γ` + g-command can.
- **Commanded-g is the right B2 axis:** stable, clamp-friendly, and reduces exactly to B1's level turn —
  so the seal's golden churn is auditable (the level cases are recognizable). AoA/stall is cleanly B3.
- **Determinism-cheap:** no new transcendental ⇒ no new MPFR risk; bit-identity rests on the same
  fixed-op-order discipline already proven for `lut_eval`/B1.
- **Honest wire growth:** the reseal lands now because the *state* grew (γ), exactly the trigger B1's ADR
  predicted — not speculatively.

## 4) Consequences
**Positive:** real nose attitude; altitude earned by energy; pulling g bleeds speed (induced drag) in the
pitch plane; the `--fly` viewer's **pitch-cue exaggeration** and **yaw/throttle re-seed hacks** retire
(W/S feeds real `target_g`). **Negative / watch:** all goldens move (expected); Sphere's anchor hash moves
(documented, first time since Pass 1); `cos γ` singularity at vertical (documented, B3+); `n` only globally
clamped (B3). interp shows remote *position* smoothly but not yet remote *pitch* (KIN-002 carries it for later).

## 5) Alternatives Considered
- **Keep `γ` derived (no new state):** rejected — there is nowhere to put attitude; pitch can't persist
  across ticks, and the wire couldn't carry it for remotes. `γ` *must* be state for real pitch.
- **Commanded-AoA now:** rejected for B2 — needs a `C_Lmax`/stall model immediately (that's B3); commanded-g
  decouples the two seals.
- **Keep Sphere byte-identical (don't hash γ):** rejected — a state not in the canonical snapshot is not
  desync-detected (breaks the lockstep promise) and the world_hash wouldn't capture full state. γ must be hashed.
- **Wrap/clamp γ in the kernel:** rejected — adds magic constants + branches to the sealed op order for a
  case the scenarios/viewer already avoid; the singularity is documented instead. (Deterministic either way.)
- **Full-loop handling (vertical):** deferred — the `1/cosγ` singularity needs a different formulation
  (quaternion / velocity-frame); out of scope for "make pitch real".
- **Extend interp for γ now:** deferred — interp is downstream presentation (no seal); position smoothing
  is the layer-4a job, attitude smoothing can follow without a reseal (γ already on KIN-002).

## 6) Acceptance & Probes
Must PASS under v1.6r0:
- `spec_monotone_check` (rails/roster: ATM_TOP=8000, R=15000, dt=0.01, roster-8 — unchanged; version 150→160).
- `det_math_oracle` (untouched ⇒ PASS), `tuning_probe`, `atm_top_probe`.
- all `gen_*.py --check` in sync; `snapshot_ref`, `lockstep_ref`, `predict_ref` self-tests PASS.
- `pytest tests/property` incl. NEW B2 properties (below).
- **ALL FIVE goldens** (Sphere + Turn/Climb/TurnClimb/Accel) + **GOLDEN-SK-Pitch-001** reproduced
  bit-for-bit **C++ ≡ Python** under GCC + Clang (and MSVC + AArch64 in guardian CI) — the new sealed hashes.
- `ctest` 7/7 under GCC + Clang (snapshot/lockstep/predict vectors updated to protocol 3 / 7-tuple).

New B2 property tests (`tests/property/test_energy.py` + `test_kernel.py`):
- wings-level `n=1` holds γ≈0 / altitude (level-flight equilibrium); banked `n=1/cosφ` holds altitude (sustained level turn).
- pulling `g>1` (wings level) climbs (alt increases) AND ends slower than level at the same throttle (energy bleed in a pull).
- pushing `g<cosγ` descends and a dive rebuilds speed (gravity along flight path).
- higher commanded-g turns faster instantaneously (greater Δψ) but bleeds more energy (sustained vs instantaneous turn).
- γ determinism (same inputs → identical γ trace); snapshot KIN-002 round-trips γ within one quantum.

## 7) Ledger Discipline
Receipt via `tools/make_receipt.py` (overall PASS), seal **v1.6r0** (SEAL_CARD + history row), this ADR,
NEXT_STEPS marking B2 done / B3 next, memory updated. Receipt fields: seal v1.6r0, six `golden_sha256`
(all new), commit_sha, toolchain matrix, all 12 gates, signoff Forge.

## 8) Implementation Notes
Files: `flight_types.h` (Command.target_g; Envelope note), `src/kernel/kernel.{h,cpp}` (γ state, add()
γ, gamma() getter, snapshot 7×f64, B2 step(cmd,env)), `tools/ref_kernel.py` (N_MIN/N_MAX + γ + B2
step_scenario + run_scenario g_cmd), 4 scenario JSONs (g_cmd) + new Pitch scenario,
`gen_scenario_params.py` (Phase.target_g), `src/kernel/scenario_main.cpp` (target_g),
`config/rails/atm.json` (wire.kin → KIN-002 + gamma_scale, protocol note, version/seal, turn_law),
`src/net/snapshot.{h,cpp}` + `snapshot_ref.py` (KIN-002 γ, protocol 3) + `gen_snapshot_vectors.py`,
`src/net/predict.{h,cpp}` + `predict_ref.py` (OwnState γ, 3-tuple cmd) + `gen_predict_vectors.py`,
`src/net/lockstep_ref.py` + `lockstep_test_main.cpp` (3-tuple cmd) + `gen_lockstep_vectors.py`,
the three test mains (Command 3-arg / γ), regenerate all headers + all goldens, `guardian.yml` (+Pitch),
`tests/property/*` (B2 properties), `src/client/viewer_main.cpp` (retire pitch/yaw/throttle band-aids).
Mirror-first: Python reference + Python gates green (defines the goldens), then bit-match C++ and verify
cross-toolchain before sealing.
