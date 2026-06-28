# Annex ATM-TOP v1.2 — Altitude Ceiling (ATM-only)

**Seal:** ATM-Sphere v1.2r0
**Rail Affected:** ATM_TOP (altitude ceiling only)
**Previous:** ATM_TOP = 6000 m, SOFT_BAND = 100 m  (v1.1r1)
**New:** ATM_TOP = 8000 m, SOFT_BAND = 100 m  (v1.2r0)

## Invariants (unchanged)
- Geometry: sphere R=15000 m, flattening=0
- Gravity: g₀=9.80665 m/s² (constant); Tick: Δt=0.01 s (exact)
- Realm: ATM-only (no terrain/orbit); altitude = MSL
- Determinism: det_math only; no fast-math/FMA; ban std libm transcendentals
- Wire/Hash: GEO-001 unchanged

## Acceptance
1. **Clamp:** spawn at 7980/7990/7995 m with sustained climb → altitude never exceeds 8000 m.
2. **Soft band:** between 7900–8000 m, climb rate is monotonically reduced vs the same input at 7800 m.
3. **Near-ceiling small-circle:** @ 7950 m, φ=30°, 60 s → bearing/step error ≤ 1e-6.
4. **Golden replay:** GOLDEN-SK-Sphere-001 world_hash governed by the sealed reference.

Verified by `python tools/atm_top_probe.py --ceil 8000 --soft 100`.

## Notes
Spawn protocol (~1.0–1.2 km) unaffected. No schema/header change — a rails value update + seal bump.
