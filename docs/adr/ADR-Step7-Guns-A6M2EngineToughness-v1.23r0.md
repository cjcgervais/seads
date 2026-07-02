# ADR — Step 7 Guns: A6M2 engine-toughness retune (data-only) — seal ATM-Sphere v1.23r0

**Status:** accepted (sealed v1.23r0, 2026-07-02)
**Depends on:** region toughness (v1.20r0) — this closes its named compromise

## Context

v1.20r0 made the region-pool fractions per-airframe and gave the radials tough engines (P-47D
0.625, La-7 0.5) — but **deliberately pinned the A6M2's Sakae radial at the old global 0.375**,
solely so GOLDEN-SK-EngineOut-001's drain sequence stayed byte-for-byte (its ADR named the
honest retune as a future data-only seal). The Sakae is a radial; 0.375 was a golden-preservation
compromise, not a modeling claim. This seal pays that debt.

## Decision

**`a6m2.json engine_frac: 0.375 → 0.5`** — La-7-class (a smaller, lighter radial than the
R-2800's 0.625), keeping the 1/8 contract (0.5 = 4/8; tuning_probe green). Wing (0.375 — the
famously unprotected tanks) and tail (0.25) untouched. One value in one JSON; ZERO code, ZERO
new det_math (eleventh consecutive).

## Golden consequences (a data VALUE seal — and a surprise)

The prediction was that EngineOut-001's story would move. Measured reality is cleaner:

- **EngineOut-001 is BYTE-IDENTICAL** (`e8ca968a…9dea8777`, re-derived and verified). The
  bigger pool still dies on the SAME 3rd connecting round (35→23→11→0(clamped) vs
  26.25→14.25→2.25→0 — four 12-damage rounds kill either pool at the same ticks 29/31/33/35),
  so the thrust-cut tick, the glider trajectory, and the FINAL snapshot (engine_hp = 0.0
  either way) are unchanged. Only the mid-run drain VALUES — which are never serialized —
  differ; the scenario description was re-written with them.
- **Exactly 3 goldens move — Gunfire/Hit/Winchester** (the goldens whose A6M2 ends with a
  LIVE engine pool): `897f543a…`, `dd24a23b…`, `f28a0fa7…`. **Value-only proof (field-wise
  snapshot diff): in each, exactly ONE f64 changed — the A6M2's `engine_hp`, 26.25 → 35.0**;
  kinematics/hp/fire_cd/ammo/last_hit_by/kills and every projectile byte identical.
- Sphere + the other 9 scenario goldens byte-identical (no A6M2 aboard, or — EngineOut — a
  drained pool).

## Downstream fingerprint

- **Regenerated:** `envelope_tables.h` (one hex literal), `session_vectors.h` (digest
  `ccc2f504…` → `f67368e9…` — the client view carries the A6M2's resized pool; the astern
  TAIL kill itself is untouched), 3 golden dirs.
- **Byte-identical (verified `--check`):** `event_vectors.h` (hit ticks + integer hp deltas
  unmoved), lockstep/predict (their scenarios carry no A6M2), all codec vectors.
- `test_region_toughness.py::test_toughness_is_actually_per_airframe`'s deliberate pin moved
  0.375 → 0.5 with the retune (the guard's documented procedure). Property tests stay **190**.
- Rails 320→330 (header seal only — no rail value changes; B4-style).
- guardian.yml unchanged (no new golden, no new ctest target).

## Rejected alternatives

- **0.625 (P-47D-class)** — rejected: the R-2800 stays the roster's toughest engine; it also
  WOULD move EngineOut-001's knockout to the 4th round (43.75 − 3×12 = 7.75 > 0), changing a
  sealed story for no historical gain.
- **Compensating hp_start/wing retune** — rejected: one honest value, one seal.

## Gates

15/15 receipt PASS; ctest 19/19 GCC + Clang; all 14 goldens C++ ≡ Python bit-for-bit on GCC AND
Clang locally (11 unchanged incl. EngineOut + Sphere, 3 moved); 190 property tests;
cross-toolchain matrix (MSVC + AArch64) proven in guardian CI.
