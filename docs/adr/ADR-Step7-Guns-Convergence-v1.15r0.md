# ADR-Step7-Guns-Convergence-v1.15r0 — Gun convergence (boresight harmonization)

**Status:** Accepted
**Date:** 2026-06-30
**Author:** Forge (Claude) — Forge/Auditor/Guardian
**Seal:** ATM-Sphere v1.15r0 (proposed; bumps from v1.14r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

The guns arc (G1→G4) models a full deterministic dogfight: ballistic rounds, hit/damage, per-airframe
roster/fire-rate, finite ammunition. But a fired round has always inherited the firer's *exact*
post-step flight-path angle `gamma` (`spawn_projectile_`), i.e. the gun points precisely along the
velocity vector. Real WWII fighters **harmonized** their guns to a set range so the rounds are on the
sight line at that distance rather than always falling below the nose under gravity.

**The modeling constraint:** SEADS models a **single centerline battery** (no left/right wing-gun
lateral offset), so *lateral* convergence — two gun lines crossing at a point — is undefined here. The
physically honest realization of harmonization for a centerline gun is **vertical boresight zeroing**
(gravity-drop compensation): aim the gun slightly *up* so the round's ballistic drop brings it back to
the aim (sight) line at the harmonization range.

## 2) Decision

**Add a per-airframe scalar `convergence_m` (harmonization range) and offset a fired round's initial
`gamma` upward by the flat-fire drop-compensation angle at spawn.**

At spawn, with muzzle speed `v = firer_tas + muzzle_v_mps`:

```
delta = 0.5 * g0 * convergence_m / (v * v)     # flat-fire zeroing angle (small-angle drop comp)
round.gamma = firer.gamma + delta              # aim UP so the trajectory crosses the sight line
```

Derivation (flat-fire zero): time to range `d` is `t = d/v`; gravity drop is `½·g0·t² = ½·g0·d²/v²`;
the elevation that lifts the round by that drop at range `d` is `θ = drop/d = ½·g0·d/v²`. Setting
`d = convergence_m` gives the formula above. The round then follows the normal G1 ballistic step
(`gamma` bends back down under gravity), crossing the firer's sight-line altitude near `convergence_m`.

**Determinism:** the offset is pure `+ − × ÷` on existing doubles — **NO new det_math** (the project's
zero-new-transcendental streak across the whole guns arc is preserved). Op order is fixed identically
in the reference (`tools/ref_kernel.py::_spawn_projectile`) and the C++ mirror
(`src/kernel/kernel.cpp::spawn_projectile_`): `0.5 * g0 * convergence_m / (v * v)`, `v` computed as
`tas + muzzle_v` on both sides — so the world_hash is bit-identical cross-toolchain.

**Envelope plumbing (the G3/G4 pattern):** `convergence_m` is appended to `envelopes.AERO_FIELDS`, the
C++ `Envelope` struct (`flight_types.h`), and all 8 envelope JSONs; `gen_envelope_tables.py` emits it
generically. Per-airframe (initial balance pass, ~180–275 m): p47d 270, p51 275, spitfire 230, ki61
230, a6m2 250, la7 200, bf109 180, yak3 180.

**Why this is a seal.** This changes **canonical kernel spawn geometry** (a round's initial `gamma`),
so the world_hash of any firing scenario moves → a golden hash changes → seal (Change-Control Law §2).
Rails version 240→250.

## 3) Verification (gates)

- **Surgical golden movement.** Only the **3 firing goldens** move — **GOLDEN-SK-Gunfire-001**
  (`b25bb81c…`), **-Hit-001** (`8b8a84be…`), **-Winchester-001** (`14aa488b…`). The **7 non-firing
  goldens are byte-identical** (Sphere/Turn/Climb/TurnClimb/Accel/Pitch/Stall never fire, so the spawn
  path is never hit) and keep their prior v1.13r0 seal stamp — verified by a dry pre-reseal diff
  (7 MATCH / 3 DIFFER) and by C++ gcc==clang==committed on all 10.
- **Outcomes preserved.** The regenerated firing goldens stay semantically valid: **Hit-001 still
  kills** the A6M2 (final hp 0, corpse frozen); **Winchester-001 still empties** to ammo 0 (22 rounds
  live); Gunfire-001 unchanged cadence. The sub-metre elevation (delta ≈ 1e-3 rad) shifts trajectories
  without changing which rounds hit or when (well within the 60 m hit gate).
- **No wire change.** `gamma` already rides KIN-002 / the projectile block, so the snapshot protocol
  and the weapon-vector parity gate are untouched. The **session digest moves** (SESSION-SK-001's P-47
  fires → round positions shift; `session_vectors.h` regenerated) but the **event digest is unchanged**
  (`event_vectors.h` byte-identical — hit/kill events derive from hp deltas, and the kill sequence is
  unchanged).
- **Property tests** `tests/property/` (+2 ⇒ **119**): `test_projectile.py` pins the exact spawn formula
  (`gamma == firer.gamma + delta`, `delta > 0`), monotonicity (`test_convergence_scales_boresight_
  elevation`: elevation grows with convergence_m, shrinks with muzzle speed), and the physical meaning
  (`test_convergence_compensates_bullet_drop_at_range`: a level-fired zeroed round rides above a
  non-harmonized round and sits far closer to the sight-line altitude near the convergence range);
  `test_weapon.py` adds `convergence_m` to the roster presence/sanity/distinctness checks.
- **Full suite:** ctest **10/10** under GCC and Clang; all 6 generated headers in sync; 10 goldens
  C++≡committed cross-toolchain. **No new golden, no new ctest target** ⇒ `guardian.yml` unchanged.

## 4) Consequences / boundaries

- **Centerline model.** Lateral (horizontal) convergence is deliberately out of scope — SEADS has one
  gun line. If wing-gun offsets are ever modeled, horizontal convergence would compose with this
  vertical zeroing.
- **Flat-fire approximation.** `delta` uses the vacuum flat-fire zero (ignores drag's effect on
  time-of-flight and sphere curvature) — a documented approximation consistent with the lumped-drag
  ballistic model. It is exact enough that the round crosses the sight line near `convergence_m`
  (proven by the range property test), and being an approximation does not affect determinism.
- **Data-tunable.** `convergence_m` is envelope data (initial balance); retuning is data-only but still
  moves the firing goldens (a reseal), like the B4 aero retune.
- **Next (optional, none blocking):** cross-PROCESS sockets over the layer-5/6 frames; attacker
  attribution (kernel event hook, own ADR); component/region damage; **B5** ISA atmosphere; renderer
  meshes / guns in the live `--fly` path.
