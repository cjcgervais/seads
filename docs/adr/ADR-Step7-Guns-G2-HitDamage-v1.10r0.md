# ADR-Step7-Guns-G2-HitDamage-v1.10r0 — Guns Phase G2: hit detection + per-aircraft hitpoints

**Status:** Accepted
**Date:** 2026-06-30
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.10r0 (proposed; bumps from v1.9r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

G1 (v1.9r0) put deterministic ballistic rounds in the kernel but they could not yet harm anything.
G2 closes the loop: rounds now **hit** aircraft and deal **damage**, so a gun kill is possible. The
owner chose a **per-aircraft hitpoints** damage model (vs binary-kill or component/region damage) —
simple, deterministic, one new state field, with component damage available to layer on later.

## 2) Decision

**Hitpoints.** Every aircraft gains a stored `hp` (f64), spawned at `START_HP=100`. `hp <= 0` **is**
the dead state (no separate flag). A **dead** aircraft: (a) is skipped by the flight integration in
`step(cmd,env)` — it **freezes** in place; (b) cannot fire (`Command.fire` is ignored); (c) is
excluded from hit tests, so rounds pass through the corpse.

**Hit test.** Folded into the projectile advance (`advance_projectiles_`): after a round moves to its
new position, it is tested against every **alive** aircraft in array order; on the **first** qualifying
strike it deals `DAMAGE_PER_ROUND=25` (HP clamped at 0) and **despawns**. A strike requires, against
aircraft j (j ≠ the firing `owner`):

- **Horizontal:** great-circle separation < `HIT_RADIUS=60 m`. Tested via the spherical law of
  cosines — `cosc = sinφ_p·sinφ_j + cosφ_p·cosφ_j·cos(λ_j − λ_p)` — compared to the precomputed
  constant `COS_HIT_ANGLE = det_cos(HIT_RADIUS/R)`. Because `acos` is monotone decreasing,
  `distance < HIT_RADIUS  ⟺  cosc > COS_HIT_ANGLE`, so **`det_acos` is never needed**.
- **Vertical:** `|alt_p − alt_j| < HIT_ALT_GATE=60 m`.

Together this is a cylinder test using **only `det_sin`/`det_cos` + `+ − * /`** — **no new det_math
primitive** (the same property that made G1 cheap). The hit is a **per-tick point test** of the
round's new position (swept/continuous collision is deferred; `HIT_RADIUS` ≫ the ~10 m/tick step, so
a well-aimed round is within the cylinder for many ticks and reliably connects).

**Op order (mirror-first, bit-exact Python↔C++).** Per tick: (1) flight step for each **alive**
aircraft; (2) `advance_projectiles_` — move each round, then `projectile_hit_` against the new
position, applying damage and dropping a round on hit (in addition to ttl/ground despawn); (3) spawn
newly-fired rounds from **alive** firers. Damage is applied while iterating projectiles in array
order — fully deterministic; a round that brings an aircraft to 0 HP kills it, and later rounds that
tick see it dead and pass through.

**Serialization.** `hp` is appended as the **8th per-aircraft f64** (after `gamma`). The projectile
block (G1) is unchanged — `owner` was already carried for exactly this attribution, so the projectile
format needed **no** reseal.

**Constants** (`START_HP`, `DAMAGE_PER_ROUND`, `HIT_ALT_GATE_M`, `COS_HIT_ANGLE`) are **global** for
G2 and live in `kernel.cpp`/`ref_kernel.py` as shared hex-floats (per-airframe HP/armament are G3).
`COS_HIT_ANGLE` was computed once via `det_cos(HIT_RADIUS/R)` and embedded.

## 3) Why this is a seal (golden hashes move)

Appending `hp` grows every aircraft record, so all 8 prior goldens' hashes move even though their
trajectories **and** `hp` (a constant 100, never damaged in those scenarios) are identical — the same
honest format growth B2 (`gamma`) and G1 (the projectile block) paid. Sphere moves again. The 8 prior
goldens are regenerated, plus the new **GOLDEN-SK-Hit-001** (a P-47D tail-chase **gun kill** of an
A6M2 — 4 hits to 0 HP at tick 42, the corpse freezes, surviving rounds pass through; 16 rounds still
airborne at tick 200).

## 4) Scope boundaries (explicit non-goals for G2)

- **Hitpoints only** — no component/region damage, no engine/control-surface effects (a later phase).
- **A dead aircraft freezes** — it does not fall ballistically or break up (a later refinement). It
  stays in the array (removing it would churn `owner` indices / remote references).
- **Per-tick point test** — not swept/continuous; fine for `HIT_RADIUS ≫ step`. No aircraft-aircraft
  collision (only round-vs-aircraft).
- **Global constants** — one generic gun lethality and one global HP; per-airframe armament/toughness
  is G3.
- **No wire change** — `hp` is not yet on the GEO-001/KIN wire (deferred with the rest of weapon wire
  transport); KIN-002 / snapshot protocol 3 unchanged.

## 5) Determinism & verification

- **No new det_math.** Hit test uses `det_sin`/`det_cos` + `+ − * /`, all proven vs the MPFR oracle;
  no `det_acos` (the monotone-cos trick), no new banned symbol, no FMA. `lint_determinism` clean.
- **Mirror-first:** `ref_kernel.py` defines the rules + bytes; `kernel.cpp` bit-matches the op order.
  All 9 goldens reproduce **C++ ≡ Python under GCC + Clang** locally (18/18); guardian CI extends to
  MSVC + GCC/Clang × x64/AArch64 (Hit added to every golden list).
- **Gates:** 12/12 receipt PASS at v1.10r0; ctest 7/7 GCC + Clang; **88 property tests**
  (+7 `tests/property/test_hit.py`: a round hits & damages, a burst kills & the corpse freezes,
  HP-drop is a whole multiple of DAMAGE, the altitude gate blocks vertically-separated rounds,
  no self-hit, a dead target stops absorbing rounds, determinism).
- lockstep/predict generated-vector digests moved with the canonical snapshot and were regenerated.

## 6) Consequences

- The kernel now resolves combat outcomes. G3 (per-airframe weapon roster) moves the G1/G2 globals
  (muzzle/drag/ttl/HP/damage) into the envelope — mostly data, but a reseal if it moves goldens.
- Wire transport of `hp` + rounds and a renderer that draws kills are the natural no-seal follow-ups.

## 7) Alternatives considered

- **Binary kill (one hit = down)** — rejected by the owner: no damage accumulation / lethality tuning.
- **Component/region damage** — deferred: much larger snapshot/seal surface; better after a simple
  hitpoints pass lands.
- **`det_acos` for the true distance** — unnecessary: `acos` is monotone, so the `cosc > COS_HIT_ANGLE`
  comparison gives the identical decision with no new det_math and no acos ill-conditioning near 0.
- **Removing dead aircraft from the array** — rejected: it would churn projectile `owner` indices and
  remote references; freezing in place is simpler and fully deterministic.
