# SEADS 2026 — Next Steps (handoff)

> Resume doc for a fresh session. State as of seal **ATM-Sphere v1.4r0**, git `main` at the
> **client-side-prediction commit `7ff21ab`** (Step 6 layers 1–4b done) — pushed to `origin/main`,
> guardian CI **GREEN** (run [28349459869](https://github.com/cjcgervais/seads/actions/runs/28349459869):
> MSVC + GCC/Clang × x64/AArch64 reproduce all 4 goldens + the geo001/snapshot/lockstep/interp/predict
> parity vectors bit-for-bit). Clean slate — **start on Step 5 (renderer), §5 below**. Read `CLAUDE.md`
> first (the constitution — governance is now lean, §2). Background facts also live in Claude memory
> (`seads-canon`, `seads-harness`).
>
> ## ► START HERE (next task)
> **Steps 1–6 are DONE and Step 5 (renderer) now has a working first cut.** The deterministic
> core, the full netcode stack (layers 1–4b), AND a downstream renderer all exist. The renderer
> ships a kernel-driven **trajectory recorder** (`seads_record` → `.seadsrec` GEO-001/KIN-001 wire
> stream + `trajectory.js`), a pure unit-tested **client lib** (`globe`/`playback`, consuming the
> 4a interpolation), a **verified web globe viewer** (`src/client/web/`, screenshot-proven), and
> an optional **raylib viewer** (`-DSEADS_CLIENT=ON`, off in CI). All read-only, outside the
> `world_hash`, **no seal** (rides v1.4r0). See **§5**.
>
> The open tracks now are: **(A) Step 5 polish** — wire a *live* local-input loop into the viewer
> (the recorder/playback path is replay-only today), aircraft-model meshes + a chase/cockpit
> camera, and feed `seads_predict` (4b) so the OWN ship is predicted live rather than replayed; and
> **(B)** non-roadmap: replace the constant-TAS approximation with an energy/drag model (new seal,
> moves goldens). After that, Step 7 (guns/projectiles — new seal, post-MVP). See **§5/§6/§7**.

## Where things stand (DONE)

Deterministic core + governance harness up; bit-for-bit promise **proven in CI**; aircraft now
maneuver within their tuning envelopes. Roadmap steps **1, 2, 3, 4 are DONE**; **Step 5 (renderer)
has a working first cut** (recorder + pure client lib + web globe viewer + optional raylib viewer —
all downstream-only, ctest 7/7, no seal; §5); **Step 6 (netcode) is COMPLETE through layer 4b** — layer 1 (GEO-001 wire codec), layer 2 (20 Hz snapshot
serialization), layer 3 (loopback lockstep desync tripwire), layer 4a (remote interpolation
buffer), and **layer 4b (client-side prediction)** are all **DONE** (details in §6 below). Layer
4b carried a **Tier-1 reseal** (v1.3r0 → **v1.4r0**) to put `phi`/`tas` on the wire via the new
auxiliary **KIN-001** block. The multiplayer-flight MVP loop is complete. Step 5 (renderer)
remains the alternative visual track, and is now the natural consumer of the 4a interpolated
remote states + 4b predicted own state.

- **Remote:** `origin` = `https://github.com/cjcgervais/seads` (public). Single branch `main` (feature
  branches merged + deleted). `guardian.yml` is **green on `main` at `eb0dabd`**
  (run [28345458656](https://github.com/cjcgervais/seads/actions/runs/28345458656)) — MSVC + GCC +
  Clang × x64 + AArch64 reproduce **all 4 sealed goldens** bit-for-bit AND the new `seads_geo001_test`
  + `seads_snapshot_test` parity vectors byte-for-byte, with a per-golden cross-toolchain aggregation
  gate. (No `gh` CLI here; watch CI via the public Actions API — `curl -s
  ".../actions/runs/<id>/jobs"` — and use a GCM token from `git credential fill` for log downloads.)
- **Seal:** ATM-Sphere **v1.4r0** (v1.3r0 + KIN-001 wire reseal for prediction). Four goldens,
  all cross-toolchain-verified and **unchanged** by the reseal:
  - GOLDEN-SK-Sphere-001 (straight) `529c6a05…9218fe16` — unchanged since Pass 1
  - GOLDEN-SK-Turn-001 `6160540c…13f152ee` · Climb-001 `74b9d556…2d9b6682` · TurnClimb-001 `f7193b99…7cedd413`
- **Roster:** all 8 tuning envelopes exist (`data/tuning/envelopes/`); the kernel consumes them for
  bank/climb limits via `Kernel::step(cmd,env)`.
- **Gates:** all Python gates green; **52** Hypothesis property tests pass (incl. 7 geo001 + 7
  snapshot + 4 lockstep + 8 interp + 6 predict); det_math ≤2 ULP vs MPFR; C++ det_math + geo001 +
  snapshot byte-exact + lockstep/predict digest-exact + interp bit-exact vs reference; **10**
  generated headers in sync (`gen_*.py --check`). ctest is **6/6** under GCC + Clang (added
  `predict_equal`).
- **Netcode (Step 6):** `src/net/` holds the GEO-001 wire codec (`geo001.{h,cpp}`), the 20 Hz
  snapshot framing (`snapshot.{h,cpp}`, **now protocol 2** with the KIN section), and the remote
  interpolation buffer (`interp.{h,cpp}`) — all in the `seads_net` lib (pure transport: no
  det_math/kernel) — plus two **kernel-driving** libs: the loopback lockstep harness
  (`lockstep.{h,cpp}` → `seads_lockstep`) and the client-side prediction harness
  (`predict.{h,cpp}` → `seads_predict`), both linking `seads_kernel`+`seads_replay`. Each mirrors
  a Python reference (`geo001_ref`, `snapshot_ref`, `lockstep_ref`, `interp_ref`, `predict_ref`)
  with a generated-vector parity gate (`seads_{geo001,snapshot,lockstep,interp,predict}_test`).
- **Ledger:** receipts in `docs/receipts/` (latest `…v1.4r0-11e07d4.yml`), all `overall: PASS`.
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
  **Also set `git config --global --add safe.directory '*'`** — the D: filesystem doesn't record
  ownership, so git flags every nested repo as "dubious ownership". Without the wildcard, the
  renderer's `-DSEADS_CLIENT=ON` raylib **FetchContent** clone fails at the tag checkout (the clone
  lands but `git checkout 5.5` is refused). Already applied.
- PowerShell gotcha: do **not** redirect native-exe streams with `*>`/`2>&1` (wraps stderr as
  errors and aborts). Pipe stdout normally; use `| Out-Null` to silence. (For build logs, the Bash
  tool with `> log 2>&1` works fine — that's how the raylib build was captured.)

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

### 5. Custom renderer (post-core, decision 1A)  ✅ FIRST CUT DONE (2026-06-28, seal v1.4r0 — no seal)
Downstream-only renderer subsystem under **`src/client/`** (full map: `src/client/README.md`).
Built this session, all read-only / outside the `world_hash` (no rail/golden/kernel touched), gates
green, **lean ledger** (commit + receipt `…v1.4r0-*.yml`, no ADR per owner's call):
- **Recorder** `seads_record` (`record_main.cpp`) — drives the **real sealed kernel** and captures
  the flight as the exact 20 Hz **GEO-001/KIN-001** wire stream (protocol 2), written as a
  `.seadsrec` container (`seadsrec.{h,cpp}`) for the native viewer **and** a `trajectory.js` for the
  web viewer, both from the *same decoded* frames. Built-in 3-ship `--demo` (KI-61/Spitfire/Bf109,
  banking+climbing — bank auto-clamped to envelope `phi_max`) plus sealed-scenario replay (`--id`).
  Proves the layer 1/2/4a path end to end.
- **Pure client lib** `seads_client` — `globe.h` (sphere→cartesian, orbit camera, perspective
  project, hemisphere cull; libm OK, `lint_determinism` excludes `src/client`) + `playback.{h,cpp}`
  (decoded recording → `interp::SnapshotBuffer`, sampled ~100 ms in the past = the 4a delay).
- **Headless gate** `seads_client_test` — **ctest 7/7** under GCC+Clang (was 6/6; added
  `client_presentation`): globe invariants + `.seadsrec` round-trip + playback == layer-4a interp.
  Built in CI (no GPU); `SEADS_CLIENT` stays OFF so guardian never pulls a GUI lib.
- **Web globe viewer** `src/client/web/` — zero-build Three.js globe (CDN), JS interpolation
  mirroring `interp_ref` (linear lat/alt, shortest-arc lon/bearing, hold at edges), HUD +
  playback controls + orbit camera + the 8 km ceiling shell. **Screenshot-verified** rendering the
  3-ship demo. (`dt` clamp guards tab-background rAF leaps.)
- **Optional native viewer** `seads_viewer` (`viewer_main.cpp`, `-DSEADS_CLIENT=ON`) — raylib 3D
  globe, wall-clock render-interpolation, trails, HUD, orbit camera, `--selfcheck N` headless mode.
- **Remaining polish (track A):** live local-input loop (replay-only today) feeding `seads_predict`
  (4b) so the OWN ship is predicted live; aircraft-model meshes; chase/cockpit camera; vendor
  Three.js for fully-offline web. None touch a rail/golden.

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
- **Client-side prediction (layer 4b)**  ✅ DONE (2026-06-28, seal **v1.4r0** — Tier-1 reseal).
  Predict the OWN aircraft from local input each tick (running the **real sealed kernel**) and
  reconcile against authoritative snapshots: snap to authoritative @ server_tick, drop inputs ≤
  that tick, **replay** the buffered local inputs forward. `tools/predict_ref.py` is the canonical
  reference (`PREDICT-SK-001`: 1 own aircraft, 300 ticks, 20 Hz snapshots, ~100 ms lag);
  `src/net/predict.{h,cpp}` mirrors it in its **own** `seads_predict` lib (drives the kernel, like
  lockstep). Reconcile re-seeds via the public `Kernel::add()`+`step()` — kernel math + snapshot
  layout **untouched** (only additive read-only getters `Kernel::phi()/tas()`). Cross-impl parity
  is digest-exact: `gen_predict_vectors.py` → `predict_vectors.h`; `seads_predict_test` asserts
  seamless (predicted == truth every tick) + digest == reference + **heal control** (a 2⁻²⁰ m
  initial-alt desync diverges at tick 1, reconciles back at tick 15) + **negative control** (no
  reconcile ⇒ stays broken). 6 new Hypothesis tests (`tests/property/test_predict.py`; 44 → 52,
  incl. +2 snapshot for phi/tas). **The reseal:** GEO-001 stays geography-only; `phi`/`tas` ride a
  new auxiliary **KIN-001** block (phi×1e6, tas×1e3) as a 2nd self-delimiting snapshot section
  (`protocol 1→2`) — the chosen wire shape (vs extending the GeoPoint). The **bit-exact** path
  reconciles against CANONICAL state (layer-3 doctrine); the **lossy-wire** reseed (real
  remote/late-join) is bounded by the quantum (property test) — *that* is why phi/tas are on the
  wire. PASS under GCC + Clang (ctest 6/6); all 4 goldens unchanged (`529c6a05…` et al.). Ledger:
  ADR-Step6-Prediction-v1.4r0, Forge card, SEAL_CARD v1.4r0, receipt `…v1.4r0-11e07d4.yml`.
- The kernel itself should not change; if it must, that's a seal event — keep net code strictly
  outside the kernel boundary (no feeding bits back in). Ledger per layer (ADR + card + receipt).

### 7. (Post-MVP) Guns/projectiles — new seal, deferred per doctrine.

## Governance reminder (lean — see CLAUDE.md §2)
Governance is now minimal. **The whole law:** (1) keep the §4 gates green; (2) if a rail value or a
golden `world_hash` changes, bump the seal (`/seal` or `/reseal`) and say *why* in the commit
message — never silently; (3) keep the auto-receipt (`make_receipt.py`) and **this file** current —
that pair is the ledger and the continuity that actually matters; (4) ADRs/Forge cards are
**optional** (write one only for a genuine architectural fork). Everything else just rides the
current seal. The `validate_snapshot`/`validate_scenarios` gates fail loudly if anything moves a
hash, so trust the gates and ship.
