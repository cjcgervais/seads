# Forge Audit Card — Step 6 layer 2: GEO-001 snapshot serialization (ATM-Sphere v1.3r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere v1.3r0 (rides current seal — GEO-001 rail used to spec, no rail/golden change)
**Realm:** ATM-only
**Scope:** new snapshot framing on top of the geo001 codec (into existing `seads_net`) +
cross-impl parity gate. No kernel, no `det_math`, no rails values, no golden.

## Gates & Results
- [x] lint_determinism.py — PASS
- [x] spec_monotone_check.py — PASS (rails header v1.3r0, untouched)
- [x] det_math_oracle.py (--samples 2000) — PASS (unchanged)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] geo001_ref.py self-test — PASS
- [x] snapshot_ref.py self-test — PASS
- [x] gen_snapshot_vectors.py: regenerate → --check — **byte-reproducible** (hex-float fields)
- [x] gen_*.py --check (all 7 headers incl. geo001_vectors.h + snapshot_vectors.h) — PASS
- [x] **seads_snapshot_test (GCC) — PASS: byte-exact vs reference (4 snapshots, 5 conv)**
- [x] **seads_snapshot_test (Clang) — PASS: byte-exact vs reference (4 snapshots, 5 conv)**
- [x] seads_geo001_test (GCC + Clang) — PASS (layer 1, still green)
- [x] validate_snapshot.py GOLDEN-SK-Sphere-001 — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] validate_scenarios GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — match sealed hashes
- [x] pytest tests/property — **32 passed** (27 → 32; +5 snapshot)
- [x] make_receipt.py — overall **PASS** (gates `geo001_codec` + `snapshot_codec` green)
- [ ] CI guardian.yml cross-toolchain (MSVC + GCC/Clang × x64/AArch64) — PENDING push

## What this layer is
The 20 Hz world snapshot transport: header `(protocol, server_tick, n)` then `n × (id,
GeoPoint)` over the GEO-001 codec. Self-delimiting. `from_kernel` maps kernel radians → wire
degrees (psi heading → GEO-001 bearing). Foundation for loopback lockstep → prediction →
correction/late-join.

## Determinism & Invariants
- Separate **quantized transport** — `Kernel::snapshot()` (raw LE f64) remains the world_hash
  source of truth. Wire snapshot is lossy, downstream, never fed back as canonical.
- Float ops are only the rad↔deg multiply (hex-float `RAD2DEG`/`DEG2RAD`) and `value×scale`;
  identical in C++ and CPython. `seads_net` still does **not** link `det_math`.
- Generated vectors: entity degree fields as exact hex-floats, ids/tick as integers ⇒ wire
  bytes byte-identical cross-platform ⇒ CI `--check` green.
- **Documented deferral:** `phi`/`tas` are not on the wire (no GEO-001 scale → would reseal);
  layer 2 supports remote interpolation, full prediction awaits a later layer. Not a silent cap.

## Affected Files
- tools/snapshot_ref.py (new — canonical reference + self-test)
- src/net/snapshot.h, src/net/snapshot.cpp (new — added to `seads_net`)
- tools/gen_snapshot_vectors.py, src/net/snapshot_vectors.h (new — generated parity vectors)
- src/net/snapshot_test_main.cpp (new — `seads_snapshot_test`)
- tests/property/test_snapshot.py (new — 5 Hypothesis/round-trip tests)
- CMakeLists.txt (snapshot.cpp in seads_net + seads_snapshot_test exe + ctest)
- .github/workflows/guardian.yml (gen --check + reference self-test + snapshot test per leg)
- tools/make_receipt.py (snapshot_codec gate)
- docs/adr/ADR-Step6-Snapshot-Serialization-v1.3r0.md; docs/receipts/receipt-…

## Decision
**APPROVE**

**Sign-off:** Forge · net/tooling · 2026-06-28
