# ADR — Step 8 Flight Model: supercharger critical altitude (per-airframe thrust lapse) — seal ATM-Sphere v1.22r0

**Status:** accepted (sealed v1.22r0, 2026-07-02)
**Depends on:** B5 ISA atmosphere (v1.21r0) — this is B5's named data-driven follow-up

## Context

B5 made engine power fall with raw density (`T *= sigma`) — the deliberately conservative choice
(its ADR: real WWII engines "sat between", supercharged to a critical altitude; "an envelope
scalar + one comparison; its own seal"). The consequence: top TAS was flat-to-falling with
altitude, and the roster's iconic altitude personalities (the turbo P-47 owning high altitude,
the Yak-3 owning the deck) did not exist. This seal is that follow-up.

## Decision

**1. The lapse model — one comparison + one divide, ZERO new det_math (tenth consecutive).**
Each envelope gains **`crit_alt_m`** (19th AERO field): the altitude up to which the
supercharger holds RATED power. In the envelope-driven step (both kernels, mirrored op-for-op):

```
sig_c = air_sigma(crit_alt_m)          // the sealed ISA LUT — an EXACT node, see (2)
lapse = sigma / sig_c;  if (lapse > 1) lapse = 1
T     = thr * thrust_static_n * (1 - V/v_max_mps) * lapse
```

Below the critical altitude `sigma >= sig_c` ⇒ lapse = 1 (rated power); above it power falls
with the density ratio (the B5 behavior, re-based at the critical altitude).
**`crit_alt_m = 0` reproduces the B5 thrust bit-for-bit** (`sigma(0) = 1.0` exactly, and
`sigma/1.0` is exact in IEEE-754) — guarded by
`test_supercharger.py::test_crit_zero_reproduces_b5_thrust_bit_for_bit`.

**2. The exactness contract (tuning_probe-enforced):** `crit_alt_m` MUST be a multiple of 500 m
inside [0, 8000] — i.e. a breakpoint of the sealed 17-node sigma LUT — so the divide's
denominator is a SEALED NODE CONSTANT (at a breakpoint `lut_eval` returns `ys[i]` exactly),
never an interpolant. Same spirit as the v1.20r0 dyadic-eighth region fractions.

**3. Sealed roster values** (WWII rated/full-throttle heights on the 500 m grid): P-47D **8000**
(turbosupercharged R-2800 — rated past the band top), P-51 **7500** (two-stage Packard Merlin),
Bf 109 F-4 / Spitfire Mk V **6000** (DB 601E ~6.2 km / Merlin 45 ~5.9 km), La-7 / A6M2 **4500**
(ASh-82FN second speed ~4.65 km / Sakae 12 ~4.55 km), Ki-61 **4000** (Ha-40), Yak-3 **3000**
(VK-105PF2 — the roster's low-altitude brawler).

**4. The speed re-anchor (`tools/supercharger_retune.py`) — why the two B4 knobs move.**
Holding rated power on the UN-re-anchored B4 numbers is unhistorical: B4 anchored the
full-throttle sea-level equilibrium to the HISTORICAL top speed — which real airframes achieved
AT ALTITUDE — and with the near-flat B4 thrust curve, rated power + sigma-falling drag tops out
at ~V·sigma_crit^(-1/2) at the critical altitude (a ~965 km/h P-51: rejected). So
`thrust_static_n`/`v_max_mps` are re-solved per airframe exactly as B4 did (every turn/stall
parameter frozen; same bisection), with ONE change: **Eq1 anchors the top speed AT the critical
altitude** (drag at sigma_crit), Eq2 keeps the B4 sea-level best-climb target (lapse = 1 there).
Emergent results:
- **Top TAS now RISES with altitude to exactly the B4 historical target at crit_alt_m, then
  falls** — the historically correct shape; no airframe exceeds its historical top speed
  anywhere in the band (the B5 claim, kept, now with the peak in the right place).
- **The emergent SEA-LEVEL top speeds land on history for free**: P-51 576 km/h (~583 hist),
  P-47D 549 (~550), La-7 585 (~580), Yak-3 597 (~567), A6M2 457 (~437), Spitfire 497,
  Bf 109 544, Ki-61 510 — 0–5% error for most (B4/B5 had sea-level = the at-altitude figure,
  ~15–25% hot).
- The solved `v_max_mps` drop from the 610–700 asymptotes to 250–360 — the thrust curve is
  steeper and `v_max` reads closer to a physical number (softens B4's documented wart).
- Sea-level best climb and the sustained/instantaneous turn ordering are preserved
  (A6M2 27.7°/s best, Spitfire second, P-47D 16.7 worst — perf_probe re-verified; the
  instantaneous column is untouched by construction).

**5. Deliberately untouched:** the no-arg kinematic path (**Sphere golden byte-identical** —
the anchor), the projectile advance (lumped global `PROJ_DRAG_K`), `q = ½ρ₀σV²` and the B3
aero ceiling (B5 exactly), gravity, still-air, and the wire (**no protocol change** — the lapse
derives from `alt` + static tuning data).

## Golden consequences (a genuine MODEL seal, B5-class)

Thrust changes at every altitude (T0/v_max moved even where lapse = 1) ⇒ **all 13 scenario
goldens move; Sphere is byte-identical.** Every sealed story RE-VERIFIED (instrumented
reference runs):
- **Hit-001**: tail-chase kill lands (6 connects, ticks 39–53; a6m2 hp 0 / tail 0 / lhb 0;
  p47d kills 1). **Winchester-001**: depletion at tick 891 exactly (fire-schedule-driven).
  **EngineOut-001**: same 4-round connect (ticks 29–35), same 26.25→14.25→2.25→0 drain, hp-22
  survivor decelerating to ~136.8 m/s. **Gunfire-001**: 9 rounds live at tick 300 (the sealed
  count — its prose "35 rounds" has been stale since G3 per its own NOTE), no hits, none near
  ground. **Turn/Climb/TurnClimb/Accel/Pitch**: structural claims hold (Climb still asymptotes
  7998 m < 8000). **Stall-001**: both binds still exercised — the Ki-61 accelerated stall
  collapses to n_aero ~0.40, and the P-47D g-10 zooms bind the STRUCTURAL 8.5 on 240 of 360
  pull ticks (n_aero up to 13.2).
- **Altitude-001 story INVERTS (schedule unchanged, description re-written):** the high
  Spitfire (6900 m, above its 6000 m critical) now out-accelerates the low one (+7.0 m/s at
  t=2000) — near-rated power at half the drag; historically correct and exactly the effect the
  model exists to produce. Both σ-node crossings and the clamped-vs-unclamped g-6 pull survive
  (n_aero low [9.41,10.00] > 6, high [5.44,5.83] < 6).
- **YakLa-001 re-measured, minimally re-tuned:** rated power below crit un-bled the La-7 tail
  (min TAS 87.9 — the 85 m/s crossing degenerated). The two tail phases stiffen g 0.6 → 1.6
  (induced drag ∝ n²); the weapon-pinning fire phase's throttle is untouched. All crossings
  restored (160 up t~1067 / down t~1918; 125 up t~1092 / down t~2447; 85 down t~2912), the g-8
  pull binds the structural limit throughout (n_aero [9.45, 9.72] > 8, measured), γ max +77°
  (inside ±90°), bursts unchanged (ammo 140→111 / 170→136, 63 live rounds). BONUS: the profile
  now straddles both lapse branches (Yak-3 just above its 3000 m crit, La-7 below its 4500 m).
- **NEW GOLDEN-SK-Supercharger-001** (the seal's golden): Yak-3 (crit 3000) + P-51 (crit 7500),
  IDENTICAL 5-phase schedules from 2700 m — the Yak-3 crosses ITS OWN critical altitude upward
  at t~2063 (**the sealed comparison flips in flight**, numerator interpolated between nodes),
  while the P-51 crosses the same 3000 m σ-node at t~2073 with NO flip (rated its whole
  flight) — distinguishing a σ-node segment switch from a critical-altitude branch flip in the
  same bytes. guardian.yml gains it in all three golden lists ⇒ **14 goldens**.

## Downstream fingerprint

- **Regenerated:** 8 envelope JSONs (`crit_alt_m` + re-anchored `thrust_static_n`/`v_max_mps`,
  headers → v1.22r0), `envelope_tables.h`, `lockstep_vectors.h`, `predict_vectors.h`,
  `scenario_params.h` (YakLa retune + Supercharger-001), `session_vectors.h` (digest
  `21aaab49…` → `ccc2f504…`; **FINAL_WEAPON facts byte-identical** — the astern kill lands on
  the same ticks), 13 golden dirs.
- **Byte-identical (verified `--check`):** `event_vectors.h` (per-round journal ticks/deltas
  unmoved), golden_params/snapshot/weapon/framing/geo001/interp/detmath/coeffs.
- **Property tests 184 → 190** (+6 `test_supercharger.py`: roster contract + flavor ordering,
  one-tick bit-exact branch replication through the kernel on both sides of crit, crit-0 ≡ B5
  bit-for-bit, acceleration improves below crit (P-47D/P-51), degrades above crit (the 6
  low-crit airframes), golden non-degeneracy guard; `test_isa.py`'s altitude-degrades test
  recalibrated to above-crit pairs).
- Rails 310→320: `atmosphere_density` gains the lapse model text +
  `supercharger_crit_alt_grid_m`/`supercharger_lapse` fields.

## Rejected alternatives

- **Hold rated power with NO re-anchor** — rejected: ~40–50% hot top speeds at altitude on the
  flat B4 thrust curve (the ~965 km/h P-51).
- **Gagg–Ferrar power lapse** `(σ−0.117)/0.883` — still rejected (B5 reasoning): another sealed
  constant with no fidelity claim at this granularity; the two-branch model IS the historical
  story (boost held to FTH, then lapse).
- **Interpolated crit altitudes** (arbitrary metres) — rejected for the 500 m grid: the divide's
  denominator stays a sealed node constant, one more place where a value CAN'T silently drift.
- **A per-airframe multi-speed blower schedule** (two critical altitudes, e.g. the ASh-82FN's
  two speeds) — deferred: doubles the data surface for a second-order kink; the single-FTH
  model already produces the altitude personalities.

## Gates

15/15 receipt PASS; Sphere `6914a994…2b13eb20` byte-identical; all 13 scenario goldens
C++ ≡ Python bit-for-bit on GCC AND Clang locally (incl. the new Supercharger-001); ctest
19/19 both toolchains; 190 property tests; all generated headers in sync; cross-toolchain
matrix (MSVC + AArch64) proven in guardian CI.
