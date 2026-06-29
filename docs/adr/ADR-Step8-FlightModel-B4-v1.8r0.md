# ADR-Step8-FlightModel-B4-v1.8r0 — Real flight model, Phase B4: per-airframe aero retune (historical top speeds, preserved turn balance)

**Status:** Accepted
**Date:** 2026-06-29
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.8r0 (proposed; bumps from v1.7r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

B1–B3 built a real 3-DOF energy/aero kernel, but the **aero numbers** in the 8 envelopes were the
un-calibrated v1.2r0 balance pass carried forward. With the energy model now live, the emergent
performance was measurably wrong in one axis and right in another (measured by the new
`tools/perf_probe.py`, which solves the *exact* sealed kernel aero for each airframe):

- **Level top speeds ran ~30 % low and compressed** — 109–142 m/s (394–513 km/h) where real WWII
  fighters did 533–703 km/h. Energy fighters did not out-run turn-fighters by a meaningful margin.
- **The turn balance already read true** — A6M2 (best sustained), Spitfire (2nd), P-47 (worst) — the
  iconic ordering, because it is set by `cl_max`/mass/area/`n_max_struct`, which B3 made coherent.

B4 is the **holistic retune** flagged repeatedly by B1–B3 ("retune is data-only in B4"). The owner
chose **historical-accurate top speeds (~180–195 m/s)** and a **full 8-airframe retune**.

**Why this is a seal (not a free data ride).** §8.4 of the roadmap hoped B4 could "ride the seal"
like §3 did. That was written **pre-B1**, when the kernel was a constant-TAS kinematic placeholder
and envelope aero did not feed any golden. Post-B3 the kernel *integrates* the aero, and **5 of the
8 airframes are baked into scenario goldens** (Turn→ki61, Climb→bf109f4, TurnClimb→spitfire_mk5,
Accel/Pitch→p51, Stall→ki61+p47d). A holistic retune therefore **moves 6 goldens**, and CLAUDE.md §2
is explicit: a golden `world_hash` change → bump the seal. So B4 is a **reseal — "golden moved, no
protocol bump"** — exactly the shape B1 was. **Rails touched:** no immutable rail *value* changes
(R, Δt, g₀, ATM_TOP, SOFT, geometry, realm, determinism bans, wire format); only the header
`version`/`seal` bump (170→180, v1.7r0→v1.8r0).

## 2) Decision

### 2.1 The retune lever — move ONLY (thrust_static_n, v_max_mps); freeze everything turn-defining
To lift top speed **without disturbing the (good) turn balance**, every turn/stall-determining
parameter is held **fixed**: `mass_kg`, `wing_area_m2`, `cl_max`, `induced_k`, `n_max_struct`,
`n_min_struct`, `stall_tas_mps`, and all four LUTs (`phi_max`, `roll_rate`, `climb_max`, `climb_min`).
Consequences that fall out for free:
- **Instantaneous-turn ordering is preserved bit-exactly** — it depends only on `cl_max`/`n_max`/
  `stall`, which did not move (perf_probe `instT` column is identical pre/post).
- **The B3 `cl_max`↔`stall` coherence invariant is untouched** — `stall_tas_mps`, `mass`, `S`,
  `cl_max` are all fixed, so `tuning_probe`'s ≤0.5 m/s C_Lmax-stall check needs no rework.

Only **`thrust_static_n`** and **`v_max_mps`** move (these are the *speed*-defining knobs), solved
per airframe (see §2.2). `v_ne_mps` is also raised for dive headroom, but it is **validation-only** —
`tools/envelopes.py` never loads it; the kernel never reads it — so it moves **no** golden.

### 2.2 The solver (`tools/b4_retune.py`) — fit (top-speed, best-climb) per airframe
For each airframe, with all of §2.1 fixed, solve the two speed knobs to a (top-speed, best-climb)
target pair against the exact kernel model:

```
Eq1 (top speed):  T0*(1 - V_top/Vmax) = D(V_top, n=1)         # full-throttle level equilibrium
Eq2 (best climb): max_V  V*(T(V)-D(V,1))/(m*g0)  = climb_tgt  # specific excess power
```
For a fixed `Vmax`, Eq1 fixes `T0 = D(V_top)/(1 - V_top/Vmax)`; raising `Vmax` flattens the thrust
curve, which (with `T0` re-anchored) lowers low-speed thrust and hence best climb **monotonically**,
so the solver bisects `Vmax`. Targets are historical: top speeds 148–195 m/s (A6M2 slowest → P-51
fastest), climb 14–22 m/s.

### 2.3 The flat-thrust insight (why `v_max` is a large, clean asymptote ~700)
The linear prop curve `T = T0·(1 − V/Vmax)` **couples** top speed and climb: naïve high thrust to
reach 195 m/s balloons best climb to ~45 m/s (its excess at low speed is enormous). The resolution is
a **near-flat** curve — a high `Vmax` with `T0 ≈ drag-at-top-speed` — so thrust is roughly constant
across the speed range and best climb settles to a historical ~18–23 m/s. `v_max_mps` is therefore
the **linear-thrust zero-crossing, NOT a physical Vne** (the real top speed is emergent and well
below it); it is capped at a clean **700 m/s** to avoid the absurd 4-digit asymptotes an uncapped
solve produces while keeping the curve flat enough. Documented in the solver and the data.

### 2.4 No new det_math, no state, no wire change
B4 changes **only envelope scalar literals**. The 7-tuple state, snapshot byte layout, KIN-002 wire
(protocol 3), and the det_math surface are all **untouched**. The only regenerated artifacts are the
envelope-fed headers: `envelope_tables.h` (feeds the kernel → the moved goldens) and the
`lockstep_vectors.h`/`predict_vectors.h` parity vectors (their scenarios use ki61/bf109f4/spitfire/
ki61 respectively, so their embedded aero literals — and digests — move; the C++↔Python mirror stays
exact). `scenario_params.h` is unchanged (it carries only inputs, no aero).

### 2.5 Goldens — 6 scenario hashes move; Sphere is byte-identical
The full retune touches every scenario airframe, so **Turn/Climb/TurnClimb/Accel/Pitch/Stall all move**
(regenerated from the Python reference, then bit-matched in C++). **GOLDEN-SK-Sphere-001 is unchanged**
(`db777327…d13ac394`) — it is the no-arg pure-kinematic anchor with no envelope. New hashes:
`Turn a1dc8116…`, `Climb de78ba92…`, `TurnClimb a502600c…`, `Accel 476cfb3f…`, `Pitch 3efaff43…`,
`Stall 8b2f4a85…` (all C++ ≡ Python under GCC + Clang).

### 2.6 One property recalibrated (honest, not loosened)
`test_energy.py::test_throttle_reaches_equilibrium` asserts TAS settles to a fixed point. The flatter
B4 thrust curve has a **weaker speed-restoring gradient** near equilibrium, so the fastest airframes
(P-47/P-51) need ~120 s to settle under the test's 1 mm/s "settled" bound vs ~80 s pre-B4. The fix
**extends the horizon 80 s → 140 s** (≈2× margin on the worst airframe) and **keeps the sharp 1 mm/s
bound** — the property (a converging fixed point) is unchanged; only the settling horizon reflects the
corrected dynamics. The `late ≤ early` convergence assertion was never in doubt.

### 2.7 New tooling (permanent B4 gates / provenance)
- **`tools/perf_probe.py`** — solves the exact kernel aero for each airframe and prints the emergent
  performance table (stall, corner, level top speed, best climb, sustained/instantaneous turn). This
  is what turned B4 from a subjective "feel" pass into an **objective** one (tune to measurable
  targets). `--check` reserved for future relative-ordering assertions.
- **`tools/b4_retune.py`** — the documented, reproducible solver (§2.2); `--write` does **surgical**
  in-place edits of the three numeric fields (preserving each JSON's hand-formatting) plus the stale
  header bump (the envelope headers still read v1.2r0 — never bumped through B1–B3 — now v1.8r0).

## 3) Rationale
- **Top speed was the one measurably-wrong axis; turn was already right.** Moving only the two
  speed knobs fixes the former while provably preserving the latter — the smallest, most auditable
  change that meets the goal.
- **Objective, not aesthetic.** `perf_probe` gives a ground-truth performance table from the sealed
  math, so the retune targets numbers (km/h, m/s climb, °/s turn) rather than vibes, fitting the
  project's high-assurance ethos.
- **Honest reseal.** The retune genuinely moves 6 goldens; per §2 that is a seal. We do not pretend
  it rides v1.7r0.

## 4) Consequences
**Positive:** historical top-speed spread (P-51 702 → A6M2 533 km/h) so energy fighters out-run
turn-fighters; turn ordering intact (A6M2/Spitfire best, P-47 worst); a reusable performance analyzer
for future tuning. **Negative / watch:** `v_max_mps` now reads as a large (~700) curve asymptote, not
a physical speed — documented, but a future B-phase could replace the linear prop curve with a
power-limited `T=P/V` model that makes `v_max` physical again and decouples climb from top speed
honestly. Best-climb is compressed (18.7–22.8 m/s) — acceptable (climb is the least-iconic axis and
all values are historical-plausible). The `climb_max/min` LUTs remain vestigial (kernel uses the
energy model; retained as data).

## 5) Alternatives Considered
- **Ride the seal (retune only the 3 scenario-unused airframes a6m2/yak3/la7):** rejected by the
  owner — it would balance only ⅜ of the roster and leave the golden airframes wrong.
- **Moderate / current speed bands:** offered; the owner chose historical-accurate.
- **Move cd0 / wing area / mass to hit top speed:** rejected — those move the turn balance and the
  B3 stall coherence; isolating the two speed knobs keeps the change provably turn-neutral.
- **Keep `v_max` physical (~1.2× top speed) and accept high climb:** rejected — a steep curve drives
  best climb to 45+ m/s; the flat-curve high-`Vmax` choice is the only way to keep both historical
  on the linear model.
- **Loosen the equilibrium test bound:** rejected in favor of extending the horizon (proves true
  settling, doesn't weaken the assertion).

## 6) Acceptance & Probes
Must PASS under v1.8r0:
- `spec_monotone_check` (rails/roster unchanged; version 170→180), `det_math_oracle` (untouched),
  `tuning_probe` (coherence still holds — turn/stall params fixed), `atm_top_probe`.
- all `gen_*.py --check` in sync; `pytest tests/property` — **72** (test_energy horizon recalibrated).
- **ALL SEVEN goldens** (Sphere unchanged + Turn/Climb/TurnClimb/Accel/Pitch/Stall regenerated)
  reproduced bit-for-bit **C++ ≡ Python** under GCC + Clang (and MSVC + AArch64 in guardian CI).
- `ctest` 7/7 under GCC + Clang (lockstep/predict vectors regenerated for the new aero literals).

## 7) Ledger Discipline
Receipt via `tools/make_receipt.py` (overall PASS), seal **v1.8r0** (SEAL_CARD + history row), this
ADR, NEXT_STEPS marking B4 done, memory updated. guardian.yml needs **no edit** (it reads hashes from
`expected.world_hash` and already lists all 7 golden IDs).

## 8) Implementation Notes
Files: `data/tuning/envelopes/*.json` (×8: `thrust_static_n`, `v_max_mps`, `v_ne_mps`, header bump),
`tools/perf_probe.py` (new), `tools/b4_retune.py` (new), regenerated `src/kernel/envelope_tables.h` +
`src/net/{lockstep,predict}_vectors.h`, resealed `tests/golden/{Turn,Climb,TurnClimb,Accel,Pitch,
Stall}-001/` (Sphere untouched), `tests/property/test_energy.py` (settle horizon), `config/rails/
atm.json` (version/seal). Mirror-first: Python reference + Python gates green (defines the goldens),
then bit-match C++ and verify cross-toolchain before sealing.
