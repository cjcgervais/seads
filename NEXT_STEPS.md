# SEADS 2026 — Next Steps (handoff)

> Resume doc for a fresh session. State as of seal **ATM-Sphere v1.3r0**, git `main` at the
> **loopback-lockstep commit** (layer 3 done; clean — push and confirm **CI green**). Read
> `CLAUDE.md` first (the constitution). Background facts also live in Claude memory
> (`seads-canon`, `seads-harness`).
>
> ## ► START HERE (next task)
> **Step 6 layer 4b — client-side prediction (needs a rail reseal).** Layers 1–3 and **4a
> (remote interpolation)** are done. The remaining half is predicting your *own* aircraft forward
> from local input + reconciling against authoritative snapshots. Accurate dead-reckoning needs
> bank (`phi`) and speed (`tas`), which are **NOT** on the GEO-001 wire — carrying them is a
> **rail reseal**: run `/reseal` (new GEO-001 scales for phi/tas, or an auxiliary non-geographic
> block), write the ADR + Forge card + receipt, bump the seal. This is a **Tier-1** change (see
> CLAUDE.md §2) — decide the wire shape explicitly before coding. See **§6** below.

## Where things stand (DONE)

Deterministic core + governance harness up; bit-for-bit promise **proven in CI**; aircraft now
maneuver within their tuning envelopes. Roadmap steps **1, 2, 3, 4 are DONE**; **Step 6 (netcode)
is now IN PROGRESS** — layer 1 (GEO-001 wire codec), layer 2 (20 Hz snapshot serialization),
layer 3 (loopback lockstep desync tripwire), and layer 4a (remote interpolation buffer) are all
**DONE** (details in §6 below). The next pickup is **Step 6 layer 4b: client-side prediction**
(predict own aircraft from local input + reconcile from authoritative snapshots) — which needs a
**rail reseal** to put `phi`/`tas` on the wire. Step 5 (renderer) remains the alternative visual
track, and is now the natural consumer of the layer-4a interpolated remote states.

- **Remote:** `origin` = `https://github.com/cjcgervais/seads` (public). Single branch `main` (feature
  branches merged + deleted). `guardian.yml` is **green on `main` at `eb0dabd`**
  (run [28345458656](https://github.com/cjcgervais/seads/actions/runs/28345458656)) — MSVC + GCC +
  Clang × x64 + AArch64 reproduce **all 4 sealed goldens** bit-for-bit AND the new `seads_geo001_test`
  + `seads_snapshot_test` parity vectors byte-for-byte, with a per-golden cross-toolchain aggregation
  gate. (No `gh` CLI here; watch CI via the public Actions API — `curl -s
  ".../actions/runs/<id>/jobs"` — and use a GCM token from `git credential fill` for log downloads.)
- **Seal:** ATM-Sphere **v1.3r0**. Four goldens, all cross-toolchain-verified:
  - GOLDEN-SK-Sphere-001 (straight) `529c6a05…9218fe16` — unchanged since Pass 1
  - GOLDEN-SK-Turn-001 `6160540c…13f152ee` · Climb-001 `74b9d556…2d9b6682` · TurnClimb-001 `f7193b99…7cedd413`
- **Roster:** all 8 tuning envelopes exist (`data/tuning/envelopes/`); the kernel consumes them for
  bank/climb limits via `Kernel::step(cmd,env)`.
- **Gates:** all Python gates green; **44** Hypothesis property tests pass (incl. 7 geo001 + 5
  snapshot + 4 lockstep + 8 interp); det_math ≤2 ULP vs MPFR; C++ det_math + geo001 + snapshot
  byte-exact + lockstep digest-exact + interp bit-exact vs reference; **9** generated headers in
  sync (`gen_*.py --check`). ctest is **5/5** under GCC + Clang.
- **Netcode (Step 6) so far:** `src/net/` holds the GEO-001 wire codec (`geo001.{h,cpp}`), the
  20 Hz snapshot framing (`snapshot.{h,cpp}`), and the remote interpolation buffer
  (`interp.{h,cpp}`) — all in the `seads_net` lib (pure transport: no det_math/kernel) — plus the
  loopback lockstep harness (`lockstep.{h,cpp}`) in its **own** `seads_lockstep` lib (it drives the
  kernel, so it links `seads_kernel`+`seads_replay`). Each mirrors a Python reference
  (`geo001_ref.py`, `snapshot_ref.py`, `lockstep_ref.py`, `interp_ref.py`) with a generated-vector
  parity gate (`seads_geo001_test`, `seads_snapshot_test`, `seads_lockstep_test`, `seads_interp_test`).
- **Ledger:** receipts in `docs/receipts/` (latest `…v1.3r0-733b8a3.yml`), all `overall: PASS`.
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
python tools/geo001_ref.py                  # GEO-001 codec reference self-test
python tools/snapshot_ref.py                # GEO-001 snapshot reference self-test
python tools/lockstep_ref.py                # loopback lockstep reference self-test (+ negative control)
for g in gen_coeffs gen_golden_params gen_detmath_vectors gen_envelope_tables gen_scenario_params \
         gen_geo001_vectors gen_snapshot_vectors gen_lockstep_vectors; do \
  python tools/$g.py --check; done          # all 8 generated headers in sync
python -m pytest tests/property -q          # 36 pass (3 scenario sealed-hash + 7 geo001 + 5 snapshot + 4 lockstep)
python tools/make_receipt.py                # runs all gates (incl. geo001_codec + snapshot_codec) -> overall: PASS
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
# Net codec parity tests (also built by the same cmake): fastest full check is ctest.
ctest --test-dir build-gcc --output-on-failure    # 4/4: detmath, geo001, snapshot, lockstep
ctest --test-dir build-clang --output-on-failure   # 4/4 under Clang too
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

### 6. Netcode state-sync (multiplayer flight MVP)  ← IN PROGRESS (option B: recommended)
Server-authoritative state synchronization: kernel both ends, predict own aircraft, interpolate
remotes ~100 ms, `world_hash` as desync tripwire, snapshots for correction/late-join. Build out
`src/net/` GEO-001 codec first (lat/lon×1e7, bearing×1e6, h×1e3, ZigZag+LEB128) + loopback tests.
Suggested first moves for the next agent (build bottom-up, each layer gated before the next):
- **GEO-001 codec first, in isolation.**  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed).
  `src/net/geo001.{h,cpp}` (`seads_net` lib) mirrors `tools/geo001_ref.py` bit-for-bit: ZigZag +
  LEB128 over fixed-point i64, quantize/dequantize (round half away from zero), and a GeoPoint record
  in fixed field order (lat, lon, bearing, alt). Cross-impl parity gate mirrors det_math exactly:
  `gen_geo001_vectors.py` → `src/net/geo001_vectors.h` (integer-driven, byte-reproducible);
  `seads_geo001_test` asserts byte-identical wire on encode + exact round-trip on decode (307 i64,
  68 point, 15 quant). 7 new Hypothesis tests (`tests/property/test_geo001.py`; 20 → 27). Gates wired:
  guardian.yml (gen `--check` + reference self-test + `seads_geo001_test` per leg), make_receipt.py
  (`geo001_codec` gate). PASS under GCC + Clang locally (ctest 2/2); golden unchanged
  (`529c6a05…9218fe16`). Ledger: ADR-Step6-GEO001-Codec-v1.3r0, Forge card Step6, receipt
  `…v1.3r0-f2d7e94.yml`. **No seal** (implements the GEO-001 rail to spec; any *deviation* would reseal).
  *Note for next agent:* `seads_net` deliberately does NOT link det_math (no transcendentals; keep it
  off the sim-feeding path). Decoded wire values are lossy by quantization — never feed back as canonical.
- **Snapshot serialization** of `KernelState` over GEO-001 at the 20 Hz snapshot cadence.
  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed). `src/net/snapshot.{h,cpp}` (in `seads_net`)
  frames a world snapshot over the geo001 codec: header `(protocol, server_tick, n)` then
  `n × (id, GeoPoint)`, self-delimiting. `from_kernel` maps kernel **radians → wire degrees**
  (psi heading → GEO-001 bearing) via hex-float `RAD2DEG`. The canonical LE-f64 `Kernel::snapshot()`
  stays the world_hash source of truth; this wire snapshot is a separate **quantized** transport.
  Parity gate mirrors layer 1: `gen_snapshot_vectors.py` → `src/net/snapshot_vectors.h`;
  `seads_snapshot_test` byte-exact (4 snapshots, 5 conv) under GCC+Clang (ctest 3/3). 5 new
  Hypothesis tests (`tests/property/test_snapshot.py`; 27 → 32). Gates wired (guardian.yml,
  make_receipt `snapshot_codec`). Ledger: ADR-Step6-Snapshot-Serialization-v1.3r0, Forge card,
  receipt `…v1.3r0-13726a4.yml`. **Field-scope deferral (documented, not silent):** `phi`/`tas`
  are NOT on the wire — GEO-001 has no scale for them, so adding them is a reseal. Layer 2 supports
  remote **interpolation**; full prediction (bank/energy) awaits a later layer.
- **Loopback lockstep harness (layer 3)**  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed).
  Two in-process kernels stepped from one shared input timeline produce an identical per-tick
  `world_hash` every tick — the desync tripwire. `tools/lockstep_ref.py` is the canonical reference
  (inline `LOCKSTEP-SK-001` scenario: 3 aircraft / 600 ticks / turns+climbs+ceiling predamp; two ref
  kernels, shared timeline, per-tick canonical hash, stop at first divergence, **negative control**
  proving the tripwire trips). `src/net/lockstep.{h,cpp}` mirrors it (`tick_hash`,
  `apply_inputs(a,b,cmd,env,tick)`, `run(...)`). Cross-impl parity is proven **exhaustively**: a
  SHA-256 digest over *every* per-tick hash (+ per-tick checkpoints) must match the reference —
  `gen_lockstep_vectors.py` → `src/net/lockstep_vectors.h` (self-contained hex-float scenario, byte-
  reproducible); `seads_lockstep_test` asserts in-sync + digest==reference + tripwire-trips. 4 new
  Hypothesis tests (`tests/property/test_lockstep.py`; 32 → 36). Gates wired: guardian.yml (gen
  `--check` + reference self-test + `seads_lockstep_test` per leg), make_receipt.py (`lockstep` gate).
  PASS under GCC + Clang locally (ctest 4/4); all 4 goldens unchanged (`529c6a05…9218fe16` et al.).
  Ledger: ADR-Step6-Lockstep-v1.3r0, Forge card Step6-Lockstep, receipt `…v1.3r0-8d5fad8.yml`.
  *Key boundaries:* the tripwire compares the **canonical** hashing snapshot (`Kernel::snapshot()`,
  raw LE f64), NOT the lossy GEO-001 wire; the timeline carries sim `Command`s (bank/climb), never
  wire bits; snapshot byte layout is untouched (only hashed per tick). **Deviation from this doc's
  earlier suggestion (documented):** lockstep is its **own** lib `seads_lockstep` (not folded into
  `seads_net`) because it drives the kernel — folding it in would make the pure wire-codec lib pull
  det_math+kernel transitively. See ADR §4.
- **Remote interpolation buffer (layer 4a)**  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed).
  Client-side, downstream-only: a buffer of decoded GEO-001 snapshots that interpolates remote
  aircraft at a render time ~100 ms in the past (smooth motion despite 20 Hz / jitter / loss).
  `tools/interp_ref.py` is the canonical reference (`SnapshotBuffer.sample(render_tick)`; LINEAR
  lat/alt, SHORTEST-ARC lon across the antimeridian + bearing across 360°, CLAMP/HOLD at edges).
  `src/net/interp.{h,cpp}` mirrors it in `seads_net` (pure IEEE +−×÷, no det_math/kernel — so it
  reproduces **bit-for-bit** cross-toolchain under the strict-FP/no-FMA flags, like the codecs).
  `gen_interp_vectors.py` → `src/net/interp_vectors.h` (10 cases, hex-float, byte-reproducible);
  `seads_interp_test` asserts bit-exact vs reference. 8 new Hypothesis tests
  (`tests/property/test_interp.py`; 36 → 44). Gates wired: guardian.yml + make_receipt.py
  (`interp` gate). PASS under GCC + Clang (ctest 5/5); all 4 goldens unchanged. Tier-2 ledger
  (CLAUDE.md §2): commit message + receipt `…v1.3r0-733b8a3.yml`, no ADR/seal. *Uses only
  lat/lon/bearing/alt — all on the wire today, so NO reseal.* It NEVER feeds the sim (downstream
  presentation only). Step 5's renderer is the natural consumer.
- **Client-side prediction (layer 4b)** ← **NEXT PICKUP — needs a rail reseal (Tier 1).** Predict
  your OWN aircraft forward from local input each frame and reconcile against authoritative
  snapshots (snap to authoritative; replay local input from the corrected base). Accurate
  dead-reckoning needs bank (`phi`) and speed (`tas`), which are **NOT** on the GEO-001 wire
  (layer-2 field-scope deferral). Carrying them is a **rail reseal**: run `/reseal` (new GEO-001
  scales for phi/tas, or an auxiliary non-geographic block), write the ADR + Forge card + receipt,
  bump the seal. Decide the wire shape explicitly first; don't smuggle new wire fields in under the
  current seal. `world_hash` from layer 3 remains the desync tripwire; late-join reuses the
  snapshot transport. Its own gated layer + Tier-1 ledger entry.
- The kernel itself should not change; if it must, that's a seal event — keep net code strictly
  outside the kernel boundary (no feeding bits back in). Ledger per layer (ADR + card + receipt).

### 7. (Post-MVP) Guns/projectiles — new seal, deferred per doctrine.

## Governance reminder (tiered — see CLAUDE.md §2)
Governance is scaled to risk now (solo, agent-built project). **Two tiers:**
- **Tier 1 (full ritual):** a change to a **rail** (R, Δt, roster, ceiling, geometry, gravity,
  determinism bans, wire), to `det_math`/the kernel/the canonical snapshot layout, or anything that
  **moves a golden world_hash** → new seal (`/seal`, or `/reseal` for a value-only rail change) +
  ADR + Forge card + receipt. Never re-baseline a golden silently.
- **Tier 2 (lightweight, the common case):** net layers, tooling, tests, renderer, docs, data-only
  tuning that **can't move a golden** → gates green + a clear commit message + the auto-generated
  receipt. ADR optional (write one only for a genuinely architectural decision). Layers 1–3 of
  Step 6 were Tier-2-shaped; the heavy ADR/card on layer 3 was more than that tier requires.
- **The backstop:** `make_receipt.py`'s `validate_snapshot`/`validate_scenarios` gates fail loudly
  if a "Tier 2" change actually moved a hash — forcing it up to Tier 1. Trust the gates.
