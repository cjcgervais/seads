# ADR — Step 7 guns: Per-airframe region toughness (seal ATM-Sphere v1.20r0)

**Status:** accepted · **Date:** 2026-07-02 · **Seal:** ATM-Sphere v1.19r0 → **v1.20r0**

## Context

Region damage (v1.18r0) sized the ENGINE/WING/TAIL sub-pools with **global** kernel hex-constants
(0.375 / 0.5 / 0.25 × hp_start) and its ADR named the follow-up explicitly: "per-airframe region
toughness is a future data-only tune." Every handoff since has carried it as a free pick
("data-only envelope scalars + a kernel consumer — its own ADR, would move goldens"). The global
fractions flatten real WWII engineering differences the roster already models everywhere else
(hp_start, ammo, convergence, aero): a P-47D's air-cooled R-2800 famously brought pilots home with
cylinders shot away, while one rifle-caliber round through a Merlin/DB 601 cooling jacket killed
the engine in minutes.

## Decision

### Three new envelope scalars

Each tuning envelope (`data/tuning/envelopes/*.json`) gains **`engine_frac` / `wing_frac` /
`tail_frac`** — the per-airframe fractions of `hp_start` sizing the region sub-pools. They join
`AERO_FIELDS` (tools/envelopes.py, the single source of truth for scalar order) as the 16th–18th
scalars and flow through the generated `envelope_tables.h` like every field before them.

**Exactness contract (tuning_probe-enforced):** each fraction is a **positive multiple of 1/8 and
≤ 1**, and `hp_start` stays an integer. Eighth-granularity keeps every pool value exact in f64
AND **milli-exact on the WEAPON-001 wire** (1000/8 = 125 clears the denominator for any integer
hp_start; integer per-round damage preserves the granularity as pools drain — the v1.19r0
"quarter-integer ⇒ milli exact" wire argument generalizes to eighths unchanged). Property test
`test_dyadic_drain_stays_exact` proves the invariant through arbitrary drain/clamp sequences.

### The sealed values

| airframe | engine | wing | tail | rationale |
|---|---|---|---|---|
| P-47D | **0.625** | 0.5 | **0.375** | legendary R-2800 radial + rugged structure |
| Bf 109 F-4 | **0.25** | **0.375** | 0.25 | liquid-cooled DB 601; light narrow wing |
| Ki-61 | **0.25** | 0.5 | 0.25 | troublesome liquid-cooled Ha-40 |
| A6M2 | 0.375 | **0.375** | 0.25 | reliable Sakae radial but zero protection; unprotected wing tanks |
| Yak-3 | **0.25** | **0.375** | 0.25 | liquid-cooled VK-105; light mixed-construction wing |
| La-7 | **0.5** | **0.375** | 0.25 | tough ASh-82 radial; wooden airframe |
| Spitfire Mk V | **0.25** | 0.5 | 0.25 | liquid-cooled Merlin; large elliptical wing |
| P-51 | **0.25** | 0.5 | **0.375** | famously vulnerable belly radiator; sturdy fuselage |

(bold = departs from the v1.18r0 global.) The **A6M2 keeps `engine_frac = 0.375` deliberately**:
it is the sealed victim in every firing golden (Hit/EngineOut/Gunfire) and SESSION-SK-001, so its
engine-drain arithmetic — GOLDEN-SK-EngineOut-001's 26.25 → 14.25 → 2.25 → 0 sequence and the
thrustless-deceleration onset tick — is preserved byte-for-byte.

### The kernel consumer

`Kernel::add` (↔ `ref_kernel.Aircraft.__init__`) gains three defaulted params
`engine_frac = 0.375, wing_frac = 0.5, tail_frac = 0.25` used at pool-sizing time
(`engine_hp = engine_frac * hp`, one multiply per pool — same op as v1.18r0, different operand).
**The defaults ARE the sealed v1.18r0 globals**, so every envelope-less caller (the no-arg Sphere
golden, lockstep/predict vector paths, default-constructed test aircraft) builds a bit-identical
aircraft. Envelope-seeded callers pass the envelope's values: `scenario_main.cpp`,
`session.cpp`, `event.cpp`, `record_main.cpp::seed` ↔ `build_scenario`,
`session_ref._build_server_kernel` (event_ref rides it). Everything downstream of `add` —
aspect cones, drain/clamp order, degradation effects, HitEvent, the wire — is **untouched**.
**Zero new det_math** (the guns arc's streak holds through its eighth kernel seal).

## Golden / digest consequences

A **value retune of hashed state** (like B4's aero retune, unlike the additive v1.16r0/v1.18r0
field appends — there is no strip-proof for a value change; the proof is field-wise):

* **11 envelope-seeded goldens move; GOLDEN-SK-Sphere-001 is byte-identical** (defaults).
* **Field-wise value-only proof:** parsing each golden's old vs new `golden_snapshot.bin`, every
  differing f64 is exactly a region-pool re-size; **all kinematics, hp, fire_cd, ammo,
  last_hit_by, kills, and every projectile byte are identical** across all 11 — no sealed
  trajectory, kill, depletion, or engine-out outcome changed. (No victim's zero-crossing moved:
  the only drained pools in sealed scenarios belong to the A6M2, whose drained-region fractions
  are unchanged.)
* All 12 goldens reproduce **C++ ≡ Python bit-for-bit on GCC and Clang** locally; the CI matrix
  gates MSVC + AArch64.
* **Generated headers:** `envelope_tables.h` (+3 scalars × 8), `lockstep_vectors.h` /
  `predict_vectors.h` (embedded `Envelope` struct literals only — no digest or trajectory lines),
  `session_vectors.h` (SESSION digest moves: the client view carries the retuned P-47D/A6M2 pool
  values). **Event digest byte-identical** (`event_vectors.h` in sync — Event records carry
  integer hp deltas, which did not move). snapshot/weapon/framing/geo001/interp/scenario_params/
  golden_params all verified in sync.
* **Rails:** `weapons.region_damage` doctrine text updated (per-airframe fractions + the 1/8
  contract), header 290 → 300, seal → v1.20r0. `guardian.yml` unchanged (no new golden, no new
  ctest target).

## Alternatives rejected

* **Absolute per-region hp in the envelope** (e.g. `engine_hp: 45`): breaks the "pools derive
  from hp" invariant callers rely on when overriding hp post-construction, and invites values
  that are not milli-exact on the wire. Fractions-of-hp keep one toughness knob (`hp_start`)
  primary and the regions relative.
* **Finer granularity (1/16, arbitrary dyadics):** 1/16 × odd hp_start × 1000 is not an integer
  (62.5-milli steps) — it would break the v1.19r0 wire-exactness argument for odd-hp airframes.
  1/8 is the coarsest step that already expresses the whole tuning range needed.
* **Passing the `Envelope*` into `Kernel::add`:** couples the kernel's construction API to the
  envelope type for three doubles; the defaulted-scalar precedent (hp v1.11r0, ammo v1.13r0) is
  established and keeps envelope-less callers compiling untouched.
* **Retuning the A6M2 engine fraction too:** would shift EngineOut-001's engine-death tick and
  make the reseal a trajectory change instead of a provable value-only re-size. Deferred; any
  future A6M2 engine retune is its own data-only seal with a regenerated story.

## Gates

15/15 receipt gates PASS · property tests 169 → **175** (+6 `test_region_toughness.py`:
dyadic-eighth roster validation, envelope-driven pool sizing, sealed-baseline defaults,
per-airframe non-degeneracy + radial-vs-inline flavor guard, toughness-binds-through-the-kernel,
Hypothesis drain-exactness) · ctest 17/17 GCC + 17/17 Clang · tuning_probe extended
(`validate_region_toughness`) · spec_monotone PASS · all 13 generated headers in sync.
