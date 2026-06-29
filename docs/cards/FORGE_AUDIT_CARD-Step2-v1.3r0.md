# Forge Audit Card — Step 2: broaden C++ det_math parity coverage (ATM-Sphere v1.3r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere v1.3r0 (rides current seal — no rail change, no golden change)
**Realm:** ATM-only
**Scope:** parity test breadth only — generator + vectors header + test dispatcher.
No kernel, no `det_math.cpp`, no `detmath_ref.py`, no rails, no wire, no golden.

## Gates & Results
- [x] lint_determinism.py — PASS
- [x] spec_monotone_check.py — PASS (rails header v1.3r0/130, untouched)
- [x] det_math_oracle.py (--samples 4000) — PASS (reference accuracy unchanged: sqrt 0, sin/cos/atan 1, asin/tan 2 ULP)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] gen_*.py --check (all 5 headers, incl. detmath_vectors.h) — PASS
- [x] gen_detmath_vectors.py: regenerate → --check — **byte-reproducible** (integer RNG + +−*/ only)
- [x] **seads_detmath_test (GCC) — PASS: bit-exact vs reference (4878 vectors)**
- [x] **seads_detmath_test (Clang) — PASS: bit-exact vs reference (4878 vectors)**
- [x] validate_snapshot.py GOLDEN-SK-Sphere-001 — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] validate_scenarios GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — match sealed hashes
- [x] pytest tests/property — 20 passed
- [x] make_receipt.py — overall **PASS** (receipt `…v1.3r0-d5e5e44.yml`)
- [ ] CI guardian.yml cross-toolchain (MSVC + GCC/Clang × x64/AArch64) — PENDING push

## Coverage delta
- Vectors: **64 → 4878** (≈76×). Per-function: boundary rows (quadrant edges k·π/2, fdlibm
  atan regions 7/16·11/16·19/16·39/16, asin ±1 clamp, atan2 axis cases, wrap edges ±k·2π,
  tiny/large magnitudes) + 512 seeded random rows/group.
- New functions now gated for C++↔reference parity (previously **zero** coverage, only
  transitive via the golden): **`det_atan2`, `wrap_pi`, `wrap_2pi`** — all live kernel calls
  (`kernel.cpp:29,66`).

## Determinism & Invariants
- Tolerance is **exact bit-equality** (parity), distinct from the ≤2-ULP MPFR accuracy gate.
- Sample points: integer **SplitMix64** PRNG; `u01()` = top-53-bits × 2⁻⁵³; only `+ − * /`
  applied to drawn doubles. **No wall-clock RNG, no libm** in point selection ⇒ header is
  byte-identical on Windows/Linux ⇒ CI `--check` stays green. Seeds fixed per group; draw
  varies by index only.
- `expected` values come from `detmath_ref` (only `+ − * /` and correctly-rounded sqrt).
- No FMA, no fast-math, det_math-only — all unchanged.

## Affected Files
- tools/gen_detmath_vectors.py (rewritten generator; SplitMix64 + structured boundaries)
- src/det_math/detmath_vectors.h (generated; Vec gains `double y`; 4878 vectors)
- src/det_math/detmath_test_main.cpp (dispatcher: +atan2/wrap_pi/wrap_2pi, two-operand print)
- docs/adr/ADR-Step2-DetMathParity-v1.3r0.md; docs/receipts/receipt-ATM-Sphere_v1.3r0-d5e5e44.yml

## Decision
**APPROVE**

**Sign-off:** Forge · det_math/tooling · 2026-06-28
