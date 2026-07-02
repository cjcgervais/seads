# ADR — Step 7 Guns: projectile σ-drag (B5's last deferral) — seal ATM-Sphere v1.24r0

**Status:** accepted (sealed v1.24r0, 2026-07-02)
**Depends on:** B5 ISA atmosphere (v1.21r0) — this closes its named projectile deferral

## Context

B5 (v1.21r0) made the air thin with altitude for AIRFRAMES — σ(alt) scales dynamic pressure and
engine power — but deliberately left the projectile advance in sea-level air ("a bullet is a
bullet"; its ADR named σ-scaling of `PROJ_DRAG_K` as a deliberate deferral that "moves every
firing golden and is its own kernel seal"). Since v1.22r0 the roster's engines even hold rated
power to altitude, yet a round fired at 8,000 m still bled speed as if at sea level — the last
physical inconsistency in the atmosphere arc. This seal pays that debt.

## Decision

In BOTH `Kernel::advance_projectiles_` (kernel.cpp) and `_advance_projectiles` (ref_kernel.py),
op-for-op identical:

```
sigma = air_sigma(p.alt)                            # the round's PRE-step altitude
Vdot  = -PROJ_DRAG_K * sigma * V * V - g0 * sg      # was: -PROJ_DRAG_K * V * V - g0 * sg
```

- **One `air_sigma` per round per tick from the PRE-step altitude** — the exact convention the
  aircraft step has used since B5 (σ from pre-step alt, matching the V the solve uses).
- **Same sealed 17-node LUT, same `lut_eval` ⇒ ZERO new det_math** (twelfth consecutive
  zero-transcendental seal).
- `PROJ_DRAG_K` stays a lumped GLOBAL coefficient (per-airframe ballistics stay out of scope);
  σ(0) = 1.0 exactly (the first sealed LUT node), so a sea-level round is **bit-identical** to
  the pre-v1.24r0 round (guarded by `test_proj_sigma`).
- Deliberately untouched: spawn geometry (convergence zeroing keeps its sealed flat-fire
  formula — it is a boresight approximation, not a trajectory integral), hit detection, ttl,
  the no-arg path, gravity, the wire (**no protocol change** — σ derives from `alt`, already
  on GEO-001/the projectile block).

## Golden consequences (a kernel MODEL seal — measured, story-by-story)

**Exactly 4 goldens move; 10 are byte-identical** (Sphere + the 6 gun-less scenarios +
Altitude/Supercharger, which never fire — and EngineOut-001, see below). In every mover the
aircraft state is **field-wise byte-identical**; only projectile kinematics (lat/lon/alt/tas/
gamma) moved — thin-air rounds keep more speed:

- **Gunfire-001** `c0170fd3…`: 0 hits; both aircraft byte-identical; all 9 live rounds faster
  (e.g. round 0 tas 729.9 → 789.3 at tick 300).
- **Hit-001** `f811b0a3…`: STILL exactly six 12-hp TAIL connects walking hp 70→0, kill on the
  6th — but the three mid-burst connects land ONE TICK EARLIER (ticks 39/41/44/47/50/53; the
  v1.22r0–v1.23r0 story read 39/42/45/48/50/53). First/fifth/killing ticks unchanged; both
  aircraft byte-identical; the 1 surviving round moved. Description re-measured + re-written
  (it also shed stale pre-G3 numbers: "25 hp / 4 hits" dated from v1.10r0).
- **Winchester-001** `cebeac79…`: the tick-891 depletion is EXACT (fire-schedule-driven,
  σ-independent — verified); aircraft byte-identical; the 22 live rounds moved.
- **YakLa-001** `fadedd98…`: non-interacting by design — both aircraft trajectories, every
  LUT/structural crossing claim, and the burst/ammo counts byte-identical; ONLY the 63 sealed
  live rounds' kinematic bytes moved.
- **EngineOut-001 BYTE-IDENTICAL** (second seal running — a different reason each time): the
  faster head-on rounds pull the FIRST connect one tick earlier (29 → 28), but ticks 31/33/35,
  every drain value (35→23→11→0), the tick-33 engine cut, and the glider are unchanged — and
  since hit ticks live only in the never-hashed hit-event journal, the FINAL snapshot did not
  move. The shifted tick is real observable output (the layer-6 event channel sees it), so the
  description was re-written honestly.

## Downstream fingerprint

- **Regenerated:** 4 golden dirs; `session_vectors.h` (**digest `f67368e9…` → `966aca05…`**;
  checkpoint tick-1 and ALL FINAL_WEAPON facts byte-identical — the astern kill lands on the
  same ticks; checkpoints 50–200 carry live-round wire bytes and moved);
  **`event_vectors.h` — the event digest MOVES for the first time since v1.17r0**
  (`06629a69…` → `2a9ae8a3…`): SESSION-SK-001's mid-burst hits arrive 1 tick earlier
  (seq 1–3: ticks 43/46/49 → 42/45/48; first hit + kill ticks 40/54 unchanged), and
  EVENT-MULTIHIT-001's volleys shift 44/47/50 → 43/46/49 (kill volley structure intact —
  two attributed events per tick, overkill-clamped kill). This is BY CONSTRUCTION: the event
  channel reports per-round hit ticks, and hit ticks are exactly what σ-drag moves.
- **Byte-identical (verified `--check`):** scenario_params/golden_params/lockstep/predict
  (their scenarios don't fire) + all codec vectors (snapshot/weapon/framing/geo001/interp/
  detmath/envelope/coeffs).
- `trajectory.js` dogfight regenerated (same 3 kills / 18 events).
- Rails 330→340 (`atmosphere_density` + `weapons.model` text; no rail VALUE changed).
- guardian.yml UNCHANGED (no new golden, no new ctest target; 4 sealed hashes moved).
- +7 property tests (`test_proj_sigma.py`): sea-level bit-identity / one-tick op-order
  replication / pre-step-altitude convention / high-round-keeps-more-speed / first-tick drag
  ∝ σ / the re-measured Hit + EngineOut journals pinned ⇒ **197**.

## Rejected alternatives

- **Per-airframe or per-caliber drag coefficients** — rejected: a data surface nobody asked
  for; the seal is about consistency with B5, not ballistics fidelity.
- **σ-scaling the convergence zeroing angle too** — rejected: δ = ½·g₀·conv/v² is a sealed
  boresight approximation (drop over the zero range is drag-independent to first order);
  changing it would move spawn geometry for no observable gain.
- **Evaluating σ at the post-step altitude** — rejected: the aircraft step's sealed convention
  is pre-step; two conventions in one kernel invite drift.

## Gates

15/15 receipt PASS; ctest 19/19 GCC + Clang; all 14 goldens C++ ≡ Python bit-for-bit on GCC AND
Clang locally (10 unchanged incl. EngineOut + Sphere, 4 moved — validated hash-by-hash); 197
property tests; det_math oracle + tuning/spec/ceiling probes + determinism lint PASS;
cross-toolchain matrix (MSVC + AArch64) proven in guardian CI.
