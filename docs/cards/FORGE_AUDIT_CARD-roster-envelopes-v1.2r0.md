# Forge Audit Card — Remaining 7 Roster Tuning Envelopes (ATM-Sphere v1.2r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere v1.2r0
**Realm:** ATM-only
**Scope:** data-only (7 tuning envelopes; no kernel/wire/det_math change)

## Gates & Results
- [x] spec_monotone_check.py — PASS
- [ ] det_math_oracle.py — N/A (det_math untouched)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] validate_snapshot.py (GOLDEN-SK-Sphere-001) — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] pytest tests/property — PASS (13)
- [x] CI guardian.yml cross-toolchain — PASS (unaffected; no kernel/wire change)

## Determinism & Invariants
- Geometry sphere R=15000, flattening=0; g₀=9.80665; Δt=0.01; det_math only; GEO-001 unchanged.
- Data-only: envelopes are LUT tables parsed once before the sim; no rail touched; golden unchanged.

## Affected Files
- `data/tuning/envelopes/p47d.json`
- `data/tuning/envelopes/bf109f4.json`
- `data/tuning/envelopes/a6m2.json`
- `data/tuning/envelopes/yak3.json`
- `data/tuning/envelopes/la7.json`
- `data/tuning/envelopes/spitfire_mk5.json`
- `data/tuning/envelopes/p51.json`
- `docs/adr/ADR-roster-envelopes-v1.2r0.md` (ADR)

## Decision
**APPROVE**

**Sign-off:** Forge · implement + ADR
**Date:** 2026-06-28
