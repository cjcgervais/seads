# SEADS 2026 — Spherical Earth Aerial Dogfight Simulator

A **deterministic** WWII prop-dogfighting simulator on a tiny perfect sphere (R = 15 km), built
like a high-assurance system: the simulation kernel produces **bit-for-bit identical output across
MSVC / Clang / GCC on x64 + AArch64**. Seal: **ATM-Sphere v1.2r0**.

> Read [`CLAUDE.md`](CLAUDE.md) — the project constitution (rails, change-control law, agent model).

## What this repo is

This is the **development harness** for SEADS — repo + deterministic verification loop + seal/ledger
governance, with the deterministic core proven on the golden replay.

```
config/rails/atm.json   rails (machine-readable, single source of truth)
src/det_math/           deterministic transcendental math (mirrors the Python reference)
src/kernel/             100 Hz fixed-step sim: great-circle kinematics, ceiling clamp
src/replay/             world_hash (SHA-256 of canonical state)
tools/                  Python verification harness + reference kernel + code generators
tests/golden/           sealed GOLDEN-SK-Sphere-001 initial state + world_hash
tests/property/         Hypothesis metamorphic relations
docs/                   governance ledger (ADRs, annexes, audit cards, receipts, seal card)
.claude/                agent operating model (Forge/Auditor/Guardian/Chronicler) + skills
.github/workflows/      Guardian — cross-toolchain determinism gate
```

## The determinism architecture

`tools/detmath_ref.py` is the **canonical reference** for all transcendental math (libm-free,
fdlibm-derived minimax, verified to ≤2 ULP against an MPFR oracle). Its exact hex-float constants are
**generated into the C++ headers** (`gen_coeffs.py`, `gen_golden_params.py`, `gen_detmath_vectors.py`)
so the C++ kernel cannot drift from the reference. The golden replay's `world_hash` is computed over
the kernel's canonical snapshot bytes; CI proves every toolchain reproduces the sealed hash.

## Quickstart (verification — no C++ toolchain required)

```bash
pip install gmpy2 hypothesis pytest
python tools/lint_determinism.py
python tools/det_math_oracle.py --samples 8000          # det_math vs MPFR
python tools/spec_monotone_check.py config/rails/atm.json
python tools/tuning_probe.py data/tuning/envelopes/*.json
python tools/atm_top_probe.py --ceil 8000 --soft 100
python -m pytest tests/property -q
python tools/ref_kernel.py                              # prints the golden world_hash
python tools/make_receipt.py                            # runs all gates -> Chronicle receipt
```

## Building the C++ kernel

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/seads_detmath_test          # det_math bit-exact vs reference
build/seads_golden --out run.bin  # produces the golden snapshot + world_hash
python tools/validate_snapshot.py \
  --golden tests/golden/GOLDEN-SK-Sphere-001/expected.world_hash --candidate run.bin
```

Cross-compiler / cross-arch bit-identity is enforced by `.github/workflows/guardian.yml`.

## Roadmap (seals)
v1.2r0 (now): sealed deterministic core + harness → next: full det_math coverage in C++ tests →
8-aircraft envelopes → custom C++ renderer → netcode state-sync (multiplayer flight) →
guns/projectiles (post-MVP, new seal).
