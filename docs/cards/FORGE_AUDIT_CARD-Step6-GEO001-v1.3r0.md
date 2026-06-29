# Forge Audit Card — Step 6 layer 1: GEO-001 wire codec (ATM-Sphere v1.3r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere v1.3r0 (rides current seal — GEO-001 rail implemented to spec, no rail/golden change)
**Realm:** ATM-only
**Scope:** new transport module + cross-impl parity gate. New `src/net/`, new `seads_net`
lib. No kernel, no `det_math`, no `detmath_ref.py`, no rails values, no golden.

## Gates & Results
- [x] lint_determinism.py — PASS
- [x] spec_monotone_check.py — PASS (rails header v1.3r0, untouched)
- [x] det_math_oracle.py (--samples 2000) — PASS (unchanged)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] geo001_ref.py self-test — PASS
- [x] gen_geo001_vectors.py: regenerate → --check — **byte-reproducible** (integer RNG only)
- [x] gen_*.py --check (all 6 headers incl. geo001_vectors.h) — PASS
- [x] **seads_geo001_test (GCC) — PASS: byte-exact vs reference (307 i64, 68 point, 15 quant)**
- [x] **seads_geo001_test (Clang) — PASS: byte-exact vs reference (307 i64, 68 point, 15 quant)**
- [x] validate_snapshot.py GOLDEN-SK-Sphere-001 — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] validate_scenarios GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — match sealed hashes
- [x] pytest tests/property — **27 passed** (20 → 27; +7 geo001)
- [x] make_receipt.py — overall **PASS** (gate `geo001_codec` green)
- [ ] CI guardian.yml cross-toolchain (MSVC + GCC/Clang × x64/AArch64) — PENDING push

## What this layer is
The first netcode layer: the GEO-001 wire codec (ZigZag + LEB128 over fixed-point i64).
GEO-001 is a **sealed rail**; this *implements* it (no reseal). Encode/decode for the
quantized fields (lat/lon×1e7, bearing×1e6, alt×1e3) + a GeoPoint record in fixed field
order (lat, lon, bearing, alt). Foundation for snapshot serialization → loopback lockstep →
prediction/interpolation.

## Determinism & Invariants
- Codec is **integer-only** (ZigZag/LEB128); cross-impl parity is exact by construction.
- The single float op is `value × scale`; quantization is **round half away from zero**
  (`floor(x+0.5)/ceil(x-0.5)`) — identical in CPython and C++ (IEEE round-to-nearest-even,
  no FMA, no x87). `seads_net` links the strict FP flags; does **not** link `det_math`.
- Generated vectors: integer **SplitMix64** + structured boundaries (LEB128 length edges,
  field ranges, INT64_MIN/MAX). No wall-clock RNG, no libm ⇒ header byte-identical on
  Windows/Linux ⇒ CI `--check` green.
- Wire is **lossy by quantization** and lives **outside** the kernel/world_hash; decoded
  bits are never fed back into the sim as canonical. Kernel + golden untouched.

## Affected Files
- tools/geo001_ref.py (new — canonical reference + self-test)
- src/net/geo001.h, src/net/geo001.cpp (new — C++ codec; `seads_net` lib)
- tools/gen_geo001_vectors.py, src/net/geo001_vectors.h (new — generated parity vectors)
- src/net/geo001_test_main.cpp (new — `seads_geo001_test`)
- tests/property/test_geo001.py (new — 7 Hypothesis/round-trip tests)
- CMakeLists.txt (seads_net lib + seads_geo001_test exe + ctest)
- .github/workflows/guardian.yml (gen --check + reference self-test + codec test per leg)
- tools/make_receipt.py (geo001_codec gate)
- docs/adr/ADR-Step6-GEO001-Codec-v1.3r0.md; docs/receipts/receipt-…

## Decision
**APPROVE**

**Sign-off:** Forge · net/tooling · 2026-06-28
