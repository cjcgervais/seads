# ADR-Step2-DetMathParity-v1.3r0 — Broaden C++ det_math parity coverage

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge — det_math/tooling
**Seal:** ATM-Sphere v1.3r0 (no rail change, no golden change — rides the current seal)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
The C++↔reference bit-exactness test (`seads_detmath_test`, driven by the generated
`src/det_math/detmath_vectors.h`) is the fast pre-golden tripwire: it catches any drift
between the C++ port (`src/det_math/det_math.cpp`) and the canonical Python reference
(`tools/detmath_ref.py`) before the slower golden replay runs. Until now it used a **fixed
64-point** hand table, and it exercised only `sin/cos/tan/atan/asin/sqrt`.

Two gaps:
- **Sparse domain.** 64 points cannot demonstrate parity across each function's full SEADS
  operating range, in particular the algorithmic hinge points (Cody-Waite quadrant flips at
  `k·π/2`; the fdlibm atan reduction-region boundaries 7/16, 11/16, 19/16, 39/16; asin's
  `±1` clamp; the wrap nudge just past `±π`).
- **Untested functions.** `det_atan2`, `wrap_pi`, and `wrap_2pi` had **zero** C++↔reference
  parity coverage, despite being called directly in the kernel
  (`src/kernel/kernel.cpp:29` uses `det_atan2` + `wrap_pi`; `:66` uses `wrap_2pi`).

No rails are touched. **Immutable rails that must stay green and are untouched here:**
sphere R=15000/flattening=0; ATM-only; ATM_TOP=8000/SOFT=100; g₀=9.80665; Δt=0.01;
det_math-only with the libm/FMA/x87 ban; GEO-001 wire. The four sealed world_hashes
(Sphere `529c6a05…9218fe16` + Turn/Climb/TurnClimb) MUST remain byte-identical — and do,
because neither the kernel, `det_math.cpp`, nor `detmath_ref.py` is modified.

## 2) Decision
Replace the fixed 64-vector table with a comprehensive, deterministically-generated parity
suite, and extend the test harness to the previously-uncovered functions.

- **`tools/gen_detmath_vectors.py`** now emits, per function, **structured boundary rows**
  (the hinge points above, plus tiny/large magnitudes and axis cases) **+ dense seeded
  random rows** (`N_RANDOM = 512` per group) over the real argument range. Domains mirror
  the accuracy gate `tools/det_math_oracle.py`:
  - `sin`/`cos` over `[-2π, 2π]` (the reference's stated pre-wrapped assumption);
  - `tan` over `±1.45 rad` (~83°, away from the poles);
  - `atan` over small/mid/large magnitudes both signs, well within the `2^66` cutoff;
  - `atan2(y,x)` over all four sign quadrants + the axis cases;
  - `asin` over `[-1, 1]` incl. the `±1` clamp and near-edge values;
  - `sqrt` over `[0, ~2.25e8]` (incl. R²) and the `(1-x)(1+x)∈[0,1]` asin-internal band;
  - `wrap_pi`/`wrap_2pi` over several `2π` periods incl. both wrap edges.
- **Determinism of the generator.** Sample points are drawn with an integer **SplitMix64**
  PRNG; `u01()` maps the top 53 bits via an exact power-of-two reciprocal, and the only
  float ops applied to drawn values are `+ − * /`. No wall-clock RNG and **no libm
  transcendental** is used to pick sample points (those could differ by 1 ULP across
  platforms and desync the header). Seeds are fixed per function group; the draw varies by
  index only. Result: the emitted header is **byte-reproducible on every platform**, so the
  existing CI `gen_detmath_vectors.py --check` gate keeps passing on the Linux runners.
- **`src/det_math/detmath_vectors.h`** struct gains a second operand column:
  `struct Vec { const char* fn; double x; double y; double expected; };` (`y` is the second
  arg for `atan2`; `0.0`/unused for unary functions). 64 → **4878 vectors**.
- **`src/det_math/detmath_test_main.cpp`** dispatcher extended to `atan2(x,y)`, `wrap_pi`,
  `wrap_2pi`, and to print both operands on a mismatch.

Tolerance stays **exact bit-equality** (this is parity, not the ≤2-ULP MPFR accuracy
budget, which remains in `det_math_oracle.py`).

## 3) Rationale
Same op order + same hex-float constants ⇒ the C++ port and the reference are bit-identical
by construction across MSVC/Clang/GCC × x64/AArch64; a dense, boundary-aware suite turns
"by construction" into "demonstrated across the whole domain." Folding `atan2`/`wrap_pi`/
`wrap_2pi` into the gate closes a real hole: those are live kernel calls that were only ever
checked transitively via the golden replay. Keeping the generator to integer + `+−*/` math
preserves cross-platform header reproducibility, so the new breadth costs nothing in CI
flakiness. Bit-equality (not ULP) is the right bar because the C++ kernel's whole promise is
to reproduce the reference exactly.

## 4) Consequences
- **Positive:** ~76× more parity points; algorithmic boundaries explicitly pinned; three
  kernel-critical functions now gated pre-golden; failures localize to a function + operands
  long before the golden hash diverges. No new gate to wire — `seads_detmath_test` and
  `gen_detmath_vectors.py --check` already run in CI per toolchain.
- **Negative / cost:** the generated header grows (~4.9k lines); `seads_detmath_test` runs
  4878 vectors (still sub-millisecond). `N_RANDOM` is a single tunable knob if size matters.
- **No** rail, kernel, wire, or golden change → **no seal**.

## 5) Alternatives Considered
- **Exhaustive / property-based fuzzing in C++:** rejected — would require an RNG and oracle
  inside the C++ test, reintroducing a platform-variance surface; the generate-then-pin
  approach keeps the single source of truth in Python and the C++ side purely table-driven.
- **Raising N into the millions:** rejected — parity holds structurally; a few thousand
  boundary-aware points give full confidence without bloating the header or compile.
- **Leaving atan2/wrap out (covered transitively by the golden):** rejected — the point of
  the fast test is to localize drift *before* the golden; transitive-only coverage defeats it.

## 6) Acceptance & Probes
All PASS (2026-06-28, local winlibs GCC 14.2 + Clang 19.1, x64):
- `python tools/gen_detmath_vectors.py && python tools/gen_detmath_vectors.py --check` — in sync, reproducible
- `seads_detmath_test` (GCC **and** Clang) — `PASS: det_math bit-exact vs reference (4878 vectors)`
- `python tools/det_math_oracle.py --samples 4000` — PASS (reference accuracy unchanged)
- `python tools/lint_determinism.py` — PASS
- `gen_*.py --check` (all 5 headers) — PASS
- `python -m pytest tests/property -q` — 20 passed
- Golden `GOLDEN-SK-Sphere-001` world_hash — **unchanged** (`529c6a05…9218fe16`); the three
  scenario goldens unchanged; `make_receipt.py` overall **PASS**.

## 7) Ledger Discipline
Chronicle receipt: `docs/receipts/receipt-ATM-Sphere_v1.3r0-d5e5e44.yml`
(seal v1.3r0, golden_sha256 `529c6a05…9218fe16`, `rail_change: false`, all gates PASS,
signoff Forge). Forge audit card: `docs/cards/FORGE_AUDIT_CARD-Step2-v1.3r0.md`.

## 8) Implementation Notes
No build-flag, kernel, wire, or det_math-implementation change. Generator uses integer
SplitMix64 + `+−*/` only (no libm) so the header is byte-stable across OSes; CI `--check`
unaffected. To re-tune breadth, change `N_RANDOM` (or a function's sub-ranges) and rerun
`python tools/gen_detmath_vectors.py`, then rebuild. Mirrors all determinism rules
(det_math-only, no FMA, hex-float constants).
