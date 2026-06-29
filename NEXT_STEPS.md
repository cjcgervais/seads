# SEADS 2026 — Next Steps (handoff)

> Resume doc for a fresh session. State as of seal **ATM-Sphere v1.2r0**, git `main`.
> Read `CLAUDE.md` first (the constitution). Background facts also live in Claude memory
> (`seads-canon`, `seads-harness`).

## Where things stand (DONE)

Pass 1 is complete: the deterministic core + governance harness is up and the bit-for-bit
promise is **proven locally**.

- Sealed golden **GOLDEN-SK-Sphere-001** world_hash = `529c6a0598eccd41facdbb69bbc4bff18e0a743c3b1cc5ff43ad14f89218fe16`.
- Reproduced bit-identically by 3 implementations: Python reference, **GCC 14.2.0** (x64), **Clang 19.1.1** (x64).
- All Python gates green; 13 Hypothesis property tests pass; det_math verified ≤2 ULP vs MPFR.
- C++ det_math is bit-exact vs the reference (64-vector `ctest`).
- Ledger receipts written (`docs/receipts/`), all `overall: PASS`. Working tree clean.

## Environment notes (important for a cold start)

- This machine has **Python 3.11 + git**; Python deps installed: `gmpy2 hypothesis pytest`.
- C++ toolchain (installed locally) is **NOT on PATH by default**. Prepend it:
  ```
  $bin="$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
  $ninja="$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
  $env:PATH="$bin;$ninja;$env:PATH"
  ```
  (g++, clang++, cmake, ninja all live in `$bin`/`$ninja`.)
- Git on D:\ needed `git config --global --add safe.directory D:/SEADS_2026` (already done).
- PowerShell gotcha: do **not** redirect native-exe streams with `*>`/`2>&1` (wraps stderr as
  errors and aborts). Pipe stdout normally; use `| Out-Null` to silence.

## Verify everything still works (2 min)

```bash
# Python side (no compiler needed)
python tools/lint_determinism.py
python tools/det_math_oracle.py --samples 8000
python tools/spec_monotone_check.py config/rails/atm.json
python tools/tuning_probe.py data/tuning/envelopes/*.json
python tools/atm_top_probe.py --ceil 8000 --soft 100
python -m pytest tests/property -q
python tools/make_receipt.py            # runs all gates -> receipt, overall: PASS
```
```powershell
# C++ side (PATH set as above)
cmake -S . -B build-gcc   -G Ninja -DCMAKE_CXX_COMPILER=g++     -DCMAKE_BUILD_TYPE=Release; cmake --build build-gcc
cmake -S . -B build-clang -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release; cmake --build build-clang
.\build-gcc\seads_golden.exe   --out run_gcc.bin
.\build-clang\seads_golden.exe --out run_clang.bin
python tools\validate_snapshot.py --golden tests\golden\GOLDEN-SK-Sphere-001\expected.world_hash --candidate run_gcc.bin
python tools\validate_snapshot.py --golden tests\golden\GOLDEN-SK-Sphere-001\expected.world_hash --candidate run_clang.bin
```

## Next steps — pick up here (in priority order)

### 1. Finish the cross-arch determinism proof (MSVC + AArch64)  ✅ DONE (2026-06-28)
**Cross-toolchain bit-identity is now PROVEN in CI.** Remote `origin` =
`https://github.com/cjcgervais/seads` (public). `guardian.yml` green on `main` —
all six legs reproduce the seal `529c6a05…9218fe16`:
MSVC x64 · GCC x64 · Clang x64 · **GCC arm64** · **Clang arm64** + the aggregation gate.
Green run: https://github.com/cjcgervais/seads/actions/runs/28340968855
- The AArch64 legs (highest divergence risk: FMA/libm) reproduce the seal exactly — no
  divergence ever occurred. All failures en route were harness/CI bugs, not the kernel:
  (a) `-lwinpthread` was hardcoded for all compilers → scoped to `if(MINGW)`, portable
  threads on Linux/macOS (`CMakeLists.txt`);
  (b) MSVC generator `VS 17 2022` vs the new `windows-2025-vs2026` runner → pinned the MSVC
  leg to `windows-2022` (`guardian.yml`);
  (c) aggregation hash compare used `tr -d ' \n'` which left a stray `\r` from the CRLF
  `expected.world_hash` → now `tr -d '[:space:]'` (`guardian.yml`).
  All three ride seal v1.2r0 (no rail/golden/kernel change).
- **STILL TODO (deferred by owner):** make **`Cross-toolchain hash aggregation`** a required
  status check (Settings → Branches → branch protection on `main`). If AArch64 ever diverges
  later: binary-search to the first divergent tick (add per-tick hashing in `src/replay`);
  SoftFloat is the fallback.

### 2. Broaden C++ det_math coverage
Currently the bit-exact C++ test uses a fixed 64-vector table. Add a randomized sweep
(seeded) comparing C++ vs the reference over each function's full SEADS domain, and grow
`tools/gen_detmath_vectors.py` ranges. Keep tolerances exact (bit-equal) for C++↔reference.

### 3. Complete the 8-aircraft tuning envelopes (data-only seals)
Only `data/tuning/envelopes/ki61.json` exists. Add the other 7 (P-47D, Bf 109 F-4, A6M2, Yak-3,
La-7, Spitfire Mk V, P-51). Each: ADR (`/seal` or data-only flow) + `tuning_probe.py` PASS +
optional `hash_sign_json.py`. Golden world_hash stays unchanged (data-only).

### 4. Wire the kernel to use envelopes + climb/bank inputs
Today the kernel ignores tuning (golden flies straight, φ=0, no climb). Feed `phi`, `tas`, and
climb commands from envelopes/inputs; extend `Kernel::step` (currently climb input hardcoded 0).
This changes dynamics, NOT the sealed golden (golden inputs are fixed) — but add new goldens for
turning/climbing scenarios and seal them.

### 5. Custom C++ renderer (post-core, decision 1A)
Thin raylib/SDL+bgfx client reading kernel state read-only, with render-interpolation between
100 Hz ticks. Renderer must never feed the sim. New `src/client/`. Needs a graphics lib added.

### 6. Netcode state-sync (multiplayer flight MVP)
Server-authoritative state synchronization: kernel both ends, predict own aircraft, interpolate
remotes ~100 ms, `world_hash` as desync tripwire, snapshots for correction/late-join. Build out
`src/net/` GEO-001 codec first (lat/lon×1e7, bearing×1e6, h×1e3, ZigZag+LEB128) + loopback tests.

### 7. (Post-MVP) Guns/projectiles — new seal, deferred per doctrine.

## Governance reminder
Any change to a **rail** (R, Δt, roster, ceiling, geometry, gravity, determinism bans, wire) or to
the golden world_hash REQUIRES a new seal: run `/seal` (or `/reseal` for value-only), write the
ADR + Forge audit card, and have Chronicler run `tools/make_receipt.py`. Never re-baseline the
golden silently.
