# Master Seal Card

**Seal:** ATM-Sphere v1.2r0
**Realm:** ATM-only
**Geometry:** R=15000 m, flattening=0
**Tick:** Δt=0.01 s (100 Hz)
**Gravity:** g₀=9.80665 m/s² (constant)
**Ceiling:** ATM_TOP=8000 m, SOFT=100 m
**Determinism:** det_math only; ban libm/fast-math/FMA/x87
**Wire/Hash:** GEO-001 (lat/lon×1e7, bearing×1e6, h×1e3; ZigZag+LEB128)
**Roster (8):** P-47D · Bf 109 F-4 · Ki-61 · A6M2 · Yak-3 · La-7 · Spitfire Mk V · P-51
**Golden:** GOLDEN-SK-Sphere-001 (10,000 ticks; (0°,0°), ψ=45°, TAS=250 m/s)

## Seal history
| Seal | Date | Change |
|------|------|--------|
| v1.1r1 | 2025-10 | Roster-8 sealed; ATM_TOP=6000 m |
| v1.2r0 | 2025-10-12 | ATM_TOP 6000→8000 m (+100 m soft band); kernel/wire unchanged |
| v1.2r0 (repo) | (Pass 1) | Deterministic core + harness stood up; golden sealed |
