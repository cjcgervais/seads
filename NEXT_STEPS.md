# SEADS 2026 — Next Steps (handoff)

> Resume doc for a fresh session. State as of seal **ATM-Sphere v1.3r0**, git `main` (clean, pushed).
> Read `CLAUDE.md` first (the constitution). Background facts also live in Claude memory
> (`seads-canon`, `seads-harness`).

## Where things stand (DONE)

Deterministic core + governance harness up; bit-for-bit promise **proven in CI**; aircraft now
maneuver within their tuning envelopes. Roadmap steps **1, 2, 3, 4 are DONE** (details in the
numbered sections below). The recommended next pickup is now **Step 5 (renderer)** or **Step 6
(netcode)**.

- **Remote:** `origin` = `https://github.com/cjcgervais/seads` (public). `guardian.yml` is **green on
  `main`** — MSVC + GCC + Clang × x64 + AArch64 reproduce **all 4 sealed goldens** bit-for-bit, with a
  per-golden cross-toolchain aggregation gate. (No `gh` CLI here; watch CI via the public Actions API,
  and use a GCM token from `git credential fill` for log downloads.)
- **Seal:** ATM-Sphere **v1.3r0**. Four goldens, all cross-toolchain-verified:
  - GOLDEN-SK-Sphere-001 (straight) `529c6a05…9218fe16` — unchanged since Pass 1
  - GOLDEN-SK-Turn-001 `6160540c…13f152ee` · Climb-001 `74b9d556…2d9b6682` · TurnClimb-001 `f7193b99…7cedd413`
- **Roster:** all 8 tuning envelopes exist (`data/tuning/envelopes/`); the kernel consumes them for
  bank/climb limits via `Kernel::step(cmd,env)`.
- **Gates:** all Python gates green; **20** Hypothesis property tests pass; det_math ≤2 ULP vs MPFR;
  C++ det_math bit-exact vs reference; 5 generated headers in sync (`gen_*.py --check`).
- **Ledger:** receipts in `docs/receipts/` (latest `…v1.3r0-8b85a32.yml`), all `overall: PASS`.
- **Deferred (owner's call, not blocking):** (a) make `Cross-toolchain hash aggregation` a **required
  status check** on `main` (branch protection — needs a PAT or `gh`); (b) `hash_sign_json.py` signing
  of the 8 envelopes.

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
for g in gen_coeffs gen_golden_params gen_detmath_vectors gen_envelope_tables gen_scenario_params; do \
  python tools/$g.py --check; done          # all 5 generated headers in sync
python -m pytest tests/property -q          # 20 pass (incl. 3 scenario sealed-hash)
python tools/make_receipt.py                # runs all gates (incl. 3 scenario goldens) -> overall: PASS
```
```powershell
# C++ side (PATH set as above). Builds seads_golden (Sphere) + seads_scenario (Turn/Climb/TurnClimb).
cmake -S . -B build-gcc   -G Ninja -DCMAKE_CXX_COMPILER=g++     -DCMAKE_BUILD_TYPE=Release; cmake --build build-gcc
cmake -S . -B build-clang -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release; cmake --build build-clang
.\build-gcc\seads_golden.exe   --out run_gcc.bin
python tools\validate_snapshot.py --golden tests\golden\GOLDEN-SK-Sphere-001\expected.world_hash --candidate run_gcc.bin
foreach ($id in "GOLDEN-SK-Turn-001","GOLDEN-SK-Climb-001","GOLDEN-SK-TurnClimb-001") {
  .\build-gcc\seads_scenario.exe --id $id --out run_scen.bin
  python tools\validate_snapshot.py --golden "tests\golden\$id\expected.world_hash" --candidate run_scen.bin }
# (repeat the two scenario/validate blocks with build-clang to confirm cross-compiler parity)
```

## Next steps — pick up here (in priority order)

> Steps **1/2/3/4 are DONE.** The two remaining roadmap pushes are **Step 5 (renderer)** and
> **Step 6 (netcode)** — both are larger, multi-session efforts. **Recommendation: Step 6 (netcode)**
> if the goal is the multiplayer-flight MVP — it stays inside the deterministic/headless world this
> harness is built for, reuses the kernel + `world_hash` directly (as a desync tripwire), and needs no
> new external dependency. Pick **Step 5 (renderer)** instead if you want something to *look at* first;
> it's lower-stakes (read-only, can never break determinism) but pulls in a graphics lib. A non-roadmap
> option worth a seal someday: replace the constant-TAS approximation (step 4) with an energy/drag
> model. Whatever you pick that touches a rail or a golden hash → follow the seal ritual (Governance
> reminder at the bottom). Neither Step 5 nor Step 6 changes a rail or a golden by itself.

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

### 2. Broaden C++ det_math coverage  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed)
`tools/gen_detmath_vectors.py` rewritten: per-function **structured boundary rows** (Cody-Waite
quadrant edges k·π/2, fdlibm atan regions 7/16·11/16·19/16·39/16, asin ±1 clamp, atan2 axis cases,
wrap edges ±k·2π, tiny/large magnitudes) **+ 512 seeded random rows/group** (integer SplitMix64 +
`+−*/` only → header is byte-reproducible cross-platform, so CI `--check` stays green). Domains
mirror `det_math_oracle.py`. **64 → 4878 vectors**, tolerance exact (bit-equal). Added previously-
**uncovered** `det_atan2`, `wrap_pi`, `wrap_2pi` to the parity gate (all live kernel calls,
`kernel.cpp:29,66`). `detmath_vectors.h` `Vec` gained a `double y` second-operand column;
`detmath_test_main.cpp` dispatcher extended. **PASS** under GCC + Clang locally; golden hashes
unchanged (no kernel/det_math/reference edit). Ledger: ADR-Step2-DetMathParity-v1.3r0, Forge card
Step2, receipt `…v1.3r0-d5e5e44.yml`. CI (guardian.yml L25 `--check`, L102 runs `seads_detmath_test`
per leg) exercises all 4878 vectors cross-toolchain/cross-arch on push.
- To re-tune breadth: change `N_RANDOM` (or a function's sub-ranges) in `gen_detmath_vectors.py`,
  regenerate, rebuild. Mirrors determinism rules (det_math-only, no FMA, hex-float constants).

### 3. Complete the 8-aircraft tuning envelopes (data-only)  ✅ DONE (2026-06-28)
All 8 roster envelopes now exist under `data/tuning/envelopes/` (p47d, bf109f4, a6m2, yak3, la7,
spitfire_mk5, p51 + existing ki61). Data-only — golden unchanged (`529c6a05…9218fe16`), seal v1.2r0.
Ledger: `docs/adr/ADR-roster-envelopes-v1.2r0.md`, `docs/cards/FORGE_AUDIT_CARD-roster-envelopes-v1.2r0.md`,
receipt `docs/receipts/receipt-ATM-Sphere_v1.2r0-0a7a258.yml` (overall PASS). CI green (run 28341209641);
`tuning_probe.py` validates all 8; `make_receipt.py` tuning gate now globs all envelopes (was ki61-only).
- Values are an initial balance pass (relative airframe strengths; retuning later stays data-only).
- Optional follow-up: `hash_sign_json.py` to sign each envelope (not yet applied).

### 4. Wire the kernel to use envelopes + climb/bank inputs  ✅ DONE (2026-06-28, seal v1.3r0)
`Kernel::step(cmd,env)` now banks toward a commanded angle at the envelope roll_rate (clamped to
phi_max(TAS)) and climbs at a commanded rate (clamped to the envelope band + ceiling predamp).
Straight golden preserved byte-for-byte via shared `advance_(i,req)`; Sphere hash unchanged. Three
new sealed goldens with scripted-timeline inputs: GOLDEN-SK-{Turn,Climb,TurnClimb}-001
(`config/scenarios/*.json` → `tests/golden/<id>/`). Runner `seads_scenario --id <id>`. Generators
`gen_envelope_tables.py`/`gen_scenario_params.py` → `src/kernel/{envelope_tables,scenario_params}.h`
(+ `flight_types.h`); `lut_eval` shared bit-identical in detmath_ref.py + kernel.cpp (no new det_math).
Guardian green for all 4 goldens × MSVC/GCC/Clang × x64/AArch64 (run 28342235633). Ledger:
ADR-Step4-Scenarios-v1.3r0, Forge card, SEAL_CARD v1.3r0, receipt -8b85a32.
- TAS held constant (no energy/drag model) — a documented step-4 approximation to revisit in a later seal.
- New goldens use scripted step-function schedules; richer maneuver scripting can extend the schema.

### 5. Custom C++ renderer (post-core, decision 1A)  ← next pickup (option A: visual)
Thin raylib/SDL+bgfx client reading kernel state read-only, with render-interpolation between
100 Hz ticks. Renderer must never feed the sim. New `src/client/`. Needs a graphics lib added.
**Hard rule:** the renderer is downstream-only — it reads a snapshot/`KernelState` and never writes
back into sim state, never advances a tick, never seeds RNG the kernel sees. It lives **outside** the
determinism gate, so it does **not** go through `det_math` and does **not** affect any `world_hash`
(no seal). Suggested first moves for the next agent:
- Add the graphics dep behind an **optional** CMake switch (e.g. `option(SEADS_CLIENT "build the
  renderer" OFF)`) so `guardian.yml` and the headless gates never pull a GUI lib — keep CI kernel-only.
  raylib is the least-friction choice (single dep, trivial CMake `FetchContent`).
- Expose a read-only accessor on `Kernel` (positions/bearings/φ for each aircraft) — do **not** widen
  the canonical snapshot used for hashing; add a separate view struct if needed.
- Render-interpolate between ticks using a wall-clock alpha **in the client only** (wall-clock is
  banned *inside* the kernel; the client is allowed it). Map sphere (lat,lon,h) → a camera/globe view.
- Drive it from a golden replay first (feed `seads_scenario` output) before any live input loop.
- Ledger: ADR + Forge card + receipt; no rail/golden touched, so no seal. Determinism gate unaffected.

### 6. Netcode state-sync (multiplayer flight MVP)  ← next pickup (option B: recommended)
Server-authoritative state synchronization: kernel both ends, predict own aircraft, interpolate
remotes ~100 ms, `world_hash` as desync tripwire, snapshots for correction/late-join. Build out
`src/net/` GEO-001 codec first (lat/lon×1e7, bearing×1e6, h×1e3, ZigZag+LEB128) + loopback tests.
Suggested first moves for the next agent (build bottom-up, each layer gated before the next):
- **GEO-001 codec first, in isolation.** New `src/net/geo001.{h,cpp}`: ZigZag+LEB128 encode/decode for
  the quantized fields (lat/lon×1e7 as i64, bearing×1e6, h×1e3). Mirror it in a Python reference
  (`tools/geo001_ref.py`) the same way det_math is mirrored, and add a **round-trip + cross-impl
  parity** gate (encode in C++, decode in Python and vice-versa; byte-identical wire). This is the
  natural, self-contained next deliverable and is **no-seal** (wire format is already a sealed rail —
  GEO-001 — so *implementing* it to spec rather than changing it does not reseal; but any deviation
  from the GEO-001 rail *would* need a seal, so match the rail exactly).
- **Snapshot serialization** of `KernelState` over GEO-001 at the 20 Hz snapshot cadence (physics stays
  100 Hz). Keep the canonical little-endian hashing snapshot as the source of truth; the wire snapshot
  is a separate, quantized transport.
- **Loopback harness**: two in-process kernels stepped in lockstep from the same inputs must keep
  identical `world_hash` every tick (desync tripwire). Then add prediction (own aircraft) +
  remote interpolation (~100 ms buffer) + correction from authoritative snapshots.
- The kernel itself should not change; if it must, that's a seal event — keep net code strictly
  outside the kernel boundary (no feeding bits back in). Ledger per layer (ADR + card + receipt).

### 7. (Post-MVP) Guns/projectiles — new seal, deferred per doctrine.

## Governance reminder
Any change to a **rail** (R, Δt, roster, ceiling, geometry, gravity, determinism bans, wire) or to
the golden world_hash REQUIRES a new seal: run `/seal` (or `/reseal` for value-only), write the
ADR + Forge audit card, and have Chronicler run `tools/make_receipt.py`. Never re-baseline the
golden silently.
