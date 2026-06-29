# ADR-Step6-GEO001-Codec-v1.3r0 — GEO-001 wire codec (netcode layer 1)

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge — kernel/tooling
**Seal:** ATM-Sphere v1.3r0 (rides current seal — no rail change, no golden change)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Roadmap Step 6 (netcode state-sync, the multiplayer-flight MVP) is built bottom-up, each
layer gated before the next. The first, self-contained layer is the **GEO-001 wire codec**.
GEO-001 is already a **sealed rail** (`config/rails/atm.json → rails.wire`):

    format = "GEO-001"; latlon_scale = 1e7; bearing_scale = 1e6; alt_scale = 1e3;
    encoding = "ZigZag+LEB128"

*Implementing* this rail to spec is **not** a reseal; any *deviation from* it would be. No
rail is touched, no kernel code changes, and no `world_hash` is involved (the codec is
transport, downstream of the sim). Immutable rails that must stay green: all of them —
this change is additive and outside the kernel boundary.

## 2) Decision
Add the GEO-001 codec as a new isolated module + its cross-impl parity gate, mirroring the
proven det_math pattern (Python reference ↔ generated vectors ↔ C++ byte-exact test):

- `tools/geo001_ref.py` — canonical reference (ZigZag, LEB128, quantize/dequantize, i64
  field codec, GeoPoint record in fixed order lat,lon,bearing,alt). Single source of truth.
- `src/net/geo001.{h,cpp}` — C++ codec, a bit-for-bit mirror of the reference. New
  `seads_net` static lib. **Deliberately does not link `det_math`** (no transcendentals;
  must never be on the path that feeds the sim) — links the strict FP flags only so the one
  float op (value×scale) rounds identically cross-toolchain.
- `tools/gen_geo001_vectors.py` → `src/net/geo001_vectors.h` — integer-driven parity
  vectors (boundaries + LEB128 length edges + SEADS field ranges + i64 extremes incl.
  INT64_MIN + seeded SplitMix64 rows); byte-reproducible so CI `--check` stays green.
- `src/net/geo001_test_main.cpp` (`seads_geo001_test`) — asserts, per vector, byte-identical
  wire on encode **and** exact round-trip on decode (307 i64, 68 point, 15 quantize rows).
- `tests/property/test_geo001.py` — Hypothesis round-trips (ZigZag, LEB128, i64 field,
  GeoPoint within one quantum, quantize/dequantize) + a rails-scale-match assertion.
- Gate wiring: `guardian.yml` (gen `--check` + `seads_geo001_test` per toolchain leg +
  reference self-test), `make_receipt.py` (`geo001_codec` gate), `CMakeLists.txt`.

## 3) Rationale
- **Correctness to the rail.** ZigZag is the protobuf sint64 mapping; LEB128 is the standard
  unsigned varint. Quantization uses round-half-away-from-zero via `floor(x+0.5)/ceil(x-0.5)`
  on IEEE doubles — explicitly specified so C++ and CPython agree to the bit.
- **Cross-impl parity, proven.** C++ encode == reference bytes (byte-for-byte) ⇒ encoders
  agree. C++ decode(bytes) == input and reference decode(bytes) == input (property tests) ⇒
  "C++ encodes, Python decodes" holds transitively. Same guarantee shape as det_math.
- **Determinism story preserved.** No GUI/network dep, headless, kernel untouched, golden
  untouched. CI stays kernel-only + this pure codec.

## 4) Consequences
- **+** A complete, gated transport primitive every later netcode layer (snapshot
  serialization, loopback lockstep, prediction/interpolation) builds on.
- **+** New required-ish gates: `seads_geo001_test` (×6 toolchain legs), `geo001_codec`
  receipt gate, 7 new property tests (20 → 27).
- **−** One more generated header to keep in sync (`gen_geo001_vectors.py --check`).
- The wire is **lossy by quantization** — decoded values must never be fed back into the
  kernel as if canonical; that boundary is documented in the header.

## 5) Alternatives Considered
- **Renderer first (Step 5).** Rejected for now: pulls the first GUI dep and de-risks
  nothing; netcode stays in the headless/deterministic world this harness is built for.
- **Fixed-width LE integers instead of ZigZag+LEB128.** Rejected: violates the GEO-001 rail
  (would be a reseal) and wastes bytes on small deltas.
- **Banker's rounding / `std::round` / `std::llround`.** Rejected: Python `round()` is
  half-to-even and `std::round` is half-away — mixing them desyncs the wire. Explicit
  `floor(x+0.5)/ceil(x-0.5)` is identical in both languages.

## 6) Acceptance & Probes
All PASS under seal v1.3r0:
- `python tools/spec_monotone_check.py config/rails/atm.json` — rails untouched.
- `python tools/geo001_ref.py` — reference self-test PASS.
- `python tools/gen_geo001_vectors.py --check` — header in sync (byte-reproducible).
- `seads_geo001_test` (GCC + Clang local; MSVC + GCC/Clang × x64/AArch64 in CI) — byte-exact.
- `python -m pytest tests/property` — 27 passed.
- Golden `GOLDEN-SK-Sphere-001` world_hash — **unchanged** (`529c6a05…9218fe16`); codec is
  not on the hash path.

## 7) Ledger Discipline
Chronicle receipt records: seal v1.3r0, golden_sha256 (unchanged), commit_sha, toolchain
matrix, gates incl. `geo001_codec`, signoff Forge.

## 8) Implementation Notes
- INT64_MIN literal emitted as `(-9223372036854775807LL - 1LL)` (the bare literal parses as
  negation of an out-of-range positive and fails `-Wc++11-narrowing` under Clang).
- LEB128 decode caps at 10 bytes (`ceil(64/7)`) and rejects truncated/overlong varints.
- No seal: GEO-001 rail implemented to spec, not changed. No kernel/wire-rail/golden edit.
