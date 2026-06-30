# SEADS 2026 — Next Steps (handoff)

> ## ►► CURRENT STATE (2026-06-30): seal **ATM-Sphere v1.9r0** — **Step 7 guns / G1 (ballistic projectiles) DONE ✅**
> The flight-model arc B1→B4 is complete and sealed; the owner chose **guns** as the next milestone, with
> projectiles as **deterministic ballistics in the kernel** (not hit-scan). **G1 ships the ballistic
> substrate.** A fired round is a **point mass on the same sphere = the n=0 (no lift) / thrust=0
> specialization of the aircraft 3-DOF step**: gravity bends its flight-path angle down, a lumped
> quadratic drag (`Vdot −= k·V²`, k=2e-4) bleeds speed, the heading is fixed (the closed-form great-circle
> step carries the geodesic), and each round has an integer **time-to-live** (250 ticks) and despawns on
> ttl OR ground contact. So it reuses `det_sin`/`det_cos`/`great_circle_step` and adds **ZERO new det_math**
> (no MPFR-oracle work, no FMA/AArch64 risk — the single biggest reason to model rounds this way). `Command`
> gains a **`bool fire`** trigger (one round/fired-tick in G1, a placeholder rate); a round inherits the
> firer's **post-step** muzzle state + `tas+850` and an `owner` index, and does NOT advance on its spawn
> tick. Rounds are **canonical kernel state**: the snapshot gains a trailing **projectile block** (`u32 n`,
> `u32 pad`, then per round `6×f64 [lat,lon,psi,alt,tas,gamma] + u32 ttl + u32 owner`), **always present
> (n=0 for gun-less scenarios)**. G1 constants are **global, in code** (like RHO0/V_MIN), not rails.
> **No wire change** (KIN-002 / protocol 3 unchanged — round wire transport is DEFERRED, like phi/tas were
> pre-4b). **Because the block is appended to EVERY snapshot, all 7 prior goldens move — INCLUDING Sphere
> this time** (the no-arg path's snapshot grew too; trajectory byte-identical) — plus new
> **GOLDEN-SK-Gunfire-001** (p47d banked-climb 20-round burst + a6m2 dive 15-round burst → **35 rounds
> airborne & hashed** at tick 300). **12/12** receipt gates PASS, **8/8 goldens C++≡Python bit-for-bit
> under GCC+Clang**, **ctest 7/7** both compilers, **81** property tests (+9 `test_projectile.py`). New
> envelope referenced by a scenario (a6m2) ⇒ `envelope_tables.h` now emits 6 of 8. **NEXT: G2 — hit
> detection (round vs aircraft on the sphere) + damage model (new seal ~v1.10r0); then G3 — per-airframe
> weapon roster + fire-rate (data reseal).** `owner` is already carried so G2 needs no projectile-format reseal.
> Goldens (v1.9r0 — all moved; the projectile block, n=0 for the 7 non-gun goldens, is appended to every snapshot):
> - **Sphere `c7d3a290…d80f8838` (moved: n=0 block appended; traj identical)** · Turn `ccbc2497…b4de8044` · Climb `35d75d42…73ba1c3a`
> - TurnClimb `3e870e50…6e396195` · Accel `1e9a425a…bff87f6` · Pitch `dad0a1ed…786b7da6` · Stall `3137992c…c27eff2f`
> - **Gunfire (NEW) `594be08a…bbe9a8b3`** — 35 ballistic rounds airborne at tick 300
> Ledger: ADR-Step7-Guns-G1-Projectiles-v1.9r0, SEAL_CARD v1.9r0, receipt `…v1.9r0-*.yml`, rails version 180→190
> (+ a descriptive `weapons` block), guardian.yml **edited** (Gunfire added to all 3 golden ID lists).
> Files: `kernel.{h,cpp}` (projectile SoA + `advance_projectiles_`/`spawn_projectile_` + snapshot block),
> `flight_types.h` (`Command.fire`), `ref_kernel.py` (Projectile + mirror), `gen_scenario_params.py`
> (Phase.fire) + `scenario_main.cpp`, `config/scenarios/GOLDEN-SK-Gunfire-001.json`, `tests/property/test_projectile.py`.
> **GIT: _pending commit_ (see Auditor-pass + commit step). Was at `1b6d2a7` (v1.8r0 landed) before this.**
>
> **Remaining roadmap after G1:** guns **G2** (hit/damage — seal) → **G3** (per-airframe armament — data reseal);
> renderer polish (draw rounds; meshes; vendored Three.js; offline web — all **no-seal**); optional **B5**
> (ISA atmosphere; rail + seal) still deferred. The sealed flight-model arc **B1→B4 is COMPLETE**.
>
> _(Prior: v1.6r0 B2 lift & pitch — γ stored state, KIN-002 wire reseal, all goldens regenerated + Pitch;
> committed + pushed, guardian green on `12a1830` run 28392491160.)_
>
> ---
>
> Resume doc for a fresh session. (Pre-B2 history below.) State was seal **ATM-Sphere v1.4r0**, git `main` at the
> **Step 5 renderer commit `e4ef652`** (Steps 1–6 done + a working downstream renderer) — pushed to
> `origin/main`, guardian CI **GREEN** (run [28355716358](https://github.com/cjcgervais/seads/actions/runs/28355716358):
> MSVC + GCC/Clang × x64/AArch64 reproduce all 4 goldens + the geo001/snapshot/lockstep/interp/predict
> parity vectors bit-for-bit **and now the client-presentation test on every leg**). **The Track A
> live-input loop is now DONE** (the native viewer's `--fly` mode flies the own ship through
> `seads_predict`); **next: Step 5 polish (meshes / chase cam / offline web) or track B (energy/drag
> model) — see START HERE + §5/§6.** Read `CLAUDE.md` first (the constitution — governance is now
> lean, §2). Background facts also live in Claude memory (`seads-canon`, `seads-harness`).
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
> **Track A (a live local-input loop) is DONE ✅ (2026-06-29, no seal).** The native viewer's new
> **`--fly`** mode (`src/client/viewer_main.cpp`, `-DSEADS_CLIENT=ON`) flies the OWN ship from live
> keyboard input (A/D → `target_phi`, W/S → `target_climb`) through the already-built
> **`seads_predict`** (`predict::Predictor`, layer 4b) at a fixed 100 Hz from wall-clock, while
> remotes stay on the `Playback`/interp path (layer 4a) — prediction (own) + interpolation (remote)
> on the same globe at once, the full 4a+4b loop finally visible. Headless proof:
> `seads_viewer --fly --selfcheck 6` (no GPU/recording needed) drives the real sealed kernel and
> prints the own state. Downstream-only: input feeds `Command`s into the kernel-driving Predictor,
> never the wire; no rail/golden/`world_hash` touched → no seal; ctest 7/7, golden
> `529c6a05…9218fe16` unchanged, receipt `…v1.4r0-89a1974.yml`. CMake now links `seads_predict` +
> kernel/replay/det_math into `seads_viewer` (guarded by `SEADS_CLIENT`, so CI is untouched).
> *(Note: this single-process viewer has no authoritative server, so only `Predictor::predict()`
> runs in `--fly`; `reconcile()` (snap+replay) stays exercised by the layer-4b parity tests and
> engages against a real server.)*
>
> **Chase cam + flight-control scheme is now DONE ✅ (2026-06-29, no seal)** — see §5. The `--fly`
> viewer now rides a **chase camera** behind/above the own ship (follows heading, wheel zooms),
> with **mouse-aim** (a central reticle drives bank/climb) as the default and **hold-SPACE
> free-look** (mouse pans freely around the plane; A/D bank, W/S pitch, Q/E yaw, Shift/Ctrl
> throttle by keyboard; release restabilizes). P pauses, R resets. Yaw + throttle are applied
> DOWNSTREAM by re-seeding the predictor (the kernel has no such axis). All presentation-only;
> golden `529c6a05…` unchanged, 12/12 gates + ctest 7/7 green.
>
> ### ⇒ Flight model Track B: **B1 DONE ✅ (v1.5r0) · B2 DONE ✅ (v1.6r0) · B3 DONE ✅ (2026-06-29, seal v1.7r0)**. NEXT: **§8.4 Phase B4 (per-airframe aero retune — mostly data)**.
> **B1 (longitudinal energy) is shipped and sealed.** TAS is now a real integrated state: thrust −
> drag (parasitic + induced from the bank load factor) − climb cost. `Command` carries **throttle
> [0,1]**; per-airframe aero params (mass, S, cd0, k, T₀, V_max) live on the envelope. Energy lives
> in `step(cmd,env)` only, so **GOLDEN-SK-Sphere-001 is unchanged** (kinematic anchor) while
> Turn/Climb/TurnClimb regenerated + new **GOLDEN-SK-Accel-001**; C++≡Python bit-for-bit under
> GCC+Clang, 12/12 gates + ctest 7/7 + 57 property tests green, receipt `…v1.5r0-*.yml`. The viewer's
> Shift/Ctrl is now a **real throttle** (re-seed hack retired). Details in **§8.1**.
>
> **What's still a band-aid (fixed by B3):** **pitch is now REAL** (B2 — γ is a kernel state, the
> viewer marker tilts by the true γ, W/S = real g-command, the pitch-cue exaggeration is gone). What
> remains: **no stall / unbounded C_L** — `n` is only globally clamped to `[-3,9]` (placeholder);
> per-airframe `C_Lmax`, accelerated stall, structural-g and the corner speed are **B3**. The
> **vertical singularity** (`ψ̇` has `cosγ` in the denominator → undefined at γ=±90°) is documented,
> not handled — scenarios/viewer stay well inside ±90°; full loops/verticals need a different
> formulation (post-B3). **Yaw (Q/E) in the viewer remains a downstream re-seed** by design (the
> coordinated-flight kernel has no independent yaw axis). Every phase stays a seal with the full
> ritual (det_math+MPFR if needed, Auditor, ADR, `/seal`). **Start at §8.3.**
>
> **Smaller no-seal alternatives if you're not ready for core work:** Track A stretch — aircraft
> **meshes** (vs the marker sphere) and **vendoring Three.js** for offline web (§5). **Step 7**
> (guns/projectiles) is a *new* seal, best deferred until the flight model lands. Whatever you pick,
> run the §4 gates first to confirm the green baseline, then again before committing.

## Where things stand (DONE)

Deterministic core + governance harness up; bit-for-bit promise **proven in CI**; aircraft now
maneuver within their tuning envelopes. Roadmap steps **1, 2, 3, 4 are DONE**; **Step 5 (renderer)
has a working first cut** (recorder + pure client lib + web globe viewer + optional raylib viewer —
all downstream-only, ctest 7/7, no seal; §5); **Step 6 (netcode) is COMPLETE through layer 4b** — layer 1 (GEO-001 wire codec), layer 2 (20 Hz snapshot
serialization), layer 3 (loopback lockstep desync tripwire), layer 4a (remote interpolation
buffer), and **layer 4b (client-side prediction)** are all **DONE** (details in §6 below). Layer
4b carried a **Tier-1 reseal** (v1.3r0 → **v1.4r0**) to put `phi`/`tas` on the wire via the new
auxiliary **KIN-001** block. The multiplayer-flight MVP loop is complete. **Step 5 (renderer) now
has a working first cut** — the downstream consumer of the 4a interpolated remote states (the 4b
predicted own state is the next polish step). See §5.

- **Remote:** `origin` = `https://github.com/cjcgervais/seads` (public). Single branch `main` (feature
  branches merged + deleted). `guardian.yml` is **green on `main` at `e4ef652`**
  (run [28355716358](https://github.com/cjcgervais/seads/actions/runs/28355716358)) — MSVC + GCC +
  Clang × x64 + AArch64 reproduce **all 4 sealed goldens** bit-for-bit AND the
  `seads_{geo001,snapshot,lockstep,interp,predict,client}_test` parity/presentation tests, with a
  per-golden cross-toolchain aggregation
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
python -m pytest tests/property -q          # 64 pass (scenario/energy/pitch + geo001 + snapshot + lockstep + interp + predict)
python tools/make_receipt.py                # runs all 12 gates -> overall: PASS (writes docs/receipts/...yml)
```
```powershell
# C++ side (PATH set as above). Builds seads_golden (Sphere) + seads_scenario (Turn/Climb/TurnClimb).
cmake -S . -B build-gcc   -G Ninja -DCMAKE_CXX_COMPILER=g++     -DCMAKE_BUILD_TYPE=Release; cmake --build build-gcc
cmake -S . -B build-clang -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release; cmake --build build-clang
.\build-gcc\seads_golden.exe   --out run_gcc.bin
python tools\validate_snapshot.py --golden tests\golden\GOLDEN-SK-Sphere-001\expected.world_hash --candidate run_gcc.bin
foreach ($id in "GOLDEN-SK-Turn-001","GOLDEN-SK-Climb-001","GOLDEN-SK-TurnClimb-001","GOLDEN-SK-Accel-001","GOLDEN-SK-Pitch-001") {
  .\build-gcc\seads_scenario.exe --id $id --out run_scen.bin
  python tools\validate_snapshot.py --golden "tests\golden\$id\expected.world_hash" --candidate run_scen.bin }
# (repeat the two scenario/validate blocks with build-clang to confirm cross-compiler parity)
# Net codec + client parity tests (also built by the same cmake): fastest full check is ctest.
ctest --test-dir build-gcc --output-on-failure    # 7/7: detmath, geo001, snapshot, lockstep, interp, predict, client
ctest --test-dir build-clang --output-on-failure   # 7/7 under Clang too

# OPTIONAL — the renderer (Step 5). Record a flight, then view it:
.\build-gcc\seads_record.exe --demo --out flight.seadsrec --js src\client\web\trajectory.js
#   web viewer:    open src\client\web\index.html  (trajectory.js sits beside it; file:// works)
#   native viewer: cmake -S . -B build-client -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DSEADS_CLIENT=ON
#                  cmake --build build-client --target seads_viewer
#                  .\build-client\seads_viewer.exe flight.seadsrec            # GUI
#                  .\build-client\seads_viewer.exe flight.seadsrec --selfcheck 6   # headless, no GPU
```

## Next steps — pick up here (in priority order)

> Steps **1/2/3/4 DONE**, **Step 6 (netcode) DONE through layer 4b**, **Step 5 (renderer) first cut
> DONE** (§5). The entries below (§1–§6) are the **history** of what shipped — read them for context
> on how each layer was built and gated. **The actual next task is in `► START HERE` at the top:**
> recommended Track A (a live local-input loop feeding `seads_predict` so the own ship is *flown*,
> not replayed — closes the visual MVP, no seal). Alternatives: Track B (energy/drag model — a real
> reseal that moves all 4 goldens) or Step 7 (guns). Anything touching a rail or a golden hash →
> follow the seal ritual (`/seal` or `/reseal`; Governance reminder at the bottom). The renderer
> tracks don't touch a rail or a golden.

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
- **Live local-input loop (track A)**  ✅ DONE (2026-06-29, no seal). The native viewer's `--fly`
  mode flies the OWN ship from keyboard input through `predict::Predictor` (layer 4b) at a fixed
  100 Hz wall-clock step; remotes stay on the layer-4a interp path — prediction + interpolation on
  one globe. Input maps A/D→`target_phi`, W/S→`target_climb` into a `seads::Command` (kernel
  re-clamps to the envelope). Headless `--fly --selfcheck N` proves the input→prediction path with
  no GPU/recording. `seads_viewer` now links `seads_predict`+kernel/replay/det_math (guarded by
  `SEADS_CLIENT`, CI untouched). ctest 7/7; golden `529c6a05…` unchanged; receipt `…-89a1974.yml`.
- **Chase camera + flight-control scheme (track A stretch)**  ✅ DONE (2026-06-29, no seal).
  `src/client/viewer_main.cpp` `run_fly` now builds a **chase camera** from the own ship's local
  tangent frame (`local_basis` at lat/lon + heading psi → forward; eye placed behind/above along
  `-forward`, offset by a free-look az/el). Two input modes:
    - **Mouse-aim (default):** a central reticle (clamped to a unit disk inside `RETICLE_ZONE_PX`)
      maps x→bank, y→climb (`fly_reticle_command`); drawn as a 2D overlay. Keyboard works too as
      gross input.
    - **Free-look (hold SPACE):** cursor is disabled so mouse delta pans the camera all the way
      around + up/down the plane; flight is keyboard-only.
  **Controls:** A/D bank, W/S pitch→climb (`fly_keyboard_command` → kernel `Command`); **Q/E yaw**
  and **Shift/Ctrl throttle** are NOT kernel commands (the kernel has no yaw axis and constant TAS)
  so they are applied DOWNSTREAM by **re-seeding `predict::Predictor`** through the public Kernel API
  (the reconcile path) — no kernel/rail/golden touched. Wheel zooms `chase_dist`; P pauses, R resets.
  The marker (`draw_aircraft`) visibly rolls with bank and tilts with pitch; remotes drawn the same.
  Presentation-only: golden `529c6a05…` unchanged, 12/12 gates + ctest 7/7 (GCC). Receipt
  `…v1.4r0-6d3b06b.yml`.
  - **⚠ INTERIM BAND-AIDS to remove once Track B (§8) lands** (flagged in code): (a) the bank/turn
    is real but **pitch is NOT** — `target_climb` is a clamped vertical rate, so the marker's pitch
    is an **exaggerated presentation cue** (`own_pitch = cmd fraction × 25°`), not a flight-path
    angle; (b) **yaw and throttle are re-seed hacks**, not physics; (c) the own ship spawns at TAS
    150 to stay inside the Ki-61 LUTs. All three exist only because the kernel is a constant-TAS
    placeholder — **§8 B1/B2 make pitch, throttle and energy real in the kernel and these come out.**
- **Remaining polish (track A stretch, no seal):** aircraft-model meshes (vs the marker sphere);
  vendor Three.js for fully-offline web; an optional `--fly` web path. None touch a rail/golden.

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

## 8. Track B — the REAL flight model (the "flight kernel")  ← THE NEXT BIG TASK

> **Why:** SEADS today flies a *constant-TAS kinematic placeholder*. To be an actual dogfight sim
> the kernel must model **energy and aerodynamics**: thrust vs drag (so speed changes and there is a
> throttle), lift vs weight at an angle of attack (so **pitch is real** and altitude is *earned*),
> and load factor (so turn rate trades against speed and you can stall/black out). This is the heart
> of the product. **It must live in the kernel** — physical truth has to be bit-for-bit identical
> across toolchains/arch for lockstep multiplayer (CLAUDE.md §0/§1). Therefore **every phase below is
> a SEAL** (moves all four goldens) and runs the **full ritual**: det_math additions proven vs the
> MPFR oracle first, an adversarial **Auditor** pass, an **ADR**, then `/seal`. Do NOT shortcut this
> with `/fp:fast`, FMA, or libm — that breaks the one promise the whole project exists to keep (§5).

### 8.0 Design decisions to lock FIRST (write an ADR — this is a genuine architectural fork)
- **State vector.** Today `(lat, lon, psi, phi, alt, tas)`. The model adds longitudinal state. Pick
  the variable set and FIX it: recommended **add flight-path angle `gamma`** (vertical channel) and
  keep **`tas` as a true integrated state**; treat **bank `phi`** as before; derive **load factor
  `n`** and **angle-of-attack `alpha`** per tick from the aero solve (don't store what you can
  recompute — fewer states = fewer wire fields = smaller reseal). Decide explicitly whether `gamma`
  and `alpha` are *states* or *derived*; the golden hashes depend on it.
- **Inputs.** `Command` grows from `{target_phi, target_climb}` to roughly
  `{target_phi, pitch_or_g_cmd, throttle}`. Decide the pitch axis semantics NOW: **commanded load
  factor `n`** (arcade, stable, easy to clamp to a structural/`CLmax` limit) **vs commanded AoA**
  (sim-y, needs a stall model). Recommended: **commanded-g** for B2, add an AoA/stall layer in B3.
- **Atmosphere/density.** Rails say "still air". DECIDE: keep **constant sea-level density ρ₀**
  (avoids `det_exp`/`det_pow` entirely — strongly recommended for the first cut) **or** ISA density
  vs altitude (forces new det_math transcendentals + full MPFR coverage — defer, and if adopted it
  is itself a rail/seal change). Document the choice in the ADR; constant-ρ keeps B1–B4 to algebra +
  `sqrt`.
- **Integration scheme.** Δt is fixed at 0.01 s (rail). Pick the integrator and FREEZE the operation
  order (semi-implicit Euler is fine and cheap). The golden is defined by the *exact* op sequence,
  so `ref_kernel.py` and `kernel.cpp` must share it bit-for-bit (same trick as `lut_eval` today).
- **det_math budget.** Energy/aero needs at minimum **`det_sqrt`** (true airspeed terms, stall
  speed, `n`↔bank). If you keep constant density and commanded-g you can likely avoid `exp/log/pow`.
  Every new det_math primitive: add to `det_math.{h,cpp}`, mirror in `detmath_ref.py`, extend
  `det_math_oracle.py` + `gen_detmath_vectors.py` (structured boundaries + seeded random), prove
  ≤ target ULP vs MPFR, and asm-audit for FMA — BEFORE any kernel call uses it.

### 8.1 Phase B1 — Longitudinal energy: thrust/drag ⇒ real TAS + throttle  ✅ DONE (2026-06-29, seal v1.5r0)
**Shipped.** Energy model in `Kernel::step(cmd,env)` + `ref_kernel.step_scenario` (bit-identical op
order): `n=1/cos φ`, `q=½ρ₀V²`, `D = qS·cd0 + k·CL²·qS` (CL from `L=n·m·g₀`), `T = thr·T₀·(1−V/Vmax)`,
`Vdot=(T−D)/m − g₀·req/V`, floor `V_MIN=30`. Constants `RHO0`/`V_MIN` as shared hex-floats; **no new
det_math** (`+−×÷` + `det_cos`). `Command.throttle` added (default 0.0). Aero params on every
envelope (`mass_kg, wing_area_m2, cd0, induced_k, thrust_static_n, v_max_mps`) via the shared
`envelopes.py` loader → `gen_envelope_tables.py` + `gen_lockstep/predict_vectors.py` (all emit the
scalars). Goldens: **Sphere unchanged** `529c6a05…`; Turn `1d15c57a…`, Climb `4119a280…`, TurnClimb
`a1d9ce03…`, **Accel (new)** `225d5c13…`. Gates: 12/12 receipt PASS, ctest 7/7 GCC+Clang, 57 property
tests (+5 `test_energy.py`). guardian.yml extended with Accel in all 3 golden lists. Ledger:
ADR-Step8-FlightModel-B1-v1.5r0, SEAL_CARD v1.5r0, receipt `…v1.5r0-*.yml`. Viewer Shift/Ctrl now a
real throttle. **Initial aero is a balance pass — top speeds run low; retune is data-only in B4.**
The ORIGINAL B1 plan (kept for reference):
Make speed a real integrated state. Per tick: `T(throttle) − D(V) − W·sin(gamma)` drives `V̇`;
`V += V̇·dt`. Drag `D = ½·ρ₀·V²·S·C_d` (parasitic `C_d0` + induced `k·C_L²`); thrust from a simple
prop curve `T = throttle · T_static · (1 − V/V_max)` (or a small LUT). Add per-airframe **mass,
`S_ref`, `C_d0`, `k`, `T_static`, `V_max`** to the envelope schema (extends `data/tuning/envelopes/`
+ `gen_envelope_tables.py`). `Command` gains `throttle`. **Wire/reseal:** `tas` is already on the
KIN-001 block; **`throttle` need not be on the wire** (it's an input, not a state) — but confirm the
snapshot still captures enough to reconstruct (it does: `tas` is the state). Likely **no wire format
change** in B1 → the reseal is "golden moved", not "protocol bumped". Deliverables: kernel + `ref_
kernel.py` mirror, `det_sqrt` (if used) through the oracle, **regenerate all 4 goldens** + a new
**GOLDEN-SK-Accel-001** (throttle step → speed up/slow down), property tests (energy monotonic under
zero throttle = decel; level flight equilibrium), ADR + `/seal` (→ v1.5r0).

### 8.2 Phase B2 — Lift & pitch: flight-path angle from commanded-g  ✅ DONE (2026-06-29, seal v1.6r0)
**Shipped.** γ (flight-path angle) is a stored kernel state; `step(cmd,env)` is a full 3-DOF point mass
(Vdot=(T−D)/m−g0·sinγ; γ̇=(g0/V)(n·cosφ−cosγ); ψ̇=(g0/V)(n·sinφ/cosγ); alṫ=V·sinγ; ground speed V·cosγ).
`Command.target_climb`→**`target_g`** (commanded load factor n; global clamp [−3,9]). Generalizes B1
(γ=0,n=1/cosφ ⇒ old ψ̇=g0·tanφ/V). **No new det_math.** State 6→7-tuple; canonical snapshot +γ ⇒ **all
goldens moved incl. Sphere** (trajectory identical, γ=0 appended). **Wire reseal KIN-001→KIN-002** (γ×1e6,
snapshot protocol 2→3). New **GOLDEN-SK-Pitch-001**. C++≡Python bit-for-bit GCC+Clang; 12/12 gates, ctest
7/7, 64 property tests. Viewer band-aids retired. Ledger: ADR-Step8-FlightModel-B2-v1.6r0, SEAL_CARD
v1.6r0, receipt `…v1.6r0-*.yml`. **Documented limits → B3:** no stall/C_Lmax (n only globally clamped);
cosγ→0 vertical singularity (kept out of scenarios). The ORIGINAL B2 plan (kept for reference):
Make pitch real. Vertical channel becomes `alṫ = V·sin(gamma)`, `gammȧ` from the net normal force:
commanded load factor `n_cmd` → required `C_L` → if within `C_Lmax` the lift turns the velocity
vector (`gammȧ = g·(n·cos(phi) − cos(gamma))/V` for the pitch plane; bank splits `n` between turn and
climb). Replace `target_climb` with **`target_g`** (or pitch). Now W/S commands actual nose
attitude/energy trade, not a teleported climb rate, and the viewer's nose tilt becomes *physical*
(remove the exaggeration band-aid in `viewer_main.cpp`). Couples to B1: pulling g adds induced drag →
bleeds speed. **Wire/reseal:** if `gamma` becomes a *state* it should join KIN-001 (→ KIN-002 or a
protocol bump) so remotes/late-join reconstruct attitude — this is a **wire reseal** like layer 4b
was. Deliverables: kernel + ref mirror, goldens regenerated + **GOLDEN-SK-Loop/Pitch-001**, property
tests (sustained-vs-instantaneous turn, energy bleed in a pull), ADR + `/seal`.

### 8.3 Phase B3 — Limits & stall: C_Lmax, accelerated stall, structural g  ✅ DONE (2026-06-29, seal v1.7r0)
**Shipped.** Achievable load factor `n` bounded per-airframe by BOTH the structural g limits
(`n_max_struct`/`n_min_struct`) AND the C_Lmax aerodynamic ceiling `n_aero=cl_max*qS/(m*g0)`:
`n=clamp(n_cmd, max(n_min_struct,-n_aero), min(n_max_struct,n_aero))`. Accelerated stall (turn
collapses below the corner speed `V*=stall*sqrt(n_max_struct)`) + the 1 g stall speed are emergent;
B2's global `[-3,9]` placeholder retired; post-stall lift held at C_Lmax (no departure — deferred).
**No new state/wire/det_math** (n derived). New envelope scalars `cl_max,n_max_struct,n_min_struct`
(x8, coherent with `stall_tas_mps`; balance pass -> retune in B4). **Conservative extension: the 6
prior goldens are byte-identical** (limits never bind; verified C++=Python GCC+Clang); only NEW
**GOLDEN-SK-Stall-001** (`1a57b6d1...a0526fc6`, ki61 accelerated stall + p47d structural-g zoom).
12/12 gates, ctest 7/7, 72 property tests (+8 `test_stall.py`), `tuning_probe` extended. Ledger:
ADR-Step8-FlightModel-B3-v1.7r0, SEAL_CARD v1.7r0. The ORIGINAL B3 plan (kept for reference):
Clamp `C_L` to `C_Lmax(M?)`; exceeding → stall (lift breaks, `gammȧ`/turn collapse, optional
departure). Add a **structural g limit** and a **corner speed** that falls out naturally (max
sustained turn where `n·V` is maximized). This is what makes airframes *feel* different and makes
energy fighting emergent. Deliverables: stall/corner property tests per airframe, goldens
regenerated, ADR + `/seal`.

### 8.4 Phase B4 — Per-airframe tuning pass  ✅ DONE (2026-06-29, seal v1.8r0)
**Shipped.** Re-tuned all **8** envelopes so the roster reads true. Emergent level top speeds were
~30 % low (109–142 m/s); B4 lifts them to **historical** (148–195 m/s ≈ 533–702 km/h) by moving
**only `thrust_static_n`/`v_max_mps`** per airframe (`tools/b4_retune.py` solves a (top-speed,
best-climb) fit); all turn/stall params held fixed ⇒ **iconic turn ordering preserved exactly**
(A6M2/Spitfire best sustained, P-47 worst; instantaneous turn byte-identical) and B3 C_Lmax↔stall
coherence untouched. `v_max_mps` is now the flat-curve thrust-zero asymptote (~700, not Vne); `v_ne`
validation-only. **No new det_math/state/wire** — but it moves the **6 scenario goldens → reseal**
(Sphere unchanged). New `tools/perf_probe.py` (emergent-perf analyzer). 12/12 gates, 7/7 goldens
C++≡Python GCC+Clang, ctest 7/7, 72 property tests (`test_energy` settle horizon 80→140 s). Ledger:
ADR-Step8-FlightModel-B4-v1.8r0, SEAL_CARD v1.8r0, receipt. The ORIGINAL B4 plan (kept for reference):
Re-tune all **8** envelopes to the new aero parameters so the roster's relative strengths read true
(Spitfire turns, P-47 dives/zooms, A6M2 low-speed turn, etc.). Mostly **data-only** (rides the
current seal) like §3 was — *unless* you change a shared kernel default. Add the energy/aero columns
to each `data/tuning/envelopes/*.json`, re-run `tuning_probe.py` (extend it with energy checks),
optionally `hash_sign_json.py` to sign them.
> **Note (B4 reality vs the plan):** post-B1/B2/B3 the kernel *integrates* envelope aero, and 5 of
> the 8 airframes are baked into goldens, so a holistic retune **did** move goldens (a reseal) — the
> "rides the seal" hope was written pre-B1. The energy *columns* already existed (added in B1); B4 was
> a value retune, done as the cheapest-possible reseal (only 2 scalars/airframe moved).

### 8.5 Phase B5 — (optional, defer) ISA atmosphere vs altitude  (rail + seal)
Only if you want altitude to change performance (thinner air up high). Forces `det_exp`/`det_pow`
into det_math with full MPFR coverage, and changes the "still air / constant" atmosphere rail →
**rail reseal** (`/reseal` for the rail value, `/seal` for the model). Big lift; not needed for a
compelling dogfight. Keep constant-ρ until proven necessary.

### Cross-cutting (every phase)
- **Mirror-first determinism:** write/extend `tools/ref_kernel.py` (and `detmath_ref.py`) to the new
  math, get the Python gate green, THEN bit-match it in `kernel.cpp`. The Python harness *defines*
  the golden; C++ must reproduce it (CLAUDE.md §4).
- **Goldens:** every phase regenerates the 4 existing goldens (their hashes WILL change — that's the
  seal) and should ADD at least one scenario that exercises the new dynamic (accel, pitch/loop,
  stall). Use the existing `config/scenarios/*.json` → `seads_scenario` machinery.
- **Gates:** run the full §4 + §-verify sweep and `make_receipt.py` before and after; CI
  (`guardian.yml`) must stay green across MSVC/GCC/Clang × x64/AArch64 — the AArch64 legs are the
  real test of any new det_math (FMA/libm divergence risk).
- **Wire & viewer:** if a new variable becomes wire state, bump the snapshot protocol (KIN-002) and
  update `snapshot.{h,cpp}` + refs (reseal). The `--fly` viewer is the live test bed — after B2,
  delete the pitch-cue exaggeration and the yaw/throttle re-seed hacks in `viewer_main.cpp` and feed
  the real `Command` instead.
- **Seal sequence:** v1.4r0 → **v1.5r0** (B1) → v1.6r0 (B2) → v1.7r0 (B3) → data (B4) → optional
  v1.8r0 (B5). Don't batch phases into one seal; small seals keep the golden diffs auditable.

## Governance reminder (lean — see CLAUDE.md §2)
Governance is now minimal. **The whole law:** (1) keep the §4 gates green; (2) if a rail value or a
golden `world_hash` changes, bump the seal (`/seal` or `/reseal`) and say *why* in the commit
message — never silently; (3) keep the auto-receipt (`make_receipt.py`) and **this file** current —
that pair is the ledger and the continuity that actually matters; (4) ADRs/Forge cards are
**optional** (write one only for a genuine architectural fork). Everything else just rides the
current seal. The `validate_snapshot`/`validate_scenarios` gates fail loudly if anything moves a
hash, so trust the gates and ship.
