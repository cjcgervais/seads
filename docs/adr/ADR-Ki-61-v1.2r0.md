# ADR-Ki-61-v1.2r0 — Ki-61 Hien Tuning Scaffold

**Status:** Accepted
**Date:** 2025-10-12
**Author:** Forge
**Seal:** ATM-Sphere v1.2r0
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Under v1.2r0 the ceiling was raised to 8000 m (+100 m soft band) to enable high-altitude
boom-and-zoom. The Ki-61 Hien (sealed roster of 8) needs a rails-compliant, deterministic tuning
envelope. Immutable rails that must stay green: sphere R=15000/flattening=0; ATM-only; ATM_TOP=8000,
SOFT=100; g₀=9.80665; Δt=0.01; S² coordinated turn `ψ̇=g₀·tan(φ)/V`; det_math only; GEO-001 wire.

## 2) Decision
Add `data/tuning/envelopes/ki61.json` (data-only; no kernel/wire change):
- `v_ne_mps=170`, `stall_tas_mps=40`
- `phi_max_deg_vs_tas=[[40,60],[60,55],[100,50],[140,45],[170,40]]`
- `climb_rate_max_mps_vs_tas=[[40,15],[80,12],[120,10],[160,8],[170,7]]`
- `climb_rate_min_mps_vs_tas=[[40,-20],[80,-18],[120,-15],[160,-12],[170,-10]]`
- `roll_rate_degps_vs_tas=[[40,80],[80,90],[120,100],[160,95],[170,90]]`

## 3) Rationale
Historically plausible agility for 0–8000 m; monotone TAS domains with Lipschitz-bounded steps
(≤15° bank, ≤10 m/s climb per 10 m/s TAS); no libm fast-paths; data-only keeps golden valid.

## 4) Consequences
Positive: validates the ceiling extension with a real airframe. Negative: none identified;
stays within probe bounds and rails.

## 5) Alternatives Considered
Tighter LUT bounds (rejected — needlessly reduces agility); higher V_ne=180 (rejected — plausibility
+ cross-roster balance).

## 6) Acceptance & Probes
- `python tools/tuning_probe.py data/tuning/envelopes/ki61.json` → PASS
- `python tools/spec_monotone_check.py config/rails/atm.json` → PASS
- `python tools/atm_top_probe.py --ceil 8000 --soft 100` → PASS (clamp + soft-band monotone)
- Golden `GOLDEN-SK-Sphere-001` world_hash unchanged (data-only change).

## 7) Ledger Discipline
Receipt fields: seal=ATM-Sphere v1.2r0, tuning_added=Ki-61-Hien, golden_sha256=<unchanged>,
probes={tuning_probe, ceiling_clamp}, signoff=Forge.

## 8) Implementation Notes
Optionally sign with `python tools/hash_sign_json.py --inplace data/tuning/envelopes/ki61.json`.
No build-flag/kernel/wire change; determinism bans remain enforced.

**Appendix — Roster (sealed 8):** P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, P-51.
