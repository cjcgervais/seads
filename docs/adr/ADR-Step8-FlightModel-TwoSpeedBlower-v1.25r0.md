# ADR — Step 8 Flight Model: two-speed blower schedule (per-airframe FS/MS gear lapse) — seal ATM-Sphere v1.25r0

**Status:** accepted (sealed v1.25r0, 2026-07-02)
**Depends on:** supercharger critical altitude (v1.22r0) — this is v1.22r0's explicitly-deferred follow-up (its "Rejected alternatives" named the multi-speed blower as deferred)

## Context

v1.22r0 gave every engine ONE critical altitude and a single-branch thrust lapse
`min(1, σ(alt)/σ(crit_alt_m))` — rated power to the full-throttle height, then falling. But
several roster engines had a **two-speed supercharger**: the pilot (or an automatic barostat)
shifted the blower to a HIGH gear that traded some sea-level/low-altitude power (the gear draws
more shaft power to spin faster) for a SECOND, higher full-throttle height. The characteristic
power curve is therefore **flat–fall–flat–fall**: rated in low (MS) gear to the low crit, falling
as the MS gear runs out of boost, flat again at the reduced HIGH-gear power once shifted, then
falling above the high (FS) crit. v1.22r0's single crit could not express the low-gear plateau or
the shift kink. This seal adds it, data-driven, for the airframes that had it.

## Decision

**1. The lapse model — two `min`s and a `max`, ZERO new det_math (thirteenth consecutive).**
Each envelope gains two fields appended after `crit_alt_m`: **`crit_lo_alt_m`** (20th AERO field —
the LOW/MS gear's full-throttle height; **`0` = single-speed**, no two-speed engine) and
**`gear2_frac`** (21st — the HIGH/FS gear's rated-power fraction of the low-gear rating). The
kernel step (both kernels, mirrored op-for-op) keeps the v1.22r0 single-speed lapse and, ONLY when
`crit_lo_alt_m > 0`, replaces it with the gear the pilot would pick at that altitude:

```
sig_c = air_sigma(crit_alt_m)                 // the sealed ISA LUT — an EXACT node
lapse = sigma / sig_c;  if (lapse > 1) lapse = 1     // v1.22r0 single-speed (== FS gear, unfractioned)
if (crit_lo_alt_m > 0):                        // two-speed engine only
    sig_lo = air_sigma(crit_lo_alt_m)          // also an EXACT node
    lo_gear = sigma / sig_lo;  if (lo_gear > 1) lo_gear = 1
    hi_gear = gear2_frac * lapse
    lapse   = max(lo_gear, hi_gear)            // whichever gear makes more power here
```

i.e. `lapse = max(min(1, σ/σ(crit_lo)), gear2_frac·min(1, σ/σ(crit_alt)))`. The `max` selects the
better gear at every altitude, producing the flat-fall-flat-fall shape automatically: low gear
wins below the shift altitude (rated to crit_lo, then falling), high gear wins above it (flat at
`gear2_frac` up to crit_alt, then falling). One divide + one multiply + comparisons — **no new
det_math** (thirteenth consecutive zero-transcendental seal).

**The branch is entered ONLY on `crit_lo_alt_m > 0`.** Single-speed airframes (the field pair
`(0, 1)`) never execute the two-min/max block, so their thrust is **bit-for-bit v1.22r0** — proven
by `test_blower.py::test_single_speed_never_enters_the_branch`. This is deliberate: moving the
`max()` out of the branch (or reordering the two `min()`s) would change single-speed bytes and is
a MODEL change, not a refactor.

**2. The exactness contract (`tuning_probe.validate_supercharger`, extended):** `crit_lo_alt_m`
MUST be a multiple of 500 m on `(0, crit_alt_m)` — a breakpoint of the sealed 17-node σ LUT, so
the second divide's denominator is a SEALED NODE CONSTANT (never an interpolant), exactly like
`crit_alt_m` at v1.22r0. `gear2_frac` MUST be a dyadic sixteenth (`k/16`, exact in f64 AND
milli/unit-exact everywhere), with the non-degeneracy band **`σ(crit)/σ(crit_lo) < gear2_frac < 1`**
so the shift is real: gear2 high enough that the HIGH gear beats the LOW gear somewhere below
crit_alt (else the second speed is dead weight), and below 1 (else it is not a reduction). A
single-speed airframe MUST carry exactly `(crit_lo_alt_m, gear2_frac) = (0, 1)`.

**3. Sealed two-speed roster** (the three engines with a genuine two-speed blower; heights on the
500 m grid, fractions dyadic /16):
- **P-51** — crit_lo **3000**, gear2 **14/16** (0.875). Packard Merlin V-1650-7 two-stage /
  two-speed; MS gear low, FS gear to the 7500 m crit_alt.
- **La-7** — crit_lo **1500**, gear2 **14/16**. ASh-82FN two-speed radial; second speed to 4500 m.
- **Yak-3** — crit_lo **1000**, gear2 **15/16** (0.9375). VK-105PF2 two-speed; a shallow shift on
  the deck brawler (crit_alt 3000).

The other five airframes (P-47D turbo — continuously regulated, modeled single-speed to 8000;
Bf 109 F-4, Ki-61, A6M2, Spitfire Mk V) stay single-speed `(0, 1)` and their speed knobs are
**untouched**.

**4. The speed re-anchor (`tools/blower_retune.py`, new tool) — why the four goldens move.**
Same solver shape as `b4_retune.py`/`supercharger_retune.py` (every turn/stall parameter frozen;
bisection on the two knobs), with the lapse now the two-speed `max()`: **Eq1 anchors top speed AT
crit_alt with `lapse = gear2_frac` there** (the airframe is in HIGH gear at its high crit, making
`gear2_frac ×` rated power), **Eq2 keeps the B4 sea-level best-climb target at rated LOW gear**
(`lapse = 1` on the deck). Only the two-speed airframes' knobs move:

| airframe | T0 (N)         | v_max (m/s) | shift altitude |
|----------|----------------|-------------|----------------|
| P-51     | 16960 → 16320  | 275 → 300   | ~4280 m        |
| La-7     | 15420 → 14600  | 290 → 325   | ~2826 m        |
| Yak-3    | 11280 → 10900  | 335 → 365   | ~1655 m        |

Emergent, verified: the top-at-crit anchors hit their historical targets exactly (P-51 703 /
La-7 661 / Yak-3 655 km/h at crit_alt); the emergent sea-level tops rise slightly to
163.9 / 166.8 / 168.6 m/s (the low gear now runs at rated power on the deck where before FS-only
power was already lapsing — documented emergent, not a target). `blower_retune.py` leaves
`supercharger_retune.py` untouched as the v1.22r0 provenance artifact (the b4_retune.py pattern).
All 8 envelope JSON headers bump to v1.25r0 (`version` 250).

**5. Deliberately untouched:** the no-arg kinematic path (**Sphere golden byte-identical**), the
projectile advance, `q = ½ρ₀σV²` and the B3 aero ceiling, gravity, still-air, and the wire
(**no protocol change** — the lapse derives from `alt` + static tuning data; protocol stays 7).

## Golden consequences (a MODEL seal, but tightly bounded)

Only the three two-speed airframes' thrust changed, so **exactly 4 goldens move; 10 are
byte-identical including Sphere.** The additive-fields-inert proof is those 10 unchanged goldens
plus `test_single_speed_never_enters_the_branch`: appending `(0, 1)` to every single-speed
envelope perturbs nothing.

- **Accel `6f146a89…` / Pitch `8d92ad88…`** — both are P-51 scenarios; they move purely because
  the P-51's two speed knobs re-tuned (qualitative descriptions untouched).
- **Supercharger-001 `6fa607ac…`** — the Yak-3 + P-51 branch-flip golden, RE-MEASURED. The SAME
  3000 m σ-node is now the Yak-3's FS full-throttle height AND the P-51's MS full-throttle
  height: the Yak's FS-gear `min` flips at t~2067, the P-51's MS-gear `min` at t~2070, but the
  outer `max()` never flips on either (the Yak is in HIGH gear throughout — flat 0.9375 below
  3000 — and the P-51 in LOW gear, rated, its whole flight). Finals Yak 137.4 m/s @3197 m
  (lapse ~0.9189), P-51 135.6 @3194 (~0.9804).
- **YakLa-001 `a99a6201…`** — RE-MEASURED, every LUT/structural crossing PRESERVED: Yak-3 crosses
  160 m/s up t~1124 / down t~1873; La-7 crosses 125 up t~1214 / down t~2405 and 85 down t~2858
  (three segments); the 1 s g-8 pull binds the structural limit (n_aero [9.03, 9.32] > 8), γ max
  +80.3°, final mush 72.7 m/s; bursts/ammo (140→111 / 170→136) and the 63 sealed live rounds
  unchanged. **BONUS: the La-7 crosses its ~2825 m gear-shift altitude TWICE** (HIGH→LOW t~1696
  diving, LOW→HIGH t~2336 zooming) — the outer `max()` flips in BOTH directions in the sealed
  bytes, exercising the shift kink itself.
- **NEW GOLDEN-SK-Blower-001 `128ee2cd…`** (the seal's golden, 3500 ticks): a Yak-3 traverses ALL
  THREE below-crit regimes in one flight — **rated LOW gear below 1000 m**, the **falling MS
  branch** (with a 1500 m σ-node crossing INSIDE it at t~1944 — interpolation under the low-gear
  `min`), the **gear-shift `max()` flip at t~2227 @~1654 m** (LOW→HIGH), then **flat FS gear at
  0.9375 EXACTLY** (the dyadic constant, pinned bit-exact) to the end — beside a single-speed
  Bf 109 F-4 control on the IDENTICAL schedule that never flips (its lapse is the v1.22r0
  single-branch curve). Finals Yak 118.2 m/s @1974 m / Bf 109 109.3 @1933 m. guardian.yml gains
  it in all three golden lists ⇒ **15 goldens**.

## Downstream fingerprint

- **Regenerated:** 8 envelope JSONs (`crit_lo_alt_m`/`gear2_frac` + re-anchored knobs on the three
  two-speed airframes, headers → v1.25r0), `envelope_tables.h`, `lockstep_vectors.h`,
  `predict_vectors.h`, `scenario_params.h`, `session_vectors.h`. The lockstep/predict/session
  vectors moved **ENVELOPE LITERALS ONLY — no digest moved** (`session_vectors.h` digest
  `966aca05…` UNCHANGED: SESSION-SK-001 carries no two-speed airframe); 4 golden dirs.
- **Byte-identical / in sync (verified):** `event_vectors.h`, golden_params/snapshot/weapon/
  framing/geo001/interp/detmath/coeffs.
- **Property tests 197 → 203** (+6 `test_blower.py`: roster contract + flavor, two-speed one-tick
  bit-exact through the kernel in every regime, single-speed-never-enters-the-branch inertness,
  flat-fall-flat-fall shape with the FS band == the dyadic constant EXACTLY, Blower-001
  non-degeneracy, SL-rated-is-low-gear). `test_supercharger.py` updated: `_expected_vnew` carries
  the two-speed branch, the crit-0 test zeroes the new fields too, the P-51 margin 2.0→1.0
  (measured +1.7 at 6500 m on 14/16 FS power — documented), two stale comments rewritten.
- **HUD (presentation rider, bundled like v1.24r0's):** `viewer_main.cpp` `hud_power` gains the
  same two-min/max branch (native replay + fly HUD `pwr`, the scoreboard crit column shows
  `lo/hi` for two-speed airframes, selfcheck echoes crit_lo/gear2); `record_main.cpp` emits a
  per-slot `"crit": [crit_alt, crit_lo, gear2]` array into `trajectory.js` meta; `web/viewer.js`
  gains `hudPower` + a `pwr` readout on each HUD row. Presentation-only ⇒ goldens byte-identical;
  the dogfight demo reads the sealed roster back exactly (P-51 3000/0.875, Yak-3 1000/0.9375,
  La-7 1500/0.875).
- Rails 340→350: `atmosphere_density` gains `blower_lapse` (the two-speed model text) +
  `blower_gear2_frac_grid` (the dyadic contract); header seal string → v1.25r0. No rail VALUE
  changed.

## Rejected alternatives

- **Move the `max()` out of the `crit_lo_alt_m > 0` branch** (compute it unconditionally with
  `gear2 = 1`, `crit_lo = crit_alt`) — rejected: `max(min(1,σ/σc), 1·min(1,σ/σc))` is the same
  value but changes the single-speed byte sequence (extra ops on the sealed path). Guarding the
  branch keeps single-speed bit-for-bit v1.22r0.
- **Arbitrary shift altitudes / non-dyadic gear fractions** — rejected for the 500 m grid + /16
  contract: keeps both divide denominators sealed nodes and the FS plateau exactly representable.
- **Model all radials/inlines as two-speed** — rejected: only P-51/La-7/Yak-3 had a genuine
  pilot-relevant two-speed blower on this roster; the P-47's turbo is continuously regulated
  (single crit to the band top is the honest abstraction) and the rest were single-speed.

## Gates

Sphere `6914a994…2b13eb20` byte-identical; all 15 goldens C++ ≡ Python bit-for-bit on GCC AND
Clang locally (10 unchanged + 4 moved + the new Blower-001, validated hash-by-hash); ctest 19/19
both toolchains; 203 property tests; tuning_probe + spec_monotone + det_math_oracle + atm_top +
determinism-lint PASS; all generated headers in sync; cross-toolchain matrix (MSVC + AArch64)
proven in guardian CI.
