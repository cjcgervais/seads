# Forge Audit Card — Step 4: envelope flight inputs + scripted goldens (ATM-Sphere v1.3r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere v1.3r0
**Realm:** ATM-only
**Scope:** kernel behavior (new flight-input model) + new sealed goldens + generators/CI. No rail change.

## Gates & Results
- [x] spec_monotone_check.py — PASS (rails header v1.3r0/130)
- [x] det_math_oracle.py — PASS (det_math untouched; lut_eval is +,-,*,/ only)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] gen_*.py --check (5 headers incl. envelope_tables.h, scenario_params.h) — PASS
- [x] validate_snapshot.py GOLDEN-SK-Sphere-001 — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] validate_snapshot.py GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — match sealed hashes (Python + C++)
- [x] pytest tests/property — PASS (20; +3 scenario sealed-hash, +3 bank-limit, +1 zero-command)
- [x] Local C++ (winlibs MinGW GCC + Clang) — all four goldens bit-for-bit vs reference
- [ ] CI guardian.yml cross-toolchain (MSVC + GCC/Clang × x64/AArch64) — PENDING push

## Determinism & Invariants
- Geometry sphere R=15000, flattening=0; g₀=9.80665; Δt=0.01; det_math only; GEO-001 unchanged.
- New ops are `+ − * /` + exact IEEE comparisons, no FMA. Constants via hex-float generated headers.
- Phase select integer-only. deg→rad baked into LUT nodes + pre-converted scheduled angles.
- Straight golden preserved: shared `advance_(i,req)` is the verbatim v1.2r0 tail; no-arg `step()`
  calls it with req=0, φ unmodified; Sphere keeps its own runner.

## Affected Files
- src/kernel/{kernel.h,kernel.cpp,flight_types.h,scenario_main.cpp}; CMakeLists.txt
- src/kernel/{envelope_tables.h,scenario_params.h} (generated)
- tools/{envelopes.py,gen_envelope_tables.py,gen_scenario_params.py,detmath_ref.py,ref_kernel.py,make_receipt.py,validate_snapshot.py}
- config/rails/atm.json (header→v1.3r0/130); config/scenarios/GOLDEN-SK-{Turn,Climb,TurnClimb}-001.json
- tests/golden/GOLDEN-SK-{Turn,Climb,TurnClimb}-001/*; tests/property/test_kernel.py
- .github/workflows/guardian.yml; docs/{adr,cards,SEAL_CARD.md}

## Decision
**APPROVE**

**Sign-off:** Forge · kernel/data · 2026-06-28
