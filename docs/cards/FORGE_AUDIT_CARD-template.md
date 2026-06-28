# Forge Audit Card — <change> (<seal>)

**Project:** SEADS 2026
**Seal:** ATM-Sphere vX.YrZ
**Realm:** ATM-only
**Scope:** <rails value change | data-only | kernel | tooling>

## Gates & Results
- [ ] spec_monotone_check.py — PASS/FAIL
- [ ] det_math_oracle.py — PASS/FAIL (if det_math touched)
- [ ] tuning_probe.py — PASS/FAIL
- [ ] atm_top_probe.py — PASS/FAIL
- [ ] validate_snapshot.py (GOLDEN-SK-Sphere-001) — world_hash <unchanged|new+sealed>
- [ ] pytest tests/property — PASS/FAIL
- [ ] CI guardian.yml cross-toolchain — PASS/FAIL

## Determinism & Invariants
- Geometry sphere R=15000, flattening=0; g₀=9.80665; Δt=0.01; det_math only; GEO-001 unchanged.

## Affected Files
- <list>

## Decision
**APPROVE / REJECT**

**Sign-off:** <agent/human> · <role>
**Date:** YYYY-MM-DD
